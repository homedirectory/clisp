#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "utils.h"
#include "env.h"
#include "common.h"
#include "hashtbl.h"

static uint hash_symbol(const Symbol *sym)
{
    const char *name = Symbol_name(sym);
    uint h = hash_simple_str(name);
    return h;
}

MalEnv *MalEnv_new(MalEnv *enclosing) {
    MalEnv *env = malloc(sizeof(MalEnv));
    env->binds = HashTbl_new((hashkey_t) hash_symbol);
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

    HashTbl_free(env->binds, (free_t) LispDatum_rls_free, (free_t) LispDatum_rls_free);
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

    LispDatum *old = HashTbl_put(env->binds, id, datum, (keyeq_t) Symbol_eq);
    if (old != NULL) // Symbol id is being bound to a different datum
        LispDatum_rls(old);
    else // first time entering Symbol id into this env, so let's own it
        LispDatum_own((LispDatum*) id);
    return old;
}

LispDatum *MalEnv_get(const MalEnv *env, const Symbol *id) {
    if (env == NULL)
        FATAL("env == NULL");

    const MalEnv *e = env;
    LispDatum *dtm = NULL;
    while (e != NULL && dtm == NULL) {
        dtm = HashTbl_get(e->binds, id, (keyeq_t) Symbol_eq);
        e = e->enclosing;
    }

    return dtm;
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

void MalEnv_rls_free(MalEnv *env)
{
    MalEnv_release(env);
    MalEnv_free(env);
}
