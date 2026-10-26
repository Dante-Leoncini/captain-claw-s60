#!/bin/bash
# Builds the S60 code path (C++03 sources + sdl2lite) as a Windows exe with MinGW, to test the
# port on the PC: symbian/tools/build_pc.sh [-j N]   ->  build_pc/openclaw_s60.exe
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
export PATH=/c/msys64/mingw64/bin:$PATH
JOBS=${JOBS:-8}
OUT=build_pc
mkdir -p $OUT/obj

GAME_FLAGS="-std=gnu++98 -O2 -g -Wno-c++0x-compat -Wno-deprecated -DSDL2LITE -DTIXML_USE_STL -DOPENCLAW_FRAME_PROFILE
 -include symbian/compat/cxx03.h
 -Isymbian/sdl2lite/include -IBox2D -IThirdParty -IThirdParty/Tinyxml -Ilibwap"
SHIM_FLAGS="-std=gnu++98 -O2 -g -Isymbian/sdl2lite/include -Ilibwap"

GAME_SRCS="$(find OpenClaw libwap Box2D/Box2D -name '*.cpp' | grep -v 'OpenClaw/Engine/Resource/Miniz.cpp' | sort)
ThirdParty/Tinyxml/tinyxml.cpp
ThirdParty/Tinyxml/tinystr.cpp
ThirdParty/Tinyxml/tinyxmlerror.cpp
ThirdParty/Tinyxml/tinyxmlparser.cpp"
SHIM_SRCS="$(ls symbian/sdl2lite/src/*.cpp symbian/sdl2lite/src/pc/*.cpp)"
C_SRCS="ThirdParty/libsmacker/smacker.c"

compile() {
    local flags="$1" src="$2"
    local obj="$OUT/obj/$(echo "$src" | tr '/' '_' | sed 's/\.c\(pp\)\?$/.o/')"
    local cc=g++
    case "$src" in *.c) cc=gcc ;; esac
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
        $cc $flags -c "$src" -o "$obj" 2>&1 | grep -E "error" && return 1
    fi
    return 0
}
export -f compile
export OUT

fail=0
echo "$GAME_SRCS" | xargs -P $JOBS -I{} bash -c 'compile "$0" "{}"' "$GAME_FLAGS" || fail=1
echo "$SHIM_SRCS" | xargs -P $JOBS -I{} bash -c 'compile "$0" "{}"' "$SHIM_FLAGS" || fail=1
echo "$C_SRCS" | xargs -P $JOBS -I{} bash -c 'compile "$0" "{}"' "-O2 -g -IThirdParty/libsmacker" || fail=1
[ $fail = 0 ] || { echo "COMPILE FAILED"; exit 1; }

g++ -o $OUT/openclaw_s60.exe $OUT/obj/*.o -lopengl32 -lgdi32 -lwinmm -static \
    && echo "OK: $OUT/openclaw_s60.exe"
