/* A Bison parser, made by GNU Bison 3.8.2.  */

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
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */

#include <stdio.h>
#include <stdlib.h>

int yylex(void);
static void yyerror(const char *s) { fprintf(stderr, "parse error: %s\n", s); }


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
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
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
    IDENT = 258,                   /* IDENT  */
    INT_LIT = 259,                 /* INT_LIT  */
    FLOAT_LIT = 260,               /* FLOAT_LIT  */
    STRING_LIT = 261,              /* STRING_LIT  */
    CHAR_LIT = 262,                /* CHAR_LIT  */
    TRUE = 263,                    /* TRUE  */
    FALSE = 264,                   /* FALSE  */
    NULL_LIT = 265,                /* NULL_LIT  */
    MODULE = 266,                  /* MODULE  */
    IMPORT = 267,                  /* IMPORT  */
    EXPORT = 268,                  /* EXPORT  */
    FROM = 269,                    /* FROM  */
    AS = 270,                      /* AS  */
    PUB = 271,                     /* PUB  */
    PRIV = 272,                    /* PRIV  */
    LET = 273,                     /* LET  */
    VAR = 274,                     /* VAR  */
    CONST = 275,                   /* CONST  */
    STATIC = 276,                  /* STATIC  */
    MUT = 277,                     /* MUT  */
    FN = 278,                      /* FN  */
    LAMBDA = 279,                  /* LAMBDA  */
    TYPEDEF = 280,                 /* TYPEDEF  */
    TYPE = 281,                    /* TYPE  */
    STRUCT = 282,                  /* STRUCT  */
    ENUM = 283,                    /* ENUM  */
    UNION = 284,                   /* UNION  */
    INTERFACE = 285,               /* INTERFACE  */
    TRAIT = 286,                   /* TRAIT  */
    IMPL = 287,                    /* IMPL  */
    WHERE = 288,                   /* WHERE  */
    EXTENDS = 289,                 /* EXTENDS  */
    IMPLEMENTS = 290,              /* IMPLEMENTS  */
    IF = 291,                      /* IF  */
    ELSE = 292,                    /* ELSE  */
    FOR = 293,                     /* FOR  */
    WHILE = 294,                   /* WHILE  */
    DO = 295,                      /* DO  */
    SWITCH = 296,                  /* SWITCH  */
    CASE = 297,                    /* CASE  */
    DEFAULT = 298,                 /* DEFAULT  */
    MATCH = 299,                   /* MATCH  */
    BREAK = 300,                   /* BREAK  */
    CONTINUE = 301,                /* CONTINUE  */
    RETURN = 302,                  /* RETURN  */
    YIELD = 303,                   /* YIELD  */
    TRY = 304,                     /* TRY  */
    CATCH = 305,                   /* CATCH  */
    FINALLY = 306,                 /* FINALLY  */
    THROW = 307,                   /* THROW  */
    DEFER = 308,                   /* DEFER  */
    NEW = 309,                     /* NEW  */
    DELETE = 310,                  /* DELETE  */
    SIZEOF = 311,                  /* SIZEOF  */
    TYPEOF = 312,                  /* TYPEOF  */
    IN = 313,                      /* IN  */
    IS = 314,                      /* IS  */
    AS_OP = 315,                   /* AS_OP  */
    AWAIT = 316,                   /* AWAIT  */
    ASYNC = 317,                   /* ASYNC  */
    LPAREN = 318,                  /* LPAREN  */
    RPAREN = 319,                  /* RPAREN  */
    LBRACE = 320,                  /* LBRACE  */
    RBRACE = 321,                  /* RBRACE  */
    LBRACK = 322,                  /* LBRACK  */
    RBRACK = 323,                  /* RBRACK  */
    COMMA = 324,                   /* COMMA  */
    SEMI = 325,                    /* SEMI  */
    DOT = 326,                     /* DOT  */
    COLON = 327,                   /* COLON  */
    DCOLON = 328,                  /* DCOLON  */
    QMARK = 329,                   /* QMARK  */
    ARROW = 330,                   /* ARROW  */
    FATARROW = 331,                /* FATARROW  */
    ELLIPSIS = 332,                /* ELLIPSIS  */
    ASSIGN = 333,                  /* ASSIGN  */
    PLUS = 334,                    /* PLUS  */
    MINUS = 335,                   /* MINUS  */
    STAR = 336,                    /* STAR  */
    SLASH = 337,                   /* SLASH  */
    PERCENT = 338,                 /* PERCENT  */
    INC = 339,                     /* INC  */
    DEC = 340,                     /* DEC  */
    EQ = 341,                      /* EQ  */
    NE = 342,                      /* NE  */
    LT = 343,                      /* LT  */
    LE = 344,                      /* LE  */
    GT = 345,                      /* GT  */
    GE = 346,                      /* GE  */
    AND = 347,                     /* AND  */
    OR = 348,                      /* OR  */
    NOT = 349,                     /* NOT  */
    BITAND = 350,                  /* BITAND  */
    BITOR = 351,                   /* BITOR  */
    BITXOR = 352,                  /* BITXOR  */
    BITNOT = 353,                  /* BITNOT  */
    SHL = 354,                     /* SHL  */
    SHR = 355,                     /* SHR  */
    PLUS_ASSIGN = 356,             /* PLUS_ASSIGN  */
    MINUS_ASSIGN = 357,            /* MINUS_ASSIGN  */
    STAR_ASSIGN = 358,             /* STAR_ASSIGN  */
    SLASH_ASSIGN = 359,            /* SLASH_ASSIGN  */
    PERCENT_ASSIGN = 360,          /* PERCENT_ASSIGN  */
    AND_ASSIGN = 361,              /* AND_ASSIGN  */
    OR_ASSIGN = 362,               /* OR_ASSIGN  */
    XOR_ASSIGN = 363,              /* XOR_ASSIGN  */
    SHL_ASSIGN = 364,              /* SHL_ASSIGN  */
    SHR_ASSIGN = 365,              /* SHR_ASSIGN  */
    COALESCE = 366,                /* COALESCE  */
    PIPE = 367,                    /* PIPE  */
    RANGE = 368,                   /* RANGE  */
    RANGE_INCL = 369,              /* RANGE_INCL  */
    UMINUS = 370                   /* UMINUS  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);



/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_IDENT = 3,                      /* IDENT  */
  YYSYMBOL_INT_LIT = 4,                    /* INT_LIT  */
  YYSYMBOL_FLOAT_LIT = 5,                  /* FLOAT_LIT  */
  YYSYMBOL_STRING_LIT = 6,                 /* STRING_LIT  */
  YYSYMBOL_CHAR_LIT = 7,                   /* CHAR_LIT  */
  YYSYMBOL_TRUE = 8,                       /* TRUE  */
  YYSYMBOL_FALSE = 9,                      /* FALSE  */
  YYSYMBOL_NULL_LIT = 10,                  /* NULL_LIT  */
  YYSYMBOL_MODULE = 11,                    /* MODULE  */
  YYSYMBOL_IMPORT = 12,                    /* IMPORT  */
  YYSYMBOL_EXPORT = 13,                    /* EXPORT  */
  YYSYMBOL_FROM = 14,                      /* FROM  */
  YYSYMBOL_AS = 15,                        /* AS  */
  YYSYMBOL_PUB = 16,                       /* PUB  */
  YYSYMBOL_PRIV = 17,                      /* PRIV  */
  YYSYMBOL_LET = 18,                       /* LET  */
  YYSYMBOL_VAR = 19,                       /* VAR  */
  YYSYMBOL_CONST = 20,                     /* CONST  */
  YYSYMBOL_STATIC = 21,                    /* STATIC  */
  YYSYMBOL_MUT = 22,                       /* MUT  */
  YYSYMBOL_FN = 23,                        /* FN  */
  YYSYMBOL_LAMBDA = 24,                    /* LAMBDA  */
  YYSYMBOL_TYPEDEF = 25,                   /* TYPEDEF  */
  YYSYMBOL_TYPE = 26,                      /* TYPE  */
  YYSYMBOL_STRUCT = 27,                    /* STRUCT  */
  YYSYMBOL_ENUM = 28,                      /* ENUM  */
  YYSYMBOL_UNION = 29,                     /* UNION  */
  YYSYMBOL_INTERFACE = 30,                 /* INTERFACE  */
  YYSYMBOL_TRAIT = 31,                     /* TRAIT  */
  YYSYMBOL_IMPL = 32,                      /* IMPL  */
  YYSYMBOL_WHERE = 33,                     /* WHERE  */
  YYSYMBOL_EXTENDS = 34,                   /* EXTENDS  */
  YYSYMBOL_IMPLEMENTS = 35,                /* IMPLEMENTS  */
  YYSYMBOL_IF = 36,                        /* IF  */
  YYSYMBOL_ELSE = 37,                      /* ELSE  */
  YYSYMBOL_FOR = 38,                       /* FOR  */
  YYSYMBOL_WHILE = 39,                     /* WHILE  */
  YYSYMBOL_DO = 40,                        /* DO  */
  YYSYMBOL_SWITCH = 41,                    /* SWITCH  */
  YYSYMBOL_CASE = 42,                      /* CASE  */
  YYSYMBOL_DEFAULT = 43,                   /* DEFAULT  */
  YYSYMBOL_MATCH = 44,                     /* MATCH  */
  YYSYMBOL_BREAK = 45,                     /* BREAK  */
  YYSYMBOL_CONTINUE = 46,                  /* CONTINUE  */
  YYSYMBOL_RETURN = 47,                    /* RETURN  */
  YYSYMBOL_YIELD = 48,                     /* YIELD  */
  YYSYMBOL_TRY = 49,                       /* TRY  */
  YYSYMBOL_CATCH = 50,                     /* CATCH  */
  YYSYMBOL_FINALLY = 51,                   /* FINALLY  */
  YYSYMBOL_THROW = 52,                     /* THROW  */
  YYSYMBOL_DEFER = 53,                     /* DEFER  */
  YYSYMBOL_NEW = 54,                       /* NEW  */
  YYSYMBOL_DELETE = 55,                    /* DELETE  */
  YYSYMBOL_SIZEOF = 56,                    /* SIZEOF  */
  YYSYMBOL_TYPEOF = 57,                    /* TYPEOF  */
  YYSYMBOL_IN = 58,                        /* IN  */
  YYSYMBOL_IS = 59,                        /* IS  */
  YYSYMBOL_AS_OP = 60,                     /* AS_OP  */
  YYSYMBOL_AWAIT = 61,                     /* AWAIT  */
  YYSYMBOL_ASYNC = 62,                     /* ASYNC  */
  YYSYMBOL_LPAREN = 63,                    /* LPAREN  */
  YYSYMBOL_RPAREN = 64,                    /* RPAREN  */
  YYSYMBOL_LBRACE = 65,                    /* LBRACE  */
  YYSYMBOL_RBRACE = 66,                    /* RBRACE  */
  YYSYMBOL_LBRACK = 67,                    /* LBRACK  */
  YYSYMBOL_RBRACK = 68,                    /* RBRACK  */
  YYSYMBOL_COMMA = 69,                     /* COMMA  */
  YYSYMBOL_SEMI = 70,                      /* SEMI  */
  YYSYMBOL_DOT = 71,                       /* DOT  */
  YYSYMBOL_COLON = 72,                     /* COLON  */
  YYSYMBOL_DCOLON = 73,                    /* DCOLON  */
  YYSYMBOL_QMARK = 74,                     /* QMARK  */
  YYSYMBOL_ARROW = 75,                     /* ARROW  */
  YYSYMBOL_FATARROW = 76,                  /* FATARROW  */
  YYSYMBOL_ELLIPSIS = 77,                  /* ELLIPSIS  */
  YYSYMBOL_ASSIGN = 78,                    /* ASSIGN  */
  YYSYMBOL_PLUS = 79,                      /* PLUS  */
  YYSYMBOL_MINUS = 80,                     /* MINUS  */
  YYSYMBOL_STAR = 81,                      /* STAR  */
  YYSYMBOL_SLASH = 82,                     /* SLASH  */
  YYSYMBOL_PERCENT = 83,                   /* PERCENT  */
  YYSYMBOL_INC = 84,                       /* INC  */
  YYSYMBOL_DEC = 85,                       /* DEC  */
  YYSYMBOL_EQ = 86,                        /* EQ  */
  YYSYMBOL_NE = 87,                        /* NE  */
  YYSYMBOL_LT = 88,                        /* LT  */
  YYSYMBOL_LE = 89,                        /* LE  */
  YYSYMBOL_GT = 90,                        /* GT  */
  YYSYMBOL_GE = 91,                        /* GE  */
  YYSYMBOL_AND = 92,                       /* AND  */
  YYSYMBOL_OR = 93,                        /* OR  */
  YYSYMBOL_NOT = 94,                       /* NOT  */
  YYSYMBOL_BITAND = 95,                    /* BITAND  */
  YYSYMBOL_BITOR = 96,                     /* BITOR  */
  YYSYMBOL_BITXOR = 97,                    /* BITXOR  */
  YYSYMBOL_BITNOT = 98,                    /* BITNOT  */
  YYSYMBOL_SHL = 99,                       /* SHL  */
  YYSYMBOL_SHR = 100,                      /* SHR  */
  YYSYMBOL_PLUS_ASSIGN = 101,              /* PLUS_ASSIGN  */
  YYSYMBOL_MINUS_ASSIGN = 102,             /* MINUS_ASSIGN  */
  YYSYMBOL_STAR_ASSIGN = 103,              /* STAR_ASSIGN  */
  YYSYMBOL_SLASH_ASSIGN = 104,             /* SLASH_ASSIGN  */
  YYSYMBOL_PERCENT_ASSIGN = 105,           /* PERCENT_ASSIGN  */
  YYSYMBOL_AND_ASSIGN = 106,               /* AND_ASSIGN  */
  YYSYMBOL_OR_ASSIGN = 107,                /* OR_ASSIGN  */
  YYSYMBOL_XOR_ASSIGN = 108,               /* XOR_ASSIGN  */
  YYSYMBOL_SHL_ASSIGN = 109,               /* SHL_ASSIGN  */
  YYSYMBOL_SHR_ASSIGN = 110,               /* SHR_ASSIGN  */
  YYSYMBOL_COALESCE = 111,                 /* COALESCE  */
  YYSYMBOL_PIPE = 112,                     /* PIPE  */
  YYSYMBOL_RANGE = 113,                    /* RANGE  */
  YYSYMBOL_RANGE_INCL = 114,               /* RANGE_INCL  */
  YYSYMBOL_UMINUS = 115,                   /* UMINUS  */
  YYSYMBOL_YYACCEPT = 116,                 /* $accept  */
  YYSYMBOL_compilation_unit = 117,         /* compilation_unit  */
  YYSYMBOL_opt_shebang = 118,              /* opt_shebang  */
  YYSYMBOL_opt_module_decl = 119,          /* opt_module_decl  */
  YYSYMBOL_opt_imports = 120,              /* opt_imports  */
  YYSYMBOL_import_stmt = 121,              /* import_stmt  */
  YYSYMBOL_import_spec = 122,              /* import_spec  */
  YYSYMBOL_import_list = 123,              /* import_list  */
  YYSYMBOL_import_item = 124,              /* import_item  */
  YYSYMBOL_opt_top_items = 125,            /* opt_top_items  */
  YYSYMBOL_top_item = 126,                 /* top_item  */
  YYSYMBOL_opt_visibility = 127,           /* opt_visibility  */
  YYSYMBOL_item = 128,                     /* item  */
  YYSYMBOL_attr_block = 129,               /* attr_block  */
  YYSYMBOL_attr = 130,                     /* attr  */
  YYSYMBOL_AT_IDENT = 131,                 /* AT_IDENT  */
  YYSYMBOL_opt_attr_args = 132,            /* opt_attr_args  */
  YYSYMBOL_attr_args = 133,                /* attr_args  */
  YYSYMBOL_attr_arg = 134,                 /* attr_arg  */
  YYSYMBOL_type_alias = 135,               /* type_alias  */
  YYSYMBOL_type_decl = 136,                /* type_decl  */
  YYSYMBOL_trait_decl = 137,               /* trait_decl  */
  YYSYMBOL_impl_block = 138,               /* impl_block  */
  YYSYMBOL_opt_type_params = 139,          /* opt_type_params  */
  YYSYMBOL_type_param_list = 140,          /* type_param_list  */
  YYSYMBOL_type_param = 141,               /* type_param  */
  YYSYMBOL_type_bound_list = 142,          /* type_bound_list  */
  YYSYMBOL_type_bound = 143,               /* type_bound  */
  YYSYMBOL_type_arg_list = 144,            /* type_arg_list  */
  YYSYMBOL_opt_where_clause = 145,         /* opt_where_clause  */
  YYSYMBOL_where_list = 146,               /* where_list  */
  YYSYMBOL_where_item = 147,               /* where_item  */
  YYSYMBOL_struct_fields = 148,            /* struct_fields  */
  YYSYMBOL_struct_field = 149,             /* struct_field  */
  YYSYMBOL_union_fields = 150,             /* union_fields  */
  YYSYMBOL_union_field = 151,              /* union_field  */
  YYSYMBOL_enum_variants = 152,            /* enum_variants  */
  YYSYMBOL_enum_variant = 153,             /* enum_variant  */
  YYSYMBOL_opt_variant_payload = 154,      /* opt_variant_payload  */
  YYSYMBOL_opt_variant_discriminant = 155, /* opt_variant_discriminant  */
  YYSYMBOL_iface_members = 156,            /* iface_members  */
  YYSYMBOL_iface_member = 157,             /* iface_member  */
  YYSYMBOL_trait_members = 158,            /* trait_members  */
  YYSYMBOL_trait_member = 159,             /* trait_member  */
  YYSYMBOL_assoc_type = 160,               /* assoc_type  */
  YYSYMBOL_const_sig = 161,                /* const_sig  */
  YYSYMBOL_impl_members = 162,             /* impl_members  */
  YYSYMBOL_impl_member = 163,              /* impl_member  */
  YYSYMBOL_func_decl = 164,                /* func_decl  */
  YYSYMBOL_opt_async = 165,                /* opt_async  */
  YYSYMBOL_func_sig = 166,                 /* func_sig  */
  YYSYMBOL_opt_ret_type = 167,             /* opt_ret_type  */
  YYSYMBOL_opt_params = 168,               /* opt_params  */
  YYSYMBOL_param_list = 169,               /* param_list  */
  YYSYMBOL_param = 170,                    /* param  */
  YYSYMBOL_param_kind = 171,               /* param_kind  */
  YYSYMBOL_opt_param_default = 172,        /* opt_param_default  */
  YYSYMBOL_global_decl = 173,              /* global_decl  */
  YYSYMBOL_storage = 174,                  /* storage  */
  YYSYMBOL_decl_list = 175,                /* decl_list  */
  YYSYMBOL_decl_item = 176,                /* decl_item  */
  YYSYMBOL_opt_type_annot = 177,           /* opt_type_annot  */
  YYSYMBOL_opt_init = 178,                 /* opt_init  */
  YYSYMBOL_block = 179,                    /* block  */
  YYSYMBOL_stmt_list = 180,                /* stmt_list  */
  YYSYMBOL_stmt = 181,                     /* stmt  */
  YYSYMBOL_labeled_stmt = 182,             /* labeled_stmt  */
  YYSYMBOL_decl_stmt = 183,                /* decl_stmt  */
  YYSYMBOL_expr_stmt = 184,                /* expr_stmt  */
  YYSYMBOL_if_stmt = 185,                  /* if_stmt  */
  YYSYMBOL_while_stmt = 186,               /* while_stmt  */
  YYSYMBOL_do_while_stmt = 187,            /* do_while_stmt  */
  YYSYMBOL_for_stmt = 188,                 /* for_stmt  */
  YYSYMBOL_opt_for_init = 189,             /* opt_for_init  */
  YYSYMBOL_opt_for_cond = 190,             /* opt_for_cond  */
  YYSYMBOL_opt_for_step = 191,             /* opt_for_step  */
  YYSYMBOL_expr_list = 192,                /* expr_list  */
  YYSYMBOL_switch_stmt = 193,              /* switch_stmt  */
  YYSYMBOL_case_blocks = 194,              /* case_blocks  */
  YYSYMBOL_case_block = 195,               /* case_block  */
  YYSYMBOL_case_value_list = 196,          /* case_value_list  */
  YYSYMBOL_opt_default = 197,              /* opt_default  */
  YYSYMBOL_match_stmt = 198,               /* match_stmt  */
  YYSYMBOL_match_arms = 199,               /* match_arms  */
  YYSYMBOL_match_arm = 200,                /* match_arm  */
  YYSYMBOL_opt_match_guard = 201,          /* opt_match_guard  */
  YYSYMBOL_match_arm_body = 202,           /* match_arm_body  */
  YYSYMBOL_jump_stmt = 203,                /* jump_stmt  */
  YYSYMBOL_opt_label = 204,                /* opt_label  */
  YYSYMBOL_opt_expr = 205,                 /* opt_expr  */
  YYSYMBOL_try_stmt = 206,                 /* try_stmt  */
  YYSYMBOL_catch_blocks = 207,             /* catch_blocks  */
  YYSYMBOL_opt_finally = 208,              /* opt_finally  */
  YYSYMBOL_throw_stmt = 209,               /* throw_stmt  */
  YYSYMBOL_defer_stmt = 210,               /* defer_stmt  */
  YYSYMBOL_pattern = 211,                  /* pattern  */
  YYSYMBOL_UNDERSCORE = 212,               /* UNDERSCORE  */
  YYSYMBOL_OR_PATTERN = 213,               /* OR_PATTERN  */
  YYSYMBOL_tuple_pattern = 214,            /* tuple_pattern  */
  YYSYMBOL_opt_pattern_list = 215,         /* opt_pattern_list  */
  YYSYMBOL_pattern_list = 216,             /* pattern_list  */
  YYSYMBOL_array_pattern = 217,            /* array_pattern  */
  YYSYMBOL_struct_pattern = 218,           /* struct_pattern  */
  YYSYMBOL_struct_pat_fields = 219,        /* struct_pat_fields  */
  YYSYMBOL_struct_pat_field = 220,         /* struct_pat_field  */
  YYSYMBOL_enum_pattern = 221,             /* enum_pattern  */
  YYSYMBOL_expr = 222,                     /* expr  */
  YYSYMBOL_assignment = 223,               /* assignment  */
  YYSYMBOL_conditional = 224,              /* conditional  */
  YYSYMBOL_pipe = 225,                     /* pipe  */
  YYSYMBOL_coalesce = 226,                 /* coalesce  */
  YYSYMBOL_logical_or = 227,               /* logical_or  */
  YYSYMBOL_logical_and = 228,              /* logical_and  */
  YYSYMBOL_bit_or = 229,                   /* bit_or  */
  YYSYMBOL_bit_xor = 230,                  /* bit_xor  */
  YYSYMBOL_bit_and = 231,                  /* bit_and  */
  YYSYMBOL_equality = 232,                 /* equality  */
  YYSYMBOL_relational = 233,               /* relational  */
  YYSYMBOL_shift = 234,                    /* shift  */
  YYSYMBOL_additive = 235,                 /* additive  */
  YYSYMBOL_multiplicative = 236,           /* multiplicative  */
  YYSYMBOL_unary = 237,                    /* unary  */
  YYSYMBOL_postfix = 238,                  /* postfix  */
  YYSYMBOL_opt_slice = 239,                /* opt_slice  */
  YYSYMBOL_opt_args = 240,                 /* opt_args  */
  YYSYMBOL_arg_list = 241,                 /* arg_list  */
  YYSYMBOL_arg = 242,                      /* arg  */
  YYSYMBOL_primary = 243,                  /* primary  */
  YYSYMBOL_literal = 244,                  /* literal  */
  YYSYMBOL_tuple_expr = 245,               /* tuple_expr  */
  YYSYMBOL_opt_expr_list = 246,            /* opt_expr_list  */
  YYSYMBOL_array_expr = 247,               /* array_expr  */
  YYSYMBOL_opt_array_items = 248,          /* opt_array_items  */
  YYSYMBOL_array_items = 249,              /* array_items  */
  YYSYMBOL_array_item = 250,               /* array_item  */
  YYSYMBOL_object_expr = 251,              /* object_expr  */
  YYSYMBOL_obj_fields = 252,               /* obj_fields  */
  YYSYMBOL_obj_field = 253,                /* obj_field  */
  YYSYMBOL_lambda_expr = 254,              /* lambda_expr  */
  YYSYMBOL_opt_lambda_params = 255,        /* opt_lambda_params  */
  YYSYMBOL_lambda_params = 256,            /* lambda_params  */
  YYSYMBOL_lambda_body = 257,              /* lambda_body  */
  YYSYMBOL_if_expr = 258,                  /* if_expr  */
  YYSYMBOL_match_expr = 259,               /* match_expr  */
  YYSYMBOL_try_expr = 260,                 /* try_expr  */
  YYSYMBOL_lvalue = 261,                   /* lvalue  */
  YYSYMBOL_path = 262,                     /* path  */
  YYSYMBOL_type_expr = 263,                /* type_expr  */
  YYSYMBOL_type_primary = 264,             /* type_primary  */
  YYSYMBOL_tuple_type = 265,               /* tuple_type  */
  YYSYMBOL_opt_type_list = 266,            /* opt_type_list  */
  YYSYMBOL_type_list = 267,                /* type_list  */
  YYSYMBOL_array_type = 268,               /* array_type  */
  YYSYMBOL_fn_type = 269,                  /* fn_type  */
  YYSYMBOL_AMP = 270                       /* AMP  */
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
typedef yytype_int16 yy_state_t;

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

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
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
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

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
#define YYFINAL  4
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1327

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  116
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  155
/* YYNRULES -- Number of rules.  */
#define YYNRULES  382
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  710

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   370


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
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    98,    98,   102,   103,   107,   108,   112,   113,   117,
     118,   122,   123,   124,   125,   129,   130,   134,   135,   139,
     140,   144,   145,   146,   150,   151,   152,   156,   157,   158,
     159,   160,   161,   165,   166,   170,   171,   175,   179,   180,
     184,   185,   189,   190,   191,   197,   198,   202,   203,   204,
     205,   209,   213,   214,   218,   219,   223,   224,   228,   229,
     230,   234,   235,   239,   240,   244,   245,   249,   250,   254,
     255,   259,   260,   264,   265,   269,   270,   274,   275,   279,
     283,   284,   288,   289,   293,   294,   295,   299,   300,   304,
     305,   309,   310,   314,   315,   319,   320,   321,   325,   326,
     330,   331,   335,   336,   340,   341,   342,   348,   349,   353,
     354,   358,   362,   363,   367,   368,   372,   373,   377,   378,
     379,   383,   384,   388,   389,   395,   399,   400,   401,   402,
     406,   407,   411,   415,   416,   420,   421,   427,   431,   432,
     436,   437,   438,   439,   440,   441,   442,   443,   444,   445,
     446,   447,   448,   449,   450,   454,   458,   462,   466,   467,
     471,   475,   479,   480,   484,   485,   486,   490,   491,   495,
     496,   500,   501,   505,   509,   510,   514,   518,   519,   523,
     524,   528,   532,   533,   537,   538,   542,   543,   547,   548,
     552,   553,   554,   555,   559,   560,   564,   565,   569,   570,
     574,   575,   579,   580,   584,   588,   594,   595,   596,   597,
     598,   599,   600,   601,   605,   609,   613,   617,   618,   622,
     623,   627,   628,   632,   636,   637,   641,   642,   643,   644,
     645,   649,   650,   656,   660,   661,   662,   663,   664,   665,
     666,   667,   668,   669,   670,   671,   675,   676,   680,   681,
     685,   686,   690,   691,   695,   696,   700,   701,   705,   706,
     710,   711,   715,   716,   717,   721,   722,   723,   724,   725,
     726,   727,   731,   732,   733,   737,   738,   739,   743,   744,
     745,   746,   750,   751,   752,   753,   754,   755,   756,   757,
     758,   759,   760,   764,   765,   766,   767,   768,   769,   770,
     774,   775,   776,   777,   778,   782,   783,   787,   788,   792,
     793,   797,   798,   799,   800,   801,   802,   803,   804,   805,
     806,   807,   811,   812,   813,   814,   815,   816,   817,   821,
     825,   826,   830,   834,   835,   839,   840,   844,   845,   849,
     853,   854,   858,   859,   860,   861,   865,   866,   870,   871,
     875,   876,   880,   881,   885,   886,   890,   894,   895,   899,
     900,   904,   905,   906,   912,   913,   914,   915,   916,   920,
     921,   922,   923,   924,   928,   932,   933,   937,   938,   942,
     943,   947,   951
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
  "\"end of file\"", "error", "\"invalid token\"", "IDENT", "INT_LIT",
  "FLOAT_LIT", "STRING_LIT", "CHAR_LIT", "TRUE", "FALSE", "NULL_LIT",
  "MODULE", "IMPORT", "EXPORT", "FROM", "AS", "PUB", "PRIV", "LET", "VAR",
  "CONST", "STATIC", "MUT", "FN", "LAMBDA", "TYPEDEF", "TYPE", "STRUCT",
  "ENUM", "UNION", "INTERFACE", "TRAIT", "IMPL", "WHERE", "EXTENDS",
  "IMPLEMENTS", "IF", "ELSE", "FOR", "WHILE", "DO", "SWITCH", "CASE",
  "DEFAULT", "MATCH", "BREAK", "CONTINUE", "RETURN", "YIELD", "TRY",
  "CATCH", "FINALLY", "THROW", "DEFER", "NEW", "DELETE", "SIZEOF",
  "TYPEOF", "IN", "IS", "AS_OP", "AWAIT", "ASYNC", "LPAREN", "RPAREN",
  "LBRACE", "RBRACE", "LBRACK", "RBRACK", "COMMA", "SEMI", "DOT", "COLON",
  "DCOLON", "QMARK", "ARROW", "FATARROW", "ELLIPSIS", "ASSIGN", "PLUS",
  "MINUS", "STAR", "SLASH", "PERCENT", "INC", "DEC", "EQ", "NE", "LT",
  "LE", "GT", "GE", "AND", "OR", "NOT", "BITAND", "BITOR", "BITXOR",
  "BITNOT", "SHL", "SHR", "PLUS_ASSIGN", "MINUS_ASSIGN", "STAR_ASSIGN",
  "SLASH_ASSIGN", "PERCENT_ASSIGN", "AND_ASSIGN", "OR_ASSIGN",
  "XOR_ASSIGN", "SHL_ASSIGN", "SHR_ASSIGN", "COALESCE", "PIPE", "RANGE",
  "RANGE_INCL", "UMINUS", "$accept", "compilation_unit", "opt_shebang",
  "opt_module_decl", "opt_imports", "import_stmt", "import_spec",
  "import_list", "import_item", "opt_top_items", "top_item",
  "opt_visibility", "item", "attr_block", "attr", "AT_IDENT",
  "opt_attr_args", "attr_args", "attr_arg", "type_alias", "type_decl",
  "trait_decl", "impl_block", "opt_type_params", "type_param_list",
  "type_param", "type_bound_list", "type_bound", "type_arg_list",
  "opt_where_clause", "where_list", "where_item", "struct_fields",
  "struct_field", "union_fields", "union_field", "enum_variants",
  "enum_variant", "opt_variant_payload", "opt_variant_discriminant",
  "iface_members", "iface_member", "trait_members", "trait_member",
  "assoc_type", "const_sig", "impl_members", "impl_member", "func_decl",
  "opt_async", "func_sig", "opt_ret_type", "opt_params", "param_list",
  "param", "param_kind", "opt_param_default", "global_decl", "storage",
  "decl_list", "decl_item", "opt_type_annot", "opt_init", "block",
  "stmt_list", "stmt", "labeled_stmt", "decl_stmt", "expr_stmt", "if_stmt",
  "while_stmt", "do_while_stmt", "for_stmt", "opt_for_init",
  "opt_for_cond", "opt_for_step", "expr_list", "switch_stmt",
  "case_blocks", "case_block", "case_value_list", "opt_default",
  "match_stmt", "match_arms", "match_arm", "opt_match_guard",
  "match_arm_body", "jump_stmt", "opt_label", "opt_expr", "try_stmt",
  "catch_blocks", "opt_finally", "throw_stmt", "defer_stmt", "pattern",
  "UNDERSCORE", "OR_PATTERN", "tuple_pattern", "opt_pattern_list",
  "pattern_list", "array_pattern", "struct_pattern", "struct_pat_fields",
  "struct_pat_field", "enum_pattern", "expr", "assignment", "conditional",
  "pipe", "coalesce", "logical_or", "logical_and", "bit_or", "bit_xor",
  "bit_and", "equality", "relational", "shift", "additive",
  "multiplicative", "unary", "postfix", "opt_slice", "opt_args",
  "arg_list", "arg", "primary", "literal", "tuple_expr", "opt_expr_list",
  "array_expr", "opt_array_items", "array_items", "array_item",
  "object_expr", "obj_fields", "obj_field", "lambda_expr",
  "opt_lambda_params", "lambda_params", "lambda_body", "if_expr",
  "match_expr", "try_expr", "lvalue", "path", "type_expr", "type_primary",
  "tuple_type", "opt_type_list", "type_list", "array_type", "fn_type",
  "AMP", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-545)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-364)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      47,  -545,    63,    66,  -545,   116,  -545,  -545,   318,   367,
    -545,   147,    81,    41,   181,  -545,   242,  -545,  -545,   134,
      81,    81,    65,   255,   143,  -545,  -545,  -545,  -545,  -545,
     198,   191,   183,   255,    41,  -545,  -545,  -545,  -545,    81,
     226,   280,   284,   294,   296,   306,   171,  -545,  -545,  -545,
    -545,   429,  -545,  -545,  -545,  -545,  -545,   288,  -545,   178,
      81,   139,   252,   249,   245,    81,  -545,  -545,  -545,  -545,
    -545,   319,   263,  -545,   321,  -545,   276,    46,   343,   171,
     171,   171,   171,   171,   347,    81,  -545,  -545,  -545,   399,
    -545,   327,   394,   376,  -545,  -545,  -545,  -545,  -545,  -545,
    -545,   178,   178,   331,  -545,   -27,  -545,  -545,  -545,  -545,
    -545,  -545,   409,   143,   378,  -545,  -545,    81,  -545,   926,
     143,   437,   448,   198,  -545,  -545,   381,    81,   424,   424,
     424,   424,   424,   120,    69,  -545,    58,  -545,   499,   171,
     383,   417,   416,   336,   178,  -545,    81,  -545,   422,   178,
     178,  -545,   412,   143,  -545,   171,   178,   432,   452,   991,
      81,  1056,  1056,  1056,  1056,   926,  -545,   926,  1056,  1056,
    1056,  1056,  1056,   460,  -545,  -545,   -37,   413,   440,   442,
     439,   445,   444,   326,   401,   355,   391,   395,  -545,   159,
    -545,  -545,  -545,  -545,  -545,  -545,  -545,  -545,  -545,   583,
     255,  -545,   531,  -545,  -545,   236,   128,   475,   478,   480,
     481,   482,   116,    81,   347,  -545,    81,   486,   474,   489,
     485,  -545,  -545,   492,  -545,   178,  -545,   487,  -545,   143,
     926,  -545,   383,   494,    17,    81,  -545,   503,   383,   493,
     501,   926,   926,   491,  -545,  -545,   143,  -545,  -545,   159,
    -545,  -545,  -545,   502,   508,   510,    70,   498,   516,   518,
    -545,  -545,  -545,  -545,  -545,  -545,  -545,   926,  1056,  1056,
    1056,  1056,  1056,  1056,  1056,  1056,  1056,  1056,  1056,  1056,
    1056,  1056,  1056,  1056,  1056,  1056,  1056,  1056,  1056,  1056,
    1121,   860,   582,   585,  -545,  -545,   926,   926,   926,   926,
     926,   926,   926,   926,   926,   926,   926,  -545,  -545,   517,
     521,  -545,   142,  -545,  -545,  -545,  -545,  -545,   512,  -545,
     304,   143,  -545,   -10,  -545,   828,  -545,   499,   529,   383,
     526,  -545,  -545,   269,  -545,  -545,  -545,   143,   529,   991,
     178,   532,   535,   659,   926,  -545,  -545,   533,  -545,   926,
    -545,   926,  -545,   926,   536,  -545,  -545,   442,   439,   445,
     444,   326,   401,   401,   355,   355,   355,   355,   355,   355,
     391,   391,   395,   395,  -545,  -545,  -545,   538,  -545,   537,
     542,  -545,   -34,   544,   546,  -545,  -545,  -545,  -545,  -545,
    -545,  -545,  -545,  -545,  -545,  -545,  -545,  -545,   116,   128,
     116,   549,   550,   551,    10,    26,   116,    81,   553,   282,
    -545,  -545,   224,   556,   555,  -545,  -545,  -545,   178,   558,
    -545,  -545,  -545,   383,   991,   561,   559,   564,   565,   567,
     727,   569,   570,   632,   632,   926,   926,   991,   926,   727,
    -545,  -545,   178,  -545,  -545,  -545,  -545,  -545,  -545,  -545,
    -545,  -545,  -545,  -545,  -545,  -545,  -545,  -545,   566,  -545,
     926,   568,  -545,  -545,  1056,   926,  -545,  1121,   926,   926,
     926,  -545,   512,  -545,   512,  -545,   429,  -545,  -545,   429,
    -545,  -545,   429,  -545,   635,  -545,  -545,   572,   616,   573,
      81,  -545,  -545,   574,   575,   577,  -545,   146,  -545,  -545,
    -545,  -545,  -545,  -545,  -545,    81,   412,  -545,    -9,   412,
     611,   614,  -545,   727,   926,   795,   926,   613,   926,   926,
    -545,   584,   586,   588,  -545,   589,   603,   600,   601,  -545,
     414,  -545,   604,  -545,  -545,  -545,  -545,  -545,  -545,  -545,
      81,   652,    81,   594,  -545,   671,  -545,    54,  -545,  -545,
    -545,   305,   390,    31,   424,  -545,  -545,   424,   629,   926,
     610,  -545,   637,   379,  1186,  1186,  -545,   626,   502,    -7,
    -545,   256,   409,   645,   647,   653,   654,  -545,  -545,  -545,
    -545,   656,   443,   603,  -545,  -545,  -545,    99,    95,   130,
      81,   171,   643,  -545,    40,   720,    97,   991,  -545,  -545,
    -545,  -545,   272,  -545,   727,   926,   926,   727,   926,   662,
     663,   178,   675,   629,  -545,   443,    23,    81,  -545,   664,
     670,   143,   678,   926,   681,   926,  -545,   674,  -545,  -545,
    -545,   257,     3,   611,   717,     4,   685,  -545,   692,  -545,
     694,  -545,  -545,   -14,   178,  -545,  -545,  -545,   926,   695,
     696,   926,   691,  -545,   529,  -545,   674,  -545,  -545,   926,
     688,   727,   926,   727,   699,   456,   704,   629,    -6,   707,
    -545,  -545,  -545,  -545,   697,  -545,  -545,   991,  -545,   714,
     502,  -545,  -545,   926,   713,  -545,   721,  1204,  -545,   629,
    -545,   412,  -545,   722,  -545,   727,   312,  -545,  -545,  -545,
    -545,   424,  -545,  -545,   926,  -545,   727,  -545,  -545,   727
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       3,     4,     0,     5,     1,     0,     7,   361,     0,    19,
       6,     0,     0,     0,     0,     8,    33,   362,   361,     0,
     375,     0,     0,   370,    65,   364,   371,   372,   373,    11,
       0,     0,     0,    12,     0,   126,   127,   128,   129,     0,
       0,     0,     0,     0,     0,     0,    54,   110,    21,    20,
      23,    24,    27,    28,    29,    30,    31,     0,    32,     0,
     375,   377,     0,   376,     0,     0,   363,   367,   365,   382,
     366,    17,     0,    15,     0,     9,     0,     0,     0,    54,
      54,    54,    54,    54,     0,     0,    37,    25,    26,   109,
      34,    35,     0,   206,   322,   323,   324,   325,   326,   327,
     328,   217,   217,     0,   130,   133,   207,   209,   210,   211,
     212,   208,     0,   377,     0,   368,   374,     0,   379,     0,
      66,     0,     0,     0,    14,    10,     0,     0,    67,    67,
      67,    67,    67,    58,     0,    56,    67,    22,    38,    54,
     219,     0,   218,     0,     0,   125,     0,   215,   135,     0,
     217,   224,   112,   378,   312,    54,   348,     0,     0,     0,
       0,     0,     0,     0,     0,   330,   340,   333,     0,     0,
       0,     0,     0,     0,   233,   234,   246,   248,   250,   252,
     254,   256,   258,   260,   262,   265,   272,   275,   278,   282,
     293,   311,   315,   316,   317,   318,   319,   320,   321,     0,
     313,    18,     0,    16,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    55,     0,     0,    42,     0,
      39,    40,    44,     0,   216,     0,   221,     0,   131,   134,
       0,   132,   213,     0,     0,     0,   381,     0,   350,     0,
     349,     0,     0,   138,   358,   357,   290,   312,   291,   282,
     288,   289,   292,   331,   171,     0,     0,   337,     0,   334,
     335,   285,   286,   287,   283,   284,   380,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     305,   196,     0,     0,   294,   295,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    13,    46,   361,
      68,    69,     0,    73,    80,    77,    89,    93,    59,    61,
      63,    60,    57,    67,   102,     0,    36,     0,    33,   220,
       0,   136,   231,   229,   223,   230,   225,   113,    33,     0,
       0,     0,     0,     0,     0,   314,   329,     0,   339,     0,
     341,     0,   332,     0,     0,   249,   251,   253,   255,   257,
     259,   261,   263,   264,   270,   271,   266,   267,   268,   269,
     273,   274,   276,   277,   279,   280,   281,   312,   309,     0,
     306,   307,     0,   197,     0,   298,   299,   235,   236,   237,
     238,   239,   240,   241,   242,   243,   244,   245,     0,     0,
       0,    33,    33,    33,   109,   109,     0,     0,     0,   109,
      43,    41,   121,     0,   115,   116,   222,   228,     0,     0,
     353,   352,   346,   351,     0,     0,   312,     0,     0,     0,
       0,     0,     0,   194,   194,   196,   196,     0,     0,     0,
     137,   140,     0,   141,   139,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,     0,   172,
       0,   345,   338,   336,     0,     0,   297,     0,   196,   196,
     196,   296,    71,    70,    72,    47,    24,    74,    48,     0,
      81,    49,    24,    78,     0,    50,    90,     0,     0,     0,
       0,    51,    94,     0,     0,     0,    62,     0,   102,    52,
     106,   103,   104,   105,   122,     0,   112,    33,   227,   112,
       0,     0,   182,     0,     0,   164,     0,     0,     0,     0,
     195,     0,     0,     0,   197,     0,   358,   357,     0,   205,
       0,   157,   343,   344,   247,   310,   308,   301,   302,   303,
       0,     0,     0,    98,    92,     0,    91,     0,    96,    97,
      95,    64,   109,     0,    67,   117,   226,    67,     0,     0,
      33,   155,     0,   312,   217,   217,   165,     0,   166,     0,
     171,   311,   313,     0,     0,     0,     0,   190,   191,   192,
     193,     0,   202,     0,   204,   156,   342,     0,    84,     0,
       0,    54,   100,    53,   123,     0,     0,     0,   138,   355,
     354,   356,     0,   183,     0,   167,     0,     0,     0,     0,
       0,     0,     0,     0,   198,   202,     0,   375,    73,    87,
       0,    99,     0,     0,     0,     0,   118,   123,   108,   107,
     347,    37,   186,   141,   158,     0,     0,   168,     0,   160,
       0,   174,   182,     0,     0,   203,   199,    75,     0,     0,
      33,     0,    83,    79,    33,   101,   123,   124,   120,     0,
       0,     0,   169,     0,     0,   179,    33,     0,     0,     0,
      85,    86,    88,    82,     0,   119,   187,     0,   159,     0,
     170,   163,   161,     0,     0,   175,     0,   181,   200,     0,
      76,   112,   189,   185,   188,     0,     0,   177,   138,   173,
     201,    67,   184,   162,     0,   138,   180,   111,   178,   176
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -545,  -545,  -545,  -545,  -545,  -545,   752,  -545,   666,  -545,
    -545,  -417,   706,   -16,  -545,  -545,  -545,  -545,   466,  -388,
    -545,  -545,  -545,   -69,  -545,   592,  -165,   402,   389,  -104,
    -545,   410,   192,  -545,  -545,  -545,  -545,  -545,  -545,  -545,
    -545,  -545,  -545,  -545,   415,  -545,   324,  -545,  -380,   114,
     418,  -491,  -331,  -545,   310,  -545,  -532,  -378,   -11,   382,
     682,  -545,  -545,   -48,  -544,  -397,  -545,   313,  -545,  -545,
    -545,  -545,  -545,  -545,  -545,  -545,  -496,  -545,  -545,  -545,
    -545,  -545,  -545,   185,  -545,  -545,  -545,  -545,   396,  -184,
    -545,   258,   214,  -545,  -545,   -19,  -545,  -545,  -545,   -96,
    -545,  -545,  -545,  -545,  -545,  -545,    -2,  1021,   384,  -545,
     578,   571,   587,   576,   581,   598,   580,   251,   300,   237,
     246,     7,    86,  -545,  -545,  -545,   375,  -545,   -35,  -545,
    -545,  -545,  -545,  -545,   490,  -545,  -545,  -545,  -545,  -545,
    -545,   248,  -545,  -545,  -545,  -545,    -4,   -17,  -545,  -545,
     -58,  -545,  -545,  -545,  -545
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     2,     3,     6,     9,    15,    32,    72,    73,    16,
      49,    89,    50,   412,    90,    91,   219,   220,   221,    52,
      53,    54,    55,    85,   134,   135,   318,   319,    22,   207,
     310,   311,   401,   477,   403,   483,   402,   480,   619,   652,
     404,   486,   405,   492,   487,   494,   409,   501,    56,    57,
     489,   236,   413,   414,   415,   505,   626,    58,   442,   103,
     104,   148,   231,   443,   343,   444,   445,   446,   447,   448,
     449,   450,   451,   567,   636,   679,   253,   452,   665,   685,
     696,   686,   453,   560,   603,   660,   693,   454,   521,   382,
     455,   582,   614,   456,   457,   140,   106,   149,   107,   141,
     142,   108,   109,   234,   336,   110,   458,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,   189,   384,   379,   380,   381,   190,   191,   192,
     255,   193,   258,   259,   260,   194,   256,   350,   195,   239,
     240,   422,   196,   197,   198,   199,   200,    24,    25,    26,
      62,    63,    27,    28,    70
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      51,     8,   114,    61,    64,    59,   143,   419,    23,    33,
     128,   129,   130,   131,   132,   554,    23,    23,   557,   568,
     333,   500,    77,   206,   111,   208,   209,   210,   211,   502,
      33,   503,   217,   517,   594,    23,   484,   267,   468,   659,
     105,   559,   529,   113,     7,   146,   490,    29,   120,   126,
     667,   606,   484,     1,   233,   112,    23,   592,   689,   540,
     556,    23,   541,     4,    67,   542,   111,   111,   136,   147,
     223,    68,    47,   347,   531,   268,   485,     5,    59,   469,
     470,    23,   147,   334,    18,    69,   237,   147,    47,   147,
     147,   206,   491,   647,   335,   658,   216,   112,   112,   147,
     153,   648,   616,   222,    19,    67,    30,   624,   595,   111,
     205,   244,    68,    23,   111,   111,   561,   173,   625,     7,
      67,   111,    31,    23,   675,   105,    69,    68,    67,   229,
     232,   309,    67,   620,    65,    68,   348,   238,   214,    68,
     112,    69,    23,   246,    20,   112,   112,   349,    21,    69,
      17,    19,   112,    69,   706,    66,    23,   245,   617,   215,
     618,   709,   598,   254,   500,   257,   680,   628,   248,   250,
     251,   252,   502,    67,   503,   261,   262,   263,   264,   265,
      68,    93,    94,    95,    96,    97,    98,    99,   100,   312,
     111,    20,   212,    34,    69,    21,   321,    60,   213,   323,
     701,    71,    23,   115,    67,    74,   329,   634,   320,    23,
     639,    68,    23,    67,   400,    65,    67,    67,   337,   408,
      68,   112,   290,    68,    68,    69,   291,    86,   331,    78,
     292,    23,   293,   472,    69,   474,   551,    69,    69,   341,
     342,   101,    -2,   294,   295,   102,   504,   249,   249,   249,
     249,   523,   525,    75,   249,   249,   249,   249,   249,    84,
      35,    36,    37,    38,   678,   354,   681,    39,    40,    41,
      42,    43,    44,    45,    46,   631,    94,    95,    96,    97,
      98,    99,   100,    79,   537,   538,   539,    80,   378,   383,
     410,   420,   222,  -206,   374,   375,   376,    81,   703,    82,
      35,    36,    37,    38,    47,   111,   308,    39,    40,    83,
      67,    92,    48,   118,  -208,   119,   116,    68,   117,    67,
    -208,   423,  -361,   674,  -208,  -208,    68,   124,    11,   122,
    -361,    69,   123,  -206,   121,   101,   112,   421,   417,   102,
      69,   418,   459,    12,    47,  -361,   125,   461,   499,   462,
     133,   257,  -208,  -206,   249,   249,   249,   249,   249,   249,
     249,   249,   249,   249,   249,   249,   249,   249,   249,   249,
     249,   249,   249,   249,   249,   249,   510,    11,  -363,    13,
      14,   704,   312,   111,   705,   476,   479,   482,    10,   526,
     138,    11,   407,  -363,   320,    23,   320,   139,    59,   508,
     144,   145,   320,    23,   226,   227,    12,   111,    35,    36,
      37,    38,   275,   276,   112,    39,    40,    35,    36,    37,
      38,   127,   511,   105,    39,    40,    41,    42,    43,    44,
      45,    46,    86,   524,   524,   527,   528,  -206,   112,  -361,
     201,  -361,   152,  -206,  -361,    87,    88,  -206,  -206,  -361,
     596,   204,    47,   597,   283,   284,   593,   206,   532,   277,
     278,    47,   202,   535,  -361,   378,   524,   524,   524,   143,
     285,   286,   150,   547,   151,  -206,   287,   288,   289,   147,
     571,   224,    11,   144,   585,   225,    23,   235,   553,   279,
     280,   281,   282,   612,   613,   241,   569,    12,   683,   684,
     230,    23,   218,    94,    95,    96,    97,    98,    99,   100,
     599,   572,   562,   570,   573,   242,   575,   576,   488,   488,
     370,   371,   622,   587,   269,   589,   362,   363,   266,   571,
     571,   372,   373,   270,   271,   272,    23,   307,    23,   274,
     313,    59,   273,   314,   602,   315,   316,   317,   629,   420,
     249,   324,   325,   326,   327,   328,   633,   600,   332,   649,
     572,   572,   254,   257,   330,   645,   338,   111,  -340,   339,
     340,   344,   345,   621,   346,   351,   111,   364,   365,   366,
     367,   368,   369,   632,   352,   385,    23,   353,   386,   398,
     399,   406,   643,  -114,   416,   421,   424,   707,   112,   425,
     113,   466,   635,   637,   638,   460,   640,   112,   464,   111,
     465,   467,  -300,    23,   471,   475,   478,   481,   498,   688,
     506,   655,   509,   657,   507,   668,   512,   514,   515,   692,
     516,   513,   518,   519,   476,   520,   531,   533,   543,   545,
     112,   700,   544,   546,   548,   549,   669,   550,   558,   672,
     602,   559,   574,   581,   577,   588,   578,   676,   579,   580,
     570,   296,   426,    94,    95,    96,    97,    98,    99,   100,
     583,   584,   590,   586,   591,   694,   601,    35,    36,    37,
      38,   697,   155,   156,   297,   298,   299,   300,   301,   302,
     303,   304,   305,   306,   598,   427,   605,   428,   429,   430,
     431,   604,   708,   432,   433,   434,   435,   436,   437,   607,
     608,   438,   439,   160,   161,   162,   163,   609,   610,   611,
     164,   623,   165,   627,   243,   440,   167,   641,   642,   441,
     426,    94,    95,    96,    97,    98,    99,   100,   644,   168,
     653,   654,   651,   169,   170,    35,    36,    37,    38,   656,
     155,   156,   625,   171,   661,   662,   663,   172,   664,   670,
     673,   691,   671,   427,   677,   428,   429,   430,   431,   682,
     687,   432,   433,   434,   435,   436,   437,   690,   695,   438,
     439,   160,   161,   162,   163,   698,    76,   699,   164,   203,
     165,   702,   243,   411,   167,   137,   497,   441,   563,    94,
      95,    96,    97,    98,    99,   100,   322,   168,   496,   473,
     650,   169,   170,    35,    36,    37,    38,   555,   155,   156,
     493,   171,   552,   495,   530,   172,   228,   666,   566,   646,
     522,   157,    94,    95,    96,    97,    98,    99,   100,   158,
     356,   615,   536,   463,   159,   630,   355,   358,   534,   160,
     161,   162,   163,   359,   361,     0,   164,   357,   564,     0,
     166,     0,   565,   154,    94,    95,    96,    97,    98,    99,
     100,   360,     0,     0,     0,   168,     0,     0,     0,   169,
     170,     0,     0,   155,   156,     0,     0,     0,     0,   171,
       0,     0,     0,   172,     0,     0,   157,     0,     0,     0,
       0,     0,     0,     0,   158,     0,     0,     0,     0,   159,
       0,     0,     0,     0,   160,   161,   162,   163,     0,     0,
       0,   164,     0,   165,     0,   166,     0,   167,  -304,   154,
      94,    95,    96,    97,    98,    99,   100,     0,     0,     0,
     168,     0,     0,     0,   169,   170,     0,     0,     0,   155,
     156,     0,     0,     0,   171,     0,     0,     0,   172,     0,
       0,     0,   157,     0,     0,     0,     0,     0,     0,     0,
     158,     0,     0,     0,     0,   159,     0,     0,     0,     0,
     160,   161,   162,   163,     0,     0,     0,   164,     0,   165,
       0,   166,     0,   167,   154,    94,    95,    96,    97,    98,
      99,   100,     0,     0,     0,     0,   168,     0,     0,     0,
     169,   170,     0,     0,   155,   156,     0,     0,     0,     0,
     171,     0,     0,     0,   172,     0,     0,   157,     0,     0,
       0,     0,     0,     0,     0,   158,     0,     0,     0,     0,
     159,     0,     0,     0,     0,   160,   161,   162,   163,     0,
       0,     0,   164,     0,   165,     0,   243,     0,   167,   247,
      94,    95,    96,    97,    98,    99,   100,     0,     0,     0,
       0,   168,     0,     0,     0,   169,   170,     0,     0,   155,
     156,     0,     0,     0,     0,   171,     0,     0,     0,   172,
       0,     0,   157,     0,     0,     0,     0,     0,     0,     0,
     158,     0,     0,     0,     0,   159,     0,     0,     0,     0,
     160,   161,   162,   163,     0,     0,     0,   164,     0,   165,
       0,   166,     0,   167,   377,    94,    95,    96,    97,    98,
      99,   100,     0,     0,     0,     0,   168,     0,     0,     0,
     169,   170,     0,     0,   155,   156,     0,     0,     0,     0,
     171,     0,     0,     0,   172,     0,     0,   157,     0,     0,
       0,     0,     0,     0,     0,   158,     0,     0,     0,     0,
     159,     0,     0,     0,     0,   160,   161,   162,   163,     0,
       0,     0,   164,     0,   165,     0,   166,     0,   167,   563,
      94,    95,    96,    97,    98,    99,   100,     0,     0,     0,
       0,   168,     0,     0,     0,   169,   170,     0,     0,   155,
     156,     0,     0,     0,     0,   171,     0,     0,     0,   172,
       0,     0,   157,     0,     0,     0,     0,     0,     0,     0,
     158,     0,     0,     0,     0,   159,     0,     0,     0,     0,
     160,   161,   162,   163,     0,     0,     0,   164,     0,   564,
       0,   166,     0,   565,     0,     0,     0,     0,     0,     0,
       0,     0,  -356,  -356,     0,     0,   168,     0,     0,     0,
     169,   170,     0,     0,     0,  -356,     0,  -356,  -356,     0,
     171,     0,  -356,  -356,   172,  -356,  -356,  -356,     0,     0,
    -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,     0,  -356,
    -356,  -356,     0,  -356,  -356,  -356,  -356,  -356,  -356,  -356,
    -356,  -356,  -356,  -356,  -356,  -356,  -356,   387,   388,   389,
     390,   391,   392,   393,   394,   395,   396,   397
};

static const yytype_int16 yycheck[] =
{
      16,     5,    60,    20,    21,    16,   102,   338,    12,    13,
      79,    80,    81,    82,    83,   506,    20,    21,   509,   515,
       3,   409,    39,    33,    59,   129,   130,   131,   132,   409,
      34,   409,   136,   430,     3,    39,    26,    74,    72,    36,
      59,    37,   439,    60,     3,    72,    20,     6,    65,     3,
      64,    58,    26,     6,   150,    59,    60,     3,    64,   476,
      69,    65,   479,     0,    74,   482,   101,   102,    85,    96,
     139,    81,    62,     3,    70,   112,    66,    11,    89,   113,
     114,    85,    96,    66,     3,    95,   155,    96,    62,    96,
      96,    33,    66,    70,    77,   627,    38,   101,   102,    96,
     117,    78,     3,   138,    23,    74,    65,    67,    77,   144,
     127,   159,    81,   117,   149,   150,   513,   119,    78,     3,
      74,   156,    81,   127,   656,   144,    95,    81,    74,   146,
     149,     3,    74,     3,    69,    81,    66,   156,    69,    81,
     144,    95,   146,   160,    63,   149,   150,    77,    67,    95,
       3,    23,   156,    95,   698,    90,   160,   159,    63,    90,
      65,   705,    65,   165,   552,   167,   662,    70,   161,   162,
     163,   164,   552,    74,   552,   168,   169,   170,   171,   172,
      81,     3,     4,     5,     6,     7,     8,     9,    10,   206,
     225,    63,    72,    12,    95,    67,   213,    63,    78,   216,
     691,     3,   206,    64,    74,    14,   225,   604,   212,   213,
     607,    81,   216,    74,    72,    69,    74,    74,   235,   323,
      81,   225,    63,    81,    81,    95,    67,     3,   230,     3,
      71,   235,    73,   398,    95,   400,    90,    95,    95,   241,
     242,    63,     0,    84,    85,    67,    22,   161,   162,   163,
     164,   435,   436,    70,   168,   169,   170,   171,   172,    88,
      18,    19,    20,    21,   661,   267,   663,    25,    26,    27,
      28,    29,    30,    31,    32,     3,     4,     5,     6,     7,
       8,     9,    10,     3,   468,   469,   470,     3,   290,   291,
     325,   339,   327,    36,   287,   288,   289,     3,   695,     3,
      18,    19,    20,    21,    62,   340,    70,    25,    26,     3,
      74,    23,    70,    68,    58,    70,    64,    81,    69,    74,
      64,   340,    65,   654,    68,    69,    81,     6,    73,    66,
      73,    95,    69,    76,    15,    63,   340,   339,    69,    67,
      95,    72,   344,    88,    62,    88,    70,   349,    66,   351,
       3,   353,    96,    96,   268,   269,   270,   271,   272,   273,
     274,   275,   276,   277,   278,   279,   280,   281,   282,   283,
     284,   285,   286,   287,   288,   289,   424,    73,    73,    12,
      13,    69,   399,   418,    72,   401,   402,   403,    70,   437,
      63,    73,    88,    88,   398,   399,   400,     3,   409,   418,
      69,    70,   406,   407,    68,    69,    88,   442,    18,    19,
      20,    21,    86,    87,   418,    25,    26,    18,    19,    20,
      21,    78,   424,   442,    25,    26,    27,    28,    29,    30,
      31,    32,     3,   435,   436,   437,   438,    58,   442,    63,
       3,    65,    64,    64,    65,    16,    17,    68,    69,    73,
     554,    70,    62,   557,    99,   100,    66,    33,   460,    58,
      59,    62,    14,   465,    88,   467,   468,   469,   470,   565,
      79,    80,    63,   490,    65,    96,    81,    82,    83,    96,
     515,    64,    73,    69,    70,    69,   490,    75,   505,    88,
      89,    90,    91,    50,    51,    63,   515,    88,    42,    43,
      78,   505,     3,     4,     5,     6,     7,     8,     9,    10,
     558,   515,   514,   515,   516,    63,   518,   519,   404,   405,
     283,   284,   591,   540,   111,   542,   275,   276,    68,   564,
     565,   285,   286,    93,    92,    96,   540,     6,   542,    95,
      65,   552,    97,    65,   560,    65,    65,    65,   596,   597,
     464,    65,    78,    64,    69,    63,   604,   559,    64,   617,
     564,   565,   564,   565,    77,   613,    63,   602,    77,    76,
      69,    69,    64,   590,    64,    77,   611,   277,   278,   279,
     280,   281,   282,   602,    68,     3,   590,    69,     3,    72,
      69,    79,   611,    64,    68,   597,    64,   701,   602,    64,
     617,    64,   604,   605,   606,    72,   608,   611,    72,   644,
      72,    69,    68,   617,    68,    66,    66,    66,    65,   667,
      64,   623,    64,   625,    69,   644,    65,    63,    63,   677,
      63,    72,    63,    63,   650,     3,    70,    69,     3,    23,
     644,   689,    70,    70,    70,    70,   648,    70,    37,   651,
     666,    37,    39,    50,    70,     3,    70,   659,    70,    70,
     662,    78,     3,     4,     5,     6,     7,     8,     9,    10,
      70,    70,    78,    69,     3,   677,    66,    18,    19,    20,
      21,   683,    23,    24,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,    65,    36,    70,    38,    39,    40,
      41,    64,   704,    44,    45,    46,    47,    48,    49,    64,
      63,    52,    53,    54,    55,    56,    57,    64,    64,    63,
      61,    78,    63,     3,    65,    66,    67,    65,    65,    70,
       3,     4,     5,     6,     7,     8,     9,    10,    63,    80,
      70,    63,    78,    84,    85,    18,    19,    20,    21,    68,
      23,    24,    78,    94,    37,    70,    64,    98,    64,    64,
      69,    64,    66,    36,    76,    38,    39,    40,    41,    70,
      66,    44,    45,    46,    47,    48,    49,    70,    64,    52,
      53,    54,    55,    56,    57,    72,    34,    66,    61,   123,
      63,    69,    65,   327,    67,    89,   407,    70,     3,     4,
       5,     6,     7,     8,     9,    10,   214,    80,   406,   399,
     618,    84,    85,    18,    19,    20,    21,   507,    23,    24,
     405,    94,   498,   405,   442,    98,   144,   642,   515,   615,
     434,    36,     4,     5,     6,     7,     8,     9,    10,    44,
     269,   583,   467,   353,    49,   597,   268,   271,   464,    54,
      55,    56,    57,   272,   274,    -1,    61,   270,    63,    -1,
      65,    -1,    67,     3,     4,     5,     6,     7,     8,     9,
      10,   273,    -1,    -1,    -1,    80,    -1,    -1,    -1,    84,
      85,    -1,    -1,    23,    24,    -1,    -1,    -1,    -1,    94,
      -1,    -1,    -1,    98,    -1,    -1,    36,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,    49,
      -1,    -1,    -1,    -1,    54,    55,    56,    57,    -1,    -1,
      -1,    61,    -1,    63,    -1,    65,    -1,    67,    68,     3,
       4,     5,     6,     7,     8,     9,    10,    -1,    -1,    -1,
      80,    -1,    -1,    -1,    84,    85,    -1,    -1,    -1,    23,
      24,    -1,    -1,    -1,    94,    -1,    -1,    -1,    98,    -1,
      -1,    -1,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      44,    -1,    -1,    -1,    -1,    49,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    -1,    -1,    -1,    61,    -1,    63,
      -1,    65,    -1,    67,     3,     4,     5,     6,     7,     8,
       9,    10,    -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,
      84,    85,    -1,    -1,    23,    24,    -1,    -1,    -1,    -1,
      94,    -1,    -1,    -1,    98,    -1,    -1,    36,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,
      49,    -1,    -1,    -1,    -1,    54,    55,    56,    57,    -1,
      -1,    -1,    61,    -1,    63,    -1,    65,    -1,    67,     3,
       4,     5,     6,     7,     8,     9,    10,    -1,    -1,    -1,
      -1,    80,    -1,    -1,    -1,    84,    85,    -1,    -1,    23,
      24,    -1,    -1,    -1,    -1,    94,    -1,    -1,    -1,    98,
      -1,    -1,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      44,    -1,    -1,    -1,    -1,    49,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    -1,    -1,    -1,    61,    -1,    63,
      -1,    65,    -1,    67,     3,     4,     5,     6,     7,     8,
       9,    10,    -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,
      84,    85,    -1,    -1,    23,    24,    -1,    -1,    -1,    -1,
      94,    -1,    -1,    -1,    98,    -1,    -1,    36,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,
      49,    -1,    -1,    -1,    -1,    54,    55,    56,    57,    -1,
      -1,    -1,    61,    -1,    63,    -1,    65,    -1,    67,     3,
       4,     5,     6,     7,     8,     9,    10,    -1,    -1,    -1,
      -1,    80,    -1,    -1,    -1,    84,    85,    -1,    -1,    23,
      24,    -1,    -1,    -1,    -1,    94,    -1,    -1,    -1,    98,
      -1,    -1,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      44,    -1,    -1,    -1,    -1,    49,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    -1,    -1,    -1,    61,    -1,    63,
      -1,    65,    -1,    67,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    58,    59,    -1,    -1,    80,    -1,    -1,    -1,
      84,    85,    -1,    -1,    -1,    71,    -1,    73,    74,    -1,
      94,    -1,    78,    79,    98,    81,    82,    83,    -1,    -1,
      86,    87,    88,    89,    90,    91,    92,    93,    -1,    95,
      96,    97,    -1,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   296,   297,   298,
     299,   300,   301,   302,   303,   304,   305,   306
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int16 yystos[] =
{
       0,     6,   117,   118,     0,    11,   119,     3,   262,   120,
      70,    73,    88,    12,    13,   121,   125,     3,     3,    23,
      63,    67,   144,   262,   263,   264,   265,   268,   269,     6,
      65,    81,   122,   262,    12,    18,    19,    20,    21,    25,
      26,    27,    28,    29,    30,    31,    32,    62,    70,   126,
     128,   129,   135,   136,   137,   138,   164,   165,   173,   174,
      63,   263,   266,   267,   263,    69,    90,    74,    81,    95,
     270,     3,   123,   124,    14,    70,   122,   263,     3,     3,
       3,     3,     3,     3,    88,   139,     3,    16,    17,   127,
     130,   131,    23,     3,     4,     5,     6,     7,     8,     9,
      10,    63,    67,   175,   176,   211,   212,   214,   217,   218,
     221,   244,   262,   263,   266,    64,    64,    69,    68,    70,
     263,    15,    66,    69,     6,    70,     3,    78,   139,   139,
     139,   139,   139,     3,   140,   141,   263,   128,    63,     3,
     211,   215,   216,   215,    69,    70,    72,    96,   177,   213,
      63,    65,    64,   263,     3,    23,    24,    36,    44,    49,
      54,    55,    56,    57,    61,    63,    65,    67,    80,    84,
      85,    94,    98,   222,   223,   224,   225,   226,   227,   228,
     229,   230,   231,   232,   233,   234,   235,   236,   237,   238,
     243,   244,   245,   247,   251,   254,   258,   259,   260,   261,
     262,     3,    14,   124,    70,   263,    33,   145,   145,   145,
     145,   145,    72,    78,    69,    90,    38,   145,     3,   132,
     133,   134,   244,   139,    64,    69,    68,    69,   176,   263,
      78,   178,   211,   215,   219,    75,   167,   139,   211,   255,
     256,    63,    63,    65,   179,   222,   263,     3,   237,   238,
     237,   237,   237,   192,   222,   246,   252,   222,   248,   249,
     250,   237,   237,   237,   237,   237,    68,    74,   112,   111,
      93,    92,    96,    97,    95,    86,    87,    58,    59,    88,
      89,    90,    91,    99,   100,    79,    80,    81,    82,    83,
      63,    67,    71,    73,    84,    85,    78,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,     6,    70,     3,
     146,   147,   263,    65,    65,    65,    65,    65,   142,   143,
     262,   263,   141,   263,    65,    78,    64,    69,    63,   211,
      77,   222,    64,     3,    66,    77,   220,   263,    63,    76,
      69,   222,   222,   180,    69,    64,    64,     3,    66,    77,
     253,    77,    68,    69,   222,   226,   227,   228,   229,   230,
     231,   232,   233,   233,   234,   234,   234,   234,   234,   234,
     235,   235,   236,   236,   237,   237,   237,     3,   222,   240,
     241,   242,   205,   222,   239,     3,     3,   223,   223,   223,
     223,   223,   223,   223,   223,   223,   223,   223,    72,    69,
      72,   148,   152,   150,   156,   158,    79,    88,   145,   162,
     244,   134,   129,   168,   169,   170,    68,    69,    72,   168,
     179,   222,   257,   211,    64,    64,     3,    36,    38,    39,
      40,    41,    44,    45,    46,    47,    48,    49,    52,    53,
      66,    70,   174,   179,   181,   182,   183,   184,   185,   186,
     187,   188,   193,   198,   203,   206,   209,   210,   222,   222,
      72,   222,   222,   250,    72,    72,    64,    69,    72,   113,
     114,    68,   142,   147,   142,    66,   129,   149,    66,   129,
     153,    66,   129,   151,    26,    66,   157,   160,   165,   166,
      20,    66,   159,   160,   161,   166,   143,   144,    65,    66,
     135,   163,   164,   173,    22,   171,    64,    69,   211,    64,
     179,   222,    65,    72,    63,    63,    63,   181,    63,    63,
       3,   204,   204,   205,   222,   205,   179,   222,   222,   181,
     175,    70,   222,    69,   224,   222,   242,   205,   205,   205,
     127,   127,   127,     3,    70,    23,    70,   263,    70,    70,
      70,    90,   162,   263,   167,   170,    69,   167,    37,    37,
     199,   181,   222,     3,    63,    67,   183,   189,   192,   211,
     222,   244,   262,   222,    39,   222,   222,    70,    70,    70,
      70,    50,   207,    70,    70,    70,    69,   263,     3,   263,
      78,     3,     3,    66,     3,    77,   145,   145,    65,   179,
     222,    66,   129,   200,    64,    70,    58,    64,    63,    64,
      64,    63,    50,    51,   208,   207,     3,    63,    65,   154,
       3,   263,   139,    78,    67,    78,   172,     3,    70,   179,
     257,     3,   211,   179,   181,   222,   190,   222,   222,   181,
     222,    65,    65,   211,    63,   179,   208,    70,    78,   266,
     148,    78,   155,    70,    63,   222,    68,   222,   172,    36,
     201,    37,    70,    64,    64,   194,   199,    64,   211,   222,
      64,    66,   222,    69,   168,   172,   222,    76,   181,   191,
     192,   181,    70,    42,    43,   195,   197,    66,   179,    64,
      70,    64,   179,   202,   222,    64,   196,   222,    72,    66,
     179,   167,    69,   181,    69,    72,   180,   145,   222,   180
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int16 yyr1[] =
{
       0,   116,   117,   118,   118,   119,   119,   120,   120,   121,
     121,   122,   122,   122,   122,   123,   123,   124,   124,   125,
     125,   126,   126,   126,   127,   127,   127,   128,   128,   128,
     128,   128,   128,   129,   129,   130,   130,   131,   132,   132,
     133,   133,   134,   134,   134,   135,   135,   136,   136,   136,
     136,   137,   138,   138,   139,   139,   140,   140,   141,   141,
     141,   142,   142,   143,   143,   144,   144,   145,   145,   146,
     146,   147,   147,   148,   148,   149,   149,   150,   150,   151,
     152,   152,   153,   153,   154,   154,   154,   155,   155,   156,
     156,   157,   157,   158,   158,   159,   159,   159,   160,   160,
     161,   161,   162,   162,   163,   163,   163,   164,   164,   165,
     165,   166,   167,   167,   168,   168,   169,   169,   170,   170,
     170,   171,   171,   172,   172,   173,   174,   174,   174,   174,
     175,   175,   176,   177,   177,   178,   178,   179,   180,   180,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   182,   183,   184,   185,   185,
     186,   187,   188,   188,   189,   189,   189,   190,   190,   191,
     191,   192,   192,   193,   194,   194,   195,   196,   196,   197,
     197,   198,   199,   199,   200,   200,   201,   201,   202,   202,
     203,   203,   203,   203,   204,   204,   205,   205,   206,   206,
     207,   207,   208,   208,   209,   210,   211,   211,   211,   211,
     211,   211,   211,   211,   212,   213,   214,   215,   215,   216,
     216,   217,   217,   218,   219,   219,   220,   220,   220,   220,
     220,   221,   221,   222,   223,   223,   223,   223,   223,   223,
     223,   223,   223,   223,   223,   223,   224,   224,   225,   225,
     226,   226,   227,   227,   228,   228,   229,   229,   230,   230,
     231,   231,   232,   232,   232,   233,   233,   233,   233,   233,
     233,   233,   234,   234,   234,   235,   235,   235,   236,   236,
     236,   236,   237,   237,   237,   237,   237,   237,   237,   237,
     237,   237,   237,   238,   238,   238,   238,   238,   238,   238,
     239,   239,   239,   239,   239,   240,   240,   241,   241,   242,
     242,   243,   243,   243,   243,   243,   243,   243,   243,   243,
     243,   243,   244,   244,   244,   244,   244,   244,   244,   245,
     246,   246,   247,   248,   248,   249,   249,   250,   250,   251,
     252,   252,   253,   253,   253,   253,   254,   254,   255,   255,
     256,   256,   257,   257,   258,   258,   259,   260,   260,   261,
     261,   262,   262,   262,   263,   263,   263,   263,   263,   264,
     264,   264,   264,   264,   265,   266,   266,   267,   267,   268,
     268,   269,   270
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     4,     0,     1,     0,     3,     0,     2,     3,
       4,     1,     1,     5,     3,     1,     3,     1,     3,     0,
       2,     1,     3,     1,     0,     1,     1,     1,     1,     1,
       1,     1,     1,     0,     2,     1,     4,     1,     0,     1,
       1,     3,     1,     3,     1,     4,     5,     7,     7,     7,
       7,     7,     7,     9,     0,     3,     1,     3,     1,     3,
       3,     1,     3,     1,     4,     1,     3,     0,     2,     1,
       3,     3,     3,     0,     2,     5,     7,     0,     2,     5,
       0,     2,     6,     5,     0,     3,     3,     0,     2,     0,
       2,     2,     2,     0,     2,     2,     2,     2,     2,     4,
       3,     5,     0,     2,     1,     1,     1,    10,    10,     0,
       1,     9,     0,     2,     0,     1,     1,     3,     5,     7,
       6,     0,     1,     0,     2,     3,     1,     1,     1,     1,
       1,     3,     3,     0,     2,     0,     2,     3,     0,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     3,     3,     2,     5,     7,
       5,     7,     9,     7,     0,     1,     1,     0,     1,     0,
       1,     1,     3,     8,     0,     2,     4,     1,     3,     0,
       3,     7,     0,     2,     6,     5,     0,     2,     1,     1,
       3,     3,     3,     3,     0,     1,     0,     1,     4,     5,
       5,     6,     0,     2,     3,     2,     1,     1,     1,     1,
       1,     1,     1,     3,     1,     1,     3,     0,     1,     1,
       3,     3,     5,     4,     0,     2,     4,     3,     2,     1,
       1,     4,     4,     1,     1,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     1,     5,     1,     3,
       1,     3,     1,     3,     1,     3,     1,     3,     1,     3,
       1,     3,     1,     3,     3,     1,     3,     3,     3,     3,
       3,     3,     1,     3,     3,     1,     3,     3,     1,     3,
       3,     3,     1,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     1,     2,     2,     4,     4,     3,     3,
       1,     3,     3,     3,     0,     0,     1,     1,     3,     1,
       3,     1,     1,     1,     3,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     3,
       0,     1,     3,     0,     1,     1,     3,     1,     3,     3,
       0,     2,     4,     3,     3,     2,     4,     8,     0,     1,
       1,     3,     1,     1,     7,     7,     7,     2,     2,     1,
       1,     1,     3,     4,     1,     2,     2,     2,     3,     1,
       1,     1,     1,     1,     3,     0,     1,     1,     3,     3,
       5,     5,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
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
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




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
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


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

  yychar = YYEMPTY; /* Cause a token to be read.  */

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
    YYNOMEM;
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
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
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
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
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
  yychar = YYEMPTY;
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
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
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
          YYNOMEM;
      }
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
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
  ++yynerrs;

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
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
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



/* End mega_test.y */

