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
#line 20 "./config/bfin-parse.y"


#include "as.h"

#include "bfin-aux.h"  /* Opcode generating auxiliaries.  */
#include "elf/common.h"
#include "elf/bfin.h"

/* This file uses an old-style yyerror returning int.  Disable
   generation of a modern prototype for yyerror.  */
#define yyerror yyerror

#define DSP32ALU(aopcde, HL, dst1, dst0, src0, src1, s, x, aop) \
	bfin_gen_dsp32alu (HL, aopcde, aop, s, x, dst0, dst1, src0, src1)

#define DSP32MAC(op1, MM, mmod, w1, P, h01, h11, h00, h10, dst, op0, src0, src1, w0) \
	bfin_gen_dsp32mac (op1, MM, mmod, w1, P, h01, h11, h00, h10, op0, \
	                   dst, src0, src1, w0)

#define DSP32MULT(op1, MM, mmod, w1, P, h01, h11, h00, h10, dst, op0, src0, src1, w0) \
	bfin_gen_dsp32mult (op1, MM, mmod, w1, P, h01, h11, h00, h10, op0, \
	                    dst, src0, src1, w0)

#define DSP32SHIFT(sopcde, dst0, src0, src1, sop, hls)  \
	bfin_gen_dsp32shift (sopcde, dst0, src0, src1, sop, hls)

#define DSP32SHIFTIMM(sopcde, dst0, immag, src1, sop, hls)  \
	bfin_gen_dsp32shiftimm (sopcde, dst0, immag, src1, sop, hls)

#define LDIMMHALF_R(reg, h, s, z, hword) \
	bfin_gen_ldimmhalf (reg, h, s, z, hword, 1)

#define LDIMMHALF_R5(reg, h, s, z, hword) \
        bfin_gen_ldimmhalf (reg, h, s, z, hword, 2)

#define LDSTIDXI(ptr, reg, w, sz, z, offset)  \
	bfin_gen_ldstidxi (ptr, reg, w, sz, z, offset)

#define LDST(ptr, reg, aop, sz, z, w)  \
	bfin_gen_ldst (ptr, reg, aop, sz, z, w)

#define LDSTII(ptr, reg, offset, w, op)  \
	bfin_gen_ldstii (ptr, reg, offset, w, op)

#define DSPLDST(i, m, reg, aop, w) \
	bfin_gen_dspldst (i, reg, aop, w, m)

#define LDSTPMOD(ptr, reg, idx, aop, w) \
	bfin_gen_ldstpmod (ptr, reg, aop, w, idx)

#define LDSTIIFP(offset, reg, w)  \
	bfin_gen_ldstiifp (reg, offset, w)

#define LOGI2OP(dst, src, opc) \
	bfin_gen_logi2op (opc, src, dst.regno & CODE_MASK)

#define ALU2OP(dst, src, opc)  \
	bfin_gen_alu2op (dst, src, opc)

#define BRCC(t, b, offset) \
	bfin_gen_brcc (t, b, offset)

#define UJUMP(offset) \
	bfin_gen_ujump (offset)

#define PROGCTRL(prgfunc, poprnd) \
	bfin_gen_progctrl (prgfunc, poprnd)

#define PUSHPOPMULTIPLE(dr, pr, d, p, w) \
	bfin_gen_pushpopmultiple (dr, pr, d, p, w)

#define PUSHPOPREG(reg, w) \
	bfin_gen_pushpopreg (reg, w)

#define CALLA(addr, s)  \
	bfin_gen_calla (addr, s)

#define LINKAGE(r, framesize) \
	bfin_gen_linkage (r, framesize)

#define COMPI2OPD(dst, src, op)  \
	bfin_gen_compi2opd (dst, src, op)

#define COMPI2OPP(dst, src, op)  \
	bfin_gen_compi2opp (dst, src, op)

#define DAGMODIK(i, op)  \
	bfin_gen_dagmodik (i, op)

#define DAGMODIM(i, m, op, br)  \
	bfin_gen_dagmodim (i, m, op, br)

#define COMP3OP(dst, src0, src1, opc)   \
	bfin_gen_comp3op (src0, src1, dst, opc)

#define PTR2OP(dst, src, opc)   \
	bfin_gen_ptr2op (dst, src, opc)

#define CCFLAG(x, y, opc, i, g)  \
	bfin_gen_ccflag (x, y, opc, i, g)

#define CCMV(src, dst, t) \
	bfin_gen_ccmv (src, dst, t)

#define CACTRL(reg, a, op) \
	bfin_gen_cactrl (reg, a, op)

#define LOOPSETUP(soffset, c, rop, eoffset, reg) \
	bfin_gen_loopsetup (soffset, c, rop, eoffset, reg)

#define HL2(r1, r0)  (IS_H (r1) << 1 | IS_H (r0))
#define IS_RANGE(bits, expr, sign, mul)    \
	value_match(expr, bits, sign, mul, 1)
#define IS_URANGE(bits, expr, sign, mul)    \
	value_match(expr, bits, sign, mul, 0)
#define IS_CONST(expr) (expr->type == Expr_Node_Constant)
#define IS_RELOC(expr) (expr->type != Expr_Node_Constant)
#define IS_IMM(expr, bits)  value_match (expr, bits, 0, 1, 1)
#define IS_UIMM(expr, bits)  value_match (expr, bits, 0, 1, 0)

#define IS_PCREL4(expr) \
	(value_match (expr, 4, 0, 2, 0))

#define IS_LPPCREL10(expr) \
	(value_match (expr, 10, 0, 2, 0))

#define IS_PCREL10(expr) \
	(value_match (expr, 10, 0, 2, 1))

#define IS_PCREL12(expr) \
	(value_match (expr, 12, 0, 2, 1))

#define IS_PCREL24(expr) \
	(value_match (expr, 24, 0, 2, 1))


static int value_match (Expr_Node *, int, int, int, int);

extern FILE *errorf;
extern INSTR_T insn;

static Expr_Node *binary (Expr_Op_Type, Expr_Node *, Expr_Node *);
static Expr_Node *unary  (Expr_Op_Type, Expr_Node *);

static void notethat (const char *, ...);

extern char *yytext;

/* Used to set SRCx fields to all 1s as described in the PRM.  */
static Register reg7 = {REG_R7, 0};

void error (const char *format, ...)
{
    va_list ap;
    static char buffer[2000];

    va_start (ap, format);
    vsprintf (buffer, format, ap);
    va_end (ap);

    as_bad ("%s", buffer);
}

static int
yyerror (const char *msg)
{
  if (msg[0] == '\0')
    error ("%s", msg);

  else if (yytext[0] != ';')
    error ("%s. Input text was %s.", msg, yytext);
  else
    error ("%s.", msg);

  return -1;
}

static int
in_range_p (Expr_Node *exp, int from, int to, unsigned int mask)
{
  int val = EXPR_VALUE (exp);
  if (exp->type != Expr_Node_Constant)
    return 0;
  if (val < from || val > to)
    return 0;
  return (val & mask) == 0;
}

extern int yylex (void);

#define imm3(x) EXPR_VALUE (x)
#define imm4(x) EXPR_VALUE (x)
#define uimm4(x) EXPR_VALUE (x)
#define imm5(x) EXPR_VALUE (x)
#define uimm5(x) EXPR_VALUE (x)
#define imm6(x) EXPR_VALUE (x)
#define imm7(x) EXPR_VALUE (x)
#define uimm8(x) EXPR_VALUE (x)
#define imm16(x) EXPR_VALUE (x)
#define uimm16s4(x) ((EXPR_VALUE (x)) >> 2)
#define uimm16(x) EXPR_VALUE (x)

/* Return true if a value is inside a range.  */
#define IN_RANGE(x, low, high) \
  (((EXPR_VALUE(x)) >= (low)) && (EXPR_VALUE(x)) <= ((high)))

/* Auxiliary functions.  */

static int
valid_dreg_pair (Register *reg1, Expr_Node *reg2)
{
  if (!IS_DREG (*reg1))
    {
      yyerror ("Dregs expected");
      return 0;
    }

  if (reg1->regno != 1 && reg1->regno != 3)
    {
      yyerror ("Bad register pair");
      return 0;
    }

  if (imm7 (reg2) != reg1->regno - 1)
    {
      yyerror ("Bad register pair");
      return 0;
    }

  reg1->regno--;
  return 1;
}

static int
check_multiply_halfregs (Macfunc *aa, Macfunc *ab)
{
  if ((!REG_EQUAL (aa->s0, ab->s0) && !REG_EQUAL (aa->s0, ab->s1))
      || (!REG_EQUAL (aa->s1, ab->s1) && !REG_EQUAL (aa->s1, ab->s0)))
    return yyerror ("Source multiplication register mismatch");

  return 0;
}


/* Check mac option.  */

static int
check_macfunc_option (Macfunc *a, Opt_mode *opt)
{
  /* Default option is always valid.  */
  if (opt->mod == 0)
    return 0;

  if ((a->w == 1 && a->P == 1
       && opt->mod != M_FU && opt->mod != M_IS && opt->mod != M_IU
       && opt->mod != M_S2RND && opt->mod != M_ISS2)
      || (a->w == 1 && a->P == 0
	  && opt->mod != M_FU && opt->mod != M_IS && opt->mod != M_IU
	  && opt->mod != M_T && opt->mod != M_TFU && opt->mod != M_S2RND
	  && opt->mod != M_ISS2 && opt->mod != M_IH)
      || (a->w == 0 && a->P == 0
	  && opt->mod != M_FU && opt->mod != M_IS && opt->mod != M_W32))
    return -1;

  return 0;
}

/* Check (vector) mac funcs and ops.  */

static int
check_macfuncs (Macfunc *aa, Opt_mode *opa,
		Macfunc *ab, Opt_mode *opb)
{
  /* Variables for swapping.  */
  Macfunc mtmp;
  Opt_mode otmp;

  /* The option mode should be put at the end of the second instruction
     of the vector except M, which should follow MAC1 instruction.  */
  if (opa->mod != 0)
    return yyerror ("Bad opt mode");

  /* If a0macfunc comes before a1macfunc, swap them.  */

  if (aa->n == 0)
    {
      /*  (M) is not allowed here.  */
      if (opa->MM != 0)
	return yyerror ("(M) not allowed with A0MAC");
      if (ab->n != 1)
	return yyerror ("Vector AxMACs can't be same");

      mtmp = *aa; *aa = *ab; *ab = mtmp;
      otmp = *opa; *opa = *opb; *opb = otmp;
    }
  else
    {
      if (opb->MM != 0)
	return yyerror ("(M) not allowed with A0MAC");
      if (ab->n != 0)
	return yyerror ("Vector AxMACs can't be same");
    }

  /*  If both ops are one of 0, 1, or 2, we have multiply_halfregs in both
  assignment_or_macfuncs.  */
  if ((aa->op == 0 || aa->op == 1 || aa->op == 2)
      && (ab->op == 0 || ab->op == 1 || ab->op == 2))
    {
      if (check_multiply_halfregs (aa, ab) < 0)
	return -1;
    }
  else
    {
      /*  Only one of the assign_macfuncs has a half reg multiply
      Evil trick: Just 'OR' their source register codes:
      We can do that, because we know they were initialized to 0
      in the rules that don't use multiply_halfregs.  */
      aa->s0.regno |= (ab->s0.regno & CODE_MASK);
      aa->s1.regno |= (ab->s1.regno & CODE_MASK);
    }

  if (aa->w == ab->w && aa->P != ab->P)
    return yyerror ("Destination Dreg sizes (full or half) must match");

  if (aa->w && ab->w)
    {
      if (aa->P && (aa->dst.regno - ab->dst.regno) != 1)
	return yyerror ("Destination Dregs (full) must differ by one");
      if (!aa->P && aa->dst.regno != ab->dst.regno)
	return yyerror ("Destination Dregs (half) must match");
    }

  /* Make sure mod flags get ORed, too.  */
  opb->mod |= opa->mod;

  /* Check option.  */
  if (check_macfunc_option (aa, opb) < 0
      && check_macfunc_option (ab, opb) < 0)
    return yyerror ("bad option");

  /* Make sure first macfunc has got both P flags ORed.  */
  aa->P |= ab->P;

  return 0;
}


static int
is_group1 (INSTR_T x)
{
  /* Group1 is dpsLDST, LDSTpmod, LDST, LDSTiiFP, LDSTii.  */
  if ((x->value & 0xc000) == 0x8000 || (x->value == 0x0000))
    return 1;

  return 0;
}

static int
is_group2 (INSTR_T x)
{
  if ((((x->value & 0xfc00) == 0x9c00)  /* dspLDST.  */
       && !((x->value & 0xfde0) == 0x9c60)  /* dagMODim.  */
       && !((x->value & 0xfde0) == 0x9ce0)  /* dagMODim with bit rev.  */
       && !((x->value & 0xfde0) == 0x9d60)) /* pick dagMODik.  */
      || (x->value == 0x0000))
    return 1;
  return 0;
}

static int
is_store (INSTR_T x)
{
  if (!x)
    return 0;

  if ((x->value & 0xf000) == 0x8000)
    {
      int aop = ((x->value >> 9) & 0x3);
      int w = ((x->value >> 11) & 0x1);
      if (!w || aop == 3)
	return 0;
      return 1;
    }

  if (((x->value & 0xFF60) == 0x9E60) ||  /* dagMODim_0 */
      ((x->value & 0xFFF0) == 0x9F60))    /* dagMODik_0 */
    return 0;

  /* decode_dspLDST_0 */
  if ((x->value & 0xFC00) == 0x9C00)
    {
      int w = ((x->value >> 9) & 0x1);
      if (w)
	return 1;
    }

  return 0;
}

static INSTR_T
gen_multi_instr_1 (INSTR_T dsp32, INSTR_T dsp16_grp1, INSTR_T dsp16_grp2)
{
  int mask1 = dsp32 ? insn_regmask (dsp32->value, dsp32->next->value) : 0;
  int mask2 = dsp16_grp1 ? insn_regmask (dsp16_grp1->value, 0) : 0;
  int mask3 = dsp16_grp2 ? insn_regmask (dsp16_grp2->value, 0) : 0;

  if ((mask1 & mask2) || (mask1 & mask3) || (mask2 & mask3))
    yyerror ("resource conflict in multi-issue instruction");

  /* Anomaly 05000074 */
  if (ENABLE_AC_05000074
      && dsp32 != NULL && dsp16_grp1 != NULL
      && (dsp32->value & 0xf780) == 0xc680
      && ((dsp16_grp1->value & 0xfe40) == 0x9240
	  || (dsp16_grp1->value & 0xfe08) == 0xba08
	  || (dsp16_grp1->value & 0xfc00) == 0xbc00))
    yyerror ("anomaly 05000074 - Multi-Issue Instruction with \
dsp32shiftimm in slot1 and P-reg Store in slot2 Not Supported");

  if (is_store (dsp16_grp1) && is_store (dsp16_grp2))
    yyerror ("Only one instruction in multi-issue instruction can be a store");

  return bfin_gen_multi_instr (dsp32, dsp16_grp1, dsp16_grp2);
}


#line 498 "config/bfin-parse.c"

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

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_YY_CONFIG_BFIN_PARSE_H_INCLUDED
# define YY_YY_CONFIG_BFIN_PARSE_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
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
    BYTEOP16P = 258,               /* BYTEOP16P  */
    BYTEOP16M = 259,               /* BYTEOP16M  */
    BYTEOP1P = 260,                /* BYTEOP1P  */
    BYTEOP2P = 261,                /* BYTEOP2P  */
    BYTEOP3P = 262,                /* BYTEOP3P  */
    BYTEUNPACK = 263,              /* BYTEUNPACK  */
    BYTEPACK = 264,                /* BYTEPACK  */
    PACK = 265,                    /* PACK  */
    SAA = 266,                     /* SAA  */
    ALIGN8 = 267,                  /* ALIGN8  */
    ALIGN16 = 268,                 /* ALIGN16  */
    ALIGN24 = 269,                 /* ALIGN24  */
    VIT_MAX = 270,                 /* VIT_MAX  */
    EXTRACT = 271,                 /* EXTRACT  */
    DEPOSIT = 272,                 /* DEPOSIT  */
    EXPADJ = 273,                  /* EXPADJ  */
    SEARCH = 274,                  /* SEARCH  */
    ONES = 275,                    /* ONES  */
    SIGN = 276,                    /* SIGN  */
    SIGNBITS = 277,                /* SIGNBITS  */
    LINK = 278,                    /* LINK  */
    UNLINK = 279,                  /* UNLINK  */
    REG = 280,                     /* REG  */
    PC = 281,                      /* PC  */
    CCREG = 282,                   /* CCREG  */
    BYTE_DREG = 283,               /* BYTE_DREG  */
    REG_A_DOUBLE_ZERO = 284,       /* REG_A_DOUBLE_ZERO  */
    REG_A_DOUBLE_ONE = 285,        /* REG_A_DOUBLE_ONE  */
    A_ZERO_DOT_L = 286,            /* A_ZERO_DOT_L  */
    A_ZERO_DOT_H = 287,            /* A_ZERO_DOT_H  */
    A_ONE_DOT_L = 288,             /* A_ONE_DOT_L  */
    A_ONE_DOT_H = 289,             /* A_ONE_DOT_H  */
    HALF_REG = 290,                /* HALF_REG  */
    NOP = 291,                     /* NOP  */
    RTI = 292,                     /* RTI  */
    RTS = 293,                     /* RTS  */
    RTX = 294,                     /* RTX  */
    RTN = 295,                     /* RTN  */
    RTE = 296,                     /* RTE  */
    HLT = 297,                     /* HLT  */
    IDLE = 298,                    /* IDLE  */
    STI = 299,                     /* STI  */
    CLI = 300,                     /* CLI  */
    CSYNC = 301,                   /* CSYNC  */
    SSYNC = 302,                   /* SSYNC  */
    EMUEXCPT = 303,                /* EMUEXCPT  */
    RAISE = 304,                   /* RAISE  */
    EXCPT = 305,                   /* EXCPT  */
    LSETUP = 306,                  /* LSETUP  */
    LOOP = 307,                    /* LOOP  */
    LOOP_BEGIN = 308,              /* LOOP_BEGIN  */
    LOOP_END = 309,                /* LOOP_END  */
    DISALGNEXCPT = 310,            /* DISALGNEXCPT  */
    JUMP = 311,                    /* JUMP  */
    JUMP_DOT_S = 312,              /* JUMP_DOT_S  */
    JUMP_DOT_L = 313,              /* JUMP_DOT_L  */
    CALL = 314,                    /* CALL  */
    ABORT = 315,                   /* ABORT  */
    NOT = 316,                     /* NOT  */
    TILDA = 317,                   /* TILDA  */
    BANG = 318,                    /* BANG  */
    AMPERSAND = 319,               /* AMPERSAND  */
    BAR = 320,                     /* BAR  */
    PERCENT = 321,                 /* PERCENT  */
    CARET = 322,                   /* CARET  */
    BXOR = 323,                    /* BXOR  */
    MINUS = 324,                   /* MINUS  */
    PLUS = 325,                    /* PLUS  */
    STAR = 326,                    /* STAR  */
    SLASH = 327,                   /* SLASH  */
    NEG = 328,                     /* NEG  */
    MIN = 329,                     /* MIN  */
    MAX = 330,                     /* MAX  */
    ABS = 331,                     /* ABS  */
    DOUBLE_BAR = 332,              /* DOUBLE_BAR  */
    _PLUS_BAR_PLUS = 333,          /* _PLUS_BAR_PLUS  */
    _PLUS_BAR_MINUS = 334,         /* _PLUS_BAR_MINUS  */
    _MINUS_BAR_PLUS = 335,         /* _MINUS_BAR_PLUS  */
    _MINUS_BAR_MINUS = 336,        /* _MINUS_BAR_MINUS  */
    _MINUS_MINUS = 337,            /* _MINUS_MINUS  */
    _PLUS_PLUS = 338,              /* _PLUS_PLUS  */
    SHIFT = 339,                   /* SHIFT  */
    LSHIFT = 340,                  /* LSHIFT  */
    ASHIFT = 341,                  /* ASHIFT  */
    BXORSHIFT = 342,               /* BXORSHIFT  */
    _GREATER_GREATER_GREATER_THAN_ASSIGN = 343, /* _GREATER_GREATER_GREATER_THAN_ASSIGN  */
    ROT = 344,                     /* ROT  */
    LESS_LESS = 345,               /* LESS_LESS  */
    GREATER_GREATER = 346,         /* GREATER_GREATER  */
    _GREATER_GREATER_GREATER = 347, /* _GREATER_GREATER_GREATER  */
    _LESS_LESS_ASSIGN = 348,       /* _LESS_LESS_ASSIGN  */
    _GREATER_GREATER_ASSIGN = 349, /* _GREATER_GREATER_ASSIGN  */
    DIVS = 350,                    /* DIVS  */
    DIVQ = 351,                    /* DIVQ  */
    ASSIGN = 352,                  /* ASSIGN  */
    _STAR_ASSIGN = 353,            /* _STAR_ASSIGN  */
    _BAR_ASSIGN = 354,             /* _BAR_ASSIGN  */
    _CARET_ASSIGN = 355,           /* _CARET_ASSIGN  */
    _AMPERSAND_ASSIGN = 356,       /* _AMPERSAND_ASSIGN  */
    _MINUS_ASSIGN = 357,           /* _MINUS_ASSIGN  */
    _PLUS_ASSIGN = 358,            /* _PLUS_ASSIGN  */
    _LESS_THAN_ASSIGN = 359,       /* _LESS_THAN_ASSIGN  */
    _ASSIGN_ASSIGN = 360,          /* _ASSIGN_ASSIGN  */
    GE = 361,                      /* GE  */
    LT = 362,                      /* LT  */
    LE = 363,                      /* LE  */
    GT = 364,                      /* GT  */
    LESS_THAN = 365,               /* LESS_THAN  */
    FLUSHINV = 366,                /* FLUSHINV  */
    FLUSH = 367,                   /* FLUSH  */
    IFLUSH = 368,                  /* IFLUSH  */
    PREFETCH = 369,                /* PREFETCH  */
    PRNT = 370,                    /* PRNT  */
    OUTC = 371,                    /* OUTC  */
    WHATREG = 372,                 /* WHATREG  */
    TESTSET = 373,                 /* TESTSET  */
    ASL = 374,                     /* ASL  */
    ASR = 375,                     /* ASR  */
    B = 376,                       /* B  */
    W = 377,                       /* W  */
    NS = 378,                      /* NS  */
    S = 379,                       /* S  */
    CO = 380,                      /* CO  */
    SCO = 381,                     /* SCO  */
    TH = 382,                      /* TH  */
    TL = 383,                      /* TL  */
    BP = 384,                      /* BP  */
    BREV = 385,                    /* BREV  */
    X = 386,                       /* X  */
    Z = 387,                       /* Z  */
    M = 388,                       /* M  */
    MMOD = 389,                    /* MMOD  */
    R = 390,                       /* R  */
    RND = 391,                     /* RND  */
    RNDL = 392,                    /* RNDL  */
    RNDH = 393,                    /* RNDH  */
    RND12 = 394,                   /* RND12  */
    RND20 = 395,                   /* RND20  */
    V = 396,                       /* V  */
    LO = 397,                      /* LO  */
    HI = 398,                      /* HI  */
    BITTGL = 399,                  /* BITTGL  */
    BITCLR = 400,                  /* BITCLR  */
    BITSET = 401,                  /* BITSET  */
    BITTST = 402,                  /* BITTST  */
    BITMUX = 403,                  /* BITMUX  */
    DBGAL = 404,                   /* DBGAL  */
    DBGAH = 405,                   /* DBGAH  */
    DBGHALT = 406,                 /* DBGHALT  */
    DBG = 407,                     /* DBG  */
    DBGA = 408,                    /* DBGA  */
    DBGCMPLX = 409,                /* DBGCMPLX  */
    IF = 410,                      /* IF  */
    COMMA = 411,                   /* COMMA  */
    BY = 412,                      /* BY  */
    COLON = 413,                   /* COLON  */
    SEMICOLON = 414,               /* SEMICOLON  */
    RPAREN = 415,                  /* RPAREN  */
    LPAREN = 416,                  /* LPAREN  */
    LBRACK = 417,                  /* LBRACK  */
    RBRACK = 418,                  /* RBRACK  */
    STATUS_REG = 419,              /* STATUS_REG  */
    MNOP = 420,                    /* MNOP  */
    SYMBOL = 421,                  /* SYMBOL  */
    NUMBER = 422,                  /* NUMBER  */
    GOT = 423,                     /* GOT  */
    GOT17M4 = 424,                 /* GOT17M4  */
    FUNCDESC_GOT17M4 = 425,        /* FUNCDESC_GOT17M4  */
    AT = 426,                      /* AT  */
    PLTPC = 427                    /* PLTPC  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define BYTEOP16P 258
#define BYTEOP16M 259
#define BYTEOP1P 260
#define BYTEOP2P 261
#define BYTEOP3P 262
#define BYTEUNPACK 263
#define BYTEPACK 264
#define PACK 265
#define SAA 266
#define ALIGN8 267
#define ALIGN16 268
#define ALIGN24 269
#define VIT_MAX 270
#define EXTRACT 271
#define DEPOSIT 272
#define EXPADJ 273
#define SEARCH 274
#define ONES 275
#define SIGN 276
#define SIGNBITS 277
#define LINK 278
#define UNLINK 279
#define REG 280
#define PC 281
#define CCREG 282
#define BYTE_DREG 283
#define REG_A_DOUBLE_ZERO 284
#define REG_A_DOUBLE_ONE 285
#define A_ZERO_DOT_L 286
#define A_ZERO_DOT_H 287
#define A_ONE_DOT_L 288
#define A_ONE_DOT_H 289
#define HALF_REG 290
#define NOP 291
#define RTI 292
#define RTS 293
#define RTX 294
#define RTN 295
#define RTE 296
#define HLT 297
#define IDLE 298
#define STI 299
#define CLI 300
#define CSYNC 301
#define SSYNC 302
#define EMUEXCPT 303
#define RAISE 304
#define EXCPT 305
#define LSETUP 306
#define LOOP 307
#define LOOP_BEGIN 308
#define LOOP_END 309
#define DISALGNEXCPT 310
#define JUMP 311
#define JUMP_DOT_S 312
#define JUMP_DOT_L 313
#define CALL 314
#define ABORT 315
#define NOT 316
#define TILDA 317
#define BANG 318
#define AMPERSAND 319
#define BAR 320
#define PERCENT 321
#define CARET 322
#define BXOR 323
#define MINUS 324
#define PLUS 325
#define STAR 326
#define SLASH 327
#define NEG 328
#define MIN 329
#define MAX 330
#define ABS 331
#define DOUBLE_BAR 332
#define _PLUS_BAR_PLUS 333
#define _PLUS_BAR_MINUS 334
#define _MINUS_BAR_PLUS 335
#define _MINUS_BAR_MINUS 336
#define _MINUS_MINUS 337
#define _PLUS_PLUS 338
#define SHIFT 339
#define LSHIFT 340
#define ASHIFT 341
#define BXORSHIFT 342
#define _GREATER_GREATER_GREATER_THAN_ASSIGN 343
#define ROT 344
#define LESS_LESS 345
#define GREATER_GREATER 346
#define _GREATER_GREATER_GREATER 347
#define _LESS_LESS_ASSIGN 348
#define _GREATER_GREATER_ASSIGN 349
#define DIVS 350
#define DIVQ 351
#define ASSIGN 352
#define _STAR_ASSIGN 353
#define _BAR_ASSIGN 354
#define _CARET_ASSIGN 355
#define _AMPERSAND_ASSIGN 356
#define _MINUS_ASSIGN 357
#define _PLUS_ASSIGN 358
#define _LESS_THAN_ASSIGN 359
#define _ASSIGN_ASSIGN 360
#define GE 361
#define LT 362
#define LE 363
#define GT 364
#define LESS_THAN 365
#define FLUSHINV 366
#define FLUSH 367
#define IFLUSH 368
#define PREFETCH 369
#define PRNT 370
#define OUTC 371
#define WHATREG 372
#define TESTSET 373
#define ASL 374
#define ASR 375
#define B 376
#define W 377
#define NS 378
#define S 379
#define CO 380
#define SCO 381
#define TH 382
#define TL 383
#define BP 384
#define BREV 385
#define X 386
#define Z 387
#define M 388
#define MMOD 389
#define R 390
#define RND 391
#define RNDL 392
#define RNDH 393
#define RND12 394
#define RND20 395
#define V 396
#define LO 397
#define HI 398
#define BITTGL 399
#define BITCLR 400
#define BITSET 401
#define BITTST 402
#define BITMUX 403
#define DBGAL 404
#define DBGAH 405
#define DBGHALT 406
#define DBG 407
#define DBGA 408
#define DBGCMPLX 409
#define IF 410
#define COMMA 411
#define BY 412
#define COLON 413
#define SEMICOLON 414
#define RPAREN 415
#define LPAREN 416
#define LBRACK 417
#define RBRACK 418
#define STATUS_REG 419
#define MNOP 420
#define SYMBOL 421
#define NUMBER 422
#define GOT 423
#define GOT17M4 424
#define FUNCDESC_GOT17M4 425
#define AT 426
#define PLTPC 427

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 447 "./config/bfin-parse.y"

  INSTR_T instr;
  Expr_Node *expr;
  SYMBOL_T symbol;
  long value;
  Register reg;
  Macfunc macfunc;
  struct { int r0; int s0; int x0; int aop; } modcodes;
  struct { int r0; } r0;
  Opt_mode mod;

#line 907 "config/bfin-parse.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_CONFIG_BFIN_PARSE_H_INCLUDED  */
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_BYTEOP16P = 3,                  /* BYTEOP16P  */
  YYSYMBOL_BYTEOP16M = 4,                  /* BYTEOP16M  */
  YYSYMBOL_BYTEOP1P = 5,                   /* BYTEOP1P  */
  YYSYMBOL_BYTEOP2P = 6,                   /* BYTEOP2P  */
  YYSYMBOL_BYTEOP3P = 7,                   /* BYTEOP3P  */
  YYSYMBOL_BYTEUNPACK = 8,                 /* BYTEUNPACK  */
  YYSYMBOL_BYTEPACK = 9,                   /* BYTEPACK  */
  YYSYMBOL_PACK = 10,                      /* PACK  */
  YYSYMBOL_SAA = 11,                       /* SAA  */
  YYSYMBOL_ALIGN8 = 12,                    /* ALIGN8  */
  YYSYMBOL_ALIGN16 = 13,                   /* ALIGN16  */
  YYSYMBOL_ALIGN24 = 14,                   /* ALIGN24  */
  YYSYMBOL_VIT_MAX = 15,                   /* VIT_MAX  */
  YYSYMBOL_EXTRACT = 16,                   /* EXTRACT  */
  YYSYMBOL_DEPOSIT = 17,                   /* DEPOSIT  */
  YYSYMBOL_EXPADJ = 18,                    /* EXPADJ  */
  YYSYMBOL_SEARCH = 19,                    /* SEARCH  */
  YYSYMBOL_ONES = 20,                      /* ONES  */
  YYSYMBOL_SIGN = 21,                      /* SIGN  */
  YYSYMBOL_SIGNBITS = 22,                  /* SIGNBITS  */
  YYSYMBOL_LINK = 23,                      /* LINK  */
  YYSYMBOL_UNLINK = 24,                    /* UNLINK  */
  YYSYMBOL_REG = 25,                       /* REG  */
  YYSYMBOL_PC = 26,                        /* PC  */
  YYSYMBOL_CCREG = 27,                     /* CCREG  */
  YYSYMBOL_BYTE_DREG = 28,                 /* BYTE_DREG  */
  YYSYMBOL_REG_A_DOUBLE_ZERO = 29,         /* REG_A_DOUBLE_ZERO  */
  YYSYMBOL_REG_A_DOUBLE_ONE = 30,          /* REG_A_DOUBLE_ONE  */
  YYSYMBOL_A_ZERO_DOT_L = 31,              /* A_ZERO_DOT_L  */
  YYSYMBOL_A_ZERO_DOT_H = 32,              /* A_ZERO_DOT_H  */
  YYSYMBOL_A_ONE_DOT_L = 33,               /* A_ONE_DOT_L  */
  YYSYMBOL_A_ONE_DOT_H = 34,               /* A_ONE_DOT_H  */
  YYSYMBOL_HALF_REG = 35,                  /* HALF_REG  */
  YYSYMBOL_NOP = 36,                       /* NOP  */
  YYSYMBOL_RTI = 37,                       /* RTI  */
  YYSYMBOL_RTS = 38,                       /* RTS  */
  YYSYMBOL_RTX = 39,                       /* RTX  */
  YYSYMBOL_RTN = 40,                       /* RTN  */
  YYSYMBOL_RTE = 41,                       /* RTE  */
  YYSYMBOL_HLT = 42,                       /* HLT  */
  YYSYMBOL_IDLE = 43,                      /* IDLE  */
  YYSYMBOL_STI = 44,                       /* STI  */
  YYSYMBOL_CLI = 45,                       /* CLI  */
  YYSYMBOL_CSYNC = 46,                     /* CSYNC  */
  YYSYMBOL_SSYNC = 47,                     /* SSYNC  */
  YYSYMBOL_EMUEXCPT = 48,                  /* EMUEXCPT  */
  YYSYMBOL_RAISE = 49,                     /* RAISE  */
  YYSYMBOL_EXCPT = 50,                     /* EXCPT  */
  YYSYMBOL_LSETUP = 51,                    /* LSETUP  */
  YYSYMBOL_LOOP = 52,                      /* LOOP  */
  YYSYMBOL_LOOP_BEGIN = 53,                /* LOOP_BEGIN  */
  YYSYMBOL_LOOP_END = 54,                  /* LOOP_END  */
  YYSYMBOL_DISALGNEXCPT = 55,              /* DISALGNEXCPT  */
  YYSYMBOL_JUMP = 56,                      /* JUMP  */
  YYSYMBOL_JUMP_DOT_S = 57,                /* JUMP_DOT_S  */
  YYSYMBOL_JUMP_DOT_L = 58,                /* JUMP_DOT_L  */
  YYSYMBOL_CALL = 59,                      /* CALL  */
  YYSYMBOL_ABORT = 60,                     /* ABORT  */
  YYSYMBOL_NOT = 61,                       /* NOT  */
  YYSYMBOL_TILDA = 62,                     /* TILDA  */
  YYSYMBOL_BANG = 63,                      /* BANG  */
  YYSYMBOL_AMPERSAND = 64,                 /* AMPERSAND  */
  YYSYMBOL_BAR = 65,                       /* BAR  */
  YYSYMBOL_PERCENT = 66,                   /* PERCENT  */
  YYSYMBOL_CARET = 67,                     /* CARET  */
  YYSYMBOL_BXOR = 68,                      /* BXOR  */
  YYSYMBOL_MINUS = 69,                     /* MINUS  */
  YYSYMBOL_PLUS = 70,                      /* PLUS  */
  YYSYMBOL_STAR = 71,                      /* STAR  */
  YYSYMBOL_SLASH = 72,                     /* SLASH  */
  YYSYMBOL_NEG = 73,                       /* NEG  */
  YYSYMBOL_MIN = 74,                       /* MIN  */
  YYSYMBOL_MAX = 75,                       /* MAX  */
  YYSYMBOL_ABS = 76,                       /* ABS  */
  YYSYMBOL_DOUBLE_BAR = 77,                /* DOUBLE_BAR  */
  YYSYMBOL__PLUS_BAR_PLUS = 78,            /* _PLUS_BAR_PLUS  */
  YYSYMBOL__PLUS_BAR_MINUS = 79,           /* _PLUS_BAR_MINUS  */
  YYSYMBOL__MINUS_BAR_PLUS = 80,           /* _MINUS_BAR_PLUS  */
  YYSYMBOL__MINUS_BAR_MINUS = 81,          /* _MINUS_BAR_MINUS  */
  YYSYMBOL__MINUS_MINUS = 82,              /* _MINUS_MINUS  */
  YYSYMBOL__PLUS_PLUS = 83,                /* _PLUS_PLUS  */
  YYSYMBOL_SHIFT = 84,                     /* SHIFT  */
  YYSYMBOL_LSHIFT = 85,                    /* LSHIFT  */
  YYSYMBOL_ASHIFT = 86,                    /* ASHIFT  */
  YYSYMBOL_BXORSHIFT = 87,                 /* BXORSHIFT  */
  YYSYMBOL__GREATER_GREATER_GREATER_THAN_ASSIGN = 88, /* _GREATER_GREATER_GREATER_THAN_ASSIGN  */
  YYSYMBOL_ROT = 89,                       /* ROT  */
  YYSYMBOL_LESS_LESS = 90,                 /* LESS_LESS  */
  YYSYMBOL_GREATER_GREATER = 91,           /* GREATER_GREATER  */
  YYSYMBOL__GREATER_GREATER_GREATER = 92,  /* _GREATER_GREATER_GREATER  */
  YYSYMBOL__LESS_LESS_ASSIGN = 93,         /* _LESS_LESS_ASSIGN  */
  YYSYMBOL__GREATER_GREATER_ASSIGN = 94,   /* _GREATER_GREATER_ASSIGN  */
  YYSYMBOL_DIVS = 95,                      /* DIVS  */
  YYSYMBOL_DIVQ = 96,                      /* DIVQ  */
  YYSYMBOL_ASSIGN = 97,                    /* ASSIGN  */
  YYSYMBOL__STAR_ASSIGN = 98,              /* _STAR_ASSIGN  */
  YYSYMBOL__BAR_ASSIGN = 99,               /* _BAR_ASSIGN  */
  YYSYMBOL__CARET_ASSIGN = 100,            /* _CARET_ASSIGN  */
  YYSYMBOL__AMPERSAND_ASSIGN = 101,        /* _AMPERSAND_ASSIGN  */
  YYSYMBOL__MINUS_ASSIGN = 102,            /* _MINUS_ASSIGN  */
  YYSYMBOL__PLUS_ASSIGN = 103,             /* _PLUS_ASSIGN  */
  YYSYMBOL__LESS_THAN_ASSIGN = 104,        /* _LESS_THAN_ASSIGN  */
  YYSYMBOL__ASSIGN_ASSIGN = 105,           /* _ASSIGN_ASSIGN  */
  YYSYMBOL_GE = 106,                       /* GE  */
  YYSYMBOL_LT = 107,                       /* LT  */
  YYSYMBOL_LE = 108,                       /* LE  */
  YYSYMBOL_GT = 109,                       /* GT  */
  YYSYMBOL_LESS_THAN = 110,                /* LESS_THAN  */
  YYSYMBOL_FLUSHINV = 111,                 /* FLUSHINV  */
  YYSYMBOL_FLUSH = 112,                    /* FLUSH  */
  YYSYMBOL_IFLUSH = 113,                   /* IFLUSH  */
  YYSYMBOL_PREFETCH = 114,                 /* PREFETCH  */
  YYSYMBOL_PRNT = 115,                     /* PRNT  */
  YYSYMBOL_OUTC = 116,                     /* OUTC  */
  YYSYMBOL_WHATREG = 117,                  /* WHATREG  */
  YYSYMBOL_TESTSET = 118,                  /* TESTSET  */
  YYSYMBOL_ASL = 119,                      /* ASL  */
  YYSYMBOL_ASR = 120,                      /* ASR  */
  YYSYMBOL_B = 121,                        /* B  */
  YYSYMBOL_W = 122,                        /* W  */
  YYSYMBOL_NS = 123,                       /* NS  */
  YYSYMBOL_S = 124,                        /* S  */
  YYSYMBOL_CO = 125,                       /* CO  */
  YYSYMBOL_SCO = 126,                      /* SCO  */
  YYSYMBOL_TH = 127,                       /* TH  */
  YYSYMBOL_TL = 128,                       /* TL  */
  YYSYMBOL_BP = 129,                       /* BP  */
  YYSYMBOL_BREV = 130,                     /* BREV  */
  YYSYMBOL_X = 131,                        /* X  */
  YYSYMBOL_Z = 132,                        /* Z  */
  YYSYMBOL_M = 133,                        /* M  */
  YYSYMBOL_MMOD = 134,                     /* MMOD  */
  YYSYMBOL_R = 135,                        /* R  */
  YYSYMBOL_RND = 136,                      /* RND  */
  YYSYMBOL_RNDL = 137,                     /* RNDL  */
  YYSYMBOL_RNDH = 138,                     /* RNDH  */
  YYSYMBOL_RND12 = 139,                    /* RND12  */
  YYSYMBOL_RND20 = 140,                    /* RND20  */
  YYSYMBOL_V = 141,                        /* V  */
  YYSYMBOL_LO = 142,                       /* LO  */
  YYSYMBOL_HI = 143,                       /* HI  */
  YYSYMBOL_BITTGL = 144,                   /* BITTGL  */
  YYSYMBOL_BITCLR = 145,                   /* BITCLR  */
  YYSYMBOL_BITSET = 146,                   /* BITSET  */
  YYSYMBOL_BITTST = 147,                   /* BITTST  */
  YYSYMBOL_BITMUX = 148,                   /* BITMUX  */
  YYSYMBOL_DBGAL = 149,                    /* DBGAL  */
  YYSYMBOL_DBGAH = 150,                    /* DBGAH  */
  YYSYMBOL_DBGHALT = 151,                  /* DBGHALT  */
  YYSYMBOL_DBG = 152,                      /* DBG  */
  YYSYMBOL_DBGA = 153,                     /* DBGA  */
  YYSYMBOL_DBGCMPLX = 154,                 /* DBGCMPLX  */
  YYSYMBOL_IF = 155,                       /* IF  */
  YYSYMBOL_COMMA = 156,                    /* COMMA  */
  YYSYMBOL_BY = 157,                       /* BY  */
  YYSYMBOL_COLON = 158,                    /* COLON  */
  YYSYMBOL_SEMICOLON = 159,                /* SEMICOLON  */
  YYSYMBOL_RPAREN = 160,                   /* RPAREN  */
  YYSYMBOL_LPAREN = 161,                   /* LPAREN  */
  YYSYMBOL_LBRACK = 162,                   /* LBRACK  */
  YYSYMBOL_RBRACK = 163,                   /* RBRACK  */
  YYSYMBOL_STATUS_REG = 164,               /* STATUS_REG  */
  YYSYMBOL_MNOP = 165,                     /* MNOP  */
  YYSYMBOL_SYMBOL = 166,                   /* SYMBOL  */
  YYSYMBOL_NUMBER = 167,                   /* NUMBER  */
  YYSYMBOL_GOT = 168,                      /* GOT  */
  YYSYMBOL_GOT17M4 = 169,                  /* GOT17M4  */
  YYSYMBOL_FUNCDESC_GOT17M4 = 170,         /* FUNCDESC_GOT17M4  */
  YYSYMBOL_AT = 171,                       /* AT  */
  YYSYMBOL_PLTPC = 172,                    /* PLTPC  */
  YYSYMBOL_YYACCEPT = 173,                 /* $accept  */
  YYSYMBOL_statement = 174,                /* statement  */
  YYSYMBOL_asm = 175,                      /* asm  */
  YYSYMBOL_asm_1 = 176,                    /* asm_1  */
  YYSYMBOL_REG_A = 177,                    /* REG_A  */
  YYSYMBOL_opt_mode = 178,                 /* opt_mode  */
  YYSYMBOL_asr_asl = 179,                  /* asr_asl  */
  YYSYMBOL_sco = 180,                      /* sco  */
  YYSYMBOL_asr_asl_0 = 181,                /* asr_asl_0  */
  YYSYMBOL_amod0 = 182,                    /* amod0  */
  YYSYMBOL_amod1 = 183,                    /* amod1  */
  YYSYMBOL_amod2 = 184,                    /* amod2  */
  YYSYMBOL_xpmod = 185,                    /* xpmod  */
  YYSYMBOL_xpmod1 = 186,                   /* xpmod1  */
  YYSYMBOL_vsmod = 187,                    /* vsmod  */
  YYSYMBOL_vmod = 188,                     /* vmod  */
  YYSYMBOL_smod = 189,                     /* smod  */
  YYSYMBOL_searchmod = 190,                /* searchmod  */
  YYSYMBOL_aligndir = 191,                 /* aligndir  */
  YYSYMBOL_byteop_mod = 192,               /* byteop_mod  */
  YYSYMBOL_c_align = 193,                  /* c_align  */
  YYSYMBOL_w32_or_nothing = 194,           /* w32_or_nothing  */
  YYSYMBOL_iu_or_nothing = 195,            /* iu_or_nothing  */
  YYSYMBOL_reg_with_predec = 196,          /* reg_with_predec  */
  YYSYMBOL_reg_with_postinc = 197,         /* reg_with_postinc  */
  YYSYMBOL_min_max = 198,                  /* min_max  */
  YYSYMBOL_op_bar_op = 199,                /* op_bar_op  */
  YYSYMBOL_plus_minus = 200,               /* plus_minus  */
  YYSYMBOL_rnd_op = 201,                   /* rnd_op  */
  YYSYMBOL_b3_op = 202,                    /* b3_op  */
  YYSYMBOL_post_op = 203,                  /* post_op  */
  YYSYMBOL_a_assign = 204,                 /* a_assign  */
  YYSYMBOL_a_minusassign = 205,            /* a_minusassign  */
  YYSYMBOL_a_plusassign = 206,             /* a_plusassign  */
  YYSYMBOL_assign_macfunc = 207,           /* assign_macfunc  */
  YYSYMBOL_a_macfunc = 208,                /* a_macfunc  */
  YYSYMBOL_multiply_halfregs = 209,        /* multiply_halfregs  */
  YYSYMBOL_cc_op = 210,                    /* cc_op  */
  YYSYMBOL_ccstat = 211,                   /* ccstat  */
  YYSYMBOL_symbol = 212,                   /* symbol  */
  YYSYMBOL_any_gotrel = 213,               /* any_gotrel  */
  YYSYMBOL_got = 214,                      /* got  */
  YYSYMBOL_got_or_expr = 215,              /* got_or_expr  */
  YYSYMBOL_pltpc = 216,                    /* pltpc  */
  YYSYMBOL_eterm = 217,                    /* eterm  */
  YYSYMBOL_expr = 218,                     /* expr  */
  YYSYMBOL_expr_1 = 219                    /* expr_1  */
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

#if !defined yyoverflow

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
#endif /* !defined yyoverflow */

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
#define YYFINAL  155
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1292

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  173
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  47
/* YYNRULES -- Number of rules.  */
#define YYNRULES  354
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  1021

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   427


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_uint8 yytranslate[] =
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
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   648,   648,   649,   661,   663,   696,   723,   734,   738,
     776,   796,   801,   811,   821,   826,   831,   849,   867,   881,
     894,   910,   932,   950,   975,   997,  1002,  1012,  1023,  1034,
    1048,  1063,  1079,  1095,  1106,  1120,  1146,  1164,  1169,  1175,
    1187,  1198,  1209,  1220,  1231,  1242,  1253,  1279,  1293,  1303,
    1348,  1367,  1378,  1389,  1400,  1411,  1422,  1438,  1455,  1471,
    1482,  1493,  1526,  1537,  1550,  1561,  1600,  1610,  1620,  1640,
    1650,  1660,  1671,  1685,  1696,  1709,  1719,  1731,  1746,  1757,
    1763,  1785,  1796,  1807,  1815,  1841,  1871,  1900,  1931,  1945,
    1956,  1970,  2004,  2022,  2047,  2059,  2077,  2088,  2099,  2110,
    2123,  2134,  2145,  2156,  2167,  2178,  2211,  2221,  2234,  2254,
    2265,  2276,  2289,  2302,  2313,  2324,  2335,  2346,  2356,  2367,
    2378,  2390,  2401,  2412,  2426,  2439,  2451,  2463,  2474,  2485,
    2496,  2508,  2520,  2531,  2542,  2553,  2563,  2569,  2575,  2581,
    2587,  2593,  2599,  2605,  2611,  2617,  2623,  2634,  2645,  2656,
    2667,  2678,  2689,  2700,  2706,  2720,  2731,  2742,  2753,  2764,
    2774,  2787,  2795,  2803,  2827,  2838,  2849,  2860,  2871,  2882,
    2894,  2907,  2916,  2927,  2938,  2950,  2961,  2972,  2983,  2997,
    3009,  3035,  3065,  3076,  3101,  3138,  3166,  3191,  3202,  3213,
    3224,  3250,  3269,  3283,  3307,  3319,  3338,  3384,  3421,  3437,
    3456,  3470,  3489,  3505,  3513,  3522,  3533,  3545,  3559,  3567,
    3577,  3589,  3600,  3610,  3621,  3632,  3638,  3643,  3648,  3654,
    3662,  3668,  3674,  3680,  3686,  3692,  3700,  3714,  3718,  3728,
    3732,  3737,  3742,  3747,  3754,  3758,  3765,  3769,  3774,  3779,
    3787,  3791,  3798,  3802,  3810,  3815,  3821,  3830,  3835,  3841,
    3847,  3853,  3862,  3865,  3869,  3876,  3879,  3883,  3890,  3895,
    3901,  3907,  3913,  3918,  3926,  3929,  3936,  3939,  3946,  3950,
    3954,  3958,  3965,  3968,  3975,  3980,  3987,  3994,  4006,  4010,
    4014,  4021,  4024,  4034,  4037,  4046,  4052,  4061,  4065,  4072,
    4076,  4080,  4084,  4091,  4095,  4102,  4110,  4118,  4126,  4134,
    4141,  4148,  4156,  4166,  4171,  4176,  4181,  4189,  4192,  4196,
    4205,  4212,  4219,  4226,  4241,  4247,  4260,  4273,  4291,  4298,
    4305,  4315,  4328,  4332,  4336,  4340,  4347,  4353,  4359,  4365,
    4375,  4384,  4386,  4388,  4392,  4400,  4404,  4411,  4417,  4423,
    4427,  4431,  4435,  4441,  4447,  4451,  4455,  4459,  4463,  4467,
    4471,  4475,  4479,  4483,  4487
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "BYTEOP16P",
  "BYTEOP16M", "BYTEOP1P", "BYTEOP2P", "BYTEOP3P", "BYTEUNPACK",
  "BYTEPACK", "PACK", "SAA", "ALIGN8", "ALIGN16", "ALIGN24", "VIT_MAX",
  "EXTRACT", "DEPOSIT", "EXPADJ", "SEARCH", "ONES", "SIGN", "SIGNBITS",
  "LINK", "UNLINK", "REG", "PC", "CCREG", "BYTE_DREG", "REG_A_DOUBLE_ZERO",
  "REG_A_DOUBLE_ONE", "A_ZERO_DOT_L", "A_ZERO_DOT_H", "A_ONE_DOT_L",
  "A_ONE_DOT_H", "HALF_REG", "NOP", "RTI", "RTS", "RTX", "RTN", "RTE",
  "HLT", "IDLE", "STI", "CLI", "CSYNC", "SSYNC", "EMUEXCPT", "RAISE",
  "EXCPT", "LSETUP", "LOOP", "LOOP_BEGIN", "LOOP_END", "DISALGNEXCPT",
  "JUMP", "JUMP_DOT_S", "JUMP_DOT_L", "CALL", "ABORT", "NOT", "TILDA",
  "BANG", "AMPERSAND", "BAR", "PERCENT", "CARET", "BXOR", "MINUS", "PLUS",
  "STAR", "SLASH", "NEG", "MIN", "MAX", "ABS", "DOUBLE_BAR",
  "_PLUS_BAR_PLUS", "_PLUS_BAR_MINUS", "_MINUS_BAR_PLUS",
  "_MINUS_BAR_MINUS", "_MINUS_MINUS", "_PLUS_PLUS", "SHIFT", "LSHIFT",
  "ASHIFT", "BXORSHIFT", "_GREATER_GREATER_GREATER_THAN_ASSIGN", "ROT",
  "LESS_LESS", "GREATER_GREATER", "_GREATER_GREATER_GREATER",
  "_LESS_LESS_ASSIGN", "_GREATER_GREATER_ASSIGN", "DIVS", "DIVQ", "ASSIGN",
  "_STAR_ASSIGN", "_BAR_ASSIGN", "_CARET_ASSIGN", "_AMPERSAND_ASSIGN",
  "_MINUS_ASSIGN", "_PLUS_ASSIGN", "_LESS_THAN_ASSIGN", "_ASSIGN_ASSIGN",
  "GE", "LT", "LE", "GT", "LESS_THAN", "FLUSHINV", "FLUSH", "IFLUSH",
  "PREFETCH", "PRNT", "OUTC", "WHATREG", "TESTSET", "ASL", "ASR", "B", "W",
  "NS", "S", "CO", "SCO", "TH", "TL", "BP", "BREV", "X", "Z", "M", "MMOD",
  "R", "RND", "RNDL", "RNDH", "RND12", "RND20", "V", "LO", "HI", "BITTGL",
  "BITCLR", "BITSET", "BITTST", "BITMUX", "DBGAL", "DBGAH", "DBGHALT",
  "DBG", "DBGA", "DBGCMPLX", "IF", "COMMA", "BY", "COLON", "SEMICOLON",
  "RPAREN", "LPAREN", "LBRACK", "RBRACK", "STATUS_REG", "MNOP", "SYMBOL",
  "NUMBER", "GOT", "GOT17M4", "FUNCDESC_GOT17M4", "AT", "PLTPC", "$accept",
  "statement", "asm", "asm_1", "REG_A", "opt_mode", "asr_asl", "sco",
  "asr_asl_0", "amod0", "amod1", "amod2", "xpmod", "xpmod1", "vsmod",
  "vmod", "smod", "searchmod", "aligndir", "byteop_mod", "c_align",
  "w32_or_nothing", "iu_or_nothing", "reg_with_predec", "reg_with_postinc",
  "min_max", "op_bar_op", "plus_minus", "rnd_op", "b3_op", "post_op",
  "a_assign", "a_minusassign", "a_plusassign", "assign_macfunc",
  "a_macfunc", "multiply_halfregs", "cc_op", "ccstat", "symbol",
  "any_gotrel", "got", "got_or_expr", "pltpc", "eterm", "expr", "expr_1", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-858)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-214)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     861,  -858,  -116,   -18,  -858,   377,   503,  -858,  -858,   -10,
      -3,    19,    45,    63,  -858,  -858,  -858,  -858,  -858,  -858,
    -858,  -858,    54,   192,  -858,  -858,  -858,   -18,   -18,    96,
     -18,   324,   380,  -858,   392,   -18,   -18,   425,  -858,   104,
     134,    57,   139,   141,   155,    88,   158,   163,   166,   508,
     190,   202,   246,   251,   259,   276,  -858,   340,   287,   289,
      20,   328,    72,   508,  -858,   462,  -858,   -21,   288,   370,
      77,   252,   442,   345,  -858,  -858,   515,   -18,   -18,   -18,
    -858,  -858,  -858,  -858,  -858,   606,   130,   143,   191,   495,
     527,   214,   231,    25,  -858,  -858,  -858,    47,   498,   513,
     519,   525,   105,  -858,  -858,  -858,  -858,   -18,   560,   -14,
    -858,    27,  -858,    59,  -858,  -858,   419,  -858,  -858,    67,
    -858,  -858,   568,   581,   588,  -858,   602,  -858,   615,  -858,
     623,  -858,  -858,  -858,   655,   656,   667,  -858,   666,   670,
     683,   685,   691,   692,   700,  -858,  -858,   693,   705,    33,
     704,   -55,    26,   707,   708,  -858,  1006,  -858,  -858,  -858,
      23,     9,  -858,   665,   201,    23,    23,    23,   576,    23,
     145,   -18,  -858,  -858,   577,  -858,  -858,   245,   583,   582,
    -858,  -858,   559,   -18,   -18,   -18,   -18,   -18,   -18,   -18,
     -18,   -18,   -18,  -858,  -858,  -858,  -858,  -858,  -858,   580,
     584,   585,   586,   587,  -858,  -858,  -858,   589,   591,   593,
     574,  -858,   594,   674,   -35,   264,   267,  -858,  -858,   724,
     728,   731,   732,   733,   597,   598,    48,   736,   694,   604,
     605,   345,   607,  -858,  -858,  -858,   608,  -858,   272,     0,
     609,   348,  -858,  -858,  -858,  -858,  -858,  -858,   610,   611,
     738,   359,   -28,   676,   132,   739,   740,   614,   201,  -858,
     345,  -858,   621,   681,   619,   697,   612,   620,   711,   626,
     627,   -26,    -2,    41,    58,   628,   229,   238,  -858,   630,
     631,   633,   634,   635,   636,   637,   638,   698,   -18,    42,
     769,   -18,  -858,  -858,  -858,   771,   -18,   639,   640,  -858,
      38,   577,  -858,   772,   764,   644,   645,   647,   648,    23,
     649,   -18,   -18,   -18,   684,  -858,   673,  -858,    50,   185,
     410,   -18,  -858,   652,   465,  -858,   643,   472,   472,  -858,
    -858,   506,   506,   784,   785,   786,   787,   778,   789,   790,
     791,   793,   794,   795,   660,  -858,  -858,  -858,  -858,   -18,
     -18,   -18,   797,   798,   295,  -858,   792,  -858,  -858,   663,
     664,   671,   672,   675,   677,   802,   805,   761,   448,   442,
     442,   252,   678,   261,    23,   808,   810,   680,   357,  -858,
     709,   277,   280,   302,  -858,   679,   812,    23,    23,    23,
     816,   227,  -858,  -858,  -858,  -858,   706,   818,   164,   -18,
     -18,   -18,   823,   813,   688,   690,   824,   252,   695,   696,
     -18,   825,  -858,   826,  -858,  -858,   828,   829,   831,   710,
    -858,  -858,  -858,  -858,  -858,  -858,   -18,   712,   833,   -18,
     716,   -18,   -18,   -18,   838,   -18,   -18,   -18,  -858,   842,
     713,   773,   -18,   720,   299,   719,   726,   774,  -858,  1006,
    -858,  -858,   725,  -858,    23,    23,   834,   852,   766,   352,
    -858,  -858,  -858,   763,   765,   734,  -858,   796,  -858,   827,
     835,   345,   770,   775,   776,   777,   780,   781,   782,   783,
     788,  -858,  -858,  -858,   903,   663,   664,   663,   176,   240,
     799,   800,   806,   117,  -858,   801,  -858,   895,   896,   905,
     360,   229,   265,   916,  -858,   803,  -858,   918,   -18,   804,
     809,   814,   815,   920,   820,   821,   830,   807,   807,  -858,
    -858,   807,   807,   921,   822,  -858,  -858,  -858,   832,   837,
     839,   836,   840,   843,   847,   857,  -858,   857,   858,   859,
     914,   915,   278,   864,  -858,   917,   867,   860,   868,   872,
     874,   907,  -858,   906,   856,   908,   909,   869,   910,   913,
     919,   841,   922,   923,   924,  -858,   855,   929,   925,   879,
     930,   888,   889,   890,   933,   844,   -18,   891,   892,   893,
    -858,  -858,    23,  -858,  -858,   865,  -858,   927,   928,     8,
      10,  -858,   939,   -18,   -18,   -18,   964,   956,   969,   963,
     974,   978,  -858,  -858,  -858,   975,   372,  -858,  1049,   439,
    -858,  -858,  -858,  1050,   931,   304,   316,   932,  -858,   664,
     663,  -858,  -858,   -18,   926,  1051,   -18,   934,   935,  -858,
     936,   937,  -858,   940,  -858,  -858,  1052,  1053,  1055,   984,
    -858,  -858,  -858,   951,  -858,  -858,  -858,  -858,   938,   -18,
     943,  1058,  1061,  -858,   434,    23,    23,   966,  -858,  -858,
    1063,  -858,  -858,   857,  1075,   942,  -858,  1009,  1082,   -18,
    -858,  -858,  -858,  -858,  1012,  1085,  1014,  1015,   315,  -858,
    -858,  -858,    23,  -858,  -858,  -858,   953,  -858,   985,   271,
     957,   954,  1096,  1098,  -858,  -858,   326,    23,    23,   970,
      23,  -858,  -858,    23,  -858,    23,   967,   973,   976,   977,
     971,   979,   980,   981,   982,   -18,  1033,  -858,  -858,  -858,
     983,  1037,   986,   988,  1038,  -858,   995,  -858,  1013,  -858,
    -858,  -858,  -858,   990,   594,   999,  1000,   594,  1048,  -858,
     535,  -858,  1047,   989,  1004,   442,  1005,   -18,  1016,   501,
    -858,  1017,  1018,  1019,  1020,  1010,  1025,  1022,  1023,  -858,
    1011,  -858,   442,  1072,  -858,  1120,  -858,  1137,  1148,  -858,
    -858,  1024,  -858,  1026,  1027,  1028,  1150,  1160,   -18,  1162,
    -858,  -858,  -858,  1165,  -858,  -858,  -858,  1164,    23,   -18,
    1167,  1168,  1169,  -858,  -858,   943,   594,  1034,  1036,  1172,
    -858,  1173,  -858,  -858,  1170,  1039,  1040,   594,  -858,   594,
     594,  -858,   -18,  -858,  -858,  -858,  -858,    23,  -858,   664,
     345,  -858,  1042,  -858,  1043,  1044,   664,  -858,  -858,  -858,
     394,  1180,  -858,  1135,  -858,   345,  1182,  -858,  -858,  -858,
     943,  -858,  1183,  1184,  1054,  1056,  1059,  1127,  1057,  1060,
    1062,  1064,  1067,  1068,  1069,  -858,  -858,  1080,  -858,   436,
     565,  1143,  -858,  -858,  -858,  -858,  -858,  -858,  1145,  -858,
    -858,  -858,  -858,  -858,  -858,  1070,  1065,  1071,  1181,  -858,
    1131,  -858,  1074,  1076,   -18,   592,  1121,   -18,  -858,  1104,
    1077,   -18,   -18,   -18,  1073,  1204,  1210,  1205,    23,  -858,
    1209,  -858,  1171,   -18,   -18,   -18,  1077,  -858,  -858,  -858,
    -858,  1079,   954,  1083,  1084,  1105,  -858,  1086,  1087,  1088,
    -858,  1081,   859,  -858,   859,  1089,  1224,  -858,  1094,  1095,
    -858,  -858,  -858,  -858,  -858,  1092,  1093,  1097,  1099,   463,
    -858,  -858,  -858,  -858,  1100,  1228,  1230,  -858,   511,  -858,
     140,  -858,   516,  -858,  -858,  -858,   343,   405,  1221,  1101,
    1106,   476,   477,   491,   507,   523,   526,   531,   534,   549,
    -858,   372,  -858,  1102,   -18,   -18,  1122,  -858,  1129,  -858,
    1130,  -858,  1132,  -858,  1133,  -858,  1134,  -858,  1136,  -858,
    1138,  -858,  1110,  1112,  1195,  1114,  1115,  1116,  1117,  1118,
    1119,  1123,  1124,  1125,  1126,  -858,  -858,  1245,  1077,  1077,
    -858,  -858,  -858,  -858,  -858,  -858,  -858,  -858,  -858,  -858,
    -858
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       0,     7,     0,     0,   204,     0,     0,   227,   228,     0,
       0,     0,     0,     0,   136,   138,   137,   139,   140,   141,
     221,   142,     0,     0,   143,   144,   145,     0,     0,     0,
       0,     0,     0,    11,     0,     0,     0,     0,   215,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   220,   216,     0,     0,
       0,     0,     0,     0,     8,     0,     3,     0,     0,     0,
       0,     0,     0,   229,   314,    79,     0,     0,     0,     0,
     330,   338,   339,   354,   203,   343,     0,     0,     0,     0,
       0,     0,     0,   322,   323,   325,   324,     0,     0,     0,
       0,     0,     0,   147,   146,   152,   153,     0,     0,   338,
     212,   338,   214,     0,   155,   156,   339,   158,   157,     0,
     160,   159,     0,     0,     0,   174,     0,   172,     0,   176,
       0,   178,   226,   225,     0,     0,     0,   322,     0,     0,
       0,     0,     0,     0,     0,   218,   217,     0,     0,     0,
       0,     0,   307,     0,     0,     1,     0,     4,   310,   311,
     312,     0,    45,     0,     0,     0,     0,     0,     0,     0,
      44,     0,   318,    48,   281,   320,   319,     0,     9,     0,
     341,   342,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   167,   170,   168,   169,   165,   166,     0,
       0,     0,     0,     0,   278,   279,   280,     0,     0,     0,
      80,    82,   252,     0,   252,     0,     0,   287,   288,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   313,     0,
       0,   229,   255,    62,    58,    56,    60,    61,    81,     0,
       0,     0,   327,   326,    26,    14,    27,    15,     0,     0,
       0,     0,    50,     0,     0,     0,     0,     0,     0,   317,
     229,    47,     0,   208,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   307,   307,   329,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   294,   293,   309,   308,     0,     0,     0,   328,
       0,   281,   202,     0,     0,    37,    25,     0,     0,     0,
       0,     0,     0,     0,     0,    39,     0,    55,     0,     0,
       0,     0,   340,   351,   353,   346,   352,   348,   347,   344,
     345,   349,   350,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   293,   289,   290,   291,   292,     0,
       0,     0,     0,     0,     0,    52,     0,    46,   164,   258,
     264,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   307,     0,     0,     0,    85,     0,    49,
       0,     0,     0,     0,    83,     0,     0,     0,     0,     0,
       0,     0,   109,   119,   120,   118,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    84,
       0,     0,   148,     0,   337,   149,     0,     0,     0,     0,
     173,   171,   175,   177,   154,   308,     0,     0,   308,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   219,     0,
     134,     0,     0,     0,     0,     0,     0,     0,   285,     0,
       6,    59,     0,   321,     0,     0,     0,     0,     0,     0,
      90,   104,    99,     0,     0,     0,   233,     0,   232,     0,
       0,   229,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    78,    66,    67,     0,   258,   264,   258,   242,   244,
       0,     0,     0,     0,   163,     0,    24,     0,     0,     0,
       0,   307,   307,     0,   312,     0,   315,   308,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   283,   283,    73,
      74,   283,   283,     0,     0,    75,    69,    70,     0,     0,
       0,     0,     0,     0,     0,   266,   106,   266,     0,   244,
       0,     0,   307,     0,   316,     0,     0,   209,     0,     0,
       0,     0,   286,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   131,     0,     0,   132,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     100,    88,     0,   114,   116,    40,   282,     0,     0,     0,
       0,    10,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    91,   105,   108,     0,   236,    51,     0,     0,
      35,   254,   253,     0,     0,     0,     0,     0,   103,   264,
     258,   115,   117,     0,     0,   308,     0,     0,     0,    12,
       0,   339,   335,     0,   336,   197,     0,     0,     0,     0,
     256,   257,    57,     0,    76,    77,    71,    72,     0,     0,
       0,     0,     0,    41,     0,     0,     0,     0,    92,   107,
       0,    38,   101,   266,   308,     0,    13,     0,     0,     0,
     151,   150,   162,   161,     0,     0,     0,     0,     0,   127,
     125,   126,     0,   224,   223,   222,     0,   130,     0,     0,
       0,     0,     0,     0,   190,     5,     0,     0,     0,     0,
       0,   230,   231,     0,   313,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   237,   238,   239,
       0,     0,     0,     0,     0,   259,     0,   260,     0,   261,
     265,   102,    93,     0,   252,     0,     0,   252,     0,   195,
       0,   196,     0,     0,     0,     0,     0,     0,     0,     0,
     121,     0,     0,     0,     0,     0,     0,     0,     0,    89,
       0,   186,     0,   205,   210,     0,   179,     0,     0,   182,
     183,     0,   135,     0,     0,     0,     0,     0,     0,     0,
     201,   191,   184,     0,   199,    54,    53,     0,     0,     0,
       0,     0,     0,    33,   110,     0,   252,    96,     0,     0,
     243,     0,   245,   246,     0,     0,     0,   252,   194,   252,
     252,   187,     0,   331,   332,   333,   334,     0,    28,   264,
     229,   284,     0,   129,     0,     0,   264,    95,    42,    43,
       0,     0,   267,     0,   189,   229,     0,   180,   192,   181,
       0,   133,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   122,    98,     0,    68,     0,
       0,     0,   263,   262,   193,   188,   185,    65,     0,    36,
      87,   128,   234,   235,    94,     0,     0,     0,     0,    86,
     206,   123,     0,     0,     0,     0,     0,     0,   124,     0,
     272,     0,     0,     0,     0,     0,     0,     0,     0,   112,
       0,   111,     0,     0,     0,     0,   272,   268,   271,   270,
     269,     0,     0,     0,     0,     0,    63,     0,     0,     0,
      97,   247,   244,    20,   244,     0,     0,   207,     0,     0,
      18,    19,   200,   198,    64,     0,    30,     0,     0,   236,
      23,    22,    21,   113,     0,     0,     0,   273,     0,    29,
       0,    31,     0,    32,   240,   241,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     249,   236,   248,     0,     0,     0,     0,   275,     0,   274,
       0,   296,     0,   298,     0,   297,     0,   295,     0,   303,
       0,   304,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   251,   250,     0,   272,   272,
     276,   277,   300,   302,   301,   299,   305,   306,    34,    16,
      17
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -858,  -858,  -858,  -131,    29,  -225,  -764,  -726,   312,  -858,
    -511,  -858,  -195,  -858,  -455,  -460,  -463,  -858,  -857,  -858,
    -858,   987,    76,  -858,   -31,  -858,   423,  -186,  -858,  -858,
    -254,     2,    17,  -144,   972,  -210,   -56,   210,  -858,   -16,
    -858,  -858,  -858,  1250,  -858,   -27,    -8
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    65,    66,    67,   368,   178,   750,   720,   957,   607,
     610,   940,   355,   379,   494,   496,   658,   911,   916,   949,
     229,   317,   644,    69,   125,   230,   352,   296,   951,   953,
     297,   369,   370,    72,    73,    74,   176,    97,    75,    82,
     816,   632,   633,   117,    83,    84,    85
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     105,   106,    70,   108,   110,   112,   377,   114,   115,   118,
     121,   127,   129,   131,   172,   175,   372,    71,   133,   357,
     116,   116,   427,   430,   353,   300,   603,   384,   661,    68,
     602,   855,   604,   231,   302,   409,   304,     7,     8,     7,
       8,   292,   293,   173,    77,    76,   260,   149,   408,   930,
     238,    78,     7,     8,     7,     8,   156,   419,   287,   194,
     196,   198,   232,  -211,   235,   237,   397,   441,   403,   180,
     181,   182,   171,   367,   659,   261,   881,     7,     8,   103,
     262,   419,   371,   150,   264,   265,   146,    98,   239,   288,
     426,   429,   267,   268,    99,   292,   293,   152,   442,   170,
     174,   290,   162,   291,  -213,   182,     7,     8,   294,   295,
      77,   182,   163,   132,   407,   449,   100,    78,   228,   509,
     248,    77,   241,   249,   419,   250,   354,   251,    78,    77,
     252,   259,   253,   396,     7,     8,    78,   420,   157,    77,
     254,   419,   101,    79,   315,  -211,   164,   385,    80,    81,
      77,  1019,  1020,   165,   153,   193,   181,    78,    70,   731,
     102,   421,   166,   167,   168,   732,   169,    77,   195,   703,
     303,   705,   240,    71,    78,   323,   324,   325,   326,   327,
     328,   329,   330,   331,   332,    68,  -213,   508,   242,   301,
     255,   256,    77,   305,   306,   307,   308,   450,   310,    78,
     759,   292,   293,   304,   422,    77,   465,   180,   181,    79,
     466,   243,    78,   956,    80,    81,   197,   104,   182,   124,
      79,   423,   399,   400,   401,    80,    81,   257,    79,   402,
       7,     8,   533,    80,    81,   311,   312,   313,    79,   234,
     614,   615,   158,    80,    81,   993,   591,   624,   627,    79,
     182,   534,   529,    77,    80,    81,   236,   107,   616,   138,
      78,   440,   530,    77,   444,   122,   258,   963,   964,   446,
      78,    80,    81,   154,   774,   775,    77,   965,   966,   776,
     395,     7,     8,    78,   460,   461,   462,   163,   665,   358,
     777,    79,   359,    77,   472,   123,    80,    81,   292,   293,
      78,   126,   517,   128,    79,   519,   314,   292,   293,    80,
      81,   294,   425,   172,   175,   623,   626,   130,   575,   134,
     294,   428,   485,   486,   487,   135,    77,   521,   136,    77,
     292,   293,   605,    78,   292,   293,    78,   606,   458,    77,
     769,   467,    77,   294,   507,   468,    78,   294,   625,    78,
     770,   139,    79,   151,   518,   520,   522,    80,    81,   869,
     294,   664,    79,   140,    77,   145,   874,    80,    81,     7,
       8,    78,   535,   536,   537,    79,   381,   382,   318,   319,
      80,    81,   383,   546,   393,   158,    77,   583,     7,     8,
     159,   160,    79,    78,   394,   621,   608,    80,    81,   553,
     505,   609,   556,   510,   558,   559,   560,   141,   562,   563,
     564,   941,   142,   942,    77,   568,   525,   526,   527,   875,
     143,    78,    77,     7,     8,    79,   490,   491,    79,    78,
      80,    81,   584,    80,    81,   469,   543,   144,    79,     7,
       8,    79,    77,    80,    81,   470,    80,    81,   147,    78,
     148,    70,   387,   388,    77,   570,   578,   579,   389,   571,
     726,    78,   155,    79,   727,    86,    71,   161,    80,    81,
      87,    88,   728,   622,    89,    90,   729,   163,    68,    91,
      92,   634,   783,   577,   577,    79,   784,    77,   514,   515,
      80,   109,   631,   372,    78,   408,   717,   718,   719,   969,
     199,   200,   201,   970,   202,   203,   177,   204,   205,   206,
     207,   208,   209,    79,   345,   346,   347,   348,    80,    81,
     210,    79,   211,   212,     7,     8,    80,    81,   213,   183,
     214,   185,   186,   244,   187,   188,   189,   190,   185,   808,
     179,    79,   811,   189,   190,   158,    80,   111,   245,   696,
     159,   504,   233,   113,   246,   191,   192,   215,    80,    81,
     247,   971,   722,   723,   216,   972,   707,   708,   709,   217,
     218,   219,   185,   753,   754,   187,   188,   189,   190,   220,
     221,   222,   954,   955,   223,   263,   119,   717,   718,   719,
     266,    80,    81,   269,   645,   870,   733,   646,   647,   736,
      93,   856,    94,    95,    96,   137,   270,    94,    95,    96,
     879,   699,   864,   271,   865,   866,   224,   225,   704,   259,
     824,   825,   748,   183,   184,   185,   186,   272,   187,   188,
     189,   190,   976,   978,   292,   293,   977,   979,   341,   342,
     273,   343,   764,   292,   344,   961,   962,   980,   274,   191,
     192,   981,   345,   346,   347,   348,   226,   227,   967,   968,
     780,    80,    81,   982,   349,   350,   351,   983,   954,   955,
     183,   184,   185,   186,   896,   187,   188,   189,   190,   984,
     275,   276,   986,   985,   755,   756,   987,   988,   798,   820,
     990,   989,   277,   278,   991,   279,   191,   192,   907,   908,
     909,   910,   788,   813,   814,   815,   835,   183,   280,   185,
     281,   771,   187,   188,   189,   190,   282,   283,   185,   322,
     822,   187,   188,   189,   190,   284,   785,   786,   285,   577,
     286,   289,   298,   191,   192,   299,   304,   309,   316,   320,
     321,   333,   191,   192,   356,   334,   335,   336,   337,   360,
     338,   846,   339,   361,   340,   354,   362,   363,   364,   365,
     366,   373,   851,   392,   374,   375,   376,   413,   378,   380,
     386,   390,   391,   398,   404,   405,   406,   410,   411,   412,
     415,   416,   417,   418,   414,   867,   431,   432,   424,   433,
     434,   435,   436,   437,   443,   439,   445,   452,   438,   453,
     454,   455,   447,   448,   456,   457,   459,   464,   463,   473,
     474,   475,   476,   477,   478,   479,   480,   850,   481,   482,
     483,   484,   488,   489,   493,   495,   492,   501,   497,   498,
     502,   503,   499,   511,   500,   512,   513,   524,   506,   516,
     523,   528,   531,   532,   538,   540,   868,   541,   539,   542,
     547,   548,   545,   549,   550,   544,   551,   906,   555,   876,
     913,    -2,     1,   561,   917,   918,   919,   565,   587,   580,
     567,   574,     2,   552,   566,   554,   927,   928,   929,   557,
     569,   932,   572,   576,     3,     4,     5,   581,     6,   573,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,   582,   585,   589,   586,   592,   924,   601,   588,
     618,   619,   590,   593,   594,   595,   596,   597,   598,   599,
     620,   628,   617,   630,   600,   639,   648,   995,   996,   662,
     663,   669,   667,   675,   687,   690,    39,    40,   694,   611,
     612,   697,   613,   629,   706,   636,   678,   635,   643,   698,
     637,   638,    41,    42,    43,    44,   689,    45,   649,    46,
     640,   641,    47,    48,   686,   691,   692,   693,   158,   710,
     642,   711,   650,   651,   712,   652,   653,   682,   713,   714,
     716,   654,    49,   695,   655,    50,    51,    52,   656,    53,
      54,    55,    56,    57,    58,    59,    60,     2,   657,   660,
     609,   700,    61,    62,   666,    63,    64,   668,   670,     3,
       4,     5,   671,     6,   672,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,   673,   715,   674,
     679,   676,   677,   680,   721,   724,   735,   742,   743,   681,
     744,   745,   683,   684,   685,   746,   688,   701,   702,   734,
     757,   725,   730,   751,   747,   738,   752,   737,   758,   739,
     760,    39,    40,   741,   749,   761,   762,   763,   740,   765,
     766,   767,   768,   772,   773,   778,   779,    41,    42,    43,
      44,   781,    45,   782,    46,   789,   787,    47,    48,   790,
     799,   793,   791,   792,   801,   804,   805,   806,   812,   794,
     795,   796,   797,   800,   817,   837,   802,    49,   803,   818,
      50,    51,    52,   807,    53,    54,    55,    56,    57,    58,
      59,    60,   809,   810,   819,   821,   830,    61,    62,   836,
      63,    64,   838,   839,   834,   844,   823,   826,   827,   828,
     829,   831,   832,   833,   840,   845,   841,   847,   842,   843,
     848,   849,   852,   853,   854,   857,   858,   859,   860,   862,
     863,   861,   871,   872,   873,   877,   878,   880,   882,   883,
     419,   894,   884,   897,   898,   887,   902,   885,   912,   886,
     888,   900,   903,   889,   890,   891,   892,   893,   914,   921,
     899,   901,   904,   920,   905,   922,   925,   923,   915,   931,
     935,   926,   939,   933,   934,   944,   936,   937,   938,   943,
     945,   946,   947,   959,   948,   960,   973,   997,   950,   974,
     952,   958,   994,   998,   975,   999,  1007,  1000,  1001,  1002,
    1005,  1003,  1006,  1004,  1008,  1009,  1010,  1011,  1012,  1013,
    1018,   992,   895,  1014,  1015,  1016,  1017,   120,   451,     0,
       0,     0,   471
};

static const yytype_int16 yycheck[] =
{
      27,    28,     0,    30,    31,    32,   231,    34,    35,    36,
      37,    42,    43,    44,    70,    71,   226,     0,    45,   214,
      36,    37,   276,   277,   210,   156,   486,    27,   539,     0,
     485,   795,   487,    89,    25,   260,    71,    29,    30,    29,
      30,    69,    70,    70,    62,   161,   102,    27,   258,   906,
      25,    69,    29,    30,    29,    30,    77,    83,    25,    86,
      87,    88,    89,    77,    91,    92,   252,    25,   254,    77,
      78,    79,    70,    25,   537,   102,   840,    29,    30,    25,
     107,    83,   226,    63,    25,    26,    57,    97,    63,    56,
     276,   277,    25,    26,    97,    69,    70,    25,    56,    70,
      71,   156,    25,   158,    77,   113,    29,    30,    82,    83,
      62,   119,    35,    25,   258,    77,    97,    69,    89,   373,
      15,    62,    93,    18,    83,    20,   161,    22,    69,    62,
      25,   102,    27,   161,    29,    30,    69,   163,   159,    62,
      35,    83,    97,   161,   171,   159,    69,   147,   166,   167,
      62,  1008,  1009,    76,    82,    25,   164,    69,   156,   619,
      97,   163,    85,    86,    87,   620,    89,    62,    25,   161,
     161,   161,   147,   156,    69,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   156,   159,   373,   141,   160,
      85,    86,    62,   164,   165,   166,   167,   159,   169,    69,
     663,    69,    70,    71,   163,    62,   156,   215,   216,   161,
     160,   164,    69,   939,   166,   167,    25,    25,   226,   162,
     161,   163,    90,    91,    92,   166,   167,   122,   161,    97,
      29,    30,    68,   166,   167,    90,    91,    92,   161,    25,
     123,   124,    97,   166,   167,   971,   471,   501,   502,   161,
     258,    87,    25,    62,   166,   167,    25,   161,   141,    49,
      69,   288,    35,    62,   291,   161,   161,   127,   128,   296,
      69,   166,   167,    63,     3,     4,    62,   137,   138,     8,
     251,    29,    30,    69,   311,   312,   313,    35,   542,    25,
      19,   161,    25,    62,   321,   161,   166,   167,    69,    70,
      69,   162,    25,   162,   161,    25,   161,    69,    70,   166,
     167,    82,    83,   369,   370,   501,   502,   162,   449,   161,
      82,    83,   349,   350,   351,   162,    62,    25,   162,    62,
      69,    70,   156,    69,    69,    70,    69,   161,   309,    62,
      25,   156,    62,    82,    83,   160,    69,    82,    83,    69,
      35,   161,   161,    25,   381,   382,   383,   166,   167,   819,
      82,    83,   161,   161,    62,    25,   826,   166,   167,    29,
      30,    69,   399,   400,   401,   161,   104,   105,   133,   134,
     166,   167,   110,   410,    25,    97,    62,    35,    29,    30,
     102,   103,   161,    69,    35,    35,   156,   166,   167,   426,
     371,   161,   429,   374,   431,   432,   433,   161,   435,   436,
     437,   922,   161,   924,    62,   442,   387,   388,   389,    25,
     161,    69,    62,    29,    30,   161,   131,   132,   161,    69,
     166,   167,   459,   166,   167,    25,   407,   161,   161,    29,
      30,   161,    62,   166,   167,    35,   166,   167,   161,    69,
     161,   449,   104,   105,    62,   156,   454,   455,   110,   160,
     156,    69,     0,   161,   160,    88,   449,    97,   166,   167,
      93,    94,   156,   500,    97,    98,   160,    35,   449,   102,
     103,   508,   156,   454,   455,   161,   160,    62,   131,   132,
     166,   167,   508,   703,    69,   705,   124,   125,   126,   156,
       5,     6,     7,   160,     9,    10,   161,    12,    13,    14,
      15,    16,    17,   161,    78,    79,    80,    81,   166,   167,
      25,   161,    27,    28,    29,    30,   166,   167,    33,    64,
      35,    66,    67,    35,    69,    70,    71,    72,    66,   734,
      25,   161,   737,    71,    72,    97,   166,   167,    35,   576,
     102,   103,    25,   161,    35,    90,    91,    62,   166,   167,
      35,   156,   123,   124,    69,   160,   593,   594,   595,    74,
      75,    76,    66,   139,   140,    69,    70,    71,    72,    84,
      85,    86,   119,   120,    89,    25,   161,   124,   125,   126,
     171,   166,   167,    25,   518,   820,   623,   521,   522,   626,
      97,   796,    99,   100,   101,    97,    25,    99,   100,   101,
     835,   582,   807,    25,   809,   810,   121,   122,   589,   590,
     119,   120,   649,    64,    65,    66,    67,    25,    69,    70,
      71,    72,   156,   156,    69,    70,   160,   160,    64,    65,
      25,    67,   669,    69,    70,   134,   135,   156,    25,    90,
      91,   160,    78,    79,    80,    81,   161,   162,   142,   143,
     691,   166,   167,   156,    90,    91,    92,   160,   119,   120,
      64,    65,    66,    67,   860,    69,    70,    71,    72,   156,
      25,    25,   156,   160,   655,   656,   160,   156,   715,   745,
     156,   160,    25,    27,   160,    25,    90,    91,   106,   107,
     108,   109,   700,   168,   169,   170,   762,    64,    25,    66,
      25,   682,    69,    70,    71,    72,    25,    25,    66,   160,
     747,    69,    70,    71,    72,    25,   697,   698,    35,   700,
      25,    27,    25,    90,    91,    27,    71,   161,   161,   156,
     158,   161,    90,    91,    70,   161,   161,   161,   161,    25,
     161,   778,   161,    25,   161,   161,    25,    25,    25,   162,
     162,    25,   789,    25,    70,   161,   161,    70,   161,   161,
     161,   161,   161,    97,    35,    35,   162,   156,    97,   160,
     160,    70,   156,   156,   172,   812,   156,   156,   160,   156,
     156,   156,   156,   156,    25,    97,    25,    25,   160,    35,
     156,   156,   163,   163,   157,   157,   157,   134,   124,    25,
      25,    25,    25,    35,    25,    25,    25,   788,    25,    25,
      25,   161,    25,    25,   161,   161,    34,    25,   157,   157,
      25,    70,   157,    25,   157,    25,   156,    25,   160,   130,
     161,    25,   136,    25,    21,   157,   817,   157,    35,    25,
      25,    25,   156,    25,    25,   160,    25,   884,    25,   830,
     887,     0,     1,    25,   891,   892,   893,    25,   134,    35,
      97,    97,    11,   163,   161,   163,   903,   904,   905,   163,
     160,   912,   163,   158,    23,    24,    25,    35,    27,   163,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,   156,   160,    97,   160,   156,   898,    25,   133,
      35,    35,    97,   158,   158,   158,   156,   156,   156,   156,
      35,    25,   141,    25,   156,    25,    25,   974,   975,    35,
      35,    91,    35,    97,    25,    25,    95,    96,    25,   160,
     160,    69,   156,   160,    25,   156,    97,   163,   161,    76,
     156,   156,   111,   112,   113,   114,    97,   116,   156,   118,
     160,   160,   121,   122,   129,    97,    97,    97,    97,    25,
     160,    35,   160,   156,    25,   156,   160,   156,    35,    25,
      25,   161,   141,   159,   161,   144,   145,   146,   161,   148,
     149,   150,   151,   152,   153,   154,   155,    11,   161,   161,
     161,   156,   161,   162,   160,   164,   165,   160,   160,    23,
      24,    25,   160,    27,   160,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,   160,    90,   163,
     160,   163,   163,   160,    25,    25,    25,    25,    25,   160,
      25,    97,   160,   160,   160,   134,   161,   160,   160,   163,
     124,   160,   160,    35,   156,   160,    35,   163,    35,   163,
      25,    95,    96,   163,   161,   163,    97,    25,   171,    97,
      25,    97,    97,   160,   129,   158,   162,   111,   112,   113,
     114,    25,   116,    25,   118,   158,   156,   121,   122,   156,
      97,   160,   156,   156,    97,    97,   141,   124,    90,   160,
     160,   160,   160,   160,    97,    25,   160,   141,   160,   160,
     144,   145,   146,   163,   148,   149,   150,   151,   152,   153,
     154,   155,   163,   163,   160,   160,   156,   161,   162,    97,
     164,   165,    35,    25,   163,    25,   160,   160,   160,   160,
     160,   156,   160,   160,   160,    25,   160,    25,   161,   161,
      25,    27,    25,    25,    25,   161,   160,    25,    25,   160,
     160,    31,   160,   160,   160,    25,    71,    25,    25,    25,
      83,   131,   158,    70,    69,   158,    35,   161,    97,   160,
     160,   156,    91,   161,   160,   158,   158,   158,   124,    25,
     160,   160,   158,   160,   158,    25,    27,    32,   161,   160,
     135,    70,   161,   160,   160,    21,   160,   160,   160,   160,
     156,   156,   160,    25,   161,    25,    35,   135,   161,   158,
     161,   161,   160,   134,   158,   135,    71,   135,   135,   135,
     160,   135,   160,   135,   160,   160,   160,   160,   160,   160,
      35,   969,   859,   160,   160,   160,   160,    37,   301,    -1,
      -1,    -1,   320
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,    11,    23,    24,    25,    27,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    95,
      96,   111,   112,   113,   114,   116,   118,   121,   122,   141,
     144,   145,   146,   148,   149,   150,   151,   152,   153,   154,
     155,   161,   162,   164,   165,   174,   175,   176,   177,   196,
     204,   205,   206,   207,   208,   211,   161,    62,    69,   161,
     166,   167,   212,   217,   218,   219,    88,    93,    94,    97,
      98,   102,   103,    97,    99,   100,   101,   210,    97,    97,
      97,    97,    97,    25,    25,   218,   218,   161,   218,   167,
     218,   167,   218,   161,   218,   218,   212,   216,   218,   161,
     216,   218,   161,   161,   162,   197,   162,   197,   162,   197,
     162,   197,    25,   218,   161,   162,   162,    97,   210,   161,
     161,   161,   161,   161,   161,    25,   177,   161,   161,    27,
      63,    25,    25,    82,   210,     0,    77,   159,    97,   102,
     103,    97,    25,    35,    69,    76,    85,    86,    87,    89,
     177,   204,   209,   218,   177,   209,   209,   161,   178,    25,
     219,   219,   219,    64,    65,    66,    67,    69,    70,    71,
      72,    90,    91,    25,   218,    25,   218,    25,   218,     5,
       6,     7,     9,    10,    12,    13,    14,    15,    16,    17,
      25,    27,    28,    33,    35,    62,    69,    74,    75,    76,
      84,    85,    86,    89,   121,   122,   161,   162,   177,   193,
     198,   209,   218,    25,    25,   218,    25,   218,    25,    63,
     147,   177,   141,   164,    35,    35,    35,    35,    15,    18,
      20,    22,    25,    27,    35,    85,    86,   122,   161,   177,
     209,   218,   218,    25,    25,    26,   171,    25,    26,    25,
      25,    25,    25,    25,    25,    25,    25,    25,    27,    25,
      25,    25,    25,    25,    25,    35,    25,    25,    56,    27,
     156,   158,    69,    70,    82,    83,   200,   203,    25,    27,
     176,   177,    25,   161,    71,   177,   177,   177,   177,   161,
     177,    90,    91,    92,   161,   218,   161,   194,   133,   134,
     156,   158,   160,   219,   219,   219,   219,   219,   219,   219,
     219,   219,   219,   161,   161,   161,   161,   161,   161,   161,
     161,    64,    65,    67,    70,    78,    79,    80,    81,    90,
      91,    92,   199,   200,   161,   185,    70,   185,    25,    25,
      25,    25,    25,    25,    25,   162,   162,    25,   177,   204,
     205,   206,   208,    25,    70,   161,   161,   178,   161,   186,
     161,   104,   105,   110,    27,   147,   161,   104,   105,   110,
     161,   161,    25,    25,    35,   177,   161,   200,    97,    90,
      91,    92,    97,   200,    35,    35,   162,   206,   208,   178,
     156,    97,   160,    70,   172,   160,    70,   156,   156,    83,
     163,   163,   163,   163,   160,    83,   200,   203,    83,   200,
     203,   156,   156,   156,   156,   156,   156,   156,   160,    97,
     218,    25,    56,    25,   218,    25,   218,   163,   163,    77,
     159,   194,    25,    35,   156,   156,   157,   157,   177,   157,
     218,   218,   218,   124,   134,   156,   160,   156,   160,    25,
      35,   207,   218,    25,    25,    25,    25,    35,    25,    25,
      25,    25,    25,    25,   161,   218,   218,   218,    25,    25,
     131,   132,    34,   161,   187,   161,   188,   157,   157,   157,
     157,    25,    25,    70,   103,   177,   160,    83,   200,   203,
     177,    25,    25,   156,   131,   132,   130,    25,   218,    25,
     218,    25,   218,   161,    25,   177,   177,   177,    25,    25,
      35,   136,    25,    68,    87,   218,   218,   218,    21,    35,
     157,   157,    25,   177,   160,   156,   218,    25,    25,    25,
      25,    25,   163,   218,   163,    25,   218,   163,   218,   218,
     218,    25,   218,   218,   218,    25,   161,    97,   218,   160,
     156,   160,   163,   163,    97,   176,   158,   177,   204,   204,
      35,    35,   156,    35,   218,   160,   160,   134,   133,    97,
      97,   178,   156,   158,   158,   158,   156,   156,   156,   156,
     156,    25,   187,   188,   187,   156,   161,   182,   156,   161,
     183,   160,   160,   156,   123,   124,   141,   141,    35,    35,
      35,    35,   218,   200,   203,    83,   200,   203,    25,   160,
      25,   212,   214,   215,   218,   163,   156,   156,   156,    25,
     160,   160,   160,   161,   195,   195,   195,   195,    25,   156,
     160,   156,   156,   160,   161,   161,   161,   161,   189,   189,
     161,   183,    35,    35,    83,   203,   160,    35,   160,    91,
     160,   160,   160,   160,   163,    97,   163,   163,    97,   160,
     160,   160,   156,   160,   160,   160,   129,    25,   161,    97,
      25,    97,    97,    97,    25,   159,   218,    69,    76,   177,
     156,   160,   160,   161,   177,   161,    25,   218,   218,   218,
      25,    35,    25,    35,    25,    90,    25,   124,   125,   126,
     180,    25,   123,   124,    25,   160,   156,   160,   156,   160,
     160,   188,   187,   218,   163,    25,   218,   163,   160,   163,
     171,   163,    25,    25,    25,    97,   134,   156,   218,   161,
     179,    35,    35,   139,   140,   177,   177,   124,    35,   189,
      25,   163,    97,    25,   218,    97,    25,    97,    97,    25,
      35,   177,   160,   129,     3,     4,     8,    19,   158,   162,
     197,    25,    25,   156,   160,   177,   177,   156,   204,   158,
     156,   156,   156,   160,   160,   160,   160,   160,   218,    97,
     160,    97,   160,   160,    97,   141,   124,   163,   185,   163,
     163,   185,    90,   168,   169,   170,   213,    97,   160,   160,
     209,   160,   218,   160,   119,   120,   160,   160,   160,   160,
     156,   156,   160,   160,   163,   209,    97,    25,    35,    25,
     160,   160,   161,   161,    25,    25,   218,    25,    25,    27,
     177,   218,    25,    25,    25,   179,   185,   161,   160,    25,
      25,    31,   160,   160,   185,   185,   185,   218,   177,   188,
     178,   160,   160,   160,   188,    25,   177,    25,    71,   178,
      25,   179,    25,    25,   158,   161,   160,   158,   160,   161,
     160,   158,   158,   158,   131,   199,   200,    70,    69,   160,
     156,   160,    35,    91,   158,   158,   218,   106,   107,   108,
     109,   190,    97,   218,   124,   161,   191,   218,   218,   218,
     160,    25,    25,    32,   177,    27,    70,   218,   218,   218,
     191,   160,   197,   160,   160,   135,   160,   160,   160,   161,
     184,   183,   183,   160,    21,   156,   156,   160,   161,   192,
     161,   201,   161,   202,   119,   120,   180,   181,   161,    25,
      25,   134,   135,   127,   128,   137,   138,   142,   143,   156,
     160,   156,   160,    35,   158,   158,   156,   160,   156,   160,
     156,   160,   156,   160,   156,   160,   156,   160,   156,   160,
     156,   160,   181,   180,   160,   218,   218,   135,   134,   135,
     135,   135,   135,   135,   135,   160,   160,    71,   160,   160,
     160,   160,   160,   160,   160,   160,   160,   160,    35,   191,
     191
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,   173,   174,   174,   175,   175,   175,   175,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   176,   176,   176,
     176,   176,   176,   176,   176,   176,   176,   177,   177,   178,
     178,   178,   178,   178,   179,   179,   180,   180,   180,   180,
     181,   181,   182,   182,   183,   183,   183,   184,   184,   184,
     184,   184,   185,   185,   185,   186,   186,   186,   187,   187,
     187,   187,   187,   187,   188,   188,   189,   189,   190,   190,
     190,   190,   191,   191,   192,   192,   192,   192,   193,   193,
     193,   194,   194,   195,   195,   196,   197,   198,   198,   199,
     199,   199,   199,   200,   200,   201,   201,   201,   201,   201,
     201,   201,   201,   202,   202,   202,   202,   203,   203,   203,
     204,   205,   206,   207,   207,   207,   207,   207,   208,   208,
     208,   209,   210,   210,   210,   210,   211,   211,   211,   211,
     212,   213,   213,   213,   214,   215,   215,   216,   217,   217,
     217,   217,   217,   218,   219,   219,   219,   219,   219,   219,
     219,   219,   219,   219,   219
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     1,     2,     6,     4,     1,     1,     2,
       5,     1,     6,     6,     3,     3,    17,    17,    11,    11,
      11,    12,    12,    12,     5,     3,     3,     3,     8,    13,
      12,    13,    13,     8,    17,     6,     9,     3,     6,     3,
       5,     6,     8,     8,     2,     2,     4,     3,     2,     4,
       3,     6,     4,     7,     7,     3,     3,     6,     3,     4,
       3,     3,     3,    11,    11,     9,     5,     5,     9,     5,
       5,     6,     6,     5,     5,     5,     6,     6,     5,     1,
       3,     3,     3,     4,     4,     4,     9,     9,     5,     7,
       4,     6,     6,     7,     9,     8,     8,    11,     9,     4,
       5,     6,     7,     6,     4,     6,     5,     6,     6,     4,
       8,    10,    10,    12,     5,     6,     5,     6,     4,     4,
       4,     7,     9,     9,     9,     6,     6,     6,     9,     8,
       6,     5,     5,     8,     4,     7,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     2,     4,     4,
       6,     6,     2,     2,     4,     2,     2,     2,     2,     2,
       2,     6,     6,     5,     4,     3,     3,     3,     3,     3,
       3,     4,     2,     4,     2,     4,     2,     4,     2,     7,
       8,     8,     7,     7,     7,     9,     7,     8,     9,     8,
       6,     7,     8,     9,     8,     7,     7,     6,    11,     7,
      11,     7,     3,     2,     1,     7,     9,    11,     3,     5,
       7,     2,     2,     2,     2,     1,     1,     2,     2,     4,
       1,     1,     6,     6,     6,     2,     2,     1,     1,     0,
       5,     5,     3,     3,     3,     3,     0,     1,     1,     1,
       1,     1,     0,     3,     0,     3,     3,     0,     3,     3,
       5,     5,     0,     3,     3,     0,     3,     3,     0,     3,
       3,     3,     5,     5,     0,     3,     0,     3,     1,     1,
       1,     1,     0,     3,     3,     3,     5,     5,     1,     1,
       1,     0,     3,     0,     3,     4,     4,     1,     1,     1,
       1,     1,     1,     1,     1,     3,     3,     3,     3,     5,
       5,     5,     5,     3,     3,     5,     5,     0,     1,     1,
       2,     2,     2,     3,     1,     5,     5,     3,     2,     2,
       2,     3,     1,     1,     1,     1,     3,     3,     3,     3,
       1,     1,     1,     1,     3,     1,     1,     3,     1,     1,
       3,     2,     2,     1,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     1
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
  case 3: /* statement: asm  */
#line 650 "./config/bfin-parse.y"
        {
	  insn = (yyvsp[0].instr);
	  if (insn == (INSTR_T) 0)
	    return NO_INSN_GENERATED;
	  else if (insn == (INSTR_T) - 1)
	    return SEMANTIC_ERROR;
	  else
	    return INSN_GENERATED;
	}
#line 2826 "config/bfin-parse.c"
    break;

  case 5: /* asm: asm_1 DOUBLE_BAR asm_1 DOUBLE_BAR asm_1 SEMICOLON  */
#line 664 "./config/bfin-parse.y"
        {
	  if (((yyvsp[-5].instr)->value & 0xf800) == 0xc000)
	    {
	      if (is_group1 ((yyvsp[-3].instr)) && is_group2 ((yyvsp[-1].instr)))
		(yyval.instr) = gen_multi_instr_1 ((yyvsp[-5].instr), (yyvsp[-3].instr), (yyvsp[-1].instr));
	      else if (is_group2 ((yyvsp[-3].instr)) && is_group1 ((yyvsp[-1].instr)))
		(yyval.instr) = gen_multi_instr_1 ((yyvsp[-5].instr), (yyvsp[-1].instr), (yyvsp[-3].instr));
	      else
		return yyerror ("Wrong 16 bit instructions groups, slot 2 and slot 3 must be 16-bit instruction group");
	    }
	  else if (((yyvsp[-3].instr)->value & 0xf800) == 0xc000)
	    {
	      if (is_group1 ((yyvsp[-5].instr)) && is_group2 ((yyvsp[-1].instr)))
		(yyval.instr) = gen_multi_instr_1 ((yyvsp[-3].instr), (yyvsp[-5].instr), (yyvsp[-1].instr));
	      else if (is_group2 ((yyvsp[-5].instr)) && is_group1 ((yyvsp[-1].instr)))
		(yyval.instr) = gen_multi_instr_1 ((yyvsp[-3].instr), (yyvsp[-1].instr), (yyvsp[-5].instr));
	      else
		return yyerror ("Wrong 16 bit instructions groups, slot 1 and slot 3 must be 16-bit instruction group");
	    }
	  else if (((yyvsp[-1].instr)->value & 0xf800) == 0xc000)
	    {
	      if (is_group1 ((yyvsp[-5].instr)) && is_group2 ((yyvsp[-3].instr)))
		(yyval.instr) = gen_multi_instr_1 ((yyvsp[-1].instr), (yyvsp[-5].instr), (yyvsp[-3].instr));
	      else if (is_group2 ((yyvsp[-5].instr)) && is_group1 ((yyvsp[-3].instr)))
		(yyval.instr) = gen_multi_instr_1 ((yyvsp[-1].instr), (yyvsp[-3].instr), (yyvsp[-5].instr));
	      else
		return yyerror ("Wrong 16 bit instructions groups, slot 1 and slot 2 must be 16-bit instruction group");
	    }
	  else
	    error ("\nIllegal Multi Issue Construct, at least any one of the slot must be DSP32 instruction group\n");
	}
#line 2862 "config/bfin-parse.c"
    break;

  case 6: /* asm: asm_1 DOUBLE_BAR asm_1 SEMICOLON  */
#line 697 "./config/bfin-parse.y"
        {
	  if (((yyvsp[-3].instr)->value & 0xf800) == 0xc000)
	    {
	      if (is_group1 ((yyvsp[-1].instr)))
		(yyval.instr) = gen_multi_instr_1 ((yyvsp[-3].instr), (yyvsp[-1].instr), 0);
	      else if (is_group2 ((yyvsp[-1].instr)))
		(yyval.instr) = gen_multi_instr_1 ((yyvsp[-3].instr), 0, (yyvsp[-1].instr));
	      else
		return yyerror ("Wrong 16 bit instructions groups, slot 2 must be the 16-bit instruction group");
	    }
	  else if (((yyvsp[-1].instr)->value & 0xf800) == 0xc000)
	    {
	      if (is_group1 ((yyvsp[-3].instr)))
		(yyval.instr) = gen_multi_instr_1 ((yyvsp[-1].instr), (yyvsp[-3].instr), 0);
	      else if (is_group2 ((yyvsp[-3].instr)))
		(yyval.instr) = gen_multi_instr_1 ((yyvsp[-1].instr), 0, (yyvsp[-3].instr));
	      else
		return yyerror ("Wrong 16 bit instructions groups, slot 1 must be the 16-bit instruction group");
	    }
	  else if (is_group1 ((yyvsp[-3].instr)) && is_group2 ((yyvsp[-1].instr)))
	      (yyval.instr) = gen_multi_instr_1 (0, (yyvsp[-3].instr), (yyvsp[-1].instr));
	  else if (is_group2 ((yyvsp[-3].instr)) && is_group1 ((yyvsp[-1].instr)))
	    (yyval.instr) = gen_multi_instr_1 (0, (yyvsp[-1].instr), (yyvsp[-3].instr));
	  else
	    return yyerror ("Wrong 16 bit instructions groups, slot 1 and slot 2 must be the 16-bit instruction group");
	}
#line 2893 "config/bfin-parse.c"
    break;

  case 7: /* asm: error  */
#line 724 "./config/bfin-parse.y"
        {
	(yyval.instr) = 0;
	yyerror ("");
	yyerrok;
	}
#line 2903 "config/bfin-parse.c"
    break;

  case 8: /* asm_1: MNOP  */
#line 735 "./config/bfin-parse.y"
        {
	  (yyval.instr) = DSP32MAC (3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0);
	}
#line 2911 "config/bfin-parse.c"
    break;

  case 9: /* asm_1: assign_macfunc opt_mode  */
#line 739 "./config/bfin-parse.y"
        {
	  int op0, op1;
	  int w0 = 0, w1 = 0;
	  int h00, h10, h01, h11;

	  if (check_macfunc_option (&(yyvsp[-1].macfunc), &(yyvsp[0].mod)) < 0)
	    return yyerror ("bad option");

	  if ((yyvsp[-1].macfunc).n == 0)
	    {
	      if ((yyvsp[0].mod).MM)
		return yyerror ("(m) not allowed with a0 unit");
	      op1 = 3;
	      op0 = (yyvsp[-1].macfunc).op;
	      w1 = 0;
              w0 = (yyvsp[-1].macfunc).w;
	      h00 = IS_H ((yyvsp[-1].macfunc).s0);
              h10 = IS_H ((yyvsp[-1].macfunc).s1);
	      h01 = h11 = 0;
	    }
	  else
	    {
	      op1 = (yyvsp[-1].macfunc).op;
	      op0 = 3;
	      w1 = (yyvsp[-1].macfunc).w;
              w0 = 0;
	      h00 = h10 = 0;
	      h01 = IS_H ((yyvsp[-1].macfunc).s0);
              h11 = IS_H ((yyvsp[-1].macfunc).s1);
	    }
	  (yyval.instr) = DSP32MAC (op1, (yyvsp[0].mod).MM, (yyvsp[0].mod).mod, w1, (yyvsp[-1].macfunc).P, h01, h11, h00, h10,
			 &(yyvsp[-1].macfunc).dst, op0, &(yyvsp[-1].macfunc).s0, &(yyvsp[-1].macfunc).s1, w0);
	}
#line 2949 "config/bfin-parse.c"
    break;

  case 10: /* asm_1: assign_macfunc opt_mode COMMA assign_macfunc opt_mode  */
#line 777 "./config/bfin-parse.y"
        {
	  Register *dst;

	  if (check_macfuncs (&(yyvsp[-4].macfunc), &(yyvsp[-3].mod), &(yyvsp[-1].macfunc), &(yyvsp[0].mod)) < 0)
	    return -1;
	  notethat ("assign_macfunc (.), assign_macfunc (.)\n");

	  if ((yyvsp[-4].macfunc).w)
	    dst = &(yyvsp[-4].macfunc).dst;
	  else
	    dst = &(yyvsp[-1].macfunc).dst;

	  (yyval.instr) = DSP32MAC ((yyvsp[-4].macfunc).op, (yyvsp[-3].mod).MM, (yyvsp[0].mod).mod, (yyvsp[-4].macfunc).w, (yyvsp[-4].macfunc).P,
			 IS_H ((yyvsp[-4].macfunc).s0),  IS_H ((yyvsp[-4].macfunc).s1), IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1),
			 dst, (yyvsp[-1].macfunc).op, &(yyvsp[-4].macfunc).s0, &(yyvsp[-4].macfunc).s1, (yyvsp[-1].macfunc).w);
	}
#line 2970 "config/bfin-parse.c"
    break;

  case 11: /* asm_1: DISALGNEXCPT  */
#line 797 "./config/bfin-parse.y"
        {
	  notethat ("dsp32alu: DISALGNEXCPT\n");
	  (yyval.instr) = DSP32ALU (18, 0, 0, 0, 0, 0, 0, 0, 3);
	}
#line 2979 "config/bfin-parse.c"
    break;

  case 12: /* asm_1: REG ASSIGN LPAREN a_plusassign REG_A RPAREN  */
#line 802 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && !IS_A1 ((yyvsp[-2].reg)) && IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = ( A0 += A1 )\n");
	      (yyval.instr) = DSP32ALU (11, 0, 0, &(yyvsp[-5].reg), &reg7, &reg7, 0, 0, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 2993 "config/bfin-parse.c"
    break;

  case 13: /* asm_1: HALF_REG ASSIGN LPAREN a_plusassign REG_A RPAREN  */
#line 812 "./config/bfin-parse.y"
        {
	  if (!IS_A1 ((yyvsp[-2].reg)) && IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs_half = ( A0 += A1 )\n");
	      (yyval.instr) = DSP32ALU (11, IS_H ((yyvsp[-5].reg)), 0, &(yyvsp[-5].reg), &reg7, &reg7, 0, 0, 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3007 "config/bfin-parse.c"
    break;

  case 14: /* asm_1: A_ZERO_DOT_H ASSIGN HALF_REG  */
#line 822 "./config/bfin-parse.y"
        {
	  notethat ("dsp32alu: A_ZERO_DOT_H = dregs_hi\n");
	  (yyval.instr) = DSP32ALU (9, IS_H ((yyvsp[0].reg)), 0, 0, &(yyvsp[0].reg), 0, 0, 0, 0);
	}
#line 3016 "config/bfin-parse.c"
    break;

  case 15: /* asm_1: A_ONE_DOT_H ASSIGN HALF_REG  */
#line 827 "./config/bfin-parse.y"
        {
	  notethat ("dsp32alu: A_ZERO_DOT_H = dregs_hi\n");
	  (yyval.instr) = DSP32ALU (9, IS_H ((yyvsp[0].reg)), 0, 0, &(yyvsp[0].reg), 0, 0, 0, 2);
	}
#line 3025 "config/bfin-parse.c"
    break;

  case 16: /* asm_1: LPAREN REG COMMA REG RPAREN ASSIGN BYTEOP16P LPAREN REG COLON expr COMMA REG COLON expr RPAREN aligndir  */
#line 833 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-15].reg)) || !IS_DREG ((yyvsp[-13].reg)))
	    return yyerror ("Dregs expected");
	  else if (REG_SAME ((yyvsp[-15].reg), (yyvsp[-13].reg)))
	    return yyerror ("Illegal dest register combination");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: (dregs , dregs ) = BYTEOP16P (dregs_pair , dregs_pair ) (aligndir)\n");
	      (yyval.instr) = DSP32ALU (21, 0, &(yyvsp[-15].reg), &(yyvsp[-13].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0, 0);
	    }
	}
#line 3045 "config/bfin-parse.c"
    break;

  case 17: /* asm_1: LPAREN REG COMMA REG RPAREN ASSIGN BYTEOP16M LPAREN REG COLON expr COMMA REG COLON expr RPAREN aligndir  */
#line 851 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-15].reg)) || !IS_DREG ((yyvsp[-13].reg)))
	    return yyerror ("Dregs expected");
	  else if (REG_SAME ((yyvsp[-15].reg), (yyvsp[-13].reg)))
	    return yyerror ("Illegal dest register combination");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: (dregs , dregs ) = BYTEOP16M (dregs_pair , dregs_pair ) (aligndir)\n");
	      (yyval.instr) = DSP32ALU (21, 0, &(yyvsp[-15].reg), &(yyvsp[-13].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0, 1);
	    }
	}
#line 3065 "config/bfin-parse.c"
    break;

  case 18: /* asm_1: LPAREN REG COMMA REG RPAREN ASSIGN BYTEUNPACK REG COLON expr aligndir  */
#line 868 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-9].reg)) || !IS_DREG ((yyvsp[-7].reg)))
	    return yyerror ("Dregs expected");
	  else if (REG_SAME ((yyvsp[-9].reg), (yyvsp[-7].reg)))
	    return yyerror ("Illegal dest register combination");
	  else if (!valid_dreg_pair (&(yyvsp[-3].reg), (yyvsp[-1].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: (dregs , dregs ) = BYTEUNPACK dregs_pair (aligndir)\n");
	      (yyval.instr) = DSP32ALU (24, 0, &(yyvsp[-9].reg), &(yyvsp[-7].reg), &(yyvsp[-3].reg), 0, (yyvsp[0].r0).r0, 0, 1);
	    }
	}
#line 3083 "config/bfin-parse.c"
    break;

  case 19: /* asm_1: LPAREN REG COMMA REG RPAREN ASSIGN SEARCH REG LPAREN searchmod RPAREN  */
#line 882 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-9].reg), (yyvsp[-7].reg)))
	    return yyerror ("Illegal dest register combination");

	  if (IS_DREG ((yyvsp[-9].reg)) && IS_DREG ((yyvsp[-7].reg)) && IS_DREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32alu: (dregs , dregs ) = SEARCH dregs (searchmod)\n");
	      (yyval.instr) = DSP32ALU (13, 0, &(yyvsp[-9].reg), &(yyvsp[-7].reg), &(yyvsp[-3].reg), 0, 0, 0, (yyvsp[-1].r0).r0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3100 "config/bfin-parse.c"
    break;

  case 20: /* asm_1: REG ASSIGN A_ONE_DOT_L PLUS A_ONE_DOT_H COMMA REG ASSIGN A_ZERO_DOT_L PLUS A_ZERO_DOT_H  */
#line 896 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-10].reg), (yyvsp[-4].reg)))
	    return yyerror ("Illegal dest register combination");

	  if (IS_DREG ((yyvsp[-10].reg)) && IS_DREG ((yyvsp[-4].reg)))
	    {
	      notethat ("dsp32alu: dregs = A1.l + A1.h, dregs = A0.l + A0.h  \n");
	      (yyval.instr) = DSP32ALU (12, 0, &(yyvsp[-10].reg), &(yyvsp[-4].reg), &reg7, &reg7, 0, 0, 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3117 "config/bfin-parse.c"
    break;

  case 21: /* asm_1: REG ASSIGN REG_A PLUS REG_A COMMA REG ASSIGN REG_A MINUS REG_A amod1  */
#line 911 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-11].reg), (yyvsp[-5].reg)))
	    return yyerror ("Resource conflict in dest reg");

	  if (IS_DREG ((yyvsp[-11].reg)) && IS_DREG ((yyvsp[-5].reg)) && !REG_SAME ((yyvsp[-9].reg), (yyvsp[-7].reg))
	      && IS_A1 ((yyvsp[-3].reg)) && !IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = A1 + A0 , dregs = A1 - A0 (amod1)\n");
	      (yyval.instr) = DSP32ALU (17, 0, &(yyvsp[-11].reg), &(yyvsp[-5].reg), &reg7, &reg7, (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, 0);

	    }
	  else if (IS_DREG ((yyvsp[-11].reg)) && IS_DREG ((yyvsp[-5].reg)) && !REG_SAME ((yyvsp[-9].reg), (yyvsp[-7].reg))
		   && !IS_A1 ((yyvsp[-3].reg)) && IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = A0 + A1 , dregs = A0 - A1 (amod1)\n");
	      (yyval.instr) = DSP32ALU (17, 0, &(yyvsp[-11].reg), &(yyvsp[-5].reg), &reg7, &reg7, (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3142 "config/bfin-parse.c"
    break;

  case 22: /* asm_1: REG ASSIGN REG plus_minus REG COMMA REG ASSIGN REG plus_minus REG amod1  */
#line 933 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-8].r0).r0 == (yyvsp[-2].r0).r0)
	    return yyerror ("Operators must differ");

	  if (IS_DREG ((yyvsp[-11].reg)) && IS_DREG ((yyvsp[-9].reg)) && IS_DREG ((yyvsp[-7].reg))
	      && REG_SAME ((yyvsp[-9].reg), (yyvsp[-3].reg)) && REG_SAME ((yyvsp[-7].reg), (yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = dregs + dregs,"
		       "dregs = dregs - dregs (amod1)\n");
	      (yyval.instr) = DSP32ALU (4, 0, &(yyvsp[-11].reg), &(yyvsp[-5].reg), &(yyvsp[-9].reg), &(yyvsp[-7].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, 2);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3161 "config/bfin-parse.c"
    break;

  case 23: /* asm_1: REG ASSIGN REG op_bar_op REG COMMA REG ASSIGN REG op_bar_op REG amod2  */
#line 951 "./config/bfin-parse.y"
        {
	  if (!REG_SAME ((yyvsp[-9].reg), (yyvsp[-3].reg)) || !REG_SAME ((yyvsp[-7].reg), (yyvsp[-1].reg)))
	    return yyerror ("Differing source registers");

	  if (!IS_DREG ((yyvsp[-11].reg)) || !IS_DREG ((yyvsp[-9].reg)) || !IS_DREG ((yyvsp[-7].reg)) || !IS_DREG ((yyvsp[-5].reg)))
	    return yyerror ("Dregs expected");

	  if (REG_SAME ((yyvsp[-11].reg), (yyvsp[-5].reg)))
	    return yyerror ("Resource conflict in dest reg");

	  if ((yyvsp[-8].r0).r0 == 1 && (yyvsp[-2].r0).r0 == 2)
	    {
	      notethat ("dsp32alu:  dregs = dregs .|. dregs , dregs = dregs .|. dregs (amod2)\n");
	      (yyval.instr) = DSP32ALU (1, 1, &(yyvsp[-11].reg), &(yyvsp[-5].reg), &(yyvsp[-9].reg), &(yyvsp[-7].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, (yyvsp[0].modcodes).r0);
	    }
	  else if ((yyvsp[-8].r0).r0 == 0 && (yyvsp[-2].r0).r0 == 3)
	    {
	      notethat ("dsp32alu:  dregs = dregs .|. dregs , dregs = dregs .|. dregs (amod2)\n");
	      (yyval.instr) = DSP32ALU (1, 0, &(yyvsp[-11].reg), &(yyvsp[-5].reg), &(yyvsp[-9].reg), &(yyvsp[-7].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, (yyvsp[0].modcodes).r0);
	    }
	  else
	    return yyerror ("Bar operand mismatch");
	}
#line 3189 "config/bfin-parse.c"
    break;

  case 24: /* asm_1: REG ASSIGN ABS REG vmod  */
#line 976 "./config/bfin-parse.y"
        {
	  int op;

	  if (IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[0].r0).r0)
		{
		  notethat ("dsp32alu: dregs = ABS dregs (v)\n");
		  op = 6;
		}
	      else
		{
		  /* Vector version of ABS.  */
		  notethat ("dsp32alu: dregs = ABS dregs\n");
		  op = 7;
		}
	      (yyval.instr) = DSP32ALU (op, 0, 0, &(yyvsp[-4].reg), &(yyvsp[-1].reg), 0, 0, 0, 2);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3215 "config/bfin-parse.c"
    break;

  case 25: /* asm_1: a_assign ABS REG_A  */
#line 998 "./config/bfin-parse.y"
        {
	  notethat ("dsp32alu: Ax = ABS Ax\n");
	  (yyval.instr) = DSP32ALU (16, IS_A1 ((yyvsp[-2].reg)), 0, 0, &reg7, &reg7, 0, 0, IS_A1 ((yyvsp[0].reg)));
	}
#line 3224 "config/bfin-parse.c"
    break;

  case 26: /* asm_1: A_ZERO_DOT_L ASSIGN HALF_REG  */
#line 1003 "./config/bfin-parse.y"
        {
	  if (IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32alu: A0.l = reg_half\n");
	      (yyval.instr) = DSP32ALU (9, IS_H ((yyvsp[0].reg)), 0, 0, &(yyvsp[0].reg), 0, 0, 0, 0);
	    }
	  else
	    return yyerror ("A0.l = Rx.l expected");
	}
#line 3238 "config/bfin-parse.c"
    break;

  case 27: /* asm_1: A_ONE_DOT_L ASSIGN HALF_REG  */
#line 1013 "./config/bfin-parse.y"
        {
	  if (IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32alu: A1.l = reg_half\n");
	      (yyval.instr) = DSP32ALU (9, IS_H ((yyvsp[0].reg)), 0, 0, &(yyvsp[0].reg), 0, 0, 0, 2);
	    }
	  else
	    return yyerror ("A1.l = Rx.l expected");
	}
#line 3252 "config/bfin-parse.c"
    break;

  case 28: /* asm_1: REG ASSIGN c_align LPAREN REG COMMA REG RPAREN  */
#line 1024 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs = ALIGN8 (dregs , dregs )\n");
	      (yyval.instr) = DSP32SHIFT (13, &(yyvsp[-7].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), (yyvsp[-5].r0).r0, 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3266 "config/bfin-parse.c"
    break;

  case 29: /* asm_1: REG ASSIGN BYTEOP1P LPAREN REG COLON expr COMMA REG COLON expr RPAREN byteop_mod  */
#line 1035 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-12].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: dregs = BYTEOP1P (dregs_pair , dregs_pair ) (T)\n");
	      (yyval.instr) = DSP32ALU (20, 0, 0, &(yyvsp[-12].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].modcodes).s0, 0, (yyvsp[0].modcodes).r0);
	    }
	}
#line 3284 "config/bfin-parse.c"
    break;

  case 30: /* asm_1: REG ASSIGN BYTEOP1P LPAREN REG COLON expr COMMA REG COLON expr RPAREN  */
#line 1049 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-11].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-7].reg), (yyvsp[-5].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-3].reg), (yyvsp[-1].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: dregs = BYTEOP1P (dregs_pair , dregs_pair ) (T)\n");
	      (yyval.instr) = DSP32ALU (20, 0, 0, &(yyvsp[-11].reg), &(yyvsp[-7].reg), &(yyvsp[-3].reg), 0, 0, 0);
	    }
	}
#line 3302 "config/bfin-parse.c"
    break;

  case 31: /* asm_1: REG ASSIGN BYTEOP2P LPAREN REG COLON expr COMMA REG COLON expr RPAREN rnd_op  */
#line 1065 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-12].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: dregs = BYTEOP2P (dregs_pair , dregs_pair ) (rnd_op)\n");
	      (yyval.instr) = DSP32ALU (22, (yyvsp[0].modcodes).r0, 0, &(yyvsp[-12].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, (yyvsp[0].modcodes).aop);
	    }
	}
#line 3320 "config/bfin-parse.c"
    break;

  case 32: /* asm_1: REG ASSIGN BYTEOP3P LPAREN REG COLON expr COMMA REG COLON expr RPAREN b3_op  */
#line 1081 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-12].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: dregs = BYTEOP3P (dregs_pair , dregs_pair ) (b3_op)\n");
	      (yyval.instr) = DSP32ALU (23, (yyvsp[0].modcodes).x0, 0, &(yyvsp[-12].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].modcodes).s0, 0, 0);
	    }
	}
#line 3338 "config/bfin-parse.c"
    break;

  case 33: /* asm_1: REG ASSIGN BYTEPACK LPAREN REG COMMA REG RPAREN  */
#line 1096 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = BYTEPACK (dregs , dregs )\n");
	      (yyval.instr) = DSP32ALU (24, 0, 0, &(yyvsp[-7].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg), 0, 0, 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3352 "config/bfin-parse.c"
    break;

  case 34: /* asm_1: HALF_REG ASSIGN HALF_REG ASSIGN SIGN LPAREN HALF_REG RPAREN STAR HALF_REG PLUS SIGN LPAREN HALF_REG RPAREN STAR HALF_REG  */
#line 1108 "./config/bfin-parse.y"
        {
	  if (IS_HCOMPL ((yyvsp[-16].reg), (yyvsp[-14].reg)) && IS_HCOMPL ((yyvsp[-10].reg), (yyvsp[-3].reg)) && IS_HCOMPL ((yyvsp[-7].reg), (yyvsp[0].reg)))
	    {
	      notethat ("dsp32alu:	dregs_hi = dregs_lo ="
		       "SIGN (dregs_hi) * dregs_hi + "
		       "SIGN (dregs_lo) * dregs_lo \n");

		(yyval.instr) = DSP32ALU (12, 0, 0, &(yyvsp[-16].reg), &(yyvsp[-10].reg), &(yyvsp[-7].reg), 0, 0, 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3369 "config/bfin-parse.c"
    break;

  case 35: /* asm_1: REG ASSIGN REG plus_minus REG amod1  */
#line 1121 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[0].modcodes).aop == 0)
		{
	          /* No saturation flag specified, generate the 16 bit variant.  */
		  notethat ("COMP3op: dregs = dregs +- dregs\n");
		  (yyval.instr) = COMP3OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg), (yyvsp[-2].r0).r0);
		}
	      else
		{
		 /* Saturation flag specified, generate the 32 bit variant.  */
                 notethat ("dsp32alu: dregs = dregs +- dregs (amod1)\n");
                 (yyval.instr) = DSP32ALU (4, 0, 0, &(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, (yyvsp[-2].r0).r0);
		}
	    }
	  else
	    if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)) && IS_PREG ((yyvsp[-1].reg)) && (yyvsp[-2].r0).r0 == 0)
	      {
		notethat ("COMP3op: pregs = pregs + pregs\n");
		(yyval.instr) = COMP3OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg), 5);
	      }
	    else
	      return yyerror ("Dregs expected");
	}
#line 3399 "config/bfin-parse.c"
    break;

  case 36: /* asm_1: REG ASSIGN min_max LPAREN REG COMMA REG RPAREN vmod  */
#line 1147 "./config/bfin-parse.y"
        {
	  int op;

	  if (IS_DREG ((yyvsp[-8].reg)) && IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-2].reg)))
	    {
	      if ((yyvsp[0].r0).r0)
		op = 6;
	      else
		op = 7;

	      notethat ("dsp32alu: dregs = {MIN|MAX} (dregs, dregs)\n");
	      (yyval.instr) = DSP32ALU (op, 0, 0, &(yyvsp[-8].reg), &(yyvsp[-4].reg), &(yyvsp[-2].reg), 0, 0, (yyvsp[-6].r0).r0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3420 "config/bfin-parse.c"
    break;

  case 37: /* asm_1: a_assign MINUS REG_A  */
#line 1165 "./config/bfin-parse.y"
        {
	  notethat ("dsp32alu: Ax = - Ax\n");
	  (yyval.instr) = DSP32ALU (14, IS_A1 ((yyvsp[-2].reg)), 0, 0, &reg7, &reg7, 0, 0, IS_A1 ((yyvsp[0].reg)));
	}
#line 3429 "config/bfin-parse.c"
    break;

  case 38: /* asm_1: HALF_REG ASSIGN HALF_REG plus_minus HALF_REG amod1  */
#line 1170 "./config/bfin-parse.y"
        {
	  notethat ("dsp32alu: dregs_lo = dregs_lo +- dregs_lo (amod1)\n");
	  (yyval.instr) = DSP32ALU (2 | (yyvsp[-2].r0).r0, IS_H ((yyvsp[-5].reg)), 0, &(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg),
			 (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, HL2 ((yyvsp[-3].reg), (yyvsp[-1].reg)));
	}
#line 3439 "config/bfin-parse.c"
    break;

  case 39: /* asm_1: a_assign a_assign expr  */
#line 1176 "./config/bfin-parse.y"
        {
	  if (EXPR_VALUE ((yyvsp[0].expr)) == 0 && !REG_SAME ((yyvsp[-2].reg), (yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: A1 = A0 = 0\n");
	      (yyval.instr) = DSP32ALU (8, 0, 0, 0, &reg7, &reg7, 0, 0, 2);
	    }
	  else
	    return yyerror ("Bad value, 0 expected");
	}
#line 3453 "config/bfin-parse.c"
    break;

  case 40: /* asm_1: a_assign REG_A LPAREN S RPAREN  */
#line 1188 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-4].reg), (yyvsp[-3].reg)))
	    {
	      notethat ("dsp32alu: Ax = Ax (S)\n");
	      (yyval.instr) = DSP32ALU (8, 0, 0, 0, &reg7, &reg7, 1, 0, IS_A1 ((yyvsp[-4].reg)));
	    }
	  else
	    return yyerror ("Registers must be equal");
	}
#line 3467 "config/bfin-parse.c"
    break;

  case 41: /* asm_1: HALF_REG ASSIGN REG LPAREN RND RPAREN  */
#line 1199 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32alu: dregs_half = dregs (RND)\n");
	      (yyval.instr) = DSP32ALU (12, IS_H ((yyvsp[-5].reg)), 0, &(yyvsp[-5].reg), &(yyvsp[-3].reg), 0, 0, 0, 3);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3481 "config/bfin-parse.c"
    break;

  case 42: /* asm_1: HALF_REG ASSIGN REG plus_minus REG LPAREN RND12 RPAREN  */
#line 1210 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32alu: dregs_half = dregs (+-) dregs (RND12)\n");
	      (yyval.instr) = DSP32ALU (5, IS_H ((yyvsp[-7].reg)), 0, &(yyvsp[-7].reg), &(yyvsp[-5].reg), &(yyvsp[-3].reg), 0, 0, (yyvsp[-4].r0).r0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3495 "config/bfin-parse.c"
    break;

  case 43: /* asm_1: HALF_REG ASSIGN REG plus_minus REG LPAREN RND20 RPAREN  */
#line 1221 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32alu: dregs_half = dregs -+ dregs (RND20)\n");
	      (yyval.instr) = DSP32ALU (5, IS_H ((yyvsp[-7].reg)), 0, &(yyvsp[-7].reg), &(yyvsp[-5].reg), &(yyvsp[-3].reg), 0, 1, (yyvsp[-4].r0).r0 | 2);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3509 "config/bfin-parse.c"
    break;

  case 44: /* asm_1: a_assign REG_A  */
#line 1232 "./config/bfin-parse.y"
        {
	  if (!REG_SAME ((yyvsp[-1].reg), (yyvsp[0].reg)))
	    {
	      notethat ("dsp32alu: An = Am\n");
	      (yyval.instr) = DSP32ALU (8, 0, 0, 0, &reg7, &reg7, IS_A1 ((yyvsp[-1].reg)), 0, 3);
	    }
	  else
	    return yyerror ("Accu reg arguments must differ");
	}
#line 3523 "config/bfin-parse.c"
    break;

  case 45: /* asm_1: a_assign REG  */
#line 1243 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32alu: An = dregs\n");
	      (yyval.instr) = DSP32ALU (9, 0, 0, 0, &(yyvsp[0].reg), 0, 1, 0, IS_A1 ((yyvsp[-1].reg)) << 1);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3537 "config/bfin-parse.c"
    break;

  case 46: /* asm_1: REG ASSIGN HALF_REG xpmod  */
#line 1254 "./config/bfin-parse.y"
        {
	  if (!IS_H ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[-3].reg).regno == REG_A0x && IS_DREG ((yyvsp[-1].reg)))
		{
		  notethat ("dsp32alu: A0.x = dregs_lo\n");
		  (yyval.instr) = DSP32ALU (9, 0, 0, 0, &(yyvsp[-1].reg), 0, 0, 0, 1);
		}
	      else if ((yyvsp[-3].reg).regno == REG_A1x && IS_DREG ((yyvsp[-1].reg)))
		{
		  notethat ("dsp32alu: A1.x = dregs_lo\n");
		  (yyval.instr) = DSP32ALU (9, 0, 0, 0, &(yyvsp[-1].reg), 0, 0, 0, 3);
		}
	      else if (IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
		{
		  notethat ("ALU2op: dregs = dregs_lo\n");
		  (yyval.instr) = ALU2OP (&(yyvsp[-3].reg), &(yyvsp[-1].reg), 10 | ((yyvsp[0].r0).r0 ? 0: 1));
		}
	      else
	        return yyerror ("Register mismatch");
	    }
	  else
	    return yyerror ("Low reg expected");
	}
#line 3566 "config/bfin-parse.c"
    break;

  case 47: /* asm_1: HALF_REG ASSIGN expr  */
#line 1280 "./config/bfin-parse.y"
        {
	  notethat ("LDIMMhalf: pregs_half = imm16\n");

	  if (!IS_DREG ((yyvsp[-2].reg)) && !IS_PREG ((yyvsp[-2].reg)) && !IS_IREG ((yyvsp[-2].reg))
	      && !IS_MREG ((yyvsp[-2].reg)) && !IS_BREG ((yyvsp[-2].reg)) && !IS_LREG ((yyvsp[-2].reg)))
	    return yyerror ("Wrong register for load immediate");

	  if (!IS_IMM ((yyvsp[0].expr), 16) && !IS_UIMM ((yyvsp[0].expr), 16))
	    return yyerror ("Constant out of range");

	  (yyval.instr) = LDIMMHALF_R (&(yyvsp[-2].reg), IS_H ((yyvsp[-2].reg)), 0, 0, (yyvsp[0].expr));
	}
#line 3583 "config/bfin-parse.c"
    break;

  case 48: /* asm_1: a_assign expr  */
#line 1294 "./config/bfin-parse.y"
        {
	  notethat ("dsp32alu: An = 0\n");

	  if (imm7 ((yyvsp[0].expr)) != 0)
	    return yyerror ("0 expected");

	  (yyval.instr) = DSP32ALU (8, 0, 0, 0, 0, 0, 0, 0, IS_A1 ((yyvsp[-1].reg)));
	}
#line 3596 "config/bfin-parse.c"
    break;

  case 49: /* asm_1: REG ASSIGN expr xpmod1  */
#line 1304 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-3].reg)) && !IS_PREG ((yyvsp[-3].reg)) && !IS_IREG ((yyvsp[-3].reg))
	      && !IS_MREG ((yyvsp[-3].reg)) && !IS_BREG ((yyvsp[-3].reg)) && !IS_LREG ((yyvsp[-3].reg)))
	    return yyerror ("Wrong register for load immediate");

	  if ((yyvsp[0].r0).r0 == 0)
	    {
	      /* 7 bit immediate value if possible.
		 We will check for that constant value for efficiency
		 If it goes to reloc, it will be 16 bit.  */
	      if (IS_CONST ((yyvsp[-1].expr)) && IS_IMM ((yyvsp[-1].expr), 7) && IS_DREG ((yyvsp[-3].reg)))
		{
		  notethat ("COMPI2opD: dregs = imm7 (x) \n");
		  (yyval.instr) = COMPI2OPD (&(yyvsp[-3].reg), imm7 ((yyvsp[-1].expr)), 0);
		}
	      else if (IS_CONST ((yyvsp[-1].expr)) && IS_IMM ((yyvsp[-1].expr), 7) && IS_PREG ((yyvsp[-3].reg)))
		{
		  notethat ("COMPI2opP: pregs = imm7 (x)\n");
		  (yyval.instr) = COMPI2OPP (&(yyvsp[-3].reg), imm7 ((yyvsp[-1].expr)), 0);
		}
	      else
		{
		  if (IS_CONST ((yyvsp[-1].expr)) && !IS_IMM ((yyvsp[-1].expr), 16))
		    return yyerror ("Immediate value out of range");

		  notethat ("LDIMMhalf: regs = luimm16 (x)\n");
		  /* reg, H, S, Z.   */
		  (yyval.instr) = LDIMMHALF_R5 (&(yyvsp[-3].reg), 0, 1, 0, (yyvsp[-1].expr));
		}
	    }
	  else
	    {
	      /* (z) There is no 7 bit zero extended instruction.
	      If the expr is a relocation, generate it.   */

	      if (IS_CONST ((yyvsp[-1].expr)) && !IS_UIMM ((yyvsp[-1].expr), 16))
		return yyerror ("Immediate value out of range");

	      notethat ("LDIMMhalf: regs = luimm16 (x)\n");
	      /* reg, H, S, Z.  */
	      (yyval.instr) = LDIMMHALF_R5 (&(yyvsp[-3].reg), 0, 0, 1, (yyvsp[-1].expr));
	    }
	}
#line 3644 "config/bfin-parse.c"
    break;

  case 50: /* asm_1: HALF_REG ASSIGN REG  */
#line 1349 "./config/bfin-parse.y"
        {
	  if (IS_H ((yyvsp[-2].reg)))
	    return yyerror ("Low reg expected");

	  if (IS_DREG ((yyvsp[-2].reg)) && (yyvsp[0].reg).regno == REG_A0x)
	    {
	      notethat ("dsp32alu: dregs_lo = A0.x\n");
	      (yyval.instr) = DSP32ALU (10, 0, 0, &(yyvsp[-2].reg), &reg7, &reg7, 0, 0, 0);
	    }
	  else if (IS_DREG ((yyvsp[-2].reg)) && (yyvsp[0].reg).regno == REG_A1x)
	    {
	      notethat ("dsp32alu: dregs_lo = A1.x\n");
	      (yyval.instr) = DSP32ALU (10, 0, 0, &(yyvsp[-2].reg), &reg7, &reg7, 0, 0, 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3666 "config/bfin-parse.c"
    break;

  case 51: /* asm_1: REG ASSIGN REG op_bar_op REG amod0  */
#line 1368 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = dregs .|. dregs (amod0)\n");
	      (yyval.instr) = DSP32ALU (0, 0, 0, &(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, (yyvsp[-2].r0).r0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3680 "config/bfin-parse.c"
    break;

  case 52: /* asm_1: REG ASSIGN BYTE_DREG xpmod  */
#line 1379 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ALU2op: dregs = dregs_byte\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-3].reg), &(yyvsp[-1].reg), 12 | ((yyvsp[0].r0).r0 ? 0: 1));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3694 "config/bfin-parse.c"
    break;

  case 53: /* asm_1: a_assign ABS REG_A COMMA a_assign ABS REG_A  */
#line 1390 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-6].reg), (yyvsp[-4].reg)) && REG_SAME ((yyvsp[-2].reg), (yyvsp[0].reg)) && !REG_SAME ((yyvsp[-6].reg), (yyvsp[-2].reg)))
	    {
	      notethat ("dsp32alu: A1 = ABS A1 , A0 = ABS A0\n");
	      (yyval.instr) = DSP32ALU (16, 0, 0, 0, &reg7, &reg7, 0, 0, 3);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3708 "config/bfin-parse.c"
    break;

  case 54: /* asm_1: a_assign MINUS REG_A COMMA a_assign MINUS REG_A  */
#line 1401 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-6].reg), (yyvsp[-4].reg)) && REG_SAME ((yyvsp[-2].reg), (yyvsp[0].reg)) && !REG_SAME ((yyvsp[-6].reg), (yyvsp[-2].reg)))
	    {
	      notethat ("dsp32alu: A1 = - A1 , A0 = - A0\n");
	      (yyval.instr) = DSP32ALU (14, 0, 0, 0, &reg7, &reg7, 0, 0, 3);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3722 "config/bfin-parse.c"
    break;

  case 55: /* asm_1: a_minusassign REG_A w32_or_nothing  */
#line 1412 "./config/bfin-parse.y"
        {
	  if (!IS_A1 ((yyvsp[-2].reg)) && IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: A0 -= A1\n");
	      (yyval.instr) = DSP32ALU (11, 0, 0, 0, &reg7, &reg7, (yyvsp[0].r0).r0, 0, 3);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3736 "config/bfin-parse.c"
    break;

  case 56: /* asm_1: REG _MINUS_ASSIGN expr  */
#line 1423 "./config/bfin-parse.y"
        {
	  if (IS_IREG ((yyvsp[-2].reg)) && EXPR_VALUE ((yyvsp[0].expr)) == 4)
	    {
	      notethat ("dagMODik: iregs -= 4\n");
	      (yyval.instr) = DAGMODIK (&(yyvsp[-2].reg), 3);
	    }
	  else if (IS_IREG ((yyvsp[-2].reg)) && EXPR_VALUE ((yyvsp[0].expr)) == 2)
	    {
	      notethat ("dagMODik: iregs -= 2\n");
	      (yyval.instr) = DAGMODIK (&(yyvsp[-2].reg), 1);
	    }
	  else
	    return yyerror ("Register or value mismatch");
	}
#line 3755 "config/bfin-parse.c"
    break;

  case 57: /* asm_1: REG _PLUS_ASSIGN REG LPAREN BREV RPAREN  */
#line 1439 "./config/bfin-parse.y"
        {
	  if (IS_IREG ((yyvsp[-5].reg)) && IS_MREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dagMODim: iregs += mregs (opt_brev)\n");
	      /* i, m, op, br.  */
	      (yyval.instr) = DAGMODIM (&(yyvsp[-5].reg), &(yyvsp[-3].reg), 0, 1);
	    }
	  else if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      notethat ("PTR2op: pregs += pregs (BREV )\n");
	      (yyval.instr) = PTR2OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), 5);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3775 "config/bfin-parse.c"
    break;

  case 58: /* asm_1: REG _MINUS_ASSIGN REG  */
#line 1456 "./config/bfin-parse.y"
        {
	  if (IS_IREG ((yyvsp[-2].reg)) && IS_MREG ((yyvsp[0].reg)))
	    {
	      notethat ("dagMODim: iregs -= mregs\n");
	      (yyval.instr) = DAGMODIM (&(yyvsp[-2].reg), &(yyvsp[0].reg), 1, 0);
	    }
	  else if (IS_PREG ((yyvsp[-2].reg)) && IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("PTR2op: pregs -= pregs\n");
	      (yyval.instr) = PTR2OP (&(yyvsp[-2].reg), &(yyvsp[0].reg), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3794 "config/bfin-parse.c"
    break;

  case 59: /* asm_1: REG_A _PLUS_ASSIGN REG_A w32_or_nothing  */
#line 1472 "./config/bfin-parse.y"
        {
	  if (!IS_A1 ((yyvsp[-3].reg)) && IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: A0 += A1 (W32)\n");
	      (yyval.instr) = DSP32ALU (11, 0, 0, 0, &reg7, &reg7, (yyvsp[0].r0).r0, 0, 2);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3808 "config/bfin-parse.c"
    break;

  case 60: /* asm_1: REG _PLUS_ASSIGN REG  */
#line 1483 "./config/bfin-parse.y"
        {
	  if (IS_IREG ((yyvsp[-2].reg)) && IS_MREG ((yyvsp[0].reg)))
	    {
	      notethat ("dagMODim: iregs += mregs\n");
	      (yyval.instr) = DAGMODIM (&(yyvsp[-2].reg), &(yyvsp[0].reg), 0, 0);
	    }
	  else
	    return yyerror ("iregs += mregs expected");
	}
#line 3822 "config/bfin-parse.c"
    break;

  case 61: /* asm_1: REG _PLUS_ASSIGN expr  */
#line 1494 "./config/bfin-parse.y"
        {
	  if (IS_IREG ((yyvsp[-2].reg)))
	    {
	      if (EXPR_VALUE ((yyvsp[0].expr)) == 4)
		{
		  notethat ("dagMODik: iregs += 4\n");
		  (yyval.instr) = DAGMODIK (&(yyvsp[-2].reg), 2);
		}
	      else if (EXPR_VALUE ((yyvsp[0].expr)) == 2)
		{
		  notethat ("dagMODik: iregs += 2\n");
		  (yyval.instr) = DAGMODIK (&(yyvsp[-2].reg), 0);
		}
	      else
		return yyerror ("iregs += [ 2 | 4 ");
	    }
	  else if (IS_PREG ((yyvsp[-2].reg)) && IS_IMM ((yyvsp[0].expr), 7))
	    {
	      notethat ("COMPI2opP: pregs += imm7\n");
	      (yyval.instr) = COMPI2OPP (&(yyvsp[-2].reg), imm7 ((yyvsp[0].expr)), 1);
	    }
	  else if (IS_DREG ((yyvsp[-2].reg)) && IS_IMM ((yyvsp[0].expr), 7))
	    {
	      notethat ("COMPI2opD: dregs += imm7\n");
	      (yyval.instr) = COMPI2OPD (&(yyvsp[-2].reg), imm7 ((yyvsp[0].expr)), 1);
	    }
	  else if ((IS_DREG ((yyvsp[-2].reg)) || IS_PREG ((yyvsp[-2].reg))) && IS_CONST ((yyvsp[0].expr)))
	    return yyerror ("Immediate value out of range");
	  else
	    return yyerror ("Register mismatch");
	}
#line 3858 "config/bfin-parse.c"
    break;

  case 62: /* asm_1: REG _STAR_ASSIGN REG  */
#line 1527 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ALU2op: dregs *= dregs\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-2].reg), &(yyvsp[0].reg), 3);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3872 "config/bfin-parse.c"
    break;

  case 63: /* asm_1: SAA LPAREN REG COLON expr COMMA REG COLON expr RPAREN aligndir  */
#line 1538 "./config/bfin-parse.y"
        {
	  if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: SAA (dregs_pair , dregs_pair ) (aligndir)\n");
	      (yyval.instr) = DSP32ALU (18, 0, 0, 0, &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0, 0);
	    }
	}
#line 3888 "config/bfin-parse.c"
    break;

  case 64: /* asm_1: a_assign REG_A LPAREN S RPAREN COMMA a_assign REG_A LPAREN S RPAREN  */
#line 1551 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-10].reg), (yyvsp[-9].reg)) && REG_SAME ((yyvsp[-4].reg), (yyvsp[-3].reg)) && !REG_SAME ((yyvsp[-10].reg), (yyvsp[-4].reg)))
	    {
	      notethat ("dsp32alu: A1 = A1 (S) , A0 = A0 (S)\n");
	      (yyval.instr) = DSP32ALU (8, 0, 0, 0, &reg7, &reg7, 1, 0, 2);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3902 "config/bfin-parse.c"
    break;

  case 65: /* asm_1: REG ASSIGN LPAREN REG PLUS REG RPAREN LESS_LESS expr  */
#line 1562 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-8].reg)) && IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg))
	      && REG_SAME ((yyvsp[-8].reg), (yyvsp[-5].reg)))
	    {
	      if (EXPR_VALUE ((yyvsp[0].expr)) == 1)
		{
		  notethat ("ALU2op: dregs = (dregs + dregs) << 1\n");
		  (yyval.instr) = ALU2OP (&(yyvsp[-8].reg), &(yyvsp[-3].reg), 4);
		}
	      else if (EXPR_VALUE ((yyvsp[0].expr)) == 2)
		{
		  notethat ("ALU2op: dregs = (dregs + dregs) << 2\n");
		  (yyval.instr) = ALU2OP (&(yyvsp[-8].reg), &(yyvsp[-3].reg), 5);
		}
	      else
		return yyerror ("Bad shift value");
	    }
	  else if (IS_PREG ((yyvsp[-8].reg)) && IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg))
		   && REG_SAME ((yyvsp[-8].reg), (yyvsp[-5].reg)))
	    {
	      if (EXPR_VALUE ((yyvsp[0].expr)) == 1)
		{
		  notethat ("PTR2op: pregs = (pregs + pregs) << 1\n");
		  (yyval.instr) = PTR2OP (&(yyvsp[-8].reg), &(yyvsp[-3].reg), 6);
		}
	      else if (EXPR_VALUE ((yyvsp[0].expr)) == 2)
		{
		  notethat ("PTR2op: pregs = (pregs + pregs) << 2\n");
		  (yyval.instr) = PTR2OP (&(yyvsp[-8].reg), &(yyvsp[-3].reg), 7);
		}
	      else
		return yyerror ("Bad shift value");
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 3943 "config/bfin-parse.c"
    break;

  case 66: /* asm_1: REG ASSIGN REG BAR REG  */
#line 1601 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("COMP3op: dregs = dregs | dregs\n");
	      (yyval.instr) = COMP3OP (&(yyvsp[-4].reg), &(yyvsp[-2].reg), &(yyvsp[0].reg), 3);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3957 "config/bfin-parse.c"
    break;

  case 67: /* asm_1: REG ASSIGN REG CARET REG  */
#line 1611 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("COMP3op: dregs = dregs ^ dregs\n");
	      (yyval.instr) = COMP3OP (&(yyvsp[-4].reg), &(yyvsp[-2].reg), &(yyvsp[0].reg), 4);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3971 "config/bfin-parse.c"
    break;

  case 68: /* asm_1: REG ASSIGN REG PLUS LPAREN REG LESS_LESS expr RPAREN  */
#line 1621 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[-8].reg)) && IS_PREG ((yyvsp[-6].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      if (EXPR_VALUE ((yyvsp[-1].expr)) == 1)
		{
		  notethat ("COMP3op: pregs = pregs + (pregs << 1)\n");
		  (yyval.instr) = COMP3OP (&(yyvsp[-8].reg), &(yyvsp[-6].reg), &(yyvsp[-3].reg), 6);
		}
	      else if (EXPR_VALUE ((yyvsp[-1].expr)) == 2)
		{
		  notethat ("COMP3op: pregs = pregs + (pregs << 2)\n");
		  (yyval.instr) = COMP3OP (&(yyvsp[-8].reg), &(yyvsp[-6].reg), &(yyvsp[-3].reg), 7);
		}
	      else
		  return yyerror ("Bad shift value");
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 3995 "config/bfin-parse.c"
    break;

  case 69: /* asm_1: CCREG ASSIGN REG_A _ASSIGN_ASSIGN REG_A  */
#line 1641 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-2].reg).regno == REG_A0 && (yyvsp[0].reg).regno == REG_A1)
	    {
	      notethat ("CCflag: CC = A0 == A1\n");
	      (yyval.instr) = CCFLAG (0, 0, 5, 0, 0);
	    }
	  else
	    return yyerror ("AREGs are in bad order or same");
	}
#line 4009 "config/bfin-parse.c"
    break;

  case 70: /* asm_1: CCREG ASSIGN REG_A LESS_THAN REG_A  */
#line 1651 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-2].reg).regno == REG_A0 && (yyvsp[0].reg).regno == REG_A1)
	    {
	      notethat ("CCflag: CC = A0 < A1\n");
	      (yyval.instr) = CCFLAG (0, 0, 6, 0, 0);
	    }
	  else
	    return yyerror ("AREGs are in bad order or same");
	}
#line 4023 "config/bfin-parse.c"
    break;

  case 71: /* asm_1: CCREG ASSIGN REG LESS_THAN REG iu_or_nothing  */
#line 1661 "./config/bfin-parse.y"
        {
	  if ((IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	      || (IS_PREG ((yyvsp[-3].reg)) && IS_PREG ((yyvsp[-1].reg))))
	    {
	      notethat ("CCflag: CC = dpregs < dpregs\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-3].reg), (yyvsp[-1].reg).regno & CODE_MASK, (yyvsp[0].r0).r0, 0, IS_PREG ((yyvsp[-3].reg)) ? 1 : 0);
	    }
	  else
	    return yyerror ("Bad register in comparison");
	}
#line 4038 "config/bfin-parse.c"
    break;

  case 72: /* asm_1: CCREG ASSIGN REG LESS_THAN expr iu_or_nothing  */
#line 1672 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-3].reg)) && !IS_PREG ((yyvsp[-3].reg)))
	    return yyerror ("Bad register in comparison");

	  if (((yyvsp[0].r0).r0 == 1 && IS_IMM ((yyvsp[-1].expr), 3))
	      || ((yyvsp[0].r0).r0 == 3 && IS_UIMM ((yyvsp[-1].expr), 3)))
	    {
	      notethat ("CCflag: CC = dpregs < (u)imm3\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-3].reg), imm3 ((yyvsp[-1].expr)), (yyvsp[0].r0).r0, 1, IS_PREG ((yyvsp[-3].reg)) ? 1 : 0);
	    }
	  else
	    return yyerror ("Bad constant value");
	}
#line 4056 "config/bfin-parse.c"
    break;

  case 73: /* asm_1: CCREG ASSIGN REG _ASSIGN_ASSIGN REG  */
#line 1686 "./config/bfin-parse.y"
        {
	  if ((IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	      || (IS_PREG ((yyvsp[-2].reg)) && IS_PREG ((yyvsp[0].reg))))
	    {
	      notethat ("CCflag: CC = dpregs == dpregs\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-2].reg), (yyvsp[0].reg).regno & CODE_MASK, 0, 0, IS_PREG ((yyvsp[-2].reg)) ? 1 : 0);
	    }
	  else
	    return yyerror ("Bad register in comparison");
	}
#line 4071 "config/bfin-parse.c"
    break;

  case 74: /* asm_1: CCREG ASSIGN REG _ASSIGN_ASSIGN expr  */
#line 1697 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-2].reg)) && !IS_PREG ((yyvsp[-2].reg)))
	    return yyerror ("Bad register in comparison");

	  if (IS_IMM ((yyvsp[0].expr), 3))
	    {
	      notethat ("CCflag: CC = dpregs == imm3\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-2].reg), imm3 ((yyvsp[0].expr)), 0, 1, IS_PREG ((yyvsp[-2].reg)) ? 1 : 0);
	    }
	  else
	    return yyerror ("Bad constant range");
	}
#line 4088 "config/bfin-parse.c"
    break;

  case 75: /* asm_1: CCREG ASSIGN REG_A _LESS_THAN_ASSIGN REG_A  */
#line 1710 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-2].reg).regno == REG_A0 && (yyvsp[0].reg).regno == REG_A1)
	    {
	      notethat ("CCflag: CC = A0 <= A1\n");
	      (yyval.instr) = CCFLAG (0, 0, 7, 0, 0);
	    }
	  else
	    return yyerror ("AREGs are in bad order or same");
	}
#line 4102 "config/bfin-parse.c"
    break;

  case 76: /* asm_1: CCREG ASSIGN REG _LESS_THAN_ASSIGN REG iu_or_nothing  */
#line 1720 "./config/bfin-parse.y"
        {
	  if ((IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	      || (IS_PREG ((yyvsp[-3].reg)) && IS_PREG ((yyvsp[-1].reg))))
	    {
	      notethat ("CCflag: CC = dpregs <= dpregs (..)\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-3].reg), (yyvsp[-1].reg).regno & CODE_MASK,
			   1 + (yyvsp[0].r0).r0, 0, IS_PREG ((yyvsp[-3].reg)) ? 1 : 0);
	    }
	  else
	    return yyerror ("Bad register in comparison");
	}
#line 4118 "config/bfin-parse.c"
    break;

  case 77: /* asm_1: CCREG ASSIGN REG _LESS_THAN_ASSIGN expr iu_or_nothing  */
#line 1732 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-3].reg)) && !IS_PREG ((yyvsp[-3].reg)))
	    return yyerror ("Bad register in comparison");

	  if (((yyvsp[0].r0).r0 == 1 && IS_IMM ((yyvsp[-1].expr), 3))
	      || ((yyvsp[0].r0).r0 == 3 && IS_UIMM ((yyvsp[-1].expr), 3)))
	    {
	      notethat ("CCflag: CC = dpregs <= (u)imm3\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-3].reg), imm3 ((yyvsp[-1].expr)), 1 + (yyvsp[0].r0).r0, 1, IS_PREG ((yyvsp[-3].reg)) ? 1 : 0);
	    }
	  else
	    return yyerror ("Bad constant value");
	}
#line 4136 "config/bfin-parse.c"
    break;

  case 78: /* asm_1: REG ASSIGN REG AMPERSAND REG  */
#line 1747 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("COMP3op: dregs = dregs & dregs\n");
	      (yyval.instr) = COMP3OP (&(yyvsp[-4].reg), &(yyvsp[-2].reg), &(yyvsp[0].reg), 2);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 4150 "config/bfin-parse.c"
    break;

  case 79: /* asm_1: ccstat  */
#line 1758 "./config/bfin-parse.y"
        {
	  notethat ("CC2stat operation\n");
	  (yyval.instr) = bfin_gen_cc2stat ((yyvsp[0].modcodes).r0, (yyvsp[0].modcodes).x0, (yyvsp[0].modcodes).s0);
	}
#line 4159 "config/bfin-parse.c"
    break;

  case 80: /* asm_1: REG ASSIGN REG  */
#line 1764 "./config/bfin-parse.y"
        {
	  if ((IS_GENREG ((yyvsp[-2].reg)) && IS_GENREG ((yyvsp[0].reg)))
	      || (IS_GENREG ((yyvsp[-2].reg)) && IS_DAGREG ((yyvsp[0].reg)))
	      || (IS_DAGREG ((yyvsp[-2].reg)) && IS_GENREG ((yyvsp[0].reg)))
	      || (IS_DAGREG ((yyvsp[-2].reg)) && IS_DAGREG ((yyvsp[0].reg)))
	      || (IS_GENREG ((yyvsp[-2].reg)) && (yyvsp[0].reg).regno == REG_USP)
	      || ((yyvsp[-2].reg).regno == REG_USP && IS_GENREG ((yyvsp[0].reg)))
	      || ((yyvsp[-2].reg).regno == REG_USP && (yyvsp[0].reg).regno == REG_USP)
	      || (IS_DREG ((yyvsp[-2].reg)) && IS_SYSREG ((yyvsp[0].reg)))
	      || (IS_PREG ((yyvsp[-2].reg)) && IS_SYSREG ((yyvsp[0].reg)))
	      || (IS_SYSREG ((yyvsp[-2].reg)) && IS_GENREG ((yyvsp[0].reg)))
	      || (IS_ALLREG ((yyvsp[-2].reg)) && IS_EMUDAT ((yyvsp[0].reg)))
	      || (IS_EMUDAT ((yyvsp[-2].reg)) && IS_ALLREG ((yyvsp[0].reg)))
	      || (IS_SYSREG ((yyvsp[-2].reg)) && (yyvsp[0].reg).regno == REG_USP))
	    {
	      (yyval.instr) = bfin_gen_regmv (&(yyvsp[0].reg), &(yyvsp[-2].reg));
	    }
	  else
	    return yyerror ("Unsupported register move");
	}
#line 4184 "config/bfin-parse.c"
    break;

  case 81: /* asm_1: CCREG ASSIGN REG  */
#line 1786 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("CC2dreg: CC = dregs\n");
	      (yyval.instr) = bfin_gen_cc2dreg (1, &(yyvsp[0].reg));
	    }
	  else
	    return yyerror ("Only 'CC = Dreg' supported");
	}
#line 4198 "config/bfin-parse.c"
    break;

  case 82: /* asm_1: REG ASSIGN CCREG  */
#line 1797 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-2].reg)))
	    {
	      notethat ("CC2dreg: dregs = CC\n");
	      (yyval.instr) = bfin_gen_cc2dreg (0, &(yyvsp[-2].reg));
	    }
	  else
	    return yyerror ("Only 'Dreg = CC' supported");
	}
#line 4212 "config/bfin-parse.c"
    break;

  case 83: /* asm_1: CCREG ASSIGN BANG CCREG  */
#line 1808 "./config/bfin-parse.y"
        {
	  notethat ("CC2dreg: CC =! CC\n");
	  (yyval.instr) = bfin_gen_cc2dreg (3, 0);
	}
#line 4221 "config/bfin-parse.c"
    break;

  case 84: /* asm_1: HALF_REG ASSIGN multiply_halfregs opt_mode  */
#line 1816 "./config/bfin-parse.y"
        {
	  notethat ("dsp32mult: dregs_half = multiply_halfregs (opt_mode)\n");

	  if (!IS_H ((yyvsp[-3].reg)) && (yyvsp[0].mod).MM)
	    return yyerror ("(M) not allowed with MAC0");

	  if ((yyvsp[0].mod).mod != 0 && (yyvsp[0].mod).mod != M_FU && (yyvsp[0].mod).mod != M_IS
	      && (yyvsp[0].mod).mod != M_IU && (yyvsp[0].mod).mod != M_T && (yyvsp[0].mod).mod != M_TFU
	      && (yyvsp[0].mod).mod != M_S2RND && (yyvsp[0].mod).mod != M_ISS2 && (yyvsp[0].mod).mod != M_IH)
	    return yyerror ("bad option.");

	  if (IS_H ((yyvsp[-3].reg)))
	    {
	      (yyval.instr) = DSP32MULT (0, (yyvsp[0].mod).MM, (yyvsp[0].mod).mod, 1, 0,
			      IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1), 0, 0,
			      &(yyvsp[-3].reg), 0, &(yyvsp[-1].macfunc).s0, &(yyvsp[-1].macfunc).s1, 0);
	    }
	  else
	    {
	      (yyval.instr) = DSP32MULT (0, 0, (yyvsp[0].mod).mod, 0, 0,
			      0, 0, IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1),
			      &(yyvsp[-3].reg), 0, &(yyvsp[-1].macfunc).s0, &(yyvsp[-1].macfunc).s1, 1);
	    }
	}
#line 4250 "config/bfin-parse.c"
    break;

  case 85: /* asm_1: REG ASSIGN multiply_halfregs opt_mode  */
#line 1842 "./config/bfin-parse.y"
        {
	  /* Odd registers can use (M).  */
	  if (!IS_DREG ((yyvsp[-3].reg)))
	    return yyerror ("Dreg expected");

	  if (IS_EVEN ((yyvsp[-3].reg)) && (yyvsp[0].mod).MM)
	    return yyerror ("(M) not allowed with MAC0");

	  if ((yyvsp[0].mod).mod != 0 && (yyvsp[0].mod).mod != M_FU && (yyvsp[0].mod).mod != M_IS
	      && (yyvsp[0].mod).mod != M_S2RND && (yyvsp[0].mod).mod != M_ISS2)
	    return yyerror ("bad option");

	  if (!IS_EVEN ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32mult: dregs = multiply_halfregs (opt_mode)\n");

	      (yyval.instr) = DSP32MULT (0, (yyvsp[0].mod).MM, (yyvsp[0].mod).mod, 1, 1,
			      IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1), 0, 0,
			      &(yyvsp[-3].reg), 0, &(yyvsp[-1].macfunc).s0, &(yyvsp[-1].macfunc).s1, 0);
	    }
	  else
	    {
	      notethat ("dsp32mult: dregs = multiply_halfregs opt_mode\n");
	      (yyval.instr) = DSP32MULT (0, 0, (yyvsp[0].mod).mod, 0, 1,
			      0, 0, IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1),
			      &(yyvsp[-3].reg),  0, &(yyvsp[-1].macfunc).s0, &(yyvsp[-1].macfunc).s1, 1);
	    }
	}
#line 4283 "config/bfin-parse.c"
    break;

  case 86: /* asm_1: HALF_REG ASSIGN multiply_halfregs opt_mode COMMA HALF_REG ASSIGN multiply_halfregs opt_mode  */
#line 1873 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-8].reg)) || !IS_DREG ((yyvsp[-3].reg)))
	    return yyerror ("Dregs expected");

	  if (!IS_HCOMPL((yyvsp[-8].reg), (yyvsp[-3].reg)))
	    return yyerror ("Dest registers mismatch");

	  if (check_multiply_halfregs (&(yyvsp[-6].macfunc), &(yyvsp[-1].macfunc)) < 0)
	    return -1;

	  if ((!IS_H ((yyvsp[-8].reg)) && (yyvsp[-5].mod).MM)
	      || (!IS_H ((yyvsp[-3].reg)) && (yyvsp[0].mod).MM))
	    return yyerror ("(M) not allowed with MAC0");

	  notethat ("dsp32mult: dregs_hi = multiply_halfregs mxd_mod, "
		    "dregs_lo = multiply_halfregs opt_mode\n");

	  if (IS_H ((yyvsp[-8].reg)))
	    (yyval.instr) = DSP32MULT (0, (yyvsp[-5].mod).MM, (yyvsp[0].mod).mod, 1, 0,
			    IS_H ((yyvsp[-6].macfunc).s0), IS_H ((yyvsp[-6].macfunc).s1), IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1),
			    &(yyvsp[-8].reg), 0, &(yyvsp[-6].macfunc).s0, &(yyvsp[-6].macfunc).s1, 1);
	  else
	    (yyval.instr) = DSP32MULT (0, (yyvsp[0].mod).MM, (yyvsp[0].mod).mod, 1, 0,
			    IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1), IS_H ((yyvsp[-6].macfunc).s0), IS_H ((yyvsp[-6].macfunc).s1),
			    &(yyvsp[-8].reg), 0, &(yyvsp[-6].macfunc).s0, &(yyvsp[-6].macfunc).s1, 1);
	}
#line 4314 "config/bfin-parse.c"
    break;

  case 87: /* asm_1: REG ASSIGN multiply_halfregs opt_mode COMMA REG ASSIGN multiply_halfregs opt_mode  */
#line 1901 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-8].reg)) || !IS_DREG ((yyvsp[-3].reg)))
	    return yyerror ("Dregs expected");

	  if ((IS_EVEN ((yyvsp[-8].reg)) && (yyvsp[-3].reg).regno - (yyvsp[-8].reg).regno != 1)
	      || (IS_EVEN ((yyvsp[-3].reg)) && (yyvsp[-8].reg).regno - (yyvsp[-3].reg).regno != 1))
	    return yyerror ("Dest registers mismatch");

	  if (check_multiply_halfregs (&(yyvsp[-6].macfunc), &(yyvsp[-1].macfunc)) < 0)
	    return -1;

	  if ((IS_EVEN ((yyvsp[-8].reg)) && (yyvsp[-5].mod).MM)
	      || (IS_EVEN ((yyvsp[-3].reg)) && (yyvsp[0].mod).MM))
	    return yyerror ("(M) not allowed with MAC0");

	  notethat ("dsp32mult: dregs = multiply_halfregs mxd_mod, "
		   "dregs = multiply_halfregs opt_mode\n");

	  if (IS_EVEN ((yyvsp[-8].reg)))
	    (yyval.instr) = DSP32MULT (0, (yyvsp[0].mod).MM, (yyvsp[0].mod).mod, 1, 1,
			    IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1), IS_H ((yyvsp[-6].macfunc).s0), IS_H ((yyvsp[-6].macfunc).s1),
			    &(yyvsp[-8].reg), 0, &(yyvsp[-6].macfunc).s0, &(yyvsp[-6].macfunc).s1, 1);
	  else
	    (yyval.instr) = DSP32MULT (0, (yyvsp[-5].mod).MM, (yyvsp[0].mod).mod, 1, 1,
			    IS_H ((yyvsp[-6].macfunc).s0), IS_H ((yyvsp[-6].macfunc).s1), IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1),
			    &(yyvsp[-8].reg), 0, &(yyvsp[-6].macfunc).s0, &(yyvsp[-6].macfunc).s1, 1);
	}
#line 4346 "config/bfin-parse.c"
    break;

  case 88: /* asm_1: a_assign ASHIFT REG_A BY HALF_REG  */
#line 1932 "./config/bfin-parse.y"
        {
	  if (!REG_SAME ((yyvsp[-4].reg), (yyvsp[-2].reg)))
	    return yyerror ("Aregs must be same");

	  if (IS_DREG ((yyvsp[0].reg)) && !IS_H ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: A0 = ASHIFT A0 BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (3, 0, &(yyvsp[0].reg), 0, 0, IS_A1 ((yyvsp[-4].reg)));
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 4363 "config/bfin-parse.c"
    break;

  case 89: /* asm_1: HALF_REG ASSIGN ASHIFT HALF_REG BY HALF_REG smod  */
#line 1946 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-1].reg)) && !IS_H ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs_half = ASHIFT dregs_half BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (0, &(yyvsp[-6].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0, HL2 ((yyvsp[-6].reg), (yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 4377 "config/bfin-parse.c"
    break;

  case 90: /* asm_1: a_assign REG_A LESS_LESS expr  */
#line 1957 "./config/bfin-parse.y"
        {
	  if (!REG_SAME ((yyvsp[-3].reg), (yyvsp[-2].reg)))
	    return yyerror ("Aregs must be same");

	  if (IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("dsp32shiftimm: A0 = A0 << uimm5\n");
	      (yyval.instr) = DSP32SHIFTIMM (3, 0, imm5 ((yyvsp[0].expr)), 0, 0, IS_A1 ((yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Bad shift value");
	}
#line 4394 "config/bfin-parse.c"
    break;

  case 91: /* asm_1: REG ASSIGN REG LESS_LESS expr vsmod  */
#line 1971 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      if ((yyvsp[0].modcodes).r0)
		{
		  /*  Vector?  */
		  notethat ("dsp32shiftimm: dregs = dregs << expr (V, .)\n");
		  (yyval.instr) = DSP32SHIFTIMM (1, &(yyvsp[-5].reg), imm4 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0 ? 1 : 2, 0);
		}
	      else
		{
		  notethat ("dsp32shiftimm: dregs =  dregs << uimm5 (.)\n");
		  (yyval.instr) = DSP32SHIFTIMM (2, &(yyvsp[-5].reg), imm6 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0 ? 1 : 2, 0);
		}
	    }
	  else if ((yyvsp[0].modcodes).s0 == 0 && IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      if (EXPR_VALUE ((yyvsp[-1].expr)) == 2)
		{
		  notethat ("PTR2op: pregs = pregs << 2\n");
		  (yyval.instr) = PTR2OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), 1);
		}
	      else if (EXPR_VALUE ((yyvsp[-1].expr)) == 1)
		{
		  notethat ("COMP3op: pregs = pregs << 1\n");
		  (yyval.instr) = COMP3OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-3].reg), 5);
		}
	      else
		return yyerror ("Bad shift value");
	    }
	  else
	    return yyerror ("Bad shift value or register");
	}
#line 4432 "config/bfin-parse.c"
    break;

  case 92: /* asm_1: HALF_REG ASSIGN HALF_REG LESS_LESS expr smod  */
#line 2005 "./config/bfin-parse.y"
        {
	  if (IS_UIMM ((yyvsp[-1].expr), 4))
	    {
	      if ((yyvsp[0].modcodes).s0)
		{
		  notethat ("dsp32shiftimm: dregs_half = dregs_half << uimm4 (S)\n");
		  (yyval.instr) = DSP32SHIFTIMM (0x0, &(yyvsp[-5].reg), imm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0, HL2 ((yyvsp[-5].reg), (yyvsp[-3].reg)));
		}
	      else
		{
		  notethat ("dsp32shiftimm: dregs_half = dregs_half << uimm4\n");
		  (yyval.instr) = DSP32SHIFTIMM (0x0, &(yyvsp[-5].reg), imm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), 2, HL2 ((yyvsp[-5].reg), (yyvsp[-3].reg)));
		}
	    }
	  else
	    return yyerror ("Bad shift value");
	}
#line 4454 "config/bfin-parse.c"
    break;

  case 93: /* asm_1: REG ASSIGN ASHIFT REG BY HALF_REG vsmod  */
#line 2023 "./config/bfin-parse.y"
        {
	  int op;

	  if (IS_DREG ((yyvsp[-6].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)) && !IS_H ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[0].modcodes).r0)
		{
		  op = 1;
		  notethat ("dsp32shift: dregs = ASHIFT dregs BY "
			   "dregs_lo (V, .)\n");
		}
	      else
		{

		  op = 2;
		  notethat ("dsp32shift: dregs = ASHIFT dregs BY dregs_lo (.)\n");
		}
	      (yyval.instr) = DSP32SHIFT (op, &(yyvsp[-6].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0, 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 4481 "config/bfin-parse.c"
    break;

  case 94: /* asm_1: HALF_REG ASSIGN EXPADJ LPAREN REG COMMA HALF_REG RPAREN vmod  */
#line 2048 "./config/bfin-parse.y"
        {
	  if (IS_DREG_L ((yyvsp[-8].reg)) && IS_DREG_L ((yyvsp[-4].reg)) && IS_DREG_L ((yyvsp[-2].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = EXPADJ (dregs , dregs_lo )\n");
	      (yyval.instr) = DSP32SHIFT (7, &(yyvsp[-8].reg), &(yyvsp[-2].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0);
	    }
	  else
	    return yyerror ("Bad shift value or register");
	}
#line 4495 "config/bfin-parse.c"
    break;

  case 95: /* asm_1: HALF_REG ASSIGN EXPADJ LPAREN HALF_REG COMMA HALF_REG RPAREN  */
#line 2060 "./config/bfin-parse.y"
        {
	  if (IS_DREG_L ((yyvsp[-7].reg)) && IS_DREG_L ((yyvsp[-3].reg)) && IS_DREG_L ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = EXPADJ (dregs_lo, dregs_lo)\n");
	      (yyval.instr) = DSP32SHIFT (7, &(yyvsp[-7].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), 2, 0);
	    }
	  else if (IS_DREG_L ((yyvsp[-7].reg)) && IS_DREG_H ((yyvsp[-3].reg)) && IS_DREG_L ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = EXPADJ (dregs_hi, dregs_lo)\n");
	      (yyval.instr) = DSP32SHIFT (7, &(yyvsp[-7].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), 3, 0);
	    }
	  else
	    return yyerror ("Bad shift value or register");
	}
#line 4514 "config/bfin-parse.c"
    break;

  case 96: /* asm_1: REG ASSIGN DEPOSIT LPAREN REG COMMA REG RPAREN  */
#line 2078 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs = DEPOSIT (dregs , dregs )\n");
	      (yyval.instr) = DSP32SHIFT (10, &(yyvsp[-7].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), 2, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4528 "config/bfin-parse.c"
    break;

  case 97: /* asm_1: REG ASSIGN DEPOSIT LPAREN REG COMMA REG RPAREN LPAREN X RPAREN  */
#line 2089 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-10].reg)) && IS_DREG ((yyvsp[-6].reg)) && IS_DREG ((yyvsp[-4].reg)))
	    {
	      notethat ("dsp32shift: dregs = DEPOSIT (dregs , dregs ) (X)\n");
	      (yyval.instr) = DSP32SHIFT (10, &(yyvsp[-10].reg), &(yyvsp[-4].reg), &(yyvsp[-6].reg), 3, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4542 "config/bfin-parse.c"
    break;

  case 98: /* asm_1: REG ASSIGN EXTRACT LPAREN REG COMMA HALF_REG RPAREN xpmod  */
#line 2100 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-8].reg)) && IS_DREG ((yyvsp[-4].reg)) && IS_DREG_L ((yyvsp[-2].reg)))
	    {
	      notethat ("dsp32shift: dregs = EXTRACT (dregs, dregs_lo ) (.)\n");
	      (yyval.instr) = DSP32SHIFT (10, &(yyvsp[-8].reg), &(yyvsp[-2].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4556 "config/bfin-parse.c"
    break;

  case 99: /* asm_1: a_assign REG_A _GREATER_GREATER_GREATER expr  */
#line 2111 "./config/bfin-parse.y"
        {
	  if (!REG_SAME ((yyvsp[-3].reg), (yyvsp[-2].reg)))
	    return yyerror ("Aregs must be same");

	  if (IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("dsp32shiftimm: Ax = Ax >>> uimm5\n");
	      (yyval.instr) = DSP32SHIFTIMM (3, 0, -imm6 ((yyvsp[0].expr)), 0, 0, IS_A1 ((yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Shift value range error");
	}
#line 4573 "config/bfin-parse.c"
    break;

  case 100: /* asm_1: a_assign LSHIFT REG_A BY HALF_REG  */
#line 2124 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-4].reg), (yyvsp[-2].reg)) && IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: Ax = LSHIFT Ax BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (3, 0, &(yyvsp[0].reg), 0, 1, IS_A1 ((yyvsp[-4].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4587 "config/bfin-parse.c"
    break;

  case 101: /* asm_1: HALF_REG ASSIGN LSHIFT HALF_REG BY HALF_REG  */
#line 2135 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = LSHIFT dregs_hi BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (0, &(yyvsp[-5].reg), &(yyvsp[0].reg), &(yyvsp[-2].reg), 2, HL2 ((yyvsp[-5].reg), (yyvsp[-2].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4601 "config/bfin-parse.c"
    break;

  case 102: /* asm_1: REG ASSIGN LSHIFT REG BY HALF_REG vmod  */
#line 2146 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-6].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG_L ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs = LSHIFT dregs BY dregs_lo (V )\n");
	      (yyval.instr) = DSP32SHIFT ((yyvsp[0].r0).r0 ? 1: 2, &(yyvsp[-6].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), 2, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4615 "config/bfin-parse.c"
    break;

  case 103: /* asm_1: REG ASSIGN SHIFT REG BY HALF_REG  */
#line 2157 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: dregs = SHIFT dregs BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (2, &(yyvsp[-5].reg), &(yyvsp[0].reg), &(yyvsp[-2].reg), 2, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4629 "config/bfin-parse.c"
    break;

  case 104: /* asm_1: a_assign REG_A GREATER_GREATER expr  */
#line 2168 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-3].reg), (yyvsp[-2].reg)) && IS_IMM ((yyvsp[0].expr), 6) >= 0)
	    {
	      notethat ("dsp32shiftimm: Ax = Ax >> imm6\n");
	      (yyval.instr) = DSP32SHIFTIMM (3, 0, -imm6 ((yyvsp[0].expr)), 0, 1, IS_A1 ((yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Accu register expected");
	}
#line 4643 "config/bfin-parse.c"
    break;

  case 105: /* asm_1: REG ASSIGN REG GREATER_GREATER expr vmod  */
#line 2179 "./config/bfin-parse.y"
        {
	  if ((yyvsp[0].r0).r0 == 1)
	    {
	      if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
		{
		  notethat ("dsp32shiftimm: dregs = dregs >> uimm5 (V)\n");
		  (yyval.instr) = DSP32SHIFTIMM (1, &(yyvsp[-5].reg), -uimm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), 2, 0);
		}
	      else
	        return yyerror ("Register mismatch");
	    }
	  else
	    {
	      if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
		{
		  notethat ("dsp32shiftimm: dregs = dregs >> uimm5\n");
		  (yyval.instr) = DSP32SHIFTIMM (2, &(yyvsp[-5].reg), -imm6 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), 2, 0);
		}
	      else if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)) && EXPR_VALUE ((yyvsp[-1].expr)) == 2)
		{
		  notethat ("PTR2op: pregs = pregs >> 2\n");
		  (yyval.instr) = PTR2OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), 3);
		}
	      else if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)) && EXPR_VALUE ((yyvsp[-1].expr)) == 1)
		{
		  notethat ("PTR2op: pregs = pregs >> 1\n");
		  (yyval.instr) = PTR2OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), 4);
		}
	      else
	        return yyerror ("Register mismatch");
	    }
	}
#line 4680 "config/bfin-parse.c"
    break;

  case 106: /* asm_1: HALF_REG ASSIGN HALF_REG GREATER_GREATER expr  */
#line 2212 "./config/bfin-parse.y"
        {
	  if (IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("dsp32shiftimm:  dregs_half =  dregs_half >> uimm5\n");
	      (yyval.instr) = DSP32SHIFTIMM (0, &(yyvsp[-4].reg), -uimm5 ((yyvsp[0].expr)), &(yyvsp[-2].reg), 2, HL2 ((yyvsp[-4].reg), (yyvsp[-2].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4694 "config/bfin-parse.c"
    break;

  case 107: /* asm_1: HALF_REG ASSIGN HALF_REG _GREATER_GREATER_GREATER expr smod  */
#line 2222 "./config/bfin-parse.y"
        {
	  if (IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("dsp32shiftimm: dregs_half = dregs_half >>> uimm5\n");
	      (yyval.instr) = DSP32SHIFTIMM (0, &(yyvsp[-5].reg), -uimm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg),
				  (yyvsp[0].modcodes).s0, HL2 ((yyvsp[-5].reg), (yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Register or modifier mismatch");
	}
#line 4709 "config/bfin-parse.c"
    break;

  case 108: /* asm_1: REG ASSIGN REG _GREATER_GREATER_GREATER expr vsmod  */
#line 2235 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      if ((yyvsp[0].modcodes).r0)
		{
		  /* Vector?  */
		  notethat ("dsp32shiftimm: dregs  =  dregs >>> uimm5 (V, .)\n");
		  (yyval.instr) = DSP32SHIFTIMM (1, &(yyvsp[-5].reg), -uimm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0, 0);
		}
	      else
		{
		  notethat ("dsp32shiftimm: dregs  =  dregs >>> uimm5 (.)\n");
		  (yyval.instr) = DSP32SHIFTIMM (2, &(yyvsp[-5].reg), -uimm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0, 0);
		}
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4732 "config/bfin-parse.c"
    break;

  case 109: /* asm_1: HALF_REG ASSIGN ONES REG  */
#line 2255 "./config/bfin-parse.y"
        {
	  if (IS_DREG_L ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = ONES dregs\n");
	      (yyval.instr) = DSP32SHIFT (6, &(yyvsp[-3].reg), 0, &(yyvsp[0].reg), 3, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4746 "config/bfin-parse.c"
    break;

  case 110: /* asm_1: REG ASSIGN PACK LPAREN HALF_REG COMMA HALF_REG RPAREN  */
#line 2266 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs = PACK (dregs_hi , dregs_hi )\n");
	      (yyval.instr) = DSP32SHIFT (4, &(yyvsp[-7].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), HL2 ((yyvsp[-3].reg), (yyvsp[-1].reg)), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4760 "config/bfin-parse.c"
    break;

  case 111: /* asm_1: HALF_REG ASSIGN CCREG ASSIGN BXORSHIFT LPAREN REG_A COMMA REG RPAREN  */
#line 2277 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-9].reg))
	      && (yyvsp[-3].reg).regno == REG_A0
	      && IS_DREG ((yyvsp[-1].reg)) && !IS_H ((yyvsp[-9].reg)) && !IS_A1 ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = CC = BXORSHIFT (A0 , dregs )\n");
	      (yyval.instr) = DSP32SHIFT (11, &(yyvsp[-9].reg), &(yyvsp[-1].reg), 0, 0, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4776 "config/bfin-parse.c"
    break;

  case 112: /* asm_1: HALF_REG ASSIGN CCREG ASSIGN BXOR LPAREN REG_A COMMA REG RPAREN  */
#line 2290 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-9].reg))
	      && (yyvsp[-3].reg).regno == REG_A0
	      && IS_DREG ((yyvsp[-1].reg)) && !IS_H ((yyvsp[-9].reg)) && !IS_A1 ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = CC = BXOR (A0 , dregs)\n");
	      (yyval.instr) = DSP32SHIFT (11, &(yyvsp[-9].reg), &(yyvsp[-1].reg), 0, 1, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4792 "config/bfin-parse.c"
    break;

  case 113: /* asm_1: HALF_REG ASSIGN CCREG ASSIGN BXOR LPAREN REG_A COMMA REG_A COMMA CCREG RPAREN  */
#line 2303 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-11].reg)) && !IS_H ((yyvsp[-11].reg)) && !REG_SAME ((yyvsp[-5].reg), (yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = CC = BXOR (A0 , A1 , CC)\n");
	      (yyval.instr) = DSP32SHIFT (12, &(yyvsp[-11].reg), 0, 0, 1, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4806 "config/bfin-parse.c"
    break;

  case 114: /* asm_1: a_assign ROT REG_A BY HALF_REG  */
#line 2314 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-4].reg), (yyvsp[-2].reg)) && IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: Ax = ROT Ax BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (3, 0, &(yyvsp[0].reg), 0, 2, IS_A1 ((yyvsp[-4].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4820 "config/bfin-parse.c"
    break;

  case 115: /* asm_1: REG ASSIGN ROT REG BY HALF_REG  */
#line 2325 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: dregs = ROT dregs BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (2, &(yyvsp[-5].reg), &(yyvsp[0].reg), &(yyvsp[-2].reg), 3, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4834 "config/bfin-parse.c"
    break;

  case 116: /* asm_1: a_assign ROT REG_A BY expr  */
#line 2336 "./config/bfin-parse.y"
        {
	  if (IS_IMM ((yyvsp[0].expr), 6))
	    {
	      notethat ("dsp32shiftimm: An = ROT An BY imm6\n");
	      (yyval.instr) = DSP32SHIFTIMM (3, 0, imm6 ((yyvsp[0].expr)), 0, 2, IS_A1 ((yyvsp[-4].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4848 "config/bfin-parse.c"
    break;

  case 117: /* asm_1: REG ASSIGN ROT REG BY expr  */
#line 2347 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_IMM ((yyvsp[0].expr), 6))
	    {
	      (yyval.instr) = DSP32SHIFTIMM (2, &(yyvsp[-5].reg), imm6 ((yyvsp[0].expr)), &(yyvsp[-2].reg), 3, IS_A1 ((yyvsp[-5].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4861 "config/bfin-parse.c"
    break;

  case 118: /* asm_1: HALF_REG ASSIGN SIGNBITS REG_A  */
#line 2357 "./config/bfin-parse.y"
        {
	  if (IS_DREG_L ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = SIGNBITS An\n");
	      (yyval.instr) = DSP32SHIFT (6, &(yyvsp[-3].reg), 0, 0, IS_A1 ((yyvsp[0].reg)), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4875 "config/bfin-parse.c"
    break;

  case 119: /* asm_1: HALF_REG ASSIGN SIGNBITS REG  */
#line 2368 "./config/bfin-parse.y"
        {
	  if (IS_DREG_L ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = SIGNBITS dregs\n");
	      (yyval.instr) = DSP32SHIFT (5, &(yyvsp[-3].reg), 0, &(yyvsp[0].reg), 0, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4889 "config/bfin-parse.c"
    break;

  case 120: /* asm_1: HALF_REG ASSIGN SIGNBITS HALF_REG  */
#line 2379 "./config/bfin-parse.y"
        {
	  if (IS_DREG_L ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = SIGNBITS dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (5, &(yyvsp[-3].reg), 0, &(yyvsp[0].reg), 1 + IS_H ((yyvsp[0].reg)), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4903 "config/bfin-parse.c"
    break;

  case 121: /* asm_1: HALF_REG ASSIGN VIT_MAX LPAREN REG RPAREN asr_asl  */
#line 2391 "./config/bfin-parse.y"
        {
	  if (IS_DREG_L ((yyvsp[-6].reg)) && IS_DREG ((yyvsp[-2].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = VIT_MAX (dregs) (..)\n");
	      (yyval.instr) = DSP32SHIFT (9, &(yyvsp[-6].reg), 0, &(yyvsp[-2].reg), ((yyvsp[0].r0).r0 ? 0 : 1), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4917 "config/bfin-parse.c"
    break;

  case 122: /* asm_1: REG ASSIGN VIT_MAX LPAREN REG COMMA REG RPAREN asr_asl  */
#line 2402 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-8].reg)) && IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-2].reg)))
	    {
	      notethat ("dsp32shift: dregs = VIT_MAX (dregs, dregs) (ASR)\n");
	      (yyval.instr) = DSP32SHIFT (9, &(yyvsp[-8].reg), &(yyvsp[-2].reg), &(yyvsp[-4].reg), 2 | ((yyvsp[0].r0).r0 ? 0 : 1), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4931 "config/bfin-parse.c"
    break;

  case 123: /* asm_1: BITMUX LPAREN REG COMMA REG COMMA REG_A RPAREN asr_asl  */
#line 2413 "./config/bfin-parse.y"
        {
	  if (REG_SAME ((yyvsp[-6].reg), (yyvsp[-4].reg)))
	    return yyerror ("Illegal source register combination");

	  if (IS_DREG ((yyvsp[-6].reg)) && IS_DREG ((yyvsp[-4].reg)) && !IS_A1 ((yyvsp[-2].reg)))
	    {
	      notethat ("dsp32shift: BITMUX (dregs , dregs , A0) (ASR)\n");
	      (yyval.instr) = DSP32SHIFT (8, 0, &(yyvsp[-6].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4948 "config/bfin-parse.c"
    break;

  case 124: /* asm_1: a_assign BXORSHIFT LPAREN REG_A COMMA REG_A COMMA CCREG RPAREN  */
#line 2427 "./config/bfin-parse.y"
        {
	  if (!IS_A1 ((yyvsp[-8].reg)) && !IS_A1 ((yyvsp[-5].reg)) && IS_A1 ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: A0 = BXORSHIFT (A0 , A1 , CC )\n");
	      (yyval.instr) = DSP32SHIFT (12, 0, 0, 0, 0, 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 4962 "config/bfin-parse.c"
    break;

  case 125: /* asm_1: BITCLR LPAREN REG COMMA expr RPAREN  */
#line 2440 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("LOGI2op: BITCLR (dregs , uimm5 )\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-3].reg), uimm5 ((yyvsp[-1].expr)), 4);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4976 "config/bfin-parse.c"
    break;

  case 126: /* asm_1: BITSET LPAREN REG COMMA expr RPAREN  */
#line 2452 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("LOGI2op: BITCLR (dregs , uimm5 )\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-3].reg), uimm5 ((yyvsp[-1].expr)), 2);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 4990 "config/bfin-parse.c"
    break;

  case 127: /* asm_1: BITTGL LPAREN REG COMMA expr RPAREN  */
#line 2464 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("LOGI2op: BITCLR (dregs , uimm5 )\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-3].reg), uimm5 ((yyvsp[-1].expr)), 3);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 5004 "config/bfin-parse.c"
    break;

  case 128: /* asm_1: CCREG ASSIGN BANG BITTST LPAREN REG COMMA expr RPAREN  */
#line 2475 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("LOGI2op: CC =! BITTST (dregs , uimm5 )\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-3].reg), uimm5 ((yyvsp[-1].expr)), 0);
	    }
	  else
	    return yyerror ("Register mismatch or value error");
	}
#line 5018 "config/bfin-parse.c"
    break;

  case 129: /* asm_1: CCREG ASSIGN BITTST LPAREN REG COMMA expr RPAREN  */
#line 2486 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("LOGI2op: CC = BITTST (dregs , uimm5 )\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-3].reg), uimm5 ((yyvsp[-1].expr)), 1);
	    }
	  else
	    return yyerror ("Register mismatch or value error");
	}
#line 5032 "config/bfin-parse.c"
    break;

  case 130: /* asm_1: IF BANG CCREG REG ASSIGN REG  */
#line 2497 "./config/bfin-parse.y"
        {
	  if ((IS_DREG ((yyvsp[-2].reg)) || IS_PREG ((yyvsp[-2].reg)))
	      && (IS_DREG ((yyvsp[0].reg)) || IS_PREG ((yyvsp[0].reg))))
	    {
	      notethat ("ccMV: IF ! CC gregs = gregs\n");
	      (yyval.instr) = CCMV (&(yyvsp[0].reg), &(yyvsp[-2].reg), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 5047 "config/bfin-parse.c"
    break;

  case 131: /* asm_1: IF CCREG REG ASSIGN REG  */
#line 2509 "./config/bfin-parse.y"
        {
	  if ((IS_DREG ((yyvsp[0].reg)) || IS_PREG ((yyvsp[0].reg)))
	      && (IS_DREG ((yyvsp[-2].reg)) || IS_PREG ((yyvsp[-2].reg))))
	    {
	      notethat ("ccMV: IF CC gregs = gregs\n");
	      (yyval.instr) = CCMV (&(yyvsp[0].reg), &(yyvsp[-2].reg), 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
#line 5062 "config/bfin-parse.c"
    break;

  case 132: /* asm_1: IF BANG CCREG JUMP expr  */
#line 2521 "./config/bfin-parse.y"
        {
	  if (IS_PCREL10 ((yyvsp[0].expr)))
	    {
	      notethat ("BRCC: IF !CC JUMP  pcrel11m2\n");
	      (yyval.instr) = BRCC (0, 0, (yyvsp[0].expr));
	    }
	  else
	    return yyerror ("Bad jump offset");
	}
#line 5076 "config/bfin-parse.c"
    break;

  case 133: /* asm_1: IF BANG CCREG JUMP expr LPAREN BP RPAREN  */
#line 2532 "./config/bfin-parse.y"
        {
	  if (IS_PCREL10 ((yyvsp[-3].expr)))
	    {
	      notethat ("BRCC: IF !CC JUMP  pcrel11m2\n");
	      (yyval.instr) = BRCC (0, 1, (yyvsp[-3].expr));
	    }
	  else
	    return yyerror ("Bad jump offset");
	}
#line 5090 "config/bfin-parse.c"
    break;

  case 134: /* asm_1: IF CCREG JUMP expr  */
#line 2543 "./config/bfin-parse.y"
        {
	  if (IS_PCREL10 ((yyvsp[0].expr)))
	    {
	      notethat ("BRCC: IF CC JUMP  pcrel11m2\n");
	      (yyval.instr) = BRCC (1, 0, (yyvsp[0].expr));
	    }
	  else
	    return yyerror ("Bad jump offset");
	}
#line 5104 "config/bfin-parse.c"
    break;

  case 135: /* asm_1: IF CCREG JUMP expr LPAREN BP RPAREN  */
#line 2554 "./config/bfin-parse.y"
        {
	  if (IS_PCREL10 ((yyvsp[-3].expr)))
	    {
	      notethat ("BRCC: IF !CC JUMP  pcrel11m2\n");
	      (yyval.instr) = BRCC (1, 1, (yyvsp[-3].expr));
	    }
	  else
	    return yyerror ("Bad jump offset");
	}
#line 5118 "config/bfin-parse.c"
    break;

  case 136: /* asm_1: NOP  */
#line 2564 "./config/bfin-parse.y"
        {
	  notethat ("ProgCtrl: NOP\n");
	  (yyval.instr) = PROGCTRL (0, 0);
	}
#line 5127 "config/bfin-parse.c"
    break;

  case 137: /* asm_1: RTS  */
#line 2570 "./config/bfin-parse.y"
        {
	  notethat ("ProgCtrl: RTS\n");
	  (yyval.instr) = PROGCTRL (1, 0);
	}
#line 5136 "config/bfin-parse.c"
    break;

  case 138: /* asm_1: RTI  */
#line 2576 "./config/bfin-parse.y"
        {
	  notethat ("ProgCtrl: RTI\n");
	  (yyval.instr) = PROGCTRL (1, 1);
	}
#line 5145 "config/bfin-parse.c"
    break;

  case 139: /* asm_1: RTX  */
#line 2582 "./config/bfin-parse.y"
        {
	  notethat ("ProgCtrl: RTX\n");
	  (yyval.instr) = PROGCTRL (1, 2);
	}
#line 5154 "config/bfin-parse.c"
    break;

  case 140: /* asm_1: RTN  */
#line 2588 "./config/bfin-parse.y"
        {
	  notethat ("ProgCtrl: RTN\n");
	  (yyval.instr) = PROGCTRL (1, 3);
	}
#line 5163 "config/bfin-parse.c"
    break;

  case 141: /* asm_1: RTE  */
#line 2594 "./config/bfin-parse.y"
        {
	  notethat ("ProgCtrl: RTE\n");
	  (yyval.instr) = PROGCTRL (1, 4);
	}
#line 5172 "config/bfin-parse.c"
    break;

  case 142: /* asm_1: IDLE  */
#line 2600 "./config/bfin-parse.y"
        {
	  notethat ("ProgCtrl: IDLE\n");
	  (yyval.instr) = PROGCTRL (2, 0);
	}
#line 5181 "config/bfin-parse.c"
    break;

  case 143: /* asm_1: CSYNC  */
#line 2606 "./config/bfin-parse.y"
        {
	  notethat ("ProgCtrl: CSYNC\n");
	  (yyval.instr) = PROGCTRL (2, 3);
	}
#line 5190 "config/bfin-parse.c"
    break;

  case 144: /* asm_1: SSYNC  */
#line 2612 "./config/bfin-parse.y"
        {
	  notethat ("ProgCtrl: SSYNC\n");
	  (yyval.instr) = PROGCTRL (2, 4);
	}
#line 5199 "config/bfin-parse.c"
    break;

  case 145: /* asm_1: EMUEXCPT  */
#line 2618 "./config/bfin-parse.y"
        {
	  notethat ("ProgCtrl: EMUEXCPT\n");
	  (yyval.instr) = PROGCTRL (2, 5);
	}
#line 5208 "config/bfin-parse.c"
    break;

  case 146: /* asm_1: CLI REG  */
#line 2624 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ProgCtrl: CLI dregs\n");
	      (yyval.instr) = PROGCTRL (3, (yyvsp[0].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Dreg expected for CLI");
	}
#line 5222 "config/bfin-parse.c"
    break;

  case 147: /* asm_1: STI REG  */
#line 2635 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ProgCtrl: STI dregs\n");
	      (yyval.instr) = PROGCTRL (4, (yyvsp[0].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Dreg expected for STI");
	}
#line 5236 "config/bfin-parse.c"
    break;

  case 148: /* asm_1: JUMP LPAREN REG RPAREN  */
#line 2646 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ProgCtrl: JUMP (pregs )\n");
	      (yyval.instr) = PROGCTRL (5, (yyvsp[-1].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Bad register for indirect jump");
	}
#line 5250 "config/bfin-parse.c"
    break;

  case 149: /* asm_1: CALL LPAREN REG RPAREN  */
#line 2657 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ProgCtrl: CALL (pregs )\n");
	      (yyval.instr) = PROGCTRL (6, (yyvsp[-1].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Bad register for indirect call");
	}
#line 5264 "config/bfin-parse.c"
    break;

  case 150: /* asm_1: CALL LPAREN PC PLUS REG RPAREN  */
#line 2668 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ProgCtrl: CALL (PC + pregs )\n");
	      (yyval.instr) = PROGCTRL (7, (yyvsp[-1].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Bad register for indirect call");
	}
#line 5278 "config/bfin-parse.c"
    break;

  case 151: /* asm_1: JUMP LPAREN PC PLUS REG RPAREN  */
#line 2679 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ProgCtrl: JUMP (PC + pregs )\n");
	      (yyval.instr) = PROGCTRL (8, (yyvsp[-1].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Bad register for indirect jump");
	}
#line 5292 "config/bfin-parse.c"
    break;

  case 152: /* asm_1: RAISE expr  */
#line 2690 "./config/bfin-parse.y"
        {
	  if (IS_UIMM ((yyvsp[0].expr), 4))
	    {
	      notethat ("ProgCtrl: RAISE uimm4\n");
	      (yyval.instr) = PROGCTRL (9, uimm4 ((yyvsp[0].expr)));
	    }
	  else
	    return yyerror ("Bad value for RAISE");
	}
#line 5306 "config/bfin-parse.c"
    break;

  case 153: /* asm_1: EXCPT expr  */
#line 2701 "./config/bfin-parse.y"
        {
		notethat ("ProgCtrl: EMUEXCPT\n");
		(yyval.instr) = PROGCTRL (10, uimm4 ((yyvsp[0].expr)));
	}
#line 5315 "config/bfin-parse.c"
    break;

  case 154: /* asm_1: TESTSET LPAREN REG RPAREN  */
#line 2707 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[-1].reg).regno == REG_SP || (yyvsp[-1].reg).regno == REG_FP)
		return yyerror ("Bad register for TESTSET");

	      notethat ("ProgCtrl: TESTSET (pregs )\n");
	      (yyval.instr) = PROGCTRL (11, (yyvsp[-1].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Preg expected");
	}
#line 5332 "config/bfin-parse.c"
    break;

  case 155: /* asm_1: JUMP expr  */
#line 2721 "./config/bfin-parse.y"
        {
	  if (IS_PCREL12 ((yyvsp[0].expr)))
	    {
	      notethat ("UJUMP: JUMP pcrel12\n");
	      (yyval.instr) = UJUMP ((yyvsp[0].expr));
	    }
	  else
	    return yyerror ("Bad value for relative jump");
	}
#line 5346 "config/bfin-parse.c"
    break;

  case 156: /* asm_1: JUMP_DOT_S expr  */
#line 2732 "./config/bfin-parse.y"
        {
	  if (IS_PCREL12 ((yyvsp[0].expr)))
	    {
	      notethat ("UJUMP: JUMP_DOT_S pcrel12\n");
	      (yyval.instr) = UJUMP((yyvsp[0].expr));
	    }
	  else
	    return yyerror ("Bad value for relative jump");
	}
#line 5360 "config/bfin-parse.c"
    break;

  case 157: /* asm_1: JUMP_DOT_L expr  */
#line 2743 "./config/bfin-parse.y"
        {
	  if (IS_PCREL24 ((yyvsp[0].expr)))
	    {
	      notethat ("CALLa: jump.l pcrel24\n");
	      (yyval.instr) = CALLA ((yyvsp[0].expr), 0);
	    }
	  else
	    return yyerror ("Bad value for long jump");
	}
#line 5374 "config/bfin-parse.c"
    break;

  case 158: /* asm_1: JUMP_DOT_L pltpc  */
#line 2754 "./config/bfin-parse.y"
        {
	  if (IS_PCREL24 ((yyvsp[0].expr)))
	    {
	      notethat ("CALLa: jump.l pcrel24\n");
	      (yyval.instr) = CALLA ((yyvsp[0].expr), 2);
	    }
	  else
	    return yyerror ("Bad value for long jump");
	}
#line 5388 "config/bfin-parse.c"
    break;

  case 159: /* asm_1: CALL expr  */
#line 2765 "./config/bfin-parse.y"
        {
	  if (IS_PCREL24 ((yyvsp[0].expr)))
	    {
	      notethat ("CALLa: CALL pcrel25m2\n");
	      (yyval.instr) = CALLA ((yyvsp[0].expr), 1);
	    }
	  else
	    return yyerror ("Bad call address");
	}
#line 5402 "config/bfin-parse.c"
    break;

  case 160: /* asm_1: CALL pltpc  */
#line 2775 "./config/bfin-parse.y"
        {
	  if (IS_PCREL24 ((yyvsp[0].expr)))
	    {
	      notethat ("CALLa: CALL pcrel25m2\n");
	      (yyval.instr) = CALLA ((yyvsp[0].expr), 2);
	    }
	  else
	    return yyerror ("Bad call address");
	}
#line 5416 "config/bfin-parse.c"
    break;

  case 161: /* asm_1: DIVQ LPAREN REG COMMA REG RPAREN  */
#line 2788 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    (yyval.instr) = ALU2OP (&(yyvsp[-3].reg), &(yyvsp[-1].reg), 8);
	  else
	    return yyerror ("Bad registers for DIVQ");
	}
#line 5427 "config/bfin-parse.c"
    break;

  case 162: /* asm_1: DIVS LPAREN REG COMMA REG RPAREN  */
#line 2796 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    (yyval.instr) = ALU2OP (&(yyvsp[-3].reg), &(yyvsp[-1].reg), 9);
	  else
	    return yyerror ("Bad registers for DIVS");
	}
#line 5438 "config/bfin-parse.c"
    break;

  case 163: /* asm_1: REG ASSIGN MINUS REG vsmod  */
#line 2804 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[0].modcodes).r0 == 0 && (yyvsp[0].modcodes).s0 == 0 && (yyvsp[0].modcodes).aop == 0)
		{
		  notethat ("ALU2op: dregs = - dregs\n");
		  (yyval.instr) = ALU2OP (&(yyvsp[-4].reg), &(yyvsp[-1].reg), 14);
		}
	      else if ((yyvsp[0].modcodes).r0 == 1 && (yyvsp[0].modcodes).s0 == 0 && (yyvsp[0].modcodes).aop == 3)
		{
		  notethat ("dsp32alu: dregs = - dregs (.)\n");
		  (yyval.instr) = DSP32ALU (15, 0, 0, &(yyvsp[-4].reg), &(yyvsp[-1].reg), 0, (yyvsp[0].modcodes).s0, 0, 3);
		}
	      else
		{
		  notethat ("dsp32alu: dregs = - dregs (.)\n");
		  (yyval.instr) = DSP32ALU (7, 0, 0, &(yyvsp[-4].reg), &(yyvsp[-1].reg), 0, (yyvsp[0].modcodes).s0, 0, 3);
		}
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 5465 "config/bfin-parse.c"
    break;

  case 164: /* asm_1: REG ASSIGN TILDA REG  */
#line 2828 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ALU2op: dregs = ~dregs\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-3].reg), &(yyvsp[0].reg), 15);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 5479 "config/bfin-parse.c"
    break;

  case 165: /* asm_1: REG _GREATER_GREATER_ASSIGN REG  */
#line 2839 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ALU2op: dregs >>= dregs\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-2].reg), &(yyvsp[0].reg), 1);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 5493 "config/bfin-parse.c"
    break;

  case 166: /* asm_1: REG _GREATER_GREATER_ASSIGN expr  */
#line 2850 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("LOGI2op: dregs >>= uimm5\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-2].reg), uimm5 ((yyvsp[0].expr)), 6);
	    }
	  else
	    return yyerror ("Dregs expected or value error");
	}
#line 5507 "config/bfin-parse.c"
    break;

  case 167: /* asm_1: REG _GREATER_GREATER_GREATER_THAN_ASSIGN REG  */
#line 2861 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ALU2op: dregs >>>= dregs\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-2].reg), &(yyvsp[0].reg), 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 5521 "config/bfin-parse.c"
    break;

  case 168: /* asm_1: REG _LESS_LESS_ASSIGN REG  */
#line 2872 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ALU2op: dregs <<= dregs\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-2].reg), &(yyvsp[0].reg), 2);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 5535 "config/bfin-parse.c"
    break;

  case 169: /* asm_1: REG _LESS_LESS_ASSIGN expr  */
#line 2883 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("LOGI2op: dregs <<= uimm5\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-2].reg), uimm5 ((yyvsp[0].expr)), 7);
	    }
	  else
	    return yyerror ("Dregs expected or const value error");
	}
#line 5549 "config/bfin-parse.c"
    break;

  case 170: /* asm_1: REG _GREATER_GREATER_GREATER_THAN_ASSIGN expr  */
#line 2895 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("LOGI2op: dregs >>>= uimm5\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-2].reg), uimm5 ((yyvsp[0].expr)), 5);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 5563 "config/bfin-parse.c"
    break;

  case 171: /* asm_1: FLUSH LBRACK REG RBRACK  */
#line 2908 "./config/bfin-parse.y"
        {
	  notethat ("CaCTRL: FLUSH [ pregs ]\n");
	  if (IS_PREG ((yyvsp[-1].reg)))
	    (yyval.instr) = CACTRL (&(yyvsp[-1].reg), 0, 2);
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
#line 5575 "config/bfin-parse.c"
    break;

  case 172: /* asm_1: FLUSH reg_with_postinc  */
#line 2917 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("CaCTRL: FLUSH [ pregs ++ ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[0].reg), 1, 2);
	    }
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
#line 5589 "config/bfin-parse.c"
    break;

  case 173: /* asm_1: FLUSHINV LBRACK REG RBRACK  */
#line 2928 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("CaCTRL: FLUSHINV [ pregs ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[-1].reg), 0, 1);
	    }
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
#line 5603 "config/bfin-parse.c"
    break;

  case 174: /* asm_1: FLUSHINV reg_with_postinc  */
#line 2939 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("CaCTRL: FLUSHINV [ pregs ++ ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[0].reg), 1, 1);
	    }
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
#line 5617 "config/bfin-parse.c"
    break;

  case 175: /* asm_1: IFLUSH LBRACK REG RBRACK  */
#line 2951 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("CaCTRL: IFLUSH [ pregs ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[-1].reg), 0, 3);
	    }
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
#line 5631 "config/bfin-parse.c"
    break;

  case 176: /* asm_1: IFLUSH reg_with_postinc  */
#line 2962 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("CaCTRL: IFLUSH [ pregs ++ ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[0].reg), 1, 3);
	    }
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
#line 5645 "config/bfin-parse.c"
    break;

  case 177: /* asm_1: PREFETCH LBRACK REG RBRACK  */
#line 2973 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("CaCTRL: PREFETCH [ pregs ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[-1].reg), 0, 0);
	    }
	  else
	    return yyerror ("Bad register(s) for PREFETCH");
	}
#line 5659 "config/bfin-parse.c"
    break;

  case 178: /* asm_1: PREFETCH reg_with_postinc  */
#line 2984 "./config/bfin-parse.y"
        {
	  if (IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("CaCTRL: PREFETCH [ pregs ++ ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[0].reg), 1, 0);
	    }
	  else
	    return yyerror ("Bad register(s) for PREFETCH");
	}
#line 5673 "config/bfin-parse.c"
    break;

  case 179: /* asm_1: B LBRACK REG post_op RBRACK ASSIGN REG  */
#line 2998 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Dreg expected for source operand");
	  if (!IS_PREG ((yyvsp[-4].reg)))
	    return yyerror ("Preg expected in address");

	  notethat ("LDST: B [ pregs <post_op> ] = dregs\n");
	  (yyval.instr) = LDST (&(yyvsp[-4].reg), &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 2, 0, 1);
	}
#line 5687 "config/bfin-parse.c"
    break;

  case 180: /* asm_1: B LBRACK REG plus_minus expr RBRACK ASSIGN REG  */
#line 3010 "./config/bfin-parse.y"
        {
	  Expr_Node *tmp = (yyvsp[-3].expr);

	  if (!IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Dreg expected for source operand");
	  if (!IS_PREG ((yyvsp[-5].reg)))
	    return yyerror ("Preg expected in address");

	  if (IS_RELOC ((yyvsp[-3].expr)))
	    return yyerror ("Plain symbol used as offset");

	  if ((yyvsp[-4].r0).r0)
	    tmp = unary (Expr_Op_Type_NEG, tmp);

	  if (in_range_p (tmp, -32768, 32767, 0))
	    {
	      notethat ("LDST: B [ pregs + imm16 ] = dregs\n");
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-5].reg), &(yyvsp[0].reg), 1, 2, 0, (yyvsp[-3].expr));
	    }
	  else
	    return yyerror ("Displacement out of range");
	}
#line 5714 "config/bfin-parse.c"
    break;

  case 181: /* asm_1: W LBRACK REG plus_minus expr RBRACK ASSIGN REG  */
#line 3036 "./config/bfin-parse.y"
        {
	  Expr_Node *tmp = (yyvsp[-3].expr);

	  if (!IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Dreg expected for source operand");
	  if (!IS_PREG ((yyvsp[-5].reg)))
	    return yyerror ("Preg expected in address");

	  if ((yyvsp[-4].r0).r0)
	    tmp = unary (Expr_Op_Type_NEG, tmp);

	  if (IS_RELOC ((yyvsp[-3].expr)))
	    return yyerror ("Plain symbol used as offset");

	  if (in_range_p (tmp, 0, 30, 1))
	    {
	      notethat ("LDSTii: W [ pregs +- uimm5m2 ] = dregs\n");
	      (yyval.instr) = LDSTII (&(yyvsp[-5].reg), &(yyvsp[0].reg), tmp, 1, 1);
	    }
	  else if (in_range_p (tmp, -65536, 65535, 1))
	    {
	      notethat ("LDSTidxI: W [ pregs + imm17m2 ] = dregs\n");
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-5].reg), &(yyvsp[0].reg), 1, 1, 0, tmp);
	    }
	  else
	    return yyerror ("Displacement out of range");
	}
#line 5746 "config/bfin-parse.c"
    break;

  case 182: /* asm_1: W LBRACK REG post_op RBRACK ASSIGN REG  */
#line 3066 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Dreg expected for source operand");
	  if (!IS_PREG ((yyvsp[-4].reg)))
	    return yyerror ("Preg expected in address");

	  notethat ("LDST: W [ pregs <post_op> ] = dregs\n");
	  (yyval.instr) = LDST (&(yyvsp[-4].reg), &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 1, 0, 1);
	}
#line 5760 "config/bfin-parse.c"
    break;

  case 183: /* asm_1: W LBRACK REG post_op RBRACK ASSIGN HALF_REG  */
#line 3077 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Dreg expected for source operand");
	  if ((yyvsp[-3].modcodes).x0 == 2)
	    {
	      if (!IS_IREG ((yyvsp[-4].reg)) && !IS_PREG ((yyvsp[-4].reg)))
		return yyerror ("Ireg or Preg expected in address");
	    }
	  else if (!IS_IREG ((yyvsp[-4].reg)))
	    return yyerror ("Ireg expected in address");

	  if (IS_IREG ((yyvsp[-4].reg)))
	    {
	      notethat ("dspLDST: W [ iregs <post_op> ] = dregs_half\n");
	      (yyval.instr) = DSPLDST (&(yyvsp[-4].reg), 1 + IS_H ((yyvsp[0].reg)), &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 1);
	    }
	  else
	    {
	      notethat ("LDSTpmod: W [ pregs ] = dregs_half\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-4].reg), &(yyvsp[0].reg), &(yyvsp[-4].reg), 1 + IS_H ((yyvsp[0].reg)), 1);
	    }
	}
#line 5787 "config/bfin-parse.c"
    break;

  case 184: /* asm_1: LBRACK REG plus_minus expr RBRACK ASSIGN REG  */
#line 3102 "./config/bfin-parse.y"
        {
	  Expr_Node *tmp = (yyvsp[-3].expr);
	  int ispreg = IS_PREG ((yyvsp[0].reg));

	  if (!IS_PREG ((yyvsp[-5].reg)))
	    return yyerror ("Preg expected in address");

	  if (!IS_DREG ((yyvsp[0].reg)) && !ispreg)
	    return yyerror ("Preg expected for source operand");

	  if ((yyvsp[-4].r0).r0)
	    tmp = unary (Expr_Op_Type_NEG, tmp);

	  if (IS_RELOC ((yyvsp[-3].expr)))
	    return yyerror ("Plain symbol used as offset");

	  if (in_range_p (tmp, 0, 63, 3))
	    {
	      notethat ("LDSTii: dpregs = [ pregs + uimm6m4 ]\n");
	      (yyval.instr) = LDSTII (&(yyvsp[-5].reg), &(yyvsp[0].reg), tmp, 1, ispreg ? 3 : 0);
	    }
	  else if ((yyvsp[-5].reg).regno == REG_FP && in_range_p (tmp, -128, 0, 3))
	    {
	      notethat ("LDSTiiFP: dpregs = [ FP - uimm7m4 ]\n");
	      tmp = unary (Expr_Op_Type_NEG, tmp);
	      (yyval.instr) = LDSTIIFP (tmp, &(yyvsp[0].reg), 1);
	    }
	  else if (in_range_p (tmp, -131072, 131071, 3))
	    {
	      notethat ("LDSTidxI: [ pregs + imm18m4 ] = dpregs\n");
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-5].reg), &(yyvsp[0].reg), 1, 0, ispreg ? 1 : 0, tmp);
	    }
	  else
	    return yyerror ("Displacement out of range");
	}
#line 5827 "config/bfin-parse.c"
    break;

  case 185: /* asm_1: REG ASSIGN W LBRACK REG plus_minus expr RBRACK xpmod  */
#line 3139 "./config/bfin-parse.y"
        {
	  Expr_Node *tmp = (yyvsp[-2].expr);
	  if (!IS_DREG ((yyvsp[-8].reg)))
	    return yyerror ("Dreg expected for destination operand");
	  if (!IS_PREG ((yyvsp[-4].reg)))
	    return yyerror ("Preg expected in address");

	  if ((yyvsp[-3].r0).r0)
	    tmp = unary (Expr_Op_Type_NEG, tmp);

	  if (IS_RELOC ((yyvsp[-2].expr)))
	    return yyerror ("Plain symbol used as offset");

	  if (in_range_p (tmp, 0, 30, 1))
	    {
	      notethat ("LDSTii: dregs = W [ pregs + uimm5m2 ] (.)\n");
	      (yyval.instr) = LDSTII (&(yyvsp[-4].reg), &(yyvsp[-8].reg), tmp, 0, 1 << (yyvsp[0].r0).r0);
	    }
	  else if (in_range_p (tmp, -65536, 65535, 1))
	    {
	      notethat ("LDSTidxI: dregs = W [ pregs + imm17m2 ] (.)\n");
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-4].reg), &(yyvsp[-8].reg), 0, 1, (yyvsp[0].r0).r0, tmp);
	    }
	  else
	    return yyerror ("Displacement out of range");
	}
#line 5858 "config/bfin-parse.c"
    break;

  case 186: /* asm_1: HALF_REG ASSIGN W LBRACK REG post_op RBRACK  */
#line 3167 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-6].reg)))
	    return yyerror ("Dreg expected for source operand");
	  if ((yyvsp[-1].modcodes).x0 == 2)
	    {
	      if (!IS_IREG ((yyvsp[-2].reg)) && !IS_PREG ((yyvsp[-2].reg)))
		return yyerror ("Ireg or Preg expected in address");
	    }
	  else if (!IS_IREG ((yyvsp[-2].reg)))
	    return yyerror ("Ireg expected in address");

	  if (IS_IREG ((yyvsp[-2].reg)))
	    {
	      notethat ("dspLDST: dregs_half = W [ iregs <post_op> ]\n");
	      (yyval.instr) = DSPLDST(&(yyvsp[-2].reg), 1 + IS_H ((yyvsp[-6].reg)), &(yyvsp[-6].reg), (yyvsp[-1].modcodes).x0, 0);
	    }
	  else
	    {
	      notethat ("LDSTpmod: dregs_half = W [ pregs <post_op> ]\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-2].reg), &(yyvsp[-6].reg), &(yyvsp[-2].reg), 1 + IS_H ((yyvsp[-6].reg)), 0);
	    }
	}
#line 5885 "config/bfin-parse.c"
    break;

  case 187: /* asm_1: REG ASSIGN W LBRACK REG post_op RBRACK xpmod  */
#line 3192 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-7].reg)))
	    return yyerror ("Dreg expected for destination operand");
	  if (!IS_PREG ((yyvsp[-3].reg)))
	    return yyerror ("Preg expected in address");

	  notethat ("LDST: dregs = W [ pregs <post_op> ] (.)\n");
	  (yyval.instr) = LDST (&(yyvsp[-3].reg), &(yyvsp[-7].reg), (yyvsp[-2].modcodes).x0, 1, (yyvsp[0].r0).r0, 0);
	}
#line 5899 "config/bfin-parse.c"
    break;

  case 188: /* asm_1: REG ASSIGN W LBRACK REG _PLUS_PLUS REG RBRACK xpmod  */
#line 3203 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-8].reg)))
	    return yyerror ("Dreg expected for destination operand");
	  if (!IS_PREG ((yyvsp[-4].reg)) || !IS_PREG ((yyvsp[-2].reg)))
	    return yyerror ("Preg expected in address");

	  notethat ("LDSTpmod: dregs = W [ pregs ++ pregs ] (.)\n");
	  (yyval.instr) = LDSTPMOD (&(yyvsp[-4].reg), &(yyvsp[-8].reg), &(yyvsp[-2].reg), 3, (yyvsp[0].r0).r0);
	}
#line 5913 "config/bfin-parse.c"
    break;

  case 189: /* asm_1: HALF_REG ASSIGN W LBRACK REG _PLUS_PLUS REG RBRACK  */
#line 3214 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-7].reg)))
	    return yyerror ("Dreg expected for destination operand");
	  if (!IS_PREG ((yyvsp[-3].reg)) || !IS_PREG ((yyvsp[-1].reg)))
	    return yyerror ("Preg expected in address");

	  notethat ("LDSTpmod: dregs_half = W [ pregs ++ pregs ]\n");
	  (yyval.instr) = LDSTPMOD (&(yyvsp[-3].reg), &(yyvsp[-7].reg), &(yyvsp[-1].reg), 1 + IS_H ((yyvsp[-7].reg)), 0);
	}
#line 5927 "config/bfin-parse.c"
    break;

  case 190: /* asm_1: LBRACK REG post_op RBRACK ASSIGN REG  */
#line 3225 "./config/bfin-parse.y"
        {
	  if (!IS_IREG ((yyvsp[-4].reg)) && !IS_PREG ((yyvsp[-4].reg)))
	    return yyerror ("Ireg or Preg expected in address");
	  else if (IS_IREG ((yyvsp[-4].reg)) && !IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Dreg expected for source operand");
	  else if (IS_PREG ((yyvsp[-4].reg)) && !IS_DREG ((yyvsp[0].reg)) && !IS_PREG ((yyvsp[0].reg)))
	    return yyerror ("Dreg or Preg expected for source operand");

	  if (IS_IREG ((yyvsp[-4].reg)))
	    {
	      notethat ("dspLDST: [ iregs <post_op> ] = dregs\n");
	      (yyval.instr) = DSPLDST(&(yyvsp[-4].reg), 0, &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 1);
	    }
	  else if (IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("LDST: [ pregs <post_op> ] = dregs\n");
	      (yyval.instr) = LDST (&(yyvsp[-4].reg), &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 0, 0, 1);
	    }
	  else
	    {
	      notethat ("LDST: [ pregs <post_op> ] = pregs\n");
	      (yyval.instr) = LDST (&(yyvsp[-4].reg), &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 0, 1, 1);
	    }
	}
#line 5956 "config/bfin-parse.c"
    break;

  case 191: /* asm_1: LBRACK REG _PLUS_PLUS REG RBRACK ASSIGN REG  */
#line 3251 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Dreg expected for source operand");

	  if (IS_IREG ((yyvsp[-5].reg)) && IS_MREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dspLDST: [ iregs ++ mregs ] = dregs\n");
	      (yyval.instr) = DSPLDST(&(yyvsp[-5].reg), (yyvsp[-3].reg).regno & CODE_MASK, &(yyvsp[0].reg), 3, 1);
	    }
	  else if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      notethat ("LDSTpmod: [ pregs ++ pregs ] = dregs\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-5].reg), &(yyvsp[0].reg), &(yyvsp[-3].reg), 0, 1);
	    }
	  else
	    return yyerror ("Preg ++ Preg or Ireg ++ Mreg expected in address");
	}
#line 5978 "config/bfin-parse.c"
    break;

  case 192: /* asm_1: W LBRACK REG _PLUS_PLUS REG RBRACK ASSIGN HALF_REG  */
#line 3270 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Dreg expected for source operand");

	  if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      notethat ("LDSTpmod: W [ pregs ++ pregs ] = dregs_half\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-5].reg), &(yyvsp[0].reg), &(yyvsp[-3].reg), 1 + IS_H ((yyvsp[0].reg)), 1);
	    }
	  else
	    return yyerror ("Preg ++ Preg expected in address");
	}
#line 5995 "config/bfin-parse.c"
    break;

  case 193: /* asm_1: REG ASSIGN B LBRACK REG plus_minus expr RBRACK xpmod  */
#line 3284 "./config/bfin-parse.y"
        {
	  Expr_Node *tmp = (yyvsp[-2].expr);
	  if (!IS_DREG ((yyvsp[-8].reg)))
	    return yyerror ("Dreg expected for destination operand");
	  if (!IS_PREG ((yyvsp[-4].reg)))
	    return yyerror ("Preg expected in address");

	  if ((yyvsp[-3].r0).r0)
	    tmp = unary (Expr_Op_Type_NEG, tmp);

	  if (IS_RELOC ((yyvsp[-2].expr)))
	    return yyerror ("Plain symbol used as offset");

	  if (in_range_p (tmp, -32768, 32767, 0))
	    {
	      notethat ("LDSTidxI: dregs = B [ pregs + imm16 ] (%c)\n",
		       (yyvsp[0].r0).r0 ? 'X' : 'Z');
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-4].reg), &(yyvsp[-8].reg), 0, 2, (yyvsp[0].r0).r0, tmp);
	    }
	  else
	    return yyerror ("Displacement out of range");
	}
#line 6022 "config/bfin-parse.c"
    break;

  case 194: /* asm_1: REG ASSIGN B LBRACK REG post_op RBRACK xpmod  */
#line 3308 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-7].reg)))
	    return yyerror ("Dreg expected for destination operand");
	  if (!IS_PREG ((yyvsp[-3].reg)))
	    return yyerror ("Preg expected in address");

	  notethat ("LDST: dregs = B [ pregs <post_op> ] (%c)\n",
		    (yyvsp[0].r0).r0 ? 'X' : 'Z');
	  (yyval.instr) = LDST (&(yyvsp[-3].reg), &(yyvsp[-7].reg), (yyvsp[-2].modcodes).x0, 2, (yyvsp[0].r0).r0, 0);
	}
#line 6037 "config/bfin-parse.c"
    break;

  case 195: /* asm_1: REG ASSIGN LBRACK REG _PLUS_PLUS REG RBRACK  */
#line 3320 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-6].reg)))
	    return yyerror ("Dreg expected for destination operand");

	  if (IS_IREG ((yyvsp[-3].reg)) && IS_MREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dspLDST: dregs = [ iregs ++ mregs ]\n");
	      (yyval.instr) = DSPLDST(&(yyvsp[-3].reg), (yyvsp[-1].reg).regno & CODE_MASK, &(yyvsp[-6].reg), 3, 0);
	    }
	  else if (IS_PREG ((yyvsp[-3].reg)) && IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("LDSTpmod: dregs = [ pregs ++ pregs ]\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-3].reg), &(yyvsp[-6].reg), &(yyvsp[-1].reg), 0, 0);
	    }
	  else
	    return yyerror ("Preg ++ Preg or Ireg ++ Mreg expected in address");
	}
#line 6059 "config/bfin-parse.c"
    break;

  case 196: /* asm_1: REG ASSIGN LBRACK REG plus_minus got_or_expr RBRACK  */
#line 3339 "./config/bfin-parse.y"
        {
	  Expr_Node *tmp = (yyvsp[-1].expr);
	  int ispreg = IS_PREG ((yyvsp[-6].reg));
	  int isgot = IS_RELOC((yyvsp[-1].expr));

	  if (!IS_PREG ((yyvsp[-3].reg)))
	    return yyerror ("Preg expected in address");

	  if (!IS_DREG ((yyvsp[-6].reg)) && !ispreg)
	    return yyerror ("Dreg or Preg expected for destination operand");

	  if (tmp->type == Expr_Node_Reloc
	      && strcmp (tmp->value.s_value,
			 "_current_shared_library_p5_offset_") != 0)
	    return yyerror ("Plain symbol used as offset");

	  if ((yyvsp[-2].r0).r0)
	    tmp = unary (Expr_Op_Type_NEG, tmp);

	  if (isgot)
	    {
	      notethat ("LDSTidxI: dpregs = [ pregs + sym@got ]\n");
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-3].reg), &(yyvsp[-6].reg), 0, 0, ispreg ? 1 : 0, tmp);
	    }
	  else if (in_range_p (tmp, 0, 63, 3))
	    {
	      notethat ("LDSTii: dpregs = [ pregs + uimm7m4 ]\n");
	      (yyval.instr) = LDSTII (&(yyvsp[-3].reg), &(yyvsp[-6].reg), tmp, 0, ispreg ? 3 : 0);
	    }
	  else if ((yyvsp[-3].reg).regno == REG_FP && in_range_p (tmp, -128, 0, 3))
	    {
	      notethat ("LDSTiiFP: dpregs = [ FP - uimm7m4 ]\n");
	      tmp = unary (Expr_Op_Type_NEG, tmp);
	      (yyval.instr) = LDSTIIFP (tmp, &(yyvsp[-6].reg), 0);
	    }
	  else if (in_range_p (tmp, -131072, 131071, 3))
	    {
	      notethat ("LDSTidxI: dpregs = [ pregs + imm18m4 ]\n");
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-3].reg), &(yyvsp[-6].reg), 0, 0, ispreg ? 1 : 0, tmp);

	    }
	  else
	    return yyerror ("Displacement out of range");
	}
#line 6108 "config/bfin-parse.c"
    break;

  case 197: /* asm_1: REG ASSIGN LBRACK REG post_op RBRACK  */
#line 3385 "./config/bfin-parse.y"
        {
	  if (!IS_IREG ((yyvsp[-2].reg)) && !IS_PREG ((yyvsp[-2].reg)))
	    return yyerror ("Ireg or Preg expected in address");
	  else if (IS_IREG ((yyvsp[-2].reg)) && !IS_DREG ((yyvsp[-5].reg)))
	    return yyerror ("Dreg expected in destination operand");
	  else if (IS_PREG ((yyvsp[-2].reg)) && !IS_DREG ((yyvsp[-5].reg)) && !IS_PREG ((yyvsp[-5].reg))
		   && ((yyvsp[-2].reg).regno != REG_SP || !IS_ALLREG ((yyvsp[-5].reg)) || (yyvsp[-1].modcodes).x0 != 0))
	    return yyerror ("Dreg or Preg expected in destination operand");

	  if (IS_IREG ((yyvsp[-2].reg)))
	    {
	      notethat ("dspLDST: dregs = [ iregs <post_op> ]\n");
	      (yyval.instr) = DSPLDST (&(yyvsp[-2].reg), 0, &(yyvsp[-5].reg), (yyvsp[-1].modcodes).x0, 0);
	    }
	  else if (IS_DREG ((yyvsp[-5].reg)))
	    {
	      notethat ("LDST: dregs = [ pregs <post_op> ]\n");
	      (yyval.instr) = LDST (&(yyvsp[-2].reg), &(yyvsp[-5].reg), (yyvsp[-1].modcodes).x0, 0, 0, 0);
	    }
	  else if (IS_PREG ((yyvsp[-5].reg)))
	    {
	      if (REG_SAME ((yyvsp[-5].reg), (yyvsp[-2].reg)) && (yyvsp[-1].modcodes).x0 != 2)
		return yyerror ("Pregs can't be same");

	      notethat ("LDST: pregs = [ pregs <post_op> ]\n");
	      (yyval.instr) = LDST (&(yyvsp[-2].reg), &(yyvsp[-5].reg), (yyvsp[-1].modcodes).x0, 0, 1, 0);
	    }
	  else
	    {
	      notethat ("PushPopReg: allregs = [ SP ++ ]\n");
	      (yyval.instr) = PUSHPOPREG (&(yyvsp[-5].reg), 0);
	    }
	}
#line 6146 "config/bfin-parse.c"
    break;

  case 198: /* asm_1: reg_with_predec ASSIGN LPAREN REG COLON expr COMMA REG COLON expr RPAREN  */
#line 3422 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-10].reg).regno != REG_SP)
	    yyerror ("Stack Pointer expected");
	  if ((yyvsp[-7].reg).regno == REG_R7
	      && IN_RANGE ((yyvsp[-5].expr), 0, 7)
	      && (yyvsp[-3].reg).regno == REG_P5
	      && IN_RANGE ((yyvsp[-1].expr), 0, 5))
	    {
	      notethat ("PushPopMultiple: [ -- SP ] = (R7 : reglim , P5 : reglim )\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (imm5 ((yyvsp[-5].expr)), imm5 ((yyvsp[-1].expr)), 1, 1, 1);
	    }
	  else
	    return yyerror ("Bad register for PushPopMultiple");
	}
#line 6165 "config/bfin-parse.c"
    break;

  case 199: /* asm_1: reg_with_predec ASSIGN LPAREN REG COLON expr RPAREN  */
#line 3438 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-6].reg).regno != REG_SP)
	    yyerror ("Stack Pointer expected");

	  if ((yyvsp[-3].reg).regno == REG_R7 && IN_RANGE ((yyvsp[-1].expr), 0, 7))
	    {
	      notethat ("PushPopMultiple: [ -- SP ] = (R7 : reglim )\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (imm5 ((yyvsp[-1].expr)), 0, 1, 0, 1);
	    }
	  else if ((yyvsp[-3].reg).regno == REG_P5 && IN_RANGE ((yyvsp[-1].expr), 0, 6))
	    {
	      notethat ("PushPopMultiple: [ -- SP ] = (P5 : reglim )\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (0, imm5 ((yyvsp[-1].expr)), 0, 1, 1);
	    }
	  else
	    return yyerror ("Bad register for PushPopMultiple");
	}
#line 6187 "config/bfin-parse.c"
    break;

  case 200: /* asm_1: LPAREN REG COLON expr COMMA REG COLON expr RPAREN ASSIGN reg_with_postinc  */
#line 3457 "./config/bfin-parse.y"
        {
	  if ((yyvsp[0].reg).regno != REG_SP)
	    yyerror ("Stack Pointer expected");
	  if ((yyvsp[-9].reg).regno == REG_R7 && (IN_RANGE ((yyvsp[-7].expr), 0, 7))
	      && (yyvsp[-5].reg).regno == REG_P5 && (IN_RANGE ((yyvsp[-3].expr), 0, 6)))
	    {
	      notethat ("PushPopMultiple: (R7 : reglim , P5 : reglim ) = [ SP ++ ]\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (imm5 ((yyvsp[-7].expr)), imm5 ((yyvsp[-3].expr)), 1, 1, 0);
	    }
	  else
	    return yyerror ("Bad register range for PushPopMultiple");
	}
#line 6204 "config/bfin-parse.c"
    break;

  case 201: /* asm_1: LPAREN REG COLON expr RPAREN ASSIGN reg_with_postinc  */
#line 3471 "./config/bfin-parse.y"
        {
	  if ((yyvsp[0].reg).regno != REG_SP)
	    yyerror ("Stack Pointer expected");

	  if ((yyvsp[-5].reg).regno == REG_R7 && IN_RANGE ((yyvsp[-3].expr), 0, 7))
	    {
	      notethat ("PushPopMultiple: (R7 : reglim ) = [ SP ++ ]\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (imm5 ((yyvsp[-3].expr)), 0, 1, 0, 0);
	    }
	  else if ((yyvsp[-5].reg).regno == REG_P5 && IN_RANGE ((yyvsp[-3].expr), 0, 6))
	    {
	      notethat ("PushPopMultiple: (P5 : reglim ) = [ SP ++ ]\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (0, imm5 ((yyvsp[-3].expr)), 0, 1, 0);
	    }
	  else
	    return yyerror ("Bad register range for PushPopMultiple");
	}
#line 6226 "config/bfin-parse.c"
    break;

  case 202: /* asm_1: reg_with_predec ASSIGN REG  */
#line 3490 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-2].reg).regno != REG_SP)
	    yyerror ("Stack Pointer expected");

	  if (IS_ALLREG ((yyvsp[0].reg)))
	    {
	      notethat ("PushPopReg: [ -- SP ] = allregs\n");
	      (yyval.instr) = PUSHPOPREG (&(yyvsp[0].reg), 1);
	    }
	  else
	    return yyerror ("Bad register for PushPopReg");
	}
#line 6243 "config/bfin-parse.c"
    break;

  case 203: /* asm_1: LINK expr  */
#line 3506 "./config/bfin-parse.y"
        {
	  if (IS_URANGE (16, (yyvsp[0].expr), 0, 4))
	    (yyval.instr) = LINKAGE (0, uimm16s4 ((yyvsp[0].expr)));
	  else
	    return yyerror ("Bad constant for LINK");
	}
#line 6254 "config/bfin-parse.c"
    break;

  case 204: /* asm_1: UNLINK  */
#line 3514 "./config/bfin-parse.y"
        {
		notethat ("linkage: UNLINK\n");
		(yyval.instr) = LINKAGE (1, 0);
	}
#line 6263 "config/bfin-parse.c"
    break;

  case 205: /* asm_1: LSETUP LPAREN expr COMMA expr RPAREN REG  */
#line 3523 "./config/bfin-parse.y"
        {
	  if (IS_PCREL4 ((yyvsp[-4].expr)) && IS_LPPCREL10 ((yyvsp[-2].expr)) && IS_CREG ((yyvsp[0].reg)))
	    {
	      notethat ("LoopSetup: LSETUP (pcrel4 , lppcrel10 ) counters\n");
	      (yyval.instr) = LOOPSETUP ((yyvsp[-4].expr), &(yyvsp[0].reg), 0, (yyvsp[-2].expr), 0);
	    }
	  else
	    return yyerror ("Bad register or values for LSETUP");

	}
#line 6278 "config/bfin-parse.c"
    break;

  case 206: /* asm_1: LSETUP LPAREN expr COMMA expr RPAREN REG ASSIGN REG  */
#line 3534 "./config/bfin-parse.y"
        {
	  if (IS_PCREL4 ((yyvsp[-6].expr)) && IS_LPPCREL10 ((yyvsp[-4].expr))
	      && IS_PREG ((yyvsp[0].reg)) && IS_CREG ((yyvsp[-2].reg)))
	    {
	      notethat ("LoopSetup: LSETUP (pcrel4 , lppcrel10 ) counters = pregs\n");
	      (yyval.instr) = LOOPSETUP ((yyvsp[-6].expr), &(yyvsp[-2].reg), 1, (yyvsp[-4].expr), &(yyvsp[0].reg));
	    }
	  else
	    return yyerror ("Bad register or values for LSETUP");
	}
#line 6293 "config/bfin-parse.c"
    break;

  case 207: /* asm_1: LSETUP LPAREN expr COMMA expr RPAREN REG ASSIGN REG GREATER_GREATER expr  */
#line 3546 "./config/bfin-parse.y"
        {
	  if (IS_PCREL4 ((yyvsp[-8].expr)) && IS_LPPCREL10 ((yyvsp[-6].expr))
	      && IS_PREG ((yyvsp[-2].reg)) && IS_CREG ((yyvsp[-4].reg))
	      && EXPR_VALUE ((yyvsp[0].expr)) == 1)
	    {
	      notethat ("LoopSetup: LSETUP (pcrel4 , lppcrel10 ) counters = pregs >> 1\n");
	      (yyval.instr) = LOOPSETUP ((yyvsp[-8].expr), &(yyvsp[-4].reg), 3, (yyvsp[-6].expr), &(yyvsp[-2].reg));
	    }
	  else
	    return yyerror ("Bad register or values for LSETUP");
	}
#line 6309 "config/bfin-parse.c"
    break;

  case 208: /* asm_1: LOOP expr REG  */
#line 3560 "./config/bfin-parse.y"
        {
	  if (!IS_RELOC ((yyvsp[-1].expr)))
	    return yyerror ("Invalid expression in loop statement");
	  if (!IS_CREG ((yyvsp[0].reg)))
            return yyerror ("Invalid loop counter register");
	(yyval.instr) = bfin_gen_loop ((yyvsp[-1].expr), &(yyvsp[0].reg), 0, 0);
	}
#line 6321 "config/bfin-parse.c"
    break;

  case 209: /* asm_1: LOOP expr REG ASSIGN REG  */
#line 3568 "./config/bfin-parse.y"
        {
	  if (IS_RELOC ((yyvsp[-3].expr)) && IS_PREG ((yyvsp[0].reg)) && IS_CREG ((yyvsp[-2].reg)))
	    {
	      notethat ("Loop: LOOP expr counters = pregs\n");
	      (yyval.instr) = bfin_gen_loop ((yyvsp[-3].expr), &(yyvsp[-2].reg), 1, &(yyvsp[0].reg));
	    }
	  else
	    return yyerror ("Bad register or values for LOOP");
	}
#line 6335 "config/bfin-parse.c"
    break;

  case 210: /* asm_1: LOOP expr REG ASSIGN REG GREATER_GREATER expr  */
#line 3578 "./config/bfin-parse.y"
        {
	  if (IS_RELOC ((yyvsp[-5].expr)) && IS_PREG ((yyvsp[-2].reg)) && IS_CREG ((yyvsp[-4].reg)) && EXPR_VALUE ((yyvsp[0].expr)) == 1)
	    {
	      notethat ("Loop: LOOP expr counters = pregs >> 1\n");
	      (yyval.instr) = bfin_gen_loop ((yyvsp[-5].expr), &(yyvsp[-4].reg), 3, &(yyvsp[-2].reg));
	    }
	  else
	    return yyerror ("Bad register or values for LOOP");
	}
#line 6349 "config/bfin-parse.c"
    break;

  case 211: /* asm_1: LOOP_BEGIN NUMBER  */
#line 3590 "./config/bfin-parse.y"
        {
	  Expr_Node_Value val;
	  val.i_value = (yyvsp[0].value);
	  Expr_Node *tmp = Expr_Node_Create (Expr_Node_Constant, val, NULL, NULL);
	  bfin_loop_attempt_create_label (tmp, 1);
	  if (!IS_RELOC (tmp))
	    return yyerror ("Invalid expression in LOOP_BEGIN statement");
	  bfin_loop_beginend (tmp, 1);
	  (yyval.instr) = 0;
	}
#line 6364 "config/bfin-parse.c"
    break;

  case 212: /* asm_1: LOOP_BEGIN expr  */
#line 3601 "./config/bfin-parse.y"
        {
	  if (!IS_RELOC ((yyvsp[0].expr)))
	    return yyerror ("Invalid expression in LOOP_BEGIN statement");

	  bfin_loop_beginend ((yyvsp[0].expr), 1);
	  (yyval.instr) = 0;
	}
#line 6376 "config/bfin-parse.c"
    break;

  case 213: /* asm_1: LOOP_END NUMBER  */
#line 3611 "./config/bfin-parse.y"
        {
	  Expr_Node_Value val;
	  val.i_value = (yyvsp[0].value);
	  Expr_Node *tmp = Expr_Node_Create (Expr_Node_Constant, val, NULL, NULL);
	  bfin_loop_attempt_create_label (tmp, 1);
	  if (!IS_RELOC (tmp))
	    return yyerror ("Invalid expression in LOOP_END statement");
	  bfin_loop_beginend (tmp, 0);
	  (yyval.instr) = 0;
	}
#line 6391 "config/bfin-parse.c"
    break;

  case 214: /* asm_1: LOOP_END expr  */
#line 3622 "./config/bfin-parse.y"
        {
	  if (!IS_RELOC ((yyvsp[0].expr)))
	    return yyerror ("Invalid expression in LOOP_END statement");

	  bfin_loop_beginend ((yyvsp[0].expr), 0);
	  (yyval.instr) = 0;
	}
#line 6403 "config/bfin-parse.c"
    break;

  case 215: /* asm_1: ABORT  */
#line 3633 "./config/bfin-parse.y"
        {
	  notethat ("psedoDEBUG: ABORT\n");
	  (yyval.instr) = bfin_gen_pseudodbg (3, 3, 0);
	}
#line 6412 "config/bfin-parse.c"
    break;

  case 216: /* asm_1: DBG  */
#line 3639 "./config/bfin-parse.y"
        {
	  notethat ("pseudoDEBUG: DBG\n");
	  (yyval.instr) = bfin_gen_pseudodbg (3, 7, 0);
	}
#line 6421 "config/bfin-parse.c"
    break;

  case 217: /* asm_1: DBG REG_A  */
#line 3644 "./config/bfin-parse.y"
        {
	  notethat ("pseudoDEBUG: DBG REG_A\n");
	  (yyval.instr) = bfin_gen_pseudodbg (3, IS_A1 ((yyvsp[0].reg)), 0);
	}
#line 6430 "config/bfin-parse.c"
    break;

  case 218: /* asm_1: DBG REG  */
#line 3649 "./config/bfin-parse.y"
        {
	  notethat ("pseudoDEBUG: DBG allregs\n");
	  (yyval.instr) = bfin_gen_pseudodbg (0, (yyvsp[0].reg).regno & CODE_MASK, ((yyvsp[0].reg).regno & CLASS_MASK) >> 4);
	}
#line 6439 "config/bfin-parse.c"
    break;

  case 219: /* asm_1: DBGCMPLX LPAREN REG RPAREN  */
#line 3655 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[-1].reg)))
	    return yyerror ("Dregs expected");
	  notethat ("pseudoDEBUG: DBGCMPLX (dregs )\n");
	  (yyval.instr) = bfin_gen_pseudodbg (3, 6, ((yyvsp[-1].reg).regno & CODE_MASK) >> 4);
	}
#line 6450 "config/bfin-parse.c"
    break;

  case 220: /* asm_1: DBGHALT  */
#line 3663 "./config/bfin-parse.y"
        {
	  notethat ("psedoDEBUG: DBGHALT\n");
	  (yyval.instr) = bfin_gen_pseudodbg (3, 5, 0);
	}
#line 6459 "config/bfin-parse.c"
    break;

  case 221: /* asm_1: HLT  */
#line 3669 "./config/bfin-parse.y"
        {
	  notethat ("psedoDEBUG: HLT\n");
	  (yyval.instr) = bfin_gen_pseudodbg (3, 4, 0);
	}
#line 6468 "config/bfin-parse.c"
    break;

  case 222: /* asm_1: DBGA LPAREN HALF_REG COMMA expr RPAREN  */
#line 3675 "./config/bfin-parse.y"
        {
	  notethat ("pseudodbg_assert: DBGA (regs_lo/hi , uimm16 )\n");
	  (yyval.instr) = bfin_gen_pseudodbg_assert (IS_H ((yyvsp[-3].reg)), &(yyvsp[-3].reg), uimm16 ((yyvsp[-1].expr)));
	}
#line 6477 "config/bfin-parse.c"
    break;

  case 223: /* asm_1: DBGAH LPAREN REG COMMA expr RPAREN  */
#line 3681 "./config/bfin-parse.y"
        {
	  notethat ("pseudodbg_assert: DBGAH (regs , uimm16 )\n");
	  (yyval.instr) = bfin_gen_pseudodbg_assert (3, &(yyvsp[-3].reg), uimm16 ((yyvsp[-1].expr)));
	}
#line 6486 "config/bfin-parse.c"
    break;

  case 224: /* asm_1: DBGAL LPAREN REG COMMA expr RPAREN  */
#line 3687 "./config/bfin-parse.y"
        {
	  notethat ("psedodbg_assert: DBGAL (regs , uimm16 )\n");
	  (yyval.instr) = bfin_gen_pseudodbg_assert (2, &(yyvsp[-3].reg), uimm16 ((yyvsp[-1].expr)));
	}
#line 6495 "config/bfin-parse.c"
    break;

  case 225: /* asm_1: OUTC expr  */
#line 3693 "./config/bfin-parse.y"
        {
	  if (!IS_UIMM ((yyvsp[0].expr), 8))
	    return yyerror ("Constant out of range");
	  notethat ("psedodbg_assert: OUTC uimm8\n");
	  (yyval.instr) = bfin_gen_pseudochr (uimm8 ((yyvsp[0].expr)));
	}
#line 6506 "config/bfin-parse.c"
    break;

  case 226: /* asm_1: OUTC REG  */
#line 3701 "./config/bfin-parse.y"
        {
	  if (!IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Dregs expected");
	  notethat ("psedodbg_assert: OUTC dreg\n");
	  (yyval.instr) = bfin_gen_pseudodbg (2, (yyvsp[0].reg).regno & CODE_MASK, 0);
	}
#line 6517 "config/bfin-parse.c"
    break;

  case 227: /* REG_A: REG_A_DOUBLE_ZERO  */
#line 3715 "./config/bfin-parse.y"
        {
	(yyval.reg) = (yyvsp[0].reg);
	}
#line 6525 "config/bfin-parse.c"
    break;

  case 228: /* REG_A: REG_A_DOUBLE_ONE  */
#line 3719 "./config/bfin-parse.y"
        {
	(yyval.reg) = (yyvsp[0].reg);
	}
#line 6533 "config/bfin-parse.c"
    break;

  case 229: /* opt_mode: %empty  */
#line 3728 "./config/bfin-parse.y"
        {
	(yyval.mod).MM = 0;
	(yyval.mod).mod = 0;
	}
#line 6542 "config/bfin-parse.c"
    break;

  case 230: /* opt_mode: LPAREN M COMMA MMOD RPAREN  */
#line 3733 "./config/bfin-parse.y"
        {
	(yyval.mod).MM = 1;
	(yyval.mod).mod = (yyvsp[-1].value);
	}
#line 6551 "config/bfin-parse.c"
    break;

  case 231: /* opt_mode: LPAREN MMOD COMMA M RPAREN  */
#line 3738 "./config/bfin-parse.y"
        {
	(yyval.mod).MM = 1;
	(yyval.mod).mod = (yyvsp[-3].value);
	}
#line 6560 "config/bfin-parse.c"
    break;

  case 232: /* opt_mode: LPAREN MMOD RPAREN  */
#line 3743 "./config/bfin-parse.y"
        {
	(yyval.mod).MM = 0;
	(yyval.mod).mod = (yyvsp[-1].value);
	}
#line 6569 "config/bfin-parse.c"
    break;

  case 233: /* opt_mode: LPAREN M RPAREN  */
#line 3748 "./config/bfin-parse.y"
        {
	(yyval.mod).MM = 1;
	(yyval.mod).mod = 0;
	}
#line 6578 "config/bfin-parse.c"
    break;

  case 234: /* asr_asl: LPAREN ASL RPAREN  */
#line 3755 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 6586 "config/bfin-parse.c"
    break;

  case 235: /* asr_asl: LPAREN ASR RPAREN  */
#line 3759 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 6594 "config/bfin-parse.c"
    break;

  case 236: /* sco: %empty  */
#line 3765 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	}
#line 6603 "config/bfin-parse.c"
    break;

  case 237: /* sco: S  */
#line 3770 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = 1;
	(yyval.modcodes).x0 = 0;
	}
#line 6612 "config/bfin-parse.c"
    break;

  case 238: /* sco: CO  */
#line 3775 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 1;
	}
#line 6621 "config/bfin-parse.c"
    break;

  case 239: /* sco: SCO  */
#line 3780 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = 1;
	(yyval.modcodes).x0 = 1;
	}
#line 6630 "config/bfin-parse.c"
    break;

  case 240: /* asr_asl_0: ASL  */
#line 3788 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 6638 "config/bfin-parse.c"
    break;

  case 241: /* asr_asl_0: ASR  */
#line 3792 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 6646 "config/bfin-parse.c"
    break;

  case 242: /* amod0: %empty  */
#line 3798 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	}
#line 6655 "config/bfin-parse.c"
    break;

  case 243: /* amod0: LPAREN sco RPAREN  */
#line 3803 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = (yyvsp[-1].modcodes).s0;
	(yyval.modcodes).x0 = (yyvsp[-1].modcodes).x0;
	}
#line 6664 "config/bfin-parse.c"
    break;

  case 244: /* amod1: %empty  */
#line 3810 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	(yyval.modcodes).aop = 0;
	}
#line 6674 "config/bfin-parse.c"
    break;

  case 245: /* amod1: LPAREN NS RPAREN  */
#line 3816 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	(yyval.modcodes).aop = 1;
	}
#line 6684 "config/bfin-parse.c"
    break;

  case 246: /* amod1: LPAREN S RPAREN  */
#line 3822 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = 1;
	(yyval.modcodes).x0 = 0;
	(yyval.modcodes).aop = 1;
	}
#line 6694 "config/bfin-parse.c"
    break;

  case 247: /* amod2: %empty  */
#line 3830 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	}
#line 6704 "config/bfin-parse.c"
    break;

  case 248: /* amod2: LPAREN asr_asl_0 RPAREN  */
#line 3836 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 2 + (yyvsp[-1].r0).r0;
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	}
#line 6714 "config/bfin-parse.c"
    break;

  case 249: /* amod2: LPAREN sco RPAREN  */
#line 3842 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = (yyvsp[-1].modcodes).s0;
	(yyval.modcodes).x0 = (yyvsp[-1].modcodes).x0;
	}
#line 6724 "config/bfin-parse.c"
    break;

  case 250: /* amod2: LPAREN asr_asl_0 COMMA sco RPAREN  */
#line 3848 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 2 + (yyvsp[-3].r0).r0;
	(yyval.modcodes).s0 = (yyvsp[-1].modcodes).s0;
	(yyval.modcodes).x0 = (yyvsp[-1].modcodes).x0;
	}
#line 6734 "config/bfin-parse.c"
    break;

  case 251: /* amod2: LPAREN sco COMMA asr_asl_0 RPAREN  */
#line 3854 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 2 + (yyvsp[-1].r0).r0;
	(yyval.modcodes).s0 = (yyvsp[-3].modcodes).s0;
	(yyval.modcodes).x0 = (yyvsp[-3].modcodes).x0;
	}
#line 6744 "config/bfin-parse.c"
    break;

  case 252: /* xpmod: %empty  */
#line 3862 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 6752 "config/bfin-parse.c"
    break;

  case 253: /* xpmod: LPAREN Z RPAREN  */
#line 3866 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 6760 "config/bfin-parse.c"
    break;

  case 254: /* xpmod: LPAREN X RPAREN  */
#line 3870 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 6768 "config/bfin-parse.c"
    break;

  case 255: /* xpmod1: %empty  */
#line 3876 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 6776 "config/bfin-parse.c"
    break;

  case 256: /* xpmod1: LPAREN X RPAREN  */
#line 3880 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 6784 "config/bfin-parse.c"
    break;

  case 257: /* xpmod1: LPAREN Z RPAREN  */
#line 3884 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 6792 "config/bfin-parse.c"
    break;

  case 258: /* vsmod: %empty  */
#line 3890 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).aop = 0;
	}
#line 6802 "config/bfin-parse.c"
    break;

  case 259: /* vsmod: LPAREN NS RPAREN  */
#line 3896 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).aop = 3;
	}
#line 6812 "config/bfin-parse.c"
    break;

  case 260: /* vsmod: LPAREN S RPAREN  */
#line 3902 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = 1;
	(yyval.modcodes).aop = 3;
	}
#line 6822 "config/bfin-parse.c"
    break;

  case 261: /* vsmod: LPAREN V RPAREN  */
#line 3908 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).aop = 3;
	}
#line 6832 "config/bfin-parse.c"
    break;

  case 262: /* vsmod: LPAREN V COMMA S RPAREN  */
#line 3914 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 1;
	}
#line 6841 "config/bfin-parse.c"
    break;

  case 263: /* vsmod: LPAREN S COMMA V RPAREN  */
#line 3919 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 1;
	}
#line 6850 "config/bfin-parse.c"
    break;

  case 264: /* vmod: %empty  */
#line 3926 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 6858 "config/bfin-parse.c"
    break;

  case 265: /* vmod: LPAREN V RPAREN  */
#line 3930 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 6866 "config/bfin-parse.c"
    break;

  case 266: /* smod: %empty  */
#line 3936 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = 0;
	}
#line 6874 "config/bfin-parse.c"
    break;

  case 267: /* smod: LPAREN S RPAREN  */
#line 3940 "./config/bfin-parse.y"
        {
	(yyval.modcodes).s0 = 1;
	}
#line 6882 "config/bfin-parse.c"
    break;

  case 268: /* searchmod: GE  */
#line 3947 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 6890 "config/bfin-parse.c"
    break;

  case 269: /* searchmod: GT  */
#line 3951 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 6898 "config/bfin-parse.c"
    break;

  case 270: /* searchmod: LE  */
#line 3955 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 3;
	}
#line 6906 "config/bfin-parse.c"
    break;

  case 271: /* searchmod: LT  */
#line 3959 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 2;
	}
#line 6914 "config/bfin-parse.c"
    break;

  case 272: /* aligndir: %empty  */
#line 3965 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 6922 "config/bfin-parse.c"
    break;

  case 273: /* aligndir: LPAREN R RPAREN  */
#line 3969 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 6930 "config/bfin-parse.c"
    break;

  case 274: /* byteop_mod: LPAREN R RPAREN  */
#line 3976 "./config/bfin-parse.y"
        {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = 1;
	}
#line 6939 "config/bfin-parse.c"
    break;

  case 275: /* byteop_mod: LPAREN MMOD RPAREN  */
#line 3981 "./config/bfin-parse.y"
        {
	if ((yyvsp[-1].value) != M_T)
	  return yyerror ("Bad modifier");
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 0;
	}
#line 6950 "config/bfin-parse.c"
    break;

  case 276: /* byteop_mod: LPAREN MMOD COMMA R RPAREN  */
#line 3988 "./config/bfin-parse.y"
        {
	if ((yyvsp[-3].value) != M_T)
	  return yyerror ("Bad modifier");
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 1;
	}
#line 6961 "config/bfin-parse.c"
    break;

  case 277: /* byteop_mod: LPAREN R COMMA MMOD RPAREN  */
#line 3995 "./config/bfin-parse.y"
        {
	if ((yyvsp[-1].value) != M_T)
	  return yyerror ("Bad modifier");
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 1;
	}
#line 6972 "config/bfin-parse.c"
    break;

  case 278: /* c_align: ALIGN8  */
#line 4007 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 6980 "config/bfin-parse.c"
    break;

  case 279: /* c_align: ALIGN16  */
#line 4011 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 6988 "config/bfin-parse.c"
    break;

  case 280: /* c_align: ALIGN24  */
#line 4015 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 2;
	}
#line 6996 "config/bfin-parse.c"
    break;

  case 281: /* w32_or_nothing: %empty  */
#line 4021 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 7004 "config/bfin-parse.c"
    break;

  case 282: /* w32_or_nothing: LPAREN MMOD RPAREN  */
#line 4025 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-1].value) == M_W32)
	    (yyval.r0).r0 = 1;
	  else
	    return yyerror ("Only (W32) allowed");
	}
#line 7015 "config/bfin-parse.c"
    break;

  case 283: /* iu_or_nothing: %empty  */
#line 4034 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 7023 "config/bfin-parse.c"
    break;

  case 284: /* iu_or_nothing: LPAREN MMOD RPAREN  */
#line 4038 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-1].value) == M_IU)
	    (yyval.r0).r0 = 3;
	  else
	    return yyerror ("(IU) expected");
	}
#line 7034 "config/bfin-parse.c"
    break;

  case 285: /* reg_with_predec: LBRACK _MINUS_MINUS REG RBRACK  */
#line 4047 "./config/bfin-parse.y"
        {
	(yyval.reg) = (yyvsp[-1].reg);
	}
#line 7042 "config/bfin-parse.c"
    break;

  case 286: /* reg_with_postinc: LBRACK REG _PLUS_PLUS RBRACK  */
#line 4053 "./config/bfin-parse.y"
        {
	(yyval.reg) = (yyvsp[-2].reg);
	}
#line 7050 "config/bfin-parse.c"
    break;

  case 287: /* min_max: MIN  */
#line 4062 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 7058 "config/bfin-parse.c"
    break;

  case 288: /* min_max: MAX  */
#line 4066 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 7066 "config/bfin-parse.c"
    break;

  case 289: /* op_bar_op: _PLUS_BAR_PLUS  */
#line 4073 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 7074 "config/bfin-parse.c"
    break;

  case 290: /* op_bar_op: _PLUS_BAR_MINUS  */
#line 4077 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 7082 "config/bfin-parse.c"
    break;

  case 291: /* op_bar_op: _MINUS_BAR_PLUS  */
#line 4081 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 2;
	}
#line 7090 "config/bfin-parse.c"
    break;

  case 292: /* op_bar_op: _MINUS_BAR_MINUS  */
#line 4085 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 3;
	}
#line 7098 "config/bfin-parse.c"
    break;

  case 293: /* plus_minus: PLUS  */
#line 4092 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 7106 "config/bfin-parse.c"
    break;

  case 294: /* plus_minus: MINUS  */
#line 4096 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 7114 "config/bfin-parse.c"
    break;

  case 295: /* rnd_op: LPAREN RNDH RPAREN  */
#line 4103 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = 1;	/* HL.  */
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 0;	/* aop.  */
	}
#line 7125 "config/bfin-parse.c"
    break;

  case 296: /* rnd_op: LPAREN TH RPAREN  */
#line 4111 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = 1;	/* HL.  */
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 1;	/* aop.  */
	}
#line 7136 "config/bfin-parse.c"
    break;

  case 297: /* rnd_op: LPAREN RNDL RPAREN  */
#line 4119 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = 0;	/* HL.  */
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 0;	/* aop.  */
	}
#line 7147 "config/bfin-parse.c"
    break;

  case 298: /* rnd_op: LPAREN TL RPAREN  */
#line 4127 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = 0;	/* HL.  */
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 1;
	}
#line 7158 "config/bfin-parse.c"
    break;

  case 299: /* rnd_op: LPAREN RNDH COMMA R RPAREN  */
#line 4135 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = 1;	/* HL.  */
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 0;	/* aop.  */
	}
#line 7169 "config/bfin-parse.c"
    break;

  case 300: /* rnd_op: LPAREN TH COMMA R RPAREN  */
#line 4142 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = 1;	/* HL.  */
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 1;	/* aop.  */
	}
#line 7180 "config/bfin-parse.c"
    break;

  case 301: /* rnd_op: LPAREN RNDL COMMA R RPAREN  */
#line 4149 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = 0;	/* HL.  */
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 0;	/* aop.  */
	}
#line 7191 "config/bfin-parse.c"
    break;

  case 302: /* rnd_op: LPAREN TL COMMA R RPAREN  */
#line 4157 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = 0;	/* HL.  */
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 1;	/* aop.  */
	}
#line 7202 "config/bfin-parse.c"
    break;

  case 303: /* b3_op: LPAREN LO RPAREN  */
#line 4167 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* HL.  */
	}
#line 7211 "config/bfin-parse.c"
    break;

  case 304: /* b3_op: LPAREN HI RPAREN  */
#line 4172 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 1;	/* HL.  */
	}
#line 7220 "config/bfin-parse.c"
    break;

  case 305: /* b3_op: LPAREN LO COMMA R RPAREN  */
#line 4177 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* HL.  */
	}
#line 7229 "config/bfin-parse.c"
    break;

  case 306: /* b3_op: LPAREN HI COMMA R RPAREN  */
#line 4182 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 1;	/* HL.  */
	}
#line 7238 "config/bfin-parse.c"
    break;

  case 307: /* post_op: %empty  */
#line 4189 "./config/bfin-parse.y"
        {
	(yyval.modcodes).x0 = 2;
	}
#line 7246 "config/bfin-parse.c"
    break;

  case 308: /* post_op: _PLUS_PLUS  */
#line 4193 "./config/bfin-parse.y"
        {
	(yyval.modcodes).x0 = 0;
	}
#line 7254 "config/bfin-parse.c"
    break;

  case 309: /* post_op: _MINUS_MINUS  */
#line 4197 "./config/bfin-parse.y"
        {
	(yyval.modcodes).x0 = 1;
	}
#line 7262 "config/bfin-parse.c"
    break;

  case 310: /* a_assign: REG_A ASSIGN  */
#line 4206 "./config/bfin-parse.y"
        {
	(yyval.reg) = (yyvsp[-1].reg);
	}
#line 7270 "config/bfin-parse.c"
    break;

  case 311: /* a_minusassign: REG_A _MINUS_ASSIGN  */
#line 4213 "./config/bfin-parse.y"
        {
	(yyval.reg) = (yyvsp[-1].reg);
	}
#line 7278 "config/bfin-parse.c"
    break;

  case 312: /* a_plusassign: REG_A _PLUS_ASSIGN  */
#line 4220 "./config/bfin-parse.y"
        {
	(yyval.reg) = (yyvsp[-1].reg);
	}
#line 7286 "config/bfin-parse.c"
    break;

  case 313: /* assign_macfunc: REG ASSIGN REG_A  */
#line 4227 "./config/bfin-parse.y"
        {
	  if (IS_A1 ((yyvsp[0].reg)) && IS_EVEN ((yyvsp[-2].reg)))
	    return yyerror ("Cannot move A1 to even register");
	  else if (!IS_A1 ((yyvsp[0].reg)) && !IS_EVEN ((yyvsp[-2].reg)))
	    return yyerror ("Cannot move A0 to odd register");

	  (yyval.macfunc).w = 1;
          (yyval.macfunc).P = 1;
          (yyval.macfunc).n = IS_A1 ((yyvsp[0].reg));
	  (yyval.macfunc).op = 3;
          (yyval.macfunc).dst = (yyvsp[-2].reg);
	  (yyval.macfunc).s0.regno = 0;
          (yyval.macfunc).s1.regno = 0;
	}
#line 7305 "config/bfin-parse.c"
    break;

  case 314: /* assign_macfunc: a_macfunc  */
#line 4242 "./config/bfin-parse.y"
        {
	  (yyval.macfunc) = (yyvsp[0].macfunc);
	  (yyval.macfunc).w = 0; (yyval.macfunc).P = 0;
	  (yyval.macfunc).dst.regno = 0;
	}
#line 7315 "config/bfin-parse.c"
    break;

  case 315: /* assign_macfunc: REG ASSIGN LPAREN a_macfunc RPAREN  */
#line 4248 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-1].macfunc).n && IS_EVEN ((yyvsp[-4].reg)))
	    return yyerror ("Cannot move A1 to even register");
	  else if (!(yyvsp[-1].macfunc).n && !IS_EVEN ((yyvsp[-4].reg)))
	    return yyerror ("Cannot move A0 to odd register");

	  (yyval.macfunc) = (yyvsp[-1].macfunc);
	  (yyval.macfunc).w = 1;
          (yyval.macfunc).P = 1;
          (yyval.macfunc).dst = (yyvsp[-4].reg);
	}
#line 7331 "config/bfin-parse.c"
    break;

  case 316: /* assign_macfunc: HALF_REG ASSIGN LPAREN a_macfunc RPAREN  */
#line 4261 "./config/bfin-parse.y"
        {
	  if ((yyvsp[-1].macfunc).n && !IS_H ((yyvsp[-4].reg)))
	    return yyerror ("Cannot move A1 to low half of register");
	  else if (!(yyvsp[-1].macfunc).n && IS_H ((yyvsp[-4].reg)))
	    return yyerror ("Cannot move A0 to high half of register");

	  (yyval.macfunc) = (yyvsp[-1].macfunc);
	  (yyval.macfunc).w = 1;
	  (yyval.macfunc).P = 0;
          (yyval.macfunc).dst = (yyvsp[-4].reg);
	}
#line 7347 "config/bfin-parse.c"
    break;

  case 317: /* assign_macfunc: HALF_REG ASSIGN REG_A  */
#line 4274 "./config/bfin-parse.y"
        {
	  if (IS_A1 ((yyvsp[0].reg)) && !IS_H ((yyvsp[-2].reg)))
	    return yyerror ("Cannot move A1 to low half of register");
	  else if (!IS_A1 ((yyvsp[0].reg)) && IS_H ((yyvsp[-2].reg)))
	    return yyerror ("Cannot move A0 to high half of register");

	  (yyval.macfunc).w = 1;
	  (yyval.macfunc).P = 0;
	  (yyval.macfunc).n = IS_A1 ((yyvsp[0].reg));
	  (yyval.macfunc).op = 3;
          (yyval.macfunc).dst = (yyvsp[-2].reg);
	  (yyval.macfunc).s0.regno = 0;
          (yyval.macfunc).s1.regno = 0;
	}
#line 7366 "config/bfin-parse.c"
    break;

  case 318: /* a_macfunc: a_assign multiply_halfregs  */
#line 4292 "./config/bfin-parse.y"
        {
	  (yyval.macfunc).n = IS_A1 ((yyvsp[-1].reg));
	  (yyval.macfunc).op = 0;
	  (yyval.macfunc).s0 = (yyvsp[0].macfunc).s0;
	  (yyval.macfunc).s1 = (yyvsp[0].macfunc).s1;
	}
#line 7377 "config/bfin-parse.c"
    break;

  case 319: /* a_macfunc: a_plusassign multiply_halfregs  */
#line 4299 "./config/bfin-parse.y"
        {
	  (yyval.macfunc).n = IS_A1 ((yyvsp[-1].reg));
	  (yyval.macfunc).op = 1;
	  (yyval.macfunc).s0 = (yyvsp[0].macfunc).s0;
	  (yyval.macfunc).s1 = (yyvsp[0].macfunc).s1;
	}
#line 7388 "config/bfin-parse.c"
    break;

  case 320: /* a_macfunc: a_minusassign multiply_halfregs  */
#line 4306 "./config/bfin-parse.y"
        {
	  (yyval.macfunc).n = IS_A1 ((yyvsp[-1].reg));
	  (yyval.macfunc).op = 2;
	  (yyval.macfunc).s0 = (yyvsp[0].macfunc).s0;
	  (yyval.macfunc).s1 = (yyvsp[0].macfunc).s1;
	}
#line 7399 "config/bfin-parse.c"
    break;

  case 321: /* multiply_halfregs: HALF_REG STAR HALF_REG  */
#line 4316 "./config/bfin-parse.y"
        {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      (yyval.macfunc).s0 = (yyvsp[-2].reg);
              (yyval.macfunc).s1 = (yyvsp[0].reg);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
#line 7413 "config/bfin-parse.c"
    break;

  case 322: /* cc_op: ASSIGN  */
#line 4329 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 0;
	}
#line 7421 "config/bfin-parse.c"
    break;

  case 323: /* cc_op: _BAR_ASSIGN  */
#line 4333 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 1;
	}
#line 7429 "config/bfin-parse.c"
    break;

  case 324: /* cc_op: _AMPERSAND_ASSIGN  */
#line 4337 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 2;
	}
#line 7437 "config/bfin-parse.c"
    break;

  case 325: /* cc_op: _CARET_ASSIGN  */
#line 4341 "./config/bfin-parse.y"
        {
	(yyval.r0).r0 = 3;
	}
#line 7445 "config/bfin-parse.c"
    break;

  case 326: /* ccstat: CCREG cc_op STATUS_REG  */
#line 4348 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = (yyvsp[0].reg).regno;
	  (yyval.modcodes).x0 = (yyvsp[-1].r0).r0;
	  (yyval.modcodes).s0 = 0;
	}
#line 7455 "config/bfin-parse.c"
    break;

  case 327: /* ccstat: CCREG cc_op V  */
#line 4354 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = 0x18;
	  (yyval.modcodes).x0 = (yyvsp[-1].r0).r0;
	  (yyval.modcodes).s0 = 0;
	}
#line 7465 "config/bfin-parse.c"
    break;

  case 328: /* ccstat: STATUS_REG cc_op CCREG  */
#line 4360 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = (yyvsp[-2].reg).regno;
	  (yyval.modcodes).x0 = (yyvsp[-1].r0).r0;
	  (yyval.modcodes).s0 = 1;
	}
#line 7475 "config/bfin-parse.c"
    break;

  case 329: /* ccstat: V cc_op CCREG  */
#line 4366 "./config/bfin-parse.y"
        {
	  (yyval.modcodes).r0 = 0x18;
	  (yyval.modcodes).x0 = (yyvsp[-1].r0).r0;
	  (yyval.modcodes).s0 = 1;
	}
#line 7485 "config/bfin-parse.c"
    break;

  case 330: /* symbol: SYMBOL  */
#line 4376 "./config/bfin-parse.y"
        {
	Expr_Node_Value val;
	val.s_value = S_GET_NAME((yyvsp[0].symbol));
	(yyval.expr) = Expr_Node_Create (Expr_Node_Reloc, val, NULL, NULL);
	}
#line 7495 "config/bfin-parse.c"
    break;

  case 331: /* any_gotrel: GOT  */
#line 4385 "./config/bfin-parse.y"
        { (yyval.value) = BFD_RELOC_BFIN_GOT; }
#line 7501 "config/bfin-parse.c"
    break;

  case 332: /* any_gotrel: GOT17M4  */
#line 4387 "./config/bfin-parse.y"
        { (yyval.value) = BFD_RELOC_BFIN_GOT17M4; }
#line 7507 "config/bfin-parse.c"
    break;

  case 333: /* any_gotrel: FUNCDESC_GOT17M4  */
#line 4389 "./config/bfin-parse.y"
        { (yyval.value) = BFD_RELOC_BFIN_FUNCDESC_GOT17M4; }
#line 7513 "config/bfin-parse.c"
    break;

  case 334: /* got: symbol AT any_gotrel  */
#line 4393 "./config/bfin-parse.y"
        {
	Expr_Node_Value val;
	val.i_value = (yyvsp[0].value);
	(yyval.expr) = Expr_Node_Create (Expr_Node_GOT_Reloc, val, (yyvsp[-2].expr), NULL);
	}
#line 7523 "config/bfin-parse.c"
    break;

  case 335: /* got_or_expr: got  */
#line 4401 "./config/bfin-parse.y"
        {
	(yyval.expr) = (yyvsp[0].expr);
	}
#line 7531 "config/bfin-parse.c"
    break;

  case 336: /* got_or_expr: expr  */
#line 4405 "./config/bfin-parse.y"
        {
	(yyval.expr) = (yyvsp[0].expr);
	}
#line 7539 "config/bfin-parse.c"
    break;

  case 337: /* pltpc: symbol AT PLTPC  */
#line 4412 "./config/bfin-parse.y"
        {
	(yyval.expr) = (yyvsp[-2].expr);
	}
#line 7547 "config/bfin-parse.c"
    break;

  case 338: /* eterm: NUMBER  */
#line 4418 "./config/bfin-parse.y"
        {
	Expr_Node_Value val;
	val.i_value = (yyvsp[0].value);
	(yyval.expr) = Expr_Node_Create (Expr_Node_Constant, val, NULL, NULL);
	}
#line 7557 "config/bfin-parse.c"
    break;

  case 339: /* eterm: symbol  */
#line 4424 "./config/bfin-parse.y"
        {
	(yyval.expr) = (yyvsp[0].expr);
	}
#line 7565 "config/bfin-parse.c"
    break;

  case 340: /* eterm: LPAREN expr_1 RPAREN  */
#line 4428 "./config/bfin-parse.y"
        {
	(yyval.expr) = (yyvsp[-1].expr);
	}
#line 7573 "config/bfin-parse.c"
    break;

  case 341: /* eterm: TILDA expr_1  */
#line 4432 "./config/bfin-parse.y"
        {
	(yyval.expr) = unary (Expr_Op_Type_COMP, (yyvsp[0].expr));
	}
#line 7581 "config/bfin-parse.c"
    break;

  case 342: /* eterm: MINUS expr_1  */
#line 4436 "./config/bfin-parse.y"
        {
	(yyval.expr) = unary (Expr_Op_Type_NEG, (yyvsp[0].expr));
	}
#line 7589 "config/bfin-parse.c"
    break;

  case 343: /* expr: expr_1  */
#line 4442 "./config/bfin-parse.y"
        {
	(yyval.expr) = (yyvsp[0].expr);
	}
#line 7597 "config/bfin-parse.c"
    break;

  case 344: /* expr_1: expr_1 STAR expr_1  */
#line 4448 "./config/bfin-parse.y"
        {
	(yyval.expr) = binary (Expr_Op_Type_Mult, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
#line 7605 "config/bfin-parse.c"
    break;

  case 345: /* expr_1: expr_1 SLASH expr_1  */
#line 4452 "./config/bfin-parse.y"
        {
	(yyval.expr) = binary (Expr_Op_Type_Div, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
#line 7613 "config/bfin-parse.c"
    break;

  case 346: /* expr_1: expr_1 PERCENT expr_1  */
#line 4456 "./config/bfin-parse.y"
        {
	(yyval.expr) = binary (Expr_Op_Type_Mod, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
#line 7621 "config/bfin-parse.c"
    break;

  case 347: /* expr_1: expr_1 PLUS expr_1  */
#line 4460 "./config/bfin-parse.y"
        {
	(yyval.expr) = binary (Expr_Op_Type_Add, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
#line 7629 "config/bfin-parse.c"
    break;

  case 348: /* expr_1: expr_1 MINUS expr_1  */
#line 4464 "./config/bfin-parse.y"
        {
	(yyval.expr) = binary (Expr_Op_Type_Sub, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
#line 7637 "config/bfin-parse.c"
    break;

  case 349: /* expr_1: expr_1 LESS_LESS expr_1  */
#line 4468 "./config/bfin-parse.y"
        {
	(yyval.expr) = binary (Expr_Op_Type_Lshift, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
#line 7645 "config/bfin-parse.c"
    break;

  case 350: /* expr_1: expr_1 GREATER_GREATER expr_1  */
#line 4472 "./config/bfin-parse.y"
        {
	(yyval.expr) = binary (Expr_Op_Type_Rshift, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
#line 7653 "config/bfin-parse.c"
    break;

  case 351: /* expr_1: expr_1 AMPERSAND expr_1  */
#line 4476 "./config/bfin-parse.y"
        {
	(yyval.expr) = binary (Expr_Op_Type_BAND, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
#line 7661 "config/bfin-parse.c"
    break;

  case 352: /* expr_1: expr_1 CARET expr_1  */
#line 4480 "./config/bfin-parse.y"
        {
	(yyval.expr) = binary (Expr_Op_Type_LOR, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
#line 7669 "config/bfin-parse.c"
    break;

  case 353: /* expr_1: expr_1 BAR expr_1  */
#line 4484 "./config/bfin-parse.y"
        {
	(yyval.expr) = binary (Expr_Op_Type_BOR, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
#line 7677 "config/bfin-parse.c"
    break;

  case 354: /* expr_1: eterm  */
#line 4488 "./config/bfin-parse.y"
        {
	(yyval.expr) = (yyvsp[0].expr);
	}
#line 7685 "config/bfin-parse.c"
    break;


#line 7689 "config/bfin-parse.c"

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
      yyerror (YY_("syntax error"));
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

  return yyresult;
}

#line 4494 "./config/bfin-parse.y"


EXPR_T
mkexpr (int x, SYMBOL_T s)
{
  EXPR_T e = XNEW (struct expression_cell);
  e->value = x;
  EXPR_SYMBOL(e) = s;
  return e;
}

static int
value_match (Expr_Node *exp, int sz, int sign, int mul, int issigned)
{
  int umax = (1 << sz) - 1;
  int min = -(1 << (sz - 1));
  int max = (1 << (sz - 1)) - 1;

  int v = (EXPR_VALUE (exp)) & 0xffffffff;

  if ((v % mul) != 0)
    {
      error ("%s:%d: Value Error -- Must align to %d\n", __FILE__, __LINE__, mul);
      return 0;
    }

  v /= mul;

  if (sign)
    v = -v;

  if (issigned)
    {
      if (v >= min && v <= max) return 1;

#ifdef DEBUG
      fprintf(stderr, "signed value %lx out of range\n", v * mul);
#endif
      return 0;
    }
  if (v <= umax && v >= 0)
    return 1;
#ifdef DEBUG
  fprintf(stderr, "unsigned value %lx out of range\n", v * mul);
#endif
  return 0;
}

/* Return the expression structure that allows symbol operations.
   If the left and right children are constants, do the operation.  */
static Expr_Node *
binary (Expr_Op_Type op, Expr_Node *x, Expr_Node *y)
{
  Expr_Node_Value val;

  if (x->type == Expr_Node_Constant && y->type == Expr_Node_Constant)
    {
      switch (op)
	{
        case Expr_Op_Type_Add:
	  x->value.i_value += y->value.i_value;
	  break;
        case Expr_Op_Type_Sub:
	  x->value.i_value -= y->value.i_value;
	  break;
        case Expr_Op_Type_Mult:
	  x->value.i_value *= y->value.i_value;
	  break;
        case Expr_Op_Type_Div:
	  if (y->value.i_value == 0)
	    error ("Illegal Expression:  Division by zero.");
	  else
	    x->value.i_value /= y->value.i_value;
	  break;
        case Expr_Op_Type_Mod:
	  x->value.i_value %= y->value.i_value;
	  break;
        case Expr_Op_Type_Lshift:
	  x->value.i_value <<= y->value.i_value;
	  break;
        case Expr_Op_Type_Rshift:
	  x->value.i_value >>= y->value.i_value;
	  break;
        case Expr_Op_Type_BAND:
	  x->value.i_value &= y->value.i_value;
	  break;
        case Expr_Op_Type_BOR:
	  x->value.i_value |= y->value.i_value;
	  break;
        case Expr_Op_Type_BXOR:
	  x->value.i_value ^= y->value.i_value;
	  break;
        case Expr_Op_Type_LAND:
	  x->value.i_value = x->value.i_value && y->value.i_value;
	  break;
        case Expr_Op_Type_LOR:
	  x->value.i_value = x->value.i_value || y->value.i_value;
	  break;

	default:
	  error ("%s:%d: Internal assembler error\n", __FILE__, __LINE__);
	}
      return x;
    }
  /* Canonicalize order to EXPR OP CONSTANT.  */
  if (x->type == Expr_Node_Constant)
    {
      Expr_Node *t = x;
      x = y;
      y = t;
    }
  /* Canonicalize subtraction of const to addition of negated const.  */
  if (op == Expr_Op_Type_Sub && y->type == Expr_Node_Constant)
    {
      op = Expr_Op_Type_Add;
      y->value.i_value = -y->value.i_value;
    }
  if (y->type == Expr_Node_Constant && x->type == Expr_Node_Binop
      && x->Right_Child->type == Expr_Node_Constant)
    {
      if (op == x->value.op_value && x->value.op_value == Expr_Op_Type_Add)
	{
	  x->Right_Child->value.i_value += y->value.i_value;
	  return x;
	}
    }

  /* Create a new expression structure.  */
  val.op_value = op;
  return Expr_Node_Create (Expr_Node_Binop, val, x, y);
}

static Expr_Node *
unary (Expr_Op_Type op, Expr_Node *x)
{
  if (x->type == Expr_Node_Constant)
    {
      switch (op)
	{
	case Expr_Op_Type_NEG:
	  x->value.i_value = -x->value.i_value;
	  break;
	case Expr_Op_Type_COMP:
	  x->value.i_value = ~x->value.i_value;
	  break;
	default:
	  error ("%s:%d: Internal assembler error\n", __FILE__, __LINE__);
	}
      return x;
    }
  else
    {
      /* Create a new expression structure.  */
      Expr_Node_Value val;
      val.op_value = op;
      return Expr_Node_Create (Expr_Node_Unop, val, x, NULL);
    }
}

int debug_codeselection = 0;
static void
notethat (const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  if (debug_codeselection)
    {
      vfprintf (errorf, format, ap);
    }
  va_end (ap);
}

#ifdef TEST
main (int argc, char **argv)
{
  yyparse();
}
#endif

