#include <stdio.h>
#include "printer.h"
#include "reader.h"
#include "utils.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>


char *pr_str(MalDatum *datum) {
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
            str = pr_list(datum->value.list);
            break;
        case STRING:
            str = dyn_strcpy(datum->value.string);
            break;
        default:
            char *s = MalType_tostr(datum->type);
            fprintf(stderr, "Unknown MalType: %s\n", s);
            free(s);
            str = NULL;
            break;
    }

    return str;
}

char *pr_list(List *list) {
    if (list == NULL) return NULL;
    size_t cap = 256;
    char *str = calloc(cap, sizeof(char));
    str[0] = '(';
    size_t len = 1;

    struct Node *node = list->head;
    while (node) {
        char *s = pr_str((MalDatum*) node->value);
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

