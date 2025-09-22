/*
  File: high_water_alloc.c

  Copyright (C) 2001-2002 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the
  Free Software Foundation; either version 2.1 of the License, or (at
  your option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include "misc.h"

int high_water_alloc(void **buf, size_t *bufsize, size_t newsize)
{
#define CHUNK_SIZE	256
	/*
	 * Goal here is to avoid unnecessary memory allocations by
	 * using static buffers which only grow when necessary.
	 * Size is increased in fixed size chunks (CHUNK_SIZE).
	 */
	if (*bufsize < newsize) {
		void *newbuf;

		newsize = (newsize + CHUNK_SIZE-1) & ~(CHUNK_SIZE-1);
		newbuf = realloc(*buf, newsize);
		if (!newbuf)
			return 1;

		*buf = newbuf;
		*bufsize = newsize;
	}
	return 0;
}
