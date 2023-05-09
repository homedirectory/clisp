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

#define BADSTX(fmt, ...) \
    error("bad syntax: " fmt "\n", ##__VA_ARGS__);

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
    if (argc < proc->argc /* too few? */
            || (!proc->variadic && argc > proc->argc)) /* too much? */
    {
        throwf("procedure application: %s expects at least %d arguments, but %d were given", 
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
        const Symbol *param = Arr_get(proc->params, i);
        MalDatum *arg = Arr_get(args, i);
        MalEnv_put(proc_env, (MalDatum*) MalDatum_symbol_get(param->name), arg); 
    }

    // if variadic, then bind the last param to the rest of arguments
    if (proc->variadic) {
        const Symbol *var_param = Arr_get(proc->params, proc->params->len - 1);
        List *var_args = List_new();
        for (size_t i = proc->argc; i < args->len; i++) {
            MalDatum *arg = Arr_get(args, i);
            List_add(var_args, arg);
        }

        MalEnv_put(proc_env, (MalDatum*) MalDatum_symbol_get(var_param->name), MalDatum_new_list(var_args));
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
        const Symbol *param_name = Arr_get(proc->params, i);
        MalEnv_put(env, (MalDatum*) MalDatum_symbol_get(param_name->name), args->items[i]);
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
        BADSTX("if expects at least 2 arguments, but %d were given", argc);
        return NULL;
    }
    if (argc > 3) {
        BADSTX("if expects at most 3 arguments, but %d were given", argc);
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
        return (MalDatum*) MalDatum_nil();
    }
}

/* 'do' expression evalutes each succeeding expression returning the result of the last one.
 * (do expr ...)
 * return expr.map(eval).last
 */
static MalDatum *eval_do(const List *list, MalEnv *env) {
    int argc = List_len(list) - 1;
    if (argc == 0) {
        BADSTX("do expects at least 1 argument");
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
        BADSTX("fn*: cannot have empty body");
        return NULL;
    }

    // 1. validate params
    List *params;
    {
        MalDatum *snd = List_ref(list, 1);
        if (!MalDatum_islist(snd)) {
            BADSTX("fn*: bad syntax at parameter declaration");
            return NULL;
        }
        params = snd->value.list;
    }

    // all parameters should be symbols
    for (struct Node *node = params->head; node != NULL; node = node->next) {
        MalDatum *par = node->value;
        if (!MalDatum_istype(par, SYMBOL)) {
            BADSTX("fn* bad parameter list: expected a list of symbols, but %s was found in the list",
                    MalType_tostr(par->type));
            return NULL;
        }
    }

    size_t proc_argc = 0; // mandatory arg count
    bool variadic = false;
    Arr *param_names_symbols = Arr_newn(List_len(params));
    OWN(param_names_symbols);

    for (struct Node *node = params->head; node != NULL; node = node->next) {
        const Symbol *sym = node->value->value.sym;

        // '&' is a special symbol that marks a variadic procedure
        // exactly one parameter is expected after it
        // NOTE: we allow that parameter to also be named '&'
        if (Symbol_eq_str(sym, "&")) {
            if (node->next == NULL || node->next->next != NULL) {
                BADSTX("fn* bad parameter list: 1 parameter expected after '&'");
                return NULL;
            }
            const Symbol *last_sym = node->next->value->value.sym;
            Arr_add(param_names_symbols, (Symbol*) last_sym); // no need to copy
            variadic = true;
            break;
        }
        else {
            proc_argc++;
            Arr_add(param_names_symbols, (Symbol*) sym); // no need to copy
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
        BADSTX("def! expects 2 arguments, but %d were given", argc);
        return NULL;
    }

    MalDatum *snd = List_ref(list, 1);
    if (snd->type != SYMBOL) {
        BADSTX("def! expects a symbol as a 2nd argument, but %s was given",
                MalType_tostr(snd->type));
        return NULL;
    }
    const Symbol *id = snd->value.sym;

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

    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get(id->name), new_assoc);

    return new_assoc;
}

// (defmacro! id <fn*-expr>)
static MalDatum *eval_defmacro(const List *list, MalEnv *env) {
    size_t argc = List_len(list) - 1;
    if (argc != 2) {
        BADSTX("defmacro! expects 2 arguments, but %zu were given", argc);
        return NULL;
    }

    const MalDatum *arg1 = List_ref(list, 1);
    if (!(MalDatum_istype(arg1, SYMBOL))) {
        BADSTX("defmacro!: 1st arg must be a symbol, but was %s", MalType_tostr(arg1->type));
        return NULL;
    }
    const Symbol *id = arg1->value.sym;

    MalDatum *macro_datum = NULL;
    {
        MalDatum *arg2 = List_ref(list, 2);
        if (!MalDatum_islist(arg2)) {
            BADSTX("defmacro!: 2nd arg must be an fn* expression");
            return NULL;
        }

        const List *arg2_list = arg2->value.list;
        if (List_isempty(arg2_list)) {
            BADSTX("defmacro!: 2nd arg must be an fn* expression");
            return NULL;
        }
        const MalDatum *arg2_list_ref0 = List_ref(arg2_list, 0);
        if (!MalDatum_istype(arg2_list_ref0, SYMBOL)) {
            BADSTX("defmacro!: 2nd arg must be an fn* expression");
            return NULL;
        }
        const Symbol *sym = arg2_list_ref0->value.sym;
        if (!Symbol_eq_str(sym, "fn*")) {
            BADSTX("defmacro!: 2nd arg must be an fn* expression");
            return NULL;
        }
        MalDatum *evaled = eval(arg2, env);
        if (!evaled) return NULL;
        if (!MalDatum_istype(evaled, PROCEDURE)) {
            MalDatum_free(evaled);
            BADSTX("defmacro!: 2nd arg must evaluate to a procedure");
            return NULL;
        }
        macro_datum = evaled;
    }

    if (!macro_datum) return NULL;

    Proc *macro_proc = macro_datum->value.proc;
    macro_proc->macro = true;

    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get(id->name), macro_datum);

    return macro_datum;
}

/* (let* (bindings) expr) 
 * bindings := (id val ...) // must have an even number of elements 
 */
static MalDatum *eval_letstar(const List *list, MalEnv *env) {
    // 1. validate the list
    int argc = List_len(list) - 1;
    if (argc != 2) {
        BADSTX("let* expects 2 arguments, but %d were given", argc);
        return NULL;
    }

    MalDatum *snd = List_ref(list, 1);
    if (snd->type != LIST) {
        BADSTX("let* expects a list as a 2nd argument, but %s was given",
                MalType_tostr(snd->type));
        return NULL;
    }

    List *bindings = snd->value.list;
    if (List_isempty(bindings)) {
        BADSTX("let* expects a non-empty list of bindings");
        return NULL;
    }
    if (List_len(bindings) % 2 != 0) {
        BADSTX("let*: illegal bindings (expected an even-length list)");
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
            BADSTX("let*: illegal bindings (expected a symbol to be bound, but %s was given)", 
                    MalType_tostr(id_node->value->type));
            FREE(let_env);
            MalEnv_free(let_env);
            return NULL;
        }
        const Symbol *id = id_node->value->value.sym;

        // it's important to evaluate the bound value using the let* env,
        // so that previous bindings can be used during evaluation
        MalDatum *val = eval(id_node->next->value, let_env);
        OWN(val);
        if (val == NULL) {
            FREE(let_env);
            MalEnv_free(let_env);
            return NULL;
        }

        MalEnv_put(let_env, (MalDatum*) MalDatum_symbol_get(id->name), val);
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
        BADSTX("quote expects 1 argument, but %zd were given", argc);
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
        BADSTX("unquote expects 1 argument, but %zd were given", argc);
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
        BADSTX("splice-unquote expects 1 argument, but %zd were given", argc);
        return NULL;
    }

    MalDatum *arg1 = List_ref(list, 1);
    MalDatum *evaled = eval(arg1, env);
    if (!MalDatum_islist(evaled)) {
        BADSTX("splice-unquote: resulting value must be a list, but was %s",
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
        return (MalDatum*) MalDatum_empty_list();

    MalDatum *ref0 = List_ref(list, 0);
    if (MalDatum_istype(ref0, SYMBOL)) {
        const Symbol *sym = ref0->value.sym;

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
        BADSTX("quasiquote expects 1 argument, but %zd were given", argc);
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
        const Symbol *sym = ast0->value.sym;
        if (Symbol_eq_str(sym, "splice-unquote")) {
            BADSTX("splice-unquote: illegal context within quasiquote (nothing to splice into)");
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
            MalDatum *assoc = MalEnv_get(env, datum);
            if (assoc == NULL) {
                throwf("symbol binding '%s' not found", datum->value.sym->name);
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

static MalDatum *macroexpand_single(MalDatum *ast, MalEnv *env)
{
    if (!MalDatum_islist(ast)) return ast;

    List *ast_list = ast->value.list;
    if (List_isempty(ast_list)) return ast;

    // this is a macro call if the first list element is a symbol that's bound to a macro procedure
    const Proc *macro = NULL;
    {
        MalDatum *ref0 = List_ref(ast_list, 0);
        if (!MalDatum_istype(ref0, SYMBOL)) return ast;

        const MalDatum *datum = MalEnv_get(env, ref0);
        if (datum && MalDatum_istype(datum, PROCEDURE)) {
            const Proc *proc = datum->value.proc;
            if (!Proc_is_macro(proc)) return ast;
            else macro = proc; 
        }
        else return ast;
    }

    Arr *args = Arr_newn(List_len(ast_list) - 1);
    for (struct Node *node = ast_list->head->next; node != NULL; node = node->next) {
        Arr_add(args, node->value);
    }

    MalDatum *out = apply_proc(macro, args, env);

    if (out) MalDatum_own(out); // hack own
    Arr_free(args);
    if (out) MalDatum_release(out); // hack release

    return out;
}

static MalDatum *macroexpand(MalDatum *ast, MalEnv *env)
{
    MalDatum *out = ast;

    while (1) {
        MalDatum *expanded = macroexpand_single(out, env);
        if (!expanded) return NULL;
        else if (expanded == out) return out;
        else out = expanded;
    }

    return out;
}

// 'macroexpand' special form
static MalDatum *eval_macroexpand(List *ast_list, MalEnv *env)
{
    size_t argc = List_len(ast_list) - 1;
    if (argc != 1) {
        BADSTX("macroexpand expects 1 argument, but %zu were given", argc);
        return NULL;
    }

    MalDatum *arg1 = List_ref(ast_list, 1);
    return macroexpand(arg1, env);
}

// 'try*' special form
// (try* <expr1> (catch* <symbol> <expr2>))
// if <expr1> throws an exception, then the exception is bound to <symbol> 
// and <expr2> is evaluated
static MalDatum *eval_try_star(List *ast_list, MalEnv *env)
{
    size_t argc = List_len(ast_list) - 1;
    if (argc != 2) {
        BADSTX("try* expects 2 arguments, but %zu were given", argc);
        return NULL;
    }

    MalDatum *expr1 = List_ref(ast_list, 1);
    MalDatum *catch_form = List_ref(ast_list, 2);
    // validate catch_form
    const Symbol *err_sym = NULL;
    MalDatum *expr2 = NULL;
    {
        if (!MalDatum_islist(catch_form)) {
            BADSTX("try* expects (catch* SYMBOL EXPR) as 2nd arg");
            return NULL;
        }
        List *catch_list = catch_form->value.list;

        if (List_len(catch_list) != 3) {
            BADSTX("try* expects (catch* SYMBOL EXPR) as 2nd arg");
            return NULL;
        }

        // validate (catch* ...)
        MalDatum *catch0 = List_ref(catch_list, 0);
        if (!MalDatum_istype(catch0, SYMBOL) 
                || !Symbol_eq_str(catch0->value.sym, "catch*")) 
        {
            BADSTX("try* expects (catch* SYMBOL EXPR) as 2nd arg");
            return NULL;
        }

        MalDatum *catch1 = List_ref(catch_list, 1);
        if (!MalDatum_istype(catch1, SYMBOL)) {
            BADSTX("try* expects (catch* SYMBOL EXPR) as 2nd arg");
            return NULL;
        }

        err_sym = catch1->value.sym;
        expr2 = List_ref(catch_list, 2);
    }

    MalDatum *expr1_rslt = eval(expr1, env);
    if (expr1_rslt == NULL && didthrow()) {
        MalEnv *catch_env = MalEnv_new(env);
        Exception *exn = thrown_copy();
        MalEnv_put(catch_env, (MalDatum*) MalDatum_symbol_get(err_sym->name), MalDatum_new_exn(exn));

        MalDatum *expr2_rslt = eval(expr2, catch_env);

        if (expr2_rslt) MalDatum_own(expr2_rslt); // hack own
        MalEnv_free(catch_env);
        if (expr2_rslt) MalDatum_release(expr2_rslt); // hack release

        return expr2_rslt;
    }
    else {
        return expr1_rslt;
    }
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
            MalDatum *expanded = macroexpand(ast, env);
            if (!expanded) break;
            else if (expanded != ast && !MalDatum_islist(expanded)) {
                out = eval_ast(expanded, env);
                break;
            }
            else {
                // expanded == ast OR expanded is a list
                ast = expanded;
            }

            List *ast_list = ast->value.list;
            if (List_isempty(ast_list)) {
                out = (MalDatum*) MalDatum_empty_list();
                break;
            }

            MalDatum *head = List_ref(ast_list, 0);
            // handle special forms: def!, let*, if, do, fn*, quote, quasiquote,
            // defmacro!, macroexpand, try*/catch*
            if (MalDatum_istype(head, SYMBOL)) {
                const Symbol *sym = head->value.sym;
                if (Symbol_eq_str(sym, "def!")) {
                    out = eval_def(ast_list, apply_env);
                    break;
                }
                if (Symbol_eq_str(sym, "defmacro!")) {
                    out = eval_defmacro(ast_list, apply_env);
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
                else if (Symbol_eq_str(sym, "macroexpand")) {
                    out = eval_macroexpand(ast_list, apply_env);
                    break;
                }
                else if (Symbol_eq_str(sym, "try*")) {
                    out = eval_try_star(ast_list, apply_env);
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
                throwf("application: expected a procedure");
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
 * (apply proc <interm> arg-list) 
 * if <interm> (intermediate arguments) are present, they are simply consed onto arg-list;
 * for example: (apply f a b '(c d)) <=> (apply f '(a b c d))
 * */
static MalDatum *mal_apply(const Proc *proc, const Arr *args, MalEnv *env)
{
    const MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, PROCEDURE);
    if (!arg0) return NULL;
    const Proc *f = arg0->value.proc;

    const MalDatum *arg_last = Arr_last(args);
    if (!MalDatum_islist(arg_last)) {
        throwf("apply: bad last arg: expected a list");
        return NULL;
    }
    const List *arg_list = arg_last->value.list;

    size_t interm_argc = args->len - 2;

    Arr *args_arr = Arr_newn(List_len(arg_list) + interm_argc);
    OWN(args_arr);

    // first intermediate arguments
    if (interm_argc > 0) {
        for (size_t i = 1; i < 1 + interm_argc; i++) {
            Arr_add(args_arr, Arr_get(args, i));
        }
    }
    // now arg-list
    for (struct Node *node = arg_list->head; node != NULL; node = node->next) {
        Arr_add(args_arr, node->value);
    }

    MalDatum *rslt = apply_proc(f, args_arr, env);

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
        throwf("read-string: could not parse bad syntax");
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
        throwf("slurp: can't read file %s", path);
        return NULL;
    }

    char *contents = file_to_str(path);
    if (!contents) {
        throwf("slurp: failed to read file %s", path);
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

// map : maps over a list/vector using a procedure
// TODO accept multiple lists/vectors
static MalDatum *mal_map(const Proc *proc, const Arr *args, MalEnv *env) 
{
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, PROCEDURE);
    if (!arg0) return NULL;
    Proc *mapper = arg0->value.proc;

    MalDatum *arg1 = verify_proc_arg_type(proc, args, 1, LIST);
    if (!arg1) return NULL;
    List *list = arg1->value.list;

    if (List_isempty(list)) {
        return (MalDatum*) MalDatum_empty_list();
    }

    List *out = List_new();
    // args to mapper proc
    Arr *mapper_args = Arr_newn(1);
    Arr_add(mapper_args, NULL); // to increase length to 1

    for (struct Node *node = list->head; node != NULL; node = node->next) {
        MalDatum *list_elt = node->value;

        Arr_replace(mapper_args, 0, list_elt);
        MalDatum *new_elt = apply_proc(mapper, mapper_args, env);
        if (!new_elt) {
            List_free(out);
            Arr_free(mapper_args);
            return NULL;
        }

        List_add(out, new_elt);
    }

    Arr_free(mapper_args);

    return MalDatum_new_list(out);
}

int main(int argc, char **argv) {
    init_symbol_table();

    MalEnv *env = MalEnv_new(NULL);
    OWN(env);
    MalEnv_own(env);

    // FIXME memory leak
    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get("nil"), (MalDatum*) MalDatum_nil());
    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get("true"), (MalDatum*) MalDatum_true());
    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get("false"), (MalDatum*) MalDatum_false());

    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get("apply"), MalDatum_new_proc(
                Proc_builtin("apply", 2, true, mal_apply)));

    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get("read-string"), MalDatum_new_proc(
            Proc_builtin("read-string", 1, false, mal_read_string)));
    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get("slurp"), MalDatum_new_proc(
            Proc_builtin("slurp", 1, false, mal_slurp)));
    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get("eval"), MalDatum_new_proc(
            Proc_builtin("eval", 1, false, mal_eval)));

    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get("swap!"), MalDatum_new_proc(
            Proc_builtin("swap!", 2, true, mal_swap_bang)));

    MalEnv_put(env, (MalDatum*) MalDatum_symbol_get("map"), MalDatum_new_proc(
            Proc_builtin("map", 2, false, mal_map)));

    core_def_procs(env);

    rep("(def! load-file\n"
            // closing paren of 'do' must be on a separate line in case a file ends
            // with a comment without a newline at the end
            "(fn* (path) (eval (read-string (str \"(do \" (slurp path) \"\n)\")))\n"
                        "(println \"loaded file\" path) nil))", 
            env);

    rep("(load-file \"lisp/core.lisp\")", env);

    // TODO if the first arg is a filename, then eval (load-file <filename>)
    // TODO bind *ARGV* to command line arguments

    // using_history();
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

    clear_history();

    free_symbol_table();
}
