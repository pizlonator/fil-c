/* CPU affinity handling for the dynamic linker.  Stub version.
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

#ifndef DL_AFFINITY_H
#define DL_AFFINITY_H

#include <errno.h>
#include <stddef.h>

/* On success, write the current CPU ID to *CPU, and the current node
   ID to *NODE, and return 0.  Return a negative error code on
   failure.  */
static inline int
_dl_getcpu (unsigned int *cpu, unsigned int *node)
{
  return -ENOSYS;
}

/* On success, write CPU ID affinity bits for the current thread to
   *BITS, which must be SIZE bytes long, and return the number of
   bytes updated, a multiple of sizeof (unsigned long int).  On
   failure, return a negative error code.  */
static int
_dl_getaffinity (unsigned long int *bits, size_t size)
{
  return -ENOSYS;
}

/* Set the CPU affinity mask for the current thread to *BITS, using
   the SIZE bytes from that array, which should be a multiple of
   sizeof (unsigned long int).  Return 0 on success, and a negative
   error code on failure.  */
static int
_dl_setaffinity (const unsigned long int *bits, size_t size)
{
  return -ENOSYS;
}

#endif /* DL_AFFINITY_H */
