// SPDX-FileCopyrightText: 2023-2024, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#ifndef SHADOW_INCLUDE_LIB_ATOI_A2I_H_
#define SHADOW_INCLUDE_LIB_ATOI_A2I_H_


#include <config.h>

#include <errno.h>

#include "atoi/strtoi.h"
#include "atoi/strtou_noneg.h"
#include "attr.h"


/*
 * See the manual of these macros in liba2i's documentation:
 * <http://www.alejandro-colomar.es/share/dist/liba2i/git/HEAD/liba2i-HEAD.pdf>
 */


#define a2i(TYPE, n, s, ...)                                                  \
(                                                                             \
	_Generic((void (*)(TYPE, typeof(s))) 0,                               \
		void (*)(short,              const char *):  a2sh_c,          \
		void (*)(short,              const void *):  a2sh_c,          \
		void (*)(short,              char *):        a2sh_nc,         \
		void (*)(short,              void *):        a2sh_nc,         \
		void (*)(int,                const char *):  a2si_c,          \
		void (*)(int,                const void *):  a2si_c,          \
		void (*)(int,                char *):        a2si_nc,         \
		void (*)(int,                void *):        a2si_nc,         \
		void (*)(long,               const char *):  a2sl_c,          \
		void (*)(long,               const void *):  a2sl_c,          \
		void (*)(long,               char *):        a2sl_nc,         \
		void (*)(long,               void *):        a2sl_nc,         \
		void (*)(long long,          const char *):  a2sll_c,         \
		void (*)(long long,          const void *):  a2sll_c,         \
		void (*)(long long,          char *):        a2sll_nc,        \
		void (*)(long long,          void *):        a2sll_nc,        \
		void (*)(unsigned short,     const char *):  a2uh_c,          \
		void (*)(unsigned short,     const void *):  a2uh_c,          \
		void (*)(unsigned short,     char *):        a2uh_nc,         \
		void (*)(unsigned short,     void *):        a2uh_nc,         \
		void (*)(unsigned int,       const char *):  a2ui_c,          \
		void (*)(unsigned int,       const void *):  a2ui_c,          \
		void (*)(unsigned int,       char *):        a2ui_nc,         \
		void (*)(unsigned int,       void *):        a2ui_nc,         \
		void (*)(unsigned long,      const char *):  a2ul_c,          \
		void (*)(unsigned long,      const void *):  a2ul_c,          \
		void (*)(unsigned long,      char *):        a2ul_nc,         \
		void (*)(unsigned long,      void *):        a2ul_nc,         \
		void (*)(unsigned long long, const char *):  a2ull_c,         \
		void (*)(unsigned long long, const void *):  a2ull_c,         \
		void (*)(unsigned long long, char *):        a2ull_nc,        \
		void (*)(unsigned long long, void *):        a2ull_nc         \
	)(n, s, __VA_ARGS__)                                                  \
)


#define a2sh(n, s, ...)                                                       \
(                                                                             \
	_Generic(s,                                                           \
		const char *:  a2sh_c,                                        \
		const void *:  a2sh_c,                                        \
		char *:        a2sh_nc,                                       \
		void *:        a2sh_nc                                        \
	)(n, s, __VA_ARGS__)                                                  \
)

#define a2si(n, s, ...)                                                       \
(                                                                             \
	_Generic(s,                                                           \
		const char *:  a2si_c,                                        \
		const void *:  a2si_c,                                        \
		char *:        a2si_nc,                                       \
		void *:        a2si_nc                                        \
	)(n, s, __VA_ARGS__)                                                  \
)

#define a2sl(n, s, ...)                                                       \
(                                                                             \
	_Generic(s,                                                           \
		const char *:  a2sl_c,                                        \
		const void *:  a2sl_c,                                        \
		char *:        a2sl_nc,                                       \
		void *:        a2sl_nc                                        \
	)(n, s, __VA_ARGS__)                                                  \
)

#define a2sll(n, s, ...)                                                      \
(                                                                             \
	_Generic(s,                                                           \
		const char *:  a2sll_c,                                       \
		const void *:  a2sll_c,                                       \
		char *:        a2sll_nc,                                      \
		void *:        a2sll_nc                                       \
	)(n, s, __VA_ARGS__)                                                  \
)

#define a2uh(n, s, ...)                                                       \
(                                                                             \
	_Generic(s,                                                           \
		const char *:  a2uh_c,                                        \
		const void *:  a2uh_c,                                        \
		char *:        a2uh_nc,                                       \
		void *:        a2uh_nc                                        \
	)(n, s, __VA_ARGS__)                                                  \
)

#define a2ui(n, s, ...)                                                       \
(                                                                             \
	_Generic(s,                                                           \
		const char *:  a2ui_c,                                        \
		const void *:  a2ui_c,                                        \
		char *:        a2ui_nc,                                       \
		void *:        a2ui_nc                                        \
	)(n, s, __VA_ARGS__)                                                  \
)

#define a2ul(n, s, ...)                                                       \
(                                                                             \
	_Generic(s,                                                           \
		const char *:  a2ul_c,                                        \
		const void *:  a2ul_c,                                        \
		char *:        a2ul_nc,                                       \
		void *:        a2ul_nc                                        \
	)(n, s, __VA_ARGS__)                                                  \
)

#define a2ull(n, s, ...)                                                      \
(                                                                             \
	_Generic(s,                                                           \
		const char *:  a2ull_c,                                       \
		const void *:  a2ull_c,                                       \
		char *:        a2ull_nc,                                      \
		void *:        a2ull_nc                                       \
	)(n, s, __VA_ARGS__)                                                  \
)


ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2sh_c(short *restrict n, const char *s,
    const char **restrict endp, int base, short min, short max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2si_c(int *restrict n, const char *s,
    const char **restrict endp, int base, int min, int max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2sl_c(long *restrict n, const char *s,
    const char **restrict endp, int base, long min, long max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2sll_c(long long *restrict n, const char *s,
    const char **restrict endp, int base, long long min, long long max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2uh_c(unsigned short *restrict n, const char *s,
    const char **restrict endp, int base, unsigned short min,
    unsigned short max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2ui_c(unsigned int *restrict n, const char *s,
    const char **restrict endp, int base, unsigned int min, unsigned int max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2ul_c(unsigned long *restrict n, const char *s,
    const char **restrict endp, int base, unsigned long min, unsigned long max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2ull_c(unsigned long long *restrict n, const char *s,
    const char **restrict endp, int base, unsigned long long min,
    unsigned long long max);

ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2sh_nc(short *restrict n, char *s,
    char **restrict endp, int base, short min, short max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2si_nc(int *restrict n, char *s,
    char **restrict endp, int base, int min, int max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2sl_nc(long *restrict n, char *s,
    char **restrict endp, int base, long min, long max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2sll_nc(long long *restrict n, char *s,
    char **restrict endp, int base, long long min, long long max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2uh_nc(unsigned short *restrict n, char *s,
    char **restrict endp, int base, unsigned short min, unsigned short max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2ui_nc(unsigned int *restrict n, char *s,
    char **restrict endp, int base, unsigned int min, unsigned int max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2ul_nc(unsigned long *restrict n, char *s,
    char **restrict endp, int base, unsigned long min, unsigned long max);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1) ATTR_ACCESS(write_only, 3)
inline int a2ull_nc(unsigned long long *restrict n, char *s,
    char **restrict endp, int base, unsigned long long min,
    unsigned long long max);


inline int
a2sh_c(short *restrict n, const char *s,
    const char **restrict endp, int base, short min, short max)
{
	return a2sh(n, (char *) s, (char **) endp, base, min, max);
}


inline int
a2si_c(int *restrict n, const char *s,
    const char **restrict endp, int base, int min, int max)
{
	return a2si(n, (char *) s, (char **) endp, base, min, max);
}


inline int
a2sl_c(long *restrict n, const char *s,
    const char **restrict endp, int base, long min, long max)
{
	return a2sl(n, (char *) s, (char **) endp, base, min, max);
}


inline int
a2sll_c(long long *restrict n, const char *s,
    const char **restrict endp, int base, long long min, long long max)
{
	return a2sll(n, (char *) s, (char **) endp, base, min, max);
}


inline int
a2uh_c(unsigned short *restrict n, const char *s,
    const char **restrict endp, int base, unsigned short min,
    unsigned short max)
{
	return a2uh(n, (char *) s, (char **) endp, base, min, max);
}


inline int
a2ui_c(unsigned int *restrict n, const char *s,
    const char **restrict endp, int base, unsigned int min, unsigned int max)
{
	return a2ui(n, (char *) s, (char **) endp, base, min, max);
}


inline int
a2ul_c(unsigned long *restrict n, const char *s,
    const char **restrict endp, int base, unsigned long min, unsigned long max)
{
	return a2ul(n, (char *) s, (char **) endp, base, min, max);
}


inline int
a2ull_c(unsigned long long *restrict n, const char *s,
    const char **restrict endp, int base, unsigned long long min,
    unsigned long long max)
{
	return a2ull(n, (char *) s, (char **) endp, base, min, max);
}


inline int
a2sh_nc(short *restrict n, char *s,
    char **restrict endp, int base, short min, short max)
{
	int  status;

	*n = strtoi_(s, endp, base, min, max, &status);
	if (status != 0) {
		errno = status;
		return -1;
	}
	return 0;
}


inline int
a2si_nc(int *restrict n, char *s,
    char **restrict endp, int base, int min, int max)
{
	int  status;

	*n = strtoi_(s, endp, base, min, max, &status);
	if (status != 0) {
		errno = status;
		return -1;
	}
	return 0;
}


inline int
a2sl_nc(long *restrict n, char *s,
    char **restrict endp, int base, long min, long max)
{
	int  status;

	*n = strtoi_(s, endp, base, min, max, &status);
	if (status != 0) {
		errno = status;
		return -1;
	}
	return 0;
}


inline int
a2sll_nc(long long *restrict n, char *s,
    char **restrict endp, int base, long long min, long long max)
{
	int  status;

	*n = strtoi_(s, endp, base, min, max, &status);
	if (status != 0) {
		errno = status;
		return -1;
	}
	return 0;
}


inline int
a2uh_nc(unsigned short *restrict n, char *s,
    char **restrict endp, int base, unsigned short min,
    unsigned short max)
{
	int  status;

	*n = strtou_noneg(s, endp, base, min, max, &status);
	if (status != 0) {
		errno = status;
		return -1;
	}
	return 0;
}


inline int
a2ui_nc(unsigned int *restrict n, char *s,
    char **restrict endp, int base, unsigned int min, unsigned int max)
{
	int  status;

	*n = strtou_noneg(s, endp, base, min, max, &status);
	if (status != 0) {
		errno = status;
		return -1;
	}
	return 0;
}


inline int
a2ul_nc(unsigned long *restrict n, char *s,
    char **restrict endp, int base, unsigned long min, unsigned long max)
{
	int  status;

	*n = strtou_noneg(s, endp, base, min, max, &status);
	if (status != 0) {
		errno = status;
		return -1;
	}
	return 0;
}


inline int
a2ull_nc(unsigned long long *restrict n, char *s,
    char **restrict endp, int base, unsigned long long min,
    unsigned long long max)
{
	int  status;

	*n = strtou_noneg(s, endp, base, min, max, &status);
	if (status != 0) {
		errno = status;
		return -1;
	}
	return 0;
}


#endif  // include guard
