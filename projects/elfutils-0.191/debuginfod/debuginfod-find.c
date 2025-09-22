/* Command-line frontend for retrieving ELF / DWARF / source files
   from the debuginfod.
   Copyright (C) 2019-2020 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include "config.h"
#include "printversion.h"
#include "debuginfod.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <argp.h>
#include <unistd.h>
#include <fcntl.h>
#include <gelf.h>
#include <libdwelf.h>


/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

/* Short description of program.  */
static const char doc[] = N_("Request debuginfo-related content "
                             "from debuginfods listed in $" DEBUGINFOD_URLS_ENV_VAR ".");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("debuginfo BUILDID\n"
                                  "debuginfo PATH\n"
                                  "executable BUILDID\n"
                                  "executable PATH\n"
                                  "source BUILDID /FILENAME\n"
                                  "source PATH /FILENAME\n"
				  "section BUILDID SECTION-NAME\n"
				  "section PATH SECTION-NAME\n");


/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
  {
   { "verbose", 'v', NULL, 0, "Increase verbosity.", 0 },
   { NULL, 0, NULL, 0, NULL, 0 }
  };

/* debuginfod connection handle.  */
static debuginfod_client *client;
static int verbose;

int progressfn(debuginfod_client *c __attribute__((__unused__)),
	       long a, long b)
{
  static bool first = true;
  static struct timespec last;
  struct timespec now;
  uint64_t delta;
  if (!first)
    {
      clock_gettime (CLOCK_MONOTONIC, &now);
      delta = ((now.tv_sec - last.tv_sec) * 1000000
	       + (now.tv_nsec - last.tv_nsec) / 1000);
    }
  else
    {
      first = false;
      delta = 250000;
    }

  /* Show progress the first time and then at most 5 times a second. */
  if (delta > 200000)
    {
      fprintf (stderr, "Progress %ld / %ld\n", a, b);
      clock_gettime (CLOCK_MONOTONIC, &last);
    }
  return 0;
}


static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  (void) arg;
  (void) state;
  switch (key)
    {
    case 'v': verbose++;
      debuginfod_set_progressfn (client, & progressfn);
      if (verbose > 1)
        debuginfod_set_verbose_fd (client, STDERR_FILENO);
      break;
    default: return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


/* Data structure to communicate with argp functions.  */
static struct argp argp =
  {
   options, parse_opt, args_doc, doc, NULL, NULL, NULL
  };



int
main(int argc, char** argv)
{
  elf_version (EV_CURRENT);

  client = debuginfod_begin ();
  if (client == NULL)
    {
      fprintf(stderr, "Couldn't create debuginfod client context\n");
      return 1;
    }

  /* Exercise user data pointer, to support testing only. */
  debuginfod_set_user_data (client, (void *)"Progress");

  int remaining;
  (void) argp_parse (&argp, argc, argv, ARGP_IN_ORDER|ARGP_NO_ARGS, &remaining, NULL);

  if (argc < 2 || remaining+1 == argc) /* no arguments or at least two non-option words */
    {
      argp_help (&argp, stderr, ARGP_HELP_USAGE, argv[0]);
      return 1;
    }

  /* If we were passed an ELF file name in the BUILDID slot, look in there. */
  unsigned char* build_id = (unsigned char*) argv[remaining+1];
  int build_id_len = 0; /* assume text */

  int any_non_hex = 0;
  int i;
  for (i = 0; build_id[i] != '\0'; i++)
    if ((build_id[i] >= '0' && build_id[i] <= '9') ||
        (build_id[i] >= 'a' && build_id[i] <= 'f'))
      ;
    else
      any_non_hex = 1;

  int fd = -1;
  Elf* elf = NULL;
  if (any_non_hex) /* raw build-id */
    {
      fd = open ((char*) build_id, O_RDONLY);
      if (fd < 0)
        fprintf (stderr, "Cannot open %s: %s\n", build_id, strerror(errno));
    }
  if (fd >= 0)
    {
      elf = dwelf_elf_begin (fd);
      if (elf == NULL)
        fprintf (stderr, "Cannot open as ELF file %s: %s\n", build_id,
		 elf_errmsg (-1));
    }
  if (elf != NULL)
    {
      const void *extracted_build_id;
      ssize_t s = dwelf_elf_gnu_build_id(elf, &extracted_build_id);
      if (s > 0)
        {
          /* Success: replace the build_id pointer/len with the binary blob
             that elfutils is keeping for us.  It'll remain valid until elf_end(). */
          build_id = (unsigned char*) extracted_build_id;
          build_id_len = s;
        }
      else
        fprintf (stderr, "Cannot extract build-id from %s: %s\n", build_id, elf_errmsg(-1));
    }

  char *cache_name;
  int rc = 0;

  /* Check whether FILETYPE is valid and call the appropriate
     debuginfod_find_* function. If FILETYPE is "source"
     then ensure a FILENAME was also supplied as an argument.  */
  if (strcmp(argv[remaining], "debuginfo") == 0)
    rc = debuginfod_find_debuginfo(client,
				   build_id, build_id_len,
				   &cache_name);
  else if (strcmp(argv[remaining], "executable") == 0)
    rc = debuginfod_find_executable(client,
                                    build_id, build_id_len,
				    &cache_name);
  else if (strcmp(argv[remaining], "source") == 0)
    {
      if (remaining+2 == argc || argv[remaining+2][0] != '/')
        {
          fprintf(stderr, "If FILETYPE is \"source\" then absolute /FILENAME must be given\n");
          return 1;
        }
      rc = debuginfod_find_source(client,
                                  build_id, build_id_len,
				  argv[remaining+2], &cache_name);
    }
  else if (strcmp(argv[remaining], "section") == 0)
    {
      if (remaining+2 >= argc)
	{
	  fprintf(stderr,
		  "If FILETYPE is \"section\" then a section name must be given\n");
	  return 1;
	}
      rc = debuginfod_find_section(client, build_id, build_id_len,
				   argv[remaining+2], &cache_name);
    }
  else
    {
      argp_help (&argp, stderr, ARGP_HELP_USAGE, argv[0]);
      return 1;
    }

  if (verbose)
    {
      const char* headers = debuginfod_get_headers(client);
      if (headers)
        fprintf(stderr, "Headers:\n%s", headers);
      const char* url = debuginfod_get_url (client);
      if (url != NULL)
        fprintf(stderr, "Downloaded from %s\n", url);
    }

  debuginfod_end (client);
  if (elf)
    elf_end(elf);
  if (fd >= 0)
    close (fd);

  if (rc < 0)
    {
      fprintf(stderr, "Server query failed: %s\n", strerror(-rc));
      return 1;
    }
  else
    close (rc);

  printf("%s\n", cache_name);
  free (cache_name);

  return 0;
}
