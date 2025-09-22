/* Abstract description of component ELF types.
   Copyright (C) 1998, 1999, 2000, 2002, 2004, 2007, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

/* ELF header.  */
#define Ehdr(Bits, Ext) \
START (Bits, Ehdr, Ext##Ehdr)						      \
  TYPE_EXTRA (unsigned char e_ident[EI_NIDENT];)			      \
  TYPE_XLATE (memmove (tdest->e_ident, tsrc->e_ident, EI_NIDENT);)	      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), e_type)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), e_machine)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), e_version)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Addr), e_entry)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Off), e_phoff)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Off), e_shoff)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), e_flags)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), e_ehsize)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), e_phentsize)			      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), e_phnum)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), e_shentsize)			      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), e_shnum)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), e_shstrndx)			      \
END (Bits, Ext##Ehdr)

#define Ehdr32(Ext) \
  Ehdr(32, Ext)
#define Ehdr64(Ext) \
  Ehdr(64, Ext)


/* Program header.  */
#define Phdr32(Ext) \
START (32, Phdr, Ext##Phdr)						      \
  TYPE_NAME (ElfW2(32, Ext##Word), p_type)				      \
  TYPE_NAME (ElfW2(32, Ext##Off), p_offset)				      \
  TYPE_NAME (ElfW2(32, Ext##Addr), p_vaddr)				      \
  TYPE_NAME (ElfW2(32, Ext##Addr), p_paddr)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), p_filesz)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), p_memsz)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), p_flags)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), p_align)				      \
END (32, Ext##Phdr)
#define Phdr64(Ext) \
START (64, Phdr, Ext##Phdr)						      \
  TYPE_NAME (ElfW2(64, Ext##Word), p_type)				      \
  TYPE_NAME (ElfW2(64, Ext##Word), p_flags)				      \
  TYPE_NAME (ElfW2(64, Ext##Off), p_offset)				      \
  TYPE_NAME (ElfW2(64, Ext##Addr), p_vaddr)				      \
  TYPE_NAME (ElfW2(64, Ext##Addr), p_paddr)				      \
  TYPE_NAME (ElfW2(64, Ext##Xword), p_filesz)				      \
  TYPE_NAME (ElfW2(64, Ext##Xword), p_memsz)				      \
  TYPE_NAME (ElfW2(64, Ext##Xword), p_align)				      \
END (64, Ext##Phdr)


/* Section header.  */
#define Shdr32(Ext) \
START (32, Shdr, Ext##Shdr)						      \
  TYPE_NAME (ElfW2(32, Ext##Word), sh_name)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), sh_type)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), sh_flags)				      \
  TYPE_NAME (ElfW2(32, Ext##Addr), sh_addr)				      \
  TYPE_NAME (ElfW2(32, Ext##Off), sh_offset)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), sh_size)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), sh_link)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), sh_info)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), sh_addralign)			      \
  TYPE_NAME (ElfW2(32, Ext##Word), sh_entsize)				      \
END (32, Ext##Shdr)
#define Shdr64(Ext) \
START (64, Shdr, Ext##Shdr)						      \
  TYPE_NAME (ElfW2(64, Ext##Word), sh_name)				      \
  TYPE_NAME (ElfW2(64, Ext##Word), sh_type)				      \
  TYPE_NAME (ElfW2(64, Ext##Xword), sh_flags)				      \
  TYPE_NAME (ElfW2(64, Ext##Addr), sh_addr)				      \
  TYPE_NAME (ElfW2(64, Ext##Off), sh_offset)				      \
  TYPE_NAME (ElfW2(64, Ext##Xword), sh_size)				      \
  TYPE_NAME (ElfW2(64, Ext##Word), sh_link)				      \
  TYPE_NAME (ElfW2(64, Ext##Word), sh_info)				      \
  TYPE_NAME (ElfW2(64, Ext##Xword), sh_addralign)			      \
  TYPE_NAME (ElfW2(64, Ext##Xword), sh_entsize)				      \
END (64, Ext##Shdr)


/* Symbol table.  */
#define Sym32(Ext) \
START (32, Sym, Ext##Sym)						      \
  TYPE_NAME (ElfW2(32, Ext##Word), st_name)				      \
  TYPE_NAME (ElfW2(32, Ext##Addr), st_value)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), st_size)				      \
  TYPE_EXTRA (unsigned char st_info;)					      \
  TYPE_XLATE (tdest->st_info = tsrc->st_info;)				      \
  TYPE_EXTRA (unsigned char st_other;)					      \
  TYPE_XLATE (tdest->st_other = tsrc->st_other;)			      \
  TYPE_NAME (ElfW2(32, Ext##Half), st_shndx)				      \
END (32, Ext##Sym)
#define Sym64(Ext) \
START (64, Sym, Ext##Sym)						      \
  TYPE_NAME (ElfW2(64, Ext##Word), st_name)				      \
  TYPE_EXTRA (unsigned char st_info;)					      \
  TYPE_XLATE (tdest->st_info = tsrc->st_info;)				      \
  TYPE_EXTRA (unsigned char st_other;)					      \
  TYPE_XLATE (tdest->st_other = tsrc->st_other;)			      \
  TYPE_NAME (ElfW2(64, Ext##Half), st_shndx)				      \
  TYPE_NAME (ElfW2(64, Ext##Addr), st_value)				      \
  TYPE_NAME (ElfW2(64, Ext##Xword), st_size)				      \
END (64, Ext##Sym)


/* Relocation.  */
#define Rel32(Ext) \
START (32, Rel, Ext##Rel)						      \
  TYPE_NAME (ElfW2(32, Ext##Addr), r_offset)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), r_info)				      \
END (32, Ext##Rel)
#define Rel64(Ext) \
START (64, Rel, Ext##Rel)						      \
  TYPE_NAME (ElfW2(64, Ext##Addr), r_offset)				      \
  TYPE_NAME (ElfW2(64, Ext##Xword), r_info)				      \
END (64, Ext##Rel)

#define Rela32(Ext) \
START (32, Rela, Ext##Rela)						      \
  TYPE_NAME (ElfW2(32, Ext##Addr), r_offset)				      \
  TYPE_NAME (ElfW2(32, Ext##Word), r_info)				      \
  TYPE_NAME (ElfW2(32, Ext##Sword), r_addend)				      \
END (32, Ext##Rela)
#define Rela64(Ext) \
START (64, Rela, Ext##Rela)						      \
  TYPE_NAME (ElfW2(64, Ext##Addr), r_offset)				      \
  TYPE_NAME (ElfW2(64, Ext##Xword), r_info)				      \
  TYPE_NAME (ElfW2(64, Ext##Sxword), r_addend)				      \
END (64, Ext##Rela)


/* Note entry header.  */
#define Note(Bits, Ext) \
START (Bits, Nhdr, Ext##Nhdr)						      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), n_namesz)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), n_descsz)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), n_type)				      \
END (Bits, Ext##Nhdr)

#define Note32(Ext) \
  Note (32, Ext)
#define Note64(Ext) \
  Note (64, Ext)


/* Dynamic section data.  */
#define Dyn32(Ext) \
START (32, Dyn, Ext##Dyn)						      \
  TYPE_NAME (ElfW2(32, Ext##Sword), d_tag)				      \
  TYPE_EXTRA (union {)							      \
  TYPE_EXTRA (ElfW2(32, Ext##Word) d_val;)				      \
  TYPE_EXTRA (ElfW2(32, Ext##Addr) d_ptr;)				      \
  TYPE_XLATE (Elf32_cvt_Addr1 (&tdest->d_un.d_val, &tsrc->d_un.d_val);)	      \
  TYPE_EXTRA (ElfW2(32, Ext##Off) d_off;)				      \
  TYPE_EXTRA (} d_un;)							      \
END (32, Ext##Dyn)
#define Dyn64(Ext) \
START (64, Dyn, Ext##Dyn)						      \
  TYPE_NAME (ElfW2(64, Ext##Xword), d_tag)				      \
  TYPE_EXTRA (union {)							      \
  TYPE_EXTRA (ElfW2(64, Ext##Xword) d_val;)				      \
  TYPE_EXTRA (ElfW2(64, Ext##Addr) d_ptr;)				      \
  TYPE_XLATE (Elf64_cvt_Addr1 (&tdest->d_un.d_val, &tsrc->d_un.d_val);)	      \
  TYPE_EXTRA (} d_un;)							      \
END (64, Ext##Dyn)


#ifndef GENERATE_CONVERSION
/* Version definitions.  */
# define Verdef(Bits, Ext) \
START (Bits, Verdef, Ext##Verdef)					      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), vd_version)			      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), vd_flags)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), vd_ndx)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), vd_cnt)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vd_hash)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vd_aux)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vd_next)				      \
END (Bits, Ext##Verdef)

# define Verdef32(Ext) \
  Verdef (32, Ext)
# define Verdef64(Ext) \
  Verdef (64, Ext)

# define Verdaux(Bits, Ext) \
START (Bits, Verdaux, Ext##Verdaux)					      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vda_name)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vda_next)				      \
END (Bits, Ext##Verdaux)

# define Verdaux32(Ext) \
  Verdaux (32, Ext)
# define Verdaux64(Ext) \
  Verdaux (64, Ext)

/* Required versions.  */
# define Verneed(Bits, Ext) \
START (Bits, Verneed, Ext##Verneed)					      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), vn_version)			      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), vn_cnt)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vn_file)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vn_aux)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vn_next)				      \
END (Bits, Ext##Verneed)

# define Verneed32(Ext) \
  Verneed (32, Ext)
# define Verneed64(Ext) \
  Verneed (64, Ext)

# define Vernaux(Bits, Ext) \
START (Bits, Vernaux, Ext##Vernaux)					      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vna_hash)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), vna_flags)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), vna_other)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vna_name)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), vna_next)				      \
END (Bits, Ext##Vernaux)

# define Vernaux32(Ext) \
  Vernaux (32, Ext)
# define Vernaux64(Ext) \
  Vernaux (64, Ext)
#endif

/* Symbol information.  */
#define Syminfo(Bits, Ext) \
START (Bits, Syminfo, Ext##Syminfo)					      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), si_boundto)			      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), si_flags)				      \
END (Bits, Ext##Syminfo)

#define Syminfo32(Ext) \
  Syminfo (32, Ext)
#define Syminfo64(Ext) \
  Syminfo (64, Ext)

/* Move information.  */
#define Move(Bits, Ext) \
START (Bits, Move, Ext##Move)						      \
  TYPE_NAME (ElfW2(Bits, Ext##Xword), m_value)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Xword), m_info)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Xword), m_poffset)			      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), m_repeat)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Half), m_stride)				      \
END (Bits, Ext##Move)

#define Move32(Ext) \
  Move (32, Ext)
#define Move64(Ext) \
  Move (64, Ext)

#define Lib(Bits, Ext) \
START (Bits, Lib, Ext##Lib)						      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), l_name)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), l_time_stamp)			      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), l_checksum)			      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), l_version)				      \
  TYPE_NAME (ElfW2(Bits, Ext##Word), l_flags)				      \
END (Bits, Ext##Lib)

#define Lib32(Ext) \
  Lib (32, Ext)
#define Lib64(Ext) \
  Lib (64, Ext)

#define auxv_t32(Ext) \
START (32, auxv_t, Ext##auxv_t)						      \
  TYPE_NAME (ElfW2(32, Ext##Word), a_type)				      \
  TYPE_EXTRA (union {)							      \
  TYPE_EXTRA (ElfW2(32, Ext##Word) a_val;)				      \
  TYPE_XLATE (Elf32_cvt_Addr1 (&tdest->a_un.a_val, &tsrc->a_un.a_val);)	      \
  TYPE_EXTRA (} a_un;)							      \
END (32, Ext##auxv_t)
#define auxv_t64(Ext) \
START (64, auxv_t, Ext##auxv_t)						      \
  TYPE_NAME (ElfW2(64, Ext##Xword), a_type)				      \
  TYPE_EXTRA (union {)							      \
  TYPE_EXTRA (ElfW2(64, Ext##Xword) a_val;)				      \
  TYPE_XLATE (Elf64_cvt_Addr1 (&tdest->a_un.a_val, &tsrc->a_un.a_val);)	      \
  TYPE_EXTRA (} a_un;)							      \
END (64, Ext##auxv_t)

/* Note that there is actual compression data right after the Chdr.
   So we also have a separate conversion function for the whole
   section.  */
#define Chdr32(Ext) \
START (32, Chdr, Ext##Chdr)						\
  TYPE_NAME (ElfW2(32, Ext##Word), ch_type)				\
  TYPE_NAME (ElfW2(32, Ext##Word), ch_size)				\
  TYPE_NAME (ElfW2(32, Ext##Word), ch_addralign)			\
END (32, Ext##Chdr)

#define Chdr64(Ext) \
START (64, Chdr, Ext##Chdr)						\
  TYPE_NAME (ElfW2(64, Ext##Word), ch_type)				\
  TYPE_NAME (ElfW2(64, Ext##Word), ch_reserved)				\
  TYPE_NAME (ElfW2(64, Ext##Xword), ch_size)				\
  TYPE_NAME (ElfW2(64, Ext##Xword), ch_addralign)			\
END (64, Ext##Chdr)
