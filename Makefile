CC=clang
CFLAGS=-O0 -g -Wall -Wextra -pedantic -std=c90 -Wno-variadic-macros
CFLAGS+=-I/usr/local/include

LD=clang
LFLAGS=
LFLAGS+=-L/usr/local/lib
LIBS=-lX11 -lXext

OBJS=xosc.o
xosc: $(OBJS)
	$(LD) $(LFLAGS) -o xosc $(OBJS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o xosc
