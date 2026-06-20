#!/usr/bin/env bash
# patches.sh — apply the (idempotent) source patches Rogue needs for the Korean
# web port. rogue/ is cloned fresh and gitignored, so these run at build time
# rather than being committed into the engine tree. build.sh calls this.
#
# Usage: bash webcurses/patches.sh <path-to-rogue-src>
set -euo pipefail
SRC="${1:?usage: patches.sh <rogue-src-dir>}"

# --- Patch #2: message hook (i18n) ------------------------------------------
# At the end of endmsg(), msgbuf holds the fully assembled English line. Route
# it through tr_msg() (EN->KO, webcurses/i18n.c) into web_emit_msg() instead of
# drawing it on the curses top line. clrtoeol()/refresh() right after still run.
#
# We append `move(0, 0)` to the replacement on purpose. The original
# `mvaddstr(0, 0, msgbuf)` had a side effect endmsg relied on: it parked the
# cursor at column 0 of row 0, so the following clrtoeol() wiped the WHOLE top
# line. Drop that and clrtoeol() instead clears from wherever the cursor last
# sat — after endmsg's own `--More--` prompt the cursor is past it, so
# `--More--` is left as residue on row 0. The touch bridge's auto-`--More--`
# advancer (web/bridge.js RogueBridge.refresh) is edge-triggered on row-0 text;
# stale `--More--` latches its `moreActive` flag true forever, so the NEXT real
# `--More--` pause never gets its synthetic space and wait_for(' ') blocks. That
# is the equip/wield hang: get_item's loop emits "is not a valid item" then
# re-prompts back-to-back, hitting that wedged pager. Restoring move(0,0) keeps
# row 0 cleared each message so the edge re-arms.
IO="$SRC/io.c"
if ! grep -q 'web_emit_msg(tr_msg' "$IO"; then
  perl -0pi -e 's/#include "rogue\.h"/#include "rogue.h"\nextern const char *tr_msg(const char *);\nextern void web_emit_msg(const char *);/' "$IO"
  perl -0pi -e 's/\Qmvaddstr(0, 0, msgbuf);\E/web_emit_msg(tr_msg(msgbuf)); move(0, 0);/' "$IO"
  grep -q 'web_emit_msg(tr_msg' "$IO" || { echo "patch #2 FAILED ($IO)" >&2; exit 1; }
  echo "patched $IO (endmsg -> web_emit_msg(tr_msg(...)) + clear row 0)"
fi

# --- Patch #4: item names (Korean) ------------------------------------------
# inv_name() builds the English item string used in the inventory screen and in
# many messages. Wrap it: keep the original as inv_name_en() and have inv_name()
# pass its output through kr_item() (webcurses/i18n.c). Result is copied back
# into prbuf so the few callers that read prbuf directly still work.
TH="$SRC/things.c"
if ! grep -q 'inv_name_en' "$TH"; then
  perl -0pi -e 's/char \*\ninv_name\(THING \*obj, bool drop\)\n/char *\ninv_name(THING *obj, bool drop)\n{\n    extern char *kr_item(const char *);\n    extern char *inv_name_en(THING *obj, bool drop);\n    strcpy(prbuf, kr_item(inv_name_en(obj, drop)));\n    return prbuf;\n}\n\nchar *\ninv_name_en(THING *obj, bool drop)\n/' "$TH"
  grep -q 'inv_name_en' "$TH" || { echo "patch #4 FAILED ($TH)" >&2; exit 1; }
  echo "patched $TH (inv_name -> kr_item(inv_name_en(...)))"
fi
