/* Return a string describing the type of a file.

   Copyright (C) 1993-1994, 2001-2002, 2004-2006, 2009-2024 Free Software
   Foundation, Inc.

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

/* Written by Paul Eggert.  */

#include <config.h>

#include "file-type.h"

#define N_(msgid) (msgid)

char const *
c_file_type (struct stat const *st)
{
  /* For some of these formats, see POSIX 1003.1-2017 "file" command,
     STDOUT section, Table: File Utility Output Strings
     <https://pubs.opengroup.org/onlinepubs/9699919799/utilities/file.html#tagtcjh_27>.

     To keep diagnostics grammatical in English, the returned string
     must start with a consonant.  */

  /* Do these three first, as they're the most common.  */

  if (S_ISREG (st->st_mode))
    return st->st_size == 0 ? N_("regular empty file") : N_("regular file");

  if (S_ISDIR (st->st_mode))
    return N_("directory");

  if (S_ISLNK (st->st_mode))
    return N_("symbolic link");

  /* Do the S_TYPEIS* macros next, as they may be implemented in terms
     of S_ISNAM, and we want the more-specialized interpretation.  */

  if (S_TYPEISMQ (st))
    return N_("message queue");

  if (S_TYPEISSEM (st))
    return N_("semaphore");

  if (S_TYPEISSHM (st))
    return N_("shared memory object");

  if (S_TYPEISTMO (st))
    return N_("typed memory object");

  /* The remaining are in alphabetical order.  */

  if (S_ISBLK (st->st_mode))
    return N_("block special file");

  if (S_ISCHR (st->st_mode))
    return N_("character special file");

  if (S_ISCTG (st->st_mode))
    return N_("contiguous data");

  if (S_ISFIFO (st->st_mode))
    return N_("fifo");

  if (S_ISDOOR (st->st_mode))
    return N_("door");

  if (S_ISMPB (st->st_mode))
    return N_("multiplexed block special file");

  if (S_ISMPC (st->st_mode))
    return N_("multiplexed character special file");

  if (S_ISMPX (st->st_mode))
    return N_("multiplexed file");

  if (S_ISNAM (st->st_mode))
    return N_("named file");

  if (S_ISNWK (st->st_mode))
    return N_("network special file");

  if (S_ISOFD (st->st_mode))
    return N_("migrated file with data");

  if (S_ISOFL (st->st_mode))
    return N_("migrated file without data");

  if (S_ISPORT (st->st_mode))
    return N_("port");

  if (S_ISSOCK (st->st_mode))
    return N_("socket");

  if (S_ISWHT (st->st_mode))
    return N_("whiteout");

  return N_("weird file");
}
