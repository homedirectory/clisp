#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "utils.h"


// signed int
#define MAX_INT_DIGITS 10

typedef struct MalDatum MalDatum;

// -----------------------------------------------------------------
// List ------------------------------------------------------------

struct Node {
    long refc; // reference count
    MalDatum *value;
    struct Node *next;
};
typedef struct {
    size_t len;
    struct Node *head;
    struct Node *tail;
} List;

List *List_new();
size_t List_len(const List *list);

/* Frees the memory allocated for each Node of the list including the MalDatums they point to. */
void List_free(List *);

// shallow free does not free the values pointed to by nodes
void List_shlw_free(List *);

void List_add(List *, MalDatum *);

// creates a new list headed by datum followed by the elements of the given list
List *List_cons_new(List *list, MalDatum *datum);
// creates a new list containing the tail of the given list
List *List_rest_new(List *list);

void List_append(List *dst, const List *src);

MalDatum *List_ref(const List *, size_t);

List *List_empty();
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
 * 8. procedure
 * 9. atom
 * 10. exception (exn for short)
 */
typedef enum MalType {
    // --- code & data ---
    INT, 
    SYMBOL, 
    LIST, 
    STRING, // code-form may differ from data-form (e.g., escaped characters)
    // --- only data ---
    NIL, 
    TRUE, 
    FALSE, 
    PROCEDURE,
    ATOM,
    EXCEPTION,
    //
    MT_COUNT
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
typedef MalDatum* (*builtin_apply_t)(const Proc*, const Arr *args, MalEnv *env);

struct Proc {
    char *name;
    int argc; // amount of mandatory arguments
    bool variadic; // accepts more arguments after mandatory ones (default: false)
    /* Declared parameter names, which include mandatory arguments and, if this procedure
     * is variadic, then the name of the variadic declared parameter. So the amount of these
     * is given by (+ argc (if variadic 1 0)).
     */
    Arr *params; // of *Symbol (makes sense only for MAL procedures) 
    bool builtin;
    bool macro;
    union {
        Arr *body; // of *MalDatum
        builtin_apply_t apply; // function pointer for built-in procedures
    } logic;
    /*const*/ MalEnv *env; // the enclosing environment in which this MAL procedure was defined
};

// a constructor for language-defined procedures
// params and body are copied
Proc *Proc_new (
        const char *name, 
        int argc, 
        bool variadic, 
        const Arr *params, 
        const Arr *body, 
        MalEnv *env
        );

Proc *Proc_new_lambda (
        int argc,
        bool variadic,
        const Arr *params,
        const Arr *body, 
        MalEnv *env
        );

// a constructor for built-in procedures
Proc *Proc_builtin(const char *name, int argc, bool variadic, const builtin_apply_t apply);

void Proc_free(Proc *proc);
Proc *Proc_copy(const Proc *proc);

bool Proc_eq(const Proc *, const Proc *);

// returns a dynamically allocated string containing the procedure's name
char *Proc_name(const Proc *proc);

bool Proc_is_named(const Proc *);
bool Proc_is_macro(const Proc *);


// Atom -----------------------------------------------------------------------
// An atom holds a reference to a single Mal value of any type; it supports
// reading that Mal value and modifying the reference to point to another Mal
// value. Note that this is the only Mal data type that is mutable
typedef struct {
    MalDatum *datum;
} Atom;

Atom *Atom_new(MalDatum *);
void Atom_free(Atom *);
Atom *Atom_copy(const Atom *);
// 2 Atoms are equal only if they point to the same value
bool Atom_eq(const Atom *, const Atom *);

void Atom_reset(Atom *, MalDatum *);

// Exception -------------------------------------------------------------------
typedef struct {
    MalDatum *datum;
} Exception;

// datum is copied
Exception *Exception_new(const MalDatum *);

void Exception_free(Exception *);
Exception *Exception_copy(const Exception *);
bool Exception_eq(const Exception *, const Exception *);

Exception *thrown_copy();

void throw(const MalDatum *);
void throwf(const char *fmt, ...);
bool didthrow();

void error(const char *fmt, ...);

// MalDatum -------------------------------------------------------------------
/* represents a dynamic mal type, which is determined by looking at the "tag" ('type' member) */
typedef struct MalDatum {
    long refc; // reference count
    MalType type;
    union {
        int i;
        Symbol *sym;
        List *list;
        char *string;
        Proc *proc;
        Atom *atom;
        Exception *exn;
    } value;
} MalDatum;

// *** functions for managing reference count of MalDatum
// increments ref count (use when you need to *own* memory)
void MalDatum_own(MalDatum *);
// decrements ref count (use when you want to *release* owned memory)
void MalDatum_release(MalDatum *);

// singletons
MalDatum *MalDatum_nil();
MalDatum *MalDatum_true();
MalDatum *MalDatum_false();
MalDatum *MalDatum_empty_list();

bool MalDatum_isnil(MalDatum *datum);
bool MalDatum_isfalse(MalDatum *datum);

// constructors
MalDatum *MalDatum_new_int(const int);
MalDatum *MalDatum_new_sym(Symbol *);
MalDatum *MalDatum_new_list(List *);
MalDatum *MalDatum_new_string(const char *);
MalDatum *MalDatum_new_proc(Proc *);
MalDatum *MalDatum_new_atom(Atom *);
MalDatum *MalDatum_new_exn(Exception *);

bool MalDatum_istype(const MalDatum *, MalType);
bool MalDatum_islist(const MalDatum *);
void MalDatum_free(MalDatum *);
void MalDatum_release_free(MalDatum *);
MalDatum *MalDatum_copy(const MalDatum *);
MalDatum *MalDatum_deep_copy(const MalDatum *);

bool MalDatum_eq(const MalDatum *, const MalDatum *);
