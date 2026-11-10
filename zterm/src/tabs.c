#include "tabs.h"
#include "serial.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

/* ─── init ───────────────────────────────────────────────────────────── */

void tabs_init(TabManager *tm) {
    tm->num_tabs    = 0;
    tm->current_tab = -1;
    for (int i = 0; i < MAX_TABS; i++)
        tm->tabs[i].active = 0;
}

/* ─── create ─────────────────────────────────────────────────────────── */

int tabs_create(TabManager *tm, const char *shell, int width, int height,
                int scrollback_lines, const char *cwd) {
    if (tm->num_tabs >= MAX_TABS) return -1;

    int  index = tm->num_tabs;
    Tab *tab   = &tm->tabs[index];

    tab->type = TAB_TYPE_SHELL;
    terminal_init(&tab->terminal, width, height);
    scrollback_init(&tab->scrollback, scrollback_lines, width);
    search_init(&tab->search);

    /* If cwd is provided, chdir before exec inside the child.
     * We achieve this by temporarily chdir-ing, forking the pty,
     * then chdir-ing back.  This is safe because forkpty forks. */
    char orig_cwd[512] = {0};
    int  did_chdir     = 0;
    if (cwd && cwd[0]) {
        if (getcwd(orig_cwd, sizeof(orig_cwd)) != NULL) {
            if (chdir(cwd) == 0) did_chdir = 1;
        }
    }

    if (pty_create(&tab->pty, shell, width, height) < 0) {
        if (did_chdir) chdir(orig_cwd);
        scrollback_free(&tab->scrollback);
        search_free(&tab->search);
        return -1;
    }

    if (did_chdir) chdir(orig_cwd);

    snprintf(tab->title, sizeof(tab->title), "Tab %d", index + 1);
    tab->active = 1;

    tm->num_tabs++;
    if (tm->current_tab < 0)
        tm->current_tab = 0;

    /* switch to the newly created tab */
    tm->current_tab = index;
    return index;
}

/* ─── close ──────────────────────────────────────────────────────────── */

int tabs_close(TabManager *tm, int index) {
    if (index < 0 || index >= tm->num_tabs || !tm->tabs[index].active)
        return -1;

    if (tm->tabs[index].type == TAB_TYPE_SHELL)
        pty_close(&tm->tabs[index].pty);
    else if (tm->tabs[index].type == TAB_TYPE_SERIAL)
        serial_close(&tm->tabs[index].serial);
    
    scrollback_free(&tm->tabs[index].scrollback);
    search_free(&tm->tabs[index].search);
    tm->tabs[index].active = 0;

    /* shift down */
    for (int i = index; i < tm->num_tabs - 1; i++)
        tm->tabs[i] = tm->tabs[i + 1];
    tm->num_tabs--;

    if (tm->num_tabs == 0)            tm->current_tab = -1;
    else if (tm->current_tab >= tm->num_tabs) tm->current_tab = tm->num_tabs - 1;

    return 0;
}

/* ─── switch / next / prev ───────────────────────────────────────────── */

void tabs_switch(TabManager *tm, int index) {
    if (index >= 0 && index < tm->num_tabs && tm->tabs[index].active)
        tm->current_tab = index;
}

void tabs_next(TabManager *tm) {
    if (tm->num_tabs > 0)
        tm->current_tab = (tm->current_tab + 1) % tm->num_tabs;
}

void tabs_prev(TabManager *tm) {
    if (tm->num_tabs > 0)
        tm->current_tab = (tm->current_tab - 1 + tm->num_tabs) % tm->num_tabs;
}

Tab* tabs_current(TabManager *tm) {
    if (tm->current_tab >= 0 && tm->current_tab < tm->num_tabs)
        return &tm->tabs[tm->current_tab];
    return NULL;
}

/* ─── update titles from OSC ─────────────────────────────────────────── */

void tabs_update_titles(TabManager *tm) {
    for (int i = 0; i < tm->num_tabs; i++) {
        if (!tm->tabs[i].active) continue;
        if (tm->tabs[i].terminal.last_title[0] != '\0') {
            strncpy(tm->tabs[i].title, tm->tabs[i].terminal.last_title, 63);
            tm->tabs[i].title[63] = '\0';
            tm->tabs[i].terminal.last_title[0] = '\0'; /* consume it */
        }
    }
}

/* ─── render bar ─────────────────────────────────────────────────────── */

void tabs_render_bar(TabManager *tm) {
    printf("\033[H\033[K");           /* home + clear line */
    printf("\033[48;5;236m");         /* dark-gray background */

    for (int i = 0; i < tm->num_tabs; i++) {
        if (i == tm->current_tab)
            printf("\033[1;97;48;5;24m");   /* bright white on blue (active) */
        else
            printf("\033[37;48;5;236m");    /* dim white on dark gray */

        printf(" %s ", tm->tabs[i].title);
        printf("\033[0m\033[48;5;236m│");
    }

    /* right-aligned hint */
    printf("\033[60G\033[90m[Ctrl+T new  Ctrl+W close  Ctrl+] next  Ctrl+[ prev]\033[0m\n");
    fflush(stdout);
}

/* ─── resize all ─────────────────────────────────────────────────────── */

void tabs_resize_all(TabManager *tm, int width, int height) {
    for (int i = 0; i < tm->num_tabs; i++) {
        if (!tm->tabs[i].active) continue;
        terminal_resize(&tm->tabs[i].terminal, width, height - 1);
        scrollback_resize(&tm->tabs[i].scrollback, width);
        pty_resize(&tm->tabs[i].pty, width, height - 1);
    }
}

/* ─── session save ───────────────────────────────────────────────────── */

int tabs_save_session(TabManager *tm, const char *dir, const char *name) {
    Session sess;
    memset(&sess, 0, sizeof(sess));

    sess.num_tabs   = tm->num_tabs;
    sess.active_tab = tm->current_tab;

    for (int i = 0; i < tm->num_tabs && i < MAX_SESSION_TABS; i++) {
        strncpy(sess.tabs[i].title, tm->tabs[i].title, MAX_SESSION_TITLE - 1);

        /* try to get the live CWD of each tab's child shell */
        char cwd[MAX_SESSION_CWD] = {0};
        if (session_get_tab_cwd(tm->tabs[i].pty.child_pid, cwd, sizeof(cwd)) == 0
            && cwd[0] != '\0') {
            strncpy(sess.tabs[i].cwd, cwd, MAX_SESSION_CWD - 1);
        } else {
            strncpy(sess.tabs[i].cwd, ".", MAX_SESSION_CWD - 1);
        }
    }

    return session_save(&sess, dir, name);
}

/* ─── session restore ────────────────────────────────────────────────── */

int tabs_restore_session(TabManager *tm, const char *shell,
                         int width, int height, int scrollback_lines,
                         const char *dir, const char *name) {
    Session sess;
    if (session_load(&sess, dir, name) != 0) return -1;

    for (int i = 0; i < sess.num_tabs && i < MAX_SESSION_TABS; i++) {
        const char *cwd = sess.tabs[i].cwd[0] ? sess.tabs[i].cwd : NULL;
        int idx = tabs_create(tm, shell, width, height - 1, scrollback_lines, cwd);
        if (idx >= 0 && sess.tabs[i].title[0]) {
            strncpy(tm->tabs[idx].title, sess.tabs[i].title, 63);
            tm->tabs[idx].title[63] = '\0';
        }
    }

    /* restore active tab */
    if (sess.active_tab >= 0 && sess.active_tab < tm->num_tabs)
        tm->current_tab = sess.active_tab;

    return 0;
}

/* ─── create serial ──────────────────────────────────────────────────── */

int tabs_create_serial(TabManager *tm, const char *device, BaudRate baud,
                       int width, int height, int scrollback_lines) {
    if (tm->num_tabs >= MAX_TABS) return -1;

    int  index = tm->num_tabs;
    Tab *tab   = &tm->tabs[index];

    tab->type = TAB_TYPE_SERIAL;
    terminal_init(&tab->terminal, width, height);
    scrollback_init(&tab->scrollback, scrollback_lines, width);
    search_init(&tab->search);

    if (serial_open(&tab->serial, device, baud, 8, PARITY_NONE, STOPBITS_1, 0) < 0) {
        scrollback_free(&tab->scrollback);
        search_free(&tab->search);
        return -1;
    }

    /* extract device name for title (e.g. /dev/cu.usbserial-0001 → usbserial-0001) */
    const char *dev_name = strrchr(device, '/');
    dev_name = dev_name ? dev_name + 1 : device;
    snprintf(tab->title, sizeof(tab->title), "Serial: %s @%d", dev_name, (int)baud);
    tab->active = 1;

    tm->num_tabs++;
    if (tm->current_tab < 0)
        tm->current_tab = 0;

    tm->current_tab = index;
    return index;
}

/* ─── cleanup ────────────────────────────────────────────────────────── */

void tabs_cleanup(TabManager *tm) {
    for (int i = 0; i < tm->num_tabs; i++) {
        if (tm->tabs[i].active) {
            if (tm->tabs[i].type == TAB_TYPE_SHELL)
                pty_close(&tm->tabs[i].pty);
            else if (tm->tabs[i].type == TAB_TYPE_SERIAL)
                serial_close(&tm->tabs[i].serial);
            scrollback_free(&tm->tabs[i].scrollback);
            search_free(&tm->tabs[i].search);
        }
    }
}
