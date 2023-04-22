#pragma once

#include <stdio.h>

#define LOG_NULL(name, loc)\
    printf(#loc ": " #name " was NULL\n")

#define ERROR(loc, fmt, ...)\
    fprintf(stderr, "ERROR in " #loc ": " fmt "\n", ##__VA_ARGS__)
