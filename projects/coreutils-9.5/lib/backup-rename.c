/* Rename a file to a backup name, Emacs style.

   Copyright 2017-2024 Free Software Foundation, Inc.

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

#include "backup-internal.h"

/* Relative to DIR_FD, rename the existing file FILE to a backup name,
   allocated with malloc, and return the backup name.  On failure
   return a null pointer, setting errno.  Do not call this function if
   backup_type == no_backups.  */

char *
backup_file_rename (int dir_fd, char const *file, enum backup_type backup_type)
{
  return backupfile_internal (dir_fd, file, backup_type, true);
}
