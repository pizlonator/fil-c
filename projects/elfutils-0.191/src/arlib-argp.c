/* Options common to ar and ranlib.
   Copyright (C) 2012 Red Hat, Inc.
   This file is part of elfutils.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <argp.h>

#include "arlib.h"

bool arlib_deterministic_output = DEFAULT_AR_DETERMINISTIC;

static const struct argp_option options[] =
  {
    { NULL, 'D', NULL, 0,
      N_("Use zero for uid, gid, and date in archive members."), 0 },
    { NULL, 'U', NULL, 0,
      N_("Use actual uid, gid, and date in archive members."), 0 },

    { NULL, 0, NULL, 0, NULL, 0 }
  };

static error_t
parse_opt (int key, char *arg __attribute__ ((unused)),
           struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case 'D':
      arlib_deterministic_output = true;
      break;

    case 'U':
      arlib_deterministic_output = false;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static char *
text_for_default (const char *text)
{
  char *new_text;
  if (unlikely (asprintf (&new_text, _("%s (default)"), text) < 0))
    return (char *) text;
  return new_text;
}

static char *
help_filter (int key, const char *text, void *input __attribute__ ((unused)))
{
  switch (key)
    {
    case 'D':
      if (DEFAULT_AR_DETERMINISTIC)
        return text_for_default (text);
      break;
    case 'U':
      if (! DEFAULT_AR_DETERMINISTIC)
        return text_for_default (text);
      break;
    }

  return (char *) text;
}

static const struct argp argp =
  {
    options, parse_opt, NULL, NULL, NULL, help_filter, NULL
  };

const struct argp_child arlib_argp_children[] =
  {
    { &argp, 0, "", 2 },
    { NULL, 0, NULL, 0 }
  };
