/* Test verification functions for memory access checks.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#ifndef SUPPORT_CHECK_MEM_ACCESS_H
#define SUPPORT_CHECK_MEM_ACCESS_H

#include <stdbool.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/* To check if the a memory region is inaccessible, this function tries
   read / write on the provided address ADDR and checks if a SIGSEGV is
   generated.  This function is not thread-safe and it changes signal
   handlers for SIGSEGV and SIGBUS.
   If WRITE is true, only the write operation is checked, otherwise only
   the read operation is checked. */
bool check_mem_access (const void *addr, bool write);

__END_DECLS

#endif // SUPPORT_CHECK_MEM_ACCESS_H
