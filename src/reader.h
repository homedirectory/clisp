#pragma once

#include "utils.h"
#include "types.h"

struct Reader {
    size_t pos;
    Arr *tokens;
};
typedef struct Reader Reader;

Reader *read_str(const char *str);
char *Reader_next(Reader *rdr);
char *Reader_peek(Reader *rdr);

void Reader_free(Reader *rdr);

MalDatum *read_form(Reader *rdr);
