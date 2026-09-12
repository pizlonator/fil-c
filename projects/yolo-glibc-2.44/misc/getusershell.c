/* Copyright The GNU Toolchain Authors.
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

#include "ctype/ctype.h"
#include "iolibio.h"
#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_SHELLS _PATH_BSHELL "\0" _PATH_CSHELL

static char *shellbuf;
static char *shellend;
static char *nextshell;

static char *
address_next_shell (void)
{
  char *curshell = nextshell;
  if (nextshell == NULL)
    return curshell;

  /* init_shells guarantees we have a \0 at the end */
  char *next0 = memchr (nextshell, '\0', shellend - nextshell);
  assert (next0 != NULL);
  nextshell = memchr (next0, '/', shellend - next0);
  return curshell;
}

/* Read /etc/shells, strip unnecessary bytes, and setup nextshell */
static void
init_shells (void)
{
  free (shellbuf);
  shellbuf = NULL;
  shellend = NULL;
  nextshell = NULL;

  struct __stat64_t64 fstat;
  FILE *fp = fopen (_PATH_SHELLS, "rce");
  if (fp == NULL)
    goto default_out;
  int rc = __fstat64_time64 (__fileno (fp), &fstat);
  if (rc == -1)
    goto close_out;
  /* Consider if buflen will overflow. */
  if (fstat.st_size < 2 || fstat.st_size > PTRDIFF_MAX - 1)
    goto close_out;
  /* 1 byte for \n (will be overwritten as \0). */
  size_t buflen = fstat.st_size + 1;
  shellbuf = malloc (buflen);
  if (shellbuf == NULL)
    goto close_out;
  shellbuf[buflen - 1] = '\n';
  _IO_setbuf (fp, NULL);
  size_t cnt = _IO_fread (shellbuf, 1, fstat.st_size, fp);
  if (cnt != fstat.st_size)
    goto free_out;

  /* Loop through file and only keep shell names.
     The rest characters are set to \0 to reduce addressing workload. */
  char *top;
  char *line_start, *line_end;
  char *slash, *discard;

  top = shellbuf + buflen;
  line_start = shellbuf;
  while ((line_end = memchr (line_start, '\n', top - line_start)) != NULL)
    {
      line_end++; /* include \n */
      discard = line_start;

      slash = memchr (line_start, '/', line_end - line_start);
      if (slash == NULL)
	goto wipe_line;

      discard = memchr (line_start, '#', line_end - line_start);
      if (discard != NULL && discard < slash)
	goto wipe_line;

      (void) memset (line_start, '\0', slash - line_start);
      discard = slash;
      while (discard < line_end && *discard != '#'
	     && !isspace_l ((unsigned char) *discard, _nl_C_locobj_ptr))
	discard++;

    wipe_line:
      (void) memset (discard, '\0', line_end - discard);
      line_start = line_end;
    }

  nextshell = memchr (shellbuf, '/', top - shellbuf);
  if (nextshell == NULL)
    goto free_out;
  shellend = top;
  (void) fclose (fp);
  return;

free_out:
  free (shellbuf);
  shellbuf = NULL;
close_out:
  (void) fclose (fp);
default_out:
  /* Fallback plan: use builtin read-only default shells.
     shellbuf is NULL here so it can be freed.*/
  nextshell = (char *) DEFAULT_SHELLS;
  shellend = (char *) DEFAULT_SHELLS + sizeof (DEFAULT_SHELLS);
}

char *
getusershell (void)
{
  if (shellend == NULL)
    init_shells ();
  return address_next_shell ();
}

void
setusershell (void)
{
  if (shellend == NULL)
    init_shells ();
  else if (shellbuf != NULL)
    nextshell = memchr (shellbuf, '/', shellend - shellbuf);
  else /* shellend != NULL && shellbuf == NULL */
    nextshell = (char *) DEFAULT_SHELLS;
}

void
endusershell (void)
{
  free (shellbuf);
  shellbuf = NULL;
  shellend = NULL;
  nextshell = NULL;
}
