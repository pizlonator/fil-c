/*	$OpenBSD: cinfo.c,v 1.17 2015/03/17 18:08:52 bcallah Exp $	*/

/* This file is in the public domain. */

/*
 *		Character class tables.
 * Do it yourself character classification
 * macros, that understand the multinational character set,
 * and let me ask some questions the standard macros (in
 * ctype.h) don't let you ask.
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "def.h"

/*
 * This table, indexed by a character drawn
 * from the 256 member character set, is used by my
 * own character type macros to answer questions about the
 * type of a character. It handles the full multinational
 * character set, and lets me ask some questions that the
 * standard "ctype" macros cannot ask.
 */
/*
 * Due to incompatible behaviour between "standard" emacs and
 * ctags word traversing, '_' character's value is changed on 
 * the fly in ctags mode, hence non-const.
 */
char cinfo[256] = {
	_MG_C, _MG_C, _MG_C, _MG_C,				      /* 0x0X */
	_MG_C, _MG_C, _MG_C, _MG_C,
	_MG_C, _MG_C, _MG_C, _MG_C,
	_MG_C, _MG_C, _MG_C, _MG_C,
	_MG_C, _MG_C, _MG_C, _MG_C,				      /* 0x1X */
	_MG_C, _MG_C, _MG_C, _MG_C,
	_MG_C, _MG_C, _MG_C, _MG_C,
	_MG_C, _MG_C, _MG_C, _MG_C,
	0, _MG_P, 0, 0,						      /* 0x2X */
	_MG_W, _MG_W, 0, _MG_W,
	0, 0, 0, 0,
	0, 0, _MG_P, 0,
	_MG_D | _MG_W, _MG_D | _MG_W, _MG_D | _MG_W, _MG_D | _MG_W,   /* 0x3X */
	_MG_D | _MG_W, _MG_D | _MG_W, _MG_D | _MG_W, _MG_D | _MG_W,
	_MG_D | _MG_W, _MG_D | _MG_W, 0, 0,
	0, 0, 0, _MG_P,
	0, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,		      /* 0x4X */
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,   /* 0x5X */
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, 0,
	0, 0, 0, 0,
	0, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,		      /* 0x6X */
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,   /* 0x7X */
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, 0,
	0, 0, 0, _MG_C,
	0, 0, 0, 0,						      /* 0x8X */
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,						      /* 0x9X */
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,						      /* 0xAX */
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,						      /* 0xBX */
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,   /* 0xCX */
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,
	0, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,		      /* 0xDX */
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,
	_MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W, _MG_U | _MG_W,
	_MG_U | _MG_W, _MG_U | _MG_W, 0, _MG_W,
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,   /* 0xEX */
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,
	0, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,		      /* 0xFX */
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,
	_MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W, _MG_L | _MG_W,
	_MG_L | _MG_W, _MG_L | _MG_W, 0, 0
};

/*
 * Find the name of a keystroke.  Needs to be changed to handle 8-bit printing
 * characters and function keys better.	 Returns a pointer to the terminating
 * '\0'.  Returns NULL on failure.
 */
char *
getkeyname(char *cp, size_t len, int k)
{
	const char	*np;
	size_t		 copied;

	if (k < 0)
		k = CHARMASK(k);	/* sign extended char */
	switch (k) {
	case CCHR('@'):
		np = "C-SPC";
		break;
	case CCHR('I'):
		np = "TAB";
		break;
	case CCHR('M'):
		np = "RET";
		break;
	case CCHR('['):
		np = "ESC";
		break;
	case ' ':
		np = "SPC";
		break;		/* yuck again */
	case CCHR('?'):
		np = "DEL";
		break;
	default:
		if (k >= KFIRST && k <= KLAST &&
		    (np = keystrings[k - KFIRST]) != NULL)
			break;
		if (k > CCHR('?')) {
			*cp++ = '0';
			*cp++ = ((k >> 6) & 7) + '0';
			*cp++ = ((k >> 3) & 7) + '0';
			*cp++ = (k & 7) + '0';
			*cp = '\0';
			return (cp);
		} else if (k < ' ') {
			*cp++ = 'C';
			*cp++ = '-';
			k = CCHR(k);
			if (ISUPPER(k))
				k = TOLOWER(k);
		}
		*cp++ = k;
		*cp = '\0';
		return (cp);
	}
	copied = strlcpy(cp, np, len);
	if (copied >= len)
		copied = len - 1;
	return (cp + copied);
}
