#include "types.h"
#include "utils.h"
#include "env.h"
#include <stdlib.h>
#include "common.h"

MalEnv *MalEnv_new(MalEnv *enclosing) {
    MalEnv *env = malloc(sizeof(MalEnv));
    env->ids = Arr_new();
    env->datums = Arr_new();
    env->enclosing = enclosing;
    MalEnv_own(enclosing);
    env->refc = 0;
    return env;
}

void MalEnv_free(MalEnv *env) {
    if (env == NULL) {
        LOG_NULL(env);
        return;
    }
    if (env->refc > 0) {
        DEBUG("Refuse to free %p (refc %ld)", env, env->refc);
        return;
    }

    DEBUG("freeing MalEnv (refc = %ld)", env->refc);

    Arr_freep(env->ids, free);
    Arr_freep(env->datums, (free_t) MalDatum_release_free);
    // the enclosing env should not be freed, but simply released
    if (env->enclosing)
        MalEnv_release(env->enclosing);
    free(env);
}

MalDatum *MalEnv_put(MalEnv *env, const char *id, MalDatum *datum) {
    if (env == NULL) {
        LOG_NULL(env);
        return NULL;
    }

    MalDatum_own(datum);

    // if datum is an unnamed procedure, then set its name to id
    if (MalDatum_istype(datum, PROCEDURE)) {
        Proc *proc = datum->value.proc;
        if (!Proc_is_named(proc)) {
            proc->name = dyn_strcpy(id);
        }
    }

    int idx = Arr_findf(env->ids, id, (equals_t) streq);
    if (idx == -1) { // new identifier
        Arr_add(env->ids, dyn_strcpy(id));
        Arr_add(env->datums, datum);
        return NULL;
    } else { // existing identifier
        MalDatum *old = Arr_replace(env->datums, idx, datum);
        MalDatum_release(old);
        return old;
    }
}

MalDatum *MalEnv_get(const MalEnv *env, const char *id) {
    if (env == NULL) {
        LOG_NULL(env);
        return NULL;
    }

    const MalEnv *e = env;
    int idx = -1;
    while (e != NULL) {
        idx = Arr_findf(e->ids, id, (equals_t) streq);
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

void MalEnv_own(MalEnv *env)
{
    if (!env) {
        LOG_NULL(env);
        return;
    }

    env->refc += 1;
}

void MalEnv_release(MalEnv *env)
{
    if (env == NULL) {
        LOG_NULL(env);
        return;
    }
    if (env->refc <= 0)
        DEBUG("illegal attempt to decrement ref count = %ld", env->refc);

    env->refc -= 1;
}
