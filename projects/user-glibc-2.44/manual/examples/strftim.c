/* Time Functions Example
   Copyright (C) 1991-2026 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include <time.h>
#include <stdio.h>

int
main (void)
{
  /* This buffer is big enough that the strftime calls
     below cannot possibly exhaust it.  */
  char buf[256];

  /* Get the current time. */
  time_t curtime = time (NULL);

  /* Convert it to local time representation. */
  struct tm *lt = localtime (&curtime);
  if (!lt)
    return 1;

  /* Print the date and time in a simple format
     that is independent of locale. */
  strftime (buf, sizeof buf, "%Y-%m-%d %H:%M:%S", lt);
  puts (buf);

/*@group*/
  /* Print it in a nicer English format. */
  strftime (buf, sizeof buf, "Today is %A, %B %d.", lt);
  puts (buf);
  strftime (buf, sizeof buf, "The time is %I:%M %p.", lt);
  puts (buf);

  return 0;
}
/*@end group*/
