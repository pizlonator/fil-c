/*
  Copyright (C) 2009  Andreas Gruenbacher <agruen@suse.de>

  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "misc.h"

#define LINE_SIZE getpagesize()

char *next_line(FILE *file)
{
	static char *line;
	static size_t line_size;
	char *c;
	int eol = 0;

	if (!line) {
		if (high_water_alloc((void **)&line, &line_size, LINE_SIZE))
			return NULL;
	}
	c = line;
	do {
		if (!fgets(c, line_size - (c - line), file))
			return NULL;
		c = strrchr(c, '\0');
		while (c > line && (*(c-1) == '\n' || *(c-1) == '\r')) {
			c--;
			*c = '\0';
			eol = 1;
		}
		if (feof(file))
			break;
		if (!eol) {
			if (high_water_alloc((void **)&line, &line_size,
					     2 * line_size))
				return NULL;
			c = strrchr(line, '\0');
		}
	} while (!eol);
	return line;
}
