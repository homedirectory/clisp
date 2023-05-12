#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "common.h"
#include "mem_debug.h"
#include "utils.h"
#include "types_oop.h"
#include "hashtbl.h"

/*
//#define INVOKE(dtm, method, args...) \
//    dtm->methods->method(dtm, ##__VA_ARGS__)
*/

// -----------------------------------------------------------------------------
// _LispDatum

// concrete types should call this function to allocate memory for the 1st member
static _LispDatum *_LispDatum_new(const DtmMethods *methods)
{
    _LispDatum *_dtm = malloc(sizeof(_LispDatum));
    _dtm->methods = methods;
    _dtm->refc = 0;
    return _dtm;
}

// concrete types should call this function to free memory allocated for the 1st member
static void _LispDatum_free(_LispDatum *_dtm)
{
    free(_dtm);
}

static void _LispDatum_own(_LispDatum *_dtm)
{
    if (_dtm == NULL) {
        LOG_NULL(_dtm);
        return;
    }

    // if (LispDatum_is_singleton(datum)) return;

    _dtm->refc += 1;
}

static void _LispDatum_rls(_LispDatum *_dtm)
{
    if (_dtm == NULL) {
        LOG_NULL(_dtm);
        return;
    }

    if (_dtm->refc <= 0)
        FATAL("WTF? Ref count = %ld", _dtm->refc);

    // if (LispDatum_is_singleton(datum)) return;

    _dtm->refc -= 1;
}

static void _LispDatum_rls_free(_LispDatum *_dtm)
{
    _LispDatum_rls(_dtm);
    _LispDatum_free(_dtm);
}

// -----------------------------------------------------------------------------
// LispDatum

typedef _LispDatum* LispDatum;

static const DtmMethods *LispDatum_methods(const LispDatum *dtm)
{
    _LispDatum *_dtm = *dtm;
    return _dtm->methods;
}

void LispDatum_own(LispDatum *dtm)
{
    _LispDatum *_dtm = *dtm;
    _LispDatum_own(_dtm);
}

void LispDatum_rls(LispDatum *dtm)
{
    _LispDatum *_dtm = *dtm;
    _LispDatum_rls(_dtm);
}

void LispDatum_rls_free(LispDatum *dtm)
{
    _LispDatum *_dtm = *dtm;
    _LispDatum_rls_free(_dtm);
}

bool LispDatum_eq(const LispDatum *dtm1, const LispDatum *dtm2)
{
    return dtm1 == dtm2 || LispDatum_methods(dtm1)->eq(dtm1, dtm2);
}

void LispDatum_free(LispDatum *dtm)
{
    _LispDatum *_dtm = *dtm;
    if (_dtm->refc > 0) {
        DEBUG("Refuse to free %p (refc %ld)", dtm, _dtm->refc);
        return;
    }

    LispDatum_methods(dtm)->free(dtm);
}

char *LispDatum_typename(const LispDatum *dtm)
{
    return LispDatum_methods(dtm)->typename(dtm);
}

LispDatum *LispDatum_copy(const LispDatum *dtm)
{
    return LispDatum_methods(dtm)->copy(dtm);
}

// -----------------------------------------------------------------------------
// Symbol < LispDatum

// Symbol constructor
static Symbol* Symbol_new(const char *name) {
    static const DtmMethods symbol_methods = {
        .free = (dtm_free_ft) Symbol_free,
        .eq = (dtm_eq_ft) Symbol_eq,
        .typename = (dtm_typename_ft) Symbol_typename,
        .copy = (dtm_copy_ft) Symbol_copy
    };

    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = dyn_strcpy(name);
    sym->super = _LispDatum_new(&symbol_methods);
    return sym;
}

static uint hash_str(const char *str)
{
    // simple and works (unique hash for each unique string)
    // collisions might happen only if str is longer than (2^32 - 1) / (2^8 - 1) = 16843009 bytes
    uint h = *(str++);
    while (*str) h += *str++;
    return h;
}

// key char*, value Symbol*
static HashTbl *g_symbol_table;

void init_symbol_table()
{
    g_symbol_table = HashTbl_newc(256, (hashkey_t) hash_str);
}

// concerned with Symbol only, doesn't modify the symbol table
static void _Symbol_free(Symbol *sym) 
{
    free(sym->name);
    _LispDatum_free(sym->super);
    free(sym);
}

static void noop(void *ptr) { }
void free_symbol_table()
{
    // key is the same pointer that's stored by a symbol, so we don't need to free keys
    HashTbl_free(g_symbol_table, noop, (free_t) _Symbol_free);
}

static Symbol *sym_tbl_pop(const char *name)
{
    return HashTbl_pop(g_symbol_table, name, (keyeq_t) streq);
}

// frees the symbol and pops it from the symbol table
void Symbol_free(Symbol *sym)
{
    Symbol *popd = sym_tbl_pop(sym->name);
    assert(popd == sym);
    _Symbol_free(sym);
}

bool Symbol_eq(const Symbol *s1, const Symbol *s2)
{
    return strcmp(s1->name, s2->name) == 0;
}

char *Symbol_typename(const Symbol *sym)
{
    return dyn_strcpy("Symbol");
}

Symbol *Symbol_copy(const Symbol *sym)
{
    return (Symbol*) sym;
}

Symbol *Symbol_intern(const char *name) 
{
    Symbol *sym = HashTbl_get(g_symbol_table, name, (keyeq_t) streq);
    if (sym)
        return sym;
    else {
        Symbol *symnew = Symbol_new(name);
        HashTbl_put(g_symbol_table, symnew->name, symnew);
        return symnew;
    }
}


bool Symbol_eq_str(const Symbol *sym, const char *str) 
{
    if (sym == NULL) {
        LOG_NULL(sym);
        return false;
    }
    if (str == NULL) {
        LOG_NULL(str);
        return false;
    }

    return strcmp(sym->name, str) == 0;
}

int main(int argc, char **argv) {
    init_symbol_table();

    {
        Symbol *sym1 = Symbol_intern("hello");
        assert(sym1 == Symbol_intern("hello"));

        Symbol *sym2 = Symbol_intern("hellz");

        printf("%s\n", Symbol_eq(sym1, sym2) ? "true" : "false");
        Symbol_free(sym1);
        Symbol_free(sym2);
    }

    {
        LispDatum *dtm1 = (LispDatum*) Symbol_intern("hello");
        LispDatum *dtm2 = (LispDatum*) Symbol_intern("hellz");
        printf("%s\n", LispDatum_eq(dtm1, dtm2) ? "true" : "false");
        LispDatum_free(dtm1);
        LispDatum_free(dtm2);
    }

    {
        LispDatum *dtm = (LispDatum*) Symbol_intern("world");
        char *s = LispDatum_typename(dtm);
        printf("%s\n", s);
        free(s);
        LispDatum_free(dtm);
    }

    free_symbol_table();
}
