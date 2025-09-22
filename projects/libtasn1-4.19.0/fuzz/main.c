/*
 * Copyright(c) 2017-2019 Tim Ruehsen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * This file is part of libtasn1.
 */

#include "../config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "fuzzer.h"

#ifdef TEST_RUN

# include <dirent.h>

static int
test_single_file (const char *fname)
{
  int fd, ret;
  struct stat st;
  uint8_t *data;
  ssize_t n;

  if ((fd = open (fname, O_RDONLY)) == -1)
    {
      fprintf (stderr, "Failed to open %s (%d)\n", fname, errno);
      return -1;
    }

  if (fstat (fd, &st) != 0)
    {
      fprintf (stderr, "Failed to stat %d (%d)\n", fd, errno);
      close (fd);
      return -1;
    }

  data = malloc (st.st_size);
  if ((n = read (fd, data, st.st_size)) == st.st_size)
    {
      printf ("testing %llu bytes from '%s'\n",
	      (unsigned long long) st.st_size, fname);
      fflush (stdout);
      LLVMFuzzerTestOneInput (data, st.st_size);
      fflush (stderr);
      ret = 0;
    }
  else
    {
      fprintf (stderr, "Failed to read %llu bytes from %s (%d), got %zd\n",
	       (unsigned long long) st.st_size, fname, errno, n);
      ret = -1;
    }

  free (data);
  close (fd);
  return ret;
}

static void
test_all_from (const char *dirname)
{
  DIR *dirp;
  struct dirent *dp;
  char fname[1024];
  int len;

  if ((dirp = opendir (dirname)))
    {
      while ((dp = readdir (dirp)))
	{
	  if (*dp->d_name == '.')
	    continue;

	  len =
	    snprintf (fname, sizeof (fname), "%s/%s", dirname, dp->d_name);
	  if (len < 0 || len >= (int) sizeof (fname))
	    {
	      fprintf (stderr, "File name truncation: %s/%s\n", dirname,
		       dp->d_name);
	      continue;
	    }

	  int fd;
	  if ((fd = open (fname, O_RDONLY)) == -1)
	    {
	      fprintf (stderr, "Failed to open %s (%d)\n", fname, errno);
	      continue;
	    }

	  struct stat st;
	  if (fstat (fd, &st) != 0)
	    {
	      fprintf (stderr, "Failed to stat %d (%d)\n", fd, errno);
	      close (fd);
	      continue;
	    }

	  uint8_t *data = malloc (st.st_size);
	  ssize_t n;
	  if ((n = read (fd, data, st.st_size)) == st.st_size)
	    {
	      printf ("testing %llu bytes from '%s'\n",
		      (unsigned long long) st.st_size, fname);
	      LLVMFuzzerTestOneInput (data, st.st_size);
	    }
	  else
	    fprintf (stderr,
		     "Failed to read %llu bytes from %s (%d), got %d\n",
		     (unsigned long long) st.st_size, fname, errno, (int) n);

	  free (data);
	  close (fd);
	}
      closedir (dirp);
    }
}

int
main (int argc, char **argv)
{
  int len;

  /* if VALGRIND testing is enabled, we have to call ourselves with valgrind checking */
  if (argc == 1)
    {
      const char *valgrind = getenv ("TESTS_VALGRIND");

      if (valgrind && *valgrind)
	{
	  char cmd[1024];	/* avoid alloca / VLA / heap allocation */

	  len =
	    snprintf (cmd, sizeof (cmd), "TESTS_VALGRIND=" " %s %s", valgrind,
		      argv[0]);
	  if (len < 0 || len >= (int) sizeof (cmd))
	    return 1;		/* failure on command truncation */

	  return system (cmd) != 0;
	}
    }

  if (argc > 1)
    return test_single_file (argv[1]);

  const char *target = strrchr (argv[0], '/');
  target = target ? target + 1 : argv[0];

  char corporadir[1024];	/* avoid alloca / VLA / heap allocation */

  len = snprintf (corporadir, sizeof (corporadir), SRCDIR "/%s.in", target);
  if (len < 0 || len >= (int) sizeof (corporadir))
    return 1;			/* failure on file name truncation */

  test_all_from (corporadir);

  snprintf (corporadir, sizeof (corporadir), SRCDIR "/%s.repro", target);

  test_all_from (corporadir);

  return 0;
}

#else

# ifndef __AFL_LOOP
static int
__AFL_LOOP (int n)
{
  static int first = 1;

  if (first)
    {
      first = 0;
      return 1;
    }

  return 0;
}
# endif

int
main (int argc, char **argv)
{
  int ret;
  unsigned char buf[64 * 1024];

  while (__AFL_LOOP (10000))
    {				// only works with afl-clang-fast
      ret = fread (buf, 1, sizeof (buf), stdin);
      if (ret < 0)
	return 0;

      LLVMFuzzerTestOneInput (buf, ret);
    }

  return 0;
}

#endif /* TEST_RUN */
