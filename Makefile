EMCCFlags :=
EMCCFlags += -s DISABLE_EXCEPTION_CATCHING=1
EMCCFlags += -s ERROR_ON_UNDEFINED_SYMBOLS=1
EMCCFlags += -s ALLOW_MEMORY_GROWTH=1
EMCCFlags += -s USE_WEBGL2=1
EMCCFlags += -s "MALLOC='emmalloc'"
EMCCFlags += -s NO_FILESYSTEM=1
EMCCFlags += -s WASM=1
EMCCFlags += -s ASSERTIONS=1
EMCCFlags += -fstrict-aliasing
EMCCFlags += -fno-exceptions
EMCCFlags += -fno-rtti
EMCCFlags += -flto
EMCCFlags += -DNDEBUG
EMCCFlags += -DSOKOL_GLES3
EMCCFlags += -s ENVIRONMENT=web,worker
EMCCFlags += -O3
EMCCFlags += -s "EXPORTED_FUNCTIONS=['_main', '_calloc']"
EMCCFlags += -s "EXPORTED_RUNTIME_METHODS=['ccall']"
EMCCFlags += -s USE_PTHREADS=1
EMCCFlags += -s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency

Includes = -Ilib -Ietterna

emscripten:
	emcc $(EMCCFlags) -msse -msimd128 $(Includes) lib/sqlite3.c src/seeminacalc.c src/cpp.cpp -o web/seeminacalc.js
	emcc $(EMCCFlags) -DNO_SSE $(Includes) lib/sqlite3.c src/seeminacalc.c src/cpp.cpp -o web/seeminacalc.nosse.js

ssefeaturetest:
	emcc -Os --no-entry -msse -msimd128 src/ssefeaturetest.c -o ssefeaturetest.wasm
