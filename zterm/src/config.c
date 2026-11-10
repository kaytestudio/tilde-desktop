#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* ─── defaults ───────────────────────────────────────────────────────── */

void config_init(Config *config) {
    /* Appearance */
    strncpy(config->font_name, "Monaco", MAX_FONT_NAME - 1);
    config->font_size  = 12;
    config->use_bold   = 1;
    config->use_italic = 0;

    /* Colors / Theme */
    strncpy(config->color_scheme, "default", MAX_COLOR_SCHEME_NAME - 1);
    strncpy(config->theme,        "default", MAX_THEME_NAME - 1);
    config->foreground_color = 7;
    config->background_color = 0;

    /* Behavior */
    const char *shell = getenv("SHELL");
    strncpy(config->shell, shell ? shell : "/bin/zsh", MAX_SHELL_PATH - 1);
    config->scrollback_lines   = 10000;
    config->tab_width          = 8;
    config->bell_enabled       = 1;

    /* Performance */
    config->refresh_rate_ms  = 10;
    config->double_click_ms  = 500;

    /* Features */
    config->mouse_enabled     = 1;
    config->clipboard_enabled = 1;
    config->search_enabled    = 1;

    /* Sessions */
    const char *home = getenv("HOME");
    if (home) snprintf(config->session_dir, MAX_SESSION_DIR - 1,
                       "%s/.zterminal/sessions", home);
    else      strncpy(config->session_dir, ".sessions", MAX_SESSION_DIR - 1);
    config->session_auto_save    = 1;
    config->session_auto_restore = 1;
}

/* ─── default path ───────────────────────────────────────────────────── */

void config_get_default_path(char *path, size_t size) {
    const char *home = getenv("HOME");
    if (home) snprintf(path, size, "%s/.zterminalrc", home);
    else      strncpy(path, ".zterminalrc", size - 1);
}

/* ─── load ───────────────────────────────────────────────────────────── */

int config_load(Config *config, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        /* strip comments */
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char key[128] = {0}, value[256] = {0};
        if (sscanf(line, " %127[^=] = %255[^\n]", key, value) != 2) continue;

        /* trim value */
        char *v = value;
        while (*v == ' ' || *v == '\t') v++;
        char *end = v + strlen(v) - 1;
        while (end > v && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
            { *end = '\0'; end--; }

        /* trim key trailing spaces */
        char *ke = key + strlen(key) - 1;
        while (ke > key && (*ke == ' ' || *ke == '\t')) { *ke = '\0'; ke--; }

        /* match */
        if      (!strcmp(key, "font_name"))              strncpy(config->font_name,        v, MAX_FONT_NAME - 1);
        else if (!strcmp(key, "font_size"))              config->font_size               = atoi(v);
        else if (!strcmp(key, "use_bold"))               config->use_bold                = atoi(v);
        else if (!strcmp(key, "use_italic"))             config->use_italic              = atoi(v);
        else if (!strcmp(key, "color_scheme"))           strncpy(config->color_scheme,    v, MAX_COLOR_SCHEME_NAME - 1);
        else if (!strcmp(key, "theme"))                  strncpy(config->theme,           v, MAX_THEME_NAME - 1);
        else if (!strcmp(key, "foreground_color"))       config->foreground_color        = atoi(v);
        else if (!strcmp(key, "background_color"))       config->background_color        = atoi(v);
        else if (!strcmp(key, "shell"))                  strncpy(config->shell,           v, MAX_SHELL_PATH - 1);
        else if (!strcmp(key, "scrollback_lines"))       config->scrollback_lines        = atoi(v);
        else if (!strcmp(key, "tab_width"))              config->tab_width               = atoi(v);
        else if (!strcmp(key, "bell_enabled"))           config->bell_enabled            = atoi(v);
        else if (!strcmp(key, "refresh_rate_ms"))        config->refresh_rate_ms         = atoi(v);
        else if (!strcmp(key, "double_click_ms"))        config->double_click_ms         = atoi(v);
        else if (!strcmp(key, "mouse_enabled"))          config->mouse_enabled           = atoi(v);
        else if (!strcmp(key, "clipboard_enabled"))      config->clipboard_enabled       = atoi(v);
        else if (!strcmp(key, "search_enabled"))         config->search_enabled          = atoi(v);
        else if (!strcmp(key, "session_dir"))            strncpy(config->session_dir,     v, MAX_SESSION_DIR - 1);
        else if (!strcmp(key, "session_auto_save"))      config->session_auto_save       = atoi(v);
        else if (!strcmp(key, "session_auto_restore"))   config->session_auto_restore    = atoi(v);
    }

    fclose(f);
    return 0;
}

/* ─── save ───────────────────────────────────────────────────────────── */

int config_save(Config *config, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f,
        "# Z-Terminal Configuration File\n"
        "# Automatically loaded from ~/.zterminalrc\n"
        "# Hot-reload: changes take effect without restarting.\n\n"

        "# ─── Appearance ─────────────────────────────\n"
        "font_name             = %s\n"
        "font_size             = %d\n"
        "use_bold              = %d\n"
        "use_italic            = %d\n\n"

        "# ─── Colors / Theme ─────────────────────────\n"
        "# Available themes: default solarized-dark nord gruvbox monokai dracula\n"
        "theme                 = %s\n"
        "color_scheme          = %s\n"
        "foreground_color      = %d\n"
        "background_color      = %d\n\n"

        "# ─── Behavior ───────────────────────────────\n"
        "shell                 = %s\n"
        "scrollback_lines      = %d\n"
        "tab_width             = %d\n"
        "bell_enabled          = %d\n\n"

        "# ─── Performance ────────────────────────────\n"
        "refresh_rate_ms       = %d\n"
        "double_click_ms       = %d\n\n"

        "# ─── Features ───────────────────────────────\n"
        "mouse_enabled         = %d\n"
        "clipboard_enabled     = %d\n"
        "search_enabled        = %d\n\n"

        "# ─── Sessions ───────────────────────────────\n"
        "session_dir           = %s\n"
        "session_auto_save     = %d\n"
        "session_auto_restore  = %d\n",

        config->font_name, config->font_size, config->use_bold, config->use_italic,
        config->theme, config->color_scheme,
        config->foreground_color, config->background_color,
        config->shell, config->scrollback_lines, config->tab_width, config->bell_enabled,
        config->refresh_rate_ms, config->double_click_ms,
        config->mouse_enabled, config->clipboard_enabled, config->search_enabled,
        config->session_dir, config->session_auto_save, config->session_auto_restore
    );

    fclose(f);
    return 0;
}

/* ─── hot-reload helpers ─────────────────────────────────────────────── */

time_t config_get_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_mtime;
}

int config_reload_if_changed(Config *config, const char *path, time_t *last_mtime) {
    time_t mt = config_get_mtime(path);
    if (mt == 0 || mt == *last_mtime) return 0;   /* unchanged or missing */

    /* file changed — reload */
    config_init(config);                           /* reset to defaults first */
    if (config_load(config, path) != 0) return 0;  /* parse failed – keep defaults */

    *last_mtime = mt;
    return 1;   /* caller should re-apply theme etc. */
}
