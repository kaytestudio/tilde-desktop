#include "session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

/* ─── mkdir -p ───────────────────────────────────────────────────────── */

static int mkdir_p(const char *path) {
    char buf[512];
    strncpy(buf, path, 511); buf[511] = '\0';

    char *p = buf;
    if (*p == '/') p++;

    while (*p) {
        char *slash = strchr(p, '/');
        if (slash) *slash = '\0';

        if (mkdir(buf, 0755) != 0 && errno != EEXIST)
            return -1;

        if (slash) { *slash = '/'; p = slash + 1; }
        else        break;
    }
    return 0;
}

int session_ensure_dir(const char *dir) {
    return mkdir_p(dir);
}

/* ─── save ───────────────────────────────────────────────────────────── */

int session_save(const Session *sess, const char *dir, const char *name) {
    if (session_ensure_dir(dir) != 0) return -1;

    char filename[512];
    if (name) {
        snprintf(filename, sizeof(filename), "%s/%s.zsess", dir, name);
    } else {
        /* auto-name from timestamp */
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        char stamp[32];
        strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", tm);
        snprintf(filename, sizeof(filename), "%s/%s.zsess", dir, stamp);
    }

    FILE *f = fopen(filename, "w");
    if (!f) return -1;

    fprintf(f, "# Z-Terminal session file\n");
    fprintf(f, "version       = 1\n");
    fprintf(f, "num_tabs      = %d\n", sess->num_tabs);
    fprintf(f, "active_tab    = %d\n", sess->active_tab);
    fprintf(f, "\n");

    for (int i = 0; i < sess->num_tabs; i++) {
        fprintf(f, "[tab_%d]\n", i);
        fprintf(f, "title         = %s\n", sess->tabs[i].title);
        fprintf(f, "cwd           = %s\n", sess->tabs[i].cwd);
        fprintf(f, "\n");
    }

    fclose(f);
    return 0;
}

/* ─── load ───────────────────────────────────────────────────────────── */

int session_load(Session *sess, const char *dir, const char *name) {
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/%s.zsess", dir, name);

    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    memset(sess, 0, sizeof(Session));
    strncpy(sess->name, name, MAX_SESSION_NAME - 1);

    int current_tab = -1;
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        /* strip comments */
        char *c = strchr(line, '#');
        if (c && c == line) continue;  /* full-line comment */

        /* section header [tab_N] */
        if (line[0] == '[') {
            int idx = -1;
            sscanf(line, "[tab_%d]", &idx);
            if (idx >= 0 && idx < MAX_SESSION_TABS) {
                current_tab = idx;
                if (idx + 1 > sess->num_tabs)
                    sess->num_tabs = idx + 1;
            }
            continue;
        }

        char key[128] = {0}, value[256] = {0};
        if (sscanf(line, " %127[^=] = %255[^\n]", key, value) != 2) continue;

        /* trim */
        char *v = value;
        while (*v == ' ' || *v == '\t') v++;
        char *end = v + strlen(v) - 1;
        while (end > v && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
            { *end = '\0'; end--; }
        char *ke = key + strlen(key) - 1;
        while (ke > key && (*ke == ' ' || *ke == '\t')) { *ke = '\0'; ke--; }

        if (current_tab < 0) {
            /* top-level keys */
            if (!strcmp(key, "num_tabs"))   sess->num_tabs   = atoi(v);
            if (!strcmp(key, "active_tab")) sess->active_tab = atoi(v);
        } else {
            /* per-tab keys */
            if (!strcmp(key, "title"))
                strncpy(sess->tabs[current_tab].title, v, MAX_SESSION_TITLE - 1);
            if (!strcmp(key, "cwd"))
                strncpy(sess->tabs[current_tab].cwd, v, MAX_SESSION_CWD - 1);
        }
    }

    fclose(f);
    return 0;
}

/* ─── list ───────────────────────────────────────────────────────────── */

int session_list(const char *dir, char out[][MAX_SESSION_NAME], int max_count) {
    DIR *d = opendir(dir);
    if (!d) return 0;

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < max_count) {
        size_t len = strlen(ent->d_name);
        /* look for .zsess suffix */
        if (len > 6 && strcmp(ent->d_name + len - 6, ".zsess") == 0) {
            /* copy name without .zsess extension */
            strncpy(out[count], ent->d_name, MAX_SESSION_NAME - 1);
            out[count][len - 6] = '\0';   /* strip extension */
            count++;
        }
    }
    closedir(d);
    return count;
}

/* ─── get CWD of child process (macOS) ──────────────────────────────── */

int session_get_tab_cwd(int child_pid, char *cwd, size_t cwd_size) {
    /* macOS: use lsof to query the cwd of the child process */
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "lsof -d cwd -a -p %d 2>/dev/null | tail -1 | awk '{print $NF}'",
             child_pid);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) return -1;

    if (fgets(cwd, (int)cwd_size, pipe) == NULL) {
        pclose(pipe);
        /* fallback: try /proc (Linux) */
        snprintf(cmd, sizeof(cmd), "/proc/%d/cwd", child_pid);
        ssize_t n = readlink(cmd, cwd, cwd_size - 1);
        if (n > 0) { cwd[n] = '\0'; return 0; }
        /* last resort */
        strncpy(cwd, ".", cwd_size - 1);
        return -1;
    }

    pclose(pipe);

    /* strip trailing newline */
    size_t len = strlen(cwd);
    if (len > 0 && cwd[len - 1] == '\n') cwd[len - 1] = '\0';

    return 0;
}
