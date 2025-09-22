/* Generate an index to speed access to archives.
   Copyright (C) 2005-2012 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2005.

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

#include <ar.h>
#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <locale.h>
#include <obstack.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <system.h>
#include <printversion.h>

#include "arlib.h"


/* Prototypes for local functions.  */
static int handle_file (const char *fname);


/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;


/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("Generate an index to speed access to archives.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("ARCHIVE");

/* Data structure to communicate with argp functions.  */
static const struct argp argp =
{
  options, NULL, args_doc, doc, arlib_argp_children, NULL, NULL
};


int
main (int argc, char *argv[])
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  (void) __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  int remaining;
  (void) argp_parse (&argp, argc, argv, ARGP_IN_ORDER, &remaining, NULL);

  /* Tell the library which version we are expecting.  */
  (void) elf_version (EV_CURRENT);

  /* There must at least be one more parameter specifying the archive.   */
  if (remaining == argc)
    {
      error (0, 0, _("Archive name required"));
      argp_help (&argp, stderr, ARGP_HELP_SEE, "ranlib");
      exit (EXIT_FAILURE);
    }

  /* We accept the names of multiple archives.  */
  int status = 0;
  do
    status |= handle_file (argv[remaining]);
  while (++remaining < argc);

  return status;
}


static int
copy_content (Elf *elf, int newfd, off_t off, size_t n)
{
  size_t len;
  char *rawfile = elf_rawfile (elf, &len);

  assert (off + n <= len);

  /* Tell the kernel we will read all the pages sequentially.  */
  size_t ps = sysconf (_SC_PAGESIZE);
  if (n > 2 * ps)
    posix_madvise (rawfile + (off & ~(ps - 1)), n, POSIX_MADV_SEQUENTIAL);

  return write_retry (newfd, rawfile + off, n) != (ssize_t) n;
}


/* Handle a file given on the command line.  */
static int
handle_file (const char *fname)
{
  int fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      error (0, errno, _("cannot open '%s'"), fname);
      return 1;
    }

  struct stat st;
  if (fstat (fd, &st) != 0)
    {
      error (0, errno, _("cannot stat '%s'"), fname);
      close (fd);
      return 1;
    }

  /* First we walk through the file, looking for all ELF files to
     collect symbols from.  */
  Elf *arelf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  if (arelf == NULL)
    {
      error (0, 0, _("cannot create ELF descriptor for '%s': %s"),
	     fname, elf_errmsg (-1));
      close (fd);
      return 1;
    }

  if (elf_kind (arelf) != ELF_K_AR)
    {
      error (0, 0, _("'%s' is no archive"), fname);
      elf_end (arelf);
      close (fd);
      return 1;
    }

  arlib_init ();

  /* Iterate over the content of the archive.  */
  off_t index_off = -1;
  size_t index_size = 0;
  off_t cur_off = SARMAG;
  Elf *elf;
  Elf_Cmd cmd = ELF_C_READ_MMAP;
  while ((elf = elf_begin (fd, cmd, arelf)) != NULL)
    {
      Elf_Arhdr *arhdr = elf_getarhdr (elf);
      assert (arhdr != NULL);

      /* If this is the index, remember the location.  */
      if (strcmp (arhdr->ar_name, "/") == 0)
	{
	  index_off = elf_getaroff (elf);
	  index_size = arhdr->ar_size;
	}
      else
	{
	  arlib_add_symbols (elf, fname, arhdr->ar_name, cur_off);
	  cur_off += (((arhdr->ar_size + 1) & ~((off_t) 1))
		      + sizeof (struct ar_hdr));
	}

      /* Get next archive element.  */
      cmd = elf_next (elf);
      if (elf_end (elf) != 0)
	error (0, 0, _("error while freeing sub-ELF descriptor: %s"),
	       elf_errmsg (-1));
    }

  arlib_finalize ();

  /* If the file contains no symbols we need not do anything.  */
  int status = 0;
  if (symtab.symsnamelen != 0
      /* We have to rewrite the file also if it initially had an index
	 but now does not need one anymore.  */
      || (symtab.symsnamelen == 0 && index_size != 0))
    {
      /* Create a new, temporary file in the same directory as the
	 original file.  */
      char tmpfname[strlen (fname) + 7];
      strcpy (stpcpy (tmpfname, fname), "XXXXXX");
      int newfd = mkstemp (tmpfname);
      if (unlikely (newfd == -1))
	{
	nonew:
	  error (0, errno, _("cannot create new file"));
	  status = 1;
	}
      else
	{
	  /* Create the header.  */
	  if (unlikely (write_retry (newfd, ARMAG, SARMAG) != SARMAG))
	    {
	      // XXX Use /prof/self/fd/%d ???
	    nonew_unlink:
	      unlink (tmpfname);
	      if (newfd != -1)
		close (newfd);
	      goto nonew;
	    }

	  /* Create the new file.  There are three parts as far we are
	     concerned: 1. original context before the index, 2. the
	     new index, 3. everything after the new index.  */
	  off_t rest_off;
	  if (index_off != -1)
	    rest_off = (index_off + sizeof (struct ar_hdr)
			+ ((index_size + 1) & ~1ul));
	  else
	    rest_off = SARMAG;

	  if (symtab.symsnamelen != 0
	      && ((write_retry (newfd, symtab.symsoff,
				symtab.symsofflen)
		   != (ssize_t) symtab.symsofflen)
		  || (write_retry (newfd, symtab.symsname,
				   symtab.symsnamelen)
		      != (ssize_t) symtab.symsnamelen)))
	    goto nonew_unlink;

	  /* Even if the original file had content before the
	     symbol table, we write it in the correct order.  */
	  if ((index_off > SARMAG
	       && copy_content (arelf, newfd, SARMAG, index_off - SARMAG))
	      || copy_content (arelf, newfd, rest_off, st.st_size - rest_off))
	    goto nonew_unlink;

	  /* Never complain about fchown failing.  */
	  if (fchown (newfd, st.st_uid, st.st_gid) != 0) { ; }
	  /* Set the mode of the new file to the same values the
	     original file has.  */
	  if (fchmod (newfd, st.st_mode & ALLPERMS) != 0
	      || close (newfd) != 0)
	    goto nonew_unlink;
	  newfd = -1;
	  if (rename (tmpfname, fname) != 0)
	    goto nonew_unlink;
	}
    }

  elf_end (arelf);

  arlib_fini ();

  close (fd);

  return status;
}


#include "debugpred.h"
