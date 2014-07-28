CC=gcc
CFLAGS=-Wall -lasound

all: clean aplay arecord

aplay:
	$(CC) -o aplay aplay.c $(CFLAGS)

arecord:
	$(CC) -o arecord aplay.c $(CFLAGS)

clean:
	rm -rf aplay arecord alsawrapper