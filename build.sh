#!/usr/bin/env bash
# build.sh — compile Rogue 5.4.4 to WASM against the web curses bridge.
# Prereq: emscripten SDK active (`source /path/emsdk/emsdk_env.sh`).
#
# Layout assumed (this port repo is the parent; clone the engine into ./rogue):
#   rogue/        <- Davidslv/rogue source (.c, .h)   — clone here, see HANDOFF.md
#   webcurses/    <- curses.h, web_curses.c           (this bridge)
#   web/          <- bridge.js, index.html
set -euo pipefail

SRC=rogue
BRIDGE=webcurses
OUT=web

# All Rogue .c files EXCEPT mdport.c (compiled separately below). vers.c
# (version strings + encstr/statlist used by the save crypto) and wizard.c
# (whatis/create_obj/passwd, called from command.c) MUST be included or the
# link fails with undefined references.
ROGUE_C=$(ls $SRC/*.c | grep -vE '/mdport\.c$')

CFLAGS="-I$BRIDGE -I$SRC -DHAVE_CONFIG_H -O2"

# --- Patch #2: message hook (i18n) ------------------------------------------
# rogue/ is cloned fresh (gitignored), so the endmsg() hook is applied here at
# build time rather than committed. Idempotent: the guard skips a patched tree.
# At the end of endmsg(), msgbuf holds the fully assembled English line; route
# it through tr_msg() (EN->KO, webcurses/i18n.c) into web_emit_msg() instead of
# drawing it on the curses top line. clrtoeol()/refresh() right after still run,
# so row 0 is cleared. See webcurses/i18n.{c,h}.
IO=$SRC/io.c
if ! grep -q 'web_emit_msg(tr_msg' "$IO"; then
  perl -0pi -e 's/#include "rogue\.h"/#include "rogue.h"\nextern const char *tr_msg(const char *);\nextern void web_emit_msg(const char *);/' "$IO"
  perl -0pi -e 's/\Qmvaddstr(0, 0, msgbuf);\E/web_emit_msg(tr_msg(msgbuf));/' "$IO"
  grep -q 'web_emit_msg(tr_msg' "$IO" || { echo "patch #2 FAILED to apply to $IO" >&2; exit 1; }
  echo "patched $IO (endmsg -> web_emit_msg(tr_msg(...)))"
fi

# mdport.c keeps every md_* function we need, but its native md_readchar() must
# NOT win over the bridge's. The -D renames ONLY mdport's definition; io.c (the
# sole caller) keeps calling the real md_readchar -> web_curses.c. Scoping the
# -D to this one file is essential: applying it globally would also rewrite
# io.c's *call* and bypass the bridge, and double-define the renamed symbol.
emcc -c $SRC/mdport.c $CFLAGS \
  -Dmd_readchar=__rogue_native_readchar_unused \
  -o $OUT/mdport.o

emcc \
  $ROGUE_C \
  $OUT/mdport.o \
  $BRIDGE/web_curses.c \
  $BRIDGE/i18n.c \
  $CFLAGS \
  -sASYNCIFY \
  -sASYNCIFY_STACK_SIZE=24576 \
  -sEMULATE_FUNCTION_POINTER_CASTS=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sFORCE_FILESYSTEM=1 \
  -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString \
  -sEXIT_RUNTIME=0 \
  -o $OUT/rogue.js

rm -f $OUT/mdport.o

echo "built -> $OUT/rogue.js (+ rogue.wasm)"

# =============================================================================
# THREE SOURCE PATCHES (apply once, in rogue/):
#
# (1) curses include — none needed: -I port/webcurses puts our curses.h first,
#     so every `#include <curses.h>` resolves to the bridge automatically.
#
# (2) message hook — APPLIED AUTOMATICALLY above (search "Patch #2"). Replaces
#     endmsg()'s `mvaddstr(0, 0, msgbuf)` with `web_emit_msg(tr_msg(msgbuf))`,
#     so the assembled line is translated (webcurses/i18n.c) and pushed to our
#     parchment log instead of the curses top line. Static messages are in the
#     i18n.c table; unmatched lines pass through (English) for now.
#     NEXT WAVE: the fragment-concatenation refactor — collapse the piecewise
#     msg("there is ")+...+msg(" to pick up") sites into single keyed format
#     strings so Korean word order / 조사 (i18n.c kr_* helpers) can be applied:
#         "there is %s to pick up"  ->  key PICKUP_HERE -> "여기 %s이(가) 있다."
#
# (3) input — md_readchar in mdport.c is renamed away by the -D above, so the
#     bridge's md_readchar() (web_curses.c) wins. Also stub md_*tty/md_nosig
#     terminal calls in mdport.c to no-ops for the browser.
#
# Save/score files: Rogue writes to disk; under FORCE_FILESYSTEM mount IDBFS at
# the save dir and FS.syncfs() on save/quit to persist across sessions.
# =============================================================================
