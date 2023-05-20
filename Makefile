CC = cc
LD = cc
IB = ib

CFLAGS = -g

LIBS = -lplist-2.0 -lsqlite3

all:
	$(IB) html.h.ib
	$(IB) irc.h.ib
	$(IB) html.c.ib
	$(IB) irc.c.ib
	$(IB) msg.h.ib
	$(IB) seqdump.c.ib
	$(CC) html.c -c -o html.o $(CFLAGS) 
	$(CC) seqdump.c -c -o seqdump.o $(CFLAGS) 
	$(LD) seqdump.o html.o -o seqdump $(LIBS)
