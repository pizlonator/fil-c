/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_ISO_H
#define UTIL_LINUX_ISO_H

#include <stdbool.h>
#include <stdint.h>

#include "c.h"

static inline uint16_t isonum_721(const unsigned char *p)
{
	return ((p[0] & 0xff)
		| ((p[1] & 0xff) << 8));
}

static inline uint16_t isonum_722(const unsigned char *p)
{
	return ((p[1] & 0xff)
		| ((p[0] & 0xff) << 8));
}

static inline uint16_t isonum_723(const unsigned char *p, bool check_match)
{
	uint16_t le = isonum_721(p);
	uint16_t be = isonum_722(p + 2);

	if (check_match && le != be)
		/* translation is useless */
		warnx("723error: le=%d be=%d", le, be);
	return (le);
}

static inline uint32_t isonum_731(const unsigned char *p)
{
	return ((p[0] & 0xff)
		| ((p[1] & 0xff) << 8)
		| ((p[2] & 0xff) << 16)
		| (((uint32_t) p[3] & 0xff) << 24));
}

static inline uint32_t isonum_732(const unsigned char *p)
{
	return ((p[3] & 0xff)
		| ((p[2] & 0xff) << 8)
		| ((p[1] & 0xff) << 16)
		| (((uint32_t) p[0] & 0xff) << 24));
}

static inline uint32_t isonum_733(const unsigned char *p, bool check_match)
{
	uint32_t le = isonum_731(p);
	uint32_t be = isonum_732(p + 4);

	if (check_match && le != be)
		/* translation is useless */
		warnx("733error: le=%d be=%d", le, be);
	return(le);
}

#endif
