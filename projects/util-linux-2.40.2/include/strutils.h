/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_STRUTILS
#define UTIL_LINUX_STRUTILS

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "c.h"

/* initialize a custom exit code for all *_or_err functions */
extern void strutils_set_exitcode(int exit_code);

extern int parse_size(const char *str, uintmax_t *res, int *power);
extern int strtosize(const char *str, uintmax_t *res);
extern uintmax_t strtosize_or_err(const char *str, const char *errmesg);

extern int ul_strtos64(const char *str, int64_t *num, int base);
extern int ul_strtou64(const char *str, uint64_t *num, int base);
extern int ul_strtos32(const char *str, int32_t *num, int base);
extern int ul_strtou32(const char *str, uint32_t *num, int base);
extern int ul_strtold(const char *str, long double *num);

extern int64_t str2num_or_err(const char *str, int base, const char *errmesg, int64_t low, int64_t up);
extern uint64_t str2unum_or_err(const char *str, int base, const char *errmesg, uint64_t up);

#define strtos64_or_err(_s, _e)	str2num_or_err(_s, 10, _e, 0, 0)
#define strtou64_or_err(_s, _e)	str2unum_or_err(_s, 10, _e, 0)
#define strtox64_or_err(_s, _e)	str2unum_or_err(_s, 16, _e, 0)

#define strtos32_or_err(_s, _e)	(int32_t) str2num_or_err(_s, 10, _e, INT32_MIN, INT32_MAX)
#define strtou32_or_err(_s, _e)	(uint32_t) str2unum_or_err(_s, 10, _e, UINT32_MAX)
#define strtox32_or_err(_s, _e)	(uint32_t) str2unum_or_err(_s, 16, _e, UINT32_MAX)

#define strtos16_or_err(_s, _e)	(int16_t) str2num_or_err(_s, 10, _e, INT16_MIN, INT16_MAX)
#define strtou16_or_err(_s, _e)	(uint16_t) str2unum_or_err(_s, 10, _e, UINT16_MAX)
#define strtox16_or_err(_s, _e)	(uint16_t) str2unum_or_err(_s, 16, _e, UINT16_MAX)

extern double strtod_or_err(const char *str, const char *errmesg);
extern long double strtold_or_err(const char *str, const char *errmesg);

#define strtol_or_err(_s, _e)	(long) str2num_or_err(_s, 10, _e, LONG_MIN, LONG_MAX)
#define strtopid_or_err(_s, _e)	(pid_t) str2num_or_err(_s, 10, _e, 1, SINT_MAX(pid_t))
#define strtoul_or_err(_s, _e)	(unsigned long) str2unum_or_err(_s, 10, _e, ULONG_MAX)

extern void strtotimeval_or_err(const char *str, struct timeval *tv,
		const char *errmesg);
extern void strtotimespec_or_err(const char *str, struct timespec *ts,
		const char *errmesg);
extern time_t strtotime_or_err(const char *str, const char *errmesg);

extern int isdigit_strend(const char *str, const char **end);
#define isdigit_string(_s)	isdigit_strend(_s, NULL)

extern int isxdigit_strend(const char *str, const char **end);
#define isxdigit_string(_s)	isxdigit_strend(_s, NULL)


extern int parse_switch(const char *arg, const char *errmesg, ...);

#ifndef HAVE_MEMPCPY
extern void *mempcpy(void *restrict dest, const void *restrict src, size_t n);
#endif
#ifndef HAVE_STRNLEN
extern size_t strnlen(const char *s, size_t maxlen);
#endif
#ifndef HAVE_STRNDUP
extern char *strndup(const char *s, size_t n);
#endif
#ifndef HAVE_STRNCHR
extern char *strnchr(const char *s, size_t maxlen, int c);
#endif

/* caller guarantees n > 0 */
static inline void xstrncpy(char *dest, const char *src, size_t n)
{
	size_t len = src ? strlen(src) : 0;

	if (!len)
		return;
	len = min(len, n - 1);
	memcpy(dest, src, len);
	dest[len] = 0;
}

/* This is like strncpy(), but based on memcpy(), so compilers and static
 * analyzers do not complain when sizeof(destination) is the same as 'n' and
 * result is not terminated by zero.
 *
 * Use this function to copy string to logs with fixed sizes (wtmp/utmp. ...)
 * where string terminator is optional.
 */
static inline void * __attribute__((nonnull (1)))
str2memcpy(void *dest, const char *src, size_t n)
{
	size_t bytes = strlen(src) + 1;

	if (bytes > n)
		bytes = n;

	memcpy(dest, src, bytes);
	return dest;
}

static inline char * __attribute__((nonnull (1)))
mem2strcpy(char *dest, const void *src, size_t n, size_t nmax)
{
	if (n + 1 > nmax)
		n = nmax - 1;

	memset(dest, '\0', nmax);
	memcpy(dest, src, n);
	return dest;
}

/* Reallocate @str according to @newstr and copy @newstr to @str; returns new @str.
 * The @str is not modified if reallocation failed (like classic realloc()).
 */
static inline char * __attribute__((warn_unused_result))
strrealloc(char *str, const char *newstr)
{
	size_t nsz, osz;

	if (!str)
		return newstr ? strdup(newstr) : NULL;
	if (!newstr)
		return NULL;

	osz = strlen(str);
	nsz = strlen(newstr);

	if (nsz > osz)
		str = realloc(str, nsz + 1);
	if (str)
		memcpy(str, newstr, nsz + 1);
	return str;
}

/* Copy string @str to struct @stru to member addressed by @offset */
static inline int strdup_to_offset(void *stru, size_t offset, const char *str)
{
	char **o;
	char *p = NULL;

	if (!stru)
		return -EINVAL;

	o = (char **) ((char *) stru + offset);
	if (str) {
		p = strdup(str);
		if (!p)
			return -ENOMEM;
	}

	free(*o);
	*o = p;
	return 0;
}

/* Copy string __str to struct member _m of the struct _s */
#define strdup_to_struct_member(_s, _m, _str) \
		strdup_to_offset((void *) _s, offsetof(__typeof__(*(_s)), _m), _str)

/* Copy string addressed by @offset between two structs */
static inline int strdup_between_offsets(void *stru_dst, void *stru_src, size_t offset)
{
	char **src;
	char **dst;
	char *p = NULL;

	if (!stru_src || !stru_dst)
		return -EINVAL;

	src = (char **) ((char *) stru_src + offset);
	dst = (char **) ((char *) stru_dst + offset);

	if (*src) {
		p = strdup(*src);
		if (!p)
			return -ENOMEM;
	}

	free(*dst);
	*dst = p;
	return 0;
}

/* Copy string addressed by struct member between two instances of the same
 * struct type */
#define strdup_between_structs(_dst, _src, _m) \
		strdup_between_offsets((void *)_dst, (void *)_src, offsetof(__typeof__(*(_src)), _m))


extern char *xstrmode(mode_t mode, char *str);

/* Options for size_to_human_string() */
enum
{
	SIZE_SUFFIX_1LETTER  = 0,
	SIZE_SUFFIX_3LETTER  = (1 << 0),
	SIZE_SUFFIX_SPACE    = (1 << 1),
	SIZE_DECIMAL_2DIGITS = (1 << 2)
};

extern char *size_to_human_string(int options, uint64_t bytes);

extern int string_to_idarray(const char *list, int ary[], size_t arysz,
			   int (name2id)(const char *, size_t));
extern int string_add_to_idarray(const char *list, int ary[],
				 size_t arysz, size_t *ary_pos,
				 int (name2id)(const char *, size_t));

extern int string_to_bitarray(const char *list, char *ary,
			    int (*name2bit)(const char *, size_t),
			    size_t allow_range);

extern int string_to_bitmask(const char *list,
			     unsigned long *mask,
			     long (*name2flag)(const char *, size_t));
extern int parse_range(const char *str, int *lower, int *upper, int def);

extern int streq_paths(const char *a, const char *b);

/*
 * Match string beginning.
 */
static inline const char *startswith(const char *s, const char *prefix)
{
	size_t sz = prefix ? strlen(prefix) : 0;

        if (s && sz && strncmp(s, prefix, sz) == 0)
                return s + sz;
	return NULL;
}

/*
 * Case insensitive match string beginning.
 */
static inline const char *startswith_no_case(const char *s, const char *prefix)
{
	size_t sz = prefix ? strlen(prefix) : 0;

        if (s && sz && strncasecmp(s, prefix, sz) == 0)
                return s + sz;
	return NULL;
}

/*
 * Match string ending.
 */
static inline const char *endswith(const char *s, const char *postfix)
{
	size_t sl = s ? strlen(s) : 0;
	size_t pl = postfix ? strlen(postfix) : 0;

	if (pl == 0)
		return s + sl;
	if (sl < pl)
		return NULL;
	if (memcmp(s + sl - pl, postfix, pl) != 0)
		return NULL;
	return s + sl - pl;
}

/*
 * Skip leading white space.
 */
static inline const char *skip_space(const char *p)
{
	while (isspace(*p))
		++p;
	return p;
}

static inline const char *skip_blank(const char *p)
{
	while (isblank(*p))
		++p;
	return p;
}


/* Removes whitespace from the right-hand side of a string (trailing
 * whitespace).
 *
 * Returns size of the new string (without \0).
 */
static inline size_t rtrim_whitespace(unsigned char *str)
{
	size_t i;

	if (!str)
		return 0;
	i = strlen((char *) str);
	while (i) {
		i--;
		if (!isspace(str[i])) {
			i++;
			break;
		}
	}
	str[i] = '\0';
	return i;
}

/* Removes whitespace from the left-hand side of a string.
 *
 * Returns size of the new string (without \0).
 */
static inline size_t ltrim_whitespace(unsigned char *str)
{
	size_t len;
	unsigned char *p;

	if (!str)
		return 0;
	for (p = str; *p && isspace(*p); p++);

	len = strlen((char *) p);

	if (p > str)
		memmove(str, p, len + 1);

	return len;
}

/* Removes left-hand, right-hand and repeating whitespaces.
 */
static inline size_t __normalize_whitespace(
				const unsigned char *src,
				size_t sz,
				unsigned char *dst,
				size_t len)
{
	size_t i, x = 0;
	int nsp = 0, intext = 0;

	if (!sz)
		goto done;

	for (i = 0, x = 0; i < sz && x < len - 1;  ) {
		if (isspace(src[i]))
			nsp++;
		else
			nsp = 0, intext = 1;

		if (nsp > 1 || (nsp && !intext))
			i++;
		else
			dst[x++] = src[i++];
	}
	if (nsp && x > 0)		/* tailing space */
		x--;
done:
	dst[x] = '\0';
	return x;
}

static inline size_t normalize_whitespace(unsigned char *str)
{
	size_t sz = strlen((char *) str);
	return __normalize_whitespace(str, sz, str, sz + 1);
}

static inline void strrep(char *s, int find, int replace)
{
	while (s && *s && (s = strchr(s, find)) != NULL)
		*s++ = replace;
}

static inline void strrem(char *s, int rem)
{
	char *p;

	if (!s)
		return;
	for (p = s; *s; s++) {
		if (*s != rem)
			*p++ = *s;
	}
	*p = '\0';
}

/* returns next string after \0 if before @end */
static inline char *ul_next_string(char *p, char *end)
{
	char *last;

	if (!p || !end || p >= end)
		return NULL;

	for (last = p; p < end; p++) {
		if (*last == '\0' && p != last)
			return p;
		last = p;
	}

	return NULL;
}

extern char *strnconcat(const char *s, const char *suffix, size_t b);
extern char *strconcat(const char *s, const char *suffix);
extern char *strfconcat(const char *s, const char *format, ...)
		 __attribute__ ((__format__ (__printf__, 2, 3)));

extern int strappend(char **a, const char *b);
extern int strfappend(char **a, const char *format, ...)
		 __attribute__ ((__format__ (__printf__, 2, 3)));
extern int strvfappend(char **a, const char *format, va_list ap)
		 __attribute__ ((__format__ (__printf__, 2, 0)));

extern const char *split(const char **state, size_t *l, const char *separator, int quoted);

extern char *ul_strchr_escaped(const char *s, int c);

extern int skip_fline(FILE *fp);
extern int ul_stralnumcmp(const char *p1, const char *p2);

extern int ul_optstr_next(char **optstr, char **name, size_t *namesz, char **value, size_t *valsz);

#endif
