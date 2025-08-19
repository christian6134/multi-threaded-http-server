# Christian Garces — Makefile

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -O2 -g -pthread
LDFLAGS = -pthread

OBJS = httpserver.o req.o queue.o rwlock.o

all: httpserver

httpserver: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

httpserver.o: httpserver.c req.h
req.o: req.c req.h
queue.o: queue.c queue.h
rwlock.o: rwlock.c rwlock.h

clean:
	rm -f $(OBJS) httpserver

.PHONY: all clean

