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

# --- PATCH #2: message hook (idempotent; see notes at bottom) -----------------
# Once Rogue has assembled the full English message in msgbuf, endmsg() draws it
# on curses row 0. Reroute that to the Korean parchment log instead:
#   web_emit_msg(tr_msg(msgbuf))   (tr_msg = EN->KO, webcurses/i18n.c)
# The fragment-concat sites are left untouched — tr_msg matches the assembled
# sentence, so no source refactor is needed across the .c files.
if ! grep -q 'web_emit_msg' "$SRC/io.c"; then
  sed -i 's|#include "rogue.h"|#include "rogue.h"\n\n/* web port (patch #2): route assembled messages to the Korean log */\nextern void web_emit_msg(const char *);\nextern const char *tr_msg(const char *);|' "$SRC/io.c"
  sed -i 's|    mvaddstr(0, 0, msgbuf);|    web_emit_msg(tr_msg(msgbuf));|' "$SRC/io.c"
  echo "patched $SRC/io.c (endmsg message hook -> web_emit_msg/tr_msg)"
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
# (2) message hook — APPLIED AUTOMATICALLY above (idempotent sed on io.c). The
#     endmsg() top-line draw `mvaddstr(0, 0, msgbuf)` becomes
#         web_emit_msg(tr_msg(msgbuf));   // tr_msg = EN->KO, webcurses/i18n.c
#     No fragment-concat refactor was needed: tr_msg() matches the fully
#     assembled English sentence (exact table + prefix/suffix frames + the
#     fight.c verb-table combat rules) and recomposes Korean with the right 조사,
#     e.g. "there is a dagger to pick up" -> "여기 단검을 주울 수 있다."
#     Unmatched messages fall back to English, so the table can grow over time.
#
# (3) input — md_readchar in mdport.c is renamed away by the -D above, so the
#     bridge's md_readchar() (web_curses.c) wins. Also stub md_*tty/md_nosig
#     terminal calls in mdport.c to no-ops for the browser.
#
# Save/score files: Rogue writes to disk; under FORCE_FILESYSTEM mount IDBFS at
# the save dir and FS.syncfs() on save/quit to persist across sessions.
# =============================================================================
