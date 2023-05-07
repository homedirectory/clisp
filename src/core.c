#include <stdlib.h>
#include "common.h"
#include "types.h"
#include "utils.h"
#include "core.h"
#include "env.h"
#include "printer.h"
#include "mem_debug.h"


MalDatum *verify_proc_arg_type(const Proc *proc, const Arr *args, size_t arg_idx, 
        MalType expect_type)
{
    MalDatum *arg = Arr_get(args, arg_idx);
    if (!MalDatum_istype(arg, expect_type)) {
        char *proc_name = Proc_name(proc);
        throwf("%s: bad arg no. %zd: expected a %s", 
                proc_name, arg_idx + 1, MalType_tostr(expect_type));
        free(proc_name);
        return NULL;
    }

    return arg;
}

static MalDatum *mal_add(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        if (!verify_proc_arg_type(proc, args, i, INT)) 
            return NULL;
    }

    int rslt = ((MalDatum*)args->items[0])->value.i;
    for (int i = 1; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        rslt += arg->value.i;
    }

    return MalDatum_new_int(rslt);
}

static MalDatum *mal_sub(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        if (!verify_proc_arg_type(proc, args, i, INT)) 
            return NULL;
    }

    int rslt = ((MalDatum*)args->items[0])->value.i;
    for (int i = 1; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        rslt -= arg->value.i;
    }

    return MalDatum_new_int(rslt);
}

static MalDatum *mal_mul(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        if (!verify_proc_arg_type(proc, args, i, INT)) 
            return NULL;
    }

    int rslt = ((MalDatum*)args->items[0])->value.i;
    for (int i = 1; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        rslt *= arg->value.i;
    }

    return MalDatum_new_int(rslt);
}

static MalDatum *mal_div(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        if (!verify_proc_arg_type(proc, args, i, INT)) 
            return NULL;
    }

    int rslt = ((MalDatum*)args->items[0])->value.i;
    for (int i = 1; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        rslt /= arg->value.i;
    }

    return MalDatum_new_int(rslt);
}

/* '=' : compare the first two parameters and return true if they are the same type
 * and contain the same value. In the case of equal length lists, each element
 * of the list should be compared for equality and if they are the same return
 * true, otherwise false
 */
static MalDatum *mal_eq(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg1 = args->items[0];
    MalDatum *arg2 = args->items[1];

    return MalDatum_eq(arg1, arg2) ? MalDatum_true() : MalDatum_false();
}

/* '>' : compare the first two numeric parameters */
static MalDatum *mal_gt(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        if (!verify_proc_arg_type(proc, args, i, INT)) 
            return NULL;
    }

    MalDatum *arg1 = args->items[0];
    MalDatum *arg2 = args->items[1];

    return arg1->value.i > arg2->value.i ? MalDatum_true() : MalDatum_false();
}

/* % : modulus */
static MalDatum *mal_mod(const Proc *proc, const Arr *args, MalEnv *env) 
{
    const MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, INT);
    if (!arg0) return NULL;
    const MalDatum *arg1 = verify_proc_arg_type(proc, args, 1, INT);
    if (!arg1) return NULL;

    int i0 = arg0->value.i;
    int i1 = arg1->value.i;
    int rslt = i0 % i1;

    return MalDatum_new_int(rslt);
}

/* even? */
static MalDatum *mal_evenp(const Proc *proc, const Arr *args, MalEnv *env) 
{
    const MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, INT);
    if (!arg0) return NULL;

    int i = arg0->value.i;
    return i & 1 ? MalDatum_false() : MalDatum_true();
}

// symbol : string to symbol 
static MalDatum *mal_symbol(const Proc *proc, const Arr *args, MalEnv *env) 
{
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, STRING);
    if (!arg0) return NULL;

    return MalDatum_new_sym(Symbol_new(arg0->value.string));
}

// symbol?
static MalDatum *mal_symbolp(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg0 = Arr_get(args, 0);
    return MalDatum_istype(arg0, SYMBOL) ? MalDatum_true() : MalDatum_false();
}

// string?
static MalDatum *mal_stringp(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg0 = Arr_get(args, 0);
    return MalDatum_istype(arg0, STRING) ? MalDatum_true() : MalDatum_false();
}

// true?
static MalDatum *mal_truep(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg0 = Arr_get(args, 0);
    return MalDatum_istype(arg0, TRUE) ? MalDatum_true() : MalDatum_false();
}

// false?
static MalDatum *mal_falsep(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg0 = Arr_get(args, 0);
    return MalDatum_istype(arg0, FALSE) ? MalDatum_true() : MalDatum_false();
}

// list
static MalDatum *mal_list(const Proc *proc, const Arr *args, MalEnv *env) {
    if (args->len == 0)
        return MalDatum_empty_list();

    List *list = List_new();
    for (int i = 0; i < args->len; i++) {
        MalDatum *dtm = args->items[i];
        List_add(list, dtm);
    }

    return MalDatum_new_list(list);
}

static MalDatum *mal_listp(const Proc *proc, const Arr *args, MalEnv *env) {
    return MalDatum_islist(args->items[0]) ? MalDatum_true() : MalDatum_false();
}

static MalDatum *mal_emptyp(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, LIST);
    if (!arg0) return NULL;
    List *list = arg0->value.list;
    return List_isempty(list) ? MalDatum_true() : MalDatum_false();
}

static MalDatum *mal_count(const Proc *proc, const Arr *args, MalEnv *env) {
    // validate arg type
    MalDatum *arg = args->items[0];

    int len;
    if (MalDatum_istype(arg, NIL))
        len = 0;
    else if (MalDatum_islist(arg))
        len = List_len(arg->value.list);
    else {
        throwf("count: expected a list, but got %s instead", MalType_tostr(arg->type));
        return NULL;
    }

    return MalDatum_new_int(len);
}

static MalDatum *mal_list_ref(const Proc *proc, const Arr *args, MalEnv *env) 
{
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, LIST);
    if (!arg0) return NULL;
    MalDatum *arg1 = verify_proc_arg_type(proc, args, 1, INT);
    if (!arg1) return NULL;

    List *list = arg0->value.list;
    int idx = arg1->value.i;

    if (idx < 0) {
        throwf("list-ref: expected non-negative index");
        return NULL;
    }
    size_t list_len = List_len(list);
    if (idx >= list_len) {
        throwf("list-ref: index too large (%d >= %zu)", idx, list_len);
        return NULL;
    }

    return List_ref(list, idx);
}

static MalDatum *mal_list_rest(const Proc *proc, const Arr *args, MalEnv *env) 
{
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, LIST);
    if (!arg0) return NULL;
    List *list = arg0->value.list;

    if (List_isempty(list)) {
        throwf("list-rest: received an empty list");
        return NULL;
    }

    return MalDatum_new_list(List_rest_new(list));
}

// nth : takes a list (or vector) and a number (index) as arguments, returns
// the element of the list/vector at the given index. If the index is out of range,
// then an error is raised.
static MalDatum *mal_nth(const Proc *proc, const Arr *args, MalEnv *env) 
{
    MalDatum *arg0 = Arr_get(args, 0);
    if (MalDatum_islist(arg0)) {
        return mal_list_ref(proc, args, env);
    }
    // else if (MalDatum_istype(arg0, VECTOR)) {
    //     return mal_vec_ref(proc, args, env);
    // }
    else {
        throwf("nth: bad 1st arg: expected LIST or VECTOR, but was %s",
                MalType_tostr(arg0->type));
        return NULL;
    }
}

// rest : takes a list (or vector) as its argument and returns a new list/vector
//     containing all the elements except the first. If the list/vector is empty
//     empty then an error is raised.
static MalDatum *mal_rest(const Proc *proc, const Arr *args, MalEnv *env) 
{
    MalDatum *arg0 = Arr_get(args, 0);
    if (MalDatum_islist(arg0)) {
        return mal_list_rest(proc, args, env);
    }
    // else if (MalDatum_istype(arg0, VECTOR)) {
    //     return mal_vec_rest(proc, args, env);
    // }
    else {
        throwf("rest: bad 1st arg: expected LIST or VECTOR, but was %s",
                MalType_tostr(arg0->type));
        return NULL;
    }
}

// prn: calls pr_str on each argument with print_readably set to true, joins the
// results with " ", prints the string to the screen and then returns nil.
static MalDatum *mal_prn(const Proc *proc, const Arr *args, MalEnv *env)
{
    if (args->len > 0) {
        char *strings[args->len];

        for (size_t i = 0; i < args->len; i++) {
            MalDatum *arg = args->items[i];
            strings[i] = pr_str(arg, true);
        }

        // join with " "
        char *joined = str_join(strings, args->len, " ");
        printf("%s\n", joined);
        free(joined);

        for (size_t i = 0; i < args->len; i++)
            free(strings[i]);
    }

    return MalDatum_nil();
}

// pr-str: calls pr_str on each argument with print_readably set to true, joins
// the results with " " and returns the new string.
static MalDatum *mal_pr_str(const Proc *proc, const Arr *args, MalEnv *env)
{
    if (args->len == 0) {
        return MalDatum_new_string("");
    }
    else {
        char *strings[args->len];

        for (size_t i = 0; i < args->len; i++) {
            MalDatum *arg = args->items[i];
            strings[i] = pr_str(arg, true);
        }

        // join with " "
        char *joined = str_join(strings, args->len, " ");

        for (size_t i = 0; i < args->len; i++)
            free(strings[i]);

        MalDatum *out = MalDatum_new_string(joined);
        free(joined);
        return out;
    }
}

/*
 * str: calls pr_str on each argument with print_readably set to false,
 * concatenates the results together ("" separator), and returns the new
 * string.
 */
static MalDatum *mal_str(const Proc *proc, const Arr *args, MalEnv *env)
{
    if (args->len == 0) {
        return MalDatum_new_string("");
    }
    else {
        char *strings[args->len];

        for (size_t i = 0; i < args->len; i++) {
            MalDatum *arg = args->items[i];
            strings[i] = pr_str(arg, false);
        }

        char *joined = str_join(strings, args->len, "");

        for (size_t i = 0; i < args->len; i++)
            free(strings[i]);

        MalDatum *out = MalDatum_new_string(joined);
        free(joined);
        return out;
    }
}

/*
 * println: calls pr_str on each argument with print_readably set to false,
 * joins the results with " ", prints the string to the screen and then returns
 * nil.
 */
static MalDatum *mal_println(const Proc *proc, const Arr *args, MalEnv *env)
{
    if (args->len > 0) {
        char *strings[args->len];

        for (size_t i = 0; i < args->len; i++) {
            MalDatum *arg = args->items[i];
            strings[i] = pr_str(arg, false);
        }

        char *joined = str_join(strings, args->len, " ");
        printf("%s\n", joined);
        free(joined);

        for (size_t i = 0; i < args->len; i++)
            free(strings[i]);
    }

    return MalDatum_nil();
}

/* (procedure? datum) : predicate for procedures */
static MalDatum *mal_procedurep(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg = Arr_get(args, 0);
    return MalDatum_istype(arg, PROCEDURE) ? MalDatum_true() : MalDatum_false();
}

/* (arity proc) : returns a list of 2 elements:
 * 1. number of mandatory procedure arguments
 * 2. true if procedure is variadic, false otherwise
 */
static MalDatum *mal_arity(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, PROCEDURE);
    if (!arg0) return NULL;

    Proc *proc_arg = arg0->value.proc;
    List *list = List_new();
    List_add(list, MalDatum_new_int(proc_arg->argc));
    List_add(list, proc_arg->variadic ? MalDatum_true() : MalDatum_false());

    return MalDatum_new_list(list);
}

// Returns true if a procedure (1st arg) is builtin
static MalDatum *mal_builtinp(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, PROCEDURE);
    if (!arg0) return NULL;

    Proc *proc_arg = arg0->value.proc;

    return proc_arg->builtin ? MalDatum_true() : MalDatum_false();
}

// Returns the address of a MalDatum as a string
static MalDatum *mal_addr(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg0 = Arr_get(args, 0);
    char *str = addr_to_str(arg0);
    MalDatum *out = MalDatum_new_string(str);
    free(str);
    return out;
}

// Returns the reference count of a given MalDatum
static MalDatum *mal_refc(const Proc *proc, const Arr *args, MalEnv *env) 
{
    MalDatum *arg0 = Arr_get(args, 0);
    // the count will be incremented by the procedure application environment
    return MalDatum_new_int(arg0->refc - 1);
}

// type : returns the type of the argument as a symbol
static MalDatum *mal_type(const Proc *proc, const Arr *args, MalEnv *env)
{
    MalDatum *arg0 = Arr_get(args, 0);
    return MalDatum_new_sym(Symbol_new(MalType_tostr(arg0->type)));
}

// env : returns the current (relative) environment as a list
static MalDatum *mal_env(const Proc *proc, const Arr *args, MalEnv *env)
{
    Arr *symbols = env->symbols;
    Arr *datums = env->datums;

    if (symbols->len == 0) {
        return MalDatum_empty_list();
    }
    else {
        List *list = List_new();

        for (size_t i = 0; i < symbols->len; i++) {
            List *pair = List_new();
            Symbol *sym = symbols->items[i];
            MalDatum *dtm = datums->items[i];
            List_add(pair, MalDatum_new_sym(Symbol_copy(sym)));
            List_add(pair, dtm);
            List_add(list, MalDatum_new_list(pair));
        }

        return MalDatum_new_list(list);
    }
}

// atom : creates a new Atom
static MalDatum *mal_atom(const Proc *proc, const Arr *args, MalEnv *env)
{
    MalDatum *arg0 = Arr_get(args, 0);
    return MalDatum_new_atom(Atom_new(arg0));
}

// atom?
static MalDatum *mal_atomp(const Proc *proc, const Arr *args, MalEnv *env) {
    return MalDatum_istype(args->items[0], ATOM) ? MalDatum_true() : MalDatum_false();
}

// deref : Takes an atom argument and returns the Mal value referenced by this atom.
static MalDatum *mal_deref(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, ATOM);
    if (!arg0) return NULL;

    Atom *atom = arg0->value.atom;
    return atom->datum;
}

// reset! : Takes an atom and a Mal value; the atom is modified to refer to the
// given Mal value. The Mal value is returned.
static MalDatum *mal_reset_bang(const Proc *proc, const Arr *args, MalEnv *env) {
    MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, ATOM);
    if (!arg0) return NULL;
    Atom *atom = arg0->value.atom;

    MalDatum *arg1 = Arr_get(args, 1);

    Atom_reset(atom, arg1);

    return arg1;
}

// cons : prepend a value to a list
static MalDatum *mal_cons(const Proc *proc, const Arr *args, MalEnv *env)
{
    List *list;
    {
        MalDatum *arg1 = verify_proc_arg_type(proc, args, 1, LIST);
        if (!arg1) return NULL;
        list = arg1->value.list;
    }

    MalDatum *arg0 = Arr_get(args, 0);

    List *new_list = List_cons_new(list, arg0);
    return MalDatum_new_list(new_list);
}

// concat : concatenates given lists, if 0 arguments are given returns an empty list
static MalDatum *mal_concat(const Proc *proc, const Arr *args, MalEnv *env)
{
    if (args->len == 0) {
        return MalDatum_empty_list();
    }

    // verify argument types and
    // find locations of the 1st and 2nd non-empty lists
    size_t idxs[2] = { -1, -1 };
    size_t j = 0;
    for (size_t i = 0; i < args->len; i++) {
        const MalDatum *arg = verify_proc_arg_type(proc, args, i, LIST);
        if (!arg) return NULL;
        const List *list = arg->value.list;

        if (j < 2 && !List_isempty(list)) {
            idxs[j++] = i;
        }
    }

    if (j == 0) return MalDatum_empty_list();
    else if (j == 1) return Arr_get(args, idxs[0]);
    else {
        // copy the 1st non-empty list and append everything else to it
        const List *first = ((MalDatum*)Arr_get(args, idxs[0]))->value.list;
        List *new_list = List_copy(first);

        for (size_t i = idxs[1]; i < args->len; i++) {
            const List *list = ((MalDatum*)Arr_get(args, i))->value.list;
            List_append(new_list, list);
        }

        return MalDatum_new_list(new_list);
    }
}

static MalDatum *mal_macrop(const Proc *proc, const Arr *args, MalEnv *env) {
    const MalDatum *arg0 = Arr_get(args, 0);
    if (!MalDatum_istype(arg0, PROCEDURE)) return MalDatum_false();
    const Proc *arg0_proc = arg0->value.proc;
    return Proc_is_macro(arg0_proc) ? MalDatum_true() : MalDatum_false();
}

// exn : exception constructor
static MalDatum *mal_exn(const Proc *proc, const Arr *args, MalEnv *env)
{
    const MalDatum *arg0 = Arr_get(args, 0);
    return MalDatum_new_exn(Exception_new(arg0));
}

// exn?
static MalDatum *mal_exnp(const Proc *proc, const Arr *args, MalEnv *env)
{
    const MalDatum *arg0 = Arr_get(args, 0);
    return MalDatum_istype(arg0, EXCEPTION) ? MalDatum_true() : MalDatum_false();
}

// exn-datum : returns the value which the exception was constructed with
static MalDatum *mal_exn_datum(const Proc *proc, const Arr *args, MalEnv *env)
{
    const MalDatum *arg0 = verify_proc_arg_type(proc, args, 0, EXCEPTION);
    if (!arg0) return NULL;

    const Exception *exn = arg0->value.exn;
    return exn->datum;
}

void core_def_procs(MalEnv *env) 
{
#define DEF(name, arity, variadic, func_ptr) \
    do { \
        Symbol *sym = Symbol_new(name); \
        MalEnv_put(env, sym, MalDatum_new_proc(\
                    Proc_builtin(name, arity, variadic, func_ptr))); \
        Symbol_free(sym); \
    } while (0);

    DEF("+", 2, true, mal_add);
    DEF("-", 2, true, mal_sub);
    DEF("*", 2, true, mal_mul);
    DEF("/", 2, true, mal_div);
    DEF("=", 2, false, mal_eq);
    DEF(">", 2, false, mal_gt);
    DEF("%", 2, false, mal_mod);
    DEF("even?", 1, false, mal_evenp);

    DEF("symbol", 1, false, mal_symbol);
    DEF("symbol?", 1, false, mal_symbolp);

    DEF("string?", 1, false, mal_stringp);

    DEF("true?", 1, false, mal_truep);
    DEF("false?", 1, false, mal_falsep);

    DEF("list", 0, true, mal_list);
    DEF("list?", 1, false, mal_listp);
    DEF("empty?", 1, false, mal_emptyp);
    DEF("count", 1, false, mal_count);
    DEF("list-ref", 2, false, mal_list_ref);
    DEF("list-rest", 1, false, mal_list_rest);

    DEF("nth", 2, false, mal_nth);
    DEF("rest", 1, false, mal_rest);

    DEF("prn", 0, true, mal_prn);
    DEF("pr-str", 0, true, mal_pr_str);
    DEF("str", 0, true, mal_str);
    DEF("println", 0, true, mal_println);

    DEF("procedure?", 1, false, mal_procedurep);
    DEF("arity", 1, false, mal_arity);
    DEF("builtin?", 1, false, mal_builtinp);

    DEF("addr", 1, false, mal_addr);
    DEF("refc", 1, false, mal_refc);
    DEF("type", 1, false, mal_type);
    DEF("env", 0, false, mal_env);

    DEF("atom", 1, false, mal_atom);
    DEF("atom?", 1, false, mal_atomp);
    DEF("deref", 1, false, mal_deref);
    DEF("reset!", 2, false, mal_reset_bang);

    DEF("cons", 2, false, mal_cons);
    DEF("concat", 0, true, mal_concat);

    DEF("macro?", 0, true, mal_macrop);

    DEF("exn", 1, false, mal_exn);
    DEF("exn?", 1, false, mal_exnp);
    DEF("exn-datum", 1, false, mal_exn_datum);
}
