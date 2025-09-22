%{
/* Copyright (C) 2004, 2005, 2007, 2008 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2004.

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

#include <ctype.h>

#include <libeu.h>
#include "system.h"
#include "i386_parse.h"


static void eat_to_eol (void);
static void invalid_char (int ch);
%}

ID              [a-zA-Z_][a-zA-Z0-9_/]*
ID2             [a-zA-Z0-9_:/]*
NUMBER		[0-9]+
WHITE		[[:space:]]+

%option yylineno
%option never-interactive
%option noyywrap


%x MAIN

%%

"%mask"				{ return kMASK; }

"%prefix"			{ return kPREFIX; }
"%suffix"			{ return kSUFFIX; }

"%synonym"			{ return kSYNONYM; }

{NUMBER}			{ i386_lval.num = strtoul (yytext, NULL, 10);
				  return kNUMBER; }

"%%"				{ BEGIN (MAIN); return kPERCPERC; }


<MAIN>"0"			{ return '0'; }
<MAIN>"1"			{ return '1'; }

<INITIAL,MAIN>"{"{ID2}"}"	{ i386_lval.str = xstrndup (yytext + 1,
							    yyleng - 2);
				  return kBITFIELD; }

<MAIN>"INVALID"			{ i386_lval.str = (void *) -1l;
				  return kID; }

<MAIN>{ID}			{ i386_lval.str = xstrndup (yytext, yyleng);
				  return kID; }

<MAIN>","			{ return ','; }

<MAIN>":"			{ return ':'; }

<INITIAL,MAIN>^"\n"		{ /* IGNORE */ }

<INITIAL,MAIN>"\n"		{ return '\n'; }

<INITIAL,MAIN>^"#"		{ eat_to_eol (); }

{WHITE}				{ /* IGNORE */ }

<MAIN>{WHITE}			{ return kSPACE; }

<MAIN>.				{ i386_lval.ch = *yytext; return kCHAR; }

.				{ invalid_char (*yytext); }


%%

static void
eat_to_eol (void)
{
  while (1)
    {
      int c = input ();

      if (c == EOF || c == '\n')
	break;
    }
}

static void
invalid_char (int ch)
{
  error (0, 0, (isascii (ch)
		? _("invalid character '%c' at line %d; ignored")
		: _("invalid character '\\%o' at line %d; ignored")),
	 ch, yylineno);
}

// Local Variables:
// mode: C
// End:
