# 에픽크로니클PE 네이티브 빌드. 구조는 크로노스 윙 native/Makefile과 같다.
# 이 저장소에는 게임 원본이 없다. 원본 jar(KTF 판 010100D5.jar)는 가진 사람이 make JAR=<경로>로 넘긴다(기본값 저장소 루트의 origin/, .gitignore로 막혀 있다).
# - all, windows, android, interp: 그 jar에서 코드와 데이터를 build/game에 꺼내 쓴다. build/의 결과물은 원본 파생물이라 올리지 않는다.
# - web: jar 없이 빌드된다. 웹 빌드는 실행할 때 사용자가 고른 원본 파일을 브라우저에서 읽는다(docs/web.md).
# 이 기계에만 맞는 경로는 적지 않는다. 도구는 PATH에서 찾고, 다른 곳에 있으면 make 변수로 넘긴다(PY, CARGO, RML).
# 의존성의 판: 파이썬 도구는 저장소 루트의 pyproject.toml과 uv.lock, C/C++ 라이브러리는 deps.sh, Rust 크레이트는 Cargo.lock.
ROOT := $(abspath ..)
# 파이썬 도구(capstone)는 uv가 uv.lock대로 맞춘 환경에서 돌린다.
PY ?= uv run --project $(ROOT) python
JAR ?= $(ROOT)/origin/010100D5.jar
B := build
GAME := $(B)/game
HOSTSRC := $(wildcard host/*.c)
HOSTOBJ := $(HOSTSRC:host/%.c=$(B)/%.o) $(patsubst host/%.cpp,$(B)/%.o,$(wildcard host/*.cpp))
# RmlUi와 FreeType(정적). deps.sh mac이 build/deps/mac에 빌드한다.
RML ?= $(B)/deps/mac/install
# 사람이 읽는 C로 옮긴 함수(restored/list.txt). 번역 코드에서는 빠지거나 orig_f_로 남는다.
# ONLY=<영역>이면 UI 래퍼는 restored/ui_<영역>*.c와 list.d/<영역>.txt만 넣는다(영역별 병렬 작업이 서로의 미완성 파일에 막히지 않게).
ifdef ONLY
RESTSRC := $(filter-out restored/ui_%.c,$(wildcard restored/*.c)) $(wildcard restored/ui_$(ONLY)*.c)
LISTD := $(wildcard restored/list.d/$(ONLY).txt)
else
RESTSRC := $(wildcard restored/*.c)
LISTD := $(wildcard restored/list.d/*.txt)
endif
RESTOBJ := $(RESTSRC:restored/%.c=$(B)/r_%.o)
# 디버깅: make TRFLAGS=-DTRACE_PC 면 접근마다 범위를 검사하고 pc를 남긴다.
TRFLAGS ?=
# 맥 빌드의 SDL3는 설치된 것을 쓴다. pkg-config가 알면 그대로, 모르면 Homebrew 접두사에서 찾는다.
BREW := $(shell brew --prefix 2>/dev/null)
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl3 2>/dev/null || echo $(if $(BREW),-I$(BREW)/include))
SDL_LIBS ?= $(shell pkg-config --libs sdl3 2>/dev/null || echo $(if $(BREW),-L$(BREW)/lib) -lSDL3)
SDL_CFLAGS := $(SDL_CFLAGS)
SDL_LIBS := $(SDL_LIBS)
CFLAGS := -O2 -g -w -std=gnu11 -Ihost $(SDL_CFLAGS)
LDFLAGS := $(SDL_LIBS) -L$(RML)/lib -lrmlui -lfreetype
# rustup의 cargo를 먼저 찾는다(Homebrew rust에는 교차 대상이 없다).
CARGO ?= PATH=$(if $(BREW),$(BREW)/opt/rustup/bin:)$$HOME/.cargo/bin:$$PATH cargo
SMAFEV := $(B)/smafev/release/libsmafev.a
# 크레이트를 이미 받아 두었으면 make CARGO_OFFLINE=--offline으로 네트워크 없이 빌드한다.
CARGO_OFFLINE ?=

.PHONY: all clean trace
all: $(B)/ecpe $(GAME)/client.bin $(B)/font.gly $(B)/1mgm.sf2 $(B)/ui/.stamp

$(GAME)/client.bin: $(JAR)
	@[ -f "$(JAR)" ] || { echo "원본 jar 경로를 준다: make JAR=<010100D5.jar 경로>" >&2; exit 1; }
	mkdir -p $(GAME)
	unzip -qo "$(JAR)" -d $(GAME)
	cp $(GAME)/client.bin1236 $(GAME)/client.bin

$(B)/codemap.json: $(GAME)/client.bin $(ROOT)/work/tools/codemap.py
	$(PY) $(ROOT)/work/tools/codemap.py $(GAME)/client.bin $@ > $(B)/codemap.txt

# 옮긴 함수 목록: restored/list.txt와 restored/list.d/*.txt(UI 영역마다 하나)를 합친다.
$(B)/list.txt: restored/list.txt $(LISTD)
	mkdir -p $(B)
	cat $^ > $@

$(B)/tr.c: $(B)/codemap.json $(ROOT)/work/tools/thumb2c.py $(B)/list.txt
	$(PY) $(ROOT)/work/tools/thumb2c.py $(GAME)/client.bin $< $@ $(B)/list.txt

$(B)/tr.o: $(B)/tr.c host/thumbrt.h
	cc -O1 -w -std=gnu11 -Ihost $(TRFLAGS) -c $< -o $@

$(B)/%.o: host/%.c host/host.h host/thumbrt.h host/ui.h
	mkdir -p $(B)
	cc $(CFLAGS) -c $< -o $@

$(B)/%.o: host/%.cpp host/ui.h host/ui_render.h host/host.h | $(RML)/lib/librmlui.a
	mkdir -p $(B)
	c++ -O2 -g -std=c++17 -DRMLUI_STATIC_LIB -Ihost $(SDL_CFLAGS) -I$(RML)/include -c $< -o $@

$(B)/deps/mac/install/lib/librmlui.a:
	./deps.sh mac

# UI 문서와 글꼴(실행 때 game 폴더 옆 ui/에서 읽는다)
# 리마스터 일러스트(work/illust/out/illust_*.png, 원본 파생물이라 저장소에는 없다. work/illust/README.md)도 같이 둔다.
ILLUST := $(wildcard $(ROOT)/work/illust/out/illust_*.png)
$(B)/ui/.stamp: $(wildcard ui/*) third_party/neodgm.ttf third_party/Galmuri9.ttf $(ILLUST)
	mkdir -p $(B)/ui
	cp ui/* third_party/neodgm.ttf third_party/Galmuri9.ttf $(ILLUST) $(B)/ui/
	touch $@

$(B)/r_%.o: restored/%.c restored/restored.h host/host.h host/thumbrt.h host/ui.h
	mkdir -p $(B)
	cc $(CFLAGS) -c $< -o $@

$(B)/ecpe: $(B)/tr.o $(HOSTOBJ) $(RESTOBJ) $(SMAFEV)
	c++ $^ $(LDFLAGS) -lm -o $@

# DrawString 글리프. WIE와 같은 neodgm.ttf(OFL)와 ab_glyph로 빌드 때 만든다.
# neodgm에 없는 KS X 1001 글자(∼, ♡, ★, 한자 등)는 Galmuri9(OFL, v2.40.4)에서 같은 크기로 가져온다.
$(B)/font.gly: third_party/neodgm.ttf third_party/Galmuri9.ttf $(ROOT)/work/tools/glyphdump/src/main.rs
	mkdir -p $(B)
	CARGO_TARGET_DIR=$(abspath $(B))/glyphdump $(CARGO) run -q $(CARGO_OFFLINE) --release --manifest-path $(ROOT)/work/tools/glyphdump/Cargo.toml -- $(abspath third_party/neodgm.ttf) $(abspath third_party/Galmuri9.ttf) $(abspath $@)

# 소리. 게임이 실행 중에 넣는 MMF를 바꾸는 정적 라이브러리(smaf-player, MIT).
$(SMAFEV): $(ROOT)/work/tools/smafev/src/lib.rs $(ROOT)/work/tools/smafev/Cargo.toml
	CARGO_TARGET_DIR=$(abspath $(B))/smafev $(CARGO) build -q $(CARGO_OFFLINE) --release --manifest-path $(ROOT)/work/tools/smafev/Cargo.toml

# 인터프리터 빌드: 번역 C(tr.o) 없이 client.bin의 코드를 직접 실행한다(interp/interp.c, 크로노스 윙과 같다). 웹 빌드의 바탕이다.
# 연결 파일(interp_gen.c)은 옮긴 함수 목록과 소스만 읽고 원본은 읽지 않는다.
INTERPSRC := $(RESTSRC) $(HOSTSRC) $(wildcard host/*.cpp) $(wildcard restored/*.h)
$(B)/interp_gen.c: $(B)/list.txt $(ROOT)/work/tools/interpgen.py $(INTERPSRC)
	mkdir -p $(B)
	python3 $(ROOT)/work/tools/interpgen.py $(B)/list.txt $@ $(INTERPSRC) > $(B)/interp_gen.txt
$(B)/interp_gen.o: $(B)/interp_gen.c host/host.h host/thumbrt.h
	cc $(CFLAGS) -c $< -o $@
$(B)/interp.o: interp/interp.c host/host.h host/thumbrt.h
	cc $(CFLAGS) -c $< -o $@
interp: $(B)/ecpe-interp $(GAME)/client.bin $(B)/font.gly $(B)/1mgm.sf2 $(B)/ui/.stamp
$(B)/ecpe-interp: $(B)/interp.o $(B)/interp_gen.o $(HOSTOBJ) $(RESTOBJ) $(SMAFEV)
	c++ $^ $(LDFLAGS) -lm -o $@
.PHONY: interp

# 웹 빌드(Emscripten, 인터프리터). 원본에서 나온 것은 넣지 않는다: 게임 데이터는 사용자가 고른 원본 파일을 브라우저가 풀어 쓴다(web/gamedata.js).
# 배포 꾸러미(ecpe.data)에는 UI 문서, 공개 글꼴(neodgm, Galmuri9)과 그것으로 만든 글리프 표(font.gly), 사운드폰트만 들어간다.
# 리마스터 일러스트(원본 파생물)는 넣지 않는다. 결과는 build/web 폴더 통째로 올린다. 서비스 워커는 https나 localhost에서만 돈다.
WEB := $(B)/web
WEBDEP := $(B)/deps/web/install
WEBOBJ := $(HOSTSRC:host/%.c=$(B)/web-obj/%.o) $(patsubst host/%.cpp,$(B)/web-obj/%.o,$(wildcard host/*.cpp)) $(RESTSRC:restored/%.c=$(B)/web-obj/r_%.o) $(B)/web-obj/interp.o $(B)/web-obj/interp_gen.o
WEBCFLAGS := -O2 -w -std=gnu11 -Ihost -I$(WEBDEP)/include
WEBSTATIC := index.html app.js gamedata.js manifest.webmanifest icon.svg icon-192.png icon-512.png
SMAFWASM := $(B)/smafev/wasm32-unknown-unknown/release/smafev.wasm

web: $(WEB)/ecpe.js $(SMAFWASM) $(addprefix web/,$(WEBSTATIC)) web/sw.js
	cd web && cp $(WEBSTATIC) $(abspath $(WEB))/
	cp $(SMAFWASM) $(WEB)/
	mkdir -p $(WEB)/licenses && cp third_party/*.txt $(WEB)/licenses/
	sed "s/__BUILD__/$$(cat $(WEB)/ecpe.js $(WEB)/ecpe.wasm $(WEB)/ecpe.data web/*.js web/index.html | shasum | cut -c1-12)/" web/sw.js > $(WEB)/sw.js

$(B)/web-ui/.stamp: $(wildcard ui/*) third_party/neodgm.ttf third_party/Galmuri9.ttf
	rm -rf $(B)/web-ui && mkdir -p $(B)/web-ui
	cp ui/*.rml ui/*.rcss third_party/neodgm.ttf third_party/Galmuri9.ttf $(B)/web-ui/
	touch $@

$(WEB)/ecpe.js: $(WEBOBJ) $(B)/web-ui/.stamp $(B)/font.gly third_party/1mgm.sf2
	mkdir -p $(WEB)
	em++ $(WEBOBJ) -O2 -L$(WEBDEP)/lib -lrmlui -lfreetype -lSDL3 -lidbfs.js \
		-sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=1048576 \
		-sEXPORTED_FUNCTIONS=_main,_malloc,_free,_web_save_picked -sEXPORTED_RUNTIME_METHODS=callMain,ccall,FS,IDBFS,UTF8ToString,HEAPU8 \
		--preload-file $(B)/web-ui@/ui --exclude-file '*.stamp' --preload-file $(B)/font.gly@/font.gly --preload-file third_party/1mgm.sf2@/1mgm.sf2 -o $@

# 소리 변환기(work/tools/smafev)를 따로 wasm으로. 호스트 audio.c가 페이지를 거쳐 부른다.
$(SMAFWASM): $(ROOT)/work/tools/smafev/src/lib.rs $(ROOT)/work/tools/smafev/Cargo.toml
	CARGO_TARGET_DIR=$(abspath $(B))/smafev $(CARGO) rustc -q $(CARGO_OFFLINE) --release --lib --crate-type cdylib --target wasm32-unknown-unknown --manifest-path $(ROOT)/work/tools/smafev/Cargo.toml

$(WEBDEP)/lib/librmlui.a:
	./deps.sh web

$(B)/web-obj/%.o: host/%.c host/host.h host/thumbrt.h host/ui.h | $(WEBDEP)/lib/librmlui.a
	mkdir -p $(B)/web-obj
	emcc $(WEBCFLAGS) -c $< -o $@

$(B)/web-obj/r_%.o: restored/%.c restored/restored.h host/host.h host/thumbrt.h host/ui.h | $(WEBDEP)/lib/librmlui.a
	mkdir -p $(B)/web-obj
	emcc $(WEBCFLAGS) -c $< -o $@

$(B)/web-obj/%.o: host/%.cpp host/ui.h host/ui_render.h host/host.h | $(WEBDEP)/lib/librmlui.a
	mkdir -p $(B)/web-obj
	em++ -O2 -std=c++17 -DRMLUI_STATIC_LIB -Ihost -I$(WEBDEP)/include -c $< -o $@

$(B)/web-obj/interp.o: interp/interp.c host/host.h host/thumbrt.h | $(WEBDEP)/lib/librmlui.a
	mkdir -p $(B)/web-obj
	emcc $(WEBCFLAGS) -c $< -o $@

$(B)/web-obj/interp_gen.o: $(B)/interp_gen.c host/host.h host/thumbrt.h | $(WEBDEP)/lib/librmlui.a
	mkdir -p $(B)/web-obj
	emcc $(WEBCFLAGS) -c $< -o $@
.PHONY: web

# 진단용: 함수 드나듦을 기록하는 빌드. ctrl 파일의 `calltree <파일>`로 한 프레임의 호출 나무를 남긴다.
trace: $(B)/ecpe-trace
$(B)/tr_ts.o: $(B)/tr.c host/thumbrt.h
	cc -O1 -w -std=gnu11 -Ihost -DTRACE_STACK -c $< -o $@
$(B)/ecpe-trace: $(B)/tr_ts.o $(HOSTOBJ) $(RESTOBJ) $(SMAFEV)
	c++ $^ $(LDFLAGS) -lm -o $@

$(B)/1mgm.sf2:
	cp third_party/1mgm.sf2 $@

clean:
	rm -rf $(B)

# Windows 교차 빌드(mingw-w64, x86_64). 결과는 build/win 폴더(옮기기용 build/ecpe-win.zip)를 통째로 옮겨 ecpe.exe를 실행한다(실행 파일 옆 game 폴더를 쓴다).
# SDL3 mingw 개발 패키지와 Windows용 RmlUi·FreeType은 deps.sh win이 build/deps에 준비한다.
# smafev는 rustup target add x86_64-pc-windows-gnu가 필요하다.
CW := $(B)/deps
WIN := $(B)/win
WOBJD := $(B)/win-obj
WCC := x86_64-w64-mingw32-gcc
WCXX := x86_64-w64-mingw32-g++
WSDL ?= $(CW)/mingw/x86_64-w64-mingw32
WRML ?= $(CW)/win/install
WSMAFEV := $(B)/smafev/x86_64-pc-windows-gnu/release/libsmafev.a
# 배포용: 실행 파일에 빌드한 컴퓨터의 경로(사용자 이름)가 남지 않게 홈 폴더 앞부분을 지운다(Android build.sh의 MAP과 같다).
WMAP := -ffile-prefix-map=$(HOME)/=
WRUSTFLAGS := --remap-path-prefix=$(HOME)/.cargo/registry/src/=crates/ --remap-path-prefix=$(HOME)/=
WCFLAGS := -O2 -w -std=gnu11 $(WMAP) -D__USE_MINGW_ANSI_STDIO=1 -Ihost -I$(WSDL)/include
WOBJ := $(patsubst host/%.c,$(WOBJD)/%.o,$(wildcard host/*.c)) $(patsubst host/%.cpp,$(WOBJD)/%.o,$(wildcard host/*.cpp)) $(RESTSRC:restored/%.c=$(WOBJD)/r_%.o)

$(WRML)/lib/librmlui.a:
	./deps.sh win

windows: $(WRML)/lib/librmlui.a $(WIN)/ecpe.exe $(GAME)/client.bin $(B)/font.gly $(B)/1mgm.sf2 $(B)/ui/.stamp
	cp $(WSDL)/bin/SDL3.dll $(B)/font.gly $(B)/1mgm.sf2 $(WIN)/
	cp "$$(find $$(dirname $$(dirname $$(realpath $$(which $(WCC))))) -name libwinpthread-1.dll | grep x86_64 | head -1)" $(WIN)/
	rm -rf $(WIN)/ui && mkdir -p $(WIN)/ui && cp $(B)/ui/* $(WIN)/ui/
	rm -rf $(WIN)/game && mkdir -p $(WIN)/game && cp $(GAME)/client.bin $(GAME)/dataset.dat $(GAME)/map.pmd $(GAME)/res.gmf $(GAME)/script.bin $(GAME)/tileset.pts $(WIN)/game/
	cp third_party/*.txt $(WIN)/
	rm -f $(B)/ecpe-win.zip && cd $(B) && cp -R win ecpe-win && zip -qr ecpe-win.zip ecpe-win && rm -rf ecpe-win

$(WIN)/ecpe.exe: $(WOBJD)/tr.o $(WOBJ) $(WOBJD)/res.o $(WSMAFEV)
	mkdir -p $(WIN)
	$(WCXX) $^ -L$(WSDL)/lib -L$(WRML)/lib -lrmlui -lfreetype -lSDL3 -lws2_32 -luserenv -lbcrypt -lntdll -lm -static-libgcc -static-libstdc++ -mwindows -o $@

$(WOBJD)/tr.o: $(B)/tr.c host/thumbrt.h
	mkdir -p $(WOBJD)
	$(WCC) -O1 -w -std=gnu11 $(WMAP) -Ihost $(TRFLAGS) -c $< -o $@

$(WOBJD)/%.o: host/%.c host/host.h host/thumbrt.h host/ui.h
	mkdir -p $(WOBJD)
	$(WCC) $(WCFLAGS) -c $< -o $@

$(WOBJD)/%.o: host/%.cpp host/ui.h host/ui_render.h host/host.h
	mkdir -p $(WOBJD)
	$(WCXX) -O2 -std=c++17 $(WMAP) -DRMLUI_STATIC_LIB -Ihost -I$(WSDL)/include -I$(WRML)/include -c $< -o $@

$(WOBJD)/r_%.o: restored/%.c restored/restored.h host/host.h host/thumbrt.h host/ui.h
	mkdir -p $(WOBJD)
	$(WCC) $(WCFLAGS) -c $< -o $@

# 아이콘은 맥 빌드로 타이틀 화면을 받아 만든다(windows/mkico.sh).
$(B)/ecpe.ico: | $(B)/ecpe
	windows/mkico.sh $(B) $@

$(WOBJD)/res.o: windows/ecpe.rc windows/ecpe.manifest $(B)/ecpe.ico
	mkdir -p $(WOBJD)
	cp windows/ecpe.manifest $(B)/ecpe.ico $(WOBJD)/
	cp windows/ecpe.rc $(WOBJD)/ecpe.rc
	cd $(WOBJD) && x86_64-w64-mingw32-windres ecpe.rc -o res.o

$(WSMAFEV): $(ROOT)/work/tools/smafev/src/lib.rs $(ROOT)/work/tools/smafev/Cargo.toml
	CARGO_TARGET_DIR=$(abspath $(B))/smafev RUSTFLAGS="$(WRUSTFLAGS)" $(CARGO) build -q $(CARGO_OFFLINE) --release --target x86_64-pc-windows-gnu --manifest-path $(ROOT)/work/tools/smafev/Cargo.toml
.PHONY: windows

# Android APK(arm64). build/android/ecpe.apk. SDL·RmlUi·FreeType 소스는 deps.sh android가 build/deps에 받는다(android/build.sh).
# 아이콘을 맥 빌드(build/ecpe)의 타이틀 프레임에서 만들므로 all이 먼저다.
android: all
	[ -d $(B)/deps/src ] || ./deps.sh android
	android/build.sh
.PHONY: android
