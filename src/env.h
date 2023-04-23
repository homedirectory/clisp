#pragma once

#include "types.h"

/* This environment is an associative structure that maps symbols to mal values */
typedef struct MalEnv {
    // TODO replace Arr by a proper hashmap
    Arr *symbols; // of Symbol*
    Arr *datums;  // of MalDatum*
    struct MalEnv *enclosing;
} MalEnv;

// Creates a new environment that is enclosed by the given environment.
// env might be NULL when a top-level environment is created.
MalEnv *MalEnv_new(MalEnv *env);

void MalEnv_free(MalEnv *env);

/* Associates a MalDatum with a symbol.
 * If the given symbol was already associated with some datum, returns that datum,
 * otherwise returns NULL.
 */
MalDatum *MalEnv_put(MalEnv *env, Symbol *sym, MalDatum *datum);

/* Returns the MalDatum associated with the given symbol or NULL. */
MalDatum *MalEnv_get(MalEnv *env, Symbol *sym);
