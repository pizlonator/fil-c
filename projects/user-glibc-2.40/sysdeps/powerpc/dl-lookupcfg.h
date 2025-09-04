/* Configuration of lookup functions.  PowerPC version.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#define DL_FIXUP_VALUE_TYPE ElfW(Addr)
#define DL_FIXUP_MAKE_VALUE(map, addr) (addr)
#define DL_FIXUP_VALUE_CODE_ADDR(value) (value)
#define DL_FIXUP_VALUE_ADDR(value) (value)
#define DL_FIXUP_ADDR_VALUE(addr) (addr)
#if __WORDSIZE == 64 && _CALL_ELF == 1
/* We need to correctly set the audit modules value for bind-now.  */
# define DL_FIXUP_BINDNOW_ADDR_VALUE(addr) \
 (((Elf64_FuncDesc *)(addr))->fd_func)
# define DL_FIXUP_BINDNOW_RELOC(l, reloc, value, new_value, st_value, lazy) \
 ({								\
    Elf64_FuncDesc *opd = (Elf64_FuncDesc *) (value);		\
    opd->fd_func = (st_value);					\
    if ((new_value) != (uintptr_t) (st_value))			\
     opd->fd_toc = ((Elf64_FuncDesc *)(new_value))->fd_toc;	\
  })
#else
# define DL_FIXUP_BINDNOW_ADDR_VALUE(addr) (addr)
# define DL_FIXUP_BINDNOW_RELOC(l, reloc, value, new_value, st_value, lazy) \
  (*value) = st_value;
#endif
