#ifndef SCROLLBACK_H
#define SCROLLBACK_H

#include "terminal.h"

#define MAX_SCROLLBACK_LINES 50000

typedef struct {
    Cell *lines;          /* flat array: lines * width */
    int   num_lines;
    int   max_lines;
    int   width;
    int   scroll_offset;  /* lines from the bottom (0 = live view) */
} Scrollback;

void  scrollback_init(Scrollback *sb, int max_lines, int width);
void  scrollback_add_line(Scrollback *sb, const Cell *line, int width);
void  scrollback_resize(Scrollback *sb, int new_width);

void  scrollback_scroll_up(Scrollback *sb, int lines);
void  scrollback_scroll_down(Scrollback *sb, int lines);
void  scrollback_scroll_to_bottom(Scrollback *sb);

/* jump so that `target_line` (0-based from top of scrollback) is visible */
void  scrollback_jump_to_line(Scrollback *sb, int target_line, int visible_height);

/* copy visible region into a Terminal buffer */
void  scrollback_render(Scrollback *sb, Terminal *term, int term_height);

/* get a pointer to raw cells for line index i (NULL if out of range) */
const Cell* scrollback_get_line(Scrollback *sb, int i);

void  scrollback_free(Scrollback *sb);

#endif /* SCROLLBACK_H */
