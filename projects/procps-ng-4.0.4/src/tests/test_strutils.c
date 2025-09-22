/*
 * test_strutils.c - tests for strutils.c routines
 * This file was copied from util-linux at fall 2011.
 *
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2010 Davidlohr Bueso <dave@gnu.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>

#include "c.h"
#include "strutils.h"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		error(EXIT_FAILURE, 0, "no arguments");
	} else if (argc < 3) {
		printf("%ld\n", strtol_or_err(argv[1], "strtol_or_err"));
	} else {
		printf("%lf\n", strtod_or_err(argv[2], "strtod_or_err"));
	}
	return EXIT_SUCCESS;
}
