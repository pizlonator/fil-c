/* Macros for handling Mach RPC messages.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <mach/message.h>

/* Macro used by MIG to cleanly check the type.  */
#define BAD_TYPECHECK(type, check) __glibc_unlikely (({	\
  union { mach_msg_type_t t; uintptr_t w; } _t, _c;	\
  _t.t = *(type); _c.t = *(check);_t.w != _c.w; }))

_Static_assert (sizeof (uintptr_t) == sizeof (mach_msg_type_t),
                "mach_msg_type_t needs to be the same size as uintptr_t");
