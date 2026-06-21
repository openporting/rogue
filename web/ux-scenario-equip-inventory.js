/* ux-scenario-equip-inventory.js — drive the REAL touch UI through the
 * equip / inventory / wear / take-off / eat flows and print a human-readable
 * trace, broken down into UI-interaction units (one line per finger action and
 * the UI/engine reaction it produced).
 *
 * Same rig as ux-walkthrough-test.js: the real web/index.html UI on the shared
 * DOM shim, wired to the real bridge.js + rogue.js (WASM). This is a narrated
 * walkthrough, not a pass/fail matrix — but it still exits non-zero if a
 * scenario's expected end-state never materialises, so it stays trustworthy.
 *
 *   prereq:  bash build.sh
 *   run:     node web/ux-scenario-equip-inventory.js
 */
"use strict";
const { createHarness, bootEngine, sleep, waitFor } = require("./ux-dom-harness");

const H = createHarness();
const { B, byId, click, cellAt } = H;
const { actions, scrim, ovlScrim } = H.els;
bootEngine();

let problems = 0;
const msgs = () => B.getMessages();
const scrimOpen = () => scrim.classList.contains("open");
const ovlOpen = () => ovlScrim.classList.contains("open");
const cmd = (re) => H.findChild(byId.cmds, (b) => re.test(b.textContent));
const letter = (ch) => H.findChild(byId.letters, (b) => b.textContent === ch);
const moreBtn = () => H.findChild(actions, (b) => /더보기/.test(b.textContent));

function banner(title) {
  console.log("\n══════════════════════════════════════════════════════════════════");
  console.log(" " + title);
  console.log("══════════════════════════════════════════════════════════════════");
}

/* Run one finger action, then report the UI-interaction unit it produced:
 * what the user touched, how the UI reacted, and what the engine said back. */
let stepNo = 0;
async function act(touch, doIt, opts = {}) {
  const before = msgs().length;
  const sheetWas = scrimOpen(), ovlWas = ovlOpen();
  doIt();
  if (opts.until) await waitFor(opts.until, opts.label || touch, opts.timeout || 6000);
  await sleep(opts.settle != null ? opts.settle : (opts.pause || 300)); // let the trailing completion message flush onto THIS step

  const news = msgs().slice(0, msgs().length - before).reverse(); // chronological
  const ui = [];
  if (scrimOpen() && !sheetWas) ui.push("하단 바텀시트(추가 명령) 슬라이드업으로 열림");
  if (!scrimOpen() && sheetWas) ui.push("명령 바텀시트 닫힘");
  if (ovlOpen() && !ovlWas) ui.push("오버레이 바텀시트 열림");
  if (!ovlOpen() && ovlWas) ui.push("오버레이 바텀시트 닫힘");
  const pk = B.getPrompt();
  if (pk === "item") ui.push("엔진이 항목 선택 대기 → a–z 응답 키패드 자동 노출 (맵 탭 차단 모드)");
  if (pk === "yesno") ui.push("엔진이 예/아니오 대기 → 응답 키패드 자동 노출");

  console.log("  " + (++stepNo) + ". [탭] " + touch);
  ui.forEach((u) => console.log("         UI → " + u));
  // when the sheet is up it dims the log; show what the sheet itself displays so
  // the trace reflects what the player can actually READ while answering.
  if (scrimOpen() && byId["sheet-msg"].textContent.trim()) {
    byId["sheet-msg"].textContent.split("\n").forEach((l) => console.log("         시트 표시 → " + l));
  }
  news.forEach((m) => console.log("         엔진 → 로그: \"" + m + "\""));
  if (opts.dumpOverlay && ovlOpen()) {
    console.log("         소지품 시트 내용 ┐");
    byId["ovl-body"].textContent.split("\n").forEach((l) => console.log("             │ " + l));
    console.log("             ┘");
  }
  if (!ui.length && !news.length) console.log("         (UI/로그 변화 없음)");
  if (opts.expect && !opts.expect()) { console.log("         ⚠ 기대 상태 불충족"); problems++; }
}

const closeSheet = () => { if (scrimOpen()) H.dispatch(scrim, "click", { target: scrim }); };

(async () => {
  await waitFor(() => B.getPlayer() !== null, "engine boot", 12000);
  console.log("엔진 부팅 완료 — 시작 소지품으로 장착/소지품 UI 시나리오를 진행합니다.");
  console.log("(각 줄 = 사용자의 한 번의 탭과 그에 대한 UI/엔진 반응 = 상호작용 단위)");

  /* ── 시나리오 1: 무기 장착 정상 흐름 (☰더보기 → 장착 w → 글자 선택) ── */
  banner("시나리오 1 — 무기 장착 (장착/w) 정상 흐름");
  await act("액션바 「☰ 더보기」", () => click(moreBtn()), { expect: () => scrimOpen() });
  await act("명령그리드 「장착 (w)」", () => click(cmd(/장착/)),
    { until: () => B.getPrompt() === "item", label: "장착 프롬프트", expect: () => scrimOpen() });
  await act("글자패드 「d」 (단궁/무기)", () => click(letter("d")),
    { until: () => B.getPrompt() === null && msgs().some((m) => /들었다|장착|wielding/.test(m)), label: "장착 완료",
      expect: () => B.getPrompt() === null });
  closeSheet();

  /* ── 시나리오 2: 잘못된 항목 선택 후 회복 (forgiving + --More-- 무freeze) ── */
  banner("시나리오 2 — 잘못 골랐을 때 회복 (장착/w → 잘못된 글자 → 다시 선택)");
  await act("액션바 「☰ 더보기」", () => click(moreBtn()), { expect: () => scrimOpen() });
  await act("명령그리드 「장착 (w)」", () => click(cmd(/장착/)),
    { until: () => B.getPrompt() === "item", label: "장착 프롬프트" });
  await act("글자패드 「z」 (무기 아님 — 일부러 오선택)", () => click(letter("z")),
    { until: () => B.getPrompt() === "item" && msgs().some((m) => /올바른 항목이 아니다/.test(m)),
      label: "거부 + 재프롬프트(--More-- 자동 진행으로 freeze 없음)",
      expect: () => B.getPrompt() === "item" && scrimOpen() });
  await act("글자패드 「c」 (철퇴/무기 — 올바른 선택)", () => click(letter("c")),
    { until: () => B.getPrompt() === null && msgs().some((m) => /\(c\)|들었다|wielding/.test(m)),
      label: "재선택 완료", expect: () => B.getPrompt() === null });
  closeSheet();

  /* ── 시나리오 3: 소지품 열람 (☰더보기 → 소지품 i → 계속) ── */
  banner("시나리오 3 — 소지품 열람 (소지품/i)");
  await act("액션바 「☰ 더보기」", () => click(moreBtn()), { expect: () => scrimOpen() });
  await act("명령그리드 「소지품 (i)」", () => click(cmd(/소지품/)),
    { until: () => ovlOpen(), label: "소지품 오버레이", dumpOverlay: true, expect: () => ovlOpen() });
  await act("오버레이 「계속 ▸」", () => click(byId["ovl-go"]),
    { until: () => !ovlOpen(), label: "오버레이 닫힘", expect: () => !ovlOpen() });

  /* ── 시나리오 4: 갑옷 벗기 → 입기 (갑옷벗기 T, 입기 W) ── */
  banner("시나리오 4 — 갑옷 벗기/입기 (갑옷벗기/T → 입기/W)");
  await act("액션바 「☰ 더보기」", () => click(moreBtn()), { expect: () => scrimOpen() });
  await act("명령그리드 「갑옷벗기 (T)」", () => click(cmd(/갑옷벗기/)),
    { pause: 220, label: "갑옷 벗기" });
  // 벗기 후 입기: get_item(갑옷) 프롬프트가 뜨면 글자로 응답.
  if (scrimOpen()) closeSheet();
  await act("액션바 「☰ 더보기」", () => click(moreBtn()), { expect: () => scrimOpen() });
  await act("명령그리드 「입기 (W)」", () => click(cmd(/입기/)), { pause: 260 });
  if (B.getPrompt() === "item") {
    await act("글자패드 「b」 (사슬 미늘 갑옷)", () => click(letter("b")),
      { until: () => B.getPrompt() === null, label: "갑옷 입기 완료" });
  }
  closeSheet();

  /* ── 시나리오 5: 음식 먹기 (먹기 e) ── */
  banner("시나리오 5 — 음식 먹기 (먹기/e)");
  await act("액션바 「☰ 더보기」", () => click(moreBtn()), { expect: () => scrimOpen() });
  await act("명령그리드 「먹기 (e)」", () => click(cmd(/먹기/)), { pause: 260 });
  if (B.getPrompt() === "item") {
    await act("글자패드 「a」 (식량)", () => click(letter("a")),
      { until: () => B.getPrompt() === null, label: "먹기 완료" });
  }
  closeSheet();

  console.log("\n──────────────────────────────────────────────────────────────────");
  if (problems === 0) {
    console.log("완료: 장착/소지품/입기/벗기/먹기 UI 흐름이 실제 엔진에서 끝까지 동작했습니다.");
    process.exit(0);
  } else {
    console.log("주의: " + problems + "개 단계에서 기대 상태가 충족되지 않았습니다(위 ⚠ 참고).");
    process.exit(1);
  }
})();
