/*	$OpenBSD: chrdef.h,v 1.9 2015/03/19 21:22:15 bcallah Exp $	*/

/* This file is in the public domain. */

/*
 * sys/default/chardef.h: character set specific #defines for Mg 2a
 * Warning: System specific ones exist
 */

/*
 * Casting should be at least as efficient as anding with 0xff,
 * and won't have the size problems.
 */
#define	CHARMASK(c)	((unsigned char) (c))

/*
 * These flags, and the macros below them,
 * make up a do-it-yourself set of "ctype" macros that
 * understand the DEC multinational set, and let me ask
 * a slightly different set of questions.
 */
#define _MG_W	0x01		/* Word.			 */
#define _MG_U	0x02		/* Upper case letter.		 */
#define _MG_L	0x04		/* Lower case letter.		 */
#define _MG_C	0x08		/* Control.			 */
#define _MG_P	0x10		/* end of sentence punctuation	 */
#define	_MG_D	0x20		/* is decimal digit		 */

#define ISWORD(c)	((cinfo[CHARMASK(c)]&_MG_W)!=0)
#define ISCTRL(c)	((cinfo[CHARMASK(c)]&_MG_C)!=0)
#define ISUPPER(c)	((cinfo[CHARMASK(c)]&_MG_U)!=0)
#define ISLOWER(c)	((cinfo[CHARMASK(c)]&_MG_L)!=0)
#define ISEOSP(c)	((cinfo[CHARMASK(c)]&_MG_P)!=0)
#define	ISDIGIT(c)	((cinfo[CHARMASK(c)]&_MG_D)!=0)
#define TOUPPER(c)	((c)-0x20)
#define TOLOWER(c)	((c)+0x20)

/*
 * Generally useful thing for chars
 */
#define CCHR(x)		((x) ^ 0x40)	/* CCHR('?') == DEL */

#define	K00		256
#define	K01		257
#define	K02		258
#define	K03		259
#define	K04		260
#define	K05		261
#define	K06		262
#define	K07		263
#define	K08		264
#define	K09		265
#define	K0A		266
#define	K0B		267
#define	K0C		268
#define	K0D		269
#define	K0E		270
#define	K0F		271
#define	K10		272
#define	K11		273
#define	K12		274
#define	K13		275
#define	K14		276
#define	K15		277
#define	K16		278
#define	K17		279
#define	K18		280
#define	K19		281
#define	K1A		282
#define	K1B		283
#define	K1C		284
#define	K1D		285
#define	K1E		286
#define	K1F		287

#define	KFIRST		K00
#define	KLAST		K00
