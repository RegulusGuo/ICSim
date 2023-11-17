CC=gcc
CFLAGS=-I/usr/include/SDL2
LDFLAGS=-lSDL2 -lSDL2_image

all: icsim controls gateway

icsim: icsim.o lib.o
	$(CC) $(CFLAGS) -o icsim icsim.c lib.o $(LDFLAGS)

controls: controls.o
	$(CC) $(CFLAGS) -o controls controls.c $(LDFLAGS)

gateway: gateway.o
	$(CC) $(CFLAGS) -o gateway gateway.c $(LDFLAGS)

lib.o:
	$(CC) lib.c

clean:
	rm -rf icsim controls gateway icsim.o controls.o gateway.o
