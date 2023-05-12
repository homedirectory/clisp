#pragma once

#include <stdbool.h>


// pre-declare
typedef struct _LispDatum _LispDatum;

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// LispDatum
// external view of a polymorphic type;
// works because pointer to a concrete type is a pointer to its 1st member,
// which must have type _LispDatum*
typedef _LispDatum* LispDatum;

// ---- generic method declarations ----
// these are part of the DtmMethods structure below and must be implemented by
// concrete types; however, if a concrete type wishes not to implement a method,
// the default implementation might be specified (if it exists of course) 
// e.g., .own = LispDatum_own_dflt

typedef unsigned int uint;
typedef uint (*dtm_type_ft)(const LispDatum *);
// returns a unique value identifying the runtime type of the datum;
// instead of storing the type in the LispDatum struct (in a tag-like fashion),
// the object itself responds to the message about its type 
uint LispDatum_type(const LispDatum *);

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

// generic method declarations that are shared by concrete types,
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

static _LispDatum *_LispDatum_new(const DtmMethods *);
static long _LispDatum_refc(const _LispDatum *_dtm);
// default implementations of ref. management methods
static void _LispDatum_free(_LispDatum *);
static void _LispDatum_own(_LispDatum *);
static void _LispDatum_rls(_LispDatum *);


// -----------------------------------------------------------------------------
// Symbol < LispDatum

void init_symbol_table();
void free_symbol_table();

typedef struct {
    /*void*/ _LispDatum *super;
    char *name;
} Symbol;

// generic method implementations
uint Symbol_type();
void Symbol_free(Symbol *sym);
bool Symbol_eq(const Symbol *s1, const Symbol *s2);
char *Symbol_typename(const Symbol *sym);
Symbol *Symbol_copy(const Symbol *sym);

// Symbol-specific methods
Symbol* Symbol_intern(const char *name);
bool Symbol_eq_str(const Symbol *sym, const char *str);


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
uint List_type();
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


// -----------------------------------------------------------------------------
// Number < LispDatum

// TODO support arbitrary large numbers
// TODO support floating point numbers (potentially as another type)
typedef struct {
    /*void*/ _LispDatum *super;
    long val;
} Number;

// generic method implementations
uint Number_type();
void Number_free(Number *num);
bool Number_eq(const Number *a, const Number *b);
char *Number_typename(const Number *num);
Number *Number_copy(const Number *num);

// Number methods
Number *Number_new(long val);
void Number_add(Number *a, const Number *b);
void Number_sub(Number *a, const Number *b);
void Number_div(Number *a, const Number *b);
void Number_mul(Number *a, const Number *b);

long Number_tol(const Number *num);


// -----------------------------------------------------------------------------
// String < LispDatum

typedef struct {
    /*void*/ _LispDatum *super;
    char *str;
} String;

// generic method implementations
uint String_type();
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
uint Nil_type();
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
uint False_type();
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
uint True_type();
void True_free(True *tru);
bool True_eq(const True *a, const True *b);
char *True_typename(const True *tru);
True *True_copy(const True *tru);

// True methods
const True *True_get();
