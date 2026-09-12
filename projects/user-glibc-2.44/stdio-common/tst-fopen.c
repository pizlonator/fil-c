/* Basic test for fopen.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <support/check.h>
#include <support/temp_file.h>
#include <support/xstdio.h>

#define APPENDED_TEXT "This is appended text. "
#define DEFAULT_TEXT "Lorem ipsum dolor sit amet, consectetur " \
  "adipiscing elit, sed do eiusmod tempor incididunt ut labore et " \
  "dolore magna aliqua."
#define MAX_BUFFER_SIZE 300


static int
do_test (void)
{
  char *temp_file;
  FILE *fd_file = NULL;
  char read_buffer[MAX_BUFFER_SIZE] = "";
  size_t ret;

  /* Prepare files. */
  int fd = create_temp_file ("tst-fopen.", &temp_file);
  TEST_VERIFY_EXIT (fd != -1);
  fd_file = fdopen (fd, "w");
  ret = fwrite (DEFAULT_TEXT, sizeof (char), strlen (DEFAULT_TEXT), fd_file);
  TEST_COMPARE (ret, strlen (DEFAULT_TEXT));
  xfclose (fd_file);

  /* Test 1: This checks for fopen with mode "r".  Open text file for
     reading.  The stream is positioned at the beginning of the file. */
  printf ("Test 1: This checks for fopen with mode \"r\".\n");
  fd_file = fopen (temp_file, "r");
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_VERIFY (fd_file != NULL);
  TEST_COMPARE (ftell (fd_file), 0);
  /* Read should succeed. */
  ret = fread (read_buffer, sizeof (char), MAX_BUFFER_SIZE, fd_file);
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_COMPARE (ret, strlen (DEFAULT_TEXT));
  TEST_VERIFY (strcmp (read_buffer, DEFAULT_TEXT) == 0);
  /* Write should fail. */
  errno = 0;
  ret = fwrite (DEFAULT_TEXT, sizeof (char), strlen (DEFAULT_TEXT), fd_file);
  TEST_VERIFY (ferror (fd_file) != 0);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (ret, 0);
  clearerr (fd_file);
  /* Opening non-existent file should fail. */
  xfclose (fd_file);
  errno = 0;
  fd_file = fopen ("file-that-does-not-exist", "r");
  TEST_VERIFY (fd_file == NULL);
  TEST_COMPARE (errno, ENOENT);
  TEST_VERIFY (fd_file == NULL);

  memset (read_buffer, 0, MAX_BUFFER_SIZE);

  /* Test 2: This checks for fopen with mode "r+".  Open for reading and
     writing.  The stream is positioned at the beginning of the file. */
  printf ("Test 2: This checks for fopen with mode \"r+\".\n");
  fd_file = fopen (temp_file, "r+");
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_VERIFY (fd_file != NULL);
  TEST_COMPARE (ftell (fd_file), 0);
  /* Read should succeed. */
  ret = fread (read_buffer, sizeof (char), MAX_BUFFER_SIZE, fd_file);
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_COMPARE (ret, strlen (DEFAULT_TEXT));
  TEST_VERIFY (strcmp (read_buffer, DEFAULT_TEXT) == 0);
  fflush (fd_file);
  /* File position indicator expected at 0 + read bytes. */
  TEST_COMPARE (ftell (fd_file), ret);
  /* Write should succeed. */
  ret = fwrite (DEFAULT_TEXT, sizeof (char), strlen (DEFAULT_TEXT), fd_file);
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_COMPARE (ret, strlen (DEFAULT_TEXT));
  /* Opening non-existent file should fail. */
  xfclose (fd_file);
  errno = 0;
  fd_file = fopen ("file-that-does-not-exist", "r+");
  TEST_VERIFY (fd_file == NULL);
  TEST_COMPARE (errno, ENOENT);
  TEST_VERIFY (fd_file == NULL);

  memset (read_buffer, 0, MAX_BUFFER_SIZE);

  /* Test 3: This checks for fopen with mode "w".  Truncate file to zero
     length or create text file for writing.  The stream is positioned
     at the beginning of the file. */
  printf ("Test 3: This checks for fopen with mode \"w\".\n");
  fd_file = fopen (temp_file, "w");
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_VERIFY (fd_file != NULL);
  TEST_COMPARE (ftell (fd_file), 0);
  /* Read should fail. */
  errno = 0;
  ret = fread (read_buffer, sizeof (char), MAX_BUFFER_SIZE, fd_file);
  TEST_VERIFY (ferror (fd_file) != 0);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (ret, 0);
  clearerr (fd_file);
  /* Write should succeed. */
  ret = fwrite (DEFAULT_TEXT, sizeof (char), strlen (DEFAULT_TEXT), fd_file);
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_COMPARE (ret, strlen (DEFAULT_TEXT));
  /* Opening non-existent file should succeed. */
  xfclose (fd_file);
  fd_file = fopen ("/tmp/file-that-does-not-exist", "w");
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_VERIFY (fd_file != NULL);
  TEST_COMPARE (ftell (fd_file), 0);

  xfclose (fd_file);
  remove ("/tmp/file-that-does-not-exist");
  memset (read_buffer, 0, MAX_BUFFER_SIZE);

  /* Test 4: This checks for fopen with mode "w+".  Open for reading and
     writing.  The file is created if it does not exist, otherwise it is
     truncated.  The stream is positioned at the beginning of the file.
   */
  printf ("Test 4: This checks for fopen with mode \"w+\".\n");
  fd_file = fopen (temp_file, "w+");
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_VERIFY (fd_file != NULL);
  TEST_COMPARE (ftell (fd_file), 0);
  /* Read should succeed. */
  ret = fread (read_buffer, sizeof (char), MAX_BUFFER_SIZE, fd_file);
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_COMPARE (ret, 0);
  TEST_VERIFY (read_buffer[0] == '\0');
  /* Write should succeed. */
  ret = fwrite (DEFAULT_TEXT, sizeof (char), strlen (DEFAULT_TEXT), fd_file);
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_COMPARE (ret, strlen (DEFAULT_TEXT));
  /* Opening non-existent file should succeed. */
  xfclose (fd_file);
  fd_file = fopen ("/tmp/file-that-does-not-exist", "w+");
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_VERIFY (fd_file != NULL);
  TEST_COMPARE (ftell (fd_file), 0);

  xfclose (fd_file);
  remove ("/tmp/file-that-does-not-exist");
  memset (read_buffer, 0, MAX_BUFFER_SIZE);

  /* Test 5: This checks for fopen with mode "a".  Open for appending
     (writing at end of file).  The file is created if it does not
     exist.  The stream is positioned at the end of the file. */
  printf ("Test 5: This checks for fopen with mode \"a\".\n");
  fd_file = fopen (temp_file, "a");
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_VERIFY (fd_file != NULL);
  TEST_COMPARE (ftell (fd_file), strlen (DEFAULT_TEXT));
  /* Read should fail. */
  errno = 0;
  ret = fread (read_buffer, sizeof (char), MAX_BUFFER_SIZE, fd_file);
  TEST_VERIFY (ferror (fd_file) != 0);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (ret, 0);
  clearerr (fd_file);
  /* Write should succeed. */
  ret = fwrite (APPENDED_TEXT, sizeof (char), strlen (APPENDED_TEXT), fd_file);
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_COMPARE (ret, strlen (APPENDED_TEXT));
  /* The file position indicator for the stream is advanced by the
   *  number of bytes successfully read or written. */
  TEST_COMPARE (ftell (fd_file), strlen (DEFAULT_TEXT) + ret);
  /* Opening non-existent file should succeed. */
  xfclose (fd_file);
  fd_file = fopen ("/tmp/file-that-does-not-exist", "a");
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_VERIFY (fd_file != NULL);
  TEST_COMPARE (ftell (fd_file), 0);

  xfclose (fd_file);
  remove ("/tmp/file-that-does-not-exist");
  memset (read_buffer, 0, MAX_BUFFER_SIZE);

  /* Test 6: This checks for fopen with mode "a+".  Open for reading and
     appending (writing at end of file).  The file is created if it does
     not exist.  Output is always appended to the end of the file.  The
     initial file position for reading is at the beginning of the file,
     but it is advanced to the end prior to each write. */
  printf ("Test 6: This checks for fopen with mode \"a+\".\n");
  errno = 0;
  fd_file = fopen (temp_file, "a+");
  TEST_COMPARE (errno, 0);
  TEST_VERIFY (fd_file != NULL);
  TEST_COMPARE (ftell (fd_file), 0);
  /* Read should succeed. */
  ret = fread (read_buffer, sizeof (char), MAX_BUFFER_SIZE, fd_file);
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_COMPARE (ret, strlen (DEFAULT_TEXT) + strlen (APPENDED_TEXT));
  TEST_VERIFY (strcmp (read_buffer, DEFAULT_TEXT APPENDED_TEXT) == 0);
  /* Write should succeed. */
  const char* SECOND_APPEND = "This is second append.";
  ret = fwrite (SECOND_APPEND, sizeof (char), strlen (SECOND_APPEND), fd_file);
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_COMPARE (ret, strlen (SECOND_APPEND));
  /* The file position indicator for the stream is advanced by the
     number of bytes successfully read or written. */
  TEST_COMPARE (ftell (fd_file),
                strlen (DEFAULT_TEXT) + strlen (APPENDED_TEXT) + ret);
  /* Opening non-existent file should succeed. */
  xfclose (fd_file);
  fd_file = fopen ("/tmp/file-that-does-not-exist", "a+");
  TEST_COMPARE (ferror (fd_file), 0);
  TEST_VERIFY (fd_file != NULL);
  TEST_COMPARE (ftell (fd_file), 0);

  xfclose (fd_file);
  remove ("/tmp/file-that-does-not-exist");
  memset (read_buffer, 0, MAX_BUFFER_SIZE);

  /* Test 7: This checks for fopen with other valid modes set, such as
     "rc", "we" or "am".  The test calls fopen with these modes and
     checks that no errors appear.  */
  printf ("Test 7: This checks for fopen with other valid modes set, "
          "such as \"rc\", \"we\" or \"am\".\n");
  /* These modes all operate correctly with the file already present. */
  static const char *valid_modes[] =
    { "rc", "we", "am", "r+x", "wb+", "ab", 0 };
  const char **p = valid_modes;
  while (*p != 0)
    {
      fd_file = fopen (temp_file, *p);
      TEST_COMPARE (ferror (fd_file), 0);
      TEST_VERIFY (fd_file != NULL);
      xfclose (fd_file);
      ++p;
    }

  /* Test 8: This checks for fopen with invalid modes.  The test calls
     fopen with these modes and checks that opening existing files with
     invalid mode fails and that opening non-existing files with invalid
     mode doesn't create a new file. */
  printf ("Test 8: This checks for fopen with invalid modes.\n");
  static const char *invalid_modes[] = { "0", "tr", "z", "x", " ", 0 };
  p = invalid_modes;
  while (*p != 0)
    {
      errno = 0;
      fd_file = fopen (temp_file, *p);
      TEST_VERIFY (fd_file == NULL);
      TEST_COMPARE (errno, EINVAL);
      errno = 0;
      fd_file = fopen ("/tmp/file-that-does-not-exist", *p);
      TEST_VERIFY (fd_file == NULL);
      TEST_COMPARE (errno, EINVAL);
      ++p;
      TEST_VERIFY (access ("/tmp/file-that-does-not-exist", F_OK) == -1);
    }

  return 0;
}

#include <support/test-driver.c>
