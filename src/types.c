#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>

#include "common.h"
#include "mem_debug.h"
#include "utils.h"
#include "types.h"
#include "hashtbl.h"
#include "env.h"
#include "printer.h"

/*
//#define INVOKE(dtm, method, args...) \
//    dtm->methods->method(dtm, ##__VA_ARGS__)
*/

// -----------------------------------------------------------------------------
// LispType

const char *LispType_name(LispType type) {
    static char* const names[] = {
        "SYMBOL", 
        "LIST", 
        "NUMBER",
        "STRING", 
        "NIL", "FALSE", "TRUE", 
        "PROCEDURE",
        "ATOM",
        "EXCEPTION",
        "*undefined*"
    };

    return names[type];
}

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

static long _LispDatum_refc(const _LispDatum *_dtm)
{
    return _dtm->refc;
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
    return LispDatum_methods(dtm)->own(dtm);
}

static void LispDatum_own_dflt(LispDatum *dtm)
{
    _LispDatum *_dtm = *dtm;
    _LispDatum_own(_dtm);
}

void LispDatum_rls(LispDatum *dtm)
{
    return LispDatum_methods(dtm)->rls(dtm);
}

static void LispDatum_rls_dflt(LispDatum *dtm)
{
    _LispDatum *_dtm = *dtm;
    _LispDatum_rls(_dtm);
}

void LispDatum_rls_free(LispDatum *dtm)
{
    LispDatum_rls(dtm);
    LispDatum_free(dtm);
}

long LispDatum_refc(const LispDatum *dtm)
{
    _LispDatum *_dtm = *dtm;
    return _LispDatum_refc(_dtm);
}

LispType LispDatum_type(const LispDatum *dtm)
{
    return LispDatum_methods(dtm)->type(dtm);
}

bool LispDatum_istype(const LispDatum *dtm, LispType type)
{
    return LispDatum_type(dtm) == type;
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
        .type = (dtm_type_ft) Symbol_type,
        .free = (dtm_free_ft) Symbol_free,
        .eq = (dtm_eq_ft) Symbol_eq,
        .typename = (dtm_typename_ft) Symbol_typename,
        .copy = (dtm_copy_ft) Symbol_copy,
        .own = LispDatum_own_dflt,
        .rls = LispDatum_rls_dflt
    };

    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = dyn_strcpy(name);
    sym->super = _LispDatum_new(&symbol_methods);
    return sym;
}

LispType Symbol_type() { 
    return SYMBOL; 
}

// key char*, value Symbol*
static HashTbl *g_symbol_table;

void init_symbol_table()
{
    g_symbol_table = HashTbl_newc(256, (hashkey_t) hash_simple_str);
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
    // since symbols are interned, their names must be unique 
    return s1 == s2;
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
        HashTbl_put(g_symbol_table, symnew->name, symnew, (keyeq_t) streq);
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

const char *Symbol_name(const Symbol *sym)
{
    return sym->name;
}

// -----------------------------------------------------------------------------
// List < LispDatum

// singleton empty list
// has to declare its own methods to replace .own and .rls with noops

// pre-declare
static void LispDatum_noown(LispDatum *dtm);
static void LispDatum_norls(LispDatum *dtm);

static const DtmMethods empty_list_methods = {
    .type = (dtm_type_ft) List_type,
    .free = (dtm_free_ft) List_free,
    .eq = (dtm_eq_ft) List_eq,
    .typename = (dtm_typename_ft) List_typename,
    .copy = (dtm_copy_ft) List_copy,
    .own = LispDatum_noown,
    .rls = LispDatum_norls
};

static const _LispDatum g_empty_list_super = {
    .methods = &empty_list_methods,
    .refc = 1
};
static const List g_empty_list = {
    .super = (_LispDatum*) &g_empty_list_super,
    .len = 0, .head = NULL, .tail = NULL 
};
const List *List_empty() { return &g_empty_list; }

LispType List_type() { 
    return LIST; 
}

/* Frees the memory allocated for each Node of the list including the LispDatums they point to. */
void List_free(List *list) {
    if (list == NULL || list == &g_empty_list) return;

    if (list->head) {
        struct Node *node = list->head;
        while (node && --node->refc == 0) {
            LispDatum_rls(node->value);
            LispDatum_free(node->value);
            struct Node *p = node;
            node = node->next;
            free(p);
        }
    }

    _LispDatum_free(list->super);
    free(list);
}

bool List_eq(const List *lst1, const List *lst2) {
    if (lst1 == lst2) return true;
    if (lst1->len != lst2->len) return false;

    struct Node *node1 = lst1->head;
    struct Node *node2 = lst2->head;
    while (node1 != NULL) {
        if (!LispDatum_eq(node1->value, node2->value))
            return false;
        node1 = node1->next;
        node2 = node2->next;
    }
    
    return true;
}

char *List_typename(const List *list) {
    return dyn_strcpy("List");
}

List *List_copy(const List *list) {
    if (list == NULL)
        FATAL("list == NULL");

    if (List_isempty(list)) return (List*) List_empty();

    List *out = List_new();

    struct Node *node = list->head;
    while (node) {
        LispDatum *cpy = LispDatum_copy(node->value);
        if (cpy == NULL)
            FATAL("cpy == NULL");
        List_add(out, cpy);
        node = node->next;
    }

    return out;
}

List *List_new() {
    static const DtmMethods list_methods = {
        .type = (dtm_type_ft) List_type,
        .free = (dtm_free_ft) List_free,
        .eq = (dtm_eq_ft) List_eq,
        .typename = (dtm_typename_ft) List_typename,
        .copy = (dtm_copy_ft) List_copy,
        .own = LispDatum_own_dflt,
        .rls = LispDatum_rls_dflt
    };

    List *list = malloc(sizeof(List));
    list->len = 0;
    list->head = NULL;
    list->tail = NULL;
    list->super = _LispDatum_new(&list_methods);
    return list;
}

size_t List_len(const List *list) {
    return list->len;
}

bool List_isempty(const List *list) {
    if (list == NULL) {
        LOG_NULL(list);
        exit(EXIT_FAILURE);
    }
    return list->len == 0;
}

void List_add(List *list, LispDatum *datum) {
    struct Node *node = malloc(sizeof(struct Node));
    node->refc = 1;
    node->value = datum;
    node->next = NULL;

    LispDatum_own(datum);

    if (list->tail == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }

    list->len += 1;
}

LispDatum *List_ref(const List *list, size_t idx) {
    if (list == NULL) {
        LOG_NULL(list);
        return NULL;
    }
    if (idx >= list->len)
        return NULL;

    size_t i = 0;
    struct Node *node = list->head;
    while (i < idx) {
        node = node->next;
        i++;
    }
    return node->value;
}

List *List_shlw_copy(const List *list) {
    if (list == NULL)
        FATAL("list == NULL")

    if (List_isempty(list)) return (List*) List_empty();

    List *out = List_new();

    struct Node *node = list->head;
    while (node) {
        List_add(out, node->value);
        node = node->next;
    }

    return out;
}

List *List_cons_new(List *list, LispDatum *datum)
{
    List *out = List_new();

    struct Node *node = malloc(sizeof(struct Node));
    node->refc = 1;
    node->value = datum;
    LispDatum_own(datum);
    node->next = list->head; // potentially NULL
    out->head = node;
    if (list->head) {
        list->head->refc++;
        out->tail = list->tail;
    }
    else {
        out->tail = node;
    }

    out->len = 1 + list->len;

    return out;
}

List *List_rest_new(List *list)
{
    if (List_isempty(list)) {
        DEBUG("got empty list");
        return NULL;
    }

    List *out = List_new();
    size_t tail_len = List_len(list) - 1;
    if (tail_len > 0) {
        struct Node *tail_head = list->head->next;
        out->head = out->tail = tail_head;
        tail_head->refc += 1;
        out->len = tail_len;
    }

    return out;
}

void List_append(List *dst, const List *src)
{
    if (dst == NULL) {
        LOG_NULL(dst);
        return;
    }
    if (src == NULL) {
        LOG_NULL(src);
        return;
    }

    if (List_isempty(src)) return;

    struct Node *src_head = src->head;
    if (List_isempty(dst)) {
        dst->head = dst->tail = src_head;
    }
    else {
        dst->tail->next = src_head;
        dst->tail = src->tail;
    }
    src_head->refc += 1;

    dst->len += src->len;
}


// -----------------------------------------------------------------------------
// Number < LispDatum

Number *Number_new(int64_t val)
{
    static const DtmMethods number_methods = {
        .type = (dtm_type_ft) Number_type,
        .free = (dtm_free_ft) Number_free,
        .eq = (dtm_eq_ft) Number_eq,
        .typename = (dtm_typename_ft) Number_typename,
        .copy = (dtm_copy_ft) Number_copy,
        .own = LispDatum_own_dflt,
        .rls = LispDatum_rls_dflt
    };

    Number *num = malloc(sizeof(Number));
    num->val = val;
    num->super = _LispDatum_new(&number_methods);
    return num;
}

// generic method implementations
LispType Number_type()
{
    return NUMBER;
}

void Number_free(Number *num)
{
    _LispDatum_free(num->super);
    free(num);
}

bool Number_eq(const Number *a, const Number *b)
{
    return a->val == b->val;
}

char *Number_typename(const Number *num)
{
    return dyn_strcpy("Number");
}

Number *Number_copy(const Number *num)
{
    return (Number*) num;
}

Number *Number_true_copy(const Number *num)
{
    return Number_new(num->val);
}

// Number-specific methods
void Number_add(Number *a, const Number *b)
{
    a->val += b->val;
}

void Number_sub(Number *a, const Number *b)
{
    a->val -= b->val;
}

void Number_div(Number *a, const Number *b)
{
    a->val /= b->val;
}

void Number_mul(Number *a, const Number *b)
{
    a->val *= b->val;
}

int Number_cmp(const Number *a, const Number *b)
{
    int64_t d = a->val - b->val;
    return d ? (d > 0 ? 1 : -1) : 0;
}

int Number_cmpl(const Number *a, long l)
{
    long d = a->val - l;
    return d ? (d > 0 ? 1 : -1) : 0;
}

void Number_mod(Number *a, const Number *b)
{
    a->val %= b->val;
}

bool Number_isneg(const Number *num)
{
    return num->val < 0;
}

bool Number_iseven(const Number *num)
{
    return !(num->val & 1);
}

long Number_tol(const Number *num)
{
    return num->val;
}

size_t Number_len(const Number *num)
{
    size_t sz = 0;
    int64_t val = num->val;
    while (val != 0) {
        val /= 10;
        sz++;
    }
    return sz;
}

static char *_Number_val_tos(int64_t val, char *dst)
{
    if (val == 0) {
        dst[0] = '0';
        dst[1] = 0;
        return dst;
    }
    else if (val < 0) {
        *dst++ = '-';
        val *= -1;
    }

    char *s = dst;
    size_t i = 0;

    while (val != 0) {
        *dst++ = itoa(val % 10);
        val /= 10;
        i++;
    }

    *dst = 0;
    strnrev(s, i);

    return dst;
}

char *Number_sprint(const Number *num, char *dst)
{
    return _Number_val_tos(num->val, dst);
}

char *Number_tostr(const Number *num)
{
    char *s = malloc(Number_len(num) + 1);
    Number_sprint(num, s);
    return s;
}

// -----------------------------------------------------------------------------
// String < LispDatum

// generic method implementations
LispType String_type()
{
    return STRING;
}

void String_free(String *string)
{
    free(string->str);
    _LispDatum_free(string->super);
    free(string);
}

bool String_eq(const String *a, const String *b)
{
    return strcmp(a->str, b->str) == 0;
}

char *String_typename(const String *string)
{
    return dyn_strcpy("String");
}

String *String_copy(const String *string)
{
    return String_new(string->str);
}

// String methods
String *String_new(const char *s)
{
    static const DtmMethods string_methods = {
        .type = (dtm_type_ft) String_type,
        .free = (dtm_free_ft) String_free,
        .eq = (dtm_eq_ft) String_eq,
        .typename = (dtm_typename_ft) String_typename,
        .copy = (dtm_copy_ft) String_copy,
        .own = LispDatum_own_dflt,
        .rls = LispDatum_rls_dflt
    };

    String *str = malloc(sizeof(String));
    str->str = dyn_strcpy(s);
    str->super = _LispDatum_new(&string_methods);
    return str;
}

char *String_str(const String *string)
{
    return string->str;
}


// -----------------------------------------------------------------------------
// Singleton types: Nil, True, False

// implementation of ref. management methods for these types is not needed
static void LispDatum_noown(LispDatum *dtm) {}
static void LispDatum_norls(LispDatum *dtm) {}

// -----------------------------------------------------------------------------
// Nil < LispDatum

// generic method implementations
LispType Nil_type()
{
    return NIL;
}

void Nil_free(Nil *nil)
{
    ;
}

bool Nil_eq(const Nil *a, const Nil *b)
{
    return true;
}

char *Nil_typename(const Nil *nil)
{
    return dyn_strcpy("Nil");
}

Nil *Nil_copy(const Nil *nil)
{
    return (Nil*) nil;
}

// Nil methods
const Nil *Nil_get()
{
    static const DtmMethods nil_methods = {
        .type = (dtm_type_ft) Nil_type,
        .free = (dtm_free_ft) Nil_free,
        .eq = (dtm_eq_ft) Nil_eq,
        .typename = (dtm_typename_ft) Nil_typename,
        .copy = (dtm_copy_ft) Nil_copy,
        .own = LispDatum_noown,
        .rls = LispDatum_norls
    };

    static const _LispDatum super = {
        .methods = &nil_methods,
        .refc = 1
    };

    static const Nil nil = {
        .super = (_LispDatum*) &super
    };

    return &nil;
}

// -----------------------------------------------------------------------------
// False < LispDatum

// generic method implementations
LispType False_type()
{
    return FALSE;
}

void False_free(False *fls)
{
    ;
}

bool False_eq(const False *a, const False *b)
{
    return true;
}

char *False_typename(const False *fls)
{
    return dyn_strcpy("False");
}

False *False_copy(const False *fls)
{
    return (False*) fls;
}

// False methods
const False *False_get()
{
    static const DtmMethods false_methods = {
        .type = (dtm_type_ft) False_type,
        .free = (dtm_free_ft) False_free,
        .eq = (dtm_eq_ft) False_eq,
        .typename = (dtm_typename_ft) False_typename,
        .copy = (dtm_copy_ft) False_copy,
        .own = LispDatum_noown,
        .rls = LispDatum_norls
    };

    static const _LispDatum super = {
        .methods = &false_methods,
        .refc = 1
    };

    static const False fls = {
        .super = (_LispDatum*) &super
    };

    return &fls;
}

// -----------------------------------------------------------------------------
// True < LispDatum

// generic method implementations
LispType True_type()
{
    return TRUE;
}

void True_free(True *tru)
{
    ;
}

bool True_eq(const True *a, const True *b)
{
    return true;
}

char *True_typename(const True *tru)
{
    return dyn_strcpy("True");
}

True *True_copy(const True *tru)
{
    return (True*) tru;
}

// True methods
const True *True_get()
{
    static const DtmMethods true_methods = {
        .type = (dtm_type_ft) True_type,
        .free = (dtm_free_ft) True_free,
        .eq = (dtm_eq_ft) True_eq,
        .typename = (dtm_typename_ft) True_typename,
        .copy = (dtm_copy_ft) True_copy,
        .own = LispDatum_noown,
        .rls = LispDatum_norls
    };

    static const _LispDatum super = {
        .methods = &true_methods,
        .refc = 1
    };

    static const True tru = {
        .super = (_LispDatum*) &super
    };

    return &tru;
}

const LispDatum *LispDatum_bool(bool b)
{
    return (b ? (LispDatum*) True_get() : (LispDatum*) False_get());
}

// -----------------------------------------------------------------------------
// Proc < LispDatum

// generic method implementations
LispType Proc_type()
{
    return PROCEDURE;
}

void Proc_free(Proc *proc)
{
    if (!proc->builtin) {
        // free params
        Arr_freep(proc->params, (free_t) LispDatum_rls_free);
        // free body
        List_free(proc->logic.body);
    }

    if (proc->name) 
        LispDatum_rls_free((LispDatum*) proc->name);

    MalEnv_release(proc->env);
    MalEnv_free(proc->env);

    _LispDatum_free(proc->super);

    free(proc);
}

bool Proc_eq(const Proc *a, const Proc *b)
{
    return a == b;
}

char *Proc_typename(const Proc *proc)
{
    return dyn_strcpy("Procedure");
}

Proc *Proc_copy(const Proc *proc)
{
    // procedures are immutable
    return (Proc*) proc;
}

// Proc methods

static const DtmMethods Proc_methods = {
    .type = (dtm_type_ft) Proc_type,
    .free = (dtm_free_ft) Proc_free,
    .eq = (dtm_eq_ft) Proc_eq,
    .typename = (dtm_typename_ft) Proc_typename,
    .copy = (dtm_copy_ft) Proc_copy,
    .own = LispDatum_own_dflt,
    .rls = LispDatum_rls_dflt
};

Proc *Proc_new(
        Symbol *name, 
        int argc, bool variadic,
        Arr *params, 
        List *body, 
        MalEnv *env)
{
    Proc *proc = malloc(sizeof(Proc));

    proc->name = name;
    if (name)
        LispDatum_own((LispDatum*) name);

    proc->variadic = variadic;
    // proc->argc = argc * (variadic ? -1 : 1);
    proc->argc = argc;

    proc->params = params;
    Arr_foreach(proc->params, (unary_void_t) LispDatum_own);

    proc->builtin = false;
    proc->macro = false;

    proc->logic.body = body;
    LispDatum_own((LispDatum*) body);

    proc->env = env;
    MalEnv_own(env);

    proc->super = _LispDatum_new(&Proc_methods);

    return proc;
}

Proc *Proc_new_lambda(
        int argc, bool variadic,
        Arr *params,
        List *body, 
        MalEnv *env)
{
    return Proc_new(NULL, argc, variadic, params, body, env);
}

Proc *Proc_builtin(Symbol *name, int argc, bool variadic, const builtin_apply_t apply)
{
    Proc *proc = malloc(sizeof(Proc));

    proc->name = name;
    LispDatum_own((LispDatum*) name);

    // proc->argc = argc * (variadic ? -1 : 1);
    proc->argc = argc;

    proc->variadic = variadic;
    proc->builtin = true;
    proc->macro = false;

    proc->logic.apply = apply;

    proc->env = NULL;

    proc->super = _LispDatum_new(&Proc_methods);

    return proc;
}

bool Proc_isva(const Proc *proc)
{
    // return proc->argc < 0;
    return proc->variadic;
}

const Symbol *Proc_name(const Proc *proc)
{
    if (proc == NULL)
        FATAL("proc == NULL");

    // TODO optimize by making *lambda* symbol static
    const Symbol *name = proc->name ? proc->name : Symbol_intern("*lambda*");
    return name;
}

bool Proc_isnamed(const Proc *proc)
{
    return proc->name != NULL;
}

bool Proc_ismacro(const Proc *proc)
{
    return proc->macro;
}

bool Proc_isbuiltin(const Proc *proc)
{
    return proc->builtin;
}

int Proc_argc(const Proc *proc)
{
    return proc->argc;
}

void Proc_set_name(Proc *proc, Symbol *name)
{
    if (proc->name) {
        LispDatum_rls_free((LispDatum*) proc->name);
    }
    proc->name = name;
    LispDatum_own((LispDatum*) name);
}

void Proc_set_macro(Proc *proc)
{
    proc->macro = true;
}


// -----------------------------------------------------------------------------
// Atom < LispDatum

// generic method implementations
LispType Atom_type()
{
    return ATOM;
}

void Atom_free(Atom *atom)
{
    LispDatum_rls_free(atom->dtm);
    _LispDatum_free(atom->super);
    free(atom);
}

// 2 Atoms are equal only if they point to the same value
bool Atom_eq(const Atom *a, const Atom *b)
{
    return a->dtm == b->dtm;
}

char *Atom_typename(const Atom *atom)
{
    return dyn_strcpy("Atom");
}

Atom *Atom_copy(const Atom *atom)
{
    return Atom_new(atom->dtm);
}

// Atom methods
Atom *Atom_new(LispDatum *dtm)
{
    static const DtmMethods atom_methods = {
        .type = (dtm_type_ft) Atom_type,
        .free = (dtm_free_ft) Atom_free,
        .eq = (dtm_eq_ft) Atom_eq,
        .typename = (dtm_typename_ft) Atom_typename,
        .copy = (dtm_copy_ft) Atom_copy,
        .own = LispDatum_own_dflt,
        .rls = LispDatum_rls_dflt
    };

    Atom *atom = malloc(sizeof(Atom));
    atom->dtm = dtm;
    LispDatum_own(dtm);

    atom->super = _LispDatum_new(&atom_methods);

    return atom;
}

void Atom_set(Atom *atom, LispDatum *dtm)
{
    if (atom->dtm == dtm) return;

    LispDatum_rls_free(atom->dtm);
    atom->dtm = dtm;
    LispDatum_own(dtm);
}

LispDatum *Atom_deref(const Atom *atom)
{
    return atom->dtm;
}


// -----------------------------------------------------------------------------
// Exception < LispDatum

static const DtmMethods exception_methods = {
    .type = (dtm_type_ft) Exception_type,
    .free = (dtm_free_ft) Exception_free,
    .eq = (dtm_eq_ft) Exception_eq,
    .typename = (dtm_typename_ft) Exception_typename,
    .copy = (dtm_copy_ft) Exception_copy,
    .own = LispDatum_own_dflt,
    .rls = LispDatum_rls_dflt
};

// generic method implementations
LispType Exception_type()
{
    return EXCEPTION;
}

void Exception_free(Exception *exn)
{
    LispDatum_rls_free(exn->dtm);
    _LispDatum_free(exn->super);
    free(exn);
}

// 2 Exceptions are equal only if they point to the same value
bool Exception_eq(const Exception *a, const Exception *b)
{
    return LispDatum_eq(a->dtm, b->dtm);
}

char *Exception_typename(const Exception *exn)
{
    return dyn_strcpy("Exception");
}

Exception *Exception_copy(const Exception *exn)
{
    Exception *copy = malloc(sizeof(Exception));
    copy->dtm = exn->dtm;

    copy->super = _LispDatum_new(&exception_methods);

    return copy;
}

// Exception methods
LispDatum *Exception_datum(const Exception *exn)
{
    return exn->dtm;
}

// datum is copied
Exception *Exception_new(const LispDatum *dtm)
{
    Exception *exn = malloc(sizeof(Exception));
    LispDatum *dtm_cpy = LispDatum_copy(dtm);
    LispDatum_own(dtm_cpy);
    exn->dtm = dtm_cpy;

    exn->super = _LispDatum_new(&exception_methods);

    return exn;
}

// global last raised exception
static const _LispDatum g_last_exn_super = {
    .methods = &exception_methods,
    .refc = 1
};
static Exception g_last_exn = {
    .super = (_LispDatum*) &g_last_exn_super,
    .dtm = NULL
};

Exception *thrown_copy()
{
    if (!g_last_exn.dtm)
        LOG_NULL(g_last_exn.dtm);

    return Exception_new(g_last_exn.dtm);
}

// we need to know the last thing that happened: error or exception?
static enum LastFail {
    LF_NONE,
    LF_ERROR,
    LF_EXCEPTION
} g_lastfail = LF_NONE;

bool didthrow()
{
    return g_lastfail == LF_EXCEPTION;
}

void throw(const char *src, const LispDatum *dtm)
{
    g_lastfail = LF_EXCEPTION;

    LispDatum *old = g_last_exn.dtm;
    if (old)
        LispDatum_rls_free(old);

    LispDatum *copy = LispDatum_copy(dtm);
    LispDatum_own(copy);
    g_last_exn.dtm = copy;

    char *s = pr_str(dtm, true);
    if (src != NULL)
        fprintf(stderr, "exception in %s: %s\n", src, s);
    else 
        fprintf(stderr, "exception: %s\n", s);
    free(s);
}

void throwf(const char *src, const char *fmt, ...)
{
    char buf[2048]; // TODO fix rigid limit

    va_list va;
    va_start(va, fmt);
    vsprintf(buf, fmt, va);
    va_end(va);

    throw(src, (LispDatum*) String_new(buf));
}

void error(const char *fmt, ...)
{
    g_lastfail = LF_ERROR;

    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}



// int main(int argc, char **argv) {
//     init_symbol_table();

//     {
//         Symbol *s_hello = Symbol_intern("hello");
//         assert(s_hello == Symbol_intern("hello"));
//         assert(LispDatum_eq((LispDatum*)s_hello, (LispDatum*)Symbol_intern("hello")));

//         Symbol *s_hellz = Symbol_intern("hellz");
//         assert(!Symbol_eq(s_hello, s_hellz));
//         assert(!LispDatum_eq((LispDatum*)s_hello, (LispDatum*)s_hellz));

//         Symbol_free(s_hello);
//         Symbol_free(s_hellz);
//     }

//     {
//         LispDatum *dtm_s_hello = (LispDatum*) Symbol_intern("hello");
//         LispDatum *dtm_s_hellz = (LispDatum*) Symbol_intern("hellz");

//         assert(!LispDatum_eq(dtm_s_hello, dtm_s_hellz));

//         LispDatum_free(dtm_s_hello);
//         LispDatum_free(dtm_s_hellz);
//     }

//     {
//         LispDatum *dtm = (LispDatum*) Symbol_intern("world");
//         char *s = LispDatum_typename(dtm);
//         assert(strcmp("Symbol", s) == 0);
//         free(s);
//         LispDatum_free(dtm);
//     }

//     {
//         Symbol *s_hello = Symbol_intern("hello");
//         Symbol *s_world = Symbol_intern("world");

//         List *list = List_new();
//         List_add(list, (LispDatum*) s_hello);
//         List_add(list, (LispDatum*) s_world);
//         assert(LIST == LispDatum_type((LispDatum*) list));
//         assert(2 == List_len(list));
//         List_free(list);
//     }

//     {
//         Number *n1 = Number_new(123);
//         Number *n2 = Number_new(8872);
//         Number *sum = Number_new(0);
//         Number_add(sum, n1);
//         Number_add(sum, n2);
//         assert(123 + 8872 == Number_tol(sum));
//         Number_free(n1);
//         Number_free(n2);
//         Number_free(sum);
//     }

//     {
//         String *str1 = String_new("hello world, it's me, the programmer");
//         assert(strcmp("hello world, it's me, the programmer", String_str(str1)) == 0);
//         LispDatum_free((LispDatum*) str1);
//     }

//     // singletons
//     {
//         const Nil *nil = Nil_get();
//         assert(nil == Nil_get());
//         const True *tru = True_get();
//         assert(tru == True_get());
//         const False *fls = False_get();
//         assert(fls == False_get());

//         assert(1 == LispDatum_refc((LispDatum*) nil));
//         LispDatum_free((LispDatum*) nil);
//         assert(1 == LispDatum_refc((LispDatum*) nil));
//         LispDatum_own((LispDatum*) nil);
//         assert(1 == LispDatum_refc((LispDatum*) nil));

//         assert(1 == LispDatum_refc((LispDatum*) fls));
//         LispDatum_free((LispDatum*) fls);
//         assert(1 == LispDatum_refc((LispDatum*) fls));
//         LispDatum_own((LispDatum*) fls);
//         assert(1 == LispDatum_refc((LispDatum*) fls));

//         assert(1 == LispDatum_refc((LispDatum*) tru));
//         LispDatum_free((LispDatum*) tru);
//         assert(1 == LispDatum_refc((LispDatum*) tru));
//         LispDatum_own((LispDatum*) tru);
//         assert(1 == LispDatum_refc((LispDatum*) tru));
//     }

//     // env
//     {
//         MalEnv *env = MalEnv_new(NULL);
//         Symbol *s_a = Symbol_intern("a");
//         Number *one = Number_new(1);

//         MalEnv_put(env, s_a, (LispDatum*) one);
//         assert((LispDatum*) one == MalEnv_get(env, s_a));

//         Number *two = Number_new(2);
//         MalEnv_put(env, s_a, (LispDatum*) two);
//         assert((LispDatum*) two == MalEnv_get(env, s_a));

//         MalEnv_free(env);
//     }

//     // atom
//     {
//         LispDatum *num = (LispDatum*) Number_new(55);
//         Atom *atm1 = Atom_new(num);
//         LispDatum *dtm1 = Atom_deref(atm1);
//         assert(NUMBER == LispDatum_type(dtm1));
//         assert(num == dtm1);
//         assert(1 == LispDatum_refc(num));

//         Atom *atm1_cpy = Atom_copy(atm1);
//         assert(2 == LispDatum_refc(num));

//         LispDatum *s_yes = (LispDatum*) Symbol_intern("yes");
//         Atom_set(atm1, s_yes);
//         assert(1 == LispDatum_refc(num));

//         assert(num == Atom_deref(atm1_cpy));

//         Atom_free(atm1);
//         assert(1 == LispDatum_refc(num));

//         assert(55 == ((Number*)Atom_deref(atm1_cpy))->val);

//         Atom_free(atm1_cpy);
//     }

//     // Exception
//     {
//         throw(NULL, (LispDatum*) Symbol_intern("error"));
//         assert(didthrow());
//         error("hey, that's an error!\n");
//         assert(!didthrow());
//     }

//     // ref count in fresh copies should be reset to 0,
//     // unless the datum is immutable (e.g., Symbol, Number) 
//     {
//         LispDatum *num = (LispDatum*) Number_new(20);
//         LispDatum_own(num);
//         assert(1 == LispDatum_refc(num));
//         LispDatum *num_cpy = LispDatum_copy(num);
//         assert(num == num_cpy);
//         assert(1 == LispDatum_refc(num_cpy));

//         LispDatum *string = (LispDatum*) String_new("hello world");
//         LispDatum_own(string);
//         assert(1 == LispDatum_refc(string));
//         LispDatum *string_cpy = LispDatum_copy(string);
//         assert(string != string_cpy);
//         assert(0 == LispDatum_refc(string_cpy));
//     }


//     free_symbol_table();
// }
