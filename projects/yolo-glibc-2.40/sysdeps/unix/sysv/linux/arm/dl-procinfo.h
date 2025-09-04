/* Linux/ARM version of processor capability information handling macros.
   Copyright (C) 2001-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _DL_PROCINFO_H
#define _DL_PROCINFO_H	1

#include <ldsodefs.h>
#include <sysdep.h>

#define _DL_HWCAP_COUNT 27

/* Low 28 bits are allocated in HWCAP.  */
#define _DL_HWCAP_LAST		27

/* Low 7 bits are allocated in HWCAP2.  */
#define _DL_HWCAP2_LAST		6


static inline const char *
__attribute__ ((unused))
_dl_hwcap_string (int idx)
{
  return GLRO(dl_arm_cap_flags)[idx];
};

static inline int
__attribute__ ((unused))
_dl_procinfo (unsigned int type, unsigned long int word)
{
  switch(type)
    {
    case AT_HWCAP:
      _dl_printf ("AT_HWCAP:       ");

      for (int i = 0; i <= _DL_HWCAP_LAST; ++i)
	if (word & (1 << i))
	  _dl_printf (" %s", _dl_hwcap_string (i));
      break;
    case AT_HWCAP2:
      {
	unsigned int offset = _DL_HWCAP_LAST + 1;

	_dl_printf ("AT_HWCAP2:      ");

	for (int i = 0; i <= _DL_HWCAP2_LAST; ++i)
	  if (word & (1 << i))
	    _dl_printf (" %s", _dl_hwcap_string (offset + i));
	break;
      }
    default:
      /* Fallback to generic output mechanism.  */
      return -1;
    }
  _dl_printf ("\n");
  return 0;
}

#endif /* dl-procinfo.h */
