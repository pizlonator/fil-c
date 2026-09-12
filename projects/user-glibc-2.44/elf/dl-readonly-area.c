/* Check if range is within a read-only from a loaded ELF object.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <ldsodefs.h>

static bool
check_relro (const struct link_map *l, uintptr_t start, uintptr_t end)
{
  if (l->l_relro_addr != 0)
    {
      uintptr_t relro_start = ALIGN_DOWN (l->l_addr + l->l_relro_addr,
					  GLRO(dl_pagesize));
      uintptr_t relro_end = ALIGN_DOWN (l->l_addr + l->l_relro_addr
					+ l->l_relro_size,
					GLRO(dl_pagesize));
      /* RELRO is caved out from a RW segment, so the next range is either
	 RW or nonexistent.  */
      return relro_start <= start && end <= relro_end
	? dl_readonly_area_rdonly : dl_readonly_area_writable;

    }
  return dl_readonly_area_writable;
}

enum dl_readonly_area_error_type
_dl_readonly_area (const void *ptr, size_t size)
{
  struct dl_find_object dlfo;
  if (_dl_find_object ((void *)ptr, &dlfo) != 0)
    return dl_readonly_area_not_found;

  const struct link_map *l = dlfo.dlfo_link_map;
  uintptr_t ptr_start = (uintptr_t) ptr;
  uintptr_t ptr_end = ptr_start + size;

  for (const ElfW(Phdr) *ph = l->l_phdr; ph < &l->l_phdr[l->l_phnum]; ++ph)
    if (ph->p_type == PT_LOAD)
      {
	/* For segments with alignment larger than the page size,
	   _dl_map_segment allocates additional space that is mark as
	   PROT_NONE (so we can ignore).  */
	uintptr_t from = l->l_addr
	  + ALIGN_DOWN (ph->p_vaddr, GLRO(dl_pagesize));
	uintptr_t to = l->l_addr
	  + ALIGN_UP (ph->p_vaddr + ph->p_filesz, GLRO(dl_pagesize));

	/* Found an entry that at least partially covers the area.  */
	if (from < ptr_end && to > ptr_start)
	  {
	    if (ph->p_flags & PF_W)
	      return check_relro (l, ptr_start, ptr_end);

	    if ((ph->p_flags & PF_R) == 0)
	      return dl_readonly_area_writable;

	    if (from <= ptr_start && to >= ptr_end)
	      return dl_readonly_area_rdonly;
	    else if (from <= ptr_start)
	      size -= to - ptr_start;
	    else if (to >= ptr_end)
	      size -= ptr_end - from;
	    else
	      size -= to - from;

	    if (size == 0)
	      break;
	  }
      }

  return size == 0 ? dl_readonly_area_rdonly : dl_readonly_area_not_found;
}
