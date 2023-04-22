CC = gcc
CFLAGS = -g -Wall -std=c99 $(DBG)

step0_repl: step0_repl.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step1_read_print: step1_read_print.c printer.c reader.c types.c utils.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@
