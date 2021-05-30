#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

void *smalloc(size_t size) {
	void *ptr = malloc(size);
	if (size == 0 || ptr != NULL) return ptr; // Success
	else {
		fprintf(stderr, "smalloc(%zd) failed\n", size);
		exit(EXIT_FAILURE);
	}
}

void *scalloc(size_t nmemb, size_t size) {
	void *ptr = calloc(nmemb, size);
	if (nmemb == 0 || size == 0 || ptr != NULL) return ptr; // Success
	else {
		fprintf(stderr, "scalloc(%zd, %zd) failed\n", nmemb, size);
		exit(EXIT_FAILURE);
	}
}

void *srealloc(void *ptr, size_t size) {
	void *ptr2 = realloc(ptr, size);
	if (size == 0 || ptr2 != NULL) return ptr2; // Success
	else {
		fprintf(stderr, "srealloc(%p, %zd) failed\n", ptr, size);
		exit(EXIT_FAILURE);
	}
}

void *sreallocarray(void *ptr, size_t nmemb, size_t size) {
	void *ptr2 = reallocarray(ptr, nmemb, size);
	if (nmemb == 0 || size == 0 || ptr2 != NULL) return ptr2; // Success
	else {
		fprintf(stderr, "sreallocarray(%p, %zd, %zd) failed\n", ptr, nmemb, size);
		exit(EXIT_FAILURE);
	}
}

