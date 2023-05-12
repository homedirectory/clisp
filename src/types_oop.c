#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "common.h"
#include "mem_debug.h"
#include "utils.h"
#include "types_oop.h"
#include "hashtbl.h"
#include "env.h"

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
    const DtmMethods *methods = LispDatum_methods(dtm);
    methods->rls(dtm);
    methods->free(dtm);
}

long LispDatum_refc(const LispDatum *dtm)
{
    _LispDatum *_dtm = *dtm;
    return _LispDatum_refc(_dtm);
}

uint LispDatum_type(const LispDatum *dtm)
{
    return LispDatum_methods(dtm)->type(dtm);
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

uint Symbol_type() { 
    return SYMBOL; 
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

// -----------------------------------------------------------------------------
// List < LispDatum

// singleton empty list
static const List g_empty_list = { .len = 0, .head = NULL, .tail = NULL };
const List *List_empty() { return &g_empty_list; }

uint List_type() { 
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

Number *Number_new(long val)
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
uint Number_type()
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

long Number_tol(const Number *num)
{
    return num->val;
}


// -----------------------------------------------------------------------------
// String < LispDatum

// generic method implementations
uint String_type()
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
        .copy = (dtm_copy_ft) String_copy
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
uint Nil_type()
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
uint False_type()
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
uint True_type()
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


// -----------------------------------------------------------------------------
// Proc < LispDatum

// generic method implementations
uint Proc_type()
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
    LispDatum_own((LispDatum*) name);

    proc->argc = argc * (variadic ? -1 : 1);

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

    proc->argc = argc * (variadic ? -1 : 1);

    proc->builtin = true;
    proc->macro = false;

    proc->logic.apply = apply;

    proc->env = NULL;

    proc->super = _LispDatum_new(&Proc_methods);

    return proc;
}

bool Proc_isva(const Proc *proc)
{
    return proc->argc < 0;
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

void Proc_set_name(Proc *proc, Symbol *name)
{
    if (proc->name) {
        LispDatum_rls_free((LispDatum*) proc->name);
    }
    proc->name = name;
    LispDatum_own((LispDatum*) name);
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

    Symbol *s_hello = Symbol_intern("hello");
    Symbol *s_world = Symbol_intern("world");

    printf("%s\n", LispDatum_type((LispDatum*) s_hello) == SYMBOL ? "true" : "false");

    {
        List *list = List_new();
        List_add(list, (LispDatum*) s_hello);
        List_add(list, (LispDatum*) s_world);
        for (struct Node *node = list->head; node; node = node->next) {
            char *s = LispDatum_typename(node->value);
            printf("%s\n", s);
            free(s);
        }
        printf("%s\n", LispDatum_type((LispDatum*) list) == LIST ? "true" : "false");
        List_free(list);
    }

    {
        Number *n1 = Number_new(123);
        Number *n2 = Number_new(8872);
        Number *sum = Number_new(0);
        Number_add(sum, n1);
        Number_add(sum, n2);
        printf("%ld + %ld = %ld\n", Number_tol(n1), Number_tol(n2), Number_tol(sum));
        Number_free(n1);
        Number_free(n2);
        Number_free(sum);
    }

    {
        String *str1 = String_new("hello world, it's me, the programmer");
        printf("%s\n", String_str(str1));
        LispDatum_free((LispDatum*) str1);
    }

    // singletons
    {
        const Nil *nil = Nil_get();
        assert(nil == Nil_get());
        const True *tru = True_get();
        assert(tru == True_get());
        const False *fls = False_get();
        assert(fls == False_get());

        assert(1 == LispDatum_refc((LispDatum*) nil));
        LispDatum_free((LispDatum*) nil);
        assert(1 == LispDatum_refc((LispDatum*) nil));
        LispDatum_own((LispDatum*) nil);
        assert(1 == LispDatum_refc((LispDatum*) nil));

        assert(1 == LispDatum_refc((LispDatum*) fls));
        LispDatum_free((LispDatum*) fls);
        assert(1 == LispDatum_refc((LispDatum*) fls));
        LispDatum_own((LispDatum*) fls);
        assert(1 == LispDatum_refc((LispDatum*) fls));

        assert(1 == LispDatum_refc((LispDatum*) tru));
        LispDatum_free((LispDatum*) tru);
        assert(1 == LispDatum_refc((LispDatum*) tru));
        LispDatum_own((LispDatum*) tru);
        assert(1 == LispDatum_refc((LispDatum*) tru));
    }

    // env
    {
        MalEnv *env = MalEnv_new(NULL);
        Symbol *s_a = Symbol_intern("a");
        Number *one = Number_new(1);

        MalEnv_put(env, s_a, (LispDatum*) one);
        assert((LispDatum*) one == MalEnv_get(env, s_a));

        Number *two = Number_new(2);
        MalEnv_put(env, s_a, (LispDatum*) two);
        assert((LispDatum*) two == MalEnv_get(env, s_a));

        MalEnv_free(env);
    }

    free_symbol_table();
}
