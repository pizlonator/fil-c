/* Implement chmod on platforms where it does not work correctly.

   Copyright 2022-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <sys/stat.h>

#include <errno.h>
#include <string.h>

int
rpl_chmod (const char *filename, mode_t mode)
#undef chmod
#if defined _WIN32 && !defined __CYGWIN__
# define chmod _chmod
#endif
{
  size_t len = strlen (filename);
  if (len > 0 && filename[len - 1] == '/')
    {
      struct stat st;
      if (lstat (filename, &st) < 0)
        return -1;
      if (!S_ISDIR (st.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
    }

  return chmod (filename, mode);
}
