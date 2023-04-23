#pragma once

#include <stdio.h>

typedef void (*free_t)(void *ptr);

#define LOG_NULL(name, loc)\
    printf(#loc ": " #name " was NULL\n")

#define ERROR(loc, fmt, ...)\
    fprintf(stderr, "ERROR in " #loc ": " fmt "\n", ##__VA_ARGS__)

#define FATAL(loc, fmt, ...) {\
    do {\
        fprintf(stderr, "FATAL ERROR in " #loc ": " fmt "\n", ##__VA_ARGS__);\
        exit(EXIT_FAILURE);\
    } while (0);\
}

#ifdef TRACE
#define DEBUG(loc, fmt, ...)\
    fprintf(stderr, "[DEBUG] in " #loc ": " fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG(loc, fmt, ...) ; // a no-op
#endif

