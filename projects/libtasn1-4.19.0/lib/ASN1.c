/* A Bison parser, made by GNU Bison 3.7.5.  */

/* Bison implementation for Yacc-like parsers in C

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30705

/* Bison version string.  */
#define YYBISON_VERSION "3.7.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         _ASN1_YYSTYPE
/* Substitute the variable and function names.  */
#define yyparse         _asn1_yyparse
#define yylex           _asn1_yylex
#define yyerror         _asn1_yyerror
#define yydebug         _asn1_yydebug
#define yynerrs         _asn1_yynerrs
#define yylval          _asn1_yylval
#define yychar          _asn1_yychar

/* First part of user prologue.  */
#line 1 "ASN1.y"

/*
 * Copyright (C) 2001-2022 Free Software Foundation, Inc.
 *
 * This file is part of LIBTASN1.
 *
 * The LIBTASN1 library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

/*****************************************************/
/* File: x509_ASN.y                                  */
/* Description: input file for 'bison' program.      */
/*   The output file is a parser (in C language) for */
/*   ASN.1 syntax                                    */
/*****************************************************/

#include <int.h>
#include <parser_aux.h>
#include <structure.h>
#include <libtasn1.h>
#include "c-ctype.h"

static list_type *e_list = NULL;
static FILE *file_asn1;			/* Pointer to file to parse */
static int result_parse = 0;	/* result of the parser
					   algorithm */
static asn1_node p_tree;		/* pointer to the root of the
					   structure created by the
					   parser*/
static unsigned int line_number;	/* line number describing the
					   parser position inside the
					   file */
static char last_error[ASN1_MAX_ERROR_DESCRIPTION_SIZE] = "";
static char last_error_token[ASN1_MAX_ERROR_DESCRIPTION_SIZE+1] = ""; /* used when expected errors occur */
static char last_token[ASN1_MAX_NAME_SIZE+1] = ""; /* last token find in the file
					   to parse before the 'parse
					   error' */
extern char _asn1_identifierMissing[];
static const char *file_name;		/* file to parse */

static void _asn1_yyerror (const char *);
static int _asn1_yylex(void);

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#define SAFE_COPY(dst, dst_size, fmt, ...) { snprintf(dst, dst_size, fmt, __VA_ARGS__); }
#else
#define SAFE_COPY(dst, dst_size, fmt, ...) { \
  int _ret = snprintf(dst, dst_size, fmt, __VA_ARGS__); \
  if (_ret != (int)strlen(dst)) \
    { \
      fprintf(stderr, "%s:%u: Oversize value\n", \
               file_name, line_number); \
      exit(1); \
    } \
}
#endif

#line 150 "ASN1.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif


/* Debug traces.  */
#ifndef _ASN1_YYDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define _ASN1_YYDEBUG 1
#  else
#   define _ASN1_YYDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define _ASN1_YYDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined _ASN1_YYDEBUG */
#if _ASN1_YYDEBUG
extern int _asn1_yydebug;
#endif

/* Token kinds.  */
#ifndef _ASN1_YYTOKENTYPE
# define _ASN1_YYTOKENTYPE
  enum _asn1_yytokentype
  {
    _ASN1_YYEMPTY = -2,
    _ASN1_YYEOF = 0,               /* "end of file"  */
    _ASN1_YYerror = 256,           /* error  */
    _ASN1_YYUNDEF = 257,           /* "invalid token"  */
    ASSIG = 258,                   /* "::="  */
    NUM = 259,                     /* NUM  */
    IDENTIFIER = 260,              /* IDENTIFIER  */
    OPTIONAL = 261,                /* OPTIONAL  */
    INTEGER = 262,                 /* INTEGER  */
    SIZE = 263,                    /* SIZE  */
    OCTET = 264,                   /* OCTET  */
    STRING = 265,                  /* STRING  */
    SEQUENCE = 266,                /* SEQUENCE  */
    BIT = 267,                     /* BIT  */
    UNIVERSAL = 268,               /* UNIVERSAL  */
    PRIVATE = 269,                 /* PRIVATE  */
    APPLICATION = 270,             /* APPLICATION  */
    DEFAULT = 271,                 /* DEFAULT  */
    CHOICE = 272,                  /* CHOICE  */
    OF = 273,                      /* OF  */
    OBJECT = 274,                  /* OBJECT  */
    STR_IDENTIFIER = 275,          /* STR_IDENTIFIER  */
    BOOLEAN = 276,                 /* BOOLEAN  */
    ASN1_TRUE = 277,               /* ASN1_TRUE  */
    ASN1_FALSE = 278,              /* ASN1_FALSE  */
    TOKEN_NULL = 279,              /* TOKEN_NULL  */
    ANY = 280,                     /* ANY  */
    DEFINED = 281,                 /* DEFINED  */
    BY = 282,                      /* BY  */
    SET = 283,                     /* SET  */
    EXPLICIT = 284,                /* EXPLICIT  */
    IMPLICIT = 285,                /* IMPLICIT  */
    DEFINITIONS = 286,             /* DEFINITIONS  */
    TAGS = 287,                    /* TAGS  */
    BEGIN = 288,                   /* BEGIN  */
    END = 289,                     /* END  */
    UTCTime = 290,                 /* UTCTime  */
    GeneralizedTime = 291,         /* GeneralizedTime  */
    GeneralString = 292,           /* GeneralString  */
    NumericString = 293,           /* NumericString  */
    IA5String = 294,               /* IA5String  */
    TeletexString = 295,           /* TeletexString  */
    PrintableString = 296,         /* PrintableString  */
    UniversalString = 297,         /* UniversalString  */
    BMPString = 298,               /* BMPString  */
    UTF8String = 299,              /* UTF8String  */
    VisibleString = 300,           /* VisibleString  */
    FROM = 301,                    /* FROM  */
    IMPORTS = 302,                 /* IMPORTS  */
    ENUMERATED = 303               /* ENUMERATED  */
  };
  typedef enum _asn1_yytokentype _asn1_yytoken_kind_t;
#endif
/* Token kinds.  */
#define _ASN1_YYEMPTY -2
#define _ASN1_YYEOF 0
#define _ASN1_YYerror 256
#define _ASN1_YYUNDEF 257
#define ASSIG 258
#define NUM 259
#define IDENTIFIER 260
#define OPTIONAL 261
#define INTEGER 262
#define SIZE 263
#define OCTET 264
#define STRING 265
#define SEQUENCE 266
#define BIT 267
#define UNIVERSAL 268
#define PRIVATE 269
#define APPLICATION 270
#define DEFAULT 271
#define CHOICE 272
#define OF 273
#define OBJECT 274
#define STR_IDENTIFIER 275
#define BOOLEAN 276
#define ASN1_TRUE 277
#define ASN1_FALSE 278
#define TOKEN_NULL 279
#define ANY 280
#define DEFINED 281
#define BY 282
#define SET 283
#define EXPLICIT 284
#define IMPLICIT 285
#define DEFINITIONS 286
#define TAGS 287
#define BEGIN 288
#define END 289
#define UTCTime 290
#define GeneralizedTime 291
#define GeneralString 292
#define NumericString 293
#define IA5String 294
#define TeletexString 295
#define PrintableString 296
#define UniversalString 297
#define BMPString 298
#define UTF8String 299
#define VisibleString 300
#define FROM 301
#define IMPORTS 302
#define ENUMERATED 303

/* Value type.  */
#if ! defined _ASN1_YYSTYPE && ! defined _ASN1_YYSTYPE_IS_DECLARED
union _ASN1_YYSTYPE
{
#line 77 "ASN1.y"

  unsigned int constant;
  char str[ASN1_MAX_NAME_SIZE+1];
  asn1_node node;

#line 310 "ASN1.c"

};
typedef union _ASN1_YYSTYPE _ASN1_YYSTYPE;
# define _ASN1_YYSTYPE_IS_TRIVIAL 1
# define _ASN1_YYSTYPE_IS_DECLARED 1
#endif


extern _ASN1_YYSTYPE _asn1_yylval;

int _asn1_yyparse (void);


/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_ASSIG = 3,                      /* "::="  */
  YYSYMBOL_NUM = 4,                        /* NUM  */
  YYSYMBOL_IDENTIFIER = 5,                 /* IDENTIFIER  */
  YYSYMBOL_OPTIONAL = 6,                   /* OPTIONAL  */
  YYSYMBOL_INTEGER = 7,                    /* INTEGER  */
  YYSYMBOL_SIZE = 8,                       /* SIZE  */
  YYSYMBOL_OCTET = 9,                      /* OCTET  */
  YYSYMBOL_STRING = 10,                    /* STRING  */
  YYSYMBOL_SEQUENCE = 11,                  /* SEQUENCE  */
  YYSYMBOL_BIT = 12,                       /* BIT  */
  YYSYMBOL_UNIVERSAL = 13,                 /* UNIVERSAL  */
  YYSYMBOL_PRIVATE = 14,                   /* PRIVATE  */
  YYSYMBOL_APPLICATION = 15,               /* APPLICATION  */
  YYSYMBOL_DEFAULT = 16,                   /* DEFAULT  */
  YYSYMBOL_CHOICE = 17,                    /* CHOICE  */
  YYSYMBOL_OF = 18,                        /* OF  */
  YYSYMBOL_OBJECT = 19,                    /* OBJECT  */
  YYSYMBOL_STR_IDENTIFIER = 20,            /* STR_IDENTIFIER  */
  YYSYMBOL_BOOLEAN = 21,                   /* BOOLEAN  */
  YYSYMBOL_ASN1_TRUE = 22,                 /* ASN1_TRUE  */
  YYSYMBOL_ASN1_FALSE = 23,                /* ASN1_FALSE  */
  YYSYMBOL_TOKEN_NULL = 24,                /* TOKEN_NULL  */
  YYSYMBOL_ANY = 25,                       /* ANY  */
  YYSYMBOL_DEFINED = 26,                   /* DEFINED  */
  YYSYMBOL_BY = 27,                        /* BY  */
  YYSYMBOL_SET = 28,                       /* SET  */
  YYSYMBOL_EXPLICIT = 29,                  /* EXPLICIT  */
  YYSYMBOL_IMPLICIT = 30,                  /* IMPLICIT  */
  YYSYMBOL_DEFINITIONS = 31,               /* DEFINITIONS  */
  YYSYMBOL_TAGS = 32,                      /* TAGS  */
  YYSYMBOL_BEGIN = 33,                     /* BEGIN  */
  YYSYMBOL_END = 34,                       /* END  */
  YYSYMBOL_UTCTime = 35,                   /* UTCTime  */
  YYSYMBOL_GeneralizedTime = 36,           /* GeneralizedTime  */
  YYSYMBOL_GeneralString = 37,             /* GeneralString  */
  YYSYMBOL_NumericString = 38,             /* NumericString  */
  YYSYMBOL_IA5String = 39,                 /* IA5String  */
  YYSYMBOL_TeletexString = 40,             /* TeletexString  */
  YYSYMBOL_PrintableString = 41,           /* PrintableString  */
  YYSYMBOL_UniversalString = 42,           /* UniversalString  */
  YYSYMBOL_BMPString = 43,                 /* BMPString  */
  YYSYMBOL_UTF8String = 44,                /* UTF8String  */
  YYSYMBOL_VisibleString = 45,             /* VisibleString  */
  YYSYMBOL_FROM = 46,                      /* FROM  */
  YYSYMBOL_IMPORTS = 47,                   /* IMPORTS  */
  YYSYMBOL_ENUMERATED = 48,                /* ENUMERATED  */
  YYSYMBOL_49_ = 49,                       /* '+'  */
  YYSYMBOL_50_ = 50,                       /* '-'  */
  YYSYMBOL_51_ = 51,                       /* '('  */
  YYSYMBOL_52_ = 52,                       /* ')'  */
  YYSYMBOL_53_ = 53,                       /* ','  */
  YYSYMBOL_54_ = 54,                       /* '['  */
  YYSYMBOL_55_ = 55,                       /* ']'  */
  YYSYMBOL_56_ = 56,                       /* '|'  */
  YYSYMBOL_57_ = 57,                       /* '{'  */
  YYSYMBOL_58_ = 58,                       /* '}'  */
  YYSYMBOL_59_ = 59,                       /* '.'  */
  YYSYMBOL_YYACCEPT = 60,                  /* $accept  */
  YYSYMBOL_definitions = 61,               /* definitions  */
  YYSYMBOL_pos_num = 62,                   /* pos_num  */
  YYSYMBOL_neg_num = 63,                   /* neg_num  */
  YYSYMBOL_pos_neg_num = 64,               /* pos_neg_num  */
  YYSYMBOL_num_identifier = 65,            /* num_identifier  */
  YYSYMBOL_int_identifier = 66,            /* int_identifier  */
  YYSYMBOL_pos_neg_identifier = 67,        /* pos_neg_identifier  */
  YYSYMBOL_constant = 68,                  /* constant  */
  YYSYMBOL_constant_list = 69,             /* constant_list  */
  YYSYMBOL_obj_constant = 70,              /* obj_constant  */
  YYSYMBOL_obj_constant_list = 71,         /* obj_constant_list  */
  YYSYMBOL_class = 72,                     /* class  */
  YYSYMBOL_tag_type = 73,                  /* tag_type  */
  YYSYMBOL_tag = 74,                       /* tag  */
  YYSYMBOL_default = 75,                   /* default  */
  YYSYMBOL_pos_neg_list = 76,              /* pos_neg_list  */
  YYSYMBOL_integer_def = 77,               /* integer_def  */
  YYSYMBOL_boolean_def = 78,               /* boolean_def  */
  YYSYMBOL_Time = 79,                      /* Time  */
  YYSYMBOL_size_def2 = 80,                 /* size_def2  */
  YYSYMBOL_size_def = 81,                  /* size_def  */
  YYSYMBOL_generalstring_def = 82,         /* generalstring_def  */
  YYSYMBOL_numericstring_def = 83,         /* numericstring_def  */
  YYSYMBOL_ia5string_def = 84,             /* ia5string_def  */
  YYSYMBOL_teletexstring_def = 85,         /* teletexstring_def  */
  YYSYMBOL_printablestring_def = 86,       /* printablestring_def  */
  YYSYMBOL_universalstring_def = 87,       /* universalstring_def  */
  YYSYMBOL_bmpstring_def = 88,             /* bmpstring_def  */
  YYSYMBOL_utf8string_def = 89,            /* utf8string_def  */
  YYSYMBOL_visiblestring_def = 90,         /* visiblestring_def  */
  YYSYMBOL_octet_string_def = 91,          /* octet_string_def  */
  YYSYMBOL_bit_element = 92,               /* bit_element  */
  YYSYMBOL_bit_element_list = 93,          /* bit_element_list  */
  YYSYMBOL_bit_string_def = 94,            /* bit_string_def  */
  YYSYMBOL_enumerated_def = 95,            /* enumerated_def  */
  YYSYMBOL_object_def = 96,                /* object_def  */
  YYSYMBOL_type_assig_right = 97,          /* type_assig_right  */
  YYSYMBOL_type_assig_right_tag = 98,      /* type_assig_right_tag  */
  YYSYMBOL_type_assig_right_tag_default = 99, /* type_assig_right_tag_default  */
  YYSYMBOL_type_assig = 100,               /* type_assig  */
  YYSYMBOL_type_assig_list = 101,          /* type_assig_list  */
  YYSYMBOL_sequence_def = 102,             /* sequence_def  */
  YYSYMBOL_set_def = 103,                  /* set_def  */
  YYSYMBOL_choise_def = 104,               /* choise_def  */
  YYSYMBOL_any_def = 105,                  /* any_def  */
  YYSYMBOL_known_string = 106,             /* known_string  */
  YYSYMBOL_type_invalid = 107,             /* type_invalid  */
  YYSYMBOL_type_def = 108,                 /* type_def  */
  YYSYMBOL_constant_def = 109,             /* constant_def  */
  YYSYMBOL_type_constant = 110,            /* type_constant  */
  YYSYMBOL_type_constant_list = 111,       /* type_constant_list  */
  YYSYMBOL_definitions_id = 112,           /* definitions_id  */
  YYSYMBOL_explicit_implicit = 113         /* explicit_implicit  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined _ASN1_YYSTYPE_IS_TRIVIAL && _ASN1_YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  5
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   248

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  60
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  54
/* YYNRULES -- Number of rules.  */
#define YYNRULES  137
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  238

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   303


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      51,    52,     2,    49,    53,    50,    59,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    54,     2,    55,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    57,    56,    58,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48
};

#if _ASN1_YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   151,   151,   164,   165,   168,   171,   172,   175,   176,
     179,   180,   181,   184,   185,   188,   190,   195,   196,   200,
     202,   207,   208,   212,   213,   214,   217,   219,   223,   224,
     225,   228,   230,   231,   235,   236,   240,   241,   243,   244,
     248,   251,   252,   255,   257,   263,   264,   267,   268,   272,
     273,   277,   278,   282,   283,   287,   288,   292,   293,   297,
     298,   302,   303,   307,   308,   312,   313,   317,   322,   323,
     327,   328,   330,   335,   341,   344,   346,   349,   350,   351,
     352,   353,   354,   355,   356,   357,   358,   359,   360,   361,
     362,   363,   364,   365,   366,   367,   368,   369,   372,   373,
     378,   379,   382,   385,   388,   389,   393,   395,   397,   402,
     404,   406,   411,   415,   416,   421,   422,   423,   424,   425,
     426,   427,   428,   431,   438,   441,   445,   450,   456,   457,
     458,   461,   462,   473,   476,   478,   502,   503
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "\"::=\"", "NUM",
  "IDENTIFIER", "OPTIONAL", "INTEGER", "SIZE", "OCTET", "STRING",
  "SEQUENCE", "BIT", "UNIVERSAL", "PRIVATE", "APPLICATION", "DEFAULT",
  "CHOICE", "OF", "OBJECT", "STR_IDENTIFIER", "BOOLEAN", "ASN1_TRUE",
  "ASN1_FALSE", "TOKEN_NULL", "ANY", "DEFINED", "BY", "SET", "EXPLICIT",
  "IMPLICIT", "DEFINITIONS", "TAGS", "BEGIN", "END", "UTCTime",
  "GeneralizedTime", "GeneralString", "NumericString", "IA5String",
  "TeletexString", "PrintableString", "UniversalString", "BMPString",
  "UTF8String", "VisibleString", "FROM", "IMPORTS", "ENUMERATED", "'+'",
  "'-'", "'('", "')'", "','", "'['", "']'", "'|'", "'{'", "'}'", "'.'",
  "$accept", "definitions", "pos_num", "neg_num", "pos_neg_num",
  "num_identifier", "int_identifier", "pos_neg_identifier", "constant",
  "constant_list", "obj_constant", "obj_constant_list", "class",
  "tag_type", "tag", "default", "pos_neg_list", "integer_def",
  "boolean_def", "Time", "size_def2", "size_def", "generalstring_def",
  "numericstring_def", "ia5string_def", "teletexstring_def",
  "printablestring_def", "universalstring_def", "bmpstring_def",
  "utf8string_def", "visiblestring_def", "octet_string_def", "bit_element",
  "bit_element_list", "bit_string_def", "enumerated_def", "object_def",
  "type_assig_right", "type_assig_right_tag",
  "type_assig_right_tag_default", "type_assig", "type_assig_list",
  "sequence_def", "set_def", "choise_def", "any_def", "known_string",
  "type_invalid", "type_def", "constant_def", "type_constant",
  "type_constant_list", "definitions_id", "explicit_implicit", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,    43,
      45,    40,    41,    44,    91,    93,   124,   123,   125,    46
};
#endif

#define YYPACT_NINF (-140)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-12)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      41,   -27,    32,    17,     0,  -140,    90,  -140,    19,  -140,
    -140,  -140,     3,  -140,  -140,    24,   126,  -140,  -140,    98,
      80,   105,  -140,   141,    30,  -140,  -140,  -140,  -140,  -140,
    -140,  -140,  -140,   131,  -140,  -140,  -140,  -140,    84,    67,
     148,   155,   170,   107,  -140,  -140,     6,    91,   184,    18,
     185,   139,   177,  -140,  -140,   172,    36,  -140,  -140,     6,
       6,     6,     6,     6,     6,     6,     6,     6,   142,    25,
     145,   128,   149,  -140,  -140,  -140,  -140,  -140,  -140,  -140,
    -140,  -140,  -140,  -140,  -140,  -140,  -140,  -140,  -140,  -140,
    -140,  -140,  -140,  -140,   144,    64,   199,   174,   152,   196,
    -140,  -140,    26,     6,   128,   200,   189,    43,   200,  -140,
     179,   128,   200,   190,  -140,  -140,  -140,  -140,  -140,  -140,
    -140,  -140,  -140,   204,   156,  -140,  -140,  -140,   206,  -140,
    -140,  -140,    92,   173,  -140,   208,   209,  -140,  -140,  -140,
     157,   211,   188,   164,   166,    64,  -140,   -11,  -140,  -140,
      67,  -140,    27,   128,   204,  -140,    78,   213,  -140,    97,
     128,   168,  -140,   101,  -140,   165,   162,  -140,   218,  -140,
     167,    10,     5,  -140,  -140,   173,   169,  -140,    -7,  -140,
      64,   171,    26,  -140,    37,  -140,   200,  -140,  -140,   104,
    -140,  -140,  -140,  -140,   221,   204,  -140,  -140,   175,   176,
    -140,    64,  -140,     7,   197,  -140,   178,   180,  -140,  -140,
    -140,    94,  -140,  -140,  -140,   181,  -140,    23,  -140,  -140,
     219,   188,  -140,  -140,  -140,  -140,  -140,  -140,  -140,  -140,
     225,   186,   220,   187,  -140,  -140,  -140,  -140
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,   135,     0,     0,     0,     1,     0,     8,     9,   134,
      19,    21,     0,   136,   137,     0,     0,   133,    22,     0,
       0,     0,    20,     0,     0,   120,   119,   121,   117,   118,
     122,   115,   116,     0,   129,   128,   130,   131,     0,     0,
       0,     0,     0,     0,     2,   132,    75,    36,     0,     0,
       0,     0,     0,    40,    97,   113,     0,    41,    42,    47,
      49,    51,    53,    55,    57,    59,    61,    63,     0,     0,
      28,     0,    77,    79,    80,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    81,    82,    78,    93,    98,   124,
      92,    96,    94,    95,     0,     0,     0,     0,     0,     0,
      45,    76,     0,    65,     0,     0,     0,    70,     0,    74,
       0,     0,     0,     0,    48,    50,    52,    54,    56,    58,
      60,    62,    64,     0,     0,    23,    24,    25,     0,    29,
      30,    99,     0,     0,     3,     0,     0,     6,     7,   127,
       0,     0,     0,     0,     0,     0,    17,     0,    66,   107,
       0,   104,     0,     0,     0,    71,     0,     0,   110,     0,
       0,     0,    68,     0,    26,     0,     3,    12,     0,    34,
       0,     0,     0,     4,     5,     0,     0,     9,     0,    46,
       0,     0,     0,    37,   100,   103,     0,   106,   108,     0,
     112,   114,   109,   111,     0,     0,    73,    27,     5,     0,
      38,     0,   126,     0,     0,    43,     0,     0,    15,    18,
     102,     0,   101,   105,    72,     0,    69,     0,    35,   125,
       0,     0,    16,    14,    32,    33,    13,    31,    67,    10,
       0,     0,     0,     0,    11,    39,   123,    44
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -140,  -140,  -140,  -140,  -130,  -139,    14,  -140,    54,  -140,
     -12,  -108,   143,  -140,  -140,  -140,  -140,  -140,  -140,  -140,
     146,   -43,  -140,  -140,  -140,  -140,  -140,  -140,  -140,  -140,
    -140,  -140,    46,    88,  -140,  -140,  -140,   -70,    93,  -140,
      58,   -53,  -140,  -140,  -140,  -140,  -140,  -140,  -140,  -140,
     210,  -140,  -140,  -140
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,     2,   137,   138,   139,    10,   170,   227,   146,   147,
      11,    12,   128,    70,    71,   212,   171,    72,    73,    74,
     100,   101,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,   162,   163,    85,    86,    87,    88,    89,   185,
     151,   152,    90,    91,    92,    93,    33,    34,    35,    36,
      37,    38,     3,    15
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      18,   131,   169,   178,     7,     8,   106,     7,     8,     7,
       8,     7,     8,   113,    98,   181,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   172,    98,   229,   167,   124,
       4,   144,     5,    39,   149,    40,   104,    41,   125,   126,
     127,   158,   182,   210,    98,   205,     1,   183,     6,    42,
     207,    98,   206,   211,   111,   156,    19,    99,     9,   159,
     148,    17,   200,   202,   155,   219,   201,   203,   134,    99,
      16,   218,    46,   230,    47,   105,    48,   145,    49,    50,
     186,   226,   233,   188,    51,   187,    52,    99,    53,    24,
     193,    54,    55,   112,    99,    56,   166,   167,   134,   223,
     154,    21,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,   135,   136,    68,   224,   225,    44,    13,
      14,    69,    25,    26,    27,    28,    29,    30,    31,    32,
      20,   186,    22,    46,    43,    47,   190,    48,    23,    49,
      50,   135,   168,   135,   136,    51,    24,    52,   102,    53,
     186,    94,    54,    55,   195,   192,    56,   195,    95,   196,
      18,    97,   214,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,   129,   130,    68,     7,     8,    25,
      26,    27,    28,    29,    30,    31,    32,   125,   126,   127,
      96,    18,     7,   177,   103,   107,   108,   109,   110,   123,
     132,   133,   140,   142,    98,   150,   157,   153,   160,   161,
     165,   164,   173,   174,   175,   176,   179,   180,   191,   194,
     197,   -10,   198,   208,   204,   215,   199,   220,   232,   234,
     236,   231,   222,   228,   -11,   217,   209,   221,   235,   237,
     141,   216,   189,   184,   213,   143,     0,     0,    45
};

static const yytype_int16 yycheck[] =
{
      12,    71,   132,   142,     4,     5,    49,     4,     5,     4,
       5,     4,     5,    56,     8,   145,    59,    60,    61,    62,
      63,    64,    65,    66,    67,   133,     8,     4,     5,     4,
      57,     5,     0,     3,   104,     5,    18,     7,    13,    14,
      15,   111,    53,     6,     8,    52,     5,    58,    31,    19,
     180,     8,    59,    16,    18,   108,    32,    51,    58,   112,
     103,    58,    52,    58,   107,    58,    56,   175,     4,    51,
      51,   201,     5,    50,     7,    57,     9,    51,    11,    12,
      53,   211,   221,   153,    17,    58,    19,    51,    21,     5,
     160,    24,    25,    57,    51,    28,     4,     5,     4,     5,
      57,     3,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    49,    50,    48,    22,    23,    34,    29,
      30,    54,    38,    39,    40,    41,    42,    43,    44,    45,
       4,    53,    52,     5,     3,     7,    58,     9,    33,    11,
      12,    49,    50,    49,    50,    17,     5,    19,    57,    21,
      53,     3,    24,    25,    53,    58,    28,    53,     3,    58,
     172,    54,    58,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    29,    30,    48,     4,     5,    38,
      39,    40,    41,    42,    43,    44,    45,    13,    14,    15,
      20,   203,     4,     5,    10,    10,    57,    20,    26,    57,
      51,    57,     3,    51,     8,     5,    27,    18,    18,     5,
       4,    55,     4,     4,    57,     4,    52,    51,     5,    51,
      55,    59,     4,    52,    55,     4,    59,    30,     9,     4,
      10,   217,    52,    52,    59,    59,   182,    59,    52,    52,
      97,   195,   154,   150,   186,    99,    -1,    -1,    38
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     5,    61,   112,    57,     0,    31,     4,     5,    58,
      65,    70,    71,    29,    30,   113,    51,    58,    70,    32,
       4,     3,    52,    33,     5,    38,    39,    40,    41,    42,
      43,    44,    45,   106,   107,   108,   109,   110,   111,     3,
       5,     7,    19,     3,    34,   110,     5,     7,     9,    11,
      12,    17,    19,    21,    24,    25,    28,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    48,    54,
      73,    74,    77,    78,    79,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    94,    95,    96,    97,    98,
     102,   103,   104,   105,     3,     3,    20,    54,     8,    51,
      80,    81,    57,    10,    18,    57,    81,    10,    57,    20,
      26,    18,    57,    81,    81,    81,    81,    81,    81,    81,
      81,    81,    81,    57,     4,    13,    14,    15,    72,    29,
      30,    97,    51,    57,     4,    49,    50,    62,    63,    64,
       3,    72,    51,    80,     5,    51,    68,    69,    81,    97,
       5,   100,   101,    18,    57,    81,   101,    27,    97,   101,
      18,     5,    92,    93,    55,     4,     4,     5,    50,    64,
      66,    76,    71,     4,     4,    57,     4,     5,    65,    52,
      51,    64,    53,    58,    98,    99,    53,    58,    97,    93,
      58,     5,    58,    97,    51,    53,    58,    55,     4,    59,
      52,    56,    58,    71,    55,    52,    59,    64,    52,    68,
       6,    16,    75,   100,    58,     4,    92,    59,    64,    58,
      30,    59,    52,     5,    22,    23,    64,    67,    52,     4,
      50,    66,     9,    65,     4,    52,    10,    52
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] =
{
       0,    60,    61,    62,    62,    63,    64,    64,    65,    65,
      66,    66,    66,    67,    67,    68,    68,    69,    69,    70,
      70,    71,    71,    72,    72,    72,    73,    73,    74,    74,
      74,    75,    75,    75,    76,    76,    77,    77,    77,    77,
      78,    79,    79,    80,    80,    81,    81,    82,    82,    83,
      83,    84,    84,    85,    85,    86,    86,    87,    87,    88,
      88,    89,    89,    90,    90,    91,    91,    92,    93,    93,
      94,    94,    94,    95,    96,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    98,    98,
      99,    99,    99,   100,   101,   101,   102,   102,   102,   103,
     103,   103,   104,   105,   105,   106,   106,   106,   106,   106,
     106,   106,   106,   107,   108,   109,   109,   109,   110,   110,
     110,   111,   111,   112,   112,   112,   113,   113
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     8,     1,     2,     2,     1,     1,     1,     1,
       1,     2,     1,     1,     1,     3,     4,     1,     3,     1,
       4,     1,     2,     1,     1,     1,     3,     4,     1,     2,
       2,     2,     2,     2,     1,     3,     1,     4,     4,     7,
       1,     1,     1,     4,     7,     1,     3,     1,     2,     1,
       2,     1,     2,     1,     2,     1,     2,     1,     2,     1,
       2,     1,     2,     1,     2,     2,     3,     4,     1,     3,
       2,     3,     5,     4,     2,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     2,     2,     2,     1,     3,     4,     3,     4,     4,
       3,     4,     4,     1,     4,     1,     1,     1,     1,     1,
       1,     1,     1,     9,     3,     7,     6,     4,     1,     1,
       1,     1,     2,     4,     3,     1,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = _ASN1_YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == _ASN1_YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use _ASN1_YYerror or _ASN1_YYUNDEF. */
#define YYERRCODE _ASN1_YYUNDEF


/* Enable debugging if requested.  */
#if _ASN1_YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
# ifndef YY_LOCATION_PRINT
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yykind < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yykind], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !_ASN1_YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !_ASN1_YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;
      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = _ASN1_YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == _ASN1_YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= _ASN1_YYEOF)
    {
      yychar = _ASN1_YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == _ASN1_YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = _ASN1_YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = _ASN1_YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* definitions: definitions_id DEFINITIONS explicit_implicit TAGS "::=" BEGIN type_constant_list END  */
#line 154 "ASN1.y"
                   {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_DEFINITIONS|(yyvsp[-5].constant));
                    _asn1_set_name((yyval.node),_asn1_get_name((yyvsp[-7].node)));
                    _asn1_set_name((yyvsp[-7].node),"");
                    _asn1_set_right((yyvsp[-7].node),(yyvsp[-1].node));
                    _asn1_set_down((yyval.node),(yyvsp[-7].node));

		    p_tree=(yyval.node);
		    }
#line 1868 "ASN1.c"
    break;

  case 3: /* pos_num: NUM  */
#line 164 "ASN1.y"
                      {snprintf((yyval.str),sizeof((yyval.str)),"%s",(yyvsp[0].str));}
#line 1874 "ASN1.c"
    break;

  case 4: /* pos_num: '+' NUM  */
#line 165 "ASN1.y"
                      {snprintf((yyval.str),sizeof((yyval.str)),"%s",(yyvsp[0].str));}
#line 1880 "ASN1.c"
    break;

  case 5: /* neg_num: '-' NUM  */
#line 168 "ASN1.y"
                      {SAFE_COPY((yyval.str),sizeof((yyval.str)),"-%s",(yyvsp[0].str));}
#line 1886 "ASN1.c"
    break;

  case 6: /* pos_neg_num: pos_num  */
#line 171 "ASN1.y"
                        {snprintf((yyval.str),sizeof((yyval.str)),"%s",(yyvsp[0].str));}
#line 1892 "ASN1.c"
    break;

  case 7: /* pos_neg_num: neg_num  */
#line 172 "ASN1.y"
                        {snprintf((yyval.str),sizeof((yyval.str)),"%s",(yyvsp[0].str));}
#line 1898 "ASN1.c"
    break;

  case 8: /* num_identifier: NUM  */
#line 175 "ASN1.y"
                                 {snprintf((yyval.str),sizeof((yyval.str)),"%s",(yyvsp[0].str));}
#line 1904 "ASN1.c"
    break;

  case 9: /* num_identifier: IDENTIFIER  */
#line 176 "ASN1.y"
                                 {snprintf((yyval.str),sizeof((yyval.str)),"%s",(yyvsp[0].str));}
#line 1910 "ASN1.c"
    break;

  case 10: /* int_identifier: NUM  */
#line 179 "ASN1.y"
                                 {snprintf((yyval.str),sizeof((yyval.str)),"%s",(yyvsp[0].str));}
#line 1916 "ASN1.c"
    break;

  case 11: /* int_identifier: '-' NUM  */
#line 180 "ASN1.y"
                                 {SAFE_COPY((yyval.str),sizeof((yyval.str)),"-%s",(yyvsp[0].str));}
#line 1922 "ASN1.c"
    break;

  case 12: /* int_identifier: IDENTIFIER  */
#line 181 "ASN1.y"
                                 {snprintf((yyval.str),sizeof((yyval.str)),"%s",(yyvsp[0].str));}
#line 1928 "ASN1.c"
    break;

  case 13: /* pos_neg_identifier: pos_neg_num  */
#line 184 "ASN1.y"
                                     {snprintf((yyval.str),sizeof((yyval.str)),"%s",(yyvsp[0].str));}
#line 1934 "ASN1.c"
    break;

  case 14: /* pos_neg_identifier: IDENTIFIER  */
#line 185 "ASN1.y"
                                     {snprintf((yyval.str),sizeof((yyval.str)),"%s",(yyvsp[0].str));}
#line 1940 "ASN1.c"
    break;

  case 15: /* constant: '(' pos_neg_num ')'  */
#line 188 "ASN1.y"
                                      {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT);
                                       _asn1_set_value((yyval.node),(yyvsp[-1].str),strlen((yyvsp[-1].str))+1);}
#line 1947 "ASN1.c"
    break;

  case 16: /* constant: IDENTIFIER '(' pos_neg_num ')'  */
#line 190 "ASN1.y"
                                      {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT);
	                               _asn1_set_name((yyval.node),(yyvsp[-3].str));
                                       _asn1_set_value((yyval.node),(yyvsp[-1].str),strlen((yyvsp[-1].str))+1);}
#line 1955 "ASN1.c"
    break;

  case 17: /* constant_list: constant  */
#line 195 "ASN1.y"
                                           {(yyval.node)=(yyvsp[0].node);}
#line 1961 "ASN1.c"
    break;

  case 18: /* constant_list: constant_list ',' constant  */
#line 196 "ASN1.y"
                                           {(yyval.node)=(yyvsp[-2].node);
                                            _asn1_set_right(_asn1_get_last_right((yyvsp[-2].node)),(yyvsp[0].node));}
#line 1968 "ASN1.c"
    break;

  case 19: /* obj_constant: num_identifier  */
#line 200 "ASN1.y"
                                  {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT);
                                   _asn1_set_value((yyval.node),(yyvsp[0].str),strlen((yyvsp[0].str))+1);}
#line 1975 "ASN1.c"
    break;

  case 20: /* obj_constant: IDENTIFIER '(' NUM ')'  */
#line 202 "ASN1.y"
                                   {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT);
	                            _asn1_set_name((yyval.node),(yyvsp[-3].str));
                                    _asn1_set_value((yyval.node),(yyvsp[-1].str),strlen((yyvsp[-1].str))+1);}
#line 1983 "ASN1.c"
    break;

  case 21: /* obj_constant_list: obj_constant  */
#line 207 "ASN1.y"
                                                   {(yyval.node)=(yyvsp[0].node);}
#line 1989 "ASN1.c"
    break;

  case 22: /* obj_constant_list: obj_constant_list obj_constant  */
#line 208 "ASN1.y"
                                                   {(yyval.node)=(yyvsp[-1].node);
                                                    _asn1_set_right(_asn1_get_last_right((yyvsp[-1].node)),(yyvsp[0].node));}
#line 1996 "ASN1.c"
    break;

  case 23: /* class: UNIVERSAL  */
#line 212 "ASN1.y"
                      {(yyval.constant)=CONST_UNIVERSAL;}
#line 2002 "ASN1.c"
    break;

  case 24: /* class: PRIVATE  */
#line 213 "ASN1.y"
                      {(yyval.constant)=CONST_PRIVATE;}
#line 2008 "ASN1.c"
    break;

  case 25: /* class: APPLICATION  */
#line 214 "ASN1.y"
                      {(yyval.constant)=CONST_APPLICATION;}
#line 2014 "ASN1.c"
    break;

  case 26: /* tag_type: '[' NUM ']'  */
#line 217 "ASN1.y"
                           {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_TAG);
                            _asn1_set_value((yyval.node),(yyvsp[-1].str),strlen((yyvsp[-1].str))+1);}
#line 2021 "ASN1.c"
    break;

  case 27: /* tag_type: '[' class NUM ']'  */
#line 219 "ASN1.y"
                               {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_TAG | (yyvsp[-2].constant));
                                _asn1_set_value((yyval.node),(yyvsp[-1].str),strlen((yyvsp[-1].str))+1);}
#line 2028 "ASN1.c"
    break;

  case 28: /* tag: tag_type  */
#line 223 "ASN1.y"
                          {(yyval.node)=(yyvsp[0].node);}
#line 2034 "ASN1.c"
    break;

  case 29: /* tag: tag_type EXPLICIT  */
#line 224 "ASN1.y"
                          {(yyval.node)=_asn1_mod_type((yyvsp[-1].node),CONST_EXPLICIT);}
#line 2040 "ASN1.c"
    break;

  case 30: /* tag: tag_type IMPLICIT  */
#line 225 "ASN1.y"
                          {(yyval.node)=_asn1_mod_type((yyvsp[-1].node),CONST_IMPLICIT);}
#line 2046 "ASN1.c"
    break;

  case 31: /* default: DEFAULT pos_neg_identifier  */
#line 228 "ASN1.y"
                                      {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_DEFAULT);
                                       _asn1_set_value((yyval.node),(yyvsp[0].str),strlen((yyvsp[0].str))+1);}
#line 2053 "ASN1.c"
    break;

  case 32: /* default: DEFAULT ASN1_TRUE  */
#line 230 "ASN1.y"
                                       {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_DEFAULT|CONST_TRUE);}
#line 2059 "ASN1.c"
    break;

  case 33: /* default: DEFAULT ASN1_FALSE  */
#line 231 "ASN1.y"
                                       {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_DEFAULT|CONST_FALSE);}
#line 2065 "ASN1.c"
    break;

  case 36: /* integer_def: INTEGER  */
#line 240 "ASN1.y"
                                        {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_INTEGER);}
#line 2071 "ASN1.c"
    break;

  case 37: /* integer_def: INTEGER '{' constant_list '}'  */
#line 241 "ASN1.y"
                                        {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_INTEGER|CONST_LIST);
	                                 _asn1_set_down((yyval.node),(yyvsp[-1].node));}
#line 2078 "ASN1.c"
    break;

  case 38: /* integer_def: integer_def '(' pos_neg_list ')'  */
#line 243 "ASN1.y"
                                             {(yyval.node)=(yyvsp[-3].node);}
#line 2084 "ASN1.c"
    break;

  case 39: /* integer_def: integer_def '(' int_identifier '.' '.' int_identifier ')'  */
#line 245 "ASN1.y"
                                        {(yyval.node)=(yyvsp[-6].node);}
#line 2090 "ASN1.c"
    break;

  case 40: /* boolean_def: BOOLEAN  */
#line 248 "ASN1.y"
                       {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_BOOLEAN);}
#line 2096 "ASN1.c"
    break;

  case 41: /* Time: UTCTime  */
#line 251 "ASN1.y"
                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_UTC_TIME);}
#line 2102 "ASN1.c"
    break;

  case 42: /* Time: GeneralizedTime  */
#line 252 "ASN1.y"
                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_GENERALIZED_TIME);}
#line 2108 "ASN1.c"
    break;

  case 43: /* size_def2: SIZE '(' num_identifier ')'  */
#line 255 "ASN1.y"
                                     {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_SIZE|CONST_1_PARAM);
	                              _asn1_set_value((yyval.node),(yyvsp[-1].str),strlen((yyvsp[-1].str))+1);}
#line 2115 "ASN1.c"
    break;

  case 44: /* size_def2: SIZE '(' num_identifier '.' '.' num_identifier ')'  */
#line 258 "ASN1.y"
                                     {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_SIZE|CONST_MIN_MAX);
	                              _asn1_set_value((yyval.node),(yyvsp[-4].str),strlen((yyvsp[-4].str))+1);
                                      _asn1_set_name((yyval.node),(yyvsp[-1].str));}
#line 2123 "ASN1.c"
    break;

  case 45: /* size_def: size_def2  */
#line 263 "ASN1.y"
                               {(yyval.node)=(yyvsp[0].node);}
#line 2129 "ASN1.c"
    break;

  case 46: /* size_def: '(' size_def2 ')'  */
#line 264 "ASN1.y"
                               {(yyval.node)=(yyvsp[-1].node);}
#line 2135 "ASN1.c"
    break;

  case 47: /* generalstring_def: GeneralString  */
#line 267 "ASN1.y"
                                 {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_GENERALSTRING);}
#line 2141 "ASN1.c"
    break;

  case 48: /* generalstring_def: GeneralString size_def  */
#line 268 "ASN1.y"
                                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_GENERALSTRING|CONST_SIZE);
					  _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2148 "ASN1.c"
    break;

  case 49: /* numericstring_def: NumericString  */
#line 272 "ASN1.y"
                                 {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_NUMERIC_STRING|CONST_UNIVERSAL);}
#line 2154 "ASN1.c"
    break;

  case 50: /* numericstring_def: NumericString size_def  */
#line 273 "ASN1.y"
                                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_NUMERIC_STRING|CONST_SIZE);
					  _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2161 "ASN1.c"
    break;

  case 51: /* ia5string_def: IA5String  */
#line 277 "ASN1.y"
                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_IA5_STRING);}
#line 2167 "ASN1.c"
    break;

  case 52: /* ia5string_def: IA5String size_def  */
#line 278 "ASN1.y"
                                     {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_IA5_STRING|CONST_SIZE);
					  _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2174 "ASN1.c"
    break;

  case 53: /* teletexstring_def: TeletexString  */
#line 282 "ASN1.y"
                                 {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_TELETEX_STRING);}
#line 2180 "ASN1.c"
    break;

  case 54: /* teletexstring_def: TeletexString size_def  */
#line 283 "ASN1.y"
                                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_TELETEX_STRING|CONST_SIZE);
					  _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2187 "ASN1.c"
    break;

  case 55: /* printablestring_def: PrintableString  */
#line 287 "ASN1.y"
                                     {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_PRINTABLE_STRING);}
#line 2193 "ASN1.c"
    break;

  case 56: /* printablestring_def: PrintableString size_def  */
#line 288 "ASN1.y"
                                           {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_PRINTABLE_STRING|CONST_SIZE);
					  _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2200 "ASN1.c"
    break;

  case 57: /* universalstring_def: UniversalString  */
#line 292 "ASN1.y"
                                     {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_UNIVERSAL_STRING);}
#line 2206 "ASN1.c"
    break;

  case 58: /* universalstring_def: UniversalString size_def  */
#line 293 "ASN1.y"
                                           {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_UNIVERSAL_STRING|CONST_SIZE);
					  _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2213 "ASN1.c"
    break;

  case 59: /* bmpstring_def: BMPString  */
#line 297 "ASN1.y"
                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_BMP_STRING);}
#line 2219 "ASN1.c"
    break;

  case 60: /* bmpstring_def: BMPString size_def  */
#line 298 "ASN1.y"
                                     {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_BMP_STRING|CONST_SIZE);
					  _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2226 "ASN1.c"
    break;

  case 61: /* utf8string_def: UTF8String  */
#line 302 "ASN1.y"
                           {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_UTF8_STRING);}
#line 2232 "ASN1.c"
    break;

  case 62: /* utf8string_def: UTF8String size_def  */
#line 303 "ASN1.y"
                                      {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_UTF8_STRING|CONST_SIZE);
					  _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2239 "ASN1.c"
    break;

  case 63: /* visiblestring_def: VisibleString  */
#line 307 "ASN1.y"
                                 {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_VISIBLE_STRING);}
#line 2245 "ASN1.c"
    break;

  case 64: /* visiblestring_def: VisibleString size_def  */
#line 308 "ASN1.y"
                                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_VISIBLE_STRING|CONST_SIZE);
					  _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2252 "ASN1.c"
    break;

  case 65: /* octet_string_def: OCTET STRING  */
#line 312 "ASN1.y"
                                          {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_OCTET_STRING);}
#line 2258 "ASN1.c"
    break;

  case 66: /* octet_string_def: OCTET STRING size_def  */
#line 313 "ASN1.y"
                                          {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_OCTET_STRING|CONST_SIZE);
                                           _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2265 "ASN1.c"
    break;

  case 67: /* bit_element: IDENTIFIER '(' NUM ')'  */
#line 317 "ASN1.y"
                                   {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT);
	                           _asn1_set_name((yyval.node),(yyvsp[-3].str));
                                    _asn1_set_value((yyval.node),(yyvsp[-1].str),strlen((yyvsp[-1].str))+1);}
#line 2273 "ASN1.c"
    break;

  case 68: /* bit_element_list: bit_element  */
#line 322 "ASN1.y"
                                  {(yyval.node)=(yyvsp[0].node);}
#line 2279 "ASN1.c"
    break;

  case 69: /* bit_element_list: bit_element_list ',' bit_element  */
#line 323 "ASN1.y"
                                                      {(yyval.node)=(yyvsp[-2].node);
                                                       _asn1_set_right(_asn1_get_last_right((yyvsp[-2].node)),(yyvsp[0].node));}
#line 2286 "ASN1.c"
    break;

  case 70: /* bit_string_def: BIT STRING  */
#line 327 "ASN1.y"
                               {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_BIT_STRING);}
#line 2292 "ASN1.c"
    break;

  case 71: /* bit_string_def: BIT STRING size_def  */
#line 328 "ASN1.y"
                                     {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_BIT_STRING|CONST_SIZE);
                                      _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2299 "ASN1.c"
    break;

  case 72: /* bit_string_def: BIT STRING '{' bit_element_list '}'  */
#line 331 "ASN1.y"
                               {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_BIT_STRING|CONST_LIST);
                                _asn1_set_down((yyval.node),(yyvsp[-1].node));}
#line 2306 "ASN1.c"
    break;

  case 73: /* enumerated_def: ENUMERATED '{' bit_element_list '}'  */
#line 336 "ASN1.y"
                               {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_ENUMERATED|CONST_LIST);
                                _asn1_set_down((yyval.node),(yyvsp[-1].node));}
#line 2313 "ASN1.c"
    break;

  case 74: /* object_def: OBJECT STR_IDENTIFIER  */
#line 341 "ASN1.y"
                                    {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID);}
#line 2319 "ASN1.c"
    break;

  case 75: /* type_assig_right: IDENTIFIER  */
#line 344 "ASN1.y"
                                      {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_IDENTIFIER);
                                       _asn1_set_value((yyval.node),(yyvsp[0].str),strlen((yyvsp[0].str))+1);}
#line 2326 "ASN1.c"
    break;

  case 76: /* type_assig_right: IDENTIFIER size_def  */
#line 346 "ASN1.y"
                                      {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_IDENTIFIER|CONST_SIZE);
                                       _asn1_set_value((yyval.node),(yyvsp[-1].str),strlen((yyvsp[-1].str))+1);
                                       _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2334 "ASN1.c"
    break;

  case 77: /* type_assig_right: integer_def  */
#line 349 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2340 "ASN1.c"
    break;

  case 78: /* type_assig_right: enumerated_def  */
#line 350 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2346 "ASN1.c"
    break;

  case 79: /* type_assig_right: boolean_def  */
#line 351 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2352 "ASN1.c"
    break;

  case 81: /* type_assig_right: octet_string_def  */
#line 353 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2358 "ASN1.c"
    break;

  case 82: /* type_assig_right: bit_string_def  */
#line 354 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2364 "ASN1.c"
    break;

  case 83: /* type_assig_right: generalstring_def  */
#line 355 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2370 "ASN1.c"
    break;

  case 84: /* type_assig_right: numericstring_def  */
#line 356 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2376 "ASN1.c"
    break;

  case 85: /* type_assig_right: ia5string_def  */
#line 357 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2382 "ASN1.c"
    break;

  case 86: /* type_assig_right: teletexstring_def  */
#line 358 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2388 "ASN1.c"
    break;

  case 87: /* type_assig_right: printablestring_def  */
#line 359 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2394 "ASN1.c"
    break;

  case 88: /* type_assig_right: universalstring_def  */
#line 360 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2400 "ASN1.c"
    break;

  case 89: /* type_assig_right: bmpstring_def  */
#line 361 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2406 "ASN1.c"
    break;

  case 90: /* type_assig_right: utf8string_def  */
#line 362 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2412 "ASN1.c"
    break;

  case 91: /* type_assig_right: visiblestring_def  */
#line 363 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2418 "ASN1.c"
    break;

  case 92: /* type_assig_right: sequence_def  */
#line 364 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2424 "ASN1.c"
    break;

  case 93: /* type_assig_right: object_def  */
#line 365 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2430 "ASN1.c"
    break;

  case 94: /* type_assig_right: choise_def  */
#line 366 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2436 "ASN1.c"
    break;

  case 95: /* type_assig_right: any_def  */
#line 367 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2442 "ASN1.c"
    break;

  case 96: /* type_assig_right: set_def  */
#line 368 "ASN1.y"
                                      {(yyval.node)=(yyvsp[0].node);}
#line 2448 "ASN1.c"
    break;

  case 97: /* type_assig_right: TOKEN_NULL  */
#line 369 "ASN1.y"
                                      {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_NULL);}
#line 2454 "ASN1.c"
    break;

  case 98: /* type_assig_right_tag: type_assig_right  */
#line 372 "ASN1.y"
                                              {(yyval.node)=(yyvsp[0].node);}
#line 2460 "ASN1.c"
    break;

  case 99: /* type_assig_right_tag: tag type_assig_right  */
#line 373 "ASN1.y"
                                              {(yyval.node)=_asn1_mod_type((yyvsp[0].node),CONST_TAG);
                                               _asn1_set_right((yyvsp[-1].node),_asn1_get_down((yyval.node)));
                                               _asn1_set_down((yyval.node),(yyvsp[-1].node));}
#line 2468 "ASN1.c"
    break;

  case 100: /* type_assig_right_tag_default: type_assig_right_tag  */
#line 378 "ASN1.y"
                                                      {(yyval.node)=(yyvsp[0].node);}
#line 2474 "ASN1.c"
    break;

  case 101: /* type_assig_right_tag_default: type_assig_right_tag default  */
#line 379 "ASN1.y"
                                                      {(yyval.node)=_asn1_mod_type((yyvsp[-1].node),CONST_DEFAULT);
                                                       _asn1_set_right((yyvsp[0].node),_asn1_get_down((yyval.node)));
						       _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2482 "ASN1.c"
    break;

  case 102: /* type_assig_right_tag_default: type_assig_right_tag OPTIONAL  */
#line 382 "ASN1.y"
                                                      {(yyval.node)=_asn1_mod_type((yyvsp[-1].node),CONST_OPTION);}
#line 2488 "ASN1.c"
    break;

  case 103: /* type_assig: IDENTIFIER type_assig_right_tag_default  */
#line 385 "ASN1.y"
                                                      {(yyval.node)=_asn1_set_name((yyvsp[0].node),(yyvsp[-1].str));}
#line 2494 "ASN1.c"
    break;

  case 104: /* type_assig_list: type_assig  */
#line 388 "ASN1.y"
                                               {(yyval.node)=(yyvsp[0].node);}
#line 2500 "ASN1.c"
    break;

  case 105: /* type_assig_list: type_assig_list ',' type_assig  */
#line 389 "ASN1.y"
                                               {(yyval.node)=(yyvsp[-2].node);
                                                _asn1_set_right(_asn1_get_last_right((yyvsp[-2].node)),(yyvsp[0].node));}
#line 2507 "ASN1.c"
    break;

  case 106: /* sequence_def: SEQUENCE '{' type_assig_list '}'  */
#line 393 "ASN1.y"
                                             {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_SEQUENCE);
                                              _asn1_set_down((yyval.node),(yyvsp[-1].node));}
#line 2514 "ASN1.c"
    break;

  case 107: /* sequence_def: SEQUENCE OF type_assig_right  */
#line 395 "ASN1.y"
                                             {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_SEQUENCE_OF);
                                              _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2521 "ASN1.c"
    break;

  case 108: /* sequence_def: SEQUENCE size_def OF type_assig_right  */
#line 397 "ASN1.y"
                                           {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_SEQUENCE_OF|CONST_SIZE);
                                            _asn1_set_right((yyvsp[-2].node),(yyvsp[0].node));
                                            _asn1_set_down((yyval.node),(yyvsp[-2].node));}
#line 2529 "ASN1.c"
    break;

  case 109: /* set_def: SET '{' type_assig_list '}'  */
#line 402 "ASN1.y"
                                    {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_SET);
                                     _asn1_set_down((yyval.node),(yyvsp[-1].node));}
#line 2536 "ASN1.c"
    break;

  case 110: /* set_def: SET OF type_assig_right  */
#line 404 "ASN1.y"
                                    {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_SET_OF);
                                     _asn1_set_down((yyval.node),(yyvsp[0].node));}
#line 2543 "ASN1.c"
    break;

  case 111: /* set_def: SET size_def OF type_assig_right  */
#line 406 "ASN1.y"
                                      {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_SET_OF|CONST_SIZE);
                                       _asn1_set_right((yyvsp[-2].node),(yyvsp[0].node));
                                       _asn1_set_down((yyval.node),(yyvsp[-2].node));}
#line 2551 "ASN1.c"
    break;

  case 112: /* choise_def: CHOICE '{' type_assig_list '}'  */
#line 411 "ASN1.y"
                                            {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_CHOICE);
                                             _asn1_set_down((yyval.node),(yyvsp[-1].node));}
#line 2558 "ASN1.c"
    break;

  case 113: /* any_def: ANY  */
#line 415 "ASN1.y"
                                       {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_ANY);}
#line 2564 "ASN1.c"
    break;

  case 114: /* any_def: ANY DEFINED BY IDENTIFIER  */
#line 416 "ASN1.y"
                                       {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_ANY|CONST_DEFINED_BY);
                                        _asn1_set_down((yyval.node),_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT));
	                                _asn1_set_name(_asn1_get_down((yyval.node)),(yyvsp[0].str));}
#line 2572 "ASN1.c"
    break;

  case 115: /* known_string: UTF8String  */
#line 421 "ASN1.y"
                         { SAFE_COPY((yyval.str),sizeof((yyval.str)),"%s",last_token); }
#line 2578 "ASN1.c"
    break;

  case 116: /* known_string: VisibleString  */
#line 422 "ASN1.y"
                         { SAFE_COPY((yyval.str),sizeof((yyval.str)),"%s",last_token); }
#line 2584 "ASN1.c"
    break;

  case 117: /* known_string: PrintableString  */
#line 423 "ASN1.y"
                           { SAFE_COPY((yyval.str),sizeof((yyval.str)),"%s",last_token); }
#line 2590 "ASN1.c"
    break;

  case 118: /* known_string: UniversalString  */
#line 424 "ASN1.y"
                           { SAFE_COPY((yyval.str),sizeof((yyval.str)),"%s",last_token); }
#line 2596 "ASN1.c"
    break;

  case 119: /* known_string: IA5String  */
#line 425 "ASN1.y"
                     { SAFE_COPY((yyval.str),sizeof((yyval.str)),"%s",last_token); }
#line 2602 "ASN1.c"
    break;

  case 120: /* known_string: NumericString  */
#line 426 "ASN1.y"
                         { SAFE_COPY((yyval.str),sizeof((yyval.str)),"%s",last_token); }
#line 2608 "ASN1.c"
    break;

  case 121: /* known_string: TeletexString  */
#line 427 "ASN1.y"
                         { SAFE_COPY((yyval.str),sizeof((yyval.str)),"%s",last_token); }
#line 2614 "ASN1.c"
    break;

  case 122: /* known_string: BMPString  */
#line 428 "ASN1.y"
                     { SAFE_COPY((yyval.str),sizeof((yyval.str)),"%s",last_token); }
#line 2620 "ASN1.c"
    break;

  case 123: /* type_invalid: known_string "::=" '[' class NUM ']' IMPLICIT OCTET STRING  */
#line 431 "ASN1.y"
                                                                          {
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
	fprintf(stderr, "%s:%u: Warning: %s is a built-in ASN.1 type.\n", file_name, line_number, (yyvsp[-8].str));
#endif
}
#line 2630 "ASN1.c"
    break;

  case 124: /* type_def: IDENTIFIER "::=" type_assig_right_tag  */
#line 438 "ASN1.y"
                                                  { (yyval.node)=_asn1_set_name((yyvsp[0].node),(yyvsp[-2].str));}
#line 2636 "ASN1.c"
    break;

  case 125: /* constant_def: IDENTIFIER OBJECT STR_IDENTIFIER "::=" '{' obj_constant_list '}'  */
#line 442 "ASN1.y"
                        {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID|CONST_ASSIGN);
                         _asn1_set_name((yyval.node),(yyvsp[-6].str));
                         _asn1_set_down((yyval.node),(yyvsp[-1].node));}
#line 2644 "ASN1.c"
    break;

  case 126: /* constant_def: IDENTIFIER IDENTIFIER "::=" '{' obj_constant_list '}'  */
#line 446 "ASN1.y"
                        {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID|CONST_ASSIGN|CONST_1_PARAM);
                         _asn1_set_name((yyval.node),(yyvsp[-5].str));
                         _asn1_set_value((yyval.node),(yyvsp[-4].str),strlen((yyvsp[-4].str))+1);
                         _asn1_set_down((yyval.node),(yyvsp[-1].node));}
#line 2653 "ASN1.c"
    break;

  case 127: /* constant_def: IDENTIFIER INTEGER "::=" pos_neg_num  */
#line 451 "ASN1.y"
                        {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_INTEGER|CONST_ASSIGN);
                         _asn1_set_name((yyval.node),(yyvsp[-3].str));
                         _asn1_set_value((yyval.node),(yyvsp[0].str),strlen((yyvsp[0].str))+1);}
#line 2661 "ASN1.c"
    break;

  case 128: /* type_constant: type_def  */
#line 456 "ASN1.y"
                              {(yyval.node)=(yyvsp[0].node);}
#line 2667 "ASN1.c"
    break;

  case 129: /* type_constant: type_invalid  */
#line 457 "ASN1.y"
                              {(yyval.node)=NULL;}
#line 2673 "ASN1.c"
    break;

  case 130: /* type_constant: constant_def  */
#line 458 "ASN1.y"
                              {(yyval.node)=(yyvsp[0].node);}
#line 2679 "ASN1.c"
    break;

  case 131: /* type_constant_list: type_constant  */
#line 461 "ASN1.y"
                                        {(yyval.node)=(yyvsp[0].node);}
#line 2685 "ASN1.c"
    break;

  case 132: /* type_constant_list: type_constant_list type_constant  */
#line 462 "ASN1.y"
                                                         {if (!(yyvsp[-1].node))
                                                            {
                                                              (yyval.node) = (yyvsp[0].node);
                                                            }
                                                          else
                                                            {
                                                              (yyval.node)=(yyvsp[-1].node);
                                                              if ((yyvsp[0].node)) _asn1_set_right(_asn1_get_last_right((yyvsp[-1].node)),(yyvsp[0].node));
                                                            }}
#line 2699 "ASN1.c"
    break;

  case 133: /* definitions_id: IDENTIFIER '{' obj_constant_list '}'  */
#line 473 "ASN1.y"
                                                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID);
                                                          _asn1_set_down((yyval.node),(yyvsp[-1].node));
                                                          _asn1_set_name((yyval.node),(yyvsp[-3].str));}
#line 2707 "ASN1.c"
    break;

  case 134: /* definitions_id: IDENTIFIER '{' '}'  */
#line 476 "ASN1.y"
                                                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID);
                                                          _asn1_set_name((yyval.node),(yyvsp[-2].str));}
#line 2714 "ASN1.c"
    break;

  case 135: /* definitions_id: IDENTIFIER  */
#line 478 "ASN1.y"
                                                         {(yyval.node)=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID);
                                                          _asn1_set_name((yyval.node),(yyvsp[0].str));}
#line 2721 "ASN1.c"
    break;

  case 136: /* explicit_implicit: EXPLICIT  */
#line 502 "ASN1.y"
                               {(yyval.constant)=CONST_EXPLICIT;}
#line 2727 "ASN1.c"
    break;

  case 137: /* explicit_implicit: IMPLICIT  */
#line 503 "ASN1.y"
                               {(yyval.constant)=CONST_IMPLICIT;}
#line 2733 "ASN1.c"
    break;


#line 2737 "ASN1.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == _ASN1_YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          goto yyexhaustedlab;
      }
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= _ASN1_YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == _ASN1_YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = _ASN1_YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if 1
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif


/*-------------------------------------------------------.
| yyreturn -- parsing is finished, clean up and return.  |
`-------------------------------------------------------*/
yyreturn:
  if (yychar != _ASN1_YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

#line 507 "ASN1.y"




static const char *key_word[] = {
  "::=","OPTIONAL","INTEGER","SIZE","OCTET","STRING",
  "SEQUENCE","BIT","UNIVERSAL","PRIVATE","OPTIONAL",
  "DEFAULT","CHOICE","OF","OBJECT","IDENTIFIER",
  "BOOLEAN","TRUE","FALSE","APPLICATION","ANY","DEFINED",
  "SET","BY","EXPLICIT","IMPLICIT","DEFINITIONS","TAGS",
  "BEGIN","END","UTCTime","GeneralizedTime",
  "GeneralString","FROM","IMPORTS","NULL","ENUMERATED",
  "NumericString", "IA5String", "TeletexString", "PrintableString",
  "UniversalString", "BMPString", "UTF8String", "VisibleString"};

static const int key_word_token[] = {
  ASSIG, OPTIONAL, INTEGER, SIZE, OCTET, STRING, SEQUENCE, BIT, UNIVERSAL,
      PRIVATE, OPTIONAL, DEFAULT, CHOICE, OF, OBJECT, STR_IDENTIFIER,
      BOOLEAN, ASN1_TRUE, ASN1_FALSE, APPLICATION, ANY, DEFINED, SET, BY,
      EXPLICIT, IMPLICIT, DEFINITIONS, TAGS, BEGIN, END, UTCTime,
      GeneralizedTime, GeneralString, FROM, IMPORTS, TOKEN_NULL,
      ENUMERATED, NumericString, IA5String, TeletexString, PrintableString,
      UniversalString, BMPString, UTF8String, VisibleString
};

/*************************************************************/
/*  Function: _asn1_yylex                                    */
/*  Description: looks for tokens in file_asn1 pointer file. */
/*  Return: int                                              */
/*    Token identifier or ASCII code or 0(zero: End Of File) */
/*************************************************************/
static int
_asn1_yylex (void)
{
  int c, counter = 0, k, lastc;
  char string[ASN1_MAX_NAME_SIZE + 1];  /* will contain the next token */
  size_t i;

  while (1)
    {
      while ((c = fgetc (file_asn1)) == ' ' || c == '\t' || c == '\n')
        if (c == '\n')
          line_number++;

      if (c == EOF)
        {
          snprintf (last_token, sizeof(last_token), "End Of File");
          return 0;
        }

      if (c == '(' || c == ')' || c == '[' || c == ']' ||
          c == '{' || c == '}' || c == ',' || c == '.' ||
          c == '+' || c == '|')
        {
          last_token[0] = c;
          last_token[1] = 0;
          return c;
        }
      if (c == '-')
        {                       /* Maybe the first '-' of a comment */
          if ((c = fgetc (file_asn1)) != '-')
            {
              ungetc (c, file_asn1);
              last_token[0] = '-';
              last_token[1] = 0;
              return '-';
            }
          else
            {                   /* Comments */
              lastc = 0;
              counter = 0;
              /* A comment finishes at the next double hypen or the end of line */
              while ((c = fgetc (file_asn1)) != EOF && c != '\n' &&
                     (lastc != '-' || (lastc == '-' && c != '-')))
                lastc = c;
              if (c == EOF)
                {
                  snprintf (last_token, sizeof(last_token), "End Of File");
                  return 0;
                }
              else
                {
                  if (c == '\n')
                    line_number++;
                  continue;     /* next char, please! (repeat the search) */
                }
            }
        }
      string[counter++] = (char) c;
      /* Till the end of the token */
      while (!
             ((c = fgetc (file_asn1)) == EOF || c == ' ' || c == '\t'
              || c == '\n' || c == '(' || c == ')' || c == '[' || c == ']'
              || c == '{' || c == '}' || c == ',' || c == '.'))
        {
          if (counter >= ASN1_MAX_NAME_SIZE)
            {
              result_parse = ASN1_NAME_TOO_LONG;
              return 0;
            }
          string[counter++] = (char) c;
        }
      ungetc (c, file_asn1);
      string[counter] = 0;
      snprintf (last_token, sizeof(last_token), "%s", string);

      /* Is STRING a number? */
      for (k = 0; k < counter; k++)
        if (!c_isdigit ((int)string[k]))
          break;
      if (k >= counter)
        {
          snprintf (yylval.str, sizeof(yylval.str), "%s", string);
          return NUM;           /* return the number */
        }

      /* Is STRING a keyword? */
      for (i = 0; i < (sizeof (key_word) / sizeof (char *)); i++)
        if (!strcmp (string, key_word[i]))
          return key_word_token[i];

      /* STRING is an IDENTIFIER */
      snprintf (yylval.str, sizeof(yylval.str), "%s", string);
      return IDENTIFIER;
    }
}

/*************************************************************/
/*  Function: _asn1_create_errorDescription                  */
/*  Description: creates a string with the description of the*/
/*    error.                                                 */
/*  Parameters:                                              */
/*    error : error to describe.                             */
/*    error_desc: string that will contain the         */
/*                      description.                         */
/*************************************************************/
static void
_asn1_create_errorDescription (int error, char *error_desc)
{
  if (error_desc == NULL)
    return;


  switch (error)
    {
    case ASN1_FILE_NOT_FOUND:
      snprintf(error_desc, ASN1_MAX_ERROR_DESCRIPTION_SIZE, "%s file was not found", file_name);
      break;
    case ASN1_SYNTAX_ERROR:
      snprintf(error_desc, ASN1_MAX_ERROR_DESCRIPTION_SIZE, "%s", last_error);
      break;
    case ASN1_NAME_TOO_LONG:
      snprintf (error_desc, ASN1_MAX_ERROR_DESCRIPTION_SIZE,
                "%s:%u: name too long (more than %u characters)", file_name,
                line_number, (unsigned)ASN1_MAX_NAME_SIZE);
      break;
    case ASN1_IDENTIFIER_NOT_FOUND:
      snprintf (error_desc, ASN1_MAX_ERROR_DESCRIPTION_SIZE,
                "%s:: identifier '%s' not found", file_name,
                _asn1_identifierMissing);
      break;
    default:
      error_desc[0] = 0;
      break;
    }

}

/**
 * asn1_parser2tree:
 * @file: specify the path and the name of file that contains
 *   ASN.1 declarations.
 * @definitions: return the pointer to the structure created from
 *   "file" ASN.1 declarations.
 * @error_desc: return the error description or an empty
 * string if success.
 *
 * Function used to start the parse algorithm.  Creates the structures
 * needed to manage the definitions included in @file file.
 *
 * Returns: %ASN1_SUCCESS if the file has a correct syntax and every
 *   identifier is known, %ASN1_ELEMENT_NOT_EMPTY if @definitions not
 *   %NULL, %ASN1_FILE_NOT_FOUND if an error occurred while
 *   opening @file, %ASN1_SYNTAX_ERROR if the syntax is not
 *   correct, %ASN1_IDENTIFIER_NOT_FOUND if in the file there is an
 *   identifier that is not defined, %ASN1_NAME_TOO_LONG if in the
 *   file there is an identifier with more than %ASN1_MAX_NAME_SIZE
 *   characters.
 **/
int
asn1_parser2tree (const char *file, asn1_node * definitions,
                  char *error_desc)
{
  if (*definitions != NULL)
    {
      result_parse = ASN1_ELEMENT_NOT_EMPTY;
      goto error;
    }

  file_name = file;

  /* open the file to parse */
  file_asn1 = fopen (file, "r");

  if (file_asn1 == NULL)
    {
      result_parse = ASN1_FILE_NOT_FOUND;
      goto error;
    }

  result_parse = ASN1_SUCCESS;

  line_number = 1;
  yyparse ();

  fclose (file_asn1);

  if (result_parse != ASN1_SUCCESS)
    goto error;

  /* set IMPLICIT or EXPLICIT property */
  _asn1_set_default_tag (p_tree);
  /* set CONST_SET and CONST_NOT_USED */
  _asn1_type_set_config (p_tree);
  /* check the identifier definitions */
  result_parse = _asn1_check_identifier (p_tree);
  if (result_parse != ASN1_SUCCESS)
    goto error;

  /* Convert into DER coding the value assign to INTEGER constants */
  _asn1_change_integer_value (p_tree);
  /* Expand the IDs of OBJECT IDENTIFIER constants */
  result_parse = _asn1_expand_object_id (&e_list, p_tree);
  if (result_parse != ASN1_SUCCESS)
    goto error;

  /* success */
  *definitions = p_tree;
  _asn1_delete_list (e_list);
  e_list = NULL;
  p_tree = NULL;
  *error_desc = 0;
  return result_parse;

error:
  _asn1_delete_list_and_nodes (e_list);
  e_list = NULL;
  p_tree = NULL;

  _asn1_create_errorDescription (result_parse, error_desc);

  return result_parse;
}

/**
 * asn1_parser2array:
 * @inputFileName: specify the path and the name of file that
 *   contains ASN.1 declarations.
 * @outputFileName: specify the path and the name of file that will
 *   contain the C vector definition.
 * @vectorName: specify the name of the C vector.
 * @error_desc: return the error description or an empty
 *   string if success.
 *
 * Function that generates a C structure from an ASN1 file.  Creates a
 * file containing a C vector to use to manage the definitions
 * included in @inputFileName file. If @inputFileName is
 * "/aa/bb/xx.yy" and @outputFileName is %NULL, the file created is
 * "/aa/bb/xx_asn1_tab.c".  If @vectorName is %NULL the vector name
 * will be "xx_asn1_tab".
 *
 * Returns: %ASN1_SUCCESS if the file has a correct syntax and every
 *   identifier is known, %ASN1_FILE_NOT_FOUND if an error occurred
 *   while opening @inputFileName, %ASN1_SYNTAX_ERROR if the syntax is
 *   not correct, %ASN1_IDENTIFIER_NOT_FOUND if in the file there is
 *   an identifier that is not defined, %ASN1_NAME_TOO_LONG if in the
 *   file there is an identifier with more than %ASN1_MAX_NAME_SIZE
 *   characters.
 **/
int
asn1_parser2array (const char *inputFileName, const char *outputFileName,
                   const char *vectorName, char *error_desc)
{
  char *file_out_name = NULL;
  char *vector_name = NULL;
  const char *char_p, *slash_p, *dot_p;

  p_tree = NULL;

  file_name = inputFileName;

  /* open the file to parse */
  file_asn1 = fopen (inputFileName, "r");

  if (file_asn1 == NULL)
    {
      result_parse = ASN1_FILE_NOT_FOUND;
      goto error2;
    }

  result_parse = ASN1_SUCCESS;

  line_number = 1;
  yyparse ();

  fclose (file_asn1);
  if (result_parse != ASN1_SUCCESS)
    goto error1;

  /* set IMPLICIT or EXPLICIT property */
  _asn1_set_default_tag (p_tree);
  /* set CONST_SET and CONST_NOT_USED */
  _asn1_type_set_config (p_tree);
  /* check the identifier definitions */
  result_parse = _asn1_check_identifier (p_tree);
  if (result_parse != ASN1_SUCCESS)
    goto error2;

  /* all identifier defined */
  /* searching the last '/' and '.' in inputFileName */
  char_p = inputFileName;
  slash_p = inputFileName;
  while ((char_p = strchr (char_p, '/')))
    {
      char_p++;
      slash_p = char_p;
    }

  char_p = slash_p;
  dot_p = inputFileName + strlen (inputFileName);

  while ((char_p = strchr (char_p, '.')))
    {
      dot_p = char_p;
      char_p++;
    }

  if (outputFileName == NULL)
    {
      /* file_out_name = inputFileName + _asn1_tab.c */
      file_out_name = malloc (dot_p - inputFileName + 1 +
                              sizeof ("_asn1_tab.c")-1);
      memcpy (file_out_name, inputFileName,
              dot_p - inputFileName);
      file_out_name[dot_p - inputFileName] = 0;
      strcat (file_out_name, "_asn1_tab.c");
    }
  else
    {
      /* file_out_name = inputFileName */
      file_out_name = strdup(outputFileName);
    }

  if (vectorName == NULL)
    {
      unsigned len, i;
      /* vector_name = file name + _asn1_tab */
      vector_name = malloc (dot_p - slash_p + 1 +
                            sizeof("_asn1_tab") - 1);
      memcpy (vector_name, slash_p, dot_p - slash_p);
      vector_name[dot_p - slash_p] = 0;
      strcat (vector_name, "_asn1_tab");

      len = strlen(vector_name);
      for (i=0;i<len;i++)
        {
          if (vector_name[i] == '-')
          vector_name[i] = '_';
        }
    }
  else
    {
      /* vector_name = vectorName */
      vector_name = strdup(vectorName);
    }

  /* Save structure in a file */
  _asn1_create_static_structure (p_tree,
                                 file_out_name, vector_name);

  free (file_out_name);
  free (vector_name);

 error1:
  _asn1_delete_list_and_nodes (e_list);
  e_list = NULL;

 error2:
  _asn1_create_errorDescription (result_parse, error_desc);

  return result_parse;
}

/*************************************************************/
/*  Function: _asn1_yyerror                                  */
/*  Description: function called when there are syntax errors*/
/*  Parameters:                                              */
/*    char *s : error description                            */
/*  Return: int                                              */
/*                                                           */
/*************************************************************/
static void
_asn1_yyerror (const char *s)
{
  /* Sends the error description to the std_out */
  last_error_token[0] = 0;

  if (result_parse != ASN1_NAME_TOO_LONG)
    {
      snprintf (last_error, sizeof(last_error),
                "%s:%u: Error: %s near '%s'", file_name,
                line_number, s, last_token);
      result_parse = ASN1_SYNTAX_ERROR;
    }

  return;
}
