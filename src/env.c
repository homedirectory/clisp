#include "types.h"
#include "utils.h"
#include "env.h"
#include <stdlib.h>
#include "common.h"

MalEnv *MalEnv_new(MalEnv *enclosing) {
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
        // TODO free each symbol and datum
        Arr_free(env->symbols);
        Arr_free(env->datums);
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

MalDatum *MalEnv_get(MalEnv *env, Symbol *sym) {
    MalEnv *e = env;
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
