#pragma once

#include "utils.h"
#include "types.h"

typedef struct Reader {
    size_t pos;
    Arr *tokens;
} Reader;

Reader *read_str(const char *str);
char *Reader_next(Reader *rdr);
char *Reader_peek(Reader *rdr);

void Reader_free(Reader *rdr);

MalDatum *read_form(Reader *rdr);
