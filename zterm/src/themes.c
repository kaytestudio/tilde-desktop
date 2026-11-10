#include "themes.h"
#include <string.h>
#include <stdio.h>

/* ─── helper macro ── rgb triple → TermColor literal ─────────────────── */
#define RGB(r,g,b) {2, 0, (r), (g), (b)}
#define DEF()      (TermColor){ .mode=0, .index=0, .r=0,   .g=0,   .b=0   }

/* ─── Default (xterm-256color style) ─────────────────────────────────── */
static const Theme theme_default = {
    .name = "default",
    .colors = {
        RGB(0,0,0),       RGB(205,0,0),     RGB(0,205,0),     RGB(205,205,0),
        RGB(0,0,238),     RGB(205,0,205),   RGB(0,205,205),   RGB(229,229,229),
        RGB(127,127,127), RGB(255,0,0),     RGB(0,255,0),     RGB(255,255,0),
        RGB(92,92,255),   RGB(255,0,255),   RGB(0,255,255),   RGB(255,255,255)
    },
    .fg_default        = RGB(229,229,229),
    .bg_default        = RGB(0,0,0),
    .cursor_color      = RGB(229,229,229),
    .selection_bg      = RGB(0,102,204),
    .search_current_bg = RGB(255,255,0),
    .search_other_bg   = RGB(0,255,255)
};

/* ─── Solarized Dark ─────────────────────────────────────────────────── */
static const Theme theme_solarized = {
    .name = "solarized-dark",
    .colors = {
        RGB(0,43,54),     RGB(220,50,47),   RGB(133,153,0),   RGB(181,137,0),
        RGB(38,139,210),  RGB(211,54,153),  RGB(42,161,152),  RGB(131,148,150),
        RGB(88,110,117),  RGB(220,50,47),   RGB(133,153,0),   RGB(181,137,0),
        RGB(38,139,210),  RGB(211,54,153),  RGB(42,161,152),  RGB(253,246,227)
    },
    .fg_default        = RGB(131,148,150),
    .bg_default        = RGB(0,43,54),
    .cursor_color      = RGB(131,148,150),
    .selection_bg      = RGB(38,139,210),
    .search_current_bg = RGB(181,137,0),
    .search_other_bg   = RGB(42,161,152)
};

/* ─── Nord ───────────────────────────────────────────────────────────── */
static const Theme theme_nord = {
    .name = "nord",
    .colors = {
        RGB(46,26,38),    RGB(191,80,80),   RGB(163,204,160), RGB(235,189,78),
        RGB(129,161,193), RGB(180,142,173), RGB(128,203,196), RGB(216,222,233),
        RGB(76,86,96),    RGB(242,119,119), RGB(166,226,46),  RGB(255,255,102),
        RGB(129,182,245), RGB(188,141,238), RGB(130,224,215), RGB(229,233,240)
    },
    .fg_default        = RGB(216,222,233),
    .bg_default        = RGB(46,26,38),
    .cursor_color      = RGB(216,222,233),
    .selection_bg      = RGB(129,161,193),
    .search_current_bg = RGB(235,189,78),
    .search_other_bg   = RGB(128,203,196)
};

/* ─── Gruvbox ────────────────────────────────────────────────────────── */
static const Theme theme_gruvbox = {
    .name = "gruvbox",
    .colors = {
        RGB(40,40,40),    RGB(204,85,85),   RGB(152,168,75),  RGB(215,166,57),
        RGB(81,151,199),  RGB(211,134,187), RGB(112,185,174), RGB(235,219,178),
        RGB(128,128,128),RGB(251,121,93),   RGB(184,187,38),  RGB(250,189,0),
        RGB(129,181,203), RGB(211,134,156), RGB(152,157,88),  RGB(253,246,227)
    },
    .fg_default        = RGB(235,219,178),
    .bg_default        = RGB(40,40,40),
    .cursor_color      = RGB(235,219,178),
    .selection_bg      = RGB(81,151,199),
    .search_current_bg = RGB(215,166,57),
    .search_other_bg   = RGB(152,168,75)
};

/* ─── Monokai ────────────────────────────────────────────────────────── */
static const Theme theme_monokai = {
    .name = "monokai",
    .colors = {
        RGB(39,40,34),    RGB(249,38,114),  RGB(166,226,46),  RGB(230,219,116),
        RGB(102,217,239), RGB(174,129,255), RGB(166,226,46),  RGB(248,248,242),
        RGB(75,78,70),    RGB(249,38,114),  RGB(166,226,46),  RGB(230,219,116),
        RGB(102,217,239), RGB(174,129,255), RGB(102,217,239), RGB(248,248,242)
    },
    .fg_default        = RGB(248,248,242),
    .bg_default        = RGB(39,40,34),
    .cursor_color      = RGB(248,248,242),
    .selection_bg      = RGB(49,49,43),
    .search_current_bg = RGB(230,219,116),
    .search_other_bg   = RGB(166,226,46)
};

/* ─── Dracula ────────────────────────────────────────────────────────── */
static const Theme theme_dracula = {
    .name = "dracula",
    .colors = {
        RGB(40,42,54),    RGB(255,85,85),   RGB(80,250,123),  RGB(241,250,140),
        RGB(139,233,253), RGB(255,121,198), RGB(80,250,123),  RGB(248,248,242),
        RGB(98,114,164),  RGB(255,85,85),   RGB(80,250,123),  RGB(241,250,140),
        RGB(139,233,253), RGB(255,121,198), RGB(139,233,253), RGB(248,248,242)
    },
    .fg_default        = RGB(248,248,242),
    .bg_default        = RGB(40,42,54),
    .cursor_color      = RGB(248,248,242),
    .selection_bg      = RGB(98,114,164),
    .search_current_bg = RGB(241,250,140),
    .search_other_bg   = RGB(80,250,123)
};

/* ─── theme table ────────────────────────────────────────────────────── */

const Theme themes[] = {
    theme_default,
    theme_solarized,
    theme_nord,
    theme_gruvbox,
    theme_monokai,
    theme_dracula
};

const int num_themes = sizeof(themes) / sizeof(themes[0]);

/* ─── lookup ─────────────────────────────────────────────────────────── */

const Theme* theme_find(const char *name) {
    for (int i = 0; i < num_themes; i++) {
        if (strcmp(themes[i].name, name) == 0)
            return &themes[i];
    }
    return &themes[0]; /* fallback to default */
}

/* ─── apply via OSC escape sequences ─────────────────────────────────── */

void theme_apply(const Theme *theme) {
    /* OSC 4 ; index ; rgb ST — set palette colour */
    for (int i = 0; i < NUM_THEME_COLORS; i++) {
        printf("\033]4;%d;rgb:%02x/%02x/%02x\007",
               i,
               theme->colors[i].r,
               theme->colors[i].g,
               theme->colors[i].b);
    }
    /* OSC 10 — default foreground */
    printf("\033]10;rgb:%02x/%02x/%02x\007",
           theme->fg_default.r, theme->fg_default.g, theme->fg_default.b);
    /* OSC 11 — default background */
    printf("\033]11;rgb:%02x/%02x/%02x\007",
           theme->bg_default.r, theme->bg_default.g, theme->bg_default.b);
    /* OSC 12 — cursor colour */
    printf("\033]12;rgb:%02x/%02x/%02x\007",
           theme->cursor_color.r, theme->cursor_color.g, theme->cursor_color.b);
    fflush(stdout);
}
