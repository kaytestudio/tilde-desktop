#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <time.h>

#define MAX_FONT_NAME        64
#define MAX_SHELL_PATH       256
#define MAX_COLOR_SCHEME_NAME 32
#define MAX_THEME_NAME       32
#define MAX_SESSION_DIR      256

typedef struct {
    /* Appearance */
    char font_name[MAX_FONT_NAME];
    int  font_size;
    int  use_bold;
    int  use_italic;

    /* Colors / Theme */
    char color_scheme[MAX_COLOR_SCHEME_NAME];
    char theme[MAX_THEME_NAME];          /* NEW: theme preset name */
    int  foreground_color;
    int  background_color;

    /* Behavior */
    char shell[MAX_SHELL_PATH];
    int  scrollback_lines;
    int  tab_width;
    int  bell_enabled;

    /* Performance */
    int  refresh_rate_ms;
    int  double_click_ms;

    /* Features */
    int  mouse_enabled;
    int  clipboard_enabled;
    int  search_enabled;

    /* Sessions */
    char session_dir[MAX_SESSION_DIR];   /* NEW: directory for session files */
    int  session_auto_save;              /* NEW: 1 = auto-save on exit       */
    int  session_auto_restore;           /* NEW: 1 = restore on start        */
} Config;

void config_init(Config *config);
int  config_load(Config *config, const char *path);
int  config_save(Config *config, const char *path);
void config_get_default_path(char *path, size_t size);

/* hot-reload support */
time_t config_get_mtime(const char *path);          /* returns mtime or 0 */
int    config_reload_if_changed(Config *config,
                                const char *path,
                                time_t *last_mtime); /* returns 1 if reloaded */

#endif /* CONFIG_H */
