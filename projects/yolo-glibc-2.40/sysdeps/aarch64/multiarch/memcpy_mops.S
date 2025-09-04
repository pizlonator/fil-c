/* Optimized memcpy for MOPS.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>

/* Assumptions:
 *
 * AArch64, MOPS.
 *
 */

ENTRY (__memcpy_mops)
	PTR_ARG (0)
	PTR_ARG (1)
	SIZE_ARG (2)

	mov	x3, x0
	.inst	0x19010443	/* cpyfp   [x3]!, [x1]!, x2!  */
	.inst	0x19410443	/* cpyfm   [x3]!, [x1]!, x2!  */
	.inst	0x19810443	/* cpyfe   [x3]!, [x1]!, x2!  */
	ret

END (__memcpy_mops)
