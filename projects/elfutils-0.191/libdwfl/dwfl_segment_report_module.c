/* Sniff out modules from ELF headers visible in memory segments.
   Copyright (C) 2008-2012, 2014, 2015, 2018 Red Hat, Inc.
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
#include "libelfP.h"	/* For NOTE_ALIGN4 and NOTE_ALIGN8.  */
#include "libdwflP.h"
#include "common.h"

#include <elf.h>
#include <gelf.h>
#include <inttypes.h>
#include <fcntl.h>

#include <system.h>


/* A good size for the initial read from memory, if it's not too costly.
   This more than covers the phdrs and note segment in the average 64-bit
   binary.  */

#define INITIAL_READ	1024

#if BYTE_ORDER == LITTLE_ENDIAN
# define MY_ELFDATA	ELFDATA2LSB
#else
# define MY_ELFDATA	ELFDATA2MSB
#endif

struct elf_build_id
{
  void *memory;
  size_t len;
  GElf_Addr vaddr;
};

struct read_state
{
  Dwfl *dwfl;
  Dwfl_Memory_Callback *memory_callback;
  void *memory_callback_arg;
  void **buffer;
  size_t *buffer_available;
};

/* Return user segment index closest to ADDR but not above it.
   If NEXT, return the closest to ADDR but not below it.  */
static int
addr_segndx (Dwfl *dwfl, size_t segment, GElf_Addr addr, bool next)
{
  int ndx = -1;
  do
    {
      if (dwfl->lookup_segndx[segment] >= 0)
	ndx = dwfl->lookup_segndx[segment];
      if (++segment >= dwfl->lookup_elts - 1)
	return next ? ndx + 1 : ndx;
    }
  while (dwfl->lookup_addr[segment] < addr);

  if (next)
    {
      while (dwfl->lookup_segndx[segment] < 0)
	if (++segment >= dwfl->lookup_elts - 1)
	  return ndx + 1;
      ndx = dwfl->lookup_segndx[segment];
    }

  return ndx;
}

/* Return whether there is SZ bytes available at PTR till END.  */

static bool
buf_has_data (const void *ptr, const void *end, size_t sz)
{
  return ptr < end && (size_t) (end - ptr) >= sz;
}

/* Read SZ bytes into *RETP from *PTRP (limited by END) in format EI_DATA.
   Function comes from src/readelf.c .  */

static bool
buf_read_ulong (unsigned char ei_data, size_t sz,
		const void **ptrp, const void *end, uint64_t *retp)
{
  if (! buf_has_data (*ptrp, end, sz))
    return false;

  union
  {
    uint64_t u64;
    uint32_t u32;
  } u;

  memcpy (&u, *ptrp, sz);
  (*ptrp) += sz;

  if (retp == NULL)
    return true;

  if (MY_ELFDATA != ei_data)
    {
      if (sz == 4)
	CONVERT (u.u32);
      else
	CONVERT (u.u64);
    }
  if (sz == 4)
    *retp = u.u32;
  else
    *retp = u.u64;
  return true;
}

/* Try to find matching entry for module from address MODULE_START to
   MODULE_END in NT_FILE note located at NOTE_FILE of NOTE_FILE_SIZE
   bytes in format EI_CLASS and EI_DATA.  */

static const char *
handle_file_note (GElf_Addr module_start, GElf_Addr module_end,
		  unsigned char ei_class, unsigned char ei_data,
		  const void *note_file, size_t note_file_size)
{
  if (note_file == NULL)
    return NULL;

  size_t sz;
  switch (ei_class)
    {
    case ELFCLASS32:
      sz = 4;
      break;
    case ELFCLASS64:
      sz = 8;
      break;
    default:
      return NULL;
    }

  const void *ptr = note_file;
  const void *end = note_file + note_file_size;
  uint64_t count;
  if (! buf_read_ulong (ei_data, sz, &ptr, end, &count))
    return NULL;
  if (! buf_read_ulong (ei_data, sz, &ptr, end, NULL)) // page_size
    return NULL;

  uint64_t maxcount = (size_t) (end - ptr) / (3 * sz);
  if (count > maxcount)
    return NULL;

  /* Where file names are stored.  */
  const char *fptr = ptr + 3 * count * sz;

  ssize_t firstix = -1;
  ssize_t lastix = -1;
  for (size_t mix = 0; mix < count; mix++)
    {
      uint64_t mstart, mend, moffset;
      if (! buf_read_ulong (ei_data, sz, &ptr, fptr, &mstart)
	  || ! buf_read_ulong (ei_data, sz, &ptr, fptr, &mend)
	  || ! buf_read_ulong (ei_data, sz, &ptr, fptr, &moffset))
	return NULL;
      if (mstart == module_start && moffset == 0)
	firstix = lastix = mix;
      if (firstix != -1 && mstart < module_end)
	lastix = mix;
      if (mend >= module_end)
	break;
    }
  if (firstix == -1)
    return NULL;

  const char *retval = NULL;
  for (ssize_t mix = 0; mix <= lastix; mix++)
    {
      const char *fnext = memchr (fptr, 0, (const char *) end - fptr);
      if (fnext == NULL)
	return NULL;
      if (mix == firstix)
	retval = fptr;
      if (firstix < mix && mix <= lastix && strcmp (fptr, retval) != 0)
	return NULL;
      fptr = fnext + 1;
    }
  return retval;
}

/* Return true iff we are certain ELF cannot match BUILD_ID of
   BUILD_ID_LEN bytes.  Pass DISK_FILE_HAS_BUILD_ID as false if it is
   certain ELF does not contain build-id (it is only a performance hit
   to pass it always as true).  */

static bool
invalid_elf (Elf *elf, bool disk_file_has_build_id,
             struct elf_build_id *build_id)
{
  if (! disk_file_has_build_id && build_id->len > 0)
    {
      /* Module found in segments with build-id is more reliable
	 than a module found via DT_DEBUG on disk without any
	 build-id.   */
      return true;
    }
  if (disk_file_has_build_id && build_id->len > 0)
    {
      const void *elf_build_id;
      ssize_t elf_build_id_len;

      /* If there is a build id in the elf file, check it.  */
      elf_build_id_len = INTUSE(dwelf_elf_gnu_build_id) (elf, &elf_build_id);
      if (elf_build_id_len > 0)
	{
	  if (build_id->len != (size_t) elf_build_id_len
	      || memcmp (build_id->memory, elf_build_id, build_id->len) != 0)
	    return true;
	}
    }
  return false;
}

static void
finish_portion (struct read_state *read_state,
		void **data, size_t *data_size)
{
  if (*data_size != 0 && *data != NULL)
    (*read_state->memory_callback) (read_state->dwfl, -1, data, data_size,
				    0, 0, read_state->memory_callback_arg);
}

static inline bool
read_portion (struct read_state *read_state,
	      void **data, size_t *data_size,
	      GElf_Addr start, size_t segment,
	      GElf_Addr vaddr, size_t filesz)
{
  /* Check whether we will have to read the segment data, or if it
     can be returned from the existing buffer.  */
  if (filesz > *read_state->buffer_available
      || vaddr - start > *read_state->buffer_available - filesz
      /* If we're in string mode, then don't consider the buffer we have
	 sufficient unless it contains the terminator of the string.  */
      || (filesz == 0 && memchr (vaddr - start + *read_state->buffer, '\0',
				 (*read_state->buffer_available
				  - (vaddr - start))) == NULL))
    {
      *data = NULL;
      *data_size = filesz;
      return !(*read_state->memory_callback) (read_state->dwfl,
					      addr_segndx (read_state->dwfl,
							   segment, vaddr,
							   false),
					      data, data_size, vaddr, filesz,
					      read_state->memory_callback_arg);
    }

  /* We already have this whole note segment from our initial read.  */
  *data = vaddr - start + (*read_state->buffer);
  *data_size = 0;
  return false;
}

int
dwfl_segment_report_module (Dwfl *dwfl, int ndx, const char *name,
			    Dwfl_Memory_Callback *memory_callback,
			    void *memory_callback_arg,
			    Dwfl_Module_Callback *read_eagerly,
			    void *read_eagerly_arg,
			    size_t maxread,
			    const void *note_file, size_t note_file_size,
			    const struct r_debug_info *r_debug_info)
{
  size_t segment = ndx;
  struct read_state read_state;

  if (segment >= dwfl->lookup_elts)
    segment = dwfl->lookup_elts - 1;

  while (segment > 0
	 && (dwfl->lookup_segndx[segment] > ndx
	     || dwfl->lookup_segndx[segment] == -1))
    --segment;

  while (dwfl->lookup_segndx[segment] < ndx)
    if (++segment == dwfl->lookup_elts)
      return 0;

  GElf_Addr start = dwfl->lookup_addr[segment];

  /* First read in the file header and check its sanity.  */

  void *buffer = NULL;
  size_t buffer_available = INITIAL_READ;
  Elf *elf = NULL;
  int fd = -1;

  read_state.dwfl = dwfl;
  read_state.memory_callback = memory_callback;
  read_state.memory_callback_arg = memory_callback_arg;
  read_state.buffer = &buffer;
  read_state.buffer_available = &buffer_available;

  /* We might have to reserve some memory for the phdrs.  Set to NULL
     here so we can always safely free it.  */
  void *phdrsp = NULL;

  /* Collect the build ID bits here.  */
  struct elf_build_id build_id;
  build_id.memory = NULL;
  build_id.len = 0;
  build_id.vaddr = 0;

  if (! (*memory_callback) (dwfl, ndx, &buffer, &buffer_available,
			    start, sizeof (Elf64_Ehdr), memory_callback_arg)
      || memcmp (buffer, ELFMAG, SELFMAG) != 0)
    goto out;

  /* Extract the information we need from the file header.  */
  const unsigned char *e_ident;
  unsigned char ei_class;
  unsigned char ei_data;
  uint16_t e_type;
  union
  {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
  } ehdr;
  GElf_Off phoff;
  uint_fast16_t phnum;
  uint_fast16_t phentsize;
  GElf_Off shdrs_end;
  Elf_Data xlatefrom =
    {
      .d_type = ELF_T_EHDR,
      .d_buf = (void *) buffer,
      .d_version = EV_CURRENT,
    };
  Elf_Data xlateto =
    {
      .d_type = ELF_T_EHDR,
      .d_buf = &ehdr,
      .d_size = sizeof ehdr,
      .d_version = EV_CURRENT,
    };
  e_ident = ((const unsigned char *) buffer);
  ei_class = e_ident[EI_CLASS];
  ei_data = e_ident[EI_DATA];
  /* buffer may be unaligned, in which case xlatetom would not work.
     xlatetom does work when the in and out d_buf are equal (but not
     for any other overlap).  */
  size_t ehdr_align = (ei_class == ELFCLASS32
		       ? __alignof__ (Elf32_Ehdr)
		       : __alignof__ (Elf64_Ehdr));
  if (((uintptr_t) buffer & (ehdr_align - 1)) != 0)
    {
      memcpy (&ehdr, buffer,
	      (ei_class == ELFCLASS32
	       ? sizeof (Elf32_Ehdr)
	       : sizeof (Elf64_Ehdr)));
      xlatefrom.d_buf = &ehdr;
    }
  switch (ei_class)
    {
    case ELFCLASS32:
      xlatefrom.d_size = sizeof (Elf32_Ehdr);
      if (elf32_xlatetom (&xlateto, &xlatefrom, ei_data) == NULL)
	goto out;
      e_type = ehdr.e32.e_type;
      phoff = ehdr.e32.e_phoff;
      phnum = ehdr.e32.e_phnum;
      phentsize = ehdr.e32.e_phentsize;
      if (phentsize != sizeof (Elf32_Phdr))
	goto out;
      /* NOTE if the number of sections is > 0xff00 then e_shnum
	 is zero and the actual number would come from the section
	 zero sh_size field. We ignore this here because getting shdrs
	 is just a nice bonus (see below in the type == PT_LOAD case
	 where we trim the last segment).  */
      shdrs_end = ehdr.e32.e_shoff + ehdr.e32.e_shnum * sizeof (Elf32_Shdr);
      break;

    case ELFCLASS64:
      xlatefrom.d_size = sizeof (Elf64_Ehdr);
      if (elf64_xlatetom (&xlateto, &xlatefrom, ei_data) == NULL)
	goto out;
      e_type = ehdr.e64.e_type;
      phoff = ehdr.e64.e_phoff;
      phnum = ehdr.e64.e_phnum;
      phentsize = ehdr.e64.e_phentsize;
      if (phentsize != sizeof (Elf64_Phdr))
	goto out;
      /* See the NOTE above for shdrs_end and ehdr.e32.e_shnum.  */
      shdrs_end = ehdr.e64.e_shoff + ehdr.e64.e_shnum * sizeof (Elf64_Shdr);
      break;

    default:
      goto out;
    }

  /* The file header tells where to find the program headers.
     These are what we need to find the boundaries of the module.
     Without them, we don't have a module to report.  */

  if (phnum == 0)
    goto out;

  xlatefrom.d_type = xlateto.d_type = ELF_T_PHDR;
  xlatefrom.d_size = phnum * phentsize;

  void *ph_buffer = NULL;
  size_t ph_buffer_size = 0;
  if (read_portion (&read_state, &ph_buffer, &ph_buffer_size,
		    start, segment,
		    start + phoff, xlatefrom.d_size))
    goto out;

  xlatefrom.d_buf = ph_buffer;

  bool class32 = ei_class == ELFCLASS32;
  size_t phdr_size = class32 ? sizeof (Elf32_Phdr) : sizeof (Elf64_Phdr);
  if (unlikely (phnum > SIZE_MAX / phdr_size))
    goto out;
  const size_t phdrsp_bytes = phnum * phdr_size;
  phdrsp = malloc (phdrsp_bytes);
  if (unlikely (phdrsp == NULL))
    goto out;

  xlateto.d_buf = phdrsp;
  xlateto.d_size = phdrsp_bytes;

  /* ph_ buffer may be unaligned, in which case xlatetom would not work.
     xlatetom does work when the in and out d_buf are equal (but not
     for any other overlap).  */
  size_t phdr_align = (class32
		       ? __alignof__ (Elf32_Phdr)
		       : __alignof__ (Elf64_Phdr));
  if (((uintptr_t) ph_buffer & (phdr_align - 1)) != 0)
    {
      memcpy (phdrsp, ph_buffer, phdrsp_bytes);
      xlatefrom.d_buf = phdrsp;
    }

  /* Track the bounds of the file visible in memory.  */
  GElf_Off file_trimmed_end = 0; /* Proper p_vaddr + p_filesz end.  */
  GElf_Off file_end = 0;	 /* Rounded up to effective page size.  */
  GElf_Off contiguous = 0;	 /* Visible as contiguous file from START.  */
  GElf_Off total_filesz = 0;	 /* Total size of data to read.  */

  /* Collect the bias between START and the containing PT_LOAD's p_vaddr.  */
  GElf_Addr bias = 0;
  bool found_bias = false;

  /* Collect the unbiased bounds of the module here.  */
  GElf_Addr module_start = -1l;
  GElf_Addr module_end = 0;
  GElf_Addr module_address_sync = 0;

  /* If we see PT_DYNAMIC, record it here.  */
  GElf_Addr dyn_vaddr = 0;
  GElf_Xword dyn_filesz = 0;

  Elf32_Phdr *p32 = phdrsp;
  Elf64_Phdr *p64 = phdrsp;
  if ((ei_class == ELFCLASS32
       && elf32_xlatetom (&xlateto, &xlatefrom, ei_data) == NULL)
      || (ei_class == ELFCLASS64
          && elf64_xlatetom (&xlateto, &xlatefrom, ei_data) == NULL))
    {
      found_bias = false; /* Trigger error check */
    }
  else
    {
      /* Consider each of the program headers we've read from the image.  */
      for (uint_fast16_t i = 0; i < phnum; ++i)
        {
          bool is32 = (ei_class == ELFCLASS32);
          GElf_Word type = is32 ? p32[i].p_type : p64[i].p_type;
          GElf_Addr vaddr = is32 ? p32[i].p_vaddr : p64[i].p_vaddr;
          GElf_Xword memsz = is32 ? p32[i].p_memsz : p64[i].p_memsz;
          GElf_Off offset = is32 ? p32[i].p_offset : p64[i].p_offset;
          GElf_Xword filesz = is32 ? p32[i].p_filesz : p64[i].p_filesz;
          GElf_Xword align = is32 ? p32[i].p_align : p64[i].p_align;

          if (type == PT_DYNAMIC)
            {
              dyn_vaddr = vaddr;
              dyn_filesz = filesz;
            }
          else if (type == PT_NOTE)
            {
              /* If we have already seen a build ID, we don't care any more.  */
              if (build_id.memory != NULL || filesz == 0)
                continue; /* Next header */

              /* We calculate from the p_offset of the note segment,
               because we don't yet know the bias for its p_vaddr.  */
              const GElf_Addr note_vaddr = start + offset;
              void *data = NULL;
              size_t data_size = 0;
              if (read_portion (&read_state, &data, &data_size,
				start, segment, note_vaddr, filesz))
                continue; /* Next header */

	      if (filesz > SIZE_MAX / sizeof (Elf32_Nhdr))
		continue;

              assert (sizeof (Elf32_Nhdr) == sizeof (Elf64_Nhdr));

              void *notes;
              if (ei_data == MY_ELFDATA
		  && (uintptr_t) data == (align == 8
					  ? NOTE_ALIGN8 ((uintptr_t) data)
					  : NOTE_ALIGN4 ((uintptr_t) data)))
                notes = data;
              else
                {
                  const unsigned int xencoding = ehdr.e32.e_ident[EI_DATA];

		  if (filesz > SIZE_MAX / sizeof (Elf32_Nhdr))
		    continue;
                  notes = malloc (filesz);
                  if (unlikely (notes == NULL))
                    continue; /* Next header */
                  xlatefrom.d_type = xlateto.d_type = (align == 8
                                                       ? ELF_T_NHDR8
						       : ELF_T_NHDR);
                  xlatefrom.d_buf = (void *) data;
                  xlatefrom.d_size = filesz;
                  xlateto.d_buf = notes;
                  xlateto.d_size = filesz;

		  /* data may be unaligned, in which case xlatetom would not work.
		     xlatetom does work when the in and out d_buf are equal (but not
		     for any other overlap).  */
		  if ((uintptr_t) data != (align == 8
					   ? NOTE_ALIGN8 ((uintptr_t) data)
					   : NOTE_ALIGN4 ((uintptr_t) data)))
		    {
		      memcpy (notes, data, filesz);
		      xlatefrom.d_buf = notes;
		    }

                  if (elf32_xlatetom (&xlateto, &xlatefrom, xencoding) == NULL)
                    {
                      free (notes);
                      finish_portion (&read_state, &data, &data_size);
                      continue;
                    }
                }

              const GElf_Nhdr *nh = notes;
              size_t len = 0;
              while (filesz - len > sizeof (*nh))
                {
		  len += sizeof (*nh);

		  size_t namesz = nh->n_namesz;
		  namesz = align == 8 ? NOTE_ALIGN8 (namesz) : NOTE_ALIGN4 (namesz);
		  if (namesz > filesz - len || len + namesz < namesz)
		    break;

		  void *note_name = notes + len;
		  len += namesz;

		  size_t descsz = nh->n_descsz;
		  descsz = align == 8 ? NOTE_ALIGN8 (descsz) : NOTE_ALIGN4 (descsz);
		  if (descsz > filesz - len || len + descsz < descsz)
		    break;

		  void *note_desc = notes + len;
		  len += descsz;

		  /* We don't handle very short or really large build-ids.  We need at
		     at least 3 and allow for up to 64 (normally ids are 20 long).  */
#define MIN_BUILD_ID_BYTES 3
#define MAX_BUILD_ID_BYTES 64
		  if (nh->n_type == NT_GNU_BUILD_ID
		      && nh->n_descsz >= MIN_BUILD_ID_BYTES
		      && nh->n_descsz <= MAX_BUILD_ID_BYTES
		      && nh->n_namesz == sizeof "GNU"
		      && !memcmp (note_name, "GNU", sizeof "GNU"))
		    {
		      build_id.vaddr = (note_desc
					- (const void *) notes
					+ note_vaddr);
		      build_id.len = nh->n_descsz;
		      build_id.memory = malloc (build_id.len);
		      if (likely (build_id.memory != NULL))
			memcpy (build_id.memory, note_desc, build_id.len);
		      break;
		    }

		  nh = (void *) notes + len;
		}

              if (notes != data)
                free (notes);
              finish_portion (&read_state, &data, &data_size);
            }
          else if (type == PT_LOAD)
            {
              align = (dwfl->segment_align > 1
                       ? dwfl->segment_align : (align ?: 1));

              GElf_Addr vaddr_end = (vaddr + memsz + align - 1) & -align;
              GElf_Addr filesz_vaddr = (filesz < memsz
                                        ? vaddr + filesz : vaddr_end);
              GElf_Off filesz_offset = filesz_vaddr - vaddr + offset;

              if (file_trimmed_end < offset + filesz)
                {
                  file_trimmed_end = offset + filesz;

                  /* Trim the last segment so we don't bother with zeros
                     in the last page that are off the end of the file.
                     However, if the extra bit in that page includes the
                     section headers, keep them.  */
                  if (shdrs_end <= filesz_offset
                      && shdrs_end > file_trimmed_end)
                    {
                      filesz += shdrs_end - file_trimmed_end;
                      file_trimmed_end = shdrs_end;
                    }
                }

              total_filesz += filesz;

              if (file_end < filesz_offset)
                {
                  file_end = filesz_offset;
                  if (filesz_vaddr - start == filesz_offset)
                    contiguous = file_end;
                }

              if (!found_bias && (offset & -align) == 0
                  && likely (filesz_offset >= phoff + phnum * phentsize))
                {
                  bias = start - vaddr;
                  found_bias = true;
                }

              if ((vaddr & -align) < module_start)
                {
                  module_start = vaddr & -align;
                  module_address_sync = vaddr + memsz;
                }

              if (module_end < vaddr_end)
                module_end = vaddr_end;
            }
        }
    }

  finish_portion (&read_state, &ph_buffer, &ph_buffer_size);

  /* We must have seen the segment covering offset 0, or else the ELF
     header we read at START was not produced by these program headers.  */
  if (unlikely (!found_bias))
    goto out;

  /* Now we know enough to report a module for sure: its bounds.  */
  module_start += bias;
  module_end += bias;

  dyn_vaddr += bias;

  /* NAME found from link map has precedence over DT_SONAME possibly read
     below.  */
  bool name_is_final = false;

  /* Try to match up DYN_VADDR against L_LD as found in link map.
     Segments sniffing may guess invalid address as the first read-only memory
     mapping may not be dumped to the core file (if ELF headers are not dumped)
     and the ELF header is dumped first with the read/write mapping of the same
     file at higher addresses.  */
  if (r_debug_info != NULL)
    for (const struct r_debug_info_module *module = r_debug_info->module;
	 module != NULL; module = module->next)
      if (module_start <= module->l_ld && module->l_ld < module_end)
	{
	  /* L_LD read from link map must be right while DYN_VADDR is unsafe.
	     Therefore subtract DYN_VADDR and add L_LD to get a possibly
	     corrective displacement for all addresses computed so far.  */
	  GElf_Addr fixup = module->l_ld - dyn_vaddr;
	  if ((fixup & (dwfl->segment_align - 1)) == 0
	      && module_start + fixup <= module->l_ld
	      && module->l_ld < module_end + fixup)
	    {
	      module_start += fixup;
	      module_end += fixup;
	      dyn_vaddr += fixup;
	      bias += fixup;
	      if (module->name[0] != '\0')
		{
		  name = xbasename (module->name);
		  name_is_final = true;
		}
	      break;
	    }
	}

  if (r_debug_info != NULL)
    {
      bool skip_this_module = false;
      for (struct r_debug_info_module *module = r_debug_info->module;
	   module != NULL; module = module->next)
	if ((module_end > module->start && module_start < module->end)
	    || dyn_vaddr == module->l_ld)
	  {
	    if (module->elf != NULL
	        && invalid_elf (module->elf, module->disk_file_has_build_id,
				&build_id))
	      {
		/* If MODULE's build-id doesn't match the disk file's
		   build-id, close ELF only if MODULE and ELF refer to
		   different builds of files with the same name.  This
		   prevents premature closure of the correct ELF in cases
		   where segments of a module are non-contiguous in memory.  */
		if (name != NULL && module->name[0] != '\0'
		    && strcmp (xbasename (module->name), xbasename (name)) == 0)
		  {
		    elf_end (module->elf);
		    close (module->fd);
		    module->elf = NULL;
		    module->fd = -1;
		  }
	      }
	    else if (module->elf != NULL)
	      {
		/* This module has already been reported.  */
		skip_this_module = true;
	      }
	    else
	      {
		/* Only report this module if we haven't already done so.  */
		for (Dwfl_Module *mod = dwfl->modulelist; mod != NULL;
		     mod = mod->next)
		  if (mod->low_addr == module_start
		      && mod->high_addr == module_end)
		    skip_this_module = true;
	      }
	  }
      if (skip_this_module)
	goto out;
    }

  const char *file_note_name = handle_file_note (module_start, module_end,
						 ei_class, ei_data,
						 note_file, note_file_size);
  if (file_note_name)
    {
      name = file_note_name;
      name_is_final = true;
      bool invalid = false;
      fd = open (name, O_RDONLY);
      if (fd >= 0)
	{
	  Dwfl_Error error = __libdw_open_file (&fd, &elf, true, false);
	  if (error == DWFL_E_NOERROR)
	    invalid = invalid_elf (elf, true /* disk_file_has_build_id */,
                                   &build_id);
	}
      if (invalid)
	{
	  /* The file was there, but the build_id didn't match.  We
	     still want to report the module, but need to get the ELF
	     some other way if possible.  */
	  close (fd);
	  fd = -1;
	  elf_end (elf);
	  elf = NULL;
	}
    }

  /* Examine its .dynamic section to get more interesting details.
     If it has DT_SONAME, we'll use that as the module name.
     If it has a DT_DEBUG, then it's actually a PIE rather than a DSO.
     We need its DT_STRTAB and DT_STRSZ to decipher DT_SONAME,
     and they also tell us the essential portion of the file
     for fetching symbols.  */
  GElf_Addr soname_stroff = 0;
  GElf_Addr dynstr_vaddr = 0;
  GElf_Xword dynstrsz = 0;
  bool execlike = false;
  const size_t dyn_entsize = (ei_class == ELFCLASS32
			      ? sizeof (Elf32_Dyn) : sizeof (Elf64_Dyn));
  void *dyn_data = NULL;
  size_t dyn_data_size = 0;
  if (dyn_filesz != 0 && dyn_filesz % dyn_entsize == 0
      && ! read_portion (&read_state, &dyn_data, &dyn_data_size,
			 start, segment, dyn_vaddr, dyn_filesz))
    {
      if ((dyn_filesz / dyn_entsize) == 0
	  || dyn_filesz > (SIZE_MAX / dyn_entsize))
	goto out;
      void *dyns = malloc (dyn_filesz);
      Elf32_Dyn *d32 = dyns;
      Elf64_Dyn *d64 = dyns;
      if (unlikely (dyns == NULL))
	goto out;

      xlatefrom.d_type = xlateto.d_type = ELF_T_DYN;
      xlatefrom.d_buf = (void *) dyn_data;
      xlatefrom.d_size = dyn_filesz;
      xlateto.d_buf = dyns;
      xlateto.d_size = dyn_filesz;

      /* dyn_data may be unaligned, in which case xlatetom would not work.
	 xlatetom does work when the in and out d_buf are equal (but not
	 for any other overlap).  */
      bool is32 = (ei_class == ELFCLASS32);
      size_t dyn_align = (is32
			  ? __alignof__ (Elf32_Dyn)
			  : __alignof__ (Elf64_Dyn));
      if (((uintptr_t) dyn_data & (dyn_align - 1)) != 0)
	{
	  memcpy (dyns, dyn_data, dyn_filesz);
	  xlatefrom.d_buf = dyns;
	}

      if ((is32 && elf32_xlatetom (&xlateto, &xlatefrom, ei_data) != NULL)
          || (!is32 && elf64_xlatetom (&xlateto, &xlatefrom, ei_data) != NULL))
        {
          size_t n = (is32
		      ? (dyn_filesz / sizeof (Elf32_Dyn))
		      : (dyn_filesz / sizeof (Elf64_Dyn)));
          for (size_t i = 0; i < n; ++i)
            {
              GElf_Sxword tag = is32 ? d32[i].d_tag : d64[i].d_tag;
              GElf_Xword val = is32 ? d32[i].d_un.d_val : d64[i].d_un.d_val;

              if (tag == DT_DEBUG)
                execlike = true;
              else if (tag == DT_SONAME)
                soname_stroff = val;
              else if (tag == DT_STRTAB)
                dynstr_vaddr = val;
              else if (tag == DT_STRSZ)
                dynstrsz = val;
              else
                continue;

              if (soname_stroff != 0 && dynstr_vaddr != 0 && dynstrsz != 0)
                break;
            }
        }
      free (dyns);
    }
  finish_portion (&read_state, &dyn_data, &dyn_data_size);

  /* We'll use the name passed in or a stupid default if not DT_SONAME.  */
  if (name == NULL)
    name = e_type == ET_EXEC ? "[exe]" : execlike ? "[pie]" : "[dso]";

  void *soname = NULL;
  size_t soname_size = 0;
  if (! name_is_final && dynstrsz != 0 && dynstr_vaddr != 0)
    {
      /* We know the bounds of the .dynstr section.

	 The DYNSTR_VADDR pointer comes from the .dynamic section
	 (DT_STRTAB, detected above).  Ordinarily the dynamic linker
	 will have adjusted this pointer in place so it's now an
	 absolute address.  But sometimes .dynamic is read-only (in
	 vDSOs and odd architectures), and sometimes the adjustment
	 just hasn't happened yet in the memory image we looked at.
	 So treat DYNSTR_VADDR as an absolute address if it falls
	 within the module bounds, or try applying the phdr bias
	 when that adjusts it to fall within the module bounds.  */

      if ((dynstr_vaddr < module_start || dynstr_vaddr >= module_end)
	  && dynstr_vaddr + bias >= module_start
	  && dynstr_vaddr + bias < module_end)
	dynstr_vaddr += bias;

      if (unlikely (dynstr_vaddr + dynstrsz > module_end))
	dynstrsz = 0;

      /* Try to get the DT_SONAME string.  */
      if (soname_stroff != 0 && soname_stroff + 1 < dynstrsz
	  && ! read_portion (&read_state, &soname, &soname_size,
			     start, segment,
			     dynstr_vaddr + soname_stroff, 0))
	name = soname;
    }

  /* Now that we have chosen the module's name and bounds, report it.
     If we found a build ID, report that too.  */

  Dwfl_Module *mod = INTUSE(dwfl_report_module) (dwfl, name,
						 module_start, module_end);

  // !execlike && ET_EXEC is PIE.
  // execlike && !ET_EXEC is a static executable.
  if (mod != NULL && (execlike || ehdr.e32.e_type == ET_EXEC))
    mod->is_executable = true;

  if (likely (mod != NULL) && build_id.memory != NULL
      && unlikely (INTUSE(dwfl_module_report_build_id) (mod,
							build_id.memory,
							build_id.len,
							build_id.vaddr)))
    {
      mod->gc = true;
      mod = NULL;
    }

  /* At this point we do not need BUILD_ID or NAME any more.
     They have been copied.  */
  free (build_id.memory);
  build_id.memory = NULL;
  finish_portion (&read_state, &soname, &soname_size);

  if (unlikely (mod == NULL))
    {
      ndx = -1;
      goto out;
    }
  else
    ndx++;

  /* We have reported the module.  Now let the caller decide whether we
     should read the whole thing in right now.  */

  const GElf_Off cost = (contiguous < file_trimmed_end ? total_filesz
			 : buffer_available >= contiguous ? 0
			 : contiguous - buffer_available);
  const GElf_Off worthwhile = ((dynstr_vaddr == 0 || dynstrsz == 0) ? 0
			       : dynstr_vaddr + dynstrsz - start);
  const GElf_Off whole = MAX (file_trimmed_end, shdrs_end);

  if (elf == NULL
      && (*read_eagerly) (MODCB_ARGS (mod), &buffer, &buffer_available,
			  cost, worthwhile, whole, contiguous,
			  read_eagerly_arg, &elf)
      && elf == NULL)
    {
      /* The caller wants to read the whole file in right now, but hasn't
	 done it for us.  Fill in a local image of the virtual file.  */

      if (file_trimmed_end > maxread)
	file_trimmed_end = maxread;

      void *contents = calloc (1, file_trimmed_end);
      if (unlikely (contents == NULL))
	goto out;

      if (contiguous < file_trimmed_end)
	{
	  /* We can't use the memory image verbatim as the file image.
	     So we'll be reading into a local image of the virtual file.  */
          for (uint_fast16_t i = 0; i < phnum; ++i)
            {
              bool is32 = (ei_class == ELFCLASS32);
              GElf_Word type = is32 ? p32[i].p_type : p64[i].p_type;

              if (type != PT_LOAD)
                continue;

              GElf_Addr vaddr = is32 ? p32[i].p_vaddr : p64[i].p_vaddr;
              GElf_Off offset = is32 ? p32[i].p_offset : p64[i].p_offset;
              GElf_Xword filesz = is32 ? p32[i].p_filesz : p64[i].p_filesz;

              /* Don't try to read beyond the actual end of file.  */
              if (offset >= file_trimmed_end)
                continue;

              void *into = contents + offset;
              size_t read_size = MIN (filesz, file_trimmed_end - offset);
              (*memory_callback) (dwfl, addr_segndx (dwfl, segment,
                                                     vaddr + bias, false),
                                  &into, &read_size, vaddr + bias, read_size,
                                  memory_callback_arg);
            }
	}
      else
	{
	  /* The whole file sits contiguous in memory,
	     but the caller didn't want to just do it.  */

	  const size_t have = MIN (buffer_available, file_trimmed_end);
	  memcpy (contents, buffer, have);

	  if (have < file_trimmed_end)
            {
	      void *into = contents + have;
	      size_t read_size = file_trimmed_end - have;
	      (*memory_callback) (dwfl,
				  addr_segndx (dwfl, segment,
					       start + have, false),
				  &into, &read_size, start + have,
				  read_size, memory_callback_arg);
            }
	}

      elf = elf_memory (contents, file_trimmed_end);
      if (unlikely (elf == NULL))
	free (contents);
      else
	elf->flags |= ELF_F_MALLOCED;
    }

  if (elf != NULL && mod->main.elf == NULL)
    {
      /* Install the file in the module.  */
      mod->main.elf = elf;
      mod->main.fd = fd;
      elf = NULL;
      fd = -1;
      mod->main.vaddr = module_start - bias;
      mod->main.address_sync = module_address_sync;
      mod->main_bias = bias;
    }

out:
  if (build_id.memory != NULL)
    free (build_id.memory);
  free (phdrsp);
  if (buffer != NULL)
    (*memory_callback) (dwfl, -1, &buffer, &buffer_available, 0, 0,
                        memory_callback_arg);

  if (elf != NULL)
    elf_end (elf);
  if (fd != -1)
    close (fd);
  return ndx;
}
