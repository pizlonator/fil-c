/* x86-64 PLT trampoline register save macros.
   Copyright (C) 2024 Free Software Foundation, Inc.
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

#ifndef DL_STACK_ALIGNMENT
/* Due to GCC bug:

   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58066

   __tls_get_addr may be called with 8-byte stack alignment.  Although
   this bug has been fixed in GCC 4.9.4, 5.3 and 6, we can't assume
   that stack will be always aligned at 16 bytes.  */
# define DL_STACK_ALIGNMENT 8
#endif

/* True if _dl_runtime_resolve should align stack for STATE_SAVE or align
   stack to 16 bytes before calling _dl_fixup.  */
#define DL_RUNTIME_RESOLVE_REALIGN_STACK \
  (STATE_SAVE_ALIGNMENT > DL_STACK_ALIGNMENT \
   || 16 > DL_STACK_ALIGNMENT)
