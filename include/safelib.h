#include <stdlib.h>

void *smalloc(size_t size) __attribute__((alloc_size(1)));
void *scalloc(size_t nmemb, size_t size) __attribute__((alloc_size(1,2)));
void *srealloc(void *ptr, size_t size) __attribute__((alloc_size(2)));
void *sreallocarray(void *ptr, size_t nmemb, size_t size)  __attribute__((alloc_size(2,3)));
void *s_mm_malloc(size_t size, size_t align)  __attribute__((alloc_size(1)));
