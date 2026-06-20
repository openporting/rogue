/* web curses.h — drop-in replacement so Rogue compiles against our bridge
 * instead of ncurses. Put this dir first on the include path (-I webcurses).
 * Only declares the surface Rogue 5.4.4 actually uses (verified by grep);
 * extend here if the first emscripten compile reports a missing symbol.       */
#ifndef WEB_CURSES_H
#define WEB_CURSES_H

/* Real ncurses <curses.h> transitively provides these, and Rogue's headers
 * (rogue.h/extern.h) rely on it for FILE, bool and va_list. Mirror that. */
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#define ROGUE_COLS 80
#define ROGUE_LINES 25

typedef struct _web_win {
    int  cy, cx;                 /* cursor */
    int  maxy, maxx;
    char ch[ROGUE_LINES][ROGUE_COLS];
    unsigned char at[ROGUE_LINES][ROGUE_COLS];   /* attribute bits */
} WINDOW;

extern WINDOW *stdscr;
extern WINDOW *curscr;

/* curses globals Rogue references */
extern int LINES, COLS;

/* attributes */
#define A_STANDOUT 0x01
#define A_NORMAL   0x00
/* winch()/mvinch() return a bare char (no attr bits packed in), so the
 * character-extraction mask used by rogue.h's CCHAR() is the full byte. */
#define A_CHARTEXT 0xFF

/* keypad key codes (canonical ncurses values). Only the ones Rogue's mdport.c
 * references outside #ifdef guards are defined; leaving the shifted/LL/B1/EOL
 * variants undefined intentionally compiles out their guarded case labels. */
#define KEY_DOWN  0402
#define KEY_UP    0403
#define KEY_LEFT  0404
#define KEY_RIGHT 0405
#define KEY_HOME  0406
#define KEY_NPAGE 0522
#define KEY_PPAGE 0523
#define KEY_A1    0534
#define KEY_A3    0535
#define KEY_B2    0536
#define KEY_C1    0537
#define KEY_C3    0540
#define KEY_END   0550

/* termcap "clear to end of line" capability — none in the browser, so md_hasclreol() reports false */
#define CE ((char *)0)

#define OK 0
#define ERR (-1)
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* lifecycle */
WINDOW *initscr(void);
int     endwin(void);
int     isendwin(void);
WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x);
int     delwin(WINDOW *w);
int     touchwin(WINDOW *w);

/* cursor / output (stdscr + windowed variants) */
int move(int y, int x);
int wmove(WINDOW *w, int y, int x);
int addch(int ch);
int waddch(WINDOW *w, int ch);
int mvaddch(int y, int x, int ch);
int addstr(const char *s);
int waddstr(WINDOW *w, const char *s);
int mvaddstr(int y, int x, const char *s);
int mvwaddstr(WINDOW *w, int y, int x, const char *s);
int printw(const char *fmt, ...);
int mvprintw(int y, int x, const char *fmt, ...);
int wprintw(WINDOW *w, const char *fmt, ...);
int mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...);

/* screen control */
int clear(void);
int wclear(WINDOW *w);
int erase(void);
int clrtoeol(void);
int clrtobot(void);
int refresh(void);
int wrefresh(WINDOW *w);
int standout(void);
int standend(void);
int wstandout(WINDOW *w);
int wstandend(WINDOW *w);
int box(WINDOW *w, int verch, int horch);

int werase(WINDOW *w);
int wclrtoeol(WINDOW *w);
int mvwaddch(WINDOW *w, int y, int x, int ch);

/* extra window ops Rogue's overlay/menu code uses */
WINDOW *subwin(WINDOW *orig, int nlines, int ncols, int begin_y, int begin_x);
int mvwin(WINDOW *w, int y, int x);

/* input / query */
int getch(void);
int wgetch(WINDOW *w);
int wgetnstr(WINDOW *w, char *str, int n);
int mvinch(int y, int x);
int winch(WINDOW *w);
int mvwinch(WINDOW *w, int y, int x);
#define inch() winch(stdscr)

/* terminal-mode controls — no-ops in the browser (no real tty) */
int raw(void);
int noraw(void);
int cbreak(void);
int nocbreak(void);
int echo(void);
int noecho(void);
int nl(void);
int nonl(void);
int clearok(WINDOW *w, int bf);
int leaveok(WINDOW *w, int bf);
int idlok(WINDOW *w, int bf);
int keypad(WINDOW *w, int bf);
int baudrate(void);
int mvcur(int oldrow, int oldcol, int newrow, int newcol);
int erasechar(void);
int killchar(void);
int flushinp(void);
int halfdelay(int tenths);
char *unctrl(int ch);   /* printable rendering of a key, e.g. "^A" */

/* mdport.c's (unused) native getpass loop calls the Win32 console reader; route
 * it through our blocking key reader so the file still compiles. */
#define _getch getch

/* macros curses normally provides */
#define getyx(w, y, x)    ((y) = (w)->cy, (x) = (w)->cx)
#define getmaxyx(w, y, x) ((y) = (w)->maxy, (x) = (w)->maxx)

/* Rogue (main.c) pokes ncurses' internal cursor fields directly; alias them. */
#define _cury cy
#define _curx cx

int getmaxx(WINDOW *w);
int getmaxy(WINDOW *w);

#endif /* WEB_CURSES_H */
