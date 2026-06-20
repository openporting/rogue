import React, { useReducer, useEffect, useRef } from "react";
import {
  Backpack,
  Search,
  Hourglass,
  ArrowDownToLine,
  Menu,
  Hand,
} from "lucide-react";

/* ============================================================
   운명의 던전 — 모바일 터치 UI 프로토타입
   (오리지널 Rogue 한글 포팅 손맛 확인용)
   ============================================================ */

/* ---- 팔레트: 횃불 켠 던전 ---- */
const C = {
  void: "#0d0b0a",
  floor: "#1a1614",
  edge: "#2e2722",
  edge2: "#4a3f36",
  ember: "#e8a04a",
  emberDim: "#8a6a3a",
  blood: "#c4543b",
  parch: "#d9c7a3",
  parchDim: "#8d8069",
  moss: "#7a9b6e",
};
const MONO =
  "'JetBrains Mono','SF Mono',ui-monospace,Menlo,Consolas,monospace";
const KR =
  "'Pretendard',-apple-system,'Apple SD Gothic Neo','Noto Sans KR',sans-serif";

/* ---- 맵 ---- */
const W = 13;
const H = 9;
const isWall = (x, y) => x <= 0 || y <= 0 || x >= W - 1 || y >= H - 1;

/* ---- 한글 조사 자동 처리 (어순/받침 대응) ---- */
const hasBatchim = (s) => {
  if (!s) return false;
  const c = s.charCodeAt(s.length - 1);
  if (c < 0xac00 || c > 0xd7a3) return false;
  return (c - 0xac00) % 28 !== 0;
};
const eulReul = (w) => w + (hasBatchim(w) ? "을" : "를");
const iGa = (w) => w + (hasBatchim(w) ? "이" : "가");

/* ---- 콘텐츠 풀 ---- */
const MONSTERS = [
  { glyph: "r", name: "쥐", hp: 5 },
  { glyph: "k", name: "코볼드", hp: 7 },
  { glyph: "B", name: "박쥐", hp: 6 },
  { glyph: "S", name: "뱀", hp: 9 },
];
const ITEMS = [
  { glyph: "!", name: "치유 물약", kind: "potion" },
  { glyph: "?", name: "식별 두루마리", kind: "scroll" },
  { glyph: ")", name: "단검", kind: "weapon" },
  { glyph: "[", name: "가죽 갑옷", kind: "armor" },
  { glyph: "*", name: "금화", kind: "gold" },
  { glyph: ":", name: "식량 배급", kind: "food" },
];

const rng = (n) => Math.floor(Math.random() * n);
const pick = (a) => a[rng(a.length)];
const floors = () => {
  const f = [];
  for (let y = 1; y < H - 1; y++)
    for (let x = 1; x < W - 1; x++) f.push({ x, y });
  return f;
};

function genLevel(depth) {
  const open = floors();
  for (let i = open.length - 1; i > 0; i--) {
    const j = rng(i + 1);
    [open[i], open[j]] = [open[j], open[i]];
  }
  let i = 0;
  const playerPos = open[i++];
  const stair = open[i++];
  const mCount = Math.min(2 + Math.floor(depth / 2), 4);
  const monsters = [];
  for (let m = 0; m < mCount; m++) {
    const base = pick(MONSTERS);
    monsters.push({
      id: depth * 100 + m,
      x: open[i].x,
      y: open[i].y,
      glyph: base.glyph,
      name: base.name,
      hp: base.hp + depth,
    });
    i++;
  }
  const iCount = 2 + rng(2);
  const items = [];
  for (let k = 0; k < iCount; k++) {
    const base = pick(ITEMS);
    items.push({ x: open[i].x, y: open[i].y, ...base });
    i++;
  }
  return { playerPos, stair, monsters, items };
}

/* ---- 초기 상태 ---- */
function freshGame() {
  const lvl = genLevel(1);
  return {
    depth: 1,
    player: {
      x: lvl.playerPos.x,
      y: lvl.playerPos.y,
      hp: 16,
      maxHp: 16,
      lvl: 1,
      str: 7,
      gold: 0,
    },
    monsters: lvl.monsters,
    items: lvl.items,
    stair: lvl.stair,
    inventory: [
      { name: "단검", glyph: ")", qty: 1, note: "장착 중" },
      { name: "가죽 갑옷", glyph: "[", qty: 1, note: "장착 중" },
      { name: "치유 물약", glyph: "!", qty: 2, kind: "potion" },
      { name: "식량 배급", glyph: ":", qty: 1, kind: "food" },
    ],
    messages: ["운명의 던전에 발을 들였다. 타일을 눌러 이동하라."],
    dead: false,
  };
}

const log = (s, m) => ({ ...s, messages: [m, ...s.messages].slice(0, 12) });
const monAt = (s, x, y) => s.monsters.find((m) => m.x === x && m.y === y);

/* ---- 한 턴 처리 ---- */
function playerStep(state, dx, dy) {
  let s = state;
  const nx = s.player.x + dx;
  const ny = s.player.y + dy;
  if (isWall(nx, ny)) return log(s, "벽에 막혔다.");

  const target = monAt(s, nx, ny);
  let acted = false;

  if (target) {
    // 공격
    const dmg = 3 + rng(5) + Math.floor(s.player.str / 4);
    const monsters = s.monsters.map((m) =>
      m.id === target.id ? { ...m, hp: m.hp - dmg } : m
    );
    s = { ...s, monsters };
    s = log(s, `${eulReul(target.name)} 내려쳤다! (${dmg} 피해)`);
    const hit = monsters.find((m) => m.id === target.id);
    if (hit.hp <= 0) {
      const reward = 4 + rng(8);
      s = {
        ...s,
        monsters: s.monsters.filter((m) => m.id !== target.id),
        player: { ...s.player, gold: s.player.gold + reward },
      };
      s = log(s, `${eulReul(target.name)} 처치했다! 금화 ${reward}닢 획득.`);
    }
    acted = true;
  } else {
    // 이동
    s = { ...s, player: { ...s.player, x: nx, y: ny } };
    const it = s.items.find((o) => o.x === nx && o.y === ny);
    if (it) {
      if (it.kind === "gold") {
        const g = 8 + rng(20);
        s = {
          ...s,
          items: s.items.filter((o) => o !== it),
          player: { ...s.player, gold: s.player.gold + g },
        };
        s = log(s, `금화 ${g}닢을 주웠다.`);
      } else {
        s = {
          ...s,
          items: s.items.filter((o) => o !== it),
          inventory: addItem(s.inventory, it),
        };
        s = log(s, `${eulReul(it.name)} 주웠다.`);
      }
    } else if (nx === s.stair.x && ny === s.stair.y) {
      s = log(s, "발밑에 아래로 향하는 계단이 있다. (계단 버튼)");
    }
    acted = true;
  }
  if (acted) s = monstersAct(s);
  return checkDeath(s);
}

function addItem(inv, it) {
  const ex = inv.find((o) => o.name === it.name);
  if (ex) return inv.map((o) => (o === ex ? { ...o, qty: o.qty + 1 } : o));
  return [...inv, { name: it.name, glyph: it.glyph, qty: 1, kind: it.kind }];
}

function monstersAct(state) {
  let s = state;
  const occupied = new Set(s.monsters.map((m) => m.x + "," + m.y));
  const nextMon = [];
  for (const m of s.monsters) {
    const adx = Math.abs(m.x - s.player.x);
    const ady = Math.abs(m.y - s.player.y);
    if (Math.max(adx, ady) === 1) {
      const dmg = 1 + rng(4);
      s = { ...s, player: { ...s.player, hp: s.player.hp - dmg } };
      s = log(s, `${iGa(m.name)} 당신을 할퀴었다. (${dmg} 피해)`);
      nextMon.push(m);
    } else {
      const sx = Math.sign(s.player.x - m.x);
      const sy = Math.sign(s.player.y - m.y);
      let tx = m.x + sx;
      let ty = m.y + sy;
      const blocked = (x, y) =>
        isWall(x, y) ||
        occupied.has(x + "," + y) ||
        (x === s.player.x && y === s.player.y);
      if (blocked(tx, ty)) {
        if (!blocked(m.x + sx, m.y)) ty = m.y;
        else if (!blocked(m.x, m.y + sy)) tx = m.x;
        else {
          tx = m.x;
          ty = m.y;
        }
      }
      occupied.delete(m.x + "," + m.y);
      occupied.add(tx + "," + ty);
      nextMon.push({ ...m, x: tx, y: ty });
    }
  }
  return { ...s, monsters: nextMon };
}

function checkDeath(s) {
  if (s.player.hp <= 0 && !s.dead) {
    return { ...log(s, "당신은 쓰러졌다…"), dead: true };
  }
  return s;
}

/* ---- 리듀서 ---- */
function reducer(state, action) {
  if (state.dead && action.type !== "reset") return state;
  switch (action.type) {
    case "move":
      return playerStep(state, action.dx, action.dy);
    case "tap": {
      const dx = Math.sign(action.x - state.player.x);
      const dy = Math.sign(action.y - state.player.y);
      if (dx === 0 && dy === 0) return state;
      return playerStep(state, dx, dy);
    }
    case "wait":
      return checkDeath(monstersAct(log(state, "잠시 숨을 골랐다.")));
    case "search":
      return checkDeath(
        monstersAct(log(state, "주변을 살폈다… 별다른 것은 없다."))
      );
    case "rest": {
      let s = { ...state };
      if (s.player.hp < s.player.maxHp)
        s = {
          ...s,
          player: { ...s.player, hp: Math.min(s.player.maxHp, s.player.hp + 2) },
        };
      return checkDeath(monstersAct(log(s, "기운을 추슬렀다.")));
    }
    case "pickup": {
      const it = state.items.find(
        (o) => o.x === state.player.x && o.y === state.player.y
      );
      if (!it) return log(state, "이곳엔 주울 것이 없다.");
      return playerStep(state, 0, 0); // no-op move triggers nothing; handle below
    }
    case "descend": {
      if (state.player.x !== state.stair.x || state.player.y !== state.stair.y)
        return log(state, "여기엔 계단이 없다. 계단(>) 위로 가야 한다.");
      const depth = state.depth + 1;
      const lvl = genLevel(depth);
      const healed = Math.min(
        state.player.maxHp,
        state.player.hp + 3
      );
      return log(
        {
          ...state,
          depth,
          player: { ...state.player, x: lvl.playerPos.x, y: lvl.playerPos.y, hp: healed },
          monsters: lvl.monsters,
          items: lvl.items,
          stair: lvl.stair,
        },
        `더 깊은 곳으로 내려간다… 던전 ${depth}층.`
      );
    }
    case "use": {
      const o = state.inventory[action.idx];
      if (!o) return state;
      if (o.kind === "potion" && o.qty > 0) {
        const heal = 6 + rng(5);
        const inv = state.inventory
          .map((x, i) => (i === action.idx ? { ...x, qty: x.qty - 1 } : x))
          .filter((x) => x.qty > 0 || x.note);
        const hp = Math.min(state.player.maxHp, state.player.hp + heal);
        return monstersAct(
          log(
            { ...state, inventory: inv, player: { ...state.player, hp } },
            `치유 물약을 마셨다. 기운이 돈다. (+${heal})`
          )
        );
      }
      return log(state, `${eulReul(o.name)} 살펴보았다.`);
    }
    case "command":
      return log(state, `“${action.name}” 명령은 본편에서 구현됩니다.`);
    case "reset":
      return freshGame();
    default:
      return state;
  }
}

/* ---- 글리프 스타일 ---- */
function glyphStyle(ch) {
  if (ch === "@") return { color: C.ember, weight: 700 };
  if ("rkBS".includes(ch)) return { color: C.blood, weight: 600 };
  if (ch === "*") return { color: C.ember, weight: 600 };
  if (ch === ":") return { color: C.moss, weight: 600 };
  if ("!?)[".includes(ch)) return { color: C.parch, weight: 600 };
  if (ch === ">" || ch === "<") return { color: C.ember, weight: 700 };
  if (ch === "#") return { color: C.edge2, weight: 400 };
  return { color: "#3a322b", weight: 400 }; // floor dot
}

export default function App() {
  const [s, dispatch] = useReducer(reducer, undefined, freshGame);
  const touch = useRef(null);

  // 키보드 (데스크톱 테스트용)
  useEffect(() => {
    const onKey = (e) => {
      const k = e.key;
      const map = {
        ArrowUp: [0, -1], ArrowDown: [0, 1], ArrowLeft: [-1, 0], ArrowRight: [1, 0],
        k: [0, -1], j: [0, 1], h: [-1, 0], l: [1, 0],
        y: [-1, -1], u: [1, -1], b: [-1, 1], n: [1, 1],
      };
      if (map[k]) { e.preventDefault(); dispatch({ type: "move", dx: map[k][0], dy: map[k][1] }); }
      else if (k === ".") dispatch({ type: "wait" });
      else if (k === "g") dispatch({ type: "pickup" });
      else if (k === ">") dispatch({ type: "descend" });
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  const buzz = () => navigator.vibrate && navigator.vibrate(8);
  const act = (a) => { buzz(); dispatch(a); };

  const onTouchStart = (e) => {
    const t = e.touches[0];
    touch.current = { x: t.clientX, y: t.clientY };
  };
  const onTouchEnd = (e) => {
    if (!touch.current) return;
    const t = e.changedTouches[0];
    const dx = t.clientX - touch.current.x;
    const dy = t.clientY - touch.current.y;
    touch.current = null;
    if (Math.max(Math.abs(dx), Math.abs(dy)) < 28) return; // 탭은 타일 onClick 처리
    if (Math.abs(dx) > Math.abs(dy)) act({ type: "move", dx: Math.sign(dx), dy: 0 });
    else act({ type: "move", dx: 0, dy: Math.sign(dy) });
  };

  const hpRatio = s.player.hp / s.player.maxHp;
  const hpColor = hpRatio > 0.5 ? C.ember : hpRatio > 0.25 ? "#d8902f" : C.blood;

  // 셀 글리프 계산
  const cellAt = (x, y) => {
    if (x === s.player.x && y === s.player.y) return "@";
    const m = monAt(s, x, y);
    if (m) return m.glyph;
    const it = s.items.find((o) => o.x === x && o.y === y);
    if (it) return it.glyph;
    if (x === s.stair.x && y === s.stair.y) return ">";
    if (isWall(x, y)) return "#";
    return "·";
  };

  const dpad = [
    ["↖", -1, -1], ["↑", 0, -1], ["↗", 1, -1],
    ["←", -1, 0], ["·", 0, 0], ["→", 1, 0],
    ["↙", -1, 1], ["↓", 0, 1], ["↘", 1, 1],
  ];

  const actions = [
    { label: "줍기", Icon: Hand, a: { type: "pickup" } },
    { label: "인벤", Icon: Backpack, a: { type: "ui-inv" } },
    { label: "계단", Icon: ArrowDownToLine, a: { type: "descend" } },
    { label: "검색", Icon: Search, a: { type: "search" } },
    { label: "쉬기", Icon: Hourglass, a: { type: "rest" } },
    { label: "더보기", Icon: Menu, a: { type: "ui-cmd" } },
  ];

  const [sheet, setSheet] = React.useState(null); // 'inv' | 'cmd' | null

  const onAction = (a) => {
    buzz();
    if (a.type === "ui-inv") setSheet("inv");
    else if (a.type === "ui-cmd") setSheet("cmd");
    else dispatch(a);
  };

  const recent = s.messages.slice(0, 3);

  return (
    <div
      style={{
        minHeight: "100vh",
        background: C.void,
        display: "flex",
        justifyContent: "center",
        fontFamily: KR,
      }}
    >
      <style>{`
        @keyframes torch {
          0%,100% { text-shadow: 0 0 6px ${C.ember}cc, 0 0 14px ${C.ember}66; }
          50%     { text-shadow: 0 0 4px ${C.ember}aa, 0 0 10px ${C.ember}44; }
        }
        .glyph-player { animation: torch 2.4s ease-in-out infinite; }
        .press:active { transform: scale(0.93); }
        @keyframes slideup { from { transform: translateY(100%); } to { transform: translateY(0); } }
        .sheet { animation: slideup .22s ease-out; }
        @media (prefers-reduced-motion: reduce) {
          .glyph-player { animation: none; text-shadow: 0 0 8px ${C.ember}aa; }
          .sheet { animation: none; }
        }
        button { -webkit-tap-highlight-color: transparent; touch-action: manipulation; }
      `}</style>

      <div
        style={{
          width: "100%",
          maxWidth: 440,
          display: "flex",
          flexDirection: "column",
          background: `radial-gradient(120% 80% at 50% 0%, #15110e 0%, ${C.void} 70%)`,
        }}
      >
        {/* ── 상태바 ── */}
        <header style={{ padding: "14px 16px 10px" }}>
          <div
            style={{
              display: "flex",
              alignItems: "center",
              justifyContent: "space-between",
            }}
          >
            <div
              style={{
                fontSize: 11,
                letterSpacing: 3,
                textTransform: "uppercase",
                color: C.emberDim,
                fontWeight: 700,
              }}
            >
              운명의 던전
            </div>
            <div
              style={{
                fontFamily: MONO,
                fontSize: 13,
                color: C.parch,
                background: C.floor,
                border: `1px solid ${C.edge}`,
                borderRadius: 6,
                padding: "2px 9px",
              }}
            >
              {s.depth}층
            </div>
          </div>

          <div
            style={{
              marginTop: 10,
              display: "flex",
              alignItems: "center",
              gap: 12,
            }}
          >
            <div style={{ flex: 1 }}>
              <div
                style={{
                  height: 9,
                  borderRadius: 5,
                  background: C.edge,
                  overflow: "hidden",
                  border: `1px solid ${C.edge}`,
                }}
              >
                <div
                  style={{
                    width: `${Math.max(0, hpRatio * 100)}%`,
                    height: "100%",
                    background: hpColor,
                    transition: "width .2s, background .2s",
                  }}
                />
              </div>
              <div
                style={{
                  fontFamily: MONO,
                  fontSize: 11,
                  color: C.parchDim,
                  marginTop: 3,
                }}
              >
                HP {Math.max(0, s.player.hp)}/{s.player.maxHp}
              </div>
            </div>
            <Stat label="Lv" value={s.player.lvl} />
            <Stat label="힘" value={s.player.str} />
            <Stat label="✦" value={s.player.gold} accent />
          </div>
        </header>

        {/* ── 맵 ── */}
        <div
          onTouchStart={onTouchStart}
          onTouchEnd={onTouchEnd}
          style={{
            padding: "8px 12px 4px",
            display: "flex",
            justifyContent: "center",
          }}
        >
          <div
            style={{
              width: "100%",
              maxWidth: 360,
              display: "grid",
              gridTemplateColumns: `repeat(${W}, 1fr)`,
              gap: 1,
              background: C.void,
              border: `1px solid ${C.edge}`,
              borderRadius: 8,
              padding: 6,
            }}
          >
            {Array.from({ length: H }).map((_, y) =>
              Array.from({ length: W }).map((__, x) => {
                const ch = cellAt(x, y);
                const st = glyphStyle(ch);
                const d = Math.max(
                  Math.abs(x - s.player.x),
                  Math.abs(y - s.player.y)
                );
                const bright = Math.max(0.16, 1 - d * 0.12);
                const isPlayer = ch === "@";
                return (
                  <button
                    key={x + "-" + y}
                    onClick={() => act({ type: "tap", x, y })}
                    aria-label={`${x},${y}`}
                    style={{
                      aspectRatio: "1 / 1",
                      display: "flex",
                      alignItems: "center",
                      justifyContent: "center",
                      background: "transparent",
                      border: "none",
                      padding: 0,
                      cursor: "pointer",
                      fontFamily: MONO,
                      fontSize: "clamp(13px,4.6vw,19px)",
                      fontWeight: st.weight,
                      color: st.color,
                      opacity: isPlayer ? 1 : bright,
                    }}
                  >
                    <span className={isPlayer ? "glyph-player" : ""}>{ch}</span>
                  </button>
                );
              })
            )}
          </div>
        </div>

        {/* ── 메시지 로그 ── */}
        <div
          style={{
            padding: "8px 16px",
            minHeight: 58,
            display: "flex",
            flexDirection: "column",
            justifyContent: "center",
            gap: 2,
          }}
        >
          {recent.map((m, i) => (
            <div
              key={i}
              style={{
                fontSize: 13.5,
                color: i === 0 ? C.parch : C.parchDim,
                opacity: i === 0 ? 1 : 0.6 - i * 0.12,
                display: "flex",
                gap: 6,
              }}
            >
              <span style={{ color: C.emberDim }}>▸</span>
              <span>{m}</span>
            </div>
          ))}
        </div>

        {/* ── 컨트롤 ── */}
        <div
          style={{
            marginTop: "auto",
            padding: "10px 16px 22px",
            display: "flex",
            gap: 14,
            alignItems: "flex-end",
          }}
        >
          {/* D-패드 */}
          <div
            style={{
              display: "grid",
              gridTemplateColumns: "repeat(3, 46px)",
              gridTemplateRows: "repeat(3, 46px)",
              gap: 4,
            }}
          >
            {dpad.map(([g, dx, dy], i) => {
              const center = dx === 0 && dy === 0;
              return (
                <button
                  key={i}
                  className="press"
                  onClick={() =>
                    center ? act({ type: "wait" }) : act({ type: "move", dx, dy })
                  }
                  aria-label={center ? "기다리기" : `이동 ${g}`}
                  style={{
                    border: `1px solid ${center ? C.edge : C.edge2}`,
                    background: center ? C.void : C.floor,
                    color: center ? C.emberDim : C.parch,
                    borderRadius: 8,
                    fontSize: 18,
                    fontFamily: MONO,
                    transition: "transform .05s",
                  }}
                >
                  {g}
                </button>
              );
            })}
          </div>

          {/* 액션 버튼 */}
          <div
            style={{
              flex: 1,
              display: "grid",
              gridTemplateColumns: "1fr 1fr 1fr",
              gap: 7,
            }}
          >
            {actions.map(({ label, Icon, a }) => (
              <button
                key={label}
                className="press"
                onClick={() => onAction(a)}
                style={{
                  display: "flex",
                  flexDirection: "column",
                  alignItems: "center",
                  gap: 3,
                  padding: "9px 0",
                  background: C.floor,
                  border: `1px solid ${C.edge2}`,
                  borderRadius: 9,
                  color: C.parch,
                  transition: "transform .05s",
                }}
              >
                <Icon size={18} color={C.ember} strokeWidth={1.8} />
                <span style={{ fontSize: 11.5, fontWeight: 600 }}>{label}</span>
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* ── 바텀시트 ── */}
      {sheet && (
        <div
          onClick={() => setSheet(null)}
          style={{
            position: "fixed",
            inset: 0,
            background: "#000000aa",
            display: "flex",
            alignItems: "flex-end",
            justifyContent: "center",
            zIndex: 20,
          }}
        >
          <div
            className="sheet"
            onClick={(e) => e.stopPropagation()}
            style={{
              width: "100%",
              maxWidth: 440,
              background: C.floor,
              borderTop: `1px solid ${C.edge2}`,
              borderRadius: "16px 16px 0 0",
              padding: "18px 18px 26px",
            }}
          >
            <div
              style={{
                width: 38,
                height: 4,
                background: C.edge2,
                borderRadius: 2,
                margin: "0 auto 14px",
              }}
            />
            <div
              style={{
                fontSize: 12,
                letterSpacing: 2,
                textTransform: "uppercase",
                color: C.emberDim,
                fontWeight: 700,
                marginBottom: 12,
              }}
            >
              {sheet === "inv" ? "소지품" : "추가 명령"}
            </div>

            {sheet === "inv" ? (
              s.inventory.map((o, i) => (
                <button
                  key={i}
                  onClick={() => {
                    dispatch({ type: "use", idx: i });
                    setSheet(null);
                  }}
                  style={rowStyle}
                >
                  <span
                    style={{
                      fontFamily: MONO,
                      color: glyphStyle(o.glyph).color,
                      width: 20,
                      textAlign: "center",
                    }}
                  >
                    {o.glyph}
                  </span>
                  <span style={{ flex: 1, textAlign: "left", color: C.parch }}>
                    {o.name}
                    {o.note && (
                      <span style={{ color: C.emberDim, fontSize: 12 }}>
                        {" "}
                        · {o.note}
                      </span>
                    )}
                  </span>
                  {!o.note && (
                    <span style={{ color: C.parchDim, fontFamily: MONO }}>
                      ×{o.qty}
                    </span>
                  )}
                </button>
              ))
            ) : (
              ["장착", "입기", "벗기", "마시기", "읽기", "던지기", "도움말"].map(
                (cmd) => (
                  <button
                    key={cmd}
                    onClick={() => {
                      dispatch({ type: "command", name: cmd });
                      setSheet(null);
                    }}
                    style={rowStyle}
                  >
                    <span style={{ flex: 1, textAlign: "left", color: C.parch }}>
                      {cmd}
                    </span>
                    <span style={{ color: C.parchDim }}>›</span>
                  </button>
                )
              )
            )}
          </div>
        </div>
      )}

      {/* ── 사망 오버레이 ── */}
      {s.dead && (
        <div
          style={{
            position: "fixed",
            inset: 0,
            background: "#000000d8",
            display: "flex",
            flexDirection: "column",
            alignItems: "center",
            justifyContent: "center",
            gap: 14,
            zIndex: 30,
            fontFamily: KR,
          }}
        >
          <div style={{ fontSize: 46 }}>☠</div>
          <div style={{ color: C.blood, fontSize: 20, fontWeight: 700 }}>
            당신은 쓰러졌다
          </div>
          <div style={{ color: C.parchDim, fontSize: 14 }}>
            던전 {s.depth}층까지 도달했다 · 금화 {s.player.gold}닢
          </div>
          <button
            onClick={() => dispatch({ type: "reset" })}
            style={{
              marginTop: 8,
              padding: "11px 26px",
              background: C.ember,
              color: C.void,
              border: "none",
              borderRadius: 10,
              fontWeight: 700,
              fontSize: 15,
            }}
          >
            새 게임
          </button>
        </div>
      )}
    </div>
  );
}

function Stat({ label, value, accent }) {
  return (
    <div style={{ textAlign: "center", minWidth: 26 }}>
      <div
        style={{
          fontSize: 10,
          color: accent ? C.ember : C.parchDim,
          fontWeight: 700,
        }}
      >
        {label}
      </div>
      <div
        style={{
          fontFamily: MONO,
          fontSize: 15,
          color: accent ? C.ember : C.parch,
          fontWeight: 600,
        }}
      >
        {value}
      </div>
    </div>
  );
}

const rowStyle = {
  width: "100%",
  display: "flex",
  alignItems: "center",
  gap: 10,
  padding: "12px 4px",
  background: "transparent",
  border: "none",
  borderBottom: `1px solid ${C.edge}`,
  fontSize: 15,
  fontFamily: KR,
  cursor: "pointer",
};
