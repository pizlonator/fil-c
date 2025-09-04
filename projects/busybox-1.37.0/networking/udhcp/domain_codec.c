/* vi: set sw=4 ts=4: */
/*
 * RFC1035 domain compression routines (C) 2007 Gabriel Somlo <somlo at cmu.edu>
 *
 * Loosely based on the isc-dhcpd implementation by dhankins@isc.org
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#ifdef DNS_COMPR_TESTING
# define _GNU_SOURCE
# define FAST_FUNC /* nothing */
# define xmalloc malloc
# define xzalloc(s) calloc(s, 1)
# define xstrdup strdup
# define xrealloc realloc
# include <stdlib.h>
# include <stdint.h>
# include <string.h>
# include <stdio.h>
# include <ctype.h>
#else
# include "common.h"
#endif

#define NS_MAXDNAME  1025	/* max domain name length */
#define NS_MAXCDNAME  255	/* max compressed domain name length */
#define NS_MAXLABEL    63	/* max label length */
#define NS_MAXDNSRCH    6	/* max domains in search path */
#define NS_CMPRSFLGS 0xc0	/* name compression pointer flag */


/* Expand a RFC1035-compressed list of domain names "cstr", of length "clen";
 * return a newly allocated string containing the space-separated domains,
 * prefixed with the contents of string pre, or NULL if an error occurs.
 */
char* FAST_FUNC dname_dec(const uint8_t *cstr, int clen, const char *pre)
{
	char *ret, *end;
	unsigned len, crtpos, retpos, depth;

	crtpos = retpos = depth = 0;
	len = strlen(pre);
	end = ret = xstrdup(pre);

	/* Scan the string once, allocating new memory as needed */
	while (crtpos < clen) {
		const uint8_t *c;
		c = cstr + crtpos;

		if ((*c & NS_CMPRSFLGS) == NS_CMPRSFLGS) {
			/* pointer */
			if (crtpos + 2 > clen) /* no offset to jump to? abort */
				goto error;
			if (retpos == 0) /* toplevel? save return spot */
				retpos = crtpos + 2;
			depth++;
			crtpos = ((c[0] << 8) | c[1]) & 0x3fff; /* jump */
		} else if (*c) {
			unsigned label_len;
			/* label */
			if (crtpos + *c + 1 > clen) /* label too long? abort */
				goto error;
			ret = xrealloc(ret, len + *c + 1);
			/* \3com ---> "com." */
			end = (char *)mempcpy(ret + len, c + 1, *c);
			*end = '.';

			label_len = *c + 1;
			len += label_len;
			crtpos += label_len;
		} else {
			/* NUL: end of current domain name */
			if (retpos == 0) {
				/* toplevel? keep going */
				crtpos++;
			} else {
				/* return to toplevel saved spot */
				crtpos = retpos;
				retpos = depth = 0;
			}

			if (len != 0) {
				/* \4host\3com\0\4host and we are at \0:
				 * \3com was converted to "com.", change dot to space.
				 */
				ret[len - 1] = ' ';
			}
		}

		if (depth > NS_MAXDNSRCH /* too many jumps? abort, it's a loop */
		 || len > NS_MAXDNAME * NS_MAXDNSRCH /* result too long? abort */
		) {
			goto error;
		}
	}

	if (ret == end) { /* expanded string is empty? abort */
 error:
		free(ret);
		return NULL;
	}

	*end = '\0';
	return ret;
}

/* Convert a domain name (src) from human-readable "foo.BLAH.com" format into
 * RFC1035 encoding "\003foo\004blah\003com\000". Return allocated string, or
 * NULL if an error occurs.
 */
static uint8_t *convert_dname(const char *src, int *retlen)
{
	uint8_t *res, *lenptr, *dst;

	res = xzalloc(strlen(src) + 2);
	dst = lenptr = res;
	dst++;

	for (;;) {
		uint8_t c;
		int len;

		c = (uint8_t)*src++;
		if (c == '.' || c == '\0') {  /* end of label */
			len = dst - lenptr - 1;
			/* label too long, too short, or two '.'s in a row (len will be 0) */
			if (len > NS_MAXLABEL || len == 0)
				goto error;

			*lenptr = len;
			if (c == '\0' || *src == '\0')	/* "" or ".": end of src */
				break;
			lenptr = dst++;
			continue;
		}
		*dst++ = tolower(c);
	}

	*retlen = dst + 1 - res;
	if (*retlen > NS_MAXCDNAME) {  /* dname too long? abort */
 error:
		free(res);
		*retlen = 0;
		return NULL;
	}

	return res;
}

#if 0 //UNUSED
/* Returns the offset within cstr at which dname can be found, or -1 */
static int find_offset(const uint8_t *cstr, int clen, const uint8_t *dname)
{
	const uint8_t *c, *d;
	int off;

	/* find all labels in cstr */
	off = 0;
	while (off < clen) {
		c = cstr + off;

		if ((*c & NS_CMPRSFLGS) == NS_CMPRSFLGS) {  /* pointer, skip */
			off += 2;
			continue;
		}
		if (*c) {  /* label, try matching dname */
			d = dname;
			while (1) {
				unsigned len1 = *c + 1;
				if (memcmp(c, d, len1) != 0)
					break;
				if (len1 == 1)  /* at terminating NUL - match, return offset */
					return off;
				d += len1;
				c += len1;
				if ((*c & NS_CMPRSFLGS) == NS_CMPRSFLGS)  /* pointer, jump */
					c = cstr + (((c[0] & 0x3f) << 8) | c[1]);
			}
			off += cstr[off] + 1;
			continue;
		}
		/* NUL, skip */
		off++;
	}

	return -1;
}
#endif

uint8_t* FAST_FUNC dname_enc(/*const uint8_t *cstr, int clen,*/ const char *src, int *retlen)
{
#if 0 //UNUSED, was intended for long, repetitive DHCP_DOMAIN_SEARCH options?
	uint8_t *d, *dname;
/* Computes string to be appended to cstr so that src would be added to
 * the compression (best case, it's a 2-byte pointer to some offset within
 * cstr; worst case, it's all of src, converted to <4>host<3>com<0> format).
 * The computed string is returned directly; its length is returned via retlen;
 * NULL and 0, respectively, are returned if an error occurs.
 */
	dname = convert_dname(src, retlen);
	if (dname == NULL) {
		return NULL;
	}

	d = dname;
	while (*d) {
		if (cstr) {
			int off = find_offset(cstr, clen, d);
			if (off >= 0) {	/* found a match, add pointer and return */
				*d++ = NS_CMPRSFLGS | (off >> 8);
				*d = off;
				break;
			}
		}
		d += *d + 1;
	}

	*retlen = d - dname + 1;
	return dname;
#endif
	return convert_dname(src, retlen);
}

#ifdef DNS_COMPR_TESTING
/* gcc -Wall -DDNS_COMPR_TESTING domain_codec.c -o domain_codec && ./domain_codec */
int main(int argc, char **argv)
{
	int len;
	uint8_t *encoded;

        uint8_t str[6] = { 0x00, 0x00, 0x02, 0x65, 0x65, 0x00 };
        printf("NUL:'%s'\n",   dname_dec(str, 6, ""));

#define DNAME_DEC(encoded,pre) dname_dec((uint8_t*)(encoded), sizeof(encoded), (pre))
	printf("'%s'\n",       DNAME_DEC("\4host\3com\0", "test1:"));
	printf("test2:'%s'\n", DNAME_DEC("\4host\3com\0\4host\3com\0", ""));
	printf("test3:'%s'\n", DNAME_DEC("\4host\3com\0\xC0\0", ""));
	printf("test4:'%s'\n", DNAME_DEC("\4host\3com\0\xC0\5", ""));
	printf("test5:'%s'\n", DNAME_DEC("\4host\3com\0\xC0\5\1z\xC0\xA", ""));

#if 0
#define DNAME_ENC(cache,source,lenp) dname_enc((uint8_t*)(cache), sizeof(cache), (source), (lenp))
	encoded = dname_enc(NULL, 0, "test.net", &len);
	printf("test6:'%s' len:%d\n", dname_dec(encoded, len, ""), len);
	encoded = DNAME_ENC("\3net\0", "test.net", &len);
	printf("test7:'%s' len:%d\n", dname_dec(encoded, len, ""), len);
	encoded = DNAME_ENC("\4test\3net\0", "test.net", &len);
	printf("test8:'%s' len:%d\n", dname_dec(encoded, len, ""), len);
#endif

	encoded = dname_enc("test.net", &len);
	printf("test6:'%s' len:%d\n", dname_dec(encoded, len, ""), len);
	encoded = dname_enc("test.host.com", &len);
	printf("test7:'%s' len:%d\n", dname_dec(encoded, len, ""), len);

	return 0;
}
#endif
