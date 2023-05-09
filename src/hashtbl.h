#pragma once

#include <stdbool.h>
#include "common.h"

typedef unsigned int uint;

typedef struct HashTbl HashTbl;

typedef uint (*hashkey_t)(const void *);
typedef bool (*keyeq_t)(const void *, const void *);

HashTbl *HashTbl_new(hashkey_t);
HashTbl *HashTbl_newc(uint cap, hashkey_t);
void HashTbl_free(HashTbl *tbl, free_t keyfree, free_t valfree);

void *HashTbl_get(const HashTbl *tbl, const void *key, const keyeq_t keyeq);
void HashTbl_put(HashTbl *tbl, const void *key, const void *val);
void *HashTbl_pop(HashTbl *tbl, const void *key, const keyeq_t keyeq);

typedef void (*printkey_t)(const void*);
typedef void (*printval_t)(const void*);
void HashTbl_print(const HashTbl *tbl, const printkey_t printkey, const printval_t printval);
