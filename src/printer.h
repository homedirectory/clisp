#pragma once

#include "types.h"
#include "stdbool.h"

char *pr_str(MalDatum *datum, bool print_readably);
char *pr_list(List *list, bool print_readably);

char *pr_repr(MalDatum *datum);
