#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "reader.h"
#include "types.h"
#include <ctype.h>
#include <stdbool.h>
#include "common.h"
#include <sys/types.h>

#define WHITESPACE_CHARS " \t\n\r"
#define SYMBOL_INV_CHARS WHITESPACE_CHARS "[]{}('\"`,;)"
#define COMMENT_CHAR ';'
#define COMMENT_CHARS ";"
#define QUOTE_MACRO_CHAR '\''
#define QUASIQUOTE_MACRO_CHAR '`'
#define UNQUOTE_MACRO_CHAR '~'
#define SPLICE_UNQUOTE_MACRO_STR "~@" // very fragile parsing, be careful if you change it

/*
 * Read characters from *str until one of characters in string *set is encountered or end
 * of string is reached. 
 * *set must be a null-terminated string.
 * Returns a dynamically allocated parsed string. An empty string might be returned.
 */
static char *parse_until(const char *str, const char *set) {
    const char *p = strchrs(str, set);
    if (p == NULL) {
        // read the whole str
        return dyn_strcpy(str);
    }
    else {
        return dyn_strncpy(str, p - str);
    }
}

// parses a string from *str that must start with a double quote
// the resulting string is wrapped in double quotes
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
        ERROR("unbalanced string: %s", str);
        return NULL;
    }
    return dyn_strncpy(str, i + 1);
}

// transforms a string from a "reader form" to a "datum" form
// by stripping surrounding doublequotes and unescaping characters
static char *str_from_token(char *dst, const char *str)
{
    size_t len = strlen(str);
    size_t di = 0;

    // len - 1 because of the last doublequote
    for (size_t si = 1; si < len - 1; si++, di++) {
        char c = str[si];
        if (c == '\\')
            dst[di] = unescape_char(str[++si]);
        else
            dst[di] = c;
    }

    dst[di] = '\0';

    return dst;
}

// Splits the input string into tokens. Returns NULL upon failure.
// NOTE: keep this procedure as simple as possible
// (i.e. delegate any validation of tokens)
static Arr *tokenize(const char *str) {
    Arr *arr = Arr_new();
    size_t i = 0;
    char c;
    while ((c = str[i]) != '\0') {
        if (isspace(c)) {
            i++;
            continue;
        }

        if (c == COMMENT_CHAR) {
            // read the rest of the line
            ssize_t idx = stridx(str + i + 1, '\n');
            if (idx == -1)
                break;
            else {
                i += idx + 1;
                continue;
            }
        }

        char *tok; // token to be added
        size_t n;  // read step (in bytes)

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
        // quote & quasiquote macros
        else if (c == QUOTE_MACRO_CHAR || c == QUASIQUOTE_MACRO_CHAR) {
            tok = malloc(2);
            sprintf(tok, "%c", c);
            n = 1;
        }
        // unquote & splice-unquote macro
        else if (c == UNQUOTE_MACRO_CHAR) {
            if (str[i + 1] == SPLICE_UNQUOTE_MACRO_STR[1]) {
                tok = dyn_strcpy(SPLICE_UNQUOTE_MACRO_STR);
                n = 2;
            } 
            else {
                tok = malloc(2);
                sprintf(tok, "%c", UNQUOTE_MACRO_CHAR);
                n = 1;
            }
        }
        // int | symbol
        else {
            // read until whitespace, paren or comment
            tok = parse_until(str + i, WHITESPACE_CHARS "()" COMMENT_CHARS);
            if (tok)
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

#ifdef _MAL_TRACE
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

const char *Reader_next(Reader *rdr) {
    const char *tok = Reader_peek(rdr);
    if (tok == NULL) return NULL;
    rdr->pos += 1;
    return tok;
}

const char *Reader_peek(const Reader *rdr) {
    if (rdr->pos >= rdr->tokens->len)
        return NULL;

    const char *tok = (char*) Arr_get(rdr->tokens, rdr->pos);
    return tok;
}

void Reader_free(Reader *rdr) {
    Arr_free(rdr->tokens);
    free(rdr);
}

static MalDatum *read_atom(const char *token) {
    //printf("read_atom token: %s\n", token);
    if (token == NULL || token[0] == '\0') return NULL;

    // int
    if (isdigit(token[0]) || (token[0] == '-' && isdigit(token[1]))) {
        int i = strtol(token, NULL, 10);
        return MalDatum_new_int(i);
    }
    // string
    else if (token[0] == '"') {
        char datum_form[strlen(token) - 2 + 1]; // 2 surrounding doublequotes
        str_from_token(datum_form, token);
        return MalDatum_new_string(datum_form);
    }
    // symbol
    else if (strchr(SYMBOL_INV_CHARS, token[0]) == NULL) {
        return MalDatum_new_sym(Symbol_new(token));
    }
    else {
        DEBUG("Unknown atom: %s", token);
        return NULL;
    }
}

// current Reader token should be the next one after an open paren
static MalDatum *read_list(Reader *rdr) {
    bool closed = false;
    List *list = List_new();

    const char *token = Reader_peek(rdr);
    while (token != NULL) {
        if (token[0] == ')') {
            closed = true;
            break;
        }
        MalDatum *form = read_form(rdr);
        if (form == NULL) {
            List_free(list);
            DEBUG("Illegal form");
            return NULL;
        }
        List_add(list, form);
        token = Reader_peek(rdr);
    }

    if (!closed) {
        ERROR("unbalanced open paren '('");
        List_free(list);
        return NULL;
    }

    Reader_next(rdr); // skip over closing paren
    return List_isempty(list) ? MalDatum_empty_list() : MalDatum_new_list(list);
}

MalDatum *read_form(Reader *rdr) {
    const char *token = Reader_next(rdr);
    if (!token) { // no more tokens
        return NULL;
    }
    // list
    else if (token[0] == '(') {
        return read_list(rdr);
    } 
    else if (token[0] == ')') {
        ERROR("unbalanced closing paren '%c'", token[0]);
        return NULL;
    }
    // quote macro
    else if (token[0] == QUOTE_MACRO_CHAR) {
        MalDatum *next_form = read_form(rdr);
        if (!next_form) {
            ERROR("bad syntax: stray quote (%c)", QUOTE_MACRO_CHAR);
            return NULL;
        }
        List *list = List_new();
        List_add(list, MalDatum_new_sym(Symbol_new("quote")));
        List_add(list, next_form);
        return MalDatum_new_list(list);
    }
    // quasiquote macro
    else if (token[0] == QUASIQUOTE_MACRO_CHAR) {
        MalDatum *next_form = read_form(rdr);
        if (!next_form) {
            ERROR("bad syntax: stray quasiquote (%c)", QUASIQUOTE_MACRO_CHAR);
            return NULL;
        }
        List *list = List_new();
        List_add(list, MalDatum_new_sym(Symbol_new("quasiquote")));
        List_add(list, next_form);
        return MalDatum_new_list(list);
    }
    // splice-unquote macro
    else if (strcmp(token, SPLICE_UNQUOTE_MACRO_STR) == 0) {
        MalDatum *next_form = read_form(rdr);
        if (!next_form) {
            ERROR("bad syntax: stray splice-unquote (%s)", SPLICE_UNQUOTE_MACRO_STR);
            return NULL;
        }
        List *list = List_new();
        List_add(list, MalDatum_new_sym(Symbol_new("splice-unquote")));
        List_add(list, next_form);
        return MalDatum_new_list(list);
    }
    // unquote macro
    else if (token[0] == UNQUOTE_MACRO_CHAR) {
        MalDatum *next_form = read_form(rdr);
        if (!next_form) {
            ERROR("bad syntax: stray unquote (%c)", UNQUOTE_MACRO_CHAR);
            return NULL;
        }
        List *list = List_new();
        List_add(list, MalDatum_new_sym(Symbol_new("unquote")));
        List_add(list, next_form);
        return MalDatum_new_list(list);
    }
    // atom
    // TODO allow multiple atoms (in a top-level expression)
    else {
        return read_atom(token);
    }
}
