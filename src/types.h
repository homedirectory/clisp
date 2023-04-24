#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "utils.h"


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
size_t List_len(const List *list);

/* Frees the memory allocated for each Node of the list including the MalDatums they point to. */
void List_free(List *);

void List_add(List *, MalDatum *);
MalDatum *List_ref(const List *, size_t);
bool List_isempty(const List *);

/* Returns a shallow copy of a list: the nodes are copied, but MalDatums they point to are not. */
List *List_copy(const List *);

/* Returns a deep copy of a list: both the nodes and MalDatums they point to are copied. */
List *List_deep_copy(const List *);

// tests two lists for equality
bool List_eq(const List *, const List *);


/*
 * supported types:
 * 1. integer - 32-bit signed int
 * 2. symbol  - string (max len 255) = char sym[256]
 * 3. list    - List *
 * 4. string  - pointer to dynamic char array
 * 5. nil
 * 6. true
 * 7. false
 * 8. procedures
 */
typedef enum MalType {
    INT, SYMBOL, LIST, STRING, NIL, TRUE, FALSE, PROCEDURE
} MalType;

char *MalType_tostr(MalType type);

// Symbol ----------------------------------------
typedef struct Symbol {
    char name[256];
} Symbol;

Symbol *Symbol_new(const char *name);
void Symbol_free(Symbol *);
bool Symbol_eq(const Symbol *sym1, const Symbol *sym2);
bool Symbol_eq_str(const Symbol *sym1, const char *str);
Symbol *Symbol_copy(const Symbol *sym);

// Procedures ----------------------------------------
// *** A note on procedure application ***
// Racket and Python evaluate function arguments before checking the arity.
// I shall do this the other way around

typedef struct Proc Proc;
typedef struct MalEnv MalEnv; // from env.h

// the type of built-in functions (e.g., list, empty?, numeric ones, etc.)
// args - array of *MalDatum
typedef MalDatum* (*builtin_apply_t)(Proc*, Arr *args);

struct Proc {
    int argc; // amount of mandatory arguments
    bool variadic; // accepts more arguments after mandatory ones (default: false)
    /* Declared parameter names, which include mandatory arguments and, if this procedure
     * is variadic, then the name of the variadic declared parameter. So the amount of these
     * is given by (+ argc (if variadic 1 0)).
     */
    Arr *params; // of *Symbol (makes sense only for MAL procedures) 
    bool builtin;
    union {
        List *body;
        builtin_apply_t apply; // function pointer for built-in procedures
    } logic;
};

// a constructor for language-defined procedures
// params and body are copied
Proc *Proc_new(int argc, bool variadic, const Arr *params, const List *body);

// a constructor for built-in procedures
Proc *Proc_builtin(int argc, bool variadic, const builtin_apply_t apply);

void Proc_free(Proc *proc);
Proc *Proc_copy(const Proc *proc);

bool Proc_eq(const Proc *, const Proc *);

/* represents a dynamic mal type, which is determined by looking at the "tag" ('type' member) */
typedef struct MalDatum {
    MalType type;
    union {
        int i;
        Symbol *sym;
        List *list;
        char *string;
        Proc *proc;
    } value;
} MalDatum;

MalDatum *MalDatum_nil();
MalDatum *MalDatum_true();
MalDatum *MalDatum_false();

bool MalDatum_isnil(MalDatum *datum);
bool MalDatum_isfalse(MalDatum *datum);

// constructors
MalDatum *MalDatum_new_int(const int);
MalDatum *MalDatum_new_sym(Symbol *);
MalDatum *MalDatum_new_list(List *);
MalDatum *MalDatum_new_string(const char *);
MalDatum *MalDatum_new_proc(Proc *);

bool MalDatum_istype(const MalDatum *, MalType);
void MalDatum_free(MalDatum *);
MalDatum *MalDatum_copy(const MalDatum *);
MalDatum *MalDatum_deep_copy(const MalDatum *);

bool MalDatum_eq(const MalDatum *, const MalDatum *);
