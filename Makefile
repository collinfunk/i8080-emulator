
CC ?= gcc
AR ?= ar

CFLAGS = -std=c11 -O3 -march=native -I./
LDFLAGS = -L./

.SUFFIXES: .c .o
.PHONY: all clean

all: libi8080.a i8080-emulator spaceinvaders

spaceinvaders: libi8080.a spaceinvaders.o
	$(CC) $(LDFLAGS) -o spaceinvaders spaceinvaders.o -li8080 -lSDL2

i8080-emulator: libi8080.a i8080-emulator.o
	$(CC) $(LDFLAGS) -o i8080-emulator i8080-emulator.o -li8080

libi8080.a: i8080.o i8080.h
	$(AR) rcs libi8080.a i8080.o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.a i8080-emulator spaceinvaders

