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

#ifndef YY_I386_I_PARSE_H_INCLUDED
# define YY_I386_I_PARSE_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int i386_debug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    kMASK = 258,                   /* kMASK  */
    kPREFIX = 259,                 /* kPREFIX  */
    kSUFFIX = 260,                 /* kSUFFIX  */
    kSYNONYM = 261,                /* kSYNONYM  */
    kID = 262,                     /* kID  */
    kNUMBER = 263,                 /* kNUMBER  */
    kPERCPERC = 264,               /* kPERCPERC  */
    kBITFIELD = 265,               /* kBITFIELD  */
    kCHAR = 266,                   /* kCHAR  */
    kSPACE = 267                   /* kSPACE  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define kMASK 258
#define kPREFIX 259
#define kSUFFIX 260
#define kSYNONYM 261
#define kID 262
#define kNUMBER 263
#define kPERCPERC 264
#define kBITFIELD 265
#define kCHAR 266
#define kSPACE 267

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 217 "i386_parse.y"

  unsigned long int num;
  char *str;
  char ch;
  struct known_bitfield *field;
  struct bitvalue *bit;
  struct argname *name;
  struct argument *arg;

#line 101 "i386_parse.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE i386_lval;


int i386_parse (void);


#endif /* !YY_I386_I_PARSE_H_INCLUDED  */
