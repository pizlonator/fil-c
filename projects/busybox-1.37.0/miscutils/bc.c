/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 * Adapted from https://github.com/gavinhoward/bc
 * Original code copyright (c) 2018 Gavin D. Howard and contributors.
 */
//TODO:
// maybe implement a^b for non-integer b? (see zbc_num_p())

#define DEBUG_LEXER   0
#define DEBUG_COMPILE 0
#define DEBUG_EXEC    0
// This can be left enabled for production as well:
#define SANITY_CHECKS 1

//config:config BC
//config:	bool "bc (38 kb)"
//config:	default y
//config:	select FEATURE_DC_BIG
//config:	help
//config:	bc is a command-line, arbitrary-precision calculator with a
//config:	Turing-complete language. See the GNU bc manual
//config:	(https://www.gnu.org/software/bc/manual/bc.html) and bc spec
//config:	(http://pubs.opengroup.org/onlinepubs/9699919799/utilities/bc.html).
//config:
//config:	This bc has five differences to the GNU bc:
//config:	  1) The period (.) is a shortcut for "last", as in the BSD bc.
//config:	  2) Arrays are copied before being passed as arguments to
//config:	     functions. This behavior is required by the bc spec.
//config:	  3) Arrays can be passed to the builtin "length" function to get
//config:	     the number of elements in the array. This prints "1":
//config:		a[0] = 0; length(a[])
//config:	  4) The precedence of the boolean "not" operator (!) is equal to
//config:	     that of the unary minus (-) negation operator. This still
//config:	     allows POSIX-compliant scripts to work while somewhat
//config:	     preserving expected behavior (versus C) and making parsing
//config:	     easier.
//config:	  5) "read()" accepts expressions, not only numeric literals.
//config:
//config:config DC
//config:	bool "dc (29 kb)"
//config:	default y
//config:	help
//config:	dc is a reverse-polish notation command-line calculator which
//config:	supports unlimited precision arithmetic. See the FreeBSD man page
//config:	(https://www.unix.com/man-page/FreeBSD/1/dc/) and GNU dc manual
//config:	(https://www.gnu.org/software/bc/manual/dc-1.05/html_mono/dc.html).
//config:
//config:	This dc has a few differences from the two above:
//config:	  1) When printing a byte stream (command "P"), this dc follows what
//config:	     the FreeBSD dc does.
//config:	  2) Implements the GNU extensions for divmod ("~") and
//config:	     modular exponentiation ("|").
//config:	  3) Implements all FreeBSD extensions, except for "J" and "M".
//config:	  4) Like the FreeBSD dc, this dc supports extended registers.
//config:	     However, they are implemented differently. When it encounters
//config:	     whitespace where a register should be, it skips the whitespace.
//config:	     If the character following is not a lowercase letter, an error
//config:	     is issued. Otherwise, the register name is parsed by the
//config:	     following regex: [a-z][a-z0-9_]*
//config:	     This generally means that register names will be surrounded by
//config:	     whitespace. Examples:
//config:		l idx s temp L index S temp2 < do_thing
//config:	     Also note that, like the FreeBSD dc, extended registers are not
//config:	     allowed unless the "-x" option is given.
//config:
//config:if BC || DC  # for menuconfig indenting
//config:
//config:config FEATURE_DC_BIG
//config:	bool "Use bc code base for dc (larger, more features)"
//config:	default y
//config:
//config:config FEATURE_DC_LIBM
//config:	bool "Enable power and exp functions (requires libm)"
//config:	default y
//config:	depends on DC && !BC && !FEATURE_DC_BIG
//config:	help
//config:	Enable power and exp functions.
//config:	NOTE: This will require libm to be present for linking.
//config:
//config:config FEATURE_BC_INTERACTIVE
//config:	bool "Interactive mode (+4kb)"
//config:	default y
//config:	depends on BC || (DC && FEATURE_DC_BIG)
//config:	help
//config:	Enable interactive mode: when started on a tty,
//config:	^C interrupts execution and returns to command line,
//config:	errors also return to command line instead of exiting,
//config:	line editing with history is available.
//config:
//config:	With this option off, input can still be taken from tty,
//config:	but all errors are fatal, ^C is fatal,
//config:	tty is treated exactly the same as any other
//config:	standard input (IOW: no line editing).
//config:
//config:config FEATURE_BC_LONG_OPTIONS
//config:	bool "Enable bc/dc long options"
//config:	default y
//config:	depends on BC || (DC && FEATURE_DC_BIG)
//config:
//config:endif

//applet:IF_BC(APPLET(bc, BB_DIR_USR_BIN, BB_SUID_DROP))
//applet:IF_DC(APPLET(dc, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_BC) += bc.o
//kbuild:lib-$(CONFIG_DC) += bc.o

//See www.gnu.org/software/bc/manual/bc.html
//usage:#define bc_trivial_usage
//usage:       "[-sqlw] [FILE]..."
//usage:
//usage:#define bc_full_usage "\n"
//usage:     "\nArbitrary precision calculator"
//usage:     "\n"
///////:     "\n	-i	Interactive" - has no effect for now
//usage:     "\n	-q	Quiet"
//usage:     "\n	-l	Load standard library"
//usage:     "\n	-s	Be POSIX compatible"
//usage:     "\n	-w	Warn if extensions are used"
///////:     "\n	-v	Version"
//usage:     "\n"
//usage:     "\n$BC_LINE_LENGTH changes output width"
//usage:
//usage:#define bc_example_usage
//usage:       "3 + 4.129\n"
//usage:       "1903 - 2893\n"
//usage:       "-129 * 213.28935\n"
//usage:       "12 / -1932\n"
//usage:       "12 % 12\n"
//usage:       "34 ^ 189\n"
//usage:       "scale = 13\n"
//usage:       "ibase = 2\n"
//usage:       "obase = A\n"
//usage:
//usage:#define dc_trivial_usage
//usage:       IF_FEATURE_DC_BIG("[-x] ")"[-eSCRIPT]... [-fFILE]... [FILE]..."
//usage:
//usage:#define dc_full_usage "\n"
//usage:     "\nTiny RPN calculator. Operations:"
//usage:     "\nArithmetic: + - * / % ^"
//usage:	IF_FEATURE_DC_BIG(
//usage:     "\n~ - divide with remainder"
//usage:     "\n| - modular exponentiation"
//usage:     "\nv - square root"
////////     "\nA-F -                   digits 10..15
////////     "\n_NNN -                  push negative number -NNN
////////     "\n[string] -              push string (in FreeBSD, \[, \] and \\ are escapes, not implemented here and in GNU)
////////     "\nR - DC_LEX_POP          pop and discard
////////     "\nc - DC_LEX_CLEAR_STACK  clear stack
////////     "\nd - DC_LEX_DUPLICATE    duplicate top-of-stack
////////     "\nr - DC_LEX_SWAP         swap top-of-stack
////////     "\n:r - DC_LEX_COLON       pop index, pop value, store to array 'r'
////////     "\n;r - DC_LEX_SCOLON      pop index, fetch from array 'r', push
////////     "\nLr - DC_LEX_LOAD_POP    pop register 'r', push
////////     "\nSr - DC_LEX_STORE_PUSH  pop, push to register 'r'
////////     "\nlr - DC_LEX_LOAD        read register 'r', push
////////     "\nsr - DC_LEX_OP_ASSIGN   pop, assign to register 'r'
////////     "\n? - DC_LEX_READ         read line and execute
////////     "\nx - DC_LEX_EXECUTE      pop string and execute
////////     "\n<r - XC_LEX_OP_REL_GT   pop, pop, execute register 'r' if top-of-stack was less
////////     "\n>r - XC_LEX_OP_REL_LT   pop, pop, execute register 'r' if top-of-stack was greater
////////     "\n=r - XC_LEX_OP_REL_EQ   pop, pop, execute register 'r' if equal
////////     "\n                        !<r !>r !=r - negated forms
////////     "\n                        >tef - "if greater execute register 't' else execute 'f'"
////////     "\nQ - DC_LEX_NQUIT        pop, "break N" from macro invocations
////////     "\nq - DC_LEX_QUIT         "break 2" (if less than 2 levels of macros, exit dc)
////////     "\nX - DC_LEX_SCALE_FACTOR pop, push number of fractional digits
////////     "\nZ - DC_LEX_LENGTH       pop, push number of digits it has (or number of characters in string)
////////     "\na - DC_LEX_ASCIIFY      pop, push low-order byte as char or 1st char of string
////////     "\n( - DC_LEX_LPAREN       (FreeBSD, not in GNU) pop, pop, if top-of-stack was less push 1 else push 0
////////     "\n{ - DC_LEX_LBRACE       (FreeBSD, not in GNU) pop, pop, if top-of-stack was less-or-equal push 1 else push 0
////////     "\nG - DC_LEX_EQ_NO_REG    (FreeBSD, not in GNU) pop, pop, if equal push 1 else push 0
////////     "\nN - DC_LEX_OP_BOOL_NOT  (FreeBSD, not in GNU) pop, if 0 push 1 else push 0
////////                                FreeBSD also has J and M commands, used internally by bc
////////     "\nn - DC_LEX_PRINT_POP    pop, print without newline
////////     "\nP - DC_LEX_PRINT_STREAM pop, print string or hex bytes
//usage:	)
//usage:     "\np - print top of the stack without popping"
//usage:     "\nf - print entire stack"
////////     "\nz - DC_LEX_STACK_LEVEL  push stack depth
////////     "\nK - DC_LEX_SCALE        push precision
////////     "\nI - DC_LEX_IBASE        push input radix
////////     "\nO - DC_LEX_OBASE        push output radix
//usage:	IF_FEATURE_DC_BIG(
//usage:     "\nk - pop the value and set precision"
//usage:     "\ni - pop the value and set input radix"
//usage:	)
//usage:     "\no - pop the value and set output radix"
//usage:     "\nExamples: dc -e'2 2 + p' -> 4, dc -e'8 8 * 2 2 + / p' -> 16"
//usage:
//usage:#define dc_example_usage
//usage:       "$ dc -e'2 2 + p'\n"
//usage:       "4\n"
//usage:       "$ dc -e'8 8 \\* 2 2 + / p'\n"
//usage:       "16\n"
//usage:       "$ dc -e'0 1 & p'\n"
//usage:       "0\n"
//usage:       "$ dc -e'0 1 | p'\n"
//usage:       "1\n"
//usage:       "$ echo '72 9 / 8 * p' | dc\n"
//usage:       "64\n"

#include "libbb.h"
#include "common_bufsiz.h"

#if !ENABLE_BC && !ENABLE_FEATURE_DC_BIG
# include "dc.c"
#else

#if DEBUG_LEXER
static uint8_t lex_indent;
#define dbg_lex(...) \
	do { \
		fprintf(stderr, "%*s", lex_indent, ""); \
		bb_error_msg(__VA_ARGS__); \
	} while (0)
#define dbg_lex_enter(...) \
	do { \
		dbg_lex(__VA_ARGS__); \
		lex_indent++; \
	} while (0)
#define dbg_lex_done(...) \
	do { \
		lex_indent--; \
		dbg_lex(__VA_ARGS__); \
	} while (0)
#else
# define dbg_lex(...)       ((void)0)
# define dbg_lex_enter(...) ((void)0)
# define dbg_lex_done(...)  ((void)0)
#endif

#if DEBUG_COMPILE
# define dbg_compile(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg_compile(...) ((void)0)
#endif

#if DEBUG_EXEC
# define dbg_exec(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg_exec(...) ((void)0)
#endif

typedef enum BcStatus {
	BC_STATUS_SUCCESS = 0,
	BC_STATUS_FAILURE = 1,
} BcStatus;

#define BC_VEC_INVALID_IDX  ((size_t) -1)
#define BC_VEC_START_CAP    (1 << 5)

typedef void (*BcVecFree)(void *) FAST_FUNC;

typedef struct BcVec {
	char *v;
	size_t len;
	size_t cap;
	size_t size;
	BcVecFree dtor;
} BcVec;

typedef signed char BcDig;

typedef struct BcNum {
	BcDig *restrict num;
	size_t rdx;
	size_t len;
	size_t cap;
	bool neg;
} BcNum;

#define BC_NUM_MAX_IBASE        36
// larger value might speed up BIGNUM calculations a bit:
#define BC_NUM_DEF_SIZE         16
#define BC_NUM_PRINT_WIDTH      70

#define BC_NUM_KARATSUBA_LEN    32

typedef enum BcInst {
#if ENABLE_BC
	BC_INST_INC_PRE,
	BC_INST_DEC_PRE,
	BC_INST_INC_POST,
	BC_INST_DEC_POST,
#endif
	XC_INST_NEG,            // order

	XC_INST_REL_EQ,         // should
	XC_INST_REL_LE,         // match
	XC_INST_REL_GE,         // LEX
	XC_INST_REL_NE,         // constants
	XC_INST_REL_LT,         // for
	XC_INST_REL_GT,         // these

	XC_INST_POWER,          // operations
	XC_INST_MULTIPLY,       // |
	XC_INST_DIVIDE,         // |
	XC_INST_MODULUS,        // |
	XC_INST_PLUS,           // |
	XC_INST_MINUS,          // |

	XC_INST_BOOL_NOT,       // |
	XC_INST_BOOL_OR,        // |
	XC_INST_BOOL_AND,       // |
#if ENABLE_BC
	BC_INST_ASSIGN_POWER,   // |
	BC_INST_ASSIGN_MULTIPLY,// |
	BC_INST_ASSIGN_DIVIDE,  // |
	BC_INST_ASSIGN_MODULUS, // |
	BC_INST_ASSIGN_PLUS,    // |
	BC_INST_ASSIGN_MINUS,   // |
#endif
	XC_INST_ASSIGN,         // V

	XC_INST_NUM,
	XC_INST_VAR,
	XC_INST_ARRAY_ELEM,
	XC_INST_ARRAY,
	XC_INST_SCALE_FUNC,

	XC_INST_IBASE,       // order of these constans should match other enums
	XC_INST_OBASE,       // order of these constans should match other enums
	XC_INST_SCALE,       // order of these constans should match other enums
	IF_BC(BC_INST_LAST,) // order of these constans should match other enums
	XC_INST_LENGTH,
	XC_INST_READ,
	XC_INST_SQRT,

	XC_INST_PRINT,
	XC_INST_PRINT_POP,
	XC_INST_STR,
	XC_INST_PRINT_STR,

#if ENABLE_BC
	BC_INST_HALT,
	BC_INST_JUMP,
	BC_INST_JUMP_ZERO,

	BC_INST_CALL,
	BC_INST_RET0,
#endif
	XC_INST_RET,

	XC_INST_POP,
#if ENABLE_DC
	DC_INST_POP_EXEC,

	DC_INST_MODEXP,
	DC_INST_DIVMOD,

	DC_INST_EXECUTE,
	DC_INST_EXEC_COND,

	DC_INST_ASCIIFY,
	DC_INST_PRINT_STREAM,

	DC_INST_PRINT_STACK,
	DC_INST_CLEAR_STACK,
	DC_INST_STACK_LEN,
	DC_INST_DUPLICATE,
	DC_INST_SWAP,

	DC_INST_LOAD,
	DC_INST_PUSH_VAR,
	DC_INST_PUSH_TO_VAR,

	DC_INST_QUIT,
	DC_INST_NQUIT,

	DC_INST_INVALID = -1,
#endif
} BcInst;

typedef struct BcId {
	char *name;
	size_t idx;
} BcId;

typedef struct BcFunc {
	BcVec code;
	IF_BC(BcVec labels;)
	IF_BC(BcVec autos;)
	IF_BC(BcVec strs;)
	IF_BC(BcVec consts;)
	IF_BC(size_t nparams;)
	IF_BC(bool voidfunc;)
} BcFunc;

typedef enum BcResultType {
	XC_RESULT_TEMP,
	IF_BC(BC_RESULT_VOID,) // same as TEMP, but INST_PRINT will ignore it

	XC_RESULT_VAR,
	XC_RESULT_ARRAY_ELEM,
	XC_RESULT_ARRAY,

	XC_RESULT_STR,

	//code uses "inst - XC_INST_IBASE + XC_RESULT_IBASE" construct,
	XC_RESULT_IBASE,       // relative order should match for: XC_INST_IBASE
	XC_RESULT_OBASE,       // relative order should match for: XC_INST_OBASE
	XC_RESULT_SCALE,       // relative order should match for: XC_INST_SCALE
	IF_BC(BC_RESULT_LAST,) // relative order should match for: BC_INST_LAST
	XC_RESULT_CONSTANT,
	IF_BC(BC_RESULT_ONE,)
} BcResultType;

typedef union BcResultData {
	BcNum n;
	BcVec v;
	BcId id;
} BcResultData;

typedef struct BcResult {
	BcResultType t;
	BcResultData d;
} BcResult;

typedef struct BcInstPtr {
	size_t func;
	size_t inst_idx;
} BcInstPtr;

typedef enum BcType {
	BC_TYPE_VAR,
	BC_TYPE_ARRAY,
	BC_TYPE_REF,
} BcType;

typedef enum BcLexType {
	XC_LEX_EOF,
	XC_LEX_INVALID,

	XC_LEX_NLINE,
	XC_LEX_WHITESPACE,
	XC_LEX_STR,
	XC_LEX_NAME,
	XC_LEX_NUMBER,

	XC_LEX_1st_op,
	XC_LEX_NEG = XC_LEX_1st_op,     // order

	XC_LEX_OP_REL_EQ,               // should
	XC_LEX_OP_REL_LE,               // match
	XC_LEX_OP_REL_GE,               // INST
	XC_LEX_OP_REL_NE,               // constants
	XC_LEX_OP_REL_LT,               // for
	XC_LEX_OP_REL_GT,               // these

	XC_LEX_OP_POWER,                // operations
	XC_LEX_OP_MULTIPLY,             // |
	XC_LEX_OP_DIVIDE,               // |
	XC_LEX_OP_MODULUS,              // |
	XC_LEX_OP_PLUS,                 // |
	XC_LEX_OP_MINUS,                // |
	XC_LEX_OP_last = XC_LEX_OP_MINUS,
#if ENABLE_BC
	BC_LEX_OP_BOOL_NOT,             // |
	BC_LEX_OP_BOOL_OR,              // |
	BC_LEX_OP_BOOL_AND,             // |

	BC_LEX_OP_ASSIGN_POWER,         // |
	BC_LEX_OP_ASSIGN_MULTIPLY,      // |
	BC_LEX_OP_ASSIGN_DIVIDE,        // |
	BC_LEX_OP_ASSIGN_MODULUS,       // |
	BC_LEX_OP_ASSIGN_PLUS,          // |
	BC_LEX_OP_ASSIGN_MINUS,         // |

	BC_LEX_OP_ASSIGN,               // V

	BC_LEX_OP_INC,
	BC_LEX_OP_DEC,

	BC_LEX_LPAREN, // () are 0x28 and 0x29
	BC_LEX_RPAREN, // must be LPAREN+1: code uses (c - '(' + BC_LEX_LPAREN)

	BC_LEX_LBRACKET, // [] are 0x5B and 0x5D
	BC_LEX_COMMA,
	BC_LEX_RBRACKET, // must be LBRACKET+2: code uses (c - '[' + BC_LEX_LBRACKET)

	BC_LEX_LBRACE, // {} are 0x7B and 0x7D
	BC_LEX_SCOLON,
	BC_LEX_RBRACE, // must be LBRACE+2: code uses (c - '{' + BC_LEX_LBRACE)

	BC_LEX_KEY_1st_keyword,
	BC_LEX_KEY_AUTO = BC_LEX_KEY_1st_keyword,
	BC_LEX_KEY_BREAK,
	BC_LEX_KEY_CONTINUE,
	BC_LEX_KEY_DEFINE,
	BC_LEX_KEY_ELSE,
	BC_LEX_KEY_FOR,
	BC_LEX_KEY_HALT,
	// code uses "type - BC_LEX_KEY_IBASE + XC_INST_IBASE" construct,
	BC_LEX_KEY_IBASE,    // relative order should match for: XC_INST_IBASE
	BC_LEX_KEY_OBASE,    // relative order should match for: XC_INST_OBASE
	BC_LEX_KEY_IF,
	BC_LEX_KEY_LAST,     // relative order should match for: BC_INST_LAST
	BC_LEX_KEY_LENGTH,
	BC_LEX_KEY_LIMITS,
	BC_LEX_KEY_PRINT,
	BC_LEX_KEY_QUIT,
	BC_LEX_KEY_READ,
	BC_LEX_KEY_RETURN,
	BC_LEX_KEY_SCALE,
	BC_LEX_KEY_SQRT,
	BC_LEX_KEY_WHILE,
#endif // ENABLE_BC

#if ENABLE_DC
	DC_LEX_OP_BOOL_NOT = XC_LEX_OP_last + 1,
	DC_LEX_OP_ASSIGN,

	DC_LEX_LPAREN,
	DC_LEX_SCOLON,
	DC_LEX_READ,
	DC_LEX_IBASE,
	DC_LEX_SCALE,
	DC_LEX_OBASE,
	DC_LEX_LENGTH,
	DC_LEX_PRINT,
	DC_LEX_QUIT,
	DC_LEX_SQRT,
	DC_LEX_LBRACE,

	DC_LEX_EQ_NO_REG,
	DC_LEX_OP_MODEXP,
	DC_LEX_OP_DIVMOD,

	DC_LEX_COLON,
	DC_LEX_ELSE,
	DC_LEX_EXECUTE,
	DC_LEX_PRINT_STACK,
	DC_LEX_CLEAR_STACK,
	DC_LEX_STACK_LEVEL,
	DC_LEX_DUPLICATE,
	DC_LEX_SWAP,
	DC_LEX_POP,

	DC_LEX_ASCIIFY,
	DC_LEX_PRINT_STREAM,

	// code uses "t - DC_LEX_STORE_IBASE + XC_INST_IBASE" construct,
	DC_LEX_STORE_IBASE,  // relative order should match for: XC_INST_IBASE
	DC_LEX_STORE_OBASE,  // relative order should match for: XC_INST_OBASE
	DC_LEX_STORE_SCALE,  // relative order should match for: XC_INST_SCALE
	DC_LEX_LOAD,
	DC_LEX_LOAD_POP,
	DC_LEX_STORE_PUSH,
	DC_LEX_PRINT_POP,
	DC_LEX_NQUIT,
	DC_LEX_SCALE_FACTOR,
#endif
} BcLexType;
// must match order of BC_LEX_KEY_foo etc above
#if ENABLE_BC
struct BcLexKeyword {
	char name8[8];
};
#define LEX_KW_ENTRY(a, b) \
	{ .name8 = a /*, .posix = b */ }
static const struct BcLexKeyword bc_lex_kws[20] ALIGN8 = {
	LEX_KW_ENTRY("auto"    , 1), // 0
	LEX_KW_ENTRY("break"   , 1), // 1
	LEX_KW_ENTRY("continue", 0), // 2 note: this one has no terminating NUL
	LEX_KW_ENTRY("define"  , 1), // 3
	LEX_KW_ENTRY("else"    , 0), // 4
	LEX_KW_ENTRY("for"     , 1), // 5
	LEX_KW_ENTRY("halt"    , 0), // 6
	LEX_KW_ENTRY("ibase"   , 1), // 7
	LEX_KW_ENTRY("obase"   , 1), // 8
	LEX_KW_ENTRY("if"      , 1), // 9
	LEX_KW_ENTRY("last"    , 0), // 10
	LEX_KW_ENTRY("length"  , 1), // 11
	LEX_KW_ENTRY("limits"  , 0), // 12
	LEX_KW_ENTRY("print"   , 0), // 13
	LEX_KW_ENTRY("quit"    , 1), // 14
	LEX_KW_ENTRY("read"    , 0), // 15
	LEX_KW_ENTRY("return"  , 1), // 16
	LEX_KW_ENTRY("scale"   , 1), // 17
	LEX_KW_ENTRY("sqrt"    , 1), // 18
	LEX_KW_ENTRY("while"   , 1), // 19
};
#undef LEX_KW_ENTRY
#define STRING_else  (bc_lex_kws[4].name8)
#define STRING_for   (bc_lex_kws[5].name8)
#define STRING_if    (bc_lex_kws[9].name8)
#define STRING_while (bc_lex_kws[19].name8)
enum {
	POSIX_KWORD_MASK = 0
		| (1 << 0)  // 0
		| (1 << 1)  // 1
		| (0 << 2)  // 2
		| (1 << 3)  // 3
		| (0 << 4)  // 4
		| (1 << 5)  // 5
		| (0 << 6)  // 6
		| (1 << 7)  // 7
		| (1 << 8)  // 8
		| (1 << 9)  // 9
		| (0 << 10) // 10
		| (1 << 11) // 11
		| (0 << 12) // 12
		| (0 << 13) // 13
		| (1 << 14) // 14
		| (0 << 15) // 15
		| (1 << 16) // 16
		| (1 << 17) // 17
		| (1 << 18) // 18
		| (1 << 19) // 19
};
#define keyword_is_POSIX(i) ((1 << (i)) & POSIX_KWORD_MASK)

// This is a bit array that corresponds to token types. An entry is
// true if the token is valid in an expression, false otherwise.
// Used to figure out when expr parsing should stop *without error message*
// - 0 element indicates this condition. 1 means "this token is to be eaten
// as part of the expression", it can then still be determined to be invalid
// by later processing.
enum {
#define EXBITS(a,b,c,d,e,f,g,h) \
	((uint64_t)((a << 0)+(b << 1)+(c << 2)+(d << 3)+(e << 4)+(f << 5)+(g << 6)+(h << 7)))
	BC_PARSE_EXPRS_BITS = 0              // corresponding BC_LEX_xyz:
	+ (EXBITS(0,0,0,0,0,1,1,1) << (0*8)) //  0: EOF    INVAL  NL     WS     STR    NAME   NUM    -
	+ (EXBITS(1,1,1,1,1,1,1,1) << (1*8)) //  8: ==     <=     >=     !=     <      >      ^      *
	+ (EXBITS(1,1,1,1,1,1,1,1) << (2*8)) // 16: /      %      +      -      !      ||     &&     ^=
	+ (EXBITS(1,1,1,1,1,1,1,1) << (3*8)) // 24: *=     /=     %=     +=     -=     =      ++     --
	+ (EXBITS(1,1,0,0,0,0,0,0) << (4*8)) // 32: (      )      [      ,      ]      {      ;      }
	+ (EXBITS(0,0,0,0,0,0,0,1) << (5*8)) // 40: auto   break  cont   define else   for    halt   ibase
	+ (EXBITS(1,0,1,1,0,0,0,1) << (6*8)) // 48: obase  if     last   length limits print  quit   read
	+ (EXBITS(0,1,1,0,0,0,0,0) << (7*8)) // 56: return scale  sqrt   while
#undef EXBITS
};
static ALWAYS_INLINE long lex_allowed_in_bc_expr(unsigned i)
{
#if ULONG_MAX > 0xffffffff
	// 64-bit version (will not work correctly for 32-bit longs!)
	return BC_PARSE_EXPRS_BITS & (1UL << i);
#else
	// 32-bit version
	unsigned long m = (uint32_t)BC_PARSE_EXPRS_BITS;
	if (i >= 32) {
		m = (uint32_t)(BC_PARSE_EXPRS_BITS >> 32);
		i &= 31;
	}
	return m & (1UL << i);
#endif
}

// This is an array of data for operators that correspond to
// [XC_LEX_1st_op...] token types.
static const uint8_t bc_ops_prec_and_assoc[] ALIGN1 = {
#define OP(p,l) ((int)(l) * 0x10 + (p))
	OP(1, false), // neg
	OP(6, true ), OP( 6, true  ), OP( 6, true  ), OP( 6, true  ), OP( 6, true  ), OP( 6, true ), // == <= >= != < >
	OP(2, false), // pow
	OP(3, true ), OP( 3, true  ), OP( 3, true  ), // mul div mod
	OP(4, true ), OP( 4, true  ), // + -
	OP(1, false), // not
	OP(7, true ), OP( 7, true  ), // or and
	OP(5, false), OP( 5, false ), OP( 5, false ), OP( 5, false ), OP( 5, false ), // ^= *= /= %= +=
	OP(5, false), OP( 5, false ), // -= =
	OP(0, false), OP( 0, false ), // inc dec
#undef OP
};
#define bc_operation_PREC(i) (bc_ops_prec_and_assoc[i] & 0x0f)
#define bc_operation_LEFT(i) (bc_ops_prec_and_assoc[i] & 0x10)
#endif // ENABLE_BC

#if ENABLE_DC
static const //BcLexType - should be this type
uint8_t
dc_char_to_LEX[] ALIGN1 = {
	// %&'(
	XC_LEX_OP_MODULUS, XC_LEX_INVALID, XC_LEX_INVALID, DC_LEX_LPAREN,
	// )*+,
	XC_LEX_INVALID, XC_LEX_OP_MULTIPLY, XC_LEX_OP_PLUS, XC_LEX_INVALID,
	// -./
	XC_LEX_OP_MINUS, XC_LEX_INVALID, XC_LEX_OP_DIVIDE,
	// 0123456789
	XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID,
	XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID,
	XC_LEX_INVALID, XC_LEX_INVALID,
	// :;<=>?@
	DC_LEX_COLON, DC_LEX_SCOLON, XC_LEX_OP_REL_GT, XC_LEX_OP_REL_EQ,
	XC_LEX_OP_REL_LT, DC_LEX_READ, XC_LEX_INVALID,
	// ABCDEFGH
	XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID,
	XC_LEX_INVALID, XC_LEX_INVALID, DC_LEX_EQ_NO_REG, XC_LEX_INVALID,
	// IJKLMNOP
	DC_LEX_IBASE, XC_LEX_INVALID, DC_LEX_SCALE, DC_LEX_LOAD_POP,
	XC_LEX_INVALID, DC_LEX_OP_BOOL_NOT, DC_LEX_OBASE, DC_LEX_PRINT_STREAM,
	// QRSTUVWX
	DC_LEX_NQUIT, DC_LEX_POP, DC_LEX_STORE_PUSH, XC_LEX_INVALID,
	XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID, DC_LEX_SCALE_FACTOR,
	// YZ
	XC_LEX_INVALID, DC_LEX_LENGTH,
	// [\]
	XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID,
	// ^_`
	XC_LEX_OP_POWER, XC_LEX_NEG, XC_LEX_INVALID,
	// abcdefgh
	DC_LEX_ASCIIFY, XC_LEX_INVALID, DC_LEX_CLEAR_STACK, DC_LEX_DUPLICATE,
	DC_LEX_ELSE, DC_LEX_PRINT_STACK, XC_LEX_INVALID, XC_LEX_INVALID,
	// ijklmnop
	DC_LEX_STORE_IBASE, XC_LEX_INVALID, DC_LEX_STORE_SCALE, DC_LEX_LOAD,
	XC_LEX_INVALID, DC_LEX_PRINT_POP, DC_LEX_STORE_OBASE, DC_LEX_PRINT,
	// qrstuvwx
	DC_LEX_QUIT, DC_LEX_SWAP, DC_LEX_OP_ASSIGN, XC_LEX_INVALID,
	XC_LEX_INVALID, DC_LEX_SQRT, XC_LEX_INVALID, DC_LEX_EXECUTE,
	// yz
	XC_LEX_INVALID, DC_LEX_STACK_LEVEL,
	// {|}~
	DC_LEX_LBRACE, DC_LEX_OP_MODEXP, XC_LEX_INVALID, DC_LEX_OP_DIVMOD,
};
static const //BcInst - should be this type. Using signed narrow type since DC_INST_INVALID is -1
int8_t
dc_LEX_to_INST[] ALIGN1 = { //starts at XC_LEX_OP_POWER // corresponding XC/DC_LEX_xyz:
	XC_INST_POWER,       XC_INST_MULTIPLY,          // XC_LEX_OP_POWER    XC_LEX_OP_MULTIPLY
	XC_INST_DIVIDE,      XC_INST_MODULUS,           // XC_LEX_OP_DIVIDE   XC_LEX_OP_MODULUS
	XC_INST_PLUS,        XC_INST_MINUS,             // XC_LEX_OP_PLUS     XC_LEX_OP_MINUS
	XC_INST_BOOL_NOT,                               // DC_LEX_OP_BOOL_NOT
	DC_INST_INVALID,                                // DC_LEX_OP_ASSIGN
	XC_INST_REL_GT,                                 // DC_LEX_LPAREN
	DC_INST_INVALID,                                // DC_LEX_SCOLON
	DC_INST_INVALID,                                // DC_LEX_READ
	XC_INST_IBASE,                                  // DC_LEX_IBASE
	XC_INST_SCALE,                                  // DC_LEX_SCALE
	XC_INST_OBASE,                                  // DC_LEX_OBASE
	XC_INST_LENGTH,                                 // DC_LEX_LENGTH
	XC_INST_PRINT,                                  // DC_LEX_PRINT
	DC_INST_QUIT,                                   // DC_LEX_QUIT
	XC_INST_SQRT,                                   // DC_LEX_SQRT
	XC_INST_REL_GE,                                 // DC_LEX_LBRACE
	XC_INST_REL_EQ,                                 // DC_LEX_EQ_NO_REG
	DC_INST_MODEXP,      DC_INST_DIVMOD,            // DC_LEX_OP_MODEXP   DC_LEX_OP_DIVMOD
	DC_INST_INVALID,     DC_INST_INVALID,           // DC_LEX_COLON       DC_LEX_ELSE
	DC_INST_EXECUTE,                                // DC_LEX_EXECUTE
	DC_INST_PRINT_STACK, DC_INST_CLEAR_STACK,       // DC_LEX_PRINT_STACK DC_LEX_CLEAR_STACK
	DC_INST_STACK_LEN,   DC_INST_DUPLICATE,         // DC_LEX_STACK_LEVEL DC_LEX_DUPLICATE
	DC_INST_SWAP,        XC_INST_POP,               // DC_LEX_SWAP        DC_LEX_POP
	DC_INST_ASCIIFY,     DC_INST_PRINT_STREAM,      // DC_LEX_ASCIIFY     DC_LEX_PRINT_STREAM
	DC_INST_INVALID,     DC_INST_INVALID,           // DC_LEX_STORE_IBASE DC_LEX_STORE_OBASE
	DC_INST_INVALID,     DC_INST_INVALID,           // DC_LEX_STORE_SCALE DC_LEX_LOAD
	DC_INST_INVALID,     DC_INST_INVALID,           // DC_LEX_LOAD_POP    DC_LEX_STORE_PUSH
	XC_INST_PRINT,       DC_INST_NQUIT,             // DC_LEX_PRINT_POP   DC_LEX_NQUIT
	XC_INST_SCALE_FUNC,                             // DC_LEX_SCALE_FACTOR
	// DC_INST_INVALID in this table either means that corresponding LEX
	// is not possible for dc, or that it does not compile one-to-one
	// to a single INST.
};
#endif // ENABLE_DC

typedef struct BcParse {
	smallint lex;      // was BcLexType // first member is most used
	smallint lex_last; // was BcLexType
	size_t lex_line;
	const char *lex_inbuf;
	const char *lex_next_at; // last lex_next() was called at this string
	const char *lex_filename;
	FILE *lex_input_fp;
	BcVec  lex_strnumbuf;

	BcFunc *func;
	size_t fidx;
	IF_BC(size_t in_funcdef;)
	IF_BC(BcVec exits;)
	IF_BC(BcVec conds;)
	IF_BC(BcVec ops;)
} BcParse;

typedef struct BcProgram {
	size_t len;
	size_t nchars;

	size_t scale;
	size_t ib_t;
	size_t ob_t;

	BcVec results;
	BcVec exestack;

	BcVec fns;
	IF_BC(BcVec fn_map;)

	BcVec vars;
	BcVec var_map;

	BcVec arrs;
	BcVec arr_map;

	IF_DC(BcVec strs;)
	IF_DC(BcVec consts;)

	BcNum zero;
	IF_BC(BcNum one;)
	IF_BC(BcNum last;)
} BcProgram;

struct globals {
	BcParse prs; // first member is most used

	// For error messages. Can be set to current parsed line,
	// or [TODO] to current executing line (can be before last parsed one)
	size_t err_line;

	BcVec input_buffer;

	IF_FEATURE_BC_INTERACTIVE(smallint ttyin;)
	IF_FEATURE_CLEAN_UP(smallint exiting;)

	BcProgram prog;

	BcVec files;

	char *env_args;

#if ENABLE_FEATURE_EDITING
	line_input_t *line_input_state;
#endif
} FIX_ALIASING;
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)
#define FREE_G() do { \
	FREE_PTR_TO_GLOBALS(); \
} while (0)
#define G_posix (ENABLE_BC && (option_mask32 & BC_FLAG_S))
#define G_warn  (ENABLE_BC && (option_mask32 & BC_FLAG_W))
#define G_exreg (ENABLE_DC && (option_mask32 & DC_FLAG_X))
#if ENABLE_FEATURE_BC_INTERACTIVE
# define G_interrupt bb_got_signal
# define G_ttyin     G.ttyin
#else
# define G_interrupt 0
# define G_ttyin     0
#endif
#if ENABLE_FEATURE_CLEAN_UP
# define G_exiting G.exiting
#else
# define G_exiting 0
#endif
#define IS_BC (ENABLE_BC && (!ENABLE_DC || applet_name[0] == 'b'))
#define IS_DC (ENABLE_DC && (!ENABLE_BC || applet_name[0] != 'b'))

#if ENABLE_BC
# define BC_PARSE_REL           (1 << 0)
# define BC_PARSE_PRINT         (1 << 1)
# define BC_PARSE_ARRAY         (1 << 2)
# define BC_PARSE_NOCALL        (1 << 3)
#endif

#define BC_PROG_MAIN      0
#define BC_PROG_READ      1
#if ENABLE_DC
#define BC_PROG_REQ_FUNCS 2
#endif

#define BC_FLAG_W (1 << 0)
#define BC_FLAG_V (1 << 1)
#define BC_FLAG_S (1 << 2)
#define BC_FLAG_Q (1 << 3)
#define BC_FLAG_L (1 << 4)
#define BC_FLAG_I ((1 << 5) * ENABLE_DC)
#define DC_FLAG_X ((1 << 6) * ENABLE_DC)

#define BC_MAX_OBASE    ((unsigned) 999)
#define BC_MAX_DIM      ((unsigned) INT_MAX)
#define BC_MAX_SCALE    ((unsigned) UINT_MAX)
#define BC_MAX_STRING   ((unsigned) UINT_MAX - 1)
#define BC_MAX_NUM      BC_MAX_STRING
// Unused apart from "limits" message. Just show a "biggish number" there.
//#define BC_MAX_EXP      ((unsigned long) LONG_MAX)
//#define BC_MAX_VARS     ((unsigned long) SIZE_MAX - 1)
#define BC_MAX_EXP_STR  "999999999"
#define BC_MAX_VARS_STR "999999999"

#define BC_MAX_OBASE_STR "999"

#if INT_MAX == 2147483647
# define BC_MAX_DIM_STR "2147483647"
#elif INT_MAX == 9223372036854775807
# define BC_MAX_DIM_STR "9223372036854775807"
#else
# error Strange INT_MAX
#endif

#if UINT_MAX == 4294967295U
# define BC_MAX_SCALE_STR  "4294967295"
# define BC_MAX_STRING_STR "4294967294"
#elif UINT_MAX == 18446744073709551615U
# define BC_MAX_SCALE_STR  "18446744073709551615"
# define BC_MAX_STRING_STR "18446744073709551614"
#else
# error Strange UINT_MAX
#endif
#define BC_MAX_NUM_STR BC_MAX_STRING_STR

// In configurations where errors abort instead of propagating error
// return code up the call chain, functions returning BC_STATUS
// actually don't return anything, they always succeed and return "void".
// A macro wrapper is provided, which makes this statement work:
//  s = zbc_func(...)
// and makes it visible to the compiler that s is always zero,
// allowing compiler to optimize dead code after the statement.
//
// To make code more readable, each such function has a "z"
// ("always returning zero") prefix, i.e. zbc_foo or zdc_foo.
//
#if ENABLE_FEATURE_BC_INTERACTIVE || ENABLE_FEATURE_CLEAN_UP
# define ERRORS_ARE_FATAL 0
# define ERRORFUNC        /*nothing*/
# define IF_ERROR_RETURN_POSSIBLE(a)  a
# define BC_STATUS        BcStatus
# define RETURN_STATUS(v) return (v)
# define COMMA_SUCCESS    /*nothing*/
#else
# define ERRORS_ARE_FATAL 1
# define ERRORFUNC        NORETURN
# define IF_ERROR_RETURN_POSSIBLE(a)  /*nothing*/
# define BC_STATUS        void
# define RETURN_STATUS(v) do { ((void)(v)); return; } while (0)
# define COMMA_SUCCESS    ,BC_STATUS_SUCCESS
#endif

//
// Utility routines
//

#define BC_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BC_MIN(a, b) ((a) < (b) ? (a) : (b))

static void fflush_and_check(void)
{
	fflush_all();
	if (ferror(stdout) || ferror(stderr))
		bb_simple_perror_msg_and_die("output error");
}

#if ENABLE_FEATURE_CLEAN_UP
#define QUIT_OR_RETURN_TO_MAIN \
do { \
	IF_FEATURE_BC_INTERACTIVE(G_ttyin = 0;) /* do not loop in main loop anymore */ \
	G_exiting = 1; \
	return BC_STATUS_FAILURE; \
} while (0)
#else
static void quit(void) NORETURN;
static void quit(void)
{
	if (ferror(stdin))
		bb_simple_perror_msg_and_die("input error");
	fflush_and_check();
	dbg_exec("quit(): exiting with exitcode SUCCESS");
	exit(0);
}
#define QUIT_OR_RETURN_TO_MAIN quit()
#endif

static void bc_verror_msg(const char *fmt, va_list p)
{
	const char *sv = sv; // for compiler
	if (G.prs.lex_filename) {
		sv = applet_name;
		applet_name = xasprintf("%s: %s:%lu", applet_name,
			G.prs.lex_filename, (unsigned long)G.err_line
		);
	}
	bb_verror_msg(fmt, p, NULL);
	if (G.prs.lex_filename) {
		free((char*)applet_name);
		applet_name = sv;
	}
}

static NOINLINE ERRORFUNC int bc_error_fmt(const char *fmt, ...)
{
	va_list p;

	va_start(p, fmt);
	bc_verror_msg(fmt, p);
	va_end(p);

	if (ENABLE_FEATURE_CLEAN_UP || G_ttyin)
		IF_ERROR_RETURN_POSSIBLE(return BC_STATUS_FAILURE);
	exit(1);
}

#if ENABLE_BC
static NOINLINE BC_STATUS zbc_posix_error_fmt(const char *fmt, ...)
{
	va_list p;

	// Are non-POSIX constructs totally ok?
	if (!(option_mask32 & (BC_FLAG_S|BC_FLAG_W)))
		RETURN_STATUS(BC_STATUS_SUCCESS); // yes

	va_start(p, fmt);
	bc_verror_msg(fmt, p);
	va_end(p);

	// Do we treat non-POSIX constructs as errors?
	if (!(option_mask32 & BC_FLAG_S))
		RETURN_STATUS(BC_STATUS_SUCCESS); // no, it's a warning

	if (ENABLE_FEATURE_CLEAN_UP || G_ttyin)
		RETURN_STATUS(BC_STATUS_FAILURE);
	exit(1);
}
#define zbc_posix_error_fmt(...) (zbc_posix_error_fmt(__VA_ARGS__) COMMA_SUCCESS)
#endif

// We use error functions with "return bc_error(FMT[, PARAMS])" idiom.
// This idiom begs for tail-call optimization, but for it to work,
// function must not have caller-cleaned parameters on stack.
// Unfortunately, vararg function API does exactly that on most arches.
// Thus, use these shims for the cases when we have no vararg PARAMS:
static ERRORFUNC int bc_error(const char *msg)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error_fmt("%s", msg);
}
static ERRORFUNC int bc_error_at(const char *msg)
{
	const char *err_at = G.prs.lex_next_at;
	if (err_at) {
		IF_ERROR_RETURN_POSSIBLE(return) bc_error_fmt(
			"%s at '%.*s'",
			msg,
			(int)(strchrnul(err_at, '\n') - err_at),
			err_at
		);
	}
	IF_ERROR_RETURN_POSSIBLE(return) bc_error_fmt("%s", msg);
}
static ERRORFUNC int bc_error_bad_character(char c)
{
	if (!c)
		IF_ERROR_RETURN_POSSIBLE(return) bc_error("NUL character");
	IF_ERROR_RETURN_POSSIBLE(return) bc_error_fmt("bad character '%c'", c);
}
#if ENABLE_BC
static ERRORFUNC int bc_error_bad_function_definition(void)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error_at("bad function definition");
}
#endif
static ERRORFUNC int bc_error_bad_expression(void)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error_at("bad expression");
}
static ERRORFUNC int bc_error_bad_assignment(void)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error_at(
		"bad assignment: left side must be variable or array element"
	);
}
static ERRORFUNC int bc_error_bad_token(void)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error_at("bad token");
}
static ERRORFUNC int bc_error_stack_has_too_few_elements(void)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error("stack has too few elements");
}
static ERRORFUNC int bc_error_variable_is_wrong_type(void)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error("variable is wrong type");
}
#if ENABLE_BC
static BC_STATUS zbc_POSIX_requires(const char *msg)
{
	RETURN_STATUS(zbc_posix_error_fmt("POSIX requires %s", msg));
}
#define zbc_POSIX_requires(...) (zbc_POSIX_requires(__VA_ARGS__) COMMA_SUCCESS)
static BC_STATUS zbc_POSIX_does_not_allow(const char *msg)
{
	RETURN_STATUS(zbc_posix_error_fmt("%s%s", "POSIX does not allow ", msg));
}
#define zbc_POSIX_does_not_allow(...) (zbc_POSIX_does_not_allow(__VA_ARGS__) COMMA_SUCCESS)
static BC_STATUS zbc_POSIX_does_not_allow_bool_ops_this_is_bad(const char *msg)
{
	RETURN_STATUS(zbc_posix_error_fmt("%s%s %s", "POSIX does not allow ", "boolean operators; this is bad:", msg));
}
#define zbc_POSIX_does_not_allow_bool_ops_this_is_bad(...) (zbc_POSIX_does_not_allow_bool_ops_this_is_bad(__VA_ARGS__) COMMA_SUCCESS)
static BC_STATUS zbc_POSIX_does_not_allow_empty_X_expression_in_for(const char *msg)
{
	RETURN_STATUS(zbc_posix_error_fmt("%san empty %s expression in 'for()'", "POSIX does not allow ", msg));
}
#define zbc_POSIX_does_not_allow_empty_X_expression_in_for(...) (zbc_POSIX_does_not_allow_empty_X_expression_in_for(__VA_ARGS__) COMMA_SUCCESS)
#endif

static void bc_vec_grow(BcVec *v, size_t n)
{
	size_t cap = v->cap * 2;
	while (cap < v->len + n) cap *= 2;
	v->v = xrealloc(v->v, v->size * cap);
	v->cap = cap;
}

static void bc_vec_init(BcVec *v, size_t esize, BcVecFree dtor)
{
	v->size = esize;
	v->cap = BC_VEC_START_CAP;
	v->len = 0;
	v->dtor = dtor;
	v->v = xmalloc(esize * BC_VEC_START_CAP);
}

static void bc_char_vec_init(BcVec *v)
{
	bc_vec_init(v, sizeof(char), NULL);
}

static void bc_vec_expand(BcVec *v, size_t req)
{
	if (v->cap < req) {
		v->v = xrealloc(v->v, v->size * req);
		v->cap = req;
	}
}

static void bc_vec_pop(BcVec *v)
{
	v->len--;
	if (v->dtor)
		v->dtor(v->v + (v->size * v->len));
}

static void bc_vec_npop(BcVec *v, size_t n)
{
	if (!v->dtor)
		v->len -= n;
	else {
		size_t len = v->len - n;
		while (v->len > len) v->dtor(v->v + (v->size * --v->len));
	}
}

static void bc_vec_pop_all(BcVec *v)
{
	bc_vec_npop(v, v->len);
}

static size_t bc_vec_npush(BcVec *v, size_t n, const void *data)
{
	size_t len = v->len;
	if (len + n > v->cap) bc_vec_grow(v, n);
	memmove(v->v + (v->size * len), data, v->size * n);
	v->len = len + n;
	return len;
}

static size_t bc_vec_push(BcVec *v, const void *data)
{
	return bc_vec_npush(v, 1, data);
	//size_t len = v->len;
	//if (len >= v->cap) bc_vec_grow(v, 1);
	//memmove(v->v + (v->size * len), data, v->size);
	//v->len = len + 1;
	//return len;
}

// G.prog.results often needs "pop old operand, push result" idiom.
// Can do this without a few extra ops
static size_t bc_result_pop_and_push(const void *data)
{
	BcVec *v = &G.prog.results;
	char *last;
	size_t len = v->len - 1;

	last = v->v + (v->size * len);
	if (v->dtor)
		v->dtor(last);
	memmove(last, data, v->size);
	return len;
}

static size_t bc_vec_pushByte(BcVec *v, char data)
{
	return bc_vec_push(v, &data);
}

static size_t bc_vec_pushZeroByte(BcVec *v)
{
	//return bc_vec_pushByte(v, '\0');
	// better:
	return bc_vec_push(v, &const_int_0);
}

static void bc_vec_pushAt(BcVec *v, const void *data, size_t idx)
{
	if (idx == v->len)
		bc_vec_push(v, data);
	else {
		char *ptr;

		if (v->len == v->cap) bc_vec_grow(v, 1);

		ptr = v->v + v->size * idx;

		memmove(ptr + v->size, ptr, v->size * (v->len++ - idx));
		memmove(ptr, data, v->size);
	}
}

static void bc_vec_string(BcVec *v, size_t len, const char *str)
{
	bc_vec_pop_all(v);
	bc_vec_expand(v, len + 1);
	memcpy(v->v, str, len);
	v->len = len;

	bc_vec_pushZeroByte(v);
}

static void *bc_vec_item(const BcVec *v, size_t idx)
{
	return v->v + v->size * idx;
}

static void *bc_vec_item_rev(const BcVec *v, size_t idx)
{
	return v->v + v->size * (v->len - idx - 1);
}

static void *bc_vec_top(const BcVec *v)
{
	return v->v + v->size * (v->len - 1);
}

static FAST_FUNC void bc_vec_free(void *vec)
{
	BcVec *v = (BcVec *) vec;
	bc_vec_pop_all(v);
	free(v->v);
}

static BcFunc* xc_program_func(size_t idx)
{
	return bc_vec_item(&G.prog.fns, idx);
}
// BC_PROG_MAIN is zeroth element, so:
#define xc_program_func_BC_PROG_MAIN() ((BcFunc*)(G.prog.fns.v))

#if ENABLE_BC
static BcFunc* bc_program_current_func(void)
{
	BcInstPtr *ip = bc_vec_top(&G.prog.exestack);
	BcFunc *func = xc_program_func(ip->func);
	return func;
}
#endif

static char** xc_program_str(size_t idx)
{
#if ENABLE_BC
	if (IS_BC) {
		BcFunc *func = bc_program_current_func();
		return bc_vec_item(&func->strs, idx);
	}
#endif
	IF_DC(return bc_vec_item(&G.prog.strs, idx);)
}

static char** xc_program_const(size_t idx)
{
#if ENABLE_BC
	if (IS_BC) {
		BcFunc *func = bc_program_current_func();
		return bc_vec_item(&func->consts, idx);
	}
#endif
	IF_DC(return bc_vec_item(&G.prog.consts, idx);)
}

static int bc_id_cmp(const void *e1, const void *e2)
{
	return strcmp(((const BcId *) e1)->name, ((const BcId *) e2)->name);
}

static FAST_FUNC void bc_id_free(void *id)
{
	free(((BcId *) id)->name);
}

static size_t bc_map_find_ge(const BcVec *v, const void *ptr)
{
	size_t low = 0, high = v->len;

	while (low < high) {
		size_t mid = (low + high) / 2;
		BcId *id = bc_vec_item(v, mid);
		int result = bc_id_cmp(ptr, id);

		if (result == 0)
			return mid;
		if (result < 0)
			high = mid;
		else
			low = mid + 1;
	}

	return low;
}

static int bc_map_insert(BcVec *v, const void *ptr, size_t *i)
{
	size_t n = *i = bc_map_find_ge(v, ptr);

	if (n == v->len)
		bc_vec_push(v, ptr);
	else if (!bc_id_cmp(ptr, bc_vec_item(v, n)))
		return 0; // "was not inserted"
	else
		bc_vec_pushAt(v, ptr, n);
	return 1; // "was inserted"
}

static size_t bc_map_find_exact(const BcVec *v, const void *ptr)
{
	size_t i = bc_map_find_ge(v, ptr);
	if (i >= v->len) return BC_VEC_INVALID_IDX;
	return bc_id_cmp(ptr, bc_vec_item(v, i)) ? BC_VEC_INVALID_IDX : i;
}

static void bc_num_setToZero(BcNum *n, size_t scale)
{
	n->len = 0;
	n->neg = false;
	n->rdx = scale;
}

static void bc_num_zero(BcNum *n)
{
	bc_num_setToZero(n, 0);
}

static void bc_num_one(BcNum *n)
{
	bc_num_setToZero(n, 0);
	n->len = 1;
	n->num[0] = 1;
}

// Note: this also sets BcNum to zero
static void bc_num_init(BcNum *n, size_t req)
{
	req = req >= BC_NUM_DEF_SIZE ? req : BC_NUM_DEF_SIZE;
	//memset(n, 0, sizeof(BcNum)); - cleared by assignments below
	n->num = xmalloc(req);
	n->cap = req;
	n->rdx = 0;
	n->len = 0;
	n->neg = false;
}

static void bc_num_init_DEF_SIZE(BcNum *n)
{
	bc_num_init(n, BC_NUM_DEF_SIZE);
}

static void bc_num_expand(BcNum *n, size_t req)
{
	req = req >= BC_NUM_DEF_SIZE ? req : BC_NUM_DEF_SIZE;
	if (req > n->cap) {
		n->num = xrealloc(n->num, req);
		n->cap = req;
	}
}

static FAST_FUNC void bc_num_free(void *num)
{
	free(((BcNum *) num)->num);
}

static void bc_num_copy(BcNum *d, BcNum *s)
{
	if (d != s) {
		bc_num_expand(d, s->cap);
		d->len = s->len;
		d->neg = s->neg;
		d->rdx = s->rdx;
		memcpy(d->num, s->num, sizeof(BcDig) * d->len);
	}
}

static void bc_num_init_and_copy(BcNum *d, BcNum *s)
{
	bc_num_init(d, s->len);
	bc_num_copy(d, s);
}

static BC_STATUS zbc_num_ulong_abs(BcNum *n, unsigned long *result_p)
{
	size_t i;
	unsigned long result;

	result = 0;
	i = n->len;
	while (i > n->rdx) {
		unsigned long prev = result;
		result = result * 10 + n->num[--i];
		// Even overflowed N*10 can still satisfy N*10>=N. For example,
		//    0x1ff00000 * 10 is 0x13f600000,
		// or 0x3f600000 truncated to 32 bits. Which is larger.
		// However, (N*10)/8 < N check is always correct.
		if ((result / 8) < prev)
			RETURN_STATUS(bc_error("overflow"));
	}
	*result_p = result;

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_num_ulong_abs(...) (zbc_num_ulong_abs(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_num_ulong(BcNum *n, unsigned long *result_p)
{
	if (n->neg) RETURN_STATUS(bc_error("negative number"));

	RETURN_STATUS(zbc_num_ulong_abs(n, result_p));
}
#define zbc_num_ulong(...) (zbc_num_ulong(__VA_ARGS__) COMMA_SUCCESS)

#if ULONG_MAX == 0xffffffffUL // 10 digits: 4294967295
# define ULONG_NUM_BUFSIZE (10 > BC_NUM_DEF_SIZE ? 10 : BC_NUM_DEF_SIZE)
#elif ULONG_MAX == 0xffffffffffffffffULL // 20 digits: 18446744073709551615
# define ULONG_NUM_BUFSIZE (20 > BC_NUM_DEF_SIZE ? 20 : BC_NUM_DEF_SIZE)
#endif
// minimum BC_NUM_DEF_SIZE, so that bc_num_expand() in bc_num_ulong2num()
// would not hit realloc() code path - not good if num[] is not malloced

static void bc_num_ulong2num(BcNum *n, unsigned long val)
{
	BcDig *ptr;

	bc_num_zero(n);

	if (val == 0) return;

	bc_num_expand(n, ULONG_NUM_BUFSIZE);

	ptr = n->num;
	for (;;) {
		n->len++;
		*ptr++ = val % 10;
		val /= 10;
		if (val == 0) break;
	}
}

static void bc_num_subArrays(BcDig *restrict a, BcDig *restrict b, size_t len)
{
	size_t i, j;
	for (i = 0; i < len; ++i) {
		a[i] -= b[i];
		for (j = i; a[j] < 0;) {
			a[j++] += 10;
			a[j] -= 1;
		}
	}
}

static ssize_t bc_num_compare(BcDig *restrict a, BcDig *restrict b, size_t len)
{
	size_t i = len;
	for (;;) {
		int c;
		if (i == 0)
			return 0;
		i--;
		c = a[i] - b[i];
		if (c != 0) {
			i++;
			if (c < 0)
				return -i;
			return i;
		}
	}
}

#define BC_NUM_NEG(n, neg)      ((((ssize_t)(n)) ^ -((ssize_t)(neg))) + (neg))
#define BC_NUM_ONE(n)           ((n)->len == 1 && (n)->rdx == 0 && (n)->num[0] == 1)
#define BC_NUM_INT(n)           ((n)->len - (n)->rdx)
//#define BC_NUM_AREQ(a, b)       (BC_MAX((a)->rdx, (b)->rdx) + BC_MAX(BC_NUM_INT(a), BC_NUM_INT(b)) + 1)
static /*ALWAYS_INLINE*/ size_t BC_NUM_AREQ(BcNum *a, BcNum *b)
{
	return BC_MAX(a->rdx, b->rdx) + BC_MAX(BC_NUM_INT(a), BC_NUM_INT(b)) + 1;
}
//#define BC_NUM_MREQ(a, b, scale) (BC_NUM_INT(a) + BC_NUM_INT(b) + BC_MAX((scale), (a)->rdx + (b)->rdx) + 1)
static /*ALWAYS_INLINE*/ size_t BC_NUM_MREQ(BcNum *a, BcNum *b, size_t scale)
{
	return BC_NUM_INT(a) + BC_NUM_INT(b) + BC_MAX(scale, a->rdx + b->rdx) + 1;
}

static ssize_t bc_num_cmp(BcNum *a, BcNum *b)
{
	size_t i, min, a_int, b_int, diff;
	BcDig *max_num, *min_num;
	bool a_max, neg;
	ssize_t cmp;

	if (a == b) return 0;
	if (a->len == 0) return BC_NUM_NEG(!!b->len, !b->neg);
	if (b->len == 0) return BC_NUM_NEG(1, a->neg);

	if (a->neg != b->neg) // signs of a and b differ
		// +a,-b = a>b = 1 or -a,+b = a<b = -1
		return (int)b->neg - (int)a->neg;
	neg = a->neg; // 1 if both negative, 0 if both positive

	a_int = BC_NUM_INT(a);
	b_int = BC_NUM_INT(b);
	a_int -= b_int;

	if (a_int != 0) {
		if (neg) return - (ssize_t) a_int;
		return (ssize_t) a_int;
	}

	a_max = (a->rdx > b->rdx);
	if (a_max) {
		min = b->rdx;
		diff = a->rdx - b->rdx;
		max_num = a->num + diff;
		min_num = b->num;
		// neg = (a_max == neg); - NOP (maps 1->1 and 0->0)
	} else {
		min = a->rdx;
		diff = b->rdx - a->rdx;
		max_num = b->num + diff;
		min_num = a->num;
		neg = !neg; // same as "neg = (a_max == neg)"
	}

	cmp = bc_num_compare(max_num, min_num, b_int + min);
	if (cmp != 0) return BC_NUM_NEG(cmp, neg);

	for (max_num -= diff, i = diff - 1; i < diff; --i) {
		if (max_num[i]) return BC_NUM_NEG(1, neg);
	}

	return 0;
}

static void bc_num_truncate(BcNum *n, size_t places)
{
	if (places == 0) return;

	n->rdx -= places;

	if (n->len != 0) {
		n->len -= places;
		memmove(n->num, n->num + places, n->len * sizeof(BcDig));
	}
}

static void bc_num_extend(BcNum *n, size_t places)
{
	size_t len = n->len + places;

	if (places != 0) {
		if (n->cap < len) bc_num_expand(n, len);

		memmove(n->num + places, n->num, sizeof(BcDig) * n->len);
		memset(n->num, 0, sizeof(BcDig) * places);

		n->len += places;
		n->rdx += places;
	}
}

static void bc_num_clean(BcNum *n)
{
	while (n->len > 0 && n->num[n->len - 1] == 0) --n->len;
	if (n->len == 0)
		n->neg = false;
	else if (n->len < n->rdx)
		n->len = n->rdx;
}

static void bc_num_retireMul(BcNum *n, size_t scale, bool neg1, bool neg2)
{
	if (n->rdx < scale)
		bc_num_extend(n, scale - n->rdx);
	else
		bc_num_truncate(n, n->rdx - scale);

	bc_num_clean(n);
	if (n->len != 0) n->neg = !neg1 != !neg2;
}

static void bc_num_split(BcNum *restrict n, size_t idx, BcNum *restrict a,
                         BcNum *restrict b)
{
	if (idx < n->len) {
		b->len = n->len - idx;
		a->len = idx;
		a->rdx = b->rdx = 0;

		memcpy(b->num, n->num + idx, b->len * sizeof(BcDig));
		memcpy(a->num, n->num, idx * sizeof(BcDig));
	} else {
		bc_num_zero(b);
		bc_num_copy(a, n);
	}

	bc_num_clean(a);
	bc_num_clean(b);
}

static BC_STATUS zbc_num_shift(BcNum *n, size_t places)
{
	if (places == 0 || n->len == 0) RETURN_STATUS(BC_STATUS_SUCCESS);

	// This check makes sense only if size_t is (much) larger than BC_MAX_NUM.
	if (SIZE_MAX > (BC_MAX_NUM | 0xff)) {
		if (places + n->len > BC_MAX_NUM)
			RETURN_STATUS(bc_error("number too long: must be [1,"BC_MAX_NUM_STR"]"));
	}

	if (n->rdx >= places)
		n->rdx -= places;
	else {
		bc_num_extend(n, places - n->rdx);
		n->rdx = 0;
	}

	bc_num_clean(n);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_num_shift(...) (zbc_num_shift(__VA_ARGS__) COMMA_SUCCESS)

typedef BC_STATUS (*BcNumBinaryOp)(BcNum *, BcNum *, BcNum *, size_t) FAST_FUNC;

static BC_STATUS zbc_num_binary(BcNum *a, BcNum *b, BcNum *c, size_t scale,
                              BcNumBinaryOp op, size_t req)
{
	BcStatus s;
	BcNum num2, *ptr_a, *ptr_b;
	bool init = false;

	if (c == a) {
		ptr_a = &num2;
		memcpy(ptr_a, c, sizeof(BcNum));
		init = true;
	} else
		ptr_a = a;

	if (c == b) {
		ptr_b = &num2;
		if (c != a) {
			memcpy(ptr_b, c, sizeof(BcNum));
			init = true;
		}
	} else
		ptr_b = b;

	if (init)
		bc_num_init(c, req);
	else
		bc_num_expand(c, req);

	s = BC_STATUS_SUCCESS;
	IF_ERROR_RETURN_POSSIBLE(s =) op(ptr_a, ptr_b, c, scale);

	if (init) bc_num_free(&num2);

	RETURN_STATUS(s);
}
#define zbc_num_binary(...) (zbc_num_binary(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_a(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);
static FAST_FUNC BC_STATUS zbc_num_s(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);
static FAST_FUNC BC_STATUS zbc_num_p(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);
static FAST_FUNC BC_STATUS zbc_num_m(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);
static FAST_FUNC BC_STATUS zbc_num_d(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);
static FAST_FUNC BC_STATUS zbc_num_rem(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);

static FAST_FUNC BC_STATUS zbc_num_add(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	BcNumBinaryOp op = (!a->neg == !b->neg) ? zbc_num_a : zbc_num_s;
	(void) scale;
	RETURN_STATUS(zbc_num_binary(a, b, c, false, op, BC_NUM_AREQ(a, b)));
}

static FAST_FUNC BC_STATUS zbc_num_sub(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	BcNumBinaryOp op = (!a->neg == !b->neg) ? zbc_num_s : zbc_num_a;
	(void) scale;
	RETURN_STATUS(zbc_num_binary(a, b, c, true, op, BC_NUM_AREQ(a, b)));
}

static FAST_FUNC BC_STATUS zbc_num_mul(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	size_t req = BC_NUM_MREQ(a, b, scale);
	RETURN_STATUS(zbc_num_binary(a, b, c, scale, zbc_num_m, req));
}

static FAST_FUNC BC_STATUS zbc_num_div(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	size_t req = BC_NUM_MREQ(a, b, scale);
	RETURN_STATUS(zbc_num_binary(a, b, c, scale, zbc_num_d, req));
}

static FAST_FUNC BC_STATUS zbc_num_mod(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	size_t req = BC_NUM_MREQ(a, b, scale);
	RETURN_STATUS(zbc_num_binary(a, b, c, scale, zbc_num_rem, req));
}

static FAST_FUNC BC_STATUS zbc_num_pow(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	RETURN_STATUS(zbc_num_binary(a, b, c, scale, zbc_num_p, a->len * b->len + 1));
}

static const BcNumBinaryOp zxc_program_ops[] = {
	zbc_num_pow, zbc_num_mul, zbc_num_div, zbc_num_mod, zbc_num_add, zbc_num_sub,
};
#define zbc_num_add(...) (zbc_num_add(__VA_ARGS__) COMMA_SUCCESS)
#define zbc_num_sub(...) (zbc_num_sub(__VA_ARGS__) COMMA_SUCCESS)
#define zbc_num_mul(...) (zbc_num_mul(__VA_ARGS__) COMMA_SUCCESS)
#define zbc_num_div(...) (zbc_num_div(__VA_ARGS__) COMMA_SUCCESS)
#define zbc_num_mod(...) (zbc_num_mod(__VA_ARGS__) COMMA_SUCCESS)
#define zbc_num_pow(...) (zbc_num_pow(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_num_inv(BcNum *a, BcNum *b, size_t scale)
{
	BcNum one;
	BcDig num[2];

	one.cap = 2;
	one.num = num;
	bc_num_one(&one);

	RETURN_STATUS(zbc_num_div(&one, a, b, scale));
}
#define zbc_num_inv(...) (zbc_num_inv(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_a(BcNum *a, BcNum *b, BcNum *restrict c, size_t sub)
{
	BcDig *ptr, *ptr_a, *ptr_b, *ptr_c;
	size_t i, max, min_rdx, min_int, diff, a_int, b_int;
	unsigned carry;

	// Because this function doesn't need to use scale (per the bc spec),
	// I am hijacking it to say whether it's doing an add or a subtract.

	if (a->len == 0) {
		bc_num_copy(c, b);
		if (sub && c->len) c->neg = !c->neg;
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (b->len == 0) {
		bc_num_copy(c, a);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	c->neg = a->neg;
	c->rdx = BC_MAX(a->rdx, b->rdx);
	min_rdx = BC_MIN(a->rdx, b->rdx);
	c->len = 0;

	if (a->rdx > b->rdx) {
		diff = a->rdx - b->rdx;
		ptr = a->num;
		ptr_a = a->num + diff;
		ptr_b = b->num;
	} else {
		diff = b->rdx - a->rdx;
		ptr = b->num;
		ptr_a = a->num;
		ptr_b = b->num + diff;
	}

	ptr_c = c->num;
	for (i = 0; i < diff; ++i, ++c->len)
		ptr_c[i] = ptr[i];

	ptr_c += diff;
	a_int = BC_NUM_INT(a);
	b_int = BC_NUM_INT(b);

	if (a_int > b_int) {
		min_int = b_int;
		max = a_int;
		ptr = ptr_a;
	} else {
		min_int = a_int;
		max = b_int;
		ptr = ptr_b;
	}

	carry = 0;
	for (i = 0; i < min_rdx + min_int; ++i) {
		unsigned in = (unsigned)ptr_a[i] + (unsigned)ptr_b[i] + carry;
		carry = in / 10;
		ptr_c[i] = (BcDig)(in % 10);
	}
	for (; i < max + min_rdx; ++i) {
		unsigned in = (unsigned)ptr[i] + carry;
		carry = in / 10;
		ptr_c[i] = (BcDig)(in % 10);
	}
	c->len += i;

	if (carry != 0) c->num[c->len++] = (BcDig) carry;

	RETURN_STATUS(BC_STATUS_SUCCESS); // can't make void, see zbc_num_binary()
}

static FAST_FUNC BC_STATUS zbc_num_s(BcNum *a, BcNum *b, BcNum *restrict c, size_t sub)
{
	ssize_t cmp;
	BcNum *minuend, *subtrahend;
	size_t start;
	bool aneg, bneg, neg;

	// Because this function doesn't need to use scale (per the bc spec),
	// I am hijacking it to say whether it's doing an add or a subtract.

	if (a->len == 0) {
		bc_num_copy(c, b);
		if (sub && c->len) c->neg = !c->neg;
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (b->len == 0) {
		bc_num_copy(c, a);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	aneg = a->neg;
	bneg = b->neg;
	a->neg = b->neg = false;

	cmp = bc_num_cmp(a, b);

	a->neg = aneg;
	b->neg = bneg;

	if (cmp == 0) {
		bc_num_setToZero(c, BC_MAX(a->rdx, b->rdx));
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (cmp > 0) {
		neg = a->neg;
		minuend = a;
		subtrahend = b;
	} else {
		neg = b->neg;
		if (sub) neg = !neg;
		minuend = b;
		subtrahend = a;
	}

	bc_num_copy(c, minuend);
	c->neg = neg;

	if (c->rdx < subtrahend->rdx) {
		bc_num_extend(c, subtrahend->rdx - c->rdx);
		start = 0;
	} else
		start = c->rdx - subtrahend->rdx;

	bc_num_subArrays(c->num + start, subtrahend->num, subtrahend->len);

	bc_num_clean(c);

	RETURN_STATUS(BC_STATUS_SUCCESS); // can't make void, see zbc_num_binary()
}

static FAST_FUNC BC_STATUS zbc_num_k(BcNum *restrict a, BcNum *restrict b,
                         BcNum *restrict c)
#define zbc_num_k(...) (zbc_num_k(__VA_ARGS__) COMMA_SUCCESS)
{
	BcStatus s;
	size_t max, max2;
	BcNum l1, h1, l2, h2, m2, m1, z0, z1, z2, temp;
	bool aone;

	if (a->len == 0 || b->len == 0) {
		bc_num_zero(c);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	aone = BC_NUM_ONE(a);
	if (aone || BC_NUM_ONE(b)) {
		bc_num_copy(c, aone ? b : a);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	if (a->len < BC_NUM_KARATSUBA_LEN
	 || b->len < BC_NUM_KARATSUBA_LEN
	/* || a->len + b->len < BC_NUM_KARATSUBA_LEN - redundant check */
	) {
		size_t i, j, len;

		bc_num_expand(c, a->len + b->len + 1);

		memset(c->num, 0, sizeof(BcDig) * c->cap);
		c->len = len = 0;

		for (i = 0; i < b->len; ++i) {
			unsigned carry = 0;
			for (j = 0; j < a->len; ++j) {
				unsigned in = c->num[i + j];
				in += (unsigned)a->num[j] * (unsigned)b->num[i] + carry;
				// note: compilers prefer _unsigned_ div/const
				carry = in / 10;
				c->num[i + j] = (BcDig)(in % 10);
			}

			c->num[i + j] += (BcDig) carry;
			len = BC_MAX(len, i + j + !!carry);

#if ENABLE_FEATURE_BC_INTERACTIVE
			// a=2^1000000
			// a*a <- without check below, this will not be interruptible
			if (G_interrupt) return BC_STATUS_FAILURE;
#endif
		}

		c->len = len;

		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	max = BC_MAX(a->len, b->len);
	bc_num_init(&l1, max);
	bc_num_init(&h1, max);
	bc_num_init(&l2, max);
	bc_num_init(&h2, max);
	bc_num_init(&m1, max);
	bc_num_init(&m2, max);
	bc_num_init(&z0, max);
	bc_num_init(&z1, max);
	bc_num_init(&z2, max);
	bc_num_init(&temp, max + max);

	max2 = (max + 1) / 2;
	bc_num_split(a, max2, &l1, &h1);
	bc_num_split(b, max2, &l2, &h2);

	s = zbc_num_add(&h1, &l1, &m1, 0);
	if (s) goto err;
	s = zbc_num_add(&h2, &l2, &m2, 0);
	if (s) goto err;

	s = zbc_num_k(&h1, &h2, &z0);
	if (s) goto err;
	s = zbc_num_k(&m1, &m2, &z1);
	if (s) goto err;
	s = zbc_num_k(&l1, &l2, &z2);
	if (s) goto err;

	s = zbc_num_sub(&z1, &z0, &temp, 0);
	if (s) goto err;
	s = zbc_num_sub(&temp, &z2, &z1, 0);
	if (s) goto err;

	s = zbc_num_shift(&z0, max2 * 2);
	if (s) goto err;
	s = zbc_num_shift(&z1, max2);
	if (s) goto err;
	s = zbc_num_add(&z0, &z1, &temp, 0);
	if (s) goto err;
	s = zbc_num_add(&temp, &z2, c, 0);
 err:
	bc_num_free(&temp);
	bc_num_free(&z2);
	bc_num_free(&z1);
	bc_num_free(&z0);
	bc_num_free(&m2);
	bc_num_free(&m1);
	bc_num_free(&h2);
	bc_num_free(&l2);
	bc_num_free(&h1);
	bc_num_free(&l1);
	RETURN_STATUS(s);
}

static FAST_FUNC BC_STATUS zbc_num_m(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s;
	BcNum cpa, cpb;
	size_t maxrdx = BC_MAX(a->rdx, b->rdx);

	scale = BC_MAX(scale, a->rdx);
	scale = BC_MAX(scale, b->rdx);
	scale = BC_MIN(a->rdx + b->rdx, scale);
	maxrdx = BC_MAX(maxrdx, scale);

	bc_num_init_and_copy(&cpa, a);
	bc_num_init_and_copy(&cpb, b);
	cpa.neg = cpb.neg = false;

	s = zbc_num_shift(&cpa, maxrdx);
	if (s) goto err;
	s = zbc_num_shift(&cpb, maxrdx);
	if (s) goto err;
	s = zbc_num_k(&cpa, &cpb, c);
	if (s) goto err;

	maxrdx += scale;
	bc_num_expand(c, c->len + maxrdx);

	if (c->len < maxrdx) {
		memset(c->num + c->len, 0, (c->cap - c->len) * sizeof(BcDig));
		c->len += maxrdx;
	}

	c->rdx = maxrdx;
	bc_num_retireMul(c, scale, a->neg, b->neg);
 err:
	bc_num_free(&cpb);
	bc_num_free(&cpa);
	RETURN_STATUS(s);
}
#define zbc_num_m(...) (zbc_num_m(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_d(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s;
	size_t len, end, i;
	BcNum cp;

	if (b->len == 0)
		RETURN_STATUS(bc_error("divide by zero"));
	if (a->len == 0) {
		bc_num_setToZero(c, scale);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (BC_NUM_ONE(b)) {
		bc_num_copy(c, a);
		bc_num_retireMul(c, scale, a->neg, b->neg);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	bc_num_init(&cp, BC_NUM_MREQ(a, b, scale));
	bc_num_copy(&cp, a);
	len = b->len;

	if (len > cp.len) {
		bc_num_expand(&cp, len + 2);
		bc_num_extend(&cp, len - cp.len);
	}

	if (b->rdx > cp.rdx) bc_num_extend(&cp, b->rdx - cp.rdx);
	cp.rdx -= b->rdx;
	if (scale > cp.rdx) bc_num_extend(&cp, scale - cp.rdx);

	if (b->rdx == b->len) {
		for (;;) {
			if (len == 0) break;
			len--;
			if (b->num[len] != 0)
				break;
		}
		len++;
	}

	if (cp.cap == cp.len) bc_num_expand(&cp, cp.len + 1);

	// We want an extra zero in front to make things simpler.
	cp.num[cp.len++] = 0;
	end = cp.len - len;

	bc_num_expand(c, cp.len);

	bc_num_zero(c);
	memset(c->num + end, 0, (c->cap - end) * sizeof(BcDig));
	c->rdx = cp.rdx;
	c->len = cp.len;

	s = BC_STATUS_SUCCESS;
	for (i = end - 1; i < end; --i) {
		BcDig *n, q;
		n = cp.num + i;
		for (q = 0; n[len] != 0 || bc_num_compare(n, b->num, len) >= 0; ++q)
			bc_num_subArrays(n, b->num, len);
		c->num[i] = q;
#if ENABLE_FEATURE_BC_INTERACTIVE
		// a=2^100000
		// scale=40000
		// 1/a <- without check below, this will not be interruptible
		if (G_interrupt) {
			s = BC_STATUS_FAILURE;
			break;
		}
#endif
	}

	bc_num_retireMul(c, scale, a->neg, b->neg);
	bc_num_free(&cp);

	RETURN_STATUS(s);
}
#define zbc_num_d(...) (zbc_num_d(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_r(BcNum *a, BcNum *b, BcNum *restrict c,
                         BcNum *restrict d, size_t scale, size_t ts)
{
	BcStatus s;
	BcNum temp;
	bool neg;

	if (b->len == 0)
		RETURN_STATUS(bc_error("divide by zero"));

	if (a->len == 0) {
		bc_num_setToZero(d, ts);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	bc_num_init(&temp, d->cap);
	s = zbc_num_d(a, b, c, scale);
	if (s) goto err;

	if (scale != 0) scale = ts;

	s = zbc_num_m(c, b, &temp, scale);
	if (s) goto err;
	s = zbc_num_sub(a, &temp, d, scale);
	if (s) goto err;

	if (ts > d->rdx && d->len) bc_num_extend(d, ts - d->rdx);

	neg = d->neg;
	bc_num_retireMul(d, ts, a->neg, b->neg);
	d->neg = neg;
 err:
	bc_num_free(&temp);
	RETURN_STATUS(s);
}
#define zbc_num_r(...) (zbc_num_r(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_rem(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s;
	BcNum c1;
	size_t ts = BC_MAX(scale + b->rdx, a->rdx), len = BC_NUM_MREQ(a, b, ts);

	bc_num_init(&c1, len);
	s = zbc_num_r(a, b, &c1, c, scale, ts);
	bc_num_free(&c1);

	RETURN_STATUS(s);
}
#define zbc_num_rem(...) (zbc_num_rem(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_p(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcNum copy;
	unsigned long pow;
	size_t i, powrdx, resrdx;
	size_t a_rdx;
	bool neg;

	// GNU bc does not allow 2^2.0 - we do
	for (i = 0; i < b->rdx; i++)
		if (b->num[i] != 0)
			RETURN_STATUS(bc_error("not an integer"));

// a^b for non-integer b (for a>0) can be implemented as exp(ln(a)*b).
// Possibly better precision would be given by a^int(b) * exp(ln(a)*frac(b)).

	if (b->len == 0) {
		bc_num_one(c);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (a->len == 0) {
		bc_num_setToZero(c, scale);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (BC_NUM_ONE(b)) {
		if (!b->neg)
			bc_num_copy(c, a);
		else
			s = zbc_num_inv(a, c, scale);
		RETURN_STATUS(s);
	}

	neg = b->neg;
	s = zbc_num_ulong_abs(b, &pow);
	if (s) RETURN_STATUS(s);
	// b is not used beyond this point

	bc_num_init_and_copy(&copy, a);
	a_rdx = a->rdx; // pull it into a CPU register (hopefully)
	// a is not used beyond this point

	if (!neg) {
		unsigned long new_scale;
		if (a_rdx > scale)
			scale = a_rdx;
		new_scale = a_rdx * pow;
		// Don't fall for multiplication overflow. Example:
		// 0.01^2147483648 a_rdx:2 pow:0x80000000, 32bit mul is 0.
//not that it matters with current algorithm, it would OOM on such large powers,
//but it can be improved to detect zero results etc. Example: with scale=0,
//result of 0.01^N for any N>1 is 0: 0.01^2 = 0.0001 ~= 0.00 (trunc to scale)
//then this would matter:
		// if a_rdx != 0 and new_scale < pow, we had overflow,
		// correct "new_scale" value is larger than ULONG_MAX,
		// thus larger than any possible current value of "scale",
		// thus "scale = new_scale" should not be done:
		if (a_rdx == 0 || new_scale >= pow)
			if (new_scale < scale)
				scale = new_scale;
	}

	for (powrdx = a_rdx; !(pow & 1); pow >>= 1) {
		powrdx <<= 1;
		s = zbc_num_mul(&copy, &copy, &copy, powrdx);
		if (s) goto err;
		// Not needed: zbc_num_mul() has a check for ^C:
		//if (G_interrupt) {
		//	s = BC_STATUS_FAILURE;
		//	goto err;
		//}
	}

	bc_num_copy(c, &copy);

	for (resrdx = powrdx, pow >>= 1; pow != 0; pow >>= 1) {
		powrdx <<= 1;
		s = zbc_num_mul(&copy, &copy, &copy, powrdx);
		if (s) goto err;

		if (pow & 1) {
			resrdx += powrdx;
			s = zbc_num_mul(c, &copy, c, resrdx);
			if (s) goto err;
		}
		// Not needed: zbc_num_mul() has a check for ^C:
		//if (G_interrupt) {
		//	s = BC_STATUS_FAILURE;
		//	goto err;
		//}
	}

	if (neg) {
		s = zbc_num_inv(c, c, scale);
		if (s) goto err;
	}

	if (c->rdx > scale) bc_num_truncate(c, c->rdx - scale);

	// We can't use bc_num_clean() here.
	for (i = 0; i < c->len; ++i)
		if (c->num[i] != 0)
			goto skip;
	bc_num_setToZero(c, scale);
 skip:

 err:
	bc_num_free(&copy);
	RETURN_STATUS(s);
}
#define zbc_num_p(...) (zbc_num_p(__VA_ARGS__) COMMA_SUCCESS)

static NOINLINE BC_STATUS zbc_num_sqrt(BcNum *a, BcNum *restrict b, size_t scale)
{
	BcStatus s;
	BcNum num1, num2, half, f, fprime, *x0, *x1, *temp;
	BcDig half_digs[1];
	size_t pow, len, digs, digs1, resrdx, req, times;
	ssize_t cmp, cmp1, cmp2;

	req = BC_MAX(scale, a->rdx) + ((BC_NUM_INT(a) + 1) >> 1) + 1;
	bc_num_expand(b, req);

	if (a->len == 0) {
		bc_num_setToZero(b, scale);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (a->neg) {
		RETURN_STATUS(bc_error("negative number"));
	}
	if (BC_NUM_ONE(a)) {
		bc_num_one(b);
		bc_num_extend(b, scale);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	scale = BC_MAX(scale, a->rdx) + 1;
	len = a->len + scale;

	bc_num_init(&num1, len);
	bc_num_init(&num2, len);

	half.cap = ARRAY_SIZE(half_digs);
	half.num = half_digs;
	bc_num_one(&half);
	half_digs[0] = 5;
	half.rdx = 1;

	bc_num_init(&f, len);
	bc_num_init(&fprime, len);

	x0 = &num1;
	x1 = &num2;

	bc_num_one(x0);
	pow = BC_NUM_INT(a);

	if (pow) {
		if (pow & 1)
			x0->num[0] = 2;
		else
			x0->num[0] = 6;

		pow -= 2 - (pow & 1);

		bc_num_extend(x0, pow);

		// Make sure to move the radix back.
		x0->rdx -= pow;
	}

	x0->rdx = digs = digs1 = times = 0;
	resrdx = scale + 2;
	len = x0->len + resrdx - 1;
	cmp = 1;
	cmp1 = cmp2 = SSIZE_MAX;
	do {
		s = zbc_num_div(a, x0, &f, resrdx);
		if (s) goto err;
		s = zbc_num_add(x0, &f, &fprime, resrdx);
		if (s) goto err;
		s = zbc_num_mul(&fprime, &half, x1, resrdx);
		if (s) goto err;

		cmp = bc_num_cmp(x1, x0);
		digs = x1->len - (unsigned long long) llabs(cmp);

		if (cmp == cmp2 && digs == digs1)
			times += 1;
		else
			times = 0;

		resrdx += times > 4;

		cmp2 = cmp1;
		cmp1 = cmp;
		digs1 = digs;

		temp = x0;
		x0 = x1;
		x1 = temp;
	} while (cmp != 0 || digs < len);

	bc_num_copy(b, x0);
	scale -= 1;
	if (b->rdx > scale)
		bc_num_truncate(b, b->rdx - scale);
 err:
	bc_num_free(&fprime);
	bc_num_free(&f);
	bc_num_free(&num2);
	bc_num_free(&num1);
	RETURN_STATUS(s);
}
#define zbc_num_sqrt(...) (zbc_num_sqrt(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_num_divmod(BcNum *a, BcNum *b, BcNum *c, BcNum *d,
                              size_t scale)
{
	BcStatus s;
	BcNum num2, *ptr_a;
	bool init = false;
	size_t ts = BC_MAX(scale + b->rdx, a->rdx), len = BC_NUM_MREQ(a, b, ts);

	if (c == a) {
		memcpy(&num2, c, sizeof(BcNum));
		ptr_a = &num2;
		bc_num_init(c, len);
		init = true;
	} else {
		ptr_a = a;
		bc_num_expand(c, len);
	}

	s = zbc_num_r(ptr_a, b, c, d, scale, ts);

	if (init) bc_num_free(&num2);

	RETURN_STATUS(s);
}
#define zbc_num_divmod(...) (zbc_num_divmod(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_DC
static BC_STATUS zdc_num_modexp(BcNum *a, BcNum *b, BcNum *c, BcNum *restrict d)
{
	BcStatus s;
	BcNum base, exp, two, temp;
	BcDig two_digs[1];

	if (c->len == 0)
		RETURN_STATUS(bc_error("divide by zero"));
	if (a->rdx || b->rdx || c->rdx)
		RETURN_STATUS(bc_error("not an integer"));
	if (b->neg)
		RETURN_STATUS(bc_error("negative number"));

	bc_num_expand(d, c->len);
	bc_num_init(&base, c->len);
	bc_num_init(&exp, b->len);
	bc_num_init(&temp, b->len);

	two.cap = ARRAY_SIZE(two_digs);
	two.num = two_digs;
	bc_num_one(&two);
	two_digs[0] = 2;

	bc_num_one(d);

	s = zbc_num_rem(a, c, &base, 0);
	if (s) goto err;
	bc_num_copy(&exp, b);

	while (exp.len != 0) {
		s = zbc_num_divmod(&exp, &two, &exp, &temp, 0);
		if (s) goto err;

		if (BC_NUM_ONE(&temp)) {
			s = zbc_num_mul(d, &base, &temp, 0);
			if (s) goto err;
			s = zbc_num_rem(&temp, c, d, 0);
			if (s) goto err;
		}

		s = zbc_num_mul(&base, &base, &temp, 0);
		if (s) goto err;
		s = zbc_num_rem(&temp, c, &base, 0);
		if (s) goto err;
	}
 err:
	bc_num_free(&temp);
	bc_num_free(&exp);
	bc_num_free(&base);
	RETURN_STATUS(s);
}
#define zdc_num_modexp(...) (zdc_num_modexp(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_DC

static FAST_FUNC void bc_string_free(void *string)
{
	free(*(char**)string);
}

static void bc_func_init(BcFunc *f)
{
	bc_char_vec_init(&f->code);
	IF_BC(bc_vec_init(&f->labels, sizeof(size_t), NULL);)
	IF_BC(bc_vec_init(&f->autos, sizeof(BcId), bc_id_free);)
	IF_BC(bc_vec_init(&f->strs, sizeof(char *), bc_string_free);)
	IF_BC(bc_vec_init(&f->consts, sizeof(char *), bc_string_free);)
	IF_BC(f->nparams = 0;)
}

static FAST_FUNC void bc_func_free(void *func)
{
	BcFunc *f = (BcFunc *) func;
	bc_vec_free(&f->code);
	IF_BC(bc_vec_free(&f->labels);)
	IF_BC(bc_vec_free(&f->autos);)
	IF_BC(bc_vec_free(&f->strs);)
	IF_BC(bc_vec_free(&f->consts);)
}

static void bc_array_expand(BcVec *a, size_t len);

static void bc_array_init(BcVec *a, bool nums)
{
	if (nums)
		bc_vec_init(a, sizeof(BcNum), bc_num_free);
	else
		bc_vec_init(a, sizeof(BcVec), bc_vec_free);
	bc_array_expand(a, 1);
}

static void bc_array_expand(BcVec *a, size_t len)
{
	if (a->dtor == bc_num_free
	 // && a->size == sizeof(BcNum) - always true
	) {
		BcNum n;
		while (len > a->len) {
			bc_num_init_DEF_SIZE(&n);
			bc_vec_push(a, &n);
		}
	} else {
		BcVec v;
		while (len > a->len) {
			bc_array_init(&v, true);
			bc_vec_push(a, &v);
		}
	}
}

static void bc_array_copy(BcVec *d, const BcVec *s)
{
	BcNum *dnum, *snum;
	size_t i;

	bc_vec_pop_all(d);
	bc_vec_expand(d, s->cap);
	d->len = s->len;

	dnum = (void*)d->v;
	snum = (void*)s->v;
	for (i = 0; i < s->len; i++, dnum++, snum++) {
		bc_num_init_and_copy(dnum, snum);
	}
}

#if ENABLE_DC
static void dc_result_copy(BcResult *d, BcResult *src)
{
	d->t = src->t;

	switch (d->t) {
		case XC_RESULT_TEMP:
		case XC_RESULT_IBASE:
		case XC_RESULT_SCALE:
		case XC_RESULT_OBASE:
			bc_num_init_and_copy(&d->d.n, &src->d.n);
			break;
		case XC_RESULT_VAR:
		case XC_RESULT_ARRAY:
		case XC_RESULT_ARRAY_ELEM:
			d->d.id.name = xstrdup(src->d.id.name);
			break;
		case XC_RESULT_CONSTANT:
		case XC_RESULT_STR:
			memcpy(&d->d.n, &src->d.n, sizeof(BcNum));
			break;
		default: // placate compiler
			// BC_RESULT_VOID, BC_RESULT_LAST, BC_RESULT_ONE - do not happen
			break;
	}
}
#endif // ENABLE_DC

static FAST_FUNC void bc_result_free(void *result)
{
	BcResult *r = (BcResult *) result;

	switch (r->t) {
		case XC_RESULT_TEMP:
		IF_BC(case BC_RESULT_VOID:)
		case XC_RESULT_IBASE:
		case XC_RESULT_SCALE:
		case XC_RESULT_OBASE:
			bc_num_free(&r->d.n);
			break;
		case XC_RESULT_VAR:
		case XC_RESULT_ARRAY:
		case XC_RESULT_ARRAY_ELEM:
			free(r->d.id.name);
			break;
		default:
			// Do nothing.
			break;
	}
}

static int bad_input_byte(char c)
{
	if ((c < ' ' && c != '\t' && c != '\r' && c != '\n') // also allow '\v' '\f'?
	 || c > 0x7e
	) {
		bc_error_fmt("illegal character 0x%02x", c);
		return 1;
	}
	return 0;
}

static void xc_read_line(BcVec *vec, FILE *fp)
{
 again:
	bc_vec_pop_all(vec);
	fflush_and_check();

#if ENABLE_FEATURE_BC_INTERACTIVE
	if (G_interrupt) { // ^C was pressed
		if (fp != stdin) {
			// ^C while running a script (bc SCRIPT): die.
			// We do not return to interactive prompt:
			// user might be running us from a shell,
			// and SCRIPT might be intended to terminate
			// (e.g. contain a "halt" stmt).
			// ^C dropping user into a bc prompt instead of
			// the shell would be unexpected.
			xfunc_die();
		}
		// There was ^C while running calculations
		G_interrupt = 0;
		// GNU bc says "interrupted execution." (to stdout, not stderr)
		// GNU dc says "Interrupt!"
		puts("\ninterrupted execution");
	}

# if ENABLE_FEATURE_EDITING
	if (G_ttyin && fp == stdin) {
		int n, i;
		if (!G.line_input_state)
			G.line_input_state = new_line_input_t(DO_HISTORY);
#  define line_buf bb_common_bufsiz1
		n = read_line_input(G.line_input_state, "", line_buf, COMMON_BUFSIZE);
		if (n <= 0) { // read errors or EOF, or ^D, or ^C
			//GNU bc prints this on ^C:
			//if (n == 0) // ^C
			//	puts("(interrupt) Exiting bc.");
			bc_vec_pushZeroByte(vec);
			return;
		}
		i = 0;
		for (;;) {
			char c = line_buf[i++];
			if (c == '\0') break;
			if (bad_input_byte(c)) goto again;
		}
		bc_vec_string(vec, n, line_buf);
#  undef line_buf
	} else
# endif
#endif
	{
		int c;
		bool bad_chars = 0;

		do {
 get_char:
#if ENABLE_FEATURE_BC_INTERACTIVE
			if (G_interrupt) {
				// ^C was pressed: ignore entire line, get another one
				goto again;
			}
#endif
			c = fgetc(fp);
			if (c == '\0')
				goto get_char;
			if (c == EOF) {
				if (ferror(fp))
					bb_simple_perror_msg_and_die("input error");
				// Note: EOF does not append '\n'
				break;
			}
			bad_chars |= bad_input_byte(c);
			bc_vec_pushByte(vec, (char)c);
		} while (c != '\n');

		if (bad_chars) {
			// Bad chars on this line
			if (!G.prs.lex_filename) { // stdin
				// ignore entire line, get another one
				goto again;
			}
			bb_perror_msg_and_die("file '%s' is not text", G.prs.lex_filename);
		}
		bc_vec_pushZeroByte(vec);
	}
}

//
// Parsing routines
//

// "Input numbers may contain the characters 0-9 and A-Z.
// (Note: They must be capitals.  Lower case letters are variable names.)
// Single digit numbers always have the value of the digit regardless of
// the value of ibase. (i.e. A = 10.) For multi-digit numbers, bc changes
// all input digits greater or equal to ibase to the value of ibase-1.
// This makes the number ZZZ always be the largest 3 digit number of the
// input base."
static bool xc_num_strValid(const char *val)
{
	bool radix = false;
	for (;;) {
		BcDig c = *val++;
		if (c == '\0')
			break;
		if (c == '.') {
			if (radix) return false;
			radix = true;
			continue;
		}
		if ((c < '0' || c > '9') && (c < 'A' || c > 'Z'))
			return false;
	}
	return true;
}

// Note: n is already "bc_num_zero()"ed,
// leading zeroes in "val" are removed
static void bc_num_parseDecimal(BcNum *n, const char *val)
{
	size_t len, i;
	const char *ptr;

	len = strlen(val);
	if (len == 0)
		return;

	bc_num_expand(n, len + 1); // +1 for e.g. "A" converting into 10

	ptr = strchr(val, '.');

	n->rdx = 0;
	if (ptr != NULL)
		n->rdx = (size_t)((val + len) - (ptr + 1));

	for (i = 0; val[i]; ++i) {
		if (val[i] != '0' && val[i] != '.') {
			// Not entirely zero value - convert it, and exit
			if (len == 1) {
				unsigned c = val[0] - '0';
				n->len = 1;
				if (c > 9) { // A-Z => 10-36
					n->len = 2;
					c -= ('A' - '9' - 1);
					n->num[1] = c/10;
					c = c%10;
				}
				n->num[0] = c;
				break;
			}
			i = len - 1;
			for (;;) {
				char c = val[i] - '0';
				if (c > 9) // A-Z => 9
					c = 9;
				n->num[n->len] = c;
				n->len++;
 skip_dot:
				if (i == 0) break;
				if (val[--i] == '.') goto skip_dot;
			}
			break;
		}
	}
	// if for() exits without hitting if(), the value is entirely zero
}

// Note: n is already "bc_num_zero()"ed,
// leading zeroes in "val" are removed
static void bc_num_parseBase(BcNum *n, const char *val, unsigned base_t)
{
	BcStatus s;
	BcNum mult, result;
	BcNum temp;
	BcNum base;
	BcDig temp_digs[ULONG_NUM_BUFSIZE];
	BcDig base_digs[ULONG_NUM_BUFSIZE];
	size_t digits;

	bc_num_init_DEF_SIZE(&mult);

	temp.cap = ARRAY_SIZE(temp_digs);
	temp.num = temp_digs;

	base.cap = ARRAY_SIZE(base_digs);
	base.num = base_digs;
	bc_num_ulong2num(&base, base_t);
	base_t--;

	for (;;) {
		unsigned v;
		char c;

		c = *val++;
		if (c == '\0') goto int_err;
		if (c == '.') break;

		v = (unsigned)(c <= '9' ? c - '0' : c - 'A' + 10);
		if (v > base_t) v = base_t;

		s = zbc_num_mul(n, &base, &mult, 0);
		if (s) goto int_err;
		bc_num_ulong2num(&temp, v);
		s = zbc_num_add(&mult, &temp, n, 0);
		if (s) goto int_err;
	}

	bc_num_init(&result, base.len);
	//bc_num_zero(&result); - already is
	bc_num_one(&mult);

	digits = 0;
	for (;;) {
		unsigned v;
		char c;

		c = *val++;
		if (c == '\0') break;
		digits++;

		v = (unsigned)(c <= '9' ? c - '0' : c - 'A' + 10);
		if (v > base_t) v = base_t;

		s = zbc_num_mul(&result, &base, &result, 0);
		if (s) goto err;
		bc_num_ulong2num(&temp, v);
		s = zbc_num_add(&result, &temp, &result, 0);
		if (s) goto err;
		s = zbc_num_mul(&mult, &base, &mult, 0);
		if (s) goto err;
	}

	s = zbc_num_div(&result, &mult, &result, digits);
	if (s) goto err;
	s = zbc_num_add(n, &result, n, digits);
	if (s) goto err;

	if (n->len != 0) {
		if (n->rdx < digits)
			bc_num_extend(n, digits - n->rdx);
	} else
		bc_num_zero(n);
 err:
	bc_num_free(&result);
 int_err:
	bc_num_free(&mult);
}

static BC_STATUS zxc_num_parse(BcNum *n, const char *val, unsigned base_t)
{
	size_t i;

	if (!xc_num_strValid(val))
		RETURN_STATUS(bc_error("bad number string"));

	bc_num_zero(n);
	while (*val == '0')
		val++;
	for (i = 0; ; ++i) {
		if (val[i] == '\0')
			RETURN_STATUS(BC_STATUS_SUCCESS);
		if (val[i] != '.' && val[i] != '0')
			break;
	}

	if (base_t == 10 || val[1] == '\0')
		// Decimal, or single-digit number
		bc_num_parseDecimal(n, val);
	else
		bc_num_parseBase(n, val, base_t);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zxc_num_parse(...) (zxc_num_parse(__VA_ARGS__) COMMA_SUCCESS)

// p->lex_inbuf points to the current string to be parsed.
// if p->lex_inbuf points to '\0', it's either EOF or it points after
// last processed line's terminating '\n' (and more reading needs to be done
// to get next character).
//
// If you are in a situation where that is a possibility, call peek_inbuf().
// If necessary, it performs more reading and changes p->lex_inbuf,
// then it returns *p->lex_inbuf (which will be '\0' only if it's EOF).
// After it, just referencing *p->lex_inbuf is valid, and if it wasn't '\0',
// it's ok to do p->lex_inbuf++ once without end-of-buffer checking.
//
// eat_inbuf() is equvalent to "peek_inbuf(); if (c) p->lex_inbuf++":
// it returns current char and advances the pointer (if not EOF).
// After eat_inbuf(), referencing p->lex_inbuf[-1] and *p->lex_inbuf is valid.
//
// In many cases, you can use fast *p->lex_inbuf instead of peek_inbuf():
// unless prev char might have been '\n', *p->lex_inbuf is '\0' ONLY
// on real EOF, not end-of-buffer.
//
// bc cases to test interactively:
// 1 #comment\  - prints "1<newline>" at once (comment is not continued)
// 1 #comment/* - prints "1<newline>" at once
// 1 #comment"  - prints "1<newline>" at once
// 1\#comment   - error at once (\ is not a line continuation)
// 1 + /*"*/2   - prints "3<newline>" at once
// 1 + /*#*/2   - prints "3<newline>" at once
// "str\"       - prints "str\" at once
// "str#"       - prints "str#" at once
// "str/*"      - prints "str/*" at once
// "str#\       - waits for second line
// end"         - ...prints "str#\<newline>end"
static char peek_inbuf(void)
{
	if (*G.prs.lex_inbuf == '\0'
	 && G.prs.lex_input_fp
	) {
		xc_read_line(&G.input_buffer, G.prs.lex_input_fp);
		G.prs.lex_inbuf = G.input_buffer.v;
		// lex_next_at may point to now-freed data, update it:
		G.prs.lex_next_at = G.prs.lex_inbuf;
		if (G.input_buffer.len <= 1) // on EOF, len is 1 (NUL byte)
			G.prs.lex_input_fp = NULL;
	}
	return *G.prs.lex_inbuf;
}
static char eat_inbuf(void)
{
	char c = peek_inbuf();
	if (c) G.prs.lex_inbuf++;
	return c;
}

static void xc_lex_lineComment(void)
{
	BcParse *p = &G.prs;
	char c;

	// Try: echo -n '#foo' | bc
	p->lex = XC_LEX_WHITESPACE;

	// Not peek_inbuf(): we depend on input being done in whole lines:
	// '\0' which isn't the EOF can only be seen after '\n'.
	while ((c = *p->lex_inbuf) != '\n' && c != '\0')
		p->lex_inbuf++;
}

static void xc_lex_whitespace(void)
{
	BcParse *p = &G.prs;

	p->lex = XC_LEX_WHITESPACE;
	for (;;) {
		// We depend here on input being done in whole lines:
		// '\0' which isn't the EOF can only be seen after '\n'.
		char c = *p->lex_inbuf;
		if (c == '\n') // this is XC_LEX_NLINE, not XC_LEX_WHITESPACE
			break;
		if (!isspace(c))
			break;
		p->lex_inbuf++;
	}
}

static BC_STATUS zxc_lex_number(char last)
{
	BcParse *p = &G.prs;
	bool pt;
	char last_valid_ch;

	bc_vec_pop_all(&p->lex_strnumbuf);
	bc_vec_pushByte(&p->lex_strnumbuf, last);

// bc: "Input numbers may contain the characters 0-9 and A-Z.
// (Note: They must be capitals.  Lower case letters are variable names.)
// Single digit numbers always have the value of the digit regardless of
// the value of ibase. (i.e. A = 10.) For multi-digit numbers, bc changes
// all input digits greater or equal to ibase to the value of ibase-1.
// This makes the number ZZZ always be the largest 3 digit number of the
// input base."
// dc only allows A-F, the rules about single-char and multi-char are the same.
	last_valid_ch = (IS_BC ? 'Z' : 'F');
	pt = (last == '.');
	p->lex = XC_LEX_NUMBER;
	for (;;) {
		// We depend here on input being done in whole lines:
		// '\0' which isn't the EOF can only be seen after '\n'.
		char c = *p->lex_inbuf;
 check_c:
		if (c == '\0')
			break;
		if (c == '\\' && p->lex_inbuf[1] == '\n') {
			p->lex_inbuf += 2;
			p->lex_line++;
			dbg_lex("++p->lex_line=%zd", p->lex_line);
			c = peek_inbuf(); // force next line to be read
			goto check_c;
		}
		if (!isdigit(c) && (c < 'A' || c > last_valid_ch)) {
			if (c != '.') break;
			// if '.' was already seen, stop on second one:
			if (pt) break;
			pt = true;
		}
		// c is one of "0-9A-Z."
		last = c;
		bc_vec_push(&p->lex_strnumbuf, p->lex_inbuf);
		p->lex_inbuf++;
	}
	if (last == '.') // remove trailing '.' if any
		bc_vec_pop(&p->lex_strnumbuf);
	bc_vec_pushZeroByte(&p->lex_strnumbuf);

	G.err_line = G.prs.lex_line;
	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zxc_lex_number(...) (zxc_lex_number(__VA_ARGS__) COMMA_SUCCESS)

static void xc_lex_name(void)
{
	BcParse *p = &G.prs;
	size_t i;
	const char *buf;

	p->lex = XC_LEX_NAME;

	// Since names can't cross lines with \<newline>,
	// we depend on the fact that whole line is in the buffer
	i = 0;
	buf = p->lex_inbuf - 1;
	for (;;) {
		char c = buf[i];
		if ((c < 'a' || c > 'z') && !isdigit(c) && c != '_') break;
		i++;
	}

#if 0 // We do not protect against people with gigabyte-long names
	// This check makes sense only if size_t is (much) larger than BC_MAX_STRING.
	if (SIZE_MAX > (BC_MAX_STRING | 0xff)) {
		if (i > BC_MAX_STRING)
			return bc_error("name too long: must be [1,"BC_MAX_STRING_STR"]");
	}
#endif
	bc_vec_string(&p->lex_strnumbuf, i, buf);

	// Increment the index. We minus 1 because it has already been incremented.
	p->lex_inbuf += i - 1;

	//return BC_STATUS_SUCCESS;
}

IF_BC(static BC_STATUS zbc_lex_token(void);)
IF_DC(static BC_STATUS zdc_lex_token(void);)
#define zbc_lex_token(...) (zbc_lex_token(__VA_ARGS__) COMMA_SUCCESS)
#define zdc_lex_token(...) (zdc_lex_token(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_lex_next(void)
{
	BcParse *p = &G.prs;
	BcStatus s;

	G.err_line = p->lex_line;
	p->lex_last = p->lex;
//why?
//	if (p->lex_last == XC_LEX_EOF)
//		RETURN_STATUS(bc_error("end of file"));

	// Loop until failure or we don't have whitespace. This
	// is so the parser doesn't get inundated with whitespace.
	// Comments are also XC_LEX_WHITESPACE tokens and eaten here.
	s = BC_STATUS_SUCCESS;
	do {
		if (*p->lex_inbuf == '\0') {
			p->lex = XC_LEX_EOF;
			if (peek_inbuf() == '\0')
				RETURN_STATUS(BC_STATUS_SUCCESS);
		}
		p->lex_next_at = p->lex_inbuf;
		dbg_lex("next string to parse:'%.*s'",
			(int)(strchrnul(p->lex_next_at, '\n') - p->lex_next_at),
			p->lex_next_at
		);
		if (IS_BC) {
			IF_BC(s = zbc_lex_token());
		} else {
			IF_DC(s = zdc_lex_token());
		}
	} while (!s && p->lex == XC_LEX_WHITESPACE);
	dbg_lex("p->lex from string:%d", p->lex);

	RETURN_STATUS(s);
}
#define zxc_lex_next(...) (zxc_lex_next(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_BC
static BC_STATUS zbc_lex_skip_if_at_NLINE(void)
{
	if (G.prs.lex == XC_LEX_NLINE)
		RETURN_STATUS(zxc_lex_next());
	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_lex_skip_if_at_NLINE(...) (zbc_lex_skip_if_at_NLINE(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_lex_next_and_skip_NLINE(void)
{
	BcStatus s;
	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);
	// if(cond)<newline>stmt is accepted too (but not 2+ newlines)
	s = zbc_lex_skip_if_at_NLINE();
	RETURN_STATUS(s);
}
#define zbc_lex_next_and_skip_NLINE(...) (zbc_lex_next_and_skip_NLINE(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_lex_identifier(void)
{
	BcParse *p = &G.prs;
	BcStatus s;
	unsigned i;
	const char *buf = p->lex_inbuf - 1;

	for (i = 0; i < ARRAY_SIZE(bc_lex_kws); ++i) {
		const char *keyword8 = bc_lex_kws[i].name8;
		unsigned j = 0;
		while (buf[j] != '\0' && buf[j] == keyword8[j]) {
			j++;
			if (j == 8) goto match;
		}
		if (keyword8[j] != '\0')
			continue;
 match:
		// buf starts with keyword bc_lex_kws[i]
		if (isalnum(buf[j]) || buf[j] == '_')
			continue; // "ifz" does not match "if" keyword, "if." does
		p->lex = BC_LEX_KEY_1st_keyword + i;
		if (!keyword_is_POSIX(i)) {
			s = zbc_posix_error_fmt("%sthe '%.8s' keyword", "POSIX does not allow ", bc_lex_kws[i].name8);
			if (s) RETURN_STATUS(s);
		}

		// We minus 1 because the index has already been incremented.
		p->lex_inbuf += j - 1;
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	xc_lex_name();
	s = BC_STATUS_SUCCESS;

	if (p->lex_strnumbuf.len > 2) {
		// Prevent this:
		// >>> qwe=1
		// bc: POSIX only allows one character names; this is bad: 'qwe=1
		// '
		unsigned len = strchrnul(buf, '\n') - buf;
		s = zbc_posix_error_fmt("POSIX only allows one character names; this is bad: '%.*s'", len, buf);
	}

	RETURN_STATUS(s);
}
#define zbc_lex_identifier(...) (zbc_lex_identifier(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_lex_string(void)
{
	BcParse *p = &G.prs;

	p->lex = XC_LEX_STR;
	bc_vec_pop_all(&p->lex_strnumbuf);
	for (;;) {
		char c = peek_inbuf(); // strings can cross lines
		if (c == '\0') {
			RETURN_STATUS(bc_error("unterminated string"));
		}
		if (c == '"')
			break;
		if (c == '\n') {
			p->lex_line++;
			dbg_lex("++p->lex_line=%zd", p->lex_line);
		}
		bc_vec_push(&p->lex_strnumbuf, p->lex_inbuf);
		p->lex_inbuf++;
	}
	bc_vec_pushZeroByte(&p->lex_strnumbuf);
	p->lex_inbuf++;

	G.err_line = p->lex_line;
	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_lex_string(...) (zbc_lex_string(__VA_ARGS__) COMMA_SUCCESS)

static void parse_lex_by_checking_eq_sign(unsigned with_and_without)
{
	BcParse *p = &G.prs;
	if (*p->lex_inbuf == '=') {
		// ^^^ not using peek_inbuf() since '==' etc can't be split across lines
		p->lex_inbuf++;
		with_and_without >>= 8; // store "with" value
	} // else store "without" value
	p->lex = (with_and_without & 0xff);
}
#define parse_lex_by_checking_eq_sign(with, without) \
	parse_lex_by_checking_eq_sign(((with)<<8)|(without))

static BC_STATUS zbc_lex_comment(void)
{
	BcParse *p = &G.prs;

	p->lex = XC_LEX_WHITESPACE;
	// here lex_inbuf is at '*' of opening comment delimiter
	for (;;) {
		char c;

		p->lex_inbuf++;
		c = peek_inbuf();
 check_star:
		if (c == '*') {
			p->lex_inbuf++;
			c = *p->lex_inbuf; // no need to peek_inbuf()
			if (c == '/')
				break;
			goto check_star;
		}
		if (c == '\0') {
			RETURN_STATUS(bc_error("unterminated comment"));
		}
		if (c == '\n') {
			p->lex_line++;
			dbg_lex("++p->lex_line=%zd", p->lex_line);
		}
	}
	p->lex_inbuf++; // skip trailing '/'

	G.err_line = p->lex_line;
	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_lex_comment(...) (zbc_lex_comment(__VA_ARGS__) COMMA_SUCCESS)

#undef zbc_lex_token
static BC_STATUS zbc_lex_token(void)
{
	BcParse *p = &G.prs;
	BcStatus s = BC_STATUS_SUCCESS;
	char c = eat_inbuf();
	char c2;

	// This is the workhorse of the lexer.
	switch (c) {
//	case '\0': // probably never reached
//		p->lex_inbuf--;
//		p->lex = XC_LEX_EOF;
//		break;
	case '\n':
		p->lex_line++;
		dbg_lex("++p->lex_line=%zd", p->lex_line);
		p->lex = XC_LEX_NLINE;
		break;
	case '\t':
	case '\v':
	case '\f':
	case '\r':
	case ' ':
		xc_lex_whitespace();
		break;
	case '!':
		parse_lex_by_checking_eq_sign(XC_LEX_OP_REL_NE, BC_LEX_OP_BOOL_NOT);
		if (p->lex == BC_LEX_OP_BOOL_NOT) {
			s = zbc_POSIX_does_not_allow_bool_ops_this_is_bad("!");
			if (s) RETURN_STATUS(s);
		}
		break;
	case '"':
		s = zbc_lex_string();
		break;
	case '#':
		s = zbc_POSIX_does_not_allow("'#' script comments");
		if (s) RETURN_STATUS(s);
		xc_lex_lineComment();
		break;
	case '%':
		parse_lex_by_checking_eq_sign(BC_LEX_OP_ASSIGN_MODULUS, XC_LEX_OP_MODULUS);
		break;
	case '&':
		c2 = *p->lex_inbuf;
		if (c2 == '&') {
			s = zbc_POSIX_does_not_allow_bool_ops_this_is_bad("&&");
			if (s) RETURN_STATUS(s);
			p->lex_inbuf++;
			p->lex = BC_LEX_OP_BOOL_AND;
		} else {
			p->lex = XC_LEX_INVALID;
			s = bc_error_bad_character('&');
		}
		break;
	case '(':
	case ')':
		p->lex = (BcLexType)(c - '(' + BC_LEX_LPAREN);
		break;
	case '*':
		parse_lex_by_checking_eq_sign(BC_LEX_OP_ASSIGN_MULTIPLY, XC_LEX_OP_MULTIPLY);
		break;
	case '+':
		c2 = *p->lex_inbuf;
		if (c2 == '+') {
			p->lex_inbuf++;
			p->lex = BC_LEX_OP_INC;
		} else
			parse_lex_by_checking_eq_sign(BC_LEX_OP_ASSIGN_PLUS, XC_LEX_OP_PLUS);
		break;
	case ',':
		p->lex = BC_LEX_COMMA;
		break;
	case '-':
		c2 = *p->lex_inbuf;
		if (c2 == '-') {
			p->lex_inbuf++;
			p->lex = BC_LEX_OP_DEC;
		} else
			parse_lex_by_checking_eq_sign(BC_LEX_OP_ASSIGN_MINUS, XC_LEX_OP_MINUS);
		break;
	case '.':
		if (isdigit(*p->lex_inbuf))
			s = zxc_lex_number(c);
		else {
			p->lex = BC_LEX_KEY_LAST;
			s = zbc_POSIX_does_not_allow("'.' as 'last'");
		}
		break;
	case '/':
		c2 = *p->lex_inbuf;
		if (c2 == '*')
			s = zbc_lex_comment();
		else
			parse_lex_by_checking_eq_sign(BC_LEX_OP_ASSIGN_DIVIDE, XC_LEX_OP_DIVIDE);
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	case 'G':
	case 'H':
	case 'I':
	case 'J':
	case 'K':
	case 'L':
	case 'M':
	case 'N':
	case 'O':
	case 'P':
	case 'Q':
	case 'R':
	case 'S':
	case 'T':
	case 'U':
	case 'V':
	case 'W':
	case 'X':
	case 'Y':
	case 'Z':
		s = zxc_lex_number(c);
		break;
	case ';':
		p->lex = BC_LEX_SCOLON;
		break;
	case '<':
		parse_lex_by_checking_eq_sign(XC_LEX_OP_REL_LE, XC_LEX_OP_REL_LT);
		break;
	case '=':
		parse_lex_by_checking_eq_sign(XC_LEX_OP_REL_EQ, BC_LEX_OP_ASSIGN);
		break;
	case '>':
		parse_lex_by_checking_eq_sign(XC_LEX_OP_REL_GE, XC_LEX_OP_REL_GT);
		break;
	case '[':
	case ']':
		p->lex = (BcLexType)(c - '[' + BC_LEX_LBRACKET);
		break;
	case '\\':
		if (*p->lex_inbuf == '\n') {
			p->lex = XC_LEX_WHITESPACE;
			p->lex_inbuf++;
		} else
			s = bc_error_bad_character(c);
		break;
	case '^':
		parse_lex_by_checking_eq_sign(BC_LEX_OP_ASSIGN_POWER, XC_LEX_OP_POWER);
		break;
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	case 'g':
	case 'h':
	case 'i':
	case 'j':
	case 'k':
	case 'l':
	case 'm':
	case 'n':
	case 'o':
	case 'p':
	case 'q':
	case 'r':
	case 's':
	case 't':
	case 'u':
	case 'v':
	case 'w':
	case 'x':
	case 'y':
	case 'z':
		s = zbc_lex_identifier();
		break;
	case '{':
	case '}':
		p->lex = (BcLexType)(c - '{' + BC_LEX_LBRACE);
		break;
	case '|':
		c2 = *p->lex_inbuf;
		if (c2 == '|') {
			s = zbc_POSIX_does_not_allow_bool_ops_this_is_bad("||");
			if (s) RETURN_STATUS(s);
			p->lex_inbuf++;
			p->lex = BC_LEX_OP_BOOL_OR;
		} else {
			p->lex = XC_LEX_INVALID;
			s = bc_error_bad_character(c);
		}
		break;
	default:
		p->lex = XC_LEX_INVALID;
		s = bc_error_bad_character(c);
		break;
	}

	RETURN_STATUS(s);
}
#define zbc_lex_token(...) (zbc_lex_token(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_BC

#if ENABLE_DC
static BC_STATUS zdc_lex_register(void)
{
	BcParse *p = &G.prs;
	if (G_exreg && isspace(*p->lex_inbuf)) {
		xc_lex_whitespace(); // eats whitespace (but not newline)
		p->lex_inbuf++; // xc_lex_name() expects this
		xc_lex_name();
	} else {
		bc_vec_pop_all(&p->lex_strnumbuf);
		bc_vec_push(&p->lex_strnumbuf, p->lex_inbuf++);
		bc_vec_pushZeroByte(&p->lex_strnumbuf);
		p->lex = XC_LEX_NAME;
	}

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zdc_lex_register(...) (zdc_lex_register(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_lex_string(void)
{
	BcParse *p = &G.prs;
	size_t depth;

	p->lex = XC_LEX_STR;
	bc_vec_pop_all(&p->lex_strnumbuf);

	depth = 1;
	for (;;) {
		char c = peek_inbuf();
		if (c == '\0') {
			RETURN_STATUS(bc_error("unterminated string"));
		}
		if (c == '[') depth++;
		if (c == ']')
			if (--depth == 0)
				break;
		if (c == '\n') {
			p->lex_line++;
			dbg_lex("++p->lex_line=%zd", p->lex_line);
		}
		bc_vec_push(&p->lex_strnumbuf, p->lex_inbuf);
		p->lex_inbuf++;
	}
	bc_vec_pushZeroByte(&p->lex_strnumbuf);
	p->lex_inbuf++; // skip trailing ']'

	G.err_line = p->lex_line;
	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zdc_lex_string(...) (zdc_lex_string(__VA_ARGS__) COMMA_SUCCESS)

#undef zdc_lex_token
static BC_STATUS zdc_lex_token(void)
{
	static const //BcLexType - should be this type, but narrower type saves size:
	uint8_t
	dc_lex_regs[] ALIGN1 = {
		XC_LEX_OP_REL_EQ, XC_LEX_OP_REL_LE, XC_LEX_OP_REL_GE, XC_LEX_OP_REL_NE,
		XC_LEX_OP_REL_LT, XC_LEX_OP_REL_GT, DC_LEX_SCOLON, DC_LEX_COLON,
		DC_LEX_ELSE, DC_LEX_LOAD, DC_LEX_LOAD_POP, DC_LEX_OP_ASSIGN,
		DC_LEX_STORE_PUSH,
	};

	BcParse *p = &G.prs;
	BcStatus s;
	char c, c2;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(dc_lex_regs); ++i) {
		if (p->lex_last == dc_lex_regs[i])
			RETURN_STATUS(zdc_lex_register());
	}

	s = BC_STATUS_SUCCESS;
	c = eat_inbuf();
	if (c >= '%' && c <= '~'
	 && (p->lex = dc_char_to_LEX[c - '%']) != XC_LEX_INVALID
	) {
		RETURN_STATUS(s);
	}

	// This is the workhorse of the lexer.
	switch (c) {
//	case '\0': // probably never reached
//		p->lex = XC_LEX_EOF;
//		break;
	case '\n':
		// '\n' is XC_LEX_NLINE, not XC_LEX_WHITESPACE
		// (and "case '\n':" is not just empty here)
		// only to allow interactive dc have a way to exit
		// "parse" stage of "parse,execute" loop
		// on <enter>, not on _next_ token (which would mean
		// commands are not executed on pressing <enter>).
		// IOW: typing "1p<enter>" should print "1" _at once_,
		// not after some more input.
		p->lex_line++;
		dbg_lex("++p->lex_line=%zd", p->lex_line);
		p->lex = XC_LEX_NLINE;
		break;
	case '\t':
	case '\v':
	case '\f':
	case '\r':
	case ' ':
		xc_lex_whitespace();
		break;
	case '!':
		c2 = *p->lex_inbuf;
		if (c2 == '=')
			p->lex = XC_LEX_OP_REL_NE;
		else if (c2 == '<')
			p->lex = XC_LEX_OP_REL_LE;
		else if (c2 == '>')
			p->lex = XC_LEX_OP_REL_GE;
		else
			RETURN_STATUS(bc_error_bad_character(c));
		p->lex_inbuf++;
		break;
	case '#':
		xc_lex_lineComment();
		break;
	case '.':
		if (isdigit(*p->lex_inbuf))
			s = zxc_lex_number(c);
		else
			s = bc_error_bad_character(c);
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
		s = zxc_lex_number(c);
		break;
	case '[':
		s = zdc_lex_string();
		break;
	default:
		p->lex = XC_LEX_INVALID;
		s = bc_error_bad_character(c);
		break;
	}

	RETURN_STATUS(s);
}
#define zdc_lex_token(...) (zdc_lex_token(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_DC

static void xc_parse_push(unsigned i)
{
	BcVec *code = &G.prs.func->code;
	dbg_compile("%s:%d pushing bytecode %zd:%d", __func__, __LINE__, code->len, i);
	bc_vec_pushByte(code, (uint8_t)i);
}

static void xc_parse_pushName(char *name)
{
#if 1
	BcVec *code = &G.prs.func->code;
	size_t pos = code->len;
	size_t len = strlen(name) + 1;

	bc_vec_expand(code, pos + len);
	strcpy(code->v + pos, name);
	code->len = pos + len;
#else
	// Smaller code, but way slow:
	do {
		xc_parse_push(*name);
	} while (*name++);
#endif
}

// Indexes < 0xfc are encoded verbatim, else first byte is
// 0xfc, 0xfd, 0xfe or 0xff, encoding "1..4 bytes",
// followed by that many bytes, lsb first.
// (The above describes 32-bit case).
#define SMALL_INDEX_LIMIT (0x100 - sizeof(size_t))

static void bc_vec_pushIndex(BcVec *v, size_t idx)
{
	size_t mask;
	unsigned amt;

	dbg_lex("%s:%d pushing index %zd", __func__, __LINE__, idx);
	if (idx < SMALL_INDEX_LIMIT) {
		bc_vec_pushByte(v, idx);
		return;
	}

	mask = ((size_t)0xff) << (sizeof(idx) * 8 - 8);
	amt = sizeof(idx);
	for (;;) {
		if (idx & mask) break;
		mask >>= 8;
		amt--;
	}
	// amt is at least 1 here - "one byte of length data follows"

	bc_vec_pushByte(v, (SMALL_INDEX_LIMIT - 1) + amt);

	do {
		bc_vec_pushByte(v, (unsigned char)idx);
		idx >>= 8;
	} while (idx != 0);
}

static void xc_parse_pushIndex(size_t idx)
{
	bc_vec_pushIndex(&G.prs.func->code, idx);
}

static void xc_parse_pushInst_and_Index(unsigned inst, size_t idx)
{
	xc_parse_push(inst);
	xc_parse_pushIndex(idx);
}

#if ENABLE_BC
static void bc_parse_pushJUMP(size_t idx)
{
	xc_parse_pushInst_and_Index(BC_INST_JUMP, idx);
}

static void bc_parse_pushJUMP_ZERO(size_t idx)
{
	xc_parse_pushInst_and_Index(BC_INST_JUMP_ZERO, idx);
}

static BC_STATUS zbc_parse_pushSTR(void)
{
	BcParse *p = &G.prs;
	char *str = xstrdup(p->lex_strnumbuf.v);

	xc_parse_pushInst_and_Index(XC_INST_STR, p->func->strs.len);
	bc_vec_push(&p->func->strs, &str);

	RETURN_STATUS(zxc_lex_next());
}
#define zbc_parse_pushSTR(...) (zbc_parse_pushSTR(__VA_ARGS__) COMMA_SUCCESS)
#endif

static void xc_parse_pushNUM(void)
{
	BcParse *p = &G.prs;
	char *num = xstrdup(p->lex_strnumbuf.v);
#if ENABLE_BC && ENABLE_DC
	size_t idx = bc_vec_push(IS_BC ? &p->func->consts : &G.prog.consts, &num);
#elif ENABLE_BC
	size_t idx = bc_vec_push(&p->func->consts, &num);
#else // DC
	size_t idx = bc_vec_push(&G.prog.consts, &num);
#endif
	xc_parse_pushInst_and_Index(XC_INST_NUM, idx);
}

static BC_STATUS zxc_parse_text_init(const char *text)
{
	G.prs.func = xc_program_func(G.prs.fidx);
	G.prs.lex_inbuf = text;
	G.prs.lex = G.prs.lex_last = XC_LEX_INVALID;
	RETURN_STATUS(zxc_lex_next());
}
#define zxc_parse_text_init(...) (zxc_parse_text_init(__VA_ARGS__) COMMA_SUCCESS)

// Called when parsing or execution detects a failure,
// resets execution structures.
static void xc_program_reset(void)
{
	BcFunc *f;
	BcInstPtr *ip;

	bc_vec_npop(&G.prog.exestack, G.prog.exestack.len - 1);
	bc_vec_pop_all(&G.prog.results);

	f = xc_program_func_BC_PROG_MAIN();
	ip = bc_vec_top(&G.prog.exestack);
	ip->inst_idx = f->code.len;
}

// Called when parsing code detects a failure,
// resets parsing structures.
static void xc_parse_reset(void)
{
	BcParse *p = &G.prs;
	if (p->fidx != BC_PROG_MAIN) {
		bc_func_free(p->func);
		bc_func_init(p->func);

		p->fidx = BC_PROG_MAIN;
		p->func = xc_program_func_BC_PROG_MAIN();
	}

	p->lex_inbuf += strlen(p->lex_inbuf);
	p->lex = XC_LEX_EOF;

	IF_BC(bc_vec_pop_all(&p->exits);)
	IF_BC(bc_vec_pop_all(&p->conds);)
	IF_BC(bc_vec_pop_all(&p->ops);)

	xc_program_reset();
}

static void xc_parse_free(void)
{
	IF_BC(bc_vec_free(&G.prs.exits);)
	IF_BC(bc_vec_free(&G.prs.conds);)
	IF_BC(bc_vec_free(&G.prs.ops);)
	bc_vec_free(&G.prs.lex_strnumbuf);
}

static void xc_parse_create(size_t fidx)
{
	BcParse *p = &G.prs;
	memset(p, 0, sizeof(BcParse));

	bc_char_vec_init(&p->lex_strnumbuf);
	IF_BC(bc_vec_init(&p->exits, sizeof(size_t), NULL);)
	IF_BC(bc_vec_init(&p->conds, sizeof(size_t), NULL);)
	IF_BC(bc_vec_init(&p->ops, sizeof(BcLexType), NULL);)

	p->fidx = fidx;
	p->func = xc_program_func(fidx);
}

static void xc_program_add_fn(void)
{
	//size_t idx;
	BcFunc f;
	bc_func_init(&f);
	//idx =
	bc_vec_push(&G.prog.fns, &f);
	//return idx;
}

#if ENABLE_BC

// Note: takes ownership of 'name' (must be malloced)
static size_t bc_program_addFunc(char *name)
{
	size_t idx;
	BcId entry, *entry_ptr;
	int inserted;

	entry.name = name;
	entry.idx = G.prog.fns.len;

	inserted = bc_map_insert(&G.prog.fn_map, &entry, &idx);
	if (!inserted) free(name);

	entry_ptr = bc_vec_item(&G.prog.fn_map, idx);
	idx = entry_ptr->idx;

	if (!inserted) {
		// There is already a function with this name.
		// It'll be redefined now, clear old definition.
		BcFunc *func = xc_program_func(entry_ptr->idx);
		bc_func_free(func);
		bc_func_init(func);
	} else {
		xc_program_add_fn();
	}

	return idx;
}

#define BC_PARSE_TOP_OP(p) (*(BcLexType*)bc_vec_top(&(p)->ops))
// We can calculate the conversion between tokens and exprs by subtracting the
// position of the first operator in the lex enum and adding the position of the
// first in the expr enum. Note: This only works for binary operators.
#define BC_TOKEN_2_INST(t) ((char) ((t) - XC_LEX_OP_POWER + XC_INST_POWER))

static BC_STATUS zbc_parse_expr(uint8_t flags);
#define zbc_parse_expr(...) (zbc_parse_expr(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_stmt_possibly_auto(bool auto_allowed);
#define zbc_parse_stmt_possibly_auto(...) (zbc_parse_stmt_possibly_auto(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_stmt(void)
{
	RETURN_STATUS(zbc_parse_stmt_possibly_auto(false));
}
#define zbc_parse_stmt(...) (zbc_parse_stmt(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_stmt_allow_NLINE_before(const char *after_X)
{
	BcParse *p = &G.prs;
	// "if(cond)<newline>stmt" is accepted too, but not 2+ newlines.
	// Same for "else", "while()", "for()".
	BcStatus s = zbc_lex_next_and_skip_NLINE();
	if (s) RETURN_STATUS(s);
	if (p->lex == XC_LEX_NLINE)
		RETURN_STATUS(bc_error_fmt("no statement after '%s'", after_X));

	RETURN_STATUS(zbc_parse_stmt());
}
#define zbc_parse_stmt_allow_NLINE_before(...) (zbc_parse_stmt_allow_NLINE_before(__VA_ARGS__) COMMA_SUCCESS)

static void bc_parse_operator(BcLexType type, size_t start, size_t *nexprs)
{
	BcParse *p = &G.prs;
	char l, r = bc_operation_PREC(type - XC_LEX_1st_op);
	bool left = bc_operation_LEFT(type - XC_LEX_1st_op);

	while (p->ops.len > start) {
		BcLexType t = BC_PARSE_TOP_OP(p);
		if (t == BC_LEX_LPAREN) break;

		l = bc_operation_PREC(t - XC_LEX_1st_op);
		if (l >= r && (l != r || !left)) break;

		xc_parse_push(BC_TOKEN_2_INST(t));
		bc_vec_pop(&p->ops);
		*nexprs -= (t != BC_LEX_OP_BOOL_NOT && t != XC_LEX_NEG);
	}

	bc_vec_push(&p->ops, &type);
}

static BC_STATUS zbc_parse_rightParen(size_t ops_bgn, size_t *nexs)
{
	BcParse *p = &G.prs;
	BcLexType top;

	if (p->ops.len <= ops_bgn)
		RETURN_STATUS(bc_error_bad_expression());
	top = BC_PARSE_TOP_OP(p);

	while (top != BC_LEX_LPAREN) {
		xc_parse_push(BC_TOKEN_2_INST(top));

		bc_vec_pop(&p->ops);
		*nexs -= (top != BC_LEX_OP_BOOL_NOT && top != XC_LEX_NEG);

		if (p->ops.len <= ops_bgn)
			RETURN_STATUS(bc_error_bad_expression());
		top = BC_PARSE_TOP_OP(p);
	}

	bc_vec_pop(&p->ops);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_parse_rightParen(...) (zbc_parse_rightParen(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_params(uint8_t flags)
{
	BcParse *p = &G.prs;
	BcStatus s;
	size_t nparams;

	dbg_lex("%s:%d p->lex:%d", __func__, __LINE__, p->lex);
	flags = (flags & ~(BC_PARSE_PRINT | BC_PARSE_REL)) | BC_PARSE_ARRAY;

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	nparams = 0;
	if (p->lex != BC_LEX_RPAREN) {
		for (;;) {
			s = zbc_parse_expr(flags);
			if (s) RETURN_STATUS(s);
			nparams++;
			if (p->lex != BC_LEX_COMMA) {
				if (p->lex == BC_LEX_RPAREN)
					break;
				RETURN_STATUS(bc_error_bad_token());
			}
			s = zxc_lex_next();
			if (s) RETURN_STATUS(s);
		}
	}

	xc_parse_pushInst_and_Index(BC_INST_CALL, nparams);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_parse_params(...) (zbc_parse_params(__VA_ARGS__) COMMA_SUCCESS)

// Note: takes ownership of 'name' (must be malloced)
static BC_STATUS zbc_parse_call(char *name, uint8_t flags)
{
	BcParse *p = &G.prs;
	BcStatus s;
	BcId entry, *entry_ptr;
	size_t idx;

	entry.name = name;

	s = zbc_parse_params(flags);
	if (s) goto err;

	if (p->lex != BC_LEX_RPAREN) {
		s = bc_error_bad_token();
		goto err;
	}

	idx = bc_map_find_exact(&G.prog.fn_map, &entry);

	if (idx == BC_VEC_INVALID_IDX) {
		// No such function exists, create an empty one
		bc_program_addFunc(name);
		idx = bc_map_find_exact(&G.prog.fn_map, &entry);
	} else
		free(name);

	entry_ptr = bc_vec_item(&G.prog.fn_map, idx);
	xc_parse_pushIndex(entry_ptr->idx);

	RETURN_STATUS(zxc_lex_next());
 err:
	free(name);
	RETURN_STATUS(s);
}
#define zbc_parse_call(...) (zbc_parse_call(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_name(BcInst *type, uint8_t flags)
{
	BcParse *p = &G.prs;
	BcStatus s;
	char *name;

	name = xstrdup(p->lex_strnumbuf.v);
	s = zxc_lex_next();
	if (s) goto err;

	if (p->lex == BC_LEX_LBRACKET) {
		s = zxc_lex_next();
		if (s) goto err;

		if (p->lex == BC_LEX_RBRACKET) {
			if (!(flags & BC_PARSE_ARRAY)) {
				s = bc_error_bad_expression();
				goto err;
			}
			*type = XC_INST_ARRAY;
		} else {
			*type = XC_INST_ARRAY_ELEM;
			flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
			s = zbc_parse_expr(flags);
			if (s) goto err;
		}
		s = zxc_lex_next();
		if (s) goto err;
		xc_parse_push(*type);
		xc_parse_pushName(name);
		free(name);
	} else if (p->lex == BC_LEX_LPAREN) {
		if (flags & BC_PARSE_NOCALL) {
			s = bc_error_bad_token();
			goto err;
		}
		*type = BC_INST_CALL;
		s = zbc_parse_call(name, flags);
	} else {
		*type = XC_INST_VAR;
		xc_parse_push(XC_INST_VAR);
		xc_parse_pushName(name);
		free(name);
	}

	RETURN_STATUS(s);
 err:
	free(name);
	RETURN_STATUS(s);
}
#define zbc_parse_name(...) (zbc_parse_name(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_read(void)
{
	BcParse *p = &G.prs;
	BcStatus s;

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);
	if (p->lex != BC_LEX_LPAREN) RETURN_STATUS(bc_error_bad_token());

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);
	if (p->lex != BC_LEX_RPAREN) RETURN_STATUS(bc_error_bad_token());

	xc_parse_push(XC_INST_READ);

	RETURN_STATUS(s);
}
#define zbc_parse_read(...) (zbc_parse_read(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_builtin(BcLexType type, uint8_t flags, BcInst *prev)
{
	BcParse *p = &G.prs;
	BcStatus s;

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);
	if (p->lex != BC_LEX_LPAREN) RETURN_STATUS(bc_error_bad_token());

	flags = (flags & ~(BC_PARSE_PRINT | BC_PARSE_REL)) | BC_PARSE_ARRAY;

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	s = zbc_parse_expr(flags);
	if (s) RETURN_STATUS(s);

	if (p->lex != BC_LEX_RPAREN) RETURN_STATUS(bc_error_bad_token());

	*prev = (type == BC_LEX_KEY_LENGTH) ? XC_INST_LENGTH : XC_INST_SQRT;
	xc_parse_push(*prev);

	RETURN_STATUS(s);
}
#define zbc_parse_builtin(...) (zbc_parse_builtin(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_scale(BcInst *type, uint8_t flags)
{
	BcParse *p = &G.prs;
	BcStatus s;

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	if (p->lex != BC_LEX_LPAREN) {
		*type = XC_INST_SCALE;
		xc_parse_push(XC_INST_SCALE);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	*type = XC_INST_SCALE_FUNC;
	flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	s = zbc_parse_expr(flags);
	if (s) RETURN_STATUS(s);
	if (p->lex != BC_LEX_RPAREN)
		RETURN_STATUS(bc_error_bad_token());
	xc_parse_push(XC_INST_SCALE_FUNC);

	RETURN_STATUS(zxc_lex_next());
}
#define zbc_parse_scale(...) (zbc_parse_scale(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_incdec(BcInst *prev, size_t *nexs, uint8_t flags)
{
	BcParse *p = &G.prs;
	BcStatus s;
	BcLexType type;
	char inst;
	BcInst etype = *prev;

	if (etype == XC_INST_VAR || etype == XC_INST_ARRAY_ELEM
	 || etype == XC_INST_SCALE || etype == BC_INST_LAST
	 || etype == XC_INST_IBASE || etype == XC_INST_OBASE
	) {
		*prev = inst = BC_INST_INC_POST + (p->lex != BC_LEX_OP_INC);
		xc_parse_push(inst);
		s = zxc_lex_next();
	} else {
		*prev = inst = BC_INST_INC_PRE + (p->lex != BC_LEX_OP_INC);

		s = zxc_lex_next();
		if (s) RETURN_STATUS(s);
		type = p->lex;

		// Because we parse the next part of the expression
		// right here, we need to increment this.
		*nexs = *nexs + 1;

		switch (type) {
		case XC_LEX_NAME:
			s = zbc_parse_name(prev, flags | BC_PARSE_NOCALL);
			break;
		case BC_LEX_KEY_IBASE:
		case BC_LEX_KEY_LAST:
		case BC_LEX_KEY_OBASE:
			xc_parse_push(type - BC_LEX_KEY_IBASE + XC_INST_IBASE);
			s = zxc_lex_next();
			break;
		case BC_LEX_KEY_SCALE:
			s = zxc_lex_next();
			if (s) RETURN_STATUS(s);
			if (p->lex == BC_LEX_LPAREN)
				s = bc_error_bad_token();
			else
				xc_parse_push(XC_INST_SCALE);
			break;
		default:
			s = bc_error_bad_token();
			break;
		}

		if (!s) xc_parse_push(inst);
	}

	RETURN_STATUS(s);
}
#define zbc_parse_incdec(...) (zbc_parse_incdec(__VA_ARGS__) COMMA_SUCCESS)

static int bc_parse_inst_isLeaf(BcInst p)
{
	return (p >= XC_INST_NUM && p <= XC_INST_SQRT)
		|| p == BC_INST_INC_POST
		|| p == BC_INST_DEC_POST
		;
}
#define BC_PARSE_LEAF(prev, bin_last, rparen) \
	(!(bin_last) && ((rparen) || bc_parse_inst_isLeaf(prev)))

static BC_STATUS zbc_parse_minus(BcInst *prev, size_t ops_bgn,
				bool rparen, bool bin_last, size_t *nexprs)
{
	BcParse *p = &G.prs;
	BcStatus s;
	BcLexType type;

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	type = BC_PARSE_LEAF(*prev, bin_last, rparen) ? XC_LEX_OP_MINUS : XC_LEX_NEG;
	*prev = BC_TOKEN_2_INST(type);

	// We can just push onto the op stack because this is the largest
	// precedence operator that gets pushed. Inc/dec does not.
	if (type != XC_LEX_OP_MINUS)
		bc_vec_push(&p->ops, &type);
	else
		bc_parse_operator(type, ops_bgn, nexprs);

	RETURN_STATUS(s);
}
#define zbc_parse_minus(...) (zbc_parse_minus(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_print(void)
{
	BcParse *p = &G.prs;
	BcStatus s;
	BcLexType type;

	for (;;) {
		s = zxc_lex_next();
		if (s) RETURN_STATUS(s);
		type = p->lex;
		if (type == XC_LEX_STR) {
			s = zbc_parse_pushSTR();
		} else {
			s = zbc_parse_expr(0);
		}
		if (s) RETURN_STATUS(s);
		xc_parse_push(XC_INST_PRINT_POP);
		if (p->lex != BC_LEX_COMMA)
			break;
	}

	RETURN_STATUS(s);
}
#define zbc_parse_print(...) (zbc_parse_print(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_return(void)
{
	BcParse *p = &G.prs;
	BcStatus s;
	BcLexType t;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	t = p->lex;
	if (t == XC_LEX_NLINE || t == BC_LEX_SCOLON || t == BC_LEX_RBRACE)
		xc_parse_push(BC_INST_RET0);
	else {
//TODO: if (p->func->voidfunc) ERROR
		s = zbc_parse_expr(0);
		if (s) RETURN_STATUS(s);

		if (t != BC_LEX_LPAREN   // "return EXPR", no ()
		 || p->lex_last != BC_LEX_RPAREN  // example: "return (a) + b"
		) {
			s = zbc_POSIX_requires("parentheses around return expressions");
			if (s) RETURN_STATUS(s);
		}

		xc_parse_push(XC_INST_RET);
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_parse_return(...) (zbc_parse_return(__VA_ARGS__) COMMA_SUCCESS)

static void rewrite_label_to_current(size_t idx)
{
	BcParse *p = &G.prs;
	size_t *label = bc_vec_item(&p->func->labels, idx);
	*label = p->func->code.len;
}

static BC_STATUS zbc_parse_if(void)
{
	BcParse *p = &G.prs;
	BcStatus s;
	size_t ip_idx;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);
	if (p->lex != BC_LEX_LPAREN) RETURN_STATUS(bc_error_bad_token());

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);
	s = zbc_parse_expr(BC_PARSE_REL);
	if (s) RETURN_STATUS(s);
	if (p->lex != BC_LEX_RPAREN) RETURN_STATUS(bc_error_bad_token());

	// Encode "if zero, jump to ..."
	// Pushed value (destination of the jump) is uninitialized,
	// will be rewritten to be address of "end of if()" or of "else".
	ip_idx = bc_vec_push(&p->func->labels, &ip_idx);
	bc_parse_pushJUMP_ZERO(ip_idx);

	s = zbc_parse_stmt_allow_NLINE_before(STRING_if);
	if (s) RETURN_STATUS(s);

	dbg_lex("%s:%d in if after stmt: p->lex:%d", __func__, __LINE__, p->lex);
	if (p->lex == BC_LEX_KEY_ELSE) {
		size_t ip2_idx;

		// Encode "after then_stmt, jump to end of if()"
		ip2_idx = bc_vec_push(&p->func->labels, &ip2_idx);
		dbg_lex("%s:%d after if() then_stmt: BC_INST_JUMP to %zd", __func__, __LINE__, ip2_idx);
		bc_parse_pushJUMP(ip2_idx);

		dbg_lex("%s:%d rewriting 'if_zero' label to jump to 'else'-> %zd", __func__, __LINE__, p->func->code.len);
		rewrite_label_to_current(ip_idx);

		ip_idx = ip2_idx;

		s = zbc_parse_stmt_allow_NLINE_before(STRING_else);
		if (s) RETURN_STATUS(s);
	}

	dbg_lex("%s:%d rewriting label to jump after 'if' body-> %zd", __func__, __LINE__, p->func->code.len);
	rewrite_label_to_current(ip_idx);

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_parse_if(...) (zbc_parse_if(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_while(void)
{
	BcParse *p = &G.prs;
	BcStatus s;
	size_t cond_idx;
	size_t ip_idx;

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);
	if (p->lex != BC_LEX_LPAREN) RETURN_STATUS(bc_error_bad_token());
	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	cond_idx = bc_vec_push(&p->func->labels, &p->func->code.len);
	ip_idx = cond_idx + 1;
	bc_vec_push(&p->conds, &cond_idx);

	bc_vec_push(&p->exits, &ip_idx);
	bc_vec_push(&p->func->labels, &ip_idx);

	s = zbc_parse_expr(BC_PARSE_REL);
	if (s) RETURN_STATUS(s);
	if (p->lex != BC_LEX_RPAREN) RETURN_STATUS(bc_error_bad_token());

	bc_parse_pushJUMP_ZERO(ip_idx);

	s = zbc_parse_stmt_allow_NLINE_before(STRING_while);
	if (s) RETURN_STATUS(s);

	dbg_lex("%s:%d BC_INST_JUMP to %zd", __func__, __LINE__, cond_idx);
	bc_parse_pushJUMP(cond_idx);

	dbg_lex("%s:%d rewriting label-> %zd", __func__, __LINE__, p->func->code.len);
	rewrite_label_to_current(ip_idx);

	bc_vec_pop(&p->exits);
	bc_vec_pop(&p->conds);

	RETURN_STATUS(s);
}
#define zbc_parse_while(...) (zbc_parse_while(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_for(void)
{
	BcParse *p = &G.prs;
	BcStatus s;
	size_t cond_idx, exit_idx, body_idx, update_idx;

	dbg_lex("%s:%d p->lex:%d", __func__, __LINE__, p->lex);
	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);
	if (p->lex != BC_LEX_LPAREN) RETURN_STATUS(bc_error_bad_token());
	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	if (p->lex != BC_LEX_SCOLON) {
		s = zbc_parse_expr(0);
		xc_parse_push(XC_INST_POP);
		if (s) RETURN_STATUS(s);
	} else {
		s = zbc_POSIX_does_not_allow_empty_X_expression_in_for("init");
		if (s) RETURN_STATUS(s);
	}

	if (p->lex != BC_LEX_SCOLON) RETURN_STATUS(bc_error_bad_token());
	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	cond_idx = bc_vec_push(&p->func->labels, &p->func->code.len);
	update_idx = cond_idx + 1;
	body_idx = update_idx + 1;
	exit_idx = body_idx + 1;

	if (p->lex != BC_LEX_SCOLON)
		s = zbc_parse_expr(BC_PARSE_REL);
	else {
		// Set this for the next call to xc_parse_pushNUM().
		// This is safe to set because the current token is a semicolon,
		// which has no string requirement.
		bc_vec_string(&p->lex_strnumbuf, 1, "1");
		xc_parse_pushNUM();
		s = zbc_POSIX_does_not_allow_empty_X_expression_in_for("condition");
	}
	if (s) RETURN_STATUS(s);

	if (p->lex != BC_LEX_SCOLON) RETURN_STATUS(bc_error_bad_token());

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	bc_parse_pushJUMP_ZERO(exit_idx);
	bc_parse_pushJUMP(body_idx);

	bc_vec_push(&p->conds, &update_idx);
	bc_vec_push(&p->func->labels, &p->func->code.len);

	if (p->lex != BC_LEX_RPAREN) {
		s = zbc_parse_expr(0);
		if (s) RETURN_STATUS(s);
		if (p->lex != BC_LEX_RPAREN) RETURN_STATUS(bc_error_bad_token());
		xc_parse_push(XC_INST_POP);
	} else {
		s = zbc_POSIX_does_not_allow_empty_X_expression_in_for("update");
		if (s) RETURN_STATUS(s);
	}

	bc_parse_pushJUMP(cond_idx);
	bc_vec_push(&p->func->labels, &p->func->code.len);

	bc_vec_push(&p->exits, &exit_idx);
	bc_vec_push(&p->func->labels, &exit_idx);

	s = zbc_parse_stmt_allow_NLINE_before(STRING_for);
	if (s) RETURN_STATUS(s);

	dbg_lex("%s:%d BC_INST_JUMP to %zd", __func__, __LINE__, update_idx);
	bc_parse_pushJUMP(update_idx);

	dbg_lex("%s:%d rewriting label-> %zd", __func__, __LINE__, p->func->code.len);
	rewrite_label_to_current(exit_idx);

	bc_vec_pop(&p->exits);
	bc_vec_pop(&p->conds);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_parse_for(...) (zbc_parse_for(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_break_or_continue(BcLexType type)
{
	BcParse *p = &G.prs;
	size_t i;

	if (type == BC_LEX_KEY_BREAK) {
		if (p->exits.len == 0) // none of the enclosing blocks is a loop
			RETURN_STATUS(bc_error_bad_token());
		i = *(size_t*)bc_vec_top(&p->exits);
	} else {
		i = *(size_t*)bc_vec_top(&p->conds);
	}
	bc_parse_pushJUMP(i);

	RETURN_STATUS(zxc_lex_next());
}
#define zbc_parse_break_or_continue(...) (zbc_parse_break_or_continue(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_func_insert(BcFunc *f, char *name, BcType type)
{
	BcId *autoid;
	BcId a;
	size_t i;

	autoid = (void*)f->autos.v;
	for (i = 0; i < f->autos.len; i++, autoid++) {
		if (strcmp(name, autoid->name) == 0
		 && type == (BcType) autoid->idx
		) {
			RETURN_STATUS(bc_error("duplicate function parameter or auto name"));
		}
	}

	a.idx = type;
	a.name = name;

	bc_vec_push(&f->autos, &a);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_func_insert(...) (zbc_func_insert(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_funcdef(void)
{
	BcParse *p = &G.prs;
	BcStatus s;
	bool comma, voidfunc;
	char *name;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);
	if (p->lex != XC_LEX_NAME)
		RETURN_STATUS(bc_error_bad_function_definition());

	// To be maximally both POSIX and GNU-compatible,
	// "void" is not treated as a normal keyword:
	// you can have variable named "void", and even a function
	// named "void": "define void() { return 6; }" is ok.
	// _Only_ "define void f() ..." syntax treats "void"
	// specially.
	voidfunc = (strcmp(p->lex_strnumbuf.v, "void") == 0);

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	voidfunc = (voidfunc && p->lex == XC_LEX_NAME);
	if (voidfunc) {
		s = zxc_lex_next();
		if (s) RETURN_STATUS(s);
	}

	if (p->lex != BC_LEX_LPAREN)
		RETURN_STATUS(bc_error_bad_function_definition());

	p->fidx = bc_program_addFunc(xstrdup(p->lex_strnumbuf.v));
	p->func = xc_program_func(p->fidx);
	p->func->voidfunc = voidfunc;

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	comma = false;
	while (p->lex != BC_LEX_RPAREN) {
		BcType t = BC_TYPE_VAR;

		if (p->lex == XC_LEX_OP_MULTIPLY) {
			t = BC_TYPE_REF;
			s = zxc_lex_next();
			if (s) RETURN_STATUS(s);
			s = zbc_POSIX_does_not_allow("references");
			if (s) RETURN_STATUS(s);
		}

		if (p->lex != XC_LEX_NAME)
			RETURN_STATUS(bc_error_bad_function_definition());

		++p->func->nparams;

		name = xstrdup(p->lex_strnumbuf.v);
		s = zxc_lex_next();
		if (s) goto err;

		if (p->lex == BC_LEX_LBRACKET) {
			if (t == BC_TYPE_VAR) t = BC_TYPE_ARRAY;
			s = zxc_lex_next();
			if (s) goto err;

			if (p->lex != BC_LEX_RBRACKET) {
				s = bc_error_bad_function_definition();
				goto err;
			}

			s = zxc_lex_next();
			if (s) goto err;
		}
		else if (t == BC_TYPE_REF) {
			s = bc_error_at("vars can't be references");
			goto err;
		}

		comma = p->lex == BC_LEX_COMMA;
		if (comma) {
			s = zxc_lex_next();
			if (s) goto err;
		}

		s = zbc_func_insert(p->func, name, t);
		if (s) goto err;
	}

	if (comma) RETURN_STATUS(bc_error_bad_function_definition());

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	if (p->lex != BC_LEX_LBRACE) {
		s = zbc_POSIX_requires("the left brace be on the same line as the function header");
		if (s) RETURN_STATUS(s);
	}

	// Prevent "define z()<newline>" from being interpreted as function with empty stmt as body
	s = zbc_lex_skip_if_at_NLINE();
	if (s) RETURN_STATUS(s);
	// GNU bc requires a {} block even if function body has single stmt, enforce this
	if (p->lex != BC_LEX_LBRACE)
		RETURN_STATUS(bc_error("function { body } expected"));

	p->in_funcdef++; // to determine whether "return" stmt is allowed, and such
	s = zbc_parse_stmt_possibly_auto(true);
	p->in_funcdef--;
	if (s) RETURN_STATUS(s);

	xc_parse_push(BC_INST_RET0);

	// Subsequent code generation is into main program
	p->fidx = BC_PROG_MAIN;
	p->func = xc_program_func_BC_PROG_MAIN();

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
 err:
	dbg_lex_done("%s:%d done (error)", __func__, __LINE__);
	free(name);
	RETURN_STATUS(s);
}
#define zbc_parse_funcdef(...) (zbc_parse_funcdef(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_auto(void)
{
	BcParse *p = &G.prs;
	BcStatus s;
	char *name;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	for (;;) {
		BcType t;

		if (p->lex != XC_LEX_NAME)
			RETURN_STATUS(bc_error_at("bad 'auto' syntax"));

		name = xstrdup(p->lex_strnumbuf.v);
		s = zxc_lex_next();
		if (s) goto err;

		t = BC_TYPE_VAR;
		if (p->lex == BC_LEX_LBRACKET) {
			t = BC_TYPE_ARRAY;
			s = zxc_lex_next();
			if (s) goto err;

			if (p->lex != BC_LEX_RBRACKET) {
				s = bc_error_at("bad 'auto' syntax");
				goto err;
			}
			s = zxc_lex_next();
			if (s) goto err;
		}

		s = zbc_func_insert(p->func, name, t);
		if (s) goto err;

		if (p->lex == XC_LEX_NLINE
		 || p->lex == BC_LEX_SCOLON
		//|| p->lex == BC_LEX_RBRACE // allow "define f() {auto a}"
		) {
			break;
		}
		if (p->lex != BC_LEX_COMMA)
			RETURN_STATUS(bc_error_at("bad 'auto' syntax"));
		s = zxc_lex_next(); // skip comma
		if (s) RETURN_STATUS(s);
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(BC_STATUS_SUCCESS);
 err:
	free(name);
	dbg_lex_done("%s:%d done (ERROR)", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_parse_auto(...) (zbc_parse_auto(__VA_ARGS__) COMMA_SUCCESS)

#undef zbc_parse_stmt_possibly_auto
static BC_STATUS zbc_parse_stmt_possibly_auto(bool auto_allowed)
{
	BcParse *p = &G.prs;
	BcStatus s = BC_STATUS_SUCCESS;

	dbg_lex_enter("%s:%d entered, p->lex:%d", __func__, __LINE__, p->lex);

	if (p->lex == XC_LEX_NLINE) {
		dbg_lex_done("%s:%d done (seen XC_LEX_NLINE)", __func__, __LINE__);
		RETURN_STATUS(s);
	}
	if (p->lex == BC_LEX_SCOLON) {
		dbg_lex_done("%s:%d done (seen BC_LEX_SCOLON)", __func__, __LINE__);
		RETURN_STATUS(s);
	}

	if (p->lex == BC_LEX_LBRACE) {
		dbg_lex("%s:%d BC_LEX_LBRACE: (auto_allowed:%d)", __func__, __LINE__, auto_allowed);
		do {
			s = zxc_lex_next();
			if (s) RETURN_STATUS(s);
		} while (p->lex == XC_LEX_NLINE);
		if (auto_allowed && p->lex == BC_LEX_KEY_AUTO) {
			dbg_lex("%s:%d calling zbc_parse_auto()", __func__, __LINE__);
			s = zbc_parse_auto();
			if (s) RETURN_STATUS(s);
		}
		while (p->lex != BC_LEX_RBRACE) {
			dbg_lex("%s:%d block parsing loop", __func__, __LINE__);
			s = zbc_parse_stmt();
			if (s) RETURN_STATUS(s);
			// Check that next token is a correct stmt delimiter -
			// disallows "print 1 print 2" and such.
			if (p->lex == BC_LEX_RBRACE)
				break;
			if (p->lex != BC_LEX_SCOLON
			 && p->lex != XC_LEX_NLINE
			) {
				RETURN_STATUS(bc_error_at("bad statement terminator"));
			}
		        s = zxc_lex_next();
			if (s) RETURN_STATUS(s);
		}
		s = zxc_lex_next();
		dbg_lex_done("%s:%d done (seen BC_LEX_RBRACE)", __func__, __LINE__);
		RETURN_STATUS(s);
	}

	dbg_lex("%s:%d p->lex:%d", __func__, __LINE__, p->lex);
	switch (p->lex) {
	case XC_LEX_OP_MINUS:
	case BC_LEX_OP_INC:
	case BC_LEX_OP_DEC:
	case BC_LEX_OP_BOOL_NOT:
	case BC_LEX_LPAREN:
	case XC_LEX_NAME:
	case XC_LEX_NUMBER:
	case BC_LEX_KEY_IBASE:
	case BC_LEX_KEY_LAST:
	case BC_LEX_KEY_LENGTH:
	case BC_LEX_KEY_OBASE:
	case BC_LEX_KEY_READ:
	case BC_LEX_KEY_SCALE:
	case BC_LEX_KEY_SQRT:
		s = zbc_parse_expr(BC_PARSE_PRINT);
		break;
	case XC_LEX_STR:
		s = zbc_parse_pushSTR();
		xc_parse_push(XC_INST_PRINT_STR);
		break;
	case BC_LEX_KEY_BREAK:
	case BC_LEX_KEY_CONTINUE:
		s = zbc_parse_break_or_continue(p->lex);
		break;
	case BC_LEX_KEY_FOR:
		s = zbc_parse_for();
		break;
	case BC_LEX_KEY_HALT:
		xc_parse_push(BC_INST_HALT);
		s = zxc_lex_next();
		break;
	case BC_LEX_KEY_IF:
		s = zbc_parse_if();
		break;
	case BC_LEX_KEY_LIMITS:
		// "limits" is a compile-time command,
		// the output is produced at _parse time_.
		printf(
			"BC_BASE_MAX     = "BC_MAX_OBASE_STR "\n"
			"BC_DIM_MAX      = "BC_MAX_DIM_STR   "\n"
			"BC_SCALE_MAX    = "BC_MAX_SCALE_STR "\n"
			"BC_STRING_MAX   = "BC_MAX_STRING_STR"\n"
		//	"BC_NUM_MAX      = "BC_MAX_NUM_STR   "\n" - GNU bc does not show this
			"MAX Exponent    = "BC_MAX_EXP_STR   "\n"
			"Number of vars  = "BC_MAX_VARS_STR  "\n"
		);
		s = zxc_lex_next();
		break;
	case BC_LEX_KEY_PRINT:
		s = zbc_parse_print();
		break;
	case BC_LEX_KEY_QUIT:
		// "quit" is a compile-time command. For example,
		// "if (0 == 1) quit" terminates when parsing the statement,
		// not when it is executed
		QUIT_OR_RETURN_TO_MAIN;
	case BC_LEX_KEY_RETURN:
		if (!p->in_funcdef)
			RETURN_STATUS(bc_error("'return' not in a function"));
		s = zbc_parse_return();
		break;
	case BC_LEX_KEY_WHILE:
		s = zbc_parse_while();
		break;
	default:
		s = bc_error_bad_token();
		break;
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_parse_stmt_possibly_auto(...) (zbc_parse_stmt_possibly_auto(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_stmt_or_funcdef(void)
{
	BcParse *p = &G.prs;
	BcStatus s;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
//why?
//	if (p->lex == XC_LEX_EOF)
//		s = bc_error("end of file");
//	else
	if (p->lex == BC_LEX_KEY_DEFINE) {
		dbg_lex("%s:%d p->lex:BC_LEX_KEY_DEFINE", __func__, __LINE__);
		s = zbc_parse_funcdef();
	} else {
		dbg_lex("%s:%d p->lex:%d (not BC_LEX_KEY_DEFINE)", __func__, __LINE__, p->lex);
		s = zbc_parse_stmt();
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_parse_stmt_or_funcdef(...) (zbc_parse_stmt_or_funcdef(__VA_ARGS__) COMMA_SUCCESS)

#undef zbc_parse_expr
static BC_STATUS zbc_parse_expr(uint8_t flags)
{
	BcParse *p = &G.prs;
	BcInst prev = XC_INST_PRINT;
	size_t nexprs = 0, ops_bgn = p->ops.len;
	unsigned nparens, nrelops;
	bool paren_first, rprn, assign, bin_last, incdec;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	paren_first = (p->lex == BC_LEX_LPAREN);
	nparens = nrelops = 0;
	rprn = assign = incdec = false;
	bin_last = true;

	for (;;) {
		bool get_token;
		BcStatus s;
		BcLexType t = p->lex;

		if (!lex_allowed_in_bc_expr(t))
			break;

		dbg_lex("%s:%d t:%d", __func__, __LINE__, t);
		get_token = false;
		s = BC_STATUS_SUCCESS;
		switch (t) {
		case BC_LEX_OP_INC:
		case BC_LEX_OP_DEC:
			dbg_lex("%s:%d LEX_OP_INC/DEC", __func__, __LINE__);
			if (incdec) RETURN_STATUS(bc_error_bad_assignment());
			s = zbc_parse_incdec(&prev, &nexprs, flags);
			incdec = true;
			rprn = bin_last = false;
			//get_token = false; - already is
			break;
		case XC_LEX_OP_MINUS:
			dbg_lex("%s:%d LEX_OP_MINUS", __func__, __LINE__);
			s = zbc_parse_minus(&prev, ops_bgn, rprn, bin_last, &nexprs);
			rprn = false;
			//get_token = false; - already is
			bin_last = (prev == XC_INST_MINUS);
			if (bin_last) incdec = false;
			break;
		case BC_LEX_OP_ASSIGN_POWER:
		case BC_LEX_OP_ASSIGN_MULTIPLY:
		case BC_LEX_OP_ASSIGN_DIVIDE:
		case BC_LEX_OP_ASSIGN_MODULUS:
		case BC_LEX_OP_ASSIGN_PLUS:
		case BC_LEX_OP_ASSIGN_MINUS:
		case BC_LEX_OP_ASSIGN:
			dbg_lex("%s:%d LEX_ASSIGNxyz", __func__, __LINE__);
			if (prev != XC_INST_VAR && prev != XC_INST_ARRAY_ELEM
			 && prev != XC_INST_SCALE && prev != XC_INST_IBASE
			 && prev != XC_INST_OBASE && prev != BC_INST_LAST
			) {
				RETURN_STATUS(bc_error_bad_assignment());
			}
		// Fallthrough.
		case XC_LEX_OP_POWER:
		case XC_LEX_OP_MULTIPLY:
		case XC_LEX_OP_DIVIDE:
		case XC_LEX_OP_MODULUS:
		case XC_LEX_OP_PLUS:
		case XC_LEX_OP_REL_EQ:
		case XC_LEX_OP_REL_LE:
		case XC_LEX_OP_REL_GE:
		case XC_LEX_OP_REL_NE:
		case XC_LEX_OP_REL_LT:
		case XC_LEX_OP_REL_GT:
		case BC_LEX_OP_BOOL_NOT:
		case BC_LEX_OP_BOOL_OR:
		case BC_LEX_OP_BOOL_AND:
			dbg_lex("%s:%d LEX_OP_xyz", __func__, __LINE__);
			if (t == BC_LEX_OP_BOOL_NOT) {
				if (!bin_last && p->lex_last != BC_LEX_OP_BOOL_NOT)
					RETURN_STATUS(bc_error_bad_expression());
			} else if (prev == XC_INST_BOOL_NOT) {
				RETURN_STATUS(bc_error_bad_expression());
			}

			nrelops += (t >= XC_LEX_OP_REL_EQ && t <= XC_LEX_OP_REL_GT);
			prev = BC_TOKEN_2_INST(t);
			bc_parse_operator(t, ops_bgn, &nexprs);
			rprn = incdec = false;
			get_token = true;
			bin_last = (t != BC_LEX_OP_BOOL_NOT);
			break;
		case BC_LEX_LPAREN:
			dbg_lex("%s:%d LEX_LPAREN", __func__, __LINE__);
			if (BC_PARSE_LEAF(prev, bin_last, rprn))
				RETURN_STATUS(bc_error_bad_expression());
			bc_vec_push(&p->ops, &t);
			nparens++;
			get_token = true;
			rprn = incdec = false;
			break;
		case BC_LEX_RPAREN:
			dbg_lex("%s:%d LEX_RPAREN", __func__, __LINE__);
//why?
//			if (p->lex_last == BC_LEX_LPAREN) {
//				RETURN_STATUS(bc_error_at("empty expression"));
//			}
			if (bin_last || prev == XC_INST_BOOL_NOT)
				RETURN_STATUS(bc_error_bad_expression());
			if (nparens == 0) {
				goto exit_loop;
			}
			s = zbc_parse_rightParen(ops_bgn, &nexprs);
			nparens--;
			get_token = true;
			rprn = true;
			bin_last = incdec = false;
			break;
		case XC_LEX_NAME:
			dbg_lex("%s:%d LEX_NAME", __func__, __LINE__);
			if (BC_PARSE_LEAF(prev, bin_last, rprn))
				RETURN_STATUS(bc_error_bad_expression());
			s = zbc_parse_name(&prev, flags & ~BC_PARSE_NOCALL);
			rprn = (prev == BC_INST_CALL);
			bin_last = false;
			//get_token = false; - already is
			nexprs++;
			break;
		case XC_LEX_NUMBER:
			dbg_lex("%s:%d LEX_NUMBER", __func__, __LINE__);
			if (BC_PARSE_LEAF(prev, bin_last, rprn))
				RETURN_STATUS(bc_error_bad_expression());
			xc_parse_pushNUM();
			prev = XC_INST_NUM;
			get_token = true;
			rprn = bin_last = false;
			nexprs++;
			break;
		case BC_LEX_KEY_IBASE:
		case BC_LEX_KEY_LAST:
		case BC_LEX_KEY_OBASE:
			dbg_lex("%s:%d LEX_IBASE/LAST/OBASE", __func__, __LINE__);
			if (BC_PARSE_LEAF(prev, bin_last, rprn))
				RETURN_STATUS(bc_error_bad_expression());
			prev = (char) (t - BC_LEX_KEY_IBASE + XC_INST_IBASE);
			xc_parse_push((char) prev);
			get_token = true;
			rprn = bin_last = false;
			nexprs++;
			break;
		case BC_LEX_KEY_LENGTH:
		case BC_LEX_KEY_SQRT:
			dbg_lex("%s:%d LEX_LEN/SQRT", __func__, __LINE__);
			if (BC_PARSE_LEAF(prev, bin_last, rprn))
				RETURN_STATUS(bc_error_bad_expression());
			s = zbc_parse_builtin(t, flags, &prev);
			get_token = true;
			rprn = bin_last = incdec = false;
			nexprs++;
			break;
		case BC_LEX_KEY_READ:
			dbg_lex("%s:%d LEX_READ", __func__, __LINE__);
			if (BC_PARSE_LEAF(prev, bin_last, rprn))
				RETURN_STATUS(bc_error_bad_expression());
			s = zbc_parse_read();
			prev = XC_INST_READ;
			get_token = true;
			rprn = bin_last = incdec = false;
			nexprs++;
			break;
		case BC_LEX_KEY_SCALE:
			dbg_lex("%s:%d LEX_SCALE", __func__, __LINE__);
			if (BC_PARSE_LEAF(prev, bin_last, rprn))
				RETURN_STATUS(bc_error_bad_expression());
			s = zbc_parse_scale(&prev, flags);
			//get_token = false; - already is
			rprn = bin_last = false;
			nexprs++;
			break;
		default:
			RETURN_STATUS(bc_error_bad_token());
		}

		if (s || G_interrupt) // error, or ^C: stop parsing
			RETURN_STATUS(BC_STATUS_FAILURE);
		if (get_token) {
			s = zxc_lex_next();
			if (s) RETURN_STATUS(s);
		}
	}
 exit_loop:

	while (p->ops.len > ops_bgn) {
		BcLexType top = BC_PARSE_TOP_OP(p);
		assign = (top >= BC_LEX_OP_ASSIGN_POWER && top <= BC_LEX_OP_ASSIGN);

		if (top == BC_LEX_LPAREN || top == BC_LEX_RPAREN)
			RETURN_STATUS(bc_error_bad_expression());

		xc_parse_push(BC_TOKEN_2_INST(top));

		nexprs -= (top != BC_LEX_OP_BOOL_NOT && top != XC_LEX_NEG);
		bc_vec_pop(&p->ops);
	}

	if (prev == XC_INST_BOOL_NOT || nexprs != 1)
		RETURN_STATUS(bc_error_bad_expression());

	if (!(flags & BC_PARSE_REL) && nrelops) {
		BcStatus s;
		s = zbc_POSIX_does_not_allow("comparison operators outside if or loops");
		if (s) RETURN_STATUS(s);
	} else if ((flags & BC_PARSE_REL) && nrelops > 1) {
		BcStatus s;
		s = zbc_POSIX_requires("exactly one comparison operator per condition");
		if (s) RETURN_STATUS(s);
	}

	if (flags & BC_PARSE_PRINT) {
		if (paren_first || !assign)
			xc_parse_push(XC_INST_PRINT);
		xc_parse_push(XC_INST_POP);
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_parse_expr(...) (zbc_parse_expr(__VA_ARGS__) COMMA_SUCCESS)

#endif // ENABLE_BC

#if ENABLE_DC

static BC_STATUS zdc_parse_register(void)
{
	BcParse *p = &G.prs;
	BcStatus s;

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);
	if (p->lex != XC_LEX_NAME) RETURN_STATUS(bc_error_bad_token());

	xc_parse_pushName(p->lex_strnumbuf.v);

	RETURN_STATUS(s);
}
#define zdc_parse_register(...) (zdc_parse_register(__VA_ARGS__) COMMA_SUCCESS)

static void dc_parse_string(void)
{
	BcParse *p = &G.prs;
	char *str;
	size_t len = G.prog.strs.len;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);

	str = xstrdup(p->lex_strnumbuf.v);
	xc_parse_pushInst_and_Index(XC_INST_STR, len);
	bc_vec_push(&G.prog.strs, &str);

	// Add an empty function so that if zdc_program_execStr ever needs to
	// parse the string into code (from the 'x' command) there's somewhere
	// to store the bytecode.
	xc_program_add_fn();
	p->func = xc_program_func(p->fidx);

	dbg_lex_done("%s:%d done", __func__, __LINE__);
}

static BC_STATUS zdc_parse_mem(uint8_t inst, bool name, bool store)
{
	BcStatus s;

	xc_parse_push(inst);
	if (name) {
		s = zdc_parse_register();
		if (s) RETURN_STATUS(s);
	}

	if (store) {
		xc_parse_push(DC_INST_SWAP);
		xc_parse_push(XC_INST_ASSIGN);
		xc_parse_push(XC_INST_POP);
	}

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zdc_parse_mem(...) (zdc_parse_mem(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_parse_cond(uint8_t inst)
{
	BcParse *p = &G.prs;
	BcStatus s;

	xc_parse_push(inst);
	xc_parse_push(DC_INST_EXEC_COND);

	s = zdc_parse_register();
	if (s) RETURN_STATUS(s);

	s = zxc_lex_next();
	if (s) RETURN_STATUS(s);

	// Note that 'else' part can not be on the next line:
	// echo -e '[1p]sa [2p]sb 2 1>a eb' | dc - OK, prints "2"
	// echo -e '[1p]sa [2p]sb 2 1>a\neb' | dc - parse error
	if (p->lex == DC_LEX_ELSE) {
		s = zdc_parse_register();
		if (s) RETURN_STATUS(s);
		s = zxc_lex_next();
	} else {
		xc_parse_push('\0');
	}

	RETURN_STATUS(s);
}
#define zdc_parse_cond(...) (zdc_parse_cond(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_parse_token(BcLexType t)
{
	BcStatus s;
	uint8_t inst;
	bool assign, get_token;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = BC_STATUS_SUCCESS;
	get_token = true;
	switch (t) {
	case XC_LEX_OP_REL_EQ:
	case XC_LEX_OP_REL_LE:
	case XC_LEX_OP_REL_GE:
	case XC_LEX_OP_REL_NE:
	case XC_LEX_OP_REL_LT:
	case XC_LEX_OP_REL_GT:
		dbg_lex("%s:%d LEX_OP_REL_xyz", __func__, __LINE__);
		s = zdc_parse_cond(t - XC_LEX_OP_REL_EQ + XC_INST_REL_EQ);
		get_token = false;
		break;
	case DC_LEX_SCOLON:
	case DC_LEX_COLON:
		dbg_lex("%s:%d LEX_[S]COLON", __func__, __LINE__);
		s = zdc_parse_mem(XC_INST_ARRAY_ELEM, true, t == DC_LEX_COLON);
		break;
	case XC_LEX_STR:
		dbg_lex("%s:%d LEX_STR", __func__, __LINE__);
		dc_parse_string();
		break;
	case XC_LEX_NEG:
		dbg_lex("%s:%d LEX_NEG", __func__, __LINE__);
		s = zxc_lex_next();
		if (s) RETURN_STATUS(s);
		if (G.prs.lex != XC_LEX_NUMBER)
			RETURN_STATUS(bc_error_bad_token());
		xc_parse_pushNUM();
		xc_parse_push(XC_INST_NEG);
		break;
	case XC_LEX_NUMBER:
		dbg_lex("%s:%d LEX_NUMBER", __func__, __LINE__);
		xc_parse_pushNUM();
		break;
	case DC_LEX_READ:
		dbg_lex("%s:%d LEX_KEY_READ", __func__, __LINE__);
		xc_parse_push(XC_INST_READ);
		break;
	case DC_LEX_OP_ASSIGN:
	case DC_LEX_STORE_PUSH:
		dbg_lex("%s:%d LEX_OP_ASSIGN/STORE_PUSH", __func__, __LINE__);
		assign = (t == DC_LEX_OP_ASSIGN);
		inst = assign ? XC_INST_VAR : DC_INST_PUSH_TO_VAR;
		s = zdc_parse_mem(inst, true, assign);
		break;
	case DC_LEX_LOAD:
	case DC_LEX_LOAD_POP:
		dbg_lex("%s:%d LEX_OP_LOAD[_POP]", __func__, __LINE__);
		inst = t == DC_LEX_LOAD_POP ? DC_INST_PUSH_VAR : DC_INST_LOAD;
		s = zdc_parse_mem(inst, true, false);
		break;
	case DC_LEX_STORE_IBASE:
	case DC_LEX_STORE_SCALE:
	case DC_LEX_STORE_OBASE:
		dbg_lex("%s:%d LEX_OP_STORE_I/OBASE/SCALE", __func__, __LINE__);
		inst = t - DC_LEX_STORE_IBASE + XC_INST_IBASE;
		s = zdc_parse_mem(inst, false, true);
		break;
	default:
		dbg_lex_done("%s:%d done (bad token)", __func__, __LINE__);
		RETURN_STATUS(bc_error_bad_token());
	}

	if (!s && get_token) s = zxc_lex_next();

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zdc_parse_token(...) (zdc_parse_token(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_parse_expr(void)
{
	BcParse *p = &G.prs;
	int i;

	if (p->lex == XC_LEX_NLINE)
		RETURN_STATUS(zxc_lex_next());

	i = (int)p->lex - (int)XC_LEX_OP_POWER;
	if (i >= 0) {
		BcInst inst = dc_LEX_to_INST[i];
		if (inst != DC_INST_INVALID) {
			xc_parse_push(inst);
			RETURN_STATUS(zxc_lex_next());
		}
	}
	RETURN_STATUS(zdc_parse_token(p->lex));
}
#define zdc_parse_expr(...) (zdc_parse_expr(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_parse_exprs_until_eof(void)
{
	BcParse *p = &G.prs;
	dbg_lex_enter("%s:%d entered, p->lex:%d", __func__, __LINE__, p->lex);
	while (p->lex != XC_LEX_EOF) {
		BcStatus s = zdc_parse_expr();
		if (s) RETURN_STATUS(s);
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zdc_parse_exprs_until_eof(...) (zdc_parse_exprs_until_eof(__VA_ARGS__) COMMA_SUCCESS)

#endif // ENABLE_DC

//
// Execution engine
//

#define BC_PROG_STR(n) (!(n)->num && !(n)->cap)
#define BC_PROG_NUM(r, n) \
	((r)->t != XC_RESULT_ARRAY && (r)->t != XC_RESULT_STR && !BC_PROG_STR(n))

#define STACK_HAS_MORE_THAN(s, n)          ((s)->len > ((size_t)(n)))
#define STACK_HAS_EQUAL_OR_MORE_THAN(s, n) ((s)->len >= ((size_t)(n)))

static size_t xc_program_index(char *code, size_t *bgn)
{
	unsigned char *bytes = (void*)(code + *bgn);
	unsigned amt;
	unsigned i;
	size_t res;

	amt = *bytes++;
	if (amt < SMALL_INDEX_LIMIT) {
		*bgn += 1;
		return amt;
	}
	amt -= (SMALL_INDEX_LIMIT - 1); // amt is 1 or more here
	*bgn += amt + 1;

	res = 0;
	i = 0;
	do {
		res |= (size_t)(*bytes++) << i;
		i += 8;
	} while (--amt != 0);

	return res;
}

static char *xc_program_name(char *code, size_t *bgn)
{
	code += *bgn;
	*bgn += strlen(code) + 1;

	return xstrdup(code);
}

static BcVec* xc_program_dereference(BcVec *vec)
{
	BcVec *v;
	size_t vidx, nidx, i = 0;

	//assert(vec->size == sizeof(uint8_t));

	vidx = xc_program_index(vec->v, &i);
	nidx = xc_program_index(vec->v, &i);

	v = bc_vec_item(&G.prog.arrs, vidx);
	v = bc_vec_item(v, nidx);

	//assert(v->size != sizeof(uint8_t));

	return v;
}

static BcVec* xc_program_search(char *id, BcType type)
{
	BcId e, *ptr;
	BcVec *v, *map;
	size_t i;
	int new;
	bool var = (type == BC_TYPE_VAR);

	v = var ? &G.prog.vars : &G.prog.arrs;
	map = var ? &G.prog.var_map : &G.prog.arr_map;

	e.name = id;
	e.idx = v->len;
	new = bc_map_insert(map, &e, &i); // 1 if insertion was successful

	if (new) {
		BcVec v2;
		bc_array_init(&v2, var);
		bc_vec_push(v, &v2);
	}

	ptr = bc_vec_item(map, i);
	if (new) ptr->name = xstrdup(e.name);
	return bc_vec_item(v, ptr->idx);
}

// 'num' need not be initialized on entry
static BC_STATUS zxc_program_num(BcResult *r, BcNum **num)
{
	switch (r->t) {
	case XC_RESULT_STR:
	case XC_RESULT_TEMP:
	IF_BC(case BC_RESULT_VOID:)
	case XC_RESULT_IBASE:
	case XC_RESULT_SCALE:
	case XC_RESULT_OBASE:
		*num = &r->d.n;
		break;
	case XC_RESULT_CONSTANT: {
		BcStatus s;
		char *str;
		size_t len;

		str = *xc_program_const(r->d.id.idx);
		len = strlen(str);

		bc_num_init(&r->d.n, len);

		s = zxc_num_parse(&r->d.n, str, G.prog.ib_t);
		if (s) {
			bc_num_free(&r->d.n);
			RETURN_STATUS(s);
		}
		*num = &r->d.n;
		r->t = XC_RESULT_TEMP;
		break;
	}
	case XC_RESULT_VAR:
	case XC_RESULT_ARRAY:
	case XC_RESULT_ARRAY_ELEM: {
		BcType type = (r->t == XC_RESULT_VAR) ? BC_TYPE_VAR : BC_TYPE_ARRAY;
		BcVec *v = xc_program_search(r->d.id.name, type);
		void *p = bc_vec_top(v);

		if (r->t == XC_RESULT_ARRAY_ELEM) {
			size_t idx = r->d.id.idx;

			v = p;
			if (v->size == sizeof(uint8_t))
				v = xc_program_dereference(v);
			//assert(v->size == sizeof(BcNum));
			if (v->len <= idx)
				bc_array_expand(v, idx + 1);
			*num = bc_vec_item(v, idx);
		} else {
			*num = p;
		}
		break;
	}
#if ENABLE_BC
	case BC_RESULT_LAST:
		*num = &G.prog.last;
		break;
	case BC_RESULT_ONE:
		*num = &G.prog.one;
		break;
#endif
#if SANITY_CHECKS
	default:
		// Testing the theory that dc does not reach LAST/ONE
		bb_error_msg_and_die("BUG:%d", r->t);
#endif
	}

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zxc_program_num(...) (zxc_program_num(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_program_binOpPrep(BcResult **l, BcNum **ln,
                                     BcResult **r, BcNum **rn, bool assign)
{
	BcStatus s;
	BcResultType lt, rt;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 1))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());

	*r = bc_vec_item_rev(&G.prog.results, 0);
	*l = bc_vec_item_rev(&G.prog.results, 1);

	s = zxc_program_num(*l, ln);
	if (s) RETURN_STATUS(s);
	s = zxc_program_num(*r, rn);
	if (s) RETURN_STATUS(s);

	lt = (*l)->t;
	rt = (*r)->t;

	// We run this again under these conditions in case any vector has been
	// reallocated out from under the BcNums or arrays we had.
	if (lt == rt && (lt == XC_RESULT_VAR || lt == XC_RESULT_ARRAY_ELEM)) {
		s = zxc_program_num(*l, ln);
		if (s) RETURN_STATUS(s);
	}

	if (!BC_PROG_NUM((*l), (*ln)) && (!assign || (*l)->t != XC_RESULT_VAR))
		RETURN_STATUS(bc_error_variable_is_wrong_type());
	if (!assign && !BC_PROG_NUM((*r), (*ln)))
		RETURN_STATUS(bc_error_variable_is_wrong_type());

	RETURN_STATUS(s);
}
#define zxc_program_binOpPrep(...) (zxc_program_binOpPrep(__VA_ARGS__) COMMA_SUCCESS)

static void xc_program_binOpRetire(BcResult *r)
{
	r->t = XC_RESULT_TEMP;
	bc_vec_pop(&G.prog.results);
	bc_result_pop_and_push(r);
}

// Note: *r and *n need not be initialized by caller
static BC_STATUS zxc_program_prep(BcResult **r, BcNum **n)
{
	BcStatus s;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	*r = bc_vec_top(&G.prog.results);

	s = zxc_program_num(*r, n);
	if (s) RETURN_STATUS(s);

	if (!BC_PROG_NUM((*r), (*n)))
		RETURN_STATUS(bc_error_variable_is_wrong_type());

	RETURN_STATUS(s);
}
#define zxc_program_prep(...) (zxc_program_prep(__VA_ARGS__) COMMA_SUCCESS)

static void xc_program_retire(BcResult *r, BcResultType t)
{
	r->t = t;
	bc_result_pop_and_push(r);
}

static BC_STATUS zxc_program_op(char inst)
{
	BcStatus s;
	BcResult *opd1, *opd2, res;
	BcNum *n1, *n2;

	s = zxc_program_binOpPrep(&opd1, &n1, &opd2, &n2, false);
	if (s) RETURN_STATUS(s);
	bc_num_init_DEF_SIZE(&res.d.n);

	s = BC_STATUS_SUCCESS;
	IF_ERROR_RETURN_POSSIBLE(s =) zxc_program_ops[inst - XC_INST_POWER](n1, n2, &res.d.n, G.prog.scale);
	if (s) goto err;
	xc_program_binOpRetire(&res);

	RETURN_STATUS(s);
 err:
	bc_num_free(&res.d.n);
	RETURN_STATUS(s);
}
#define zxc_program_op(...) (zxc_program_op(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_program_read(void)
{
	BcStatus s;
	BcParse sv_parse;
	BcVec buf;
	BcInstPtr ip;
	BcFunc *f;

	bc_char_vec_init(&buf);
	xc_read_line(&buf, stdin);

	f = xc_program_func(BC_PROG_READ);
	bc_vec_pop_all(&f->code);

	sv_parse = G.prs; // struct copy
	xc_parse_create(BC_PROG_READ);
	//G.err_line = G.prs.lex_line = 1; - not needed, error line info is not printed for read()

	s = zxc_parse_text_init(buf.v);
	if (s) goto exec_err;
	if (IS_BC) {
		IF_BC(s = zbc_parse_expr(0));
	} else {
		IF_DC(s = zdc_parse_exprs_until_eof());
	}
	if (s) goto exec_err;
	if (G.prs.lex != XC_LEX_NLINE && G.prs.lex != XC_LEX_EOF) {
		s = bc_error_at("bad read() expression");
		goto exec_err;
	}
	xc_parse_push(XC_INST_RET);

	ip.func = BC_PROG_READ;
	ip.inst_idx = 0;
	bc_vec_push(&G.prog.exestack, &ip);

 exec_err:
	xc_parse_free();
	G.prs = sv_parse; // struct copy
	bc_vec_free(&buf);
	RETURN_STATUS(s);
}
#define zxc_program_read(...) (zxc_program_read(__VA_ARGS__) COMMA_SUCCESS)

static void xc_program_printString(const char *str)
{
#if ENABLE_DC
	if (!str[0] && IS_DC) {
		// Example: echo '[]ap' | dc
		// should print two bytes: 0x00, 0x0A
		bb_putchar('\0');
		return;
	}
#endif
	while (*str) {
		char c = *str++;
		if (c == '\\') {
			static const char esc[] ALIGN1 = "nabfrt""e\\";
			char *n;

			c = *str++;
			n = strchr(esc, c); // note: if c is NUL, n = \0 at end of esc
			if (!n || !c) {
				// Just print the backslash and following character
				bb_putchar('\\');
				++G.prog.nchars;
				// But if we're at the end of the string, stop
				if (!c) break;
			} else {
				if (n - esc == 0) // "\n" ?
					G.prog.nchars = SIZE_MAX;
				c = "\n\a\b\f\r\t""\\\\""\\"[n - esc];
				//   n a b f r t   e \   \<end of line>
			}
		}
		putchar(c);
		++G.prog.nchars;
	}
}

static void bc_num_printNewline(void)
{
	if (G.prog.nchars == G.prog.len - 1) {
		bb_putchar('\\');
		bb_putchar('\n');
		G.prog.nchars = 0;
	}
}

#if ENABLE_DC
static FAST_FUNC void dc_num_printChar(size_t num, size_t width, bool radix)
{
	(void) radix;
	bb_putchar((char) num);
	G.prog.nchars += width;
}
#endif

static FAST_FUNC void bc_num_printDigits(size_t num, size_t width, bool radix)
{
	size_t exp, pow;

	bc_num_printNewline();
	bb_putchar(radix ? '.' : ' ');
	++G.prog.nchars;

	bc_num_printNewline();
	for (exp = 0, pow = 1; exp < width - 1; ++exp, pow *= 10)
		continue;

	for (exp = 0; exp < width; pow /= 10, ++G.prog.nchars, ++exp) {
		size_t dig;
		bc_num_printNewline();
		dig = num / pow;
		num -= dig * pow;
		bb_putchar(((char) dig) + '0');
	}
}

static FAST_FUNC void bc_num_printHex(size_t num, size_t width, bool radix)
{
	if (radix) {
		bc_num_printNewline();
		bb_putchar('.');
		G.prog.nchars++;
	}

	bc_num_printNewline();
	bb_putchar(bb_hexdigits_upcase[num]);
	G.prog.nchars += width;
}

static void bc_num_printDecimal(BcNum *n)
{
	size_t i, rdx = n->rdx - 1;

	if (n->neg) {
		bb_putchar('-');
		G.prog.nchars++;
	}

	for (i = n->len - 1; i < n->len; --i)
		bc_num_printHex((size_t) n->num[i], 1, i == rdx);
}

typedef void (*BcNumDigitOp)(size_t, size_t, bool) FAST_FUNC;

static BC_STATUS zxc_num_printNum(BcNum *n, unsigned base_t, size_t width, BcNumDigitOp print)
{
	BcStatus s;
	BcVec stack;
	BcNum base;
	BcDig base_digs[ULONG_NUM_BUFSIZE];
	BcNum intp, fracp, digit, frac_len;
	unsigned long dig, *ptr;
	size_t i;
	bool radix;

	if (n->len == 0) {
		print(0, width, false);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	bc_vec_init(&stack, sizeof(long), NULL);
	bc_num_init_and_copy(&intp, n);
	bc_num_init(&fracp, n->rdx);
	bc_num_init(&digit, width);
	bc_num_init(&frac_len, BC_NUM_INT(n));
	bc_num_one(&frac_len);
	base.cap = ARRAY_SIZE(base_digs);
	base.num = base_digs;
	bc_num_ulong2num(&base, base_t);

	bc_num_truncate(&intp, intp.rdx);
	s = zbc_num_sub(n, &intp, &fracp, 0);
	if (s) goto err;

	while (intp.len != 0) {
		s = zbc_num_divmod(&intp, &base, &intp, &digit, 0);
		if (s) goto err;
		s = zbc_num_ulong(&digit, &dig);
		if (s) goto err;
		bc_vec_push(&stack, &dig);
	}

	for (i = 0; i < stack.len; ++i) {
		ptr = bc_vec_item_rev(&stack, i);
		print(*ptr, width, false);
	}

	if (!n->rdx) goto err;

	for (radix = true; frac_len.len <= n->rdx; radix = false) {
		s = zbc_num_mul(&fracp, &base, &fracp, n->rdx);
		if (s) goto err;
		s = zbc_num_ulong(&fracp, &dig);
		if (s) goto err;
		bc_num_ulong2num(&intp, dig);
		s = zbc_num_sub(&fracp, &intp, &fracp, 0);
		if (s) goto err;
		print(dig, width, radix);
		s = zbc_num_mul(&frac_len, &base, &frac_len, 0);
		if (s) goto err;
	}
 err:
	bc_num_free(&frac_len);
	bc_num_free(&digit);
	bc_num_free(&fracp);
	bc_num_free(&intp);
	bc_vec_free(&stack);
	RETURN_STATUS(s);
}
#define zxc_num_printNum(...) (zxc_num_printNum(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_num_printBase(BcNum *n)
{
	BcStatus s;
	size_t width;
	BcNumDigitOp print;
	bool neg = n->neg;

	if (neg) {
		bb_putchar('-');
		G.prog.nchars++;
	}

	n->neg = false;

	if (G.prog.ob_t <= 16) {
		width = 1;
		print = bc_num_printHex;
	} else {
		unsigned i = G.prog.ob_t - 1;
		width = 0;
		for (;;) {
			width++;
			i /= 10;
			if (i == 0)
				break;
		}
		print = bc_num_printDigits;
	}

	s = zxc_num_printNum(n, G.prog.ob_t, width, print);
	n->neg = neg;

	RETURN_STATUS(s);
}
#define zxc_num_printBase(...) (zxc_num_printBase(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_num_print(BcNum *n, bool newline)
{
	BcStatus s = BC_STATUS_SUCCESS;

	bc_num_printNewline();

	if (n->len == 0) {
		bb_putchar('0');
		++G.prog.nchars;
	} else if (G.prog.ob_t == 10)
		bc_num_printDecimal(n);
	else
		s = zxc_num_printBase(n);

	if (newline) {
		bb_putchar('\n');
		G.prog.nchars = 0;
	}

	RETURN_STATUS(s);
}
#define zxc_num_print(...) (zxc_num_print(__VA_ARGS__) COMMA_SUCCESS)

#if !ENABLE_DC
// for bc, idx is always 0
#define xc_program_print(inst, idx) \
	xc_program_print(inst)
#endif
static BC_STATUS xc_program_print(char inst, size_t idx)
{
	BcStatus s;
	BcResult *r;
	BcNum *num;
	IF_NOT_DC(size_t idx = 0);

	if (!STACK_HAS_MORE_THAN(&G.prog.results, idx))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());

	r = bc_vec_item_rev(&G.prog.results, idx);
#if ENABLE_BC
	if (inst == XC_INST_PRINT && r->t == BC_RESULT_VOID)
		// void function's result on stack, ignore
		RETURN_STATUS(BC_STATUS_SUCCESS);
#endif
	s = zxc_program_num(r, &num);
	if (s) RETURN_STATUS(s);

	if (BC_PROG_NUM(r, num)) {
		s = zxc_num_print(num, /*newline:*/ inst == XC_INST_PRINT);
#if ENABLE_BC
		if (!s && IS_BC) bc_num_copy(&G.prog.last, num);
#endif
	} else {
		char *str;

		idx = (r->t == XC_RESULT_STR) ? r->d.id.idx : num->rdx;
		str = *xc_program_str(idx);

		if (inst == XC_INST_PRINT_STR) {
			char *nl;
			G.prog.nchars += printf("%s", str);
			nl = strrchr(str, '\n');
			if (nl)
				G.prog.nchars = strlen(nl + 1);
		} else {
			xc_program_printString(str);
			if (inst == XC_INST_PRINT)
				bb_putchar('\n');
		}
	}

	if (!s && inst != XC_INST_PRINT) bc_vec_pop(&G.prog.results);

	RETURN_STATUS(s);
}
#define zxc_program_print(...) (xc_program_print(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_program_negate(void)
{
	BcStatus s;
	BcResult res, *ptr;
	BcNum *num;

	s = zxc_program_prep(&ptr, &num);
	if (s) RETURN_STATUS(s);

	bc_num_init_and_copy(&res.d.n, num);
	if (res.d.n.len) res.d.n.neg = !res.d.n.neg;

	xc_program_retire(&res, XC_RESULT_TEMP);

	RETURN_STATUS(s);
}
#define zxc_program_negate(...) (zxc_program_negate(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_program_logical(char inst)
{
	BcStatus s;
	BcResult *opd1, *opd2, res;
	BcNum *n1, *n2;
	ssize_t cond;

	s = zxc_program_binOpPrep(&opd1, &n1, &opd2, &n2, false);
	if (s) RETURN_STATUS(s);

	bc_num_init_DEF_SIZE(&res.d.n);

	if (inst == XC_INST_BOOL_AND)
		cond = bc_num_cmp(n1, &G.prog.zero) && bc_num_cmp(n2, &G.prog.zero);
	else if (inst == XC_INST_BOOL_OR)
		cond = bc_num_cmp(n1, &G.prog.zero) || bc_num_cmp(n2, &G.prog.zero);
	else {
		cond = bc_num_cmp(n1, n2);
		switch (inst) {
		case XC_INST_REL_EQ:
			cond = (cond == 0);
			break;
		case XC_INST_REL_LE:
			cond = (cond <= 0);
			break;
		case XC_INST_REL_GE:
			cond = (cond >= 0);
			break;
		case XC_INST_REL_LT:
			cond = (cond < 0);
			break;
		case XC_INST_REL_GT:
			cond = (cond > 0);
			break;
		default: // = case XC_INST_REL_NE:
			//cond = (cond != 0); - not needed
			break;
		}
	}

	if (cond) bc_num_one(&res.d.n);
	//else bc_num_zero(&res.d.n); - already is

	xc_program_binOpRetire(&res);

	RETURN_STATUS(s);
}
#define zxc_program_logical(...) (zxc_program_logical(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_DC
static BC_STATUS zdc_program_assignStr(BcResult *r, BcVec *v, bool push)
{
	BcNum n2;
	BcResult res;

	memset(&n2, 0, sizeof(BcNum));
	n2.rdx = res.d.id.idx = r->d.id.idx;
	res.t = XC_RESULT_STR;

	if (!push) {
		if (!STACK_HAS_MORE_THAN(&G.prog.results, 1))
			RETURN_STATUS(bc_error_stack_has_too_few_elements());
		bc_vec_pop(v);
		bc_vec_pop(&G.prog.results);
	}

	bc_result_pop_and_push(&res);
	bc_vec_push(v, &n2);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zdc_program_assignStr(...) (zdc_program_assignStr(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_DC

static BC_STATUS zxc_program_popResultAndCopyToVar(char *name, BcType t)
{
	BcStatus s;
	BcResult *ptr, r;
	BcVec *vec;
	BcNum *n;
	bool var = (t == BC_TYPE_VAR);

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());

	ptr = bc_vec_top(&G.prog.results);
	if ((ptr->t == XC_RESULT_ARRAY) == var)
		RETURN_STATUS(bc_error_variable_is_wrong_type());
	vec = xc_program_search(name, t);

#if ENABLE_DC
	if (ptr->t == XC_RESULT_STR) {
		if (!var)
			RETURN_STATUS(bc_error_variable_is_wrong_type());
		RETURN_STATUS(zdc_program_assignStr(ptr, vec, true));
	}
#endif

	s = zxc_program_num(ptr, &n);
	if (s) RETURN_STATUS(s);

	// Do this once more to make sure that pointers were not invalidated.
	vec = xc_program_search(name, t);

	if (var) {
		bc_num_init_DEF_SIZE(&r.d.n);
		bc_num_copy(&r.d.n, n);
	} else {
		BcVec *v = (BcVec*) n;
		bool ref, ref_size;

		ref = (v->size == sizeof(BcVec) && t != BC_TYPE_ARRAY);
		ref_size = (v->size == sizeof(uint8_t));

		if (ref || (ref_size && t == BC_TYPE_REF)) {
			bc_vec_init(&r.d.v, sizeof(uint8_t), NULL);
			if (ref) {
				size_t vidx, idx;
				BcId id;

				id.name = ptr->d.id.name;
				v = xc_program_search(ptr->d.id.name, BC_TYPE_REF);

				// Make sure the pointer was not invalidated.
				vec = xc_program_search(name, t);

				vidx = bc_map_find_exact(&G.prog.arr_map, &id);
				//assert(vidx != BC_VEC_INVALID_IDX);
				vidx = ((BcId*) bc_vec_item(&G.prog.arr_map, vidx))->idx;
				idx = v->len - 1;

				bc_vec_pushIndex(&r.d.v, vidx);
				bc_vec_pushIndex(&r.d.v, idx);
			}
			// If we get here, we are copying a ref to a ref.
			else bc_vec_npush(&r.d.v, v->len, v->v);

			// We need to return early.
			goto ret;
		}

		if (ref_size && t != BC_TYPE_REF)
			v = xc_program_dereference(v);

		bc_array_init(&r.d.v, true);
		bc_array_copy(&r.d.v, v);
	}
 ret:
	bc_vec_push(vec, &r.d);
	bc_vec_pop(&G.prog.results);

	RETURN_STATUS(s);
}
#define zxc_program_popResultAndCopyToVar(...) (zxc_program_popResultAndCopyToVar(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_program_assign(char inst)
{
	BcStatus s;
	BcResult *left, *right, res;
	BcNum *l, *r;
	bool assign = (inst == XC_INST_ASSIGN);
	bool ib, sc;

	s = zxc_program_binOpPrep(&left, &l, &right, &r, assign);
	if (s) RETURN_STATUS(s);

	ib = left->t == XC_RESULT_IBASE;
	sc = left->t == XC_RESULT_SCALE;

#if ENABLE_DC
	if (right->t == XC_RESULT_STR) {
		BcVec *v;

		if (left->t != XC_RESULT_VAR)
			RETURN_STATUS(bc_error_variable_is_wrong_type());
		v = xc_program_search(left->d.id.name, BC_TYPE_VAR);

		RETURN_STATUS(zdc_program_assignStr(right, v, false));
	}
#endif

	if (left->t == XC_RESULT_CONSTANT
	 || left->t == XC_RESULT_TEMP
	IF_BC(|| left->t == BC_RESULT_VOID)
	) {
		RETURN_STATUS(bc_error_bad_assignment());
	}

#if ENABLE_BC
	if (assign)
		bc_num_copy(l, r);
	else {
		s = BC_STATUS_SUCCESS;
		IF_ERROR_RETURN_POSSIBLE(s =) zxc_program_ops[inst - BC_INST_ASSIGN_POWER](l, r, l, G.prog.scale);
	}
	if (s) RETURN_STATUS(s);
#else
	bc_num_copy(l, r);
#endif

	if (ib || sc || left->t == XC_RESULT_OBASE) {
		static const char *const msg[] ALIGN_PTR = {
			"bad ibase; must be [2,16]",                 //XC_RESULT_IBASE
			"bad obase; must be [2,"BC_MAX_OBASE_STR"]", //XC_RESULT_OBASE
			"bad scale; must be [0,"BC_MAX_SCALE_STR"]", //XC_RESULT_SCALE
		};
		size_t *ptr;
		size_t max;
		unsigned long val;

		s = zbc_num_ulong(l, &val);
		if (s) RETURN_STATUS(s);
		s = left->t - XC_RESULT_IBASE;
		if (sc) {
			max = BC_MAX_SCALE;
			ptr = &G.prog.scale;
		} else {
			if (val < 2)
				RETURN_STATUS(bc_error(msg[s]));
			max = ib ? BC_NUM_MAX_IBASE : BC_MAX_OBASE;
			ptr = ib ? &G.prog.ib_t : &G.prog.ob_t;
		}

		if (val > max)
			RETURN_STATUS(bc_error(msg[s]));

		*ptr = (size_t) val;
		s = BC_STATUS_SUCCESS;
	}

	bc_num_init_and_copy(&res.d.n, l);
	xc_program_binOpRetire(&res);

	RETURN_STATUS(s);
}
#define zxc_program_assign(...) (zxc_program_assign(__VA_ARGS__) COMMA_SUCCESS)

#if !ENABLE_DC
#define xc_program_pushVar(code, bgn, pop, copy) \
	xc_program_pushVar(code, bgn)
// for bc, 'pop' and 'copy' are always false
#endif
static BC_STATUS xc_program_pushVar(char *code, size_t *bgn,
                                   bool pop, bool copy)
{
	BcResult r;
	char *name = xc_program_name(code, bgn);

	r.t = XC_RESULT_VAR;
	r.d.id.name = name;

#if ENABLE_DC
	if (pop || copy) {
		BcVec *v = xc_program_search(name, BC_TYPE_VAR);
		BcNum *num = bc_vec_top(v);

		free(name);
		if (!STACK_HAS_MORE_THAN(v, 1 - copy)) {
			RETURN_STATUS(bc_error_stack_has_too_few_elements());
		}

		if (!BC_PROG_STR(num)) {
			r.t = XC_RESULT_TEMP;
			bc_num_init_DEF_SIZE(&r.d.n);
			bc_num_copy(&r.d.n, num);
		} else {
			r.t = XC_RESULT_STR;
			r.d.id.idx = num->rdx;
		}

		if (!copy) bc_vec_pop(v);
	}
#endif // ENABLE_DC

	bc_vec_push(&G.prog.results, &r);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zxc_program_pushVar(...) (xc_program_pushVar(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_pushArray(char *code, size_t *bgn, char inst)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcResult r;
	BcNum *num;

	r.d.id.name = xc_program_name(code, bgn);

	if (inst == XC_INST_ARRAY) {
		r.t = XC_RESULT_ARRAY;
		bc_vec_push(&G.prog.results, &r);
	} else {
		BcResult *operand;
		unsigned long temp;

		s = zxc_program_prep(&operand, &num);
		if (s) goto err;
		s = zbc_num_ulong(num, &temp);
		if (s) goto err;

		if (temp > BC_MAX_DIM) {
			s = bc_error("array too long; must be [1,"BC_MAX_DIM_STR"]");
			goto err;
		}

		r.d.id.idx = (size_t) temp;
		xc_program_retire(&r, XC_RESULT_ARRAY_ELEM);
	}
 err:
	if (s) free(r.d.id.name);
	RETURN_STATUS(s);
}
#define zbc_program_pushArray(...) (zbc_program_pushArray(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_BC
static BC_STATUS zbc_program_incdec(char inst)
{
	BcStatus s;
	BcResult *ptr, res, copy;
	BcNum *num;
	char inst2 = inst;

	s = zxc_program_prep(&ptr, &num);
	if (s) RETURN_STATUS(s);

	if (inst == BC_INST_INC_POST || inst == BC_INST_DEC_POST) {
		copy.t = XC_RESULT_TEMP;
		bc_num_init_and_copy(&copy.d.n, num);
	}

	res.t = BC_RESULT_ONE;
	inst = (inst == BC_INST_INC_PRE || inst == BC_INST_INC_POST)
			? BC_INST_ASSIGN_PLUS
			: BC_INST_ASSIGN_MINUS;

	bc_vec_push(&G.prog.results, &res);
	s = zxc_program_assign(inst);
	if (s) RETURN_STATUS(s);

	if (inst2 == BC_INST_INC_POST || inst2 == BC_INST_DEC_POST) {
		bc_result_pop_and_push(&copy);
	}

	RETURN_STATUS(s);
}
#define zbc_program_incdec(...) (zbc_program_incdec(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_call(char *code, size_t *idx)
{
	BcInstPtr ip;
	size_t i, nparams;
	BcId *a;
	BcFunc *func;

	nparams = xc_program_index(code, idx);
	ip.func = xc_program_index(code, idx);
	func = xc_program_func(ip.func);

	if (func->code.len == 0) {
		RETURN_STATUS(bc_error("undefined function"));
	}
	if (nparams != func->nparams) {
		RETURN_STATUS(bc_error_fmt("function has %u parameters, but called with %u", func->nparams, nparams));
	}
	ip.inst_idx = 0;

	for (i = 0; i < nparams; ++i) {
		BcResult *arg;
		BcStatus s;
		bool arr;

		a = bc_vec_item(&func->autos, nparams - 1 - i);
		arg = bc_vec_top(&G.prog.results);

		arr = (a->idx == BC_TYPE_ARRAY || a->idx == BC_TYPE_REF);

		if (arr != (arg->t == XC_RESULT_ARRAY) // array/variable mismatch
		// || arg->t == XC_RESULT_STR - impossible, f("str") is not a legal syntax (strings are not bc expressions)
		) {
			RETURN_STATUS(bc_error_variable_is_wrong_type());
		}
		s = zxc_program_popResultAndCopyToVar(a->name, (BcType) a->idx);
		if (s) RETURN_STATUS(s);
	}

	a = bc_vec_item(&func->autos, i);
	for (; i < func->autos.len; i++, a++) {
		BcVec *v;

		v = xc_program_search(a->name, (BcType) a->idx);
		if (a->idx == BC_TYPE_VAR) {
			BcNum n2;
			bc_num_init_DEF_SIZE(&n2);
			bc_vec_push(v, &n2);
		} else {
			//assert(a->idx == BC_TYPE_ARRAY);
			BcVec v2;
			bc_array_init(&v2, true);
			bc_vec_push(v, &v2);
		}
	}

	bc_vec_push(&G.prog.exestack, &ip);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_program_call(...) (zbc_program_call(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_return(char inst)
{
	BcResult res;
	BcFunc *f;
	BcId *a;
	size_t i;
	BcInstPtr *ip = bc_vec_top(&G.prog.exestack);

	f = xc_program_func(ip->func);

	res.t = XC_RESULT_TEMP;
	if (inst == XC_INST_RET) {
		// bc needs this for e.g. RESULT_CONSTANT ("return 5")
		// because bc constants are per-function.
		// TODO: maybe avoid if value is already RESULT_TEMP?
		BcStatus s;
		BcNum *num;
		BcResult *operand = bc_vec_top(&G.prog.results);

		s = zxc_program_num(operand, &num);
		if (s) RETURN_STATUS(s);
		bc_num_init_and_copy(&res.d.n, num);
		bc_vec_pop(&G.prog.results);
	} else {
		if (f->voidfunc)
			res.t = BC_RESULT_VOID;
		bc_num_init_DEF_SIZE(&res.d.n);
		//bc_num_zero(&res.d.n); - already is
	}
	bc_vec_push(&G.prog.results, &res);

	bc_vec_pop(&G.prog.exestack);

	// We need to pop arguments as well, so this takes that into account.
	a = (void*)f->autos.v;
	for (i = 0; i < f->autos.len; i++, a++) {
		BcVec *v;
		v = xc_program_search(a->name, (BcType) a->idx);
		bc_vec_pop(v);
	}

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_program_return(...) (zbc_program_return(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_BC

static unsigned long xc_program_scale(BcNum *n)
{
	return (unsigned long) n->rdx;
}

static unsigned long xc_program_len(BcNum *n)
{
	size_t len = n->len;

	if (n->rdx != len)
		// length(100): rdx 0 len 3, return 3
		// length(0.01-0.01): rdx 2 len 0, return 2
		// dc: 0.01 0.01 - Zp: rdx 2 len 0, return 1
		return len != 0 ? len : (IS_BC ? n->rdx : 1);

	// length(0): return 1
	// length(0.000nnn): count nnn
	for (;;) {
		if (len == 0) break;
		len--;
		if (n->num[len] != 0) break;
	}
	return len + 1;
}

static BC_STATUS zxc_program_builtin(char inst)
{
	BcStatus s;
	BcResult *opnd;
	BcNum *num;
	BcResult res;
	bool len = (inst == XC_INST_LENGTH);

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	opnd = bc_vec_top(&G.prog.results);

	s = zxc_program_num(opnd, &num);
	if (s) RETURN_STATUS(s);

#if ENABLE_DC
	if (!BC_PROG_NUM(opnd, num) && !len)
		RETURN_STATUS(bc_error_variable_is_wrong_type());
#endif

	bc_num_init_DEF_SIZE(&res.d.n);

	if (inst == XC_INST_SQRT)
		s = zbc_num_sqrt(num, &res.d.n, G.prog.scale);
#if ENABLE_BC
	else if (len && opnd->t == XC_RESULT_ARRAY) {
		bc_num_ulong2num(&res.d.n, (unsigned long) ((BcVec *) num)->len);
	}
#endif
#if ENABLE_DC
	else if (len && !BC_PROG_NUM(opnd, num)) {
		char **str;
		size_t idx = opnd->t == XC_RESULT_STR ? opnd->d.id.idx : num->rdx;

		str = xc_program_str(idx);
		bc_num_ulong2num(&res.d.n, strlen(*str));
	}
#endif
	else {
//TODO: length(.00) and scale(.00) should return 2, they return 1 and 0 now
//(don't forget to check that dc Z and X commands do not break)
		bc_num_ulong2num(&res.d.n, len ? xc_program_len(num) : xc_program_scale(num));
	}

	xc_program_retire(&res, XC_RESULT_TEMP);

	RETURN_STATUS(s);
}
#define zxc_program_builtin(...) (zxc_program_builtin(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_DC
static BC_STATUS zdc_program_divmod(void)
{
	BcStatus s;
	BcResult *opd1, *opd2, res, res2;
	BcNum *n1, *n2;

	s = zxc_program_binOpPrep(&opd1, &n1, &opd2, &n2, false);
	if (s) RETURN_STATUS(s);

	bc_num_init_DEF_SIZE(&res.d.n);
	bc_num_init(&res2.d.n, n2->len);

	s = zbc_num_divmod(n1, n2, &res2.d.n, &res.d.n, G.prog.scale);
	if (s) goto err;

	xc_program_binOpRetire(&res2);
	res.t = XC_RESULT_TEMP;
	bc_vec_push(&G.prog.results, &res);

	RETURN_STATUS(s);
 err:
	bc_num_free(&res2.d.n);
	bc_num_free(&res.d.n);
	RETURN_STATUS(s);
}
#define zdc_program_divmod(...) (zdc_program_divmod(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_program_modexp(void)
{
	BcStatus s;
	BcResult *r1, *r2, *r3, res;
	BcNum *n1, *n2, *n3;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 2))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	s = zxc_program_binOpPrep(&r2, &n2, &r3, &n3, false);
	if (s) RETURN_STATUS(s);

	r1 = bc_vec_item_rev(&G.prog.results, 2);
	s = zxc_program_num(r1, &n1);
	if (s) RETURN_STATUS(s);
	if (!BC_PROG_NUM(r1, n1))
		RETURN_STATUS(bc_error_variable_is_wrong_type());

	// Make sure that the values have their pointers updated, if necessary.
	if (r1->t == XC_RESULT_VAR || r1->t == XC_RESULT_ARRAY_ELEM) {
		if (r1->t == r2->t) {
			s = zxc_program_num(r2, &n2);
			if (s) RETURN_STATUS(s);
		}
		if (r1->t == r3->t) {
			s = zxc_program_num(r3, &n3);
			if (s) RETURN_STATUS(s);
		}
	}

	bc_num_init(&res.d.n, n3->len);
	s = zdc_num_modexp(n1, n2, n3, &res.d.n);
	if (s) goto err;

	bc_vec_pop(&G.prog.results);
	xc_program_binOpRetire(&res);

	RETURN_STATUS(s);
 err:
	bc_num_free(&res.d.n);
	RETURN_STATUS(s);
}
#define zdc_program_modexp(...) (zdc_program_modexp(__VA_ARGS__) COMMA_SUCCESS)

static void dc_program_stackLen(void)
{
	BcResult res;
	size_t len = G.prog.results.len;

	res.t = XC_RESULT_TEMP;

	bc_num_init_DEF_SIZE(&res.d.n);
	bc_num_ulong2num(&res.d.n, len);
	bc_vec_push(&G.prog.results, &res);
}

static BC_STATUS zdc_program_asciify(void)
{
	BcStatus s;
	BcResult *r, res;
	BcNum *num, n;
	char **strs;
	char *str;
	char c;
	size_t idx;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());

	r = bc_vec_top(&G.prog.results);
	s = zxc_program_num(r, &num);
	if (s) RETURN_STATUS(s);

	if (BC_PROG_NUM(r, num)) {
		unsigned long val;
		BcNum strmb;
		BcDig strmb_digs[ULONG_NUM_BUFSIZE];

		bc_num_init_DEF_SIZE(&n);
		bc_num_copy(&n, num);
		bc_num_truncate(&n, n.rdx);

		strmb.cap = ARRAY_SIZE(strmb_digs);
		strmb.num = strmb_digs;
		bc_num_ulong2num(&strmb, 0x100);

		s = zbc_num_mod(&n, &strmb, &n, 0);
		if (s) goto num_err;
		s = zbc_num_ulong(&n, &val);
		if (s) goto num_err;

		c = (char) val;

		bc_num_free(&n);
	} else {
		char *sp;
		idx = (r->t == XC_RESULT_STR) ? r->d.id.idx : num->rdx;
		sp = *xc_program_str(idx);
		c = sp[0];
	}

	strs = (void*)G.prog.strs.v;
	for (idx = 0; idx < G.prog.strs.len; idx++) {
		if (strs[idx][0] == c && strs[idx][1] == '\0') {
			goto dup;
		}
	}
	str = xzalloc(2);
	str[0] = c;
	//str[1] = '\0'; - already is
	idx = bc_vec_push(&G.prog.strs, &str);
	// Add an empty function so that if zdc_program_execStr ever needs to
	// parse the string into code (from the 'x' command) there's somewhere
	// to store the bytecode.
	xc_program_add_fn();
 dup:
	res.t = XC_RESULT_STR;
	res.d.id.idx = idx;
	bc_result_pop_and_push(&res);

	RETURN_STATUS(BC_STATUS_SUCCESS);
 num_err:
	bc_num_free(&n);
	RETURN_STATUS(s);
}
#define zdc_program_asciify(...) (zdc_program_asciify(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_program_printStream(void)
{
	BcStatus s;
	BcResult *r;
	BcNum *n;
	size_t idx;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	r = bc_vec_top(&G.prog.results);

	s = zxc_program_num(r, &n);
	if (s) RETURN_STATUS(s);

	if (BC_PROG_NUM(r, n)) {
		s = zxc_num_printNum(n, 0x100, 1, dc_num_printChar);
	} else {
		char *str;
		idx = (r->t == XC_RESULT_STR) ? r->d.id.idx : n->rdx;
		str = *xc_program_str(idx);
		fputs_stdout(str);
	}

	RETURN_STATUS(s);
}
#define zdc_program_printStream(...) (zdc_program_printStream(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_program_nquit(void)
{
	BcStatus s;
	BcResult *opnd;
	BcNum *num;
	unsigned long val;

	s = zxc_program_prep(&opnd, &num);
	if (s) RETURN_STATUS(s);
	s = zbc_num_ulong(num, &val);
	if (s) RETURN_STATUS(s);

	bc_vec_pop(&G.prog.results);

	if (G.prog.exestack.len < val)
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	if (G.prog.exestack.len == val) {
		QUIT_OR_RETURN_TO_MAIN;
	}

	bc_vec_npop(&G.prog.exestack, val);

	RETURN_STATUS(s);
}
#define zdc_program_nquit(...) (zdc_program_nquit(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_program_execStr(char *code, size_t *bgn, bool cond)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcResult *r;
	BcFunc *f;
	BcInstPtr ip;
	size_t fidx, sidx;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());

	r = bc_vec_top(&G.prog.results);

	if (cond) {
		BcNum *n = n; // for compiler
		bool exec;
		char *name;
		char *then_name = xc_program_name(code, bgn);
		char *else_name = NULL;

		if (code[*bgn] == '\0')
			(*bgn) += 1;
		else
			else_name = xc_program_name(code, bgn);

		exec = r->d.n.len != 0;
		name = then_name;
		if (!exec && else_name != NULL) {
			exec = true;
			name = else_name;
		}

		if (exec) {
			BcVec *v;
			v = xc_program_search(name, BC_TYPE_VAR);
			n = bc_vec_top(v);
		}

		free(then_name);
		free(else_name);

		if (!exec) goto exit;
		if (!BC_PROG_STR(n)) {
			s = bc_error_variable_is_wrong_type();
			goto exit;
		}

		sidx = n->rdx;
	} else {
		if (r->t == XC_RESULT_STR) {
			sidx = r->d.id.idx;
		} else if (r->t == XC_RESULT_VAR) {
			BcNum *n;
			s = zxc_program_num(r, &n);
			if (s || !BC_PROG_STR(n)) goto exit;
			sidx = n->rdx;
		} else
			goto exit_nopop;
	}

	fidx = sidx + BC_PROG_REQ_FUNCS;

	f = xc_program_func(fidx);

	if (f->code.len == 0) {
		BcParse sv_parse;
		char *str;

		sv_parse = G.prs; // struct copy
		xc_parse_create(fidx);
		str = *xc_program_str(sidx);
		s = zxc_parse_text_init(str);
		if (s) goto err;

		s = zdc_parse_exprs_until_eof();
		if (s) goto err;
		xc_parse_push(DC_INST_POP_EXEC);
		if (G.prs.lex != XC_LEX_EOF)
			s = bc_error_bad_expression();
		xc_parse_free();
		G.prs = sv_parse; // struct copy
		if (s) {
 err:
			bc_vec_pop_all(&f->code);
			goto exit;
		}
	}

	ip.inst_idx = 0;
	ip.func = fidx;

	bc_vec_pop(&G.prog.results);
	bc_vec_push(&G.prog.exestack, &ip);

	RETURN_STATUS(BC_STATUS_SUCCESS);
 exit:
	bc_vec_pop(&G.prog.results);
 exit_nopop:
	RETURN_STATUS(s);
}
#define zdc_program_execStr(...) (zdc_program_execStr(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_DC

static void xc_program_pushGlobal(char inst)
{
	BcResult res;
	unsigned long val;

	res.t = inst - XC_INST_IBASE + XC_RESULT_IBASE;
	if (inst == XC_INST_IBASE)
		val = (unsigned long) G.prog.ib_t;
	else if (inst == XC_INST_SCALE)
		val = (unsigned long) G.prog.scale;
	else
		val = (unsigned long) G.prog.ob_t;

	bc_num_init_DEF_SIZE(&res.d.n);
	bc_num_ulong2num(&res.d.n, val);
	bc_vec_push(&G.prog.results, &res);
}

static BC_STATUS zxc_program_exec(void)
{
	BcResult r, *ptr;
	BcInstPtr *ip = bc_vec_top(&G.prog.exestack);
	BcFunc *func = xc_program_func(ip->func);
	char *code = func->code.v;

	dbg_exec("func:%zd bytes:%zd ip:%zd results.len:%d",
			ip->func, func->code.len, ip->inst_idx, G.prog.results.len);
	while (ip->inst_idx < func->code.len) {
		BcStatus s = BC_STATUS_SUCCESS;
		char inst = code[ip->inst_idx++];

		dbg_exec("inst at %zd:%d results.len:%d", ip->inst_idx - 1, inst, G.prog.results.len);
		switch (inst) {
		case XC_INST_RET:
			if (IS_DC) { // end of '?' reached
				bc_vec_pop(&G.prog.exestack);
				goto read_updated_ip;
			}
			// bc: fall through
#if ENABLE_BC
		case BC_INST_RET0:
			dbg_exec("BC_INST_RET[0]:");
			s = zbc_program_return(inst);
			goto read_updated_ip;
		case BC_INST_JUMP_ZERO: {
			BcNum *num;
			bool zero;
			dbg_exec("BC_INST_JUMP_ZERO:");
			s = zxc_program_prep(&ptr, &num);
			if (s) RETURN_STATUS(s);
			zero = (bc_num_cmp(num, &G.prog.zero) == 0);
			bc_vec_pop(&G.prog.results);
			if (!zero) {
				xc_program_index(code, &ip->inst_idx);
				break;
			}
			// else: fall through
		}
		case BC_INST_JUMP: {
			size_t idx = xc_program_index(code, &ip->inst_idx);
			size_t *addr = bc_vec_item(&func->labels, idx);
			dbg_exec("BC_INST_JUMP: to %ld", (long)*addr);
			ip->inst_idx = *addr;
			break;
		}
		case BC_INST_CALL:
			dbg_exec("BC_INST_CALL:");
			s = zbc_program_call(code, &ip->inst_idx);
			goto read_updated_ip;
		case BC_INST_INC_PRE:
		case BC_INST_DEC_PRE:
		case BC_INST_INC_POST:
		case BC_INST_DEC_POST:
			dbg_exec("BC_INST_INCDEC:");
			s = zbc_program_incdec(inst);
			break;
		case BC_INST_HALT:
			dbg_exec("BC_INST_HALT:");
			QUIT_OR_RETURN_TO_MAIN;
			break;
		case XC_INST_BOOL_OR:
		case XC_INST_BOOL_AND:
#endif // ENABLE_BC
		case XC_INST_REL_EQ:
		case XC_INST_REL_LE:
		case XC_INST_REL_GE:
		case XC_INST_REL_NE:
		case XC_INST_REL_LT:
		case XC_INST_REL_GT:
			dbg_exec("BC_INST_BOOL:");
			s = zxc_program_logical(inst);
			break;
		case XC_INST_READ:
			dbg_exec("XC_INST_READ:");
			s = zxc_program_read();
			goto read_updated_ip;
		case XC_INST_VAR:
			dbg_exec("XC_INST_VAR:");
			s = zxc_program_pushVar(code, &ip->inst_idx, false, false);
			break;
		case XC_INST_ARRAY_ELEM:
		case XC_INST_ARRAY:
			dbg_exec("XC_INST_ARRAY[_ELEM]:");
			s = zbc_program_pushArray(code, &ip->inst_idx, inst);
			break;
#if ENABLE_BC
		case BC_INST_LAST:
			dbg_exec("BC_INST_LAST:");
			r.t = BC_RESULT_LAST;
			bc_vec_push(&G.prog.results, &r);
			break;
#endif
		case XC_INST_IBASE:
		case XC_INST_OBASE:
		case XC_INST_SCALE:
			dbg_exec("XC_INST_internalvar(%d):", inst - XC_INST_IBASE);
			xc_program_pushGlobal(inst);
			break;
		case XC_INST_SCALE_FUNC:
		case XC_INST_LENGTH:
		case XC_INST_SQRT:
			dbg_exec("BC_INST_builtin:");
			s = zxc_program_builtin(inst);
			break;
		case XC_INST_NUM:
			dbg_exec("XC_INST_NUM:");
			r.t = XC_RESULT_CONSTANT;
			r.d.id.idx = xc_program_index(code, &ip->inst_idx);
			bc_vec_push(&G.prog.results, &r);
			break;
		case XC_INST_POP:
			dbg_exec("XC_INST_POP:");
			if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
				s = bc_error_stack_has_too_few_elements();
			else
				bc_vec_pop(&G.prog.results);
			break;
		case XC_INST_PRINT:
		case XC_INST_PRINT_POP:
		case XC_INST_PRINT_STR:
			dbg_exec("XC_INST_PRINTxyz(%d):", inst - XC_INST_PRINT);
			s = zxc_program_print(inst, 0);
			break;
		case XC_INST_STR:
			dbg_exec("XC_INST_STR:");
			r.t = XC_RESULT_STR;
			r.d.id.idx = xc_program_index(code, &ip->inst_idx);
			bc_vec_push(&G.prog.results, &r);
			break;
		case XC_INST_POWER:
		case XC_INST_MULTIPLY:
		case XC_INST_DIVIDE:
		case XC_INST_MODULUS:
		case XC_INST_PLUS:
		case XC_INST_MINUS:
			dbg_exec("BC_INST_binaryop:");
			s = zxc_program_op(inst);
			break;
		case XC_INST_BOOL_NOT: {
			BcNum *num;
			dbg_exec("XC_INST_BOOL_NOT:");
			s = zxc_program_prep(&ptr, &num);
			if (s) RETURN_STATUS(s);
			bc_num_init_DEF_SIZE(&r.d.n);
			if (bc_num_cmp(num, &G.prog.zero) == 0)
				bc_num_one(&r.d.n);
			//else bc_num_zero(&r.d.n); - already is
			xc_program_retire(&r, XC_RESULT_TEMP);
			break;
		}
		case XC_INST_NEG:
			dbg_exec("XC_INST_NEG:");
			s = zxc_program_negate();
			break;
#if ENABLE_BC
		case BC_INST_ASSIGN_POWER:
		case BC_INST_ASSIGN_MULTIPLY:
		case BC_INST_ASSIGN_DIVIDE:
		case BC_INST_ASSIGN_MODULUS:
		case BC_INST_ASSIGN_PLUS:
		case BC_INST_ASSIGN_MINUS:
#endif
		case XC_INST_ASSIGN:
			dbg_exec("BC_INST_ASSIGNxyz:");
			s = zxc_program_assign(inst);
			break;
#if ENABLE_DC
		case DC_INST_POP_EXEC:
			dbg_exec("DC_INST_POP_EXEC:");
			bc_vec_pop(&G.prog.exestack);
			goto read_updated_ip;
		case DC_INST_MODEXP:
			dbg_exec("DC_INST_MODEXP:");
			s = zdc_program_modexp();
			break;
		case DC_INST_DIVMOD:
			dbg_exec("DC_INST_DIVMOD:");
			s = zdc_program_divmod();
			break;
		case DC_INST_EXECUTE:
		case DC_INST_EXEC_COND:
			dbg_exec("DC_INST_EXEC[_COND]:");
			s = zdc_program_execStr(code, &ip->inst_idx, inst == DC_INST_EXEC_COND);
			goto read_updated_ip;
		case DC_INST_PRINT_STACK: {
			size_t idx;
			dbg_exec("DC_INST_PRINT_STACK:");
			for (idx = 0; idx < G.prog.results.len; ++idx) {
				s = zxc_program_print(XC_INST_PRINT, idx);
				if (s) break;
			}
			break;
		}
		case DC_INST_CLEAR_STACK:
			dbg_exec("DC_INST_CLEAR_STACK:");
			bc_vec_pop_all(&G.prog.results);
			break;
		case DC_INST_STACK_LEN:
			dbg_exec("DC_INST_STACK_LEN:");
			dc_program_stackLen();
			break;
		case DC_INST_DUPLICATE:
			dbg_exec("DC_INST_DUPLICATE:");
			if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
				RETURN_STATUS(bc_error_stack_has_too_few_elements());
			ptr = bc_vec_top(&G.prog.results);
			dc_result_copy(&r, ptr);
			bc_vec_push(&G.prog.results, &r);
			break;
		case DC_INST_SWAP: {
			BcResult *ptr2;
			dbg_exec("DC_INST_SWAP:");
			if (!STACK_HAS_MORE_THAN(&G.prog.results, 1))
				RETURN_STATUS(bc_error_stack_has_too_few_elements());
			ptr = bc_vec_item_rev(&G.prog.results, 0);
			ptr2 = bc_vec_item_rev(&G.prog.results, 1);
			memcpy(&r, ptr, sizeof(BcResult));
			memcpy(ptr, ptr2, sizeof(BcResult));
			memcpy(ptr2, &r, sizeof(BcResult));
			break;
		}
		case DC_INST_ASCIIFY:
			dbg_exec("DC_INST_ASCIIFY:");
			s = zdc_program_asciify();
			break;
		case DC_INST_PRINT_STREAM:
			dbg_exec("DC_INST_PRINT_STREAM:");
			s = zdc_program_printStream();
			break;
		case DC_INST_LOAD:
		case DC_INST_PUSH_VAR: {
			bool copy = inst == DC_INST_LOAD;
			s = zxc_program_pushVar(code, &ip->inst_idx, true, copy);
			break;
		}
		case DC_INST_PUSH_TO_VAR: {
			char *name = xc_program_name(code, &ip->inst_idx);
			s = zxc_program_popResultAndCopyToVar(name, BC_TYPE_VAR);
			free(name);
			break;
		}
		case DC_INST_QUIT:
			dbg_exec("DC_INST_QUIT:");
			if (G.prog.exestack.len <= 2)
				QUIT_OR_RETURN_TO_MAIN;
			bc_vec_npop(&G.prog.exestack, 2);
			goto read_updated_ip;
		case DC_INST_NQUIT:
			dbg_exec("DC_INST_NQUIT:");
			s = zdc_program_nquit();
			//goto read_updated_ip; - just fall through to it
#endif // ENABLE_DC
 read_updated_ip:
			// Instruction stack has changed, read new pointers
			ip = bc_vec_top(&G.prog.exestack);
			func = xc_program_func(ip->func);
			code = func->code.v;
			dbg_exec("func:%zd bytes:%zd ip:%zd", ip->func, func->code.len, ip->inst_idx);
		}

		if (s || G_interrupt) {
			xc_program_reset();
			RETURN_STATUS(s);
		}

		fflush_and_check();
	}

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zxc_program_exec(...) (zxc_program_exec(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_vm_process(const char *text)
{
	BcStatus s;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = zxc_parse_text_init(text); // does the first zxc_lex_next()
	if (s) RETURN_STATUS(s);

	while (G.prs.lex != XC_LEX_EOF) {
		BcInstPtr *ip;
		BcFunc *f;

		dbg_lex("%s:%d G.prs.lex:%d, parsing...", __func__, __LINE__, G.prs.lex);
		if (IS_BC) {
#if ENABLE_BC
			s = zbc_parse_stmt_or_funcdef();
			if (s) goto err;

			// Check that next token is a correct stmt delimiter -
			// disallows "print 1 print 2" and such.
			if (G.prs.lex != BC_LEX_SCOLON
			 && G.prs.lex != XC_LEX_NLINE
			 && G.prs.lex != XC_LEX_EOF
			) {
				bc_error_at("bad statement terminator");
				goto err;
			}
			// The above logic is fragile. Check these examples:
			// - interactive read() still works
#endif
		} else {
#if ENABLE_DC
			s = zdc_parse_expr();
#endif
		}
		if (s || G_interrupt) {
 err:
			xc_parse_reset(); // includes xc_program_reset()
			RETURN_STATUS(BC_STATUS_FAILURE);
		}

		dbg_lex("%s:%d executing...", __func__, __LINE__);
		s = zxc_program_exec();
		if (s) {
			xc_program_reset();
			break;
		}

		ip = (void*)G.prog.exestack.v;
#if SANITY_CHECKS
		if (G.prog.exestack.len != 1) // should have only main's IP
			bb_simple_error_msg_and_die("BUG:call stack");
		if (ip->func != BC_PROG_MAIN)
			bb_simple_error_msg_and_die("BUG:not MAIN");
#endif
		f = xc_program_func_BC_PROG_MAIN();
		// bc discards strings, constants and code after each
		// top-level statement in the "main program".
		// This prevents "yes 1 | bc" from growing its memory
		// without bound. This can be done because data stack
		// is empty and thus can't hold any references to
		// strings or constants, there is no generated code
		// which can hold references (after we discard one
		// we just executed). Code of functions can have references,
		// but bc stores function strings/constants in per-function
		// storage.
		if (IS_BC) {
#if SANITY_CHECKS
			if (G.prog.results.len != 0) // should be empty
				bb_simple_error_msg_and_die("BUG:data stack");
#endif
			IF_BC(bc_vec_pop_all(&f->strs);)
			IF_BC(bc_vec_pop_all(&f->consts);)
			// We are at SCOLON/NLINE, skip it:
			s = zxc_lex_next();
			if (s) goto err;
		} else {
			if (G.prog.results.len == 0
			 && G.prog.vars.len == 0
			) {
				// If stack is empty and no registers exist (TODO: or they are all empty),
				// we can get rid of accumulated strings and constants.
				// In this example dc process should not grow
				// its memory consumption with time:
				// yes 1pc | dc
				IF_DC(bc_vec_pop_all(&G.prog.strs);)
				IF_DC(bc_vec_pop_all(&G.prog.consts);)
			}
			// The code is discarded always (below), thus this example
			// should also not grow its memory consumption with time,
			// even though its data stack is not empty:
			// { echo 1; yes dk; } | dc
		}
		// We drop generated and executed code for both bc and dc:
		bc_vec_pop_all(&f->code);
		ip->inst_idx = 0;
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zxc_vm_process(...) (zxc_vm_process(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_vm_execute_FILE(FILE *fp, const char *filename)
{
	// So far bc/dc have no way to include a file from another file,
	// therefore we know G.prs.lex_filename == NULL on entry
	//const char *sv_file;
	BcStatus s;

	G.prs.lex_filename = filename;
	G.prs.lex_input_fp = fp;
	G.err_line = G.prs.lex_line = 1;
	dbg_lex("p->lex_line reset to 1");

	do {
		s = zxc_vm_process("");
		// We do not stop looping on errors here if reading stdin.
		// Example: start interactive bc and enter "return".
		// It should say "'return' not in a function"
		// but should not exit.
	} while (G.prs.lex_input_fp == stdin);
	G.prs.lex_filename = NULL;
	RETURN_STATUS(s);
}
#define zxc_vm_execute_FILE(...) (zxc_vm_execute_FILE(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zxc_vm_file(const char *file)
{
	BcStatus s;
	FILE *fp;

	fp = xfopen_for_read(file);
	s = zxc_vm_execute_FILE(fp, file);
	fclose(fp);

	RETURN_STATUS(s);
}
#define zxc_vm_file(...) (zxc_vm_file(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_BC
static void bc_vm_info(void)
{
	printf("%s "BB_VER"\n"
		"Adapted from https://github.com/gavinhoward/bc\n"
		"Original code (c) 2018 Gavin D. Howard and contributors\n"
	, applet_name);
}

static void bc_args(char **argv)
{
	unsigned opts;
	int i;

	GETOPT_RESET();
#if ENABLE_FEATURE_BC_LONG_OPTIONS
	opts = option_mask32 |= getopt32long(argv, "wvsqli",
		"warn\0"              No_argument "w"
		"version\0"           No_argument "v"
		"standard\0"          No_argument "s"
		"quiet\0"             No_argument "q"
		"mathlib\0"           No_argument "l"
		"interactive\0"       No_argument "i"
	);
#else
	opts = option_mask32 |= getopt32(argv, "wvsqli");
#endif
	if (getenv("POSIXLY_CORRECT"))
		option_mask32 |= BC_FLAG_S;

	if (opts & BC_FLAG_V) {
		bc_vm_info();
		exit(0);
	}

	for (i = optind; argv[i]; ++i)
		bc_vec_push(&G.files, argv + i);
}

static void bc_vm_envArgs(void)
{
	BcVec v;
	char *buf;
	char *env_args = getenv("BC_ENV_ARGS");

	if (!env_args) return;

	G.env_args = xstrdup(env_args);
	buf = G.env_args;

	bc_vec_init(&v, sizeof(char *), NULL);

	while (*(buf = skip_whitespace(buf)) != '\0') {
		bc_vec_push(&v, &buf);
		buf = skip_non_whitespace(buf);
		if (!*buf)
			break;
		*buf++ = '\0';
	}

	// NULL terminate, and pass argv[] so that first arg is argv[1]
	if (sizeof(int) == sizeof(char*)) {
		bc_vec_push(&v, &const_int_0);
	} else {
		static char *const nullptr = NULL;
		bc_vec_push(&v, &nullptr);
	}
	bc_args(((char **)v.v) - 1);

	bc_vec_free(&v);
}

static const char bc_lib[] ALIGN1 = {
	"scale=20"
"\n"	"define e(x){"
"\n"		"auto b,s,n,r,d,i,p,f,v"
////////////////"if(x<0)return(1/e(-x))" // and drop 'n' and x<0 logic below
//^^^^^^^^^^^^^^^^ this would work, and is even more precise than GNU bc:
//e(-.998896): GNU:.36828580434569428695
//      above code:.36828580434569428696
//    actual value:.3682858043456942869594...
// but for now let's be "GNU compatible"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"if(x<0){"
"\n"			"n=1"
"\n"			"x=-x"
"\n"		"}"
"\n"		"s=scale"
"\n"		"r=6+s+.44*x"
"\n"		"scale=scale(x)+1"
"\n"		"while(x>1){"
"\n"			"d+=1"
"\n"			"x/=2"
"\n"			"scale+=1"
"\n"		"}"
"\n"		"scale=r"
"\n"		"r=x+1"
"\n"		"p=x"
"\n"		"f=v=1"
"\n"		"for(i=2;v;++i){"
"\n"			"p*=x"
"\n"			"f*=i"
"\n"			"v=p/f"
"\n"			"r+=v"
"\n"		"}"
"\n"		"while(d--)r*=r"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"if(n)return(1/r)"
"\n"		"return(r/1)"
"\n"	"}"
"\n"	"define l(x){"
"\n"		"auto b,s,r,p,a,q,i,v"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"if(x<=0){"
"\n"			"r=(1-10^scale)/1"
"\n"			"ibase=b"
"\n"			"return(r)"
"\n"		"}"
"\n"		"s=scale"
"\n"		"scale+=6"
"\n"		"p=2"
"\n"		"while(x>=2){"
"\n"			"p*=2"
"\n"			"x=sqrt(x)"
"\n"		"}"
"\n"		"while(x<=.5){"
"\n"			"p*=2"
"\n"			"x=sqrt(x)"
"\n"		"}"
"\n"		"r=a=(x-1)/(x+1)"
"\n"		"q=a*a"
"\n"		"v=1"
"\n"		"for(i=3;v;i+=2){"
"\n"			"a*=q"
"\n"			"v=a/i"
"\n"			"r+=v"
"\n"		"}"
"\n"		"r*=p"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"return(r/1)"
"\n"	"}"
"\n"	"define s(x){"
"\n"		"auto b,s,r,a,q,i"
"\n"		"if(x<0)return(-s(-x))"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"s=scale"
"\n"		"scale=1.1*s+2"
"\n"		"a=a(1)"
"\n"		"scale=0"
"\n"		"q=(x/a+2)/4"
"\n"		"x-=4*q*a"
"\n"		"if(q%2)x=-x"
"\n"		"scale=s+2"
"\n"		"r=a=x"
"\n"		"q=-x*x"
"\n"		"for(i=3;a;i+=2){"
"\n"			"a*=q/(i*(i-1))"
"\n"			"r+=a"
"\n"		"}"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"return(r/1)"
"\n"	"}"
"\n"	"define c(x){"
"\n"		"auto b,s"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"s=scale"
"\n"		"scale*=1.2"
"\n"		"x=s(2*a(1)+x)"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"return(x/1)"
"\n"	"}"
"\n"	"define a(x){"
"\n"		"auto b,s,r,n,a,m,t,f,i,u"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"n=1"
"\n"		"if(x<0){"
"\n"			"n=-1"
"\n"			"x=-x"
"\n"		"}"
"\n"		"if(scale<65){"
"\n"			"if(x==1)return(.7853981633974483096156608458198757210492923498437764552437361480/n)"
"\n"			"if(x==.2)return(.1973955598498807583700497651947902934475851037878521015176889402/n)"
"\n"		"}"
"\n"		"s=scale"
"\n"		"if(x>.2){"
"\n"			"scale+=5"
"\n"			"a=a(.2)"
"\n"		"}"
"\n"		"scale=s+3"
"\n"		"while(x>.2){"
"\n"			"m+=1"
"\n"			"x=(x-.2)/(1+.2*x)"
"\n"		"}"
"\n"		"r=u=x"
"\n"		"f=-x*x"
"\n"		"t=1"
"\n"		"for(i=3;t;i+=2){"
"\n"			"u*=f"
"\n"			"t=u/i"
"\n"			"r+=t"
"\n"		"}"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"return((m*a+r)/n)"
"\n"	"}"
"\n"	"define j(n,x){"
"\n"		"auto b,s,o,a,i,v,f"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"s=scale"
"\n"		"scale=0"
"\n"		"n/=1"
"\n"		"if(n<0){"
"\n"			"n=-n"
"\n"			"o=n%2"
"\n"		"}"
"\n"		"a=1"
"\n"		"for(i=2;i<=n;++i)a*=i"
"\n"		"scale=1.5*s"
"\n"		"a=(x^n)/2^n/a"
"\n"		"r=v=1"
"\n"		"f=-x*x/4"
"\n"		"scale+=length(a)-scale(a)"
"\n"		"for(i=1;v;++i){"
"\n"			"v=v*f/i/(n+i)"
"\n"			"r+=v"
"\n"		"}"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"if(o)a=-a"
"\n"		"return(a*r/1)"
"\n"	"}"
};
#endif // ENABLE_BC

static BC_STATUS zxc_vm_exec(void)
{
	char **fname;
	BcStatus s;
	size_t i;

#if ENABLE_BC
	if (option_mask32 & BC_FLAG_L) {
		// We know that internal library is not buggy,
		// thus error checking is normally disabled.
# define DEBUG_LIB 0
		s = zxc_vm_process(bc_lib);
		if (DEBUG_LIB && s) RETURN_STATUS(s);
	}
#endif

	s = BC_STATUS_SUCCESS;
	fname = (void*)G.files.v;
	for (i = 0; i < G.files.len; i++) {
		s = zxc_vm_file(*fname++);
		if (ENABLE_FEATURE_CLEAN_UP && !G_ttyin && s) {
			// Debug config, non-interactive mode:
			// return all the way back to main.
			// Non-debug builds do not come here
			// in non-interactive mode, they exit.
			RETURN_STATUS(s);
		}
	}

	if (IS_BC || (option_mask32 & BC_FLAG_I))
		s = zxc_vm_execute_FILE(stdin, /*filename:*/ NULL);

	RETURN_STATUS(s);
}
#define zxc_vm_exec(...) (zxc_vm_exec(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_FEATURE_CLEAN_UP
static void xc_program_free(void)
{
	bc_vec_free(&G.prog.fns);
	IF_BC(bc_vec_free(&G.prog.fn_map);)
	bc_vec_free(&G.prog.vars);
	bc_vec_free(&G.prog.var_map);
	bc_vec_free(&G.prog.arrs);
	bc_vec_free(&G.prog.arr_map);
	IF_DC(bc_vec_free(&G.prog.strs);)
	IF_DC(bc_vec_free(&G.prog.consts);)
	bc_vec_free(&G.prog.results);
	bc_vec_free(&G.prog.exestack);
	IF_BC(bc_num_free(&G.prog.last);)
	//IF_BC(bc_num_free(&G.prog.zero);)
	IF_BC(bc_num_free(&G.prog.one);)
	bc_vec_free(&G.input_buffer);
}
#endif

static void xc_program_init(void)
{
	BcInstPtr ip;

	// memset(&G.prog, 0, sizeof(G.prog)); - already is
	memset(&ip, 0, sizeof(BcInstPtr));

	// G.prog.nchars = G.prog.scale = 0; - already is
	G.prog.ib_t = 10;
	G.prog.ob_t = 10;

	IF_BC(bc_num_init_DEF_SIZE(&G.prog.last);)
	//IF_BC(bc_num_zero(&G.prog.last);) - already is

	//bc_num_init_DEF_SIZE(&G.prog.zero); - not needed
	//bc_num_zero(&G.prog.zero); - already is

	IF_BC(bc_num_init_DEF_SIZE(&G.prog.one);)
	IF_BC(bc_num_one(&G.prog.one);)

	bc_vec_init(&G.prog.fns, sizeof(BcFunc), bc_func_free);
	IF_BC(bc_vec_init(&G.prog.fn_map, sizeof(BcId), bc_id_free);)

	if (IS_BC) {
		// Names are chosen simply to be distinct and never match
		// a valid function name (and be short)
		IF_BC(bc_program_addFunc(xstrdup(""))); // func #0: main
		IF_BC(bc_program_addFunc(xstrdup("1"))); // func #1: for read()
	} else {
		// in dc, functions have no names
		xc_program_add_fn();
		xc_program_add_fn();
	}

	bc_vec_init(&G.prog.vars, sizeof(BcVec), bc_vec_free);
	bc_vec_init(&G.prog.var_map, sizeof(BcId), bc_id_free);

	bc_vec_init(&G.prog.arrs, sizeof(BcVec), bc_vec_free);
	bc_vec_init(&G.prog.arr_map, sizeof(BcId), bc_id_free);

	IF_DC(bc_vec_init(&G.prog.strs, sizeof(char *), bc_string_free);)
	IF_DC(bc_vec_init(&G.prog.consts, sizeof(char *), bc_string_free);)
	bc_vec_init(&G.prog.results, sizeof(BcResult), bc_result_free);
	bc_vec_init(&G.prog.exestack, sizeof(BcInstPtr), NULL);
	bc_vec_push(&G.prog.exestack, &ip);

	bc_char_vec_init(&G.input_buffer);
}

static unsigned xc_vm_envLen(const char *var)
{
	char *lenv;
	unsigned len;

	lenv = getenv(var);
	len = BC_NUM_PRINT_WIDTH;
	if (lenv) {
		len = bb_strtou(lenv, NULL, 10);
		if (len == 0 || len > INT_MAX)
			len = INT_MAX;
		if (errno)
			len = BC_NUM_PRINT_WIDTH;
	}

	// dc (GNU bc 1.07.1) 1.4.1 seems to use width
	// 1 char wider than bc from the same package.
	// Both default width, and xC_LINE_LENGTH=N are wider:
	// "DC_LINE_LENGTH=5 dc -e'123456 p'" prints:
	//	|1234\   |
	//	|56      |
	// "echo '123456' | BC_LINE_LENGTH=5 bc" prints:
	//	|123\    |
	//	|456     |
	// Do the same, but it might be a bug in GNU package
	if (IS_BC)
		len--;

	if (len < 2)
		len = IS_BC ? BC_NUM_PRINT_WIDTH - 1 : BC_NUM_PRINT_WIDTH;

	return len;
}

static int xc_vm_init(const char *env_len)
{
	G.prog.len = xc_vm_envLen(env_len);
	bc_vec_init(&G.files, sizeof(char *), NULL);

	xc_program_init();
	IF_BC(if (IS_BC) bc_vm_envArgs();)
	xc_parse_create(BC_PROG_MAIN);

//TODO: in GNU bc, the check is (isatty(0) && isatty(1)),
//-i option unconditionally enables this regardless of isatty():
	if (isatty(0)) {
#if ENABLE_FEATURE_BC_INTERACTIVE
		G_ttyin = 1;
		// With SA_RESTART, most system calls will restart
		// (IOW: they won't fail with EINTR).
		// In particular, this means ^C won't cause
		// stdout to get into "error state" if SIGINT hits
		// within write() syscall.
		//
		// The downside is that ^C while tty input is taken
		// will only be handled after [Enter] since read()
		// from stdin is not interrupted by ^C either,
		// it restarts, thus fgetc() does not return on ^C.
		// (This problem manifests only if line editing is disabled)
		signal_SA_RESTART_empty_mask(SIGINT, record_signo);

		// Without SA_RESTART, this exhibits a bug:
		// "while (1) print 1" and try ^C-ing it.
		// Intermittently, instead of returning to input line,
		// you'll get "output error: Interrupted system call"
		// and exit.
		//signal_no_SA_RESTART_empty_mask(SIGINT, record_signo);
#endif
		return 1; // "tty"
	}
	return 0; // "not a tty"
}

static BcStatus xc_vm_run(void)
{
	BcStatus st = zxc_vm_exec();
#if ENABLE_FEATURE_CLEAN_UP
	if (G_exiting) // it was actually "halt" or "quit"
		st = EXIT_SUCCESS;

	bc_vec_free(&G.files);
	xc_program_free();
	xc_parse_free();
	free(G.env_args);
# if ENABLE_FEATURE_EDITING
	free_line_input_t(G.line_input_state);
# endif
	FREE_G();
#endif
	dbg_exec("exiting with exitcode %d", st);
	return st;
}

#if ENABLE_BC
int bc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bc_main(int argc UNUSED_PARAM, char **argv)
{
	int is_tty;

	INIT_G();

	is_tty = xc_vm_init("BC_LINE_LENGTH");

	bc_args(argv);

	if (is_tty && !(option_mask32 & BC_FLAG_Q))
		bc_vm_info();

	return xc_vm_run();
}
#endif

#if ENABLE_DC
int dc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dc_main(int argc UNUSED_PARAM, char **argv)
{
	int noscript;

	INIT_G();

	xc_vm_init("DC_LINE_LENGTH");

	// Run -e'SCRIPT' and -fFILE in order of appearance, then handle FILEs
	noscript = BC_FLAG_I;
	for (;;) {
		int n = getopt(argc, argv, "e:f:x");
		if (n <= 0)
			break;
		switch (n) {
		case 'e':
			noscript = 0;
			n = zxc_vm_process(optarg);
			if (n) return n;
			break;
		case 'f':
			noscript = 0;
			n = zxc_vm_file(optarg);
			if (n) return n;
			break;
		case 'x':
			option_mask32 |= DC_FLAG_X;
			break;
		default:
			bb_show_usage();
		}
	}
	argv += optind;

	while (*argv) {
		noscript = 0;
		bc_vec_push(&G.files, argv++);
	}

	option_mask32 |= noscript; // set BC_FLAG_I if we need to interpret stdin

	return xc_vm_run();
}
#endif

#endif // DC_BIG
