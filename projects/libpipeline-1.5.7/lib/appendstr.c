/* appendstr.c -- append to a dynamically allocated string
   Copyright (C) 1994 Markus Armbruster

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Library Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.LIB.  If not, write
   to the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA
   02139, USA.  */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdarg.h>
#include <string.h>

#include "xalloc.h"

#include "pipeline-private.h"

/* append strings to first argument, which is realloced to the correct size
   first arg may be NULL */
char *appendstr (char *str, ...)
{
	va_list ap;
	int len, newlen;
	char *next, *end;

	len = str ? strlen (str) : 0;

	va_start (ap, str);
	newlen = len + 1;
	while ((next = va_arg (ap, char *)))
		newlen += strlen (next);
	va_end (ap);

	str = xrealloc (str, newlen);
	end = str + len;

	va_start (ap, str);
	while ((next = va_arg (ap, char *))) {
		strcpy (end, next);
		end += strlen (next);
	}
	va_end (ap);

	return str;
}
