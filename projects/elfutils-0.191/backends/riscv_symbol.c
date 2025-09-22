/* RISC-V specific symbolic name handling.
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
#include <elf.h>
#include <stddef.h>
#include <string.h>

#define BACKEND riscv_
#include "libebl_CPU.h"


/* Check for the simple reloc types.  */
Elf_Type
riscv_reloc_simple_type (Ebl *ebl __attribute__ ((unused)), int type,
			 int *addsub)
{
  switch (type)
    {
    case R_RISCV_SET8:
      return ELF_T_BYTE;
    case R_RISCV_SET16:
      return ELF_T_HALF;
    case R_RISCV_32:
    case R_RISCV_SET32:
      return ELF_T_WORD;
    case R_RISCV_64:
      return ELF_T_XWORD;
    case R_RISCV_ADD16:
      *addsub = 1;
      return ELF_T_HALF;
    case R_RISCV_SUB16:
      *addsub = -1;
      return ELF_T_HALF;
    case R_RISCV_ADD32:
      *addsub = 1;
      return ELF_T_WORD;
    case R_RISCV_SUB32:
      *addsub = -1;
      return ELF_T_WORD;
    case R_RISCV_ADD64:
      *addsub = 1;
      return ELF_T_XWORD;
    case R_RISCV_SUB64:
      *addsub = -1;
      return ELF_T_XWORD;
    default:
      return ELF_T_NUM;
    }
}

/* Check whether machine flags are valid.  */
bool
riscv_machine_flag_check (GElf_Word flags)
{
  return ((flags &~ (EF_RISCV_RVC
		     | EF_RISCV_FLOAT_ABI)) == 0);
}

/* Check whether given symbol's st_value and st_size are OK despite failing
   normal checks.  */
bool
riscv_check_special_symbol (Elf *elf, const GElf_Sym *sym,
			    const char *name, const GElf_Shdr *destshdr)
{
  if (name == NULL)
    return false;

  size_t shstrndx;
  if (elf_getshdrstrndx (elf, &shstrndx) != 0)
    return false;
  const char *sname = elf_strptr (elf, shstrndx, destshdr->sh_name);
  if (sname == NULL)
    return false;

  /* _GLOBAL_OFFSET_TABLE_ points to the start of the .got section, but it
     is preceded by the .got.plt section in the output .got section.  */
  if (strcmp (name, "_GLOBAL_OFFSET_TABLE_") == 0)
    return (strcmp (sname, ".got") == 0
	    && sym->st_value >= destshdr->sh_addr
	    && sym->st_value < destshdr->sh_addr + destshdr->sh_size);

  /* __global_pointer$ points to the .sdata section with an offset of
     0x800.  It might however fall in the .got section, in which case we
     cannot check the offset.  The size always should be zero.  */
  if (strcmp (name, "__global_pointer$") == 0)
    return (((strcmp (sname, ".sdata") == 0
	      && sym->st_value == destshdr->sh_addr + 0x800)
	     || strcmp (sname, ".got") == 0)
	    && sym->st_size == 0);

  return false;
}

const char *
riscv_segment_type_name (int segment, char *buf __attribute__ ((unused)),
			 size_t len __attribute__ ((unused)))
{
  switch (segment)
    {
    case PT_RISCV_ATTRIBUTES:
      return "RISCV_ATTRIBUTES";
    }
  return NULL;
}

/* Return symbolic representation of section type.  */
const char *
riscv_section_type_name (int type,
			 char *buf __attribute__ ((unused)),
			 size_t len __attribute__ ((unused)))
{
  switch (type)
    {
    case SHT_RISCV_ATTRIBUTES:
      return "RISCV_ATTRIBUTES";
    }

  return NULL;
}

const char *
riscv_dynamic_tag_name (int64_t tag, char *buf __attribute__ ((unused)),
			size_t len __attribute__ ((unused)))
{
  switch (tag)
    {
    case DT_RISCV_VARIANT_CC:
      return "RISCV_VARIANT_CC";
    }
  return NULL;
}

bool
riscv_dynamic_tag_check (int64_t tag)
{
  return tag == DT_RISCV_VARIANT_CC;
}
