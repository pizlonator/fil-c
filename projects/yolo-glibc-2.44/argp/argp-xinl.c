/* Real definitions for extern inline functions in argp.h
   Copyright (C) 1997-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Written by Miles Bader <miles@gnu.ai.mit.edu>.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined _LIBC || defined HAVE_FEATURES_H
# include <features.h>
#endif

#include <argp.h>

#ifdef _LIBC

void
__argp_usage (const struct argp_state *__state)
{
  __argp_state_help (__state, stderr, ARGP_HELP_STD_USAGE);
}
weak_alias (__argp_usage, argp_usage)

int
__option_is_short (const struct argp_option *__opt)
{
  if (__opt->flags & OPTION_DOC)
    return 0;
  else
    {
      int __key = __opt->key;
      return __key > 0 && __key <= UCHAR_MAX && isprint (__key);
    }
}
weak_alias (__option_is_short, _option_is_short)

int
__option_is_end (const struct argp_option *__opt)
{
  return !__opt->key && !__opt->name && !__opt->doc && !__opt->group;
}
weak_alias (__option_is_end, _option_is_end)

#endif
