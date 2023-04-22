#pragma once

#include "types.h"

typedef int (*intfun)(int,int);

/* This environment is an associative structure that maps symbols (or symbol
 * names) to numeric functions */
typedef struct MalEnv {
    Arr *symbols;
    Arr *funcs;
} MalEnv;

MalEnv *MalEnv_new();

/* Associates a numeric function with a symbol.
 * If the given symbol was already associated with some function, returns that function,
 * otherwise returns NULL.
 */
intfun MalEnv_put(MalEnv *env, Symbol *sym, intfun fun);

/* Returns the function associated with the given symbol or NULL. */
intfun MalEnv_get(MalEnv *env, Symbol *sym);
