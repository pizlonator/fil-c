/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 *
 */

#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <glib.h>
#include <string.h>

#include "date-endian.h"

/* We default to returning DATE_ENDIANESS_LITTLE because that's
 * what 3.4 billion people use */
#define DEFAULT_ENDIANESS DATE_ENDIANESS_LITTLE

typedef enum {
	ITEM_NONE = 0,
	ITEM_DAY,
	ITEM_MONTH,
	ITEM_YEAR
} Item;

static gboolean
has_item (Item *items,
	  Item  item)
{
	guint i;

	for (i = 0; i < 3; i++) {
		if (items[i] == ITEM_NONE)
			return FALSE;
		if (items[i] == item)
			return TRUE;
	}
	return FALSE;
}

DateEndianess
date_endian_get_default (gboolean verbose)
{
	const char *fmt;
	const char *p;
	Item items[3];
	guint i;

	fmt = nl_langinfo (D_FMT);
	g_return_val_if_fail (fmt != NULL, DEFAULT_ENDIANESS);

	if (verbose)
		g_print ("%s", fmt);

	if (g_str_equal (fmt, "%F"))
		return DATE_ENDIANESS_BIG;

	i = 0;
	memset (&items, 0, sizeof(items));

	/* Assume ASCII only */
	for (p = fmt; *p != '\0'; p++) {
		char c;

		/* Look for '%' */
		if (*p != '%')
			continue;

		/* Only assert when we're sure we don't have another '%' */
		if (i >= 4) {
			g_warning ("Could not parse format '%s', too many formats", fmt);
			return DEFAULT_ENDIANESS;
		}

		c = *(p + 1);
		/* Ignore alternative formats */
		if (c == 'O' || c == '-' || c == 'E')
			c = *(p + 2);
		if (c == '\0') {
			g_warning ("Count not parse format '%s', unterminated '%%'", fmt);
			return DEFAULT_ENDIANESS;
		}
		switch (c) {
		case 'A':
		case 'd':
		case 'e':
			if (has_item (items, ITEM_DAY) == FALSE) {
				items[i] = ITEM_DAY;
				i++;
			}
			break;
		case 'm':
		case 'b':
		case 'B':
			if (has_item (items, ITEM_MONTH) == FALSE) {
				items[i] = ITEM_MONTH;
				i++;
			}
			break;
		case 'y':
		case 'Y':
			if (has_item (items, ITEM_YEAR) == FALSE) {
				items[i] = ITEM_YEAR;
				i++;
			}
			break;
		case 'a':
			/* Ignore */
			;
		}
	}

	if (items[0] == ITEM_DAY &&
	    items[1] == ITEM_MONTH &&
	    items[2] == ITEM_YEAR)
		return DATE_ENDIANESS_LITTLE;
	if (items[0] == ITEM_YEAR &&
	    items[1] == ITEM_MONTH &&
	    items[2] == ITEM_DAY)
		return DATE_ENDIANESS_BIG;
	if (items[0] == ITEM_MONTH &&
	    items[1] == ITEM_DAY &&
	    items[2] == ITEM_YEAR)
		return DATE_ENDIANESS_MIDDLE;
	if (items[0] == ITEM_YEAR &&
	    items[1] == ITEM_DAY &&
	    items[2] == ITEM_MONTH)
		return DATE_ENDIANESS_YDM;

	g_warning ("Could not parse format '%s'", fmt);

	return DEFAULT_ENDIANESS;
}

DateEndianess
date_endian_get_for_lang (const char *lang,
			  gboolean    verbose)
{
	locale_t locale;
	locale_t old_locale;
	DateEndianess endian;

	locale = newlocale (LC_TIME_MASK, lang, (locale_t) 0);
	if (locale == (locale_t) 0)
		g_warning ("Failed to create locale %s: %s", lang, g_strerror (errno));
	else
		old_locale = uselocale (locale);

	endian = date_endian_get_default (verbose);

	if (locale != (locale_t) 0) {
		uselocale (old_locale);
		freelocale (locale);
	}

	return endian;
}

const char *
date_endian_to_string (DateEndianess endianess)
{
	switch (endianess) {
	case DATE_ENDIANESS_LITTLE:
		return "Little (DD-MM-YYYY)";
	case DATE_ENDIANESS_BIG:
		return "Big (YYYY-MM-DD)";
	case DATE_ENDIANESS_MIDDLE:
		return "Middle (MM-DD-YYYY)";
        case DATE_ENDIANESS_YDM:
		return "YDM (YYYY-DD-MM)";
	default:
		g_assert_not_reached ();
	}
}
