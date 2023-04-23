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
#include "common.h"

#define PROMPT "user> "

MalDatum *eval(MalDatum *datum, MalEnv *env);

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

// returns a new list that is the result of calling EVAL on each list element
static List *eval_list(List *list, MalEnv *env) {
    if (list == NULL) {
        LOG_NULL(list, eval_list);
        return NULL;
    }

    if (List_isempty(list)) {
        return List_new(); // TODO use singleton empty list 
    }

    List *out = List_new();
    struct Node *node = list->head;
    while (node) {
        MalDatum *evaled = eval(node->value, env);
        if (evaled == NULL) {
            LOG_NULL(evaled, eval_list);
            List_free(out);
            return NULL;
        }
        List_add(out, evaled);
        node = node->next;
    }

    return out;
}

/* Evaluates a MalDatum in an environment and returns the result in the form of a 
 * new dynamically allocted MalDatum.
 */
static MalDatum *eval_ast(MalDatum *datum, MalEnv *env) {
    MalDatum *out = NULL;

    switch (datum->type) {
        case SYMBOL:
            Symbol *sym = datum->value.sym;
            MalDatum *assoc = MalEnv_get(env, sym);
            if (assoc == NULL) {
                // given symbol is not associated with any datum
                ERROR(eval_ast, "undefined symbol '%s'", sym->name);
            } else {
                out = MalDatum_deep_copy(assoc);
            }
            break;
        case LIST:
            List *elist = eval_list(datum->value.list, env);
            if (elist == NULL) {
                LOG_NULL(elist, eval_ast);
            } else {
                out = MalDatum_new_list(elist);
            }
            break;
        default:
            out = MalDatum_copy(datum);
            break;
    }

    return out;
}

MalDatum *eval(MalDatum *datum, MalEnv *env) {
    if (datum == NULL) return NULL;

    MalDatum *out = NULL;

    switch (datum->type) {
        case LIST:
            //List *elist = eval_ast(datum->value.list, env);
            MalDatum *evaled = eval_ast(datum, env);
            if (evaled == NULL) {
                LOG_NULL(evaled, eval);
                MalDatum_free(evaled);
                return NULL;
            }
            List *elist = evaled->value.list;
            // empty list? return as is
            if (List_isempty(elist)) {
                // TODO use a single global instance of an empty list
                //out = MalDatum_new_list(List_new());
                out = evaled;
            } else {
                // NOTE: currently we only support arity of 2 
                if (elist->len != 3) {
                    ERROR(eval, "only procedures of arity 2 are supported");
                    //List_free(elist);
                    MalDatum_free(evaled);
                    return NULL;
                }
                // Take the first item of the evaluated list and call it as
                // a function using the rest of the evaluated list as its arguments.
                intproc2_t proc = List_ref(elist, 0)->value.intproc2;
                int arg1 = List_ref(elist, 1)->value.i;
                int arg2 = List_ref(elist, 2)->value.i;
                int rslt = proc(arg1, arg2);
                out = MalDatum_new_int(rslt);
                MalDatum_free(evaled);
            }
            break;
        default:
            out = eval_ast(datum, env);
            break;
    }

    return out;
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
