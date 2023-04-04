#scripts/make-asmjit.sh linux
#gcc -std=gnu99 -c core/sha256.c -O3 -o build/sha256.o -fPIC -I include
#gcc -std=gnu99 -c compiler/cvm.c -O3 -o build/cvm.o -fPIC -I include
#gcc -std=gnu99 core/threadedreader.c runtime/driver.c runtime/linenoise.c build/cvm.o build/sha256.o lstanza.s -o lstanza -DPLATFORM_LINUX -lm -ldl -fPIC -I include -Lbin -lasmjit-linux -lstdc++ -lrt -lpthread

../stanza ../core/stanza.proj ../compiler/stanza.proj ../core/debug/stanza.proj debug-adapter -s debug-adapter.s -pkg ../pkgs -optimize
STANZADIR=/home/opliss/Work/lbstanza
cc debug-adapter.s -DBUILD_DEBUG_ADAPTER -D_GNU_SOURCE -std=gnu99 -I$STANZADIR/include $STANZADIR/runtime/driver.c $STANZADIR/core/debug/debug-adapter.c $STANZADIR/core/sighandler.c $STANZADIR/runtime/linenoise.c -lm -lpthread -ldl -fPIC -DPLATFORM_LINUX -o debug-adapter
cp debug-adapter /home/opliss/Work/lbstanza/runtime/debug
