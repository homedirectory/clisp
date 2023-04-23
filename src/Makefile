CC = gcc
# use _CFLAGS="-D TRACE" to enable debug messages
CFLAGS = -g -Wall -std=c99 $(_CFLAGS)

step0_repl: step0_repl.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step1_read_print: step1_read_print.c printer.c reader.c types.c utils.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step2_eval: step2_eval.c printer.c reader.c types.c utils.c env.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step3_env: step3_env.c printer.c reader.c types.c utils.c env.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@
