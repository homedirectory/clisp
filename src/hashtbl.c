#include <stdio.h>
#include <stdlib.h>
#include "hashtbl.h"
#include "common.h"

#define DEFAULT_CAPACITY 16
#define SIZE_THRESH_RATIO 0.75
#define GROW_RATIO 2

typedef struct Bucket {
    const void *key;
    const void *val;
    struct Bucket *next;
} Bucket;

static Bucket *Bucket_new(const void *key, const void *val) {
    Bucket *bkt = malloc(sizeof(Bucket));
    bkt->key = key;
    bkt->val = val;
    bkt->next = NULL;
    return bkt;
}

static void Bucket_free(Bucket *bkt, const free_t keyfree, const free_t valfree)
{
    Bucket *b = bkt;
    while (b) {
        keyfree((void*) b->key);
        valfree((void*) b->val);
        Bucket *tmp = b;
        b = b->next;
        free(tmp);
    }
}

static void *Bucket_find(const Bucket *bkt, const void *key, const keyeq_t keyeq)
{
    while (bkt) {
        if (keyeq(bkt->key, key)) 
            return (void*) bkt->val;
        bkt = bkt->next;
    }

    return NULL;
}

typedef struct HashTbl {
    uint size;
    uint cap;
    Bucket **buckets;
    hashkey_t hashkey;
} HashTbl;

HashTbl *HashTbl_new(hashkey_t hashkey)
{
    return HashTbl_newc(DEFAULT_CAPACITY, hashkey);
}

HashTbl *HashTbl_newc(uint cap, hashkey_t hashkey)
{
    HashTbl *tbl = malloc(sizeof(HashTbl));
    tbl->buckets = malloc(sizeof(Bucket*) * cap);
    for (uint i = 0; i < cap; i++) {
        tbl->buckets[i] = NULL;
    }
    tbl->size = 0;
    tbl->cap = cap;
    tbl->hashkey = hashkey;
    return tbl;
}

void HashTbl_free(HashTbl *tbl, free_t keyfree, free_t valfree)
{
    for (uint i = 0; i < tbl->cap; i++) {
        Bucket *bkt = tbl->buckets[i];
        if (bkt)
            Bucket_free(bkt, keyfree, valfree);
    }
    free(tbl->buckets);
    free(tbl);
}

static void try_grow(HashTbl *tbl)
{
    if (tbl->size >= SIZE_THRESH_RATIO * tbl->cap) {
        uint newcap = tbl->cap * GROW_RATIO;
        tbl->buckets = realloc(tbl->buckets, sizeof(Bucket*) * newcap);
        // NULL-out added memory
        for (uint i = tbl->cap; i < newcap; i++)
            tbl->buckets[i] = NULL;
        tbl->cap = newcap;
    }
}

void *HashTbl_get(const HashTbl *tbl, const void *key, const keyeq_t keyeq)
{
    uint idx = tbl->hashkey(key) % tbl->cap;
    Bucket *bkt = tbl->buckets[idx];
    if (!bkt)
        return NULL;
    else
        return Bucket_find(bkt, key, keyeq);
}

void HashTbl_put(HashTbl *tbl, const void *key, const void *val)
{
    try_grow(tbl);

    Bucket *bkt_new = Bucket_new(key, val);

    uint idx = tbl->hashkey(key) % tbl->cap;
    Bucket *bkt = tbl->buckets[idx];
    if (!bkt)
        tbl->buckets[idx] = bkt_new;
    else {
        bkt_new->next = bkt;
        tbl->buckets[idx] = bkt_new;
    }

    tbl->size += 1;
}

void *HashTbl_pop(HashTbl *tbl, const void *key)
{
    FATAL("UNIMPLEMENTED");
}
