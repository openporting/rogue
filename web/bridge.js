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
  let overlay = null;          // current overlay sheet (array of text lines) or null
  let overlayBuf = null;       // accumulator between overlayBegin/overlayEnd
  let moreActive = false;      // is a "--More--" pager currently on row 0?
  const keyQueue = [];
  const listeners = new Set();
  const notify = () => listeners.forEach((cb) => cb());

  window.RogueBridge = {
    /* ---- called by C (engine -> UI) ---- */
    drawCell(y, x, ch, attr) {
      if (y >= 0 && y < ROWS && x >= 0 && x < COLS)
        screen[y][x] = { ch: String.fromCharCode(ch), attr };
    },
    refresh() {
      // a stdscr commit means the game redrew the map -> any overlay is done.
      if (overlay) overlay = null;
      // Auto-advance the "--More--" message pager. Rogue blocks on wait_for(' ')
      // whenever a turn emits 2+ messages, so the player can read the earlier one
      // before it's overwritten (io.c endmsg). There's no spacebar on mobile, and
      // our Korean log already keeps the full history, so the pause is pointless —
      // feed the space ourselves. The marker lives only on row 0 (translated to
      // "--계속--"); rising-edge so each --More-- gets exactly one space. Each
      // pager step is preceded by a cleared-row-0 refresh, so the edge re-arms.
      const row0 = screen[0].map((c) => c.ch).join("");
      const hasMore = row0.indexOf("계속") !== -1 || row0.indexOf("More") !== -1;
      if (hasMore && !moreActive) keyQueue.push(32);
      moreActive = hasMore;
      notify();
    },
    clearScreen() {
      for (let y = 0; y < ROWS; y++)
        for (let x = 0; x < COLS; x++) screen[y][x] = { ch: " ", attr: 0 };
      notify();
    },
    msg(korean) {
      messages = [korean, ...messages].slice(0, 50);
      // §10-#5 audio: react to the (already-Korean) message with a sound effect.
      // Guarded so the bridge still works if audio.js isn't loaded.
      if (window.RogueAudio) window.RogueAudio.onMessage(korean);
      notify();
    },
    /* Terminal BEL (\007). The engine drops it into the screen buffer today, so
     * this is only called if web_curses.c's waddch is later taught to forward it
     * (§12.3, a one-line shim boost). Invalid-input feedback already routes
     * through msg()/onMessage(), so the audible cue works without that change. */
    bell() { if (window.RogueAudio) window.RogueAudio.bell(); },

    /* Overlay seam (inventory / help / options / detection). The C side
     * (web_curses.c wrefresh of a non-stdscr window) streams one text line per
     * non-blank row between begin/end, then blocks on a key — the UI shows the
     * sheet and answers with space/letters. The next stdscr refresh() clears it. */
    overlayBegin() { overlayBuf = []; },
    overlayLine(s) { if (overlayBuf) overlayBuf.push(s); },
    overlayEnd() { overlay = overlayBuf || []; overlayBuf = null; notify(); },
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
    /* current overlay sheet lines, or null when none is up (UI reads this to
     * decide whether to show the inventory/help bottom sheet). */
    getOverlay() { return overlay; },
    /* dismiss the overlay from the UI (e.g. the close button) without waiting
     * for the engine's redraw; the engine itself is unblocked via a key press. */
    closeOverlay() { if (overlay) { overlay = null; notify(); } },

    /* ---- derived reads the touch UI uses (no engine changes needed) ---- */

    /* Player glyph position on the dungeon rows (1..ROWS-2), or null if the
     * map isn't drawn yet (boot, or a full-screen overlay like death/help). */
    getPlayer() {
      for (let y = 1; y < ROWS - 1; y++)
        for (let x = 0; x < COLS; x++)
          if (screen[y][x].ch === "@") return { x, y };
      return null;
    },

    /* Rogue's status line stays English/numeric on purpose (§8): instead of a
     * C hook we parse it straight off the buffer. Returns null until it shows.
     *   "Level: 1  Gold: 0  Hp: 12(12)  Str: 16(16)  Arm: 4  Exp: 1/0  ..."
     * Note Rogue's "Level" is the DUNGEON depth; the character level is Exp's
     * first field. The trailing word, if any, is the hunger state. */
    getStats() {
      for (let y = ROWS - 1; y >= 0; y--) {
        const line = screen[y].map((c) => c.ch).join("");
        if (!/Hp:\s*\d/.test(line)) continue;
        const num = (re) => { const m = line.match(re); return m ? m.slice(1).map(Number) : null; };
        const depth = num(/Level:\s*(\d+)/);
        const gold = num(/Gold:\s*(\d+)/);
        const hp = num(/Hp:\s*(\d+)\((\d+)\)/);
        const str = num(/Str:\s*(\d+)\((\d+)\)/);
        const arm = num(/Arm:\s*(-?\d+)/);
        const exp = num(/Exp:\s*(\d+)\/(\d+)/);
        const hunger = (line.match(/Exp:\s*\d+\/\d+\s+([A-Za-z]+)/) || [])[1] || "";
        if (!hp) return null;
        return {
          depth: depth ? depth[0] : 1,
          gold: gold ? gold[0] : 0,
          hp: hp[0], maxhp: hp[1],
          str: str ? str[0] : 0, maxstr: str ? str[1] : 0,
          arm: arm ? arm[0] : 0,
          level: exp ? exp[0] : 1, exp: exp ? exp[1] : 0,
          hunger,
        };
      }
      return null;
    },
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
    ascend()    { window.RogueBridge.pushKey("<"); },
    search()    { window.RogueBridge.pushKey("s"); },
    rest()      { window.RogueBridge.pushKey("."); },
    command(c)  { window.RogueBridge.pushKey(c); }, // q,r,w,W,e,t,? from drawer
    // answer engine prompts (item letters, --More--, y/n, name entry)
    key(c)      { window.RogueBridge.pushKey(c); },
    enter()     { window.RogueBridge.pushKey(13); },
    escape()    { window.RogueBridge.pushKey(27); },
    space()     { window.RogueBridge.pushKey(" "); },
  };
})();

/* ---------------------------------------------------------------------------
 * UI 결선 — DONE. The live touch UI is web/index.html (dependency-free vanilla
 * JS rebuild of prototype/rogue-touch-prototype.jsx, wired to this bridge):
 *
 *   1. Map:   render a viewport centred on @ from RogueBridge.getScreen()
 *             (dungeon rows 1..23), torch-FOV brightness by distance to
 *             RogueBridge.getPlayer(), glyph colour via glyphStyle().
 *             Log: RogueBridge.getMessages() (Korean, from web_emit_msg).
 *
 *   2. Input: D-pad/tap/swipe -> RogueInput.move(dx,dy); action buttons ->
 *             pickup()/descend()/ascend()/search()/rest(); the 더보기 sheet ->
 *             command('q'|'r'|'w'|...) plus an a–z keypad + key()/enter()/
 *             escape()/space() to answer engine prompts.
 *
 *   3. Status bar (depth/HP/Lv/Str/Gold): RogueBridge.getStats() parses Rogue's
 *             English status row straight off the buffer — no C hook or rebuild
 *             needed (the status line is intentionally left untranslated, §8).
 *
 *   4. Load order in index.html:  bridge.js  ->  this UI  ->  rogue.js .
 *
 * §10-#4 DONE: inventory / discoveries / help / options / detection windows
 * (everything Rogue draws to a non-stdscr window: hw, and INV_OVER's tw) are now
 * forwarded by web_curses.c wrefresh() as a text overlay — overlayBegin/Line/End
 * here build RogueBridge.getOverlay(), which index.html renders as a bottom
 * sheet. The engine blocks on a key after each page, so the sheet's "계속"
 * button sends space; the next stdscr refresh() auto-clears the overlay.
 * (Fix that made INV_OVER work: subwin() now truly aliases its parent window in
 * web_curses.c, so the copied item lines + prompt land in the refreshed window.)
 * ------------------------------------------------------------------------- */
