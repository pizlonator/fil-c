/*
 * Copyright (C) 2018-2024 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <stdlib.h>

#include "bench.h"

int
main (int argc, char *argv[])
{
  if (argc != 3)
    {
      fprintf (stderr, "Usage: %s SIZE REPETITIONS\n", argv[0]);
      exit (1);
    }

  size_t size = atol (argv[1]);
  int repeat = atoi (argv[2]);

  char *memblock = (char *) malloc (size);
  if (!memblock)
    {
      fprintf (stderr, "%s: memory exhausted\n", argv[0]);
      return 1;
    }

  /* Fill the memory block.  */
  {
    size_t i;
    for (i = 0; i < size; i++)
      memblock[i] =
        (unsigned char) (((i * (i-1) * (i-5)) >> 6) + (i % 499) + (i % 101));
  }

  struct timings_state ts;
  timing_start (&ts);

  int count;
  for (count = 0; count < repeat; count++)
    {
      char digest[64];
      FUNC (memblock, size, digest);
    }

  timing_end (&ts);
  timing_output (&ts);

  return 0;
}
