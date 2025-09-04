/* Configuration of lookup functions.  SPARC64 version.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

/* The type of the return value of fixup/profile_fixup.  */
#define DL_FIXUP_VALUE_TYPE ElfW(Addr)
/* Construct a value of type DL_FIXUP_VALUE_TYPE from a code address
   and a link map.  */
#define DL_FIXUP_MAKE_VALUE(map, addr) (addr)
/* Extract the code address from a value of type DL_FIXUP_MAKE_VALUE.
 */
#define DL_FIXUP_VALUE_CODE_ADDR(value) (value)
#define DL_FIXUP_VALUE_ADDR(value) (value)
#define DL_FIXUP_ADDR_VALUE(addr) (addr)
#define DL_FIXUP_BINDNOW_ADDR_VALUE(addr) (addr)
/* For bindnow, _dl_audit_symbind will be responsible to setup the final value
   while for lazy binding _dl_fixup/_dl_profile_fixup will call the audit
   callbacks and tail cail elf_machine_fixup_plt.  */
#ifdef __arch64__
# define DL_SPARC_FIXUP(l, r, value, new_value)                              \
  sparc64_fixup_plt (l, r, value, new_value, (r)->r_addend, 0)
#else
# define DL_SPARC_FIXUP(l, r, value, new_value)                              \
  sparc_fixup_plt (r, value, new_value, 0, 1)
#endif
#define DL_FIXUP_BINDNOW_RELOC(l, reloc, value, new_value, st_value, lazy)   \
  ({                                                                         \
     if (lazy)                                                               \
       (*value) = st_value;                                                  \
     else                                                                    \
       {                                                                     \
         const PLTREL *__r = (reloc);                                        \
         DL_SPARC_FIXUP (l, __r, value, new_value);                          \
       }                                                                     \
  })
