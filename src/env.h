#pragma once

#include "types.h"
#include "stdbool.h"

/* This environment is an associative structure that maps identifiers to mal values */
typedef struct MalEnv {
    // TODO replace Arr by a proper hashmap
    Arr *ids; // of char*
    Arr *datums;  // of MalDatum*
    struct MalEnv *enclosing;
    long refc; // reference count
} MalEnv;

// Creates a new environment that is enclosed by the given environment.
// env might be NULL when a top-level environment is created.
MalEnv *MalEnv_new(MalEnv *env);

void MalEnv_free(MalEnv *env);

/* Associates a MalDatum with an identifier.
 * If the given identifier was already associated with some datum, returns that datum,
 * otherwise returns NULL.
 */
MalDatum *MalEnv_put(MalEnv *env, const char *id, MalDatum *datum);

/* Returns the MalDatum associated with the given identifier or NULL. */
MalDatum *MalEnv_get(const MalEnv *env, const char *id);

// returns the top-most enclosing environment of the given one
MalEnv *MalEnv_enclosing_root(MalEnv *env);

// reference counting
void MalEnv_own(MalEnv *env);
void MalEnv_release(MalEnv *env);
