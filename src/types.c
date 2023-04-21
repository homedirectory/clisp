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

char *List_tostr(List *list) {
    if (list == NULL) return NULL;
    int cap = 256;
    char *str = calloc(cap, sizeof(char));
    str[0] = '(';
    int len = 1;

    struct Node *node = list->head;
    while (node) {
        char *s = MalDatum_tostr((MalDatum*) node->value);
        int slen = strlen(s);
        if (len + slen >= cap) {
            cap = slen + cap * 1.5;
            str = realloc(str, cap * sizeof(char));
        }
        strcat(str, s);
        len += slen;
        strcat(str, " ");
        len++;
        free(s);
        node = node->next;
    }
    // if non-empty replace last redundant ' '
    if (len > 1) {
        len--;
    }
    str[len] = ')';
    str[len + 1] = '\0';

    return str;
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

void MalDatum_free(MalDatum *datum) {
    if (datum == NULL) return;
    switch (datum->type) {
        case LIST:
            List_free(datum->value.list);
            break;
        default:
            break;
    }
    free(datum);
}

char *MalDatum_tostr(MalDatum *datum) {
    if (datum == NULL) return NULL;

    char *str;
    switch (datum->type) {
        case INT:
            str = malloc(MAX_INT_DIGITS + 1);
            sprintf(str, "%d", datum->value.i);
            break;
        case SYMBOL:
            str = dyn_strcpy(datum->value.sym);
            break;
        case LIST:
            str = List_tostr(datum->value.list);
            break;
        default:
            fprintf(stderr, "Unknown MalType: %d\n", datum->type);
            str = NULL;
            break;
    }

    return str;
}

bool MalDatum_istype(MalDatum *datum, MalType type) {
    return datum->type == type;
}
