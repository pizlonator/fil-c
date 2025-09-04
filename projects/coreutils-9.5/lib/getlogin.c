/* Provide a working getlogin for systems which lack it.

   Copyright (C) 2010-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible, 2010.  */

#include <config.h>

/* Specification.  */
#include <unistd.h>

#if defined _WIN32 && ! defined __CYGWIN__
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
/* Don't assume that UNICODE is not defined.  */
# undef GetUserName
# define GetUserName GetUserNameA
#endif

char *
getlogin (void)
{
#if defined _WIN32 && ! defined __CYGWIN__
  static char login_name[1024];
  DWORD sz = sizeof (login_name);

  if (GetUserName (login_name, &sz))
    return login_name;
#endif
  return NULL;
}
