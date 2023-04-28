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
    env->reachable = false;
    return env;
}

void MalEnv_free(MalEnv *env) {
    if (env == NULL) {
        LOG_NULL(env);
        return;
    }
    if (env->reachable) {
        DEBUG("attempt to free a reachable MalEnv was prevented");
        return;
    }
    Arr_freep(env->symbols, (free_t) Symbol_free);
    for (size_t i = 0; i < env->datums->len; i++)
        MalDatum_release(env->datums->items[i]);

    Arr_freep(env->datums, (free_t) MalDatum_free);
    // the enclosing env should not be freed
    free(env);
}

MalDatum *MalEnv_put(MalEnv *env, const Symbol *sym, MalDatum *datum) {
    if (env == NULL) {
        LOG_NULL(env);
        return NULL;
    }

    MalDatum_own(datum);

    int idx = Arr_findf(env->symbols, sym, (equals_t) Symbol_eq);
    if (idx == -1) { // new symbol
        Arr_add(env->symbols, Symbol_copy(sym));
        Arr_add(env->datums, datum);
        return NULL;
    } else { // existing symbol
        MalDatum *old = Arr_replace(env->datums, idx, datum);
        MalDatum_release(old);
        return old;
    }
}

MalDatum *MalEnv_get(const MalEnv *env, const Symbol *sym) {
    if (env == NULL) {
        LOG_NULL(env);
        return NULL;
    }

    const MalEnv *e = env;
    int idx = -1;
    while (e != NULL) {
        idx = Arr_findf(e->symbols, sym, (equals_t) Symbol_eq);
        if (idx != -1)
            return e->datums->items[idx];
        e = e->enclosing;
    }

    return NULL;
}

MalEnv *MalEnv_enclosing_root(MalEnv *env) 
{
    while (env->enclosing) env = env->enclosing;
    return env;
}
