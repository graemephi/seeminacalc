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
else
	todo
endif

all: build/main.exe

build:
	@mkdir build || true

build/cminacalc.obj: sm.h cminacalc.h *.cpp build
	cl $(CompilerOptions) -c cminacalc.cpp

build/main.exe: *.c *.h build/cminacalc.obj
	cl $(CompilerOptions) main.c build/cminacalc.obj
