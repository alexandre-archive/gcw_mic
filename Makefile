#CC=mipsel-linux-gcc
#CCPP=mipsel-linux-g++
CC=gcc
CCPP=g++

SYSROOT=$(shell $(CC) --print-sysroot)

CFLAGS=-Wall -Wextra -Wundef -Wunused-macros -Wendif-labels

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

CLIBS=-lasound -lpthread
CPPLIBS=-lSDL -lSDL_image `$(SYSROOT)/usr/bin/sdl-config --cflags --libs`

all: clean voice

voice:
	$(CC) -c -o aplay.o alsawrapper.c $(CFLAGS) $(UFLAGS) $(CLIBS)
	$(CCPP) -c -o mic.o screen.cpp $(CFLAGS) $(CPPLIBS)
	$(CCPP) -g -o voice  mic.o aplay.o $(CPPLIBS) $(CLIBS)

opk: voice
	mkdir temp
	cp voice voice.png default.gcw0.desktop temp
	mksquashfs temp voice.opk -all-root -noappend -no-exports -no-xattrs
	rm -rf temp

clean:
	rm -rf voice *.o temp *.opk
