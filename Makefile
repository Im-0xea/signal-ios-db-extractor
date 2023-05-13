all:
	ib seqdump.c.ib
	cc seqdump.c -o seqdump -lsqlite3 -g
