#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "reader.h"
#include "types.h"
#include <ctype.h>
#include <stdbool.h>

#define WHITESPACE_CHARS " \t\n\r"
#define SYMBOL_INV_CHARS WHITESPACE_CHARS "[]{}('\"`,;)"

/* 
 * parses an int from *str and stores it as a string in *out
 * returns the length of the parsed int (digits count + minus sign optionally) or -1 on failure
 */
static int parse_int(const char *str, char *out) {
    int d;
    int ret = sscanf(str, "%d", &d);
    if (ret == 0 || ret == EOF) {
        return -1;
    } else {
        sprintf(out, "%d", d);
        return strlen(out);
    }
}

/*
 * Read characters from *str until one of characters in string *set is encountered or end
 * of string is reached. 
 * *set must be a null-terminated string.
 * Returns a dynamically allocated parsed string. An empty string might be returned.
 */
static char *parse_until(const char *str, const char *set) {
    char *p = strchrs(str, set);
    if (p == NULL) {
        // read the whole str
        return dyn_strcpy(str);
    }
    else {
        size_t n = p - str;
        char *out = calloc(n + 1, sizeof(char));
        memcpy(out, str, n);
        out[n] = '\0';
        return out;
    }
}

// splits the input string into tokens
// NOTE: keep this procedure as simple as possible
// (i.e. delegate any validation of tokens)
static Arr *tokenize(const char *str) {
    Arr *arr = Arr_new();

    // skip whitespace
    size_t i = 0;
    char c;
    while ((c = str[i]) != '\0') {
        if (isspace(c)) {
            i++;
            continue;
        }

        char *tok; // token to be added
        size_t n;  // read step

        if (c == '(' || c == ')') {
            tok = malloc(2);
            sprintf(tok, "%c", c);
            n = 1;
        }
        // TODO special characters
        // TODO strings
        // TODO comments
        else {
            // read until whitespace or paren
            tok = parse_until(str + i, WHITESPACE_CHARS "()");
            n = strlen(tok);
        }

        Arr_add(arr, tok);
        i += n;
    }

    return arr;
}

Reader *read_str(const char *str) {
    Arr *tokens = tokenize(str);
    if (tokens == NULL)
        return NULL;

    //puts("tokens:");
    //for (size_t i = 0; i < tokens->len; i++) {
    //    printf("%s\n", (char*) tokens->items[i]);
    //}

    Reader *rdr = malloc(sizeof(Reader));
    rdr->pos = 0;
    rdr->tokens = tokens;
    return rdr;
}

char *Reader_next(Reader *rdr) {
    if (rdr->pos >= rdr->tokens->len)
        return NULL;

    char *tok = Reader_peek(rdr);
    rdr->pos += 1;
    return tok;
}

char *Reader_peek(Reader *rdr) {
    if (rdr->pos >= rdr->tokens->len)
        return NULL;

    char *tok = (char*) Arr_get(rdr->tokens, rdr->pos);
    return tok;
}

void Reader_free(Reader *rdr) {
    Arr_free(rdr->tokens);
    free(rdr);
}

static MalDatum *read_atom(char *token) {
    //printf("read_atom token: %s\n", token);
    if (token == NULL || token[0] == '\0') return NULL;

    // int
    if (isdigit(token[0]) || (token[0] == '-' && isdigit(token[1]))) {
        int i = strtol(token, NULL, 10);
        return MalDatum_new_int(i);
    }
    // symbol
    else if (strchr(SYMBOL_INV_CHARS, token[0]) == NULL) {
        return MalDatum_new_sym(token);
    }
    // TODO nil
    // TODO false
    // TODO true
    // TODO string
    else {
        fprintf(stderr, "Unknown atom: %s\n", token);
        return NULL;
    }
}

// TODO make sure all parens are balanced (during tokenization?)
static MalDatum *read_list(Reader *rdr) {
    bool closed = false;
    List *list = List_new();

    char *token = Reader_peek(rdr);
    while (token != NULL) {
        if (token[0] == ')') {
            closed = true;
            break;
        }
        MalDatum *form = read_form(rdr);
        if (form == NULL) {
            fprintf(stderr, "ERR: Illegal form\n");
            return NULL;
        }
        List_add(list, form);
        token = Reader_peek(rdr);
    }

    if (!closed) {
        fprintf(stderr, "ERR: Unclosed list\n");
        List_free(list);
        return NULL;
    }

    Reader_next(rdr); // skip over ')'
    return MalDatum_new_list(list);
}

MalDatum *read_form(Reader *rdr) {
    char *token = Reader_next(rdr);
    // list
    if (token[0] == '(') {
        return read_list(rdr);
    } 
    else if (token[0] == ')') {
        fprintf(stderr, "ERR: Unopened list\n");
        return NULL;
    }
    // atom
    else {
        return read_atom(token);
    }
}