CC=mipsel-linux-g++
#CC=g++

SYSROOT=$(shell $(CC) --print-sysroot)

CFLAGS=
#-Wall
EFLAGS=-Wextra -Wundef -Wunused-macros -Wendif-labels
UFLAGS=-std=c99 -pedantic -Wcast-qual \
		-Wstrict-prototypes -Wmissing-prototypes \
		-Wno-missing-braces -Wno-missing-field-initializers -Wformat=2 \
		-Wswitch-default -Wswitch-enum -Wcast-align -Wpointer-arith \
		-Wbad-function-cast -Wstrict-overflow=5 -Winline \
		-Wundef -Wnested-externs -Wshadow -Wunreachable-code \
		-Wlogical-op -Wfloat-equal -Wstrict-aliasing=2 -Wredundant-decls \
		-Wold-style-definition \
		-ggdb3 \
		-O0 \
		-fno-omit-frame-pointer -ffloat-store -fno-common -fstrict-aliasing \
		-lm

CXXLIBS=-lasound -lpthread -lSDL -lSDL_ttf -lSDL_image `$(SYSROOT)/usr/bin/sdl-config --cflags --libs`

all: clean voice

voice:
	$(CC) -std=c++11 -g -o voice screen.cpp alsawrapper.c mic.cpp mixer.cpp $(CXXLIBS)

opk: all
	mkdir temp
	cp voice voice.png default.gcw0.desktop temp
	cp -r resources temp
	mksquashfs temp voice.opk -all-root -noappend -no-exports -no-xattrs
	rm -rf temp

deploy: opk
	scp voice.opk root@10.1.1.2:/media/data/apps

clean:
	rm -rf voice *.o temp *.opk rec_*
