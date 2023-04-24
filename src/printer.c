#include <stdio.h>
#include "printer.h"
#include "reader.h"
#include "utils.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include "common.h"

char *pr_str(MalDatum *datum) {
    if (datum == NULL) return NULL;

    char *str;
    switch (datum->type) {
        case INT:
            str = malloc(MAX_INT_DIGITS + 1);
            sprintf(str, "%d", datum->value.i);
            break;
        case SYMBOL:
            str = dyn_strcpy(datum->value.sym->name);
            break;
        case LIST:
            List *list = datum->value.list;
            str = list ? pr_list(list) : NULL;
            break;
        case EMPTY_LIST:
            str = dyn_strcpy("()");
            break;
        case STRING:
            char *string = datum->value.string;
            str = string ? dyn_strcpy(string) : NULL;
            break;
        case NIL:
            str = dyn_strcpy("nil");
            break;
        case TRUE:
            str = dyn_strcpy("true");
            break;
        case FALSE:
            str = dyn_strcpy("false");
            break;
        case PROCEDURE:
            str = dyn_strcpy("#<procedure>");
            break;
        default:
            char *s = MalType_tostr(datum->type);
            DEBUG("Unknown MalType");
            free(s);
            str = NULL;
            break;
    }

    return str;
}

// returns a new string with the contents of the given list separeted by spaces 
// and wrapped in parens
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

