/*
 * Copyright (c) 1997-8,2007-8,2019,2021 Andrew G Morgan <morgan@kernel.org>
 * Copyright (c) 1997 Andrew Main <zefram@dcs.warwick.ac.uk>
 *
 * This file deals with exchanging internal and textual
 * representations of capability sets.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>

#define LIBCAP_PLEASE_INCLUDE_ARRAY
#include "libcap.h"

static char const *_cap_names[__CAP_BITS] = LIBCAP_CAP_NAMES;

#include <ctype.h>
#include <limits.h>

#ifdef INCLUDE_GPERF_OUTPUT
/* we need to include it after #define _GNU_SOURCE is set */
#include INCLUDE_GPERF_OUTPUT
#endif

/* Maximum output text length */
#define CAP_TEXT_SIZE    (__CAP_NAME_SIZE * __CAP_MAXBITS)

/*
 * Parse a textual representation of capabilities, returning an internal
 * representation.
 */

#define raise_cap_mask(flat, c)  (flat)[CAP_TO_INDEX(c)] |= CAP_TO_MASK(c)

static void setbits(cap_t a, const __u32 *b, cap_flag_t set, unsigned blks)
{
    int n;
    for (n = blks; n--; ) {
	a->u[n].flat[set] |= b[n];
    }
}

static void clrbits(cap_t a, const __u32 *b, cap_flag_t set, unsigned blks)
{
    int n;
    for (n = blks; n--; )
	a->u[n].flat[set] &= ~b[n];
}

static char const *namcmp(char const *str, char const *nam)
{
    while (*nam && tolower((unsigned char)*str) == *nam) {
	str++;
	nam++;
    }
    if (*nam || isalnum((unsigned char)*str) || *str == '_')
	return NULL;
    return str;
}

/*
 * forceall forces all of the kernel named capabilities to be assigned
 * the masked value, and zeroed otherwise. Note, if the kernel is ahead
 * of libcap, the upper bits will be referred to by number.
 */
static void forceall(__u32 *flat, __u32 value, unsigned blks)
{
    unsigned n;
    cap_value_t cmb = cap_max_bits();
    for (n = blks; n--; ) {
	unsigned base = 32*n;
	__u32 mask = 0;
	if (cmb >= base + 32) {
	    mask = ~0;
	} else if (cmb > base) {
	    mask = (unsigned) ((1ULL << (cmb % 32)) - 1);
	}
	flat[n] = value & mask;
    }

    return;
}

static int lookupname(char const **strp)
{
    union {
	char const *constp;
	char *p;
    } str;

    str.constp = *strp;
    if (isdigit(*str.constp)) {
	unsigned long n = strtoul(str.constp, &str.p, 0);
	if (n >= __CAP_MAXBITS)
	    return -1;
	*strp = str.constp;
	return n;
    } else {
	int c;
	size_t len;

	for (len=0; (c = str.constp[len]); ++len) {
	    if (!(isalpha(c) || (c == '_'))) {
		break;
	    }
	}

#ifdef GPERF_DOWNCASE
	const struct __cap_token_s *token_info;

	token_info = __cap_lookup_name(str.constp, len);
	if (token_info != NULL) {
	    *strp = str.constp + len;
	    return token_info->index;
	}
#else /* ie., ndef GPERF_DOWNCASE */
	char const *s;
	unsigned n = cap_max_bits();
	if (n > __CAP_BITS) {
	    n = __CAP_BITS;
	}
	while (n--) {
	    if (_cap_names[n] && (s = namcmp(str.constp, _cap_names[n]))) {
		*strp = s;
		return n;
	    }
	}
#endif /* def GPERF_DOWNCASE */

	return -1;   	/* No definition available */
    }
}

cap_t cap_from_text(const char *str)
{
    cap_t res;
    int n;
    unsigned cap_blks;

    if (str == NULL) {
	_cap_debug("bad argument");
	errno = EINVAL;
	return NULL;
    }

    if (!(res = cap_init()))
	return NULL;

    switch (res->head.version) {
    case _LINUX_CAPABILITY_VERSION_1:
	cap_blks = _LINUX_CAPABILITY_U32S_1;
	break;
    case _LINUX_CAPABILITY_VERSION_2:
	cap_blks = _LINUX_CAPABILITY_U32S_2;
	break;
    case _LINUX_CAPABILITY_VERSION_3:
	cap_blks = _LINUX_CAPABILITY_U32S_3;
	break;
    default:
	cap_free(res);
	errno = EINVAL;
	return NULL;
    }

    _cap_debug("%s", str);

    for (;;) {
	__u32 list[__CAP_BLKS];
	char op;
	int flags = 0, listed=0;

	memset(list, 0, sizeof(__u32)*__CAP_BLKS);

	/* skip leading spaces */
	while (isspace((unsigned char)*str))
	    str++;
	if (!*str) {
	    _cap_debugcap("e = ", *res, CAP_EFFECTIVE);
	    _cap_debugcap("i = ", *res, CAP_INHERITABLE);
	    _cap_debugcap("p = ", *res, CAP_PERMITTED);

	    return res;
	}

	/* identify caps specified by this clause */
	if (isalnum((unsigned char)*str) || *str == '_') {
	    for (;;) {
		if (namcmp(str, "all")) {
		    str += 3;
		    forceall(list, ~0, cap_blks);
		} else {
		    n = lookupname(&str);
		    if (n == -1)
			goto bad;
		    raise_cap_mask(list, n);
		}
		if (*str != ',')
		    break;
		if (!isalnum((unsigned char)*++str) && *str != '_')
		    goto bad;
	    }
	    listed = 1;
	} else if (*str == '+' || *str == '-') {
	    goto bad;                    /* require a list of capabilities */
	} else {
	    forceall(list, ~0, cap_blks);
	}

	/* identify first operation on list of capabilities */
	op = *str++;
	if (op == '=' && (*str == '+' || *str == '-')) {
	    if (!listed)
		goto bad;
	    op = (*str++ == '+' ? 'P':'M'); /* skip '=' and take next op */
	} else if (op != '+' && op != '-' && op != '=')
	    goto bad;

	/* cycle through list of actions */
	do {
	    _cap_debug("next char = '%c'", *str);
	    if (*str && !isspace(*str)) {
		switch (*str++) {    /* Effective, Inheritable, Permitted */
		case 'e':
		    flags |= LIBCAP_EFF;
		    break;
		case 'i':
		    flags |= LIBCAP_INH;
		    break;
		case 'p':
		    flags |= LIBCAP_PER;
		    break;
		default:
		    goto bad;
		}
	    } else if (op != '=') {
		_cap_debug("only '=' can be followed by space");
		goto bad;
	    }

	    _cap_debug("how to read?");
	    switch (op) {               /* how do we interpret the caps? */
	    case '=':
	    case 'P':                                              /* =+ */
	    case 'M':                                              /* =- */
		clrbits(res, list, CAP_EFFECTIVE, cap_blks);
		clrbits(res, list, CAP_PERMITTED, cap_blks);
		clrbits(res, list, CAP_INHERITABLE, cap_blks);
		if (op == 'M')
		    goto minus;
		/* fall through */
	    case '+':
		if (flags & LIBCAP_EFF)
		    setbits(res, list, CAP_EFFECTIVE, cap_blks);
		if (flags & LIBCAP_PER)
		    setbits(res, list, CAP_PERMITTED, cap_blks);
		if (flags & LIBCAP_INH)
		    setbits(res, list, CAP_INHERITABLE, cap_blks);
		break;
	    case '-':
	    minus:
		if (flags & LIBCAP_EFF)
		    clrbits(res, list, CAP_EFFECTIVE, cap_blks);
		if (flags & LIBCAP_PER)
		    clrbits(res, list, CAP_PERMITTED, cap_blks);
		if (flags & LIBCAP_INH)
		    clrbits(res, list, CAP_INHERITABLE, cap_blks);
		break;
	    }

	    /* new directive? */
	    if (*str == '+' || *str == '-') {
		if (!listed) {
		    _cap_debug("for + & - must list capabilities");
		    goto bad;
		}
		flags = 0;                       /* reset the flags */
		op = *str++;
		if (!isalpha(*str))
		    goto bad;
	    }
	} while (*str && !isspace(*str));
	_cap_debug("next clause");
    }

bad:
    cap_free(res);
    res = NULL;
    errno = EINVAL;
    return res;
}

/*
 * lookup a capability name and return its numerical value
 */
int cap_from_name(const char *name, cap_value_t *value_p)
{
    int n;

    if (((n = lookupname(&name)) >= 0) && (value_p != NULL)) {
	*value_p = (unsigned) n;
    }
    return -(n < 0);
}

/*
 * Convert a single capability index number into a string representation
 */
char *cap_to_name(cap_value_t cap)
{
    char *tmp, *result;

    if ((cap >= 0) && (cap < __CAP_BITS)) {
	return _libcap_strdup(_cap_names[cap]);
    }
    if (asprintf(&tmp, "%u", cap) <= 0) {
	_cap_debug("asprintf filed");
	return NULL;
    }

    result = _libcap_strdup(tmp);
    free(tmp);
    return result;
}

/*
 * Convert an internal representation to a textual one. The textual
 * representation is stored in static memory. It will be overwritten
 * on the next occasion that this function is called.
 */

static int getstateflags(cap_t caps, int capno)
{
    int f = 0;

    if (isset_cap(caps, capno, CAP_EFFECTIVE)) {
	f |= LIBCAP_EFF;
    }
    if (isset_cap(caps, capno, CAP_PERMITTED)) {
	f |= LIBCAP_PER;
    }
    if (isset_cap(caps, capno, CAP_INHERITABLE)) {
	f |= LIBCAP_INH;
    }

    return f;
}

/*
 * This code assumes that the longest named capability is longer than
 * the decimal text representation of __CAP_MAXBITS. This is very true
 * at the time of writing and likely to remain so. However, we have
 * a test in cap_text to validate it at build time.
 */
#define CAP_TEXT_BUFFER_ZONE 100

char *cap_to_text(cap_t caps, ssize_t *length_p)
{
    char buf[CAP_TEXT_SIZE+CAP_TEXT_BUFFER_ZONE];
    char *p, *base;
    int histo[8];
    int m, t;
    unsigned n;

    /* Check arguments */
    if (!good_cap_t(caps)) {
	errno = EINVAL;
	return NULL;
    }

    _cap_debugcap("e = ", *caps, CAP_EFFECTIVE);
    _cap_debugcap("i = ", *caps, CAP_INHERITABLE);
    _cap_debugcap("p = ", *caps, CAP_PERMITTED);

    memset(histo, 0, sizeof(histo));

    /* default prevailing state to the named bits */
    cap_value_t cmb = cap_max_bits();
    for (n = 0; n < cmb; n++)
	histo[getstateflags(caps, n)]++;

    /* find which combination of capability sets shares the most bits
       we bias to preferring non-set (m=0) with the >= 0 test. Failing
       to do this causes strange things to happen with older systems
       that don't know about bits 32+. */
    for (m=t=7; t--; )
	if (histo[t] >= histo[m])
	    m = t;

    /* blank is not a valid capability set */
    base = buf;
    p = sprintf(buf, "=%s%s%s",
		(m & LIBCAP_EFF) ? "e" : "",
		(m & LIBCAP_INH) ? "i" : "",
		(m & LIBCAP_PER) ? "p" : "" ) + buf;

    for (t = 8; t--; ) {
	if (t == m || !histo[t]) {
	    continue;
	}
	*p++ = ' ';
	for (n = 0; n < cmb; n++) {
	    if (getstateflags(caps, n) == t) {
	        char *this_cap_name = cap_to_name(n);
		if (this_cap_name == NULL) {
		    return NULL;
		}
	        if ((strlen(this_cap_name) + (p - buf)) > CAP_TEXT_SIZE) {
		    cap_free(this_cap_name);
		    errno = ERANGE;
		    return NULL;
	        }
	        p += sprintf(p, "%s,", this_cap_name);
	        cap_free(this_cap_name);
	    }
	}
	p--;
	n = t & ~m;
	if (n) {
	    char op = '+';
	    if (base[0] == '=' && base[1] == ' ') {
		/*
		 * Special case all lowered default "= foo,...+eip
		 * ..." as "foo,...=eip ...". (Equivalent but shorter.)
		 */
		base += 2;
		op = '=';
	    }
	    p += sprintf(p, "%c%s%s%s", op,
			 (n & LIBCAP_EFF) ? "e" : "",
			 (n & LIBCAP_INH) ? "i" : "",
			 (n & LIBCAP_PER) ? "p" : "");
	}
	n = ~t & m;
	if (n) {
	    p += sprintf(p, "-%s%s%s",
			 (n & LIBCAP_EFF) ? "e" : "",
			 (n & LIBCAP_INH) ? "i" : "",
			 (n & LIBCAP_PER) ? "p" : "");
	}
	if (p - buf > CAP_TEXT_SIZE) {
	    errno = ERANGE;
	    return NULL;
	}
    }

    /* capture remaining unnamed bits - which must all be +. */
    memset(histo, 0, sizeof(histo));
    for (n = cmb; n < __CAP_MAXBITS; n++)
	histo[getstateflags(caps, n)]++;

    for (t = 8; t-- > 1; ) {
	if (!histo[t]) {
	    continue;
	}
	*p++ = ' ';
	for (n = cmb; n < __CAP_MAXBITS; n++) {
	    if (getstateflags(caps, n) == t) {
		char *this_cap_name = cap_to_name(n);
		if (this_cap_name == NULL) {
		    return NULL;
		}
	        if ((strlen(this_cap_name) + (p - buf)) > CAP_TEXT_SIZE) {
		    cap_free(this_cap_name);
		    errno = ERANGE;
		    return NULL;
	        }
		p += sprintf(p, "%s,", this_cap_name);
		cap_free(this_cap_name);
	    }
	}
	p--;
	p += sprintf(p, "+%s%s%s",
		     (t & LIBCAP_EFF) ? "e" : "",
		     (t & LIBCAP_INH) ? "i" : "",
		     (t & LIBCAP_PER) ? "p" : "");
	if (p - buf > CAP_TEXT_SIZE) {
	    errno = ERANGE;
	    return NULL;
	}
    }

    _cap_debug("%s", base);
    if (length_p) {
	*length_p = p - base;
    }

    return (_libcap_strdup(base));
}

/*
 * cap_mode_name returns a text token naming the specified mode.
 */
const char *cap_mode_name(cap_mode_t flavor) {
    switch (flavor) {
    case CAP_MODE_NOPRIV:
	return "NOPRIV";
    case CAP_MODE_PURE1E_INIT:
	return "PURE1E_INIT";
    case CAP_MODE_PURE1E:
	return "PURE1E";
    case CAP_MODE_UNCERTAIN:
	return "UNCERTAIN";
    case CAP_MODE_HYBRID:
	return "HYBRID";
    default:
	return "UNKNOWN";
    }
}

/*
 * cap_iab_to_text serializes an iab into a canonical text
 * representation.
 */
char *cap_iab_to_text(cap_iab_t iab)
{
    char buf[CAP_TEXT_SIZE+CAP_TEXT_BUFFER_ZONE];
    char *p = buf;
    cap_value_t c, cmb = cap_max_bits();
    int first = 1;

    if (good_cap_iab_t(iab)) {
	_cap_mu_lock(&iab->mutex);
	for (c = 0; c < cmb; c++) {
	    int keep = 0;
	    int o = c >> 5;
	    __u32 bit = 1U << (c & 31);
	    __u32 ib = iab->i[o] & bit;
	    __u32 ab = iab->a[o] & bit;
	    __u32 nbb = iab->nb[o] & bit;
	    if (!(nbb | ab | ib)) {
		continue;
	    }
	    if (!first) {
		*p++ = ',';
	    }
	    if (nbb) {
		*p++ = '!';
		keep = 1;
	    }
	    if (ab) {
		*p++ = '^';
		keep = 1;
	    } else if (nbb && ib) {
		*p++ = '%';
	    }
	    if (keep || ib) {
		if (c < __CAP_BITS) {
		    strcpy(p, _cap_names[c]);
		} else {
		    sprintf(p, "%u", c);
		}
		p += strlen(p);
		first = 0;
	    }
	}
	_cap_mu_unlock(&iab->mutex);
    }
    *p = '\0';
    return _libcap_strdup(buf);
}

cap_iab_t cap_iab_from_text(const char *text)
{
    cap_iab_t iab = cap_iab_init();
    if (iab == NULL) {
	return iab;
    }
    if (text != NULL) {
	unsigned flags;
	for (flags = 0; *text; text++) {
	    /* consume prefixes */
	    switch (*text) {
	    case '!':
		flags |= LIBCAP_IAB_NB_FLAG;
		continue;
	    case '^':
		flags |= LIBCAP_IAB_IA_FLAG;
		continue;
	    case '%':
		flags |= LIBCAP_IAB_I_FLAG;
		continue;
	    default:
		break;
	    }
	    if (!flags) {
		flags = LIBCAP_IAB_I_FLAG;
	    }

	    /* consume cap name */
	    cap_value_t c = lookupname(&text);
	    if (c == -1) {
		goto cleanup;
	    }
	    unsigned o = c >> 5;
	    __u32 mask = 1U << (c & 31);
	    if (flags & LIBCAP_IAB_I_FLAG) {
		iab->i[o] |= mask;
	    }
	    if (flags & LIBCAP_IAB_A_FLAG) {
		iab->a[o] |= mask;
	    }
	    if (flags & LIBCAP_IAB_NB_FLAG) {
		iab->nb[o] |= mask;
	    }

	    /* rest should be end or comma */
	    if (*text == '\0') {
		break;
	    }
	    if (*text != ',') {
		goto cleanup;
	    }
	    flags = 0;
	}
    }
    return iab;

cleanup:
    cap_free(iab);
    errno = EINVAL;
    return NULL;
}

static __u32 _parse_hex32(const char *c)
{
    int i;
    __u32 v = 0;
    for (i=0; i < 8; i++, c++) {
	v <<= 4;
	if (*c == 0 || *c < '0') {
	    return 0;
	} else if (*c <= '9') {
	    v += *c - '0';
	} else if (*c > 'f') {
	    return 0;
	} else if (*c >= 'a') {
	    v += *c + 10 - 'a';
	} else if (*c < 'A') {
	    return 0;
	} else if (*c <= 'F') {
	    v += *c + 10 - 'A';
	} else {
	    return 0;
	}
    }
    return v;
}

/*
 * _parse_vec_string converts the hex dumps in /proc/<pid>/current into
 * an array of u32s - masked as per the forceall() mask.
 */
static __u32 _parse_vec_string(__u32 *vals, const char *c, int invert)
{
    int i;
    int words = strlen(c)/8;
    if (words > _LIBCAP_CAPABILITY_U32S) {
	return 0;
    }
    forceall(vals, ~0, words);
    for (i = 0; i < words; i++) {
	__u32 val = _parse_hex32(c+8*(words-1-i));
	if (invert) {
	    val = ~val;
	}
	vals[i] &= val;
    }
    return ~0;
}

/*
 * libcap believes this is the root of the mounted "/proc"
 * filesystem. (NULL == "/proc".)
 */
static char *_cap_proc_dir;

/*
 * If the constructor is called (see cap_alloc.c) then we'll need the
 * corresponding destructor.
 */
__attribute__((destructor (300))) static void _cleanup_libcap(void)
{
    if (_cap_proc_dir == NULL) {
	return;
    }
    cap_free(_cap_proc_dir);
    _cap_proc_dir = NULL;
}

/*
 * cap_proc_root reads and (optionally: when root != NULL) changes
 * libcap's notion of where the "/proc" filesystem is mounted. It
 * defaults to the value "/proc". Note, this is a global value and not
 * considered thread safe to write - so the client should take
 * suitable care when changing it. Further, libcap will allocate
 * memory for storing the replacement root, and it is this memory that
 * is returned. So, when changing the value, the caller should
 * cap_free(the-return-value) when done with it.
 *
 * A return value of NULL implies the default is in effect "/proc".
 */
char *cap_proc_root(const char *root)
{
    char *old = _cap_proc_dir;
    if (root != NULL) {
	_cap_proc_dir = _libcap_strdup(root);
    }
    return old;
}

#define PROC_LINE_MAX (8 + 8*_LIBCAP_CAPABILITY_U32S + 100)
/*
 * cap_iab_get_pid fills an IAB tuple from the content of
 * /proc/<pid>/status. Linux doesn't support syscall access to the
 * needed information, so we parse it out of that file.
 */
cap_iab_t cap_iab_get_pid(pid_t pid)
{
    cap_iab_t iab;
    char *path;
    FILE *file;
    char line[PROC_LINE_MAX];
    const char *proc_root = _cap_proc_dir;

    if (proc_root == NULL) {
	proc_root = "/proc";
    }
    if (asprintf(&path, "%s/%d/status", proc_root, pid) <= 0) {
	return NULL;
    }
    file = fopen(path, "r");
    free(path);
    if (file == NULL) {
	return NULL;
    }

    iab = cap_iab_init();
    uint ok = 0;
    if (iab != NULL) {
	while (fgets(line, PROC_LINE_MAX-1, file) != NULL) {
	    if (strncmp("Cap", line, 3) != 0) {
		continue;
	    }
	    if (strncmp("Inh:\t", line+3, 5) == 0) {
		ok = (_parse_vec_string(iab->i, line+8, 0) &
		    LIBCAP_IAB_I_FLAG) | ok;
		continue;
	    }
	    if (strncmp("Bnd:\t", line+3, 5) == 0) {
		ok = (_parse_vec_string(iab->nb, line+8, 1) &
		      LIBCAP_IAB_NB_FLAG) | ok;
		continue;
	    }
	    if (strncmp("Amb:\t", line+3, 5) == 0) {
		ok = (_parse_vec_string(iab->a, line+8, 0) &
		      LIBCAP_IAB_A_FLAG) | ok;
		continue;
	    }
	}
    }
    if (ok != (LIBCAP_IAB_IA_FLAG | LIBCAP_IAB_NB_FLAG)) {
	cap_free(iab);
	iab = NULL;
    }
    fclose(file);
    return iab;
}
