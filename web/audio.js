/* audio.js — §10-#5 음향 (SFX + BGM). The engine (WASM) is untouched; all sound
 * lives in the web layer, riding the three seams we already hold (draw / input /
 * message). Zero assets, zero CDN: every SFX is synthesized with Web Audio and
 * the BGM is a generative dungeon drone, so it ships as static files and works
 * offline (§12.1 "외부 CDN 금지").
 *
 * Wiring (see index.html / bridge.js):
 *   - First user gesture → RogueAudio.unlock(): create/resume the AudioContext
 *     (mobile autoplay policy) and, if enabled, start the BGM.
 *   - bridge.js msg(korean) → RogueAudio.onMessage(korean): pattern-match the
 *     already-Korean message (tr_msg normalized it, so keys are stable) → sfx.
 *   - index.html render() → RogueAudio.setDepth(depth): retune the BGM and play
 *     a descend cue when the dungeon depth increases.
 *   - RogueBridge.bell() → RogueAudio.bell(): terminal BEL (if the shim ever
 *     forwards it; invalid-input messages already trigger it via onMessage).
 *
 * Settings (sfx/bgm volume + mute) persist in localStorage; the UI has a 🔈
 * toggle bound to RogueAudio.toggleMute()/getState().
 */
(function () {
  const LS_KEY = "rogue.audio";
  const def = { sfx: 0.7, bgm: 0.4, muted: false };
  let cfg = (() => {
    try { return Object.assign({}, def, JSON.parse(localStorage.getItem(LS_KEY) || "{}")); }
    catch (_) { return Object.assign({}, def); }
  })();
  const save = () => { try { localStorage.setItem(LS_KEY, JSON.stringify(cfg)); } catch (_) {} };

  let ctx = null, master = null, sfxBus = null, musicBus = null, noiseBuf = null;
  let unlocked = false;

  function build() {
    if (ctx) return true;
    try {
      const AC = window.AudioContext || window.webkitAudioContext;
      if (!AC) return false;
      ctx = new AC();
      master = ctx.createGain(); master.gain.value = cfg.muted ? 0 : 1; master.connect(ctx.destination);
      sfxBus = ctx.createGain(); sfxBus.gain.value = cfg.sfx; sfxBus.connect(master);
      musicBus = ctx.createGain(); musicBus.gain.value = cfg.bgm; musicBus.connect(master);
      // one shared 1s white-noise buffer for percussive/airy SFX
      noiseBuf = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate);
      const d = noiseBuf.getChannelData(0);
      for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
      return true;
    } catch (_) { ctx = null; return false; }
  }

  /* ── tiny synth primitives (all routed to sfxBus) ── */
  const T = () => ctx.currentTime;
  function env(g, t, a, peak, dur) {
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(Math.max(peak, 0.0002), t + a);
    g.gain.exponentialRampToValueAtTime(0.0001, t + a + dur);
  }
  function tone(t, freq, dur, type, peak) {
    const o = ctx.createOscillator(), g = ctx.createGain();
    o.type = type; o.frequency.setValueAtTime(freq, t);
    o.connect(g); g.connect(sfxBus); env(g, t, 0.005, peak, dur);
    o.start(t); o.stop(t + dur + 0.06);
  }
  function sweep(t, f0, f1, dur, type, peak) {
    const o = ctx.createOscillator(), g = ctx.createGain();
    o.type = type; o.frequency.setValueAtTime(f0, t);
    o.frequency.exponentialRampToValueAtTime(Math.max(f1, 1), t + dur);
    o.connect(g); g.connect(sfxBus); env(g, t, 0.005, peak, dur);
    o.start(t); o.stop(t + dur + 0.06);
  }
  function noise(t, dur, peak, filter, freq) {
    const src = ctx.createBufferSource(); src.buffer = noiseBuf;
    const f = ctx.createBiquadFilter(); f.type = filter || "bandpass"; f.frequency.setValueAtTime(freq || 1000, t);
    const g = ctx.createGain();
    src.connect(f); f.connect(g); g.connect(sfxBus); env(g, t, 0.004, peak, dur);
    src.start(t); src.stop(t + dur + 0.06);
    return f; // caller may ramp the filter for whooshes
  }

  /* ── the SFX set ── */
  const SFX = {
    hit(t)    { tone(t, 110, 0.12, "square", 0.5); noise(t, 0.1, 0.4, "lowpass", 320); },
    miss(t)   { const f = noise(t, 0.18, 0.28, "bandpass", 2000); f.frequency.exponentialRampToValueAtTime(500, t + 0.18); },
    kill(t)   { tone(t, 523, 0.09, "square", 0.35); tone(t + 0.09, 659, 0.09, "square", 0.35); tone(t + 0.18, 784, 0.15, "square", 0.4); },
    hurt(t)   { sweep(t, 210, 90, 0.18, "sawtooth", 0.45); noise(t, 0.12, 0.3, "lowpass", 500); },
    coin(t)   { tone(t, 1318, 0.07, "triangle", 0.3); tone(t + 0.06, 1760, 0.13, "triangle", 0.3); },
    pickup(t) { sweep(t, 440, 880, 0.1, "triangle", 0.3); },
    potion(t) { sweep(t, 300, 760, 0.22, "sine", 0.3); },
    scroll(t) { noise(t, 0.26, 0.18, "highpass", 2400); },
    levelup(t){ [523, 659, 784, 1046].forEach((f, i) => tone(t + i * 0.08, f, 0.13, "triangle", 0.35)); },
    magic(t)  { tone(t, 1568, 0.5, "sine", 0.25); tone(t, 1572, 0.5, "sine", 0.2); },
    trap(t)   { tone(t, 300, 0.05, "square", 0.35); sweep(t, 300, 130, 0.32, "square", 0.3); },
    death(t)  { sweep(t, 330, 70, 0.9, "sawtooth", 0.4); tone(t, 55, 1.0, "sine", 0.3); },
    hunger(t) { tone(t, 330, 0.15, "sine", 0.25); tone(t + 0.2, 294, 0.22, "sine", 0.25); },
    descend(t){ sweep(t, 150, 70, 0.6, "sine", 0.35); noise(t, 0.6, 0.14, "lowpass", 220); },
    bell(t)   { tone(t, 880, 0.07, "square", 0.22); },
  };

  /* Message → SFX. Ordered: first match wins, so the more specific / more urgent
   * cues come first (death before level-up, monster-hits-you before you-hit). The
   * strings are the Korean produced by webcurses/i18n.c (tr_msg), so they're
   * stable keys. */
  const RULES = [
    [/죽었다|쓰러졌|당신은 죽었/, "death"],
    [/도달했다|근육이 불끈|훨씬 능숙해진/, "levelup"],
    [/처치했다/, "kill"],
    [/빗나갔다/, "miss"],
    [/당신을 공격했다|물려서|물려 더|약해진 기분|강타한다|어깨에 꽂|화살에 맞|귓가|할퀴|메스꺼운|뒤틀리는|따끔/, "hurt"],
    [/멋지게 명중시켰다|공격했다/, "hit"],
    [/금화/, "coin"],
    [/발견했다|가지고 있다|들었다|착용했다|떨어뜨렸다|훔쳐 갔다/, "pickup"],
    [/두루마리/, "scroll"],
    [/함정|곰덫|덫에 걸/, "trap"],
    [/빛난다|빛으로|빛이 번쩍|번쩍인다|어른거리는|사그라든다/, "magic"],
    [/기분이 나아|따뜻해지는|맛있|강해진 기분/, "potion"],
    [/배가 고파|기운이 빠지|굶주/, "hunger"],
    [/올바른 항목이 아니다|잘못된 명령|배낭에 없다|알 수 없는 문자/, "bell"],
  ];

  /* ── generative dungeon BGM ── */
  let music = { on: false, drone: [], timer: null, depth: 1 };
  const SCALE = [0, 3, 5, 7, 10]; // minor pentatonic
  const mtof = (m) => 440 * Math.pow(2, (m - 69) / 12);
  const rootMidi = () => 45 - ((music.depth - 1) % 6); // A2, sinking with depth

  function startMusic() {
    if (!ctx || music.on) return;
    music.on = true;
    const t = T(), root = mtof(rootMidi());
    // sustained drone: root + fifth, detuned, through a slowly wobbling lowpass
    const lp = ctx.createBiquadFilter(); lp.type = "lowpass"; lp.frequency.value = 600;
    const lfo = ctx.createOscillator(), lfoG = ctx.createGain();
    lfo.frequency.value = 0.07; lfoG.gain.value = 220; lfo.connect(lfoG); lfoG.connect(lp.frequency);
    lp.connect(musicBus); lfo.start(t);
    const droneG = ctx.createGain(); droneG.gain.setValueAtTime(0.0001, t);
    droneG.gain.exponentialRampToValueAtTime(0.16, t + 3); droneG.connect(lp);
    const o1 = ctx.createOscillator(), o2 = ctx.createOscillator();
    o1.type = o2.type = "sawtooth";
    o1.frequency.value = root; o2.frequency.value = root * 1.5 * 1.003;
    o1.connect(droneG); o2.connect(droneG); o1.start(t); o2.start(t);
    music.drone = [o1, o2, lfo, droneG, lp];
    // sparse melodic motes from the scale
    const step = () => {
      if (!music.on) return;
      if (Math.random() < 0.6) {
        const t2 = T() + 0.05;
        const m = rootMidi() + 12 + SCALE[(Math.random() * SCALE.length) | 0] + (Math.random() < 0.3 ? 12 : 0);
        const o = ctx.createOscillator(), g = ctx.createGain();
        o.type = "triangle"; o.frequency.value = mtof(m);
        o.connect(g); g.connect(musicBus);
        g.gain.setValueAtTime(0.0001, t2);
        g.gain.exponentialRampToValueAtTime(0.08, t2 + 0.04);
        g.gain.exponentialRampToValueAtTime(0.0001, t2 + 1.6);
        o.start(t2); o.stop(t2 + 1.7);
      }
      music.timer = setTimeout(step, 1400 + Math.random() * 2200);
    };
    music.timer = setTimeout(step, 1200);
  }
  function stopMusic() {
    music.on = false;
    if (music.timer) { clearTimeout(music.timer); music.timer = null; }
    if (ctx && music.drone.length) {
      const t = T(), g = music.drone[3];
      try { g.gain.cancelScheduledValues(t); g.gain.setValueAtTime(g.gain.value, t);
            g.gain.exponentialRampToValueAtTime(0.0001, t + 1.2); } catch (_) {}
      music.drone.forEach((n) => { try { n.stop && n.stop(t + 1.3); } catch (_) {} });
    }
    music.drone = [];
  }

  window.RogueAudio = {
    /* call from the first user gesture (tap/key); safe to call repeatedly. */
    unlock() {
      if (!build()) return;
      if (ctx.state === "suspended") ctx.resume();
      if (!unlocked) { unlocked = true; if (!cfg.muted) startMusic(); }
    },
    sfx(name) {
      if (!ctx || cfg.muted || !unlocked) return;
      const fn = SFX[name]; if (fn) try { fn(T() + 0.001); } catch (_) {}
    },
    onMessage(korean) {
      if (!korean) return;
      for (let i = 0; i < RULES.length; i++)
        if (RULES[i][0].test(korean)) { this.sfx(RULES[i][1]); return; }
    },
    bell() { this.sfx("bell"); },
    /* dungeon depth changed: retune the drone, and cue a descend whoosh when we
       go deeper (Rogue prints no message on stairs, so this is the only signal). */
    setDepth(d) {
      d = +d || 1;
      if (d === music.depth) return;
      const deeper = d > music.depth;
      music.depth = d;
      if (ctx && music.on && music.drone.length) {
        const root = mtof(rootMidi()), t = T();
        try { music.drone[0].frequency.setTargetAtTime(root, t, 1.5);
              music.drone[1].frequency.setTargetAtTime(root * 1.5 * 1.003, t, 1.5); } catch (_) {}
      }
      if (deeper) this.sfx("descend");
    },

    /* ── settings (bound to the UI 🔈 toggle) ── */
    toggleMute() {
      cfg.muted = !cfg.muted; save();
      if (ctx) master.gain.setTargetAtTime(cfg.muted ? 0 : 1, T(), 0.02);
      if (cfg.muted) stopMusic(); else if (unlocked) startMusic();
      return cfg.muted;
    },
    setSfx(v) { cfg.sfx = Math.max(0, Math.min(1, v)); save(); if (sfxBus) sfxBus.gain.setTargetAtTime(cfg.sfx, T(), 0.02); },
    setBgm(v) { cfg.bgm = Math.max(0, Math.min(1, v)); save(); if (musicBus) musicBus.gain.setTargetAtTime(cfg.bgm, T(), 0.02); },
    getState() { return { muted: cfg.muted, sfx: cfg.sfx, bgm: cfg.bgm, ready: !!ctx, music: music.on }; },
  };
})();
