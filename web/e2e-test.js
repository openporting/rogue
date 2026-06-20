/* e2e-test.js — END-TO-END test through the REAL compiled engine.
 *
 * Unlike headless-test.js (stub bridge) this loads the actual web/bridge.js and
 * drives web/rogue.js (the WASM build) exactly as the browser would: keys go in
 * via RogueBridge.pushKey, frames/messages come out via the real bridge. It
 * proves, on the shipping artifact, that:
 *
 *   1. opening the wield prompt ('w') is detected as an item prompt
 *      (RogueBridge.getPrompt() === "item") — the signal the touch UI uses to
 *      block stray map taps and pop the answer keypad;
 *   2. feeding the prompt TWO bad keys in a row (what a stray tap/swipe sends)
 *      does NOT wedge the game — each bogus key produces "올바른 항목이 아니다"
 *      and a fresh prompt, crossing two back-to-back --More-- pauses (the exact
 *      residue-latch that used to freeze equip and post-first-hit combat);
 *   3. a valid weapon letter then completes the wield ("…들었다");
 *   4. the engine stays live afterwards (quit confirm + a normal command).
 *
 *   prereq:  bash build.sh        (needs emscripten active)
 *   run:     node web/e2e-test.js
 *
 * ENVIRONMENT_IS_NODE wins over ENVIRONMENT_IS_WEB in the emscripten runtime, so
 * defining window (which bridge.js needs) does not disturb Node file loading.
 */
const path = require("path");

globalThis.window = {};                               // bridge.js attaches here
require(path.join(__dirname, "bridge.js"));
const B = globalThis.window.RogueBridge;
globalThis.RogueBridge = B;                           // EM_ASM uses a bare global

const msgs = [];
const origMsg = B.msg.bind(B);
B.msg = (s) => { msgs.push(s); origMsg(s); };
const since = (i, re) => msgs.slice(i).some((m) => re.test(m));
const newest = () => msgs[msgs.length - 1] || "";

let failures = 0;
const check = (name, cond) => {
  console.log((cond ? "  PASS  " : "  FAIL  ") + name);
  if (!cond) failures++;
};

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function waitFor(pred, label, timeout = 6000) {
  const t0 = Date.now();
  while (Date.now() - t0 < timeout) { if (pred()) return true; await sleep(20); }
  console.log("    (timeout waiting for: " + label + ")");
  return false;
}

process.env.HOME = process.env.HOME || "/tmp";
require(path.join(__dirname, "rogue.js"));            // boots main()

(async () => {
  // 1) boot: the dungeon (and @) must render.
  const booted = await waitFor(() => B.getPlayer() !== null, "map + @ to render", 10000);
  check("engine boots and draws the map", booted);

  // 2) open the wield prompt. Starting pack: a) food b) chain mail (worn)
  //    c) mace (in hand) d) short bow e) arrows — so get_item("wield", WEAPON)
  //    always has items to offer; 'd' is a clean wieldable weapon.
  B.pushKey("w");
  const gotPrompt = await waitFor(() => B.getPrompt() === "item", "wield item prompt");
  check("wield ('w') raises an item prompt", gotPrompt);
  check("prompt text is the Korean item prompt", /목록|for list/.test(newest()));

  // 3) two stray keys in a row — what a tap/swipe would inject. 'z' is not a
  //    weapon letter, so each is rejected and the prompt re-issued. The 2nd one
  //    only succeeds if the --More-- pager auto-advance recovered after the 1st
  //    (the bug froze here).
  let i = msgs.length;
  B.pushKey("z");
  const bad1 = await waitFor(() => since(i, /올바른 항목이 아니다/), "1st invalid-item rejection");
  check("1st stray key is rejected, not swallowed", bad1);
  const reprompt1 = await waitFor(() => B.getPrompt() === "item", "re-prompt after 1st stray");
  check("prompt is re-issued after 1st stray key", reprompt1);

  i = msgs.length;
  B.pushKey("z");
  const bad2 = await waitFor(() => since(i, /올바른 항목이 아니다/), "2nd invalid-item rejection");
  check("2nd stray key recovers (no --More-- freeze)", bad2);
  const reprompt2 = await waitFor(() => B.getPrompt() === "item", "re-prompt after 2nd stray");
  check("prompt still live after 2nd stray key", reprompt2);

  // 4) a valid weapon letter completes the wield.
  i = msgs.length;
  B.pushKey("d");                                     // the +1,+0 short bow
  const wielded = await waitFor(() => since(i, /들었다|wielding/), "wield to complete");
  check("valid letter completes the wield", wielded);
  check("prompt clears once answered", B.getPrompt() === null);

  // 5) liveness: the game still takes commands. Quit confirm proves 'Q' was
  //    processed; answer 'n' to stay, then a normal '>' off-stairs message
  //    proves the command loop kept running.
  i = msgs.length;
  B.pushKey("Q");
  const quitAsk = await waitFor(() => since(i, /정말 종료하겠는가/), "quit confirmation");
  check("engine is still live (quit confirm shown)", quitAsk);
  check("quit confirm detected as a yes/no prompt", B.getPrompt() === "yesno");
  B.pushKey("n");                                     // cancel quit

  i = msgs.length;
  B.pushKey(">");
  const descend = await waitFor(() => since(i, /내려가는 길이 보이지 않는다|way down/), "post-recovery command");
  check("a normal command works after the prompt round-trip", descend);

  console.log(failures === 0
    ? "\nPASS: equip prompt + --More-- recovery verified end-to-end on the WASM build"
    : `\nFAIL: ${failures} check(s) failed`);
  process.exit(failures === 0 ? 0 : 1);
})();
