/*
 *  libzvbi -- BCD arithmetic for Teletext page numbers
 *
 *  Copyright (C) 2001, 2002 Michael H. Schimek
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

/* $Id: bcd.h,v 1.19 2008-02-21 07:18:52 mschimek Exp $ */

#ifndef BCD_H
#define BCD_H

#include "misc.h"

/**
 * @addtogroup BCD BCD arithmetic for Teletext page numbers
 * @ingroup HiDec
 *
 * Teletext page numbers are expressed as packed binary coded decimal
 * numbers in range 0x100 to 0x8FF. The bcd format encodes one decimal
 * digit in every hex nibble (four bits) of the number. Page numbers
 * containing digits 0xA to 0xF are reserved for various system purposes
 * and not intended for display.
 */

/* Public */

/**
 * @ingroup HiDec
 * 
 * Teletext or Closed Caption page number. For Teletext pages
 * this is a packed bcd number in range 0x100 ... 0x8FF. Page
 * numbers containing digits 0xA to 0xF are reserved for various
 * system purposes, these pages are not intended for display.
 * 
 * Closed Caption page numbers between 1 ... 8 correspond
 * to the four Caption and Text channels:
 * <table>
 * <tr><td>1</td><td>Caption 1</td><td>
 *   "Primary synchronous caption service [English]"</td></tr>
 * <tr><td>2</td><td>Caption 2</td><td>
 *   "Special non-synchronous data that is intended to
 *   augment information carried in the program"</td></tr>
 * <tr><td>3</td><td>Caption 3</td><td>
 *   "Secondary synchronous caption service, usually
 *    second language [Spanish, French]"</td></tr>
 * <tr><td>4</td><td>Caption 4</td><td>
 *   "Special non-synchronous data similar to Caption 2"</td></tr>
 * <tr><td>5</td><td>Text 1</td><td>
 *   "First text service, data usually not program related"</td></tr>
 * <tr><td>6</td><td>Text 2</td><td>
 *   "Second text service, additional data usually not program related
 *    [ITV data]"</td></tr>
 * <tr><td>7</td><td>Text 3</td><td>
 *   "Additional text channel"</td></tr>
 * <tr><td>8</td><td>Text 4</td><td>
 *   "Additional text channel"</td></tr>
 * </table>
 */
/* XXX unsigned? */
typedef int vbi_pgno;

/**
 * @ingroup HiDec
 *
 * This is the subpage number only applicable to Teletext pages,
 * a packed bcd number in range 0x00 ... 0x99. On special 'clock' pages
 * (for example listing the current time in different time zones)
 * it can assume values between 0x0000 ... 0x2359 expressing
 * local time. These are not actually subpages.
 */
typedef int vbi_subno;

/**
 * @ingroup HiDec
 */
#define VBI_ANY_SUBNO 0x3F7F
/**
 * @ingroup HiDec
 */
#define VBI_NO_SUBNO 0x3F7F

/**
 * @ingroup BCD
 * @param dec Decimal number.
 * 
 * Converts a two's complement binary between 0 ... 999 to a
 * packed bcd number in range  0x000 ... 0x999. Extra digits in
 * the input will be discarded.
 * 
 * @return
 * BCD number.
 */
_vbi_inline unsigned int
vbi_dec2bcd(unsigned int dec)
{
	return (dec % 10) + ((dec / 10) % 10) * 16 + ((dec / 100) % 10) * 256;
}

/**
 * @ingroup BCD
 * @since 0.2.28
 */
#define vbi_bin2bcd(n) vbi_dec2bcd(n)

/**
 * @ingroup BCD
 * @param bcd BCD number.
 * 
 * Converts a packed bcd number between 0x000 ... 0xFFF to a two's
 * complement binary in range 0 ... 999. Extra digits in the input
 * will be discarded.
 * 
 * @return
 * Decimal number. The result is undefined when the bcd number contains
 * hex digits 0xA ... 0xF.
 **/
_vbi_inline unsigned int
vbi_bcd2dec(unsigned int bcd)
{
	return (bcd & 15) + ((bcd >> 4) & 15) * 10 + ((bcd >> 8) & 15) * 100;
}

/**
 * @ingroup BCD
 * @since 0.2.28
 */
#define vbi_bcd2bin(n) vbi_bcd2dec(n)

/**
 * @ingroup BCD
 * @param a BCD number.
 * @param b BCD number.
 * 
 * Adds two packed bcd numbers, returning a packed bcd sum. Arguments
 * and result are in range 0xF000&nbsp;0000 ... 0x0999&nbsp;9999, that
 * is -10**7 ... +10**7 - 1 in decimal notation. To subtract you can
 * add the 10's complement, e. g. -1 = 0xF999&nbsp;9999.
 * 
 * @return
 * Packed bcd number. The result is undefined when any of the arguments
 * contain hex digits 0xA ... 0xF.
 */
_vbi_inline unsigned int
vbi_add_bcd(unsigned int a, unsigned int b)
{
	unsigned int t;

	a += 0x06666666;
	t  = a + b;
	b ^= a ^ t;
	b  = (~b & 0x11111110) >> 3;
	b |= b * 2;

	return t - b;
}

/**
 * @ingroup BCD
 * @param bcd BCD number.
 * 
 * Tests if @a bcd forms a valid BCD number. The argument must be
 * in range 0x0000&nbsp;0000 ... 0x0999&nbsp;9999.
 * 
 * @return
 * @c FALSE if @a bcd contains hex digits 0xA ... 0xF.
 */
_vbi_inline vbi_bool
vbi_is_bcd(unsigned int bcd)
{
	static const unsigned int x = 0x06666666;

	return (((bcd + x) ^ (bcd ^ x)) & 0x11111110) == 0;
}

/**
 * @ingroup BCD
 * @param bcd Unsigned BCD number.
 * @param maximum Unsigned maximum value.
 *
 * Compares an unsigned packed bcd number digit-wise against a maximum
 * value, for example 0x295959. @a maximum can contain digits 0x0
 * ... 0xF.
 *
 * @return
 * @c TRUE if any digit of @a bcd is greater than the
 * corresponding digit of @a maximum.
 *
 * @since 0.2.28
 */
_vbi_inline vbi_bool
vbi_bcd_digits_greater		(unsigned int		bcd,
				 unsigned int		maximum)
{
	maximum ^= ~0;

	return 0 != (((bcd + maximum) ^ bcd ^ maximum) & 0x11111110);
}

/* Private */

#endif /* BCD_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
