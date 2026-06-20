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
# (2) message hook — at the end of endmsg() in io.c, after the final message
#     string `buf` (or `msgbuf`) is assembled, add:
#         extern void web_emit_msg(const char *);
#         web_emit_msg(tr_msg(msgbuf));     // tr_msg = EN->KO (see i18n.c)
#     and skip the curses message-line draw. This is also where the
#     fragment-concatenation refactor lands: collapse the piecewise
#     msg("there is ")+...+msg(" to pick up") sites into single keyed
#     format strings so Korean word order / 조사 can be applied:
#         "there is %s to pick up"  ->  key PICKUP_HERE -> "여기 %s이(가) 있다."
#
# (3) input — md_readchar in mdport.c is renamed away by the -D above, so the
#     bridge's md_readchar() (web_curses.c) wins. Also stub md_*tty/md_nosig
#     terminal calls in mdport.c to no-ops for the browser.
#
# Save/score files: Rogue writes to disk; under FORCE_FILESYSTEM mount IDBFS at
# the save dir and FS.syncfs() on save/quit to persist across sessions.
# =============================================================================
