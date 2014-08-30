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
FILES=screen.cpp mic.cpp mixer.cpp config.cpp

all: clean build

build: voice

voice: $(FILES)
	$(CC) -std=c++11 -g -o $@ $^ $(CFLAGS) $(LDFLAGS)

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
