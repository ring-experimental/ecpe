# UI 조사 도구

원작 UI(메뉴, 창)를 RmlUi로 다시 만들기 위한 조사용 안내다. 주소는 모두 `client.bin` 파일 오프셋(hex)이고 게스트 주소는 +0x100000이다.

- 번역 C: `native/build/tr.c`의 `DEF(<오프셋>)`이 그 함수다(기계 번역이라 길다. `grep -n "^DEF(31b18)" native/build/tr.c`).
- 역어셈블: `work/tools/py work/tools/tdis.py <오프셋>...`, `--callers <오프셋>`, `--callees <오프셋>`, `--str <글>`.
- 띄우기(창·소리 없이): `ECPE_HEADLESS=1 ECPE_BIN=$PWD/native/build/ecpe-trace work/tools/nctl.sh start <폴더> --width 512`.
  사용자 저장으로 시작하려면 먼저 `mkdir -p <폴더> && cp -R ~/Library/Application\ Support/ecpe/save <폴더>/save`(원본 저장은 절대 건드리지 않는다. 항상 사본).
  저장 없이 띄우면 첫 실행 최적화 뒤 타이틀로 간다(새 게임 흐름).
- 조작: `work/tools/nctl.sh <폴더> <명령...>`. 명령: `ok up down left right clear lsk rsk star hash 0~9`(키), `key <이름> <ms>`(길게), `wait<초>`, `shot <이름>`(→ `<폴더>/<이름>.png`), `memdump <파일>`(게스트 메모리 0x100000부터), `calltree <파일>`. 끝에 `<폴더>/now.png`가 남는다. 끝나면 `nctl.sh stop <폴더>`.
- 호출 나무: `calltree <파일>`은 다음 한 프레임의 게스트 호출 나무를 남긴다(CP949). `python work/tools/treepath.py <파일>`은 그리기 API 잎(DrawString 글·좌표, FillRect 등)마다 뿌리부터의 함수 경로를 보인다. `--all -d N`은 나무를 깊이 N까지 보인다.
- 사용자 저장 사본으로 필드까지: 시작 3초 뒤 `ok wait1 ok wait2 ok wait2 key ok 30 wait1.2`, 이어서 `key ok 30 wait2.5` 세 번(`work/tools/tobattle.sh` 참고, 전투 진입도 여기 있다).
- 필드에서 `clear`가 메뉴를 연다.

## 이미 아는 것

- 틱: `0x6604 > 0x6720 > 0xbba0 >` (간접 호출 베니어 `0x44f1c`~`0x44f30`) `> 장면`. 필드 장면 틱 `0x109b8`, 필드 그리기 `0xcab0`.
- 필드 메뉴 분배 `0x2faa4`: 메뉴 객체 `+0x18`이 현재 화면 번호, `+0x24 + 4*번호`가 화면 객체, 그 vtable `+4`가 그리기. 메인 메뉴 그리기 `0x310e8`, 아이템 `0x31b18`.
- 글자: `0x3c0c`(글 그리기, 안에서 `0x3d68`이 DrawString), 글 상자 `0x3b48`/`0x3a1c`. 스프라이트: `0x4798`/`0x4838`/`0x47e8`/`0x4888` 계열(→ `0x6e60` 등 줄 단위 블리터).
- 화면 폭은 gfx 객체 `+0xdc` 클립 사각형. 그 밖의 주소는 `native/README.md`의 "찾은 주소".

- `poke8|16|32 <주소> <값>`의 값은 `%lli`로 읽는다. 16진수는 `0x`를 꼭 붙이고, 앞에 0을 붙인 수는 8진수로 읽히니 쓰지 않는다.
