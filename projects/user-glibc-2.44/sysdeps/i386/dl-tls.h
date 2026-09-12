/* Thread-local storage handling in the ELF dynamic linker.  i386 version.
   Copyright (C) 2002-2026 Free Software Foundation, Inc.
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

#ifndef _DL_TLS_H
#define _DL_TLS_H

/* Type used for the representation of TLS information in the GOT.  */
typedef struct dl_tls_index
{
  unsigned long int ti_module;
  unsigned long int ti_offset;
} tls_index;

/* Dynamic thread vector pointers point to the start of each
   TLS block.  */
#define TLS_DTV_OFFSET 0

/* Static TLS offsets are relative to the unadjusted thread pointer.  */
#define TLS_TP_OFFSET 0

#ifdef SHARED
/* This is the prototype for the GNU version.  */
extern void *___tls_get_addr (tls_index *ti)
     __attribute__ ((__regparm__ (1)));
# if IS_IN (rtld)
/* Prepare using the definition of __tls_get_addr in the generic
   version of this file.  */
# define __tls_get_addr \
    __attribute__ ((__regparm__ (1))) ___tls_get_addr_internal
# else
/* Users should get the better interface.  */
#  define __tls_get_addr ___tls_get_addr
# endif
#endif

#endif /* _DL_TLS_H */
