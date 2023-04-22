#pragma once

#include <stddef.h>

// this is a dynamic array of pointers
// it only makes sense to store pointers to dynamically allocated memory in this struct
typedef struct Arr {
    size_t len;
    size_t cap;
    void **items;
} Arr;

Arr *Arr_new();
Arr *Arr_newn(const size_t);
void Arr_free(const Arr*);
size_t Arr_add(Arr*, void*);
void *Arr_replace(Arr*, size_t, void*);
void *Arr_get(Arr*, size_t idx);
size_t Arr_find(Arr*, void*);

char *dyn_strcpy(const char *);
char *dyn_strncpy(const char *s, size_t n);

char *strchrs(char *str, const char *chars);
