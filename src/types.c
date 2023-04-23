#include "types.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"


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
    if (list == NULL) return;
    else if (list->head == NULL) {
        free(list);
    } 
    else {
        struct Node *node = list->head;
        while (node) {
            MalDatum_free(node->value);
            struct Node *p = node;
            node = node->next;
            free(p);
        }
        free(list);
    }
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

    return strcmp(sym1->name, sym2->name) == 0;
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

// MalType ----------------------------------------
char *MalType_tostr(MalType type) {
    char *buf;
    switch (type) {
        case INT:
            buf = "INT";
            break;
        case SYMBOL:
            buf = "SYMBOL";
            break;
        case LIST:
            buf = "LIST";
            break;
        case STRING:
            buf = "STRING";
            break;
        case INTPROC2:
            buf = "INTPROC2";
            break;
        case NIL:
            buf = "NIL";
            break;
        case TRUE:
            buf = "TRUE";
            break;
        case FALSE:
            buf = "FALSE";
            break;
        default:
            buf = "*unknown*";
            break;
    }
    // TODO use statically allocated memory
    return dyn_strcpy(buf);
}

// singletons
static MalDatum _MalDatum_nil = { NIL };
static MalDatum _MalDatum_true = { TRUE };
static MalDatum _MalDatum_false = { FALSE };

MalDatum *MalDatum_nil() {
    return &_MalDatum_nil;
};
MalDatum *MalDatum_true() {
    return &_MalDatum_true;
};
MalDatum *MalDatum_false() {
    return &_MalDatum_false;
};

MalDatum *MalDatum_new_int(const int i) {
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->type = INT;
    mdp->value.i = i;
    return mdp;
}

// *symbol is not copied
MalDatum *MalDatum_new_sym(Symbol *symbol) { 
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->type = SYMBOL;
    mdp->value.sym = symbol;
    return mdp;
}

// *list is not copied
MalDatum *MalDatum_new_list(List *list) {
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->type = LIST;
    mdp->value.list = list;
    return mdp;
}

// *char is copied
MalDatum *MalDatum_new_string(const char *str) {
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->type = STRING;
    mdp->value.string = dyn_strcpy(str);
    return mdp;
}

MalDatum *MalDatum_new_intproc2(const intproc2_t proc) {
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->type = INTPROC2;
    mdp->value.intproc2 = proc;
    return mdp;
}

void MalDatum_free(MalDatum *datum) {
    if (datum == NULL) return;

    switch (datum->type) {
        // NIL, TRUE, FALSE should never be freed
        case NIL: return;
        case TRUE: return;
        case FALSE: return;
        case LIST:
            List_free(datum->value.list);
            break;
        case STRING:
            free(datum->value.string);
            break;
        case SYMBOL:
            Symbol_free(datum->value.sym);
            break;
        default:
            break;
    }

    free(datum);
}

bool MalDatum_istype(const MalDatum *datum, MalType type) {
    return datum->type == type;
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
        case INTPROC2:
            out = MalDatum_new_intproc2(datum->value.intproc2);
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
        default:
            DEBUG("MalDatum_copy: unknown MalType");
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
