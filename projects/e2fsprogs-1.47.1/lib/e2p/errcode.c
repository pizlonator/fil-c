/*
 * errcode.c		- convert an error code to a string
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e2p.h"

static const char *err_string[] = {
	"",
	"UNKNOWN",		/*  1 */
	"EIO",			/*  2 */
	"ENOMEM",		/*  3 */
	"EFSBADCRC",		/*  4 */
	"EFSCORRUPTED",		/*  5 */
	"ENOSPC",		/*  6 */
	"ENOKEY",		/*  7 */
	"EROFS",		/*  8 */
	"EFBIG",		/*  9 */
	"EEXIST",		/* 10 */
	"ERANGE",		/* 11 */
	"EOVERFLOW",		/* 12 */
	"EBUSY",		/* 13 */
	"ENOTDIR",		/* 14 */
	"ENOTEMPTY",		/* 15 */
	"ESHUTDOWN",		/* 16 */
	"EFAULT",		/* 17 */
};

#define ARRAY_SIZE(array)			\
        (sizeof(array) / sizeof(array[0]))

/* Return the name of an encoding or NULL */
const char *e2p_errcode2str(unsigned int err)
{
	static char buf[32];

	if (err < ARRAY_SIZE(err_string))
		return err_string[err];

	sprintf(buf, "UNKNOWN_ERRCODE_%u", err);
	return buf;
}


