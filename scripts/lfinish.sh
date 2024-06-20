# scripts/make-asmjit.sh linux
gcc -std=gnu99 -c core/sha256.c -O3 -o build/sha256.o -fPIC -I include
gcc -std=gnu99 -c compiler/cvm.c -O3 -o build/cvm.o -fPIC -I include
gcc -std=c++11 -c runtime/linenoise-ng/linenoise.cpp -O3 -o build/linenoise.o -I include -I runtime/linenoise-ng
gcc -std=c++11 -c runtime/linenoise-ng/ConvertUTF.cpp -O3 -o build/ConvertUTF.o -I runtime/linenoise-ng
gcc -std=c++11 -c runtime/linenoise-ng/wcwidth.cpp -O3 -o build/wcwidth.o -I runtime/linenoise-ng
gcc -std=gnu99 core/threadedreader.c runtime/driver.c compiler/exec-alloc.c build/linenoise.o build/ConvertUTF.o build/wcwidth.o build/cvm.o build/sha256.o lstanza.s -o lstanza -DPLATFORM_LINUX -D_GNU_SOURCE -lm -ldl -fPIC -I include -Lbin -lasmjit-linux -lstdc++ -lrt -lpthread
