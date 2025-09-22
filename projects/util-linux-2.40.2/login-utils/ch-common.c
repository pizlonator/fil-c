/*
 * chfn and chsh shared functions
 *
 * this program is free software.  you can redistribute it and
 * modify it under the terms of the gnu general public license.
 * there is no warranty.
 */

#include <ctype.h>
#include <string.h>

#include "c.h"
#include "nls.h"

#include "ch-common.h"

/*
 *  illegal_passwd_chars () -
 *	check whether a string contains illegal characters
 */
int illegal_passwd_chars(const char *str)
{
	const char illegal[] = ",:=\"\n";
	const size_t len = strlen(str);
	size_t i;

	if (strpbrk(str, illegal))
		return 1;
	for (i = 0; i < len; i++) {
		if (iscntrl(str[i]))
			return 1;
	}
	return 0;
}
