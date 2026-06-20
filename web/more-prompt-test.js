/* more-prompt-test.js — headless regression test for the --More-- pager
 * deadlock (equip/wield prompt hang AND the after-first-hit combat freeze).
 *
 * No browser, no wasm: it loads the REAL web/bridge.js and replays the exact
 * curses calls Rogue makes — pack.c get_item() looping over io.c endmsg(), and
 * a combat turn chaining messages through endmsg() (with web_curses.c's stdscr
 * shadow-diff in between) — to prove the touch bridge's auto-`--More--`
 * advancer keeps recovering instead of latching itself into a deadlock.
 *
 *   run:  node web/more-prompt-test.js
 *
 * The bug (fixed in webcurses/patches.sh Patch #2): the i18n hook dropped the
 * `mvaddstr(0,0,msgbuf)` whose side effect parked the cursor at row 0 col 0, so
 * endmsg's clrtoeol() no longer wiped the top line and a `--More--` prompt was
 * left as residue. RogueBridge.refresh() is edge-triggered on row-0 text, so the
 * stale `--More--` latched moreActive=true and the NEXT pager pause never got
 * its synthetic space — wait_for(' ') blocked forever. get_item (wield 'w',
 * wear 'W', drop, …) triggers it: it emits "is not a valid item" then re-prompts
 * back-to-back, landing on the wedged pager. The fix restores `move(0, 0)` so
 * row 0 is cleared every message and the edge re-arms.
 *
 * `fixed:false` models the pre-fix endmsg as a control: it must still hang, so
 * the test fails loudly if the scenario ever stops exercising the bug.
 */
const path = require("path");

const ROWS = 25, COLS = 80;

/* A faithful, minimal mirror of the C pipeline driving one bridge instance. */
function makeEngine(B, fixed) {
  let cur = Array.from({ length: ROWS }, () => Array(COLS).fill(" "));
  let shadow = Array.from({ length: ROWS }, () => Array(COLS).fill("\0")); // memset(shadow,0)
  let cy = 0, cx = 0;

  const move = (y, x) => { cy = y; cx = x; };
  const clrtoeol = () => { for (let x = cx; x < COLS; x++) cur[cy][x] = " "; };
  // web_curses.c writes one *byte* per cell, so a Korean string lands as its
  // UTF-8 bytes spread across cells (each stored as a single char-code).
  const putBytes = (s) => { for (const b of new TextEncoder().encode(s)) if (cx < COLS) cur[cy][cx++] = String.fromCharCode(b); };
  const mvaddstr = (y, x, s) => { move(y, x); putBytes(s); };
  const refresh = () => {                                  // web_curses.c wrefresh(stdscr)
    for (let y = 0; y < ROWS; y++)
      for (let x = 0; x < COLS; x++)
        if (cur[y][x] !== shadow[y][x]) {
          shadow[y][x] = cur[y][x];
          B.drawCell(y, x, cur[y][x].charCodeAt(0) & 0xff, 0);
        }
    B.refresh();
  };

  // input: user keys arrive via B.pushKey; the engine drains via B.popKey. When
  // the queue is empty the real engine Asyncify-sleeps; here we feed the next
  // scripted user action, and if none remain we declare a hang.
  const pending = [];
  let hang = false, idle = 0;
  const readchar = () => {
    for (;;) {
      const k = B.popKey();
      if (k >= 0) { idle = 0; return k; }
      if (pending.length) { pending.shift()(); continue; }
      if (++idle > 50) { hang = true; return -1; }
    }
  };

  // io.c message machinery (the patched endmsg).
  let mpos = 0, newpos = 0, msgbuf = "";
  const endmsg = () => {
    if (mpos) {                                            // chained message -> pager
      mvaddstr(0, mpos, "--계속--");                        // i18n'd "--More--" (tr_screen)
      refresh();
      let c; do { c = readchar(); if (hang) return; } while (c !== 32); // wait_for(' ')
    }
    B.msg(msgbuf);                                         // web_emit_msg(tr_msg(...))
    if (fixed) move(0, 0);                                 // <-- the fix
    clrtoeol();
    mpos = newpos; newpos = 0; msgbuf = "";
    refresh();
  };
  const doadd = (s) => { msgbuf += s; newpos = msgbuf.length; };
  const addmsg = (s) => doadd(s);
  const msg = (s) => {
    if (s === "") { move(0, 0); clrtoeol(); mpos = 0; return; }
    doadd(s); endmsg();
  };

  // pack.c get_item — only weapon 'a' is carried.
  const PACK = ["a"];
  const unctrl = (ch) => (ch < 0x20 ? "^" + String.fromCharCode(ch + 64)
    : ch < 0x80 ? String.fromCharCode(ch) : "M-");
  const getItem = (purpose) => {
    for (;;) {
      addmsg("which object do you want to "); addmsg(purpose); msg("? (* for list): ");
      if (hang) return null;
      const ch = readchar(); if (hang) return null;
      mpos = 0;
      if (ch === 27) { msg(""); return null; }            // ESC
      if (ch === 42) continue;                             // '*'
      if (PACK.includes(String.fromCharCode(ch))) return String.fromCharCode(ch);
      msg("'" + unctrl(ch) + "' is not a valid item");
      if (hang) return null;
    }
  };

  return {
    isHang: () => hang,
    pushUser: (codes) => codes.forEach((c) => pending.push(() => B.pushKey(c))),
    priorMessage: (s) => msg(s),                           // leaves mpos>0, like a real turn
    getItem,
    // A combat turn chains messages straight through endmsg (no msg("") between),
    // so each extra line is a --More-- pager the UI must auto-advance unattended.
    combatTurn: () => { msg("you hit the kobold"); msg("the kobold hits you"); },
  };
}

/* Run one wield scenario against a FRESH bridge (module-level moreActive state
 * must not leak between cases). */
function runScenario(fixed, taps) {
  delete require.cache[require.resolve(path.join(__dirname, "bridge.js"))];
  global.window = {};
  require(path.join(__dirname, "bridge.js"));
  const B = global.window.RogueBridge;

  const eng = makeEngine(B, fixed);
  eng.priorMessage("you found 12 gold pieces");            // prior-turn msg -> mpos>0
  eng.pushUser(taps);
  const result = eng.getItem("wield");
  return { result, hang: eng.isHang() };
}

let failures = 0;
const check = (name, cond) => {
  console.log((cond ? "  PASS  " : "  FAIL  ") + name);
  if (!cond) failures++;
};

// taps: the keys the player feeds the prompt. 108='l' (a stray move from tapping
// the map), 32=' ' (a tap while the map is blank), 97='a' (the real weapon).
const CASES = [
  { taps: [97],           label: "valid item straight away" },
  { taps: [108, 97],      label: "stray move 'l' then 'a'" },
  { taps: [32, 97],       label: "stray space then 'a'" },
  { taps: [108, 32, 97],  label: "move + space then 'a'" },
];

console.log("=== fixed build (shipping): wield prompt must always resolve ===");
for (const c of CASES) {
  const { result, hang } = runScenario(true, c.taps);
  check(`${c.label} -> wields 'a', no hang`, result === "a" && !hang);
}

// Same root cause, no prompt involved: fighting a monster chains messages, so
// every turn after the first raises a --More-- pager. With the bug the first
// turn's stale --More-- latches moreActive and turn 2 freezes ("can't move
// after the first hit"); the fix must let combat run indefinitely.
function runCombat(fixed, turns) {
  delete require.cache[require.resolve(path.join(__dirname, "bridge.js"))];
  global.window = {};
  require(path.join(__dirname, "bridge.js"));
  const eng = makeEngine(global.window.RogueBridge, fixed);
  let survived = 0;
  for (let i = 0; i < turns && !eng.isHang(); i++) { eng.combatTurn(); if (!eng.isHang()) survived++; }
  return { survived, hang: eng.isHang() };
}

console.log("\n=== fixed build (shipping): combat must never freeze ===");
{
  const { survived, hang } = runCombat(true, 5);
  check("5 attack turns of chained messages, no --More-- freeze", survived === 5 && !hang);
}

console.log("\n=== control (pre-fix endmsg): the bugs must still reproduce ===");
{
  const { hang } = runScenario(false, [108, 97]);          // invalid key first -> wedged pager
  check("stray move before valid item hangs without the fix", hang === true);
}
{
  const { survived, hang } = runCombat(false, 5);          // freezes after the first hit
  check("combat freezes after the first turn without the fix", hang === true && survived < 5);
}

// The touch UI blocks stray map taps by reading RogueBridge.getPrompt(); verify
// that contract directly. (The thin DOM handler is just `if getPrompt() is
// item/yesno -> open keypad, swallow the move`.)
function freshBridge() {
  delete require.cache[require.resolve(path.join(__dirname, "bridge.js"))];
  global.window = {};
  require(path.join(__dirname, "bridge.js"));
  return global.window.RogueBridge;
}
console.log("\n=== prompt signal the UI uses to block taps ===");
{
  const B = freshBridge();
  check("no prompt at rest", B.getPrompt() === null);
  B.msg("어느 것을 들까? (* = 목록): ");
  check("Korean item prompt detected", B.getPrompt() === "item");
  B.msg("'z'는 올바른 항목이 아니다");
  check("a normal message clears the prompt", B.getPrompt() === null);
  B.msg("Which object do you want to drop? (* for list): ");
  check("English item prompt detected (fallback)", B.getPrompt() === "item");
  B.pushKey("a");
  check("answering a prompt clears it (no wedge after ESC/answer)", B.getPrompt() === null);
  B.msg("정말 종료하겠는가?");
  check("quit confirm detected as yes/no", B.getPrompt() === "yesno");
}

if (failures === 0) {
  console.log("\nPASS: equip/wield prompt recovers from invalid input (no --More-- deadlock)");
  process.exit(0);
} else {
  console.log(`\nFAIL: ${failures} check(s) failed`);
  process.exit(1);
}
