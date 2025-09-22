/* Scanner for reccalc.   -*- C -*-

   Copyright (C) 2019-2021 Free Software Foundation, Inc.

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

/* Prologue (directives).   -*- C -*- */

/* Disable Flex features we don't need, to avoid warnings. */
%option nodefault noinput nounput noyywrap

%option reentrant

%{
#include <assert.h>
#include <limits.h> /* INT_MIN */
#include <stdlib.h> /* strtol */

#include "parse.h"
%}

%x SC_STRING

%%
%{
  // Number of opened parentheses.
  int nesting = 0;
  // A buffer storing the text inside the outer parentheses.
  char *str = NULL;
  // Its allocated size.
  int capacity = 0;
  // Its used size.
  int size = 0;
#define STR_APPEND()                                    \
  do {                                                  \
    if (capacity < size + yyleng + 1)                   \
      {                                                 \
        do                                              \
          capacity = capacity ? 2 * capacity : 128;     \
        while (capacity < size + yyleng + 1);           \
        str = realloc (str, (size_t) capacity);         \
      }                                                 \
    memcpy (str + size, yytext, (size_t) yyleng);       \
    size += yyleng;                                     \
    assert (size < capacity);                           \
  } while (0)
%}

 // Rules.

"+"      return TOK_PLUS;
"-"      return TOK_MINUS;
"*"      return TOK_STAR;
"/"      return TOK_SLASH;

"("      nesting += 1; BEGIN SC_STRING;

 /* Scan an integer.  */
[0-9]+   {
  errno = 0;
  long n = strtol (yytext, NULL, 10);
  if (! (INT_MIN <= n && n <= INT_MAX && errno != ERANGE))
    yyerror (yyscanner, res, "integer is out of range");
  yylval->TOK_NUM = (int) n;
  return TOK_NUM;
}

 /* Ignore white spaces. */
[ \t]+   continue;

"\n"     return TOK_EOL;

.        yyerror (yyscanner, res, "syntax error, invalid character: %c", yytext[0]);

<SC_STRING>
{
  "("+   nesting += yyleng; STR_APPEND ();
  ")"    {
    if (!--nesting)
      {
        BEGIN INITIAL;
        if (str)
          str[size] = 0;
        yylval->TOK_STR = str;
        return TOK_STR;
      }
    else
      STR_APPEND ();
  }
  [^()]+  STR_APPEND ();
}

<<EOF>>  return TOK_EOF;
%%
/* Epilogue (C code). */
