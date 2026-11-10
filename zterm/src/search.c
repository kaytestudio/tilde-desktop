#include "search.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─── init / free ────────────────────────────────────────────────────── */

void search_init(Search *search) {
    memset(search, 0, sizeof(Search));
}

void search_free(Search *search) {
    (void)search; /* no heap allocations anymore */
}

/* ─── start ──────────────────────────────────────────────────────────── */

void search_start(Search *search, const char *query, int case_sensitive) {
    strncpy(search->query, query, MAX_SEARCH_QUERY - 1);
    search->query[MAX_SEARCH_QUERY - 1] = '\0';
    search->case_sensitive = case_sensitive;
    search->active         = 1;
    search->current_match  = 0;
    search->num_matches    = 0;
}

/* ─── execute ────────────────────────────────────────────────────────── */

static int match_at(const Cell *row, int col, const char *query, int qlen, int case_sens) {
    for (int i = 0; i < qlen; i++) {
        char a = row[col + i].ch;
        char b = query[i];
        if (!case_sens) { a = (char)tolower((unsigned char)a);
                          b = (char)tolower((unsigned char)b); }
        if (a != b) return 0;
    }
    return 1;
}

void search_execute(Search *search, Terminal *term, Scrollback *sb) {
    if (!search->active || search->query[0] == '\0') return;

    search->num_matches = 0;
    int qlen = (int)strlen(search->query);

    /* 1. scan scrollback */
    if (sb && sb->lines) {
        for (int y = 0; y < sb->num_lines && search->num_matches < MAX_SEARCH_MATCHES; y++) {
            const Cell *row = scrollback_get_line(sb, y);
            if (!row) continue;
            for (int x = 0; x <= sb->width - qlen; x++) {
                if (match_at(row, x, search->query, qlen, search->case_sensitive)) {
                    search->matches[search->num_matches].line = y;
                    search->matches[search->num_matches].col  = x;
                    search->num_matches++;
                    if (search->num_matches >= MAX_SEARCH_MATCHES) goto done;
                }
            }
        }
    }

    /* 2. scan visible terminal (map row → sb->num_lines + row) */
    {
        int base = sb ? sb->num_lines : 0;
        for (int y = 0; y < term->height && search->num_matches < MAX_SEARCH_MATCHES; y++) {
            for (int x = 0; x <= term->width - qlen; x++) {
                if (match_at(term->buffer[y], x, search->query, qlen, search->case_sensitive)) {
                    search->matches[search->num_matches].line = base + y;
                    search->matches[search->num_matches].col  = x;
                    search->num_matches++;
                    if (search->num_matches >= MAX_SEARCH_MATCHES) goto done;
                }
            }
        }
    }

done:
    /* clamp current_match */
    if (search->num_matches == 0) search->current_match = 0;
    else if (search->current_match >= search->num_matches)
        search->current_match = search->num_matches - 1;
}

/* ─── next / prev ────────────────────────────────────────────────────── */

void search_next(Search *search) {
    if (search->num_matches > 0)
        search->current_match = (search->current_match + 1) % search->num_matches;
}

void search_prev(Search *search) {
    if (search->num_matches > 0)
        search->current_match = (search->current_match - 1 + search->num_matches) % search->num_matches;
}

/* ─── jump ───────────────────────────────────────────────────────────── */

int search_jump_to_current(Search *search, Scrollback *sb, int visible_height) {
    if (!search->active || search->num_matches == 0 || !sb) return 0;

    int abs_line = search->matches[search->current_match].line;
    int sb_base  = sb->num_lines;   /* lines >= sb_base are in the visible terminal */

    if (abs_line >= sb_base) {
        /* match is in the live visible buffer — scroll to bottom */
        scrollback_scroll_to_bottom(sb);
        return 1;
    }

    /* match is in scrollback — jump so it's visible */
    scrollback_jump_to_line(sb, abs_line, visible_height);
    return 1;
}

/* ─── highlight ──────────────────────────────────────────────────────── */

void search_highlight(Search *search, Terminal *term, int term_height,
                      Scrollback *sb, int sb_visible_start) {
    if (!search->active || search->num_matches == 0) return;

    int qlen   = (int)strlen(search->query);
    int sb_base = sb ? sb->num_lines : 0;

    /* what absolute-line range is currently displayed?
     * if scroll_offset > 0: lines [sb_visible_start .. sb_visible_start + term_height)
     * if scroll_offset == 0 (live): visible terminal rows map to [sb_base .. sb_base + term_height) */
    int vis_start, vis_end;
    if (sb && sb->scroll_offset > 0) {
        vis_start = sb_visible_start;
        vis_end   = sb_visible_start + term_height;
    } else {
        vis_start = sb_base;
        vis_end   = sb_base + term_height;
    }

    TermColor cur_bg  = termcolor_rgb(255, 255,   0);  /* yellow  – current */
    TermColor oth_bg  = termcolor_rgb(  0, 255, 255);  /* cyan    – others  */
    TermColor hlf     = termcolor_index(0);             /* black text */

    for (int i = 0; i < search->num_matches; i++) {
        int abs = search->matches[i].line;
        int col = search->matches[i].col;

        if (abs < vis_start || abs >= vis_end) continue;

        int row = abs - vis_start;   /* row in terminal buffer */
        if (row < 0 || row >= term_height) continue;

        TermColor bg = (i == search->current_match) ? cur_bg : oth_bg;

        for (int j = 0; j < qlen && col + j < term->width; j++) {
            term->buffer[row][col + j].bg = bg;
            term->buffer[row][col + j].fg = hlf;
        }
    }
}

/* ─── stop ───────────────────────────────────────────────────────────── */

void search_stop(Search *search) {
    search->active        = 0;
    search->num_matches   = 0;
    search->current_match = 0;
}
