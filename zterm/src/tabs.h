#ifndef TABS_H
#define TABS_H

#include "terminal.h"
#include "pty.h"
#include "scrollback.h"
#include "search.h"
#include "session.h"
#include "serial.h"

#define MAX_TABS 10

typedef enum {
    TAB_TYPE_SHELL  = 0,
    TAB_TYPE_SERIAL = 1
} TabType;

typedef struct {
    TabType    type;          /* shell or serial                    */
    Terminal   terminal;
    PTY        pty;           /* only used if type == TAB_TYPE_SHELL */
    SerialPort serial;        /* only used if type == TAB_TYPE_SERIAL */
    Scrollback scrollback;
    Search     search;
    char       title[64];
    int        active;
} Tab;

typedef struct {
    Tab  tabs[MAX_TABS];
    int  num_tabs;
    int  current_tab;
} TabManager;

void  tabs_init(TabManager *tm);
int   tabs_create(TabManager *tm, const char *shell, int width, int height,
                  int scrollback_lines, const char *cwd);

/* Create a serial port tab.  `device` e.g. "/dev/cu.usbserial-0001" */
int   tabs_create_serial(TabManager *tm, const char *device, BaudRate baud,
                         int width, int height, int scrollback_lines);

int   tabs_close(TabManager *tm, int index);
void  tabs_switch(TabManager *tm, int index);
void  tabs_next(TabManager *tm);
void  tabs_prev(TabManager *tm);
Tab*  tabs_current(TabManager *tm);
void  tabs_render_bar(TabManager *tm);
void  tabs_resize_all(TabManager *tm, int width, int height);
void  tabs_update_titles(TabManager *tm);

int   tabs_save_session(TabManager *tm, const char *dir, const char *name);
int   tabs_restore_session(TabManager *tm, const char *shell,
                           int width, int height, int scrollback_lines,
                           const char *dir, const char *name);

void  tabs_cleanup(TabManager *tm);

#endif /* TABS_H */
