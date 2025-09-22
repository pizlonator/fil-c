/* Describe known auxv types.
   Copyright (C) 2007, 2008, 2009 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <byteswap.h>
#include <endian.h>
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <libeblP.h>

#define AUXV_TYPES							      \
  TYPE (NULL, "")							      \
  TYPE (IGNORE, "")							      \
  TYPE (EXECFD, "d")							      \
  TYPE (EXECFN, "s")							      \
  TYPE (PHDR, "p")							      \
  TYPE (PHENT, "u")							      \
  TYPE (PHNUM, "u")							      \
  TYPE (PAGESZ, "u")							      \
  TYPE (BASE, "p")							      \
  TYPE (FLAGS, "x")							      \
  TYPE (ENTRY, "p")							      \
  TYPE (NOTELF, "")							      \
  TYPE (UID, "u")							      \
  TYPE (EUID, "u")							      \
  TYPE (GID, "u")							      \
  TYPE (EGID, "u")							      \
  TYPE (CLKTCK, "u")							      \
  TYPE (PLATFORM, "s")							      \
  TYPE (BASE_PLATFORM, "s")						      \
  TYPE (HWCAP, "x")							      \
  TYPE (FPUCW, "x")							      \
  TYPE (DCACHEBSIZE, "d")						      \
  TYPE (ICACHEBSIZE, "d")						      \
  TYPE (UCACHEBSIZE, "d")						      \
  TYPE (IGNOREPPC, "")							      \
  TYPE (SECURE, "u")							      \
  TYPE (SYSINFO, "p")							      \
  TYPE (SYSINFO_EHDR, "p")						      \
  TYPE (L1I_CACHESHAPE, "d")						      \
  TYPE (L1D_CACHESHAPE, "d")						      \
  TYPE (L2_CACHESHAPE, "d")						      \
  TYPE (L3_CACHESHAPE, "d")						      \
  TYPE (RANDOM, "p")

static const struct
{
  const char *name, *format;
} auxv_types[] =
  {
#define TYPE(name, fmt) [AT_##name] = { #name, fmt },
    AUXV_TYPES
#undef	TYPE
  };
#define nauxv_types (sizeof auxv_types / sizeof auxv_types[0])

int
ebl_auxv_info (Ebl *ebl, GElf_Xword a_type, const char **name,
	       const char **format)
{
  int result = ebl->auxv_info (a_type, name, format);
  if (result == 0 && a_type < nauxv_types && auxv_types[a_type].name != NULL)
    {
      /* The machine specific function did not know this type.  */
      *name = auxv_types[a_type].name;
      *format = auxv_types[a_type].format;
      result = 1;
    }
  return result;
}
