#ifndef SEARCH_H
#define SEARCH_H

#include "terminal.h"
#include "scrollback.h"

#define MAX_SEARCH_QUERY 256
#define MAX_SEARCH_MATCHES 4096

/* Each match records where it lives:
 *   line >= 0                → index into scrollback
 *   line == -1               → visible terminal (col/row relative to terminal buffer)
 * For simplicity we store absolute scrollback index for everything.
 * Visible-terminal lines are mapped to (scrollback.num_lines + terminal_row). */
typedef struct {
    int line;   /* scrollback absolute index, or sb.num_lines + term_row for visible */
    int col;
} SearchMatch;

typedef struct {
    char          query[MAX_SEARCH_QUERY];
    int           active;
    int           current_match;
    int           num_matches;
    SearchMatch   matches[MAX_SEARCH_MATCHES];
    int           case_sensitive;
} Search;

void search_init(Search *search);
void search_free(Search *search);
void search_start(Search *search, const char *query, int case_sensitive);

/* Re-scan scrollback + visible buffer for all occurrences */
void search_execute(Search *search, Terminal *term, Scrollback *sb);

void search_next(Search *search);
void search_prev(Search *search);

/* Highlight matches that fall within the currently visible terminal buffer.
 * sb_visible_start = first scrollback line currently rendered (0 when live). */
void search_highlight(Search *search, Terminal *term, int term_height,
                      Scrollback *sb, int sb_visible_start);

/* Jump scrollback so current match is visible; returns 1 if jump happened */
int  search_jump_to_current(Search *search, Scrollback *sb, int visible_height);

void search_stop(Search *search);

#endif /* SEARCH_H */
