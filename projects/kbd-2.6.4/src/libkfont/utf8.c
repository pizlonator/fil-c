// SPDX-License-Identifier: LGPL-2.0-or-later
/*
 * Copyright (C) 2007-2020 Alexey Gladkov <gladkov.alexey@gmail.com>
 *
 * Originally written by Andries Brouwer
 */
#include "config.h"
#include "utf8.h"

/*
 * Convert utf8 to long.
 * On success: update *inptr to be the first nonread character,
 *   set *err to 0, and return the obtained value.
 * On failure: leave *inptr unchanged, set *err to some nonzero error value:
 *   UTF8_BAD: bad utf8, UTF8_SHORT: input too short
 *   and return 0;
 *
 * cnt is either 0 or gives the number of available bytes
 */
int32_t
from_utf8(const unsigned char **inptr, long int cnt, int *err)
{
	const unsigned char *in;
	uint32_t uc, uc2, bit;
	int need, bad = 0;

	in   = *inptr;
	uc   = *in++;
	need = 0;
	bit  = 0x80;
	while (uc & bit) {
		need++;
		bit >>= 1;
	}
	uc &= (bit - 1);
	if (cnt && cnt < need) {
		*err = UTF8_SHORT;
		return 0;
	}
	if (need == 1)
		bad = 1;
	else if (need)
		while (--need) {
			uc2 = *in++;
			if ((uc2 & 0xc0) != 0x80) {
				bad = 1;
				break;
			}
			uc = ((uc << 6) | (uc2 & 0x3f));
		}
	if (bad) {
		*err = UTF8_BAD;
		return 0;
	}
	*inptr = in;
	*err   = 0;
	return (int32_t)uc;
}
