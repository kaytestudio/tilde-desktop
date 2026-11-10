#include "scrollback.h"
#include <stdlib.h>
#include <string.h>

/* ─── init ───────────────────────────────────────────────────────────── */

void scrollback_init(Scrollback *sb, int max_lines, int width) {
    sb->max_lines     = max_lines > MAX_SCROLLBACK_LINES ? MAX_SCROLLBACK_LINES : max_lines;
    sb->num_lines     = 0;
    sb->width         = width;
    sb->scroll_offset = 0;
    sb->lines         = (Cell*)calloc((size_t)sb->max_lines * (size_t)width, sizeof(Cell));
}

/* ─── add ────────────────────────────────────────────────────────────── */

void scrollback_add_line(Scrollback *sb, const Cell *line, int width) {
    if (!sb->lines) return;

    /* ring: shift everything up when full */
    if (sb->num_lines >= sb->max_lines) {
        memmove(sb->lines, sb->lines + sb->width,
                (size_t)(sb->max_lines - 1) * (size_t)sb->width * sizeof(Cell));
        sb->num_lines = sb->max_lines - 1;
    }

    int copy_w = width < sb->width ? width : sb->width;
    Cell *dest = sb->lines + (size_t)sb->num_lines * (size_t)sb->width;
    memcpy(dest, line, (size_t)copy_w * sizeof(Cell));

    /* pad remainder */
    for (int i = copy_w; i < sb->width; i++) {
        dest[i].ch = ' ';
        dest[i].fg = termcolor_default();
        dest[i].bg = termcolor_default();
        dest[i].bold = dest[i].italic = dest[i].underline = 0;
        dest[i].inverse = dest[i].blink = 0;
    }
    sb->num_lines++;
}

/* ─── resize ─────────────────────────────────────────────────────────── */

void scrollback_resize(Scrollback *sb, int new_width) {
    if (new_width == sb->width || !sb->lines) return;

    Cell *new_lines = (Cell*)calloc((size_t)sb->max_lines * (size_t)new_width, sizeof(Cell));
    if (!new_lines) return;

    int cw = new_width < sb->width ? new_width : sb->width;
    for (int i = 0; i < sb->num_lines; i++) {
        memcpy(new_lines + (size_t)i * (size_t)new_width,
               sb->lines  + (size_t)i * (size_t)sb->width,
               (size_t)cw * sizeof(Cell));
    }

    free(sb->lines);
    sb->lines = new_lines;
    sb->width = new_width;
}

/* ─── scroll ─────────────────────────────────────────────────────────── */

void scrollback_scroll_up(Scrollback *sb, int lines) {
    sb->scroll_offset += lines;
    if (sb->scroll_offset > sb->num_lines)
        sb->scroll_offset = sb->num_lines;
}

void scrollback_scroll_down(Scrollback *sb, int lines) {
    sb->scroll_offset -= lines;
    if (sb->scroll_offset < 0)
        sb->scroll_offset = 0;
}

void scrollback_scroll_to_bottom(Scrollback *sb) {
    sb->scroll_offset = 0;
}

/* ─── jump ───────────────────────────────────────────────────────────── */
/* Make target_line (0-based from top of scrollback) visible on screen.
 * We set scroll_offset so that target_line appears near the top of the
 * visible window.  scroll_offset is "lines from the bottom". */

void scrollback_jump_to_line(Scrollback *sb, int target_line, int visible_height) {
    /* how many lines exist below target_line (inclusive of it) */
    int lines_below = sb->num_lines - target_line;
    /* we want target_line at row 0 → offset = lines_below */
    int offset = lines_below;
    /* but clamp: offset can't exceed total lines, and the visible window
       can show at most visible_height lines of scrollback */
    if (offset > sb->num_lines)
        offset = sb->num_lines;
    /* if target is near the bottom, don't scroll past live view */
    if (offset < visible_height && sb->num_lines >= visible_height)
        offset = visible_height;
    sb->scroll_offset = offset;
}

/* ─── render ─────────────────────────────────────────────────────────── */

void scrollback_render(Scrollback *sb, Terminal *term, int term_height) {
    if (sb->scroll_offset == 0 || !sb->lines) return;

    /* first scrollback line to show */
    int start = sb->num_lines - sb->scroll_offset;
    if (start < 0) start = 0;

    int to_show = term_height;
    if (start + to_show > sb->num_lines)
        to_show = sb->num_lines - start;

    int cw = sb->width < term->width ? sb->width : term->width;
    for (int i = 0; i < to_show; i++) {
        memcpy(term->buffer[i],
               sb->lines + (size_t)(start + i) * (size_t)sb->width,
               (size_t)cw * sizeof(Cell));
    }
}

/* ─── get_line ───────────────────────────────────────────────────────── */

const Cell* scrollback_get_line(Scrollback *sb, int i) {
    if (!sb->lines || i < 0 || i >= sb->num_lines) return NULL;
    return sb->lines + (size_t)i * (size_t)sb->width;
}

/* ─── free ───────────────────────────────────────────────────────────── */

void scrollback_free(Scrollback *sb) {
    if (sb->lines) { free(sb->lines); sb->lines = NULL; }
    sb->num_lines = 0;
}
