/* Scanner for lexcalc.   -*- C -*-

   Copyright (C) 2018-2021 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

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

/* Prologue (directives). */

/* Disable Flex features we don't need, to avoid warnings. */
%option nodefault noinput nounput noyywrap

%{
#include <errno.h>  /* errno, ERANGE */
#include <limits.h> /* INT_MIN */
#include <stdlib.h> /* strtol */

#include "parse.h"

  // Each time a rule is matched, advance the end cursor/position.
#define YY_USER_ACTION                          \
  yylloc->last_column += (int) yyleng;

  // Move the first position onto the last.
#define LOCATION_STEP()                         \
  do {                                          \
    yylloc->first_line = yylloc->last_line;     \
    yylloc->first_column = yylloc->last_column; \
  } while (0)
%}

%%
%{
  // Each time yylex is called, move the head position to the end one.
  LOCATION_STEP ();
%}
 /* Rules.  */

"+"      return TOK_PLUS;
"-"      return TOK_MINUS;
"*"      return TOK_STAR;
"/"      return TOK_SLASH;

"("      return TOK_LPAREN;
")"      return TOK_RPAREN;

 /* Scan an integer.  */
[0-9]+   {
  errno = 0;
  long n = strtol (yytext, NULL, 10);
  if (! (INT_MIN <= n && n <= INT_MAX && errno != ERANGE))
    yyerror (yylloc, nerrs, "integer is out of range");
  yylval->TOK_NUM = (int) n;
  return TOK_NUM;
}

"\n"     yylloc->last_line++; yylloc->last_column = 1; return TOK_EOL;

 /* Ignore white spaces. */
[ \t]+   LOCATION_STEP (); continue;

.        yyerror (yylloc, nerrs, "syntax error, invalid character"); continue;

<<EOF>>  return TOK_YYEOF;
%%
/* Epilogue (C code). */
