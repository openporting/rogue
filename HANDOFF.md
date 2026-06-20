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
│  ├─ web_curses.c             ← curses 셰임 + md_readchar + web_emit_msg.
│  │                              dual-mode(네이티브/emscripten). -Wall 무경고 검증됨.
│  ├─ i18n.h / i18n.c          ← EN→KO `tr_msg()`(정적 메시지 테이블 ~101개, 미등록은
│  │                              passthrough) + UTF-8 조사 엔진(kr_eul_reul 등).
│  │                              `gcc -DI18N_DEMO`로 자가검증. (패치 #2가 endmsg에서 호출)
└─ web/
   └─ bridge.js                ← WASM 엔진 ↔ 터치 UI 연결. 화면버퍼/메시지/입력큐 +
                                  RogueInput(터치→키맵). 하단에 UI 결선 4단계 가이드.
```

## 5. 현재 상태 (정확히)
- ✅ **UI 프로토타입**: 동작 확정. 입력 방식(탭+스와이프+D-패드 전부) / 레이아웃 사용자 승인 완료("지금처럼").
- ✅ **브리지 C**: `gcc -DWEBCURSES_DEMO -Wall` 컴파일·실행 검증(스모크 테스트: `@` 배치 + 한글 메시지 emit + 키 echo).
- ✅ **JS 브리지 / 빌드 스크립트 / 패치 명세**: 작성 완료.
- ✅ **emcc 풀빌드 완료**: `build.sh`로 `web/rogue.js`(+`rogue.wasm`) 생성. 33개 원본 `.c` 전부 컴파일 + 링크 성공.
- ✅ **런타임 검증(Node)**: 엔진이 던전을 렌더(드로콜 2000+), `@`/몬스터/아이템/상태줄 표시, 키 입력→이동/공격/계단/종료 동작 확인. 브라우저용 최소 터미널 렌더러 `web/index.html` 추가.
- ✅ **메시지 훅(패치 #2) 적용 + i18n 1차**: `build.sh`가 `io.c` endmsg()의 상단 라인 그리기를
  `web_emit_msg(tr_msg(msgbuf))`로 멱등 치환(클론마다 자동, gitignore된 `rogue/` 무수정 커밋).
  `webcurses/i18n.c`에 정적 메시지 ~101개 한글 테이블 + UTF-8 조사 엔진 작성·검증.
  미등록 메시지는 영어 passthrough라 게임은 안 깨짐.
- ❌ **아직 안 된 것**:
  1. **메시지 i18n 2차(동적/조각 메시지)**: 아이템·몬스터 이름이 끼는 조각 concat 메시지
     (`"there is %s to pick up"` 등)는 아직 영어 passthrough. 키 템플릿으로 리팩터 + 한글
     이름 테이블 + `kr_*` 조사 적용 필요(§8). 상태줄 stats 훅(§9.3)도 미연결.
  2. **모바일 터치 UI 결선**(프로토타입 → 실엔진, §9). 현재 `index.html`은 키보드/간이 D-패드만.
  3. **INV_OVER 인벤/도움말 오버레이**가 별도 윈도(`tw`/`sw`)로 그려져 JS UI에 안 뜸(§11, 바텀시트로 분기 필요).

### 5.1 emcc 빌드에서 실제로 필요했던 것 (이번 핸드오프에서 해결)
설계상 "첫 컴파일에서 확장" 전제대로, 다음을 추가/수정함:
- **`webcurses/config.h` 신규**: 원본은 `./configure`로 `config.h`를 생성하지만 브라우저/musl에서 의미 있는 feature test가 불가. emscripten이 실제 지원하는 능력만 정의(`HAVE_TERMIOS_H`/`HAVE_PWD_H`/`HAVE_ERASECHAR` 등), 미지원은 정의하지 않음(`HAVE_TERM_H`/`HAVE_WORKING_FORK`/`HAVE_GETLOADAVG` 제외 → termcap·fork·loadav 코드 컴파일 아웃). `-I webcurses`라 `extern.h`의 `#include "config.h"`가 여기로 잡힘(원본 트리 무수정).
- **`curses.h`/`web_curses.c` 심볼 보강**: 첫 컴파일이 요구한 것 전부 추가 — `<stdio.h>/<stdbool.h>/<stdarg.h>` 포함(FILE/bool/va_list), `A_CHARTEXT`, `unctrl`, `flushinp`, `halfdelay`, `isendwin`, `mvwprintw`, `wgetnstr`, `getmaxx/getmaxy`, `mvwinch`, `werase`, `wclrtoeol`, `mvwaddch`, `subwin`/`mvwin`, 터미널 모드 no-op(`raw/noecho/keypad/clearok/...`), `erasechar/killchar`, `KEY_*` 상수(가드 밖에서 쓰는 것만), `_getch`→`getch`, `_cury/_curx`→`cy/cx`, `CE`(NULL).
- **`newwin`/`subwin` 분리 할당**: 기존 단일 static을 malloc 분리(INV_OVER가 `hw`+`tw`+`sw` 동시 사용·복사하므로 aliasing 방지). `delwin`은 free.
- **`build.sh` 수정 (중요)**:
  - `vers.c`/`wizard.c`를 **다시 포함**(원본 제외 목록은 잘못 — `encstr`/`statlist`/`whatis`/`create_obj`/`passwd` 등 링크에 필수).
  - `md_readchar` 비활성화 `-D`를 **`mdport.c` 단독 컴파일에만** 적용. 전역 적용하면 `io.c`의 *호출부*까지 바뀌어 브리지를 우회하고 심볼 중복됨.
  - `-sEMULATE_FUNCTION_POINTER_CASTS=1` 추가: 데몬/퓨즈 디스패치(`(*d_func)(arg)`, `void(*)()`에 인자 1개)가 WASM 엄격 시그니처 검사에서 트랩 → 이 플래그로 해결.

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
1. ✅ **emcc 첫 빌드 + 디버깅 패스**: 완료(§5.1). `build.sh` → `web/rogue.js`/`rogue.wasm`, Node에서 렌더·입력 동작 확인, `web/index.html`로 브라우저 구동 가능.
2. ✅/◐ **메시지 훅 + i18n.c**(패치 #2, §8) → `endmsg()`에서 `web_emit_msg(tr_msg(...))` 호출(완료). 정적 메시지 한글화 완료. **남은 것: 동적/조각 메시지**(아이템·몬스터 이름) 키 템플릿 리팩터 + 한글 이름 테이블 + 조사 적용.
3. **UI 결선**(9단계) → React 터치 프로토타입을 실엔진에 연결, 터치로 실제 플레이.
4. **INV_OVER 오버레이 → JS 바텀시트 분기**(§11) → 인벤토리/도움말 메뉴 표시.
5. 기기 테스트·세이브 영속화(IDBFS `FS.syncfs`)·정적 호스팅 배포.

## 11. 알려진 리스크
- Asyncify로 `getch` 블로킹을 푸는 구조라, 스택 사이즈(`-sASYNCIFY_STACK_SIZE`) 조정이 필요할 수 있음. `-sEMULATE_FUNCTION_POINTER_CASTS=1`과 함께 쓰고 있는데(데몬 디스패치 때문) 둘 다 켠 상태로 빌드/구동은 확인됨. 장기적으로는 데몬/퓨즈 함수 시그니처를 통일해 `EMULATE_FUNCTION_POINTER_CASTS`를 떼는 게 더 가벼움.
- 인벤토리/도움말은 기본 `inv_type = INV_OVER`라 별도 윈도(`tw`/`sw`)에 그려짐. `newwin`/`subwin`을 분리 malloc해 크래시는 없지만, 이 윈도는 `stdscr`가 아니라 **JS UI로 push되지 않음** → 메뉴가 화면에 안 뜸. `wrefresh(w!=stdscr)` 경로를 JS 오버레이/바텀시트로 분기해야 함(§10.4).
- 메시지(패치 #2)가 아직 미적용이라 게임 메시지가 화면 0행에 영어로 그려짐 — i18n 단계에서 `web_emit_msg`로 전환.
- 세이브/스코어 영속화(IDBFS)는 아직 미연결 — 현재 MEMFS라 새로고침 시 세이브 소실.
