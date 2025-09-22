/* IA-64 specific symbolic name handling.
   Copyright (C) 2002-2009, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <elf.h>
#include <stddef.h>
#include <assert.h>

#define BACKEND		ia64_
#include "libebl_CPU.h"


const char *
ia64_segment_type_name (int segment, char *buf __attribute__ ((unused)),
			size_t len __attribute__ ((unused)))
{
  switch (segment)
    {
    case PT_IA_64_ARCHEXT:
      return "IA_64_ARCHEXT";
    case PT_IA_64_UNWIND:
      return "IA_64_UNWIND";
    case PT_IA_64_HP_OPT_ANOT:
      return "IA_64_HP_OPT_ANOT";
    case PT_IA_64_HP_HSL_ANOT:
      return "IA_64_HP_HSL_ANOT";
    case PT_IA_64_HP_STACK:
      return "IA_64_HP_STACK";
    default:
      break;
    }
  return NULL;
}

const char *
ia64_dynamic_tag_name (int64_t tag, char *buf __attribute__ ((unused)),
		       size_t len __attribute__ ((unused)))
{
  switch (tag)
    {
    case DT_IA_64_PLT_RESERVE:
      return "IA_64_PLT_RESERVE";
    default:
      break;
    }
  return NULL;
}

/* Check dynamic tag.  */
bool
ia64_dynamic_tag_check (int64_t tag)
{
  return tag == DT_IA_64_PLT_RESERVE;
}

/* Check whether machine flags are valid.  */
bool
ia64_machine_flag_check (GElf_Word flags)
{
  return ((flags &~ EF_IA_64_ABI64) == 0);
}

/* Check whether SHF_MASKPROC flags are valid.  */
bool
ia64_machine_section_flag_check (GElf_Xword sh_flags)
{
  return (sh_flags &~ (SHF_IA_64_SHORT | SHF_IA_64_NORECOV)) == 0;
}

/* Return symbolic representation of section type.  */
const char *
ia64_section_type_name (int type,
			char *buf __attribute__ ((unused)),
			size_t len __attribute__ ((unused)))
{
  switch (type)
    {
    case SHT_IA_64_EXT:
      return "IA_64_EXT";
    case SHT_IA_64_UNWIND:
      return "IA_64_UNWIND";
    }

  return NULL;
}

/* Check for the simple reloc types.  */
Elf_Type
ia64_reloc_simple_type (Ebl *ebl, int type,
			int *addsub __attribute__ ((unused)))
{
  switch (type)
    {
      /* The SECREL types when used with non-allocated sections
	 like .debug_* are the same as direct absolute relocs
	 applied to those sections, since a 0 section address is assumed.
	 So we treat them the same here.  */

    case R_IA64_SECREL32MSB:
    case R_IA64_DIR32MSB:
      if (ebl->data == ELFDATA2MSB)
	return ELF_T_WORD;
      break;
    case R_IA64_SECREL32LSB:
    case R_IA64_DIR32LSB:
      if (ebl->data == ELFDATA2LSB)
	return ELF_T_WORD;
      break;
    case R_IA64_DIR64MSB:
    case R_IA64_SECREL64MSB:
      if (ebl->data == ELFDATA2MSB)
	return ELF_T_XWORD;
      break;
    case R_IA64_SECREL64LSB:
    case R_IA64_DIR64LSB:
      if (ebl->data == ELFDATA2LSB)
	return ELF_T_XWORD;
      break;
    }

  return ELF_T_NUM;
}

/* The SHT_IA_64_UNWIND section type is a valid target for relocation.  */
bool
ia64_check_reloc_target_type (Ebl *ebl __attribute__ ((unused)), Elf64_Word sh_type)
{
  return sh_type == SHT_IA_64_UNWIND;
}
