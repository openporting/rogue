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
│  ├─ i18n.h / i18n.c          ← EN→KO 번역 엔진. `tr_msg`(정적표+동적 프레임+조사),
│  │                              `kr_item`(아이템명), `tr_screen`(도움말/죽음 화면),
│  │                              명사 사전(몬스터26/식별/색/장비). `gcc -DI18N_DEMO` 자가검증.
│  └─ patches.sh               ← 빌드 시 rogue/에 멱등 적용하는 소스 패치(메시지 훅 #2 +
│                                 inv_name 래퍼 #4). build.sh가 호출.
└─ web/
   ├─ index.html               ← 실엔진에 결선된 모바일 터치 UI(메인 진입점). 바닐라 JS,
   │                              의존성 0. 맵 뷰포트·한글 로그·상태바·D패드/탭/스와이프/바텀시트.
   ├─ terminal.html            ← 80×25 원본 화면 그대로 보는 디버그 터미널 뷰.
   ├─ bridge.js                ← WASM 엔진 ↔ 터치 UI 연결. 화면버퍼/메시지/입력큐 +
   │                              RogueInput(터치→키맵) + getPlayer/getStats(버퍼 파싱).
   │                              msg()가 RogueAudio.onMessage()로 효과음 트리거.
   ├─ audio.js                 ← RogueAudio: Web Audio 합성 SFX 14종 + 생성형 던전 BGM
   │                              (§10-#5). 에셋·CDN 0, localStorage 설정 영속화.
   ├─ persist.js               ← IDBFS 세이브 영속화(§10-#6). Module.preRun에서 /save에
   │                              IDBFS 마운트(=$HOME) + FS.syncfs(true) 로드(있으면 argv로
   │                              자동 이어하기) + onExit에서 FS.syncfs(false) 저장.
   ├─ headless-test.js         ← 브라우저 없이 Node로 i18n 훅 검증(한글 메시지 PASS).
   ├─ persist-test.js          ← persist.js IDBFS 흐름 헤드리스 검증(Emscripten FS 목, PASS).
   └─ more-prompt-test.js      ← 장착/장비(get_item) 프롬프트의 --More-- 교착 회귀 테스트.
                                  실 bridge.js + endmsg/get_item 재현(wasm 불필요, PASS).
```

## 5. 현재 상태 (정확히)
- ✅ **UI 프로토타입**: 동작 확정. 입력 방식(탭+스와이프+D-패드 전부) / 레이아웃 사용자 승인 완료("지금처럼").
- ✅ **브리지 C**: `gcc -DWEBCURSES_DEMO -Wall` 컴파일·실행 검증(스모크 테스트: `@` 배치 + 한글 메시지 emit + 키 echo).
- ✅ **JS 브리지 / 빌드 스크립트 / 패치 명세**: 작성 완료.
- ✅ **emcc 풀빌드 완료**: `build.sh`로 `web/rogue.js`(+`rogue.wasm`) 생성. 33개 원본 `.c` 전부 컴파일 + 링크 성공.
- ✅ **런타임 검증(Node)**: 엔진이 던전을 렌더(드로콜 2000+), `@`/몬스터/아이템/상태줄 표시, 키 입력→이동/공격/계단/종료 동작 확인. 브라우저용 최소 터미널 렌더러 `web/index.html` 추가.
- ✅ **i18n 완성 (메시지·이름·화면 텍스트) — emcc 6.0.0로 풀 검증**:
  - **메시지 훅**(패치 #2): `io.c` endmsg()의 라인 그리기를 `web_emit_msg(tr_msg(msgbuf))`로
    멱등 치환(`webcurses/patches.sh`, gitignore된 `rogue/`에 빌드 시 적용).
  - **`tr_msg`**(webcurses/i18n.c): 정적 메시지 테이블(~120, ASCII 대소문자 무시 매칭) +
    동적 프레임 엔진(전투 명중/빗나감/처치, 줍기/버리기/장착/착용/먹기, 금화, 레벨업, 마법
    글로우·식별, 발사체, 입력 검증, get_item 프롬프트) + UTF-8 **조사 엔진**(을/를·이/가·은/는·
    으로/로, ㄹ받침 예외).
  - **아이템 이름**(패치 #4): `inv_name`을 `kr_item()`으로 감싸(things.c 래퍼) 물약/두루마리/
    반지/지팡이/무기/갑옷(+강화 `[방어 N]`)/음식/금화/부적 + 착용표시 + 개수까지 한글화.
    인벤토리 화면과 메시지 공용. 몬스터 26종·식별 사전·색상 사전 포함.
  - **화면 텍스트**(`tr_screen`, web_curses.c `waddstr` 경유): 죽음 화면(stdscr, 렌더됨)·
    도움말·`--More--`/계속 프롬프트. 상태줄·맵·한글 아이템명은 통과.
  - **검증**: `gcc -DI18N_DEMO` 단위테스트 전부 통과 + `node web/headless-test.js` +
    인게임(드롭/먹기/장착/검증 메시지 한글 확인).
- ✅ **모바일 터치 UI 결선 (§10-#3) — 완료**: `web/index.html`이 이제 실제 터치 UI.
  프로토타입(`rogue-touch-prototype.jsx`)의 손맛을 **의존성 없는 바닐라 JS**로 재구현(React/
  Babel/CDN 불필요 → 정적 호스팅 그대로 배포)하고 실엔진에 결선:
  - **맵**: `RogueBridge.getScreen()`의 던전 행(1~23)을 `@` 중심 뷰포트로 렌더, `getPlayer()`
    기준 횃불 FOV 밝기 + 글리프 색. **로그**: `getMessages()`(한글).
  - **입력**: D-패드/탭/스와이프 → `RogueInput.move`; 액션(줍기·계단↓·계단↑·검색·쉬기) →
    각 메서드; 더보기 바텀시트 → `command(q/r/e/w/W/t/T/i/?)` + a–z 키패드 +
    `key()/enter()/escape()/space()`로 엔진 프롬프트 응답.
  - **상태줄 stats 훅(§9.3) — 해결**: C 훅/리빌드 없이 `RogueBridge.getStats()`가 엔진의
    **영어 상태줄을 버퍼에서 직접 파싱**(Level=던전 깊이, Gold, Hp, Str, Arm, Exp=캐릭터
    레벨, 허기). 상태줄은 의도대로 번역하지 않으므로 안정적. Node 단위검증 PASS.
  - 기존 80×25 원본 화면 그대로 보는 디버그용 터미널 뷰는 `web/terminal.html`로 분리.
- ✅ **INV_OVER 오버레이 → JS 바텀시트 (§10-#4) — 완료**: 인벤토리(`i`)·발견목록(`*`)·도움말(`?`)·
  설정·마법탐지 등 **stdscr이 아닌 모든 윈도**(`hw`, INV_OVER의 `tw`)를 `web_curses.c`의
  `wrefresh(w!=stdscr)`가 텍스트 오버레이로 JS에 push. `bridge.js`의 `overlayBegin/Line/End`가
  `RogueBridge.getOverlay()`를 만들고, `web/index.html`이 별도 바텀시트(`#ovl-scrim`, 모노스페이스
  `<pre>`)로 렌더. 엔진은 페이지마다 키 대기 → "계속 ▸" 버튼이 space 전송, 다음 stdscr
  `refresh()`가 오버레이 자동 해제. **핵심 수정**: `subwin()`이 부모 윈도를 진짜로 aliasing하도록
  바꿔(우리 윈도는 이미 풀사이즈) INV_OVER가 복사한 아이템 줄 + 프롬프트가 실제로 그려지는 윈도(tw)에
  들어가게 함. **emcc 6.0.0 풀빌드 + Node로 인게임 검증**: `i`(INV_OVER) → 시작 소지품이 한글로 정확히
  렌더(`a) 음식`, `b) +1 사슬 미늘 갑옷 [방어 4] (착용 중)`, `c) +1,+1 철퇴 (장착 중)`, `d) +1,+0 단궁`,
  `e) +0,+0 화살 27개`, `--계속하려면 스페이스--`); `?`+`*`(hw 경로) → 2단 한글 도움말 렌더. i18n 회귀
  테스트(`node web/headless-test.js`)도 PASS. (네이티브 데모 `gcc -DWEBCURSES_DEMO`로도 subwin alias 확인.)
- ✅ **`--More--` 메시지 페이저 자동 진행 (모바일) — 완료**: 한 턴에 메시지가 2개 이상이면 엔진이
  `io.c endmsg()`에서 `wait_for(' ')`로 멈춰 이전 메시지를 읽게 함. **모바일엔 물리 스페이스가 없고**
  우리 한글 로그는 50개를 보존하므로 이 멈춤이 불필요 → `bridge.js`의 `refresh()`가 stdscr 0행에서
  `--More--`(번역 시 `--계속--`) 마커를 **상승엣지**로 감지해 스페이스를 자동 주입(각 페이저 단계 앞에는
  0행을 비우는 refresh가 있어 엣지 재무장). 오버레이의 "계속 ▸"(인벤/도움말)와는 별개 — 오버레이는
  비-stdscr 윈도라 0행 감지에 안 걸리고 사용자가 직접 닫음. 검증: 실제 bridge 코드로 상승엣지
  단위테스트(연쇄 `--More--`·중복프레임 dedup·ASCII 폴백) PASS + wasm 수백 턴 구동 시 입력 데드락 없음.
- ✅ **음향 (SFX + BGM) (§10-#5) — 완료**: 엔진(WASM) 무수정, 전부 `web/` 계층(`web/audio.js` 신규).
  `RogueAudio` 싱글턴이 **외부 에셋·CDN 0**으로 Web Audio 합성만 사용 → 정적 호스팅·오프라인 동작.
  - **첫 제스처 게이트**: 모바일 자동재생 정책 때문에 첫 탭/키에서 `RogueAudio.unlock()`
    (UI `buzz()` + 일회성 `pointerdown/keydown` 리스너)으로 `AudioContext` 생성·resume + BGM 시작.
  - **메시지 기반 SFX (§12.2)**: `bridge.js`의 `msg()`가 한글 메시지를 `RogueAudio.onMessage()`로
    넘기고, 순서 있는 정규식 룰 14종이 첫 매칭으로 효과음 선택(타격/빗나감/처치/피격/금화/줍기/
    물약/두루마리/레벨업/마법·함정/사망/허기/잘못된 입력). 키는 `tr_msg`가 정규화한 한국어라 안정적.
    실제 i18n 출력 18케이스로 매칭 단위검증 PASS.
  - **던전 BGM**: 깊이별로 음정이 가라앉는 생성형(generative) 드론 + 5음 펜타토닉 모테. `getStats().depth`
    변화 시 `setDepth()`가 드론을 리튠하고 **하강 시 우우웅 큐** 재생(원작은 계단 메시지가 없으므로
    유일한 신호). 잘못된 입력/`RogueBridge.bell()`은 짧은 블립.
  - **설정 영속화**: sfx/bgm 볼륨·뮤트를 `localStorage`(`rogue.audio`)에 저장. 헤더에 🔈 토글(뮤트 시 🔇).
  - **검증**: `node --check` + WebAudio/DOM 목으로 `audio.js` 헤드리스 스모크(unlock→음악 시작,
    뮤트→정지·영속화, 볼륨 설정·영속화) PASS. i18n 회귀(`headless-test.js`)는 자체 스텁이라 무영향.
- ✅ **세이브 영속화 (IDBFS `FS.syncfs`) (§10-#6) — 완료**: 엔진(WASM) 무수정, 전부 `web/` 계층
  (`web/persist.js` 신규 + `build.sh`에 `-lidbfs.js`·런타임 심볼 export). 원본은 `<home>/rogue.save`에
  세이브하고(`main.c`: `file_name = md_gethomedir()+"rogue.save"`, emscripten에선 `$HOME`로 귀결), 스코어
  파일은 컴파일 타임 비활성(`webcurses/config.h`에 `SCOREFILE` 미정의 → `scoreboard=NULL`)이라 **유일한
  영속 대상은 `rogue.save`**.
  - **마운트/로드**: `Module.preRun`에서 `ENV.HOME=/save` 설정 후 `/save`에 IDBFS 마운트, `FS.syncfs(true)`로
    IndexedDB→MEMFS 적재(런 디펜던시로 startup 게이트). 세이브가 있으면 `Module.arguments`에 경로를 넣어
    엔진이 `restore()`로 **자동 이어하기**(`main.c`: `argc==2`). `restore()`는 세이브를 `unlink`(세이브 스컴
    방지) → 재개된 게임이 사망/종료(`exit()`)할 때 삭제가 함께 flush돼 다음엔 새 게임.
  - **저장**: 원작은 `S`(저장)·사망 모두 `exit()`로 끝나고 그 직전에만 `rogue.save`가 쓰이/지워지므로
    `Module.onExit`에서 `FS.syncfs(false)`로 MEMFS→IndexedDB flush(재진입 가드 + 탭 숨김/`pagehide` 세이프티
    넷). 종료 후 "새로고침하면 이어하기" 안내 배너. `RoguePersist.flush()/reset()` 콘솔/복구용 핸들.
  - **검증**: `node web/persist-test.js`(Emscripten FS/IDBFS/ENV/런디펜던시 목으로 부팅 로드·argv 재개·exit
    flush·reset 전 항목 PASS) + `node --check`. 남은 것: 실기기/브라우저에서 풀빌드 결합 시 `md_gethomedir`가
    `$HOME`로 귀결되는지 최종 확인(아니면 `web_curses.c`에 `getpwuid` 셰임으로 `pw_dir=/save` 고정).
- ✅ **정적 호스팅 배포 (CI/CD → GitHub Pages) (§10-#7) — 완료**: 엔진(WASM) 무수정. 빌드 산출물
  (`web/rogue.js`/`.wasm`)과 엔진 소스(`/rogue/`)는 gitignore라 저장소에 없으므로 **CI가 매번 엔진을
  클론 + Emscripten 6.0.0 셋업 + `build.sh`로 새로 빌드** 후 게시. `.github/workflows/deploy.yml`:
  - **build 잡**(모든 push/PR/dispatch): 엔진 클론 → emsdk 6.0.0 → `bash build.sh` →
    `node web/headless-test.js`(한글 i18n 훅 검증) + `node web/persist-test.js` → 런타임 파일만
    `_site/`로 모아(`index.html`/`terminal.html`/`bridge.js`/`audio.js`/`persist.js`/`rogue.js`/
    `rogue.wasm`/`.nojekyll`; Node 테스트·`*.o` 제외) Pages 아티팩트 업로드.
  - **deploy 잡**(`main` push 또는 수동 dispatch 한정): `actions/deploy-pages`로 게시. PR/피처
    브랜치는 build 잡까지만 → **CI 게이트**.
  - **수동 1회 설정**: 저장소 Settings → Pages → Source = "GitHub Actions"(DEPLOY.md §1). 의존성 0
    (바닐라 JS·외부 CDN 0·에셋 합성)이라 SharedArrayBuffer/pthreads 미사용 → COOP/COEP 헤더 불필요,
    아무 정적 호스트나 가능. 절차·임의 호스트 배포·MIME(`.wasm`) 메모는 **DEPLOY.md** 참조.
- ❌ **아직 안 된 것**: (핵심 마일스톤 §10 전부 완료 + 배포 파이프라인 완료) **실기기 플레이 검증**만
  수동으로 남음(DEPLOY.md §4): 터치 입력·한글 로그, 오디오 첫 제스처 게이트, 세이브 이어하기.

### 5.1 emcc 빌드에서 실제로 필요했던 것 (이번 핸드오프에서 해결)
설계상 "첫 컴파일에서 확장" 전제대로, 다음을 추가/수정함:
- **`webcurses/config.h` 신규**: 원본은 `./configure`로 `config.h`를 생성하지만 브라우저/musl에서 의미 있는 feature test가 불가. emscripten이 실제 지원하는 능력만 정의(`HAVE_TERMIOS_H`/`HAVE_PWD_H`/`HAVE_ERASECHAR` 등), 미지원은 정의하지 않음(`HAVE_TERM_H`/`HAVE_WORKING_FORK`/`HAVE_GETLOADAVG` 제외 → termcap·fork·loadav 코드 컴파일 아웃). `-I webcurses`라 `extern.h`의 `#include "config.h"`가 여기로 잡힘(원본 트리 무수정).
- **`curses.h`/`web_curses.c` 심볼 보강**: 첫 컴파일이 요구한 것 전부 추가 — `<stdio.h>/<stdbool.h>/<stdarg.h>` 포함(FILE/bool/va_list), `A_CHARTEXT`, `unctrl`, `flushinp`, `halfdelay`, `isendwin`, `mvwprintw`, `wgetnstr`, `getmaxx/getmaxy`, `mvwinch`, `werase`, `wclrtoeol`, `mvwaddch`, `subwin`/`mvwin`, 터미널 모드 no-op(`raw/noecho/keypad/clearok/...`), `erasechar/killchar`, `KEY_*` 상수(가드 밖에서 쓰는 것만), `_getch`→`getch`, `_cury/_curx`→`cy/cx`, `CE`(NULL).
- **`newwin`/`subwin`**: `newwin`은 풀사이즈 윈도를 malloc(INV_OVER가 `hw`→`tw` 복사 시 둘이 별개 버퍼여야 함). `subwin(tw)`은 **부모(tw)를 그대로 반환(alias)** — 서브윈도는 본디 부모 버퍼를 공유하므로, 복사된 아이템 줄 + 프롬프트가 refresh되는 윈도(tw)에 모임(§10-#4). `delwin`은 `tw`만 free(=`sw`는 alias라 별도 free 없음 → 안전).
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
# 4) 헤드리스 검증(브라우저 없이 Node로 i18n 훅 확인)
node web/headless-test.js      # '>'/'<'/'Q' -> 한글 메시지 PASS
```
검증 환경: **emscripten 6.0.0**으로 풀빌드 + Node 구동 확인됨(정적 메시지 한글 방출 PASS).
주요 플래그: `-I webcurses`(우리 curses.h 우선), `-sASYNCIFY`(blocking getch→`emscripten_sleep`), `-sFORCE_FILESYSTEM` + `-lidbfs.js`(세이브 영속화, §10-#6), `-sALLOW_MEMORY_GROWTH`. 런타임 심볼 `FS/IDBFS/ENV/addRunDependency/removeRunDependency`를 `EXPORTED_RUNTIME_METHODS`로 내보내 `web/persist.js`가 마운트·동기화에 사용.

## 7. 원본 패치 (전부 `webcurses/patches.sh`에서 멱등 적용; build.sh가 호출)
1. **include**: 패치 불필요 — `-I webcurses`로 `#include <curses.h>`가 자동으로 셰임에 연결.
2. **메시지 훅(패치 #2)**: `io.c` `endmsg()`의 `mvaddstr(0,0,msgbuf)`를 `web_emit_msg(tr_msg(msgbuf))`로
   치환 + extern 선언 삽입. 완성된 메시지가 한글로 우리 로그에 감.
3. **입력**: `mdport.c`의 `md_readchar`를 `-Dmd_readchar=__rogue_native_readchar_unused`로 비활성화해 셰임 정의가 우선되게(build.sh에서 mdport.c 단독 컴파일에만 적용). `md_*tty`/시그널은 no-op 스텁.
4. **아이템 이름(패치 #4)**: `things.c`의 `inv_name`을 `inv_name_en`으로 개명하고, `kr_item(inv_name_en(...))`를
   호출해 결과를 `prbuf`에 되돌려 주는 얇은 래퍼로 감쌈. 인벤토리·메시지의 아이템명이 한글로.
5. **화면 텍스트**: 패치 아님 — `web_curses.c`의 `waddstr`가 `tr_screen()`을 거쳐 도움말/죽음 화면/프롬프트를 한글화.

추가(완료, §10-#6): 세이브는 패치 아님 — `FORCE_FILESYSTEM` + `-lidbfs.js` 위에서 `web/persist.js`가
`/save`(=`$HOME`)에 IDBFS를 마운트하고 부팅 `FS.syncfs(true)`/종료 `FS.syncfs(false)`로 영속화. 스코어
파일은 `SCOREFILE` 미정의로 비활성이라 영속화 대상 아님.

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
2. ✅ **i18n 완료**(§5) → 메시지(`tr_msg` 정적+동적 프레임+조사), 아이템명(`kr_item`), 몬스터/식별 사전, 화면 텍스트(`tr_screen`, 도움말/죽음). emcc 6.0.0 풀빌드 + Node 헤드리스 + 인게임 검증. 남은 연동: 상태줄 stats 훅(§9.3)·오버레이 렌더(§11).
3. ✅ **UI 결선**(§9) → `web/index.html`이 실엔진에 결선된 바닐라 JS 터치 UI(맵 뷰포트·
   한글 로그·버퍼 파싱 상태바·D패드/탭/스와이프/액션/바텀시트). 디버그 터미널 뷰는
   `web/terminal.html`. 남은 것: 브라우저/실기기에서 `rogue.js` 풀빌드와 결합한 플레이 검증.
4. ✅ **INV_OVER 오버레이 → JS 바텀시트 분기**(§5) → 인벤토리/발견목록/도움말/설정 표시 완료.
   `wrefresh(w!=stdscr)` → `RogueBridge.overlay*` → `#ovl-scrim` 바텀시트. subwin alias 수정 포함.
5. ✅ **음향(SFX + BGM)**(§12) → `web/audio.js`의 `RogueAudio`: 합성 효과음 14종(메시지 패턴 매칭) +
   생성형 던전 BGM(깊이별 리튠·하강 큐). 첫 제스처 게이트, 🔈 토글, `localStorage` 설정 영속화.
   엔진 리빌드 0, 에셋·CDN 0. (§5 참고.) 남은 것: 실기기에서 풀빌드 결합 청취 확인 + BEL 셰임(§12.3, 선택).
6. ✅ **세이브 영속화(IDBFS `FS.syncfs`)**(§5) → `web/persist.js`: `/save`에 IDBFS 마운트(=`$HOME`),
   부팅 시 `syncfs(true)` 로드 + 세이브 있으면 argv로 자동 이어하기, `onExit`에서 `syncfs(false)` 저장.
   엔진 리빌드 0(빌드 플래그 `-lidbfs.js` + 런타임 심볼 export만).
7. ✅ **정적 호스팅 배포(CI/CD → GitHub Pages)**(§5) → `.github/workflows/deploy.yml`: CI가 엔진 클론 +
   emsdk 6.0.0 + `build.sh` 빌드 + 헤드리스 검증 후 `web/` 런타임을 Pages에 게시. `main`만 배포,
   PR/피처는 빌드 게이트. 절차·임의 호스트 배포는 **DEPLOY.md**. 남은 것: 실기기 플레이 검증(수동).

## 11. 알려진 리스크
- Asyncify로 `getch` 블로킹을 푸는 구조라, 스택 사이즈(`-sASYNCIFY_STACK_SIZE`) 조정이 필요할 수 있음. `-sEMULATE_FUNCTION_POINTER_CASTS=1`과 함께 쓰고 있는데(데몬 디스패치 때문) 둘 다 켠 상태로 빌드/구동은 확인됨. 장기적으로는 데몬/퓨즈 함수 시그니처를 통일해 `EMULATE_FUNCTION_POINTER_CASTS`를 떼는 게 더 가벼움.
- ~~인벤토리/도움말이 `stdscr`가 아닌 윈도(`tw`/`sw`)라 JS UI에 안 뜸~~ → **해결(§5, §10-#4)**:
  `wrefresh(w!=stdscr)`를 오버레이 바텀시트로 분기. `subwin`을 부모 alias로 바꿔 INV_OVER 복사가
  실제 그려지는 윈도에 들어가게 함(`sw`는 `tw`의 서브윈도이므로 alias가 정상; `delwin`은 `tw`만 함 → 안전).
- 메시지(패치 #2)가 아직 미적용이라 게임 메시지가 화면 0행에 영어로 그려짐 — i18n 단계에서 `web_emit_msg`로 전환.
- ~~세이브/스코어 영속화(IDBFS)는 아직 미연결~~ → **해결(§5, §10-#6)**: `web/persist.js`가 `/save`에
  IDBFS 마운트 + 부팅 `syncfs(true)`/종료 `syncfs(false)`. 잔여 가정: `md_gethomedir`가 `$HOME(=/save)`로
  귀결(emscripten에서 그렇게 동작). 실기기에서 세이브가 `/save` 밖에 떨어지면 셰임에 `getpwuid`로 홈 고정.

## 12. 음향 설계 (SFX + BGM) — 마일스톤 §10-#5
**원작은 무음.** Rogue 5.4.4의 유일한 소리는 터미널 벨(`\007`)뿐이라, 음향은 전부 **웹 계층에서
새로 얹는다.** 엔진(WASM)은 손대지 않고, 이미 쥐고 있는 3개 가로채기 지점(그리기·입력·메시지)
위에 Web Audio를 붙인다. 모든 코드는 `web/`에만.

### 12.1 아키텍처
- **`web/audio.js` 신규** (`bridge.js` 다음, UI 앞에 로드): `RogueAudio` 싱글턴.
  - `AudioContext` 1개. 모바일은 **첫 사용자 제스처 후에만** 재생 허용 → 첫 탭/키에서
    `ctx.resume()` (UI의 `buzz()` 자리에 1줄). 그 전엔 음소거.
  - `sfx(name)`: 짧은 샘플/합성음 원샷. `master`·`sfxGain`·`musicGain` 버스로 볼륨/뮤트.
  - `music(track)`: 루프 소스 크로스페이드. 던전 깊이(`getStats().depth`) 구간별 트랙 전환 옵션.
  - 설정 영속화: `localStorage`(sfx/bgm 볼륨·뮤트). UI에 작은 🔈 토글.
- **에셋**: 저용량 우선. SFX는 합성(WebAudio 오실레이터/노이즈)로 시작해도 되고, 후에
  CC0 샘플(`.ogg`/`.mp3`)로 교체. BGM은 1~2 루프. **외부 CDN 금지**(정적 호스팅·오프라인).

### 12.2 트리거 (엔진 수정 0 — 이벤트는 이미 우리 손에)
- **메시지 기반 SFX**: `RogueBridge.msg(korean)` 훅에서 한글 메시지를 패턴 매칭.
  이미 `tr_msg`가 한국어로 정규화하므로 키가 안정적. 매핑 예:
  | 이벤트 | 매칭(부분 문자열) | 소리 |
  |---|---|---|
  | 명중/타격 | `내려쳤다`,`명중` | 둔탁한 타격 |
  | 빗나감 | `빗나갔다` | 휙 |
  | 처치 | `처치했다` | 짧은 팡파레 |
  | 피격 | `할퀴었다`,`물었다`,`피해` | 신음/임팩트 |
  | 줍기/금화 | `주웠다`,`금화` | 짤랑 |
  | 물약/두루마리 | `마셨다`,`읽었다` | 보글/사락 |
  | 계단 하강 | `내려간다`,`층` | 우우웅 |
  | 레벨업 | `레벨`,`강해졌`,`올랐다` | 상승음 |
  | 함정/마법 | `함정`,`빛났다` | 칭 |
  | 사망 | `쓰러졌다` | 사망 스팅어 |
  | 잘못된 입력(벨) | (아래 12.3) | 짧은 블립 |
  - 구현은 `bridge.js`의 `msg()`가 `notify()` 외에 `RogueAudio.onMessage(korean)`도 호출,
    또는 UI의 로그 렌더에서 새 0번 메시지가 바뀔 때 매칭(중복 방지: 마지막 처리 인덱스 보관).
- **상태 기반 SFX**(선택): `getStats()` 폴링으로 HP 급감 시 경고음, 허기 상태 진입음.

### 12.3 터미널 벨(`\007`) 처리
엔진이 BEL을 쓰면 현재는 화면 버퍼로 흘러 무시됨. 깔끔하게 하려면:
- `web_curses.c`의 `waddch`에서 `ch=='\007'`을 가로채 셀에 쓰지 말고 `EM_ASM RogueBridge.bell()`
  호출(없으면 `RogueAudio.sfx('bell')`로 위임). 또는 ncurses `beep()`를 셰임에 추가해 같은 경로로.
  → C 한두 줄. **유일하게 엔진 측(셰임) 손이 가는 부분**이지만 패치가 아니라 셰임 보강.

### 12.4 검증
- 데스크톱 브라우저에서 이벤트별 SFX 확인 + 첫 제스처 게이트(자동재생 차단) 확인.
- 모바일(iOS Safari 포함): 무음 스위치/배경 전환 시 컨텍스트 suspend/resume 동작.
- 회귀: 음소거 상태에서 게임플레이 무영향, `localStorage` 설정 유지.
