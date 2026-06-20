# HANDOFF — 오리지널 Rogue 한글 + 모바일 터치 웹 포팅

다른 에이전트가 이 문서만으로 작업을 이어받을 수 있도록 정리한 인계 문서입니다.

## 1. 목표
1980년 로그라이크 원조 **Rogue** 를 ① 한글화(대사·메시지·메뉴 전체), ② 웹 기반 **모바일 터치 UI** 로 포팅한다. 맵 글리프(`@`, 몬스터=알파벳, 아이템 기호)는 번역하지 않고 ASCII 유지한다(원본 충실).

## 2. 소스 베이스라인
- **Davidslv/rogue** (Rogue 5.4.4, Toy/Arnold/Wichman). `git clone --depth 1 https://github.com/Davidslv/rogue.git`
- 라이선스: 저장소 `LICENSE.TXT`(Rogue Restoration 계열). 개인·팬 포팅은 대체로 가능하나 배포 전 확인 + 원저자 크레딧 유지. (참고: Tim Stoehr의 rogue-clone 계열은 비상업 조건이 붙음 — 이쪽은 사용하지 않음.)
- 본 패키지에는 원본 소스를 포함하지 않음(외부 의존). 위 명령으로 `rogue/`에 받아서 빌드.

## 3. 아키텍처 결정 (핵심)
**"엔진은 원본 C를 WASM으로 그대로 컴파일, UI는 새로 작성한다."**
NetHackJS 패턴(엔진/렌더링 분리)을 Rogue에 적용. Rogue는 윈도우 추상화가 없고 코드 전반이 표준 curses를 직접 호출하므로, **curses 계층을 통째로 얇은 셰임으로 교체**해 그리기·입력·메시지를 JS로 빼낸다.

### 검증된 가로채기 지점 — 딱 3곳 (grep으로 확인)
| 구분 | 병목 | 위치 | 처리 |
|---|---|---|---|
| 그리기 | `move/addch/mvaddch/addstr/standout/refresh/clear/box` 등 ~15개 | `*.c` → stdscr, scratch win `hw` | `web_curses.c`가 가상 80×25 버퍼로 받아 `RogueBridge.drawCell/refresh`로 전송 |
| 입력 | `readchar() → md_readchar()` 단일 경로 | `io.c:151`, `mdport.c` | `web_curses.c`의 `md_readchar()`가 `RogueBridge.popKey()` 큐에서 키를 뺌(Asyncify로 양보) |
| 메시지 | `msg()/addmsg() → doadd() → endmsg()` 단일 경로 | `io.c:24~84` | `endmsg()` 끝에 `web_emit_msg(번역문)` 호출 → 우리 양피지 로그 |

### 확정된 키맵 (command.c 기준)
이동 `h`←/`l`→/`k`↑/`j`↓, 대각선 `y`↖`u`↗`b`↙`n`↘ · 줍기 `,` · 인벤 `i` · 계단 `>`(하강)/`<`(상승) · 검색 `s` · 쉬기 `.` · 드로어용 `q`(마시기)`r`(읽기)`w`(장착)`W`(입기)`e`(먹기)`t`(던지기)`?`(도움말).

## 4. 파일 구성
```
rogue-kr-port/
├─ HANDOFF.md                  ← 이 문서
├─ build.sh                    ← Emscripten 빌드 + 원본 패치 3개 설명
├─ prototype/
│  └─ rogue-touch-prototype.jsx  ← 모바일 터치 UI 프로토타입(React). 손맛 확정본.
│                                  횃불 시야 + 조사 자동처리(을/를·이/가) 시연 포함.
├─ webcurses/
│  ├─ curses.h                 ← Rogue의 <curses.h>를 가로채는 대체 헤더
│  └─ web_curses.c             ← curses 셰임 + md_readchar + web_emit_msg.
│                                 dual-mode(네이티브/emscripten). -Wall 무경고 검증됨.
└─ web/
   └─ bridge.js                ← WASM 엔진 ↔ 터치 UI 연결. 화면버퍼/메시지/입력큐 +
                                  RogueInput(터치→키맵). 하단에 UI 결선 4단계 가이드.
```

## 5. 현재 상태 (정확히)
- ✅ **UI 프로토타입**: 동작 확정. 입력 방식(탭+스와이프+D-패드 전부) / 레이아웃 사용자 승인 완료("지금처럼").
- ✅ **브리지 C**: `gcc -DWEBCURSES_DEMO -Wall` 컴파일·실행 검증(스모크 테스트: `@` 배치 + 한글 메시지 emit + 키 echo).
- ✅ **JS 브리지 / 빌드 스크립트 / 패치 명세**: 작성 완료.
- ❌ **아직 안 된 것**: 실제 `emcc` 풀빌드와 플레이테스트. (작업 샌드박스에 emsdk 부재) → **이게 다음 핸드오프의 첫 작업.**

## 6. 빌드 방법
```bash
# 1) 사전준비
git clone --depth 1 https://github.com/Davidslv/rogue.git   # -> rogue/
source /path/to/emsdk/emsdk_env.sh                          # emscripten 활성화
# 2) 디렉터리 배치: rogue/ 와 rogue-kr-port/ 를 형제로 두거나 build.sh의 경로 수정
# 3) 빌드
bash rogue-kr-port/build.sh    # -> web/rogue.js + rogue.wasm
```
주요 플래그: `-I webcurses`(우리 curses.h 우선), `-sASYNCIFY`(blocking getch→`emscripten_sleep`), `-sFORCE_FILESYSTEM`(세이브), `-sALLOW_MEMORY_GROWTH`.

## 7. 원본 패치 3개 (build.sh에도 명시)
1. **include**: 패치 불필요 — `-I webcurses`로 `#include <curses.h>`가 자동으로 셰임에 연결.
2. **메시지 훅**: `io.c`의 `endmsg()` 끝에서 완성된 메시지 버퍼를 `web_emit_msg(tr_msg(buf))`로 보내고 curses 메시지 라인 그리기는 생략. **여기서 한글화 리팩터링이 일어남**(아래 8).
3. **입력**: `mdport.c`의 `md_readchar`를 `-Dmd_readchar=__rogue_native_readchar_unused`로 비활성화해 셰임 정의가 우선되게. `md_*tty`/시그널 관련 터미널 호출은 브라우저용 no-op 스텁.

추가: 세이브/스코어 파일은 `FORCE_FILESYSTEM` + IDBFS 마운트 후 `FS.syncfs()`로 영속화.

## 8. 한글화(i18n) 전략 — 코드로 입증된 난점
영어 원본은 메시지를 **조각으로 이어 붙임**: `msg("there is ") … msg(" to pick up")`, `msg("I see ")`. 한국어는 어순이 달라 **단순 치환이 깨짐**. 따라서:
- 조각 concat 지점들을 **단일 키 포맷 문자열로 합치는 리팩터링**이 필요.
  예: `"there is %s to pick up"` → 키 `PICKUP_HERE` → `"여기 %s이(가) 있다."`
- **조사 자동 처리**는 프로토타입 `rogue-touch-prototype.jsx`에 구현돼 있음(`hasBatchim`/`eulReul`/`iGa`). 이 로직을 `i18n.c`의 `tr_msg()`로 이식하면 됨.
- 상태줄(`io.c` status, `"Level: %d Gold:..."`)은 **번역하지 않음** — 값만 `RogueBridge.stats({...})`로 보내 JS 상태바에서 렌더(프로토타입에 이미 있음).
- **다음 작업의 큰 덩어리 = `i18n.c` 메시지 테이블 채우기.** 모든 `msg("...")` 사이트를 키로 추출 → 한국어 포맷 작성. (전수 추출은 `grep -rhoE 'msg\("[^"]+"' *.c` 로 시작)

## 9. UI 결선 (프로토타입 → 실엔진)
`web/bridge.js` 하단 주석의 4단계 그대로:
1. 프로토타입의 mock `useReducer` 제거 → `RogueBridge.subscribe(force)`로 리렌더, 맵은 `getScreen()`(던전 행만), 로그는 `getMessages()`.
2. 컨트롤 `dispatch(...)` → `RogueInput.move/tap/pickup/inventory/descend/search/rest/command(...)`.
3. 상태바는 C의 status 훅(`RogueBridge.stats`)에서 받아 기존 JS 상태바 유지.
4. `index.html` 로드 순서: `bridge.js` → `rogue.js`.

## 10. 다음 마일스톤 (권장 순서)
1. **emcc 첫 빌드 + 디버깅 패스**: 누락 심볼은 `curses.h`/`web_curses.c`에 추가, mdport 스텁, 세이브 IDBFS. → 터미널이라도 브라우저에서 돌게.
2. **UI 결선**(9단계) → 터치로 실제 플레이.
3. **i18n.c 채우기**(8) → 한글 메시지 전면 적용.
4. 기기 테스트·세이브 영속화·정적 호스팅 배포.

## 11. 알려진 리스크
- Asyncify로 `getch` 블로킹을 푸는 구조라, 스택 사이즈(`-sASYNCIFY_STACK_SIZE`) 조정이 필요할 수 있음.
- Rogue의 `hw` scratch 윈도우(인벤토리/도움말 목록)는 현재 단일 정적 윈도우로 처리 — 메뉴를 바텀시트로 빼려면 `wrefresh(scratch)` 경로를 JS 오버레이로 분기 필요.
- 첫 빌드에서 `curses.h`가 커버 못 한 매크로/심볼이 나올 수 있음(설계상 "첫 컴파일에서 확장" 전제).
