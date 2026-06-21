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
 * shim (no jsdom — this repo ships zero dependencies). Keys go in by dispatching
 * DOM events on the actual buttons/cells; frames, messages, prompts and overlays
 * come back out of the live engine through the bridge and are rendered by the UI
 * exactly as in a browser. Then it walks a full session and asserts the
 * experience stays friendly at every step:
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
const fs = require("fs");
const path = require("path");

let failures = 0;
function check(name, cond) {
  console.log((cond ? "  PASS  " : "  FAIL  ") + name);
  if (!cond) failures++;
}
function section(t) { console.log("\n=== " + t + " ==="); }

/* ──────────────────────────────────────────────────────────────────────────
 * 1. A minimal DOM, just enough to host web/index.html's UI script.
 *    Supports: getElementById/createElement, appendChild, className/classList,
 *    style, dataset, textContent, innerHTML (parsed), querySelectorAll(2 forms),
 *    closest, setAttribute, and bubbling dispatch with stopPropagation.
 * ────────────────────────────────────────────────────────────────────────── */
function makeClassList(el) {
  const sync = () => { el._className = [...el._classSet].join(" "); };
  return {
    add(c) { el._classSet.add(c); sync(); },
    remove(c) { el._classSet.delete(c); sync(); },
    contains(c) { return el._classSet.has(c); },
    toggle(c, force) {
      const on = force === undefined ? !el._classSet.has(c) : force;
      on ? el._classSet.add(c) : el._classSet.delete(c); sync(); return on;
    },
  };
}

class El {
  constructor(tag) {
    this.tag = tag;
    this.nodeType = 1;
    this._text = "";
    this.style = {};
    this.dataset = {};
    this.attributes = {};
    this._listeners = {};
    this.parentNode = null;
    this.childNodes = [];
    this._className = "";
    this._classSet = new Set();
    this.classList = makeClassList(this);
    this.id = "";
  }
  get className() { return this._className; }
  set className(v) {
    this._className = v || "";
    this._classSet = new Set(this._className.split(/\s+/).filter(Boolean));
  }
  get children() { return this.childNodes.filter((n) => n.nodeType === 1); }
  get firstChild() { return this.childNodes[0] || null; }
  appendChild(child) { child.parentNode = this; this.childNodes.push(child); return child; }
  setAttribute(k, v) { this.attributes[k] = String(v); }
  getAttribute(k) { return Object.prototype.hasOwnProperty.call(this.attributes, k) ? this.attributes[k] : null; }
  addEventListener(type, fn) { (this._listeners[type] || (this._listeners[type] = [])).push(fn); }
  get textContent() {
    if (this.childNodes.length)
      return this.childNodes.map((n) => (n.nodeType === 3 ? n._text : n.textContent)).join("");
    return this._text;
  }
  set textContent(v) { this.childNodes = []; this._text = String(v); }
  set innerHTML(html) { this.childNodes = parseHTML(String(html), this); }
}
function textNode(t) { const n = new El("#text"); n.nodeType = 3; n._text = t; return n; }

/* tiny HTML fragment parser: handles <div>/<span> with class="…", <br>, text. */
function parseHTML(html, parent) {
  const root = new El("#frag");
  const stack = [root];
  const re = /<(\/?)([a-zA-Z0-9]+)([^>]*?)(\/?)>|([^<]+)/g;
  let m;
  while ((m = re.exec(html))) {
    const [, closing, tag, attrs, selfClose, text] = m;
    const top = stack[stack.length - 1];
    if (text != null) { top.appendChild(textNode(text)); continue; }
    if (closing) { if (stack.length > 1) stack.pop(); continue; }
    const el = new El(tag.toLowerCase());
    const cls = /class="([^"]*)"/.exec(attrs || "");
    if (cls) el.className = cls[1];
    top.appendChild(el);
    if (!selfClose && tag.toLowerCase() !== "br") stack.push(el);
  }
  const kids = root.childNodes;
  kids.forEach((k) => (k.parentNode = parent));
  return kids;
}

/* selector engine for the two forms index.html uses:
 *   ".msg span:last-child"   and   ".cell" (via closest). */
function parseSimple(sel) {
  const step = { tag: null, classes: [], last: false };
  (sel.match(/\.[\w-]+|:last-child|[\w-]+/g) || []).forEach((t) => {
    if (t === ":last-child") step.last = true;
    else if (t[0] === ".") step.classes.push(t.slice(1));
    else step.tag = t.toLowerCase();
  });
  return step;
}
function isLastElementChild(el) {
  const p = el.parentNode; if (!p) return false;
  const ch = p.children; return ch[ch.length - 1] === el;
}
function matchSimple(el, step) {
  if (el.nodeType !== 1) return false;
  if (step.tag && el.tag !== step.tag) return false;
  if (!step.classes.every((c) => el._classSet.has(c))) return false;
  if (step.last && !isLastElementChild(el)) return false;
  return true;
}
function descendants(node, out) {
  for (const c of node.childNodes) if (c.nodeType === 1) { out.push(c); descendants(c, out); }
  return out;
}
El.prototype.querySelectorAll = function (sel) {
  const compound = sel.trim().split(/\s+/).map(parseSimple);
  let ctx = [this];
  for (const step of compound) {
    const next = [];
    for (const n of ctx) for (const d of descendants(n, [])) if (matchSimple(d, step) && !next.includes(d)) next.push(d);
    ctx = next;
  }
  return ctx;
};
El.prototype.closest = function (sel) {
  const step = parseSimple(sel.replace(/^\s+|\s+$/g, ""));
  let el = this;
  while (el && el.nodeType === 1) { if (matchSimple(el, step)) return el; el = el.parentNode; }
  return null;
};

/* event dispatch with real bubbling (target → parents) + stopPropagation. */
function dispatch(target, type, extra) {
  const ev = Object.assign({ type, target, defaultPrevented: false }, extra || {});
  let stopped = false;
  ev.stopPropagation = () => { stopped = true; };
  ev.preventDefault = () => { ev.defaultPrevented = true; };
  let el = target;
  while (el) {
    ev.currentTarget = el;
    const ls = (el._listeners && el._listeners[type]) || [];
    for (const fn of ls.slice()) { fn(ev); if (stopped) break; }
    if (stopped) break;
    el = el.parentNode;
  }
  return ev;
}

/* document + the static markup index.html declares (only the bits the script
 * reaches for). Parent links mirror the real tree so bubbling/stopPropagation
 * behave (e.g. a letter tap must NOT bubble to the scrim and close the sheet). */
const byId = {};
const document = {
  getElementById: (id) => byId[id] || null,
  createElement: (tag) => new El(tag),
};
function mk(id, parent) {
  const el = new El("div");
  el.id = id;
  if (parent) parent.appendChild(el);
  byId[id] = el;
  return el;
}
const body = new El("body");
const app = mk("app", body), col = mk("col", app);
const header = new El("header"); col.appendChild(header);
const sound = mk("sound", header); const depth = mk("depth", header);
mk("hpfill", header); mk("hptext", header); mk("lv", header); mk("str", header); mk("gold", header);
const mapwrap = mk("mapwrap", col); const map = mk("map", mapwrap); const boot = mk("boot", mapwrap);
boot.textContent = "로딩 중… (rogue.js)";   // index.html's static markup text (render() only toggles its display)
const log = mk("log", col);
const controls = mk("controls", col); const dpad = mk("dpad", controls); const actions = mk("actions", controls);
// command bottom sheet
const scrim = mk("scrim", body); const sheet = mk("sheet", scrim);
mk("cmds", sheet); mk("letters", sheet);
// overlay (inventory/help) bottom sheet
const ovlScrim = mk("ovl-scrim", body); const ovlSheet = mk("ovl-sheet", ovlScrim);
mk("ovl-h", ovlSheet); mk("ovl-body", ovlSheet); mk("ovl-go", ovlSheet);
// the markup's onclick="event.stopPropagation()" on the sheets: a tap inside
// the sheet must not reach the scrim's close handler.
sheet.addEventListener("click", (e) => e.stopPropagation());
ovlSheet.addEventListener("click", (e) => e.stopPropagation());
void [depth, sound, boot, log, dpad, actions, header, mapwrap, controls];

/* window / navigator / a small RogueAudio stub (the real audio.js needs a Web
 * Audio context; here we only verify the *contract* index.html relies on). */
const vibrateLog = [];
const navigator = { vibrate: (ms) => { vibrateLog.push(ms); return true; } };
const audio = {
  unlockCount: 0, muted: false, depth: null, messages: [], bells: 0,
  unlock() { this.unlockCount++; },
  getState() { return { muted: this.muted }; },
  toggleMute() { this.muted = !this.muted; return this.muted; },
  setDepth(d) { this.depth = d; },
  onMessage(m) { this.messages.push(m); },
  bell() { this.bells++; },
};
const winListeners = {};
const window = {
  RogueAudio: audio,
  navigator,
  addEventListener(type, fn, opts) {
    (winListeners[type] || (winListeners[type] = [])).push({ fn, once: !!(opts && opts.once) });
  },
};
function fireWindow(type, extra) {
  const ev = Object.assign({ type, defaultPrevented: false, preventDefault() { this.defaultPrevented = true; } }, extra || {});
  const ls = winListeners[type] || [];
  for (const L of ls.slice()) L.fn(ev);
  winListeners[type] = ls.filter((L) => !L.once);
  return ev;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 2. Load the REAL bridge.js onto our window; expose RogueBridge as a bare
 *    global too (web_curses.c's EM_ASM calls it unqualified). Spy on the keys
 *    the UI sends so we can assert the touch→key contract.
 * ────────────────────────────────────────────────────────────────────────── */
global.window = window;
require(path.join(__dirname, "bridge.js")); // attaches window.RogueBridge / RogueInput
const B = window.RogueBridge;
const I = window.RogueInput;
global.RogueBridge = B;

const pushed = []; // every key the UI hands the engine (user intent, not auto-space)
const realPush = B.pushKey.bind(B);
B.pushKey = (code) => { pushed.push(typeof code === "string" ? code : String.fromCharCode(code)); realPush(code); };

/* ──────────────────────────────────────────────────────────────────────────
 * 3. Load the REAL index.html UI script onto the shim (it subscribes render()).
 * ────────────────────────────────────────────────────────────────────────── */
const indexSrc = fs.readFileSync(path.join(__dirname, "index.html"), "utf8");
const uiScript = (indexSrc.match(/<script>([\s\S]*?)<\/script>/g) || [])
  .map((s) => s.replace(/^<script>|<\/script>$/g, ""))
  .find((s) => /B\.subscribe\(render\)/.test(s));
if (!uiScript) { console.error("could not extract the index.html UI script"); process.exit(2); }
// eslint-disable-next-line no-new-func
new Function("window", "document", "navigator", uiScript)(window, document, navigator);

// The UI ran render() once at load against the still-empty bridge buffer, so the
// "로딩 중" affordance is up *now* — snapshot it before the engine draws its
// first frame (which happens synchronously while requiring rogue.js below).
const bootAtLoad = { display: boot.style.display, text: boot.textContent };

/* ──────────────────────────────────────────────────────────────────────────
 * 4. Boot the REAL engine (emscripten build). It drives the bridge, which the
 *    UI renders into our shim — exactly the browser data path.
 * ────────────────────────────────────────────────────────────────────────── */
const enginePath = path.join(__dirname, "rogue.js");
if (!fs.existsSync(enginePath)) {
  console.error("\nweb/rogue.js not found — run `bash build.sh` first (needs emscripten).");
  process.exit(2);
}
process.env.HOME = process.env.HOME || "/tmp";
require(enginePath); // boots main() (async via Asyncify)

/* ── async helpers (mirrors e2e-test.js) ── */
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function waitFor(pred, label, timeout = 8000) {
  const t0 = Date.now();
  while (Date.now() - t0 < timeout) { if (pred()) return true; await sleep(20); }
  console.log("    (timeout waiting for: " + label + ")");
  return false;
}
const click = (el, extra) => dispatch(el, "click", extra);
const findChild = (parent, pred) => parent.children.find(pred);
const cellAt = (bx, by) => map.children.find((c) => +c.dataset.bx === bx && +c.dataset.by === by);
const messages = () => B.getMessages();
const hasMsg = (re) => messages().some((m) => re.test(m));
const newest = () => messages()[0] || "";

/* ──────────────────────────────────────────────────────────────────────────
 * 5. THE WALKTHROUGH.
 * ────────────────────────────────────────────────────────────────────────── */
(async () => {
  section("1. boot affordance (real engine)");
  // captured at UI load, before the engine drew its first frame.
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
    check("depth chip shows '" + st.depth + "층'", depth.textContent === st.depth + "층");
    check("HP text matches HP " + st.hp + "/" + st.maxhp, byId.hptext.textContent === "HP " + Math.max(0, st.hp) + "/" + st.maxhp);
    const ratioPct = Math.max(0, Math.min(1, st.maxhp ? st.hp / st.maxhp : 0)) * 100 + "%";
    check("HP bar width tracks the HP ratio", byId.hpfill.style.width === ratioPct);
    check("Lv / 힘 / ✦골드 painted from parsed stats",
      byId.lv.textContent === String(st.level) && byId.str.textContent === String(st.str) && byId.gold.textContent === String(st.gold));
  }

  section("3. an action yields a Korean log line (real engine round-trip)");
  // '<' off the entrance is a deterministic "no way up" message (it's in the
  // i18n table) — proves action button → engine → Korean log → UI render.
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
    dispatch(map, "touchstart", { touches: [{ clientX: 100, clientY: 100 }] });
    dispatch(map, "touchend", { changedTouches: [{ clientX: 110, clientY: 104 }] }); // <28px
    check("a <28px drag is NOT a swipe", pushed.length === before);
    dispatch(map, "touchstart", { touches: [{ clientX: 200, clientY: 100 }] });
    dispatch(map, "touchend", { changedTouches: [{ clientX: 140, clientY: 96 }] });  // left
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
    let i = messages().length;
    click(findChild(byId.letters, (b) => b.textContent === "z"));     // not a weapon
    const rejected = await waitFor(() => messages().slice(0, messages().length - i + 1).some((m) => /올바른 항목이 아니다/.test(m)) || hasMsg(/올바른 항목이 아니다/), "invalid-item rejection");
    check("a wrong letter is rejected ('올바른 항목이 아니다')", rejected);
    const reprompt = await waitFor(() => B.getPrompt() === "item", "re-prompt after stray (proves --More-- auto-advanced)");
    check("the prompt is re-issued — engine did NOT freeze on --More--", reprompt);
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
