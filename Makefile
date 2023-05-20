CC = cc
LD = cc
IB = ib

CFLAGS = -g
LDFLAGS = 

LIBS = -lplist-2.0 -lsqlite3
INC = -I build

all:
	mkdir -p build
	$(IB) src/html.h.ib -o build/html.h
	$(IB) src/irc.h.ib -o build/irc.h
	$(IB) src/html.c.ib -o build/html.c
	$(IB) src/irc.c.ib -o build/irc.c
	$(IB) src/msg.h.ib -o build/msg.h
	$(IB) src/seqdump.c.ib -o build/seqdump.c
	$(CC) build/html.c -c -o build/html.o $(CFLAGS) $(INC)
	$(CC) build/irc.c -c -o build/irc.o $(CFLAGS) $(INC)
	$(CC) build/seqdump.c -c -o build/seqdump.o $(CFLAGS) $(INC)
	$(LD) build/seqdump.o build/html.o build/irc.o -o seqdump $(LDFLAGS) $(LIBS)
