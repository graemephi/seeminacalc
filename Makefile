target := debug

ifeq ($(target), debug)
Debug := 1
Force :=
endif

ifeq ($(target), release)
Debug := 0
Force := *
endif

Clang := 0
Includes := -Ietterna -Ilib

ifeq ($(OS),Windows_NT)
	ifeq ($(Clang), 1)
		Compiler := clang-cl
		LTO := -flto -fuse-ld=lld
	else
		Compiler := cl
		LTO := -GL
	endif

	ifeq ($(Debug), 1)
		OptimisationLevel := -Od -Ob1 -MTd -Zi
	else
		OptimisationLevel := -O2 -Ob2 -MT $(LTO)
	endif

	CompilerOptions := -Oi -nologo -EHsc -W4 -DSOKOL_NO_DEPRECATED $(OptimisationLevel) $(Includes) -Fdbuild/ -Fobuild/ -Febuild/
	COptions := -WX
	CPPOptions := -std:c++17
else
	$(error todo)
endif

all: build build/main.exe

clean:
	rm -r build

build:
	@-mkdir build

build/cpp.obj: *.h *.cpp Makefile
	$(Compiler) $(CompilerOptions) $(CPPOptions) -c cpp.cpp

build/main.exe: *.h *.c build/cpp.obj Makefile
	$(Compiler) $(CompilerOptions) $(COptions) main.c build/cpp.obj

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
EMCCFlags += -s "EXPORTED_FUNCTIONS=['_main', '_init', '_set_font', '_open_file']"
EMCCFlags += -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall']"

emscripten:
	emcc $(EMCCFlags) -msse -msimd128 $(Includes) main.c cpp.cpp -o web/main.js

.PHONY: all clean $(Force)
