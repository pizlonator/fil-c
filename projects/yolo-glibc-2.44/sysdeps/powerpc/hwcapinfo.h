/* powerpc HWCAP/HWCAP2 and AT_PLATFORM data pre-processing.
   Copyright (C) 2015-2026 Free Software Foundation, Inc.
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

#include <string.h>
#include <stdint.h>

#ifndef HWCAPINFO_H
# define HWCAPINFO_H

#define _DL_FIRST_PLATFORM	32

/* Platform bits (relative to _DL_FIRST_PLATFORM).  */
#define PPC_PLATFORM_POWER4		0
#define PPC_PLATFORM_PPC970		1
#define PPC_PLATFORM_POWER5		2
#define PPC_PLATFORM_POWER5_PLUS	3
#define PPC_PLATFORM_POWER6		4
#define PPC_PLATFORM_CELL_BE		5
#define PPC_PLATFORM_POWER6X		6
#define PPC_PLATFORM_POWER7		7
#define PPC_PLATFORM_PPCA2		8
#define PPC_PLATFORM_PPC405		9
#define PPC_PLATFORM_PPC440		10
#define PPC_PLATFORM_PPC464		11
#define PPC_PLATFORM_PPC476		12
#define PPC_PLATFORM_POWER8		13
#define PPC_PLATFORM_POWER9		14
#define PPC_PLATFORM_POWER10		15
#define PPC_PLATFORM_POWER11		16

static inline int
__attribute__ ((unused, always_inline))
_dl_string_platform (const char *str)
{
  if (str == NULL)
    return -1;

  if (strncmp (str, "power", 5) == 0)
    {
      int ret;
      str += 5;
      switch (*str)
	{
	case '1':
	  if (str[1] == '0')
	    {
	      ret = _DL_FIRST_PLATFORM + PPC_PLATFORM_POWER10;
	      str++;
	    }
	  else if (str[1] == '1')
	    {
	      ret = _DL_FIRST_PLATFORM + PPC_PLATFORM_POWER11;
	      str++;
	    }
	  else
	    return -1;
	  break;
	case '4':
	  ret = _DL_FIRST_PLATFORM + PPC_PLATFORM_POWER4;
	  break;
	case '5':
	  ret = _DL_FIRST_PLATFORM + PPC_PLATFORM_POWER5;
	  if (str[1] == '+')
	    {
	      ret = _DL_FIRST_PLATFORM + PPC_PLATFORM_POWER5_PLUS;
	      ++str;
	    }
	  break;
	case '6':
	  ret = _DL_FIRST_PLATFORM + PPC_PLATFORM_POWER6;
	  if (str[1] == 'x')
	    {
	      ret = _DL_FIRST_PLATFORM + PPC_PLATFORM_POWER6X;
	      ++str;
	    }
	  break;
	case '7':
	  ret = _DL_FIRST_PLATFORM + PPC_PLATFORM_POWER7;
	  break;
	case '8':
	  ret = _DL_FIRST_PLATFORM + PPC_PLATFORM_POWER8;
	  break;
	case '9':
	  ret = _DL_FIRST_PLATFORM + PPC_PLATFORM_POWER9;
	  break;
	default:
	  return -1;
	}
      if (str[1] == '\0')
       return ret;
    }
  else if (strncmp (str, "ppc", 3) == 0)
    {
      if (strcmp (str + 3, "970") == 0)
	return _DL_FIRST_PLATFORM + PPC_PLATFORM_PPC970;
      else if (strcmp (str + 3, "-cell-be") == 0)
	return _DL_FIRST_PLATFORM + PPC_PLATFORM_CELL_BE;
      else if (strcmp (str + 3, "a2") == 0)
	return _DL_FIRST_PLATFORM + PPC_PLATFORM_PPCA2;
      else if (strcmp (str + 3, "405") == 0)
	return _DL_FIRST_PLATFORM + PPC_PLATFORM_PPC405;
      else if (strcmp (str + 3, "440") == 0)
	return _DL_FIRST_PLATFORM + PPC_PLATFORM_PPC440;
      else if (strcmp (str + 3, "464") == 0)
	return _DL_FIRST_PLATFORM + PPC_PLATFORM_PPC464;
      else if (strcmp (str + 3, "476") == 0)
	return _DL_FIRST_PLATFORM + PPC_PLATFORM_PPC476;
    }

  return -1;
}

extern void __tcb_parse_hwcap_and_convert_at_platform (void);

#endif
