/* ux-context-inventory-test.js — END-TO-END test of the contextual inventory.
 *
 * Verifies the new mobile UX (replacing the old 더보기 → 장착 → 목록 → 닫기 →
 * 키패드 dance) on the REAL engine through the REAL touch UI:
 *
 *   1. home    : a 🎒 소지품 button sits on the first screen (no 더보기 needed).
 *   2. list    : tapping it opens an INTERACTIVE inventory — each item is a row
 *                with a type tag, not a read-only dump.
 *   3. menu    : tapping an item opens a context menu whose actions match the
 *                item's type (무기→장착/던지기, 방어구→입기/벗기, 음식→먹기 …).
 *   4. wield   : 장착 on the bow runs `w`+letter end-to-end (engine wields it).
 *   5. armor   : 벗기 then 입기 round-trips the worn armor (T, then W+letter).
 *   6. eat     : 먹기 on the food runs `e`+letter.
 *   7. throw   : 던지기 on the arrows asks for a direction (engine wants it
 *                first), then AUTO-FEEDS the chosen arrow letter after the
 *                direction — the whole `t`→dir→item handshake from one tap+swipe.
 *
 *   prereq:  bash build.sh        (needs emscripten active — builds web/rogue.js)
 *   run:     node web/ux-context-inventory-test.js
 */
"use strict";
const { createHarness, bootEngine, sleep, waitFor } = require("./ux-dom-harness");

let failures = 0;
function check(name, cond) {
  console.log((cond ? "  PASS  " : "  FAIL  ") + name);
  if (!cond) failures++;
}
function section(t) { console.log("\n=== " + t + " ==="); }

const H = createHarness();
const { B, byId, click, findChild, pushed } = H;
const { actions, ovlScrim, ovlSheet, dpad } = H.els;
bootEngine();

const messages = () => B.getMessages();
const hasMsg = (re) => messages().some((m) => re.test(m));
const ovlOpen = () => ovlScrim.classList.contains("open");
const invBtn = () => findChild(actions, (b) => /소지품/.test(b.textContent));
const rows = () => ovlSheet.querySelectorAll(".inv-row");
const rowByName = (re) => rows().find((r) => re.test(r.textContent));
const actBtns = () => ovlSheet.querySelectorAll(".inv-acts button");
const actBtn = (label) => actBtns().find((b) => b.textContent === label);
const letterOf = (row) => (row.querySelectorAll(".ik")[0].textContent || "").replace(")", "").trim();

/* Open the inventory robustly. A command tapped in the brief window while the
 * previous turn is still flushing a multi-message `--More--` pause can be eaten
 * by that pause's `wait_for(' ')` before the bridge's auto-space resolves it —
 * a real finger is too slow to hit this, but the test taps instantly. So wait
 * for the engine to go idle, then retry the open if the key was swallowed. */
async function openInventory() {
  for (let attempt = 0; attempt < 5; attempt++) {
    await waitFor(() => B.getPlayer() !== null && !ovlOpen(), "engine idle before 소지품", 4000);
    await sleep(120);
    click(invBtn());
    if (await waitFor(ovlOpen, "inventory overlay", 2500)) return true;
  }
  return false;
}

(async () => {
  await waitFor(() => B.getPlayer() !== null, "engine boot", 12000);

  section("1. 🎒 소지품 button is on the home screen (no 더보기 needed)");
  check("a 소지품 action button exists on the first screen", !!invBtn());

  section("2. it opens an interactive inventory (tappable rows + type tags)");
  await openInventory();
  check("inventory overlay opened", ovlOpen());
  check("items render as interactive rows (not a flat dump)", rows().length >= 4);
  check("rows carry a Korean type tag (무기/방어구/음식/화살 …)",
    rows().some((r) => /무기|방어구|음식|화살|활/.test(r.querySelectorAll(".itag")[0].textContent)));
  check("the starting pack is listed in Korean",
    /갑옷|철퇴|화살|음식|단궁/.test(ovlSheet.textContent));

  section("3. tapping an item opens a type-aware context menu");
  {
    const bow = rowByName(/단궁/);
    check("the bow (단궁) row is present", !!bow);
    click(bow);
    await sleep(40);
    const labels = actBtns().map((b) => b.textContent);
    check("a weapon menu offers 장착", labels.includes("장착"));
    check("a weapon menu offers 던지기", labels.includes("던지기"));
    check("every menu offers 버리기", labels.includes("버리기"));
    check("a 뒤로 (back) affordance is present", labels.includes("← 뒤로"));
  }

  section("4. 장착 on the bow wields it on the real engine (w + letter)");
  {
    const before = pushed.length;
    click(actBtn("장착"));                    // → space (close inv) + 'w' + letter
    const wielded = await waitFor(() => hasMsg(/들었다|wielding/) && B.getPrompt() === null,
      "bow wielded");
    check("장착 wielded the bow end-to-end", wielded);
    check("the item letter was auto-fed (no manual keypad needed)",
      pushed.slice(before).some((k) => /^[a-z]$/.test(k)));
    check("the inventory overlay closed after the action", !ovlOpen());
  }

  section("5. 벗기 then 입기 round-trips the worn armor (T, then W + letter)");
  {
    await openInventory();
    const worn = rowByName(/갑옷.*\(착용 중\)/);
    check("the worn armor shows a (착용 중) marker", !!worn);
    click(worn);
    await sleep(40);
    check("a worn-armor menu offers 벗기 (not 입기)",
      actBtns().map((b) => b.textContent).includes("벗기"));
    click(actBtn("벗기"));
    const tookOff = await waitFor(() => hasMsg(/벗었다|took off|used to be/), "armor taken off");
    check("벗기 took the armor off", tookOff);

    await openInventory();
    const bare = rowByName(/갑옷/);
    click(bare);
    await sleep(40);
    check("the now-unworn armor menu offers 입기",
      actBtns().map((b) => b.textContent).includes("입기"));
    const before = pushed.length;
    click(actBtn("입기"));                     // → space + 'W' + letter
    const wore = await waitFor(() => hasMsg(/착용했다|now wearing/) && B.getPrompt() === null,
      "armor worn again");
    check("입기 put the armor back on", wore);
    check("the armor letter was auto-fed", pushed.slice(before).some((k) => /^[a-z]$/.test(k)));
  }

  section("6. 먹기 on the food runs e + letter");
  {
    await openInventory();
    const food = rowByName(/음식|식량/);
    check("the food row is present", !!food);
    click(food);
    await sleep(40);
    check("a food menu offers 먹기", actBtns().map((b) => b.textContent).includes("먹기"));
    click(actBtn("먹기"));
    const ate = await waitFor(() => B.getPrompt() === null && !ovlOpen(), "eat resolves");
    check("먹기 ran the eat command to completion", ate);
  }

  section("7. 던지기 asks for a direction, then auto-feeds the arrow (t→dir→item)");
  {
    const opened = await openInventory();
    check("inventory reopened for the throw scenario", opened);
    const arrows = rowByName(/화살/);
    check("the arrows (화살) row is present", !!arrows);
    if (!arrows) { console.log("FAIL: 화살 행을 찾지 못해 던지기 시나리오를 건너뜁니다"); failures++; }
    else {
    const arrowLetter = letterOf(arrows);
    click(arrows);
    await sleep(40);
    check("the arrows menu offers 던지기", actBtns().map((b) => b.textContent).includes("던지기"));

    click(actBtn("던지기"));                   // → space + 't'; engine wants a direction
    const wantsDir = await waitFor(() => hasMsg(/방향/) && !ovlOpen(), "engine asks for a direction");
    check("the engine asks for a throw direction first", wantsDir);
    check("the direction hint banner is shown", byId["throw-hint"].classList.contains("open"));

    const before = pushed.length;
    click(findChild(dpad, (b) => b.textContent === "→"));   // choose a direction (sends 'l')
    const fed = await waitFor(
      () => pushed.slice(before).includes(arrowLetter) && B.getPrompt() === null,
      "arrow letter auto-fed after the direction");
    check("the chosen arrow letter is auto-fed after the direction", fed);
    check("the throw resolved (prompt cleared, hint hidden)",
      B.getPrompt() === null && !byId["throw-hint"].classList.contains("open"));
    }
  }

  console.log("");
  if (failures === 0) {
    console.log("PASS: contextual inventory works end-to-end on the real engine");
    process.exit(0);
  } else {
    console.log("FAIL: " + failures + " contextual-inventory check(s) failed");
    process.exit(1);
  }
})();
