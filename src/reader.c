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
/*
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
*/

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

// parses a string from *str that must start with '"'
// the resulting string is wrapped with double quotes
static char *parse_string(const char *str) {
    // str[0] is '"', so start from str + 1
    size_t i = 1;
    char c;
    bool escaped = false;
    while ((c = str[i]) != '\0' && (c != '"' || escaped)) {
        if (c == '\\')
            escaped = !escaped;
        else if (escaped)
            escaped = false;
        i++;
    }
    if (c == '\0') {
        fprintf(stderr, "ERR: unbalanced string: %s\n", str);
        return NULL;
    }
    char *out = calloc(i + 2, sizeof(char));
    strncat(out, str, i + 1);
    out[i + 1] = '\0';
    return out;
}

// Splits the input string into tokens. Returns NULL upon failure.
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

        // paren
        if (c == '(' || c == ')') {
            tok = malloc(2);
            sprintf(tok, "%c", c);
            n = 1;
        }
        // string
        else if (c == '"') {
            tok = parse_string(str + i);
            if (tok) {
                n = strlen(tok);
            }
        }

        // TODO special characters
        // TODO comments

        // int | symbol
        else {
            // read until whitespace or paren
            tok = parse_until(str + i, WHITESPACE_CHARS "()");
            n = strlen(tok);
        }

        if (tok == NULL) {
            Arr_free(arr);
            return NULL;
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

#ifdef DEBUG
    puts("tokens:");
    for (size_t i = 0; i < tokens->len; i++) {
        printf("%s\n", (char*) tokens->items[i]);
    }
#endif

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
    // string
    else if (token[0] == '"') {
        return MalDatum_new_string(token);
    }
    // symbol
    else if (strchr(SYMBOL_INV_CHARS, token[0]) == NULL) {
        return MalDatum_new_sym(Symbol_new(token));
    }
    // TODO nil
    // TODO false
    // TODO true
    else {
        fprintf(stderr, "Unknown atom: %s\n", token);
        return NULL;
    }
}

// current Reader token should be the next one after an open paren
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
            List_free(list);
            //fprintf(stderr, "ERR: Illegal form\n");
            return NULL;
        }
        List_add(list, form);
        token = Reader_peek(rdr);
    }

    if (!closed) {
        fprintf(stderr, "ERR: unbalanced open paren '('\n");
        List_free(list);
        return NULL;
    }

    Reader_next(rdr); // skip over closing paren
    return MalDatum_new_list(list);
}

MalDatum *read_form(Reader *rdr) {
    char *token = Reader_next(rdr);
    // list
    if (token[0] == '(') {
        return read_list(rdr);
    } 
    else if (token[0] == ')') {
        fprintf(stderr, "ERR: unbalanced closing paren '%c'\n", token[0]);
        return NULL;
    }
    // atom
    // TODO allow multiple atoms (in a top-level expression)
    else {
        return read_atom(token);
    }
}
