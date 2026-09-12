/* Architectural parameters for Linux termios - Alpha/PowerPC version

   Copyright (C) 1997-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef K_TERMIOS_H
# error "<termios_arch.h> should only be included from <k_termios.h>"
#endif

#define _TERMIOS2_NCCS 19
#define _HAVE_TERMIOS2_C_CC_BEFORE_C_LINE 1
#define _HAVE_STRUCT_OLD_TERMIOS 0
