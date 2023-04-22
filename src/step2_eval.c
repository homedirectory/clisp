#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <string.h>
#include <stddef.h>
#include "reader.h"
#include "types.h"
#include "printer.h"
#include "env.h"

#define PROMPT "user> "


static MalDatum *read(char* in) {
    Reader *rdr = read_str(in);
    if (rdr == NULL) return NULL;
    if (rdr->tokens->len == 0) {
        Reader_free(rdr);
        return NULL;
    }
    MalDatum *form = read_form(rdr);
    Reader_free(rdr);
    return form;
}

static MalDatum *eval(MalDatum *datum, MalEnv *env) {
    return datum;
}

static char *print(MalDatum *datum) {
    if (datum == NULL) {
        return NULL;
    }

    char *str = pr_str(datum);
    return str;
}

static int mal_add(int x, int y) {
    return x + y;
}

static int mal_sub(int x, int y) {
    return x - y;
}

static int mal_mul(int x, int y) {
    return x * y;
}

static int mal_div(int x, int y) {
    return x / y;
}

int main(int argc, char **argv) {
    MalEnv *env = MalEnv_new();
    MalEnv_put(env, Symbol_new("+"), MalDatum_new_intproc2(mal_add));
    MalEnv_put(env, Symbol_new("-"), MalDatum_new_intproc2(mal_sub));
    MalEnv_put(env, Symbol_new("*"), MalDatum_new_intproc2(mal_mul));
    MalEnv_put(env, Symbol_new("/"), MalDatum_new_intproc2(mal_div));

    while (1) {
        //char *sp = malloc(50);
        //strcpy(sp, " 123 ");
        //char *line = sp;
        char *line = readline(PROMPT);
        if (line == NULL) {
            exit(EXIT_SUCCESS);
        }

        // read
        MalDatum *r = read(line);
        free(line);
        if (r == NULL) continue;
        // eval
        MalDatum *e = eval(r, env);
        //MalDatum_free(r);
        // print
        char *p = print(e);
        MalDatum_free(e);
        if (p != NULL) { 
            if (p[0] != '\0')
                printf("%s\n", p);
            free(p);
        }
    }

    MalEnv_free(env);
}
