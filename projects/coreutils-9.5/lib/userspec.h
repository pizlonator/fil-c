/* Parse a 'user:group' specifier (e.g. the first argument of chown utility).
   Copyright (C) 2003-2024 Free Software Foundation, Inc.

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

/* Written by Jim Meyering, 2003.  */

#ifndef USERSPEC_H
# define USERSPEC_H 1

# include <sys/types.h>

char const *
parse_user_spec (char const *spec_arg, uid_t *uid, gid_t *gid,
                 char **username_arg, char **groupname_arg);
char const *
parse_user_spec_warn (char const *spec_arg, uid_t *uid, gid_t *gid,
                      char **username_arg, char **groupname_arg, bool *pwarn);

#endif
