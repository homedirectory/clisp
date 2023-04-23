#include "types.h"
#include "utils.h"
#include "env.h"
#include <stdlib.h>
#include "common.h"

MalEnv *MalEnv_new(const MalEnv *enclosing) {
    MalEnv *env = malloc(sizeof(MalEnv));
    env->symbols = Arr_new();
    env->datums = Arr_new();
    env->enclosing = enclosing;
    return env;
}

void MalEnv_free(MalEnv *env) {
    if (env == NULL) {
        LOG_NULL(env, MalEnv_free);
    } else {
        Arr_freep(env->symbols, (free_t) Symbol_free);
        Arr_freep(env->datums, (free_t) MalDatum_free);
        // the enclosing env should not be freed
        free(env);
    }
}

MalDatum *MalEnv_put(MalEnv *env, Symbol *sym, MalDatum *datum) {
    int idx = Arr_findf(env->symbols, sym, (equals_t) Symbol_eq);
    if (idx == -1) { // new symbol
        Arr_add(env->symbols, sym);
        Arr_add(env->datums, datum);
        return NULL;
    } else { // existing symbol
        MalDatum *old = Arr_replace(env->datums, idx, datum);
        return old;
    }
}

MalDatum *MalEnv_get(const MalEnv *env, const Symbol *sym) {
    const MalEnv *e = env;
    int idx = -1;
    while (e != NULL) {
        idx = Arr_findf(e->symbols, sym, (equals_t) Symbol_eq);
        if (idx != -1)
            break;
        e = e->enclosing;
    }

    if (idx == -1)
        return NULL;
    else
        return e->datums->items[idx];
}
