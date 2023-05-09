#pragma once

#include "types.h"
#include "stdbool.h"

char *pr_str(const MalDatum *datum, bool print_readably);
char *pr_list(const List *list, bool print_readably);

char *pr_repr(const MalDatum *datum);
