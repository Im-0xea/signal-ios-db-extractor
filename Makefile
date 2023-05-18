CC = cc
IB = ib

CFLAGS = -g

LIBS = -lplist-2.0 -lsqlite3

all:
	$(IB) html.h.ib
	$(IB) seqdump.c.ib
	$(CC) seqdump.c -o seqdump $(CFLAGS) $(LIBS)
