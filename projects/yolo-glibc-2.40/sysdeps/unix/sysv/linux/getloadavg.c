/* Get system load averages.  Linux version.
   Copyright (C) 1999-2024 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <sys/param.h>
#include <sys/sysinfo.h>

/* Put the 1 minute, 5 minute and 15 minute load averages
   into the first NELEM elements of LOADAVG.
   Return the number written (never more than 3, but may be less than NELEM),
   or -1 if an error occurred.  */

#define CLAMP(v, lo, hi) MIN (MAX (v, lo), hi)

#define SYSINFO_LOADS_SCALE (1 << SI_LOAD_SHIFT)

int
getloadavg (double loadavg[], int nelem)
{
  struct sysinfo info;

  if (__sysinfo (&info) != 0)
    return -1;

  nelem = CLAMP (nelem, 0, array_length (info.loads));

  for (int i = 0; i < nelem; i++)
    loadavg[i] = (double) info.loads[i] / SYSINFO_LOADS_SCALE;

  return nelem;
}
