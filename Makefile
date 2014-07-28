CC=gcc
CFLAGS=-Wall -lasound

all: clean wrapper

wrapper:
	$(CC) -o mic mic.c $(CFLAGS)

clean:
	rm -rf mic