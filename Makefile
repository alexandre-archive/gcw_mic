CC=mipsel-linux-g++
#CC=g++

SYSROOT=$(shell $(CC) --print-sysroot)

CFLAGS=-Wall
EFLAGS=-Wextra -Wundef -Wunused-macros -Wendif-labels -Wshadow -Wunreachable-code
UFLAGS=-pedantic -Wcast-qual -Wundef -ggdb3 -O0 \
		-Wno-missing-braces -Wno-missing-field-initializers -Wformat=2 \
		-Wswitch-default -Wswitch-enum -Wcast-align -Wpointer-arith \
		-Wstrict-overflow=5 -Winline -lm -fstrict-aliasing \
		-Wlogical-op -Wfloat-equal -Wstrict-aliasing=2 -Wredundant-decls \
		-fno-omit-frame-pointer -ffloat-store -fno-common

LDFLAGS=-lSDL -lSDL_ttf -lSDL_image -lasound -lpthread `$(SYSROOT)/usr/bin/sdl-config --cflags --libs`
FILES=src/screen.cpp src/mic.cpp src/mixer.cpp src/config.cpp src/aplay.c

all: clean build

build: voice

voice: $(FILES)
	$(CC) -std=c++11 -g -o $@ $^ $(CFLAGS) $(LDFLAGS)

aplay:
	$(CC) -std=c++11 -g -c -o aplay.o src/aplay.c -lasound -lpthread

opk: all
	mkdir temp temp/resources
	cp voice resources/voice.png resources/default.gcw0.desktop temp
	cp -r resources/32 resources/font temp/resources
	mksquashfs temp voice.opk -all-root -noappend -no-exports -no-xattrs
	rm -rf temp

deploy: opk
	scp voice.opk root@10.1.1.2:/media/data/apps

clean:
	rm -rf voice *.o temp *.opk rec_*
