/* Convert text in given files from the specified from-set to the to-set.
   Copyright (C) 1998-2026 Free Software Foundation, Inc.
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

#include <argp.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <iconv.h>
#include <langinfo.h>
#include <locale.h>
#include <search.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libintl.h>
#include <charmap.h>
#include <gconv_int.h>
#include "iconv_prog.h"
#include "iconvconfig.h"
#include "gconv_charset.h"

/* Get libc version number.  */
#include "../version.h"

#define PACKAGE _libc_intl_domainname


/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;

enum
  {
    OPT_VERBOSE = 1000,
    OPT_BUFFER_SIZE,
  };
#define OPT_LIST	'l'

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Input/Output format specification:") },
  { "from-code", 'f', N_("NAME"), 0, N_("encoding of original text") },
  { "to-code", 't', N_("NAME"), 0, N_("encoding for output") },
  { NULL, 0, NULL, 0, N_("Information:") },
  { "list", 'l', NULL, 0, N_("list all known coded character sets") },
  { NULL, 0, NULL, 0, N_("Output control:") },
  { NULL, 'c', NULL, 0, N_("omit invalid characters from output") },
  { "output", 'o', N_("FILE"), 0, N_("output file") },
  { "silent", 's', NULL, 0, N_("suppress warnings") },
  { "verbose", OPT_VERBOSE, NULL, 0, N_("print progress information") },
  /* This is an internal option intended for testing only.  Very small
     buffers do not work with all character sets.  */
  { "buffer-size", OPT_BUFFER_SIZE, N_("BYTE-COUNT"), OPTION_HIDDEN,
    N_("size of in-memory scratch buffer") },
  { NULL, 0, NULL, 0, NULL }
};

/* Short description of program.  */
static const char doc[] = N_("\
Convert encoding of given files from one encoding to another.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[FILE...]");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Function to print some extra text in the help message.  */
static char *more_help (int key, const char *text, void *input);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, more_help
};

/* Code sets to convert from and to respectively.  An empty string as the
   default causes the 'iconv_open' function to look up the charset of the
   currently selected locale and use it.  */
static const char *from_code = "";
static const char *to_code = "";

/* File to write output to.  If NULL write to stdout.  */
static const char *output_file;

/* Nonzero if list of all coded character sets is wanted.  */
static int list;

/* If nonzero omit invalid character from output.  */
int omit_invalid;

/* Current index in argv (after command line processing) with the
   input file name.  */
static int current_input_file_index;

/* Size of the temporary, in-memory buffer.  Exceeding it needs
   spooling to disk in a temporary file.  Controlled by --buffer_size.  */
static size_t output_buffer_size = 1024 * 1024;

/* Prototypes for the functions doing the actual work.  */
static void prepare_output_file (char **argv);
static void close_output_file (__gconv_t cd, int status);
static int process_block (iconv_t cd, char **addr, size_t *len,
			  off64_t file_offset, bool *incomplete);
static int process_fd (iconv_t cd, int fd);
static int process_file (iconv_t cd, FILE *input);
static void print_known_names (void);


int
main (int argc, char *argv[])
{
  int status = EXIT_SUCCESS;
  __gconv_t cd;
  struct charmap_t *from_charmap = NULL;
  struct charmap_t *to_charmap = NULL;

  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");

  /* Set the text message domain.  */
  textdomain (_libc_intl_domainname);

  /* Parse and process arguments.  */
  argp_parse (&argp, argc, argv, 0, &current_input_file_index, NULL);

  /* List all coded character sets if wanted.  */
  if (list)
    {
      print_known_names ();
      exit (EXIT_SUCCESS);
    }

  /* POSIX 1003.2b introduces a silly thing: the arguments to -t anf -f
     can be file names of charmaps.  In this case iconv will have to read
     those charmaps and use them to do the conversion.  But there are
     holes in the specification.  There is nothing said that if -f is a
     charmap filename that -t must be, too.  And vice versa.  There is
     also no word about the symbolic names used.  What if they don't
     match?  */
  if (strchr (from_code, '/') != NULL)
    /* The from-name might be a charmap file name.  Try reading the
       file.  */
    from_charmap = charmap_read (from_code, /*0, 1*/1, 0, 0, 0);

  if (strchr (to_code, '/') != NULL)
    /* The to-name might be a charmap file name.  Try reading the
       file.  */
    to_charmap = charmap_read (to_code, /*0, 1,*/1, 0, 0, 0);


  /* At this point we have to handle two cases.  The first one is
     where a charmap is used for the from- or to-charset, or both.  We
     handle this special since it is very different from the sane way of
     doing things.  The other case allows converting using the iconv()
     function.  */
  if (from_charmap != NULL || to_charmap != NULL)
    /* Construct the conversion table and do the conversion.  */
    status = charmap_conversion (from_code, from_charmap, to_code, to_charmap,
				 argc, current_input_file_index, argv,
				 output_file);
  else
    {
      struct gconv_spec conv_spec;
      int res;

      if (__gconv_create_spec (&conv_spec, from_code, to_code) == NULL)
        {
          error (EXIT_FAILURE, errno,
                 _("failed to start conversion processing"));
          exit (1);
        }

      if (omit_invalid)
        conv_spec.ignore = true;

      /* Let's see whether we have these coded character sets.  */
      res = __gconv_open (&conv_spec, &cd, 0);

      __gconv_destroy_spec (&conv_spec);

      if (res != __GCONV_OK)
	{
	  if (res == __GCONV_NOCONV || res == __GCONV_NODB)
	    {
	      /* Try to be nice with the user and tell her which of the
		 two encoding names is wrong.  This is possible because
		 all supported encodings can be converted from/to Unicode,
		 in other words, because the graph of encodings is
		 connected.  */
	      bool from_wrong =
		(iconv_open ("UTF-8", from_code) == (iconv_t) -1
		 && errno == EINVAL);
	      bool to_wrong =
		(iconv_open (to_code, "UTF-8") == (iconv_t) -1
		 && errno == EINVAL);
	      const char *from_pretty =
		(from_code[0] ? from_code : nl_langinfo (CODESET));
	      const char *to_pretty =
		(to_code[0] ? to_code : nl_langinfo (CODESET));

	      if (from_wrong)
		{
		  if (to_wrong)
		    error (0, 0,
			   _("\
conversions from `%s' and to `%s' are not supported"),
			   from_pretty, to_pretty);
		  else
		    error (0, 0,
			   _("conversion from `%s' is not supported"),
			   from_pretty);
		}
	      else
		{
		  if (to_wrong)
		    error (0, 0,
			   _("conversion to `%s' is not supported"),
			   to_pretty);
		  else
		    error (0, 0,
			   _("conversion from `%s' to `%s' is not supported"),
			   from_pretty, to_pretty);
		}

	      argp_help (&argp, stderr, ARGP_HELP_SEE,
			 program_invocation_short_name);
	      exit (1);
	    }
	  else
	    error (EXIT_FAILURE, errno,
		   _("failed to start conversion processing"));
	}

      prepare_output_file (argv);

      /* Now process the remaining files.  Write them to stdout or the file
	 specified with the `-o' parameter.  If we have no file given as
	 the parameter process all from stdin.  */
      if (current_input_file_index == argc)
	{
	  if (process_file (cd, stdin) != 0)
	    status = EXIT_FAILURE;
	}
      else
	do
	  {
	    int fd, ret;

	    if (verbose)
	      fprintf (stderr, "%s:\n", argv[current_input_file_index]);
	    if (strcmp (argv[current_input_file_index], "-") == 0)
	      fd = STDIN_FILENO;
	    else
	      {
		fd = open (argv[current_input_file_index], O_RDONLY);

		if (fd == -1)
		  {
		    error (0, errno, _("cannot open input file `%s'"),
			   argv[current_input_file_index]);
		    status = EXIT_FAILURE;
		    continue;
		  }
	      }

	      {
		/* Read the file in pieces.  */
		ret = process_fd (cd, fd);

		/* Now close the file.  */
		if (fd != STDIN_FILENO)
		  close (fd);

		if (ret != 0)
		  {
		    /* Something went wrong.  */
		    status = EXIT_FAILURE;

		    if (ret < 0)
		      /* We cannot go on with producing output since it might
			 lead to problem because the last output might leave
			 the output stream in an undefined state.  */
		      break;
		  }
	      }
	  }
	while (++current_input_file_index < argc);

      /* Ensure that iconv -c still exits with failure if iconv (the
	 function) has failed with E2BIG instead of EILSEQ.  */
      if (__gconv_has_illegal_input (cd))
	status = EXIT_FAILURE;

      /* Close the output file now.  */
      close_output_file (cd, status);
    }

  return status;
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'f':
      from_code = arg;
      break;
    case 't':
      to_code = arg;
      break;
    case 'o':
      output_file = arg;
      break;
    case 's':
      /* Nothing, for now at least.  We are not giving out any information
	 about missing character or so.  */
      break;
    case 'c':
      /* Omit invalid characters from output.  */
      omit_invalid = 1;
      break;
    case OPT_BUFFER_SIZE:
      {
	int i = atoi (arg);
	if (i <= 0)
	  error (EXIT_FAILURE, 0, _("invalid buffer size: %s"), arg);
	output_buffer_size = i;
      }
      break;
    case OPT_VERBOSE:
      verbose = 1;
      break;
    case OPT_LIST:
      list = 1;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static char *
more_help (int key, const char *text, void *input)
{
  char *tp = NULL;
  switch (key)
    {
    case ARGP_KEY_HELP_EXTRA:
      /* We print some extra information.  */
      if (asprintf (&tp, gettext ("\
For bug reporting instructions, please see:\n\
%s.\n"), REPORT_BUGS_TO) < 0)
	return NULL;
      return tp;
    default:
      break;
    }
  return (char *) text;
}


/* Print the version information.  */
static void
print_version (FILE *stream, struct argp_state *state)
{
  fprintf (stream, "iconv %s%s\n", PKGVERSION, VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Free Software Foundation, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2024");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}

/* Command line index of the last input file that overlaps with the
   output file.  Zero means no temporary file is ever required.  */
static int last_overlapping_file_index;

/* This is set to true if the output is written to a temporary file.   */
static bool output_using_temporary_file;

/* This is the file descriptor that will be used by write_output.  */
static int output_fd = -1;

/* Pointers at the start and end of the fixed-size output buffer.  */
static char *output_buffer_start;

/* Current write position in the output buffer.  */
static char *output_buffer_current;

/* Remaining bytes after output_buffer_current in the output buffer.  */
static size_t output_buffer_remaining;


/* Reduce the buffer size when writing directly to the output file, to
   reduce cache utilization.  */
static size_t copy_buffer_size = BUFSIZ;

static void
output_error (void)
{
  error (EXIT_FAILURE, errno, _("cannot open output file"));
}

static void
input_error (const char *path)
{
  error (0, errno, _("cannot open input file `%s'"), path);
}

/* Opens output_file for writing, truncating it.  */
static void
open_output_direct (void)
{
  output_fd = open64 (output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (output_fd < 0)
    output_error ();
}

static void
prepare_output_file (char **argv)
{
  if (copy_buffer_size > output_buffer_size)
    copy_buffer_size = output_buffer_size;

  if (output_file == NULL || strcmp (output_file, "-") == 0)
    {
      /* No buffering is required when writing to standard output
	 because input overlap is expected to be solved externally.  */
      output_fd = STDOUT_FILENO;
      output_buffer_size = copy_buffer_size;
    }
  else
    {
      /* If iconv creates the output file, no overlap is possible.  */
      output_fd = open64 (output_file, O_WRONLY | O_CREAT | O_EXCL, 0666);
      if (output_fd >= 0)
	output_buffer_size = copy_buffer_size;
      else
	{
	  /* Otherwise, check if any of the input files overlap with the
	     output file.  */
	  struct statx st;
	  if (statx (AT_FDCWD, output_file, 0, STATX_INO | STATX_MODE, &st)
	      != 0)
	    output_error ();
	  uint32_t out_dev_minor = st.stx_dev_minor;
	  uint32_t out_dev_major = st.stx_dev_major;
	  uint64_t out_ino = st.stx_ino;

	  int idx = current_input_file_index;
	  while (true)
	    {
	      /* Special case: no input files means standard input.  */
	      if (argv[idx] == NULL && idx != current_input_file_index)
		break;

	      int ret;
	      if (argv[idx] == NULL || strcmp (argv[idx], "-") == 0)
		ret = statx (STDIN_FILENO, "", AT_EMPTY_PATH, STATX_INO, &st);
	      else
		ret = statx (AT_FDCWD, argv[idx], 0, STATX_INO, &st);
	      if (ret != 0)
		{
		  input_error (argv[idx]);
		  exit (EXIT_FAILURE);
		}
	      if (out_dev_minor == st.stx_dev_minor
		  && out_dev_major == st.stx_dev_major
		  && out_ino == st.stx_ino)
		{
		  if (argv[idx] == NULL)
		    /* Corner case: index of NULL would be larger than
		       idx while converting, triggering a switch away
		       from the temporary file.  */
		    last_overlapping_file_index = INT_MAX;
		  else
		    last_overlapping_file_index = idx;
		}

	      if (argv[idx] == NULL)
		break;
	      ++idx;
	    }

	  /* If there is no overlap, avoid using a temporary file.  */
	  if (last_overlapping_file_index == 0)
	    {
	      open_output_direct ();
	      output_buffer_size = copy_buffer_size;
	    }
	}
    }

  output_buffer_start = malloc (output_buffer_size);
  if (output_buffer_start == NULL)
    output_error ();
  output_buffer_current = output_buffer_start;
  output_buffer_remaining = output_buffer_size;
}

/* Write out the range [first, last), terminating the process on write
   error.  */
static void
write_fully (int fd, const char *first, const char *last)
{
  while (first < last)
    {
      ssize_t ret = write (fd, first, last - first);
      if (ret == 0)
	{
	  errno = ENOSPC;
	  output_error ();
	}
      if (ret < 0)
	error (EXIT_FAILURE, errno, _("\
conversion stopped due to problem in writing the output"));
      first += ret;
    }
}

static void
flush_output (void)
{
  bool temporary_file_not_needed
    = current_input_file_index > last_overlapping_file_index;
  if (output_fd < 0)
    {
      if (temporary_file_not_needed)
	open_output_direct ();
      else
	{
	  /* Create an anonymous temporary file.  */
	  FILE *fp = tmpfile ();
	  if (fp == NULL)
	    output_error ();
	  output_fd = dup (fileno (fp));
	  if (output_fd < 0)
	    output_error ();
	  fclose (fp);
	  output_using_temporary_file = true;
	}
      /* Either way, no longer use a memory-only staging buffer.  */
      output_buffer_size = copy_buffer_size;
    }
  else if (output_using_temporary_file && temporary_file_not_needed)
    {
      /* The temporary file is no longer needed.  Switch to direct
	 output, replacing output_fd.  */
      int temp_fd = output_fd;
      open_output_direct ();

      /* Copy over the data spooled to the temporary file.  */
      if (lseek (temp_fd, 0, SEEK_SET) < 0)
	output_error ();
      while (true)
	{
	  char buf[BUFSIZ];
	  ssize_t ret = read (temp_fd, buf, sizeof (buf));
	  if (ret < 0)
	    output_error ();
	  if (ret == 0)
	    break;
	  write_fully (output_fd, buf, buf + ret);
	}
      close (temp_fd);

      /* No longer using a temporary file from now on.  */
      output_using_temporary_file = false;
      output_buffer_size = copy_buffer_size;
    }

  write_fully (output_fd, output_buffer_start, output_buffer_current);
  output_buffer_current = output_buffer_start;
  output_buffer_remaining = output_buffer_size;
}

static void
close_output_file (__gconv_t cd, int status)
{
  /* Do not perform a flush if a temporary file or the in-memory
     buffer is in use and there was an error.  It would clobber the
     overlapping input file.  */
  if (status != EXIT_SUCCESS && !omit_invalid &&
      (output_using_temporary_file || output_fd < 0))
    return;

  /* All the input text is processed.  For state-dependent character
     sets we have to flush the state now.

     The current_input_file_index variable is now larger than
     last_overlapping_file_index, so the flush_output calls switch
     away from the temporary file.  */
  size_t n = iconv (cd, NULL, NULL,
		    &output_buffer_current, &output_buffer_remaining);
  if (n == (size_t) -1 && errno == E2BIG)
    {
      /* Try again if the state flush exceeded the buffer space.  */
      flush_output ();
      n = iconv (cd, NULL, NULL,
		 &output_buffer_current, &output_buffer_remaining);
    }
  int saved_errno = errno;
  flush_output ();
  if (n == (size_t) -1 && !omit_invalid)
    {
      errno = saved_errno;
      output_error ();
    }

  if (output_fd == STDOUT_FILENO)
    {
      /* Close standard output in safe manner, to report certain
	 ENOSPC errors.  */
      output_fd = dup (output_fd);
      if (output_fd < 0)
	output_error ();
    }
  if (close (output_fd) < 0)
    output_error ();
}

/* CD is the iconv handle.  Input processing starts at *ADDR, and
   consumes upto *LEN bytes.  *ADDR and *LEN are updated.  FILE_OFFSET
   is the file offset of the data initially at ADDR.  *INCOMPLETE is
   set to true if conversion stops due to an incomplete input
   sequence.  */
static int
process_block (iconv_t cd, char **addr, size_t *len, off64_t file_offset,
	       bool *incomplete)
{
  const char *start = *addr;
  size_t n;
  int ret = 0;

  while (*len > 0)
    {
      n = iconv (cd, addr, len,
		 &output_buffer_current, &output_buffer_remaining);

      if (n == (size_t) -1 && omit_invalid && errno == EILSEQ)
	{
	  ret = 1;
	  if (*len == 0)
	    n = 0;
	  else
	    errno = E2BIG;
	}

      if (n != (size_t) -1)
	break;

      if (errno == E2BIG)
	flush_output ();
      else
	{
	  /* iconv() ran into a problem.  */
	  switch (errno)
	    {
	    case EILSEQ:
	      if (! omit_invalid)
		error (0, 0, _("illegal input sequence at position %lld"),
		       (long long int) (file_offset + (*addr - start)));
	      break;
	    case EINVAL:
	      *incomplete = true;
	      return ret;
	    case EBADF:
	      error (0, 0, _("internal error (illegal descriptor)"));
	      break;
	    default:
	      error (0, 0, _("unknown iconv() error %d"), errno);
	      break;
	    }

	  return -1;
	}
    }

  return ret;
}


static int
process_fd (iconv_t cd, int fd)
{
  char inbuf[BUFSIZ];
  char *inbuf_end = inbuf + sizeof (inbuf);
  size_t inbuf_used = 0;
  off64_t file_offset = 0;
  int status = 0;
  bool incomplete = false;

  while (true)
    {
      char *p = inbuf + inbuf_used;
      ssize_t read_ret = read (fd, p, inbuf_end - p);
      if (read_ret == 0)
	{
	  /* On EOF, check if the previous iconv invocation saw an
	     incomplete sequence.  */
	  if (incomplete)
	    {
	      error (0, 0, _("\
incomplete character or shift sequence at end of buffer"));
	      return 1;
	    }
	  return 0;
	}
      if (read_ret < 0)
	{
	  error (0, errno, _("error while reading the input"));
	  return -1;
	}
      inbuf_used += read_ret;
      incomplete = false;
      p = inbuf;
      int ret = process_block (cd, &p, &inbuf_used, file_offset, &incomplete);
      if (ret != 0)
	{
	  status = ret;
	  if (ret < 0)
	    break;
	}
      /* The next loop iteration consumes the leftover bytes.  */
      memmove (inbuf, p, inbuf_used);
      file_offset += read_ret - inbuf_used;
    }
  return status;
}


static int
process_file (iconv_t cd, FILE *input)
{
  /* This should be safe since we use this function only for `stdin' and
     we haven't read anything so far.  */
  return process_fd (cd, fileno (input));
}


/* Print all known character sets/encodings.  */
static void *printlist;
static size_t column;
static int not_first;

static void
insert_print_list (const void *nodep, VISIT value, int level)
{
  if (value == leaf || value == postorder)
    {
      const struct gconv_alias *s = *(const struct gconv_alias **) nodep;
      tsearch (s->fromname, &printlist, (__compar_fn_t) strverscmp);
    }
}

static void
do_print_human  (const void *nodep, VISIT value, int level)
{
  if (value == leaf || value == postorder)
    {
      const char *s = *(const char **) nodep;
      size_t len = strlen (s);
      size_t cnt;

      while (len > 0 && s[len - 1] == '/')
	--len;

      for (cnt = 0; cnt < len; ++cnt)
	if (isalnum (s[cnt]))
	  break;
      if (cnt == len)
	return;

      if (not_first)
	{
	  putchar (',');
	  ++column;

	  if (column > 2 && column + len > 77)
	    {
	      fputs ("\n  ", stdout);
	      column = 2;
	    }
	  else
	    {
	      putchar (' ');
	      ++column;
	    }
	}
      else
	not_first = 1;

      fwrite (s, len, 1, stdout);
      column += len;
    }
}

static void
do_print  (const void *nodep, VISIT value, int level)
{
  if (value == leaf || value == postorder)
    {
      const char *s = *(const char **) nodep;

      puts (s);
    }
}

static void
add_known_names (struct gconv_module *node)
{
  if (node->left != NULL)
    add_known_names (node->left);
  if (node->right != NULL)
    add_known_names (node->right);
  do
    {
      if (strcmp (node->from_string, "INTERNAL") != 0)
	tsearch (node->from_string, &printlist, (__compar_fn_t) strverscmp);
      if (strcmp (node->to_string, "INTERNAL") != 0)
	tsearch (node->to_string, &printlist, (__compar_fn_t) strverscmp);

      node = node->same;
    }
  while (node != NULL);
}


static void
insert_cache (void)
{
  const struct gconvcache_header *header;
  const char *strtab;
  const struct hash_entry *hashtab;
  size_t cnt;

  header = (const struct gconvcache_header *) __gconv_get_cache ();
  strtab = (char *) header + header->string_offset;
  hashtab = (struct hash_entry *) ((char *) header + header->hash_offset);

  for (cnt = 0; cnt < header->hash_size; ++cnt)
    if (hashtab[cnt].string_offset != 0)
      {
	const char *str = strtab + hashtab[cnt].string_offset;

	if (strcmp (str, "INTERNAL") != 0)
	  tsearch (str, &printlist, (__compar_fn_t) strverscmp);
      }
}


static void
print_known_names (void)
{
  iconv_t h;
  void *cache;

  /* We must initialize the internal databases first.  */
  h = iconv_open ("L1", "L1");
  iconv_close (h);

  /* See whether we have a cache.  */
  cache = __gconv_get_cache ();
  if (cache != NULL)
    /* Yep, use only this information.  */
    insert_cache ();
  else
    {
      struct gconv_module *modules;

      /* No, then use the information read from the gconv-modules file.
	 First add the aliases.  */
      twalk (__gconv_get_alias_db (), insert_print_list);

      /* Add the from- and to-names from the known modules.  */
      modules = __gconv_get_modules_db ();
      if (modules != NULL)
	add_known_names (modules);
    }

  bool human_readable = isatty (fileno (stdout));

  if (human_readable)
    fputs (_("\
The following list contains all the coded character sets known.  This does\n\
not necessarily mean that all combinations of these names can be used for\n\
the FROM and TO command line parameters.  One coded character set can be\n\
listed with several different names (aliases).\n\n  "), stdout);

  /* Now print the collected names.  */
  column = 2;
  twalk (printlist, human_readable ? do_print_human : do_print);

  if (human_readable && column != 0)
    puts ("");
}
