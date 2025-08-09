/*
 * tclUtf.c --
 *
 *	Routines for manipulating UTF-8 strings.
 *
 * Copyright (c) 1997-1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * Include the static character classification tables and macros.
 */

#include "tclUniData.c"

/*
 * The following macros are used for fast character category tests. The x_BITS
 * values are shifted right by the category value to determine whether the
 * given category is included in the set.
 */

#define ALPHA_BITS ((1 << UPPERCASE_LETTER) | (1 << LOWERCASE_LETTER) \
	| (1 << TITLECASE_LETTER) | (1 << MODIFIER_LETTER) | (1<<OTHER_LETTER))

#define CONTROL_BITS ((1 << CONTROL) | (1 << FORMAT))

#define DIGIT_BITS (1 << DECIMAL_DIGIT_NUMBER)

#define SPACE_BITS ((1 << SPACE_SEPARATOR) | (1 << LINE_SEPARATOR) \
	| (1 << PARAGRAPH_SEPARATOR))

#define WORD_BITS (ALPHA_BITS | DIGIT_BITS | (1 << CONNECTOR_PUNCTUATION))

#define PUNCT_BITS ((1 << CONNECTOR_PUNCTUATION) | \
	(1 << DASH_PUNCTUATION) | (1 << OPEN_PUNCTUATION) | \
	(1 << CLOSE_PUNCTUATION) | (1 << INITIAL_QUOTE_PUNCTUATION) | \
	(1 << FINAL_QUOTE_PUNCTUATION) | (1 << OTHER_PUNCTUATION))

#define GRAPH_BITS (WORD_BITS | PUNCT_BITS | \
	(1 << NON_SPACING_MARK) | (1 << ENCLOSING_MARK) | \
	(1 << COMBINING_SPACING_MARK) | (1 << LETTER_NUMBER) | \
	(1 << OTHER_NUMBER) | \
	(1 << MATH_SYMBOL) | (1 << CURRENCY_SYMBOL) | \
	(1 << MODIFIER_SYMBOL) | (1 << OTHER_SYMBOL))

/*
 * Unicode characters less than this value are represented by themselves in
 * UTF-8 strings.
 */

#define UNICODE_SELF	0x80

/*
 * The following structures are used when mapping between Unicode (UCS-2) and
 * UTF-8.
 */

static const unsigned char totalBytes[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
#if TCL_UTF_MAX > 3
    4,4,4,4,4,
#else
    1,1,1,1,1,
#endif
    1,1,1,1,1,1,1,1,1,1,1
};

static const unsigned char complete[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* Tcl_UtfCharComplete() might point to 2nd byte of valid 4-byte sequence */
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/* End of "continuation byte section" */
    2,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
#if TCL_UTF_MAX > 3
    4,4,4,4,4,
#else
    /* Tcl_UtfToUniChar() accesses src[1] and src[2] to check whether
     * the UTF-8 sequence is valid, so we cannot use 1 here. */
    3,3,3,3,3,
#endif
    1,1,1,1,1,1,1,1,1,1,1
};

/*
 * Functions used only in this module.
 */

static int		UtfCount(int ch);
static int		Invalid(const char *src);
static int		UCS4ToUpper(int ch);
static int		UCS4ToTitle(int ch);

/*
 *---------------------------------------------------------------------------
 *
 * UtfCount --
 *
 *	Find the number of bytes in the Utf character "ch".
 *
 * Results:
 *	The return values is the number of bytes in the Utf character "ch".
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static inline int
UtfCount(
    int ch)			/* The Unicode character whose size is returned. */
{
    if ((unsigned)(ch - 1) < (UNICODE_SELF - 1)) {
	return 1;
    }
    if (ch <= 0x7FF) {
	return 2;
    }
#if TCL_UTF_MAX > 3
    if (((unsigned)(ch - 0x10000) <= 0xFFFFF)) {
	return 4;
    }
#endif
    return 3;
}

/*
 *---------------------------------------------------------------------------
 *
 * Invalid --
 *
 *	Given a pointer to a two-byte prefix of a well-formed UTF-8 byte
 *	sequence (a lead byte followed by a trail byte) this routine
 *	examines those two bytes to determine whether the sequence is
 *	invalid in UTF-8.  This might be because it is an overlong
 *	encoding, or because it encodes something out of the proper range.
 *
 *	Given a pointer to the bytes \xF8 or \xFC , this routine will
 *	try to read beyond the end of the "bounds" table.  Callers must
 *	prevent this.
 *
 *	Given a pointer to something else (an ASCII byte, a trail byte,
 *	or another byte	that can never begin a valid byte sequence such
 *	as \xF5) this routine returns false.  That makes the routine poorly
 *	named, as it does not detect and report all invalid sequences.
 *
 *	Callers have to take care that this routine does something useful
 *	for their needs.
 *
 * Results:
 *	A boolean.
 *---------------------------------------------------------------------------
 */

static const unsigned char bounds[28] = {
    0x80, 0x80,		/* \xC0 accepts \x80 only */
    0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF,
    0x80, 0xBF,		/* (\xC4 - \xDC) -- all sequences valid */
    0xA0, 0xBF,	/* \xE0\x80 through \xE0\x9F are invalid prefixes */
    0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF, /* (\xE4 - \xEC) -- all valid */
#if TCL_UTF_MAX > 3
    0x90, 0xBF,	/* \xF0\x80 through \xF0\x8F are invalid prefixes */
    0x80, 0x8F  /* \xF4\x90 and higher are invalid prefixes */
#else
    0xC0, 0xBF,	/* Not used, but reject all again for safety. */
    0xC0, 0xBF	/* Not used, but reject all again for safety. */
#endif
};

static int
Invalid(
    const char *src)	/* Points to lead byte of a UTF-8 byte sequence */
{
    unsigned char byte = UCHAR(*src);
    int index;

    if ((byte & 0xC3) == 0xC0) {
	/* Only lead bytes 0xC0, 0xE0, 0xF0, 0xF4 need examination */
	index = (byte - 0xC0) >> 1;
	if (UCHAR(src[1]) < bounds[index] || UCHAR(src[1]) > bounds[index+1]) {
	    /* Out of bounds - report invalid. */
	    return 1;
	}
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UniCharToUtf --
 *
 *	Stores the given Tcl_UniChar as a sequence of UTF-8 bytes in the
 *	provided buffer. Equivalent to Plan 9 runetochar().
 *
 * Results:
 *	Returns the number of bytes stored into the buffer.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_UniCharToUtf(
    int ch,			/* The Tcl_UniChar to be stored in the
				 * buffer. */
    char *buf)			/* Buffer in which the UTF-8 representation of
				 * the Tcl_UniChar is stored. Buffer must be
				 * large enough to hold the UTF-8 character
				 * (at most TCL_UTF_MAX bytes). */
{
    if ((unsigned)(ch - 1) < (UNICODE_SELF - 1)) {
	buf[0] = (char) ch;
	return 1;
    }
    if (ch >= 0) {
	if (ch <= 0x7FF) {
	    buf[1] = (char) (0x80 | (0x3F & ch));
	    buf[0] = (char) (0xC0 | (ch >> 6));
	    return 2;
	}
	if (ch <= 0xFFFF) {
#if TCL_UTF_MAX > 3
	    if ((ch & 0xF800) == 0xD800) {
		if (ch & 0x0400) {
		    /* Low surrogate */
		    if (   (0x80 == (0xC0 & buf[0]))
			&& (0    == (0xCF & buf[1]))) {
			/* Previous Tcl_UniChar was a high surrogate, so combine */
			buf[2]  = (char) (0x80 | (0x3F & ch));
			buf[1] |= (char) (0x80 | (0x0F & (ch >> 6)));
			return 3;
		    }
		    /* Previous Tcl_UniChar was not a high surrogate, so just output */
		} else {
		    /* High surrogate */
		    ch += 0x40;
		    /* Fill buffer with specific 3-byte (invalid) byte combination,
		       so following low surrogate can recognize it and combine */
		    buf[2] = (char) ((ch << 4) & 0x30);
		    buf[1] = (char) (0x80 | (0x3F & (ch >> 2)));
		    buf[0] = (char) (0xF0 | (0x07 & (ch >> 8)));
		    return 1;
		}
	    }
#endif
	    goto three;
	}

#if TCL_UTF_MAX > 3
	if (ch <= 0x10FFFF) {
	    buf[3] = (char) (0x80 | (0x3F & ch));
	    buf[2] = (char) (0x80 | (0x3F & (ch >> 6)));
	    buf[1] = (char) (0x80 | (0x3F & (ch >> 12)));
	    buf[0] = (char) (0xF0 |         (ch >> 18));
	    return 4;
	}
    } else if (ch == -1) {
	if (   (0x80 == (0xC0 & buf[0]))
	    && (0    == (0xCF & buf[1]))
	    && (0xF0 == (0xF8 & buf[-1]))) {
	    ch = 0xD7C0
		+ ((0x07 & buf[-1]) << 8)
		+ ((0x3F & buf[0])  << 2)
		+ ((0x30 & buf[1])  >> 4);
	    buf[1]  = (char) (0x80 | (0x3F & ch));
	    buf[0]  = (char) (0x80 | (0x3F & (ch >> 6)));
	    buf[-1] = (char) (0xE0 | (ch >> 12));
	    return 2;
	}
#endif
    }

    ch = 0xFFFD;
three:
    buf[2] = (char) (0x80 | (0x3F & ch));
    buf[1] = (char) (0x80 | (0x3F & (ch >> 6)));
    buf[0] = (char) (0xE0 |         (ch >> 12));
    return 3;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UniCharToUtfDString --
 *
 *	Convert the given Unicode string to UTF-8.
 *
 * Results:
 *	The return value is a pointer to the UTF-8 representation of the
 *	Unicode string. Storage for the return value is appended to the end of
 *	dsPtr.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

char *
Tcl_UniCharToUtfDString(
    const Tcl_UniChar *uniStr,	/* Unicode string to convert to UTF-8. */
    int uniLength,		/* Length of Unicode string in Tcl_UniChars
				 * (must be >= 0). */
    Tcl_DString *dsPtr)		/* UTF-8 representation of string is appended
				 * to this previously initialized DString. */
{
    const Tcl_UniChar *w, *wEnd;
    char *p, *string;
    int oldLength;

    /*
     * UTF-8 string length in bytes will be <= Unicode string length *
     * TCL_UTF_MAX.
     */

    oldLength = Tcl_DStringLength(dsPtr);
    Tcl_DStringSetLength(dsPtr, oldLength + (uniLength + 1) * TCL_UTF_MAX);
    string = Tcl_DStringValue(dsPtr) + oldLength;

    p = string;
    wEnd = uniStr + uniLength;
    for (w = uniStr; w < wEnd; ) {
	p += Tcl_UniCharToUtf(*w, p);
	w++;
    }
    Tcl_DStringSetLength(dsPtr, oldLength + (p - string));

    return string;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfToUniChar --
 *
 *	Extract the Tcl_UniChar represented by the UTF-8 string. Bad UTF-8
 *	sequences are converted to valid Tcl_UniChars and processing
 *	continues. Equivalent to Plan 9 chartorune().
 *
 *	The caller must ensure that the source buffer is long enough that this
 *	routine does not run off the end and dereference non-existent memory
 *	looking for trail bytes. If the source buffer is known to be '\0'
 *	terminated, this cannot happen. Otherwise, the caller should call
 *	Tcl_UtfCharComplete() before calling this routine to ensure that
 *	enough bytes remain in the string.
 *
 *	If TCL_UTF_MAX <= 4, special handling of Surrogate pairs is done:
 *	For any UTF-8 string containing a character outside of the BMP, the
 *	first call to this function will fill *chPtr with the high surrogate
 *	and generate a return value of 1. Calling Tcl_UtfToUniChar again
 *	will produce the low surrogate and a return value of 3. Because *chPtr
 *	is used to remember whether the high surrogate is already produced, it
 *	is recommended to initialize the variable it points to as 0 before
 *	the first call to Tcl_UtfToUniChar is done.
 *
 * Results:
 *	*chPtr is filled with the Tcl_UniChar, and the return value is the
 *	number of bytes from the UTF-8 string that were consumed.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_UtfToUniChar(
    const char *src,	/* The UTF-8 string. */
    Tcl_UniChar *chPtr)/* Filled with the Tcl_UniChar represented by
				 * the UTF-8 string. */
{
    Tcl_UniChar byte;

    /*
     * Unroll 1 to 3 (or 4) byte UTF-8 sequences.
     */

    byte = UCHAR(*src);
    if (byte < 0xC0) {
	/*
	 * Handles properly formed UTF-8 characters between 0x01 and 0x7F.
	 * Also treats \0 and naked trail bytes 0x80 to 0xBF as valid
	 * characters representing themselves.
	 */

#if TCL_UTF_MAX <= 4
	/* If *chPtr contains a high surrogate (produced by a previous
	 * Tcl_UtfToUniChar() call) and the next 3 bytes are UTF-8 continuation
	 * bytes, then we must produce a follow-up low surrogate. We only
	 * do that if the high surrogate matches the bits we encounter.
	 */
	if (((byte & 0xC0) == 0x80)
		&& ((src[1] & 0xC0) == 0x80) && ((src[2] & 0xC0) == 0x80)
		&& (((((byte - 0x10) << 2) & 0xFC) | 0xD800) == (*chPtr & 0xFCFC))
		&& ((src[1] & 0xF0) == (((*chPtr << 4) & 0x30) | 0x80))) {
	    *chPtr = ((src[1] & 0x0F) << 6) + (src[2] & 0x3F) + 0xDC00;
	    return 3;
	}
#endif
	*chPtr = byte;
	return 1;
    } else if (byte < 0xE0) {
	if ((byte != 0xC1) && ((src[1] & 0xC0) == 0x80)) {
	    /*
	     * Two-byte-character lead-byte followed by a trail-byte.
	     */

	    *chPtr = (((byte & 0x1F) << 6) | (src[1] & 0x3F));
	    if ((unsigned)(*chPtr - 1) >= (UNICODE_SELF - 1)) {
		return 2;
	    }
	}

	/*
	 * A two-byte-character lead-byte not followed by trail-byte
	 * represents itself.
	 */
    } else if (byte < 0xF0) {
	if (((src[1] & 0xC0) == 0x80) && ((src[2] & 0xC0) == 0x80)) {
	    /*
	     * Three-byte-character lead byte followed by two trail bytes.
	     */

	    *chPtr = (((byte & 0x0F) << 12)
		    | ((src[1] & 0x3F) << 6) | (src[2] & 0x3F));
	    if (*chPtr > 0x7FF) {
		return 3;
	    }
	}

	/*
	 * A three-byte-character lead-byte not followed by two trail-bytes
	 * represents itself.
	 */
    } else if (byte < 0xF5) {
	if (((src[1] & 0xC0) == 0x80) && ((src[2] & 0xC0) == 0x80)) {
	    /*
	     * Four-byte-character lead byte followed by at least two trail bytes.
	     * We don't test the validity of 3th trail byte, see [ed29806ba]
	     */
#if TCL_UTF_MAX <= 4
	    Tcl_UniChar high = (((byte & 0x07) << 8) | ((src[1] & 0x3F) << 2)
		    | ((src[2] & 0x3F) >> 4)) - 0x40;
	    if (high < 0x400) {
		/* produce high surrogate, advance source pointer */
		*chPtr = 0xD800 + high;
		return 1;
	    }
	    /* out of range, < 0x10000 or > 0x10FFFF */
#else
	    if ((src[3] & 0xC0) == 0x80) {
		*chPtr = (((byte & 0x07) << 18) | ((src[1] & 0x3F) << 12)
			| ((src[2] & 0x3F) << 6) | (src[3] & 0x3F));
		if ((unsigned)(*chPtr - 0x10000) <= 0xFFFFF) {
		    return 4;
		}
	    }
#endif
	}

	/*
	 * A four-byte-character lead-byte not followed by three trail-bytes
	 * represents itself.
	 */
    }

    *chPtr = byte;
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfToUniCharDString --
 *
 *	Convert the UTF-8 string to Unicode.
 *
 * Results:
 *	The return value is a pointer to the Unicode representation of the
 *	UTF-8 string. Storage for the return value is appended to the end of
 *	dsPtr. The Unicode string is terminated with a Unicode NULL character.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

Tcl_UniChar *
Tcl_UtfToUniCharDString(
    const char *src,		/* UTF-8 string to convert to Unicode. */
    int length,			/* Length of UTF-8 string in bytes, or -1 for
				 * strlen(). */
    Tcl_DString *dsPtr)		/* Unicode representation of string is
				 * appended to this previously initialized
				 * DString. */
{
    Tcl_UniChar ch = 0, *w, *wString;
    const char *p;
    int oldLength;
    /* Pointer to the end of string. Never read endPtr[0] */
    const char *endPtr = src + length;
    /* Pointer to last byte where optimization still can be used */
    const char *optPtr = endPtr - TCL_UTF_MAX;

    if (length < 0) {
	length = strlen(src);
    }

    /*
     * Unicode string length in Tcl_UniChars will be <= UTF-8 string length in
     * bytes.
     */

    oldLength = Tcl_DStringLength(dsPtr);

    Tcl_DStringSetLength(dsPtr,
	    oldLength + ((length + 1) * sizeof(Tcl_UniChar)));
    wString = (Tcl_UniChar *) (Tcl_DStringValue(dsPtr) + oldLength);

    w = wString;
    p = src;
    endPtr = src + length;
    optPtr = endPtr - ((TCL_UTF_MAX > 3) ? 4 : 3) ;
    while (p <= optPtr) {
	p += TclUtfToUniChar(p, &ch);
	*w++ = ch;
    }
    while (p < endPtr) {
	if (Tcl_UtfCharComplete(p, endPtr-p)) {
	    p += TclUtfToUniChar(p, &ch);
	    *w++ = ch;
	} else {
	    *w++ = UCHAR(*p++);
	}
    }
    *w = '\0';
    Tcl_DStringSetLength(dsPtr,
	    oldLength + ((char *) w - (char *) wString));

    return wString;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfCharComplete --
 *
 *	Determine if the UTF-8 string of the given length is long enough to be
 *	decoded by Tcl_UtfToUniChar(). This does not ensure that the UTF-8
 *	string is properly formed. Equivalent to Plan 9 fullrune().
 *
 * Results:
 *	The return value is 0 if the string is not long enough, non-zero
 *	otherwise.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_UtfCharComplete(
    const char *src,		/* String to check if first few bytes contain
				 * a complete UTF-8 character. */
    int length)			/* Length of above string in bytes. */
{
    return length >= complete[UCHAR(*src)];
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_NumUtfChars --
 *
 *	Returns the number of characters (not bytes) in the UTF-8 string, not
 *	including the terminating NULL byte. This is equivalent to Plan 9
 *	utflen() and utfnlen().
 *
 * Results:
 *	As above.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_NumUtfChars(
    const char *src,	/* The UTF-8 string to measure. */
    int length)		/* The length of the string in bytes, or -1
			 * for strlen(string). */
{
    Tcl_UniChar ch = 0;
    int i = 0;

    if (length < 0) {
	/* string is NUL-terminated, so TclUtfToUniChar calls are safe. */
	while ((*src != '\0') && (i < INT_MAX)) {
	    src += TclUtfToUniChar(src, &ch);
	    i++;
	}
    } else {
	/* Will return value between 0 and length. No overflow checks. */

	/* Pointer to the end of string. Never read endPtr[0] */
	const char *endPtr = src + length;
	/* Pointer to last byte where optimization still can be used */
	const char *optPtr = endPtr - ((TCL_UTF_MAX > 3) ? 4 : 3);

	/*
	 * Optimize away the call in this loop. Justified because...
	 * when (src <= optPtr), (endPtr - src) >= (endPtr - optPtr)
	 * By initialization above (endPtr - optPtr) = TCL_UTF_MAX
	 * So (endPtr - src) >= TCL_UTF_MAX, and passing that to
	 * Tcl_UtfCharComplete we know will cause return of 1.
	 */
	while (src <= optPtr
		/* && Tcl_UtfCharComplete(src, endPtr - src) */ ) {
	    src += TclUtfToUniChar(src, &ch);
	    i++;
	}
	/* Loop over the remaining string where call must happen */
	while (src < endPtr) {
	    if (Tcl_UtfCharComplete(src, endPtr - src)) {
		src += TclUtfToUniChar(src, &ch);
	    } else {
		/*
		 * src points to incomplete UTF-8 sequence
		 * Treat first byte as character and count it
		 */
		src++;
	    }
	    i++;
	}
    }
    return i;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfFindFirst --
 *
 *	Returns a pointer to the first occurrence of the given Unicode character
 *	in the NULL-terminated UTF-8 string. The NULL terminator is considered
 *	part of the UTF-8 string. Equivalent to Plan 9 utfrune().
 *
 * Results:
 *	As above. If the Unicode character does not exist in the given string,
 *	the return value is NULL.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

const char *
Tcl_UtfFindFirst(
    const char *src,		/* The UTF-8 string to be searched. */
    int ch)			/* The Unicode character to search for. */
{
    while (1) {
	int find, len = TclUtfToUCS4(src, &find);

	if (find == ch) {
	    return src;
	}
	if (*src == '\0') {
	    return NULL;
	}
	src += len;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfFindLast --
 *
 *	Returns a pointer to the last occurrence of the given Unicode character
 *	in the NULL-terminated UTF-8 string. The NULL terminator is considered
 *	part of the UTF-8 string. Equivalent to Plan 9 utfrrune().
 *
 * Results:
 *	As above. If the Unicode character does not exist in the given string, the
 *	return value is NULL.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

const char *
Tcl_UtfFindLast(
    const char *src,		/* The UTF-8 string to be searched. */
    int ch)			/* The Unicode character to search for. */
{
    const char *last = NULL;

    while (1) {
	int find, len = TclUtfToUCS4(src, &find);

	if (find == ch) {
	    last = src;
	}
	if (*src == '\0') {
	    break;
	}
	src += len;
    }
    return last;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfNext --
 *
 *	Given a pointer to some location in a UTF-8 string, Tcl_UtfNext
 *	returns a pointer to the next UTF-8 character in the string.
 *	The caller must not ask for the next character after the last
 *	character in the string if the string is not terminated by a null
 *	character.
 *
 * Results:
 *	The return value is the pointer to the next character in the UTF-8
 *	string.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

const char *
Tcl_UtfNext(
    const char *src)		/* The current location in the string. */
{
    int left;
    const char *next;

#if TCL_UTF_MAX > 3
    if (((*src) & 0xC0) == 0x80) {
	/* Continuation byte, so we start 'inside' a (possible valid) UTF-8
	 * sequence. Since we are not allowed to access src[-1], we cannot
	 * check if the sequence is actually valid, the best we can do is
	 * just assume it is valid and locate the end. */
	if ((((*++src) & 0xC0) == 0x80) && (((*++src) & 0xC0) == 0x80)) {
	    ++src;
	}
	return src;
    }
#endif

    left = totalBytes[UCHAR(*src)];
    next = src + 1;
    while (--left) {
	if ((*next & 0xC0) != 0x80) {
	    /*
	     * src points to non-trail byte; We ran out of trail bytes
	     * before the needs of the lead byte were satisfied.
	     * Let the (malformed) lead byte alone be a character
	     */
	    return src + 1;
	}
	next++;
    }
    /*
     * Call Invalid() here only if required conditions are met:
     *    src[0] is known a lead byte.
     *    src[1] is known a trail byte.
     * Especially important to prevent calls when src[0] == '\xF8' or '\xFC'
     * See tests utf-6.37 through utf-6.43 through valgrind or similar tool.
     */
    if ((next == src + 1) || Invalid(src)) {
	return src + 1;
    }
    return next;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfPrev --
 *
 *	Given a pointer to some current location in a UTF-8 string, move
 *	backwards one character. This works correctly when the pointer is in
 *	the middle of a UTF-8 character.
 *
 * Results:
 *	The return value is a pointer to the previous character in the UTF-8
 *	string. If the current location was already at the beginning of the
 *	string, the return value will also be a pointer to the beginning of
 *	the string.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

const char *
Tcl_UtfPrev(
    const char *src,		/* A location in a UTF-8 string. */
    const char *start)		/* Pointer to the beginning of the string */
{
    int trailBytesSeen = 0;	/* How many trail bytes have been verified? */
    const char *fallback = src - 1;
				/* If we cannot find a lead byte that might
				 * start a prefix of a valid UTF byte sequence,
				 * we will fallback to a one-byte back step */
    const char *look = fallback;
				/* Start search at the fallback position */

    /* Quick boundary case exit. */
    if (fallback <= start) {
	return start;
    }

    do {
	unsigned char byte = UCHAR(look[0]);

	if (byte < 0x80) {
	    /*
	     * Single byte character. Either this is a correct previous
	     * character, or it is followed by at least one trail byte
	     * which indicates a malformed sequence. In either case the
	     * correct result is to return the fallback.
	     */
	    return fallback;
	}
	if (byte >= 0xC0) {
	    /* Non-trail byte; May be multibyte lead. */

	    if ((trailBytesSeen == 0)
		/*
		 * We've seen no trailing context to use to check
		 * anything. From what we know, this non-trail byte
		 * is a prefix of a previous character, and accepting
		 * it (the fallback) is correct.
		 */

		    || (trailBytesSeen >= totalBytes[byte])) {
		/*
		 * That is, (1 + trailBytesSeen > needed).
		 * We've examined more bytes than needed to complete
		 * this lead byte. No matter about well-formedness or
		 * validity, the sequence starting with this lead byte
		 * will never include the fallback location, so we must
		 * return the fallback location. See test utf-7.17
		 */
		return fallback;
	    }

	    /*
	     * trailBytesSeen > 0, so we can examine look[1] safely.
	     * Use that capability to screen out invalid sequences.
	     */

	    if (Invalid(look)) {
		/* Reject */
		return fallback;
	    }
	    return (const char *)look;
	}

	/* We saw a trail byte. */
	trailBytesSeen++;

	if ((const char *)look == start) {
	    /*
	     * Do not read before the start of the string
	     *
	     * If we get here, we've examined bytes at every location
	     * >= start and < src and all of them are trail bytes,
	     * including (*start).  We need to return our fallback
	     * and exit this loop before we run past the start of the string.
	     */
	    return fallback;
	}

	/* Continue the search backwards... */
	look--;
    } while (trailBytesSeen < (TCL_UTF_MAX < 4 ? 3 : 4));

    /*
     * We've seen 3 trail bytes, so we know there will not be a
     * properly formed byte sequence to find, and we can stop looking,
     * accepting the fallback.
     */
    return fallback;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UniCharAtIndex --
 *
 *	Returns the Tcl_UniChar represented at the specified character
 *	(not byte) position in the UTF-8 string.
 *
 * Results:
 *	As above.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

Tcl_UniChar
Tcl_UniCharAtIndex(
    const char *src,	/* The UTF-8 string to dereference. */
    int index)		/* The position of the desired character. */
{
    Tcl_UniChar ch = 0;

    while (index-- >= 0) {
	src += TclUtfToUniChar(src, &ch);
    }
    return ch;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfAtIndex --
 *
 *	Returns a pointer to the specified character (not byte) position in
 *	the UTF-8 string.
 *
 * Results:
 *	As above.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

const char *
Tcl_UtfAtIndex(
    const char *src,	/* The UTF-8 string. */
    int index)		/* The position of the desired character. */
{
    Tcl_UniChar ch = 0;
    int len = 0;

    while (index-- > 0) {
	len = TclUtfToUniChar(src, &ch);
	src += len;
    }
#if TCL_UTF_MAX == 4
    if ((ch >= 0xD800) && (len < 3)) {
	/* Index points at character following high Surrogate */
	src += TclUtfToUniChar(src, &ch);
    }
#endif
    return src;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfBackslash --
 *
 *	Figure out how to handle a backslash sequence.
 *
 * Results:
 *	Stores the bytes represented by the backslash sequence in dst and
 *	returns the number of bytes written to dst. At most TCL_UTF_MAX bytes
 *	are written to dst; dst must have been large enough to accept those
 *	bytes. If readPtr isn't NULL then it is filled in with a count of the
 *	number of bytes in the backslash sequence.
 *
 * Side effects:
 *	The maximum number of bytes it takes to represent a Unicode character
 *	in UTF-8 is guaranteed to be less than the number of bytes used to
 *	express the backslash sequence that represents that Unicode character.
 *	If the target buffer into which the caller is going to store the bytes
 *	that represent the Unicode character is at least as large as the
 *	source buffer from which the backslashed sequence was extracted, no
 *	buffer overruns should occur.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_UtfBackslash(
    const char *src,		/* Points to the backslash character of a
				 * backslash sequence. */
    int *readPtr,		/* Fill in with number of characters read from
				 * src, unless NULL. */
    char *dst)			/* Filled with the bytes represented by the
				 * backslash sequence. */
{
#define LINE_LENGTH 128
    int numRead;
    int result;

    result = TclParseBackslash(src, LINE_LENGTH, &numRead, dst);
    if (numRead == LINE_LENGTH) {
	/*
	 * We ate a whole line. Pay the price of a strlen()
	 */

	result = TclParseBackslash(src, strlen(src), &numRead, dst);
    }
    if (readPtr != NULL) {
	*readPtr = numRead;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UtfToUpper --
 *
 *	Convert lowercase characters to uppercase characters in a UTF string
 *	in place. The conversion may shrink the UTF string.
 *
 * Results:
 *	Returns the number of bytes in the resulting string excluding the
 *	trailing null.
 *
 * Side effects:
 *	Writes a terminating null after the last converted character.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UtfToUpper(
    char *str)			/* String to convert in place. */
{
    int ch, upChar;
    char *src, *dst;
    int len;

    /*
     * Iterate over the string until we hit the terminating null.
     */

    src = dst = str;
    while (*src) {
	len = TclUtfToUCS4(src, &ch);
	upChar = UCS4ToUpper(ch);

	/*
	 * To keep badly formed Utf strings from getting inflated by the
	 * conversion (thereby causing a segfault), only copy the upper case
	 * char to dst if its size is <= the original char.
	 */

	if (len < UtfCount(upChar)) {
	    memmove(dst, src, len);
	    dst += len;
	} else {
	    dst += TclUCS4ToUtf(upChar, dst);
	}
	src += len;
    }
    *dst = '\0';
    return (dst - str);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UtfToLower --
 *
 *	Convert uppercase characters to lowercase characters in a UTF string
 *	in place. The conversion may shrink the UTF string.
 *
 * Results:
 *	Returns the number of bytes in the resulting string excluding the
 *	trailing null.
 *
 * Side effects:
 *	Writes a terminating null after the last converted character.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UtfToLower(
    char *str)			/* String to convert in place. */
{
    int ch, lowChar;
    char *src, *dst;
    int len;

    /*
     * Iterate over the string until we hit the terminating null.
     */

    src = dst = str;
    while (*src) {
	len = TclUtfToUCS4(src, &ch);
	lowChar = TclUCS4ToLower(ch);

	/*
	 * To keep badly formed Utf strings from getting inflated by the
	 * conversion (thereby causing a segfault), only copy the lower case
	 * char to dst if its size is <= the original char.
	 */

	if (len < UtfCount(lowChar)) {
	    memmove(dst, src, len);
	    dst += len;
	} else {
	    dst += TclUCS4ToUtf(lowChar, dst);
	}
	src += len;
    }
    *dst = '\0';
    return (dst - str);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UtfToTitle --
 *
 *	Changes the first character of a UTF string to title case or uppercase
 *	and the rest of the string to lowercase. The conversion happens in
 *	place and may shrink the UTF string.
 *
 * Results:
 *	Returns the number of bytes in the resulting string excluding the
 *	trailing null.
 *
 * Side effects:
 *	Writes a terminating null after the last converted character.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UtfToTitle(
    char *str)			/* String to convert in place. */
{
    int ch, titleChar, lowChar;
    char *src, *dst;
    int len;

    /*
     * Capitalize the first character and then lowercase the rest of the
     * characters until we get to a null.
     */

    src = dst = str;

    if (*src) {
	len = TclUtfToUCS4(src, &ch);
	titleChar = UCS4ToTitle(ch);

	if (len < UtfCount(titleChar)) {
	    memmove(dst, src, len);
	    dst += len;
	} else {
	    dst += TclUCS4ToUtf(titleChar, dst);
	}
	src += len;
    }
    while (*src) {
	len = TclUtfToUCS4(src, &ch);
	lowChar = ch;
	/* Special exception for Georgian Asomtavruli chars, no titlecase. */
	if ((unsigned)(lowChar - 0x1C90) >= 0x30) {
	    lowChar = TclUCS4ToLower(lowChar);
	}

	if (len < UtfCount(lowChar)) {
	    memmove(dst, src, len);
	    dst += len;
	} else {
	    dst += TclUCS4ToUtf(lowChar, dst);
	}
	src += len;
    }
    *dst = '\0';
    return (dst - str);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpUtfNcmp2 --
 *
 *	Compare at most numBytes bytes of utf-8 strings cs and ct. Both cs and
 *	ct are assumed to be at least numBytes bytes long.
 *
 * Results:
 *	Return <0 if cs < ct, 0 if cs == ct, or >0 if cs > ct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclpUtfNcmp2(
    const char *cs,		/* UTF string to compare to ct. */
    const char *ct,		/* UTF string cs is compared to. */
    unsigned long numBytes)	/* Number of *bytes* to compare. */
{
	return TclUtfNcmp2(cs, ct, numBytes);
}

int
TclUtfNcmp2(
    const void *csPtr,		/* UTF string to compare to ct. */
    const void *ctPtr,		/* UTF string cs is compared to. */
    size_t numBytes)	/* Number of *bytes* to compare. */
{
    const char *cs = (const char *)csPtr;
    const char *ct = (const char *)ctPtr;
    /*
     * We can't simply call 'memcmp(cs, ct, numBytes);' because we need to
     * check for Tcl's \xC0\x80 non-utf-8 null encoding. Otherwise utf-8 lexes
     * fine in the strcmp manner.
     */

    int result = 0;

    for ( ; numBytes != 0; numBytes--, cs++, ct++) {
	if (*cs != *ct) {
	    result = UCHAR(*cs) - UCHAR(*ct);
	    break;
	}
    }
    if (numBytes && ((UCHAR(*cs) == 0xC0) || (UCHAR(*ct) == 0xC0))) {
	unsigned char c1, c2;

	c1 = ((UCHAR(*cs) == 0xC0) && (UCHAR(cs[1]) == 0x80)) ? 0 : UCHAR(*cs);
	c2 = ((UCHAR(*ct) == 0xC0) && (UCHAR(ct[1]) == 0x80)) ? 0 : UCHAR(*ct);
	result = (c1 - c2);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UtfNcmp --
 *
 *	Compare at most numChars UTF chars of string cs to string ct. Both cs
 *	and ct are assumed to be at least numChars UTF chars long.
 *
 * Results:
 *	Return <0 if cs < ct, 0 if cs == ct, or >0 if cs > ct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UtfNcmp(
    const char *cs,		/* UTF string to compare to ct. */
    const char *ct,		/* UTF string cs is compared to. */
    unsigned long numChars)	/* Number of UTF chars to compare. */
{
    return TclUtfNcmp(cs, ct, numChars);
}

int
TclUtfNcmp(
    const void *csPtr,		/* UTF string to compare to ct. */
    const void *ctPtr,		/* UTF string cs is compared to. */
    size_t numChars)	/* Number of UTF chars to compare. */
{
    const char *cs = (const char *)csPtr;
    const char *ct = (const char *)ctPtr;
    Tcl_UniChar ch1 = 0, ch2 = 0;

    /*
     * Cannot use 'memcmp(cs, ct, n);' as byte representation of \u0000 (the
     * pair of bytes 0xC0,0x80) is larger than byte representation of \u0001
     * (the byte 0x01.)
     */

    while (numChars-- > 0) {
	/*
	 * n must be interpreted as chars, not bytes. This should be called
	 * only when both strings are of at least n chars long (no need for \0
	 * check)
	 */

	cs += TclUtfToUniChar(cs, &ch1);
	ct += TclUtfToUniChar(ct, &ch2);
	if (ch1 != ch2) {
#if TCL_UTF_MAX == 4
	    /* Surrogates always report higher than non-surrogates */
	    if (((ch1 & 0xFC00) == 0xD800)) {
	    if ((ch2 & 0xFC00) != 0xD800) {
		return ch1;
	    }
	    } else if ((ch2 & 0xFC00) == 0xD800) {
		return -ch2;
	    }
#endif
	    return (ch1 - ch2);
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UtfNcasecmp --
 *
 *	Compare at most numChars UTF chars of string cs to string ct case
 *	insensitive. Both cs and ct are assumed to be at least numChars UTF
 *	chars long.
 *
 * Results:
 *	Return <0 if cs < ct, 0 if cs == ct, or >0 if cs > ct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UtfNcasecmp(
    const char *cs,		/* UTF string to compare to ct. */
    const char *ct,		/* UTF string cs is compared to. */
    unsigned long numChars)	/* Number of UTF chars to compare. */
{
    return TclUtfNcasecmp(cs, ct, numChars);
}

int
TclUtfNcasecmp(
    const void *csPtr,		/* UTF string to compare to ct. */
    const void *ctPtr,		/* UTF string cs is compared to. */
    size_t numChars)	/* Number of UTF chars to compare. */
{
    const char *cs = (const char *)csPtr;
    const char *ct = (const char *)ctPtr;
    Tcl_UniChar ch1 = 0, ch2 = 0;

    while (numChars-- > 0) {
	/*
	 * n must be interpreted as chars, not bytes.
	 * This should be called only when both strings are of
	 * at least n chars long (no need for \0 check)
	 */
	cs += TclUtfToUniChar(cs, &ch1);
	ct += TclUtfToUniChar(ct, &ch2);
	if (ch1 != ch2) {
#if TCL_UTF_MAX == 4
	    /* Surrogates always report higher than non-surrogates */
	    if (((ch1 & 0xFC00) == 0xD800)) {
	    if ((ch2 & 0xFC00) != 0xD800) {
		return ch1;
	    }
	    } else if ((ch2 & 0xFC00) == 0xD800) {
		return -ch2;
	    }
#endif
	    ch1 = Tcl_UniCharToLower(ch1);
	    ch2 = Tcl_UniCharToLower(ch2);
	    if (ch1 != ch2) {
		return (ch1 - ch2);
	    }
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclUtfCasecmp --
 *
 *	Compare UTF chars of string cs to string ct case insensitively.
 *	Replacement for strcasecmp in Tcl core, in places where UTF-8 should
 *	be handled.
 *
 * Results:
 *	Return <0 if cs < ct, 0 if cs == ct, or >0 if cs > ct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUtfCasecmp(
    const char *cs,		/* UTF string to compare to ct. */
    const char *ct)		/* UTF string cs is compared to. */
{
    Tcl_UniChar ch1 = 0, ch2 = 0;

    while (*cs && *ct) {
	cs += TclUtfToUniChar(cs, &ch1);
	ct += TclUtfToUniChar(ct, &ch2);
	if (ch1 != ch2) {
#if TCL_UTF_MAX == 4
	    /* Surrogates always report higher than non-surrogates */
	    if (((ch1 & 0xFC00) == 0xD800)) {
	    if ((ch2 & 0xFC00) != 0xD800) {
		return ch1;
	    }
	    } else if ((ch2 & 0xFC00) == 0xD800) {
		return -ch2;
	    }
#endif
	    ch1 = Tcl_UniCharToLower(ch1);
	    ch2 = Tcl_UniCharToLower(ch2);
	    if (ch1 != ch2) {
		return ch1 - ch2;
	    }
	}
    }
    return UCHAR(*cs) - UCHAR(*ct);
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharToUpper --
 *
 *	Compute the uppercase equivalent of the given Unicode character.
 *
 * Results:
 *	Returns the uppercase Unicode character.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
UCS4ToUpper(
    int ch)			/* Unicode character to convert. */
{
    if (!UNICODE_OUT_OF_RANGE(ch)) {
	int info = GetUniCharInfo(ch);

	if (GetCaseType(info) & 0x04) {
	    ch -= GetDelta(info);
	}
    }
    /* Clear away extension bits, if any */
    return ch & 0x1FFFFF;
}

Tcl_UniChar
Tcl_UniCharToUpper(
    int ch)			/* Unicode character to convert. */
{
    return (Tcl_UniChar) UCS4ToUpper(ch);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharToLower --
 *
 *	Compute the lowercase equivalent of the given Unicode character.
 *
 * Results:
 *	Returns the lowercase Unicode character.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUCS4ToLower(
    int ch)			/* Unicode character to convert. */
{
    if (!UNICODE_OUT_OF_RANGE(ch)) {
	int info = GetUniCharInfo(ch);
	int mode = GetCaseType(info);

	if ((mode & 0x02) && (mode != 0x7)) {
	    ch += GetDelta(info);
	}
    }
    /* Clear away extension bits, if any */
    return ch & 0x1FFFFF;
}

Tcl_UniChar
Tcl_UniCharToLower(
    int ch)			/* Unicode character to convert. */
{
    return (Tcl_UniChar) TclUCS4ToLower(ch);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharToTitle --
 *
 *	Compute the titlecase equivalent of the given Unicode character.
 *
 * Results:
 *	Returns the titlecase Unicode character.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
UCS4ToTitle(
    int ch)			/* Unicode character to convert. */
{
    if (!UNICODE_OUT_OF_RANGE(ch)) {
	int info = GetUniCharInfo(ch);
	int mode = GetCaseType(info);

	if (mode & 0x1) {
	    /*
	     * Subtract or add one depending on the original case.
	     */

	    if (mode != 0x7) {
		ch += ((mode & 0x4) ? -1 : 1);
	    }
	} else if (mode == 0x4) {
	    ch -= GetDelta(info);
	}
    }
    /* Clear away extension bits, if any */
    return ch & 0x1FFFFF;
}

Tcl_UniChar
Tcl_UniCharToTitle(
    int ch)			/* Unicode character to convert. */
{
    return (Tcl_UniChar) UCS4ToTitle(ch);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharLen --
 *
 *	Find the length of a UniChar string. The str input must be null
 *	terminated.
 *
 * Results:
 *	Returns the length of str in UniChars (not bytes).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharLen(
    const Tcl_UniChar *uniStr)	/* Unicode string to find length of. */
{
    int len = 0;

    while (*uniStr != '\0') {
	len++;
	uniStr++;
    }
    return len;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharNcmp --
 *
 *	Compare at most numChars unichars of string ucs to string uct.
 *	Both ucs and uct are assumed to be at least numChars unichars long.
 *
 * Results:
 *	Return <0 if ucs < uct, 0 if ucs == uct, or >0 if ucs > uct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#undef TclUniCharNcmp
int
Tcl_UniCharNcmp(
    const Tcl_UniChar *ucs,	/* Unicode string to compare to uct. */
    const Tcl_UniChar *uct,	/* Unicode string ucs is compared to. */
    unsigned long numChars)	/* Number of unichars to compare. */
{
    return TclUniCharNcmp(ucs, uct, numChars);
}

int
TclUniCharNcmp(
    const void *ucsPtr,	/* Unicode string to compare to uct. */
    const void *uctPtr,	/* Unicode string ucs is compared to. */
    size_t numChars)	/* Number of unichars to compare. */
{
    const Tcl_UniChar *ucs = (const Tcl_UniChar *)ucsPtr;
    const Tcl_UniChar *uct = (const Tcl_UniChar *)uctPtr;
#if defined(WORDS_BIGENDIAN) && (TCL_UTF_MAX != 4)
    /*
     * We are definitely on a big-endian machine; memcmp() is safe
     */

    return memcmp(ucs, uct, numChars*sizeof(Tcl_UniChar));

#else /* !WORDS_BIGENDIAN */
    /*
     * We can't simply call memcmp() because that is not lexically correct.
     */

    for ( ; numChars != 0; ucs++, uct++, numChars--) {
	if (*ucs != *uct) {
#if TCL_UTF_MAX == 4
	    /* special case for handling upper surrogates */
	    if (((*ucs & 0xFC00) == 0xD800) && ((*uct & 0xFC00) != 0xD800)) {
		return 1;
	    } else if (((*uct & 0xFC00) == 0xD800)) {
		return -1;
	    }
#endif
	    return (*ucs - *uct);
	}
    }
    return 0;
#endif /* WORDS_BIGENDIAN */
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharNcasecmp --
 *
 *	Compare at most numChars unichars of string ucs to string uct case
 *	insensitive. Both ucs and uct are assumed to be at least numChars
 *	unichars long.
 *
 * Results:
 *	Return <0 if ucs < uct, 0 if ucs == uct, or >0 if ucs > uct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharNcasecmp(
    const Tcl_UniChar *ucs,	/* Unicode string to compare to uct. */
    const Tcl_UniChar *uct,	/* Unicode string ucs is compared to. */
    unsigned long numChars)	/* Number of Unichars to compare. */
{
    return TclUniCharNcasecmp(ucs, uct, numChars);
}

int
TclUniCharNcasecmp(
    const void *ucsPtr,	/* Unicode string to compare to uct. */
    const void *uctPtr,	/* Unicode string ucs is compared to. */
    size_t numChars)	/* Number of Unichars to compare. */
{
    const Tcl_UniChar *ucs = (const Tcl_UniChar *)ucsPtr;
    const Tcl_UniChar *uct = (const Tcl_UniChar *)uctPtr;

    for ( ; numChars != 0; numChars--, ucs++, uct++) {
	if (*ucs != *uct) {
	    Tcl_UniChar lcs = Tcl_UniCharToLower(*ucs);
	    Tcl_UniChar lct = Tcl_UniCharToLower(*uct);

	    if (lcs != lct) {
#if TCL_UTF_MAX == 4
	    /* special case for handling upper surrogates */
	    if (((lcs & 0xFC00) == 0xD800) && ((lct & 0xFC00) != 0xD800)) {
		return 1;
	    } else if (((lct & 0xFC00) == 0xD800)) {
		return -1;
	    }
#endif
		return (lcs - lct);
	    }
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsAlnum --
 *
 *	Test if a character is an alphanumeric Unicode character.
 *
 * Results:
 *	Returns 1 if character is alphanumeric.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsAlnum(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    if (UNICODE_OUT_OF_RANGE(ch)) {
	return 0;
    }
#endif
    return (((ALPHA_BITS | DIGIT_BITS) >> GetCategory(ch)) & 1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsAlpha --
 *
 *	Test if a character is an alphabetic Unicode character.
 *
 * Results:
 *	Returns 1 if character is alphabetic.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsAlpha(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    if (UNICODE_OUT_OF_RANGE(ch)) {
	return 0;
    }
#endif
    return ((ALPHA_BITS >> GetCategory(ch)) & 1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsControl --
 *
 *	Test if a character is a Unicode control character.
 *
 * Results:
 *	Returns non-zero if character is a control.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsControl(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    if (UNICODE_OUT_OF_RANGE(ch)) {
	/* Clear away extension bits, if any */
	ch &= 0x1FFFFF;
	return ((ch == 0xE0001) || ((unsigned)(ch - 0xE0020) <= 0x5F));
    }
#endif
    return ((CONTROL_BITS >> GetCategory(ch)) & 1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsDigit --
 *
 *	Test if a character is a numeric Unicode character.
 *
 * Results:
 *	Returns non-zero if character is a digit.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsDigit(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    if (UNICODE_OUT_OF_RANGE(ch)) {
	return 0;
    }
#endif
    return (GetCategory(ch) == DECIMAL_DIGIT_NUMBER);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsGraph --
 *
 *	Test if a character is any Unicode print character except space.
 *
 * Results:
 *	Returns non-zero if character is printable, but not space.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsGraph(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    if (UNICODE_OUT_OF_RANGE(ch)) {
	return ((unsigned)((ch & 0x1FFFFF) - 0xE0100) <= 0xEF);
    }
#endif
    return ((GRAPH_BITS >> GetCategory(ch)) & 1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsLower --
 *
 *	Test if a character is a lowercase Unicode character.
 *
 * Results:
 *	Returns non-zero if character is lowercase.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsLower(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    if (UNICODE_OUT_OF_RANGE(ch)) {
	return 0;
    }
#endif
    return (GetCategory(ch) == LOWERCASE_LETTER);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsPrint --
 *
 *	Test if a character is a Unicode print character.
 *
 * Results:
 *	Returns non-zero if character is printable.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsPrint(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    if (UNICODE_OUT_OF_RANGE(ch)) {
	return ((unsigned)((ch & 0x1FFFFF) - 0xE0100) <= 0xEF);
    }
#endif
    return (((GRAPH_BITS|SPACE_BITS) >> GetCategory(ch)) & 1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsPunct --
 *
 *	Test if a character is a Unicode punctuation character.
 *
 * Results:
 *	Returns non-zero if character is punct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsPunct(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    if (UNICODE_OUT_OF_RANGE(ch)) {
	return 0;
    }
#endif
    return ((PUNCT_BITS >> GetCategory(ch)) & 1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsSpace --
 *
 *	Test if a character is a whitespace Unicode character.
 *
 * Results:
 *	Returns non-zero if character is a space.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsSpace(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    /* Ignore upper 11 bits. */
    ch &= 0x1FFFFF;
#else
    /* Ignore upper 16 bits. */
    ch &= 0xFFFF;
#endif

    /*
     * If the character is within the first 127 characters, just use the
     * standard C function, otherwise consult the Unicode table.
     */

    if (ch < 0x80) {
	return TclIsSpaceProcM((char) ch);
#if TCL_UTF_MAX > 3
    } else if (UNICODE_OUT_OF_RANGE(ch)) {
	return 0;
#endif
    } else if (ch == 0x0085 || ch == 0x180E || ch == 0x200B
	    || ch == 0x202F || ch == 0x2060 || ch == 0xFEFF) {
	return 1;
    } else {
	return ((SPACE_BITS >> GetCategory(ch)) & 1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsUpper --
 *
 *	Test if a character is a uppercase Unicode character.
 *
 * Results:
 *	Returns non-zero if character is uppercase.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsUpper(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    if (UNICODE_OUT_OF_RANGE(ch)) {
	return 0;
    }
#endif
    return (GetCategory(ch) == UPPERCASE_LETTER);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharIsWordChar --
 *
 *	Test if a character is alphanumeric or a connector punctuation mark.
 *
 * Results:
 *	Returns 1 if character is a word character.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharIsWordChar(
    int ch)			/* Unicode character to test. */
{
#if TCL_UTF_MAX > 3
    if (UNICODE_OUT_OF_RANGE(ch)) {
	return 0;
    }
#endif
    return ((WORD_BITS >> GetCategory(ch)) & 1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharCaseMatch --
 *
 *	See if a particular Unicode string matches a particular pattern.
 *	Allows case insensitivity. This is the Unicode equivalent of the char*
 *	Tcl_StringCaseMatch. The UniChar strings must be NULL-terminated.
 *	This has no provision for counted UniChar strings, thus should not be
 *	used where NULLs are expected in the UniChar string. Use
 *	TclUniCharMatch where possible.
 *
 * Results:
 *	The return value is 1 if string matches pattern, and 0 otherwise. The
 *	matching operation permits the following special characters in the
 *	pattern: *?\[] (see the manual entry for details on what these mean).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UniCharCaseMatch(
    const Tcl_UniChar *uniStr,	/* Unicode String. */
    const Tcl_UniChar *uniPattern,
				/* Pattern, which may contain special
				 * characters. */
    int nocase)			/* 0 for case sensitive, 1 for insensitive */
{
    Tcl_UniChar ch1 = 0, p;

    while (1) {
	p = *uniPattern;

	/*
	 * See if we're at the end of both the pattern and the string. If so,
	 * we succeeded. If we're at the end of the pattern but not at the end
	 * of the string, we failed.
	 */

	if (p == 0) {
	    return (*uniStr == 0);
	}
	if ((*uniStr == 0) && (p != '*')) {
	    return 0;
	}

	/*
	 * Check for a "*" as the next pattern character. It matches any
	 * substring. We handle this by skipping all the characters up to the
	 * next matching one in the pattern, and then calling ourselves
	 * recursively for each postfix of string, until either we match or we
	 * reach the end of the string.
	 */

	if (p == '*') {
	    /*
	     * Skip all successive *'s in the pattern
	     */

	    while (*(++uniPattern) == '*') {
		/* empty body */
	    }
	    p = *uniPattern;
	    if (p == 0) {
		return 1;
	    }
	    if (nocase) {
		p = Tcl_UniCharToLower(p);
	    }
	    while (1) {
		/*
		 * Optimization for matching - cruise through the string
		 * quickly if the next char in the pattern isn't a special
		 * character
		 */

		if ((p != '[') && (p != '?') && (p != '\\')) {
		    if (nocase) {
			while (*uniStr && (p != *uniStr)
				&& (p != Tcl_UniCharToLower(*uniStr))) {
			    uniStr++;
			}
		    } else {
			while (*uniStr && (p != *uniStr)) {
			    uniStr++;
			}
		    }
		}
		if (Tcl_UniCharCaseMatch(uniStr, uniPattern, nocase)) {
		    return 1;
		}
		if (*uniStr == 0) {
		    return 0;
		}
		uniStr++;
	    }
	}

	/*
	 * Check for a "?" as the next pattern character. It matches any
	 * single character.
	 */

	if (p == '?') {
	    uniPattern++;
	    uniStr++;
	    continue;
	}

	/*
	 * Check for a "[" as the next pattern character. It is followed by a
	 * list of characters that are acceptable, or by a range (two
	 * characters separated by "-").
	 */

	if (p == '[') {
	    Tcl_UniChar startChar, endChar;

	    uniPattern++;
	    ch1 = (nocase ? Tcl_UniCharToLower(*uniStr) : *uniStr);
	    uniStr++;
	    while (1) {
		if ((*uniPattern == ']') || (*uniPattern == 0)) {
		    return 0;
		}
		startChar = (nocase ? Tcl_UniCharToLower(*uniPattern)
			: *uniPattern);
		uniPattern++;
		if (*uniPattern == '-') {
		    uniPattern++;
		    if (*uniPattern == 0) {
			return 0;
		    }
		    endChar = (nocase ? Tcl_UniCharToLower(*uniPattern)
			    : *uniPattern);
		    uniPattern++;
		    if (((startChar <= ch1) && (ch1 <= endChar))
			    || ((endChar <= ch1) && (ch1 <= startChar))) {
			/*
			 * Matches ranges of form [a-z] or [z-a].
			 */
			break;
		    }
		} else if (startChar == ch1) {
		    break;
		}
	    }
	    while (*uniPattern != ']') {
		if (*uniPattern == 0) {
		    uniPattern--;
		    break;
		}
		uniPattern++;
	    }
	    uniPattern++;
	    continue;
	}

	/*
	 * If the next pattern character is '\', just strip off the '\' so we
	 * do exact matching on the character that follows.
	 */

	if (p == '\\') {
	    if (*(++uniPattern) == '\0') {
		return 0;
	    }
	}

	/*
	 * There's no special character. Just make sure that the next bytes of
	 * each string match.
	 */

	if (nocase) {
	    if (Tcl_UniCharToLower(*uniStr) !=
		    Tcl_UniCharToLower(*uniPattern)) {
		return 0;
	    }
	} else if (*uniStr != *uniPattern) {
	    return 0;
	}
	uniStr++;
	uniPattern++;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclUniCharMatch --
 *
 *	See if a particular Unicode string matches a particular pattern.
 *	Allows case insensitivity. This is the Unicode equivalent of the char*
 *	Tcl_StringCaseMatch. This variant of Tcl_UniCharCaseMatch uses counted
 *	Strings, so embedded NULLs are allowed.
 *
 * Results:
 *	The return value is 1 if string matches pattern, and 0 otherwise. The
 *	matching operation permits the following special characters in the
 *	pattern: *?\[] (see the manual entry for details on what these mean).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUniCharMatch(
    const Tcl_UniChar *string,	/* Unicode String. */
    int strLen,			/* Length of String */
    const Tcl_UniChar *pattern,	/* Pattern, which may contain special
				 * characters. */
    int ptnLen,			/* Length of Pattern */
    int nocase)			/* 0 for case sensitive, 1 for insensitive */
{
    const Tcl_UniChar *stringEnd, *patternEnd;
    Tcl_UniChar p;

    stringEnd = string + strLen;
    patternEnd = pattern + ptnLen;

    while (1) {
	/*
	 * See if we're at the end of both the pattern and the string. If so,
	 * we succeeded. If we're at the end of the pattern but not at the end
	 * of the string, we failed.
	 */

	if (pattern == patternEnd) {
	    return (string == stringEnd);
	}
	p = *pattern;
	if ((string == stringEnd) && (p != '*')) {
	    return 0;
	}

	/*
	 * Check for a "*" as the next pattern character. It matches any
	 * substring. We handle this by skipping all the characters up to the
	 * next matching one in the pattern, and then calling ourselves
	 * recursively for each postfix of string, until either we match or we
	 * reach the end of the string.
	 */

	if (p == '*') {
	    /*
	     * Skip all successive *'s in the pattern.
	     */

	    while (*(++pattern) == '*') {
		/* empty body */
	    }
	    if (pattern == patternEnd) {
		return 1;
	    }
	    p = *pattern;
	    if (nocase) {
		p = Tcl_UniCharToLower(p);
	    }
	    while (1) {
		/*
		 * Optimization for matching - cruise through the string
		 * quickly if the next char in the pattern isn't a special
		 * character.
		 */

		if ((p != '[') && (p != '?') && (p != '\\')) {
		    if (nocase) {
			while ((string < stringEnd) && (p != *string)
				&& (p != Tcl_UniCharToLower(*string))) {
			    string++;
			}
		    } else {
			while ((string < stringEnd) && (p != *string)) {
			    string++;
			}
		    }
		}
		if (TclUniCharMatch(string, stringEnd - string,
			pattern, patternEnd - pattern, nocase)) {
		    return 1;
		}
		if (string == stringEnd) {
		    return 0;
		}
		string++;
	    }
	}

	/*
	 * Check for a "?" as the next pattern character. It matches any
	 * single character.
	 */

	if (p == '?') {
	    pattern++;
	    string++;
	    continue;
	}

	/*
	 * Check for a "[" as the next pattern character. It is followed by a
	 * list of characters that are acceptable, or by a range (two
	 * characters separated by "-").
	 */

	if (p == '[') {
	    Tcl_UniChar ch1, startChar, endChar;

	    pattern++;
	    ch1 = (nocase ? Tcl_UniCharToLower(*string) : *string);
	    string++;
	    while (1) {
		if ((*pattern == ']') || (pattern == patternEnd)) {
		    return 0;
		}
		startChar = (nocase ? Tcl_UniCharToLower(*pattern) : *pattern);
		pattern++;
		if (*pattern == '-') {
		    pattern++;
		    if (pattern == patternEnd) {
			return 0;
		    }
		    endChar = (nocase ? Tcl_UniCharToLower(*pattern)
			    : *pattern);
		    pattern++;
		    if (((startChar <= ch1) && (ch1 <= endChar))
			    || ((endChar <= ch1) && (ch1 <= startChar))) {
			/*
			 * Matches ranges of form [a-z] or [z-a].
			 */
			break;
		    }
		} else if (startChar == ch1) {
		    break;
		}
	    }
	    while (*pattern != ']') {
		if (pattern == patternEnd) {
		    pattern--;
		    break;
		}
		pattern++;
	    }
	    pattern++;
	    continue;
	}

	/*
	 * If the next pattern character is '\', just strip off the '\' so we
	 * do exact matching on the character that follows.
	 */

	if (p == '\\') {
	    if (++pattern == patternEnd) {
		return 0;
	    }
	}

	/*
	 * There's no special character. Just make sure that the next bytes of
	 * each string match.
	 */

	if (nocase) {
	    if (Tcl_UniCharToLower(*string) != Tcl_UniCharToLower(*pattern)) {
		return 0;
	    }
	} else if (*string != *pattern) {
	    return 0;
	}
	string++;
	pattern++;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * TclUtfToUCS4 --
 *
 *	Extracts the 4-byte codepoint from the leading bytes of the
 *	Modified UTF-8 string "src".  This is a utility routine to
 *	contain the surrogate gymnastics in one place.
 *
 *	The caller must ensure that the source buffer is long enough that this
 *	routine does not run off the end and dereference non-existent memory
 *	looking for trail bytes. If the source buffer is known to be '\0'
 *	terminated, this cannot happen. Otherwise, the caller should call
 *	TclUCS4Complete() before calling this routine to ensure that
 *	enough bytes remain in the string.
 *
 * Results:
 *	Fills *usc4Ptr with the UCS4 code point and returns the number of bytes
 *	consumed from the source string.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
TclpUtfToUCS4(
    const char *src,	/* The UTF-8 string. */
    int *ucs4Ptr)	/* Filled with the UCS4 codepoint represented
			 * by the UTF-8 string. */
{
    Tcl_UniChar ch = 0;
    int len = TclUtfToUniChar(src, &ch);

#if TCL_UTF_MAX <= 4
    if ((ch & ~0x3FF) == 0xD800) {
	Tcl_UniChar low = ch;
	int len2 = TclUtfToUniChar(src+len, &low);
	if ((low & ~0x3FF) == 0xDC00) {
	    *ucs4Ptr = (((ch & 0x3FF) << 10) | (low & 0x3FF)) + 0x10000;
	    return len + len2;
	}
    }
#endif
    *ucs4Ptr = (int)ch;
    return len;
}

#if TCL_UTF_MAX == 4
int
TclUniCharToUCS4(
    const Tcl_UniChar *src,	/* The Tcl_UniChar string. */
    int *ucs4Ptr)	/* Filled with the UCS4 codepoint represented
			 * by the Tcl_UniChar string. */
{
    if (((src[0] & 0xFC00) == 0xD800) && ((src[1] & 0xFC00) == 0xDC00)) {
	*ucs4Ptr = (((src[0] & 0x3FF) << 10) | (src[01] & 0x3FF)) + 0x10000;
	return 2;
    }
    *ucs4Ptr = src[0];
    return 1;
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * TclUCS4ToUtf --
 *
 *	Store the given Unicode character as a sequence of UTF-8 bytes in the
 *	provided buffer. Might output 6 bytes, if the code point > 0xFFFF.
 *
 * Results:
 *	The return values is the number of bytes in the buffer that were
 *	consumed. If ch == -1, this function outputs 0 bytes (empty string),
 *	since TclGetUCS4 returns -1 for out-of-range indices.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
TclUCS4ToUtf(
    int ch,			/* Unicode character to be stored in the
				 * buffer. */
    char *buf)			/* Buffer in which the UTF-8 representation of
				 * the Unicode character is stored. Buffer must be
				 * large enough to hold the UTF-8 character(s)
				 * (at most 6 bytes). */
{
#if TCL_UTF_MAX <= 4
    if (((unsigned)(ch - 0x10000) <= 0xFFFFF)) {
	/* Spit out a 4-byte UTF-8 character or 2 x 3-byte UTF-8 characters, depending on Tcl
	 * version and/or TCL_UTF_MAX build value */
	int len = Tcl_UniCharToUtf(0xD800 | ((ch - 0x10000) >> 10), buf);
	return len + Tcl_UniCharToUtf(0xDC00 | (ch & 0x7FF), buf + len);
    }
#endif
    if ((ch & ~0x7FF) == 0xD800) {
	buf[2] = (char) ((ch | 0x80) & 0xBF);
	buf[1] = (char) (((ch >> 6) | 0x80) & 0xBF);
	buf[0] = (char) ((ch >> 12) | 0xE0);
	return 3;
    }
    if (ch == -1) {
	return 0;
    }
    return Tcl_UniCharToUtf(ch, buf);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
