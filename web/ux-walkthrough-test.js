/* ux-walkthrough-test.js — END-TO-END UX-friendliness walkthrough on the REAL
 * engine.
 *
 * e2e-test.js drives the actual WASM build but only at the bridge level; the
 * other headless tests stub the engine entirely. Neither exercises the touch UI
 * a finger actually pokes — the DOM handlers in web/index.html: tap-to-step,
 * swipe, the D-pad, the prompt keypad that auto-pops, the inventory bottom
 * sheet, the human-readable status bar, the sound toggle.
 *
 * This test wires the REAL web/index.html UI script to the REAL web/bridge.js
 * and the REAL web/rogue.js (the emscripten build), on a tiny self-contained DOM
 * shim (web/ux-dom-harness.js — no jsdom, this repo ships zero dependencies).
 * Keys go in by dispatching DOM events on the actual buttons/cells; frames,
 * messages, prompts and overlays come back out of the live engine through the
 * bridge and are rendered by the UI exactly as in a browser. Then it walks a
 * full session and asserts the experience stays friendly at every step:
 *
 *   1. boot    : a "로딩 중" affordance shows until the engine draws, then hides.
 *   2. status  : the engine's English/numeric status row is shown as a Korean
 *                status bar (depth "N층", HP bar, Lv/힘/✦) — values match getStats.
 *   3. log     : an action produces a Korean line, shown with its ▸ tick.
 *   4. tap     : tapping a tile sends the correct one-square step toward it.
 *   5. swipe   : a swipe is a cardinal move; a short drag stays a tap.
 *   6. d-pad   : the 8-way pad (incl. diagonals) maps to the right Rogue keys.
 *   7. prompt  : the engine's get_item prompt auto-pops the answer keypad, a
 *                stray map tap is SWALLOWED (not fed to the prompt), and the
 *                keypad answers it — verified against the live engine.
 *   8. forgive : a wrong letter is rejected and re-prompted with NO --More--
 *                freeze (the real pager auto-advances), then a good letter wins.
 *   9. overlay : the real inventory renders as a Korean bottom sheet; "계속"
 *                advances the engine and dismisses it.
 *  10. quit    : the engine's quit confirm is a yes/no; cancelling keeps playing.
 *  11. sound   : the 🔊 toggle flips + persists; first gesture unlocks audio.
 *  12. a11y    : lang=ko, no user-scaling, aria-labels, reduced-motion honoured.
 *
 *   prereq:  bash build.sh        (needs emscripten active — builds web/rogue.js)
 *   run:     node web/ux-walkthrough-test.js
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
const { B, byId, indexSrc, bootAtLoad, click, fireWindow, cellAt, findChild, pushed, vibrateLog, audio } = H;
const { sound, map, dpad, actions, scrim, ovlScrim, boot, log } = H.els;
bootEngine();

const messages = () => B.getMessages();
const hasMsg = (re) => messages().some((m) => re.test(m));

(async () => {
  section("1. boot affordance (real engine)");
  check("'로딩 중' shown before the first frame", bootAtLoad.display === "flex" && /로딩 중/.test(bootAtLoad.text));
  const booted = await waitFor(() => B.getPlayer() !== null, "engine to draw the map + @", 12000);
  check("engine boots and the map renders", booted);
  check("boot indicator hides once the map is drawn", boot.style.display === "none");
  const p0 = B.getPlayer();
  check("the player @ is on the map viewport",
    !!p0 && !!cellAt(p0.x, p0.y) && cellAt(p0.x, p0.y).firstChild.textContent === "@");

  section("2. human-readable status bar matches the engine");
  const st = B.getStats();
  check("engine status row parsed", !!st);
  if (st) {
    check("depth chip shows '" + st.depth + "층'", H.els.depth.textContent === st.depth + "층");
    check("HP text matches HP " + st.hp + "/" + st.maxhp, byId.hptext.textContent === "HP " + Math.max(0, st.hp) + "/" + st.maxhp);
    const ratioPct = Math.max(0, Math.min(1, st.maxhp ? st.hp / st.maxhp : 0)) * 100 + "%";
    check("HP bar width tracks the HP ratio", byId.hpfill.style.width === ratioPct);
    check("Lv / 힘 / ✦골드 painted from parsed stats",
      byId.lv.textContent === String(st.level) && byId.str.textContent === String(st.str) && byId.gold.textContent === String(st.gold));
  }

  section("3. an action yields a Korean log line (real engine round-trip)");
  // '<' off the entrance is a deterministic "no way up" message (in the i18n
  // table) — proves action button → engine → Korean log → UI render.
  click(findChild(actions, (b) => /계단↑/.test(b.textContent)));
  const gotLog = await waitFor(() => hasMsg(/올라가는 길이 보이지 않는다/), "Korean 'no way up' message");
  check("the engine emitted the Korean message", gotLog);
  check("the log shows it with a ▸ tick", /▸/.test(log.textContent) && /올라가는 길이 보이지 않는다/.test(log.textContent));

  section("4. tap a tile → the right one-square step toward it");
  {
    const p = B.getPlayer();
    const target = cellAt(p.x + 1, p.y);            // a tile due-right of @
    check("a tile right of @ is tappable", !!target);
    if (target) {
      const before = pushed.length;
      click(target, { target });
      check("tap sends the 'l' (right) step", pushed[before] === "l");
    }
    await waitFor(() => B.getPlayer() !== null, "engine to stay live after tap");
    check("engine still responsive after the tap", B.getPlayer() !== null);
  }

  section("5. swipe → cardinal move; short drag stays a tap");
  {
    const before = pushed.length;
    H.dispatch(map, "touchstart", { touches: [{ clientX: 100, clientY: 100 }] });
    H.dispatch(map, "touchend", { changedTouches: [{ clientX: 110, clientY: 104 }] }); // <28px
    check("a <28px drag is NOT a swipe", pushed.length === before);
    H.dispatch(map, "touchstart", { touches: [{ clientX: 200, clientY: 100 }] });
    H.dispatch(map, "touchend", { changedTouches: [{ clientX: 140, clientY: 96 }] });  // left
    check("leftward swipe sends the 'h' (left) step", pushed[before] === "h");
  }

  section("6. D-pad (incl. diagonals) maps to Rogue keys");
  {
    const up = findChild(dpad, (b) => b.textContent === "↗");
    check("↗ button exists with an aria-label", !!up && /이동/.test(up.getAttribute("aria-label")));
    if (up) {
      const before = pushed.length;
      click(up);
      check("↗ sends the 'u' (up-right) key", pushed[before] === "u");
    }
    await sleep(60);
  }

  section("7. item prompt: keypad auto-pops + stray taps are swallowed (live)");
  {
    click(findChild(actions, (b) => /더보기/.test(b.textContent)));
    click(findChild(byId.cmds, (b) => /장착/.test(b.textContent)));   // 'w' → get_item
    const gotPrompt = await waitFor(() => B.getPrompt() === "item", "wield item prompt");
    check("engine raises an item prompt", gotPrompt);
    check("the answer keypad auto-popped", scrim.classList.contains("open"));
    // the open sheet dims the log, so the prompt MUST be echoed inside the sheet
    // or the player is answering a question they can't read.
    check("the sheet echoes the prompt question (readable while answering)",
      /목록|들까|것을/.test(byId["sheet-msg"].textContent));

    const before = pushed.length, p = B.getPlayer();
    const stray = cellAt(p.x + 1, p.y) || map;
    click(stray, { target: stray });
    await sleep(80);
    check("a stray map tap during the prompt is swallowed (no key sent)", pushed.length === before);
    check("the keypad stays open for the real answer", scrim.classList.contains("open"));

    click(findChild(byId.letters, (b) => b.textContent === "d"));     // 'd' = short bow
    const wielded = await waitFor(() => hasMsg(/들었다|wielding/), "wield to complete");
    check("the keypad letter answers the prompt (wield completes)", wielded);
    check("the prompt clears once answered", B.getPrompt() === null);
  }

  section("8. forgiving: wrong letter rejected + re-prompted, NO --More-- freeze");
  {
    click(findChild(actions, (b) => /더보기/.test(b.textContent)));
    click(findChild(byId.cmds, (b) => /장착/.test(b.textContent)));
    await waitFor(() => B.getPrompt() === "item", "wield prompt again");
    click(findChild(byId.letters, (b) => b.textContent === "z"));     // not a weapon
    const rejected = await waitFor(() => hasMsg(/올바른 항목이 아니다/), "invalid-item rejection");
    check("a wrong letter is rejected ('올바른 항목이 아니다')", rejected);
    const reprompt = await waitFor(() => B.getPrompt() === "item", "re-prompt after stray (proves --More-- auto-advanced)");
    check("the prompt is re-issued — engine did NOT freeze on --More--", reprompt);
    // the rejection feedback must be visible in the sheet too (the log is dimmed).
    check("the sheet echoes the rejection feedback (so the player sees why)",
      /올바른 항목이 아니다/.test(byId["sheet-msg"].textContent));
    click(findChild(byId.letters, (b) => b.textContent === "d"));
    const ok = await waitFor(() => B.getPrompt() === null, "prompt clears after a valid letter");
    check("a valid letter still completes after the mistake", ok);
  }

  section("9. inventory overlay → Korean bottom sheet, 계속 dismisses");
  {
    click(findChild(actions, (b) => /더보기/.test(b.textContent)));
    click(findChild(byId.cmds, (b) => /소지품/.test(b.textContent)));  // 'i'
    const shown = await waitFor(() => ovlScrim.classList.contains("open"), "inventory overlay");
    check("inventory overlay is shown", shown);
    check("overlay lists the starting pack in Korean",
      /갑옷|철퇴|화살|식량|단궁/.test(byId["ovl-body"].textContent));
    check("the engine's own '--…--' prompt line is stripped from the sheet",
      !/^\s*--.*--\s*$/m.test(byId["ovl-body"].textContent));
    click(byId["ovl-go"]);   // "계속" → space
    const gone = await waitFor(() => !ovlScrim.classList.contains("open"), "overlay to dismiss");
    check("계속 advances the engine and dismisses the overlay", gone);
  }

  section("10. quit confirm is yes/no; cancelling keeps you playing");
  {
    fireWindow("keydown", { key: "Q" });   // physical keyboard pass-through
    const ask = await waitFor(() => B.getPrompt() === "yesno", "quit confirmation");
    check("'Q' raises a yes/no confirm", ask);
    check("the confirm pops the keypad too", scrim.classList.contains("open"));
    fireWindow("keydown", { key: "n" });   // cancel
    const alive = await waitFor(() => B.getPrompt() === null, "quit to be cancelled");
    check("cancelling clears the prompt (still alive)", alive);
    check("the engine still takes commands", B.getPlayer() !== null);
  }

  section("11. sound toggle flips + persists; gesture unlocks audio");
  {
    const before = audio.unlockCount;
    check("sound button starts un-muted (🔊)", sound.textContent === "🔊");
    click(sound);
    check("tapping 🔊 mutes (🔇) and marks the icon muted",
      sound.textContent === "🔇" && sound.classList.contains("muted") && audio.muted === true);
    check("muting went through the audio layer (unlock then toggle)", audio.unlockCount > before);
    click(sound);
    check("tapping again un-mutes", sound.textContent === "🔊" && audio.muted === false);
    check("interactions unlocked audio + buzzed haptics", audio.unlockCount > 0 && vibrateLog.length > 0);
  }

  section("12. accessibility & mobile-friendly markup (static)");
  {
    check("page declares Korean (lang=\"ko\")", /<html lang="ko">/.test(indexSrc));
    check("viewport disables zoom + fits the notch", /user-scalable=no/.test(indexSrc) && /viewport-fit=cover/.test(indexSrc));
    check("the sound toggle has an aria-label", /id="sound"[^>]*aria-label=/.test(indexSrc));
    check("every D-pad button gets an aria-label", /setAttribute\("aria-label"/.test(indexSrc));
    check("safe-area insets respected (home-bar padding)", /env\(safe-area-inset-bottom\)/.test(indexSrc));
    check("reduced-motion users get no torch/slide animation", /prefers-reduced-motion: reduce/.test(indexSrc));
    check("taps don't double as text-selection / 300ms delay", /touch-action:manipulation/.test(indexSrc));
  }

  console.log("");
  if (failures === 0) {
    console.log("PASS: the touch UI walkthrough is UX-friendly end-to-end on the real engine");
    process.exit(0);
  } else {
    console.log("FAIL: " + failures + " UX check(s) failed");
    process.exit(1);
  }
})();
