#pragma once

#include "utils.h"
#include "types.h"

typedef struct Reader {
    size_t pos;
    Arr *tokens;
} Reader;

Reader *read_str(const char *str);
const char *Reader_next(Reader *rdr);
const char *Reader_peek(const Reader *rdr);

void Reader_free(Reader *rdr);

MalDatum *read_form(Reader *rdr);
