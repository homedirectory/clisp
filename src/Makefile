CC = gcc
# use _CFLAGS="-D TRACE" to enable debug messages
CFLAGS = -ggdb -Wall -std=c99 $(_CFLAGS)

mylisp: mylisp.c printer.c reader.c types.c utils.c env.c core.c mem_debug.c hashtbl.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@
