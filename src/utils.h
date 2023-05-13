#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "common.h"
#include <sys/types.h>

// -----------------------------------------------------------------------------
// Array -----------------------------------------------------------------------

// this is a dynamic array of pointers
// it only makes sense to store pointers to dynamically allocated memory in this struct
typedef struct Arr {
    size_t len;
    size_t cap;
    void *items[];
} Arr;

Arr *Arr_new();
Arr *Arr_newn(size_t);

void Arr_free(Arr *arr);

// Like Arr_free but also applies the given free proc to each array item
void Arr_freep(Arr *arr, free_t freer);

Arr *Arr_copy(const Arr *arr);
typedef void*(*copier_t)(void*);
Arr *Arr_copyf(const Arr *arr, const copier_t copier);

size_t Arr_add(Arr*, void*);
void *Arr_replace(Arr*, size_t, void*);
void *Arr_get(const Arr*, size_t idx);
void *Arr_last(const Arr*);
int Arr_find(const Arr*, const void*);

typedef bool (*equals_t)(const void*, const void*);
// Finds *ptr in *arr using the equals? function eq
int Arr_findf(const Arr *arr, const void *ptr, const equals_t);

typedef void (*unary_void_t)(void*);
void Arr_foreach(const Arr *arr, const unary_void_t func);

// -----------------------------------------------------------------------------
// String utilities ------------------------------------------------------------

char *dyn_strcpy(const char *);
char *dyn_strncpy(const char *s, size_t n);
const char *strchrs(const char *str, const char *chars);
// returns the index of first occurence of c in str, otherwise -1
ssize_t stridx(const char *str, char c);
short escape_char(unsigned char c);
unsigned char unescape_char(unsigned char c);
char *str_escape(const char *src);
char *str_join(const char * const strings[], size_t n, const char *sep);
char *addr_to_str(void *ptr);
bool streq(const char *s1, const char *s2);

// string assembler ------------------------------------------------------------
typedef struct StrAsm {
    char *str;
    size_t len;
    size_t cap;
} StrAsm;

StrAsm *StrAsm_init(StrAsm *sasm);
StrAsm *StrAsm_initsz(StrAsm *sasm, size_t cap);
void StrAsm_destroy(StrAsm *sasm);
void StrAsm_add(StrAsm *sasm, const char *s);
void StrAsm_addn(StrAsm *sasm, const char *s, size_t n);
void StrAsm_addc(StrAsm *sasm, char c);
void StrAsm_drop(StrAsm *sasm, size_t n);
size_t StrAsm_len(const StrAsm *sasm);
char *StrAsm_str(const StrAsm *sasm);

// -----------------------------------------------------------------------------
// File utilities --------------------------------------------------------------
 
bool file_readable(const char *path);
char *file_to_str(const char *path);

// -----------------------------------------------------------------------------
// Miscellaneous ---------------------------------------------------------------
char itoa(int i);
void strnrev(char *s, size_t n);
char *ltos(long l, char *dst);
