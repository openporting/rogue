/* web_curses.c — implements the curses surface as a virtual 80x25 screen and
 * forwards it to the JS touch UI. Also owns the input + message chokepoints.
 *
 *   build (web)    : emcc ... -I webcurses  (defines __EMSCRIPTEN__)
 *   build (native) : gcc -DWEBCURSES_DEMO web_curses.c -o demo   (for testing)
 *
 * Three seams (verified against Rogue 5.4.4 source):
 *   1. drawing  -> curses fns below      -> RogueBridge.drawCell / refresh
 *   2. input    -> md_readchar()         -> RogueBridge.popKey (touch queue)
 *   3. messages -> web_emit_msg()        -> RogueBridge.msg (Korean log)
 */
#include "curses.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define JS_CELL(y, x, ch, a)  EM_ASM({ RogueBridge.drawCell($0, $1, $2, $3); }, (y), (x), (ch), (a))
#define JS_REFRESH()          EM_ASM({ RogueBridge.refresh(); })
#define JS_CLEAR()            EM_ASM({ RogueBridge.clearScreen(); })
#define JS_MSG(s)             EM_ASM({ RogueBridge.msg(UTF8ToString($0)); }, (s))
#define JS_POPKEY()           EM_ASM_INT({ return RogueBridge.popKey(); })
#define JS_SLEEP(ms)          emscripten_sleep(ms)
/* overlay seam: inventory / help / options / detection windows (everything that
 * is NOT stdscr) get pushed as a text overlay the JS UI shows as a sheet. */
#define JS_OVL_BEGIN()        EM_ASM({ RogueBridge.overlayBegin(); })
#define JS_OVL_LINE(s)        EM_ASM({ RogueBridge.overlayLine(UTF8ToString($0)); }, (s))
#define JS_OVL_END()          EM_ASM({ RogueBridge.overlayEnd(); })
#else
/* native fallbacks so the file compiles & runs without emscripten */
static int  JS_POPKEY(void) { int c = getchar(); return c == EOF ? 'Q' : c; }
#define JS_CELL(y, x, ch, a)  ((void)0)
#define JS_REFRESH()          fflush(stdout)
#define JS_CLEAR()            ((void)0)
#define JS_MSG(s)             fprintf(stderr, "[msg] %s\n", (s))
#define JS_SLEEP(ms)          ((void)0)
#define JS_OVL_BEGIN()        fprintf(stderr, "[ovl ----\n")
#define JS_OVL_LINE(s)        fprintf(stderr, "[ovl] %s\n", (s))
#define JS_OVL_END()          fprintf(stderr, "[ovl ----]\n")
#endif

int LINES = ROGUE_LINES, COLS = ROGUE_COLS;

static WINDOW  _stdscr, _curscr;
WINDOW *stdscr = &_stdscr;
WINDOW *curscr = &_curscr;

static int cur_attr = A_NORMAL;                 /* standout state */
static char shadow[ROGUE_LINES][ROGUE_COLS];    /* last pushed frame, for diffing */

static void win_init(WINDOW *w) {
    w->cy = w->cx = 0;
    w->maxy = ROGUE_LINES; w->maxx = ROGUE_COLS;
    memset(w->ch, ' ', sizeof w->ch);
    memset(w->at, 0, sizeof w->at);
}

WINDOW *initscr(void) {
    win_init(&_stdscr);
    win_init(&_curscr);
    memset(shadow, 0, sizeof shadow);
    JS_CLEAR();
    return stdscr;
}
int endwin(void) { JS_REFRESH(); return OK; }
int isendwin(void) { return FALSE; }

/* Rogue keeps a long-lived scratch window `hw`, and the INV_OVER menu briefly
 * opens a second window (tw) plus a subwindow (sw = subwin(tw)) and copies
 * hw -> sw, then writes the prompt to tw and refreshes tw. `hw` and `tw` must be
 * distinct buffers (the copy reads hw, writes tw), so newwin allocates a fresh
 * sheet. But `sw` is a *subwindow of tw* — it must share tw's buffer so the
 * copied item lines and the prompt end up in the one window that gets refreshed;
 * since our windows are already full ROGUE_LINES x ROGUE_COLS sheets (origin args
 * ignored), subwin just returns the parent. delwin only ever frees tw, never sw,
 * so the shared pointer is safe. Size/origin args are ignored; writes stay
 * in-bounds and wrefresh(w != stdscr) is forwarded to the JS overlay sheet. */
WINDOW *newwin(int nl, int nc, int by, int bx) {
    (void)nl; (void)nc; (void)by; (void)bx;
    WINDOW *w = (WINDOW *)malloc(sizeof *w);
    if (w) win_init(w);
    return w;
}
WINDOW *subwin(WINDOW *orig, int nl, int nc, int by, int bx) {
    (void)nl; (void)nc; (void)by; (void)bx;
    return orig;   /* true subwindow: shares the parent's full-size buffer */
}
int delwin(WINDOW *w) {
    if (w && w != &_stdscr && w != &_curscr) free(w);
    return OK;
}
int mvwin(WINDOW *w, int y, int x) { (void)w; (void)y; (void)x; return OK; }
int touchwin(WINDOW *w){ (void)w; return OK; }
int getmaxx(WINDOW *w) { return w->maxx; }
int getmaxy(WINDOW *w) { return w->maxy; }

/* ---- cursor ---- */
int wmove(WINDOW *w, int y, int x) {
    if (y < 0 || y >= w->maxy || x < 0 || x >= w->maxx) return ERR;
    w->cy = y; w->cx = x; return OK;
}
int move(int y, int x) { return wmove(stdscr, y, x); }

/* ---- single char ---- */
int waddch(WINDOW *w, int ch) {
    if (w->cy < w->maxy && w->cx < w->maxx) {
        w->ch[w->cy][w->cx] = (char)ch;
        w->at[w->cy][w->cx] = (unsigned char)cur_attr;
        if (++w->cx >= w->maxx) { w->cx = 0; if (w->cy < w->maxy - 1) w->cy++; }
    }
    return OK;
}
int addch(int ch)                 { return waddch(stdscr, ch); }
int mvaddch(int y, int x, int ch) { return move(y, x) == OK ? addch(ch) : ERR; }

/* ---- strings ---- */
/* Screen text (help/death/prompts) bypasses the msg() path, so localize whole
 * strings here. tr_screen() exact-matches known screen strings and passes
 * everything else (status line, Korean item names, the map) through. */
extern const char *tr_screen(const char *);
int waddstr(WINDOW *w, const char *s) { s = tr_screen(s); while (*s) waddch(w, (unsigned char)*s++); return OK; }
int addstr(const char *s)             { return waddstr(stdscr, s); }
int mvaddstr(int y, int x, const char *s)            { return move(y, x) == OK ? addstr(s) : ERR; }
int mvwaddstr(WINDOW *w, int y, int x, const char *s){ return wmove(w, y, x) == OK ? waddstr(w, s) : ERR; }

static int vw(WINDOW *w, const char *fmt, va_list ap) {
    char buf[256]; vsnprintf(buf, sizeof buf, fmt, ap); return waddstr(w, buf);
}
int printw(const char *fmt, ...)            { va_list a; va_start(a, fmt); int r = vw(stdscr, fmt, a); va_end(a); return r; }
int wprintw(WINDOW *w, const char *fmt, ...){ va_list a; va_start(a, fmt); int r = vw(w, fmt, a);      va_end(a); return r; }
int mvprintw(int y, int x, const char *fmt, ...) {
    if (move(y, x) != OK) return ERR;
    va_list a; va_start(a, fmt); int r = vw(stdscr, fmt, a); va_end(a); return r;
}
int mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...) {
    if (wmove(w, y, x) != OK) return ERR;
    va_list a; va_start(a, fmt); int r = vw(w, fmt, a); va_end(a); return r;
}

/* ---- attributes ---- */
int wstandout(WINDOW *w){ (void)w; cur_attr = A_STANDOUT; return OK; }
int wstandend(WINDOW *w){ (void)w; cur_attr = A_NORMAL;   return OK; }
int standout(void) { return wstandout(stdscr); }
int standend(void) { return wstandend(stdscr); }

/* ---- screen control ---- */
int wclear(WINDOW *w) { win_init(w); return OK; }
int clear(void)  { return wclear(stdscr); }
int erase(void)  { return wclear(stdscr); }
int clrtoeol(void) {
    for (int x = stdscr->cx; x < stdscr->maxx; x++) stdscr->ch[stdscr->cy][x] = ' ';
    return OK;
}
int clrtobot(void) {
    clrtoeol();
    for (int y = stdscr->cy + 1; y < stdscr->maxy; y++)
        for (int x = 0; x < stdscr->maxx; x++) stdscr->ch[y][x] = ' ';
    return OK;
}
int box(WINDOW *w, int v, int h) { (void)w; (void)v; (void)h; return OK; }

/* Forward a non-stdscr window (inventory list, help, options, magic detection)
 * to the JS overlay sheet: one trimmed text line per non-blank row. Cells hold
 * raw bytes, so Korean (UTF-8, kr_item) lines come across whole; nul/control
 * bytes (e.g. the help screen's tabs) collapse to spaces. */
static void push_overlay(WINDOW *w) {
    char line[ROGUE_COLS + 1];
    int lastrow = -1;
    for (int y = 0; y < w->maxy && y < ROGUE_LINES; y++)
        for (int x = 0; x < w->maxx && x < ROGUE_COLS; x++) {
            unsigned char c = (unsigned char)w->ch[y][x];
            if (c != ' ' && c != 0) { lastrow = y; break; }
        }
    if (lastrow < 0) return;                 /* nothing to show */
    JS_OVL_BEGIN();
    for (int y = 0; y <= lastrow; y++) {
        int last = -1;
        for (int x = 0; x < w->maxx && x < ROGUE_COLS; x++) {
            unsigned char c = (unsigned char)w->ch[y][x];
            if (c == 0 || c < 0x20) c = ' ';  /* nul / tab / other controls */
            line[x] = (char)c;
            if (c != ' ') last = x;
        }
        line[last + 1] = '\0';               /* right-trim trailing blanks */
        JS_OVL_LINE(line);
    }
    JS_OVL_END();
}

int wrefresh(WINDOW *w) {
    if (w == stdscr) {
        /* push only changed cells of stdscr to the touch UI, then commit a
         * frame. A stdscr refresh also dismisses any overlay (the game redrew). */
        for (int y = 0; y < ROGUE_LINES; y++)
            for (int x = 0; x < ROGUE_COLS; x++)
                if (w->ch[y][x] != shadow[y][x]) {
                    shadow[y][x] = w->ch[y][x];
                    JS_CELL(y, x, (int)(unsigned char)w->ch[y][x], (int)w->at[y][x]);
                }
        JS_REFRESH();
    } else if (w != curscr) {
        push_overlay(w);          /* inventory / help / options / detection sheet */
    } else {
        JS_REFRESH();             /* curscr = full-redraw request */
    }
    return OK;
}
int refresh(void) { return wrefresh(stdscr); }

int mvwaddch(WINDOW *w, int y, int x, int ch) { return wmove(w, y, x) == OK ? waddch(w, ch) : ERR; }

int werase(WINDOW *w) { return wclear(w); }
int wclrtoeol(WINDOW *w) {
    for (int x = w->cx; x < w->maxx; x++) w->ch[w->cy][x] = ' ';
    return OK;
}

/* ---- query ---- */
int winch(WINDOW *w) { return (unsigned char)w->ch[w->cy][w->cx]; }
int mvinch(int y, int x) { return move(y, x) == OK ? winch(stdscr) : ERR; }
int mvwinch(WINDOW *w, int y, int x) { return wmove(w, y, x) == OK ? winch(w) : ERR; }

/* ---- terminal modes: browser has no tty, so these are no-ops ---- */
int raw(void)      { return OK; }
int noraw(void)    { return OK; }
int cbreak(void)   { return OK; }
int nocbreak(void) { return OK; }
int echo(void)     { return OK; }
int noecho(void)   { return OK; }
int nl(void)       { return OK; }
int nonl(void)     { return OK; }
int clearok(WINDOW *w, int bf) { (void)w; (void)bf; return OK; }
int leaveok(WINDOW *w, int bf) { (void)w; (void)bf; return OK; }
int idlok(WINDOW *w, int bf)   { (void)w; (void)bf; return OK; }
int keypad(WINDOW *w, int bf)  { (void)w; (void)bf; return OK; }
int baudrate(void) { return 38400; }
int mvcur(int oy, int ox, int ny, int nx) { (void)oy; (void)ox; return move(ny, nx); }
int erasechar(void) { return '\b'; }      /* backspace */
int killchar(void)  { return 0x15; }      /* ^U */
int flushinp(void)  { return OK; }        /* input flush — no tty buffer here */
int halfdelay(int t) { (void)t; return OK; }  /* timed input mode — unused */

/* printable rendering of a key code, like ncurses unctrl(): control chars as
 * ^X, DEL as ^?, high-bit chars as M-x, everything else as itself. */
char *unctrl(int ch) {
    static char buf[5];
    unsigned char c = (unsigned char)ch;
    if (c < 0x20)        { buf[0] = '^'; buf[1] = (char)(c + '@'); buf[2] = '\0'; }
    else if (c == 0x7f)  { buf[0] = '^'; buf[1] = '?';             buf[2] = '\0'; }
    else if (c < 0x80)   { buf[0] = (char)c;                       buf[1] = '\0'; }
    else                 { buf[0] = 'M'; buf[1] = '-'; buf[2] = (char)(c & 0x7f); buf[3] = '\0'; }
    return buf;
}

/* ============================================================
 *  INPUT seam — replaces md_readchar() in mdport.c.
 *  Build mdport.c with -Dmd_readchar=__rogue_unused_readchar (or delete its
 *  body) so this definition wins. Touch UI feeds keys via RogueBridge.pushKey.
 * ============================================================ */
int md_readchar(void) {
    int k;
    while ((k = JS_POPKEY()) < 0) JS_SLEEP(16);  /* yield to browser via Asyncify */
    return k;
}
int getch(void)            { return md_readchar(); }
int wgetch(WINDOW *w)      { (void)w; return md_readchar(); }

/* line input: read keys until Enter, echoing into the window, honoring
 * backspace, capped at n-1 chars. Used by rip.c's "press return" / name entry. */
int wgetnstr(WINDOW *w, char *str, int n) {
    int i = 0, c;
    while ((c = md_readchar()) != '\n' && c != '\r') {
        if (c == '\b' || c == 0x7f) {            /* backspace / DEL */
            if (i > 0) {
                i--;
                if (w->cx > 0) { w->cx--; w->ch[w->cy][w->cx] = ' '; }
                refresh();
            }
            continue;
        }
        if (i < n - 1) {
            str[i++] = (char)c;
            waddch(w, c);
            refresh();
        }
    }
    str[i] = '\0';
    return OK;
}

/* ============================================================
 *  MESSAGE seam — call this from the END of endmsg() in io.c, passing the
 *  already-built (and translated) buffer, so messages render in our parchment
 *  log instead of Rogue's top line. See i18n note in the chat / README.
 * ============================================================ */
void web_emit_msg(const char *korean) { JS_MSG(korean); }

#ifdef WEBCURSES_DEMO
/* tiny native smoke test: draws @, prints a line, echoes one key */
int main(void) {
    initscr();
    mvaddch(3, 5, '@');
    mvaddstr(0, 0, "there is a dagger here");
    refresh();
    web_emit_msg("\xEC\x97\xAC\xEA\xB8\xB0 \xEB\x8B\xA8\xEA\xB2\x80\xEC\x9D\xB4 \xEC\x9E\x88\xEB\x8B\xA4."); /* 여기 단검이 있다. */

    /* overlay smoke test: emulate INV_OVER (newwin tw + subwin sw = tw), copy a
     * "pack listing" from hw into sw, write the prompt to tw, refresh tw. */
    {
        WINDOW *hw_demo = newwin(LINES, COLS, 0, 0);
        WINDOW *tw = newwin(LINES, COLS, 0, 0);
        WINDOW *sw = subwin(tw, LINES, COLS, 0, 0);
        wmove(hw_demo, 0, 0); waddstr(hw_demo, "a) +0 dagger");
        wmove(hw_demo, 1, 0); waddstr(hw_demo, "b) some food");
        for (int y = 0; y < 2; y++) { wmove(sw, y, 0); for (int x = 0; x < 12; x++) waddch(sw, mvwinch(hw_demo, y, x)); }
        wmove(tw, 2, 1); waddstr(tw, "--Press space to continue--");
        wrefresh(tw);   /* -> [ovl] lines on stderr; sw must alias tw */
        delwin(tw);
        delwin(hw_demo);
    }

    printf("\n[demo] @ at (5,3), key read = %c\n", (char)getch());
    endwin();
    return 0;
}
#endif
