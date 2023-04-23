#pragma once

#include <stdbool.h>
#include <stddef.h>


// signed int
#define MAX_INT_DIGITS 10

typedef struct MalDatum MalDatum;

// mal linked list
struct Node {
    MalDatum *value;
    struct Node *next;
};
typedef struct List {
    size_t len;
    struct Node *head;
    struct Node *tail;
} List;

List *List_new();

/* Frees the memory allocated for each Node of the list including the MalDatums they point to. */
void List_free(List *);

void List_add(List *, MalDatum *);
MalDatum *List_ref(List *, size_t);
bool List_isempty(List *);

/* Returns a shallow copy of a list: the nodes are copied, but MalDatums they point to are not. */
List *List_copy(List *);

/* Returns a deep copy of a list: both the nodes and MalDatums they point to are copied. */
List *List_deep_copy(List *);


/*
 * supported types:
 * 1. integer - 32-bit signed int
 * 2. symbol  - string (max len 255) = char sym[256]
 * 3. list    - List *
 * 4. string  - pointer to dynamic char array
 * 5. procedures
 */
typedef enum MalType {
    INT, SYMBOL, LIST, STRING, INTPROC2
} MalType;

char *MalType_tostr(MalType type);

// Symbol ----------------------------------------
typedef struct Symbol {
    char name[256];
} Symbol;

Symbol *Symbol_new(char *name);
void Symbol_free(Symbol *);
bool Symbol_eq(Symbol *sym1, Symbol *sym2);

// Procedures ----------------------------------------
typedef int (*intproc2_t)(int,int);

/* represents a dynamic mal type, which is determined by looking at the "tag" ('type' member) */
typedef struct MalDatum {
    MalType type;
    union {
        int i;
        Symbol *sym;
        List *list;
        char *string;
        intproc2_t intproc2;
    } value;
} MalDatum;

// constructors
MalDatum *MalDatum_new_int(const int);
MalDatum *MalDatum_new_sym(Symbol *);
MalDatum *MalDatum_new_list(List *);
MalDatum *MalDatum_new_string(const char *);
MalDatum *MalDatum_new_intproc2(const intproc2_t);

bool MalDatum_istype(MalDatum *, MalType);
void MalDatum_free(MalDatum *);
MalDatum *MalDatum_copy(MalDatum *);
MalDatum *MalDatum_deep_copy(MalDatum *);
