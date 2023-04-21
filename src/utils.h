#pragma once

#include <stddef.h>

struct Arr {
    size_t len;
    size_t cap;
    void **items;
};

typedef struct Arr Arr;

Arr *Arr_new();
Arr *Arr_newn(const size_t);
size_t Arr_add(Arr*, void*);
void *Arr_get(Arr*, size_t idx);
void Arr_free(const Arr*);

char *dyn_strcpy(const char *);

char *strchrs(char *str, const char *chars);
