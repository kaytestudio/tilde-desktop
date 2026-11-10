#ifndef SESSION_H
#define SESSION_H

#include <stddef.h>

#define MAX_SESSION_TABS   10
#define MAX_SESSION_TITLE  64
#define MAX_SESSION_CWD    512
#define MAX_SESSION_NAME   64

typedef struct {
    char title[MAX_SESSION_TITLE];
    char cwd[MAX_SESSION_CWD];
} SessionTab;

typedef struct {
    char        name[MAX_SESSION_NAME];
    int         num_tabs;
    int         active_tab;
    SessionTab  tabs[MAX_SESSION_TABS];
} Session;

/* Ensure the session directory exists (mkdir -p equivalent) */
int  session_ensure_dir(const char *dir);

/* Save the current session.  `dir` = directory, `name` = filename stem.
 * Pass name=NULL to auto-generate from timestamp. */
int  session_save(const Session *sess, const char *dir, const char *name);

/* Load a session by name.  Returns 0 on success. */
int  session_load(Session *sess, const char *dir, const char *name);

/* List available sessions (names only).  Caller must free each string.
 * Returns count; fills `out` (max `max_count` entries). */
int  session_list(const char *dir, char out[][MAX_SESSION_NAME], int max_count);

/* Get the CWD of a running tab's child process via /proc or macOS equivalent */
int  session_get_tab_cwd(int child_pid, char *cwd, size_t cwd_size);

#endif /* SESSION_H */
