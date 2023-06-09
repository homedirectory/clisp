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
#include "core.h"
#include "mem_debug.h"

#define PROMPT "user> "

MalDatum *eval(const MalDatum *datum, MalEnv *env);
MalDatum *eval_ast(const MalDatum *datum, MalEnv *env);
List *eval_list(const List *list, MalEnv *env);

static MalDatum *read(const char* in) {
    Reader *rdr = read_str(in);
    OWN(rdr);
    if (rdr == NULL) return NULL;
    if (rdr->tokens->len == 0) {
        FREE(rdr);
        Reader_free(rdr);
        return NULL;
    }
    MalDatum *form = read_form(rdr);
    FREE(rdr);
    Reader_free(rdr);
    return form;
}

// args - array of *MalDatum (argument values)
static MalDatum *apply_proc(const Proc *proc, const Arr *args, MalEnv *env) {
    if (proc->builtin) {
        return proc->logic.apply(proc, args);
    } 
    else {
        // local env is created even if a procedure expects no parameters,
        // so that def! inside it have only local effect
        // NOTE: this is where the need to track reachability stems from,
        // since we don't know whether the environment of this particular application
        // (with all the arguments) will be needed after its applied.
        // Example where it won't be needed and thus can be safely discarded:
        // ((fn* (x) x) 10) => 10
        // Here a local env { x = 10 } with enclosing one set to the global env will be created
        // and discarded immediately after the result (10) is obtained.
        // (((fn* (x) (fn* () x)) 10)) => 10
        // But here the result of this application will be a procedure that should
        // "remember" about x = 10, so the local env should be preserved. 
        MalEnv *proc_env = MalEnv_new(proc->env);
        OWN(proc_env);

        // 1. bind params to args in the local env
        // mandatory arguments
        for (int i = 0; i < proc->argc; i++) {
            Symbol *param = Arr_get(proc->params, i);
            MalDatum *arg = Arr_get(args, i);
            MalEnv_put(proc_env, param, arg); 
        }

        // if variadic, then bind the last param to the rest of arguments
        if (proc->variadic) {
            Symbol *var_param = Arr_get(proc->params, proc->params->len - 1);
            List *var_args = List_new();
            for (size_t i = proc->argc; i < args->len; i++) {
                MalDatum *arg = Arr_get(args, i);
                // TODO optimise: avoid copying arguments
                List_add(var_args, MalDatum_deep_copy(arg));
            }

            MalEnv_put(proc_env, var_param, MalDatum_new_list(var_args));
        }

        // 2. evaluate the body
        const Arr *body = proc->logic.body;
        // the body must not be empty at this point
        if (body->len == 0) FATAL("empty body");
        // evalute each expression and return the result of the last one
        for (int i = 0; i < body->len - 1; i++) {
            const MalDatum *dtm = body->items[i];
            MalDatum_free(eval(dtm, proc_env));
        }
        MalDatum *out = eval(body->items[body->len - 1], proc_env);

        FREE(proc_env);
        MalEnv_free(proc_env);

        return out;
    }
}

// list - raw unevaled list form
static MalDatum *eval_application(const List *list, MalEnv *env) {
    if (List_isempty(list)) {
        ERROR("procedure application: expected a non-empty list");
        return NULL;
    }

    List *ev_list = eval_list(list, env);
    OWN(ev_list);
    if (ev_list == NULL) return NULL;

    // this can be either a named procedure (bound to a symbol) or an fn*-produced one
    bool named = MalDatum_istype(List_ref(list, 0), SYMBOL);
    char *proc_name = named ? List_ref(list, 0)->value.sym->name : "*unnamed*";

    Proc *proc = List_ref(ev_list, 0)->value.proc;

    int argc = List_len(ev_list) - 1;
    if (argc < proc->argc) {
        ERROR("procedure application: %s expects at least %d arguments, but %d were given", 
                proc_name, proc->argc, argc);
        FREE(ev_list);
        List_free(ev_list);
        return NULL;
    }
    else if (!proc->variadic && argc > proc->argc) {
        ERROR("procedure application: %s expects %d arguments, but %d were given", 
                proc_name, proc->argc, argc);
        FREE(ev_list);
        List_free(ev_list);
        return NULL;
    }

    // array of *MalDatum
    Arr *args = Arr_newn(argc);
    OWN(args);
    for (struct Node *node = ev_list->head->next; node != NULL; node = node->next) {
        Arr_add(args, node->value);
    }

    MalDatum *out = apply_proc(proc, args, env);

    FREE(args);
    Arr_free(args);
    FREE(ev_list);
    List_free(ev_list);

    return out;
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
        ERROR("if expects at least 2 arguments, but %d were given", argc);
        return NULL;
    }
    if (argc > 3) {
        ERROR("if expects at most 3 arguments, but %d were given", argc);
        return NULL;
    }

    MalDatum *ev_cond = eval(List_ref(list, 1), env);
    OWN(ev_cond);
    if (ev_cond == NULL)
        return NULL;

    // eval(cond) is true if it's neither 'nil' nor 'false'
    if (!MalDatum_isnil(ev_cond) && !MalDatum_isfalse(ev_cond)) {
        // eval(if-true)
        MalDatum *ev_iftrue = eval(List_ref(list, 2), env);
        FREE(ev_cond);
        MalDatum_free(ev_cond);
        return ev_iftrue;
    } else {
        if (argc == 3) {
            // eval(if-false)
            MalDatum *ev_iffalse = eval(List_ref(list, 3), env);
            FREE(ev_cond);
            MalDatum_free(ev_cond);
            return ev_iffalse;
        } else {
            // nil
            FREE(ev_cond);
            MalDatum_free(ev_cond);
            return MalDatum_nil();
        }
    }
}

/* 'do' expression evalutes each succeeding expression returning the result of the last one.
 * (do expr ...)
 * return expr.map(eval).last
 */
static MalDatum *eval_do(const List *list, MalEnv *env) {
    int argc = List_len(list) - 1;
    if (argc == 0) {
        ERROR("do expects at least 1 argument");
        return NULL;
    }

    struct Node *node;
    for (node = list->head->next; node->next != NULL; node = node->next) {
        MalDatum *ev = eval(node->value, env);
        if (ev == NULL) return NULL;
        FREE(ev);
        MalDatum_free(ev);
    }

    return eval(node->value, env);
}

/* 'fn*' expression is like the 'lambda' expression, it creates and returns a function
 * it comes in 2 forms:
 * 1. (fn* params body)
 * params := () | (param ...)
 * param := SYMBOL
 * 2. (fn* var-params body)
 * var-params := (& rest) | (param ... & rest)
 * rest is then bound to the list of the remaining arguments
 */
static MalDatum *eval_fnstar(const List *list, MalEnv *env) {
    int argc = List_len(list) - 1;
    if (argc < 2) {
        ERROR("fn* expects at least 2 arguments, but %d were given", argc);
        return NULL;
    }

    // 1. validate params
    List *params;
    {
        MalDatum *snd = List_ref(list, 1);
        if (!MalDatum_islist(snd)) {
            ERROR("fn* expects a list as a 2nd argument, but %s was given",
                    MalType_tostr(snd->type));
            return NULL;
        }
        params = snd->value.list;
    }

    // all parameters should be symbols
    for (struct Node *node = params->head; node != NULL; node = node->next) {
        MalDatum *par = node->value;
        if (!MalDatum_istype(par, SYMBOL)) {
            ERROR("fn* bad parameter list: expected a list of symbols, but %s was found in the list",
                    MalType_tostr(par->type));
            return NULL;
        }
    }

    size_t proc_argc = 0; // mandatory arg count
    bool variadic = false;
    Arr *param_names_symbols = Arr_newn(List_len(params));
    OWN(param_names_symbols);

    for (struct Node *node = params->head; node != NULL; node = node->next) {
        Symbol *sym = node->value->value.sym;

        // '&' is a special symbol that marks a variadic procedure
        // exactly one parameter is expected after it
        // NOTE: we allow that parameter to also be named '&'
        if (Symbol_eq_str(sym, "&")) {
            if (node->next == NULL || node->next->next != NULL) {
                ERROR("fn* bad parameter list: 1 parameter expected after '&'");
                return NULL;
            }
            Symbol *last_sym = node->next->value->value.sym;
            Arr_add(param_names_symbols, last_sym); // no need to copy
            variadic = true;
            break;
        }
        else {
            proc_argc++;
            Arr_add(param_names_symbols, sym); // no need to copy
        }
    }

    // 2. construct the Procedure
    // body
    int body_len = argc - 1;
    // array of *MalDatum
    Arr *body = Arr_newn(body_len);
    OWN(body);
    for (struct Node *node = list->head->next->next; node != NULL; node = node->next) {
        Arr_add(body, node->value); // no need to copy
    }

    Proc *proc = Proc_new(proc_argc, variadic, param_names_symbols, body, env);
    env->reachable = true;

    FREE(param_names_symbols);
    Arr_free(param_names_symbols);
    FREE(body);
    Arr_free(body);

    return MalDatum_new_proc(proc);
}

// (def! id <MalDatum>)
static MalDatum *eval_def(const List *list, MalEnv *env) {
    int argc = List_len(list) - 1;
    if (argc != 2) {
        ERROR("def! expects 2 arguments, but %d were given", argc);
        return NULL;
    }

    MalDatum *snd = List_ref(list, 1);
    if (snd->type != SYMBOL) {
        ERROR("def! expects a symbol as a 2nd argument, but %s was given",
                MalType_tostr(snd->type));
        return NULL;
    }
    Symbol *id = snd->value.sym;

    MalDatum *new_assoc = eval(List_ref(list, 2), env);
    if (new_assoc == NULL) {
        return NULL;
    }

    MalEnv_put(env, id, new_assoc);

    return new_assoc;
}

/* (let* (bindings) expr) 
 * bindings := (id val ...) // must have an even number of elements 
 */
static MalDatum *eval_letstar(const List *list, MalEnv *env) {
    // 1. validate the list
    int argc = List_len(list) - 1;
    if (argc != 2) {
        ERROR("let* expects 2 arguments, but %d were given", argc);
        return NULL;
    }

    MalDatum *snd = List_ref(list, 1);
    if (snd->type != LIST) {
        ERROR("let* expects a list as a 2nd argument, but %s was given",
                MalType_tostr(snd->type));
        return NULL;
    }

    List *bindings = snd->value.list;
    if (List_isempty(bindings)) {
        ERROR("let* expects a non-empty list of bindings");
        return NULL;
    }
    if (List_len(bindings) % 2 != 0) {
        ERROR("let*: illegal bindings (expected an even-length list)");
        return NULL;
    }

    MalDatum *expr = List_ref(list, 2);

    // 2. initialise the let* environment 
    MalEnv *let_env = MalEnv_new(env);
    OWN(let_env);
    // step = 2
    for (struct Node *id_node = bindings->head; id_node != NULL; id_node = id_node->next->next) {
        // make sure that a symbol is being bound
        if (!MalDatum_istype(id_node->value, SYMBOL)) {
            char *s = MalType_tostr(id_node->value->type); 
            ERROR("let*: illegal bindings (expected a symbol to be bound, but %s was given)", s);
            free(s);
            FREE(let_env);
            MalEnv_free(let_env);
            return NULL;
        }
        Symbol *id = id_node->value->value.sym; // borrowed

        // it's important to evaluate the bound value using the let* env,
        // so that previous bindings can be used during evaluation
        MalDatum *val = eval(id_node->next->value, let_env);
        OWN(val);
        if (val == NULL) {
            FREE(let_env);
            MalEnv_free(let_env);
            return NULL;
        }

        MalEnv_put(let_env, id, val);
    }

    // 3. evaluate the expr using the let* env
    MalDatum *out = eval(expr, let_env);
    OWN(out);

    // discard the let* env
    FREE(let_env);
    MalEnv_free(let_env);

    return out;
}

// returns a new list that is the result of calling EVAL on each list element
List *eval_list(const List *list, MalEnv *env) {
    if (list == NULL) {
        LOG_NULL(list);
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
            LOG_NULL(evaled);
            FREE(out);
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
MalDatum *eval_ast(const MalDatum *datum, MalEnv *env) {
    MalDatum *out = NULL;

    switch (datum->type) {
        case SYMBOL:
            Symbol *sym = datum->value.sym;
            MalDatum *assoc = MalEnv_get(env, sym);
            if (assoc == NULL) {
                ERROR("symbol binding '%s' not found", sym->name);
            } else {
                out = MalDatum_deep_copy(assoc);
            }
            break;
        case LIST:
            List *elist = eval_list(datum->value.list, env);
            if (elist == NULL) {
                LOG_NULL(elist);
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

                // it's a regular list form (i.e., it's a procedure application)
                return eval_application(list, env);
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

    char *str = pr_str(datum, true);
    return str;
}

static void rep(const char *str, MalEnv *env) {
    // read
    MalDatum *r = read(str);
    if (r == NULL) return;
    // eval
    // TODO implement a stack trace of error messages
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

int main(int argc, char **argv) {
    MalEnv *env = MalEnv_new(NULL);
    OWN(env);
    env->reachable = true;

    // FIXME memory leak
    MalEnv_put(env, Symbol_new("nil"), MalDatum_nil());
    MalEnv_put(env, Symbol_new("true"), MalDatum_true());
    MalEnv_put(env, Symbol_new("false"), MalDatum_false());

    core_def_procs(env);

    rep("(def! not (fn* (x) (if x false true)))", env);
    // TODO implement using 'and' and 'or'
    rep("(def! >= (fn* (x y) (if (= x y) true (> x y))))", env);
    rep("(def! <  (fn* (x y) (not (>= x y))))", env);
    rep("(def! <= (fn* (x y) (if (= x y) true (< x y))))", env);

    while (1) {
        char *line = readline(PROMPT);
        if (line == NULL) {
            exit(EXIT_SUCCESS);
        }
        rep(line, env);
        free(line);
    }

    env->reachable = false;
    FREE(env);
    MalEnv_free(env);
}
