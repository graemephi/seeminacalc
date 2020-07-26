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

Debug := -Fd"build/debug/" -Fo"build/debug/" -Fe"build/debug/"
Release := -O2 -Ob2 -MT $(LTO) -Fd"build/release/" -Fo"build/release/" -Fe"build/release/"
Common := -Oi -nologo -EHsc -W4 -WX $(Includes) $(ExtraWarnings)
C := -Od -Ob1 -MT -Zi
CPP := -std:c++17 -O2 -Ob2 -MT

all: build build/debug/main.exe

clean:
	rm -r build

build:
	@-mkdir -p build/debug
	@-mkdir -p build/release

cog:
	cog -rc graphs.c

build/debug/cpp.obj: *.h *.cpp Makefile
	$(Compiler) $(Common) $(CPP) $(Debug) -c cpp.cpp

build/debug/main.exe: *.h *.c build/debug/cpp.obj Makefile
	$(Compiler) $(Common) $(C) $(Debug) main.c build/debug/cpp.obj

build/release/main.exe: *.h *.c build/release/cpp.obj Makefile
	$(Compiler) $(Common) $(CPP) $(Release) main.c cpp.cpp

EMCCFlags :=
EMCCFlags += -s DISABLE_EXCEPTION_CATCHING=1
EMCCFlags += -s ERROR_ON_UNDEFINED_SYMBOLS=1
# emcc warns that this is slow with threads; haven't investigated
# EMCCFlags += -s ALLOW_MEMORY_GROWTH=1
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
EMCCFlags += -O0
EMCCFlags += -s "EXPORTED_FUNCTIONS=['_main', '_set_font', '_open_file', '_calloc']"
EMCCFlags += -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall']"
EMCCFlags += -s USE_PTHREADS=1
EMCCFlags += -s PTHREAD_POOL_SIZE=4
EMCCFlags += -s INITIAL_MEMORY 268435456

emscripten:
	emcc $(EMCCFlags) -msse -msimd128 $(Includes) main.c cpp.cpp -o web/main.js

debug: build/debug/main.exe
release: build/release/main.exe
.PHONY: all clean debug release
