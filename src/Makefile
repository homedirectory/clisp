CC = gcc
# use _CFLAGS="-D TRACE" to enable debug messages
CFLAGS = -ggdb -Wall -std=c99 -O0 $(_CFLAGS)

mylisp: mylisp.c printer.c reader.c types_oop.c utils.c env.c core.c mem_debug.c hashtbl.c
	$(CC) $(CFLAGS) -o $@ -lreadline $^

types_oop: types_oop.c env.c utils.c hashtbl.c
	$(CC) $(CFLAGS) -o $@ $^

reader: reader.c types_oop.c utils.c env.c hashtbl.c
	$(CC) $(CFLAGS) -o $@ $^

printer: printer.c reader.c types_oop.c utils.c env.c hashtbl.c
	$(CC) $(CFLAGS) -o $@ $^

utils: utils.c
	$(CC) $(CFLAGS) -o $@ $^

core: core.c utils.c types_oop.c env.c printer.c hashtbl.c
	$(CC) $(CFLAGS) -o $@ $^
