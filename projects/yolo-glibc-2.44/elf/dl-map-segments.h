/* Map in a shared object's segments.  Generic version.
   Copyright (C) 1995-2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
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

#include <dl-load.h>
#include <setvmaname.h>

/* Map a segment and align it properly.  */

static __always_inline ElfW(Addr)
_dl_map_segment (const struct loadcmd *c, ElfW(Addr) mappref,
		 const size_t maplength, int fd)
{
  if (__glibc_likely (c->mapalign <= GLRO(dl_pagesize)))
    return (ElfW(Addr)) __mmap ((void *) mappref, maplength, c->prot,
				MAP_COPY|MAP_FILE, fd, c->mapoff);

  /* If the segment alignment > the page size, allocate enough space to
     ensure that the segment can be properly aligned.  */
  ElfW(Addr) maplen = (maplength >= c->mapalign
		       ? (maplength + c->mapalign)
		       : (2 * c->mapalign));
  ElfW(Addr) map_start = (ElfW(Addr)) __mmap ((void *) mappref, maplen,
					      PROT_NONE,
					      MAP_ANONYMOUS|MAP_COPY,
					      -1, 0);
  if (__glibc_unlikely ((void *) map_start == MAP_FAILED))
    return map_start;

  ElfW(Addr) map_start_aligned = ALIGN_UP (map_start, c->mapalign);
  map_start_aligned = (ElfW(Addr)) __mmap ((void *) map_start_aligned,
					   maplength, c->prot,
					   MAP_COPY|MAP_FILE|MAP_FIXED,
					   fd, c->mapoff);
  if (__glibc_unlikely ((void *) map_start_aligned == MAP_FAILED))
    __munmap ((void *) map_start, maplen);
  else
    {
      /* Unmap the unused regions.  */
      ElfW(Addr) delta = map_start_aligned - map_start;
      if (delta)
	__munmap ((void *) map_start, delta);
      ElfW(Addr) map_end = map_start_aligned + maplength;
      map_end = ALIGN_UP (map_end, GLRO(dl_pagesize));
      delta = map_start + maplen - map_end;
      if (delta)
	__munmap ((void *) map_end, delta);
    }

  return map_start_aligned;
}

/* This implementation assumes (as does the corresponding implementation
   of _dl_unmap_segments, in dl-unmap-segments.h) that shared objects
   are always laid out with all segments contiguous (or with gaps
   between them small enough that it's preferable to reserve all whole
   pages inside the gaps with PROT_NONE mappings rather than permitting
   other use of those parts of the address space).  */

static __always_inline const char *
_dl_map_segments (struct link_map *l, int fd,
                  const ElfW(Ehdr) *header, int type,
                  struct dl_pt_load_iterator *it,
                  const size_t maplength, bool has_holes,
                  struct link_map *loader)
{
  /* Fetch the first PT_LOAD segment.  _dl_pt_load_iterator_init already
     verified nloadcmds > 0, so this call always succeeds.  */
  struct loadcmd c = { 0 };
  _dl_pt_load_iterator_next (it, &c);

  if (__glibc_likely (type == ET_DYN))
    {
      /* This is a position-independent shared object.  We can let the
         kernel map it anywhere it likes, but we must have space for all
         the segments in their specified positions relative to the first.
         So we map the first segment without MAP_FIXED, but with its
         extent increased to cover all the segments.  Then we remove
         access from excess portion, and there is known sufficient space
         there to remap from the later segments.

         As a refinement, sometimes we have an address that we would
         prefer to map such objects at; but this is only a preference,
         the OS can do whatever it likes. */
      ElfW(Addr) mappref
        = (ELF_PREFERRED_ADDRESS (loader, maplength, c.mapstart)
           - MAP_BASE_ADDR (l));

      /* Remember which part of the address space this object uses.  */
      l->l_map_start = _dl_map_segment (&c, mappref, maplength, fd);
      if (__glibc_unlikely ((void *) l->l_map_start == MAP_FAILED))
        return DL_MAP_SEGMENTS_ERROR_MAP_SEGMENT;

      l->l_map_end = l->l_map_start + maplength;
      l->l_addr = l->l_map_start - c.mapstart;

      if (has_holes)
        {
          /* Change protection on the excess portion to disallow all access;
             the portions we do not remap later will be inaccessible as if
             unallocated.  Then jump into the normal segment-mapping loop to
             handle the portion of the segment past the end of the file
             mapping.  */
          if (__glibc_unlikely (it->last_mapstart < c.mapend))
            return N_("ELF load command address/offset not page-aligned");
          if (__glibc_unlikely
              (__mprotect ((caddr_t) (l->l_addr + c.mapend),
                           it->last_mapstart - c.mapend,
                           PROT_NONE) < 0))
            return DL_MAP_SEGMENTS_ERROR_MPROTECT;
        }

      l->l_contiguous = 1;

      goto postmap;
    }

  /* Remember which part of the address space this object uses.  */
  l->l_map_start = c.mapstart + l->l_addr;
  l->l_map_end = l->l_map_start + maplength;
  l->l_contiguous = !has_holes;

  do
    {
      if (c.mapend > c.mapstart
          /* Map the segment contents from the file.  */
          && (__mmap ((void *) (l->l_addr + c.mapstart),
                      c.mapend - c.mapstart, c.prot,
                      MAP_FIXED|MAP_COPY|MAP_FILE,
                      fd, c.mapoff)
              == MAP_FAILED))
        return DL_MAP_SEGMENTS_ERROR_MAP_SEGMENT;

    postmap:
      _dl_postprocess_loadcmd (l, header, &c);

      if (c.allocend > c.dataend)
        {
          /* Extra zero pages should appear at the end of this segment,
             after the data mapped from the file.   */
          ElfW(Addr) zero, zeroend, zeropage;

          zero = l->l_addr + c.dataend;
          zeroend = l->l_addr + c.allocend;
          zeropage = ((zero + GLRO(dl_pagesize) - 1)
                      & ~(GLRO(dl_pagesize) - 1));

          if (zeroend < zeropage)
            /* All the extra data is in the last page of the segment.
               We can just zero it.  */
            zeropage = zeroend;

          if (zeropage > zero)
            {
              /* Zero the final part of the last page of the segment.  */
              if (__glibc_unlikely ((c.prot & PROT_WRITE) == 0))
                {
                  /* Dag nab it.  */
                  if (__mprotect ((caddr_t) (zero
                                             & ~(GLRO(dl_pagesize) - 1)),
                                  GLRO(dl_pagesize), c.prot|PROT_WRITE) < 0)
                    return DL_MAP_SEGMENTS_ERROR_MPROTECT;
                }
              memset ((void *) zero, '\0', zeropage - zero);
              if (__glibc_unlikely ((c.prot & PROT_WRITE) == 0))
                __mprotect ((caddr_t) (zero & ~(GLRO(dl_pagesize) - 1)),
                            GLRO(dl_pagesize), c.prot);
            }

          if (zeroend > zeropage)
            {
              /* Map the remaining zero pages in from the zero fill FD.  */
              char bssname[ANON_VMA_NAME_MAX_LEN] = " glibc: .bss";

              caddr_t mapat;
              mapat = __mmap ((caddr_t) zeropage, zeroend - zeropage,
                              c.prot, MAP_ANON|MAP_PRIVATE|MAP_FIXED,
                              -1, 0);
              if (__glibc_unlikely (mapat == MAP_FAILED))
                return DL_MAP_SEGMENTS_ERROR_MAP_ZERO_FILL;
              if (__is_decorate_maps_enabled ())
                {
                  if (l->l_name != NULL && *l->l_name != '\0')
                    {
                      int i = strlen (bssname), j = 0;
                      int namelen = strlen (l->l_name);

                      bssname[i++] = ' ';
                      if (namelen > sizeof (bssname) - i - 1)
                        for (j = namelen - 1; j > 0; j--)
                          if (l->l_name[j - 1] == '/')
                            break;

                      for (; l->l_name[j] != '\0' && i < sizeof (bssname) - 1;
                           i++, j++)
                        {
                          char ch = l->l_name[j];
                          /* Replace non-printable characters and
                             \, `, $, [ and ].  */
                          if (ch <= 0x1f || ch >= 0x7f || strchr("\\`$[]", ch))
                            ch = '!';
                          bssname[i] = ch;
                        }
                      bssname[i] = 0;
                    }
                  __set_vma_name ((void*)zeropage, zeroend - zeropage, bssname);
                }
            }
        }
    }
  while (_dl_pt_load_iterator_next (it, &c));

  /* Notify ELF_PREFERRED_ADDRESS that we have to load this one
     fixed.  */
  ELF_FIXED_ADDRESS (loader, it->last_mapstart);

  return NULL;
}
