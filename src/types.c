#include "types.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


List *List_new() {
    List *list = malloc(sizeof(List));
    list->len = 0;
    list->head = NULL;
    list->tail = NULL;
    return list;
}

void List_add(List *list, void *ptr) {
    struct Node *node = malloc(sizeof(struct Node));
    node->value = ptr;
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

// TODO accept a function to pointer to free Node values
void List_free(List *list) {
    if (list == NULL) return;
    else if (list->head == NULL) {
        free(list);
    } 
    else {
        struct Node *node = list->head;
        while (node) {
            free(node->value);
            struct Node *p = node;
            node = node->next;
            free(p);
        }
        free(list);
    }
}

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
        default:
            buf = "*unknown*";
            break;
    }
    return dyn_strcpy(buf);
}

MalDatum *MalDatum_new_int(const int i) {
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->type = INT;
    mdp->value.i = i;
    return mdp;
}

// *sym is copied
MalDatum *MalDatum_new_sym(const char *sym) { 
    MalDatum *mdp = malloc(sizeof(MalDatum));
    mdp->type = SYMBOL;
    //char *cpy = dyn_strcpy(sym);
    //mdp->value.sym = sym;
    // TODO sym might break the length boundary
    strcpy(mdp->value.sym, sym);
    return mdp;
}

// *list is used, not copied
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

void MalDatum_free(MalDatum *datum) {
    if (datum == NULL) return;
    switch (datum->type) {
        case LIST:
            List_free(datum->value.list);
            break;
        case STRING:
            free(datum->value.string);
            break;
        default:
            break;
    }
    free(datum);
}

bool MalDatum_istype(MalDatum *datum, MalType type) {
    return datum->type == type;
}
