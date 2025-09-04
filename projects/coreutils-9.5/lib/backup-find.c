/* backupfile.c -- make Emacs style backup file names

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

#include "argmatch.h"
#include "xalloc.h"

#include <stdlib.h>

/* Relative to DIR_FD, return the name of a backup file for the
   existing file FILE, allocated with malloc.  Report an error and
   exit if out of memory.  Do not call this function if
   backup_type == no_backups.  */

char *
find_backup_file_name (int dir_fd, char const *file,
                       enum backup_type backup_type)
{
  char *result = backupfile_internal (dir_fd, file, backup_type, false);
  if (!result)
    xalloc_die ();
  return result;
}

static char const *const backup_args[] =
{
  /* In a series of synonyms, present the most meaningful first, so
     that argmatch_valid be more readable. */
  "none", "off",
  "simple", "never",
  "existing", "nil",
  "numbered", "t",
  NULL
};

static const enum backup_type backup_types[] =
{
  no_backups, no_backups,
  simple_backups, simple_backups,
  numbered_existing_backups, numbered_existing_backups,
  numbered_backups, numbered_backups
};

/* Ensure that these two vectors have the same number of elements,
   not counting the final NULL in the first one.  */
ARGMATCH_VERIFY (backup_args, backup_types);

/* Return the type of backup specified by VERSION.
   If VERSION is NULL or the empty string, return numbered_existing_backups.
   If VERSION is invalid or ambiguous, fail with a diagnostic appropriate
   for the specified CONTEXT.  Unambiguous abbreviations are accepted.  */

enum backup_type
get_version (char const *context, char const *version)
{
  if (version == 0 || *version == 0)
    return numbered_existing_backups;
  else
    return XARGMATCH (context, version, backup_args, backup_types);
}


/* Return the type of backup specified by VERSION.
   If VERSION is NULL, use the value of the envvar VERSION_CONTROL.
   If the specified string is invalid or ambiguous, fail with a diagnostic
   appropriate for the specified CONTEXT.
   Unambiguous abbreviations are accepted.  */

enum backup_type
xget_version (char const *context, char const *version)
{
  if (version && *version)
    return get_version (context, version);
  else
    return get_version ("$VERSION_CONTROL", getenv ("VERSION_CONTROL"));
}
