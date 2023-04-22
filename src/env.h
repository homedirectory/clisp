#pragma once

#include "types.h"

/* This environment is an associative structure that maps symbols to mal values */
typedef struct MalEnv {
    Arr *symbols; // of Symbol*
    Arr *datums;  // of MalDatum*
} MalEnv;

MalEnv *MalEnv_new();
void MalEnv_free(MalEnv *env);

/* Associates a MalDatum with a symbol.
 * If the given symbol was already associated with some datum, returns that datum,
 * otherwise returns NULL.
 */
MalDatum *MalEnv_put(MalEnv *env, Symbol *sym, MalDatum *datum);

/* Returns the MalDatum associated with the given symbol or NULL. */
MalDatum *MalEnv_get(MalEnv *env, Symbol *sym);
