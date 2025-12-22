/*
 *  libzvbi -- Error correction functions
 *
 *  Copyright (C) 2001, 2002, 2003, 2004, 2007 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: hamm.h,v 1.16 2013-07-10 11:37:13 mschimek Exp $ */

#ifndef __ZVBI_HAMM_H__
#define __ZVBI_HAMM_H__

#include <inttypes.h>		/* uintN_t */
#include "macros.h"

VBI_BEGIN_DECLS

/* Public */

extern const uint8_t		_vbi_bit_reverse [256];
extern const uint8_t		_vbi_hamm8_fwd [16];
extern const int8_t		_vbi_hamm8_inv [256];
extern const int8_t		_vbi_hamm24_inv_par [3][256];

/**
 * @addtogroup Error Error correction functions
 * @ingroup Raw
 * @brief Helper functions to decode sliced VBI data.
 * @{
 */

/**
 * @param c Unsigned byte.
 * 
 * Reverses the bits of the argument.
 * 
 * @returns
 * Data bits 0 [msb] ... 7 [lsb].
 *
 * @since 0.2.12
 */
_vbi_inline unsigned int
vbi_rev8			(unsigned int		c)
{
	return _vbi_bit_reverse[(uint8_t) c];
}

/**
 * @param c Unsigned 16 bit word.
 * 
 * Reverses (or "reflects") the bits of the argument.
 * 
 * @returns
 * Data bits 0 [msb] ... 15 [lsb].
 *
 * @since 0.2.12
 */
_vbi_inline unsigned int
vbi_rev16			(unsigned int		c)
{
	return _vbi_bit_reverse[(uint8_t) c] * 256
		+ _vbi_bit_reverse[(uint8_t)(c >> 8)];
}

/**
 * @param p Pointer to a 16 bit word, last significant
 *   byte first.
 * 
 * Reverses (or "reflects") the bits of the argument.
 * 
 * @returns
 * Data bits 0 [msb] ... 15 [lsb].
 *
 * @since 0.2.12
 */
_vbi_inline unsigned int
vbi_rev16p			(const uint8_t *	p)
{
	return _vbi_bit_reverse[p[0]] * 256
		+ _vbi_bit_reverse[p[1]];
}

/**
 * @param c Unsigned byte.
 *
 * @returns
 * Changes the most significant bit of the byte
 * to make the number of set bits odd.
 *
 * @since 0.2.12
 */
_vbi_inline unsigned int
vbi_par8			(unsigned int		c)
{
	c &= 255;

	/* if 0 == (inv_par[] & 32) change bit 7 of c. */
	c ^= 128 & ~(_vbi_hamm24_inv_par[0][c] << 2);

	return c;
}

/**
 * @param c Unsigned byte. 
 * 
 * @returns
 * If the byte has odd parity (sum of bits modulo 2 is 1) the
 * byte AND 127, otherwise a negative value.
 *
 * @since 0.2.12
 */
_vbi_inline int
vbi_unpar8			(unsigned int		c)
{
/* Disabled until someone finds a reliable way
   to test for cmov support at compile time. */
#if 0
	int r = c & 127;

	/* This saves cache flushes and an explicit branch. */
	__asm__ (" testb	%1,%1\n"
		 " cmovp	%2,%0\n"
		 : "+&a" (r) : "c" (c), "rm" (-1));
	return r;
#endif
	if (_vbi_hamm24_inv_par[0][(uint8_t) c] & 32) {
		return c & 127;
	} else {
		/* The idea is to OR results together to find a parity
		   error in a sequence, rather than a test and branch on
		   each byte. */
		return -1;
	}
}

extern void
vbi_par				(uint8_t *		p,
				 unsigned int		n);
extern int
vbi_unpar			(uint8_t *		p,
				 unsigned int		n);

/**
 * @param c Integer between 0 ... 15.
 * 
 * Encodes a nibble with Hamming 8/4 protection
 * as specified in EN 300 706, Section 8.2.
 * 
 * @returns
 * Hamming encoded unsigned byte, lsb first transmitted.
 *
 * @since 0.2.12
 */
_vbi_inline unsigned int
vbi_ham8			(unsigned int		c)
{
	return _vbi_hamm8_fwd[c & 15];
}

/**
 * @param c Hamming 8/4 protected byte, lsb first transmitted.
 * 
 * Decodes a Hamming 8/4 protected byte
 * as specified in EN 300 706, Section 8.2.
 * 
 * @returns
 * Data bits (D4 [msb] ... D1 [lsb]) or a negative
 * value if the byte contained uncorrectable errors.
 *
 * @since 0.2.12
 */
_vbi_inline int
vbi_unham8			(unsigned int		c)
{
	return _vbi_hamm8_inv[(uint8_t) c];
}

/**
 * @param p Pointer to a Hamming 8/4 protected 16 bit word,
 *   last significant byte first, lsb first transmitted.
 * 
 * Decodes a Hamming 8/4 protected byte pair
 * as specified in EN 300 706, Section 8.2.
 * 
 * @returns
 * Data bits D4 [msb] ... D1 of first byte and D4 ... D1 [lsb]
 * of second byte, or a negative value if any of the bytes
 * contained uncorrectable errors.
 *
 * @since 0.2.12
 */
_vbi_inline int
vbi_unham16p			(const uint8_t *	p)
{
	return ((int) _vbi_hamm8_inv[p[0]])
	  | (((int) _vbi_hamm8_inv[p[1]]) << 4);
}

extern void
vbi_ham24p			(uint8_t *		p,
				 unsigned int		c);
extern int
vbi_unham24p			(const uint8_t *	p)
  _vbi_pure;

/** @} */

/* Private */

VBI_END_DECLS

#endif /* __ZVBI_HAMM_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
