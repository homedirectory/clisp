#include "common.h"
#include "types.h"
#include "utils.h"
#include "core.h"
#include "env.h"

static MalDatum *mal_add(Proc *proc, Arr *args) {
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

static MalDatum *mal_sub(Proc *proc, Arr *args) {
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

static MalDatum *mal_mul(Proc *proc, Arr *args) {
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

static MalDatum *mal_div(Proc *proc, Arr *args) {
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

void core_def_procs(MalEnv *env) {
    MalEnv_put(env, Symbol_new("+"), MalDatum_new_proc(Proc_builtin(2, true, mal_add)));
    MalEnv_put(env, Symbol_new("-"), MalDatum_new_proc(Proc_builtin(2, true, mal_sub)));
    MalEnv_put(env, Symbol_new("*"), MalDatum_new_proc(Proc_builtin(2, true, mal_mul)));
    MalEnv_put(env, Symbol_new("/"), MalDatum_new_proc(Proc_builtin(2, true, mal_div)));
}
