EMCCFlags :=
EMCCFlags += -fno-strict-aliasing
EMCCFlags += -fno-exceptions
EMCCFlags += -fno-rtti
EMCCFlags += -flto
EMCCFlags += -DNDEBUG
EMCCFlags += -DSOKOL_GLES3

EMCCFlagsSqlite += -Os
EMCCFlagsSqlite += -DSQLITE_ENABLE_DESERIALIZE
EMCCFlagsSqlite += -DSQLITE_THREADSAFE=0
EMCCFlagsSqlite += -DSQLITE_DEFAULT_MEMSTATUS=0
EMCCFlagsSqlite += -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
EMCCFlagsSqlite += -DSQLITE_LIKE_DOESNT_MATCH_BLOBS
EMCCFlagsSqlite += -DSQLITE_MAX_EXPR_DEPTH=0
EMCCFlagsSqlite += -DSQLITE_OMIT_DECLTYPE
EMCCFlagsSqlite += -DSQLITE_OMIT_DEPRECATED
EMCCFlagsSqlite += -DSQLITE_OMIT_PROGRESS_CALLBACK
EMCCFlagsSqlite += -DSQLITE_OMIT_SHARED_CACHE
EMCCFlagsSqlite += -DSQLITE_USE_ALLOCA
EMCCFlagsSqlite += -DSQLITE_DQS=0
EMCCFlagsSqlite += -DSQLITE_MAX_EXPR_DEPTH=0

EMCCFlagsBinary += -O3
EMCCFlagsBinary += -s DISABLE_EXCEPTION_CATCHING=1
EMCCFlagsBinary += -s ERROR_ON_UNDEFINED_SYMBOLS=1
EMCCFlagsBinary += -s ALLOW_MEMORY_GROWTH=1
EMCCFlagsBinary += -s USE_WEBGL2=1
EMCCFlagsBinary += -s "MALLOC='emmalloc'"
EMCCFlagsBinary += -s -lidbfs.js
EMCCFlagsBinary += -s WASM=1
EMCCFlagsBinary += -s ASSERTIONS=1
EMCCFlagsBinary += -s ENVIRONMENT=web,worker
EMCCFlagsBinary += -s "EXPORTED_FUNCTIONS=['_main', '_calloc']"
EMCCFlagsBinary += -s "EXPORTED_RUNTIME_METHODS=['ccall']"
EMCCFlagsBinary += -s USE_PTHREADS=1
EMCCFlagsBinary += -s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency

Includes = -Ilib -Ietterna

emscripten: web/sqlite3.o
	emcc $(EMCCFlags) $(EMCCFlagsBinary) -msse -msimd128 $(Includes) web/sqlite3.o src/seeminacalc.c src/cpp.cpp -o web/seeminacalc.js
	emcc $(EMCCFlags) $(EMCCFlagsBinary) -DNO_SSE $(Includes) web/sqlite3.o src/seeminacalc.c src/cpp.cpp -o web/seeminacalc.nosse.js

web/sqlite3.o: lib/sqlite3.c
	emcc $(EMCCFlags) $(EMCCFlagsSqlite) -c lib/sqlite3.c -o web/sqlite3.o

ssefeaturetest:
	emcc -Os --no-entry -msse -msimd128 src/ssefeaturetest.c -o ssefeaturetest.wasm
