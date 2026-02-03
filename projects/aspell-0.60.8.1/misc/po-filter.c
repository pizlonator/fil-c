/* This file is part of The New Aspell
   Copyright (C) 2000,2001 Sergey Poznyakoff
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* This file implements the loadable filter for *.po files.
   It is an example of loadable dynamic filters for the Aspell. It
   should be compiled into a shared library. The only symbol it is
   expected to export is process(). */

#include <stdio.h>

enum state
{
  st_init,
  st_m,
  st_s,
  st_g,
  st_s1,
  st_t,
  st_r,
  st_bracket,
  st_hide,
  st_echo,
  st_newline
};

int msgstr_count = 0;
enum state state = st_init;

int
process (char *start, char *stop)
{
  int c;

  for ( ; start < stop; start++)
    {
      c = *start;
      switch (state)
	{
	case st_init:
	init:
	  switch (c)
	    {
	    default:
	      break;

	    case '#':
	      state = st_hide;
	      break;

	    case 'm':
	      state = st_m;
	      break;
	    }

	  c = ' ';
	  break;

	case st_m:
	  state = (c == 's') ? st_s : st_hide;
	  c = ' ';
	  break;

	case st_s:
	  state = (c == 'g') ? st_g : st_hide;
	  c = ' ';
	  break;

	case st_g:
	  state = (c == 's') ? st_s1 : st_hide;
	  c = ' ';
	  break;

	case st_s1:
	  state = (c == 't') ? st_t : st_hide;
	  c = ' ';
	  break;

	case st_t:
	  state = (c == 'r') ? st_r : st_hide;
	  c = ' ';
	  break;

	case st_r:
	  if (c == '[')
	    state = st_bracket;
	  else if (msgstr_count > 0)
	    state = st_echo;
	  else
	    state = st_hide;
	  msgstr_count++;
	  c = ' ';
	  break;

	case st_bracket:
	  if (c == ']')
	    {
	      state = st_echo;
	      c = ' ';
	      break;
	    }
	 /*FALLTHROUGH*/
	case st_hide:
	  if (c == '\n')
	    state = st_init;
	  else
	    c = ' ';
	  break;

	case st_newline:
	  switch (c)
	    {
	    case '"':
	      state = st_echo;
	      break;

	    default:
	      goto init;
	    }
	  break;

	case st_echo:
	  if (c == '\n')
	    state = st_newline;
	  break;
	}

      *start = c;
    }

  return 0;
}
