/* web curses.h — drop-in replacement so Rogue compiles against our bridge
 * instead of ncurses. Put this dir first on the include path (-I webcurses).
 * Only declares the surface Rogue 5.4.4 actually uses (verified by grep);
 * extend here if the first emscripten compile reports a missing symbol.       */
#ifndef WEB_CURSES_H
#define WEB_CURSES_H

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

#define OK 0
#define ERR (-1)
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* lifecycle */
WINDOW *initscr(void);
int     endwin(void);
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

/* input / query */
int getch(void);
int wgetch(WINDOW *w);
int mvinch(int y, int x);
int winch(WINDOW *w);
#define inch() winch(stdscr)

/* macros curses normally provides */
#define getyx(w, y, x)    ((y) = (w)->cy, (x) = (w)->cx)
#define getmaxyx(w, y, x) ((y) = (w)->maxy, (x) = (w)->maxx)

#endif /* WEB_CURSES_H */
