/* Support to debug branch prediction.
   Copyright (C) 2007 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2007.

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

#include <stdio.h>

#if DEBUGPRED
extern const unsigned long int __start_predict_data;
extern const unsigned long int __stop_predict_data;
extern const unsigned long int __start_predict_line;
extern const char *const __start_predict_file;

static void
__attribute__ ((destructor))
predprint (void)
{
  const unsigned long int *s = &__start_predict_data;
  const unsigned long int *e = &__stop_predict_data;
  const unsigned long int *sl = &__start_predict_line;
  const char *const *sf = &__start_predict_file;
  while (s < e)
    {
      if (s[0] != 0 || s[1] != 0)
	printf ("%s:%lu: wrong=%lu, correct=%lu%s\n", *sf, *sl, s[0], s[1],
		s[0] > s[1] ? "   <==== WARNING" : "");
      ++sl;
      ++sf;
      s += 2;
    }
}
#endif
