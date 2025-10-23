/*
 * Copyright (c) 2020 Dmitry V. Levin <ldv@altlinux.org>
 *
 * Handy inline functions and macros providing some convenient functionality
 * to libpam and its modules.
 */

#ifndef PAM_INLINE_H
#define PAM_INLINE_H

#include "pam_cc_compat.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/*
 * Evaluates to
 * - a syntax error if the argument is 0,
 * 0, otherwise.
 */
#define PAM_FAIL_BUILD_ON_ZERO(e_)	(sizeof(int[-1 + 2 * !!(e_)]) * 0)

/*
 * Evaluates to
 * 1, if the given type is known to be a non-array type
 * 0, otherwise.
 */
#define PAM_IS_NOT_ARRAY(a_)		PAM_IS_SAME_TYPE((a_), &(a_)[0])

/*
 * Evaluates to
 * - a syntax error if the argument is not an array,
 * 0, otherwise.
 */
#define PAM_MUST_BE_ARRAY(a_)		PAM_FAIL_BUILD_ON_ZERO(!PAM_IS_NOT_ARRAY(a_))
/*
 * Evaluates to
 * - a syntax error if the argument is an array,
 * 0, otherwise.
 */
#define PAM_MUST_NOT_BE_ARRAY(a_)	PAM_FAIL_BUILD_ON_ZERO(PAM_IS_NOT_ARRAY(a_))

/* Evaluates to the number of elements in the specified array.  */
#define PAM_ARRAY_SIZE(a_)		(sizeof(a_) / sizeof((a_)[0]) + PAM_MUST_BE_ARRAY(a_))

/*
 * Zero-extend a signed integer type to unsigned long long.
 */
# define zero_extend_signed_to_ull(v_) \
	(sizeof(v_) == sizeof(char) ? (unsigned long long) (unsigned char) (v_) : \
	 sizeof(v_) == sizeof(short) ? (unsigned long long) (unsigned short) (v_) : \
	 sizeof(v_) == sizeof(int) ? (unsigned long long) (unsigned int) (v_) : \
	 sizeof(v_) == sizeof(long) ? (unsigned long long) (unsigned long) (v_) : \
	 (unsigned long long) (v_))

/*
 * Sign-extend an unsigned integer type to long long.
 */
# define sign_extend_unsigned_to_ll(v_) \
	(sizeof(v_) == sizeof(char) ? (long long) (signed char) (v_) : \
	 sizeof(v_) == sizeof(short) ? (long long) (signed short) (v_) : \
	 sizeof(v_) == sizeof(int) ? (long long) (signed int) (v_) : \
	 sizeof(v_) == sizeof(long) ? (long long) (signed long) (v_) : \
	 (long long) (v_))

/*
 * Returns NULL if STR does not start with PREFIX,
 * or a pointer to the first char in STR after PREFIX.
 * The length of PREFIX is specified by PREFIX_LEN.
 */
static inline const char *
pam_str_skip_prefix_len(const char *str, const char *prefix, size_t prefix_len)
{
	return strncmp(str, prefix, prefix_len) ? NULL : str + prefix_len;
}

#define pam_str_skip_prefix(str_, prefix_)	\
	pam_str_skip_prefix_len((str_), (prefix_), sizeof(prefix_) - 1 + PAM_MUST_BE_ARRAY(prefix_))

/*
 * Returns NULL if STR does not start with PREFIX
 * (ignoring the case of the characters),
 * or a pointer to the first char in STR after PREFIX.
 * The length of PREFIX is specified by PREFIX_LEN.
 */
static inline const char *
pam_str_skip_icase_prefix_len(const char *str, const char *prefix, size_t prefix_len)
{
	return strncasecmp(str, prefix, prefix_len) ? NULL : str + prefix_len;
}

#define pam_str_skip_icase_prefix(str_, prefix_)	\
	pam_str_skip_icase_prefix_len((str_), (prefix_), sizeof(prefix_) - 1 + PAM_MUST_BE_ARRAY(prefix_))


/*
 * Macros to securely erase memory
 */

#ifdef HAVE_MEMSET_EXPLICIT
static inline void pam_overwrite_n(void *ptr, size_t len)
{
	if (ptr)
		memset_explicit(ptr, '\0', len);
}
#elif defined HAVE_EXPLICIT_BZERO
static inline void pam_overwrite_n(void *ptr, size_t len)
{
	if (ptr)
		explicit_bzero(ptr, len);
}
#else
static inline void pam_overwrite_n(void *ptr, size_t len)
{
	if (ptr) {
		ptr = memset(ptr, '\0', len);
		__asm__ __volatile__ ("" : : "r"(ptr) : "memory");
	}
}
#endif

#define pam_overwrite_string(x)                      \
do {                                                 \
	char *xx__ = (x) + PAM_MUST_NOT_BE_ARRAY(x); \
	if (xx__)                                    \
		pam_overwrite_n(xx__, strlen(xx__)); \
} while(0)

#define pam_overwrite_array(x) pam_overwrite_n(x, sizeof(x) + PAM_MUST_BE_ARRAY(x))

#define pam_overwrite_object(x) pam_overwrite_n(x, sizeof(*(x)) + PAM_MUST_NOT_BE_ARRAY(x))

static inline void
pam_drop_response(struct pam_response *reply, int replies)
{
	int reply_i;

	for (reply_i = 0; reply_i < replies; ++reply_i) {
		if (reply[reply_i].resp) {
			pam_overwrite_string(reply[reply_i].resp);
			free(reply[reply_i].resp);
		}
	}
	free(reply);
}

static inline char * PAM_FORMAT((printf, 1, 2)) PAM_NONNULL((1)) PAM_ATTRIBUTE_MALLOC
pam_asprintf(const char *fmt, ...)
{
	int rc;
	char *res;
	va_list ap;

	va_start(ap, fmt);
	rc = vasprintf(&res, fmt, ap);
	va_end(ap);

	return rc < 0 ? NULL : res;
}

static inline int PAM_FORMAT((printf, 3, 4)) PAM_NONNULL((3))
pam_snprintf(char *str, size_t size, const char *fmt, ...)
{
	int rc;
	va_list ap;

	va_start(ap, fmt);
	rc = vsnprintf(str, size, fmt, ap);
	va_end(ap);

	if (rc < 0 || (unsigned int) rc >= size)
		return -1;
	return rc;
}

#define pam_sprintf(str_, fmt_, ...)						\
	pam_snprintf((str_), sizeof(str_) + PAM_MUST_BE_ARRAY(str_), (fmt_),	\
		     ##__VA_ARGS__)


static inline int
pam_read_passwords(int fd, int npass, char **passwords)
{
	/*
	 * The passwords array must contain npass preallocated
	 * buffers of length PAM_MAX_RESP_SIZE + 1.
	 */
	int rbytes = 0;
	int offset = 0;
	int i = 0;
	char *pptr;
	while (npass > 0) {
		rbytes = read(fd, passwords[i]+offset, PAM_MAX_RESP_SIZE+1-offset);

		if (rbytes < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		if (rbytes == 0) {
			break;
		}

		while (npass > 0 &&
		       (pptr = memchr(passwords[i] + offset, '\0', rbytes)) != NULL) {
			++pptr; /* skip the '\0' */
			rbytes -= pptr - (passwords[i] + offset);
			i++;
			offset = 0;
			npass--;
			if (rbytes > 0) {
				if (npass > 0) {
					memcpy(passwords[i], pptr, rbytes);
				}
				pam_overwrite_n(pptr, rbytes);
			}
		}
		offset += rbytes;
	}

	/* clear up */
	if (offset > 0 && npass > 0) {
		pam_overwrite_n(passwords[i], offset);
	}

	return i;
}

static inline int
pam_consttime_streq(const char *userinput, const char *secret) {
	volatile const char *u = userinput, *s = secret;
	volatile int ret = 0;

	do {
		ret |= *u ^ *s;

		s += !!*s;
	} while (*u++ != '\0');

	return ret == 0;
}

#endif /* PAM_INLINE_H */
