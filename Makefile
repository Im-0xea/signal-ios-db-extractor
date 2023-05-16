CC = cc
IB = ib

CFLAGS = -g

LIBS = -lplist -lsqlite3

all:
	$(IB) seqdump.c.ib
	$(CC) seqdump.c -o seqdump $(CFLAGS) $(LIBS)
