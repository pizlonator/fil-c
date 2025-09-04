/*
 * Arithmetic code ripped out of ash shell for code sharing.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Original BSD copyright notice is retained at the end of this file.
 *
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1997-2005 Herbert Xu <herbert@gondor.apana.org.au>
 * was re-ported from NetBSD and debianized.
 *
 * rewrite arith.y to micro stack based cryptic algorithm by
 * Copyright (c) 2001 Aaron Lehmann <aaronl@vitelus.com>
 *
 * Modified by Paul Mundt <lethal@linux-sh.org> (c) 2004 to support
 * dynamic variables.
 *
 * Modified by Vladimir Oleynik <dzo@simtreas.ru> (c) 2001-2005 to be
 * used in busybox and size optimizations,
 * rewrote arith (see notes to this), added locale support,
 * rewrote dynamic variables.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
/* Copyright (c) 2001 Aaron Lehmann <aaronl@vitelus.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* This is my infix parser/evaluator. It is optimized for size, intended
 * as a replacement for yacc-based parsers. However, it may well be faster
 * than a comparable parser written in yacc. The supported operators are
 * listed in #defines below. Parens, order of operations, and error handling
 * are supported. This code is thread safe. The exact expression format should
 * be that which POSIX specifies for shells.
 *
 * The code uses a simple two-stack algorithm. See
 * http://www.onthenet.com.au/~grahamis/int2008/week02/lect02.html
 * for a detailed explanation of the infix-to-postfix algorithm on which
 * this is based (this code differs in that it applies operators immediately
 * to the stack instead of adding them to a queue to end up with an
 * expression).
 */
/*
 * Aug 24, 2001              Manuel Novoa III
 *
 * Reduced the generated code size by about 30% (i386) and fixed several bugs.
 *
 * 1) In arith_apply():
 *    a) Cached values of *numptr and &(numptr[-1]).
 *    b) Removed redundant test for zero denominator.
 *
 * 2) In arith():
 *    a) Eliminated redundant code for processing operator tokens by moving
 *       to a table-based implementation.  Also folded handling of parens
 *       into the table.
 *    b) Combined all 3 loops which called arith_apply to reduce generated
 *       code size at the cost of speed.
 *
 * 3) The following expressions were treated as valid by the original code:
 *       1()  ,    0!  ,    1 ( *3 )   .
 *    These bugs have been fixed by internally enclosing the expression in
 *    parens and then checking that all binary ops and right parens are
 *    preceded by a valid expression (NUM_TOKEN).
 *
 * Note: It may be desirable to replace Aaron's test for whitespace with
 * ctype's isspace() if it is used by another busybox applet or if additional
 * whitespace chars should be considered.  Look below the "#include"s for a
 * precompiler test.
 */
/*
 * Aug 26, 2001              Manuel Novoa III
 *
 * Return 0 for null expressions.  Pointed out by Vladimir Oleynik.
 *
 * Merge in Aaron's comments previously posted to the busybox list,
 * modified slightly to take account of my changes to the code.
 */
/*
 *  (C) 2003 Vladimir Oleynik <dzo@simtreas.ru>
 *
 * - allow access to variable,
 *   use recursive value indirection: c="2*2"; a="c"; echo $((a+=2)) produce 6
 * - implement assign syntax (VAR=expr, +=, *= etc)
 * - implement exponentiation (** operator)
 * - implement comma separated - expr, expr
 * - implement ++expr --expr expr++ expr--
 * - implement expr ? expr : expr (but second expr is always calculated)
 * - allow hexadecimal and octal numbers
 * - restore lost XOR operator
 * - protect $((num num)) as true zero expr (Manuel's error)
 * - always use special isspace(), see comment from bash ;-)
 */
#include "libbb.h"
#include "math.h"

#if 1
# define dbg(...) ((void)0)
#else
# define dbg(...) bb_error_msg(__VA_ARGS__)
#endif

typedef unsigned char operator;

/* An operator's token id is a bit of a bitfield. The lower 5 bits are the
 * precedence, and 3 high bits are an ID unique across operators of that
 * precedence. The ID portion is so that multiple operators can have the
 * same precedence, ensuring that the leftmost one is evaluated first.
 * Consider * and /
 */
#define tok_decl(prec,id)       (((id)<<5) | (prec))
#define ID_SHIFT                5
#define PREC(op)                ((op) & 0x1f)

#define PREC_LPAREN             0
#define TOK_LPAREN              tok_decl(0,0)
/* Precedence value of RPAREN is used only to distinguish it from LPAREN */
#define TOK_RPAREN              tok_decl(1,1)

#define TOK_COMMA               tok_decl(1,0)

/* All assignments are right associative and have the same precedence,
 * but there are 11 of them, which doesn't fit into 3 bits for unique id.
 * Abusing another precedence level:
 */
#define PREC_ASSIGN1            2
#define TOK_ASSIGN              tok_decl(2,0)
#define TOK_AND_ASSIGN          tok_decl(2,1)
#define TOK_OR_ASSIGN           tok_decl(2,2)
#define TOK_XOR_ASSIGN          tok_decl(2,3)
#define TOK_ADD_ASSIGN          tok_decl(2,4)
#define TOK_SUB_ASSIGN          tok_decl(2,5)
#define TOK_LSHIFT_ASSIGN       tok_decl(2,6)
#define TOK_RSHIFT_ASSIGN       tok_decl(2,7)

#define PREC_ASSIGN2            3
#define TOK_MUL_ASSIGN          tok_decl(3,0)
/* "/" and "/=" ops have the same id bits */
#define DIV_ID1                 1
#define TOK_DIV_ASSIGN          tok_decl(3,DIV_ID1)
#define TOK_REM_ASSIGN          tok_decl(3,2)

#define fix_assignment_prec(prec) do { prec -= (prec == 3); } while (0)

/* Ternary conditional operator is right associative too */
/*
 * bash documentation says that precedence order is:
 *  ...
 *  expr ? expr1 : expr2
 *  = *= /= %= += -= <<= >>= &= ^= |=
 *  exprA , exprB
 * What it omits is that expr1 is parsed as if parenthesized
 * (this matches the rules of ?: in C language):
 * "v ? 1,2 : 3,4" is parsed as "(v ? (1,2) : 3),4"
 * "v ? a=2 : b=4" is parsed as "(v ? (a=1) : b)=4" (thus, this is a syntax error)
 */
#define TOK_CONDITIONAL         tok_decl(4,0)
#define TOK_CONDITIONAL_SEP     tok_decl(4,1)

#define TOK_OR                  tok_decl(5,0)

#define TOK_AND                 tok_decl(6,0)

#define TOK_BOR                 tok_decl(7,0)

#define TOK_BXOR                tok_decl(8,0)

#define TOK_BAND                tok_decl(9,0)

#define TOK_EQ                  tok_decl(10,0)
#define TOK_NE                  tok_decl(10,1)

#define TOK_LT                  tok_decl(11,0)
#define TOK_GT                  tok_decl(11,1)
#define TOK_GE                  tok_decl(11,2)
#define TOK_LE                  tok_decl(11,3)

#define TOK_LSHIFT              tok_decl(12,0)
#define TOK_RSHIFT              tok_decl(12,1)

#define TOK_ADD                 tok_decl(13,0)
#define TOK_SUB                 tok_decl(13,1)

#define TOK_MUL                 tok_decl(14,0)
#define TOK_DIV                 tok_decl(14,DIV_ID1)
#define TOK_REM                 tok_decl(14,2)

/* Exponent is right associative */
#define TOK_EXPONENT            tok_decl(15,1)

/* Unary operators */
#define UNARYPREC               16
#define TOK_BNOT                tok_decl(UNARYPREC,0)
#define TOK_NOT                 tok_decl(UNARYPREC,1)

#define TOK_UMINUS              tok_decl(UNARYPREC+1,0)
#define TOK_UPLUS               tok_decl(UNARYPREC+1,1)

#define PREC_PRE                (UNARYPREC+2)
#define TOK_PRE_INC             tok_decl(PREC_PRE,0)
#define TOK_PRE_DEC             tok_decl(PREC_PRE,1)

#define PREC_POST               (UNARYPREC+3)
#define TOK_POST_INC            tok_decl(PREC_POST,0)
#define TOK_POST_DEC            tok_decl(PREC_POST,1)

/* TOK_VALUE marks a number, name, name++/name--, or (EXPR):
 * IOW: something which can be used as the left side of a binary op.
 * Since it's never pushed to opstack, its precedence does not matter.
 */
#define TOK_VALUE               tok_decl(PREC_POST,2)

static int
is_assign_op(operator op)
{
	operator prec = PREC(op);
	return prec == PREC_ASSIGN1
	|| prec == PREC_ASSIGN2
	|| prec == PREC_PRE
	|| prec == PREC_POST;
}

static int
is_right_associative(operator prec)
{
	return prec == PREC(TOK_ASSIGN)
	|| prec == PREC(TOK_EXPONENT)
	|| prec == PREC(TOK_CONDITIONAL);
}

typedef struct {
	arith_t val;
	const char *var_name;
} var_or_num_t;

#define VALID_NAME(name) (name)
#define NOT_NAME(name)   (!(name))

typedef struct remembered_name {
	struct remembered_name *next;
	const char *var_name;
} remembered_name;

static ALWAYS_INLINE int isalnum_(int c)
{
	return (isalnum(c) || c == '_');
}

static arith_t
evaluate_string(arith_state_t *math_state, const char *expr);

static arith_t
arith_lookup_val(arith_state_t *math_state, const char *name, char *endname)
{
	char c;
	const char *p;

	c = *endname;
	*endname = '\0';
	p = math_state->lookupvar(name);
	*endname = c;
	if (p) {
		arith_t val;
		size_t len = endname - name;
		remembered_name *cur;
		remembered_name remember;

		/* did we already see this name?
		 * testcase: a=b; b=a; echo $((a))
		 */
		for (cur = math_state->list_of_recursed_names; cur; cur = cur->next) {
			if (strncmp(cur->var_name, name, len) == 0
			 && !isalnum_(cur->var_name[len])
			) {
				/* yes */
				math_state->errmsg = "expression recursion loop detected";
				return -1;
			}
		}

		/* push current var name */
		remember.var_name = name;
		remember.next = math_state->list_of_recursed_names;
		math_state->list_of_recursed_names = &remember;

		/* recursively evaluate p as expression */
		/* this sets math_state->errmsg on error */
		val = evaluate_string(math_state, p);

		/* pop current var name */
		math_state->list_of_recursed_names = remember.next;

		return val;
	}
	/* treat undefined var as 0 */
	return 0;
}

/* "Applying" a token means performing it on the top elements on the integer
 * stack. For an unary operator it will only change the top element,
 * a binary operator will pop two arguments and push the result,
 * the ternary ?: op will pop three arguments and push the result.
 */
static NOINLINE const char*
arith_apply(arith_state_t *math_state, operator op, var_or_num_t *numstack, var_or_num_t **numstackptr)
{
#define NUMSTACKPTR (*numstackptr)

	var_or_num_t *top_of_stack;
	arith_t rez;

	/* There is no operator that can work without arguments */
	if (NUMSTACKPTR == numstack)
		goto syntax_err;

	top_of_stack = NUMSTACKPTR - 1;

	if (op == TOK_CONDITIONAL_SEP) {
		/* "expr1 ? expr2 : expr3" operation */
		var_or_num_t *expr1 = &top_of_stack[-2];
		NUMSTACKPTR = expr1 + 1;
		if (expr1 < numstack) /* Example: $((2:3)) */
			return "malformed ?: operator";
		if (expr1->val != 0) /* select expr2 or expr3 */
			top_of_stack--;
		rez = top_of_stack->val;
		top_of_stack = expr1;
		goto ret_rez;
	}
	if (op == TOK_CONDITIONAL) /* Example: $((a ? b)) */
		return "malformed ?: operator";

	rez = top_of_stack->val;
	if (op == TOK_UMINUS)
		rez = -rez;
	else if (op == TOK_NOT)
		rez = !rez;
	else if (op == TOK_BNOT)
		rez = ~rez;
	else if (op == TOK_POST_INC || op == TOK_PRE_INC)
		rez++;
	else if (op == TOK_POST_DEC || op == TOK_PRE_DEC)
		rez--;
	else /*if (op != TOK_UPLUS) - always true, we drop TOK_UPLUS earlier */ {
		/* Binary operators */
		arith_t right_side_val;

		if (top_of_stack == numstack) /* have two arguments? */
			goto syntax_err; /* no */

		/* Pop numstack */
		NUMSTACKPTR = top_of_stack; /* this decrements NUMSTACKPTR */

		if (math_state->evaluation_disabled) {
			dbg("binary op %02x skipped", op);
			return NULL;
			/* bash 5.2.12 does not execute "2/0" in disabled
			 * branches of ?: (and thus does not complain),
			 * but complains about negative exp: "2**-1".
			 * I don't think we need to emulate that.
			 */
		}

		top_of_stack--; /* now points to left side */
		right_side_val = rez;
		rez = top_of_stack->val;
		if (op == TOK_BOR || op == TOK_OR_ASSIGN)
			rez |= right_side_val;
		else if (op == TOK_OR)
			rez = right_side_val || rez;
		else if (op == TOK_BAND || op == TOK_AND_ASSIGN)
			rez &= right_side_val;
		else if (op == TOK_BXOR || op == TOK_XOR_ASSIGN)
			rez ^= right_side_val;
		else if (op == TOK_AND)
			rez = rez && right_side_val;
		else if (op == TOK_EQ)
			rez = (rez == right_side_val);
		else if (op == TOK_NE)
			rez = (rez != right_side_val);
		else if (op == TOK_GE)
			rez = (rez >= right_side_val);
		else if (op == TOK_RSHIFT || op == TOK_RSHIFT_ASSIGN)
			rez >>= right_side_val;
		else if (op == TOK_LSHIFT || op == TOK_LSHIFT_ASSIGN)
			rez <<= right_side_val;
		else if (op == TOK_GT)
			rez = (rez > right_side_val);
		else if (op == TOK_LT)
			rez = (rez < right_side_val);
		else if (op == TOK_LE)
			rez = (rez <= right_side_val);
		else if (op == TOK_MUL || op == TOK_MUL_ASSIGN)
			rez *= right_side_val;
		else if (op == TOK_ADD || op == TOK_ADD_ASSIGN)
			rez += right_side_val;
		else if (op == TOK_SUB || op == TOK_SUB_ASSIGN)
			rez -= right_side_val;
		else if (op == TOK_ASSIGN || op == TOK_COMMA)
			rez = right_side_val;
		else if (op == TOK_EXPONENT) {
			arith_t c;
			if (right_side_val < 0)
				return "exponent less than 0";
			c = 1;
			while (right_side_val != 0) {
				if ((right_side_val & 1) == 0) {
					/* this if() block is not necessary for correctness,
					 * but otherwise echo $((3**999999999999999999))
					 * takes a VERY LONG time
					 * (and it's not interruptible by ^C)
					 */
					rez *= rez;
					right_side_val >>= 1;
				}
				c *= rez;
				right_side_val--;
			}
			rez = c;
		}
		else /*if (op == TOK_DIV || op == TOK_DIV_ASSIGN
		      || op == TOK_REM || op == TOK_REM_ASSIGN) - always true */
		{
			if (right_side_val == 0)
				return "divide by zero";
			/*
			 * bash 4.2.45 x86 64bit: SEGV on 'echo $((2**63 / -1))'
			 *
			 * MAX_NEGATIVE_INT / -1 = MAX_POSITIVE_INT+1
			 * and thus is not representable.
			 * Some CPUs segfault trying such op.
			 * Others overflow MAX_POSITIVE_INT+1 to
			 * MAX_NEGATIVE_INT (0x7fff+1 = 0x8000).
			 * Make sure to at least not SEGV here:
			 */
			if (right_side_val == -1
			 && (rez << 1) == 0 /* MAX_NEGATIVE_INT or 0 */
			) {
				right_side_val = 1;
			}
			if (op & (DIV_ID1 << ID_SHIFT)) /* DIV or DIV_ASSIGN? */
				rez /= right_side_val;
			else
				rez %= right_side_val;
		}
	}

	if (math_state->evaluation_disabled) {
		dbg("unary op %02x skipped", op);
		return NULL;
	}

	if (is_assign_op(op)) {
		char buf[sizeof(arith_t)*3 + 2];

		if (NOT_NAME(top_of_stack->var_name)) {
			/* Hmm, 1=2 ? */
			goto syntax_err;
		}
		/* Save to shell variable */
		sprintf(buf, ARITH_FMT, rez);
		{
			char *e = (char*)endofname(top_of_stack->var_name);
			char c = *e;
			*e = '\0';
			math_state->setvar(top_of_stack->var_name, buf);
			*e = c;
		}
		/* VAR++ or VAR--? */
		if (PREC(op) == PREC_POST) {
			/* Do not store new value to stack (keep old value) */
			goto ret_NULL;
		}
	}
 ret_rez:
	top_of_stack->val = rez;
 ret_NULL:
	/* Erase var name, it is just a number now */
	top_of_stack->var_name = NULL;
	return NULL;
 syntax_err:
	return "arithmetic syntax error";
#undef NUMSTACKPTR
}

/* longest must be first */
static const char op_tokens[] ALIGN1 = {
	'<','<','=',0, TOK_LSHIFT_ASSIGN,
	'>','>','=',0, TOK_RSHIFT_ASSIGN,
	'<','<',    0, TOK_LSHIFT,
	'>','>',    0, TOK_RSHIFT,
	'|','|',    0, TOK_OR,
	'&','&',    0, TOK_AND,
	'!','=',    0, TOK_NE,
	'<','=',    0, TOK_LE,
	'>','=',    0, TOK_GE,
	'=','=',    0, TOK_EQ,
	'|','=',    0, TOK_OR_ASSIGN,
	'&','=',    0, TOK_AND_ASSIGN,
	'*','=',    0, TOK_MUL_ASSIGN,
	'/','=',    0, TOK_DIV_ASSIGN,
	'%','=',    0, TOK_REM_ASSIGN,
	'+','=',    0, TOK_ADD_ASSIGN,
	'-','=',    0, TOK_SUB_ASSIGN,
	'-','-',    0, TOK_POST_DEC,
	'^','=',    0, TOK_XOR_ASSIGN,
	'+','+',    0, TOK_POST_INC,
	'*','*',    0, TOK_EXPONENT,
	'!',        0, TOK_NOT,
	'<',        0, TOK_LT,
	'>',        0, TOK_GT,
	'=',        0, TOK_ASSIGN,
	'|',        0, TOK_BOR,
	'&',        0, TOK_BAND,
	'*',        0, TOK_MUL,
	'/',        0, TOK_DIV,
	'%',        0, TOK_REM,
	'+',        0, TOK_ADD,
	'-',        0, TOK_SUB,
	'^',        0, TOK_BXOR,
	'~',        0, TOK_BNOT,
	',',        0, TOK_COMMA,
	'?',        0, TOK_CONDITIONAL,
	':',        0, TOK_CONDITIONAL_SEP,
	')',        0, TOK_RPAREN,
	'(',        0, TOK_LPAREN,
	0
};
#define END_POINTER (&op_tokens[sizeof(op_tokens)-1])

#if ENABLE_FEATURE_SH_MATH_BASE
static arith_t parse_with_base(const char *nptr, char **endptr, unsigned base)
{
	arith_t n = 0;
	const char *start = nptr;

	for (;;) {
		unsigned digit = (unsigned)*nptr - '0';
		if (digit >= 10 /* not 0..9 */
		 && digit <= 'z' - '0' /* reject e.g. $((64#~)) */
		) {
			/* current char is one of :;<=>?@A..Z[\]^_`a..z */

			/* in bases up to 36, case does not matter for a-z,
			 * map @A..Z and `a..z to 9..35: */
			digit = (unsigned)(*nptr | 0x20) - ('a' - 10);
			if (base > 36 && *nptr <= '_') {
				/* base > 36: A-Z,@,_ are 36-61,62,63 */
				if (*nptr == '_')
					digit = 63;
				else if (*nptr == '@')
					digit = 62;
				else if (digit < 36) /* A-Z */
					digit += 36 - 10;
				else
					break; /* error: one of [\]^ */
			}
			//bb_error_msg("ch:'%c'%d digit:%u", *nptr, *nptr, digit);
			if (digit < 10) /* reject e.g. $((36#@)) */
				break;
		}
		if (digit >= base)
			break;
		/* bash does not check for overflows */
		n = n * base + digit;
		nptr++;
	}
	*endptr = (char*)nptr;
	/* "64#" and "64#+1" used to be valid expressions, but bash 5.2.15
	 * no longer allow such, detect this:
	 */
// NB: bash allows $((0x)), this is probably a bug...
	if (nptr == start)
		*endptr = NULL; /* there weren't any digits, bad */
	return n;
}

static arith_t strto_arith_t(const char *nptr, char **endptr)
{
/* NB: we do not use strtoull here to be bash-compatible:
 * $((99999999999999999999)) is 7766279631452241919
 * (the 64-bit truncated value).
 */
	unsigned base;

	/* nptr[0] is '0'..'9' here */

	base = nptr[0] - '0';
	if (base == 0) { /* nptr[0] is '0' */
		base = 8;
		if ((nptr[1] | 0x20) == 'x') {
			base = 16;
			nptr += 2;
		}
// NB: bash allows $((0x)), this is probably a bug...
		return parse_with_base(nptr, endptr, base);
	}

	/* base is 1..9 here */

	if (nptr[1] == '#') {
		if (base > 1)
			return parse_with_base(nptr + 2, endptr, base);
		/* else: "1#NN", bash says "invalid arithmetic base" */
	}

	if (isdigit(nptr[1]) && nptr[2] == '#') {
		base = 10 * base + (nptr[1] - '0');
		/* base is at least 10 here */
		if (base <= 64)
			return parse_with_base(nptr + 3, endptr, base);
		/* else: bash says "invalid arithmetic base" */
	}

	return parse_with_base(nptr, endptr, 10);
}
#else /* !ENABLE_FEATURE_SH_MATH_BASE */
# if ENABLE_FEATURE_SH_MATH_64
#  define strto_arith_t(nptr, endptr) strtoull(nptr, endptr, 0)
# else
#  define strto_arith_t(nptr, endptr) strtoul(nptr, endptr, 0)
# endif
#endif

static arith_t
evaluate_string(arith_state_t *math_state, const char *expr)
{
	/* Stack of integers/names */
	var_or_num_t *numstack, *numstackptr;
	/* Stack of operator tokens */
	operator *opstack, *opstackptr;
	/* To detect whether we are after a "value": */
	operator lasttok;
	/* To insert implicit () in ?: ternary op: */
	operator insert_op = 0xff;
	unsigned ternary_level = 0;
	const char *errmsg;
	const char *start_expr = expr = skip_whitespace(expr);

	{
		unsigned expr_len = strlen(expr);
		/* If LOTS of whitespace, do not blow up the estimation */
		const char *p = expr;
		while (*p) {
			/* in a run of whitespace, count only 1st char */
			if (isspace(*p)) {
				while (p++, isspace(*p))
					expr_len--;
			} else {
				p++;
			}
		}
		dbg("expr:'%s' expr_len:%u", expr, expr_len);
		/* expr_len deep opstack is needed. Think "------------7".
		 * Only "?" operator temporarily needs two opstack slots
		 * (IOW: more than one slot), but its second slot (LPAREN)
		 * is popped off when ":" is reached.
		 */
		expr_len++; /* +1 for 1st LPAREN. See what $((1?)) pushes to opstack */
		opstackptr = opstack = alloca(expr_len * sizeof(opstack[0]));
		/* There can be no more than (expr_len/2 + 1)
		 * integers/names in any given correct or incorrect expression.
		 * (modulo "09", "0v" cases where 2 chars are 2 ints/names,
		 * but we have code to detect that early)
		 */
		expr_len = (expr_len / 2)
			+ 1 /* "1+2" has two nums, 2 = len/2+1, NOT len/2 */;
		numstackptr = numstack = alloca(expr_len * sizeof(numstack[0]));
	}

	/* Start with a left paren */
	dbg("(%d) op:TOK_LPAREN", (int)(opstackptr - opstack));
	*opstackptr++ = lasttok = TOK_LPAREN;

	while (1) {
		const char *p;
		operator op;
		operator prec;

		expr = skip_whitespace(expr);
		if (*expr == '\0') {
			if (expr == start_expr) {
				/* Null expression */
				return 0;
			}

			/* This is only reached after all tokens have been extracted from the
			 * input stream. If there are still tokens on the operator stack, they
			 * are to be applied in order. At the end, there should be a final
			 * result on the integer stack */

			if (expr != END_POINTER) {
				/* If we haven't done so already,
				 * append a closing right paren
				 * and let the loop process it */
				expr = END_POINTER;
				op = TOK_RPAREN;
				goto tok_found1;
			}
			/* At this point, we're done with the expression */
			if (numstackptr != numstack + 1) {
				/* if there is not exactly one result, it's bad */
				/* Example: $((1 2)) */
				goto syntax_err;
			}
			return numstack->val;
		}

		p = endofname(expr);
		if (p != expr) {
			/* Name */
			if (!math_state->evaluation_disabled) {
				numstackptr->var_name = expr;
				dbg("[%d] var:'%.*s'", (int)(numstackptr - numstack), (int)(p - expr), expr);
				expr = skip_whitespace(p);
				/* If it is not followed by "=" operator... */
				if (expr[0] != '=' /* not "=..." */
				 || expr[1] == '=' /* or "==..." */
				) {
					/* Evaluate variable to value */
					arith_t val = arith_lookup_val(math_state, numstackptr->var_name, (char*)p);
					if (math_state->errmsg)
						return val; /* -1 */
					numstackptr->val = val;
				}
			} else {
				dbg("[%d] var:IGNORED", (int)(numstackptr - numstack));
				expr = p;
				numstackptr->var_name = NULL; /* not needed, paranoia */
				numstackptr->val = 0; /* not needed, paranoia */
			}
 push_value:
			numstackptr++;
			lasttok = TOK_VALUE;
			continue;
		}

		if (isdigit(*expr)) {
			/* Number */
			char *end;
			numstackptr->var_name = NULL;
			/* code is smaller compared to using &expr here: */
			numstackptr->val = strto_arith_t(expr, &end);
			expr = end;
			dbg("[%d] val:%lld", (int)(numstackptr - numstack), numstackptr->val);
			if (!expr) /* example: $((10#)) */
				goto syntax_err;
			/* A number can't be followed by another number, or a variable name.
			 * We'd catch this later anyway, but this would require numstack[]
			 * to be ~twice as deep to handle strings where _every_ char is
			 * a new number or name.
			 * Examples: "09" is two numbers, "0v" is number and name.
			 */
			if (isalnum(*expr) || *expr == '_')
				goto syntax_err;
			goto push_value;
		}

		/* Should be an operator */

		/* Special case: XYZ--, XYZ++, --XYZ, ++XYZ are recognized
		 * only if XYZ is a variable name, not a number or EXPR. IOW:
		 * "a+++v" is a++ + v.
		 * "(a)+++7" is ( a ) + + + 7.
		 * "7+++v" is 7 + ++v, not 7++ + v.
		 * "--7" is - - 7, not --7.
		 * "++++a" is + + ++a, not ++ ++a.
		 */
		if ((expr[0] == '+' || expr[0] == '-')
		 && (expr[1] == expr[0])
		) {
			if (numstackptr == numstack || NOT_NAME(numstackptr[-1].var_name)) {
				/* not a VAR++ */
				char next = skip_whitespace(expr + 2)[0];
				if (!(isalpha(next) || next == '_')) {
					/* not a ++VAR */
					op = (expr[0] == '+' ? TOK_ADD : TOK_SUB);
					expr++;
					goto tok_found1;
				}
			}
		}

		p = op_tokens;
		while (1) {
			/* Compare expr to current op_tokens[] element */
			const char *e = expr;
			while (1) {
				if (*p == '\0') {
					/* Match: operator is found */
					expr = e;
					goto tok_found;
				}
				if (*p != *e)
					break;
				p++;
				e++;
			}
			/* No match, go to next element of op_tokens[] */
			while (*p)
				p++;
			p += 2; /* skip NUL and TOK_foo bytes */
			if (*p == '\0') {
				/* No next element, operator not found */
				//math_state->syntax_error_at = expr;
				goto syntax_err;
			}
		}
		/* NB: expr now points past the operator */
 tok_found:
		op = p[1]; /* fetch TOK_foo value */

		/* Special rule for "? EXPR :"
		 * "EXPR in the middle of ? : is parsed as if parenthesized"
		 * (this quirk originates in C grammar, I think).
		 */
		if (op == TOK_CONDITIONAL) {
			insert_op = TOK_LPAREN;
			dbg("insert_op=%02x", insert_op);
		}
		if (op == TOK_CONDITIONAL_SEP) {
			insert_op = op;
			op = TOK_RPAREN;
			dbg("insert_op=%02x op=%02x", insert_op, op);
		}
 tok_found1:
		/* NAME++ is a "value" (something suitable for a binop) */
		if (PREC(lasttok) == PREC_POST)
			lasttok = TOK_VALUE;

		/* Plus and minus are binary (not unary) _only_ if the last
		 * token was a "value". Think about it. It makes sense.
		 */
		if (lasttok != TOK_VALUE) {
			switch (op) {
			case TOK_ADD:
				//op = TOK_UPLUS;
				//break;
				/* Unary plus does nothing, do not even push it to opstack */
				continue;
			case TOK_SUB:
				op = TOK_UMINUS;
				break;
			case TOK_POST_INC:
				op = TOK_PRE_INC;
				break;
			case TOK_POST_DEC:
				op = TOK_PRE_DEC;
				break;
			}
		}
		/* We don't want an unary operator to cause recursive descent on the
		 * stack, because there can be many in a row and it could cause an
		 * operator to be evaluated before its argument is pushed onto the
		 * integer stack.
		 * But for binary operators, "apply" everything on the operator
		 * stack until we find an operator with a lesser priority than the
		 * one we have just extracted. If op is right-associative,
		 * then stop "applying" on the equal priority too.
		 * Left paren will never be "applied" in this way.
		 */
		prec = PREC(op);
		if (prec != PREC_LPAREN && prec < UNARYPREC) {
			/* Binary, ternary or RPAREN */
			if (lasttok != TOK_VALUE) {
				/* Must be preceded by a value.
				 * $((2 2 + * 3)) would be accepted without this.
				 */
				goto syntax_err;
			}
			/* if op is RPAREN:
			 *     while opstack is not empty:
			 *         pop prev_op
			 *         if prev_op is LPAREN (finished evaluating (EXPR)):
			 *             goto N
			 *         evaluate prev_op on top of numstack
			 *     BUG (unpaired RPAREN)
			 * else (op is not RPAREN):
			 *     while opstack is not empty:
			 *         pop prev_op
			 *         if can't evaluate prev_op (it is lower precedence than op):
			 *             push prev_op back
			 *             goto C
			 *         evaluate prev_op on top of numstack
			 *     C:if op is "?": check result, set disable flag if needed
			 * push op
			 * N:loop to parse the rest of string
			 */
			while (opstackptr != opstack) {
				operator prev_op = *--opstackptr;
				if (op == TOK_RPAREN) {
					if (prev_op == TOK_LPAREN) {
						/* Erase var name: for example, (VAR) = 1 is not valid */
						numstackptr[-1].var_name = NULL;
						/* (EXPR) is a "value": next operator directly after
						 * close paren should be considered binary
						 */
						lasttok = TOK_VALUE;
						goto next;
					}
					/* Not (y), but ...x~y). Fall through to evaluate x~y */
				} else {
					operator prev_prec = PREC(prev_op);
					fix_assignment_prec(prec);
					fix_assignment_prec(prev_prec);
					if (prev_prec < prec
					 || (prev_prec == prec && is_right_associative(prec))
					) {
						/* ...x~y@. push @ on opstack */
						opstackptr++; /* undo removal of ~ op */
						goto check_cond;
					}
					/* else: ...x~y@. Evaluate x~y, replace it on stack with result. Then repeat */
				}
				dbg("arith_apply(prev_op:%02x, numstack:%d)", prev_op, (int)(numstackptr - numstack));
				errmsg = arith_apply(math_state, prev_op, numstack, &numstackptr);
				if (errmsg)
					goto err_with_custom_msg;
dbg("    numstack:%d val:%lld '%s'", (int)(numstackptr - numstack), numstackptr[-1].val, numstackptr[-1].var_name);
				if (prev_op == TOK_CONDITIONAL_SEP) {
					/* We just executed ":" */
					/* Remove "?" from opstack too, not just ":" */
					opstackptr--;
					if (*opstackptr != TOK_CONDITIONAL) {
						/* Example: $((1,2:3)) */
						errmsg = "malformed ?: operator";
						goto err_with_custom_msg;
					}
					/* Example: a=1?2:3,a. We just executed ":".
					 * Prevent assignment from being still disabled.
					 */
					if (ternary_level == math_state->evaluation_disabled) {
						math_state->evaluation_disabled = 0;
						dbg("':' executed: evaluation_disabled=CLEAR");
					}
					ternary_level--;
				}
			} /* while (opstack not empty) */

			if (op == TOK_RPAREN) /* unpaired RPAREN? */
				goto syntax_err;
 check_cond:
			if (op == TOK_CONDITIONAL) {
				/* We just now evaluated EXPR before "?".
				 * Should we disable evaluation now?
				 */
				ternary_level++;
				if (numstackptr[-1].val == 0 && !math_state->evaluation_disabled) {
					math_state->evaluation_disabled = ternary_level;
					dbg("'?' entered: evaluation_disabled=%u", math_state->evaluation_disabled);
				}
			}
		} /* if */
		/* else: LPAREN or UNARY: push it on opstack */

		/* Push this operator to opstack */
		dbg("(%d) op:%02x insert_op:%02x", (int)(opstackptr - opstack), op, insert_op);
		*opstackptr++ = lasttok = op;
 next:
		if (insert_op != 0xff) {
			op = insert_op;
			insert_op = 0xff;
			dbg("inserting %02x", op);
			if (op == TOK_CONDITIONAL_SEP) {
				/* The next token is ":". Toggle "do not evaluate" state */
				if (!math_state->evaluation_disabled) {
					math_state->evaluation_disabled = ternary_level;
					dbg("':' entered: evaluation_disabled=%u", math_state->evaluation_disabled);
				} else if (ternary_level == math_state->evaluation_disabled) {
					math_state->evaluation_disabled = 0;
					dbg("':' entered: evaluation_disabled=CLEAR");
				} /* else: ternary_level > evaluation_disabled && evaluation_disabled != 0 */
					/* We are in nested "?:" while in outer "?:" disabled branch */
					/* do_nothing */
			}
			goto tok_found1;
		}
	} /* while (1) */

 syntax_err:
	errmsg = "arithmetic syntax error";
 err_with_custom_msg:
	math_state->errmsg = errmsg;
	return -1;
}

arith_t FAST_FUNC
arith(arith_state_t *math_state, const char *expr)
{
	math_state->evaluation_disabled = 0;
	math_state->errmsg = NULL;
	math_state->list_of_recursed_names = NULL;
	return evaluate_string(math_state, expr);
}

/*
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
