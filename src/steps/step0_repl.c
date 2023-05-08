#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <string.h>

/*
   Add the 4 trivial functions READ, EVAL, PRINT, and rep (READ-EVAL-PRINT).
   READ, EVAL, and PRINT are basically just stubs that return their first
   parameter (a string if your target language is a statically typed) and rep
   calls them in order passing the return to the input of the next.

   Add a main loop that repeatedly prints a prompt (needs to be "user> " for
   later tests to pass), gets a line of input from the user, calls rep with
   that line of input, and then prints out the result from rep. It should also
   exit when you send it an EOF (often Ctrl-D).
 */

#define PROMPT "user> "

static char *READ(char* in) {
    char *out = malloc(strlen(in) + 1);
    strcpy(out, in);
    return out;
}

static char *EVAL(char* in) {
    char *out = malloc(strlen(in) + 1);
    strcpy(out, in);
    return out;
}

static char *PRINT(char* in) {
    char *out = malloc(strlen(in) + 1);
    strcpy(out, in);
    return out;
}

int main(int argc, char **argv) {
    while (1) {
        char *line = readline(PROMPT);
        if (line == NULL) {
            exit(EXIT_SUCCESS);
        }

        char *r = READ(line);
        free(line);
        char *e = EVAL(r);
        free(r);
        char *p = PRINT(e);
        free(e);
        if (p[0] != '\0')
            printf("%s\n", p);
        free(p);
    }
}
