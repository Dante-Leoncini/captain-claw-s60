#!/bin/bash
# Syntax-checks the game sources as C++98 against sdl2lite + the C++03 compat header.
# Usage: symbian/tools/check_cxx03.sh [file.cpp ...]   (default: every source)
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
export PATH=/c/msys64/mingw64/bin:$PATH

FLAGS="-std=gnu++98 -fsyntax-only -Wno-c++0x-compat -Wno-deprecated -DSDL2LITE -DTIXML_USE_STL -Werror=c++11-extensions
 -include symbian/compat/cxx03.h
 -Isymbian/sdl2lite/include -IBox2D -IThirdParty -IThirdParty/Tinyxml -Ilibwap"

if [ $# -gt 0 ]; then
    FILES="$@"
else
    FILES="$(find OpenClaw libwap -name '*.cpp' | grep -v '/Touch/' | sort)
ThirdParty/Tinyxml/tinyxml.cpp ThirdParty/Tinyxml/tinystr.cpp ThirdParty/Tinyxml/tinyxmlerror.cpp ThirdParty/Tinyxml/tinyxmlparser.cpp
$(find Box2D/Box2D -name '*.cpp' | sort)"
fi

fail=0
for f in $FILES; do
    out=$(g++ $FLAGS "$f" 2>&1 | grep -E "error|Error" | head -${MAXERR:-5})
    if [ -n "$out" ]; then
        echo "== $f"
        echo "$out"
        fail=$((fail+1))
    fi
done
echo "files with errors: $fail"
