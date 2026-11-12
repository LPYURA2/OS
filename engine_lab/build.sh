#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
if command -v gcc >/dev/null; then CC="${CC:-gcc}"; else CC="${CC:-clang}"; fi
if command -v g++ >/dev/null; then CXX="${CXX:-g++}"; else CXX="${CXX:-clang++}"; fi
CFLAGS="-O2 -fPIC -std=c11 -Wall -Wextra -DMW_BUILD -Iinclude"
"$CC" $CFLAGS -c src/mw_engine.c -o build/mw_engine.o
"$CXX" -O2 -fPIC -std=c++17 -Wall -Wextra -DMW_BUILD -Iinclude -c src/mw_scene.cpp -o build/mw_scene.o
"$CXX" -shared build/mw_engine.o build/mw_scene.o -o build/libmatryoshka_engine.so
"$CXX" -O2 -std=c++17 -Iinclude src/demo.cpp -Lbuild -lmatryoshka_engine -Wl,-rpath,'$ORIGIN' -o build/demo
build/demo build/frame_cpp.ppm
python3 python/demo.py
if command -v javac >/dev/null && command -v java >/dev/null; then
    JAVA_HOME="${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")}";
    "$CC" -O2 -std=c11 -fPIC -shared -Iinclude -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/linux" src/mw_jni.c -Lbuild -lmatryoshka_engine -Wl,-rpath,'$ORIGIN' -o build/libmatryoshka_jni.so
    javac -d build java/MatryoshkaEngine.java
    java -Djava.library.path=build -cp build MatryoshkaEngine build/frame_java.ppm
fi
python3 tests/verify.py
