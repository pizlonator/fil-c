/* Support for STV_PROTECTED visibility.  Generic version.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#ifndef _DL_PROTECTED_H
#define _DL_PROTECTED_H

static inline void __attribute__ ((always_inline))
_dl_check_protected_symbol (const char *undef_name,
			    const struct link_map *undef_map,
			    const ElfW(Sym) *ref,
			    const struct link_map *map,
			    int type_class)
{
  if (undef_map == NULL || undef_map->l_type != lt_executable)
    return;

  if (type_class & ELF_RTYPE_CLASS_COPY)
    /* Disallow copy relocations in executable against protected
       data symbols in a shared object which needs indirect external
       access.  */
    _dl_error_printf ("warning: copy relocation against non-copyable "
		      "protected symbol `%s' in `%s'\n",
		      undef_name, map->l_name);
  else if ((type_class & ELF_RTYPE_CLASS_PLT) && ref->st_value != 0
	   && ref->st_shndx == SHN_UNDEF)
    /* Disallow non-zero symbol values of undefined symbols in
       executable, which are used as the function pointer, against
       protected function symbols in a shared object with indirect
       external access.  */
    _dl_error_printf (
	"warning: direct reference to "
	"protected function `%s' in `%s' may break pointer equality\n",
	undef_name, map->l_name);
  else
    return;

  if (map->l_1_needed & GNU_PROPERTY_1_NEEDED_INDIRECT_EXTERN_ACCESS)
    _dl_signal_error (
	0, map->l_name, undef_name,
	N_ ("error due to GNU_PROPERTY_1_NEEDED_INDIRECT_EXTERN_ACCESS"));
}

#endif /* _DL_PROTECTED_H */
