#include "types.h"
#include "env.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"
#include "mem_debug.h"
#include <assert.h>
#include "printer.h"


List *List_new() {
    List *list = malloc(sizeof(List));
    list->len = 0;
    list->head = NULL;
    list->tail = NULL;
    return list;
}

size_t List_len(const List *list) {
    return list->len;
}

List *List_copy(const List *list) {
    if (list == NULL) {
        LOG_NULL(list);
        return NULL;
    }

    List *out = List_new();

    struct Node *node = list->head;
    while (node) {
        List_add(out, node->value);
        node = node->next;
    }

    return out;
}

List *List_deep_copy(const List *list) {
    if (list == NULL) {
        LOG_NULL(list);
        return NULL;
    }

    List *out = List_new();

    struct Node *node = list->head;
    while (node) {
        MalDatum *cpy = MalDatum_deep_copy(node->value);
        if (cpy == NULL) {
            LOG_NULL(cpy);
            List_free(out);
            return NULL;
        }
        List_add(out, cpy);
        node = node->next;
    }

    return out;
}

static List _empty_list = { .len = 0, .head = NULL, .tail = NULL };
List *List_empty() {
    return &_empty_list;
}

bool List_isempty(const List *list) {
    if (list == NULL) {
        LOG_NULL(list);
        exit(EXIT_FAILURE);
    }
    return list->len == 0;
}

void List_add(List *list, MalDatum *datum) {
    struct Node *node = malloc(sizeof(struct Node));
    node->value = datum;
    node->next = NULL;

    if (list->tail == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }

    MalDatum_own(datum);

    list->len += 1;
}

MalDatum *List_ref(const List *list, size_t idx) { 
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

/* Frees the memory allocated for each Node of the list including the MalDatums they point to. */
void List_free(List *list) {
    if (list == NULL || list == &_empty_list) return;
    else if (list->head == NULL) {
        free(list);
    } 
    else {
        struct Node *node = list->head;
        while (node) {
            MalDatum_release(node->value);
            MalDatum_free(node->value);
            struct Node *p = node;
            node = node->next;
            free(p);
        }
        free(list);
    }
}

void List_shlw_free(List *list)
{
    if (list == NULL || list == &_empty_list) 
        return;

    struct Node *node = list->head;
    while (node) {
        struct Node *p = node;
        node = node->next;
        free(p);
    }
    free(list);
}

bool List_eq(const List *lst1, const List *lst2) {
    if (lst1 == lst2) return true;
    if (lst1->len != lst2->len) return false;

    struct Node *node1 = lst1->head;
    struct Node *node2 = lst2->head;
    while (node1 != NULL) {
        if (!MalDatum_eq(node1->value, node2->value))
            return false;
        node1 = node1->next;
        node2 = node2->next;
    }
    
    return true;
}

// Symbol ----------------------------------------
Symbol *Symbol_new(const char *name) {
    Symbol *sym = malloc(sizeof(Symbol));
    strcpy(sym->name, name);
    return sym;
}

void Symbol_free(Symbol *symbol) {
    free(symbol);
}

// TODO intern all symbols
bool Symbol_eq(const Symbol *sym1, const Symbol *sym2) {
    if (sym1 == NULL) {
        LOG_NULL(sym1);
        return false;
    }
    if (sym2 == NULL) {
        LOG_NULL(sym1);
        return false;
    }

    return sym1 == sym2 || strcmp(sym1->name, sym2->name) == 0;
}

bool Symbol_eq_str(const Symbol *sym, const char *str) {
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

Symbol *Symbol_copy(const Symbol *sym) {
    if (sym == NULL) {
        LOG_NULL(sym);
        return NULL;
    }

    return Symbol_new(sym->name);
}

// Procedures ----------------------------------------
Proc *Proc_new(const char *name, int argc, bool variadic, const Arr *params, const Arr *body, const MalEnv *env) {
    Proc *proc = malloc(sizeof(Proc));
    proc->name = dyn_strcpy(name);
    proc->argc = argc;
    proc->variadic = variadic;
    proc->params = Arr_copy(params, (copier_t) Symbol_copy);
    proc->builtin = false;
    proc->logic.body = Arr_copy(body, (copier_t) MalDatum_deep_copy);
    proc->env = env;
    return proc;
}

Proc *Proc_builtin(const char *name, int argc, bool variadic, const builtin_apply_t apply) {
    Proc *proc = malloc(sizeof(Proc));
    proc->name = dyn_strcpy(name);
    proc->argc = argc;
    proc->variadic = variadic;
    proc->builtin = true;
    proc->logic.apply = apply;
    proc->env = NULL;
    return proc;
}

Proc *Proc_new_lambda(int argc, bool variadic, const Arr *params, const Arr *body, const MalEnv *env) {
    Proc *proc = malloc(sizeof(Proc));
    proc->name = NULL;
    proc->argc = argc;
    proc->variadic = variadic;
    proc->params = Arr_copy(params, (copier_t) Symbol_copy);
    proc->builtin = false;
    proc->logic.body = Arr_copy(body, (copier_t) MalDatum_deep_copy);
    proc->env = env;
    return proc;
}

char *Proc_name(const Proc *proc)
{
    static const char* lambda_name = "*lambda*";
    if (proc == NULL) {
        LOG_NULL(proc);
        return NULL;
    }

    const char* name = proc->name ? proc->name : lambda_name;
    return dyn_strcpy(name);
}


void Proc_free(Proc *proc) {
    if (proc == NULL) {
        LOG_NULL(proc);
        return;
    }

    if (!proc->builtin) {
        // free params
        Arr_freep(proc->params, (free_t) Symbol_free);
        // free body
        Arr_freep(proc->logic.body, (free_t) MalDatum_free);
    }

    if (proc->name) 
        free(proc->name);

    free(proc);
}

Proc *Proc_copy(const Proc *proc) {
    if (proc == NULL) {
        LOG_NULL(proc);
        return NULL;
    }

    if (proc->builtin) {
        return Proc_builtin(proc->name, proc->argc, proc->variadic, proc->logic.apply);
    }
    else if (proc->name) { // named procedure?
        return Proc_new(proc->name, proc->argc, proc->variadic, proc->params, proc->logic.body, 
                proc->env);
    }
    else { // lambda
        return Proc_new_lambda(proc->argc, proc->variadic, proc->params, proc->logic.body, 
                proc->env);
    }
}

bool Proc_eq(const Proc *proc1, const Proc *proc2) {
    return proc1 == proc2;
}

// MalType ----------------------------------------
char *MalType_tostr(MalType type) {
    static char *names[] = {
        "INT", "SYMBOL", "LIST", 
        "EMPTY_LIST",
        "STRING", "NIL", "TRUE", "FALSE", "PROCEDURE",
        "*undefined*"
    };

    return names[type];
}

// singletons
/*const*/ static MalDatum _MalDatum_nil = { 
    .refc = 1,
    .type = NIL 
};
/*const*/ static MalDatum _MalDatum_true = { 
    .refc = 1,
    .type = TRUE 
};
/*const*/ static MalDatum _MalDatum_false = { 
    .refc = 1,
    .type = FALSE 
};

static MalDatum _MalDatum_empty_list = { 
    .refc = 1,
    .type = EMPTY_LIST, 
    .value.list = &_empty_list
};

MalDatum *MalDatum_nil() {
    return &_MalDatum_nil;
};
MalDatum *MalDatum_true() {
    return &_MalDatum_true;
};
MalDatum *MalDatum_false() {
    return &_MalDatum_false;
};
MalDatum *MalDatum_empty_list() {
    return &_MalDatum_empty_list;
}

MalDatum *MalDatum_new_int(const int i) {
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->refc = 0;
    mdp->type = INT;
    mdp->value.i = i;
    return mdp;
}

// *symbol is not copied
MalDatum *MalDatum_new_sym(Symbol *symbol) { 
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->refc = 0;
    mdp->type = SYMBOL;
    mdp->value.sym = symbol;
    return mdp;
}

// *list is not copied
MalDatum *MalDatum_new_list(List *list) {
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->refc = 0;
    mdp->type = LIST;
    mdp->value.list = list;
    return mdp;
}

// char is copied
MalDatum *MalDatum_new_string(const char *str) {
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->refc = 0;
    mdp->type = STRING;
    mdp->value.string = dyn_strcpy(str);
    return mdp;
}

// *proc is not copied
MalDatum *MalDatum_new_proc(Proc *proc) {
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->refc = 0;
    mdp->type = PROCEDURE;
    mdp->value.proc = proc;
    return mdp;
}

void MalDatum_own(MalDatum *datum) 
{
    if (datum == NULL) {
        LOG_NULL(datum);
        return;
    }

    char *repr = pr_repr(datum);
    DEBUG("own %p -> %s", datum, repr);
    free(repr);
    datum->refc += 1;
}

void MalDatum_release(MalDatum *datum)
{
    if (datum == NULL) {
        LOG_NULL(datum);
        return;
    }
    if (datum->refc <= 0)
        DEBUG("illegal attempt to decrement ref count = %ld", datum->refc);

    char *repr = pr_repr(datum);
    DEBUG("release %p -> %s", datum, repr);
    free(repr);
    datum->refc -= 1;
}

void MalDatum_free(MalDatum *datum) {
    if (datum == NULL) return;
    if (datum->refc > 0) {
        DEBUG("Refuse to free %p with ref count = %ld", datum, datum->refc);
        return;
    }

    switch (datum->type) {
        // NIL, TRUE, FALSE should never be freed
        case NIL: return;
        case TRUE: return;
        case FALSE: return;
        case EMPTY_LIST: return;
        case LIST:
            List_free(datum->value.list);
            break;
        case STRING:
            free(datum->value.string);
            break;
        case SYMBOL:
            Symbol_free(datum->value.sym);
            break;
        case PROCEDURE:
            Proc_free(datum->value.proc);
            break;
        default:
            break;
    }

    FREE(datum);
    free(datum);
}

bool MalDatum_istype(const MalDatum *datum, MalType type) {
    return datum->type == type;
}

bool MalDatum_islist(const MalDatum *datum) {
    return datum->type == LIST || datum->type == EMPTY_LIST;
}

MalDatum *MalDatum_copy(const MalDatum *datum) {
    if (datum == NULL) {
        LOG_NULL(datum);
        return NULL;
    };

    MalDatum *out = NULL;

    switch (datum->type) {
        case INT:
            out = MalDatum_new_int(datum->value.i);
            break;
        case SYMBOL:
            out = MalDatum_new_sym(Symbol_new(datum->value.sym->name));
            break;
        case STRING:
            out = MalDatum_new_string(datum->value.string);
            break;
        case EMPTY_LIST:
            out = MalDatum_empty_list();
            break;
        case LIST:
            out = MalDatum_new_list(List_copy(datum->value.list));
            break;
        case NIL:
            out = MalDatum_nil();
            break;
        case TRUE:
            out = MalDatum_true();
            break;
        case FALSE:
            out = MalDatum_false();
            break;
        case PROCEDURE:
            out = MalDatum_new_proc(Proc_copy(datum->value.proc));
            break;
        default:
            FATAL("unknown MalType");
            break;
    }

    return out;
}

MalDatum *MalDatum_deep_copy(const MalDatum *datum) {
    if (datum == NULL) {
        LOG_NULL(datum);
        return NULL;
    };

    MalDatum *out = NULL;

    switch (datum->type) {
        case LIST:
            out = MalDatum_new_list(List_deep_copy(datum->value.list));
            break;
        default:
            out = MalDatum_copy(datum);
            break;
    }

    return out;
}

bool MalDatum_isnil(MalDatum *datum) {
    if (datum == NULL) {
        LOG_NULL(datum);
        return false;
    }
    return datum->type == NIL;
}

bool MalDatum_isfalse(MalDatum *datum) {
    if (datum == NULL) {
        LOG_NULL(datum);
        return false;
    }
    return datum->type == FALSE;
}

bool MalDatum_eq(const MalDatum *md1, const MalDatum *md2) {
    if (md1 == NULL) {
        LOG_NULL(md1);
        return false;
    }
    if (md2 == NULL) {
        LOG_NULL(md2);
        return false;
    }

    if (md1->type != md2->type)
        return false;

    switch (md1->type) {
        case INT:
            return md1->value.i == md2->value.i;
        case SYMBOL:
            return Symbol_eq(md1->value.sym, md2->value.sym);
        case STRING:
            return strcmp(md1->value.string, md2->value.string) == 0;
        case LIST:
            return List_eq(md1->value.list, md2->value.list);
        case EMPTY_LIST:
            return true;
        case NIL:
            return true;
        case TRUE:
            return true;
        case FALSE:
            return true;
        case PROCEDURE:
            return Proc_eq(md1->value.proc, md2->value.proc);
        default:
            FATAL("unknown MalType");
    }
}
