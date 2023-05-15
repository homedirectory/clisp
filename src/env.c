#include <stdlib.h>
#include <assert.h>
#include "types.h"
#include "utils.h"
#include "env.h"
#include "common.h"

MalEnv *MalEnv_new(MalEnv *enclosing) {
    MalEnv *env = malloc(sizeof(MalEnv));
    env->ids = Arr_newn(32);
    env->datums = Arr_newn(32);
    env->enclosing = enclosing;
    if (enclosing)
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

    Arr_freep(env->ids, (free_t) LispDatum_rls_free);
    Arr_freep(env->datums, (free_t) LispDatum_rls_free);
    // the enclosing env should not be freed, but simply released
    if (env->enclosing)
        MalEnv_release(env->enclosing);
    free(env);
}

LispDatum *MalEnv_put(MalEnv *env, Symbol *id, LispDatum *datum) {
    if (env == NULL) {
        LOG_NULL(env);
        return NULL;
    }

    LispDatum_own(datum);

    // if datum is an unnamed procedure, then set its name to id
    if (LispDatum_type(datum) == PROCEDURE) {
        Proc *proc = (Proc*) datum;
        if (!Proc_isnamed(proc)) {
            Proc_set_name(proc, id);
        }
    }

    int idx = Arr_findf(env->ids, id, (equals_t) Symbol_eq);
    if (idx == -1) { // new identifier
        Arr_add(env->ids, (void*) id);
        LispDatum_own((LispDatum*) id);
        Arr_add(env->datums, datum);
        return NULL;
    } else { // existing identifier
        LispDatum *old_dtm = Arr_replace(env->datums, idx, datum);
        // FIXME rls_free ?
        LispDatum_rls(old_dtm);
        return old_dtm;
    }
}

LispDatum *MalEnv_get(const MalEnv *env, const Symbol *id) {
    if (env == NULL)
        FATAL("env == NULL");

    const MalEnv *e = env;
    int idx = -1;
    while (e != NULL) {
        idx = Arr_findf(e->ids, id, (equals_t) Symbol_eq);
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
