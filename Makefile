Debug := 1

ifdef Release
Debug := 0
Force := *
endif

Clang := 0
Includes := -I$(CURDIR)\ettsrc

ifeq ($(OS),Windows_NT)
	NOpt := -Od -Ob1 -MTd -Zi
	YOpt := -O2 -Ob2 -MT

	ifeq ($(Clang), 1)
		Compiler := clang-cl
		YOpt += -flto -fuse-ld=lld
		Includes += -I.
	else
		Compiler := cl
		YOpt += -GL
	endif

	ifeq ($(Debug), 1)
		OptimisationLevel := $(NOpt)
	else
		OptimisationLevel := $(YOpt)
	endif

	CompilerOptions := $(OptimisationLevel) $(Includes) -Fdbuild/ -Fobuild/ -Febuild/ -Oi -nologo -EHsc -W4
	COptions := -WX
	CPPOptions := -std:c++17
else
	todo
endif

all: build build/main.exe

clean:
	rm -r build

build:
	@mkdir build || true

build/cpp.obj: *.h *.cpp Makefile
	$(Compiler) $(CompilerOptions) $(CPPOptions) -c cpp.cpp

build/main.exe: *.h *.c build/cpp.obj Makefile
	$(Compiler) $(CompilerOptions) $(COptions) main.c build/cpp.obj

.PHONY: all clean $(Force)
