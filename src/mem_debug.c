#include <stdio.h>

#define PREFIX "[mem_debug] "

void mem_debug_own(const char *name, const void *ptr, const char *file, unsigned int line, const char *func)
{
    if (ptr == NULL)
        printf(PREFIX "%s:%u in %s allocated memory: %s = NULL\n", file, line, func, name);
    else
        printf(PREFIX "%s:%u in %s allocated memory: %s = %p\n", file, line, func, name, ptr);
}

void mem_debug_free(const char *name, const void *ptr, const char *file, unsigned int line, const char *func)
{
    if (ptr == NULL)
        fprintf(stderr, PREFIX "BUG FOUND %s:%u in %s frees memory %s = NULL\n",
                file, line, func, name);
    else
        printf(PREFIX "%s:%u in %s frees memory: %s = %p\n", file, line, func, name, ptr);
}
