CC = cc
LD = cc
IB = ib

CFLAGS = -g
LDFLAGS = 

LIBS = -lplist-2.0 -lsqlite3
INC = -I build

all:
	mkdir -p build
	$(IB) html.h.ib -o build/html.h
	$(IB) irc.h.ib -o build/irc.h
	$(IB) html.c.ib -o build/html.c
	$(IB) irc.c.ib -o build/irc.c
	$(IB) msg.h.ib -o build/msg.h
	$(IB) seqdump.c.ib -o build/seqdump.c
	$(CC) html.c -c -o build/html.o $(CFLAGS) $(INC)
	$(CC) irc.c -c -o build/irc.o $(CFLAGS) $(INC)
	$(CC) seqdump.c -c -o build/seqdump.o $(CFLAGS) $(INC)
	$(LD) seqdump.o build/html.o build/irc.o -o seqdump $(LDFLAGS) $(LIBS)
