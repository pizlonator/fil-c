/* Report a module to libdwfl based on ELF program headers.
   Copyright (C) 2005-2010 Red Hat, Inc.
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

#include "libdwflP.h"
#include <fcntl.h>

/* We start every ET_REL module at a moderately aligned boundary.
   This keeps the low addresses easy to read compared to a layout
   starting at 0 (as when using -e).  It also makes it unlikely
   that a middle section will have a larger alignment and require
   rejiggering (see below).  */
#define REL_MIN_ALIGN	((GElf_Xword) 0x100)

bool
internal_function
__libdwfl_elf_address_range (Elf *elf, GElf_Addr base, bool add_p_vaddr,
			     bool sanity, GElf_Addr *vaddrp,
			     GElf_Addr *address_syncp, GElf_Addr *startp,
			     GElf_Addr *endp, GElf_Addr *biasp,
			     GElf_Half *e_typep)
{
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    {
    elf_error:
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return false;
    }

  GElf_Addr vaddr = 0;
  GElf_Addr address_sync = 0;
  GElf_Addr start = 0, end = 0, bias = 0;
  switch (ehdr->e_type)
    {
    case ET_REL:
      /* For a relocatable object, we do an arbitrary section layout.
	 By updating the section header in place, we leave the layout
	 information to be found by relocation.  */

      start = end = base = (base + REL_MIN_ALIGN - 1) & -REL_MIN_ALIGN;

      bool first = true;
      Elf_Scn *scn = NULL;
      while ((scn = elf_nextscn (elf, scn)) != NULL)
	{
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	  if (unlikely (shdr == NULL))
	    goto elf_error;

	  if (shdr->sh_flags & SHF_ALLOC)
	    {
	      const GElf_Xword align = shdr->sh_addralign ?: 1;
	      const GElf_Addr next = (end + align - 1) & -align;
	      if (shdr->sh_addr == 0
		  /* Once we've started doing layout we have to do it all,
		     unless we just laid out the first section at 0 when
		     it already was at 0.  */
		  || (bias == 0 && end > start && end != next))
		{
		  shdr->sh_addr = next;
		  if (end == base)
		    /* This is the first section assigned a location.
		       Use its aligned address as the module's base.  */
		    start = base = shdr->sh_addr;
		  else if (unlikely (base & (align - 1)))
		    {
		      /* If BASE has less than the maximum alignment of
			 any section, we eat more than the optimal amount
			 of padding and so make the module's apparent
			 size come out larger than it would when placed
			 at zero.  So reset the layout with a better base.  */

		      start = end = base = (base + align - 1) & -align;
		      Elf_Scn *prev_scn = NULL;
		      do
			{
			  prev_scn = elf_nextscn (elf, prev_scn);
			  GElf_Shdr prev_shdr_mem;
			  GElf_Shdr *prev_shdr = gelf_getshdr (prev_scn,
							       &prev_shdr_mem);
			  if (unlikely (prev_shdr == NULL))
			    goto elf_error;
			  if (prev_shdr->sh_flags & SHF_ALLOC)
			    {
			      const GElf_Xword prev_align
				= prev_shdr->sh_addralign ?: 1;

			      prev_shdr->sh_addr
				= (end + prev_align - 1) & -prev_align;
			      end = prev_shdr->sh_addr + prev_shdr->sh_size;

			      if (unlikely (! gelf_update_shdr (prev_scn,
								prev_shdr)))
				goto elf_error;
			    }
			}
		      while (prev_scn != scn);
		      continue;
		    }

		  end = shdr->sh_addr + shdr->sh_size;
		  if (likely (shdr->sh_addr != 0)
		      && unlikely (! gelf_update_shdr (scn, shdr)))
		    goto elf_error;
		}
	      else
		{
		  /* The address is already assigned.  Just track it.  */
		  if (first || end < shdr->sh_addr + shdr->sh_size)
		    end = shdr->sh_addr + shdr->sh_size;
		  if (first || bias > shdr->sh_addr)
		    /* This is the lowest address in the module.  */
		    bias = shdr->sh_addr;

		  if ((shdr->sh_addr - bias + base) & (align - 1))
		    /* This section winds up misaligned using BASE.
		       Adjust BASE upwards to make it congruent to
		       the lowest section address in the file modulo ALIGN.  */
		    base = (((base + align - 1) & -align)
			    + (bias & (align - 1)));
		}

	      first = false;
	    }
	}

      if (bias != 0)
	{
	  /* The section headers had nonzero sh_addr values.  The layout
	     was already done.  We've just collected the total span.
	     Now just compute the bias from the requested base.  */
	  start = base;
	  end = end - bias + start;
	  bias = start - bias;
	}
      break;

      /* Everything else has to have program headers.  */

    case ET_EXEC:
    case ET_CORE:
      /* An assigned base address is meaningless for these.  */
      base = 0;
      add_p_vaddr = true;
      FALLTHROUGH;
    case ET_DYN:
    default:;
      size_t phnum;
      if (unlikely (elf_getphdrnum (elf, &phnum) != 0))
	goto elf_error;
      for (size_t i = 0; i < phnum; ++i)
	{
	  GElf_Phdr phdr_mem, *ph = gelf_getphdr (elf, i, &phdr_mem);
	  if (unlikely (ph == NULL))
	    goto elf_error;
	  if (ph->p_type == PT_LOAD)
	    {
	      vaddr = ph->p_vaddr & -ph->p_align;
	      address_sync = ph->p_vaddr + ph->p_memsz;
	      break;
	    }
	}
      if (add_p_vaddr)
	{
	  start = base + vaddr;
	  bias = base;
	}
      else
	{
	  start = base;
	  bias = base - vaddr;
	}

      for (size_t i = phnum; i-- > 0;)
	{
	  GElf_Phdr phdr_mem, *ph = gelf_getphdr (elf, i, &phdr_mem);
	  if (unlikely (ph == NULL))
	    goto elf_error;
	  if (ph->p_type == PT_LOAD
	      && ph->p_vaddr + ph->p_memsz > 0)
	    {
	      end = bias + (ph->p_vaddr + ph->p_memsz);
	      break;
	    }
	}

      if (end == 0 && sanity)
	{
	  __libdwfl_seterrno (DWFL_E_NO_PHDR);
	  return false;
	}
      break;
    }
  if (vaddrp)
    *vaddrp = vaddr;
  if (address_syncp)
    *address_syncp = address_sync;
  if (startp)
    *startp = start;
  if (endp)
    *endp = end;
  if (biasp)
    *biasp = bias;
  if (e_typep)
    *e_typep = ehdr->e_type;
  return true;
}

Dwfl_Module *
internal_function
__libdwfl_report_elf (Dwfl *dwfl, const char *name, const char *file_name,
		      int fd, Elf *elf, GElf_Addr base, bool add_p_vaddr,
		      bool sanity)
{
  GElf_Addr vaddr, address_sync, start, end, bias;
  GElf_Half e_type;
  if (! __libdwfl_elf_address_range (elf, base, add_p_vaddr, sanity, &vaddr,
				     &address_sync, &start, &end, &bias,
				     &e_type))
    return NULL;
  Dwfl_Module *m = INTUSE(dwfl_report_module) (dwfl, name, start, end);
  if (m != NULL)
    {
      if (m->main.name == NULL)
	{
	  m->main.name = strdup (file_name);
	  m->main.fd = fd;
	}
      else if ((fd >= 0 && m->main.fd != fd)
	       || strcmp (m->main.name, file_name))
	{
	overlap:
	  m->gc = true;
	  __libdwfl_seterrno (DWFL_E_OVERLAP);
	  return NULL;
	}

      /* Preinstall the open ELF handle for the module.  */
      if (m->main.elf == NULL)
	{
	  m->main.elf = elf;
	  m->main.vaddr = vaddr;
	  m->main.address_sync = address_sync;
	  m->main_bias = bias;
	  m->e_type = e_type;
	}
      else
	{
	  if (m->main_bias != bias
	      || m->main.vaddr != vaddr || m->main.address_sync != address_sync)
	    goto overlap;
	  elf_end (m->main.elf);
	  m->main.elf = elf;
	}
    }
  return m;
}

NEW_VERSION (dwfl_report_elf, ELFUTILS_0.156)
Dwfl_Module *
dwfl_report_elf (Dwfl *dwfl, const char *name, const char *file_name, int fd,
		 GElf_Addr base, bool add_p_vaddr)
{
  bool closefd = false;
  if (fd < 0)
    {
      closefd = true;
      fd = open (file_name, O_RDONLY);
      if (fd < 0)
	{
	  __libdwfl_seterrno (DWFL_E_ERRNO);
	  return NULL;
	}
    }

  Elf *elf;
  Dwfl_Error error = __libdw_open_file (&fd, &elf, closefd, false);
  if (error != DWFL_E_NOERROR)
    {
      __libdwfl_seterrno (error);
      return NULL;
    }

  Dwfl_Module *mod = __libdwfl_report_elf (dwfl, name, file_name,
					   fd, elf, base, add_p_vaddr, true);
  if (mod == NULL)
    {
      elf_end (elf);
      if (closefd)
	close (fd);
    }

  return mod;
}
NEW_INTDEF (dwfl_report_elf)

#ifdef SYMBOL_VERSIONING
Dwfl_Module *
  _compat_without_add_p_vaddr_dwfl_report_elf (Dwfl *dwfl, const char *name,
					       const char *file_name, int fd,
					       GElf_Addr base);
COMPAT_VERSION_NEWPROTO (dwfl_report_elf, ELFUTILS_0.122, without_add_p_vaddr)

Dwfl_Module *
_compat_without_add_p_vaddr_dwfl_report_elf (Dwfl *dwfl, const char *name,
					     const char *file_name, int fd,
					     GElf_Addr base)
{
  return dwfl_report_elf (dwfl, name, file_name, fd, base, true);
}
#endif
