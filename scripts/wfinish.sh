#!/usr/bin/env bash

set -e

if [[ -z "$CC" ]]; then
    if [[ "$(uname -s)" == MINGW* ]]; then
        # Not cross-compiling, so just use the stock GCC
        CC=gcc
    else
        echo 'Need to set $CC to a valid compiler or cross-compiler' >/dev/stderr
        exit 2
    fi
fi

CCFLAGS="-I include -DPLATFORM_WINDOWS -std=gnu99 -O3 -fPIC $CCFLAGS"
CXXFLAGS="-I include -DPLATFORM_WINDOWS -std=c++11 -O3 -fPIC $CXXFLAGS"
LINENOISEFLAG="-D_WIN32 -I runtime/linenoise-ng $CXXFLAGS"

[[ ! -d "build" ]] && mkdir "build"

"$CC" $CCFLAGS -c core/sha256.c       -o build/sha256.o
"$CC" $CCFLAGS -c compiler/cvm.c      -o build/cvm.o
"$CC" $LINENOISEFLAGS -c runtime/linenoise-ng/linenoise.cpp  -o build/linenoise-ng.o
"$CC" $LINENOISEFLAGS -c runtime/linenoise-ng/ConvertUTF.cpp -o build/ConvertUTF.o
"$CC" $LINENOISEFLAGS -c runtime/linenoise-ng/wcwidth.cpp    -o build/wcwidth.o
"$CC" $CCFLAGS -c runtime/driver.c    -o build/driver.o

"$CC" $CCFLAGS \
    core/threadedreader.c \
    core/dynamic-library.c \
    compiler/exec-alloc.c \
    build/sha256.o    \
    build/cvm.o       \
    build/linenoise-ng.o \
    build/ConvertUTF.o \
    build/wcwidth.o \
    build/driver.o    \
    wstanza.s         \
    -o wstanza -Wl,-Bstatic -lm -lpthread -fPIC \
    -Lbin -lasmjit-windows -lstdc++
