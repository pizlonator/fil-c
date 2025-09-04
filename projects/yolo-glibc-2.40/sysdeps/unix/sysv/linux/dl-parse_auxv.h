/* Parse the Linux auxiliary vector.
   Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

#include <elf.h>
#include <entry.h>
#include <fpu_control.h>
#include <ldsodefs.h>
#include <link.h>

typedef ElfW(Addr) dl_parse_auxv_t[AT_MINSIGSTKSZ + 1];

/* Copy the auxiliary vector into AUXV_VALUES and set up GLRO
   variables.  */
static inline
void _dl_parse_auxv (ElfW(auxv_t) *av, dl_parse_auxv_t auxv_values)
{
  auxv_values[AT_ENTRY] = (ElfW(Addr)) ENTRY_POINT;
  auxv_values[AT_PAGESZ] = EXEC_PAGESIZE;
  auxv_values[AT_FPUCW] = _FPU_DEFAULT;

  /* NB: Default to a constant CONSTANT_MINSIGSTKSZ.  */
  _Static_assert (__builtin_constant_p (CONSTANT_MINSIGSTKSZ),
                  "CONSTANT_MINSIGSTKSZ is constant");
  auxv_values[AT_MINSIGSTKSZ] = CONSTANT_MINSIGSTKSZ;

  for (; av->a_type != AT_NULL; av++)
    if (av->a_type <= AT_MINSIGSTKSZ)
      auxv_values[av->a_type] = av->a_un.a_val;

  GLRO(dl_pagesize) = auxv_values[AT_PAGESZ];
  __libc_enable_secure = auxv_values[AT_SECURE];
  GLRO(dl_platform) = (void *) auxv_values[AT_PLATFORM];
  GLRO(dl_hwcap) = auxv_values[AT_HWCAP];
  GLRO(dl_hwcap2) = auxv_values[AT_HWCAP2];
  GLRO(dl_hwcap3) = auxv_values[AT_HWCAP3];
  GLRO(dl_hwcap4) = auxv_values[AT_HWCAP4];
  GLRO(dl_clktck) = auxv_values[AT_CLKTCK];
  GLRO(dl_fpu_control) = auxv_values[AT_FPUCW];
  _dl_random = (void *) auxv_values[AT_RANDOM];
  GLRO(dl_minsigstacksize) = auxv_values[AT_MINSIGSTKSZ];
  GLRO(dl_sysinfo_dso) = (void *) auxv_values[AT_SYSINFO_EHDR];
#ifdef NEED_DL_SYSINFO
  if (GLRO(dl_sysinfo_dso) != NULL)
    GLRO(dl_sysinfo) = auxv_values[AT_SYSINFO];
#endif

  DL_PLATFORM_AUXV
}
