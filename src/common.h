#pragma once

#include <stdio.h>

typedef void (*free_t)(void *ptr);

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define LOG(fmt, ...) \
    do { \
        fprintf(stderr, "%s:%d in %s: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    } while (0);

#define ERROR(fmt, ...) \
    LOG("ERROR: " fmt, ##__VA_ARGS__);

#define FATAL(fmt, ...) \
    do {\
        LOG("FATAL ERROR: " fmt, ##__VA_ARGS__); \
        exit(EXIT_FAILURE); \
    } while (0);

#ifdef _MAL_TRACE
#define DEBUG(fmt, ...) \
    do { \
    fprintf(stderr, "[DEBUG] "); \
    LOG(fmt, ##__VA_ARGS__); \
    } while (0);
#else
#define DEBUG(fmt, ...) ; // a no-op
#endif

#define LOG_NULL(name)\
    DEBUG(#name " was NULL");
