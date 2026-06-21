/* ux-dom-harness.js — a tiny, dependency-free DOM that hosts the REAL touch UI.
 *
 * Shared by web/ux-walkthrough-test.js (pass/fail E2E) and
 * web/ux-scenario-equip-inventory.js (human-readable interaction trace). It
 * builds just enough DOM for web/index.html's UI script, loads the REAL
 * web/bridge.js, wires the UI onto it, and (separately) boots the REAL
 * web/rogue.js engine — exactly the browser data path, headless.
 *
 *   const H = require("./ux-dom-harness").createHarness();
 *   ...drive H.click(...) / H.fireWindow(...)...; H.bootEngine();
 */
"use strict";
const fs = require("fs");
const path = require("path");

/* ── classList / Element ─────────────────────────────────────────────────── */
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
    this.tag = tag; this.nodeType = 1; this._text = "";
    this.style = {}; this.dataset = {}; this.attributes = {}; this._listeners = {};
    this.parentNode = null; this.childNodes = [];
    this._className = ""; this._classSet = new Set();
    this.classList = makeClassList(this); this.id = "";
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

/* tiny HTML fragment parser: <div>/<span> with class="…", <br>, text. */
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

/* selector engine for ".msg span:last-child" and ".cell" (closest). */
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

/* ── async helpers (mirrors e2e-test.js) ─────────────────────────────────── */
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function waitFor(pred, label, timeout = 8000) {
  const t0 = Date.now();
  while (Date.now() - t0 < timeout) { if (pred()) return true; await sleep(20); }
  console.log("    (timeout waiting for: " + label + ")");
  return false;
}

/* ── build the harness: DOM + bridge.js + index.html UI script ────────────── */
function createHarness() {
  const byId = {};
  const document = {
    getElementById: (id) => byId[id] || null,
    createElement: (tag) => new El(tag),
  };
  const mk = (id, parent) => { const el = new El("div"); el.id = id; if (parent) parent.appendChild(el); byId[id] = el; return el; };

  const body = new El("body");
  const app = mk("app", body), col = mk("col", app);
  const header = new El("header"); col.appendChild(header);
  const sound = mk("sound", header); const depth = mk("depth", header);
  mk("hpfill", header); mk("hptext", header); mk("lv", header); mk("str", header); mk("gold", header);
  const mapwrap = mk("mapwrap", col); const map = mk("map", mapwrap); const boot = mk("boot", mapwrap);
  boot.textContent = "로딩 중… (rogue.js)";   // index.html static markup (render() only toggles display)
  const log = mk("log", col);
  const controls = mk("controls", col); const dpad = mk("dpad", controls); const actions = mk("actions", controls);
  const scrim = mk("scrim", body); const sheet = mk("sheet", scrim);
  mk("sheet-msg", sheet); mk("cmds", sheet); mk("letters", sheet);
  const ovlScrim = mk("ovl-scrim", body); const ovlSheet = mk("ovl-sheet", ovlScrim);
  mk("ovl-h", ovlSheet); mk("ovl-body", ovlSheet); mk("ovl-go", ovlSheet);
  mk("throw-hint", body);   // 던지기 방향 안내 배너 (index.html과 동일 구성)
  // the markup's onclick="event.stopPropagation()" on the sheets.
  sheet.addEventListener("click", (e) => e.stopPropagation());
  ovlSheet.addEventListener("click", (e) => e.stopPropagation());

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
    RogueAudio: audio, navigator,
    addEventListener(type, fn, opts) {
      (winListeners[type] || (winListeners[type] = [])).push({ fn, once: !!(opts && opts.once) });
    },
  };
  const fireWindow = (type, extra) => {
    const ev = Object.assign({ type, defaultPrevented: false, preventDefault() { this.defaultPrevented = true; } }, extra || {});
    const ls = winListeners[type] || [];
    for (const L of ls.slice()) L.fn(ev);
    winListeners[type] = ls.filter((L) => !L.once);
    return ev;
  };

  // load the REAL bridge.js onto this window; expose RogueBridge as a bare
  // global too (web_curses.c's EM_ASM calls it unqualified).
  global.window = window;
  require(path.join(__dirname, "bridge.js"));
  const B = window.RogueBridge;
  const I = window.RogueInput;
  global.RogueBridge = B;

  // spy on every key the UI hands the engine (user intent, not the auto-space).
  const pushed = [];
  const realPush = B.pushKey.bind(B);
  B.pushKey = (code) => { pushed.push(typeof code === "string" ? code : String.fromCharCode(code)); realPush(code); };

  // load the REAL index.html UI script (subscribes render()).
  const indexSrc = fs.readFileSync(path.join(__dirname, "index.html"), "utf8");
  const uiScript = (indexSrc.match(/<script>([\s\S]*?)<\/script>/g) || [])
    .map((s) => s.replace(/^<script>|<\/script>$/g, ""))
    .find((s) => /B\.subscribe\(render\)/.test(s));
  if (!uiScript) throw new Error("could not extract the index.html UI script");
  // eslint-disable-next-line no-new-func
  new Function("window", "document", "navigator", uiScript)(window, document, navigator);

  // the UI ran render() once at load against the empty bridge buffer, so the
  // "로딩 중" affordance is up now — snapshot it before the engine draws.
  const bootAtLoad = { display: boot.style.display, text: boot.textContent };

  const click = (el, extra) => dispatch(el, "click", extra);
  const findChild = (parent, pred) => parent.children.find(pred);
  const cellAt = (bx, by) => map.children.find((c) => +c.dataset.bx === bx && +c.dataset.by === by);

  return {
    El, document, window, navigator, B, I, byId, indexSrc, bootAtLoad,
    els: { body, app, col, header, sound, depth, map, boot, log, dpad, actions, scrim, sheet, ovlScrim, ovlSheet },
    dispatch, click, fireWindow, cellAt, findChild, pushed, vibrateLog, audio,
  };
}

/* boot the REAL engine (emscripten build). */
function bootEngine() {
  const enginePath = path.join(__dirname, "rogue.js");
  if (!fs.existsSync(enginePath)) {
    console.error("\nweb/rogue.js not found — run `bash build.sh` first (needs emscripten).");
    process.exit(2);
  }
  process.env.HOME = process.env.HOME || "/tmp";
  require(enginePath); // boots main() (async via Asyncify)
}

module.exports = { createHarness, bootEngine, sleep, waitFor };
