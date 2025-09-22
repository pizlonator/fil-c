/* Print the strings of printable characters in files.
   Copyright (C) 2005-2010, 2012, 2014 Red Hat, Inc.
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

#include <argp.h>
#include <assert.h>
#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <libeu.h>
#include <system.h>
#include <printversion.h>

#ifndef MAP_POPULATE
# define MAP_POPULATE 0
#endif


/* Prototypes of local functions.  */
static int read_fd (int fd, const char *fname, off_t fdlen);
static int read_elf (Elf *elf, int fd, const char *fname, off_t fdlen);


/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Output Selection:"), 0 },
  { "all", 'a', NULL, 0, N_("Scan entire file, not only loaded sections"), 0 },
  { "bytes", 'n', "MIN-LEN", 0,
    N_("Only NUL-terminated sequences of MIN-LEN characters or more are printed"), 0 },
  { "encoding", 'e', "SELECTOR", 0, N_("\
Select character size and endianness: s = 7-bit, S = 8-bit, {b,l} = 16-bit, {B,L} = 32-bit"),
    0},
  { "print-file-name", 'f', NULL, 0,
    N_("Print name of the file before each string."), 0 },
  { "radix", 't', "{o,d,x}", 0,
    N_("Print location of the string in base 8, 10, or 16 respectively."), 0 },
  { NULL, 'o', NULL, 0, N_("Alias for --radix=o"), 0 },

  { NULL, 0, NULL, 0, N_("Miscellaneous:"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Print the strings of printable characters in files.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[FILE...]");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};


/* Global variables.  */

/* True if whole file and not only loaded sections are looked at.  */
static bool entire_file;

/* Minimum length of any sequence reported.  */
static size_t min_len = 4;

/* Number of bytes per character.  */
static size_t bytes_per_char = 1;

/* Minimum length of any sequence reported in bytes.  */
static size_t min_len_bytes;

/* True if multibyte characters are in big-endian order.  */
static bool big_endian;

/* True unless 7-bit ASCII are expected.  */
static bool char_7bit;

/* True if file names should be printed before strings.  */
static bool print_file_name;

/* Radix for printed numbers.  */
static enum
{
  radix_none = 0,
  radix_decimal,
  radix_hex,
  radix_octal
} radix = radix_none;


/* Page size in use.  */
static size_t ps;


/* Mapped parts of the ELF file.  */
static unsigned char *elfmap;
static unsigned char *elfmap_base;
static size_t elfmap_size;
static off_t elfmap_off;


int
main (int argc, char *argv[])
{
  /* We use no threads.  */
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  int remaining;
  (void) argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* Tell the library which version we are expecting.  */
  elf_version (EV_CURRENT);

  /* Determine the page size.  We will likely need it a couple of times.  */
  ps = sysconf (_SC_PAGESIZE);

  struct stat st;
  int result = 0;
  if (remaining == argc)
    /* We read from standard input.  This we cannot do for a
       structured file.  */
    result = read_fd (STDIN_FILENO,
		      print_file_name ? "{standard input}" : NULL,
		      (fstat (STDIN_FILENO, &st) == 0 && S_ISREG (st.st_mode))
		      ? st.st_size : INT64_C (0x7fffffffffffffff));
  else
    do
      {
	int fd = (strcmp (argv[remaining], "-") == 0
		  ? STDIN_FILENO : open (argv[remaining], O_RDONLY));
	if (unlikely (fd == -1))
	  {
	    error (0, errno, _("cannot open '%s'"), argv[remaining]);
	    result = 1;
	  }
	else
	  {
	    const char *fname = print_file_name ? argv[remaining] : NULL;
	    int fstat_fail = fstat (fd, &st);
	    off_t fdlen = (fstat_fail
			     ? INT64_C (0x7fffffffffffffff) : st.st_size);
	    if (fdlen > (off_t) min_len_bytes)
	      {
		Elf *elf = NULL;
		if (entire_file
		    || fstat_fail
		    || !S_ISREG (st.st_mode)
		    || (elf = elf_begin (fd, ELF_C_READ, NULL)) == NULL
		    || elf_kind (elf) != ELF_K_ELF)
		  result |= read_fd (fd, fname, fdlen);
		else
		  result |= read_elf (elf, fd, fname, fdlen);

		/* This call will succeed even if ELF is NULL.  */
		elf_end (elf);
	      }

	    if (strcmp (argv[remaining], "-") != 0)
	      close (fd);
	  }

	if (elfmap != NULL && elfmap != MAP_FAILED)
	  munmap (elfmap, elfmap_size);
	elfmap = NULL;
      }
    while (++remaining < argc);

  return result;
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case 'a':
      entire_file = true;
      break;

    case 'e':
      /* We expect a string of one character.  */
      switch (arg[1] != '\0' ? '\0' : arg[0])
	{
	case 's':
	case 'S':
	  char_7bit = arg[0] == 's';
	  bytes_per_char = 1;
	  break;

	case 'b':
	case 'B':
	  big_endian = true;
	  FALLTHROUGH;

	case 'l':
	case 'L':
	  bytes_per_char = isupper (arg[0]) ? 4 : 2;
	  break;

	default:
	  error (0, 0, _("invalid value '%s' for %s parameter"),
		 arg, "-e");
	  argp_help (&argp, stderr, ARGP_HELP_SEE, "strings");
	  return ARGP_ERR_UNKNOWN;
	}
      break;

    case 'f':
      print_file_name = true;
      break;

    case 'n':
      min_len = atoi (arg);
      break;

    case 'o':
      goto octfmt;

    case 't':
      switch (arg[0])
	{
	case 'd':
	  radix = radix_decimal;
	  break;

	case 'o':
	octfmt:
	  radix = radix_octal;
	  break;

	case 'x':
	  radix = radix_hex;
	  break;

	default:
	  error (0, 0, _("invalid value '%s' for %s parameter"),
		 arg, "-t");
	  argp_help (&argp, stderr, ARGP_HELP_SEE, "strings");
	  return ARGP_ERR_UNKNOWN;
	}
      break;

    case ARGP_KEY_FINI:
      /* Compute the length in bytes of any match.  */
      if (min_len <= 0 || min_len > INT_MAX / bytes_per_char)
	error_exit (0, _("invalid minimum length of matched string size"));
      min_len_bytes = min_len * bytes_per_char;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static void
process_chunk_mb (const char *fname, const unsigned char *buf, off_t to,
		  size_t len, char **unprinted)
{
  size_t curlen = *unprinted == NULL ? 0 : strlen (*unprinted);
  const unsigned char *start = buf;
  while (len >= bytes_per_char)
    {
      uint32_t ch;

      if (bytes_per_char == 2)
	{
	  if (big_endian)
	    ch = buf[0] << 8 | buf[1];
	  else
	    ch = buf[1] << 8 | buf[0];
	}
      else
	{
	  if (big_endian)
	    ch = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
	  else
	    ch = buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0];
	}

      if (ch <= 255 && (isprint (ch) || ch == '\t'))
	{
	  ++buf;
	  ++curlen;
	}
      else
	{
	  if (curlen >= min_len)
	    {
	      /* We found a match.  */
	      if (unlikely (fname != NULL))
		{
		  fputs_unlocked (fname, stdout);
		  fputs_unlocked (": ", stdout);
		}

	      if (unlikely (radix != radix_none))
		printf ((radix == radix_octal ? "%7" PRIo64 " "
			 : (radix == radix_decimal ? "%7" PRId64 " "
			    : "%7" PRIx64 " ")),
			(int64_t) to - len - (buf - start));

	      if (unlikely (*unprinted != NULL))
		{
		  fputs_unlocked (*unprinted, stdout);
		  free (*unprinted);
		  *unprinted = NULL;
		}

	      /* There is no sane way of printing the string.  If we
		 assume the file data is encoded in UCS-2/UTF-16 or
		 UCS-4/UTF-32 respectively we could convert the string.
		 But there is no such guarantee.  */
	      fwrite_unlocked (start, 1, buf - start, stdout);
	      putc_unlocked ('\n', stdout);
	    }

	  start = ++buf;
	  curlen =  0;

	  if (len <= min_len)
	    break;
	}

      --len;
    }

  if (curlen != 0)
    *unprinted = xstrndup ((const char *) start, curlen);
}


static void
process_chunk (const char *fname, const unsigned char *buf, off_t to,
	       size_t len, char **unprinted)
{
  /* We are not going to slow the check down for the 2- and 4-byte
     encodings.  Handle them special.  */
  if (unlikely (bytes_per_char != 1))
    {
      process_chunk_mb (fname, buf, to, len, unprinted);
      return;
    }

  size_t curlen = *unprinted == NULL ? 0 : strlen (*unprinted);
  const unsigned char *start = buf;
  while (len > 0)
    {
      if ((isprint (*buf) || *buf == '\t') && (! char_7bit || *buf <= 127))
	{
	  ++buf;
	  ++curlen;
	}
      else
	{
	  if (curlen >= min_len)
	    {
	      /* We found a match.  */
	      if (likely (fname != NULL))
		{
		  fputs_unlocked (fname, stdout);
		  fputs_unlocked (": ", stdout);
		}

	      if (likely (radix != radix_none))
		printf ((radix == radix_octal ? "%7" PRIo64 " "
			 : (radix == radix_decimal ? "%7" PRId64 " "
			    : "%7" PRIx64 " ")),
			(int64_t) to - len - (buf - start));

	      if (unlikely (*unprinted != NULL))
		{
		  fputs_unlocked (*unprinted, stdout);
		  free (*unprinted);
		  *unprinted = NULL;
		}
	      fwrite_unlocked (start, 1, buf - start, stdout);
	      putc_unlocked ('\n', stdout);
	    }

	  start = ++buf;
	  curlen =  0;

	  if (len <= min_len)
	    break;
	}

      --len;
    }

  if (curlen != 0)
    *unprinted = xstrndup ((const char *) start, curlen);
}


/* Map a file in as large chunks as possible.  */
static void *
map_file (int fd, off_t start_off, off_t fdlen, size_t *map_sizep)
{
  /* Maximum size we mmap.  We use an #ifdef to avoid overflows on
     32-bit machines.  64-bit machines these days do not have usable
     address spaces larger than about 43 bits.  Not that any file
     should be that large.  */
# if SIZE_MAX > 0xffffffff
  const size_t mmap_max = 0x4000000000lu;
# else
  const size_t mmap_max = 0x40000000lu;
# endif

  /* Try to mmap the file.  */
  size_t map_size = MIN ((off_t) mmap_max, fdlen);
  const size_t map_size_min = MAX (MAX (SIZE_MAX / 16, 2 * ps),
				   roundup (2 * min_len_bytes + 1, ps));
  void *mem;
  while (1)
    {
      /* We map the memory for reading only here.  Since we will
	 always look at every byte of the file it makes sense to
	 use MAP_POPULATE.  */
      mem = mmap (NULL, map_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE,
		  fd, start_off);
      if (mem != MAP_FAILED)
	{
	  /* We will go through the mapping sequentially.  */
	  (void) posix_madvise (mem, map_size, POSIX_MADV_SEQUENTIAL);
	  break;
	}
      if (errno != EINVAL && errno != ENOMEM)
	/* This is an error other than the lack of address space.  */
	break;

      /* Maybe the size of the mapping is too big.  Try again.  */
      map_size /= 2;
      if (map_size < map_size_min)
	/* That size should have fit.  */
	break;
    }

  *map_sizep = map_size;
  return mem;
}


/* Read the file without mapping.  */
static int
read_block_no_mmap (int fd, const char *fname, off_t from, off_t fdlen)
{
  char *unprinted = NULL;
#define CHUNKSIZE 65536
  unsigned char *buf = xmalloc (CHUNKSIZE + min_len_bytes
				+ bytes_per_char - 1);
  size_t ntrailer = 0;
  int result = 0;
  while (fdlen > 0)
    {
      ssize_t n = TEMP_FAILURE_RETRY (read (fd, buf + ntrailer,
					    MIN (fdlen, CHUNKSIZE)));
      if (n == 0)
	{
	  /* There are less than MIN_LEN+1 bytes left so there cannot be
	     another match.  */
	  assert (unprinted == NULL || ntrailer == 0);
	  break;
	}
      if (unlikely (n < 0))
	{
	  /* Something went wrong.  */
	  result = 1;
	  break;
	}

      /* Account for the number of bytes read in this round.  */
      fdlen -= n;

      /* Do not use the signed N value.  Note that the addition cannot
	 overflow.  */
      size_t nb = (size_t) n + ntrailer;
      if (nb >= min_len_bytes)
	{
	  /* We only use complete characters.  */
	  nb &= ~(bytes_per_char - 1);

	  process_chunk (fname, buf, from + nb, nb, &unprinted);

	  /* If the last bytes of the buffer (modulo the character
	     size) have been printed we are not copying them.  */
	  size_t to_keep = unprinted != NULL ? 0 : min_len_bytes;

	  memmove (buf, buf + nb - to_keep, to_keep);
	  ntrailer = to_keep;
	  from += nb;
	}
      else
	ntrailer = nb;
    }

  free (buf);

  /* Don't print anything we collected so far.  There is no
     terminating NUL byte.  */
  free (unprinted);

  return result;
}


static int
read_block (int fd, const char *fname, off_t fdlen, off_t from, off_t to)
{
  if (elfmap == NULL)
    {
      /* We need a completely new mapping.  */
      elfmap_off = from & ~(ps - 1);
      elfmap_base = elfmap = map_file (fd, elfmap_off, fdlen, &elfmap_size);

      if (unlikely (elfmap == MAP_FAILED))
	/* Let the kernel know we are going to read everything in sequence.  */
	(void) posix_fadvise (fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    }

  if (unlikely (elfmap == MAP_FAILED))
    {
      /* Read from the file descriptor.  For this we must position the
	 read pointer.  */
      // XXX Eventually add flag which avoids this if the position
      // XXX is known to match.
      if (from != 0 && lseek (fd, from, SEEK_SET) != from)
	error_exit (errno, _("lseek failed"));

      return read_block_no_mmap (fd, fname, from, to - from);
    }

  assert ((off_t) min_len_bytes < fdlen);

  if (to < (off_t) elfmap_off || from > (off_t) (elfmap_off + elfmap_size))
    {
      /* The existing mapping cannot fit at all.  Map the new area.
	 We always map the full range of ELFMAP_SIZE bytes even if
	 this extend beyond the end of the file.  The Linux kernel
	 handles this OK if the access pages are not touched.  */
      elfmap_off = from & ~(ps - 1);
      if (mmap (elfmap, elfmap_size, PROT_READ,
		MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, fd, from)
	  == MAP_FAILED)
	error_exit (errno, _("re-mmap failed"));
      elfmap_base = elfmap;
    }

  char *unprinted = NULL;

  /* Use the existing mapping as much as possible.  If necessary, map
     new pages.  */
  if (from >= (off_t) elfmap_off
      && from < (off_t) (elfmap_off + elfmap_size))
    /* There are at least a few bytes in this mapping which we can
       use.  */
    process_chunk (fname, elfmap_base + (from - elfmap_off),
		   MIN (to, (off_t) (elfmap_off + elfmap_size)),
		   MIN (to, (off_t) (elfmap_off + elfmap_size)) - from,
		   &unprinted);

  if (to > (off_t) (elfmap_off + elfmap_size))
    {
      unsigned char *remap_base = elfmap_base;
      size_t read_now = elfmap_size - (elfmap_base - elfmap);

      assert (from >= (off_t) elfmap_off
	      && from < (off_t) (elfmap_off + elfmap_size));
      off_t handled_to = elfmap_off + elfmap_size;
      assert (elfmap == elfmap_base
	      || (elfmap_base - elfmap
		  == (ptrdiff_t) ((min_len_bytes + ps - 1) & ~(ps - 1))));
      if (elfmap == elfmap_base)
	{
	  size_t keep_area = (min_len_bytes + ps - 1) & ~(ps - 1);
	  assert (elfmap_size >= keep_area + ps);
	  /* The keep area is used for the content of the previous
	     buffer we have to keep.  This means copying those bytes
	     and for this we have to make the data writable.  */
	  if (unlikely (mprotect (elfmap, keep_area, PROT_READ | PROT_WRITE)
			!= 0))
	    error_exit (errno, _("mprotect failed"));

	  elfmap_base = elfmap + keep_area;
	}

      while (1)
	{
	  /* Map the rest of the file, eventually again in pieces.
	     We speed things up with a nice Linux feature.  Note
	     that we have at least two pages mapped.  */
	  size_t to_keep = unprinted != NULL ? 0 : min_len_bytes;

	  assert (read_now >= to_keep);
	  memmove (elfmap_base - to_keep,
		   remap_base + read_now - to_keep, to_keep);
	  remap_base = elfmap_base;

	  assert ((elfmap_size - (elfmap_base - elfmap)) % bytes_per_char
		  == 0);
	  read_now = MIN (to - handled_to,
			  (ptrdiff_t) elfmap_size - (elfmap_base - elfmap));

	  assert (handled_to % ps == 0);
	  assert (handled_to % bytes_per_char == 0);
	  if (mmap (remap_base, read_now, PROT_READ,
		    MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, fd, handled_to)
	      == MAP_FAILED)
	    error_exit (errno, _("re-mmap failed"));
	  elfmap_off = handled_to;

	  process_chunk (fname, remap_base - to_keep,
			 elfmap_off + (read_now & ~(bytes_per_char - 1)),
			 to_keep + (read_now & ~(bytes_per_char - 1)),
			 &unprinted);
	  handled_to += read_now;
	  if (handled_to >= to)
	    break;
	}
    }

  /* Don't print anything we collected so far.  There is no
     terminating NUL byte.  */
  free (unprinted);

  return 0;
}


static int
read_fd (int fd, const char *fname, off_t fdlen)
{
  return read_block (fd, fname, fdlen, 0, fdlen);
}


static int
read_elf (Elf *elf, int fd, const char *fname, off_t fdlen)
{
  assert (fdlen >= 0);

  /* We will look at each section separately.  The ELF file is not
     mmapped.  The libelf implementation will load the needed parts on
     demand.  Since we only iterate over the section header table the
     memory consumption at this stage is kept minimal.  */
  Elf_Scn *scn = elf_nextscn (elf, NULL);
  if (scn == NULL)
    return read_fd (fd, fname, fdlen);

  int result = 0;
  do
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      /* Only look in sections which are loaded at runtime and
	 actually have content.  */
      if (shdr != NULL && shdr->sh_type != SHT_NOBITS
	  && (shdr->sh_flags & SHF_ALLOC) != 0)
	{
	  if (shdr->sh_offset > (Elf64_Off) fdlen
	      || fdlen - shdr->sh_offset < shdr->sh_size)
	    {
	      size_t strndx = 0;
	      const char *sname;
	      if (unlikely (elf_getshdrstrndx (elf, &strndx) < 0))
		sname = "<unknown>";
	      else
		sname = elf_strptr (elf, strndx, shdr->sh_name) ?: "<unknown>";
	      error (0, 0,
		     _("Skipping section %zd '%s' data outside file"),
		     elf_ndxscn (scn), sname);
	      result = 1;
	    }
	  else
	    result |= read_block (fd, fname, fdlen, shdr->sh_offset,
				  shdr->sh_offset + shdr->sh_size);
	}
    }
  while ((scn = elf_nextscn (elf, scn)) != NULL);

  if (elfmap != NULL && elfmap != MAP_FAILED)
    munmap (elfmap, elfmap_size);
  elfmap = NULL;

  return result;
}


#include "debugpred.h"
