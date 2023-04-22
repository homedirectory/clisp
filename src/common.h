#pragma once

#include <stdio.h>

#define LOG_NULL(name, loc) {\
    printf(#loc ": " #name " was NULL\n");\
}
