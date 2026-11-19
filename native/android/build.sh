#!/bin/zsh
# APK 빌드. Gradle 없이 NDK, build-tools로 만든다(크로노스 윙 android/build.sh와 같은 구조).
# 먼저 native에서 make를 돌려 build/tr.c, 게임 데이터, font.gly, ui를 준비한다(make android가 알아서 한다).
# 결과: build/android/ecpe.apk (android/ecpe.keystore 서명, arm64-v8a)
# SDL·RmlUi·FreeType 소스는 deps.sh android가 받아 둔 build/deps를 쓴다. 다른 곳에 받아 두었으면 CW=<그 native 폴더>로 가리킨다.
set -eu
HERE=${0:A:h}; N=$HERE:h; B=$N/build; O=$B/android
CW=${CW:-$N}
BREW=$(brew --prefix 2>/dev/null || true)
SDK=${ANDROID_HOME:-$BREW/share/android-commandlinetools}
NDK=${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}; BT=$SDK/build-tools/35.0.0; JAR=$SDK/platforms/android-35/android.jar
JDK=${JAVA_HOME:+$JAVA_HOME/bin}; JDK=${JDK:-$BREW/opt/openjdk@21/bin}; export PATH=$JDK:$HOME/.cargo/bin:$PATH
SRC=$CW/build/deps/src; ABI=arm64-v8a; API=24
TC=$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin
mkdir -p $O/lib/lib/$ABI $O/java $O/assets

# 배포용으로 실행 파일에 빌드한 컴퓨터의 경로(사용자 이름, 폴더 구조)가 남지 않게 __FILE__과 디버그 경로를 바꾼다.
# 홈 폴더 아래는 앞부분을 지우고(~/private/... → private/...), NDK는 ndk/로. Rust는 --remap-path-prefix.
MAP="-ffile-prefix-map=$HOME/= -ffile-prefix-map=$NDK/=ndk/ -ffile-prefix-map=$SDK/=sdk/"
export RUSTFLAGS="--remap-path-prefix=$HOME/= --remap-path-prefix=$HOME/.cargo/registry/src/=crates/"

# RmlUi와 FreeType(정적). 소스는 build/deps 것을 쓰되, 경로를 지운 채 여기(build/android/deps)에 따로 빌드한다
# (deps.sh android의 install은 경로가 남아 있어 쓰지 않는다). 구성은 deps.sh android와 같다.
RML=$O/deps/install
if [[ ! -f $RML/lib/librmlui.a ]]; then
  D=$CW/build/deps
  C=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=$RML -DCMAKE_FIND_ROOT_PATH=$RML -DCMAKE_PREFIX_PATH=$RML
     -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH
     -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API -DANDROID_STL=c++_static
     "-DCMAKE_C_FLAGS=$MAP" "-DCMAKE_CXX_FLAGS=$MAP")
  cmake -S $D/freetype-2.13.3 -B $O/deps/ft $C -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON >/dev/null
  cmake --build $O/deps/ft --target install >/dev/null
  cmake -S $D/rmlui -B $O/deps/rml $C -DBUILD_SHARED_LIBS=OFF -DRMLUI_SAMPLES=OFF -DBUILD_TESTING=OFF -DRMLUI_FONT_ENGINE=freetype >/dev/null
  cmake --build $O/deps/rml --target install >/dev/null
fi

# SDL3. 16KB 페이지 기기에서도 올라가도록 세그먼트를 16KB로 정렬한다. 경로도 지운다(위 MAP).
if [[ ! -f $O/sdl16k/libSDL3.so ]]; then
  cmake -S $SRC -B $O/sdl16k -G Ninja "-DCMAKE_C_FLAGS=$MAP" -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384 -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API -DCMAKE_BUILD_TYPE=Release -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
  cmake --build $O/sdl16k --target SDL3-shared
fi
cp $O/sdl16k/libSDL3.so $O/lib/lib/$ABI/

# 소리 변환 Rust 정적 라이브러리(aarch64-linux-android). 크레이트는 --offline으로 이미 받아 둔 것을 쓴다.
# 필요하면 먼저: rustup target add aarch64-linux-android
SMAFEV=$O/smafev/aarch64-linux-android/release/libsmafev.a
CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER=$TC/aarch64-linux-android$API-clang CC_aarch64_linux_android=$TC/aarch64-linux-android$API-clang AR_aarch64_linux_android=$TC/llvm-ar \
  CARGO_TARGET_DIR=$O/smafev cargo build -q ${CARGO_OFFLINE:-} --release --target aarch64-linux-android --manifest-path $N/../work/tools/smafev/Cargo.toml

# 게임
CC=$TC/aarch64-linux-android$API-clang
INC=(-Ihost -I$SRC/include)
cd $N
OBJS=()
mkdir -p $O/obj
for f in host/*.c restored/*.c build/ver_tables.c; do
  o=$O/obj/${${f//\//_}%.c}.o; OBJS+=$o
  [[ $o -nt $f && $o -nt host/host.h && $o -nt host/ui.h ]] || $CC -O2 -g0 -w -fPIC -std=gnu11 ${=MAP} $INC -c $f -o $o &
done
for f in host/*.cpp; do
  o=$O/obj/${${f//\//_}%.cpp}.o; OBJS+=$o
  [[ $o -nt $f && $o -nt host/ui.h && $o -nt host/ui_render.h ]] || $CC++ -O2 -g0 -w -fPIC -std=c++17 ${=MAP} -DRMLUI_STATIC_LIB $INC -I$RML/include -c $f -o $o &
done
tr=$O/obj/tr.o; OBJS+=$tr
[[ $tr -nt $B/tr.c ]] || $CC -O1 -g0 -w -fPIC -fvisibility=hidden -std=gnu11 ${=MAP} -Ihost -c $B/tr.c -o $tr &
wait
$CC++ -shared -static-libstdc++ -Wl,-z,max-page-size=16384 $OBJS $SMAFEV -L$O/lib/lib/$ABI -L$RML/lib -lrmlui -lfreetype -lSDL3 -lm -ldl -landroid -llog -o $O/lib/lib/$ABI/libmain.so

# 자바와 패키징. 세이브 데이터 내보내기/불러오기(host/savefile.c)의 파일 창 때문에 SDLActivity를 고쳐 쓴다(patch_activity.py).
python3 $HERE/mkpak.py $B $O/assets/game.pak $B/ui
JSRC=$O/java-src; rm -rf $JSRC; mkdir -p $JSRC; cp $SRC/android-project/app/src/main/java/org/libsdl/app/*.java $JSRC/
python3 $HERE/patch_activity.py $JSRC/SDLActivity.java
$JDK/javac -nowarn -source 8 -target 8 -cp $JAR -d $O/java $JSRC/*.java 2>/dev/null
$BT/d8 --release --lib $JAR --min-api $API --output $O $O/java/org/libsdl/app/*.class
# 아이콘. 타이틀 화면을 맥 빌드로 띄워 글자가 다 나온 안정 프레임(300)을 받아 만든다. 원본 파생물이라 build에만 둔다.
# 첫 실행은 "속도최적화" 뒤 gopt.sav를 남기고 끝나므로 두 번 돌린다(첫 번째는 그 종료까지 충분히 돌려야 한다).
if [[ ! -f $O/res/mipmap-anydpi-v26/ic_launcher.xml ]]; then
  F=$O/icon-frames; rm -rf $F $O/res; mkdir -p $F/save
  print '500 quit' > $F/script.txt
  ECPE_HEADLESS=1 ECPE_SCRIPT=$F/script.txt $B/ecpe $B/game --windowed --width 240 --save $F/save >/dev/null 2>&1 || true
  print '300 quit' > $F/script.txt
  ECPE_HEADLESS=1 ECPE_FRAMELOG=$F ECPE_SCRIPT=$F/script.txt $B/ecpe $B/game --windowed --width 240 --save $F/save >/dev/null 2>&1
  python3 $HERE/mkicon.py $F/$(awk '$1==299{print $2}' $F/log.txt).raw $O/res
fi
$BT/aapt2 compile --dir $O/res -o $O/res.zip
$BT/aapt2 link ${ECPE_DEBUGGABLE:+--debug-mode} -o $O/base.apk -I $JAR --manifest $HERE/AndroidManifest.xml -A $O/assets -R $O/res.zip --auto-add-overlay
python3 $HERE/patch_manifest.py $O/base.apk $BT/aapt2
cd $O
zip -q -j base.apk classes.dex
(cd lib && zip -q -r ../base.apk lib)
$BT/zipalign -f -p 16 base.apk aligned.apk
# 서명 키. 키가 바뀌면 설치된 앱을 지워야 업데이트되므로(세이브 유실) build 밖에 두고 계속 쓴다. git에는 올리지 않는다(.gitignore).
KS=$HERE/ecpe.keystore
[[ -f $KS ]] || $JDK/keytool -genkeypair -keystore $KS -storetype PKCS12 -storepass ecpe-android -keypass ecpe-android \
  -alias ecpe -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Epic Chronicle PE" 2>/dev/null
$BT/apksigner sign --ks $KS --ks-key-alias ecpe --ks-pass pass:ecpe-android --out ecpe.apk aligned.apk
ls -la ecpe.apk
