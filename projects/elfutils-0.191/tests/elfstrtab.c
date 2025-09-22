/* Test program for elf_strptr function.
   Copyright (C) 2015 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "system.h"

#include ELFUTILS_HEADER(elf)
#include <gelf.h>


/* Index of last string added.  Returned by add_string ().  */
static size_t stridx = 0;

/* Some random strings.  */
static char *str1;
static size_t str1_off;
static char *str2;
static size_t str2_off;
static char *str3;
static size_t str3_off;

/* First three strings we write out. They should always be there.  */
static char *orig_str1;
static size_t orig_str1_off;
static char *orig_str2;
static size_t orig_str2_off;
static char *orig_str3;
static size_t orig_str3_off;

static void
check_orig_strings (Elf *elf, int ndx, const char *msg)
{
  printf ("checking orig strings: %s\n", msg);

  const char *str = elf_strptr (elf, ndx, 0);
  printf ("\t'%s'\n", str);
  if (str == NULL || strcmp ("", str) != 0)
    exit (1);

  str = elf_strptr (elf, ndx, 1);
  printf ("\t'%s'\n", str);
  if (str == NULL || strcmp (".strings", str) != 0)
    exit (1);

  str = elf_strptr (elf, ndx, orig_str1_off);
  printf ("\t'%s'\n", str);
  if (str == NULL || strcmp (orig_str1, str) != 0)
    exit (1);

  str = elf_strptr (elf, ndx, orig_str2_off);
  printf ("\t'%s'\n", str);
  if (str == NULL || strcmp (orig_str2, str) != 0)
    exit (1);

  str = elf_strptr (elf, ndx, orig_str3_off);
  printf ("\t'%s'\n", str);
  if (str == NULL || strcmp (orig_str3, str) != 0)
    exit (1);
}

static void
check_strings (Elf *elf, int ndx, const char *msg)
{
  check_orig_strings (elf, ndx, msg);

  const char *str = elf_strptr (elf, ndx, str1_off);
  printf ("\t'%s'\n", str);
  if (str == NULL || strcmp (str1, str) != 0)
    exit (1);

  str = elf_strptr (elf, ndx, str2_off);
  printf ("\t'%s'\n", str);
  if (str == NULL || strcmp (str2, str) != 0)
    exit (1);

  str = elf_strptr (elf, ndx, str3_off);
  printf ("\t'%s'\n", str);
  if (str == NULL || strcmp (str3, str) != 0)
    exit (1);
}

/* Adds a string and returns the offset in the section.  */
static size_t
add_string (Elf_Scn *scn, char *str)
{
  size_t lastidx = stridx;
  size_t size = strlen (str) + 1;
  
  Elf_Data *data = elf_newdata (scn);
  if (data == NULL)
    {
      printf ("cannot create data SHSTRTAB section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  data->d_buf = str;
  data->d_type = ELF_T_BYTE;
  data->d_size = size;
  data->d_align = 1;
  data->d_version = EV_CURRENT;

  stridx += size;
  printf ("add_string: '%s', stridx: %zd, lastidx: %zd\n",
	  str, stridx, lastidx);
  return lastidx;
}

static void
check_elf (const char *fname, int class, int use_mmap)
{
  printf ("\nfname: %s\n", fname);
  stridx = 0;

  int fd = open (fname, O_RDWR | O_CREAT | O_TRUNC, DEFFILEMODE);
  if (fd == -1)
    {
      printf ("cannot open `%s': %s\n", fname, strerror (errno));
      exit (1);
    }

  Elf *elf = elf_begin (fd, use_mmap ? ELF_C_WRITE_MMAP : ELF_C_WRITE, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Create an ELF header.
  if (gelf_newehdr (elf, class) == 0)
    {
      printf ("cannot create ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      printf ("cannot get ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Initialize header.
  ehdr->e_ident[EI_DATA] = class == ELFCLASS64 ? ELFDATA2LSB : ELFDATA2MSB;
  ehdr->e_ident[EI_OSABI] = ELFOSABI_GNU;
  ehdr->e_type = ET_NONE;
  ehdr->e_machine = EM_X86_64;
  ehdr->e_version = EV_CURRENT;

  // Create strings section.
  Elf_Scn *scn = elf_newscn (elf);
  if (scn == NULL)
    {
      printf ("cannot create strings section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Add an empty string to the table as NUL entry for section zero.
  add_string (scn, "");

  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    {
      printf ("cannot get header for strings section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  shdr->sh_type = SHT_STRTAB;
  shdr->sh_flags = 0;
  shdr->sh_addr = 0;
  shdr->sh_link = SHN_UNDEF;
  shdr->sh_info = SHN_UNDEF;
  shdr->sh_addralign = 1;
  shdr->sh_entsize = 0;
  shdr->sh_name = add_string (scn, ".strings");

  // We have to store the section strtab index in the ELF header.
  // So sections have actual names.
  int ndx = elf_ndxscn (scn);
  ehdr->e_shstrndx = ndx;

  if (gelf_update_ehdr (elf, ehdr) == 0)
    {
      printf ("cannot update ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Add some random strings. These are the original ones. They should
  // always be there (together with the empty "" and .strings section
  // name strings.
  orig_str1 = "elfutils";
  orig_str1_off = add_string (scn, orig_str1);
  orig_str2 = "strtabelf";
  orig_str2_off = add_string (scn, orig_str2);
  orig_str3 = "three";
  orig_str3_off = add_string (scn, orig_str3);

  // Finished strings section, update the header.
  if (gelf_update_shdr (scn, shdr) == 0)
    {
      printf ("cannot update STRTAB section header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Let the library compute the internal structure information.
  if (elf_update (elf, ELF_C_NULL) < 0)
    {
      printf ("failure in elf_update(NULL): %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Check our strings are there.
  check_orig_strings (elf, ndx, "first elf_update, before write");

  // Write everything to disk.
  if (elf_update (elf, ELF_C_WRITE) < 0)
    {
      printf ("failure in elf_update(WRITE): %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Check out strings are there.
  check_orig_strings (elf, ndx, "first elf_update, after write");

  // Add some more random strings.  These will not be written to disk.
  scn = elf_getscn (elf, ndx);
  if (scn == NULL)
    {
      printf ("couldn't re-get strings section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  str1 = "elfutils2";
  str1_off = add_string (scn, str1);
  str2 = "strtabelf2";
  str2_off = add_string (scn, str2);
  str3 = "three2";
  str3_off = add_string (scn, str3);

  // Update internal structure information again.
  if (elf_update (elf, ELF_C_NULL) < 0)
    {
      printf ("failure in re-elf_update(NULL): %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Check our new strings are there.
  check_strings (elf, ndx, "first extra strings");

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  /* Read the ELF from disk now.  */
  fd = open (fname, O_RDWR);
  if (fd == -1)
    {
      printf ("cannot open `%s' read-only: %s\n", fname, strerror (errno));
      exit (1);
    }

  elf = elf_begin (fd, use_mmap ? ELF_C_RDWR_MMAP : ELF_C_RDWR, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor read-only: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* Are our strings there?  */
  check_orig_strings (elf, ndx, "read ELF file, orig strings");

  // Add some more random strings.
  scn = elf_getscn (elf, ndx);
  if (scn == NULL)
    {
      printf ("couldn't re-get strings section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    {
      printf ("cannot get header for strings section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Reset stridx to end of section.
  printf ("sh_size: %" PRIu64 "\n", shdr->sh_size);
  stridx = shdr->sh_size;

  str1 = "0123456789";
  str1_off = add_string (scn, str1);
  str2 = "supercalifragilisticexpialidocious";
  str2_off = add_string (scn, str2);
  str3 = "forty-two";
  str3_off = add_string (scn, str3);

  // Update internal structure information.
  if (elf_update (elf, ELF_C_NULL) < 0)
    {
      printf ("failure in rw-elf_update(NULL): %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* Check our new strings are there.  */
  check_strings (elf, ndx, "read file, added strings");

  // Write updated ELF file.
  if (elf_update (elf, ELF_C_WRITE) < 0)
    {
      printf ("failure in re-elf_update(NULL): %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  // And read it in one last time.
  fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open `%s' read-only: %s\n", fname, strerror (errno));
      exit (1);
    }

  elf = elf_begin (fd, use_mmap ? ELF_C_READ_MMAP : ELF_C_READ, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor read-only: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* Are all our strings there?  */
  check_strings (elf, ndx, "all together now");

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  unlink (fname);
}

int
main (int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
  elf_version (EV_CURRENT);

  // Fill holes with something non-zero to more easily spot
  // unterminated strings.
  elf_fill ('X');

  check_elf ("strtab.elf.32", ELFCLASS32, 0);
  check_elf ("strtab.elf.32.mmap", ELFCLASS32, 1);
  check_elf ("strtab.elf.64", ELFCLASS64, 0);
  check_elf ("strtab.elf.64.mmap", ELFCLASS64, 1);

  return 0;
}

