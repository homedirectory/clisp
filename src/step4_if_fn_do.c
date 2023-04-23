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

MalDatum *eval(const MalDatum *datum, MalEnv *env);

static MalDatum *read(const char* in) {
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

/* 'if' expression comes in 2 forms:
 * 1. (if cond if-true if-false)
 * return eval(cond) ? eval(if-true) : eval(if-false)
 * 2. (if cond if-true)
 * return eval(cond) ? eval(if-true) : nil
 */
static MalDatum *eval_if(const List *list, MalEnv *env) {
    // 1. validate the list
    int argc = List_len(list) - 1;
    if (argc < 2) {
        ERROR(eval_if, "if expects at least 2 arguments, but %d were given", argc);
        return NULL;
    }
    if (argc > 3) {
        ERROR(eval_if, "if expects at most 3 arguments, but %d were given", argc);
        return NULL;
    }

    MalDatum *ev_cond = eval(List_ref(list, 1), env); // own
    if (ev_cond == NULL)
        return NULL;

    // eval(cond) is true if it's neither 'nil' nor 'false'
    if (!MalDatum_isnil(ev_cond) && !MalDatum_isfalse(ev_cond)) {
        // eval(if-true)
        MalDatum *ev_iftrue = eval(List_ref(list, 2), env);
        MalDatum_free(ev_cond);
        return ev_iftrue;
    } else {
        if (argc == 3) {
            // eval(if-false)
            MalDatum *ev_iffalse = eval(List_ref(list, 3), env);
            MalDatum_free(ev_cond);
            return ev_iffalse;
        } else {
            // nil
            MalDatum_free(ev_cond);
            return MalDatum_nil();
        }
    }
}

/* 'do' expression evalutes each succeeding expression returning the result of the last one 
 * (do expr ...)
 * return expr.map(eval).last
 */
static MalDatum *eval_do(const List *list, MalEnv *env) {
    FATAL(eval_do, "Not implemented");
}

/* 'fn*' expression is like the 'lambda' expression, it creates and returns a function
 * it comes in 2 forms:
 * 1. (fn* params body)
 * params := () | (param ...)
 * 2. (fn* var-params body)
 * var-params := (& rest) | (param ... & rest)
 * rest is then bound to the list of the remaining arguments
 */
static MalDatum *eval_fnstar(const List *list, MalEnv *env) {
    FATAL(eval_fnstar, "Not implemented");
}

// (def! id <MalDatum>)
static MalDatum *eval_def(const List *list, MalEnv *env) {
    if (list->len != 3) {
        ERROR(eval_def, "def! expects 2 arguments, but %ld were given", list->len - 1);
        return NULL;
    }

    MalDatum *snd = List_ref(list, 1);
    if (snd->type != SYMBOL) {
        ERROR(eval_def, "def! expects a symbol as a 2nd argument, but %s was given",
                MalType_tostr(snd->type));
        return NULL;
    }
    Symbol *id = snd->value.sym;

    MalDatum *new_assoc = eval(List_ref(list, 2), env);
    if (new_assoc == NULL) {
        return NULL;
    }

    MalEnv_put(env, Symbol_copy(id), MalDatum_deep_copy(new_assoc));

    return new_assoc;
}

/* (let* (bindings) expr) 
 * bindings := (id val ...) // must have an even number of elements 
 */
static MalDatum *eval_letstar(const List *list, MalEnv *env) {
    // 1. validate the list
    if (list->len != 3) {
        ERROR(eval_letstar, "let* expects 2 arguments, but %ld were given", list->len - 1);
        return NULL;
    }

    MalDatum *snd = List_ref(list, 1);
    if (snd->type != LIST) {
        ERROR(eval_letstar, "let* expects a list as a 2nd argument, but %s was given",
                MalType_tostr(snd->type));
        return NULL;
    }

    List *bindings = snd->value.list;
    if (List_isempty(bindings)) {
        ERROR(eval_letstar, "let* expects a non-empty list of bindings");
        return NULL;
    }
    if (List_len(bindings) % 2 != 0) {
        ERROR(eval_letstar, "let*: illegal bindings (expected an even-length list)");
        return NULL;
    }

    MalDatum *expr = List_ref(list, 2);

    // 2. initialise the let* environment 
    MalEnv *let_env = MalEnv_new(env); // own
    // step = 2
    for (struct Node *id_node = bindings->head; id_node != NULL; id_node = id_node->next->next) {
        // make sure that a symbol is being bound
        if (!MalDatum_istype(id_node->value, SYMBOL)) {
            char *s = MalType_tostr(id_node->value->type); 
            ERROR(eval_letstar, 
                    "let*: illegal bindings (expected a symbol to be bound, but %s was given)",
                    s);
            free(s);
            MalEnv_free(let_env);
            return NULL;
        }
        Symbol *id = id_node->value->value.sym; // borrowed

        // it's important to evaluate the bound value using the let* env,
        // so that previous bindings can be used during evaluation
        MalDatum *val = eval(id_node->next->value, let_env); // own
        if (val == NULL) {
            MalEnv_free(let_env);
            return NULL;
        }

        MalEnv_put(let_env, Symbol_copy(id), val);
    }

    // 3. evaluate the expr using the let* env
    MalDatum *out = eval(expr, let_env); // own

    // discard the let* env
    MalEnv_free(let_env);

    return out;
}

// returns a new list that is the result of calling EVAL on each list element
static List *eval_list(const List *list, MalEnv *env) {
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
static MalDatum *eval_ast(const MalDatum *datum, MalEnv *env) {
    MalDatum *out = NULL;

    switch (datum->type) {
        case SYMBOL:
            Symbol *sym = datum->value.sym;
            MalDatum *assoc = MalEnv_get(env, sym);
            if (assoc == NULL) {
                ERROR(eval_ast, "symbol binding '%s' not found", sym->name);
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

MalDatum *eval(const MalDatum *datum, MalEnv *env) {
    if (datum == NULL) return NULL;

    switch (datum->type) {
        case LIST:
            List *list = datum->value.list;
            if (List_isempty(list)) {
                // TODO use a single global instance of an empty list
                return MalDatum_copy(datum);;
            } 
            else {
                // handle special forms: def!, let*, if, do, fn*
                MalDatum *head = List_ref(list, 0);
                switch (head->type) {
                    case SYMBOL:
                        Symbol *sym = head->value.sym;
                        if (Symbol_eq_str(sym, "def!"))
                            return eval_def(list, env);
                        else if (Symbol_eq_str(sym, "let*"))
                            return eval_letstar(list, env);
                        else if (Symbol_eq_str(sym, "if"))
                            return eval_if(list, env);
                        else if (Symbol_eq_str(sym, "do"))
                            return eval_do(list, env);
                        else if (Symbol_eq_str(sym, "fn*"))
                            return eval_fnstar(list, env);
                        break;
                    default:
                        break;
                }

                // it's a regular list form
                MalDatum *evaled = eval_ast(datum, env);
                if (evaled == NULL) {
                    LOG_NULL(evaled, eval);
                    return NULL;
                }

                List *elist = evaled->value.list;
                // NOTE: currently we only support arity of 2 
                if (elist->len != 3) {
                    ERROR(eval, "only procedures of arity 2 are supported");
                    MalDatum_free(evaled);
                    return NULL;
                }
                // Take the first item of the evaluated list and call it as
                // a function using the rest of the evaluated list as its arguments.
                intproc2_t proc = List_ref(elist, 0)->value.intproc2;
                int arg1 = List_ref(elist, 1)->value.i;
                int arg2 = List_ref(elist, 2)->value.i;
                int rslt = proc(arg1, arg2);
                MalDatum_free(evaled);
                return MalDatum_new_int(rslt);
            }
            break;
        default:
            return eval_ast(datum, env);
    }
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
    MalEnv *env = MalEnv_new(NULL);

    MalEnv_put(env, Symbol_new("nil"), MalDatum_nil());
    MalEnv_put(env, Symbol_new("true"), MalDatum_true());
    MalEnv_put(env, Symbol_new("false"), MalDatum_false());

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
        // TODO implement an stack trace of error messages
        MalDatum *e = eval(r, env);
        MalDatum_free(r);
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
