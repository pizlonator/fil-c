/* Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <stdbool.h>
#include <glob.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <libintl.h>
#include <ctype.h>
#include <stdio_ext.h>
#include <libgen.h>

#include <ldconfig.h>

static void
parse_conf_include (const char *config_file, unsigned int lineno,
		    bool do_chroot, const char *pattern,
		    char *opt_chroot, ldconfig_parse_config_cb cb);

static void
ldconfig_parse_config_1 (const char *filename, bool do_chroot,
			 char *opt_chroot,
			 ldconfig_parse_config_cb callback);

/* Parse a configuration file.

   Parameters:

     filename - The name of the file.  It may be a full path or relative.

     opt_chroot - If non-NULL, all paths are relative to this.

     callback - for each non-blank line in the file, this function is
	called with the line and it's location.  Will also be called
	with a NULL line at the start and end of each file, for
	file-scoped config items.
 */

void
ldconfig_parse_config (const char *filename, char *opt_chroot,
		       ldconfig_parse_config_cb callback)
{
  ldconfig_parse_config_1 (filename, true, opt_chroot, callback);
}

static void
ldconfig_parse_config_1 (const char *filename, bool do_chroot,
			 char *opt_chroot, ldconfig_parse_config_cb callback)
{
  FILE *file = NULL;
  char *line = NULL;
  const char *canon;
  size_t len = 0;
  unsigned int lineno;

  if (do_chroot && opt_chroot)
    {
      canon = chroot_canon (opt_chroot, filename);
      if (canon)
	file = fopen (canon, "r");
      else
	canon = filename;
    }
  else
    {
      canon = filename;
      file = fopen (filename, "r");
    }

  if (file == NULL)
    {
      if (errno != ENOENT)
	error (0, errno, _("\
Warning: ignoring configuration file that cannot be opened: %s"),
	       canon);
      if (canon != filename)
	free ((char *) canon);
      return;
    }

  /* No threads use this stream.  */
  __fsetlocking (file, FSETLOCKING_BYCALLER);

  if (canon != filename)
    free ((char *) canon);

  lineno = 0;
  do
    {
      ssize_t n = getline (&line, &len, file);
      if (n < 0)
	break;

      ++lineno;
      if (line[n - 1] == '\n')
	line[n - 1] = '\0';

      /* Because the file format does not know any form of quoting we
	 can search forward for the next '#' character and if found
	 make it terminating the line.  */
      *strchrnul (line, '#') = '\0';

      /* Remove leading whitespace.  NUL is no whitespace character.  */
      char *cp = line;
      while (isspace (*cp))
	++cp;

      /* If the line is blank it is ignored.  */
      if (cp[0] == '\0')
	continue;

      if (!strncmp (cp, "include", 7) && isblank (cp[7]))
	{
	  char *dir;
	  cp += 8;
	  while ((dir = strsep (&cp, " \t")) != NULL)
	    if (dir[0] != '\0')
	      parse_conf_include (filename, lineno, do_chroot, dir, opt_chroot, callback);
	}
      else
	(*callback) (cp, filename, lineno);
    }
  while (!feof_unlocked (file));

  /* Free buffer and close file.  */
  free (line);
  fclose (file);
}

/* Handle one word in an `include' line, a glob pattern of additional
   config files to read.  */
void
parse_conf_include (const char *config_file, unsigned int lineno,
		    bool do_chroot, const char *pattern, char *opt_chroot,
		    ldconfig_parse_config_cb callback)
{
  if (opt_chroot != NULL && pattern[0] != '/')
    error (EXIT_FAILURE, 0,
	   _("need absolute file name for configuration file when using -r"));

  char *copy = NULL;
  if (pattern[0] != '/' && strchr (config_file, '/') != NULL)
    {
      if (asprintf (&copy, "%s/%s", dirname (strdupa (config_file)),
		    pattern) < 0)
	error (EXIT_FAILURE, 0, _("memory exhausted"));
      pattern = copy;
    }

  glob64_t gl;
  int result;
  if (do_chroot && opt_chroot)
    {
      char *canon = chroot_canon (opt_chroot, pattern);
      if (canon == NULL)
	return;
      result = glob64 (canon, 0, NULL, &gl);
      free (canon);
    }
  else
    result = glob64 (pattern, 0, NULL, &gl);

  switch (result)
    {
    case 0:
      for (size_t i = 0; i < gl.gl_pathc; ++i)
	ldconfig_parse_config_1 (gl.gl_pathv[i], false, opt_chroot, callback);
      globfree64 (&gl);
      break;

    case GLOB_NOMATCH:
      break;

    case GLOB_NOSPACE:
      errno = ENOMEM;
      [[fallthrough]];
    case GLOB_ABORTED:
      if (opt_verbose)
	error (0, errno, _("%s:%u: cannot read directory %s"),
	       config_file, lineno, pattern);
      break;

    default:
      abort ();
      break;
    }

  free (copy);
}
