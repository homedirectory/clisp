#pragma once

#include <stdbool.h>
#include <stddef.h>


// unsigned int
#define MAX_INT_DIGITS 10

struct Node {
    void *value; // this is always MalDatum *
    struct Node *next;
};

// linked list
struct List {
    size_t len;
    struct Node *head;
    struct Node *tail;
};
typedef struct List List;


List *List_new();
void List_add(List *, void *);
void List_free(List *);
char *List_tostr(List *);


/*
 * supported types:
 * 1. integer - 32-bit signed int
 * 2. symbol  - string (max len 255) = char sym[256]
 * 3. list    - List *
 */
enum MalType {
    INT, SYMBOL, LIST
};
typedef enum MalType MalType;

struct MalDatum {
    MalType type;
    union {
        int i;
        char sym[256];
        List *list;
    } value;
};
typedef struct MalDatum MalDatum;

MalDatum *MalDatum_new_int(const int);
MalDatum *MalDatum_new_sym(const char *);
MalDatum *MalDatum_new_list(List *);

bool MalDatum_istype(MalDatum *, MalType);

void MalDatum_free(MalDatum *);
char *MalDatum_tostr(MalDatum *);
