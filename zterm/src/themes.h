#ifndef THEMES_H
#define THEMES_H

#include "terminal.h"

#define MAX_THEME_NAME 32
#define NUM_THEME_COLORS 16

typedef struct {
    char       name[MAX_THEME_NAME];
    TermColor  colors[NUM_THEME_COLORS]; /* indices 0-15 mapped to theme palette */
    TermColor  fg_default;
    TermColor  bg_default;
    TermColor  cursor_color;
    TermColor  selection_bg;
    TermColor  search_current_bg;
    TermColor  search_other_bg;
} Theme;

/* built-in theme table */
extern const Theme themes[];
extern const int   num_themes;

const Theme* theme_find(const char *name);
void         theme_apply(const Theme *theme);

#endif /* THEMES_H */
