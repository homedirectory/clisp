CC = gcc
# use _CFLAGS="-D TRACE" to enable debug messages
CFLAGS = -ggdb -Wall -std=c99 $(_CFLAGS)

step0_repl: step0_repl.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step1_read_print: step1_read_print.c printer.c reader.c types.c utils.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step2_eval: step2_eval.c printer.c reader.c types.c utils.c env.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step3_env: step3_env.c printer.c reader.c types.c utils.c env.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step4_if_fn_do: step4_if_fn_do.c printer.c reader.c types.c utils.c env.c core.c mem_debug.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step5_tco: step5_tco.c printer.c reader.c types.c utils.c env.c core.c mem_debug.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step6_file: step6_file.c printer.c reader.c types.c utils.c env.c core.c mem_debug.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step7_quote: step7_quote.c printer.c reader.c types.c utils.c env.c core.c mem_debug.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step8_macros: step8_macros.c printer.c reader.c types.c utils.c env.c core.c mem_debug.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

step9_try: step9_try.c printer.c reader.c types.c utils.c env.c core.c mem_debug.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@

mylisp: mylisp.c printer.c reader.c types.c utils.c env.c core.c mem_debug.c hashtbl.c
	$(CC) $(CFLAGS) $^ -lreadline -o $@
