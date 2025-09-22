/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef UTIL_LINUX_STRV
#define UTIL_LINUX_STRV

#include <stdarg.h>

#include "c.h"

char **strv_free(char **l);
void strv_clear(char **l);
char **strv_copy(char * const *l);
unsigned strv_length(char * const *l);

int strv_extend_strv(char ***a, char **b);
int strv_extend_strv_concat(char ***a, char **b, const char *suffix);
int strv_extend(char ***l, const char *value);

int strv_extendv(char ***l, const char *format, va_list ap)
		__attribute__ ((__format__ (__printf__, 2, 0)));
int strv_extendf(char ***l, const char *format, ...)
		__attribute__ ((__format__ (__printf__, 2, 3)));

int strv_push(char ***l, char *value);
int strv_push_prepend(char ***l, char *value);
int strv_consume(char ***l, char *value);
int strv_consume_prepend(char ***l, char *value);

char **strv_remove(char **l, const char *s);

char **strv_new(const char *x, ...);

static inline const char* STRV_IFNOTNULL(const char *x) {
        return x ? x : (const char *) -1;
}

static inline int strv_isempty(char * const *l) {
        return !l || !*l;
}

char **strv_split(const char *s, const char *separator);
char *strv_join(char **l, const char *separator);

#define STRV_FOREACH(s, l)                      \
        for ((s) = (l); (s) && *(s); (s)++)

#define STRV_FOREACH_BACKWARDS(s, l)            \
        STRV_FOREACH(s, l)                      \
                ;                               \
        for ((s)--; (l) && ((s) >= (l)); (s)--)


#define STRV_MAKE_EMPTY ((char*[1]) { NULL })

char **strv_reverse(char **l);

#endif /* UTIL_LINUX_STRV */


