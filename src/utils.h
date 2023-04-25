#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "common.h"

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

// Like Arr_free but also applies the given free proc to each array item
void Arr_freep(Arr *arr, free_t freer);

typedef void*(*copier_t)(void*);
Arr *Arr_copy(const Arr *arr, const copier_t copier);

size_t Arr_add(Arr*, void*);
void *Arr_replace(Arr*, size_t, void*);
void *Arr_get(const Arr*, size_t idx);
int Arr_find(const Arr*, const void*);

typedef bool (*equals_t)(const void*, const void*);
// Finds *ptr in *arr using the equals? function eq
int Arr_findf(const Arr *arr, const void *ptr, const equals_t);

// String utilities ----------------------------------------
char *dyn_strcpy(const char *);
char *dyn_strncpy(const char *s, size_t n);
const char *strchrs(const char *str, const char *chars);
short escape_char(unsigned char c);
unsigned char unescape_char(unsigned char c);
char *str_escape(const char *src);
