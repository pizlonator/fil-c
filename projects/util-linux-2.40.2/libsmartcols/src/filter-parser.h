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

#ifndef YY_YY_LIBSMARTCOLS_SRC_FILTER_PARSER_H_INCLUDED
# define YY_YY_LIBSMARTCOLS_SRC_FILTER_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */
#line 27 "libsmartcols/src/filter-parser.y"


#line 52 "libsmartcols/src/filter-parser.h"

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    T_NUMBER = 258,                /* T_NUMBER  */
    T_STRING = 259,                /* T_STRING  */
    T_HOLDER = 260,                /* T_HOLDER  */
    T_FLOAT = 261,                 /* T_FLOAT  */
    T_OR = 262,                    /* T_OR  */
    T_AND = 263,                   /* T_AND  */
    T_EQ = 264,                    /* T_EQ  */
    T_NE = 265,                    /* T_NE  */
    T_LT = 266,                    /* T_LT  */
    T_LE = 267,                    /* T_LE  */
    T_GT = 268,                    /* T_GT  */
    T_GE = 269,                    /* T_GE  */
    T_REG = 270,                   /* T_REG  */
    T_NREG = 271,                  /* T_NREG  */
    T_TRUE = 272,                  /* T_TRUE  */
    T_FALSE = 273,                 /* T_FALSE  */
    T_NEG = 274                    /* T_NEG  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 40 "libsmartcols/src/filter-parser.y"

	unsigned long long	param_number;
	const char*		param_string;
	const char*		param_name;
	long double		param_float;
	struct filter_node	*param;
	struct filter_node	*expr;

#line 97 "libsmartcols/src/filter-parser.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif




int yyparse (void *scanner, struct libscols_filter *fltr);


#endif /* !YY_YY_LIBSMARTCOLS_SRC_FILTER_PARSER_H_INCLUDED  */
