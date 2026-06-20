# 배포 — 정적 호스팅 (GitHub Pages)

오리지널 Rogue 한글/모바일 웹 포팅을 정적 호스팅으로 배포하는 절차. 빌드 산출물
(`web/rogue.js`, `web/rogue.wasm`)과 엔진 소스(`/rogue/`)는 gitignore되어 저장소에
없으므로, **CI가 매번 엔진을 클론하고 `build.sh`로 새로 빌드**한 뒤 게시한다(HANDOFF.md §2/§6).

의존성 0(바닐라 JS·외부 CDN 없음·에셋 합성)이라 어떤 정적 호스트에도 올릴 수 있다.
아래는 GitHub Pages 자동 배포를 기준으로 한다.

## 1. 자동 배포 (GitHub Actions → Pages)

워크플로: `.github/workflows/deploy.yml`

- **build 잡** (모든 push/PR/수동 실행): 엔진 클론 → Emscripten 6.0.0 셋업 →
  `bash build.sh` → `node web/headless-test.js`(한글 i18n 훅 검증) +
  `node web/persist-test.js` → 런타임 파일만 `_site/`로 모아 Pages 아티팩트 업로드.
- **deploy 잡** (`main` push 또는 수동 `workflow_dispatch` 한정): 아티팩트를 Pages에 게시.
  PR/피처 브랜치는 build 잡까지만 돌아 **CI 게이트**로 작동(배포는 안 함).

### 최초 1회 수동 설정 (저장소 관리자)

GitHub Actions 배포는 저장소 설정에서 Pages 소스를 "GitHub Actions"로 지정해야 동작한다:

1. 저장소 **Settings → Pages**
2. **Build and deployment → Source** = **GitHub Actions** 선택

이후 `main`에 머지될 때마다 자동 빌드·배포되고, 게시 URL은 deploy 잡의
`environment` 링크(Actions 실행 페이지)와 Settings → Pages 에 표시된다
(`https://openporting.github.io/rogue/` 형태).

수동 배포가 필요하면 **Actions → Build & Deploy (GitHub Pages) → Run workflow**.

### 게시되는 파일

런타임에 필요한 것만 (`_site/`):
`index.html`(진입점) · `terminal.html`(디버그 터미널 뷰) · `bridge.js` · `audio.js` ·
`persist.js` · `rogue.js` · `rogue.wasm` · `.nojekyll`.
Node 전용 테스트(`headless-test.js`/`persist-test.js`)와 빌드 중간물(`*.o`)은 제외.

## 2. 수동/로컬 빌드 후 임의 호스트에 올리기

```bash
git clone --depth 1 https://github.com/Davidslv/rogue.git rogue   # 엔진 소스
source /path/to/emsdk/emsdk_env.sh                                # emscripten 6.0.0
bash build.sh                                                     # web/rogue.js (+ .wasm)
```

그 다음 `web/`의 런타임 파일(위 목록)을 정적 호스트 루트에 올리면 끝. 단순 정적 서빙이면
충분하다 — 이 빌드는 SharedArrayBuffer/pthreads를 쓰지 않으므로(Asyncify 기반) 별도의
**COOP/COEP 교차출처 격리 헤더가 필요 없다.** 로컬 확인:

```bash
( cd web && python3 -m http.server 8000 )   # http://localhost:8000/
```

## 3. 호스트 요구사항 메모

- **MIME**: `.wasm` 은 `application/wasm` 으로 서빙되어야 한다(GitHub Pages·대부분의
  호스트는 기본 지원). 일부 구형 정적 서버는 매핑 추가가 필요할 수 있다.
- **헤더**: 특별 헤더 불필요(위 참고). 캐싱은 호스트 기본값으로 충분.
- **경로**: 모든 자원은 `index.html` 기준 상대경로라 서브패스(`/rogue/`) 배포에도 안전.

## 4. 남은 검증 (실기기)

CI는 빌드·i18n·세이브 흐름을 헤드리스로 검증하지만, **실기기 플레이 검증**은 수동이다
(HANDOFF.md §5 "실기기 테스트"). 배포 후 모바일에서 확인할 항목:

- 터치 입력(D패드/탭/스와이프/바텀시트)과 한글 로그 렌더.
- **오디오 첫 제스처 게이트**: 첫 탭/키에서 음소거 해제·BGM 시작(§12.4).
- **세이브 영속화**: `S`(저장)/사망 후 새로고침 시 이어하기. 만약 세이브가 `/save`(=`$HOME`)
  밖에 떨어지면 `md_gethomedir`가 `$HOME`로 귀결되지 않은 것 → `web_curses.c`에 `getpwuid`
  셰임으로 `pw_dir=/save` 고정(HANDOFF.md §11).
