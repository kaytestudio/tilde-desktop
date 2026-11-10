#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>
#include <stdint.h>

#define MAX_TERMINAL_WIDTH  300
#define MAX_TERMINAL_HEIGHT 100

/* ──────────────────────────────────────────────
 * Color representation
 * mode 0 = default/standard (index 0-15)
 * mode 1 = 256-color        (index 0-255)
 * mode 2 = 24-bit truecolor (r, g, b)
 * ────────────────────────────────────────────── */
typedef struct {
    uint8_t mode;       /* 0=default, 1=256, 2=truecolor */
    uint8_t index;      /* used when mode <= 1           */
    uint8_t r, g, b;    /* used when mode == 2           */
} TermColor;

typedef struct {
    char      ch;
    TermColor fg;
    TermColor bg;
    int       bold;
    int       italic;
    int       underline;
    int       inverse;
    int       blink;
} Cell;

typedef struct {
    Cell   buffer[MAX_TERMINAL_HEIGHT][MAX_TERMINAL_WIDTH];
    int    cursor_x;
    int    cursor_y;
    int    width;
    int    height;

    /* current drawing attributes */
    TermColor current_fg;
    TermColor current_bg;
    int       current_bold;
    int       current_italic;
    int       current_underline;
    int       current_inverse;
    int       current_blink;

    /* ANSI escape-sequence parser state */
    char escape_buf[256];
    int  escape_len;
    int  in_escape;

    /* OSC title buffer (for shell title updates) */
    char osc_buf[256];
    int  osc_len;
    int  in_osc;
    char last_title[256];   /* most recent title set by shell */
} Terminal;

/* ── helpers ── */
TermColor termcolor_default(void);
TermColor termcolor_index(uint8_t idx);
TermColor termcolor_rgb(uint8_t r, uint8_t g, uint8_t b);
int       termcolor_equal(TermColor a, TermColor b);

void terminal_init(Terminal *term, int width, int height);
void terminal_resize(Terminal *term, int new_width, int new_height);
void terminal_write(Terminal *term, const char *data, size_t len);
void terminal_render(Terminal *term);
void terminal_clear(Terminal *term);

#endif /* TERMINAL_H */
