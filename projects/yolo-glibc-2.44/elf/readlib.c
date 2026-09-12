/* Copyright (C) 1999-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation; version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.  */

/* The code in this file and in readelflib is a heavily simplified
   version of the readelf program that's part of the current binutils
   development version.  Besides the simplification, it has also been
   modified to read some other file formats.  */

#include <a.out.h>
#include <elf.h>
#include <error.h>
#include <libintl.h>
#include <link.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <gnu/lib-names.h>

#include <ldconfig.h>
#include <endswith.h>

#define Elf32_CLASS ELFCLASS32
#define Elf64_CLASS ELFCLASS64

struct known_names
{
  const char *soname;
  int flag;
};

/* Check if string corresponds to a GDB extension file.  */
static bool
is_gdb_extension_file (const char *name)
{
  size_t len = strlen (name);
  return (endswithn (name, len, "-gdb.gdb")
	  || endswithn (name, len, "-gdb.py")
	  || endswithn (name, len, "-gdb.scm"));
}

/* Returns 0 if everything is ok, != 0 in case of error.  */
int
process_file (const char *real_file_name, const char *file_name,
	      const char *lib, int *flag, unsigned int *isa_level,
	      char **soname, int is_link, struct stat *stat_buf)
{
  FILE *file;
  struct stat statbuf;
  void *file_contents;
  int ret;
  ElfW(Ehdr) *elf_header;
  struct exec *aout_header;

  ret = 0;
  /* Just set FLAG_ELF_LIBC6 as old formats are not supported anymore.  */
  *flag = FLAG_ELF_LIBC6;
  *soname = NULL;

  file = fopen (real_file_name, "rb");
  if (file == NULL)
    {
      /* No error for stale symlink.  */
      if (is_link && strstr (file_name, ".so") != NULL)
	return 1;
      error (0, 0, _("Input file %s not found.\n"), file_name);
      return 1;
    }

  if (fstat (fileno (file), &statbuf) < 0)
    {
      error (0, 0, _("Cannot fstat file %s.\n"), file_name);
      fclose (file);
      return 1;
    }

  /* Check that the file is large enough so that we can access the
     information.  We're only checking the size of the headers here.  */
  if ((size_t) statbuf.st_size < sizeof (struct exec)
      || (size_t) statbuf.st_size < sizeof (ElfW(Ehdr)))
    {
      if (statbuf.st_size == 0)
	error (0, 0, _("File %s is empty, not checked."), file_name);
      else
	{
	  char buf[SELFMAG];
	  size_t n = MIN (statbuf.st_size, SELFMAG);
	  if (fread (buf, n, 1, file) == 1 && memcmp (buf, ELFMAG, n) == 0)
	    error (0, 0, _("File %s is too small, not checked."), file_name);
	}
      fclose (file);
      return 1;
    }

  file_contents = mmap (NULL, statbuf.st_size, PROT_READ, MAP_SHARED,
			fileno (file), 0);
  if (file_contents == MAP_FAILED)
    {
      error (0, 0, _("Cannot mmap file %s.\n"), file_name);
      fclose (file);
      return 1;
    }

  /* First check if this is an aout file.  */
  aout_header = (struct exec *) file_contents;
  if (N_MAGIC (*aout_header) == ZMAGIC
#ifdef QMAGIC			/* Linuxism.  */
      || N_MAGIC (*aout_header) == QMAGIC
#endif
      )
    {
      /* Aout files don't have a soname, just return the name
	 including the major number.  */
      char *copy, *major, *dot;
      copy = xstrdup (lib);
      major = strstr (copy, ".so.");
      if (major)
	{
	  dot = strstr (major + 4, ".");
	  if (dot)
	    *dot = '\0';
	}
      *soname = copy;
      goto done;
    }

  elf_header = (ElfW(Ehdr) *) file_contents;
  if (memcmp (elf_header->e_ident, ELFMAG, SELFMAG) != 0)
    {
      /* The file is neither ELF nor aout.  Check if it's a linker
	 script, like libc.so - otherwise complain.  Only search the
	 beginning of the file.  */
      size_t len = MIN (statbuf.st_size, 512);
      if (memmem (file_contents, len, "GROUP", 5) == NULL
	  && memmem (file_contents, len, "GNU ld script", 13) == NULL
	  && !is_gdb_extension_file (file_name))
	error (0, 0, _("%s is not an ELF file - it has the wrong magic bytes at the start.\n"),
	       file_name);
      ret = 1;
    }
  /* Libraries have to be shared object files.  */
  else if (elf_header->e_type != ET_DYN)
    ret = 1;
  else if (process_elf_file (file_name, lib, flag, isa_level, soname,
			     file_contents, statbuf.st_size))
    ret = 1;

 done:
  /* Clean up allocated memory and resources.  */
  munmap (file_contents, statbuf.st_size);
  fclose (file);

  *stat_buf = statbuf;
  return ret;
}

/* Get architecture specific version of process_elf_file.  */
#include <readelflib.c>
