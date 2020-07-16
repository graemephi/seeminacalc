Debug := 1
Includes := -I$(CURDIR)\ettsrc

ifeq ($(OS),Windows_NT)
	NOpt := -Od -Ob1 -MTd -Zi
	YOpt := -O2 -GL -Ob2 -MT

	ifeq ($(Debug), 1)
		OptimisationLevel := $(NOpt)
	else
		OptimisationLevel := $(YOpt)
	endif

	CompilerOptions := $(OptimisationLevel) $(Includes) -Fdbuild/ -Fobuild/ -Febuild/ -Oi -nologo -std:c++latest -fp:fast -EHsc
	OurCodeOptions := -W4 -WX
	ExternalCodeOptions := -w
else
	todo
endif

all: build build/main.exe

clean:
	rm -r build

build:
	@mkdir build || true

build/cpp.obj: *.h *.cpp Makefile
	cl $(CompilerOptions) $(ExternalCodeOptions) -c cpp.cpp

build/main.exe: *.h *.c build/cpp.obj Makefile
	cl $(CompilerOptions) $(OurCodeOptions) main.c build/cpp.obj

.PHONY: all clean
