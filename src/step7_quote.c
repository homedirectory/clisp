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

static bool verify_proc_application(const Proc *proc, const Arr* args)
{
    char *proc_name = Proc_name(proc);

    int argc = args->len;
    // too few arguments?
    if (argc < proc->argc) {
        ERROR("procedure application: %s expects at least %d arguments, but %d were given", 
                proc_name, proc->argc, argc);
        free(proc_name);
        return false;
    }
    // too much arguments?
    else if (!proc->variadic && argc > proc->argc) {
        ERROR("procedure application: %s expects %d arguments, but %d were given", 
                proc_name, proc->argc, argc);
        free(proc_name);
        return false;
    }

    free(proc_name);
    return true;
}

// procedure application without TCO
// args: array of *MalDatum (argument values)
static MalDatum *apply_proc(const Proc *proc, const Arr *args, MalEnv *env) {
    if (!verify_proc_application(proc, args)) return NULL;

    if (proc->builtin) {
        return proc->logic.apply(proc, args, env);
    }

    // local env is created even if a procedure expects no parameters
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

    // a hack to prevent the return value of a procedure to be freed
    MalDatum_own(out); // hack own

    FREE(proc_env);
    MalEnv_free(proc_env);

    MalDatum_release(out); // hack release

    return out;
}

static MalDatum *eval_application_tco(const Proc *proc, const Arr* args, MalEnv *env)
{
    if (!verify_proc_application(proc, args)) return NULL;

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
        ERROR("fn*: cannot have empty body");
        return NULL;
    }

    // 1. validate params
    List *params;
    {
        MalDatum *snd = List_ref(list, 1);
        if (!MalDatum_islist(snd)) {
            ERROR("fn*: bad syntax at parameter declaration");
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
        Arr_add(body, node->value);
    }

    Proc *proc = Proc_new_lambda(proc_argc, variadic, param_names_symbols, body, env);

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

// quote : this special form returns its argument without evaluating it
static MalDatum *eval_quote(const List *list, MalEnv *env) {
    size_t argc = List_len(list) - 1;
    if (argc != 1) {
        ERROR("quote expects 1 argument, but %zd were given", argc);
        return NULL;
    }

    MalDatum *arg1 = List_ref(list, 1);
    return arg1;
}

// helper function for eval_quasiquote_list
static MalDatum *eval_unquote(const List *list, MalEnv *env) 
{
    size_t argc = List_len(list) - 1;
    if (argc != 1) {
        ERROR("unquote expects 1 argument, but %zd were given", argc);
        return NULL;
    }

    MalDatum *arg1 = List_ref(list, 1);
    return eval(arg1, env);
}

// helper function for eval_quasiquote_list
static List *eval_splice_unquote(const List *list, MalEnv *env) 
{
    size_t argc = List_len(list) - 1;
    if (argc != 1) {
        ERROR("splice-unquote expects 1 argument, but %zd were given", argc);
        return NULL;
    }

    MalDatum *arg1 = List_ref(list, 1);
    MalDatum *evaled = eval(arg1, env);
    if (!MalDatum_islist(evaled)) {
        ERROR("splice-unquote: resulting value must be a list, but was %s",
                MalType_tostr(evaled->type));
        MalDatum_free(evaled);
        return NULL;
    }
    else {
        return evaled->value.list;
    }
}

// helper function for eval_quasiquote
static MalDatum *eval_quasiquote_list(const List *list, MalEnv *env, bool *splice)
{
    if (List_isempty(list)) 
        return MalDatum_empty_list();

    MalDatum *ref0 = List_ref(list, 0);
    if (MalDatum_istype(ref0, SYMBOL)) {
        Symbol *sym = ref0->value.sym;

        if (Symbol_eq_str(sym, "unquote")) {
            return eval_unquote(list, env);
        }
        else if (Symbol_eq_str(sym, "splice-unquote")) {
            List *evaled = eval_splice_unquote(list, env);
            if (!evaled)  {
                return NULL;
            } 
            else {
                *splice = true;
                return MalDatum_new_list(evaled);
            }
        }
    }

    List *out_list = List_new();

    for (struct Node *node = list->head; node != NULL; node = node->next) {
        MalDatum *dtm = node->value;

        if (MalDatum_islist(dtm)) { // recurse
            bool _splice = false;
            MalDatum *evaled = eval_quasiquote_list(dtm->value.list, env, &_splice);
            if (!evaled) {
                List_free(out_list);
                return NULL;
            }
            if (_splice) {
                List_append(out_list, evaled->value.list);
            }
            else {
                List_add(out_list, evaled);
            }
        }
        else { // not a list
            List_add(out_list, dtm);
        }
    }

    return MalDatum_new_list(out_list);
}

// quasiquote : This allows a quoted list to have internal elements of the list
// that are temporarily unquoted (normal evaluation). There are two special forms
// that only mean something within a quasiquoted list: unquote and splice-unquote.

// some examples:
// (quasiquote (unquote 1))                 -> 1
// (def! lst (quote (b c)))
// (quasiquote (a (unquote lst) d))         -> (a (b c) d)
// (quasiquote (a (splice-unquote lst) d))  -> (a b c d)
// (quasiquote (a (and (unquote lst)) d))   -> (a (and (b c)) d)

// splice-unquote may only appear in an enclosing list form:
// (quasiquote (splice-unquote (list 1 2)))   -> ERROR!
// (quasiquote ((splice-unquote (list 1 2)))) -> (1 2)
static MalDatum *eval_quasiquote(const List *list, MalEnv *env) 
{
    size_t argc = List_len(list) - 1;
    if (argc != 1) {
        ERROR("quasiquote expects 1 argument, but %zd were given", argc);
        return NULL;
    }

    MalDatum *ast = List_ref(list, 1);
    if (!MalDatum_islist(ast))
        return ast;

    const List *ast_list = ast->value.list;
    if (List_isempty(ast_list)) 
        return ast;

    // splice-unquote may only appear in an enclosing list form
    MalDatum *ast0 = List_ref(ast_list, 0);
    if (MalDatum_istype(ast0, SYMBOL)) {
        Symbol *sym = ast0->value.sym;
        if (Symbol_eq_str(sym, "splice-unquote")) {
            ERROR("splice-unquote: illegal context within quasiquote");
            return NULL;
        }
    }

    MalDatum *out = eval_quasiquote_list(ast_list, env, NULL);
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
    // we create a new environment for each procedure application to bind params to args
    // because of TCO, ast might be the last body part of a procedure, so we might need 
    // apply_env created in the previous loop cycle to evaluate ast
    MalEnv *apply_env = env;
    MalDatum *out = NULL;

    while (ast) {
        if (MalDatum_islist(ast)) {
            List *ast_list = ast->value.list;
            if (List_isempty(ast_list)) {
                out = MalDatum_empty_list();
                break;
            }

            MalDatum *head = List_ref(ast_list, 0);
            // handle special forms: def!, let*, if, do, fn*, quote, quasiquote
            if (MalDatum_istype(head, SYMBOL)) {
                Symbol *sym = head->value.sym;
                if (Symbol_eq_str(sym, "def!")) {
                    out = eval_def(ast_list, apply_env);
                    break;
                }
                else if (Symbol_eq_str(sym, "let*")) {
                    // applying TCO to let* saves us only 1 level of call stack depth
                    // TODO so we can be lazy about it
                    out = eval_letstar(ast_list, apply_env);
                    break;
                }
                else if (Symbol_eq_str(sym, "if")) {
                    // eval the condition and replace AST with the AST of the branched part
                    ast = eval_if(ast_list, apply_env);
                    continue;
                }
                else if (Symbol_eq_str(sym, "do")) {
                    // applying TCO to do saves us only 1 level of call stack depth
                    // TODO so we can be lazy about it
                    out = eval_do(ast_list, apply_env);
                    break;
                }
                else if (Symbol_eq_str(sym, "fn*")) {
                    out = eval_fnstar(ast_list, apply_env);
                    break;
                }
                else if (Symbol_eq_str(sym, "quote")) {
                    out = eval_quote(ast_list, apply_env);
                    break;
                }
                else if (Symbol_eq_str(sym, "quasiquote")) {
                    out = eval_quasiquote(ast_list, apply_env);
                    break;
                }
            }

            // looks like a procedure application
            // if TCO has been applied, then ast_list is the last body part of a procedure
            // 1. eval the ast_list
            List *evaled_list = eval_list(ast_list, apply_env);
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
                // MalDatum_own(node->value); // hold onto argument values
            }

            // previous application's env is no longer needed after we have argument values
            if (apply_env != env) {
                MalEnv_free(apply_env);
                apply_env = NULL;
            }

            // 3. apply TCO only if it's a non-lambda MAL procedure
            if (!proc->builtin && Proc_is_named(proc)) {
                // args will be put into apply_env
                apply_env = MalEnv_new(proc->env);
                ast = eval_application_tco(proc, args, apply_env);

                // release and free args
                FREE(args);
                // Arr_freep(args, (free_t) MalDatum_release_free);
                Arr_free(args);

                FREE(evaled_list);
                List_free(evaled_list);
            }
            else {
                // 4. otherwise just return the result of procedure application
                // builtin procedures do not get TCO
                // unnamed procedures cannot be called recursively apriori
                out = apply_proc(proc, args, env);
                if (out) MalDatum_own(out); // hack own

                FREE(args);
                // Arr_freep(args, (free_t) MalDatum_release_free);
                Arr_free(args);

                FREE(evaled_list);
                List_free(evaled_list);

                if (out) MalDatum_release(out); // hack release
                break;
            }
        }
        else { // AST is not a list
            out = eval_ast(ast, apply_env);
            break;
        }
    } // end while

    // we might need to free the application env of the last tail call 
    if (apply_env && apply_env != env) {
        // a hack to prevent the return value of a procedure to be freed (similar to let* hack)
        if (out) MalDatum_own(out); // hack own

        FREE(apply_env);
        MalEnv_free(apply_env);

        if (out) MalDatum_release(out); // hack release
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
    if (!e) return;
    MalDatum_own(e); // prevent from being freed before printing

    MalDatum_free(r);

    // print
    char *p = print(e);
    if (p != NULL) { 
        printf("%s\n", p);
        free(p);
    }

    // the evaled value can be either discarded (e.g., (+ 1 2) => 3)
    // or owned by something (e.g., (def! x 5) => 5)
    MalDatum_release_free(e);
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

    MalDatum *rslt = apply_proc(applied_proc, args_arr, env);

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

// swap! : Takes an atom, a function, and zero or more function arguments. The
// atom's value is modified to the result of applying the function with the atom's
// value as the first argument and the optionally given function arguments as the
// rest of the arguments. The new atom's value is returned.
static MalDatum *mal_swap_bang(const Proc *proc, const Arr *args, MalEnv *env) {
    Atom *atom;
    {
        MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, ATOM);
        if (!arg0) return NULL;
        atom = arg0->value.atom;
    }

    const Proc *applied_proc;
    {
        MalDatum *arg1 = verify_proc_arg_type(proc, args, 1, PROCEDURE);
        if (!arg1) return NULL;
        applied_proc = arg1->value.proc;
    }

    Arr *proc_args = Arr_newn(1 + args->len - 2); // of *MalDatum
    OWN(proc_args);

    // use atom's value as the 1st argument 
    Arr_add(proc_args, atom->datum);

    for (size_t i = 2; i < args->len; i++) {
        Arr_add(proc_args, args->items[i]);
    }

    MalDatum *rslt = NULL;

    if (verify_proc_application(applied_proc, proc_args)) {
        rslt = apply_proc(applied_proc, proc_args, env);
        Atom_reset(atom, rslt);
    }

    FREE(proc_args);
    Arr_free(proc_args);

    return rslt;
}


int main(int argc, char **argv) {
    MalEnv *env = MalEnv_new(NULL);
    OWN(env);
    MalEnv_own(env);

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

    MalEnv_put(env, Symbol_new("swap!"), MalDatum_new_proc(
            Proc_builtin("swap!", 2, true, mal_swap_bang)));

    core_def_procs(env);

    rep("(def! load-file\n"
            // closing paren of 'do' must be on a separate line in case a file ends
            // with a comment without a newline at the end
            "(fn* (path) (eval (read-string (str \"(do \" (slurp path) \"\n)\")))\n"
                        "(println \"loaded file\" path) nil))", 
            env);

    rep("(load-file \"core.mal\")", env);

    // TODO if the first arg is a filename, then eval (load-file <filename>)
    // TODO bind *ARGV* to command line arguments

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

        if (line[0] != '\0') {
            add_history(line);
            // FIXME append to history file just once on exit
            if (append_history(1, HISTORY_FILE) != 0)
                fprintf(stderr, 
                        "failed to append to history file %s (try creating it manually)\n", 
                        HISTORY_FILE);
        }

        rep(line, env);
        free(line);
    }

    MalEnv_release(env);
    FREE(env);
    MalEnv_free(env);
}
