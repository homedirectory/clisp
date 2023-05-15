#pragma once

#include <stdbool.h>
#include "utils.h"


// pre-declare
typedef struct _LispDatum _LispDatum;

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// LispDatum
// external view of a polymorphic type;
// works because pointer to a concrete type is a pointer to its 1st member,
// which must have type _LispDatum*
typedef _LispDatum* LispDatum;

// datum type identifiers
typedef enum LispType {
    SYMBOL,
    LIST,
    NUMBER,
    STRING,
    NIL, FALSE, TRUE,
    PROCEDURE,
    ATOM,
    EXCEPTION,
    TYPE_COUNT
} LispType;

const char *LispType_name(LispType type);

// ---- generic method declarations ----
// these are part of the DtmMethods structure below and must be implemented by
// concrete types; however, if a concrete type wishes not to implement a method,
// the default implementation might be specified (if it exists of course) 
// e.g., .own = LispDatum_own_dflt

typedef LispType (*dtm_type_ft)(const LispDatum *);
// returns a unique value identifying the runtime type of the datum;
// instead of storing the type in the LispDatum struct (in a tag-like fashion),
// the object itself responds to the message about its type 
LispType LispDatum_type(const LispDatum *);
bool LispDatum_istype(const LispDatum *, LispType);

typedef void (*dtm_free_ft)(LispDatum *);
void LispDatum_free(LispDatum *);

typedef bool (*dtm_eq_ft)(const LispDatum*, const LispDatum *);
bool LispDatum_eq(const LispDatum*, const LispDatum *);

typedef char* (*dtm_typename_ft)(const LispDatum *);
char *LispDatum_typename(const LispDatum *);

typedef LispDatum* (*dtm_copy_ft)(const LispDatum *);
LispDatum *LispDatum_copy(const LispDatum *);

// --- functions for managing reference count of LispDatum ---
// they are generic, because some data types, such as Nil, are singletons,
// thus need to ignore any modifications to their ref counts

// increments ref count (use when you need to *own* memory)
typedef void (*dtm_own_ft)(LispDatum *);
void LispDatum_own(LispDatum *);

// decrements ref count (use when you want to *release* owned memory)
typedef void (*dtm_rls_ft)(LispDatum *);
void LispDatum_rls(LispDatum *);


typedef struct {
    dtm_type_ft type;
    dtm_free_ft free;
    dtm_eq_ft eq;
    dtm_typename_ft typename;
    dtm_copy_ft copy;
    dtm_own_ft own;
    dtm_rls_ft rls;
} DtmMethods;

// -- generic method declarations that are shared by concrete types,
// similar to Java final methods

// LispDatum_release + LispDatum_free
void LispDatum_rls_free(LispDatum *);

// returns the ref. count of the given datum
long LispDatum_refc(const LispDatum *dtm);
// --------


// internal super-type;
// each concrete type must declare its 1st member as a pointer to this type
// e.g. struct List { _LispDatum *super; ... }
typedef struct _LispDatum {
    const DtmMethods *methods;
    long refc; // reference count
} _LispDatum;

// internal
//static _LispDatum *_LispDatum_new(const DtmMethods *);
//static long _LispDatum_refc(const _LispDatum *_dtm);
// default implementations of ref. management methods
//static void _LispDatum_free(_LispDatum *);
//static void _LispDatum_own(_LispDatum *);
//static void _LispDatum_rls(_LispDatum *);


// -----------------------------------------------------------------------------
// Symbol < LispDatum

void init_symbol_table();
void free_symbol_table();

typedef struct {
    /*void*/ _LispDatum *super;
    char *name;
} Symbol;

// generic method implementations
LispType Symbol_type();
void Symbol_free(Symbol *sym);
bool Symbol_eq(const Symbol *s1, const Symbol *s2);
char *Symbol_typename(const Symbol *sym);
Symbol *Symbol_copy(const Symbol *sym);

// Symbol-specific methods
Symbol* Symbol_intern(const char *name);
bool Symbol_eq_str(const Symbol *sym, const char *str);
const char *Symbol_name(const Symbol *sym);


// -----------------------------------------------------------------------------
// List < LispDatum

struct Node {
    long refc; // reference count
    LispDatum *value;
    struct Node *next;
};
typedef struct {
    /*void*/ _LispDatum *super;
    size_t len;
    struct Node *head;
    struct Node *tail;
} List;

// generic method implementations
LispType List_type();
void List_free(List *list);
bool List_eq(const List *l1, const List *l2);
char *List_typename(const List *list);
/* Returns a deep copy of a list: both the nodes and LispDatums they point to are copied. */
List *List_copy(const List *list);

// List methods
List *List_new();
size_t List_len(const List *list);
bool List_isempty(const List *list);
void List_add(List *list, LispDatum *dtm);
LispDatum *List_ref(const List *list, size_t idx);
// shallow copy: only nodes are copied
List *List_shlw_copy(const List *list);

// creates a new list headed by datum followed by the elements of the given list
List *List_cons_new(List *list, LispDatum *dtm);

// creates a new list containing the tail of the given list
List *List_rest_new(List *list);

void List_append(List *dst, const List *src);

// List functions
const List *List_empty();


// -----------------------------------------------------------------------------
// Number < LispDatum

// TODO support arbitrary large numbers
// TODO support floating point numbers (potentially as another type)
typedef struct {
    /*void*/ _LispDatum *super;
    long val;
} Number;

// generic method implementations
LispType Number_type();
void Number_free(Number *num);
bool Number_eq(const Number *a, const Number *b);
char *Number_typename(const Number *num);
Number *Number_copy(const Number *num);
Number *Number_true_copy(const Number *num);

// Number methods
Number *Number_new(long val);
void Number_add(Number *a, const Number *b);
void Number_sub(Number *a, const Number *b);
void Number_div(Number *a, const Number *b);
void Number_mul(Number *a, const Number *b);
int Number_cmp(const Number *a, const Number *b);
int Number_cmpl(const Number *a, long l);
void Number_mod(Number *a, const Number *b);

bool Number_isneg(const Number *num);
bool Number_iseven(const Number *num);

// Number length: amount of digits excluding the sign 
size_t Number_len(const Number *num);
long Number_tol(const Number *num);
char *Number_sprint(const Number *num, char *dst);
char *Number_tostr(const Number *num);


// -----------------------------------------------------------------------------
// String < LispDatum

typedef struct {
    /*void*/ _LispDatum *super;
    char *str;
} String;

// generic method implementations
LispType String_type();
void String_free(String *string);
bool String_eq(const String *a, const String *b);
char *String_typename(const String *string);
String *String_copy(const String *string);

// String methods
String *String_new(const char *s);
char *String_str(const String *string);


// -----------------------------------------------------------------------------
// Nil < LispDatum

typedef struct {
    /*void*/ _LispDatum *super;
} Nil;

// generic method implementations
LispType Nil_type();
void Nil_free(Nil *nil);
bool Nil_eq(const Nil *a, const Nil *b);
char *Nil_typename(const Nil *nil);
Nil *Nil_copy(const Nil *nil);

// Nil methods
const Nil *Nil_get();


// -----------------------------------------------------------------------------
// False < LispDatum

typedef struct {
    /*void*/ _LispDatum *super;
} False;

// generic method implementations
LispType False_type();
void False_free(False *fls);
bool False_eq(const False *a, const False *b);
char *False_typename(const False *fls);
False *False_copy(const False *fls);

// False methods
const False *False_get();


// -----------------------------------------------------------------------------
// True < LispDatum

typedef struct {
    /*void*/ _LispDatum *super;
} True;

// generic method implementations
LispType True_type();
void True_free(True *tru);
bool True_eq(const True *a, const True *b);
char *True_typename(const True *tru);
True *True_copy(const True *tru);

// True methods
const True *True_get();

// maps bool to either True or False
const LispDatum *LispDatum_bool(bool b);

// -----------------------------------------------------------------------------
// Proc < LispDatum

// *** A note on procedure application ***
// Racket and Python evaluate function arguments before checking the arity.
// I shall do this the other way around

// pre-declare 
typedef struct Proc Proc;
typedef struct MalEnv MalEnv; // env.h

// the type of built-in procedures (e.g., list?, empty?, numeric ones, etc.)
// args - array of arguments (LispDatum*)
// env - application environment
typedef LispDatum* (*builtin_apply_t)(const Proc *proc, const Arr *args, MalEnv *env);

typedef struct Proc {
    /*void*/ _LispDatum *super;
    Symbol *name; // NULL for lambdas
    // number of mandatory arguments, (TODO optimise: if negative then it's also variadic)
    int argc;
    bool variadic;
    /* Declared parameter names, which include mandatory ones and potentially
     * the name of the variadic one. Number of these is given by
     * (+ (abs argc) (if (< argc 0) 1 0)). 
     */
    Arr *params; // of *Symbol
    // TODO replace by bitmask (and include variadic into it)
    bool macro;
    bool builtin;
    union {
        List *body;
        builtin_apply_t apply; // function pointer to the built-in procedure
    } logic;
    // the enclosing environment in which this procedure was defined
    /*const*/ MalEnv *env;
} Proc;

// generic method implementations
LispType Proc_type();
void Proc_free(Proc *proc);
bool Proc_eq(const Proc *a, const Proc *b);
char *Proc_typename(const Proc *proc);
Proc *Proc_copy(const Proc *proc);

// Proc methods

// constructor for language-defined procedures;
// neither params nor body are copied
Proc *Proc_new(
        Symbol *name, 
        int argc, bool variadic,
        Arr *params, 
        List *body, 
        MalEnv *env);

Proc *Proc_new_lambda(
        int argc, bool variadic,
        Arr *params,
        List *body, 
        MalEnv *env);

// constructor for built-in procedures
Proc *Proc_builtin(Symbol *name, int argc, bool variadic, const builtin_apply_t apply);

bool Proc_isva(const Proc *proc);
const Symbol *Proc_name(const Proc *proc);
bool Proc_isnamed(const Proc *proc);
bool Proc_ismacro(const Proc *proc);
bool Proc_isbuiltin(const Proc *proc);
int Proc_argc(const Proc *proc);

void Proc_set_name(Proc *proc, Symbol *name);
void Proc_set_macro(Proc *proc);


// -----------------------------------------------------------------------------
// Atom < LispDatum

// An atom holds a reference to a LispDatum of any type; it supports
// reading the pointed value and modifying the reference to point to another value.
// Atoms are mutable.
typedef struct Atom {
    /*void*/ _LispDatum *super;
    LispDatum *dtm;
} Atom;

// generic method implementations
LispType Atom_type();
void Atom_free(Atom *atom);
// 2 Atoms are equal only if they point to the same value
bool Atom_eq(const Atom *a, const Atom *b);
char *Atom_typename(const Atom *atom);
Atom *Atom_copy(const Atom *atom);

// Atom methods
Atom *Atom_new(LispDatum *dtm);
void Atom_set(Atom *atom, LispDatum *dtm);
LispDatum *Atom_deref(const Atom *atom);


// -----------------------------------------------------------------------------
// Exception < LispDatum
typedef struct {
    /*void*/ _LispDatum *super;
    LispDatum *dtm;
} Exception;

// generic method implementations
LispType Exception_type();
void Exception_free(Exception *exn);
// 2 Exceptions are equal only if they point to the same value
bool Exception_eq(const Exception *a, const Exception *b);
char *Exception_typename(const Exception *exn);
// since Exception is immutable, this does not copy the underlying LispDatum
Exception *Exception_copy(const Exception *exn);

// Exception methods
LispDatum *Exception_datum(const Exception *exn);

// datum is copied
Exception *Exception_new(const LispDatum *);

// returns a deep copy of the last thrown exception
Exception *thrown_copy();

bool didthrow();
void throw(const char *src, const LispDatum *dtm);
void throwf(const char *src, const char *fmt, ...);

void error(const char *fmt, ...);
