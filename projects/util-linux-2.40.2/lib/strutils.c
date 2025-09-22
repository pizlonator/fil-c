/*
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2010 Davidlohr Bueso <dave@gnu.org>
 *
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include "c.h"
#include "nls.h"
#include "strutils.h"
#include "bitops.h"
#include "pathnames.h"

static int STRTOXX_EXIT_CODE = EXIT_FAILURE;

void strutils_set_exitcode(int ex) {
	STRTOXX_EXIT_CODE = ex;
}

static int do_scale_by_power (uintmax_t *x, int base, int power)
{
	while (power--) {
		if (UINTMAX_MAX / base < *x)
			return -ERANGE;
		*x *= base;
	}
	return 0;
}

/*
 * strtosize() - convert string to size (uintmax_t).
 *
 * Supported suffixes:
 *
 * XiB or X for 2^N
 *     where X = {K,M,G,T,P,E,Z,Y}
 *        or X = {k,m,g,t,p,e}  (undocumented for backward compatibility only)
 * for example:
 *		10KiB	= 10240
 *		10K	= 10240
 *
 * XB for 10^N
 *     where X = {K,M,G,T,P,E,Z,Y}
 * for example:
 *		10KB	= 10000
 *
 * The optional 'power' variable returns number associated with used suffix
 * {K,M,G,T,P,E,Z,Y}  = {1,2,3,4,5,6,7,8}.
 *
 * The function also supports decimal point, for example:
 *              0.5MB   = 500000
 *              0.5MiB  = 512000
 *
 * Note that the function does not accept numbers with '-' (negative sign)
 * prefix.
 */
int parse_size(const char *str, uintmax_t *res, int *power)
{
	const char *p;
	char *end;
	uintmax_t x, frac = 0;
	int base = 1024, rc = 0, pwr = 0, frac_zeros = 0;

	static const char *suf  = "KMGTPEZY";
	static const char *suf2 = "kmgtpezy";
	const char *sp;

	*res = 0;

	if (!str || !*str) {
		rc = -EINVAL;
		goto err;
	}

	/* Only positive numbers are acceptable
	 *
	 * Note that this check is not perfect, it would be better to
	 * use lconv->negative_sign. But coreutils use the same solution,
	 * so it's probably good enough...
	 */
	p = str;
	while (isspace((unsigned char) *p))
		p++;
	if (*p == '-') {
		rc = -EINVAL;
		goto err;
	}

	errno = 0, end = NULL;
	x = strtoumax(str, &end, 0);

	if (end == str ||
	    (errno != 0 && (x == UINTMAX_MAX || x == 0))) {
		rc = errno ? -errno : -EINVAL;
		goto err;
	}
	if (!end || !*end)
		goto done;			/* without suffix */
	p = end;

	/*
	 * Check size suffixes
	 */
check_suffix:
	if (*(p + 1) == 'i' && (*(p + 2) == 'B' || *(p + 2) == 'b') && !*(p + 3))
		base = 1024;			/* XiB, 2^N */
	else if ((*(p + 1) == 'B' || *(p + 1) == 'b') && !*(p + 2))
		base = 1000;			/* XB, 10^N */
	else if (*(p + 1)) {
		struct lconv const *l = localeconv();
		const char *dp = l ? l->decimal_point : NULL;
		size_t dpsz = dp ? strlen(dp) : 0;

		if (frac == 0 && *p && dp && strncmp(dp, p, dpsz) == 0) {
			const char *fstr = p + dpsz;

			for (p = fstr; *p == '0'; p++)
				frac_zeros++;
			fstr = p;
			if (isdigit(*fstr)) {
				errno = 0, end = NULL;
				frac = strtoumax(fstr, &end, 0);
				if (end == fstr ||
				    (errno != 0 && (frac == UINTMAX_MAX || frac == 0))) {
					rc = errno ? -errno : -EINVAL;
					goto err;
				}
			} else
				end = (char *) p;

			if (frac && (!end  || !*end)) {
				rc = -EINVAL;
				goto err;		/* without suffix, but with frac */
			}
			p = end;
			goto check_suffix;
		}
		rc = -EINVAL;
		goto err;			/* unexpected suffix */
	}

	sp = strchr(suf, *p);
	if (sp)
		pwr = (sp - suf) + 1;
	else {
		sp = strchr(suf2, *p);
		if (sp)
			pwr = (sp - suf2) + 1;
		else {
			rc = -EINVAL;
			goto err;
		}
	}

	rc = do_scale_by_power(&x, base, pwr);
	if (power)
		*power = pwr;
	if (frac && pwr) {
		int i;
		uintmax_t frac_div = 10, frac_poz = 1, frac_base = 1;

		/* mega, giga, ... */
		do_scale_by_power(&frac_base, base, pwr);

		/* maximal divisor for last digit (e.g. for 0.05 is
		 * frac_div=100, for 0.054 is frac_div=1000, etc.)
		 *
		 * Reduce frac if too large.
		 */
		while (frac_div < frac) {
			if (frac_div <= UINTMAX_MAX/10)
				frac_div *= 10;
			else
				frac /= 10;
		}

		/* 'frac' is without zeros (5 means 0.5 as well as 0.05) */
		for (i = 0; i < frac_zeros; i++) {
			if (frac_div <= UINTMAX_MAX/10)
				frac_div *= 10;
			else
				frac /= 10;
		}

		/*
		 * Go backwardly from last digit and add to result what the
		 * digit represents in the frac_base. For example 0.25G
		 *
		 *  5 means 1GiB / (100/5)
		 *  2 means 1GiB / (10/2)
		 */
		do {
			unsigned int seg = frac % 10;		 /* last digit of the frac */
			uintmax_t seg_div = frac_div / frac_poz; /* what represents the segment 1000, 100, .. */

			frac /= 10;	/* remove last digit from frac */
			frac_poz *= 10;

			if (seg && seg_div / seg)
				x += frac_base / (seg_div / seg);
		} while (frac);
	}
done:
	*res = x;
err:
	if (rc < 0)
		errno = -rc;
	return rc;
}

int strtosize(const char *str, uintmax_t *res)
{
	return parse_size(str, res, NULL);
}

int isdigit_strend(const char *str, const char **end)
{
	const char *p;

	for (p = str; p && *p && isdigit((unsigned char) *p); p++);

	if (end)
		*end = p;
	return p && p > str && !*p;
}

int isxdigit_strend(const char *str, const char **end)
{
	const char *p;

	for (p = str; p && *p && isxdigit((unsigned char) *p); p++);

	if (end)
		*end = p;

	return p && p > str && !*p;
}

/*
 *  parse_switch(argv[i], "on", "off",  "yes", "no",  NULL);
 */
int parse_switch(const char *arg, const char *errmesg, ...)
{
	const char *a, *b;
	va_list ap;

	va_start(ap, errmesg);
	do {
		a = va_arg(ap, char *);
		if (!a)
			break;
		b = va_arg(ap, char *);
		if (!b)
			break;

		if (strcmp(arg, a) == 0) {
			va_end(ap);
			return 1;
		}

		if (strcmp(arg, b) == 0) {
			va_end(ap);
			return 0;
		}
	} while (1);
	va_end(ap);

	errx(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, arg);
}

#ifndef HAVE_MEMPCPY
void *mempcpy(void *restrict dest, const void *restrict src, size_t n)
{
    return ((char *)memcpy(dest, src, n)) + n;
}
#endif

#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t maxlen)
{
        size_t i;

        for (i = 0; i < maxlen; i++) {
                if (s[i] == '\0')
                        return i;
        }
        return maxlen;
}
#endif

#ifndef HAVE_STRNCHR
char *strnchr(const char *s, size_t maxlen, int c)
{
	for (; maxlen-- && *s != '\0'; ++s)
		if (*s == (char)c)
			return (char *)s;
	return NULL;
}
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *new = malloc((len + 1) * sizeof(char));
	if (!new)
		return NULL;
	new[len] = '\0';
	return (char *) memcpy(new, s, len);
}
#endif

/*
 * convert strings to numbers; returns <0 on error, and 0 on success
 */
int ul_strtos64(const char *str, int64_t *num, int base)
{
	char *end = NULL;

	if (str == NULL || *str == '\0')
		return -(errno = EINVAL);

	errno = 0;
	*num = (int64_t) strtoimax(str, &end, base);

	if (errno != 0)
		return -errno;
	if (str == end || (end && *end))
		return -(errno = EINVAL);
	return 0;
}

int ul_strtou64(const char *str, uint64_t *num, int base)
{
	char *end = NULL;
	int64_t tmp;

	if (str == NULL || *str == '\0')
		return -(errno = EINVAL);

	/* we need to ignore negative numbers, note that for invalid negative
	 * number strtoimax() returns negative number too, so we do not
	 * need to check errno here */
	errno = 0;
	tmp = (int64_t) strtoimax(str, &end, base);
	if (tmp < 0)
		errno = ERANGE;
	else {
		errno = 0;
		*num = strtoumax(str, &end, base);
	}

	if (errno != 0)
		return -errno;
	if (str == end || (end && *end))
		return -(errno = EINVAL);
	return 0;
}

int ul_strtos32(const char *str, int32_t *num, int base)
{
	int64_t tmp;
	int rc;

	rc = ul_strtos64(str, &tmp, base);
	if (rc == 0 && (tmp < INT32_MIN || tmp > INT32_MAX))
		rc = -(errno = ERANGE);
	if (rc == 0)
		*num = (int32_t) tmp;
	return rc;
}

int ul_strtou32(const char *str, uint32_t *num, int base)
{
	uint64_t tmp;
	int rc;

	rc = ul_strtou64(str, &tmp, base);
	if (rc == 0 && tmp > UINT32_MAX)
		rc = -(errno = ERANGE);
	if (rc == 0)
		*num = (uint32_t) tmp;
	return rc;
}

/*
 * Convert strings to numbers in defined range and print message on error.
 *
 * These functions are used when we read input from users (getopt() etc.). It's
 * better to consolidate the code and keep it all based on 64-bit numbers than
 * implement it for 32 and 16-bit numbers too.
 */
int64_t str2num_or_err(const char *str, int base, const char *errmesg,
			     int64_t low, int64_t up)
{
	int64_t num = 0;
	int rc;

	rc = ul_strtos64(str, &num, base);
	if (rc == 0 && ((low && num < low) || (up && num > up)))
		rc = -(errno = ERANGE);

	if (rc) {
		if (errno == ERANGE)
			err(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
		errx(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
	}
	return num;
}

uint64_t str2unum_or_err(const char *str, int base, const char *errmesg, uint64_t up)
{
	uint64_t num = 0;
	int rc;

	rc = ul_strtou64(str, &num, base);
	if (rc == 0 && (up && num > up))
		rc = -(errno = ERANGE);

	if (rc) {
		if (errno == ERANGE)
			err(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
		errx(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
	}
	return num;
}

double strtod_or_err(const char *str, const char *errmesg)
{
	double num;
	char *end = NULL;

	errno = 0;
	if (str == NULL || *str == '\0')
		goto err;
	num = strtod(str, &end);

	if (errno || str == end || (end && *end))
		goto err;

	return num;
err:
	if (errno == ERANGE)
		err(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);

	errx(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
}

int ul_strtold(const char *str, long double *num)
{
	char *end = NULL;

	errno = 0;
	if (str == NULL || *str == '\0')
		return -(errno = EINVAL);
	*num = strtold(str, &end);

	if (errno != 0)
		return -errno;
	if (str == end || (end && *end))
		return -(errno = EINVAL);
	return 0;
}

long double strtold_or_err(const char *str, const char *errmesg)
{
	long double num = 0;

	if (ul_strtold(str, &num) == 0)
		return num;
	if (errno == ERANGE)
		err(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);

	errx(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
}

uintmax_t strtosize_or_err(const char *str, const char *errmesg)
{
	uintmax_t num;

	if (strtosize(str, &num) == 0)
		return num;

	if (errno)
		err(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);

	errx(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
}


void strtotimeval_or_err(const char *str, struct timeval *tv, const char *errmesg)
{
	long double user_input;

	user_input = strtold_or_err(str, errmesg);
	tv->tv_sec = (time_t) user_input;
	tv->tv_usec = (suseconds_t)((user_input - tv->tv_sec) * 1000000);
}

void strtotimespec_or_err(const char *str, struct timespec *ts, const char *errmesg)
{
	long double user_input;

	user_input = strtold_or_err(str, errmesg);
	ts->tv_sec = (time_t) user_input;
	ts->tv_nsec = (long)((user_input - ts->tv_sec) * 1000000000);
}

time_t strtotime_or_err(const char *str, const char *errmesg)
{
	int64_t user_input;

	user_input = strtos64_or_err(str, errmesg);
	return (time_t) user_input;
}

/*
 * Converts stat->st_mode to ls(1)-like mode string. The size of "str" must
 * be 11 bytes.
 */
char *xstrmode(mode_t mode, char *str)
{
	unsigned short i = 0;

	if (S_ISDIR(mode))
		str[i++] = 'd';
	else if (S_ISLNK(mode))
		str[i++] = 'l';
	else if (S_ISCHR(mode))
		str[i++] = 'c';
	else if (S_ISBLK(mode))
		str[i++] = 'b';
	else if (S_ISSOCK(mode))
		str[i++] = 's';
	else if (S_ISFIFO(mode))
		str[i++] = 'p';
	else if (S_ISREG(mode))
		str[i++] = '-';

	str[i++] = mode & S_IRUSR ? 'r' : '-';
	str[i++] = mode & S_IWUSR ? 'w' : '-';
	str[i++] = (mode & S_ISUID
		? (mode & S_IXUSR ? 's' : 'S')
		: (mode & S_IXUSR ? 'x' : '-'));
	str[i++] = mode & S_IRGRP ? 'r' : '-';
	str[i++] = mode & S_IWGRP ? 'w' : '-';
	str[i++] = (mode & S_ISGID
		? (mode & S_IXGRP ? 's' : 'S')
		: (mode & S_IXGRP ? 'x' : '-'));
	str[i++] = mode & S_IROTH ? 'r' : '-';
	str[i++] = mode & S_IWOTH ? 'w' : '-';
	str[i++] = (mode & S_ISVTX
		? (mode & S_IXOTH ? 't' : 'T')
		: (mode & S_IXOTH ? 'x' : '-'));
	str[i] = '\0';

	return str;
}

/*
 * returns exponent (2^x=n) in range KiB..EiB (2^10..2^60)
 */
static int get_exp(uint64_t n)
{
	int shft;

	for (shft = 10; shft <= 60; shft += 10) {
		if (n < (1ULL << shft))
			break;
	}
	return shft - 10;
}

char *size_to_human_string(int options, uint64_t bytes)
{
	char buf[32];
	int dec, exp;
	uint64_t frac;
	const char *letters = "BKMGTPE";
	char suffix[sizeof(" KiB")], *psuf = suffix;
	char c;

	if (options & SIZE_SUFFIX_SPACE)
		*psuf++ = ' ';


	exp  = get_exp(bytes);
	c    = *(letters + (exp ? exp / 10 : 0));
	dec  = exp ? bytes / (1ULL << exp) : bytes;
	frac = exp ? bytes % (1ULL << exp) : 0;

	*psuf++ = c;

	if ((options & SIZE_SUFFIX_3LETTER) && (c != 'B')) {
		*psuf++ = 'i';
		*psuf++ = 'B';
	}

	*psuf = '\0';

	/* fprintf(stderr, "exp: %d, unit: %c, dec: %d, frac: %jd\n",
	 *                 exp, suffix[0], dec, frac);
	 */

	/* round */
	if (frac) {
		/* get 3 digits after decimal point */
		if (frac >= UINT64_MAX / 1000)
			frac = ((frac / 1024) * 1000) / (1ULL << (exp - 10)) ;
		else
			frac = (frac * 1000) / (1ULL << (exp)) ;

		if (options & SIZE_DECIMAL_2DIGITS) {
			/* round 4/5 and keep 2 digits after decimal point */
			frac = (frac + 5) / 10 ;
		} else {
			/* round 4/5 and keep 1 digit after decimal point */
			frac = ((frac + 50) / 100) * 10 ;
		}

		/* rounding could have overflowed */
		if (frac == 100) {
			dec++;
			frac = 0;
		}
	}

	if (frac) {
		struct lconv const *l = localeconv();
		char *dp = l ? l->decimal_point : NULL;
		int len;

		if (!dp || !*dp)
			dp = ".";

		len = snprintf(buf, sizeof(buf), "%d%s%02" PRIu64, dec, dp, frac);
		if (len > 0 && (size_t) len < sizeof(buf)) {
			/* remove potential extraneous zero */
			if (buf[len - 1] == '0')
				buf[len--] = '\0';
			/* append suffix */
			xstrncpy(buf+len, suffix, sizeof(buf) - len);
		} else
			*buf = '\0';	/* snprintf error */
	} else
		snprintf(buf, sizeof(buf), "%d%s", dec, suffix);

	return strdup(buf);
}

/*
 * Parses comma delimited list to array with IDs, for example:
 *
 * "aaa,bbb,ccc" --> ary[0] = FOO_AAA;
 *                   ary[1] = FOO_BBB;
 *                   ary[3] = FOO_CCC;
 *
 * The function name2id() provides conversion from string to ID.
 *
 * Returns: >= 0  : number of items added to ary[]
 *            -1  : parse error or unknown item
 *            -2  : arysz reached
 */
int string_to_idarray(const char *list, int ary[], size_t arysz,
			int (name2id)(const char *, size_t))
{
	const char *begin = NULL, *p;
	size_t n = 0;

	if (!list || !*list || !ary || !arysz || !name2id)
		return -1;

	for (p = list; p && *p; p++) {
		const char *end = NULL;
		int id;

		if (n >= arysz)
			return -2;
		if (!begin)
			begin = p;		/* begin of the column name */
		if (*p == ',')
			end = p;		/* terminate the name */
		if (*(p + 1) == '\0')
			end = p + 1;		/* end of string */
		if (!begin || !end)
			continue;
		if (end <= begin)
			return -1;

		id = name2id(begin, end - begin);
		if (id == -1)
			return -1;
		ary[ n++ ] = id;
		begin = NULL;
		if (end && !*end)
			break;
	}
	return n;
}

/*
 * Parses the array like string_to_idarray but if format is "+aaa,bbb"
 * it adds fields to array instead of replacing them.
 */
int string_add_to_idarray(const char *list, int ary[], size_t arysz,
			size_t *ary_pos, int (name2id)(const char *, size_t))
{
	const char *list_add;
	int r;

	if (!list || !*list || !ary_pos || *ary_pos > arysz)
		return -1;

	if (list[0] == '+')
		list_add = &list[1];
	else {
		list_add = list;
		*ary_pos = 0;
	}

	r = string_to_idarray(list_add, &ary[*ary_pos], arysz - *ary_pos, name2id);
	if (r > 0)
		*ary_pos += r;
	return r;
}

/*
 * LIST ::= <item> [, <item>]
 *
 * The <item> is translated to 'id' by name2id() function and the 'id' is used
 * as a position in the 'ary' bit array. It means that the 'id' has to be in
 * range <0..N> where N < sizeof(ary) * NBBY.
 *
 * If allow_range is enabled:
 * An item ending in '+' also sets all bits in <0..N>.
 * An item beginning with '+' also sets all bits in <N..allow_minus>.
 *
 * Returns: 0 on success, <0 on error.
 */
int string_to_bitarray(const char *list,
		     char *ary,
		     int (*name2bit)(const char *, size_t),
		     size_t allow_range)
{
	const char *begin = NULL, *p;

	if (!list || !name2bit || !ary)
		return -EINVAL;

	for (p = list; p && *p; p++) {
		const char *end = NULL;
		int bit, set_lower = 0, set_higher = 0;

		if (!begin)
			begin = p;		/* begin of the level name */
		if (*p == ',')
			end = p;		/* terminate the name */
		if (*(p + 1) == '\0')
			end = p + 1;		/* end of string */
		if (!begin || !end)
			continue;
		if (end <= begin)
			return -1;
		if (allow_range) {
			if (*(end - 1) == '+') {
				end--;
				set_lower = 1;
			} else if (*begin == '+') {
				begin++;
				set_higher = 1;
			}
		}

		bit = name2bit(begin, end - begin);
		if (bit < 0)
			return bit;
		setbit(ary, bit);
		if (set_lower)
			while (--bit >= 0)
				setbit(ary, bit);
		else if (set_higher)
			while (++bit < (int) allow_range)
				setbit(ary, bit);
		begin = NULL;
		if (end && !*end)
			break;
	}
	return 0;
}

/*
 * LIST ::= <item> [, <item>]
 *
 * The <item> is translated to 'id' by name2flag() function and the flags is
 * set to the 'mask'
*
 * Returns: 0 on success, <0 on error.
 */
int string_to_bitmask(const char *list,
		     unsigned long *mask,
		     long (*name2flag)(const char *, size_t))
{
	const char *begin = NULL, *p;

	if (!list || !name2flag || !mask)
		return -EINVAL;

	for (p = list; p && *p; p++) {
		const char *end = NULL;
		long flag;

		if (!begin)
			begin = p;		/* begin of the level name */
		if (*p == ',')
			end = p;		/* terminate the name */
		if (*(p + 1) == '\0')
			end = p + 1;		/* end of string */
		if (!begin || !end)
			continue;
		if (end <= begin)
			return -1;

		flag = name2flag(begin, end - begin);
		if (flag < 0)
			return flag;	/* error */
		*mask |= flag;
		begin = NULL;
		if (end && !*end)
			break;
	}
	return 0;
}

/*
 * Parse the lower and higher values in a string containing
 * "lower:higher" or "lower-higher" format. Note that either
 * the lower or the higher values may be missing, and the def
 * value will be assigned to it by default.
 *
 * Returns: 0 on success, <0 on error.
 */
int parse_range(const char *str, int *lower, int *upper, int def)
{
	char *end = NULL;

	if (!str)
		return 0;

	*upper = *lower = def;
	errno = 0;

	if (*str == ':') {				/* <:N> */
		str++;
		*upper = strtol(str, &end, 10);
		if (errno || !end || *end || end == str)
			return -1;
	} else {
		*upper = *lower = strtol(str, &end, 10);
		if (errno || !end || end == str)
			return -1;

		if (*end == ':' && !*(end + 1))		/* <M:> */
			*upper = def;
		else if (*end == '-' || *end == ':') {	/* <M:N> <M-N> */
			str = end + 1;
			end = NULL;
			errno = 0;
			*upper = strtol(str, &end, 10);

			if (errno || !end || *end || end == str)
				return -1;
		}
	}
	return 0;
}

static const char *next_path_segment(const char *str, size_t *sz)
{
	const char *start, *p;

	start = str;
	*sz = 0;
	while (start && *start == '/' && *(start + 1) == '/')
		start++;

	if (!start || !*start)
		return NULL;

	for (*sz = 1, p = start + 1; *p && *p != '/'; p++) {
		(*sz)++;
	}

	return start;
}

int streq_paths(const char *a, const char *b)
{
	while (a && b) {
		size_t a_sz, b_sz;
		const char *a_seg = next_path_segment(a, &a_sz);
		const char *b_seg = next_path_segment(b, &b_sz);

		/*
		fprintf(stderr, "A>>>(%zu) '%s'\n", a_sz, a_seg);
		fprintf(stderr, "B>>>(%zu) '%s'\n", b_sz, b_seg);
		*/

		/* end of the path */
		if (a_sz + b_sz == 0)
			return 1;

		/* ignore tailing slash */
		if (a_sz + b_sz == 1 &&
		    ((a_seg && *a_seg == '/') || (b_seg && *b_seg == '/')))
			return 1;

		if (!a_seg || !b_seg)
			break;
		if (a_sz != b_sz || strncmp(a_seg, b_seg, a_sz) != 0)
			break;

		a = a_seg + a_sz;
		b = b_seg + b_sz;
	};

	return 0;
}

/* concatenate two strings to a new string, the size of the second string is limited by @b */
char *strnconcat(const char *s, const char *suffix, size_t b)
{
        size_t a;
        char *r;

        if (!s && !suffix)
                return strdup("");
        if (!s)
                return strndup(suffix, b);
        if (!suffix)
                return strdup(s);

        assert(s);
        assert(suffix);

        a = strlen(s);
        if (b > ((size_t) -1) - a)
                return NULL;

        r = malloc(a + b + 1);
        if (!r)
                return NULL;

        memcpy(r, s, a);
        memcpy(r + a, suffix, b);
        r[a+b] = 0;

        return r;
}

/* concatenate two strings to a new string */
char *strconcat(const char *s, const char *suffix)
{
        return strnconcat(s, suffix, suffix ? strlen(suffix) : 0);
}

/* concatenate @s and string defined by @format to a new string */
char *strfconcat(const char *s, const char *format, ...)
{
	va_list ap;
	char *val, *res;
	int sz;

	va_start(ap, format);
	sz = vasprintf(&val, format, ap);
	va_end(ap);

	if (sz < 0)
		return NULL;

	res = strnconcat(s, val, sz);
	free(val);
	return res;
}

int strappend(char **a, const char *b)
{
	size_t al, bl;
	char *tmp;

	if (!a)
		return -EINVAL;
	if (!b || !*b)
		return 0;
	if (!*a) {
		*a = strdup(b);
		return !*a ? -ENOMEM : 0;
	}

	al = strlen(*a);
	bl = strlen(b);

	tmp = realloc(*a, al + bl + 1);
	if (!tmp)
		return -ENOMEM;
	*a = tmp;
	memcpy((*a) + al, b, bl + 1);
	return 0;
}

/* the hybrid version of strfconcat and strappend. */
int strfappend(char **a, const char *format, ...)
{
	va_list ap;
	int res;

	va_start(ap, format);
	res = strvfappend(a, format, ap);
	va_end(ap);

	return res;
}

extern int strvfappend(char **a, const char *format, va_list ap)
{
	char *val;
	int sz;
	int res;

	sz = vasprintf(&val, format, ap);
	if (sz < 0)
		return -errno;

	res = strappend(a, val);
	free(val);
	return res;
}

static size_t strcspn_escaped(const char *s, const char *reject)
{
        int escaped = 0;
        int n;

        for (n=0; s[n]; n++) {
                if (escaped)
                        escaped = 0;
                else if (s[n] == '\\')
                        escaped = 1;
                else if (strchr(reject, s[n]))
                        break;
        }

        /* if s ends in \, return index of previous char */
        return n - escaped;
}

/*
 * Like strchr() but ignores @c if escaped by '\', '\\' is interpreted like '\'.
 *
 * For example for @c='X':
 *
 *      "abcdXefgXh"    --> "XefgXh"
 *	"abcd\XefgXh"   --> "Xh"
 *	"abcd\\XefgXh"  --> "XefgXh"
 *	"abcd\\\XefgXh" --> "Xh"
 *	"abcd\Xefg\Xh"  --> (null)
 *
 *	"abcd\\XefgXh"  --> "\XefgXh"   for @c='\\'
 */
char *ul_strchr_escaped(const char *s, int c)
{
	char *p;
	int esc = 0;

	for (p = (char *) s; p && *p; p++) {
		if (!esc && *p == '\\') {
			esc = 1;
			continue;
		}
		if (*p == c && (!esc || c == '\\'))
			return p;
		esc = 0;
	}

	return NULL;
}

/* Split a string into words. */
const char *split(const char **state, size_t *l, const char *separator, int quoted)
{
        const char *current;

        current = *state;

        if (!*current) {
                assert(**state == '\0');
                return NULL;
        }

        current += strspn(current, separator);
        if (!*current) {
                *state = current;
                return NULL;
        }

        if (quoted && strchr("\'\"", *current)) {
                char quotechars[2] = {*current, '\0'};

                *l = strcspn_escaped(current + 1, quotechars);
                if (current[*l + 1] == '\0' || current[*l + 1] != quotechars[0] ||
                    (current[*l + 2] && !strchr(separator, current[*l + 2]))) {
                        /* right quote missing or garbage at the end */
                        *state = current;
                        return NULL;
                }
                *state = current++ + *l + 2;
        } else if (quoted) {
                *l = strcspn_escaped(current, separator);
                if (current[*l] && !strchr(separator, current[*l])) {
                        /* unfinished escape */
                        *state = current;
                        return NULL;
                }
                *state = current + *l;
        } else {
                *l = strcspn(current, separator);
                *state = current + *l;
        }

        return current;
}

/* Rewind file pointer forward to new line.  */
int skip_fline(FILE *fp)
{
	int ch;

	do {
		if ((ch = fgetc(fp)) == EOF)
			return 1;
		if (ch == '\n')
			return 0;
	} while (1);
}


/* compare two strings, but ignoring non-alnum and case of the characters, for example
 * "Hello (123)!" is the same as "hello123".
 */
int ul_stralnumcmp(const char *p1, const char *p2)
{
	const unsigned char *s1 = (const unsigned char *) p1;
	const unsigned char *s2 = (const unsigned char *) p2;
	unsigned char c1, c2;

	do {
		do {
			c1 = (unsigned char) *s1++;
		} while (c1 != '\0' && !isalnum((unsigned int) c1));

		do {
			c2 = (unsigned char) *s2++;
		} while (c2 != '\0' && !isalnum((unsigned int) c2));

		if (c1 != '\0')
			c1 = tolower(c1);
		if (c2 != '\0')
			c2 = tolower(c2);
		if (c1 == '\0')
			return c1 - c2;
	} while (c1 == c2);

	return c1 - c2;
}

/*
 * Parses the first option from @optstr. The @optstr pointer is set to the beginning
 * of the next option. The options string looks like 'aaa,bbb=data,foo,bar="xxx"'.
 *
 * Note this function is used by libmount to parse mount options. Be careful when modify.
 *
 * Returns -EINVAL on parse error, 1 at the end of optstr and 0 on success.
 */
int ul_optstr_next(char **optstr, char **name, size_t *namesz,
		   char **value, size_t *valsz)
{
	int open_quote = 0;
	char *start = NULL, *stop = NULL, *p, *sep = NULL;
	char *optstr0;

	assert(optstr);
	assert(*optstr);

	optstr0 = *optstr;

	if (name)
		*name = NULL;
	if (namesz)
		*namesz = 0;
	if (value)
		*value = NULL;
	if (valsz)
		*valsz = 0;

	/* trim leading commas as to not invalidate option
	 * strings with multiple consecutive commas */
	while (optstr0 && *optstr0 == ',')
		optstr0++;

	for (p = optstr0; p && *p; p++) {
		if (!start)
			start = p;		/* beginning of the option item */
		if (*p == '"')
			open_quote ^= 1;	/* reverse the status */
		if (open_quote)
			continue;		/* still in quoted block */
		if (!sep && p > start && *p == '=')
			sep = p;		/* name and value separator */
		if (*p == ',' && (p == optstr0 || *(p - 1) != '\\'))
			stop = p;		/* terminate the option item */
		else if (*(p + 1) == '\0')
			stop = p + 1;		/* end of optstr */
		if (!start || !stop)
			continue;
		if (stop <= start)
			return -EINVAL;

		if (name)
			*name = start;
		if (namesz)
			*namesz = sep ? sep - start : stop - start;
		*optstr = *stop ? stop + 1 : stop;

		if (sep) {
			if (value)
				*value = sep + 1;
			if (valsz)
				*valsz = stop - sep - 1;
		}
		return 0;
	}

	return 1;				/* end of optstr */
}

#ifdef TEST_PROGRAM_STRUTILS

#include "cctype.h"

struct testS {
	char *name;
	char *value;
};

static int test_strdup_to_member(int argc, char *argv[])
{
	struct testS *xx;

	if (argc < 3)
		return EXIT_FAILURE;

	xx = calloc(1, sizeof(*xx));
	if (!xx)
		err(EXIT_FAILURE, "calloc() failed");

	strdup_to_struct_member(xx, name, argv[1]);
	strdup_to_struct_member(xx, value, argv[2]);

	if (strcmp(xx->name, argv[1]) != 0 &&
	    strcmp(xx->value, argv[2]) != 0)
		errx(EXIT_FAILURE, "strdup_to_struct_member() failed");

	printf("1: '%s', 2: '%s'\n", xx->name, xx->value);

	free(xx->name);
	free(xx->value);
	free(xx);
	return EXIT_SUCCESS;
}

static int test_strutils_sizes(int argc, char *argv[])
{
	uintmax_t size = 0;
	char *hum1, *hum2, *hum3;

	if (argc < 2)
		return EXIT_FAILURE;

	if (strtosize(argv[1], &size))
		errx(EXIT_FAILURE, "invalid size '%s' value", argv[1]);

	hum1 = size_to_human_string(SIZE_SUFFIX_1LETTER, size);
	hum2 = size_to_human_string(SIZE_SUFFIX_3LETTER |
				    SIZE_SUFFIX_SPACE, size);
	hum3 = size_to_human_string(SIZE_SUFFIX_3LETTER |
				    SIZE_SUFFIX_SPACE |
				    SIZE_DECIMAL_2DIGITS, size);

	printf("%25s : %20ju : %8s : %12s : %13s\n", argv[1], size, hum1, hum2, hum3);
	free(hum1);
	free(hum2);
	free(hum3);

	return EXIT_SUCCESS;
}

static int test_strutils_cmp_paths(int argc, char *argv[])
{
	int rc = streq_paths(argv[1], argv[2]);

	if (argc < 3)
		return EXIT_FAILURE;

	printf("%s: '%s' '%s'\n", rc == 1 ? "YES" : "NOT", argv[1], argv[2]);
	return EXIT_SUCCESS;
}

static int test_strutils_normalize(int argc, char *argv[])
{
	unsigned char *src, *dst, *org;
	size_t sz, len;

	if (argc < 2)
		return EXIT_FAILURE;

	org = (unsigned char *) strdup(argv[1]);
	src = (unsigned char *) strdup((char *) org);
	len = strlen((char *) src);
	dst = malloc(len + 1);

	if (!org || !src || !dst)
		goto done;

	/* two buffers */
	sz = __normalize_whitespace(src, len, dst, len + 1);
	printf("1: '%s' --> '%s' [sz=%zu]\n", src, dst, sz);

	/* one buffer */
	sz = normalize_whitespace(src);
	printf("2: '%s' --> '%s' [sz=%zu]\n", org, src, sz);

done:
	free(src);
	free(dst);
	free(org);

	return EXIT_SUCCESS;
}

static int test_strutils_cstrcasecmp(int argc, char *argv[])
{
	char *a, *b;

	if (argc < 3)
		return EXIT_FAILURE;

	a = argv[1];
	b = argv[2];

	if (!a || !b)
		return EXIT_FAILURE;

	printf("cmp    '%s' '%s' = %d\n", a, b, strcasecmp(a, b));
	printf("c_cmp  '%s' '%s' = %d\n", a, b, c_strcasecmp(a, b));
	printf("c_ncmp '%s' '%s' = %d\n", a, b, c_strncasecmp(a, b, strlen(a)));

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	if (argc == 3 && strcmp(argv[1], "--size") == 0) {
		return test_strutils_sizes(argc - 1, argv + 1);

	} else if (argc == 4 && strcmp(argv[1], "--cmp-paths") == 0) {
		return test_strutils_cmp_paths(argc - 1, argv + 1);

	} else if (argc == 4 && strcmp(argv[1], "--strdup-member") == 0) {
		return test_strdup_to_member(argc - 1, argv + 1);

	} else if  (argc == 4 && strcmp(argv[1], "--stralnumcmp") == 0) {
		printf("%s\n", ul_stralnumcmp(argv[2], argv[3]) == 0 ?
				"match" : "dismatch");
		return EXIT_SUCCESS;

	} else if (argc == 4 && strcmp(argv[1], "--cstrcasecmp") == 0) {
		return test_strutils_cstrcasecmp(argc - 1, argv + 1);

	} else if (argc == 3 && strcmp(argv[1], "--normalize") == 0) {
		return test_strutils_normalize(argc - 1, argv + 1);

	} else if (argc == 3 && strcmp(argv[1], "--strtos64") == 0) {
		printf("'%s'-->%jd\n", argv[2], strtos64_or_err(argv[2], "strtos64 failed"));
		return EXIT_SUCCESS;
	} else if (argc == 3 && strcmp(argv[1], "--strtou64") == 0) {
		printf("'%s'-->%ju\n", argv[2], strtou64_or_err(argv[2], "strtou64 failed"));
		return EXIT_SUCCESS;
	} else if (argc == 3 && strcmp(argv[1], "--strtos32") == 0) {
		printf("'%s'-->%d\n", argv[2], strtos32_or_err(argv[2], "strtos32 failed"));
		return EXIT_SUCCESS;
	} else if (argc == 3 && strcmp(argv[1], "--strtou32") == 0) {
		printf("'%s'-->%u\n", argv[2], strtou32_or_err(argv[2], "strtou32 failed"));
		return EXIT_SUCCESS;
	} else if (argc == 3 && strcmp(argv[1], "--strtos16") == 0) {
		printf("'%s'-->%hd\n", argv[2], strtos16_or_err(argv[2], "strtos16 failed"));
		return EXIT_SUCCESS;
	} else if (argc == 3 && strcmp(argv[1], "--strtou16") == 0) {
		printf("'%s'-->%hu\n", argv[2], strtou16_or_err(argv[2], "strtou16 failed"));
		return EXIT_SUCCESS;

	} else if (argc == 4 && strcmp(argv[1], "--strchr-escaped") == 0) {
		printf("\"%s\" --> \"%s\"\n", argv[2], ul_strchr_escaped(argv[2], *argv[3]));
		return EXIT_SUCCESS;

	} else if (argc == 2 && strcmp(argv[1], "--next-string") == 0) {
		char *buf = "abc\0Y\0\0xyz\0X";
		char *end = buf + 12;
		char *p = buf;

		do {
			printf("str: '%s'\n", p);
		} while ((p = ul_next_string(p, end)));

	} else {
		fprintf(stderr, "usage: %1$s --size <number>[suffix]\n"
				"       %1$s --cmp-paths <path> <path>\n"
				"       %1$s --strdup-member <str> <str>\n"
				"       %1$s --stralnumcmp <str> <str>\n"
				"       %1$s --cstrcasecmp <str> <str>\n"
				"       %1$s --normalize <str>\n"
				"       %1$s --strto{s,u}{16,32,64} <str>\n",
				argv[0]);
		exit(EXIT_FAILURE);
	}

	return EXIT_FAILURE;
}
#endif /* TEST_PROGRAM_STRUTILS */
