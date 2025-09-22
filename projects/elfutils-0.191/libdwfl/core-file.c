/* Core file handling.
   Copyright (C) 2008-2010, 2013, 2015 Red Hat, Inc.
   Copyright (C) 2021 Mark J. Wielaard <mark@klomp.org>
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

#include <config.h>
#include "libelfP.h"	/* For NOTE_ALIGN.  */
#include "libdwflP.h"
#include <gelf.h>

/* On failure return, we update *NEXT to point back at OFFSET.  */
static inline Elf *
do_fail (int error, off_t *next, off_t offset)
{
    if (next != NULL)
      *next = offset;
    //__libelf_seterrno (error);
    __libdwfl_seterrno (DWFL_E (LIBELF, error));
    return NULL;
}

#define fail(error) do_fail (error, next, offset)

/* This is a prototype of what a new libelf interface might be.
   This implementation is pessimal for non-mmap cases and should
   be replaced by more diddling inside libelf internals.  */
static Elf *
elf_begin_rand (Elf *parent, off_t offset, off_t size, off_t *next)
{
  if (parent == NULL)
    return NULL;

  off_t min = (parent->kind == ELF_K_ELF ?
		(parent->class == ELFCLASS32
		 ? sizeof (Elf32_Ehdr) : sizeof (Elf64_Ehdr))
		: parent->kind == ELF_K_AR ? SARMAG
		: 0);

  if (unlikely (offset < min)
      || unlikely (offset >= (off_t) parent->maximum_size))
    return fail (ELF_E_RANGE);

  /* For an archive, fetch just the size field
     from the archive header to override SIZE.  */
  if (parent->kind == ELF_K_AR)
    {
      /* File size, in ASCII decimal, right-padded with ASCII spaces.
         Max 10 characters. Not zero terminated. So make this ar_size
         array one larger and explicitly zero terminate it.  As needed
         for strtoll.  */
      #define AR_SIZE_CHARS 10
      char ar_size[AR_SIZE_CHARS + 1];
      ar_size[AR_SIZE_CHARS] = '\0';

      if (unlikely (parent->maximum_size - offset < sizeof (struct ar_hdr)))
	return fail (ELF_E_RANGE);

      if (parent->map_address != NULL)
	memcpy (ar_size, parent->map_address + parent->start_offset + offset,
		AR_SIZE_CHARS);
      else if (unlikely (pread_retry (parent->fildes,
				      ar_size, AR_SIZE_CHARS,
				      parent->start_offset + offset
				      + offsetof (struct ar_hdr, ar_size))
			 != AR_SIZE_CHARS))
	return fail (ELF_E_READ_ERROR);

      offset += sizeof (struct ar_hdr);

      char *endp;
      size = strtoll (ar_size, &endp, 10);
      if (unlikely (endp == ar_size)
	  || unlikely ((off_t) parent->maximum_size - offset < size))
	return fail (ELF_E_INVALID_ARCHIVE);
    }

  if (unlikely ((off_t) parent->maximum_size - offset < size))
    return fail (ELF_E_RANGE);

  /* Even if we fail at this point, update *NEXT to point past the file.  */
  if (next != NULL)
    *next = offset + size;

  if (unlikely (offset == 0)
      && unlikely (size == (off_t) parent->maximum_size))
    return elf_clone (parent, parent->cmd);

  /* Note the image is guaranteed live only as long as PARENT
     lives.  Using elf_memory is quite suboptimal if the whole
     file is not mmap'd.  We really should have something like
     a generalization of the archive support.  */
  Elf_Data *data = elf_getdata_rawchunk (parent, offset, size, ELF_T_BYTE);
  if (data == NULL)
    return NULL;
  assert ((off_t) data->d_size == size);
  return elf_memory (data->d_buf, size);
}


int
dwfl_report_core_segments (Dwfl *dwfl, Elf *elf, size_t phnum, GElf_Phdr *notes)
{
  if (unlikely (dwfl == NULL))
    return -1;

  int result = 0;

  if (notes != NULL)
    notes->p_type = PT_NULL;

  for (size_t ndx = 0; result >= 0 && ndx < phnum; ++ndx)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (elf, ndx, &phdr_mem);
      if (unlikely (phdr == NULL))
	{
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  return -1;
	}
      switch (phdr->p_type)
	{
	case PT_LOAD:
	  result = dwfl_report_segment (dwfl, ndx, phdr, 0, NULL);
	  break;

	case PT_NOTE:
	  if (notes != NULL)
	    {
	      *notes = *phdr;
	      notes = NULL;
	    }
	  break;
	}
    }

  return result;
}

/* Never read more than this much without mmap.  */
#define MAX_EAGER_COST	8192

/* Dwfl_Module_Callback passed to and called by dwfl_segment_report_module
   to read in a segment as ELF image directly if possible or indicate an
   attempt must be made to read in the while segment right now.  */
static bool
core_file_read_eagerly (Dwfl_Module *mod,
			void **userdata __attribute__ ((unused)),
			const char *name __attribute__ ((unused)),
			Dwarf_Addr start __attribute__ ((unused)),
			void **buffer, size_t *buffer_available,
			GElf_Off cost, GElf_Off worthwhile,
			GElf_Off whole,
			GElf_Off contiguous __attribute__ ((unused)),
			void *arg, Elf **elfp)
{
  Elf *core = arg;

  /* The available buffer is often the whole segment when the core file
     was mmap'd if used together with the dwfl_elf_phdr_memory_callback.
     Which means that if it is complete we can just construct the whole
     ELF image right now without having to read in anything more.  */
  if (whole <= *buffer_available)
    {
      /* All there ever was, we already have on hand.  */

      if (core->map_address == NULL)
	{
	  /* We already malloc'd the buffer.  */
	  *elfp = elf_memory (*buffer, whole);
	  if (unlikely (*elfp == NULL))
	    return false;

	  (*elfp)->flags |= ELF_F_MALLOCED;
	  *buffer = NULL;
	  *buffer_available = 0;
	  return true;
	}

      /* We can use the image inside the core file directly.  */
      *elfp = elf_begin_rand (core, *buffer - core->map_address, whole, NULL);
      *buffer = NULL;
      *buffer_available = 0;
      return *elfp != NULL;
    }

  /* We don't have the whole file.  Which either means the core file
     wasn't mmap'd, but needs to still be read in, or that the segment
     is truncated.  Figure out if this is better than nothing.  */

  if (worthwhile == 0)
    /* Caller doesn't think so.  */
    return false;

  /*
    XXX would like to fall back to partial file via memory
    when build id find_elf fails
    also, link_map name may give file name from disk better than partial here
    requires find_elf hook re-doing the magic to fall back if no file found
  */

  if (whole > MAX_EAGER_COST && mod->build_id_len > 0)
    /* We can't cheaply read the whole file here, so we'd
       be using a partial file.  But there is a build ID that could
       help us find the whole file, which might be more useful than
       what we have.  We'll just rely on that.  */
    return false;

  /* The file is either small (most likely the vdso) or big and incomplete,
     but we don't have a build-id.  */

  if (core->map_address != NULL)
    /* It's cheap to get, so get it.  */
    return true;

  /* Only use it if there isn't too much to be read.  */
  return cost <= MAX_EAGER_COST;
}

static inline void
update_end (GElf_Phdr *pphdr, const GElf_Off align,
            GElf_Off *pend, GElf_Addr *pend_vaddr)
{
  *pend = (pphdr->p_offset + pphdr->p_filesz + align - 1) & -align;
  *pend_vaddr = (pphdr->p_vaddr + pphdr->p_memsz + align - 1) & -align;
}

/* Use following contiguous segments to get towards SIZE.  */
static inline bool
do_more (size_t size, GElf_Phdr *pphdr, const GElf_Off align,
         Elf *elf, GElf_Off start, int *pndx,
         GElf_Off *pend, GElf_Addr *pend_vaddr)
{
  while (*pend <= start || *pend - start < size)
    {
      if (pphdr->p_filesz < pphdr->p_memsz)
	/* This segment is truncated, so no following one helps us.  */
	return false;

      if (unlikely (gelf_getphdr (elf, (*pndx)++, pphdr) == NULL))
	return false;

      if (pphdr->p_type == PT_LOAD)
	{
	  if (pphdr->p_offset > *pend
	      || pphdr->p_vaddr > *pend_vaddr)
	    /* It's discontiguous!  */
	    return false;

	  update_end (pphdr, align, pend, pend_vaddr);
	}
    }
  return true;
}

#define more(size) do_more (size, &phdr, align, elf, start, &ndx, &end, &end_vaddr)

bool
dwfl_elf_phdr_memory_callback (Dwfl *dwfl, int ndx,
			       void **buffer, size_t *buffer_available,
			       GElf_Addr vaddr,
			       size_t minread,
			       void *arg)
{
  Elf *elf = arg;

  if (ndx == -1)
    {
      /* Called for cleanup.  */
      if (elf->map_address == NULL)
	free (*buffer);
      *buffer = NULL;
      *buffer_available = 0;
      return false;
    }

  const GElf_Off align = dwfl->segment_align ?: 1;
  GElf_Phdr phdr;

  do
    if (unlikely (gelf_getphdr (elf, ndx++, &phdr) == NULL))
      return false;
  while (phdr.p_type != PT_LOAD
	 || ((phdr.p_vaddr + phdr.p_memsz + align - 1) & -align) <= vaddr);

  GElf_Off start = vaddr - phdr.p_vaddr + phdr.p_offset;
  GElf_Off end;
  GElf_Addr end_vaddr;

  update_end (&phdr, align, &end, &end_vaddr);

  /* We need at least this much.  */
  if (! more (minread))
    return false;

  /* See how much more we can get of what the caller wants.  */
  (void) more (*buffer_available);

  /* If it's already on hand anyway, use as much as there is.  */
  if (elf->map_address != NULL && start < elf->maximum_size)
    (void) more (elf->maximum_size - start);

  /* Make sure we don't look past the end of the actual file,
     even if the headers tell us to.  */
  if (unlikely (end > elf->maximum_size))
    end = elf->maximum_size;

  /* If the file is too small, there is nothing at all to get.  */
  if (unlikely (start >= end))
    return false;

  if (end - start < minread)
    return false;

  if (elf->map_address != NULL)
    {
      void *contents = elf->map_address + elf->start_offset + start;
      size_t size = end - start;

      if (minread == 0)		/* String mode.  */
	{
	  const void *eos = memchr (contents, '\0', size);
	  if (unlikely (eos == NULL) || unlikely (eos == contents))
	    return false;
	  size = eos + 1 - contents;
	}

      if (*buffer == NULL)
	{
	  *buffer = contents;
	  *buffer_available = size;
	}
      else
	{
	  *buffer_available = MIN (size, *buffer_available);
	  memcpy (*buffer, contents, *buffer_available);
	}
    }
  else
    {
      void *into = *buffer;
      if (*buffer == NULL)
	{
	  *buffer_available = MIN (minread ?: 512,
				   MAX (4096, MIN (end - start,
						   *buffer_available)));
	  into = malloc (*buffer_available);
	  if (unlikely (into == NULL))
	    {
	      __libdwfl_seterrno (DWFL_E_NOMEM);
	      return false;
	    }
	}

      ssize_t nread = pread_retry (elf->fildes, into, *buffer_available, start);
      if (nread < (ssize_t) minread)
	{
	  if (into != *buffer)
	    free (into);
	  if (nread < 0)
	    __libdwfl_seterrno (DWFL_E_ERRNO);
	  return false;
	}

      if (minread == 0)		/* String mode.  */
	{
	  const void *eos = memchr (into, '\0', nread);
	  if (unlikely (eos == NULL) || unlikely (eos == into))
	    {
	      if (*buffer == NULL)
		free (into);
	      return false;
	    }
	  nread = eos + 1 - into;
	}

      if (*buffer == NULL)
	*buffer = into;
      *buffer_available = nread;
    }

  return true;
}

/* Free the contents of R_DEBUG_INFO without the R_DEBUG_INFO memory itself.  */

static void
clear_r_debug_info (struct r_debug_info *r_debug_info)
{
  while (r_debug_info->module != NULL)
    {
      struct r_debug_info_module *module = r_debug_info->module;
      r_debug_info->module = module->next;
      elf_end (module->elf);
      if (module->fd != -1)
	close (module->fd);
      free (module);
    }
}

bool
internal_function
__libdwfl_dynamic_vaddr_get (Elf *elf, GElf_Addr *vaddrp)
{
  size_t phnum;
  if (unlikely (elf_getphdrnum (elf, &phnum) != 0))
    return false;
  for (size_t i = 0; i < phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (elf, i, &phdr_mem);
      if (unlikely (phdr == NULL))
	return false;
      if (phdr->p_type == PT_DYNAMIC)
	{
	  *vaddrp = phdr->p_vaddr;
	  return true;
	}
    }
  return false;
}

NEW_VERSION (dwfl_core_file_report, ELFUTILS_0.158)
int
dwfl_core_file_report (Dwfl *dwfl, Elf *elf, const char *executable)
{
  size_t phnum;
  if (unlikely (elf_getphdrnum (elf, &phnum) != 0))
    {
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return -1;
    }

  bool cleanup_user_core = false;
  if (dwfl->user_core != NULL)
    free (dwfl->user_core->executable_for_core);
  if (executable == NULL)
    {
      if (dwfl->user_core != NULL)
	dwfl->user_core->executable_for_core = NULL;
    }
  else
    {
      if (dwfl->user_core == NULL)
	{
	  cleanup_user_core = true;
	  dwfl->user_core = calloc (1, sizeof (struct Dwfl_User_Core));
	  if (dwfl->user_core == NULL)
	    {
	      __libdwfl_seterrno (DWFL_E_NOMEM);
	      return -1;
	    }
	  dwfl->user_core->fd = -1;
	}
      dwfl->user_core->executable_for_core = strdup (executable);
      if (dwfl->user_core->executable_for_core == NULL)
	{
	  if (cleanup_user_core)
	    {
	      free (dwfl->user_core);
	      dwfl->user_core = NULL;
	    }
	  __libdwfl_seterrno (DWFL_E_NOMEM);
	  return -1;
	}
    }

  /* First report each PT_LOAD segment.  */
  GElf_Phdr notes_phdr;
  int ndx = dwfl_report_core_segments (dwfl, elf, phnum, &notes_phdr);
  if (unlikely (ndx <= 0))
    {
      if (cleanup_user_core)
	{
	  free (dwfl->user_core->executable_for_core);
	  free (dwfl->user_core);
	  dwfl->user_core = NULL;
	}
      return ndx;
    }

  /* Next, we should follow the chain from DT_DEBUG.  */

  const void *auxv = NULL;
  const void *note_file = NULL;
  size_t auxv_size = 0;
  size_t note_file_size = 0;
  if (likely (notes_phdr.p_type == PT_NOTE))
    {
      /* PT_NOTE -> NT_AUXV -> AT_PHDR -> PT_DYNAMIC -> DT_DEBUG */

      Elf_Data *notes = elf_getdata_rawchunk (elf,
					      notes_phdr.p_offset,
					      notes_phdr.p_filesz,
					      (notes_phdr.p_align == 8
					       ? ELF_T_NHDR8
					       : ELF_T_NHDR));
      if (likely (notes != NULL))
	{
	  size_t pos = 0;
	  GElf_Nhdr nhdr;
	  size_t name_pos;
	  size_t desc_pos;
	  while ((pos = gelf_getnote (notes, pos, &nhdr,
				      &name_pos, &desc_pos)) > 0)
	    if (nhdr.n_namesz == sizeof "CORE"
		&& !memcmp (notes->d_buf + name_pos, "CORE", sizeof "CORE"))
	      {
		if (nhdr.n_type == NT_AUXV)
		  {
		    auxv = notes->d_buf + desc_pos;
		    auxv_size = nhdr.n_descsz;
		  }
		if (nhdr.n_type == NT_FILE)
		  {
		    note_file = notes->d_buf + desc_pos;
		    note_file_size = nhdr.n_descsz;
		  }
	      }
	}
    }

  /* Now we have NT_AUXV contents.  From here on this processing could be
     used for a live process with auxv read from /proc.  */

  struct r_debug_info r_debug_info;
  memset (&r_debug_info, 0, sizeof r_debug_info);
  int retval = dwfl_link_map_report (dwfl, auxv, auxv_size,
				     dwfl_elf_phdr_memory_callback, elf,
				     &r_debug_info);
  int listed = retval > 0 ? retval : 0;

  /* Now sniff segment contents for modules hinted by information gathered
     from DT_DEBUG.  */

  ndx = 0;
  do
    {
      int seg = dwfl_segment_report_module (dwfl, ndx, NULL,
					    &dwfl_elf_phdr_memory_callback, elf,
					    core_file_read_eagerly, elf,
					    elf->maximum_size,
					    note_file, note_file_size,
					    &r_debug_info);
      if (unlikely (seg < 0))
	{
	  clear_r_debug_info (&r_debug_info);
	  return seg;
	}
      if (seg > ndx)
	{
	  ndx = seg;
	  ++listed;
	}
      else
	++ndx;
    }
  while (ndx < (int) phnum);

  /* Now report the modules from dwfl_link_map_report which were not filtered
     out by dwfl_segment_report_module.  */

  Dwfl_Module **lastmodp = &dwfl->modulelist;
  while (*lastmodp != NULL)
    lastmodp = &(*lastmodp)->next;
  for (struct r_debug_info_module *module = r_debug_info.module;
       module != NULL; module = module->next)
    {
      if (module->elf == NULL)
	continue;
      GElf_Addr file_dynamic_vaddr;
      if (! __libdwfl_dynamic_vaddr_get (module->elf, &file_dynamic_vaddr))
	continue;
      Dwfl_Module *mod;
      mod = __libdwfl_report_elf (dwfl, xbasename (module->name), module->name,
				  module->fd, module->elf,
				  module->l_ld - file_dynamic_vaddr,
				  true, true);
      if (mod == NULL)
	continue;
      ++listed;
      module->elf = NULL;
      module->fd = -1;
      /* Move this module to the end of the list, so that we end
	 up with a list in the same order as the link_map chain.  */
      if (mod->next != NULL)
	{
	  if (*lastmodp != mod)
	    {
	      lastmodp = &dwfl->modulelist;
	      while (*lastmodp != mod)
		lastmodp = &(*lastmodp)->next;
	    }
	  *lastmodp = mod->next;
	  mod->next = NULL;
	  while (*lastmodp != NULL)
	    lastmodp = &(*lastmodp)->next;
	  *lastmodp = mod;
	}
      lastmodp = &mod->next;
    }

  clear_r_debug_info (&r_debug_info);

  /* We return the number of modules we found if we found any.
     If we found none, we return -1 instead of 0 if there was an
     error rather than just nothing found.  */
  return listed > 0 ? listed : retval;
}
NEW_INTDEF (dwfl_core_file_report)

#ifdef SYMBOL_VERSIONING
int _compat_without_executable_dwfl_core_file_report (Dwfl *dwfl, Elf *elf);
COMPAT_VERSION_NEWPROTO (dwfl_core_file_report, ELFUTILS_0.146,
			 without_executable)

int
_compat_without_executable_dwfl_core_file_report (Dwfl *dwfl, Elf *elf)
{
  return dwfl_core_file_report (dwfl, elf, NULL);
}
#endif
