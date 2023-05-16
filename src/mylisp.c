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

LispDatum *eval(LispDatum *datum, MalEnv *env);
LispDatum *eval_ast(const LispDatum *datum, MalEnv *env);
List *eval_list(const List *list, MalEnv *env);

static LispDatum *read(const char* in) {
    Reader *rdr = read_str(in);
    OWN(rdr);
    if (rdr == NULL) return NULL;
    if (rdr->tokens->len == 0) {
        FREE(rdr);
        Reader_free(rdr);
        return NULL;
    }
    LispDatum *form = read_form(rdr);
    FREE(rdr);
    Reader_free(rdr);
    return form;
}

static bool verify_proc_application(const Proc *proc, const Arr* args)
{
    const Symbol *proc_name = Proc_name(proc);

    int argc = args->len;
    int proc_argc = Proc_argc(proc);
    if (argc < proc_argc /* too few? */
            || (!Proc_isva(proc) && argc > proc_argc)) /* too much? */
    {
        throwf(Symbol_name(proc_name),
                "expected at least %d arguments, but %d were given", 
                proc_argc, argc);
        return false;
    }

    return true;
}

// procedure application without TCO
// args: array of *LispDatum (argument values)
static LispDatum *apply_proc(const Proc *proc, const Arr *args, MalEnv *env) {
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
    // ((lambda (x) x) 10) => 10
    // Here a local env { x = 10 } with enclosing one set to the global env will be created
    // and discarded immediately after the result (10) is obtained.
    // (((lambda (x) (lambda () x)) 10)) => 10
    // But here the result of this application will be a procedure that should
    // "remember" about x = 10, so the local env should be preserved. 
    MalEnv *proc_env = MalEnv_new(proc->env);
    OWN(proc_env);

    // 1. bind params to args in the local env
    // mandatory arguments
    for (int i = 0; i < proc->argc; i++) {
        Symbol *param = Arr_get(proc->params, i);
        LispDatum *arg = Arr_get(args, i);
        MalEnv_put(proc_env, param, arg); 
    }

    // if variadic, then bind the last param to the rest of arguments
    if (Proc_isva(proc)) {
        Symbol *var_param = Arr_get(proc->params, proc->params->len - 1);
        List *var_args = List_new();
        for (size_t i = proc->argc; i < args->len; i++) {
            LispDatum *arg = Arr_get(args, i);
            List_add(var_args, arg);
        }

        MalEnv_put(proc_env, var_param, (LispDatum*) var_args);
    }

    // 2. evaluate the body
    const List *body = proc->logic.body;
    // the body must not be empty at this point
    if (List_isempty(body)) FATAL("empty body");
    // evalute each expression and return the result of the last one
    struct Node *node;
    // for all except last last 
    for (node = body->head; node->next != NULL; node = node->next) {
        LispDatum *dtm = node->value;
        LispDatum *evaled = eval(dtm, proc_env);
        LispDatum_free(evaled);
    }
    LispDatum *out = eval(node->value, proc_env);

    // a hack to prevent the return value of a procedure to be freed
    if (out) 
        LispDatum_own(out); // hack own

    FREE(proc_env);
    MalEnv_free(proc_env);

    if (out) 
        LispDatum_rls(out); // hack release

    return out;
}

static LispDatum *eval_application_tco(const Proc *proc, const Arr* args, MalEnv *env)
{
    if (!verify_proc_application(proc, args)) return NULL;

    for (size_t i = 0; i < args->len; i++) {
        Symbol *param = Arr_get(proc->params, i);
        MalEnv_put(env, param, args->items[i]);
    }

    const List *body = proc->logic.body;
    // eval body except for the last expression 
    // (TODO transform into 'do' special form)
    struct Node *node;
    // for all except last
    for (node = body->head; node->next; node = node->next) {
        LispDatum *body_part = node->value;
        LispDatum *evaled = eval(body_part, env);
        if (evaled)
            LispDatum_free(evaled);
        else {
            LOG_NULL(evaled);
            return NULL;
        }
    }

    LispDatum *body_last = node->value;
    return body_last;
}

/* 'if' expression comes in 2 forms:
 * 1. (if cond if-true if-false)
 * return eval(cond) ? eval(if-true) : eval(if-false)
 * 2. (if cond if-true)
 * return eval(cond) ? eval(if-true) : nil
 */
static LispDatum *eval_if(const List *ast_list, MalEnv *env) {
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

    LispDatum *ev_cond = eval(List_ref(ast_list, 1), env);
    if (ev_cond == NULL) return NULL;
    OWN(ev_cond);

    // eval(cond) is true if it's neither 'nil' nor 'false'
    if (!LispDatum_istype(ev_cond, NIL) && !LispDatum_istype(ev_cond, FALSE)) {
        return List_ref(ast_list, 2);
    } 
    else if (argc == 3) {
        return List_ref(ast_list, 3);
    } 
    else {
        return (LispDatum*) Nil_get();
    }
}

/* 'do' expression evalutes each succeeding expression returning the result of the last one.
 * (do expr ...)
 * return expr.map(eval).last
 */
static LispDatum *eval_do(const List *list, MalEnv *env) {
    int argc = List_len(list) - 1;
    if (argc == 0) {
        BADSTX("do expects at least 1 argument");
        return NULL;
    }

    struct Node *node;
    for (node = list->head->next; node->next != NULL; node = node->next) {
        LispDatum *ev = eval(node->value, env);
        if (ev == NULL) return NULL;
        FREE(ev);
        LispDatum_free(ev);
    }

    return eval(node->value, env);
}

/* 'lambda' expression is like the 'lambda' expression, it creates and returns a function
 * it comes in 2 forms:
 * 1. (lambda params body)
 * params := () | (param ...)
 * param := SYMBOL
 * 2. (lambda var-params body)
 * var-params := (& rest) | (param ... & rest)
 * rest is then bound to the list of the remaining arguments
 */
static LispDatum *eval_fnstar(const List *list, MalEnv *env) {
    int argc = List_len(list) - 1;
    if (argc < 2) {
        BADSTX("lambda: cannot have empty body");
        return NULL;
    }

    // 1. validate params
    List *params;
    {
        LispDatum *snd = List_ref(list, 1);
        if (!LispDatum_istype(snd, LIST)) {
            BADSTX("lambda: bad syntax at parameter declaration");
            return NULL;
        }
        params = (List*) snd;
    }

    // all parameters should be symbols
    for (struct Node *node = params->head; node != NULL; node = node->next) {
        LispDatum *par = node->value;
        if (!LispDatum_istype(par, SYMBOL)) {
            BADSTX("lambda bad parameter list: expected a list of symbols, but %s was found in the list",
                    LispType_name(LispDatum_type(par)));
            return NULL;
        }
    }

    size_t proc_argc = 0; // mandatory arg count
    bool variadic = false;
    Arr *param_names_symbols = Arr_newn(List_len(params)); // of Symbol*
    OWN(param_names_symbols);

    for (struct Node *node = params->head; node != NULL; node = node->next) {
        Symbol *sym = (Symbol*) node->value;

        // '&' is a special symbol that marks a variadic procedure
        // exactly one parameter is expected after it
        // NOTE: we allow that parameter to also be named '&'
        if (Symbol_eq_str(sym, "&")) {
            if (node->next == NULL || node->next->next != NULL) {
                BADSTX("lambda bad parameter list: 1 parameter expected after '&'");
                return NULL;
            }
            Symbol *last_dtm_sym = (Symbol*) node->next->value;
            Arr_add(param_names_symbols, last_dtm_sym); // no need to copy
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
    // TODO replace by List_slice
    List *body = List_new();
    for (struct Node *node = list->head->next->next; node; node = node->next) {
        List_add(body, node->value);
    }

    Proc *proc = Proc_new_lambda(proc_argc, variadic, param_names_symbols, body, env);

    return (LispDatum*) proc;
}

// (def! id <LispDatum>)
static LispDatum *eval_def(const List *list, MalEnv *env) {
    int argc = List_len(list) - 1;
    if (argc != 2) {
        BADSTX("def! expects 2 arguments, but %d were given", argc);
        return NULL;
    }

    LispDatum *snd = List_ref(list, 1);
    if (LispDatum_type(snd) != SYMBOL) {
        BADSTX("def! expects a symbol as a 2nd argument, but %s was given",
                LispType_name(LispDatum_type(snd)));
        return NULL;
    }
    Symbol *id = (Symbol*) snd;

    LispDatum *new_assoc = eval(List_ref(list, 2), env);
    if (new_assoc == NULL) {
        return NULL;
    }

    // if id is being bound to an unnamed procedure, then set id as its name
    if (LispDatum_istype(new_assoc, PROCEDURE)) {
        Proc *proc = (Proc*) new_assoc;
        if (!Proc_isnamed(proc))
            Proc_set_name(proc, id);
    }

    MalEnv_put(env, id, new_assoc);

    return new_assoc;
}

// (defmacro! id <lambda-expr>)
static LispDatum *eval_defmacro(const List *list, MalEnv *env) {
    size_t argc = List_len(list) - 1;
    if (argc != 2) {
        BADSTX("defmacro! expects 2 arguments, but %zu were given", argc);
        return NULL;
    }

    LispDatum *arg1 = List_ref(list, 1);
    if (!(LispDatum_istype(arg1, SYMBOL))) {
        BADSTX("defmacro!: 1st arg must be a symbol, but was %s", LispType_name(LispDatum_type(arg1)));
        return NULL;
    }
    Symbol *id = (Symbol*) arg1;

    LispDatum *macro_datum = NULL;
    {
        LispDatum *arg2 = List_ref(list, 2);
        if (!LispDatum_istype(arg2, LIST)) {
            BADSTX("defmacro!: 2nd arg must be an lambda expression");
            return NULL;
        }

        const List *arg2_list = (List*) arg2;
        if (List_isempty(arg2_list)) {
            BADSTX("defmacro!: 2nd arg must be an lambda expression");
            return NULL;
        }
        const LispDatum *arg2_list_ref0 = List_ref(arg2_list, 0);
        if (!LispDatum_istype(arg2_list_ref0, SYMBOL)) {
            BADSTX("defmacro!: 2nd arg must be an lambda expression");
            return NULL;
        }
        const Symbol *sym = (Symbol*) arg2_list_ref0;
        if (!Symbol_eq_str(sym, "lambda")) {
            BADSTX("defmacro!: 2nd arg must be an lambda expression");
            return NULL;
        }
        LispDatum *evaled = eval(arg2, env);
        if (!evaled) 
            return NULL;
        if (!LispDatum_istype(evaled, PROCEDURE)) {
            LispDatum_free(evaled);
            BADSTX("defmacro!: 2nd arg must evaluate to a procedure");
            return NULL;
        }
        macro_datum = evaled;
    }

    if (!macro_datum) 
        return NULL;

    Proc_set_macro((Proc*) macro_datum);

    MalEnv_put(env, id, macro_datum);

    return macro_datum;
}

/* (let* (bindings) expr) 
 * bindings := ((id val) ...)
 */
static LispDatum *eval_letstar(const List *list, MalEnv *env) {
    // 1. validate the list
    int argc = List_len(list) - 1;
    if (argc != 2) {
        BADSTX("let* expects 2 arguments, but %d were given", argc);
        return NULL;
    }

    LispDatum *snd = List_ref(list, 1);
    if (LispDatum_type(snd) != LIST) {
        BADSTX("let* expects a list as a 2nd argument, but %s was given",
                LispType_name(LispDatum_type(snd)));
        return NULL;
    }

    List *bindings = (List*) snd;
    if (List_isempty(bindings)) {
        BADSTX("let* expects a non-empty list of bindings");
        return NULL;
    }

    LispDatum *expr = List_ref(list, 2);

    // 2. initialise the let* environment 
    MalEnv *let_env = MalEnv_new(env);
    OWN(let_env);
    // step = 2
    for (struct Node *bind_node = bindings->head; bind_node; bind_node = bind_node->next) {
        const LispDatum *dtm = bind_node->value;
        // bind_node must be a List
        if (!LispDatum_istype(dtm, LIST)) {
            BADSTX("let*: expected a list of bindings");
            FREE(let_env);
            MalEnv_free(let_env);
            return NULL;
        }
        List *bind = (List*) dtm;
        // must be a list of length 2
        if (List_len(bind) != 2) { 
            char *s = pr_list(bind, true);
            BADSTX("let*: bad binding form: %s", s);
            free(s);
            FREE(let_env);
            MalEnv_free(let_env);
            return NULL;
        }
        // must be headed by a symbol
        LispDatum *bind_sym = List_ref(bind, 0);
        if (!LispDatum_istype(bind_sym, SYMBOL)) {
            BADSTX("let*: bad binding form (expected a symbol to be bound, but was %s)", 
                    LispType_name(LispDatum_type(bind_sym)));
            FREE(let_env);
            MalEnv_free(let_env);
            return NULL;
        }

        // it's important to evaluate the bound value using the let* env,
        // so that previous bindings can be used during evaluation
        LispDatum *val = eval(List_ref(bind, 1), let_env);
        OWN(val);
        if (val == NULL) {
            FREE(let_env);
            MalEnv_free(let_env);
            return NULL;
        }

        MalEnv_put(let_env, (Symbol*) bind_sym, val);
    }

    // 3. evaluate the expr using the let* env
    LispDatum *out = eval(expr, let_env);

    // this is a hack
    // if the returned value was computed in let* bindings,
    // then we don't want it to be freed when we free the let_env,
    // so we increment its ref count only to decrement it after let_env is freed
    if (out)
        LispDatum_own(out);

    // discard the let* env
    FREE(let_env);
    MalEnv_free(let_env);

    // the hack cont.
    if (out)
        LispDatum_rls(out);

    return out;
}

// quote : this special form returns its argument without evaluating it
static LispDatum *eval_quote(const List *list, MalEnv *env) {
    size_t argc = List_len(list) - 1;
    if (argc != 1) {
        BADSTX("quote expects 1 argument, but %zd were given", argc);
        return NULL;
    }

    LispDatum *arg1 = List_ref(list, 1);
    return arg1;
}

// helper function for eval_quasiquote_list
static LispDatum *eval_unquote(const List *list, MalEnv *env) 
{
    size_t argc = List_len(list) - 1;
    if (argc != 1) {
        BADSTX("unquote expects 1 argument, but %zd were given", argc);
        return NULL;
    }

    LispDatum *arg1 = List_ref(list, 1);
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

    LispDatum *arg1 = List_ref(list, 1);
    LispDatum *evaled = eval(arg1, env);
    if (!evaled)
        return NULL;

    if (!LispDatum_istype(evaled, LIST)) {
        BADSTX("splice-unquote: resulting value must be a list, but was %s",
                LispType_name(LispDatum_type(evaled)));
        LispDatum_free(evaled);
        return NULL;
    }
    else {
        return (List*) evaled;
    }
}

// helper function for eval_quasiquote
static LispDatum *eval_quasiquote_list(const List *list, MalEnv *env, bool *splice)
{
    if (List_isempty(list)) 
        return (LispDatum*) List_empty();

    LispDatum *ref0 = List_ref(list, 0);
    if (LispDatum_istype(ref0, SYMBOL)) {
        const Symbol *sym = (Symbol*) ref0;

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
                return (LispDatum*) evaled;
            }
        }
    }

    List *out_list = List_new();

    for (struct Node *node = list->head; node != NULL; node = node->next) {
        LispDatum *dtm = node->value;

        if (LispDatum_istype(dtm, LIST)) { // recurse
            bool _splice = false;
            LispDatum *evaled = eval_quasiquote_list((List*) dtm, env, &_splice);
            if (!evaled) {
                List_free(out_list);
                return NULL;
            }
            if (_splice) {
                List_append(out_list, (List*) evaled);
            }
            else {
                List_add(out_list, evaled);
            }
        }
        else { // not a list
            List_add(out_list, dtm);
        }
    }

    return (LispDatum*) out_list;
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
static LispDatum *eval_quasiquote(const List *list, MalEnv *env) 
{
    size_t argc = List_len(list) - 1;
    if (argc != 1) {
        BADSTX("quasiquote expects 1 argument, but %zd were given", argc);
        return NULL;
    }

    LispDatum *ast = List_ref(list, 1);
    if (!LispDatum_istype(ast, LIST))
        return ast;

    const List *ast_list = (List*) ast;
    if (List_isempty(ast_list)) 
        return ast;

    // splice-unquote may only appear in an enclosing list form
    LispDatum *ast0 = List_ref(ast_list, 0);
    if (LispDatum_istype(ast0, SYMBOL)) {
        const Symbol *sym = (Symbol*) ast0;
        if (Symbol_eq_str(sym, "splice-unquote")) {
            BADSTX("splice-unquote: illegal context within quasiquote (nothing to splice into)");
            return NULL;
        }
    }

    LispDatum *out = eval_quasiquote_list(ast_list, env, NULL);
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
        LispDatum *evaled = eval(node->value, env);
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

LispDatum *eval_ast(const LispDatum *datum, MalEnv *env) {
    LispDatum *out = NULL;

    switch (LispDatum_type(datum)) {
        case SYMBOL:
            Symbol *sym = (Symbol*) datum;
            LispDatum *assoc = MalEnv_get(env, sym);
            if (assoc == NULL) {
                throwf(NULL, "symbol binding '%s' not found", Symbol_name(sym));
            } else {
                out = assoc;;
            }
            break;
        case LIST:
            List *elist = eval_list((List*) datum, env);
            if (elist == NULL) {
                LOG_NULL(elist);
            } else {
                out = (LispDatum*) elist;
            }
            break;
        default:
            // STRING | INT
            out = LispDatum_copy(datum);
            break;
    }

    return out;
}

static LispDatum *macroexpand_single(LispDatum *ast, MalEnv *env)
{
    if (!LispDatum_istype(ast, LIST)) return ast;

    List *ast_list = (List*) ast;
    if (List_isempty(ast_list)) return ast;

    // this is a macro call if the first list element is a symbol that's bound to a macro procedure
    const Proc *macro = NULL;
    {
        LispDatum *ref0 = List_ref(ast_list, 0);
        if (!LispDatum_istype(ref0, SYMBOL)) 
            return ast;

        const LispDatum *datum = MalEnv_get(env, (Symbol*) ref0);
        if (datum && LispDatum_istype(datum, PROCEDURE)) {
            const Proc *proc = (Proc*) datum;
            if (!Proc_ismacro(proc)) 
                return ast;
            else 
                macro = proc; 
        }
        else return ast;
    }

    Arr *args = Arr_newn(List_len(ast_list) - 1);
    for (struct Node *node = ast_list->head->next; node != NULL; node = node->next) {
        Arr_add(args, node->value);
    }

    LispDatum *out = apply_proc(macro, args, env);

    if (out) 
        LispDatum_own(out); // hack own
    Arr_free(args);
    if (out) 
        LispDatum_rls(out); // hack release

    return out;
}

static LispDatum *macroexpand(LispDatum *ast, MalEnv *env)
{
    LispDatum *out = ast;

    while (1) {
        LispDatum *expanded = macroexpand_single(out, env);
        if (!expanded) return NULL;
        else if (expanded == out) return out;
        else out = expanded;
    }

    return out;
}

// 'macroexpand' special form
static LispDatum *eval_macroexpand(List *ast_list, MalEnv *env)
{
    size_t argc = List_len(ast_list) - 1;
    if (argc != 1) {
        BADSTX("macroexpand expects 1 argument, but %zu were given", argc);
        return NULL;
    }

    LispDatum *arg1 = List_ref(ast_list, 1);
    return macroexpand(arg1, env);
}

// 'try*' special form
// (try* <expr1> (catch* <symbol> <expr2>))
// if <expr1> throws an exception, then the exception is bound to <symbol> 
// and <expr2> is evaluated
static LispDatum *eval_try_star(List *ast_list, MalEnv *env)
{
    size_t argc = List_len(ast_list) - 1;
    if (argc != 2) {
        BADSTX("try* expects 2 arguments, but %zu were given", argc);
        return NULL;
    }

    LispDatum *expr1 = List_ref(ast_list, 1);
    LispDatum *catch_form = List_ref(ast_list, 2);
    // validate catch_form
    LispDatum *catch_sym = 0;
    LispDatum *expr2 = NULL;
    {
        if (!LispDatum_istype(catch_form, LIST)) {
            BADSTX("try* expects (catch* SYMBOL EXPR) as 2nd arg");
            return NULL;
        }
        List *catch_list = (List*) catch_form;

        if (List_len(catch_list) != 3) {
            BADSTX("try* expects (catch* SYMBOL EXPR) as 2nd arg");
            return NULL;
        }

        // validate (catch* ...)
        LispDatum *catch0 = List_ref(catch_list, 0);
        if (!LispDatum_istype(catch0, SYMBOL) 
                || !Symbol_eq_str((Symbol*) catch0, "catch*")) 
        {
            BADSTX("try* expects (catch* SYMBOL EXPR) as 2nd arg");
            return NULL;
        }

        catch_sym = List_ref(catch_list, 1);
        if (!LispDatum_istype(catch_sym, SYMBOL)) {
            BADSTX("try* expects (catch* SYMBOL EXPR) as 2nd arg");
            return NULL;
        }

        expr2 = List_ref(catch_list, 2);
    }

    LispDatum *expr1_rslt = eval(expr1, env);
    if (expr1_rslt == NULL && didthrow()) {
        MalEnv *catch_env = MalEnv_new(env);
        Exception *exn = thrown_copy();
        MalEnv_put(catch_env, (Symbol*) catch_sym, (LispDatum*) exn);

        LispDatum *expr2_rslt = eval(expr2, catch_env);

        if (expr2_rslt)
            LispDatum_own(expr2_rslt); // hack own
        MalEnv_free(catch_env);
        if (expr2_rslt) 
            LispDatum_rls(expr2_rslt); // hack release

        return expr2_rslt;
    }
    else {
        return expr1_rslt;
    }
}

#ifdef EVAL_STACK_DEPTH
static int eval_stack_depth = 0; 
#endif

LispDatum *eval(LispDatum *ast, MalEnv *env) {
#ifdef EVAL_STACK_DEPTH
    eval_stack_depth++;
    printf("ENTER eval, stack depth: %d\n", eval_stack_depth);
#endif
    // we create a new environment for each procedure application to bind params to args
    // because of TCO, ast might be the last body part of a procedure, so we might need 
    // apply_env created in the previous loop cycle to evaluate ast
    MalEnv *apply_env = env;
    LispDatum *out = NULL;

    while (ast) {
        if (LispDatum_istype(ast, LIST)) {
            LispDatum *expanded = macroexpand(ast, env);
            if (!expanded) break;
            else if (expanded != ast && !LispDatum_istype(expanded, LIST)) {
                out = eval_ast(expanded, env);
                break;
            }
            else {
                // expanded == ast OR expanded is a list
                ast = expanded;
            }

            List *ast_list = (List*) ast;
            if (List_isempty(ast_list)) {
                BADSTX("empty application ()");
                out = NULL;
                break;
            }

            LispDatum *head = List_ref(ast_list, 0);
            // handle special forms: def!, let*, if, do, lambda, quote, quasiquote,
            // defmacro!, macroexpand, try*/catch*
            if (LispDatum_istype(head, SYMBOL)) {
                const Symbol *sym = (Symbol*) head;
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
                else if (Symbol_eq_str(sym, "lambda")) {
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
            LispDatum *first = List_ref(evaled_list, 0);
            if (!LispDatum_istype(first, PROCEDURE)) {
                throwf(NULL, "application: expected a procedure");
                out = NULL;
                FREE(evaled_list);
                List_free(evaled_list);
                break;
            }

            Proc *proc = (Proc*) first;

            Arr *args = Arr_newn(List_len(evaled_list) - 1);
            OWN(args);
            for (struct Node *node = evaled_list->head->next; node != NULL; node = node->next) {
                Arr_add(args, node->value);
                // LispDatum_own(node->value); // hold onto argument values
            }

            // previous application's env is no longer needed after we have argument values
            if (apply_env != env) {
                MalEnv_free(apply_env);
                apply_env = NULL;
            }

            // 3. apply TCO only if it's a non-lambda MAL procedure
            if (!proc->builtin && Proc_isnamed(proc)) {
                // args will be put into apply_env
                apply_env = MalEnv_new(proc->env);
                ast = eval_application_tco(proc, args, apply_env);

                // release and free args
                FREE(args);
                // Arr_freep(args, (free_t) LispDatum_rls_free);
                Arr_free(args);

                FREE(evaled_list);
                List_free(evaled_list);
            }
            else {
                // 4. otherwise just return the result of procedure application
                // builtin procedures do not get TCO
                // unnamed procedures cannot be called recursively apriori
                out = apply_proc(proc, args, env);
                if (out) 
                    LispDatum_own(out); // hack own

                FREE(args);
                // Arr_freep(args, (free_t) LispDatum_rls_free);
                Arr_free(args);

                FREE(evaled_list);
                List_free(evaled_list);

                if (out) 
                    LispDatum_rls(out); // hack release
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
        if (out) 
            LispDatum_own(out); // hack own

        FREE(apply_env);
        MalEnv_free(apply_env);

        if (out) 
            LispDatum_rls(out); // hack release
    }

#ifdef EVAL_STACK_DEPTH
    eval_stack_depth--;
    printf("LEAVE eval, stack depth: %d\n", eval_stack_depth);
#endif

    return out;
}

static char *print(LispDatum *datum) {
    if (datum == NULL) {
        return NULL;
    }

    char *str = pr_str(datum, true);
    return str;
}

static void rep(const char *str, MalEnv *env) {
    // read
    LispDatum *r = read(str);
    if (r == NULL) return;

    // eval
    // TODO implement a stack trace of error messages
    LispDatum *e = eval(r, env);
    if (!e) return;
    LispDatum_own(e); // prevent from being freed before printing

    LispDatum_free(r);

    // print
    char *p = print(e);
    if (p != NULL) { 
        printf("%s\n", p);
        free(p);
    }

    // the evaled value can be either discarded (e.g., (+ 1 2) => 3)
    // or owned by something (e.g., (def! x 5) => 5)
    LispDatum_rls_free(e);
}

// TODO reorganise file structure and move to core.c
/* apply : applies a procedure to the list of arguments 
 * (apply proc <interm> arg-list) 
 * if <interm> (intermediate arguments) are present, they are simply consed onto arg-list;
 * for example: (apply f a b '(c d)) <=> (apply f '(a b c d))
 * */
static LispDatum *lisp_apply(const Proc *proc, const Arr *args, MalEnv *env)
{
    const Proc *f = verify_proc_arg_type(proc, args, 0, PROCEDURE);
    if (!f) return NULL;

    const LispDatum *arg_last = Arr_last(args);
    if (!LispDatum_istype(arg_last, LIST)) {
        throwf("apply", "bad last arg: expected a list");
        return NULL;
    }
    const List *arg_list = (List*) arg_last;

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

    LispDatum *rslt = apply_proc(f, args_arr, env);

    FREE(args_arr);
    Arr_free(args_arr);

    return rslt;
}

// read-string : takes a Mal string and reads it as if it were entered into the prompt,
// transforming it into a raw AST. Essentially, exposes the internal READ function
static LispDatum *lisp_read_string(const Proc *proc, const Arr *args, MalEnv *env) 
{
    String *string = verify_proc_arg_type(proc, args, 0, STRING);
    if (!string) return NULL;

    const char *str = String_str(string);
    LispDatum *ast = read(str);

    if (ast == NULL) {
        throwf("read-string", "could not parse bad syntax");
        return NULL;
    }

    return ast;
}

// slurp : takes a file name (string) and returns the contents of the file as a string
static LispDatum *lisp_slurp(const Proc *proc, const Arr *args, MalEnv *env) 
{
    String *string = verify_proc_arg_type(proc, args, 0, STRING);
    if (!string) return NULL;

    const char *path = String_str(string);
    if (!file_readable(path)) {
        throwf("slurp", "can't read file %s", path);
        return NULL;
    }

    char *contents = file_to_str(path);
    if (!contents) {
        throwf("slurp", "failed to read file %s", path);
        return NULL;
    }

    String *out = String_new(contents);
    free(contents);

    return (LispDatum*) out;
}

// eval : takes an AST and evaluates it in the top-level environment
// local environments are not taken into account by eval
static LispDatum *lisp_eval(const Proc *proc, const Arr *args, MalEnv *env) 
{
    LispDatum *arg0 = Arr_get(args, 0);
    MalEnv *top_env = MalEnv_enclosing_root(env);
    return eval(arg0, top_env);
}

// swap! : Takes an atom, a function, and zero or more function arguments. The
// atom's value is modified to the result of applying the function with the atom's
// value as the first argument and the optionally given function arguments as the
// rest of the arguments. The new atom's value is returned.
static LispDatum *lisp_swap_bang(const Proc *proc, const Arr *args, MalEnv *env) {
    Atom *atom = verify_proc_arg_type(proc, args, 0, ATOM);
    if (!atom) return NULL;

    const Proc *applied_proc = verify_proc_arg_type(proc, args, 1, PROCEDURE);
    if (!applied_proc) return NULL;

    Arr *proc_args = Arr_newn(1 + args->len - 2); // of *LispDatum
    OWN(proc_args);

    // use atom's value as the 1st argument 
    Arr_add(proc_args, Atom_deref(atom));

    for (size_t i = 2; i < args->len; i++) {
        Arr_add(proc_args, args->items[i]);
    }

    LispDatum *rslt = NULL;

    if (verify_proc_application(applied_proc, proc_args)) {
        rslt = apply_proc(applied_proc, proc_args, env);
        Atom_set(atom, rslt);
    }

    FREE(proc_args);
    Arr_free(proc_args);

    return rslt;
}

// map : maps over a list/vector using a procedure
// TODO accept multiple lists/vectors
static LispDatum *lisp_map(const Proc *proc, const Arr *args, MalEnv *env) 
{
    Proc *mapper = verify_proc_arg_type(proc, args, 0, PROCEDURE);
    if (!mapper) return NULL;

    List *list = verify_proc_arg_type(proc, args, 1, LIST);
    if (!list) return NULL;

    if (List_isempty(list)) {
        return (LispDatum*) List_empty();
    }

    List *out = List_new();
    // args to mapper proc
    Arr *mapper_args = Arr_newn(1);
    Arr_add(mapper_args, NULL); // to increase length to 1

    for (struct Node *node = list->head; node != NULL; node = node->next) {
        LispDatum *list_elt = node->value;

        Arr_replace(mapper_args, 0, list_elt);
        LispDatum *new_elt = apply_proc(mapper, mapper_args, env);
        if (!new_elt) {
            List_free(out);
            Arr_free(mapper_args);
            return NULL;
        }

        List_add(out, new_elt);
    }

    Arr_free(mapper_args);

    return (LispDatum*) out;
}

int main(int argc, char **argv) {
    init_symbol_table();

    MalEnv *env = MalEnv_new(NULL);
    OWN(env);
    MalEnv_own(env);

    MalEnv_put(env, Symbol_intern("nil"),   (LispDatum*) Nil_get());
    MalEnv_put(env, Symbol_intern("true"),  (LispDatum*) True_get());
    MalEnv_put(env, Symbol_intern("false"), (LispDatum*) False_get());

#define ENV_PUT_PROC(name, arity, variadic, funp) \
    { \
        Symbol *_s = Symbol_intern(name); \
        MalEnv_put(env, _s, (LispDatum*) Proc_builtin(_s, arity, variadic, funp)); \
    }

    ENV_PUT_PROC("apply", 2, true, lisp_apply);
    ENV_PUT_PROC("read-string", 1, false, lisp_read_string);
    ENV_PUT_PROC("slurp", 1, false, lisp_slurp);
    ENV_PUT_PROC("eval", 1, false, lisp_eval);
    ENV_PUT_PROC("swap!", 2, true, lisp_swap_bang);
    ENV_PUT_PROC("map", 2, false, lisp_map);

    core_def_procs(env);

    rep("(def! load-file\n"
            // closing paren of 'do' must be on a separate line in case a file ends
            // with a comment without a newline at the end
            "(lambda (path) (eval (read-string (str \"(do \" (slurp path) \"\n)\")))\n"
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
