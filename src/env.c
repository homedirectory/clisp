#include "types.h"
#include "utils.h"
#include "env.h"
#include <stdlib.h>

MalEnv *MalEnv_new() {
    MalEnv *env = malloc(sizeof(MalEnv));
    env->symbols = Arr_new();
    env->funcs = Arr_new();
    return env;
}

intfun MalEnv_put(MalEnv *env, Symbol *sym, intfun fun) {
    size_t idx = Arr_find(env->symbols, sym);
    if (idx == -1) { // new symbol
        Arr_add(env->symbols, sym);
        Arr_add(env->funcs, fun);
        return NULL;
    } else { // existing symbol
        intfun old = Arr_replace(env->funcs, idx, fun);
        return old;
    }
}

intfun MalEnv_get(MalEnv *env, Symbol *sym) {
    size_t idx = Arr_find(env->symbols, sym);
    if (idx == -1)
        return NULL;
    else
        return env->funcs->items[idx];
}
