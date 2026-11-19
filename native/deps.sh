#!/bin/zsh
# deps.sh <mac|win|android|web>: FreeType과 RmlUi를 정적 라이브러리로 build/deps/<대상>에 빌드한다. 소스가 build/deps에 없으면 공개 배포본을 받아 해시를 확인한다.
# web은 Emscripten으로 SDL3(build/deps/src)도 함께 빌드한다. 스레드는 쓰지 않는다. mac은 설치된 SDL3를 쓴다(Makefile의 SDL_CFLAGS, SDL_LIBS).
# Android SDK는 ANDROID_HOME, NDK는 ANDROID_NDK_HOME으로 넘긴다. 없으면 Homebrew의 android-commandlinetools를 본다.
# 여기서 받는 것은 모두 공개 라이브러리다. 게임 원본은 이 저장소에도, 이 스크립트가 받는 것에도 없다.
set -eu
P=$1; N=${0:A:h}; D=$N/build/deps; O=$D/$P; I=$O/install
BREW=$(brew --prefix 2>/dev/null || true)
SDK=${ANDROID_HOME:-$BREW/share/android-commandlinetools}
NDK=${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}
mkdir -p $D
# 의존성의 판과 해시는 여기 한 곳에 적는다. 판을 올릴 때는 해시도 같이 바꾸고 build/deps의 풀어 둔 폴더를 지운다.
FT=freetype-2.13.3; SDL=3.4.18; RML=6.3
# fetch <파일> <sha256> <주소>: 없으면 받고, 해시가 다르면 멈춘다.
fetch() {
  [[ -f $D/$1 ]] || curl -fsSL -o $D/$1 $3
  [[ $(shasum -a 256 $D/$1 | cut -d' ' -f1) == $2 ]] || { print -u2 "해시가 다르다: $D/$1"; exit 1 }
}
if [[ ! -d $D/$FT ]]; then
  fetch $FT.tar.xz 0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289 https://download.savannah.gnu.org/releases/freetype/$FT.tar.xz
  tar xJf $D/$FT.tar.xz -C $D
fi
if [[ ! -d $D/rmlui ]]; then
  fetch rmlui-$RML.tar.gz d977298bb6147610e5984d5db85ddf284020d655a8713913f6982074f1dbdede https://github.com/mikke89/RmlUi/archive/refs/tags/$RML.tar.gz
  mkdir -p $D/rmlui && tar xzf $D/rmlui-$RML.tar.gz -C $D/rmlui --strip-components 1
fi
# SDL3 소스는 web(정적 빌드)과 android(android/build.sh가 16KB 정렬로 빌드)가 쓴다.
if [[ ($P == web || $P == android) && ! -d $D/src ]]; then
  fetch SDL3-$SDL.tar.gz 9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3 https://github.com/libsdl-org/SDL/releases/download/release-$SDL/SDL3-$SDL.tar.gz
  mkdir -p $D/src && tar xzf $D/SDL3-$SDL.tar.gz -C $D/src --strip-components 1
fi
# Windows는 SDL이 배포하는 mingw 개발 패키지를 그대로 쓴다.
if [[ $P == win && ! -d $D/mingw ]]; then
  fetch SDL3-devel-$SDL-mingw.tar.gz 049b9dd711e3fb3f8d84feb998dfec54a75e17cf539c4a80ff4d058d839771cf https://github.com/libsdl-org/SDL/releases/download/release-$SDL/SDL3-devel-$SDL-mingw.tar.gz
  mkdir -p $D/mingw && tar xzf $D/SDL3-devel-$SDL-mingw.tar.gz -C $D/mingw --strip-components 1
fi
case $P in
  mac) T=() ;;
  win) T=(-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres) ;;
  android) T=(-DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static) ;;
  web) T=(-DCMAKE_TOOLCHAIN_FILE=$(em-config EMSCRIPTEN_ROOT)/cmake/Modules/Platform/Emscripten.cmake) ;;
  *) print "대상: mac win android web"; exit 1 ;;
esac
C=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=$I -DCMAKE_FIND_ROOT_PATH=$I -DCMAKE_PREFIX_PATH=$I -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH)
if [[ $P == web ]]; then
  cmake -S $D/src -B $O/sdl $C $T -DBUILD_SHARED_LIBS=OFF -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_PTHREADS=OFF >/dev/null
  cmake --build $O/sdl --target install >/dev/null
fi
cmake -S $D/$FT -B $O/ft $C $T -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON >/dev/null
cmake --build $O/ft --target install >/dev/null
cmake -S $D/rmlui -B $O/rml $C $T -DBUILD_SHARED_LIBS=OFF -DRMLUI_SAMPLES=OFF -DBUILD_TESTING=OFF -DRMLUI_FONT_ENGINE=freetype >/dev/null
cmake --build $O/rml --target install >/dev/null
ls $I/lib/*.a
