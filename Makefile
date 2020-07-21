Clang := 0
Includes := -Ietterna -Ilib

ifneq ($(OS),Windows_NT)
	$(error haven't done this. look at emcc)
endif


ifdef clang
Compiler := clang-cl
LTO := -flto -fuse-ld=lld
else
Compiler := cl
LTO := -GL
endif

Debug := -Od -Ob1 -MTd -Zi -Fd"build/debug/" -Fo"build/debug/" -Fe"build/debug/"
Release := -O2 -Ob2 -MT $(LTO) -Fd"build/release/" -Fo"build/release/" -Fe"build/release/"
Common := -Oi -nologo -EHsc -W4 $(Includes)
C := -WX
CPP := -std:c++17

all: build build/debug/main.exe

clean:
	rm -r build

build:
	@-mkdir -p build/debug
	@-mkdir -p build/release

build/debug/cpp.obj: *.h *.cpp Makefile
	$(Compiler) $(Common) $(CPP) $(Debug) -c cpp.cpp

build/debug/main.exe: *.h *.c build/debug/cpp.obj Makefile
	$(Compiler) $(Common) $(C) $(Debug) main.c build/debug/cpp.obj

build/release/main.exe:
	$(Compiler) $(Common) $(CPP) $(Release) main.c cpp.cpp

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
EMCCFlags += -s ENVIRONMENT=web
EMCCFlags += -O3
EMCCFlags += -s "EXPORTED_FUNCTIONS=['_main', '_set_font', '_open_file', '_calloc']"
EMCCFlags += -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall']"

emscripten:
	emcc $(EMCCFlags) -msse -msimd128 $(Includes) main.c cpp.cpp -o web/main.js

debug: build/debug/main.exe
release: build/release/main.exe
.PHONY: all clean debug release
