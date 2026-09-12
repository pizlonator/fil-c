/* Internal definitions for Linux getrandom implementation.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#ifndef _GETRANDOM_INTERNAL_H
#define _GETRANDOM_INTERNAL_H

#include <pthreadP.h>

extern void __getrandom_early_init (_Bool) attribute_hidden;

extern void __getrandom_fork_subprocess (void) attribute_hidden;
extern void __getrandom_vdso_release (struct pthread *curp) attribute_hidden;
extern void __getrandom_reset_state (struct pthread *curp) attribute_hidden;
#endif
