%{
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>

#include "parse_y.h"
%}

%option noyywrap
%%

\#[\$]+[a-zA-Z]*(\=[0-9]+)?          return NEW_COUNTER;
\#\{[a-zA-Z][a-zA-Z0-9\_]*\}         return LABEL;
\#                                   return NO_INDENT;
\#\#                                 return RIGHT;
\\\#                                 return HASH;
[^\n]                                return CHAR;
[\n]                                 return NEWLINE;

%%
