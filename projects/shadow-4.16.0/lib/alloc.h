// SPDX-FileCopyrightText: 2023-2024, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#ifndef SHADOW_INCLUDE_LIB_MALLOC_H_
#define SHADOW_INCLUDE_LIB_MALLOC_H_


#include <config.h>

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "attr.h"
#include "defines.h"


#define CALLOC(n, type)   ((type *) calloc(n, sizeof(type)))
#define XCALLOC(n, type)  ((type *) xcalloc(n, sizeof(type)))
#define MALLOC(n, type)   ((type *) mallocarray(n, sizeof(type)))
#define XMALLOC(n, type)  ((type *) xmallocarray(n, sizeof(type)))

#define REALLOC(ptr, n, type)                                                 \
(                                                                             \
	_Generic(ptr, type *:  (type *) reallocarray(ptr, n, sizeof(type)))   \
)

#define REALLOCF(ptr, n, type)                                                \
(                                                                             \
	_Generic(ptr, type *:  (type *) reallocarrayf(ptr, n, sizeof(type)))  \
)

#define XREALLOC(ptr, n, type)                                                \
(                                                                             \
	_Generic(ptr, type *:  (type *) xreallocarray(ptr, n, sizeof(type)))  \
)


ATTR_MALLOC(free)
inline void *xmalloc(size_t size);
ATTR_MALLOC(free)
inline void *xmallocarray(size_t nmemb, size_t size);
ATTR_MALLOC(free)
inline void *mallocarray(size_t nmemb, size_t size);
ATTR_MALLOC(free)
inline void *reallocarrayf(void *p, size_t nmemb, size_t size);
ATTR_MALLOC(free)
inline char *xstrdup(const char *str);

ATTR_MALLOC(free)
void *xcalloc(size_t nmemb, size_t size);
ATTR_MALLOC(free)
void *xreallocarray(void *p, size_t nmemb, size_t size);


inline void *
xmalloc(size_t size)
{
	return xmallocarray(1, size);
}


inline void *
xmallocarray(size_t nmemb, size_t size)
{
	return xreallocarray(NULL, nmemb, size);
}


inline void *
mallocarray(size_t nmemb, size_t size)
{
	return reallocarray(NULL, nmemb, size);
}


inline void *
reallocarrayf(void *p, size_t nmemb, size_t size)
{
	void  *q;

	q = reallocarray(p, nmemb, size);

	/* realloc(p, 0) is equivalent to free(p);  avoid double free.  */
	if (q == NULL && nmemb != 0 && size != 0)
		free(p);
	return q;
}


inline char *
xstrdup(const char *str)
{
	return strcpy(XMALLOC(strlen(str) + 1, char), str);
}


#endif  // include guard
