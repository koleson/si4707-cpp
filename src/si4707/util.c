#include "util.h"

// unoptimized reverse memcpy for confidence buffer copy
void* r_memcpy(void *dest, const void *src, size_t size)
{
	unsigned char *dptr = dest;
	const unsigned char *ptr = src + size;
	const unsigned char *end = src;

	while (ptr > end)
		*dptr++ = *ptr--;

	return dest;
}