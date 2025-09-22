// SPDX-FileCopyrightText: 2008, Nicolas François
// SPDX-FileCopyrightText: 2023-2024, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#include <config.h>

#ident "$Id: $"

#include <ctype.h>
#include <stdlib.h>

#include "atoi/a2i.h"
#include "defines.h"
#include "prototypes.h"


/*
 * Parse a range and indicate if the range is valid.
 * Valid ranges are in the form:
 *     <long>          -> min=max=long         has_min  has_max
 *     -<long>         -> max=long            !has_min  has_max
 *     <long>-         -> min=long             has_min !has_max
 *     <long1>-<long2> -> min=long1 max=long2  has_min  has_max
 */
int
getrange(const char *range,
         unsigned long *min, bool *has_min,
         unsigned long *max, bool *has_max)
{
	const char  *end;

	if (NULL == range)
		return -1;

	*min = 0;
	*has_min = false;
	*has_max = false;

	if ('-' == range[0]) {
		end = range + 1;
		goto parse_max;
	}

	if (a2ul(min, range, &end, 10, 0, ULONG_MAX) == -1 && errno != ENOTSUP)
		return -1;
	*has_min = true;

	switch (*end++) {
	case '\0':
		*has_max = true;
		*max = *min;
		return 0;  /* <long> */

	case '-':
		if ('\0' == *end)
			return 0;  /* <long>- */
parse_max:
		if (!isdigit((unsigned char) *end))
			return -1;

		if (a2ul(max, end, NULL, 10, *min, ULONG_MAX) == -1)
			return -1;
		*has_max = true;

		return 0;  /* <long>-<long>, or -<long> */

	default:
		return -1;
	}
}
