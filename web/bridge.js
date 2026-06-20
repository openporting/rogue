/* bridge.js — the seam between the WASM Rogue engine and the React touch UI.
 *
 * The C shim (web_curses.c) calls into window.RogueBridge:
 *    drawCell(y,x,ch,attr)  refresh()  clearScreen()  msg(korean)  popKey()
 * The touch UI calls:
 *    pushKey(rogueKeyCode)   subscribe(cb)   getScreen()   getMessages()
 *
 * Keymap is taken from Rogue 5.4.4 command.c (verified):
 *    move h/j/k/l + diagonals y/u/b/n,  ',' pickup,  'i' inventory,
 *    '>' descend,  's' search,  '.' rest,  q/r/w/W/e/t for the action drawer.
 */
(function () {
  const ROWS = 25, COLS = 80;

  const screen = Array.from({ length: ROWS }, () =>
    Array.from({ length: COLS }, () => ({ ch: " ", attr: 0 }))
  );
  let messages = [];
  const keyQueue = [];
  const listeners = new Set();
  const notify = () => listeners.forEach((cb) => cb());

  window.RogueBridge = {
    /* ---- called by C (engine -> UI) ---- */
    drawCell(y, x, ch, attr) {
      if (y >= 0 && y < ROWS && x >= 0 && x < COLS)
        screen[y][x] = { ch: String.fromCharCode(ch), attr };
    },
    refresh() { notify(); },
    clearScreen() {
      for (let y = 0; y < ROWS; y++)
        for (let x = 0; x < COLS; x++) screen[y][x] = { ch: " ", attr: 0 };
      notify();
    },
    msg(korean) {
      messages = [korean, ...messages].slice(0, 50);
      notify();
    },
    popKey() {
      return keyQueue.length ? keyQueue.shift() : -1; // -1 => C side yields
    },

    /* ---- called by the touch UI (UI -> engine) ---- */
    pushKey(code) {
      keyQueue.push(typeof code === "string" ? code.charCodeAt(0) : code);
    },
    subscribe(cb) { listeners.add(cb); return () => listeners.delete(cb); },
    getScreen() { return screen; },
    getMessages() { return messages; },
  };

  /* Semantic helpers the touch UI uses instead of raw key codes. */
  const DIR = {
    left: "h", right: "l", up: "k", down: "j",
    upleft: "y", upright: "u", downleft: "b", downright: "n",
  };
  window.RogueInput = {
    move(dx, dy) {
      const k =
        dx < 0 && dy < 0 ? DIR.upleft :
        dx > 0 && dy < 0 ? DIR.upright :
        dx < 0 && dy > 0 ? DIR.downleft :
        dx > 0 && dy > 0 ? DIR.downright :
        dx < 0 ? DIR.left : dx > 0 ? DIR.right :
        dy < 0 ? DIR.up : dy > 0 ? DIR.down : null;
      if (k) window.RogueBridge.pushKey(k);
    },
    // tap a tile at (tx,ty): step one square toward it (engine resolves walls)
    tap(tx, ty, px, py) {
      this.move(Math.sign(tx - px), Math.sign(ty - py));
    },
    pickup()    { window.RogueBridge.pushKey(","); },
    inventory() { window.RogueBridge.pushKey("i"); },
    descend()   { window.RogueBridge.pushKey(">"); },
    search()    { window.RogueBridge.pushKey("s"); },
    rest()      { window.RogueBridge.pushKey("."); },
    command(c)  { window.RogueBridge.pushKey(c); }, // q,r,w,W,e,t,? from drawer
  };
})();

/* ---------------------------------------------------------------------------
 * Wiring into the prototype's React component (rogue-touch-prototype.jsx):
 *
 *   1. Drop the mock useReducer. Instead:
 *        const [, force] = useReducer(x => x + 1, 0);
 *        useEffect(() => RogueBridge.subscribe(force), []);
 *      and render the map from RogueBridge.getScreen() (rows ~1..22 = dungeon,
 *      glyph color via the existing glyphStyle()), and the log from
 *      RogueBridge.getMessages(). Player @ position is read off the screen
 *      buffer, so torch-FOV brightness still works unchanged.
 *
 *   2. Replace dispatch(...) calls in the controls:
 *        D-pad      -> RogueInput.move(dx, dy)
 *        tap tile   -> RogueInput.tap(x, y, px, py)
 *        줍기/인벤/계단/검색/쉬기 -> RogueInput.pickup()/inventory()/...
 *        더보기 drawer items     -> RogueInput.command('q'|'r'|'w'|...)
 *
 *   3. Status bar (HP/Lv/Gold/Str/depth): export those from C via a tiny
 *      EM_ASM hook in Rogue's status() (io.c) -> RogueBridge.stats({...}),
 *      so we keep our JS status bar and skip translating Rogue's status line.
 *
 *   4. Load order in index.html:  bridge.js  ->  rogue.js (emscripten) .
 * ------------------------------------------------------------------------- */
