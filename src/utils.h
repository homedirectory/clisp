#pragma once

#include <stddef.h>
#include <stdbool.h>

// this is a dynamic array of pointers
// it only makes sense to store pointers to dynamically allocated memory in this struct
typedef struct Arr {
    size_t len;
    size_t cap;
    void **items;
} Arr;

Arr *Arr_new();
Arr *Arr_newn(size_t);
void Arr_free(Arr *arr);
size_t Arr_add(Arr*, void*);
void *Arr_replace(Arr*, size_t, void*);
void *Arr_get(Arr*, size_t idx);
int Arr_find(const Arr*, const void*);

typedef bool (*equals_t)(const void*, const void*);
// Finds *ptr in *arr using the equals? function eq
int Arr_findf(const Arr *arr, const void *ptr, const equals_t);

char *dyn_strcpy(const char *);
char *dyn_strncpy(const char *s, size_t n);

const char *strchrs(const char *str, const char *chars);
