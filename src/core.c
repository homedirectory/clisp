#include "common.h"
#include "types.h"
#include "utils.h"
#include "core.h"
#include "env.h"

static MalDatum *mal_add(const Proc *proc, const Arr *args) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        if (!MalDatum_istype(arg, INT)) {
            ERROR("+: expected INT arguments, but received %s", MalType_tostr(arg->type));
            return NULL;
        }
    }

    int rslt = ((MalDatum*)args->items[0])->value.i;
    for (int i = 1; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        rslt += arg->value.i;
    }

    return MalDatum_new_int(rslt);
}

static MalDatum *mal_sub(const Proc *proc, const Arr *args) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        if (!MalDatum_istype(arg, INT)) {
            ERROR("+: expected INT arguments, but received %s", MalType_tostr(arg->type));
            return NULL;
        }
    }

    int rslt = ((MalDatum*)args->items[0])->value.i;
    for (int i = 1; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        rslt -= arg->value.i;
    }

    return MalDatum_new_int(rslt);
}

static MalDatum *mal_mul(const Proc *proc, const Arr *args) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        if (!MalDatum_istype(arg, INT)) {
            ERROR("+: expected INT arguments, but received %s", MalType_tostr(arg->type));
            return NULL;
        }
    }

    int rslt = ((MalDatum*)args->items[0])->value.i;
    for (int i = 1; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        rslt *= arg->value.i;
    }

    return MalDatum_new_int(rslt);
}

static MalDatum *mal_div(const Proc *proc, const Arr *args) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        if (!MalDatum_istype(arg, INT)) {
            ERROR("+: expected INT arguments, but received %s", MalType_tostr(arg->type));
            return NULL;
        }
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
static MalDatum *mal_eq(const Proc *proc, const Arr *args) {
    MalDatum *arg1 = args->items[0];
    MalDatum *arg2 = args->items[1];

    return MalDatum_eq(arg1, arg2) ? MalDatum_true() : MalDatum_false();
}

/* '>' : compare the first two numeric parameters */
static MalDatum *mal_gt(const Proc *proc, const Arr *args) {
    // validate arg types
    for (int i = 0; i < args->len; i++) {
        MalDatum *arg = args->items[i];
        if (!MalDatum_istype(arg, INT)) {
            ERROR(">: expected INT arguments, but received %s", MalType_tostr(arg->type));
            return NULL;
        }
    }

    MalDatum *arg1 = args->items[0];
    MalDatum *arg2 = args->items[1];

    return arg1->value.i > arg2->value.i ? MalDatum_true() : MalDatum_false();
}

static MalDatum *mal_list(const Proc *proc, const Arr *args) {
    // TODO use singleton empty list
    List *list = List_new();
    for (int i = 0; i < args->len; i++) {
        MalDatum *dtm = args->items[i];
        // TODO avoid copying args to speed up
        List_add(list, MalDatum_deep_copy(dtm));
    }

    return MalDatum_new_list(list);
}

static MalDatum *mal_listp(const Proc *proc, const Arr *args) {
    return MalDatum_istype(args->items[0], LIST) ? MalDatum_true() : MalDatum_false();
}

static MalDatum *mal_emptyp(const Proc *proc, const Arr *args) {
    // validate arg type
    MalDatum *arg = args->items[0];
    if (!MalDatum_istype(arg, LIST)) {
        ERROR("empty?: expected LIST, but got %s instead", MalType_tostr(arg->type));
        return NULL;
    }
    List *list = arg->value.list;
    return List_isempty(list) ? MalDatum_true() : MalDatum_false();
}

static MalDatum *mal_count(const Proc *proc, const Arr *args) {
    // validate arg type
    MalDatum *arg = args->items[0];
    if (!MalDatum_istype(arg, LIST)) {
        ERROR("count: expected LIST, but got %s instead", MalType_tostr(arg->type));
        return NULL;
    }
    List *list = arg->value.list;
    return MalDatum_new_int(List_len(list));
}

void core_def_procs(MalEnv *env) {
    // FIXME memory leak
    MalEnv_put(env, Symbol_new("+"), MalDatum_new_proc(Proc_builtin(2, true, mal_add)));
    MalEnv_put(env, Symbol_new("-"), MalDatum_new_proc(Proc_builtin(2, true, mal_sub)));
    MalEnv_put(env, Symbol_new("*"), MalDatum_new_proc(Proc_builtin(2, true, mal_mul)));
    MalEnv_put(env, Symbol_new("/"), MalDatum_new_proc(Proc_builtin(2, true, mal_div)));
    MalEnv_put(env, Symbol_new("="), MalDatum_new_proc(Proc_builtin(2, false, mal_eq)));
    MalEnv_put(env, Symbol_new(">"), MalDatum_new_proc(Proc_builtin(2, false, mal_gt)));

    MalEnv_put(env, Symbol_new("list"), MalDatum_new_proc(Proc_builtin(0, true, mal_list)));
    MalEnv_put(env, Symbol_new("list?"), MalDatum_new_proc(Proc_builtin(1, false, mal_listp)));
    MalEnv_put(env, Symbol_new("empty?"), MalDatum_new_proc(Proc_builtin(1, false, mal_emptyp)));
    MalEnv_put(env, Symbol_new("count"), MalDatum_new_proc(Proc_builtin(1, false, mal_count)));
}
