#pragma once

#include "types.h"
#include "stdbool.h"
#include "hashtbl.h"

/* This environment is an associative structure that maps identifiers to mal values */
typedef struct MalEnv {
    HashTbl *binds; // Symbol* -> LispDatum*
    struct MalEnv *enclosing;
    long refc;    // reference count
} MalEnv;

// Creates a new environment that is enclosed by the given environment.
// env might be NULL when a top-level environment is created.
MalEnv *MalEnv_new(MalEnv *env);

void MalEnv_free(MalEnv *env);

/* Associates a LispDatum with an identifier.
 * If the given identifier was already associated with some datum, returns that datum,
 * otherwise returns NULL.
 */
LispDatum *MalEnv_put(MalEnv *env, Symbol *id, LispDatum *datum);
LispDatum *MalEnv_puts(MalEnv *env, const char *id, LispDatum *datum);

/* Returns the LispDatum associated with the given identifier or NULL. */
LispDatum *MalEnv_get(const MalEnv *env, const Symbol *id);
LispDatum *MalEnv_gets(const MalEnv *env, const char *id);

// returns the top-most enclosing environment of the given one
MalEnv *MalEnv_enclosing_root(MalEnv *env);

// reference counting
void MalEnv_own(MalEnv *env);
void MalEnv_release(MalEnv *env);
void MalEnv_rls_free(MalEnv *env);
