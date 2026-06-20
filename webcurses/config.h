/* config.h — hand-written build config for the Emscripten/WASM web port.
 *
 * Rogue normally generates this via ./configure. Under emscripten we can't run
 * the autoconf feature tests meaningfully (no tty, no fork, no term.h), so we
 * declare exactly the capability set the browser/musl target actually provides.
 * extern.h includes this when HAVE_CONFIG_H is defined (see build.sh).
 *
 * Found on -I webcurses, so #include "config.h" in extern.h resolves here
 * without touching the upstream rogue/ source tree.
 *
 * Deliberately NOT defined (unsupported under emscripten):
 *   HAVE_TERM_H / HAVE_NCURSES_TERM_H  -> avoids termcap tputs(SO/SE)
 *   HAVE_WORKING_FORK / HAVE_*SPAWNL   -> md_shellescape() becomes a no-op
 *   HAVE_GETLOADAVG / HAVE_NLIST       -> md_loadav() returns zeros
 *   HAVE_ESCDELAY                      -> no ncurses ESCDELAY global
 *   HAVE_GETPASS / HAVE_ARPA_INET_H / HAVE_SYS_UTSNAME
 */
#ifndef ROGUE_WEB_CONFIG_H
#define ROGUE_WEB_CONFIG_H

/* our drop-in curses shim supplies <curses.h> */
#define HAVE_CURSES_H 1

/* musl headers available under emscripten */
#define HAVE_UNISTD_H 1
#define HAVE_TERMIOS_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_PWD_H 1
#define HAVE_GETPWUID 1

/* erasechar()/killchar() are provided by the curses shim. Defining these keeps
 * mdport.c on the curses path and out of the undeclared `_tty` sgtty branch. */
#define HAVE_ERASECHAR 1
#define HAVE_KILLCHAR 1

#endif /* ROGUE_WEB_CONFIG_H */
