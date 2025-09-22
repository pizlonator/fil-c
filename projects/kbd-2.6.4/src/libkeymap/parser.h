/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

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

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_PARSER_H_INCLUDED
# define YY_YY_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */
#line 24 "parser.y"

#include <kbdfile.h>
#include "keymap.h"

#ifndef STRDATA_STRUCT
#define STRDATA_STRUCT
#define MAX_PARSER_STRING 512
struct strdata {
	unsigned long len;
	unsigned char data[MAX_PARSER_STRING];
};
#endif

#line 63 "parser.h"

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    EOL = 258,                     /* EOL  */
    NUMBER = 259,                  /* NUMBER  */
    LITERAL = 260,                 /* LITERAL  */
    CHARSET = 261,                 /* CHARSET  */
    KEYMAPS = 262,                 /* KEYMAPS  */
    KEYCODE = 263,                 /* KEYCODE  */
    EQUALS = 264,                  /* EQUALS  */
    PLAIN = 265,                   /* PLAIN  */
    SHIFT = 266,                   /* SHIFT  */
    CONTROL = 267,                 /* CONTROL  */
    ALT = 268,                     /* ALT  */
    ALTGR = 269,                   /* ALTGR  */
    SHIFTL = 270,                  /* SHIFTL  */
    SHIFTR = 271,                  /* SHIFTR  */
    CTRLL = 272,                   /* CTRLL  */
    CTRLR = 273,                   /* CTRLR  */
    CAPSSHIFT = 274,               /* CAPSSHIFT  */
    COMMA = 275,                   /* COMMA  */
    DASH = 276,                    /* DASH  */
    STRING = 277,                  /* STRING  */
    STRLITERAL = 278,              /* STRLITERAL  */
    COMPOSE = 279,                 /* COMPOSE  */
    TO = 280,                      /* TO  */
    CCHAR = 281,                   /* CCHAR  */
    ERROR = 282,                   /* ERROR  */
    PLUS = 283,                    /* PLUS  */
    UNUMBER = 284,                 /* UNUMBER  */
    ALT_IS_META = 285,             /* ALT_IS_META  */
    STRINGS = 286,                 /* STRINGS  */
    AS = 287,                      /* AS  */
    USUAL = 288,                   /* USUAL  */
    ON = 289,                      /* ON  */
    FOR = 290                      /* FOR  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define EOL 258
#define NUMBER 259
#define LITERAL 260
#define CHARSET 261
#define KEYMAPS 262
#define KEYCODE 263
#define EQUALS 264
#define PLAIN 265
#define SHIFT 266
#define CONTROL 267
#define ALT 268
#define ALTGR 269
#define SHIFTL 270
#define SHIFTR 271
#define CTRLL 272
#define CTRLR 273
#define CAPSSHIFT 274
#define COMMA 275
#define DASH 276
#define STRING 277
#define STRLITERAL 278
#define COMPOSE 279
#define TO 280
#define CCHAR 281
#define ERROR 282
#define PLUS 283
#define UNUMBER 284
#define ALT_IS_META 285
#define STRINGS 286
#define AS 287
#define USUAL 288
#define ON 289
#define FOR 290

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 56 "parser.y"

	int num;
	struct strdata str;

#line 158 "parser.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif




int yyparse (void *scanner, struct lk_ctx *ctx);


#endif /* !YY_YY_PARSER_H_INCLUDED  */
