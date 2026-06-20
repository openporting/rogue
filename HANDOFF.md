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
│  │                             dual-mode(네이티브/emscripten). -Wall 무경고 검증됨.
│  ├─ i18n.h / i18n.c          ← EN→KO 번역(tr_msg) + 한글 조사 엔진. endmsg()가 조립한
│  │                             영어 문장을 한글로 변환(매칭 안 되면 영어 폴백). 두 엔진을
│  │                             합본: ① 1차 = 정확표 + 접두/접미 프레임 + fight.c 전투동사표
│  │                             + 명사사전(몬스터/색/아이템). ② 2차 fallback(`mz_*`) = 패턴
│  │                             테이블(%s→$N 캡처, `@N` 액터정규화)로 1차 미커버 메시지 처리.
│  │                             자체 테스트 `cc -DI18N_TEST`.
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
- ✅ **메시지 훅(패치 #2) + i18n 적용**: `build.sh`가 `io.c`의 `endmsg()` 상단 라인
  그리기(`mvaddstr(0,0,msgbuf)`)를 `web_emit_msg(tr_msg(msgbuf))`로 자동 치환(idempotent sed).
  `webcurses/i18n.c`는 **두 i18n 구현의 합본**(병렬로 개발된 PR #2 + PR #3을 통합):
  - **1차 엔진**(주): 조립된 영어 문장을 정확표 + 접두/접미 프레임 + fight.c 전투동사표(CVERB)로
    매칭, **명사 사전(몬스터 26 + 색상 + 기본 아이템)** 으로 이름까지 한글화. 조사(을/를·이/가·
    은/는·와/과·(으)로)를 받침 기준 자동 선택. → "여기 단검을 주울 수 있다.", "당신이 박쥐를 맞혔다."
  - **2차 fallback 엔진**(`mz_*`, 1차가 못 잡은 메시지만): %s/%d/%c→`$N` 캡처 패턴 + `@N` 액터
    정규화 + `$N{을}` 조사. 프롬프트·장비(착용/해제/반지)·허기 단계·상태효과·오류·메두사 시선·
    함정 발견 등을 커버. 1차가 항상 우선(중복 시 1차 승).
  - **검증**: `cc -DI18N_TEST i18n.c` 자체 테스트(양 엔진) 통과. 커버리지 하네스로 원본 메시지
    전수 통과 → 미번역 **128→61**로 절반 이하(2차 합본 효과). 미매칭은 영어 폴백.
- ❌ **아직 안 된 것**:
  1. **아이템 풀네임 한글화** → `inv_name`의 `+1,+2 메이스`·두루마리/물약 식별명·재질 등. 기본
     아이템 명사 사전은 있으나 수식·강화치·식별상태까지는 미번역(영어 폴백).
  2. **인벤/도움말 텍스트**는 `endmsg()`가 아니라 INV_OVER 윈도(`tw`/`sw`)로 그려져 `tr_msg`를 안 탐 → §11/4의 오버레이 분기와 함께 처리.
  3. **모바일 터치 UI 결선**(프로토타입 → 실엔진, §9). 현재 `index.html`은 키보드/간이 D-패드만.
  4. **INV_OVER 인벤/도움말 오버레이**가 별도 윈도(`tw`/`sw`)로 그려져 JS UI에 안 뜸(§11, 바텀시트로 분기 필요).
  5. **상태줄(status)·`--More--` 페이징**: 상태줄은 JS 상태바로 분리(§9.3) 예정, `--More--`는
     이제 메시지가 로그로 가므로 자동 ack/생략 검토 필요(§11).

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
2. **메시지 훅 — `build.sh`에서 자동 적용(완료)**: `io.c`의 `endmsg()`가 완성한 `msgbuf`를
   `web_emit_msg(tr_msg(msgbuf))`로 보내도록 상단 라인 그리기를 sed로 치환(이미 적용됐으면 건너뜀).
   조각 concat 사이트는 **무수정** — `tr_msg()`가 조립된 문장 전체를 매칭하므로 소스 리팩터링 불필요(아래 8).
3. **입력**: `mdport.c`의 `md_readchar`를 `-Dmd_readchar=__rogue_native_readchar_unused`로 비활성화해 셰임 정의가 우선되게. `md_*tty`/시그널 관련 터미널 호출은 브라우저용 no-op 스텁.

추가: 세이브/스코어 파일은 `FORCE_FILESYSTEM` + IDBFS 마운트 후 `FS.syncfs()`로 영속화.

## 8. 한글화(i18n) 전략 — 코드로 입증된 난점
영어 원본은 메시지를 **조각으로 이어 붙임**: `msg("there is ") … msg(" to pick up")`, `msg("I see ")`. 한국어는 어순이 달라 **단순 치환이 깨짐**.

**채택한 해법(구현 완료) — 소스 리팩터링 대신 "조립 후 전체 문장 매칭".** concat은 `endmsg()` 시점이면 이미 영어 완성문(`msgbuf`)이 되므로, `tr_msg()`가 그 완성문을 받아 처리한다. `io.c` 패치는 한 줄(상단 라인 그리기 → `web_emit_msg(tr_msg(msgbuf))`)이라 위험이 작고, 한국어 어순은 룰마다 자유롭게 구성한다. `i18n.c` 매칭 3단계:
- **정확표(EXACT)**: 변수 없는 고정 메시지 1:1 (가장 큰 손쉬운 커버리지). 내부 대문자(`What`, `.  Defeated `) 때문에 매칭은 전부 소문자 정규화 후 비교.
- **프레임 룰**: 접두/접미를 맞추고 가운데 명사를 뽑아 번역 → 조사 붙여 재구성. 예 `"there is %s to pick up"` → `"여기 단검을 주울 수 있다."`, `"welcome to level %d"`, `"you now have …"`, 색상(`your hands begin to glow %s`) 등.
- **전투 룰**: `fight.c`의 `h_names`/`m_names` 동사표를 길이 내림차순으로 매칭(부정형 ` doesn't hit`가 ` hit `보다 먼저)해 `<공격자>이(가) <대상>을(를) 맞혔다/빗맞혔다`로 구성. 처치/`.  Defeated ` 콤보 포함.

매칭 실패 시 **영어 원문 그대로 폴백**하므로 테이블은 점진 확장 가능(안전).

- **조사 자동 처리**: 프로토타입(`hasBatchim`/`eulReul`/`iGa`)을 일반화해 `i18n.c`에 이식(UTF-8 종성 디코드 → 을/를·이/가·은/는·와/과·(으)로). 비한글 꼬리(미번역 영어 명사·숫자)는 받침 없음으로 처리.
- 상태줄(`io.c` status, `"Level: %d Gold:..."`)은 **번역하지 않음** — 기본값에서 status는 `printw`로 STATLINE에 그려지므로 메시지 로그를 오염시키지 않음. 추후 값만 `RogueBridge.stats({...})`로 보내 JS 상태바에서 렌더(프로토타입에 이미 있음).
- **남은 큰 덩어리 = 테이블 확장 + `inv_name` 아이템 풀네임 한글화.** 미번역 사이트는 전수 추출로 보강: `grep -rhoE 'msg\("[^"]+"' *.c`.

## 9. UI 결선 (프로토타입 → 실엔진)
`web/bridge.js` 하단 주석의 4단계 그대로:
1. 프로토타입의 mock `useReducer` 제거 → `RogueBridge.subscribe(force)`로 리렌더, 맵은 `getScreen()`(던전 행만), 로그는 `getMessages()`.
2. 컨트롤 `dispatch(...)` → `RogueInput.move/tap/pickup/inventory/descend/search/rest/command(...)`.
3. 상태바는 C의 status 훅(`RogueBridge.stats`)에서 받아 기존 JS 상태바 유지.
4. `index.html` 로드 순서: `bridge.js` → `rogue.js`.

## 10. 다음 마일스톤 (권장 순서)
1. ✅ **emcc 첫 빌드 + 디버깅 패스**: 완료(§5.1). `build.sh` → `web/rogue.js`/`rogue.wasm`, Node에서 렌더·입력 동작 확인, `web/index.html`로 브라우저 구동 가능.
2. ✅ **메시지 훅 + i18n.c**(패치 #2, §8): 완료(합본). `build.sh`가 `endmsg()`를
   `web_emit_msg(tr_msg(...))`로 자동 치환. `i18n.c`는 1차 엔진(정확표+프레임+전투동사표
   +명사사전)과 2차 fallback 패턴 엔진(`mz_*`)을 결합 — 전투/줍기/장비/허기/상태/오류/프롬프트
   /함정/메두사 등 광범위 커버, 명사(몬스터·색·아이템) 한글화 포함. 미매칭은 영어 폴백.
   네이티브 자체 테스트 통과 + 커버리지 하네스로 검증(미번역 128→61). (다음: 아이템 풀네임.)
   · 전투 동사: §8이 우려한 조각-concat을 *소스 리팩터링 없이* i18n 레이어에서 해결 — 1차는
     fight.c 동사표(CVERB)로, 2차는 패턴(`@N` 액터정규화)으로 처리. fight.c 무수정.
3. **UI 결선**(9단계) → React 터치 프로토타입을 실엔진에 연결, 터치로 실제 플레이.
4. **INV_OVER 오버레이 → JS 바텀시트 분기**(§11) → 인벤토리/도움말 메뉴 표시.
5. 기기 테스트·세이브 영속화(IDBFS `FS.syncfs`)·정적 호스팅 배포.

## 11. 알려진 리스크
- Asyncify로 `getch` 블로킹을 푸는 구조라, 스택 사이즈(`-sASYNCIFY_STACK_SIZE`) 조정이 필요할 수 있음. `-sEMULATE_FUNCTION_POINTER_CASTS=1`과 함께 쓰고 있는데(데몬 디스패치 때문) 둘 다 켠 상태로 빌드/구동은 확인됨. 장기적으로는 데몬/퓨즈 함수 시그니처를 통일해 `EMULATE_FUNCTION_POINTER_CASTS`를 떼는 게 더 가벼움.
- 인벤토리/도움말은 기본 `inv_type = INV_OVER`라 별도 윈도(`tw`/`sw`)에 그려짐. `newwin`/`subwin`을 분리 malloc해 크래시는 없지만, 이 윈도는 `stdscr`가 아니라 **JS UI로 push되지 않음** → 메뉴가 화면에 안 뜸. `wrefresh(w!=stdscr)` 경로를 JS 오버레이/바텀시트로 분기해야 함(§10.4).
- 메시지(패치 #2) 적용됨 — 미번역 메시지는 영문으로 로그에 폴백 표시(§10.2 아이템 풀네임 등).
- `endmsg()`의 `--More--` 페이징 분기는 그대로라 한 턴에 메시지가 여럿이면 `wait_for(' ')`로
  블로킹될 수 있음(스페이스 입력 필요). 메시지가 이제 로그로 가므로 추후 자동 ack 또는 `mpos` 무력화 검토.
- i18n는 **조립 문장 매칭**이라 `terse` 옵션을 켜면 문장 형태가 달라져 일부 룰이 빗나갈 수 있음(기본=verbose 기준). 미매칭은 영어 폴백.
- i18n.c 합본 구조: **1차 엔진이 항상 우선**, 2차(`mz_*`) fallback은 1차가 `en`을 그대로 돌려줄 때만
  동작. 두 엔진이 같은 메시지를 다르게 번역할 수 있으나 1차가 이기므로 충돌 없음. 2차 패턴 테이블은
  **순서 의존**(구체적 패턴을 위에): 예 `"you found %d gold pieces"`가 `"you found %s"`보다 먼저.
- 세이브/스코어 영속화(IDBFS)는 아직 미연결 — 현재 MEMFS라 새로고침 시 세이브 소실.
