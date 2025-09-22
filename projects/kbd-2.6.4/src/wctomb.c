/*
#include	<sys/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<errno.h>
*/

/*
	the our_* routines are implementations for the corresponding library
	routines. for a while, i tried to actually name them wctomb etc
	but stopped that after i found a system which made wchar_t an
	unsigned char.
*/
enum {
	T1 = 0x00,
	Tx = 0x80,
	T2 = 0xC0,
	T3 = 0xE0,
	T4 = 0xF0,
	T5 = 0xF8,
	T6 = 0xFC,

	Bit1 = 7,
	Bitx = 6,
	Bit2 = 5,
	Bit3 = 4,
	Bit4 = 3,
	Bit5 = 2,
	Bit6 = 2,

	Mask1 = (1 << Bit1) - 1,
	Maskx = (1 << Bitx) - 1,
	Mask2 = (1 << Bit2) - 1,
	Mask3 = (1 << Bit3) - 1,
	Mask4 = (1 << Bit4) - 1,
	Mask5 = (1 << Bit5) - 1,
	Mask6 = (1 << Bit6) - 1,

	Wchar1 = (1UL << Bit1) - 1,
	Wchar2 = (1UL << (Bit2 + Bitx)) - 1,
	Wchar3 = (1UL << (Bit3 + 2 * Bitx)) - 1,
	Wchar4 = (1UL << (Bit4 + 3 * Bitx)) - 1,
	Wchar5 = (1UL << (Bit5 + 4 * Bitx)) - 1

#ifndef EILSEQ
	, /* we hate ansi c's comma rules */
	EILSEQ = 123
#endif /* PLAN9 */
};

static int
our_wctomb(char *s, unsigned long wc)
{
	if (s == 0)
		return 0; /* no shift states */
	if (wc & ~(unsigned long)Wchar2) {
		if (wc & ~(unsigned long)Wchar4) {
			if (wc & ~(unsigned long)Wchar5) {
				/* 6 bytes */
				s[0] = T6 | ((wc >> 5 * Bitx) & Mask6);
				s[1] = Tx | ((wc >> 4 * Bitx) & Maskx);
				s[2] = Tx | ((wc >> 3 * Bitx) & Maskx);
				s[3] = Tx | ((wc >> 2 * Bitx) & Maskx);
				s[4] = Tx | ((wc >> 1 * Bitx) & Maskx);
				s[5] = Tx | (wc & Maskx);
				return 6;
			}
			/* 5 bytes */
			s[0] = T5 | (wc >> 4 * Bitx);
			s[1] = Tx | ((wc >> 3 * Bitx) & Maskx);
			s[2] = Tx | ((wc >> 2 * Bitx) & Maskx);
			s[3] = Tx | ((wc >> 1 * Bitx) & Maskx);
			s[4] = Tx | (wc & Maskx);
			return 5;
		}
		if (wc & ~(unsigned long)Wchar3) {
			/* 4 bytes */
			s[0] = T4 | (wc >> 3 * Bitx);
			s[1] = Tx | ((wc >> 2 * Bitx) & Maskx);
			s[2] = Tx | ((wc >> 1 * Bitx) & Maskx);
			s[3] = Tx | (wc & Maskx);
			return 4;
		}
		/* 3 bytes */
		s[0] = T3 | (wc >> 2 * Bitx);
		s[1] = Tx | ((wc >> 1 * Bitx) & Maskx);
		s[2] = Tx | (wc & Maskx);
		return 3;
	}
	if (wc & ~(unsigned long)Wchar1) {
		/* 2 bytes */
		s[0] = T2 | (wc >> 1 * Bitx);
		s[1] = Tx | (wc & Maskx);
		return 2;
	}
	/* 1 byte */
	s[0] = T1 | wc;
	return 1;
}
