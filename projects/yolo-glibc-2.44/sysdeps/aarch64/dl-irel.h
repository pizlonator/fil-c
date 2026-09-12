/* Machine-dependent ELF indirect relocation inline functions.
   AArch64 version.
   Copyright (C) 2012-2026 Free Software Foundation, Inc.
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

#ifndef _DL_IREL_H
#define _DL_IREL_H

#include <stdio.h>
#include <ldsodefs.h>
#include <sys/ifunc.h>

#define _IFUNC_ARG_SIZE_VER0 24 /* sizeof 1st published __ifunc_arg_t */
#define _IFUNC_ARG_SIZE_VER1 40 /* sizeof 2nd published __ifunc_arg_t */

#define sizeof_field(TYPE, MEMBER) sizeof ((((TYPE *)0)->MEMBER))
#define offsetofend(TYPE, MEMBER) \
  (offsetof (TYPE, MEMBER) + sizeof_field (TYPE, MEMBER))

_Static_assert (sizeof (__ifunc_arg_t) == _IFUNC_ARG_SIZE_VER1,
  "sizeof (__ifunc_arg_t) != _IFUNC_ARG_SIZE_VER1");

_Static_assert (_IFUNC_ARG_SIZE_VER1
  == (_IFUNC_HWCAP_MAX + 1) * sizeof (unsigned long),
  "_IFUNC_ARG_SIZE_VER1 and _IFUNC_HWCAP_MAX mismatch");

#undef offsetofend
#undef sizeof_field

#define ELF_MACHINE_IRELA	1

static inline ElfW(Addr)
__attribute ((always_inline))
elf_ifunc_invoke (ElfW(Addr) addr)
{
  __ifunc_arg_t arg;

  arg._size = sizeof (arg);
  arg._hwcap = GLRO(dl_hwcap);
  arg._hwcap2 = GLRO(dl_hwcap2);
  arg._hwcap3 = GLRO(dl_hwcap3);
  arg._hwcap4 = GLRO(dl_hwcap4);
  return ((ElfW(Addr) (*) (uint64_t, const __ifunc_arg_t *)) (addr))
	 (GLRO(dl_hwcap) | _IFUNC_ARG_HWCAP, &arg);
}

static inline void
__attribute ((always_inline))
elf_irela (const ElfW(Rela) *reloc)
{
  ElfW(Addr) *const reloc_addr = (void *) reloc->r_offset;
  const unsigned long int r_type = ELFW(R_TYPE) (reloc->r_info);

  if (__glibc_likely (r_type == R_AARCH64_IRELATIVE))
    {
      ElfW(Addr) value = elf_ifunc_invoke (reloc->r_addend);
      *reloc_addr = value;
    }
  else
    __libc_fatal ("Unexpected reloc type in static binary.\n");
}

#endif
