/* Check LD_AUDIT and LD_BIND_NOW.  Common definitions.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

static void
compare_output (void *buffer, size_t length, const char *ref[], size_t reflen)
{
  FILE *in = fmemopen (buffer, length, "r");
  TEST_VERIFY_EXIT (in != NULL);
  char *line = NULL;
  size_t linelen = 0;

  bool found[reflen];
  for (int i = 0; i < reflen; i++)
    found[i] = false;

  size_t nlines = 0;
  while (xgetline (&line, &linelen, in))
    {
      for (int i = 0; i < reflen; i++)
	if (strcmp (line, ref[i]) == 0)
	  {
	    TEST_COMPARE (found[i], false);
	    found[i] = true;
	  }
      nlines++;
    }

  TEST_COMPARE (reflen, nlines);
  for (int i = 0; i < reflen; i++)
    TEST_COMPARE (found[i], true);

  free (line);
  fclose (in);
}
