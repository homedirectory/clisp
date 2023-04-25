//void *mem_debug_malloc(size_t size, const char *file, unsigned int line, const char *func);

void mem_debug_own(const char *name, const void *ptr, const char *file, unsigned int line, const char *func);
void mem_debug_free(const char *name, const void *ptr, const char *file, unsigned int line, const char *func);

#ifdef MEM_DEBUG

//#define malloc(n) mem_debug_malloc(n, __FILE__, __LINE__, __func__)

#define OWN(var) mem_debug_own(#var, var, __FILE__, __LINE__, __func__);
#define FREE(var) mem_debug_free(#var, var, __FILE__, __LINE__, __func__);

#else

#define OWN(var) ;
#define FREE(var) ;

#endif
