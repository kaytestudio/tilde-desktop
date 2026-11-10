#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>

#include "terminal.h"
#include "pty.h"
#include "tabs.h"
#include "mouse.h"
#include "clipboard.h"
#include "config.h"
#include "scrollback.h"
#include "search.h"
#include "themes.h"
#include "session.h"

/* ─── globals shared with signal handler ─────────────────────────────── */
static struct termios orig_termios;
static volatile sig_atomic_t resize_pending = 0;

/* ─── search-mode state ──────────────────────────────────────────────── */
static int  search_mode     = 0;
static char search_query[MAX_SEARCH_QUERY] = {0};
static int  search_query_len = 0;

/* ─── raw mode ───────────────────────────────────────────────────────── */

static void disable_raw_mode(void) {
    mouse_disable();
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/* ─── SIGWINCH ───────────────────────────────────────────────────────── */

static void handle_sigwinch(int sig) { (void)sig; resize_pending = 1; }

static void get_terminal_size(int *w, int *h) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) { *w = ws.ws_col; *h = ws.ws_row; }
    else { *w = 80; *h = 24; }
}

/* ─── render helpers ─────────────────────────────────────────────────── */

static void render_search_bar(Tab *tab) {
    printf("\033[2;1H\033[K");   /* row 2, clear */
    printf("\033[48;5;58m\033[1;97m");
    printf(" 🔍 Search: %s", search_query);
    if (tab->search.num_matches > 0)
        printf(" \033[0;97;48;5;58m[%d/%d]", tab->search.current_match + 1, tab->search.num_matches);
    else if (search_query[0])
        printf(" \033[0;91;48;5;58m[No matches]");
    printf(" \033[0;90;48;5;58m(n next  p prev  Enter done  Esc cancel)\033[0m");
    fflush(stdout);
}

static void full_render(Tab *tab, TabManager *tm) {
    printf("\033[2J");
    tabs_render_bar(tm);

    if (search_mode) render_search_bar(tab);

    /* if scrolled, overlay scrollback into terminal buffer */
    if (tab->scrollback.scroll_offset > 0)
        scrollback_render(&tab->scrollback, &tab->terminal, tab->terminal.height);

    /* highlight search matches */
    if (tab->search.active) {
        int sb_vis_start = 0;
        if (tab->scrollback.scroll_offset > 0)
            sb_vis_start = tab->scrollback.num_lines - tab->scrollback.scroll_offset;
        search_highlight(&tab->search, &tab->terminal, tab->terminal.height,
                         &tab->scrollback, sb_vis_start);
    }

    terminal_render(&tab->terminal);
}

/* ─── resize handler ─────────────────────────────────────────────────── */

static void handle_resize(TabManager *tm) {
    int w, h;
    get_terminal_size(&w, &h);
    tabs_resize_all(tm, w, h);

    Tab *tab = tabs_current(tm);
    if (tab) full_render(tab, tm);
}

/* ─── main ───────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* ── config ── */
    Config config;
    config_init(&config);

    char config_path[512];
    config_get_default_path(config_path, sizeof(config_path));
    if (config_load(&config, config_path) < 0)
        config_save(&config, config_path);   /* write defaults on first run */

    time_t config_mtime = config_get_mtime(config_path);

    /* ── theme ── */
    const Theme *theme = theme_find(config.theme);

    /* ── tab manager (heap — too large for stack) ── */
    TabManager *tm = malloc(sizeof(TabManager));
    if (!tm) { fprintf(stderr, "fatal: cannot allocate TabManager\n"); return 1; }
    tabs_init(tm);

    int width, height;
    get_terminal_size(&width, &height);

    /* ── session restore or fresh start ── */
    int restored = 0;
    if (config.session_auto_restore) {
        /* try to load "last" session */
        restored = (tabs_restore_session(tm, config.shell, width, height,
                                         config.scrollback_lines,
                                         config.session_dir, "last") == 0);
    }
    if (!restored || tm->num_tabs == 0) {
        tabs_create(tm, config.shell, width, height - 1,
                    config.scrollback_lines, NULL);
    }

    /* ── signal handler ── */
    struct sigaction sa;
    sa.sa_handler = handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, NULL);

    /* ── raw + mouse ── */
    enable_raw_mode();
    if (config.mouse_enabled) mouse_enable();

    /* ── apply theme (OSC sequences) ── */
    theme_apply(theme);

    /* ── splash ── */
    printf("\033[2J\033[H");
    printf("\033[1;36mZ-Terminal\033[0m \033[32mv3.0\033[0m  "
           "(%dx%d)  theme: \033[33m%s\033[0m\n", width, height, theme->name);
    if (restored)
        printf("\033[32mSession restored.\033[0m\n");
    printf("\033[90mCtrl+T new tab | Ctrl+W close | Ctrl+F search | "
           "Ctrl+V paste | Ctrl+Q quit\033[0m\n");
    sleep(1);

    /* ─── main loop ──────────────────────────────────────────────────── */
    char input_buf[256];
    char output_buf[4096];

    while (1) {
        /* ── resize ── */
        if (resize_pending) {
            resize_pending = 0;
            get_terminal_size(&width, &height);
            handle_resize(tm);
        }

        /* ── config hot-reload ── */
        if (config_reload_if_changed(&config, config_path, &config_mtime)) {
            /* re-apply theme if it changed */
            const Theme *new_theme = theme_find(config.theme);
            if (new_theme != theme) {
                theme = new_theme;
                theme_apply(theme);
            }
            /* mouse toggle */
            if (config.mouse_enabled) mouse_enable();
            else mouse_disable();
        }

        Tab *current = tabs_current(tm);
        if (!current) break;

        /* ── update titles from shell OSC ── */
        tabs_update_titles(tm);

        /* ── PTY/Serial read ── */
        if (!search_mode) {
            ssize_t n = 0;
            if (current->type == TAB_TYPE_SHELL) {
                n = read(current->pty.master_fd, output_buf, sizeof(output_buf) - 1);
            } else if (current->type == TAB_TYPE_SERIAL) {
                n = serial_read(&current->serial, output_buf, sizeof(output_buf) - 1);
            }

            if (n > 0) {
                /* push top line to scrollback before writing */
                scrollback_add_line(&current->scrollback,
                                    current->terminal.buffer[0],
                                    current->terminal.width);
                terminal_write(&current->terminal, output_buf, (size_t)n);
                full_render(current, tm);
            }
        }

        /* ── stdin read ── */
        ssize_t n = read(STDIN_FILENO, input_buf, sizeof(input_buf));
        if (n <= 0) { usleep((unsigned int)config.refresh_rate_ms * 1000); continue; }

        /* ════════════════════════════════════════════════════════════════
         * SEARCH MODE
         * ════════════════════════════════════════════════════════════════ */
        if (search_mode) {
            if (input_buf[0] == 27) { /* Esc */
                search_stop(&current->search);
                search_mode = 0;
                search_query[0] = '\0'; search_query_len = 0;
                full_render(current, tm);
            } else if (input_buf[0] == '\n' || input_buf[0] == '\r') { /* Enter */
                search_mode = 0;
                full_render(current, tm);
            } else if (input_buf[0] == 127 || input_buf[0] == 8) { /* Backspace */
                if (search_query_len > 0) {
                    search_query[--search_query_len] = '\0';
                    search_start(&current->search, search_query, 0);
                    search_execute(&current->search, &current->terminal, &current->scrollback);
                    search_jump_to_current(&current->search, &current->scrollback,
                                           current->terminal.height);
                    full_render(current, tm);
                }
            } else if (input_buf[0] == 'n') { /* next */
                search_next(&current->search);
                search_jump_to_current(&current->search, &current->scrollback,
                                       current->terminal.height);
                full_render(current, tm);
            } else if (input_buf[0] == 'p') { /* prev */
                search_prev(&current->search);
                search_jump_to_current(&current->search, &current->scrollback,
                                       current->terminal.height);
                full_render(current, tm);
            } else if (input_buf[0] >= 32 && input_buf[0] < 127) { /* printable */
                if (search_query_len < MAX_SEARCH_QUERY - 1) {
                    search_query[search_query_len++] = input_buf[0];
                    search_query[search_query_len]   = '\0';
                    search_start(&current->search, search_query, 0);
                    search_execute(&current->search, &current->terminal, &current->scrollback);
                    search_jump_to_current(&current->search, &current->scrollback,
                                           current->terminal.height);
                    full_render(current, tm);
                }
            }
            continue;
        }

        /* ════════════════════════════════════════════════════════════════
         * MOUSE EVENTS
         * ════════════════════════════════════════════════════════════════ */
        if (config.mouse_enabled && n >= 3 &&
            input_buf[0] == '\033' && input_buf[1] == '[' && input_buf[2] == '<') {
            MouseEvent ev;
            if (mouse_parse_event(input_buf, (size_t)n, &ev)) {
                /* scroll wheel */
                if (ev.type == MOUSE_EVENT_SCROLL_UP) {
                    scrollback_scroll_up(&current->scrollback, 3);
                    full_render(current, tm);
                    continue;
                }
                if (ev.type == MOUSE_EVENT_SCROLL_DOWN) {
                    scrollback_scroll_down(&current->scrollback, 3);
                    full_render(current, tm);
                    continue;
                }

                /* double-click word selection */
                if (config.clipboard_enabled &&
                    mouse_handle_double_click(&current->terminal, &ev, config.double_click_ms)) {
                    char *word = mouse_get_selection(&current->terminal);
                    if (word) { clipboard_set(word); free(word); }
                    full_render(current, tm);
                    continue;
                }

                /* single-click drag */
                mouse_handle_selection(&current->terminal, &ev);

                /* middle-click → copy selection */
                if (ev.type == MOUSE_EVENT_PRESS && ev.button == 1 && config.clipboard_enabled) {
                    char *sel = mouse_get_selection(&current->terminal);
                    if (sel) { clipboard_set(sel); free(sel); }
                }
                continue;
            }
        }

        /* ════════════════════════════════════════════════════════════════
         * KEYBOARD — single-byte control codes
         * ════════════════════════════════════════════════════════════════ */
        if (n == 1) {
            switch (input_buf[0]) {
                case 17: /* Ctrl+Q — quit */
                    goto quit;

                case 20: /* Ctrl+T — new tab */
                    tabs_create(tm, config.shell, width, height - 1,
                                config.scrollback_lines, NULL);
                    current = tabs_current(tm);
                    full_render(current, tm);
                    continue;

                case 13: /* Ctrl+M — serial port */
                    {
                        /* list available ports and create serial tab with first one */
                        char ports[MAX_SERIAL_PORTS][MAX_PORT_NAME];
                        int count = serial_list_ports(ports, MAX_SERIAL_PORTS);
                        if (count > 0) {
                            tabs_create_serial(tm, ports[0], BAUD_115200, width, height - 1,
                                             config.scrollback_lines);
                            current = tabs_current(tm);
                            full_render(current, tm);
                        } else {
                            /* TODO: show "no serial ports found" message */
                        }
                    }
                    continue;

                case 23: /* Ctrl+W — close tab */
                    tabs_close(tm, tm->current_tab);
                    if (tm->num_tabs == 0) goto quit;
                    current = tabs_current(tm);
                    full_render(current, tm);
                    continue;

                case 3: /* Ctrl+C — copy */
                    if (config.clipboard_enabled) {
                        char *sel = mouse_get_selection(&current->terminal);
                        if (sel) { clipboard_set(sel); free(sel); }
                    }
                    continue;

                case 22: /* Ctrl+V — paste */
                    if (config.clipboard_enabled) {
                        char *clip = clipboard_get();
                        if (clip) {
                            pty_write(&current->pty, clip, strlen(clip));
                            free(clip);
                        }
                    }
                    continue;

                case 6: /* Ctrl+F — search */
                    if (config.search_enabled) {
                        search_mode = 1;
                        search_query[0] = '\0'; search_query_len = 0;
                        search_start(&current->search, "", 0);
                        full_render(current, tm);
                    }
                    continue;

                case 19: /* Ctrl+S — toggle scroll */
                    if (current->scrollback.scroll_offset == 0)
                        scrollback_scroll_up(&current->scrollback, current->terminal.height);
                    else
                        scrollback_scroll_to_bottom(&current->scrollback);
                    full_render(current, tm);
                    continue;
            }
        }

        /* ════════════════════════════════════════════════════════════════
         * KEYBOARD — multi-byte sequences
         * ════════════════════════════════════════════════════════════════ */
        if (n >= 3 && input_buf[0] == '\033' && input_buf[1] == '[') {
            /* Page Up */
            if (input_buf[2] == '5' && n >= 4 && input_buf[3] == '~') {
                scrollback_scroll_up(&current->scrollback, current->terminal.height);
                full_render(current, tm);
                continue;
            }
            /* Page Down */
            if (input_buf[2] == '6' && n >= 4 && input_buf[3] == '~') {
                scrollback_scroll_down(&current->scrollback, current->terminal.height);
                full_render(current, tm);
                continue;
            }
        }

        if (n == 2 && input_buf[0] == '\033') {
            if (input_buf[1] == ']') { /* Ctrl+] — next tab */
                tabs_next(tm);
                current = tabs_current(tm);
                full_render(current, tm);
                continue;
            }
            if (input_buf[1] == '[') { /* Ctrl+[ — prev tab */
                tabs_prev(tm);
                current = tabs_current(tm);
                full_render(current, tm);
                continue;
            }
        }

        /* ── forward everything else to the PTY/Serial ── */
        if (current->type == TAB_TYPE_SHELL)
            pty_write(&current->pty, input_buf, (size_t)n);
        else if (current->type == TAB_TYPE_SERIAL)
            serial_write(&current->serial, input_buf, (size_t)n);

        usleep((unsigned int)config.refresh_rate_ms * 1000);
    }

quit:
    /* ── auto-save session ── */
    if (config.session_auto_save)
        tabs_save_session(tm, config.session_dir, "last");

    tabs_cleanup(tm);
    disable_raw_mode();
    printf("\n\n\033[1;35mZ-Terminal exited.\033[0m\n");
    free(tm);
    return 0;
}
