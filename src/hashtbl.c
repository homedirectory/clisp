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

static uint HashTbl_keyidx(const HashTbl *tbl, const void *key)
{
    return tbl->hashkey(key) % tbl->cap;
}

static void try_grow(HashTbl *tbl)
{
    if (tbl->size < SIZE_THRESH_RATIO * tbl->cap)
        return;

    uint newcap = tbl->cap * GROW_RATIO;
    DEBUG("Growing HashTbl %u -> %u", tbl->cap, newcap);
    uint oldcap = tbl->cap;
    tbl->cap = newcap;

    // 1. allocate memory for a new array of buckets
    // 2. recalculate hashes to get a new position of each bucket and put it
    // into the new array
    // 3. free memory used by the previous bucket array

    Bucket **new_buckets = malloc(sizeof(Bucket*) * newcap);
    for (uint i = 0; i < newcap; i++)
        new_buckets[i] = NULL;

    for (uint i = 0; i < oldcap; i++) {
        Bucket *bkt = tbl->buckets[i];
        if (!bkt)
            continue;

        // go through all buckets at the current index
        while (bkt) {
            Bucket *next_bkt = bkt->next;

            uint newidx = HashTbl_keyidx(tbl, bkt->key);
            Bucket *curr_bkt = new_buckets[newidx];
            new_buckets[newidx] = bkt;
            bkt->next = curr_bkt;

            bkt = next_bkt;
        }
    }

    free(tbl->buckets);
    tbl->buckets = new_buckets;

    DEBUG("Grew HashTbl %u -> %u\n", oldcap, newcap);
}

void *HashTbl_get(const HashTbl *tbl, const void *key, const keyeq_t keyeq)
{
    uint idx = HashTbl_keyidx(tbl, key);
    Bucket *bkt = tbl->buckets[idx];
    if (!bkt)
        return NULL;
    else
        return Bucket_find(bkt, key, keyeq);
}

void *HashTbl_put(HashTbl *tbl, const void *key, const void *val, const keyeq_t keyeq)
{
    try_grow(tbl);

    // TODO optimise: compute hash only once
    void *popd = HashTbl_pop(tbl, key, keyeq);

    Bucket *bkt_new = Bucket_new(key, val);

    uint idx = HashTbl_keyidx(tbl, key);
    Bucket *bkt = tbl->buckets[idx];
    if (!bkt)
        tbl->buckets[idx] = bkt_new;
    else {
        bkt_new->next = bkt;
        tbl->buckets[idx] = bkt_new;
    }

    tbl->size += 1;

    return popd;
}

void *HashTbl_pop(HashTbl *tbl, const void *key, const keyeq_t keyeq)
{
    uint idx = HashTbl_keyidx(tbl, key);
    Bucket *bkt = tbl->buckets[idx];
    const void *val_out = NULL;

    if (bkt) {
        Bucket *prev = NULL;
        while (bkt) {
            if (keyeq(bkt->key, key)) {
                val_out = bkt->val;

                if (prev)
                    prev->next = bkt->next;
                else {
                    // this was the 1st key in the bucket
                    tbl->buckets[idx] = bkt->next;
                }

                // free key?
                free(bkt);
                tbl->size -= 1;
                break;
            }
            prev = bkt;
            bkt = bkt->next;
        }
    }

    return (void*) val_out;
}

unsigned int HashTbl_size(const HashTbl *tbl)
{
    return tbl->size;
}

void HashTbl_keys(const HashTbl *tbl, void **arr)
{
    for (uint i = 0; i < tbl->cap; i++) {
        Bucket *bkt = tbl->buckets[i];
        if (bkt == NULL)
            continue;
        for (Bucket *b = bkt; b != NULL; b = b->next) {
            *arr++ = (void*) b->key;
        }
    }
}

void HashTbl_values(const HashTbl *tbl, void **arr)
{
    for (uint i = 0; i < tbl->cap; i++) {
        Bucket *bkt = tbl->buckets[i];
        if (bkt == NULL)
            continue;
        for (Bucket *b = bkt; b != NULL; b = b->next) {
            *arr++ = (void*) b->val;
        }
    }
}

void HashTbl_print(const HashTbl *tbl, const printkey_t printkey, const printval_t printval)
{
    for (size_t i = 0; i < tbl->cap; i++) {
        Bucket *bkt = tbl->buckets[i];
        if (!bkt)
            continue;
        printf("%zu\n", i);
        for (Bucket *b = bkt; b != NULL; b = b->next) {
            printf("  ");
            printkey(b->key);
            printf(" : ");
            printval(b->val);
            printf("\n");
        }
    }
}
