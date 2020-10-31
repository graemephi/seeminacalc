Clang := 0
Includes := -Ietterna -Ilib

ifneq ($(OS),Windows_NT)
$(error haven't done this. look at emcc flags that dont begin with -s)
endif

ifeq ($(Clang), 1)
Compiler := clang-cl
LTO := -flto -fuse-ld=lld
ExtraWarnings := -Wno-unused-command-line-argument -Wdouble-promotion -Wimplicit-int-float-conversion -Wstrict-prototypes
else
Compiler := cl
LTO := -GL
ExtraWarnings :=
endif

Debug := -DDEBUG -Fd"build/debug/" -Fo"build/debug/" -Fe"build/debug/"
Release := -DRELEASE -O2 -Ob2 -MT -Fd"build/release/" -Fo"build/release/" -Fe"build/release/"
Common := -Oi -nologo -EHsc -W4 -WX $(Includes) $(ExtraWarnings)
C := -Od -Ob1 -MT -Zi
CPP := -std:c++17 -O2 -Ob2 -MT -Zi

all: build build/cachedb/cachedb.exe build/debug/seeminacalc.exe

clean:
	rm -r build

build:
	@-mkdir -p build/debug
	@-mkdir -p build/release

cog:
	cog -rc graphs.c

build/debug/sqlite3.obj:
	$(Compiler) $(Common) $(Release) -c lib/sqlite3.c

build/release/sqlite3.obj:
	$(Compiler) $(Common) $(Release) $(LTO) -c lib/sqlite3.c

build/debug/cpp.obj: *.h *.cpp Makefile
	$(Compiler) $(Common) $(CPP) $(Debug) -c cpp.cpp

build/debug/seeminacalc.exe: *.h *.c build/debug/cpp.obj build/debug/sqlite3.obj Makefile
	$(Compiler) $(Common) $(C) $(Debug) seeminacalc.c build/debug/sqlite3.obj build/debug/cpp.obj

build/cachedb/cachedb.exe: cachedb.c build/debug/cpp.obj build/debug/sqlite3.obj Makefile
	@-mkdir -p build/cachedb
	$(Compiler) $(Common) $(C) -DDEBUG -Fd"build/cachedb/" -Fo"build/cachedb/" -Fe"build/cachedb/" cachedb.c build/debug/sqlite3.obj build/debug/cpp.obj

build/release/seeminacalc.exe: *.h *.c Makefile build/release/sqlite3.obj
	$(Compiler) $(Common) $(CPP) $(Release) seeminacalc.c cpp.cpp  build/release/sqlite3.obj

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
EMCCFlags += -s "EXPORTED_FUNCTIONS=['_main', '_set_font', '_open_file', '_calloc']"
EMCCFlags += -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall']"
EMCCFlags += -s USE_PTHREADS=1

emscripten:
	emcc $(EMCCFlags) -msse -msimd128 $(Includes) seeminacalc.c cpp.cpp -o web/seeminacalc.js
	emcc $(EMCCFlags) -DNO_SSE $(Includes) seeminacalc.c cpp.cpp -o web/seeminacalc.nosse.js

ssefeaturetest:
	emcc -Os --no-entry -msse -msimd128 ssefeaturetest.c -o ssefeaturetest.wasm

debug: build/debug/seeminacalc.exe
release: build/release/seeminacalc.exe
cachedb: build/cachedb/cachedb.exe
.PHONY: all clean debug release cachedb
.SUFFIXES:
