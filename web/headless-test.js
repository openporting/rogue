/* headless-test.js — drive the WASM Rogue engine in Node (no browser) and
 * assert that the i18n message hook emits Korean. This is the verifiable
 * check for the localization work: it builds nothing, it just loads the
 * already-built web/rogue.js with a stub RogueBridge and feeds a key script.
 *
 *   prereq:  bash build.sh           (needs emscripten active)
 *   run:     node web/headless-test.js
 *
 * Default script: '>' and '<' off the stairs (deterministic "no way down/up"
 * messages) then 'Q','y' to quit — all three are in the i18n table, so we can
 * assert exact Korean output end-to-end through the real wasm.
 */
const path = require("path");

const captured = [];
const keyScript = (process.argv.slice(2).join("") || ">< Qy")
  .replace(/\s+/g, "")
  .split("");
let pops = 0;

globalThis.RogueBridge = {
  drawCell() {},
  refresh() {},
  clearScreen() {},
  msg(korean) {
    captured.push(korean);
    console.log("[MSG] " + korean);
  },
  popKey() {
    if (keyScript.length) return keyScript.shift().charCodeAt(0);
    if (++pops > 500) return "y".charCodeAt(0); // safety: force quit-confirm
    return -1; // queue empty -> C side yields via Asyncify
  },
};

process.env.HOME = process.env.HOME || "/tmp";
require(path.join(__dirname, "rogue.js"));

setTimeout(() => {
  const expected = [
    "내려가는 길이 보이지 않는다",
    "올라가는 길이 보이지 않는다",
    "정말 종료하겠는가?",
  ];
  const ok = expected.every((e) => captured.includes(e));
  console.log("\n=== captured ===");
  captured.forEach((m) => console.log(" • " + m));
  if (ok) {
    console.log("\nPASS: i18n hook emits Korean through the wasm build");
    process.exit(0);
  } else {
    console.log("\nFAIL: expected Korean lines not all present");
    process.exit(1);
  }
}, 4000);
