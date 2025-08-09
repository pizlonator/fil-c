/*
 * strstr.c --
 *
 *	Source code for the "strstr" library routine.
 *
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tcl.h"
#ifndef NULL
#define NULL 0
#endif

/*
 *----------------------------------------------------------------------
 *
 * strstr --
 *
 *	Locate the first instance of a substring in a string.
 *
 * Results:
 *	If string contains substring, the return value is the location of the
 *	first matching instance of substring in string. If string doesn't
 *	contain substring, the return value is 0. Matching is done on an exact
 *	character-for-character basis with no wildcards or special characters.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
strstr(
    const char *string,		/* String to search. */
    const char *substring)		/* Substring to try to find in string. */
{
    const char *a, *b;

    /*
     * First scan quickly through the two strings looking for a
     * single-character match. When it's found, then compare the rest of the
     * substring.
     */

    b = substring;
    if (*b == 0) {
	return (char *)string;
    }
    for ( ; *string != 0; string += 1) {
	if (*string != *b) {
	    continue;
	}
	a = string;
	while (1) {
	    if (*b == 0) {
		return (char *)string;
	    }
	    if (*a++ != *b++) {
		break;
	    }
	}
	b = substring;
    }
    return NULL;
}
