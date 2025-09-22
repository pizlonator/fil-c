// SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#ifndef SHADOW_INCLUDE_LIB_ATOI_STRTOI_H_
#define SHADOW_INCLUDE_LIB_ATOI_STRTOI_H_


#include <config.h>

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/param.h>

#include "attr.h"


#define strtoNmax(TYPE, ...)                                                  \
(                                                                             \
	_Generic((TYPE) 0,                                                    \
		intmax_t:  strtoimax,                                         \
		uintmax_t: strtoumax                                          \
	)(__VA_ARGS__)                                                        \
)


#define strtoN(s, endp, base, min, max, status, TYPE)                         \
({                                                                            \
	const char  *s_ = s;                                                  \
	char        **endp_ = endp;                                           \
	int         base_ = base;                                             \
	TYPE        min_ = min;                                               \
	TYPE        max_ = max;                                               \
	int         *status_ = status;                                        \
                                                                              \
	int         e_, st_;                                                  \
	char        *end_;                                                    \
	TYPE        n_;                                                       \
                                                                              \
	if (endp_ == NULL)                                                    \
		endp_ = &end_;                                                \
	if (status_ == NULL)                                                  \
		status_ = &st_;                                               \
                                                                              \
	if (base_ != 0 && (base_ < 0 || base_ > 36)) {                        \
		*status_ = EINVAL;                                            \
		n_ = 0;                                                       \
                                                                              \
	} else {                                                              \
		e_ = errno;                                                   \
		errno = 0;                                                    \
		n_ = strtoNmax(TYPE, s_, endp_, base_);                       \
                                                                              \
		if (*endp_ == s_)                                             \
			*status_ = ECANCELED;                                 \
		else if (errno == ERANGE || n_ < min_ || n_ > max_)           \
			*status_ = ERANGE;                                    \
		else if (**endp_ != '\0')                                     \
			*status_ = ENOTSUP;                                   \
		else                                                          \
			*status_ = 0;                                         \
                                                                              \
		errno = e_;                                                   \
	}                                                                     \
	MAX(min_, MIN(max_, n_));                                             \
})


ATTR_STRING(1) ATTR_ACCESS(write_only, 2) ATTR_ACCESS(write_only, 6)
inline intmax_t strtoi_(const char *s, char **restrict endp, int base,
    intmax_t min, intmax_t max, int *restrict status);
ATTR_STRING(1) ATTR_ACCESS(write_only, 2) ATTR_ACCESS(write_only, 6)
inline uintmax_t strtou_(const char *s, char **restrict endp, int base,
    uintmax_t min, uintmax_t max, int *restrict status);


inline intmax_t
strtoi_(const char *s, char **restrict endp, int base,
    intmax_t min, intmax_t max, int *restrict status)
{
	return strtoN(s, endp, base, min, max, status, intmax_t);
}


inline uintmax_t
strtou_(const char *s, char **restrict endp, int base,
    uintmax_t min, uintmax_t max, int *restrict status)
{
	return strtoN(s, endp, base, min, max, status, uintmax_t);
}


#endif  // include guard
