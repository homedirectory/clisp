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

// generic method declarations that are shared by concrete types,
// similar to Java final methods

// *** functions for managing reference count of LispDatum
// increments ref count (use when you need to *own* memory)
void LispDatum_own(LispDatum *);
// decrements ref count (use when you want to *release* owned memory)
void LispDatum_rls(LispDatum *);
// LispDatum_release + LispDatum_free
void LispDatum_rls_free(LispDatum *);

// generic method declarations
// these are part of the DtmMethods structure below and must be implemented
// by concrete types
typedef void (*dtm_free_ft)(LispDatum *);
void LispDatum_free(LispDatum *);

typedef bool (*dtm_eq_ft)(const LispDatum*, const LispDatum *);
bool LispDatum_eq(const LispDatum*, const LispDatum *);

typedef char* (*dtm_typename_ft)(const LispDatum *);
char *LispDatum_typename(const LispDatum *);

typedef LispDatum* (*dtm_copy_ft)(const LispDatum *);
LispDatum *LispDatum_copy(const LispDatum *);

typedef struct {
    dtm_free_ft free;
    dtm_eq_ft eq;
    dtm_typename_ft typename;
    dtm_copy_ft copy;
} DtmMethods;

// internal super-type;
// each concrete type must declare its 1st member as a pointer to this type
// e.g. struct List { _LispDatum *super; ... }
typedef struct _LispDatum {
    const DtmMethods *methods;
    long refc; // reference count
} _LispDatum;

static _LispDatum *_LispDatum_new(const DtmMethods *);
static void _LispDatum_free(_LispDatum *);
static void _LispDatum_own(_LispDatum *);
static void _LispDatum_rls(_LispDatum *);
static void _LispDatum_rls_free(_LispDatum *);


// -----------------------------------------------------------------------------
// Symbol < LispDatum

void init_symbol_table();
void free_symbol_table();

typedef struct {
    /*void*/ _LispDatum *super;
    char *name;
} Symbol;

// generic method implementations
void Symbol_free(Symbol *sym);
bool Symbol_eq(const Symbol *s1, const Symbol *s2);
char *Symbol_typename(const Symbol *sym);
Symbol *Symbol_copy(const Symbol *sym);

// Symbol-specific methods
Symbol* Symbol_intern(const char *name);
bool Symbol_eq_str(const Symbol *sym, const char *str);
