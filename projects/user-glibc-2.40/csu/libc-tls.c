/* Initialization code for TLS in statically linked application.
   Copyright (C) 2002-2024 Free Software Foundation, Inc.
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

#include <startup.h>
#include <errno.h>
#include <ldsodefs.h>
#include <tls.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/param.h>
#include <array_length.h>
#include <pthreadP.h>
#include <dl-call_tls_init_tp.h>
#include <stdfil.h>

void
__libc_setup_tls (void)
{
  void* pd = zgc_alloc (TLS_TCB_SIZE);
  call_tls_init_tp (pd);  
}
