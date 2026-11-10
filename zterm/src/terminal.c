#include "terminal.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

/* ─── TermColor helpers ─────────────────────────────────────────────── */

TermColor termcolor_default(void) {
    TermColor c = {0, 0, 0, 0, 0};
    return c;
}

TermColor termcolor_index(uint8_t idx) {
    TermColor c;
    c.mode  = (idx < 16) ? 0 : 1;
    c.index = idx;
    c.r = c.g = c.b = 0;
    return c;
}

TermColor termcolor_rgb(uint8_t r, uint8_t g, uint8_t b) {
    TermColor c;
    c.mode  = 2;
    c.index = 0;
    c.r = r; c.g = g; c.b = b;
    return c;
}

int termcolor_equal(TermColor a, TermColor b) {
    if (a.mode != b.mode) return 0;
    if (a.mode <= 1)      return a.index == b.index;
    return (a.r == b.r) && (a.g == b.g) && (a.b == b.b);
}

/* ─── static tables for the 16 standard colours ─────────────────────── */
static const char *fg16[] = {
    "\033[30m","\033[31m","\033[32m","\033[33m",
    "\033[34m","\033[35m","\033[36m","\033[37m",
    "\033[90m","\033[91m","\033[92m","\033[93m",
    "\033[94m","\033[95m","\033[96m","\033[97m"
};
static const char *bg16[] = {
    "\033[40m","\033[41m","\033[42m","\033[43m",
    "\033[44m","\033[45m","\033[46m","\033[47m",
    "\033[100m","\033[101m","\033[102m","\033[103m",
    "\033[104m","\033[105m","\033[106m","\033[107m"
};

/* ─── emit a TermColor as an ANSI escape to stdout ───────────────────── */
static void emit_fg(TermColor c) {
    if (c.mode == 0 && c.index == 0) { printf("\033[39m"); return; }
    if (c.mode <= 1 && c.index < 16) { printf("%s", fg16[c.index]); return; }
    if (c.mode == 1)                 { printf("\033[38;5;%dm", c.index); return; }
                                       printf("\033[38;2;%d;%d;%dm", c.r, c.g, c.b);
}

static void emit_bg(TermColor c) {
    if (c.mode == 0 && c.index == 0) { printf("\033[49m"); return; }
    if (c.mode <= 1 && c.index < 16) { printf("%s", bg16[c.index]); return; }
    if (c.mode == 1)                 { printf("\033[48;5;%dm", c.index); return; }
                                       printf("\033[48;2;%d;%d;%dm", c.r, c.g, c.b);
}

/* ─── init / clear / resize ──────────────────────────────────────────── */

static void clear_cell(Cell *c) {
    c->ch        = ' ';
    c->fg        = termcolor_default();
    c->bg        = termcolor_default();
    c->bold      = 0; c->italic   = 0;
    c->underline = 0; c->inverse  = 0; c->blink = 0;
}

void terminal_init(Terminal *term, int width, int height) {
    term->width  = width  > MAX_TERMINAL_WIDTH  ? MAX_TERMINAL_WIDTH  : width;
    term->height = height > MAX_TERMINAL_HEIGHT ? MAX_TERMINAL_HEIGHT : height;
    for (int y = 0; y < MAX_TERMINAL_HEIGHT; y++)
        for (int x = 0; x < MAX_TERMINAL_WIDTH; x++)
            clear_cell(&term->buffer[y][x]);

    term->cursor_x = term->cursor_y = 0;
    term->current_fg     = termcolor_default();
    term->current_bg     = termcolor_default();
    term->current_bold   = 0; term->current_italic   = 0;
    term->current_underline = 0; term->current_inverse = 0;
    term->current_blink  = 0;
    term->in_escape      = 0; term->escape_len = 0;
    term->in_osc         = 0; term->osc_len    = 0;
    term->last_title[0]  = '\0';
}

void terminal_clear(Terminal *term) {
    for (int y = 0; y < term->height; y++)
        for (int x = 0; x < term->width; x++)
            clear_cell(&term->buffer[y][x]);
    term->cursor_x = term->cursor_y = 0;
}

void terminal_resize(Terminal *term, int nw, int nh) {
    if (nw > MAX_TERMINAL_WIDTH)  nw = MAX_TERMINAL_WIDTH;
    if (nh > MAX_TERMINAL_HEIGHT) nh = MAX_TERMINAL_HEIGHT;
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;

    if (nw > term->width)
        for (int y = 0; y < term->height; y++)
            for (int x = term->width; x < nw; x++)
                clear_cell(&term->buffer[y][x]);
    if (nh > term->height)
        for (int y = term->height; y < nh; y++)
            for (int x = 0; x < nw; x++)
                clear_cell(&term->buffer[y][x]);

    term->width  = nw;
    term->height = nh;
    if (term->cursor_x >= nw) term->cursor_x = nw - 1;
    if (term->cursor_y >= nh) term->cursor_y = nh - 1;
}

/* ─── scroll helper ──────────────────────────────────────────────────── */

static void scroll_up(Terminal *term) {
    memmove(term->buffer[0], term->buffer[1],
            sizeof(term->buffer[0]) * (term->height - 1));
    for (int x = 0; x < term->width; x++)
        clear_cell(&term->buffer[term->height - 1][x]);
    term->cursor_y = term->height - 1;
}

/* ─── SGR parser (handles 256 + truecolor) ──────────────────────────── */

static void parse_sgr(Terminal *term, const char *params) {
    if (!params[0] || strcmp(params, "0") == 0) {
        term->current_fg        = termcolor_default();
        term->current_bg        = termcolor_default();
        term->current_bold      = 0; term->current_italic   = 0;
        term->current_underline = 0; term->current_inverse  = 0;
        term->current_blink     = 0;
        return;
    }

    char buf[512];
    strncpy(buf, params, 511); buf[511] = '\0';

    int codes[64], n = 0;
    char *tok = strtok(buf, ";");
    while (tok && n < 64) { codes[n++] = atoi(tok); tok = strtok(NULL, ";"); }

    for (int i = 0; i < n; i++) {
        int c = codes[i];
        switch (c) {
            case 0:
                term->current_fg        = termcolor_default();
                term->current_bg        = termcolor_default();
                term->current_bold      = 0; term->current_italic   = 0;
                term->current_underline = 0; term->current_inverse  = 0;
                term->current_blink     = 0;
                break;
            case 1:  term->current_bold      = 1; break;
            case 3:  term->current_italic    = 1; break;
            case 4:  term->current_underline = 1; break;
            case 7:  term->current_inverse   = 1; break;
            case 5:  term->current_blink     = 1; break;
            case 22: term->current_bold      = 0; break;
            case 23: term->current_italic    = 0; break;
            case 24: term->current_underline = 0; break;
            case 27: term->current_inverse   = 0; break;
            case 25: term->current_blink     = 0; break;

            case 30: case 31: case 32: case 33:
            case 34: case 35: case 36: case 37:
                term->current_fg = termcolor_index(c - 30); break;
            case 39: term->current_fg = termcolor_default(); break;

            case 40: case 41: case 42: case 43:
            case 44: case 45: case 46: case 47:
                term->current_bg = termcolor_index(c - 40); break;
            case 49: term->current_bg = termcolor_default(); break;

            case 90: case 91: case 92: case 93:
            case 94: case 95: case 96: case 97:
                term->current_fg = termcolor_index(c - 90 + 8); break;

            case 100: case 101: case 102: case 103:
            case 104: case 105: case 106: case 107:
                term->current_bg = termcolor_index(c - 100 + 8); break;

            /* extended fg:  38;5;n   or  38;2;r;g;b */
            case 38:
                if (i+1 < n && codes[i+1] == 5 && i+2 < n) {
                    term->current_fg = termcolor_index((uint8_t)codes[i+2]);
                    i += 2;
                } else if (i+1 < n && codes[i+1] == 2 && i+4 < n) {
                    term->current_fg = termcolor_rgb(
                        (uint8_t)codes[i+2], (uint8_t)codes[i+3], (uint8_t)codes[i+4]);
                    i += 4;
                }
                break;

            /* extended bg:  48;5;n   or  48;2;r;g;b */
            case 48:
                if (i+1 < n && codes[i+1] == 5 && i+2 < n) {
                    term->current_bg = termcolor_index((uint8_t)codes[i+2]);
                    i += 2;
                } else if (i+1 < n && codes[i+1] == 2 && i+4 < n) {
                    term->current_bg = termcolor_rgb(
                        (uint8_t)codes[i+2], (uint8_t)codes[i+3], (uint8_t)codes[i+4]);
                    i += 4;
                }
                break;
        }
    }
}

/* ─── CSI sequence dispatcher ────────────────────────────────────────── */

static void process_escape_sequence(Terminal *term) {
    if (term->escape_len < 2) return;
    if (term->escape_buf[0] != '[') return;

    char cmd    = term->escape_buf[term->escape_len - 1];
    char params[256] = {0};
    if (term->escape_len > 2)
        strncpy(params, term->escape_buf + 1, term->escape_len - 2);

    switch (cmd) {
        case 'm':
            parse_sgr(term, params);
            break;

        case 'H': case 'f': {
            int row = 1, col = 1;
            if (params[0]) sscanf(params, "%d;%d", &row, &col);
            term->cursor_y = row - 1;
            term->cursor_x = col - 1;
            if (term->cursor_y < 0)             term->cursor_y = 0;
            if (term->cursor_y >= term->height) term->cursor_y = term->height - 1;
            if (term->cursor_x < 0)             term->cursor_x = 0;
            if (term->cursor_x >= term->width)  term->cursor_x = term->width  - 1;
            break;
        }

        case 'A': {
            int n = params[0] ? atoi(params) : 1;
            term->cursor_y -= n;
            if (term->cursor_y < 0) term->cursor_y = 0;
            break;
        }
        case 'B': {
            int n = params[0] ? atoi(params) : 1;
            term->cursor_y += n;
            if (term->cursor_y >= term->height) term->cursor_y = term->height - 1;
            break;
        }
        case 'C': {
            int n = params[0] ? atoi(params) : 1;
            term->cursor_x += n;
            if (term->cursor_x >= term->width) term->cursor_x = term->width - 1;
            break;
        }
        case 'D': {
            int n = params[0] ? atoi(params) : 1;
            term->cursor_x -= n;
            if (term->cursor_x < 0) term->cursor_x = 0;
            break;
        }

        case 'J':
            if (params[0] == '2' || params[0] == '\0')
                terminal_clear(term);
            break;

        case 'K': {
            int mode = params[0] ? atoi(params) : 0;
            if (mode == 0) {
                for (int x = term->cursor_x; x < term->width; x++)
                    clear_cell(&term->buffer[term->cursor_y][x]);
            } else if (mode == 1) {
                for (int x = 0; x <= term->cursor_x && x < term->width; x++)
                    clear_cell(&term->buffer[term->cursor_y][x]);
            } else if (mode == 2) {
                for (int x = 0; x < term->width; x++)
                    clear_cell(&term->buffer[term->cursor_y][x]);
            }
            break;
        }

        case 'L': {
            int n = params[0] ? atoi(params) : 1;
            for (int i = term->height - 1; i >= term->cursor_y + n; i--)
                memcpy(term->buffer[i], term->buffer[i - n], sizeof(Cell) * term->width);
            for (int i = 0; i < n && term->cursor_y + i < term->height; i++)
                for (int x = 0; x < term->width; x++)
                    clear_cell(&term->buffer[term->cursor_y + i][x]);
            break;
        }

        case 'M': {
            int n = params[0] ? atoi(params) : 1;
            for (int i = term->cursor_y; i < term->height - n; i++)
                memcpy(term->buffer[i], term->buffer[i + n], sizeof(Cell) * term->width);
            for (int i = term->height - n; i < term->height; i++)
                for (int x = 0; x < term->width; x++)
                    clear_cell(&term->buffer[i][x]);
            break;
        }

        default: break;
    }
}

/* ─── OSC handler (title) ────────────────────────────────────────────── */

static void process_osc(Terminal *term) {
    if (term->osc_len < 2) return;
    if ((term->osc_buf[0] == '0' || term->osc_buf[0] == '2') && term->osc_buf[1] == ';') {
        strncpy(term->last_title, term->osc_buf + 2, 255);
        term->last_title[255] = '\0';
    }
}

/* ─── main write ─────────────────────────────────────────────────────── */

void terminal_write(Terminal *term, const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)data[i];

        /* ── OSC state machine ── */
        if (term->in_osc) {
            if (c == 0x07) {
                term->osc_buf[term->osc_len] = '\0';
                process_osc(term);
                term->in_osc = 0; term->osc_len = 0;
                continue;
            }
            if (c == '\033' && i + 1 < len && data[i+1] == '\\') {
                term->osc_buf[term->osc_len] = '\0';
                process_osc(term);
                term->in_osc = 0; term->osc_len = 0;
                i++;
                continue;
            }
            if (term->osc_len < 255)
                term->osc_buf[term->osc_len++] = (char)c;
            continue;
        }

        /* ── CSI state machine ── */
        if (term->in_escape) {
            term->escape_buf[term->escape_len++] = (char)c;
            if (term->escape_len >= 255 ||
                (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z')) {
                term->escape_buf[term->escape_len] = '\0';
                process_escape_sequence(term);
                term->in_escape = 0;
                term->escape_len = 0;
            }
            continue;
        }

        /* ── ESC dispatch ── */
        if (c == '\033') {
            if (i + 1 < len && data[i+1] == ']') {
                term->in_osc = 1;
                term->osc_len = 0;
                i++;
                continue;
            }
            term->in_escape = 1;
            term->escape_len = 0;
            continue;
        }

        /* ── control characters ── */
        if (c == '\n') {
            term->cursor_y++;
            if (term->cursor_y >= term->height) scroll_up(term);
            continue;
        }
        if (c == '\r') { term->cursor_x = 0; continue; }
        if (c == '\b' || c == 127) {
            if (term->cursor_x > 0) term->cursor_x--;
            continue;
        }
        if (c == '\t') {
            int spaces = 8 - (term->cursor_x % 8);
            for (int j = 0; j < spaces && term->cursor_x < term->width; j++) {
                clear_cell(&term->buffer[term->cursor_y][term->cursor_x]);
                term->cursor_x++;
            }
            continue;
        }
        if (c == '\a') continue;

        /* ── printable ── */
        if (c >= 32) {
            Cell *cell = &term->buffer[term->cursor_y][term->cursor_x];
            cell->ch        = (char)c;
            cell->fg        = term->current_fg;
            cell->bg        = term->current_bg;
            cell->bold      = term->current_bold;
            cell->italic    = term->current_italic;
            cell->underline = term->current_underline;
            cell->inverse   = term->current_inverse;
            cell->blink     = term->current_blink;

            term->cursor_x++;
            if (term->cursor_x >= term->width) {
                term->cursor_x = 0;
                term->cursor_y++;
                if (term->cursor_y >= term->height) scroll_up(term);
            }
        }
    }
}

/* ─── render ─────────────────────────────────────────────────────────── */

void terminal_render(Terminal *term) {
    printf("\033[2J\033[H");

    TermColor last_fg = termcolor_default();
    TermColor last_bg = termcolor_default();
    int last_bold = 0, last_italic = 0, last_underline = 0;
    int last_inverse = 0, last_blink = 0;

    for (int y = 0; y < term->height; y++) {
        for (int x = 0; x < term->width; x++) {
            Cell *cell = &term->buffer[y][x];

            int changed =
                !termcolor_equal(cell->fg, last_fg)  ||
                !termcolor_equal(cell->bg, last_bg)  ||
                cell->bold      != last_bold         ||
                cell->italic    != last_italic       ||
                cell->underline != last_underline    ||
                cell->inverse   != last_inverse      ||
                cell->blink     != last_blink;

            if (changed) {
                printf("\033[0m");
                if (cell->bold)      printf("\033[1m");
                if (cell->italic)    printf("\033[3m");
                if (cell->underline) printf("\033[4m");
                if (cell->blink)     printf("\033[5m");
                if (cell->inverse)   printf("\033[7m");
                emit_fg(cell->fg);
                emit_bg(cell->bg);

                last_fg        = cell->fg;
                last_bg        = cell->bg;
                last_bold      = cell->bold;
                last_italic    = cell->italic;
                last_underline = cell->underline;
                last_inverse   = cell->inverse;
                last_blink     = cell->blink;
            }

            putchar(cell->ch);
        }
        printf("\033[0m\n");
        last_fg = last_bg = termcolor_default();
        last_bold = last_italic = last_underline = last_inverse = last_blink = 0;
    }

    printf("\033[%d;%dH", term->cursor_y + 1, term->cursor_x + 1);
    fflush(stdout);
}
