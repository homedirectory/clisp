#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <stddef.h>
#include "reader.h"
#include "types.h"
#include "printer.h"
#include "env.h"
#include "common.h"
#include "core.h"
#include "mem_debug.h"
#include "utils.h"

#define PROMPT "user> "
#define HISTORY_FILE ".mal_history"

MalDatum *eval(MalDatum *datum, MalEnv *env);
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
        return proc->logic.apply(proc, args, env);
    }

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
            List_add(var_args, arg);
        }

        MalEnv_put(proc_env, var_param, MalDatum_new_list(var_args));
    }

    // 2. evaluate the body
    const Arr *body = proc->logic.body;
    // the body must not be empty at this point
    if (body->len == 0) FATAL("empty body");
    // evalute each expression and return the result of the last one
    for (int i = 0; i < body->len - 1; i++) {
        MalDatum *dtm = body->items[i];
        // TODO check NULL
        MalDatum_free(eval(dtm, proc_env));
    }
    MalDatum *out = eval(body->items[body->len - 1], proc_env);

    FREE(proc_env);
    MalEnv_free(proc_env);

    return out;
}

static MalDatum *eval_application_tco(const Proc *proc, const Arr* args, MalEnv *env)
{
    char *proc_name = Proc_name(proc);
    OWN(proc_name);

    int argc = args->len;
    // too few arguments?
    if (argc < proc->argc) {
        ERROR("procedure application: %s expects at least %d arguments, but %d were given", 
                proc_name, proc->argc, argc);
        FREE(proc_name);
        free(proc_name);
        return NULL;
    }
    // too much arguments?
    else if (!proc->variadic && argc > proc->argc) {
        ERROR("procedure application: %s expects %d arguments, but %d were given", 
                proc_name, proc->argc, argc);
        FREE(proc_name);
        free(proc_name);
        return NULL;
    }

    FREE(proc_name);
    free(proc_name);

    for (size_t i = 0; i < args->len; i++) {
        Symbol *param_name = Arr_get(proc->params, i);
        MalEnv_put(env, param_name, args->items[i]);
    }

    Arr *body = proc->logic.body;
    // eval body except for the last expression 
    // (TODO transform into 'do' special form)
    for (size_t i = 0; i < body->len - 1; i++) {
        MalDatum *evaled = eval(body->items[i], env);
        if (evaled)
            MalDatum_free(evaled);
        else {
            LOG_NULL(evaled);
            return NULL;
        }
    }

    MalDatum *body_last = body->items[body->len - 1];
    return body_last;
}

/* 'if' expression comes in 2 forms:
 * 1. (if cond if-true if-false)
 * return eval(cond) ? eval(if-true) : eval(if-false)
 * 2. (if cond if-true)
 * return eval(cond) ? eval(if-true) : nil
 */
static MalDatum *eval_if(const List *ast_list, MalEnv *env) {
    // 1. validate the AST
    int argc = List_len(ast_list) - 1;
    if (argc < 2) {
        ERROR("if expects at least 2 arguments, but %d were given", argc);
        return NULL;
    }
    if (argc > 3) {
        ERROR("if expects at most 3 arguments, but %d were given", argc);
        return NULL;
    }

    MalDatum *ev_cond = eval(List_ref(ast_list, 1), env);
    if (ev_cond == NULL) return NULL;
    OWN(ev_cond);

    // eval(cond) is true if it's neither 'nil' nor 'false'
    if (!MalDatum_isnil(ev_cond) && !MalDatum_isfalse(ev_cond)) {
        return List_ref(ast_list, 2);
    } 
    else if (argc == 3) {
        return List_ref(ast_list, 3);
    } 
    else {
        return MalDatum_nil();
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

    Proc *proc = Proc_new_lambda(proc_argc, variadic, param_names_symbols, body, env);
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

    // if id is being bound to an unnamed procedure, then set id as its name
    if (MalDatum_istype(new_assoc, PROCEDURE)) {
        Proc *proc = new_assoc->value.proc;
        if (!proc->name) {
            proc->name = dyn_strcpy(id->name);
        }
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
            ERROR("let*: illegal bindings (expected a symbol to be bound, but %s was given)", 
                    MalType_tostr(id_node->value->type));
            FREE(let_env);
            MalEnv_free(let_env);
            return NULL;
        }
        Symbol *id = id_node->value->value.sym;

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

    // this is a hack
    // if the returned value was computed in let* bindings,
    // then we don't want it to be freed when we free the let_env,
    // so we increment its ref count only to decrement it after let_env is freed
    MalDatum_own(out);

    // discard the let* env
    FREE(let_env);
    MalEnv_free(let_env);

    // the hack cont.
    MalDatum_release(out);

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

MalDatum *eval_ast(const MalDatum *datum, MalEnv *env) {
    MalDatum *out = NULL;

    switch (datum->type) {
        case SYMBOL:
            Symbol *sym = datum->value.sym;
            MalDatum *assoc = MalEnv_get(env, sym);
            if (assoc == NULL) {
                ERROR("symbol binding '%s' not found", sym->name);
            } else {
                out = assoc;;
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
            // STRING | INT
            out = MalDatum_copy(datum);
            break;
    }

    return out;
}

#ifdef EVAL_STACK_DEPTH
static int eval_stack_depth = 0; 
#endif

MalDatum *eval(MalDatum *ast, MalEnv *env) {
#ifdef EVAL_STACK_DEPTH
    eval_stack_depth++;
    printf("ENTER eval, stack depth: %d\n", eval_stack_depth);
#endif
    MalEnv *eval_env = env;
    bool tco = false;

    MalDatum *out = NULL;

    while (ast) {
        if (MalDatum_islist(ast)) {
            List *ast_list = ast->value.list;
            if (List_isempty(ast_list)) {
                out = MalDatum_empty_list();
                break;
            }

            MalDatum *head = List_ref(ast_list, 0);
            // handle special forms: def!, let*, if, do, fn*
            if (MalDatum_istype(head, SYMBOL)) {
                Symbol *sym = head->value.sym;
                if (Symbol_eq_str(sym, "def!")) {
                    out = eval_def(ast_list, eval_env);
                    break;
                }
                else if (Symbol_eq_str(sym, "let*")) {
                    // applying TCO to let* saves us only 1 level of call stack depth
                    // TODO so we can be lazy about it
                    out = eval_letstar(ast_list, eval_env);
                    break;
                }
                else if (Symbol_eq_str(sym, "if")) {
                    // eval the condition and replace AST with the AST of the branched part
                    ast = eval_if(ast_list, eval_env);
                    continue;
                }
                else if (Symbol_eq_str(sym, "do")) {
                    // applying TCO to do saves us only 1 level of call stack depth
                    // TODO so we can be lazy about it
                    out = eval_do(ast_list, eval_env);
                    break;
                }
                else if (Symbol_eq_str(sym, "fn*")) {
                    out = eval_fnstar(ast_list, eval_env);
                    break;
                }
            }

            // looks like a procedure application
            // 1. eval the ast_list
            List *evaled_list = eval_list(ast_list, eval_env);
            if (evaled_list == NULL) {
                out = NULL;
                break;
            }
            OWN(evaled_list);
            // 2. make sure that the 1st element is a procedure
            MalDatum *first = List_ref(evaled_list, 0);
            if (!MalDatum_istype(first, PROCEDURE)) {
                ERROR("application: expected a procedure");
                out = NULL;
                FREE(evaled_list);
                List_free(evaled_list);
                break;
            }

            Proc *proc = first->value.proc;

            Arr *args = Arr_newn(List_len(evaled_list) - 1);
            OWN(args);
            for (struct Node *node = evaled_list->head->next; node != NULL; node = node->next) {
                Arr_add(args, node->value);
            }

            // 3. apply TCO only if it's a non-lambda MAL procedure
            if (!proc->builtin && Proc_is_named(proc)) {
                if (!tco) {
                    // one-time creation of env for TCO
                    eval_env = MalEnv_new(eval_env);
                    eval_env->reachable = true;
                    OWN(eval_env);
                    tco = true;
                }
                // args will be put into eval_env
                ast = eval_application_tco(proc, args, eval_env);

                FREE(args);
                Arr_freep(args, (free_t) MalDatum_free);

                FREE(evaled_list);
                List_free(evaled_list);
            }
            else {
                // 4. otherwise just return the result of procedure application
                // builtin procedures do not get TCO
                // unnamed procedures cannot be called recursively apriori
                out = apply_proc(proc, args, eval_env);
                FREE(args);
                Arr_freep(args, (free_t) MalDatum_free);
                FREE(evaled_list);
                List_free(evaled_list);
                break;
            }
        }
        else { // AST is not a list
            out = eval_ast(ast, eval_env);
            break;
        }
    }

    if (eval_env != env) {
        // a hack to prevent the return value of a procedure to be freed (similar to let* hack)
        MalDatum_own(out); // hack own

        eval_env->reachable = false;
        FREE(eval_env);
        MalEnv_free(eval_env);

        MalDatum_release(out); // hack release
    }

#ifdef EVAL_STACK_DEPTH
    eval_stack_depth--;
    printf("LEAVE eval, stack depth: %d\n", eval_stack_depth);
#endif

    return out;
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
    // the evaled value can be either discarded (e.g., (+ 1 2) => 3)
    // or owned by something (e.g., (def! x 5) => 5)
    MalDatum_free(e);
    if (p != NULL) { 
        printf("%s\n", p);
        free(p);
    }
}

// TODO reorganise file structure and move to core.c
/* apply : applies a procedure to the list of arguments 
 * (apply proc args-list) */
static MalDatum *mal_apply(const Proc *proc, const Arr *args, MalEnv *env)
{
    MalDatum *proc_arg = Arr_get(args, 0);
    if (!MalDatum_istype(proc_arg, PROCEDURE)) {
        ERROR("apply: bad 1st arg: expected a procedure");
        return NULL;
    }

    MalDatum *list_arg = Arr_get(args, 1);
    if (!MalDatum_islist(list_arg)) {
        ERROR("apply: bad 2nd arg: expected a list");
        return NULL;
    }

    Proc *applied_proc = proc_arg->value.proc;
    List *list = list_arg->value.list;

    Arr *args_arr = Arr_newn(List_len(list)); // of *MalDatum
    OWN(args_arr);

    for (struct Node *node = list->head; node != NULL; node = node->next) {
        Arr_add(args_arr, node->value);
    }

    MalDatum *rslt = apply_proc(applied_proc, args_arr, NULL);

    FREE(args_arr);
    Arr_free(args_arr);

    return rslt;
}

// read-string : takes a Mal string and reads it as if it were entered into the prompt,
// transforming it into a raw AST. Essentially, exposes the internal READ function
static MalDatum *mal_read_string(const Proc *proc, const Arr *args, MalEnv *env) 
{
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, STRING);
    if (!arg0) return NULL;

    const char *string = arg0->value.string;
    MalDatum *ast = read(string);

    if (ast == NULL) {
        ERROR("read-string: could not parse bad syntax");
        return NULL;
    }

    return ast;
}

// slurp : takes a file name (string) and returns the contents of the file as a string
static MalDatum *mal_slurp(const Proc *proc, const Arr *args, MalEnv *env) 
{
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, STRING);
    if (!arg0) return NULL;

    const char *path = arg0->value.string;
    if (!file_readable(path)) {
        ERROR("slurp: can't read file %s", path);
        return NULL;
    }

    char *contents = file_to_str(path);
    if (!contents) {
        ERROR("slurp: failed to read file %s", path);
        return NULL;
    }

    MalDatum *out = MalDatum_new_string(contents);
    free(contents);

    return out;
}

// eval : takes an AST and evaluates it in the top-level environment
// local environments are not taken into account by eval
static MalDatum *mal_eval(const Proc *proc, const Arr *args, MalEnv *env) 
{
    MalDatum *arg0 = Arr_get(args, 0);
    MalEnv *top_env = MalEnv_enclosing_root(env);
    return eval(arg0, top_env);
}


int main(int argc, char **argv) {
    MalEnv *env = MalEnv_new(NULL);
    OWN(env);
    env->reachable = true;

    // FIXME memory leak
    MalEnv_put(env, Symbol_new("nil"), MalDatum_nil());
    MalEnv_put(env, Symbol_new("true"), MalDatum_true());
    MalEnv_put(env, Symbol_new("false"), MalDatum_false());

    MalEnv_put(env, Symbol_new("apply"), MalDatum_new_proc(
                Proc_builtin("apply", 2, false, mal_apply)));

    MalEnv_put(env, Symbol_new("read-string"), MalDatum_new_proc(
            Proc_builtin("read-string", 1, false, mal_read_string)));
    MalEnv_put(env, Symbol_new("slurp"), MalDatum_new_proc(
            Proc_builtin("slurp", 1, false, mal_slurp)));
    MalEnv_put(env, Symbol_new("eval"), MalDatum_new_proc(
            Proc_builtin("eval", 1, false, mal_eval)));

    core_def_procs(env);

    rep("(def! load-file\n"
            "(fn* (path) (eval (read-string (str \"(do \" (slurp path) \")\")))\n"
            "(println \"loaded file\" path) nil))", 
            env);

    rep("(load-file \"core.mal\")", env);

    read_history(HISTORY_FILE);
    // if (read_history(HISTORY_FILE) != 0) {
    //     fprintf(stderr, "failed to read history file %s\n", HISTORY_FILE);
    //     exit(EXIT_FAILURE);
    // }

    while (1) {
        char *line = readline(PROMPT);
        if (line == NULL) {
            exit(EXIT_SUCCESS);
        }

        add_history(line);
        // FIXME append to history file just once on exit
        if (append_history(1, HISTORY_FILE) != 0)
            fprintf(stderr, 
                    "failed to append to history file %s (try creating it manually)\n", 
                    HISTORY_FILE);

        rep(line, env);
        free(line);
    }

    env->reachable = false;
    FREE(env);
    MalEnv_free(env);
}
