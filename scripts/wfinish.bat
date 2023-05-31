call scripts\make-asmjit.bat
gcc -std=gnu99 -c core/sha256.c -O3 -o build/sha256.o -I include
gcc -std=gnu99 -c compiler/cvm.c -O3 -o build/cvm.o -I include
gcc -std=c++11 -D_WIN32 -c runtime/linenoise-ng/linenoise.cpp -O3 -o build/linenoise-ng.o -I include -I runtime/linenoise-ng
gcc -std=c++11 -D_WIN32 -c runtime/linenoise-ng/ConvertUTF.cpp -O3 -o build/ConvertUTF.o -I runtime/linenoise-ng
gcc -std=c++11 -D_WIN32 -c runtime/linenoise-ng/wcwidth.cpp -O3 -o build/wcwidth.o -I runtime/linenoise-ng
gcc -std=gnu99 core/threadedreader.c core/dynamic-library.c compiler/exec-alloc.c runtime/driver.c build/cvm.o build/sha256.o build/linenoise-ng.o build/ConvertUTF.o build/wcwidth.o wstanza.s -o wstanza -DPLATFORM_WINDOWS -Wl,-Bstatic -lm -lpthread -I include -Lbin -lasmjit-windows -lstdc++
