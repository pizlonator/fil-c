/* Create descriptor from ELF descriptor for processing file.
   Copyright (C) 2002-2011, 2014, 2015, 2017, 2018 Red Hat, Inc.
   Copyright (C) 2023, Mark J. Wielaard <mark@klomp.org>
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

#include <system.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libelfP.h"
#include "libdwP.h"


/* Section names.  (Note .debug_str_offsets is the largest 19 chars.)  */
static const char dwarf_scnnames[IDX_last][19] =
{
  [IDX_debug_info] = ".debug_info",
  [IDX_debug_types] = ".debug_types",
  [IDX_debug_abbrev] = ".debug_abbrev",
  [IDX_debug_addr] = ".debug_addr",
  [IDX_debug_aranges] = ".debug_aranges",
  [IDX_debug_line] = ".debug_line",
  [IDX_debug_line_str] = ".debug_line_str",
  [IDX_debug_frame] = ".debug_frame",
  [IDX_debug_loc] = ".debug_loc",
  [IDX_debug_loclists] = ".debug_loclists",
  [IDX_debug_pubnames] = ".debug_pubnames",
  [IDX_debug_str] = ".debug_str",
  [IDX_debug_str_offsets] = ".debug_str_offsets",
  [IDX_debug_macinfo] = ".debug_macinfo",
  [IDX_debug_macro] = ".debug_macro",
  [IDX_debug_ranges] = ".debug_ranges",
  [IDX_debug_rnglists] = ".debug_rnglists",
  [IDX_debug_cu_index] = ".debug_cu_index",
  [IDX_debug_tu_index] = ".debug_tu_index",
  [IDX_gnu_debugaltlink] = ".gnu_debugaltlink"
};
#define ndwarf_scnnames (sizeof (dwarf_scnnames) / sizeof (dwarf_scnnames[0]))

/* Map from section index to string section index.
   Non-string sections should have STR_SCN_IDX_last.  */
static const enum string_section_index scn_to_string_section_idx[IDX_last] =
{
  [IDX_debug_info] = STR_SCN_IDX_last,
  [IDX_debug_types] = STR_SCN_IDX_last,
  [IDX_debug_abbrev] = STR_SCN_IDX_last,
  [IDX_debug_addr] = STR_SCN_IDX_last,
  [IDX_debug_aranges] = STR_SCN_IDX_last,
  [IDX_debug_line] = STR_SCN_IDX_last,
  [IDX_debug_line_str] = STR_SCN_IDX_debug_line_str,
  [IDX_debug_frame] = STR_SCN_IDX_last,
  [IDX_debug_loc] = STR_SCN_IDX_last,
  [IDX_debug_loclists] = STR_SCN_IDX_last,
  [IDX_debug_pubnames] = STR_SCN_IDX_last,
  [IDX_debug_str] = STR_SCN_IDX_debug_str,
  [IDX_debug_str_offsets] = STR_SCN_IDX_last,
  [IDX_debug_macinfo] = STR_SCN_IDX_last,
  [IDX_debug_macro] = STR_SCN_IDX_last,
  [IDX_debug_ranges] = STR_SCN_IDX_last,
  [IDX_debug_rnglists] = STR_SCN_IDX_last,
  [IDX_debug_cu_index] = STR_SCN_IDX_last,
  [IDX_debug_tu_index] = STR_SCN_IDX_last,
  [IDX_gnu_debugaltlink] = STR_SCN_IDX_last
};

static enum dwarf_type
scn_dwarf_type (Dwarf *result, size_t shstrndx, Elf_Scn *scn)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    return TYPE_UNKNOWN;

  const char *scnname = elf_strptr (result->elf, shstrndx,
				    shdr->sh_name);
  if (scnname != NULL)
    {
      if (startswith (scnname, ".gnu.debuglto_.debug"))
	return TYPE_GNU_LTO;
      else if (strcmp (scnname, ".debug_cu_index") == 0
	       || strcmp (scnname, ".debug_tu_index") == 0
	       || strcmp (scnname, ".zdebug_cu_index") == 0
	       || strcmp (scnname, ".zdebug_tu_index") == 0)
	return TYPE_DWO;
      else if (startswith (scnname, ".debug_") || startswith (scnname, ".zdebug_"))
	{
	  size_t len = strlen (scnname);
	  if (strcmp (scnname + len - 4, ".dwo") == 0)
	    return TYPE_DWO;
	  else
	    return TYPE_PLAIN;
	}
    }
  return TYPE_UNKNOWN;
}
static Dwarf *
check_section (Dwarf *result, size_t shstrndx, Elf_Scn *scn, bool inscngrp)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;

  /* Get the section header data.  */
  shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    /* We may read /proc/PID/mem with only program headers mapped and section
       headers out of the mapped pages.  */
    goto err;

  /* Ignore any SHT_NOBITS sections.  Debugging sections should not
     have been stripped, but in case of a corrupt file we won't try
     to look at the missing data.  */
  if (unlikely (shdr->sh_type == SHT_NOBITS))
    return result;

  /* Make sure the section is part of a section group only iff we
     really need it.  If we are looking for the global (= non-section
     group debug info) we have to ignore all the info in section
     groups.  If we are looking into a section group we cannot look at
     a section which isn't part of the section group.  */
  if (! inscngrp && (shdr->sh_flags & SHF_GROUP) != 0)
    /* Ignore the section.  */
    return result;


  /* We recognize the DWARF section by their names.  This is not very
     safe and stable but the best we can do.  */
  const char *scnname = elf_strptr (result->elf, shstrndx,
				    shdr->sh_name);
  if (scnname == NULL)
    {
      /* The section name must be valid.  Otherwise is the ELF file
	 invalid.  */
    err:
      Dwarf_Sig8_Hash_free (&result->sig8_hash);
      __libdw_seterrno (DWARF_E_INVALID_ELF);
      free (result);
      return NULL;
    }

  /* Recognize the various sections.  Most names start with .debug_.
     They might be compressed (and start with .z).  Or end with .dwo
     for split dwarf sections.  Or start with .gnu.debuglto_ for
     LTO debug sections.  We should only use one consistent set at
     a time.  We prefer PLAIN over DWO over LTO.  */
  size_t cnt;
  bool gnu_compressed = false;
  for (cnt = 0; cnt < ndwarf_scnnames; ++cnt)
    {
      /* .debug_cu_index and .debug_tu_index don't have a .dwo suffix,
	 but they are for DWO.  */
      if (result->type != TYPE_DWO
	  && (cnt == IDX_debug_cu_index || cnt == IDX_debug_tu_index))
	continue;
      bool need_dot_dwo =
	(result->type == TYPE_DWO
	 && cnt != IDX_debug_cu_index
	 && cnt != IDX_debug_tu_index);
      size_t dbglen = strlen (dwarf_scnnames[cnt]);
      size_t scnlen = strlen (scnname);
      if (strncmp (scnname, dwarf_scnnames[cnt], dbglen) == 0
	  && ((!need_dot_dwo && dbglen == scnlen)
	      || (need_dot_dwo
		  && scnlen == dbglen + 4
		  && strstr (scnname, ".dwo") == scnname + dbglen)))
	break;
      else if (scnname[0] == '.' && scnname[1] == 'z'
	       && (strncmp (&scnname[2], &dwarf_scnnames[cnt][1],
			    dbglen - 1) == 0
		   && ((!need_dot_dwo && scnlen == dbglen + 1)
		       || (need_dot_dwo
			   && scnlen == dbglen + 5
			   && strstr (scnname,
				      ".dwo") == scnname + dbglen + 1))))
	{
	  gnu_compressed = true;
	  break;
	}
      else if (scnlen > 14 /* .gnu.debuglto_ prefix. */
	       && startswith (scnname, ".gnu.debuglto_")
	       && strcmp (&scnname[14], dwarf_scnnames[cnt]) == 0)
	{
	  if (result->type == TYPE_GNU_LTO)
	    break;
	}
    }

  if (cnt >= ndwarf_scnnames)
    /* Not a debug section; ignore it. */
    return result;

  if (unlikely (result->sectiondata[cnt] != NULL))
    /* A section appears twice.  That's bad.  We ignore the section.  */
    return result;

  /* We cannot know whether or not a GNU compressed section has already
     been uncompressed or not, so ignore any errors.  */
  if (gnu_compressed)
    elf_compress_gnu (scn, 0, 0);

  if ((shdr->sh_flags & SHF_COMPRESSED) != 0)
    {
      if (elf_compress (scn, 0, 0) < 0)
	{
	  /* It would be nice if we could fail with a specific error.
	     But we don't know if this was an essential section or not.
	     So just continue for now. See also valid_p().  */
	  return result;
	}
    }

  /* Get the section data.  Should be raw bytes, no conversion needed.  */
  Elf_Data *data = elf_rawdata (scn, NULL);
  if (data == NULL)
    goto err;

  if (data->d_buf == NULL || data->d_size == 0)
    /* No data actually available, ignore it. */
    return result;

  /* We can now read the section data into results. */
  result->sectiondata[cnt] = data;

  /* If the section contains string data, we want to know a size of a prefix
     where any string will be null-terminated. */
  enum string_section_index string_section_idx = scn_to_string_section_idx[cnt];
  if (string_section_idx < STR_SCN_IDX_last)
    {
      size_t size = data->d_size;
      /* Reduce the size by the number of non-zero bytes at the end of the
	 section.  */
      while (size > 0 && *((const char *) data->d_buf + size - 1) != '\0')
	--size;
      result->string_section_size[string_section_idx] = size;
    }

  return result;
}

char *
__libdw_elfpath (int fd)
{
  /* strlen ("/proc/self/fd/") = 14 + strlen (<MAXINT>) = 10 + 1 = 25.  */
  char devfdpath[25];
  sprintf (devfdpath, "/proc/self/fd/%u", fd);
  return realpath (devfdpath, NULL);
}


void
__libdw_set_debugdir (Dwarf *dbg)
{
  if (dbg->elfpath == NULL || dbg->elfpath[0] != '/')
    return;
  size_t dirlen = strrchr (dbg->elfpath, '/') - dbg->elfpath + 1;
  dbg->debugdir = malloc (dirlen + 1);
  if (dbg->debugdir == NULL)
    return;
  memcpy (dbg->debugdir, dbg->elfpath, dirlen);
  dbg->debugdir[dirlen] = '\0';
}


/* Check whether all the necessary DWARF information is available.  */
static Dwarf *
valid_p (Dwarf *result)
{
  /* We looked at all the sections.  Now determine whether all the
     sections with debugging information we need are there.

     Require at least one section that can be read "standalone".  */
  if (likely (result != NULL)
      && unlikely (result->sectiondata[IDX_debug_info] == NULL
		   && result->sectiondata[IDX_debug_line] == NULL
		   && result->sectiondata[IDX_debug_frame] == NULL))
    {
      Dwarf_Sig8_Hash_free (&result->sig8_hash);
      __libdw_seterrno (DWARF_E_NO_DWARF);
      free (result);
      result = NULL;
    }

  /* We are setting up some "fake" CUs, which need an address size.
     Check the ELF class to come up with something reasonable.  */
  int elf_addr_size = 8;
  if (result != NULL)
    {
      GElf_Ehdr ehdr;
      if (gelf_getehdr (result->elf, &ehdr) == NULL)
	{
	  Dwarf_Sig8_Hash_free (&result->sig8_hash);
	  __libdw_seterrno (DWARF_E_INVALID_ELF);
	  free (result);
	  result = NULL;
	}
      else if (ehdr.e_ident[EI_CLASS] == ELFCLASS32)
	elf_addr_size = 4;
    }

  /* For dwarf_location_attr () we need a "fake" CU to indicate
     where the "fake" attribute data comes from.  This is a block
     inside the .debug_loc or .debug_loclists section.  */
  if (result != NULL && result->sectiondata[IDX_debug_loc] != NULL)
    {
      result->fake_loc_cu = malloc (sizeof (Dwarf_CU));
      if (unlikely (result->fake_loc_cu == NULL))
	{
	  Dwarf_Sig8_Hash_free (&result->sig8_hash);
	  __libdw_seterrno (DWARF_E_NOMEM);
	  free (result);
	  result = NULL;
	}
      else
	{
	  result->fake_loc_cu->sec_idx = IDX_debug_loc;
	  result->fake_loc_cu->dbg = result;
	  result->fake_loc_cu->startp
	    = result->sectiondata[IDX_debug_loc]->d_buf;
	  result->fake_loc_cu->endp
	    = (result->sectiondata[IDX_debug_loc]->d_buf
	       + result->sectiondata[IDX_debug_loc]->d_size);
	  result->fake_loc_cu->locs = NULL;
	  result->fake_loc_cu->address_size = elf_addr_size;
	  result->fake_loc_cu->offset_size = 4;
	  result->fake_loc_cu->version = 4;
	  result->fake_loc_cu->split = NULL;
	}
    }

  if (result != NULL && result->sectiondata[IDX_debug_loclists] != NULL)
    {
      result->fake_loclists_cu = malloc (sizeof (Dwarf_CU));
      if (unlikely (result->fake_loclists_cu == NULL))
	{
	  Dwarf_Sig8_Hash_free (&result->sig8_hash);
	  __libdw_seterrno (DWARF_E_NOMEM);
	  free (result->fake_loc_cu);
	  free (result);
	  result = NULL;
	}
      else
	{
	  result->fake_loclists_cu->sec_idx = IDX_debug_loclists;
	  result->fake_loclists_cu->dbg = result;
	  result->fake_loclists_cu->startp
	    = result->sectiondata[IDX_debug_loclists]->d_buf;
	  result->fake_loclists_cu->endp
	    = (result->sectiondata[IDX_debug_loclists]->d_buf
	       + result->sectiondata[IDX_debug_loclists]->d_size);
	  result->fake_loclists_cu->locs = NULL;
	  result->fake_loclists_cu->address_size = elf_addr_size;
	  result->fake_loclists_cu->offset_size = 4;
	  result->fake_loclists_cu->version = 5;
	  result->fake_loclists_cu->split = NULL;
	}
    }

  /* For DW_OP_constx/GNU_const_index and DW_OP_addrx/GNU_addr_index
     the dwarf_location_attr () will need a "fake" address CU to
     indicate where the attribute data comes from.  This is a just
     inside the .debug_addr section, if it exists.  */
  if (result != NULL && result->sectiondata[IDX_debug_addr] != NULL)
    {
      result->fake_addr_cu = malloc (sizeof (Dwarf_CU));
      if (unlikely (result->fake_addr_cu == NULL))
	{
	  Dwarf_Sig8_Hash_free (&result->sig8_hash);
	  __libdw_seterrno (DWARF_E_NOMEM);
	  free (result->fake_loc_cu);
	  free (result->fake_loclists_cu);
	  free (result);
	  result = NULL;
	}
      else
	{
	  result->fake_addr_cu->sec_idx = IDX_debug_addr;
	  result->fake_addr_cu->dbg = result;
	  result->fake_addr_cu->startp
	    = result->sectiondata[IDX_debug_addr]->d_buf;
	  result->fake_addr_cu->endp
	    = (result->sectiondata[IDX_debug_addr]->d_buf
	       + result->sectiondata[IDX_debug_addr]->d_size);
	  result->fake_addr_cu->locs = NULL;
	  result->fake_addr_cu->address_size = elf_addr_size;
	  result->fake_addr_cu->offset_size = 4;
	  result->fake_addr_cu->version = 5;
	  result->fake_addr_cu->split = NULL;
	}
    }

  if (result != NULL)
    {
      result->elfpath = __libdw_elfpath (result->elf->fildes);
      __libdw_set_debugdir(result);
    }

  return result;
}


static Dwarf *
global_read (Dwarf *result, Elf *elf, size_t shstrndx)
{
  Elf_Scn *scn = NULL;

  /* First check the type (PLAIN, DWO, LTO) we are looking for.  We
     prefer PLAIN if available over DWO, over LTO.  */
  while ((scn = elf_nextscn (elf, scn)) != NULL && result->type != TYPE_PLAIN)
    {
      enum dwarf_type type = scn_dwarf_type (result, shstrndx, scn);
      if (type > result->type)
	result->type = type;
    }

  scn = NULL;
  while (result != NULL && (scn = elf_nextscn (elf, scn)) != NULL)
    result = check_section (result, shstrndx, scn, false);

  return valid_p (result);
}


static Dwarf *
scngrp_read (Dwarf *result, Elf *elf, size_t shstrndx, Elf_Scn *scngrp)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (scngrp, &shdr_mem);
  if (shdr == NULL)
    {
      Dwarf_Sig8_Hash_free (&result->sig8_hash);
      __libdw_seterrno (DWARF_E_INVALID_ELF);
      free (result);
      return NULL;
    }

  if ((shdr->sh_flags & SHF_COMPRESSED) != 0
      && elf_compress (scngrp, 0, 0) < 0)
    {
      Dwarf_Sig8_Hash_free (&result->sig8_hash);
      __libdw_seterrno (DWARF_E_COMPRESSED_ERROR);
      free (result);
      return NULL;
    }

  /* SCNGRP is the section descriptor for a section group which might
     contain debug sections.  */
  Elf_Data *data = elf_getdata (scngrp, NULL);
  if (data == NULL)
    {
      /* We cannot read the section content.  Fail!  */
      Dwarf_Sig8_Hash_free (&result->sig8_hash);
      free (result);
      return NULL;
    }

  /* The content of the section is a number of 32-bit words which
     represent section indices.  The first word is a flag word.  */
  Elf32_Word *scnidx = (Elf32_Word *) data->d_buf;
  size_t cnt;

  /* First check the type (PLAIN, DWO, LTO) we are looking for.  We
     prefer PLAIN if available over DWO, over LTO.  */
  for (cnt = 1; cnt * sizeof (Elf32_Word) <= data->d_size; ++cnt)
    {
      Elf_Scn *scn = elf_getscn (elf, scnidx[cnt]);
      if (scn == NULL)
	{
	  /* A section group refers to a non-existing section.  Should
	     never happen.  */
	  Dwarf_Sig8_Hash_free (&result->sig8_hash);
	  __libdw_seterrno (DWARF_E_INVALID_ELF);
	  free (result);
	  return NULL;
	}

      enum dwarf_type type = scn_dwarf_type (result, shstrndx, scn);
      if (type > result->type)
	result->type = type;
    }

  for (cnt = 1; cnt * sizeof (Elf32_Word) <= data->d_size && result != NULL; ++cnt)
    {
      Elf_Scn *scn = elf_getscn (elf, scnidx[cnt]);
      assert (scn != NULL); // checked above
      result = check_section (result, shstrndx, scn, true);
      if (result == NULL)
	break;
    }

  return valid_p (result);
}


Dwarf *
dwarf_begin_elf (Elf *elf, Dwarf_Cmd cmd, Elf_Scn *scngrp)
{
  GElf_Ehdr *ehdr;
  GElf_Ehdr ehdr_mem;

  /* Get the ELF header of the file.  We need various pieces of
     information from it.  */
  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      if (elf_kind (elf) != ELF_K_ELF)
	__libdw_seterrno (DWARF_E_NOELF);
      else
	__libdw_seterrno (DWARF_E_GETEHDR_ERROR);

      return NULL;
    }


  /* Default memory allocation size.  */
  size_t mem_default_size = sysconf (_SC_PAGESIZE) - 4 * sizeof (void *);
  assert (sizeof (struct Dwarf) < mem_default_size);

  /* Allocate the data structure.  */
  Dwarf *result = calloc (1, sizeof (Dwarf));
  if (unlikely (result == NULL)
      || unlikely (Dwarf_Sig8_Hash_init (&result->sig8_hash, 11) < 0))
    {
      free (result);
      __libdw_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  /* Fill in some values.  */
  if ((BYTE_ORDER == LITTLE_ENDIAN && ehdr->e_ident[EI_DATA] == ELFDATA2MSB)
      || (BYTE_ORDER == BIG_ENDIAN && ehdr->e_ident[EI_DATA] == ELFDATA2LSB))
    result->other_byte_order = true;

  result->elf = elf;
  result->alt_fd = -1;
  result->dwp_fd = -1;

  /* Initialize the memory handling.  Initial blocks are allocated on first
     actual allocation.  */
  result->mem_default_size = mem_default_size;
  result->oom_handler = __libdw_oom;
  if (pthread_rwlock_init(&result->mem_rwl, NULL) != 0)
    {
      free (result);
      __libdw_seterrno (DWARF_E_NOMEM); /* no memory.  */
      return NULL;
    }
  result->mem_stacks = 0;
  result->mem_tails = NULL;

  if (cmd == DWARF_C_READ || cmd == DWARF_C_RDWR)
    {
      /* All sections are recognized by name, so pass the section header
	 string index along to easily get the section names.  */
      size_t shstrndx;
      if (elf_getshdrstrndx (elf, &shstrndx) != 0)
	{
	  Dwarf_Sig8_Hash_free (&result->sig8_hash);
	  __libdw_seterrno (DWARF_E_INVALID_ELF);
	  free (result);
	  return NULL;
	}

      /* If the caller provides a section group we get the DWARF
	 sections only from this section group.  Otherwise we search
	 for the first section with the required name.  Further
	 sections with the name are ignored.  The DWARF specification
	 does not really say this is allowed.  */
      if (scngrp == NULL)
	return global_read (result, elf, shstrndx);
      else
	return scngrp_read (result, elf, shstrndx, scngrp);
    }
  else if (cmd == DWARF_C_WRITE)
    {
      Dwarf_Sig8_Hash_free (&result->sig8_hash);
      __libdw_seterrno (DWARF_E_UNIMPL);
      free (result);
      return NULL;
    }

  Dwarf_Sig8_Hash_free (&result->sig8_hash);
  __libdw_seterrno (DWARF_E_INVALID_CMD);
  free (result);
  return NULL;
}
INTDEF(dwarf_begin_elf)
