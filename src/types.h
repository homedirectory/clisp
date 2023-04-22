#pragma once

#include <stdbool.h>
#include <stddef.h>


// signed int
#define MAX_INT_DIGITS 10

struct Node {
    void *value; // this is always MalDatum *
    struct Node *next;
};


// mal linked list
typedef struct List {
    size_t len;
    struct Node *head;
    struct Node *tail;
} List;


List *List_new();
void List_add(List *, void *);
void List_free(List *);


typedef struct Symbol {
    char name[256];
} Symbol;

Symbol *Symbol_new(char *name);
void Symbol_free(Symbol *);


/*
 * supported types:
 * 1. integer - 32-bit signed int
 * 2. symbol  - string (max len 255) = char sym[256]
 * 3. list    - List *
 * 4. string  - pointer to dynamic char array
 */
typedef enum MalType {
    INT, SYMBOL, LIST, STRING
} MalType;

char *MalType_tostr(MalType type);

typedef struct MalDatum {
    MalType type;
    union {
        int i;
        Symbol *sym;
        List *list;
        char *string;
    } value;
} MalDatum;

MalDatum *MalDatum_new_int(const int);
MalDatum *MalDatum_new_sym(Symbol *);
MalDatum *MalDatum_new_list(List *);
MalDatum *MalDatum_new_string(const char *);

bool MalDatum_istype(MalDatum *, MalType);

void MalDatum_free(MalDatum *);
