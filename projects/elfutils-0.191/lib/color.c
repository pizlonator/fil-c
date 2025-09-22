/* Handling of color output.
   Copyright (C) 2011 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2011.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <argp.h>
#include <stdlib.h>
#include <string.h>
#include "system.h"
#include "libeu.h"
#include "color.h"

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Option values.  */
#define OPT_COLOR 0x100100

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { "color", OPT_COLOR, "WHEN", OPTION_ARG_OPTIONAL,
    N_("colorize the output.  WHEN defaults to 'always' or can be 'auto' or 'never'"), 0 },

  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Parser data structure.  */
const struct argp color_argp =
  {
    options, parse_opt, NULL, NULL, NULL, NULL, NULL
  };

/* Coloring mode.  */
enum color_enum color_mode;

/* Colors to use for the various components.  */
char *color_address = "";
char *color_bytes = "";
char *color_mnemonic = "";
char *color_operand = NULL;
char *color_operand1 = "";
char *color_operand2 = "";
char *color_operand3 = "";
char *color_operand4 = "";
char *color_operand5 = "";
char *color_label = "";
char *color_undef = "";
char *color_undef_tls = "";
char *color_undef_weak = "";
char *color_symbol = "";
char *color_tls = "";
char *color_weak = "";

const char color_off[] = "\e[0m";


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case OPT_COLOR:
      if (arg == NULL)
	color_mode = color_always;
      else
	{
	  static const struct
	  {
	    const char str[7];
	    enum color_enum mode;
	  } values[] =
	      {
		{ "always", color_always },
		{ "yes", color_always },
		{ "force", color_always },
		{ "never", color_never },
		{ "no", color_never },
		{ "none", color_never },
		{ "auto", color_auto },
		{ "tty", color_auto },
		{ "if-tty", color_auto }
	      };
	  const int nvalues = sizeof (values) / sizeof (values[0]);
	  int i;
	  for (i = 0; i < nvalues; ++i)
	    if (strcmp (arg, values[i].str) == 0)
	      {
		color_mode = values[i].mode;
		if (color_mode == color_auto)
		  color_mode
		    = isatty (STDOUT_FILENO) ? color_always : color_never;
		break;
	      }
	  if (i == nvalues)
	    {
	      error (0, 0, _("\
%s: invalid argument '%s' for '--color'\n\
valid arguments are:\n\
  - 'always', 'yes', 'force'\n\
  - 'never', 'no', 'none'\n\
  - 'auto', 'tty', 'if-tty'\n"),
		     program_invocation_short_name, arg);
	      argp_help (&color_argp, stderr, ARGP_HELP_SEE,
			 (char *) program_invocation_short_name);
	      exit (EXIT_FAILURE);
	    }
	}

      if (color_mode == color_always)
	{
	  const char *env = getenv ("ELFUTILS_COLORS");
	  if (env != NULL)
	    {
	      do
		{
		  const char *start = env;
		  while (*env != '=' && *env != '\0')
		    ++env;
		  if (*env == '=' && env != start)
		    {
		      size_t name_len = env - start;
		      const char *val = ++env;
		      env = strchrnul (env, ':');
		      if (val != env)
			{
			  static const struct
			  {
			    unsigned char len;
			    const char name[sizeof (char *) - 1];
			    char **varp;
			  } known[] =
			      {
#define E(name, var) { sizeof (#name) - 1, #name,  &color_##var }
				E (a, address),
				E (b, bytes),
				E (m, mnemonic),
				E (o, operand),
				E (o1, operand1),
				E (o2, operand2),
				E (o3, operand3),
				E (o4, operand4),
				E (o5, operand5),
				E (l, label),
				E (u, undef),
				E (ut, undef_tls),
				E (uw, undef_weak),
				E (sy, symbol),
				E (st, tls),
				E (sw, weak),
			      };
			  const size_t nknown = (sizeof (known)
						 / sizeof (known[0]));

			  for (size_t i = 0; i < nknown; ++i)
			    if (name_len == known[i].len
				&& memcmp (start, known[i].name, name_len) == 0)
			      {
				*known[i].varp =
				  xasprintf ("\e[%.*sm", (int) (env - val), val);
				break;
			      }
			}
		      if (*env == ':')
			++env;
		    }
		}
	      while (*env != '\0');

	      if (color_operand != NULL)
		{
		  if (color_operand1[0] == '\0')
		    color_operand1 = color_operand;
		  if (color_operand2[0] == '\0')
		    color_operand2 = color_operand;
		  if (color_operand3[0] == '\0')
		    color_operand3 = color_operand;
		  if (color_operand4[0] == '\0')
		    color_operand4 = color_operand;
		  if (color_operand5[0] == '\0')
		    color_operand5 = color_operand;
		}
	    }
#if 0
	  else
	    {
	      // XXX Just for testing.
	      color_address = xstrdup ("\e[38;5;166;1m");
	      color_bytes = xstrdup ("\e[38;5;141m");
	      color_mnemonic = xstrdup ("\e[38;5;202;1m");
	      color_operand1 = xstrdup ("\e[38;5;220m");
	      color_operand2 = xstrdup ("\e[38;5;48m");
	      color_operand = xstrdup ("\e[38;5;112m");
	      color_label = xstrdup ("\e[38;5;21m");
	    }
#endif
	}
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}
