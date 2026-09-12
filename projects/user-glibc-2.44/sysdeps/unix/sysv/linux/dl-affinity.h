/* CPU affinity handling for the dynamic linker.  Linux version.
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

/* See sysdeps/generic/dl-affinity.h for documentation of these interfaces.  */

#ifndef DL_AFFINITY_H
#define DL_AFFINITY_H

#include <sysdep.h>
#include <stddef.h>
#include <unistd.h>

static inline int
_dl_getcpu (unsigned int *cpu, unsigned int *node)
{
  return INTERNAL_SYSCALL_CALL (getcpu, cpu, node);
}

static int
_dl_getaffinity (unsigned long int *bits, size_t size)
{
  return INTERNAL_SYSCALL_CALL (sched_getaffinity, /* TID */ 0, size, bits);
}

static int
_dl_setaffinity (const unsigned long int *bits, size_t size)
{
  return INTERNAL_SYSCALL_CALL (sched_setaffinity, /* TID */ 0, size, bits);
}

#endif /* DL_AFFINITY_H */
