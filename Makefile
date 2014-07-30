CC=gcc
CFLAGS=-Wall
LIBS=-lasound -lpthread

all: clean wrapper

wrapper:
	$(CC) -o mic mic.c $(CFLAGS) $(LIBS)

clean:
	rm -rf mic