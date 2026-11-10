#include "mouse.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>

/* ─── drag-selection state ───────────────────────────────────────────── */
static int sel_start_x = -1, sel_start_y = -1;
static int sel_end_x   = -1, sel_end_y   = -1;
static int selecting   = 0;

/* ─── double-click state ─────────────────────────────────────────────── */
static struct timeval last_click_time = {0, 0};
static int last_click_x = -1, last_click_y = -1;
static int dbl_word_start_x = -1, dbl_word_end_x = -1, dbl_word_y = -1;

/* ─── enable / disable ───────────────────────────────────────────────── */

void mouse_enable(void) {
    printf("\033[?1000h");  /* normal click tracking     */
    printf("\033[?1002h");  /* button-event (drag)       */
    printf("\033[?1006h");  /* SGR extended coordinates  */
    fflush(stdout);
}

void mouse_disable(void) {
    printf("\033[?1000l");
    printf("\033[?1002l");
    printf("\033[?1006l");
    fflush(stdout);
}

/* ─── parse ──────────────────────────────────────────────────────────── */

int mouse_parse_event(const char *buf, size_t len, MouseEvent *event) {
    if (len < 6 || buf[0] != '\033' || buf[1] != '[' || buf[2] != '<')
        return 0;

    int button, x, y;
    char action;
    if (sscanf(buf + 3, "%d;%d;%d%c", &button, &x, &y, &action) != 4)
        return 0;

    event->x      = x - 1;          /* 0-indexed */
    event->y      = y - 1;
    event->button = button & 3;

    if (action == 'M') {
        if (button == 64)       event->type = MOUSE_EVENT_SCROLL_UP;
        else if (button == 65)  event->type = MOUSE_EVENT_SCROLL_DOWN;
        else                    event->type = MOUSE_EVENT_PRESS;
    } else if (action == 'm') {
        event->type = MOUSE_EVENT_RELEASE;
    } else {
        return 0;
    }
    return 1;
}

/* ─── drag selection ─────────────────────────────────────────────────── */

void mouse_handle_selection(Terminal *term, MouseEvent *event) {
    (void)term;
    if (event->type == MOUSE_EVENT_PRESS && event->button == 0) {
        sel_start_x = sel_end_x = event->x;
        sel_start_y = sel_end_y = event->y;
        selecting = 1;
        /* clear any previous double-click word sel */
        dbl_word_y = -1;
    } else if (event->type == MOUSE_EVENT_DRAG && selecting) {
        sel_end_x = event->x;
        sel_end_y = event->y;
    } else if (event->type == MOUSE_EVENT_RELEASE && event->button == 0) {
        selecting = 0;
    }
}

/* ─── extract drag selection string ─────────────────────────────────── */

char* mouse_get_selection(Terminal *term) {
    /* if we have a double-click word selection, return that instead */
    if (dbl_word_y >= 0 && dbl_word_start_x >= 0) {
        int len = dbl_word_end_x - dbl_word_start_x + 1;
        if (len <= 0) return NULL;
        char *out = (char*)malloc((size_t)len + 1);
        if (!out) return NULL;
        for (int i = 0; i < len; i++)
            out[i] = term->buffer[dbl_word_y][dbl_word_start_x + i].ch;
        out[len] = '\0';
        return out;
    }

    /* normal drag selection */
    if (sel_start_x < 0 || sel_start_y < 0 || sel_end_x < 0 || sel_end_y < 0)
        return NULL;

    int sy = sel_start_y, ey = sel_end_y;
    int sx = sel_start_x, ex = sel_end_x;

    /* normalise so sy <= ey */
    if (sy > ey || (sy == ey && sx > ex)) {
        int tmp;
        tmp = sy; sy = ey; ey = tmp;
        tmp = sx; sx = ex; ex = tmp;
    }
    if (sy == ey && sx > ex) { int t = sx; sx = ex; ex = t; }

    /* estimate size */
    size_t cap = 0;
    for (int y = sy; y <= ey && y < term->height; y++) {
        int lx = (y == sy) ? sx : 0;
        int rx = (y == ey) ? ex : term->width - 1;
        cap += (size_t)(rx - lx + 1) + 1;
    }

    char *out = (char*)malloc(cap + 1);
    if (!out) return NULL;
    char *p = out;

    for (int y = sy; y <= ey && y < term->height; y++) {
        int lx = (y == sy) ? sx : 0;
        int rx = (y == ey) ? ex : term->width - 1;
        for (int x = lx; x <= rx && x < term->width; x++)
            *p++ = term->buffer[y][x].ch;
        if (y < ey) *p++ = '\n';
    }
    *p = '\0';
    return out;
}

/* ─── word boundary helpers ──────────────────────────────────────────── */

static int is_word_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

char* mouse_get_word_at(Terminal *term, int x, int y) {
    if (y < 0 || y >= term->height || x < 0 || x >= term->width)
        return NULL;
    if (!is_word_char(term->buffer[y][x].ch))
        return NULL;

    /* expand left */
    int lx = x;
    while (lx > 0 && is_word_char(term->buffer[y][lx - 1].ch)) lx--;
    /* expand right */
    int rx = x;
    while (rx < term->width - 1 && is_word_char(term->buffer[y][rx + 1].ch)) rx++;

    int len = rx - lx + 1;
    char *out = (char*)malloc((size_t)len + 1);
    if (!out) return NULL;
    for (int i = 0; i < len; i++)
        out[i] = term->buffer[y][lx + i].ch;
    out[len] = '\0';

    return out;
}

/* ─── double-click ───────────────────────────────────────────────────── */

static long usec_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

int mouse_handle_double_click(Terminal *term, MouseEvent *event, int double_click_ms) {
    if (event->type != MOUSE_EVENT_PRESS || event->button != 0) return 0;

    struct timeval now_tv;
    gettimeofday(&now_tv, NULL);
    long now_us   = now_tv.tv_sec * 1000000L + now_tv.tv_usec;
    long last_us  = last_click_time.tv_sec * 1000000L + last_click_time.tv_usec;
    long gap_ms   = (now_us - last_us) / 1000;

    int is_dbl = (gap_ms <= double_click_ms &&
                  event->x == last_click_x &&
                  event->y == last_click_y);

    /* save for next comparison */
    last_click_time = now_tv;
    last_click_x    = event->x;
    last_click_y    = event->y;

    if (!is_dbl) return 0;

    /* double click detected — find word boundaries */
    int x = event->x, y = event->y;
    if (y < 0 || y >= term->height || x < 0 || x >= term->width) return 0;
    if (!is_word_char(term->buffer[y][x].ch)) return 0;

    int lx = x;
    while (lx > 0 && is_word_char(term->buffer[y][lx - 1].ch)) lx--;
    int rx = x;
    while (rx < term->width - 1 && is_word_char(term->buffer[y][rx + 1].ch)) rx++;

    dbl_word_y       = y;
    dbl_word_start_x = lx;
    dbl_word_end_x   = rx;

    /* also set drag-sel range so highlight works */
    sel_start_x = lx; sel_start_y = y;
    sel_end_x   = rx; sel_end_y   = y;

    return 1;
}
