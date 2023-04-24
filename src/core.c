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

void core_def_procs(MalEnv *env) {
    // FIXME memory leak
    MalEnv_put(env, Symbol_new("+"), MalDatum_new_proc(Proc_builtin(2, true, mal_add)));
    MalEnv_put(env, Symbol_new("-"), MalDatum_new_proc(Proc_builtin(2, true, mal_sub)));
    MalEnv_put(env, Symbol_new("*"), MalDatum_new_proc(Proc_builtin(2, true, mal_mul)));
    MalEnv_put(env, Symbol_new("/"), MalDatum_new_proc(Proc_builtin(2, true, mal_div)));
    MalEnv_put(env, Symbol_new("="), MalDatum_new_proc(Proc_builtin(2, false, mal_eq)));
    MalEnv_put(env, Symbol_new(">"), MalDatum_new_proc(Proc_builtin(2, false, mal_gt)));
}
