/* Support for reading ELF files.
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


int process_elf64_file (const char *file_name, const char *lib,
			int *flag, unsigned int *isa_level, char **soname,
			void *file_contents, size_t file_length);

#define SUPPORTED_ELF_FLAGS \
  (EF_LARCH_ABI_DOUBLE_FLOAT | EF_LARCH_ABI_SOFT_FLOAT)

/* Returns 0 if everything is ok, != 0 in case of error.  */
int
process_elf_file (const char *file_name, const char *lib, int *flag,
		  unsigned int *isa_level, char **soname, void *file_contents,
		  size_t file_length)
{
  ElfW(Ehdr) *elf_header = (ElfW(Ehdr) *) file_contents;
  Elf64_Ehdr *elf64_header = (Elf64_Ehdr *) elf_header;
  int ret;
  long flags;

  /* LoongArch libraries are always libc.so.6+.  */
  *flag = FLAG_ELF_LIBC6;

  ret = process_elf64_file (file_name, lib, flag, isa_level, soname,
				file_contents, file_length);

  /* The EF_LARCH_OBJABI_V1 flag indicate which set of static relocations
   the object might use and it only considered during static linking,
   it does not reflect in runtime relocations.  However some binutils
   version might set it on dynamic shared object, so clear it to avoid
   see the SO as unsupported.  */
  flags = elf64_header->e_flags & ~EF_LARCH_OBJABI_V1;

  /* LoongArch linkers encode the floating point ABI as part of the ELF headers.  */
  switch (flags & SUPPORTED_ELF_FLAGS)
    {
      case EF_LARCH_ABI_SOFT_FLOAT:
        *flag |= FLAG_LARCH_FLOAT_ABI_SOFT;
	break;
      case EF_LARCH_ABI_DOUBLE_FLOAT:
        *flag |= FLAG_LARCH_FLOAT_ABI_DOUBLE;
	break;
      default:
        return 1;
    }

  /* If there are any other ELF flags set then glibc doesn't support this
     library.  */
  if (flags & ~SUPPORTED_ELF_FLAGS)
    return 1;

  return ret;
}

#undef __ELF_NATIVE_CLASS
#undef process_elf_file
#define process_elf_file process_elf64_file
#define __ELF_NATIVE_CLASS 64
#include "elf/readelflib.c"
