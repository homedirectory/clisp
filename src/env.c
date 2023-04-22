#include "types.h"
#include "utils.h"
#include "env.h"
#include <stdlib.h>
#include "common.h"

MalEnv *MalEnv_new() {
    MalEnv *env = malloc(sizeof(MalEnv));
    env->symbols = Arr_new();
    env->datums = Arr_new();
    return env;
}

void MalEnv_free(MalEnv *env) {
    if (env == NULL) {
        LOG_NULL(env, MalEnv_free);
    } else {
        Arr_free(env->symbols);
        Arr_free(env->datums);
        free(env);
    }
}

MalDatum *MalEnv_put(MalEnv *env, Symbol *sym, MalDatum *datum) {
    size_t idx = Arr_findf(env->symbols, sym, (equals_t) Symbol_eq);
    if (idx == -1) { // new symbol
        Arr_add(env->symbols, sym);
        Arr_add(env->datums, datum);
        return NULL;
    } else { // existing symbol
        MalDatum *old = Arr_replace(env->datums, idx, datum);
        return old;
    }
}

MalDatum *MalEnv_get(MalEnv *env, Symbol *sym) {
    size_t idx = Arr_findf(env->symbols, sym, (equals_t) Symbol_eq);
    if (idx == -1)
        return NULL;
    else
        return env->datums->items[idx];
}
