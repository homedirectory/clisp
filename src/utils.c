#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#define CAPACITY_INCR_RATIO 1.5
#define DEFAULT_CAPACITY 10

static void resize(Arr* arr, const size_t new_cap) {
    arr->items = realloc(arr->items, new_cap * sizeof(*(arr->items)));
    arr->cap = new_cap;
}

Arr *Arr_new() {
    return Arr_newn(DEFAULT_CAPACITY);
}

Arr *Arr_newn(const size_t cap) {
    Arr *arr = malloc(sizeof(Arr));
    arr->len = 0;
    arr->cap = cap;
    arr->items = malloc(cap * sizeof(void*));
    return arr;
}

Arr *Arr_copy(const Arr *arr)
{
    if (arr == NULL) {
        LOG_NULL(arr);
        return NULL;
    }

    Arr *copy = Arr_newn(arr->cap);
    copy->len = arr->len;

    for (int i = 0; i < arr->len; i++) {
        copy->items[i] = arr->items[i];
    }

    return copy;
}

Arr *Arr_copyf(const Arr *arr, const copier_t copier) {
    if (arr == NULL) {
        LOG_NULL(arr);
        return NULL;
    }

    Arr *copy = Arr_newn(arr->cap);
    copy->len = arr->len;

    for (int i = 0; i < arr->len; i++) {
        copy->items[i] = copier(arr->items[i]);
    }

    return copy;
}

size_t Arr_add(Arr *arr, void *ptr) {
    if (arr->cap == arr->len) {
        resize(arr, arr->cap * CAPACITY_INCR_RATIO);
    }
    (arr->items)[arr->len] = ptr;
    arr->len += 1;
    return arr->len;
}

void *Arr_replace(Arr *arr, size_t idx, void *ptr) {
    if (idx >= arr->cap) {
        LOG("idx >= arr->cap");
        return NULL;
    }

    void *old = arr->items[idx];
    arr->items[idx] = ptr;
    return old;
}

void *Arr_get(const Arr *arr, size_t idx) {
    if (idx >= arr->len)
        return NULL;
    return arr->items[idx];
}

void *Arr_last(const Arr *arr)
{
    return arr->len == 0 ? NULL : arr->items[arr->len - 1];
}

void Arr_free(Arr *arr) {
    free(arr->items);
    free(arr);
}

void Arr_freep(Arr *arr, free_t freer) {
    for (int i = 0; i < arr->len; i++) {
        freer(arr->items[i]);
    }
    Arr_free(arr);
}

int Arr_find(const Arr *arr, const void *ptr) {
    if (arr == NULL) {
        LOG_NULL(arr);
        return -1;
    }
    if (ptr == NULL) {
        LOG_NULL(ptr);
        return -1;
    }

    for (size_t i = 0; i < arr->len; i++) {
        if (arr->items[i] == ptr)
            return i;
    }
    return -1;
}

int Arr_findf(const Arr *arr, const void *ptr, const equals_t eq) {
    if (arr == NULL) {
        LOG_NULL(arr);
        return -1;
    }
    if (ptr == NULL) {
        LOG_NULL(ptr);
        return -1;
    }

    for (size_t i = 0; i < arr->len; i++) {
        if (eq(arr->items[i], ptr))
            return i;
    }
    return -1;
}

void Arr_foreach(const Arr *arr, const unary_void_t func)
{
    if (arr == NULL) FATAL("arr is NULL");

    for (size_t i = 0; i < arr->len; i++) {
        func(arr->items[i]);
    }
}

// String utilities ----------------------------------------

char *dyn_strcpy(const char *s) {
    char *cpy = calloc(strlen(s) + 1, sizeof(char));
    strcpy(cpy, s);
    return cpy;
}

char *dyn_strncpy(const char *s, size_t n) {
    char *cpy = calloc(n + 1, sizeof(char));
    memcpy(cpy, s, n);
    cpy[n] = '\0';
    return cpy;
}

/* Like strchr but looks for the first occurence of one of the chars.  
 * *chars must be a null-terminated string.
 * */
const char *strchrs(const char *str, const char *chars) {
    while (*str != '\0') {
        if (strchr(chars, *str))
            return str;
        ++str;
    }
    return NULL;
}

// returns the index of first occurence of c in str, otherwise -1
ssize_t stridx(const char *str, char c)
{
    const char *s = str;
    while (*str != '\0' && *str != c) str++;
    return *str == c ? str - s : -1;
}

short escape_char(unsigned char c) 
{
    switch (c) {
        case '"' : return '"';
        case '\'': return '\'';
        case '\n': return 'n';
        case '\t': return 't';
        case '\\': return '\\';
        case '\r': return 'r';
        case '\b': return 'b';
        case '\f': return 'f';
    }
    return -1;
}

unsigned char unescape_char(unsigned char c) 
{
    switch (c) {
        case 'n':
            return '\n';
        case 't':
            return '\t';
        case 'r':
            return '\r';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
    }
    return c;
}

char *str_escape(const char *src)
{
    //           0  1  2  3  4 
    // src       a  b  "  c  \n
    // escapes  -1 -1  " -1  n
    size_t src_len = strlen(src);
    short escapes[src_len];
    size_t n = 0; // amount of escaped characters
    for (size_t i = 0; i < src_len; i++) {
        short esc = escape_char(src[i]);
        escapes[i] = esc;
        if (esc != -1)
            n++;
    }

    char *out;
    out = malloc(sizeof(*out) * (src_len + n) + 1);
    size_t j = 0;
    for (size_t i = 0; i < src_len; i++, j++) {
        short esc = escapes[i];
        if (esc == -1)
            out[j] = src[i];
        else {
            out[j] = '\\';
            out[++j] = esc;
        }
    }
    out[j] = '\0';

    return out;
}

char *str_join(char *strings[], size_t n, const char *sep)
{
    if (n == 0) {
        DEBUG("n == 0");
        return NULL;
    }

    size_t sep_len = strlen(sep);
    size_t tot_len = sep_len * (n - 1);
    size_t lengths[n];
    for (size_t i = 0; i < n; i++) {
        size_t len = strlen(strings[i]);
        lengths[i] = len;
        tot_len += len;
    }

    char *out = calloc(tot_len + 1, sizeof(char));
    out[0] = '\0';

    size_t curr_len = 0;
    for (size_t i = 0; i < n - 1; i++) {
        memcpy(out + curr_len, strings[i], lengths[i]);
        curr_len += lengths[i];
        memcpy(out + curr_len, sep, sep_len);
        curr_len += sep_len;
    }
    // now the last one
    memcpy(out + curr_len, strings[n - 1], lengths[n - 1]);
    curr_len += lengths[n - 1];

    out[curr_len] = '\0';

    return out;
}

char *addr_to_str(const void *ptr)
{
    // each byte is 2 hex chars + 2 for "0x" prefix
    size_t n = sizeof(ptr) * 2 + 2;
    char *str = malloc(sizeof(str[0]) * (n + 1));
    sprintf(str, "%p", ptr);
    str[n] = '\0';
    return str;
}

bool streq(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0;
}

unsigned int hash_simple_str(const char *s)
{
    // simple and works (unique hash for each unique string)
    // collisions might happen only if str is longer than (2^32 - 1) / (2^8 - 1) = 16843009 bytes
    unsigned int h = *(s++);
    while (*s) h += *s++;
    return h;
}

// String assembler
#define STR_ASM_DEF_CAP 128
#define STR_ASM_GROW_RAT 1.75

StrAsm *StrAsm_init(StrAsm *sasm)
{
    return StrAsm_initsz(sasm, STR_ASM_DEF_CAP);
}

StrAsm *StrAsm_initsz(StrAsm *sasm, size_t cap)
{
    sasm->str = malloc(cap * sizeof(*(sasm->str)));
    sasm->len = 0;
    sasm->cap = cap;
    return sasm;
}

void StrAsm_destroy(StrAsm *sasm)
{
    free(sasm->str);
}

static bool StrAsm_hasroom(const StrAsm *sasm, size_t n)
{
    // need to remember about the last null-byte
    return sasm->cap - sasm->len - 1 >= n;
}

static void StrAsm_mkroom(StrAsm *sasm, size_t n)
{
    size_t newcap = (sasm->cap * STR_ASM_GROW_RAT) + n;
    sasm->str = realloc(sasm->str, newcap * sizeof(*(sasm->str)));
    sasm->cap = newcap;
}

void StrAsm_add(StrAsm *sasm, const char *s)
{
    StrAsm_addn(sasm, s, strlen(s));
}

void StrAsm_addn(StrAsm *sasm, const char *s, size_t n)
{
    if (n == 0) return;

    if (!StrAsm_hasroom(sasm, n))
        StrAsm_mkroom(sasm, n);

    memcpy(sasm->str + sasm->len, s, n);
    sasm->len += n;
    sasm->str[sasm->len] = 0;
}

void StrAsm_addc(StrAsm *sasm, char c)
{
    if (!StrAsm_hasroom(sasm, 1))
        StrAsm_mkroom(sasm, 1);

    sasm->str[sasm->len++] = c;
    sasm->str[sasm->len] = 0;
}

void StrAsm_drop(StrAsm *sasm, size_t n)
{
#ifdef _MAL_TRACE
    if (sasm->len < n)
        DEBUG("drop %zu, but len = %zu", n, sasm->len);
#endif
    sasm->len -= n;
}

size_t StrAsm_len(const StrAsm *sasm)
{
    return sasm->len;
}

char *StrAsm_str(const StrAsm *sasm)
{
    return sasm->str;
}

// File utilities ----------------------------------------
bool file_readable(const char *path)
{
    return access(path, R_OK) != -1;
}

char *file_to_str(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1)
        return NULL;

    static size_t chunk_size = 1024 * 1024;
    size_t tot_size = 0;
    char *buf = malloc(sizeof(*buf) * chunk_size);

    ssize_t nread;
    while (1) {
        nread = read(fd, buf, chunk_size);
        if (nread == -1) {
            close(fd);
            free(buf);
            return NULL;
        } else {
            tot_size += nread;
            // last chunk?
            if (nread < chunk_size) break;
            // make more room
            buf = realloc(buf, tot_size + (sizeof(*buf) * chunk_size));
        }
    }
    close(fd);

    if (tot_size % chunk_size == 0) { // filled up to the brim?
        // make room for null-byte
        buf = realloc(buf, tot_size++);
    } else {
        // get rid of unused space
        buf = realloc(buf, tot_size);
    }
    buf[tot_size] = '\0';

    return buf;
}

// -----------------------------------------------------------------------------
// Miscellaneous ---------------------------------------------------------------
char itoa(int i)
{
    assert(i >= 0 && i <= 9);
    return i + 48;
}

void strnrev(char *s, size_t n)
{
    size_t i, j;
    for (i = 0, j = n - 1; i < n / 2; i++, j--) {
        char c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

char *ltos(long l, char *dst)
{
    if (l < 0) {
        *dst++ = '-';
        l *= -1;
    }

    char *s = dst;
    size_t i = 0;

    while (l != 0) {
        *dst++ = itoa(l % 10);
        l /= 10;
        i++;
    }

    *dst = 0;
    strnrev(s, i);

    return dst;
}

// int main(int argc, char **argv)
// {
//     {
//         char *s = dyn_strcpy("hello");
//         strnrev(s, strlen(s));
//         printf("%s\n", s);
//     }

//     {
//         long l = 5435843;
//         char buf[10];
//         ltos(l, buf);
//         printf("%ld == %s\n", l, buf);

//         long nl = -5435843;
//         ltos(nl, buf);
//         printf("%ld == %s\n", nl, buf);
//     }
// }

/*
int main(int argc, char **argv) {
    Arr *arr = Arr_new();
    {
        int *x = malloc(sizeof(int));
        *x = 50;
        Arr_add(arr, x);
    }
    int *x = Arr_get(arr, 0);
    printf("%d\n", *x);
    Arr_free(arr);
}
*/

// int main(int argc, char **argv)
// {
//     StrAsm sa;
//     StrAsm_init(&sa);
//     StrAsm_add(&sa, "hello");
//     StrAsm_addc(&sa, ' ');
//     StrAsm_addn(&sa, "world", 2);
//     StrAsm_add(&sa, "rld");
//     char *s = StrAsm_str(&sa);
//     printf("%s\n", s);
//     free(s);
// }
