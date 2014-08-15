#CC=mipsel-linux-gcc
#CCPP=mipsel-linux-g++
CC=gcc
CCPP=g++
SYSROOT=$(shell $(CC) --print-sysroot)
CFLAGS=-Wall
CLIBS= -lasound -lpthread
CPPLIBS=-lSDL -lSDL_image `$(SYSROOT)/usr/bin/sdl-config --cflags --libs`

all: clean voice

voice:
	$(CC) -c -o aplay.o alsawrapper.c $(CFLAGS) $(CLIBS) -DC
	$(CCPP) -c -o mic.o screen.cpp $(CFLAGS) $(CPPLIBS)
	$(CCPP) -g -o  mic  mic.o aplay.o $(CPPLIBS) $(CLIBS)

clean:
	rm -rf mic aplay.o mic.o
