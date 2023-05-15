#include <stdlib.h>

#include "common.h"
#include "types.h"
#include "utils.h"
#include "core.h"
#include "env.h"
#include "printer.h"
#include "mem_debug.h"


void *verify_proc_arg_type(const Proc *proc, const Arr *args, size_t arg_idx, 
        LispType expect_type)
{
    LispDatum *arg = Arr_get(args, arg_idx);
    if (!LispDatum_istype(arg, expect_type)) {
        const char *proc_name = Symbol_name(Proc_name(proc));
        throwf(proc_name, "bad arg no. %zd: expected a %s", 
               arg_idx + 1, LispType_name(expect_type));
        return NULL;
    }

    return (void*) arg;
}

static LispDatum *lisp_add(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg types
    for (size_t i = 0; i < args->len; i++) {
        if (!verify_proc_arg_type(proc, args, i, NUMBER)) 
            return NULL;
    }

    Number *arg1 = args->items[0];
    Number *sum = Number_true_copy(arg1);
    for (size_t i = 1; i < args->len; i++) {
        Number *arg = args->items[i];
        Number_add(sum, arg);
    }

    return (LispDatum*) sum;
}

static LispDatum *lisp_sub(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg types
    for (size_t i = 0; i < args->len; i++) {
        if (!verify_proc_arg_type(proc, args, i, NUMBER)) 
            return NULL;
    }

    Number *arg1 = args->items[0];
    Number *rslt = Number_true_copy(arg1);
    for (size_t i = 1; i < args->len; i++) {
        Number *arg = args->items[i];
        Number_sub(rslt, arg);
    }

    return (LispDatum*) rslt;
}

static LispDatum *lisp_mul(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg types
    for (size_t i = 0; i < args->len; i++) {
        if (!verify_proc_arg_type(proc, args, i, NUMBER)) 
            return NULL;
    }

    Number *arg1 = args->items[0];
    Number *rslt = Number_true_copy(arg1);
    for (size_t i = 1; i < args->len; i++) {
        Number *arg = args->items[i];
        Number_mul(rslt, arg);
    }

    return (LispDatum*) rslt;
}

static LispDatum *lisp_div(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg types
    for (size_t i = 0; i < args->len; i++) {
        if (!verify_proc_arg_type(proc, args, i, NUMBER)) 
            return NULL;
    }

    Number *arg1 = args->items[0];
    Number *rslt = Number_true_copy(arg1);
    for (size_t i = 1; i < args->len; i++) {
        Number *arg = args->items[i];
        Number_div(rslt, arg);
    }

    return (LispDatum*) rslt;
}

/* '=' : compare the first two parameters and return true if they are the same type
 * and contain the same value. In the case of equal length lists, each element
 * of the list should be compared for equality and if they are the same return
 * true, otherwise false
 */
static LispDatum *lisp_eq(const Proc *proc, const Arr *args, MalEnv *env) {
    const LispDatum *arg1 = args->items[0];
    const LispDatum *arg2 = args->items[1];

    return (LispDatum*) LispDatum_bool(LispDatum_eq(arg1, arg2));
}

/* '>' : compare the first two numeric parameters */
static LispDatum *lisp_gt(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        if (!verify_proc_arg_type(proc, args, i, NUMBER)) 
            return NULL;
    }

    const Number *arg1 = args->items[0];
    const Number *arg2 = args->items[1];

    return (LispDatum*) LispDatum_bool(Number_cmp(arg1, arg2) > 0);
}

/* % : modulus */
static LispDatum *lisp_mod(const Proc *proc, const Arr *args, MalEnv *env) 
{
    const Number *arg0 = verify_proc_arg_type(proc, args, 0, NUMBER);
    if (!arg0) return NULL;
    const Number *arg1 = verify_proc_arg_type(proc, args, 1, NUMBER);
    if (!arg1) return NULL;

    Number *rslt = Number_true_copy(arg0);
    Number_mod(rslt, arg1);

    return (LispDatum*) rslt;
}

/* even? */
static LispDatum *lisp_evenp(const Proc *proc, const Arr *args, MalEnv *env) 
{
    const Number *arg0 = verify_proc_arg_type(proc, args, 0, NUMBER);
    if (!arg0) return NULL;

    return (LispDatum*) LispDatum_bool(Number_iseven(arg0));
}

// number?
static LispDatum *lisp_numberp(const Proc *proc, const Arr *args, MalEnv *env) 
{
    const LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) LispDatum_bool(LispDatum_istype(arg0, NUMBER));
}

// symbol : string to symbol 
static LispDatum *lisp_symbol(const Proc *proc, const Arr *args, MalEnv *env) 
{
    const String *arg0 = verify_proc_arg_type(proc, args, 0, STRING);
    if (!arg0) return NULL;

    return (LispDatum*) Symbol_intern(String_str(arg0));
}

// symbol?
static LispDatum *lisp_symbolp(const Proc *proc, const Arr *args, MalEnv *env) {
    const LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) LispDatum_bool(LispDatum_istype(arg0, SYMBOL));
}

// string?
static LispDatum *lisp_stringp(const Proc *proc, const Arr *args, MalEnv *env) {
    const LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) LispDatum_bool(LispDatum_istype(arg0, STRING));
}

// true?
static LispDatum *lisp_truep(const Proc *proc, const Arr *args, MalEnv *env) {
    const LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) LispDatum_bool(LispDatum_istype(arg0, TRUE));
}

// false?
static LispDatum *lisp_falsep(const Proc *proc, const Arr *args, MalEnv *env) {
    const LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) LispDatum_bool(LispDatum_istype(arg0, FALSE));
}

// list?
static LispDatum *lisp_listp(const Proc *proc, const Arr *args, MalEnv *env) {
    const LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) LispDatum_bool(LispDatum_istype(arg0, LIST));
}

// list
static LispDatum *lisp_list(const Proc *proc, const Arr *args, MalEnv *env) {
    if (args->len == 0)
        return (LispDatum*) List_empty();

    List *list = List_new();
    for (size_t i = 0; i < args->len; i++) {
        LispDatum *dtm = args->items[i];
        List_add(list, dtm);
    }

    return (LispDatum*) list;
}

// empty?
static LispDatum *lisp_emptyp(const Proc *proc, const Arr *args, MalEnv *env) {
    const List *list = verify_proc_arg_type(proc, args, 0, LIST);
    if (!list) return NULL;
    return (LispDatum*) LispDatum_bool(List_isempty(list));
}

// TODO replace with length
// static LispDatum *lisp_count(const Proc *proc, const Arr *args, MalEnv *env) {
//     // validate arg type
//     LispDatum *arg = args->items[0];

//     int len;
//     if (LispDatum_istype(arg, NIL))
//         len = 0;
//     else if (LispDatum_islist(arg))
//         len = List_len(arg->value.list);
//     else {
//         throwf("count", "expected a list, but got %s instead", LispType_tostr(arg->type));
//         return NULL;
//     }

//     return LispDatum_new_int(len);
// }

static LispDatum *lisp_list_ref(const Proc *proc, const Arr *args, MalEnv *env) 
{
    const List *list = verify_proc_arg_type(proc, args, 0, LIST);
    if (!list) return NULL;
    const Number *idx = verify_proc_arg_type(proc, args, 1, NUMBER);
    if (!idx) return NULL;

    if (Number_isneg(idx)) {
        throwf("list-ref", "expected non-negative index");
        return NULL;
    }

    size_t list_len = List_len(list);
    if (Number_cmpl(idx, list_len) >= 0) {
        char *s = Number_tostr(idx);
        throwf("list-ref", "index too large (%s >= %zu)", s, list_len);
        free(s);
        return NULL;
    }

    return List_ref(list, Number_tol(idx));
}

static LispDatum *lisp_list_rest(const Proc *proc, const Arr *args, MalEnv *env) 
{
    List *list = verify_proc_arg_type(proc, args, 0, LIST);
    if (!list) return NULL;

    if (List_isempty(list)) {
        throwf("list-rest", "received an empty list");
        return NULL;
    }

    return (LispDatum*) List_rest_new(list);
}

// nth : takes a list (or vector) and a number (index) as arguments, returns
// the element of the list/vector at the given index. If the index is out of range,
// then an error is raised.
static LispDatum *lisp_nth(const Proc *proc, const Arr *args, MalEnv *env) 
{
    LispDatum *arg0 = Arr_get(args, 0);
    if (LispDatum_istype(arg0, LIST)) {
        return lisp_list_ref(proc, args, env);
    }
    // else if (LispDatum_istype(arg0, VECTOR)) {
    //     return lisp_vec_ref(proc, args, env);
    // }
    else {
        throwf("nth", "bad 1st arg: expected LIST or VECTOR, but was %s",
                LispType_name(LispDatum_type(arg0)));
        return NULL;
    }
}

// rest : takes a list (or vector) as its argument and returns a new list/vector
//     containing all the elements except the first. If the list/vector is empty
//     empty then an error is raised.
static LispDatum *lisp_rest(const Proc *proc, const Arr *args, MalEnv *env) 
{
    LispDatum *arg0 = Arr_get(args, 0);
    if (LispDatum_istype(arg0, LIST)) {
        return lisp_list_rest(proc, args, env);
    }
    // else if (LispDatum_istype(arg0, VECTOR)) {
    //     return lisp_vec_rest(proc, args, env);
    // }
    else {
        throwf("rest", "bad 1st arg: expected LIST or VECTOR, but was %s",
                LispType_name(LispDatum_type(arg0)));
        return NULL;
    }
}

// prn: calls pr_str on each argument with print_readably set to true, joins the
// results with " ", prints the string to the screen and then returns nil.
static LispDatum *lisp_prn(const Proc *proc, const Arr *args, MalEnv *env)
{
    if (args->len > 0) {
        char *strings[args->len];

        for (size_t i = 0; i < args->len; i++) {
            LispDatum *arg = args->items[i];
            strings[i] = pr_str(arg, true);
        }

        // join with " "
        char *joined = str_join(strings, args->len, " ");
        printf("%s\n", joined);
        free(joined);

        for (size_t i = 0; i < args->len; i++)
            free(strings[i]);
    }

    return (LispDatum*) Nil_get();
}

// pr-str: calls pr_str on each argument with print_readably set to true, joins
// the results with " " and returns the new string.
static LispDatum *lisp_pr_str(const Proc *proc, const Arr *args, MalEnv *env)
{
    if (args->len == 0) {
        return (LispDatum*) String_new("");
    }
    else {
        char *strings[args->len];

        for (size_t i = 0; i < args->len; i++) {
            LispDatum *arg = args->items[i];
            strings[i] = pr_str(arg, true);
        }

        // join with " "
        char *joined = str_join(strings, args->len, " ");

        for (size_t i = 0; i < args->len; i++)
            free(strings[i]);

        // TODO optimize: add function String_from that does not copy the input string
        LispDatum *out = (LispDatum*) String_new(joined);
        free(joined);
        return out;
    }
}

/*
 * str: calls pr_str on each argument with print_readably set to false,
 * concatenates the results together ("" separator), and returns the new
 * string.
 */
static LispDatum *lisp_str(const Proc *proc, const Arr *args, MalEnv *env)
{
    if (args->len == 0) {
        return (LispDatum*) String_new("");
    }
    else {
        char *strings[args->len];

        for (size_t i = 0; i < args->len; i++) {
            LispDatum *arg = args->items[i];
            strings[i] = pr_str(arg, false);
        }

        char *joined = str_join(strings, args->len, "");

        for (size_t i = 0; i < args->len; i++)
            free(strings[i]);

        // TODO optimize: add function String_from that does not copy the input string
        LispDatum *out = (LispDatum*) String_new(joined);
        free(joined);
        return out;
    }
}

/*
 * println: calls pr_str on each argument with print_readably set to false,
 * joins the results with " ", prints the string to the screen and then returns
 * nil.
 */
static LispDatum *lisp_println(const Proc *proc, const Arr *args, MalEnv *env)
{
    if (args->len > 0) {
        char *strings[args->len];

        for (size_t i = 0; i < args->len; i++) {
            LispDatum *arg = args->items[i];
            strings[i] = pr_str(arg, false);
        }

        char *joined = str_join(strings, args->len, " ");
        printf("%s\n", joined);
        free(joined);

        for (size_t i = 0; i < args->len; i++)
            free(strings[i]);
    }

    return (LispDatum*) Nil_get();
}

/* (procedure? datum) : predicate for procedures */
static LispDatum *lisp_procedurep(const Proc *proc, const Arr *args, MalEnv *env) {
    const LispDatum *arg = Arr_get(args, 0);
    return (LispDatum*) LispDatum_bool(LispDatum_istype(arg, PROCEDURE));
}

/* (arity proc) : returns a list of 2 elements:
 * 1. number of mandatory procedure arguments
 * 2. true if procedure is variadic, false otherwise
 */
static LispDatum *lisp_arity(const Proc *proc, const Arr *args, MalEnv *env) {
    const Proc *arg_proc = verify_proc_arg_type(proc, args, 0, PROCEDURE);
    if (!arg_proc) return NULL;

    List *list = List_new();
    List_add(list, (LispDatum*) Number_new(Proc_argc(arg_proc)));
    List_add(list, (LispDatum*) LispDatum_bool(Proc_isva(arg_proc)));

    return (LispDatum*) list;
}

// builtin? : Returns true if a procedure (1st arg) is builtin
static LispDatum *lisp_builtinp(const Proc *proc, const Arr *args, MalEnv *env) {
    const Proc *arg_proc = verify_proc_arg_type(proc, args, 0, PROCEDURE);
    if (!arg_proc) return NULL;

    return (LispDatum*) LispDatum_bool(Proc_isbuiltin(arg_proc));
}

// Returns the address of a LispDatum as a string
static LispDatum *lisp_addr(const Proc *proc, const Arr *args, MalEnv *env) {
    const LispDatum *arg0 = Arr_get(args, 0);
    char *str = addr_to_str(arg0);
    LispDatum *out = (LispDatum*) String_new(str);
    free(str);
    return out;
}

// refc : Returns the reference count of a given LispDatum
static LispDatum *lisp_refc(const Proc *proc, const Arr *args, MalEnv *env) 
{
    const LispDatum *arg0 = Arr_get(args, 0);
    // the count will be incremented by the procedure application environment
    // so we substract 1
    return (LispDatum*) Number_new(LispDatum_refc(arg0) - 1);
}

// type : returns the type of the argument as a symbol
static LispDatum *lisp_type(const Proc *proc, const Arr *args, MalEnv *env)
{
    const LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) Symbol_intern(LispType_name(LispDatum_type(arg0)));
}

// env : returns the current (enclosing) environment as a list
// FIXME once MalEnv becomes a first-class type
static LispDatum *lisp_env(const Proc *proc, const Arr *args, MalEnv *env)
{
    Arr *ids = env->ids;
    Arr *datums = env->datums;

    if (ids->len == 0) {
        return (LispDatum*) List_empty();
    }
    else {
        List *list = List_new();

        for (size_t i = 0; i < ids->len; i++) {
            List *pair = List_new();
            LispDatum *id = ids->items[i];
            LispDatum *dtm = datums->items[i];
            List_add(pair, id);
            List_add(pair, dtm);
            List_add(list, (LispDatum*) pair);
        }

        return (LispDatum*) list;
    }
}

// atom : creates a new Atom
static LispDatum *lisp_atom(const Proc *proc, const Arr *args, MalEnv *env)
{
    LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) Atom_new(arg0);
}

// atom?
static LispDatum *lisp_atomp(const Proc *proc, const Arr *args, MalEnv *env) {
    const LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) LispDatum_bool(LispDatum_istype(arg0, ATOM));
}

// deref : Takes an atom argument and returns the Mal value referenced by this atom.
static LispDatum *lisp_deref(const Proc *proc, const Arr *args, MalEnv *env) {
    const Atom *atom = verify_proc_arg_type(proc, args, 0, ATOM);
    if (!atom) return NULL;

    return Atom_deref(atom);
}

// atom-set! : Takes an atom and a LispDatum; the atom is modified to refer to the
// given datum, which is returned
static LispDatum *lisp_atom_set_bang(const Proc *proc, const Arr *args, MalEnv *env) {
    Atom *atom = verify_proc_arg_type(proc, args, 0, ATOM);
    if (!atom) return NULL;

    LispDatum *arg1 = Arr_get(args, 1);

    Atom_set(atom, arg1);

    return arg1;
}

// cons : prepend a value to a list
static LispDatum *lisp_cons(const Proc *proc, const Arr *args, MalEnv *env)
{
    List *list = verify_proc_arg_type(proc, args, 1, LIST);
    if (!list) return NULL;

    LispDatum *arg0 = Arr_get(args, 0);

    List *new_list = List_cons_new(list, arg0);
    return (LispDatum*) new_list;
}

// concat : concatenates given lists, if 0 arguments are given returns an empty list
static LispDatum *lisp_concat(const Proc *proc, const Arr *args, MalEnv *env)
{
    if (args->len == 0) {
        return (LispDatum*) List_empty();
    }

    // verify argument types and find locations of the 1st and 2nd non-empty lists
    size_t idxs[2] = { -1, -1 };
    size_t j = 0;
    for (size_t i = 0; i < args->len; i++) {
        const List *list = verify_proc_arg_type(proc, args, i, LIST);
        if (!list) return NULL;

        if (j < 2 && !List_isempty(list)) {
            idxs[j++] = i;
        }
    }

    if (j == 0) 
        return (LispDatum*) List_empty();
    else if (j == 1) 
        return Arr_get(args, idxs[0]);
    else {
        // copy the 1st non-empty list and append everything else to it
        const List *first = Arr_get(args, idxs[0]);
        List *new_list = List_shlw_copy(first);

        for (size_t i = idxs[1]; i < args->len; i++) {
            const List *list = Arr_get(args, i);
            List_append(new_list, list);
        }

        return (LispDatum*) new_list;
    }
}

// macro?
static LispDatum *lisp_macrop(const Proc *proc, const Arr *args, MalEnv *env) {
    const LispDatum *arg0 = Arr_get(args, 0);
    if (!LispDatum_istype(arg0, PROCEDURE)) 
        return (LispDatum*) False_get();

    return (LispDatum*) LispDatum_bool(Proc_ismacro((Proc*) arg0));
}

// exn : exception constructor
static LispDatum *lisp_exn(const Proc *proc, const Arr *args, MalEnv *env)
{
    const LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) Exception_new(arg0);
}

// exn?
static LispDatum *lisp_exnp(const Proc *proc, const Arr *args, MalEnv *env)
{
    const LispDatum *arg0 = Arr_get(args, 0);
    return (LispDatum*) LispDatum_bool(LispDatum_istype(arg0, EXCEPTION));
}

// exn-datum : returns the value which the exception was constructed with
static LispDatum *lisp_exn_datum(const Proc *proc, const Arr *args, MalEnv *env)
{
    const Exception *exn = verify_proc_arg_type(proc, args, 0, EXCEPTION);
    if (!exn) return NULL;
    return Exception_datum(exn);
}

// throw : throws an exception
static LispDatum *lisp_throw(const Proc *proc, const Arr *args, MalEnv *env)
{
    const LispDatum *arg0 = Arr_get(args, 0);
    throw(NULL, arg0);
    return NULL;
}

void core_def_procs(MalEnv *env) 
{
#define DEF(name, arity, variadic, funp) \
    { \
        Symbol *_s = Symbol_intern(name); \
        MalEnv_put(env, _s, (LispDatum*) Proc_builtin(_s, arity, variadic, funp)); \
    }

    DEF("+", 2, true, lisp_add);
    DEF("-", 2, true, lisp_sub);
    DEF("*", 2, true, lisp_mul);
    DEF("/", 2, true, lisp_div);
    DEF("=", 2, false, lisp_eq);
    DEF(">", 2, false, lisp_gt);
    DEF("%", 2, false, lisp_mod);
    DEF("even?", 1, false, lisp_evenp);
    DEF("number?", 1, false, lisp_numberp);

    DEF("symbol", 1, false, lisp_symbol);
    DEF("symbol?", 1, false, lisp_symbolp);

    DEF("string?", 1, false, lisp_stringp);

    DEF("true?", 1, false, lisp_truep);
    DEF("false?", 1, false, lisp_falsep);

    DEF("list", 0, true, lisp_list);
    DEF("list?", 1, false, lisp_listp);
    DEF("empty?", 1, false, lisp_emptyp);
    // DEF("count", 1, false, lisp_count);
    DEF("list-ref", 2, false, lisp_list_ref);
    DEF("list-rest", 1, false, lisp_list_rest);

    DEF("nth", 2, false, lisp_nth);
    DEF("rest", 1, false, lisp_rest);

    DEF("prn", 0, true, lisp_prn);
    DEF("pr-str", 0, true, lisp_pr_str);
    DEF("str", 0, true, lisp_str);
    DEF("println", 0, true, lisp_println);

    DEF("procedure?", 1, false, lisp_procedurep);
    DEF("arity", 1, false, lisp_arity);
    DEF("builtin?", 1, false, lisp_builtinp);

    DEF("addr", 1, false, lisp_addr);
    DEF("refc", 1, false, lisp_refc);
    DEF("type", 1, false, lisp_type);
    DEF("env", 0, false, lisp_env);

    DEF("atom", 1, false, lisp_atom);
    DEF("atom?", 1, false, lisp_atomp);
    DEF("deref", 1, false, lisp_deref);
    DEF("atom-set!", 2, false, lisp_atom_set_bang);

    DEF("cons", 2, false, lisp_cons);
    DEF("concat", 0, true, lisp_concat);

    DEF("macro?", 0, true, lisp_macrop);

    DEF("exn", 1, false, lisp_exn);
    DEF("exn?", 1, false, lisp_exnp);
    DEF("exn-datum", 1, false, lisp_exn_datum);
    DEF("throw", 1, false, lisp_throw);
}
