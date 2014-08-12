#CC=mipsel-linux-g++
CC=g++
#SYSROOT=$(shell $(CC) --print-sysroot)
CFLAGS=-Wall -lasound -lSDL -lSDL_image `$(SYSROOT)/usr/bin/sdl-config --cflags --libs`


all: clean voice

voice:
	$(CC) -g -o mic screen.cpp $(CFLAGS)

clean:
	rm -rf mic
