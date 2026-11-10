#ifndef MOUSE_H
#define MOUSE_H

#include "terminal.h"
#include <time.h>

typedef enum {
    MOUSE_EVENT_NONE,
    MOUSE_EVENT_PRESS,
    MOUSE_EVENT_RELEASE,
    MOUSE_EVENT_DRAG,
    MOUSE_EVENT_SCROLL_UP,
    MOUSE_EVENT_SCROLL_DOWN
} MouseEventType;

typedef struct {
    MouseEventType type;
    int x;
    int y;
    int button; /* 0=left, 1=middle, 2=right */
} MouseEvent;

/* ── enable / disable SGR mouse tracking ── */
void mouse_enable(void);
void mouse_disable(void);

/* ── parse raw byte stream → MouseEvent; returns 1 on success ── */
int  mouse_parse_event(const char *buf, size_t len, MouseEvent *event);

/* ── single-click drag selection ── */
void mouse_handle_selection(Terminal *term, MouseEvent *event);

/* ── returns heap-allocated string of current selection (caller frees), or NULL ── */
char* mouse_get_selection(Terminal *term);

/* ── double-click word selection ──
 * Call after a left-press event.  Returns 1 if this press completed a
 * double-click and word-selection was performed; 0 otherwise.
 * double_click_ms = max gap in milliseconds between two clicks. */
int   mouse_handle_double_click(Terminal *term, MouseEvent *event, int double_click_ms);

/* ── returns heap-allocated string of the word under (x,y), or NULL ── */
char* mouse_get_word_at(Terminal *term, int x, int y);

#endif /* MOUSE_H */
