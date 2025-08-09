/* bc.c - An implementation of POSIX bc.
 *
 * Copyright 2018 Gavin D. Howard <yzena.tech@gmail.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/bc.html

USE_BC(NEWTOY(bc, "i(interactive)l(mathlib)q(quiet)s(standard)w(warn)", TOYFLAG_USR|TOYFLAG_BIN))

config BC
  bool "bc"
  default n
  help
    usage: bc [-ilqsw] [file ...]

    bc is a command-line calculator with a Turing-complete language.

    options:

      -i  --interactive  force interactive mode
      -l  --mathlib      use predefined math routines:

                         s(expr)  =  sine of expr in radians
                         c(expr)  =  cosine of expr in radians
                         a(expr)  =  arctangent of expr, returning radians
                         l(expr)  =  natural log of expr
                         e(expr)  =  raises e to the power of expr
                         j(n, x)  =  Bessel function of integer order n of x

      -q  --quiet        don't print version and copyright
      -s  --standard     error if any non-POSIX extensions are used
      -w  --warn         warn if any non-POSIX extensions are used

*/

#define FOR_bc
#include "toys.h"

GLOBALS(
  // This actually needs to be a BcVm*, but the toybox build
  // system complains if I make it so. Instead, we'll just cast.
  char *vm;

  size_t nchars;
  char *file, sig, max_ibase;
  uint16_t line_len;
)

struct str_len {
  char *str;
  long len;
};

#define BC_VM ((BcVm*) TT.vm)

typedef enum BcStatus {

  BC_STATUS_SUCCESS = 0,
  BC_STATUS_ERROR,
  BC_STATUS_EOF,
  BC_STATUS_EMPTY_EXPR,
  BC_STATUS_SIGNAL,
  BC_STATUS_QUIT,

} BcStatus;

typedef enum BcError {

  BC_ERROR_VM_ALLOC_ERR,
  BC_ERROR_VM_IO_ERR,
  BC_ERROR_VM_BIN_FILE,
  BC_ERROR_VM_PATH_DIR,

  BC_ERROR_PARSE_EOF,
  BC_ERROR_PARSE_CHAR,
  BC_ERROR_PARSE_STRING,
  BC_ERROR_PARSE_COMMENT,
  BC_ERROR_PARSE_TOKEN,
  BC_ERROR_EXEC_NUM_LEN,
  BC_ERROR_EXEC_NAME_LEN,
  BC_ERROR_EXEC_STRING_LEN,
  BC_ERROR_PARSE_EXPR,
  BC_ERROR_PARSE_EMPTY_EXPR,
  BC_ERROR_PARSE_PRINT,
  BC_ERROR_PARSE_FUNC,
  BC_ERROR_PARSE_ASSIGN,
  BC_ERROR_PARSE_NO_AUTO,
  BC_ERROR_PARSE_DUP_LOCAL,
  BC_ERROR_PARSE_BLOCK,
  BC_ERROR_PARSE_RET_VOID,

  BC_ERROR_MATH_NEGATIVE,
  BC_ERROR_MATH_NON_INTEGER,
  BC_ERROR_MATH_OVERFLOW,
  BC_ERROR_MATH_DIVIDE_BY_ZERO,

  BC_ERROR_EXEC_FILE_ERR,
  BC_ERROR_EXEC_ARRAY_LEN,
  BC_ERROR_EXEC_IBASE,
  BC_ERROR_EXEC_OBASE,
  BC_ERROR_EXEC_SCALE,
  BC_ERROR_EXEC_READ_EXPR,
  BC_ERROR_EXEC_REC_READ,
  BC_ERROR_EXEC_TYPE,
  BC_ERROR_EXEC_PARAMS,
  BC_ERROR_EXEC_UNDEF_FUNC,
  BC_ERROR_EXEC_VOID_VAL,

  BC_ERROR_POSIX_START,

  BC_ERROR_POSIX_NAME_LEN = BC_ERROR_POSIX_START,
  BC_ERROR_POSIX_COMMENT,
  BC_ERROR_POSIX_KW,
  BC_ERROR_POSIX_DOT,
  BC_ERROR_POSIX_RET,
  BC_ERROR_POSIX_BOOL,
  BC_ERROR_POSIX_REL_POS,
  BC_ERROR_POSIX_MULTIREL,
  BC_ERROR_POSIX_FOR1,
  BC_ERROR_POSIX_FOR2,
  BC_ERROR_POSIX_FOR3,
  BC_ERROR_POSIX_BRACE,
  BC_ERROR_POSIX_REF,

} BcError;

#define BC_ERR_IDX_VM (0)
#define BC_ERR_IDX_PARSE (1)
#define BC_ERR_IDX_MATH (2)
#define BC_ERR_IDX_EXEC (3)
#define BC_ERR_IDX_POSIX (4)

#define BC_VEC_START_CAP (1<<5)

typedef void (*BcVecFree)(void*);

typedef struct BcVec {
  char *v;
  size_t len, cap, size;
  BcVecFree dtor;
} BcVec;

#define bc_vec_pop(v) (bc_vec_npop((v), 1))
#define bc_vec_top(v) (bc_vec_item_rev((v), 0))

typedef signed char BcDig;

typedef struct BcNum {
  signed char *num;
  unsigned long rdx, len, cap;
  int neg;
} BcNum;

#define BC_NUM_DEF_SIZE (16)

// A crude, but always big enough, calculation of
// the size required for ibase and obase BcNum's.
#define BC_NUM_LONG_LOG10 ((CHAR_BIT * sizeof(unsigned long) + 1) / 2 + 1)

#define BC_NUM_NEG(n, neg) ((((ssize_t) (n)) ^ -((ssize_t) (neg))) + (neg))

#define BC_NUM_ONE(n) ((n)->len == 1 && (n)->rdx == 0 && (n)->num[0] == 1)
#define BC_NUM_INT(n) ((n)->len - (n)->rdx)
#define BC_NUM_CMP_ZERO(a) (BC_NUM_NEG((a)->len != 0, (a)->neg))

typedef BcStatus (*BcNumBinaryOp)(BcNum*, BcNum*, BcNum*, size_t);
typedef size_t (*BcNumBinaryOpReq)(BcNum*, BcNum*, size_t);
typedef void (*BcNumDigitOp)(size_t, size_t, int);

void bc_num_init(BcNum *n, size_t req);
void bc_num_expand(BcNum *n, size_t req);
void bc_num_copy(BcNum *d, BcNum *s);
void bc_num_createCopy(BcNum *d, BcNum *s);
void bc_num_createFromUlong(BcNum *n, unsigned long val);
void bc_num_free(void *num);

BcStatus bc_num_ulong(BcNum *n, unsigned long *result);
void bc_num_ulong2num(BcNum *n, unsigned long val);

BcStatus bc_num_add(BcNum *a, BcNum *b, BcNum *c, size_t scale);
BcStatus bc_num_sub(BcNum *a, BcNum *b, BcNum *c, size_t scale);
BcStatus bc_num_mul(BcNum *a, BcNum *b, BcNum *c, size_t scale);
BcStatus bc_num_div(BcNum *a, BcNum *b, BcNum *c, size_t scale);
BcStatus bc_num_mod(BcNum *a, BcNum *b, BcNum *c, size_t scale);
BcStatus bc_num_pow(BcNum *a, BcNum *b, BcNum *c, size_t scale);
BcStatus bc_num_sqrt(BcNum *a, BcNum *b, size_t scale);
BcStatus bc_num_divmod(BcNum *a, BcNum *b, BcNum *c, BcNum *d, size_t scale);

size_t bc_num_addReq(BcNum *a, BcNum *b, size_t scale);

size_t bc_num_mulReq(BcNum *a, BcNum *b, size_t scale);
size_t bc_num_powReq(BcNum *a, BcNum *b, size_t scale);

typedef enum BcInst {

  BC_INST_INC_POST = 0,
  BC_INST_DEC_POST,
  BC_INST_INC_PRE,
  BC_INST_DEC_PRE,

  BC_INST_NEG,
  BC_INST_BOOL_NOT,

  BC_INST_POWER,
  BC_INST_MULTIPLY,
  BC_INST_DIVIDE,
  BC_INST_MODULUS,
  BC_INST_PLUS,
  BC_INST_MINUS,

  BC_INST_REL_EQ,
  BC_INST_REL_LE,
  BC_INST_REL_GE,
  BC_INST_REL_NE,
  BC_INST_REL_LT,
  BC_INST_REL_GT,

  BC_INST_BOOL_OR,
  BC_INST_BOOL_AND,

  BC_INST_ASSIGN_POWER,
  BC_INST_ASSIGN_MULTIPLY,
  BC_INST_ASSIGN_DIVIDE,
  BC_INST_ASSIGN_MODULUS,
  BC_INST_ASSIGN_PLUS,
  BC_INST_ASSIGN_MINUS,
  BC_INST_ASSIGN,

  BC_INST_NUM,
  BC_INST_VAR,
  BC_INST_ARRAY_ELEM,
  BC_INST_ARRAY,

  BC_INST_LAST,
  BC_INST_IBASE,
  BC_INST_OBASE,
  BC_INST_SCALE,
  BC_INST_LENGTH,
  BC_INST_SCALE_FUNC,
  BC_INST_SQRT,
  BC_INST_ABS,
  BC_INST_READ,

  BC_INST_PRINT,
  BC_INST_PRINT_POP,
  BC_INST_STR,
  BC_INST_PRINT_STR,

  BC_INST_JUMP,
  BC_INST_JUMP_ZERO,

  BC_INST_CALL,

  BC_INST_RET,
  BC_INST_RET0,
  BC_INST_RET_VOID,

  BC_INST_HALT,

  BC_INST_POP,
  BC_INST_POP_EXEC,

} BcInst;

typedef struct BcFunc {

  BcVec code;
  BcVec labels;
  BcVec autos;
  size_t nparams;

  BcVec strs;
  BcVec consts;

  char *name;
  int voidfn;

} BcFunc;

typedef enum BcResultType {

  BC_RESULT_VAR,
  BC_RESULT_ARRAY_ELEM,
  BC_RESULT_ARRAY,

  BC_RESULT_STR,

  BC_RESULT_CONSTANT,
  BC_RESULT_TEMP,

  BC_RESULT_VOID,
  BC_RESULT_ONE,
  BC_RESULT_LAST,
  BC_RESULT_IBASE,
  BC_RESULT_OBASE,
  BC_RESULT_SCALE,

} BcResultType;

typedef union BcResultData {
  BcNum n;
  BcVec v;
  struct str_len id;
} BcResultData;

typedef struct BcResult {
  BcResultType t;
  BcResultData d;
} BcResult;

typedef struct BcInstPtr {
  size_t func;
  size_t idx;
  size_t len;
} BcInstPtr;

typedef enum BcType {
  BC_TYPE_VAR,
  BC_TYPE_ARRAY,
} BcType;

void bc_array_expand(BcVec *a, size_t len);
int bc_id_cmp(struct str_len *e1, struct str_len *e2);

#define bc_lex_err(l, e) (bc_vm_error((e), (l)->line))
#define bc_lex_verr(l, e, ...) (bc_vm_error((e), (l)->line, __VA_ARGS__))

#define BC_LEX_NUM_CHAR(c, l, pt) \
  (isdigit(c) || ((c) >= 'A' && (c) <= (l)) || ((c) == '.' && !(pt)))

// BC_LEX_NEG is not used in lexing; it is only for parsing.
typedef enum BcLexType {

  BC_LEX_EOF,
  BC_LEX_INVALID,

  BC_LEX_OP_INC,
  BC_LEX_OP_DEC,

  BC_LEX_NEG,
  BC_LEX_OP_BOOL_NOT,

  BC_LEX_OP_POWER,
  BC_LEX_OP_MULTIPLY,
  BC_LEX_OP_DIVIDE,
  BC_LEX_OP_MODULUS,
  BC_LEX_OP_PLUS,
  BC_LEX_OP_MINUS,

  BC_LEX_OP_REL_EQ,
  BC_LEX_OP_REL_LE,
  BC_LEX_OP_REL_GE,
  BC_LEX_OP_REL_NE,
  BC_LEX_OP_REL_LT,
  BC_LEX_OP_REL_GT,

  BC_LEX_OP_BOOL_OR,
  BC_LEX_OP_BOOL_AND,

  BC_LEX_OP_ASSIGN_POWER,
  BC_LEX_OP_ASSIGN_MULTIPLY,
  BC_LEX_OP_ASSIGN_DIVIDE,
  BC_LEX_OP_ASSIGN_MODULUS,
  BC_LEX_OP_ASSIGN_PLUS,
  BC_LEX_OP_ASSIGN_MINUS,
  BC_LEX_OP_ASSIGN,

  BC_LEX_NLINE,
  BC_LEX_WHITESPACE,

  BC_LEX_LPAREN,
  BC_LEX_RPAREN,

  BC_LEX_LBRACKET,
  BC_LEX_COMMA,
  BC_LEX_RBRACKET,

  BC_LEX_LBRACE,
  BC_LEX_SCOLON,
  BC_LEX_RBRACE,

  BC_LEX_STR,
  BC_LEX_NAME,
  BC_LEX_NUMBER,

  BC_LEX_KEY_AUTO,
  BC_LEX_KEY_BREAK,
  BC_LEX_KEY_CONTINUE,
  BC_LEX_KEY_DEFINE,
  BC_LEX_KEY_FOR,
  BC_LEX_KEY_IF,
  BC_LEX_KEY_LIMITS,
  BC_LEX_KEY_RETURN,
  BC_LEX_KEY_WHILE,
  BC_LEX_KEY_HALT,
  BC_LEX_KEY_LAST,
  BC_LEX_KEY_IBASE,
  BC_LEX_KEY_OBASE,
  BC_LEX_KEY_SCALE,
  BC_LEX_KEY_LENGTH,
  BC_LEX_KEY_PRINT,
  BC_LEX_KEY_SQRT,
  BC_LEX_KEY_ABS,
  BC_LEX_KEY_QUIT,
  BC_LEX_KEY_READ,
  BC_LEX_KEY_ELSE,

} BcLexType;

typedef struct BcLex {

  char *buf;
  size_t i;
  size_t line;
  size_t len;

  BcLexType t;
  BcLexType last;
  BcVec str;

} BcLex;

#define BC_PARSE_REL (1<<0)
#define BC_PARSE_PRINT (1<<1)
#define BC_PARSE_NOCALL (1<<2)
#define BC_PARSE_NOREAD (1<<3)
#define BC_PARSE_ARRAY (1<<4)

#define bc_parse_push(p, i) (bc_vec_pushByte(&(p)->func->code, i))
#define bc_parse_number(p)(bc_parse_addId((p), BC_INST_NUM))
#define bc_parse_string(p)(bc_parse_addId((p), BC_INST_STR))

#define bc_parse_err(p, e) (bc_vm_error((e), (p)->l.line))
#define bc_parse_verr(p, e, ...) (bc_vm_error((e), (p)->l.line, __VA_ARGS__))

typedef struct BcParseNext {
  char len, tokens[4];
} BcParseNext;

#define BC_PARSE_NEXT_TOKENS(...) .tokens = { __VA_ARGS__ }
#define BC_PARSE_NEXT(a, ...) { .len = a, BC_PARSE_NEXT_TOKENS(__VA_ARGS__) }

struct BcProgram;

typedef struct BcParse {

  BcLex l;

  BcVec flags;
  BcVec exits;
  BcVec conds;
  BcVec ops;

  struct BcProgram *prog;
  BcFunc *func;
  size_t fidx;

  int auto_part;

} BcParse;

typedef struct BcLexKeyword {
  char data, name[9];
} BcLexKeyword;

#define BC_LEX_CHAR_MSB(bit) ((bit) << (CHAR_BIT - 1))

#define BC_LEX_KW_POSIX(kw) ((kw)->data & (BC_LEX_CHAR_MSB(1)))
#define BC_LEX_KW_LEN(kw) ((size_t) ((kw)->data & ~(BC_LEX_CHAR_MSB(1))))

#define BC_LEX_KW_ENTRY(a, b, c) \
  { .data = ((b) & ~(BC_LEX_CHAR_MSB(1))) | BC_LEX_CHAR_MSB(c),.name = a }

#define bc_lex_posixErr(l, e) (bc_vm_posixError((e), (l)->line))
#define bc_lex_vposixErr(l, e, ...) \
  (bc_vm_posixError((e), (l)->line, __VA_ARGS__))

BcStatus bc_lex_token(BcLex *l);

#define BC_PARSE_TOP_FLAG_PTR(p) ((uint16_t*) bc_vec_top(&(p)->flags))
#define BC_PARSE_TOP_FLAG(p) (*(BC_PARSE_TOP_FLAG_PTR(p)))

#define BC_PARSE_FLAG_BRACE (1<<0)
#define BC_PARSE_BRACE(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_BRACE)

#define BC_PARSE_FLAG_FUNC_INNER (1<<1)
#define BC_PARSE_FUNC_INNER(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_FUNC_INNER)

#define BC_PARSE_FLAG_FUNC (1<<2)
#define BC_PARSE_FUNC(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_FUNC)

#define BC_PARSE_FLAG_BODY (1<<3)
#define BC_PARSE_BODY(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_BODY)

#define BC_PARSE_FLAG_LOOP (1<<4)
#define BC_PARSE_LOOP(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_LOOP)

#define BC_PARSE_FLAG_LOOP_INNER (1<<5)
#define BC_PARSE_LOOP_INNER(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_LOOP_INNER)

#define BC_PARSE_FLAG_IF (1<<6)
#define BC_PARSE_IF(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_IF)

#define BC_PARSE_FLAG_ELSE (1<<7)
#define BC_PARSE_ELSE(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_ELSE)

#define BC_PARSE_FLAG_IF_END (1<<8)
#define BC_PARSE_IF_END(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_IF_END)

#define BC_PARSE_NO_EXEC(p) ((p)->flags.len != 1 || BC_PARSE_TOP_FLAG(p) != 0)

#define BC_PARSE_DELIMITER(t) \
  ((t) == BC_LEX_SCOLON || (t) == BC_LEX_NLINE || (t) == BC_LEX_EOF)

#define BC_PARSE_BLOCK_STMT(f) \
  ((f) & (BC_PARSE_FLAG_ELSE | BC_PARSE_FLAG_LOOP_INNER))

#define BC_PARSE_OP(p, l) (((p) & ~(BC_LEX_CHAR_MSB(1))) | (BC_LEX_CHAR_MSB(l)))

#define BC_PARSE_OP_DATA(t) bc_parse_ops[((t) - BC_LEX_OP_INC)]
#define BC_PARSE_OP_LEFT(op) (BC_PARSE_OP_DATA(op) & BC_LEX_CHAR_MSB(1))
#define BC_PARSE_OP_PREC(op) (BC_PARSE_OP_DATA(op) & ~(BC_LEX_CHAR_MSB(1)))

#define BC_PARSE_TOP_OP(p) (*((BcLexType*) bc_vec_top(&(p)->ops)))
#define BC_PARSE_LEAF(prev, bin_last, rparen) \
  (!(bin_last) && ((rparen) || bc_parse_inst_isLeaf(prev)))
#define BC_PARSE_INST_VAR(t) \
  ((t) >= BC_INST_VAR && (t) <= BC_INST_SCALE && (t) != BC_INST_ARRAY)

#define BC_PARSE_PREV_PREFIX(p) \
  ((p) >= BC_INST_INC_PRE && (p) <= BC_INST_BOOL_NOT)
#define BC_PARSE_OP_PREFIX(t) ((t) == BC_LEX_OP_BOOL_NOT || (t) == BC_LEX_NEG)

// We can calculate the conversion between tokens and exprs by subtracting the
// position of the first operator in the lex enum and adding the position of
// the first in the expr enum. Note: This only works for binary operators.
#define BC_PARSE_TOKEN_INST(t) ((char) ((t) - BC_LEX_NEG + BC_INST_NEG))

#define bc_parse_posixErr(p, e) (bc_vm_posixError((e), (p)->l.line))

BcStatus bc_parse_parse(BcParse *p);
BcStatus bc_parse_expr_status(BcParse *p, uint8_t flags, BcParseNext next);
void bc_parse_noElse(BcParse *p);

#define BC_PROG_ONE_CAP (1)

typedef struct BcProgram {

  size_t scale;

  BcNum ib;
  size_t ib_t;
  BcNum ob;
  size_t ob_t;

  BcVec results;
  BcVec stack;

  BcVec fns;
  BcVec fn_map;

  BcVec vars;
  BcVec var_map;

  BcVec arrs;
  BcVec arr_map;

  BcNum one;
  BcNum last;

  signed char ib_num[BC_NUM_LONG_LOG10], ob_num[BC_NUM_LONG_LOG10],
         one_num[BC_PROG_ONE_CAP];
} BcProgram;

#define BC_PROG_STACK(s, n) ((s)->len >= ((size_t) (n)))

#define BC_PROG_MAIN (0)
#define BC_PROG_READ (1)

#define BC_PROG_STR(n) (!(n)->num && !(n)->cap)
#define BC_PROG_NUM(r, n) \
  ((r)->t != BC_RESULT_ARRAY && (r)->t != BC_RESULT_STR && !BC_PROG_STR(n))

typedef void (*BcProgramUnary)(BcResult*, BcNum*);

void bc_program_addFunc(BcProgram *p, BcFunc *f, char *name);
size_t bc_program_insertFunc(BcProgram *p, char *name);
BcStatus bc_program_reset(BcProgram *p, BcStatus s);
BcStatus bc_program_exec(BcProgram *p);

unsigned long bc_program_scale(BcNum *n);
unsigned long bc_program_len(BcNum *n);

void bc_program_negate(BcResult *r, BcNum *n);
void bc_program_not(BcResult *r, BcNum *n);

#define BC_FLAG_TTYIN (1<<7)
#define BC_TTYIN (toys.optflags & BC_FLAG_TTYIN)

#define BC_MAX_OBASE ((unsigned long) INT_MAX)
#define BC_MAX_DIM ((unsigned long) INT_MAX)
#define BC_MAX_SCALE ((unsigned long) UINT_MAX)
#define BC_MAX_STRING ((unsigned long) UINT_MAX - 1)
#define BC_MAX_NAME BC_MAX_STRING
#define BC_MAX_NUM BC_MAX_STRING
#define BC_MAX_EXP ((unsigned long) ULONG_MAX)
#define BC_MAX_VARS ((unsigned long) SIZE_MAX - 1)

#define bc_vm_err(e) (bc_vm_error((e), 0))
#define bc_vm_verr(e, ...) (bc_vm_error((e), 0, __VA_ARGS__))

typedef struct BcVm {
  BcParse prs;
  BcProgram prog;
} BcVm;

BcStatus bc_vm_posixError(BcError e, size_t line, ...);

BcStatus bc_vm_error(BcError e, size_t line, ...);

char bc_sig_msg[] = "\ninterrupt (type \"quit\" to exit)\n";

char bc_copyright[] =
  "Copyright (c) 2018 Gavin D. Howard and contributors\n"
  "Report bugs at: https://github.com/gavinhoward/bc\n\n"
  "This is free software with ABSOLUTELY NO WARRANTY.\n";

char *bc_err_fmt = "\n%s error: ";
char *bc_warn_fmt = "\n%s warning: ";
char *bc_err_line = ":%zu";

char *bc_errs[] = {
  "VM",
  "Parse",
  "Math",
  "Runtime",
  "POSIX",
};

char bc_err_ids[] = {
  BC_ERR_IDX_VM, BC_ERR_IDX_VM, BC_ERR_IDX_VM, BC_ERR_IDX_VM,
  BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
  BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
  BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
  BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
  BC_ERR_IDX_PARSE,
  BC_ERR_IDX_MATH, BC_ERR_IDX_MATH, BC_ERR_IDX_MATH, BC_ERR_IDX_MATH,
  BC_ERR_IDX_EXEC, BC_ERR_IDX_EXEC, BC_ERR_IDX_EXEC, BC_ERR_IDX_EXEC,
  BC_ERR_IDX_EXEC, BC_ERR_IDX_EXEC, BC_ERR_IDX_EXEC, BC_ERR_IDX_EXEC,
  BC_ERR_IDX_EXEC, BC_ERR_IDX_EXEC, BC_ERR_IDX_EXEC,
  BC_ERR_IDX_POSIX, BC_ERR_IDX_POSIX, BC_ERR_IDX_POSIX, BC_ERR_IDX_POSIX,
  BC_ERR_IDX_POSIX, BC_ERR_IDX_POSIX, BC_ERR_IDX_POSIX, BC_ERR_IDX_POSIX,
  BC_ERR_IDX_POSIX, BC_ERR_IDX_POSIX, BC_ERR_IDX_POSIX, BC_ERR_IDX_POSIX,
  BC_ERR_IDX_POSIX,
};

char *bc_err_msgs[] = {

  "memory allocation error",
  "I/O error",
  "file is not ASCII: %s",
  "path is a directory: %s",

  "end of file",
  "bad character (%c)",
  "string end could not be found",
  "comment end could not be found",
  "bad token",
  "name too long: must be [1, %lu]",
  "string too long: must be [1, %lu]",
  "array too long; must be [1, %lu]",
  "bad expression",
  "empty expression",
  "bad print statement",
  "bad function definition",
  "bad assignment: left side must be scale, ibase, "
    "obase, last, var, or array element",
  "no auto variable found",
  "function parameter or auto \"%s\" already exists",
  "block end could not be found",
  "cannot return a value from void function: %s()",

  "negative number",
  "non integer number",
  "overflow",
  "divide by zero",

  "could not open file: %s",
  "number too long: must be [1, %lu]",
  "bad ibase; must be [%lu, %lu]",
  "bad obase; must be [%lu, %lu]",
  "bad scale; must be [%lu, %lu]",
  "bad read() expression",
  "read() call inside of a read() call",
  "variable is wrong type",
  "mismatched parameters; need %zu, have %zu",
  "undefined function: %s()",
  "cannot use a void value in an expression",

  "POSIX does not allow names longer than 1 character, like \"%s\"",
  "POSIX does not allow '#' script comments",
  "POSIX does not allow \"%s\" as a keyword",
  "POSIX does not allow a period ('.') as a shortcut for the last result",
  "POSIX requires parentheses around return expressions",
  "POSIX does not allow the \"%s\" operators",
  "POSIX does not allow comparison operators outside if or loops",
  "POSIX requires zero or one comparison operator per condition",
  "POSIX does not allow an empty init expression in a for loop",
  "POSIX does not allow an empty condition expression in a for loop",
  "POSIX does not allow an empty update expression in a for loop",
  "POSIX requires the left brace be on the same line as the function header",
  "POSIX does not allow array references as function parameters",

};

char bc_func_main[] = "(main)";
char bc_func_read[] = "(read)";

BcLexKeyword bc_lex_kws[] = {
  BC_LEX_KW_ENTRY("auto", 4, 1),
  BC_LEX_KW_ENTRY("break", 5, 1),
  BC_LEX_KW_ENTRY("continue", 8, 0),
  BC_LEX_KW_ENTRY("define", 6, 1),
  BC_LEX_KW_ENTRY("for", 3, 1),
  BC_LEX_KW_ENTRY("if", 2, 1),
  BC_LEX_KW_ENTRY("limits", 6, 0),
  BC_LEX_KW_ENTRY("return", 6, 1),
  BC_LEX_KW_ENTRY("while", 5, 1),
  BC_LEX_KW_ENTRY("halt", 4, 0),
  BC_LEX_KW_ENTRY("last", 4, 0),
  BC_LEX_KW_ENTRY("ibase", 5, 1),
  BC_LEX_KW_ENTRY("obase", 5, 1),
  BC_LEX_KW_ENTRY("scale", 5, 1),
  BC_LEX_KW_ENTRY("length", 6, 1),
  BC_LEX_KW_ENTRY("print", 5, 0),
  BC_LEX_KW_ENTRY("sqrt", 4, 1),
  BC_LEX_KW_ENTRY("abs", 3, 0),
  BC_LEX_KW_ENTRY("quit", 4, 1),
  BC_LEX_KW_ENTRY("read", 4, 0),
  BC_LEX_KW_ENTRY("else", 4, 0),
};

size_t bc_lex_kws_len = sizeof(bc_lex_kws) / sizeof(BcLexKeyword);

char *bc_parse_const1 = "1";

// This is an array of data for operators that correspond to token types.
char bc_parse_ops[] = {
  BC_PARSE_OP(0, 0), BC_PARSE_OP(0, 0),
  BC_PARSE_OP(1, 0), BC_PARSE_OP(1, 0),
  BC_PARSE_OP(4, 0),
  BC_PARSE_OP(5, 1), BC_PARSE_OP(5, 1), BC_PARSE_OP(5, 1),
  BC_PARSE_OP(6, 1), BC_PARSE_OP(6, 1),
  BC_PARSE_OP(9, 1), BC_PARSE_OP(9, 1), BC_PARSE_OP(9, 1),
  BC_PARSE_OP(9, 1), BC_PARSE_OP(9, 1), BC_PARSE_OP(9, 1),
  BC_PARSE_OP(11, 1), BC_PARSE_OP(10, 1),
  BC_PARSE_OP(8, 0), BC_PARSE_OP(8, 0), BC_PARSE_OP(8, 0),
  BC_PARSE_OP(8, 0), BC_PARSE_OP(8, 0), BC_PARSE_OP(8, 0),
  BC_PARSE_OP(8, 0),
};

// These identify what tokens can come after expressions in certain cases.
BcParseNext bc_parse_next_expr =
  BC_PARSE_NEXT(4, BC_LEX_NLINE, BC_LEX_SCOLON, BC_LEX_RBRACE, BC_LEX_EOF);
BcParseNext bc_parse_next_param =
  BC_PARSE_NEXT(2, BC_LEX_RPAREN, BC_LEX_COMMA);
BcParseNext bc_parse_next_print =
  BC_PARSE_NEXT(4, BC_LEX_COMMA, BC_LEX_NLINE, BC_LEX_SCOLON, BC_LEX_EOF);
BcParseNext bc_parse_next_rel = BC_PARSE_NEXT(1, BC_LEX_RPAREN);
BcParseNext bc_parse_next_elem = BC_PARSE_NEXT(1, BC_LEX_RBRACKET);
BcParseNext bc_parse_next_for = BC_PARSE_NEXT(1, BC_LEX_SCOLON);
BcParseNext bc_parse_next_read =
  BC_PARSE_NEXT(2, BC_LEX_NLINE, BC_LEX_EOF);

char bc_num_hex_digits[] = "0123456789ABCDEF";

BcNumBinaryOp bc_program_ops[] = {
  bc_num_pow, bc_num_mul, bc_num_div, bc_num_mod, bc_num_add, bc_num_sub,
};

BcNumBinaryOpReq bc_program_opReqs[] = {
  bc_num_powReq, bc_num_mulReq, bc_num_mulReq, bc_num_mulReq,
  bc_num_addReq, bc_num_addReq,
};

BcProgramUnary bc_program_unarys[] = {
  bc_program_negate, bc_program_not,
};

char bc_program_ready_msg[] = "ready for more input\n";

char *bc_lib_name = "gen/lib.bc";

char *bc_lib =
"scale=20;\n"

"define e(x){\n"
"  auto b,s,n,r,d,i,p,f,v;\n"
"  b=ibase;\n"
"  ibase=A;\n"
"  if(x<0){\n"
"    n=1; x=-x;\n"
"  }\n"
"  s=scale;\n"
"  r=6+s+.44*x;\n"
"  scale=scale(x)+1;\n"
"  while(x>1){\n"
"    d+=1; x/=2;\n"
"    scale+=1;\n"
"  }\n"
"  scale=r;\n"
"  r=x+1; p=x; f=v=1;\n"
"  for(i=2;v;++i){;\n"
"    p*=x; f*=i; v=p/f; r+=v;\n"
"  }\n"
"  while(d--)r*=r;\n"
"  scale=s;\n"
"  ibase=b;\n"
"  if(n)return(1/r)\n"
"    return(r/1)\n"
"}\n"

"define l(x){\n"
"  auto b,s,r,p,a,q,i,v;\n"
"  b=ibase;\n"
"  ibase=A;\n"
"  if(x<=0){;\n"
"    r=(1-10^scale)/1;\n"
"    ibase=b;\n"
"    return(r)\n"
"  }\n"
"  s=scale;\n"
"  scale+=6;\n"
"  p=2;\n"
"  while(x>=2){;\n"
"    p*=2;\n"
"    x=sqrt(x);\n"
"  }\n"
"  while(x<=.5){;\n"
"    p*=2;\n"
"    x=sqrt(x);\n"
"  }\n"
"  r=a=(x-1)/(x+1);\n"
"  q=a*a;\n"
"  v=1;\n"
"  for(i=3;v;i+=2){;\n"
"    a*=q; v=a/i; r+=v;\n"
"  }\n"
"  r*=p;\n"
"  scale=s;\n"
"  ibase=b;\n"
"  return(r/1)\n"
"}\n"

"define s(x){\n"
"  auto b,s,r,a,q,i;\n"
"  if(x<0)return(-s(-x))\n"
"    b=ibase;\n"
"  ibase=A;\n"
"  s=scale;\n"
"  scale=1.1*s+2;\n"
"  a=a(1);\n"
"  scale=0;\n"
"  q=(x/a+2)/4;\n"
"  x-=4*q*a;\n"
"  if(q%2) x=-x;\n"
"  scale=s+2;\n"
"  r=a=x;\n"
"  q=-x*x;\n"
"  for(i=3;a;i+=2){;\n"
"    a*=q/(i*(i-1)); r+=a;\n"
"  }\n"
"  scale=s;\n"
"  ibase=b;\n"
"  return(r/1)\n"
"}\n"

"define c(x){\n"
"  auto b,s;\n"
"  b=ibase;\n"
"  ibase=A;\n"
"  s=scale;\n"
"  scale*=1.2;\n"
"  x=s(2*a(1)+x);\n"
"  scale=s;\n"
"  ibase=b;\n"
"  return(x/1)\n"
"}\n"

"define a(x){\n"
"  auto b,s,r,n,a,m,t,f,i,u;\n"
"  b=ibase;\n"
"  ibase=A;\n"
"  n=1;\n"
"  if(x<0){\n"
"    n=-1;\n"
"    x=-x;\n"
"  }\n"
"  if(scale<65){\n"
"    if(x==1){;\n"
"      r=.7853981633974483096156608458198757210492923498437764552437361480/n;\n"
"      ibase=b;\n"
"      return(r)\n"
"    }\n"
"    if(x==.2){;\n"
"      r=.1973955598498807583700497651947902934475851037878521015176889402/n;\n"
"      ibase=b;\n"
"      return(r)\n"
"    }\n"
"  }\n"
"  s=scale;\n"
"  if(x>.2){\n"
"    scale+=5; a=a(.2);\n"
"  }\n"
"  scale=s+3;\n"
"  while(x>.2){\n"
"    m+=1; x=(x-.2)/(1+.2*x);\n"
"  }\n"
"  r=u=x; f=-x*x; t=1;\n"
"  for(i=3;t;i+=2){;\n"
"    u*=f; t=u/i; r+=t;\n"
"  }\n"
"  scale=s;\n"
"  ibase=b;\n"
"  return((m*a+r)/n)\n"
"}\n"

"define j(n,x){\n"
"  auto b,s,o,a,i,v,f;\n"
"  b=ibase;\n"
"  ibase=A;\n"
"  s=scale;\n"
"  scale=0;\n"
"  n/=1;\n"
"  if(n<0){\n"
"    n=-n; o=n%2;\n"
"  }\n"
"  a=1;\n"
"  for(i=2;i<=n;++i)a*=i;\n"
"  scale=1.5*s;\n"
"  a=(x^n)/2^n/a;\n"
"  r=v=1;\n"
"  f=-x*x/4;\n"
"  scale+=length(a)-scale(a);\n"
"  for(i=1;v;++i){;\n"
"    v=v*f/i/(n+i); r+=v;\n"
"  }\n"
"  scale=s;\n"
"  ibase=b;\n"
"  if(o)a=-a;\n"
"  return(a*r/1)\n"
"}\n";

static void bc_vec_grow(BcVec *v, unsigned long n) {
  unsigned long old = v->cap;

  while (v->cap < v->len + n) v->cap *= 2;
  if (old != v->cap) v->v = xrealloc(v->v, v->size * v->cap);
}

void bc_vec_init(BcVec *v, size_t esize, BcVecFree dtor) {
  v->size = esize;
  v->cap = BC_VEC_START_CAP;
  v->len = 0;
  v->dtor = dtor;
  v->v = xmalloc(esize * BC_VEC_START_CAP);
}

void bc_vec_expand(BcVec *v, size_t req) {
  if (v->cap < req) {
    v->v = xrealloc(v->v, v->size * req);
    v->cap = req;
  }
}

void bc_vec_npop(BcVec *v, size_t n) {
  if (!v->dtor) v->len -= n;
  else {
    size_t len = v->len - n;
    while (v->len > len) v->dtor(v->v + (v->size * --v->len));
  }
}

void bc_vec_npush(BcVec *v, size_t n, void *data) {
  bc_vec_grow(v, n);
  memcpy(v->v + (v->size * v->len), data, v->size * n);
  v->len += n;
}

void bc_vec_push(BcVec *v, void *data) {
  bc_vec_npush(v, 1, data);
}

void bc_vec_pushByte(BcVec *v, char data) {
  bc_vec_push(v, &data);
}

void bc_vec_pushIndex(BcVec *v, size_t idx) {

  char amt, nums[sizeof(size_t)];

  for (amt = 0; idx; ++amt) {
    nums[amt] = (char) idx;
    idx &= ((size_t) ~(UCHAR_MAX));
    idx >>= sizeof(char) * CHAR_BIT;
  }

  bc_vec_push(v, &amt);
  bc_vec_npush(v, amt, nums);
}

static void bc_vec_pushAt(BcVec *v, void *data, size_t idx) {

  if (idx == v->len) bc_vec_push(v, data);
  else {

    char *ptr;

    bc_vec_grow(v, 1);

    ptr = v->v + v->size * idx;

    memmove(ptr + v->size, ptr, v->size * (v->len++ - idx));
    memmove(ptr, data, v->size);
  }
}

void bc_vec_string(BcVec *v, size_t len, char *str) {

  bc_vec_npop(v, v->len);
  bc_vec_expand(v, len + 1);
  memcpy(v->v, str, len);
  v->len = len;

  bc_vec_pushByte(v, '\0');
}

void bc_vec_concat(BcVec *v, char *str) {
  unsigned long len;

  if (!v->len) bc_vec_pushByte(v, '\0');

  len = strlen(str);
  bc_vec_grow(v, len);
  strcpy(v->v+v->len-1, str);
  v->len += len;
}

void bc_vec_empty(BcVec *v) {
  bc_vec_npop(v, v->len);
  bc_vec_pushByte(v, '\0');
}

void* bc_vec_item(BcVec *v, size_t idx) {
  return v->v + v->size * idx;
}

void* bc_vec_item_rev(BcVec *v, size_t idx) {
  return v->v + v->size * (v->len - idx - 1);
}

void bc_vec_free(void *vec) {
  BcVec *v = (BcVec*) vec;
  bc_vec_npop(v, v->len);
  free(v->v);
}

static size_t bc_map_find(BcVec *v, struct str_len *ptr) {

  size_t low = 0, high = v->len;

  while (low < high) {

    size_t mid = (low + high) / 2;
    struct str_len *id = bc_vec_item(v, mid);
    int result = bc_id_cmp(ptr, id);

    if (!result) return mid;
    else if (result < 0) high = mid;
    else low = mid + 1;
  }

  return low;
}

int bc_map_insert(BcVec *v, struct str_len *ptr, size_t *i) {

  *i = bc_map_find(v, ptr);

  if (*i == v->len) bc_vec_push(v, ptr);
  else if (!bc_id_cmp(ptr, bc_vec_item(v, *i))) return 0;
  else bc_vec_pushAt(v, ptr, *i);

  return 1;
}

size_t bc_map_index(BcVec *v, struct str_len *ptr) {
  size_t i = bc_map_find(v, ptr);
  if (i >= v->len) return SIZE_MAX;
  return bc_id_cmp(ptr, bc_vec_item(v, i)) ? SIZE_MAX : i;
}

static int bc_read_binary(char *buf, size_t size) {

  size_t i;

  for (i = 0; i < size; ++i)
    if ((buf[i]<' ' && !isspace(buf[i])) || buf[i]>'~') return 1;

  return 0;
}

BcStatus bc_read_chars(BcVec *vec, char *prompt) {

  int i;
  signed char c = 0;

  bc_vec_npop(vec, vec->len);

  if (BC_TTYIN && !FLAG(s)) {
    fputs(prompt, stderr);
    fflush(stderr);
  }

  while (!TT.sig && c != '\n') {

    i = fgetc(stdin);

    if (i == EOF) {

      if (errno == EINTR) {

        if (TT.sig == SIGTERM || TT.sig == SIGQUIT) return BC_STATUS_SIGNAL;

        TT.sig = 0;

        if (BC_TTYIN) {
          fputs(bc_program_ready_msg, stderr);
          if (!FLAG(s)) fputs(prompt, stderr);
          fflush(stderr);
        }
        else return BC_STATUS_SIGNAL;

        continue;
      }

      bc_vec_pushByte(vec, '\0');
      return BC_STATUS_EOF;
    }

    c = (signed char) i;
    bc_vec_push(vec, &c);
  }

  bc_vec_pushByte(vec, '\0');

  return TT.sig ? BC_STATUS_SIGNAL : BC_STATUS_SUCCESS;
}

BcStatus bc_read_line(BcVec *vec, char *prompt) {

  BcStatus s;

  // We are about to output to stderr, so flush stdout to
  // make sure that we don't get the outputs mixed up.
  fflush(stdout);

  s = bc_read_chars(vec, prompt);
  if (s && s != BC_STATUS_EOF) return s;
  if (bc_read_binary(vec->v, vec->len - 1))
    return bc_vm_verr(BC_ERROR_VM_BIN_FILE, "<stdin>");

  return BC_STATUS_SUCCESS;
}

BcStatus bc_read_file(char *path, char **buf) {

  BcError e = BC_ERROR_VM_IO_ERR;
  FILE *f;
  size_t size, read;
  long res;
  struct stat pstat;

  f = fopen(path, "r");
  if (!f) return bc_vm_verr(BC_ERROR_EXEC_FILE_ERR, path);
  if (fstat(fileno(f), &pstat) == -1) goto malloc_err;

  if (S_ISDIR(pstat.st_mode)) {
    e = BC_ERROR_VM_PATH_DIR;
    goto malloc_err;
  }

  if (fseek(f, 0, SEEK_END) == -1) goto malloc_err;
  res = ftell(f);
  if (res < 0) goto malloc_err;
  if (fseek(f, 0, SEEK_SET) == -1) goto malloc_err;

  size = (size_t) res;
  *buf = xmalloc(size + 1);

  read = fread(*buf, 1, size, f);
  if (read != size) goto read_err;

  (*buf)[size] = '\0';

  if (bc_read_binary(*buf, size)) {
    e = BC_ERROR_VM_BIN_FILE;
    goto read_err;
  }

  fclose(f);

  return BC_STATUS_SUCCESS;

read_err:
  free(*buf);
malloc_err:
  fclose(f);
  return bc_vm_verr(e, path);
}

static void bc_num_setToZero(BcNum *n, size_t scale) {
  n->len = 0;
  n->neg = 0;
  n->rdx = scale;
}

void bc_num_one(BcNum *n) {
  bc_num_setToZero(n, 0);
  n->len = 1;
  n->num[0] = 1;
}

void bc_num_ten(BcNum *n) {
  bc_num_setToZero(n, 0);
  n->len = 2;
  n->num[0] = 0;
  n->num[1] = 1;
}

static size_t bc_num_log10(size_t i) {
  size_t len;
  for (len = 1; i; i /= 10, ++len);
  return len;
}

static BcStatus bc_num_subArrays(signed char *a, signed char *b, size_t len)
{
  size_t i, j;
  for (i = 0; !TT.sig && i < len; ++i) {
    for (a[i] -= b[i], j = 0; !TT.sig && a[i + j] < 0;) {
      a[i + j++] += 10;
      a[i + j] -= 1;
    }
  }
  return TT.sig ? BC_STATUS_SIGNAL : BC_STATUS_SUCCESS;
}

static ssize_t bc_num_compare(signed char *a, signed char *b, size_t len)
{
  size_t i;
  int c = 0;

  for (i = len - 1; !TT.sig && i < len && !(c = a[i] - b[i]); --i);
  return BC_NUM_NEG(i + 1, c < 0);
}

ssize_t bc_num_cmp(BcNum *a, BcNum *b) {

  size_t i, min, a_int, b_int, diff;
  signed char *max_num, *min_num;
  int a_max, neg = 0;
  ssize_t cmp;

  if (a == b) return 0;
  if (!a->len) return BC_NUM_NEG(b->len != 0, !b->neg);
  if (!b->len) return BC_NUM_CMP_ZERO(a);
  if (a->neg) {
    if (b->neg) neg = 1;
    else return -1;
  } else if (b->neg) return 1;

  a_int = BC_NUM_INT(a);
  b_int = BC_NUM_INT(b);
  a_int -= b_int;
  a_max = (a->rdx > b->rdx);

  if (a_int) return neg ? -((ssize_t) a_int) : (ssize_t) a_int;

  if (a_max) {
    min = b->rdx;
    diff = a->rdx - b->rdx;
    max_num = a->num + diff;
    min_num = b->num;
  } else {
    min = a->rdx;
    diff = b->rdx - a->rdx;
    max_num = b->num + diff;
    min_num = a->num;
  }

  cmp = bc_num_compare(max_num, min_num, b_int + min);
  if (cmp) return BC_NUM_NEG(cmp, (!a_max) != neg);

  for (max_num -= diff, i = diff - 1; !TT.sig && i < diff; --i) {
    if (max_num[i]) return BC_NUM_NEG(1, (!a_max) != neg);
  }

  return 0;
}

static void bc_num_clean(BcNum *n) {
  while (n->len && !n->num[n->len - 1]) --n->len;
  if (!n->len) n->neg = 0;
  else if (n->len < n->rdx) n->len = n->rdx;
}

void bc_num_truncate(BcNum *n, size_t places) {

  if (!places) return;

  n->rdx -= places;

  if (n->len) {
    n->len -= places;
    memmove(n->num, n->num + places, n->len);
    bc_num_clean(n);
  }
}

static void bc_num_extend(BcNum *n, size_t places) {

  size_t len = n->len + places;

  if (!places) return;

  if (n->cap < len) bc_num_expand(n, len);

  memmove(n->num + places, n->num, n->len);
  memset(n->num, 0, places);

  if (n->len) n->len += places;

  n->rdx += places;
}

static void bc_num_retireMul(BcNum *n, size_t scale, int neg1, int neg2) {

  if (n->rdx < scale) bc_num_extend(n, scale - n->rdx);
  else bc_num_truncate(n, n->rdx - scale);

  bc_num_clean(n);
  if (n->len) n->neg = (!neg1 != !neg2);
}

static void bc_num_split(BcNum *n, size_t idx, BcNum *a, BcNum *b) {

  if (idx < n->len) {

    b->len = n->len - idx;
    a->len = idx;
    a->rdx = b->rdx = 0;

    memcpy(b->num, n->num + idx, b->len);
    memcpy(a->num, n->num, idx);

    bc_num_clean(b);
  }
  else bc_num_copy(a, n);

  bc_num_clean(a);
}

static BcStatus bc_num_shift(BcNum *n, size_t places) {

  if (!places || !n->len) return BC_STATUS_SUCCESS;
  if (places + n->len > BC_MAX_NUM)
    return bc_vm_verr(BC_ERROR_MATH_OVERFLOW, "shifted left too far");

  if (n->rdx >= places) n->rdx -= places;
  else {
    bc_num_extend(n, places - n->rdx);
    n->rdx = 0;
  }

  bc_num_clean(n);

  return BC_STATUS_SUCCESS;
}

static BcStatus bc_num_inv(BcNum *a, BcNum *b, size_t scale) {

  BcNum one;
  signed char num[2];

  one.cap = 2;
  one.num = num;
  bc_num_one(&one);

  return bc_num_div(&one, a, b, scale);
}

static unsigned int bc_num_addDigit(signed char *num, unsigned int d, unsigned int c)
{
  d += c;
  *num = d % 10;
  return d / 10;
}

static BcStatus bc_num_a(BcNum *a, BcNum *b, BcNum *c, size_t sub) {

  signed char *ptr, *ptr_a, *ptr_b, *ptr_c;
  size_t i, max, min_rdx, min_int, diff, a_int, b_int;
  unsigned int carry;

  // Because this function doesn't need to use scale (per the bc spec),
  // I am hijacking it to say whether it's doing an add or a subtract.

  if (!a->len) {
    bc_num_copy(c, b);
    if (sub && c->len) c->neg = !c->neg;
    return BC_STATUS_SUCCESS;
  }
  if (!b->len) {
    bc_num_copy(c, a);
    return BC_STATUS_SUCCESS;
  }

  c->neg = a->neg;
  c->rdx = maxof(a->rdx, b->rdx);
  min_rdx = minof(a->rdx, b->rdx);

  if (a->rdx > b->rdx) {
    diff = a->rdx - b->rdx;
    ptr = a->num;
    ptr_a = a->num + diff;
    ptr_b = b->num;
  }
  else {
    diff = b->rdx - a->rdx;
    ptr = b->num;
    ptr_a = a->num;
    ptr_b = b->num + diff;
  }

  for (ptr_c = c->num, i = 0; i < diff; ++i) ptr_c[i] = ptr[i];

  c->len = diff;
  ptr_c += diff;
  a_int = BC_NUM_INT(a);
  b_int = BC_NUM_INT(b);

  if (a_int > b_int) {
    min_int = b_int;
    max = a_int;
    ptr = ptr_a;
  }
  else {
    min_int = a_int;
    max = b_int;
    ptr = ptr_b;
  }

  for (carry = 0, i = 0; !TT.sig && i < min_rdx + min_int; ++i) {
    unsigned int in = (unsigned int) (ptr_a[i] + ptr_b[i]);
    carry = bc_num_addDigit(ptr_c + i, in, carry);
  }

  for (; !TT.sig && i < max + min_rdx; ++i)
    carry = bc_num_addDigit(ptr_c + i, (unsigned int) ptr[i], carry);

  c->len += i;

  if (carry) c->num[c->len++] = carry;

  return TT.sig ? BC_STATUS_SIGNAL : BC_STATUS_SUCCESS;
}

static BcStatus bc_num_s(BcNum *a, BcNum *b, BcNum *c, size_t sub) {

  BcStatus s;
  ssize_t cmp;
  BcNum *minuend, *subtrahend;
  size_t start;
  int aneg, bneg, neg;

  // Because this function doesn't need to use scale (per the bc spec),
  // I am hijacking it to say whether it's doing an add or a subtract.

  if (!a->len) {
    bc_num_copy(c, b);
    if (sub && c->len) c->neg = !c->neg;
    return BC_STATUS_SUCCESS;
  }
  if (!b->len) {
    bc_num_copy(c, a);
    return BC_STATUS_SUCCESS;
  }

  aneg = a->neg;
  bneg = b->neg;
  a->neg = b->neg = 0;

  cmp = bc_num_cmp(a, b);

  a->neg = aneg;
  b->neg = bneg;

  if (!cmp) {
    bc_num_setToZero(c, maxof(a->rdx, b->rdx));
    return BC_STATUS_SUCCESS;
  }

  if (cmp > 0) {
    neg = a->neg;
    minuend = a;
    subtrahend = b;
  }
  else {
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
  }
  else start = c->rdx - subtrahend->rdx;

  s = bc_num_subArrays(c->num + start, subtrahend->num, subtrahend->len);

  bc_num_clean(c);

  return s;
}

static BcStatus bc_num_k(BcNum *a, BcNum *b, BcNum *c) {

  BcStatus s;
  size_t max = maxof(a->len, b->len), max2 = (max + 1) / 2;
  BcNum l1, h1, l2, h2, m2, m1, z0, z1, z2, temp;
  int aone = BC_NUM_ONE(a);

  // This is here because the function is recursive.
  if (TT.sig) return BC_STATUS_SIGNAL;
  if (!a->len || !b->len) {
    bc_num_setToZero(c, 0);
    return BC_STATUS_SUCCESS;
  }
  if (aone || BC_NUM_ONE(b)) {
    bc_num_copy(c, aone ? b : a);
    return BC_STATUS_SUCCESS;
  }

  // check karatsuba length
  if (a->len + b->len < 32 || a->len < 32 || b->len < 32)
  {
    size_t i, j, len;
    unsigned int carry;
    signed char *ptr_c;

    bc_num_expand(c, a->len + b->len + 1);

    ptr_c = c->num;
    memset(ptr_c, 0, c->cap);
    c->len = len = 0;

    for (i = 0; !TT.sig && i < b->len; ++i) {

      signed char *ptr = ptr_c + i;

      carry = 0;

      for (j = 0; !TT.sig && j < a->len; ++j) {
        unsigned int in = (char) ptr[j];
        in += ((unsigned int) a->num[j]) * ((unsigned int) b->num[i]);
        carry = bc_num_addDigit(ptr + j, in, carry);
      }
// todo: is this typecast useless?
      ptr[j] += (signed) carry;
      len = maxof(len, i + j + (carry != 0));
    }

    c->len = len;

    return TT.sig ? BC_STATUS_SIGNAL : BC_STATUS_SUCCESS;
  }

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

  bc_num_split(a, max2, &l1, &h1);
  bc_num_split(b, max2, &l2, &h2);

  s = bc_num_add(&h1, &l1, &m1, 0);
  if (s) goto err;
  s = bc_num_add(&h2, &l2, &m2, 0);
  if (s) goto err;

  s = bc_num_k(&h1, &h2, &z0);
  if (s) goto err;
  s = bc_num_k(&m1, &m2, &z1);
  if (s) goto err;
  s = bc_num_k(&l1, &l2, &z2);
  if (s) goto err;

  s = bc_num_sub(&z1, &z0, &temp, 0);
  if (s) goto err;
  s = bc_num_sub(&temp, &z2, &z1, 0);
  if (s) goto err;

  s = bc_num_shift(&z0, max2 * 2);
  if (s) goto err;
  s = bc_num_shift(&z1, max2);
  if (s) goto err;
  s = bc_num_add(&z0, &z1, &temp, 0);
  if (s) goto err;
  s = bc_num_add(&temp, &z2, c, 0);

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
  return s;
}

static BcStatus bc_num_m(BcNum *a, BcNum *b, BcNum *c, size_t scale) {

  BcStatus s;
  BcNum cpa, cpb;
  size_t maxrdx = maxof(a->rdx, b->rdx);

  scale = maxof(scale, a->rdx);
  scale = maxof(scale, b->rdx);
  scale = minof(a->rdx + b->rdx, scale);
  maxrdx = maxof(maxrdx, scale);

  bc_num_createCopy(&cpa, a);
  bc_num_createCopy(&cpb, b);

  cpa.neg = cpb.neg = 0;

  s = bc_num_shift(&cpa, maxrdx);
  if (s) goto err;
  s = bc_num_shift(&cpb, maxrdx);
  if (s) goto err;
  s = bc_num_k(&cpa, &cpb, c);
  if (s) goto err;

  maxrdx += scale;
  bc_num_expand(c, c->len + maxrdx);

  if (c->len < maxrdx) {
    memset(c->num + c->len, 0, c->cap - c->len);
    c->len += maxrdx;
  }

  c->rdx = maxrdx;
  bc_num_retireMul(c, scale, a->neg, b->neg);

err:
  bc_num_free(&cpb);
  bc_num_free(&cpa);
  return s;
}

static BcStatus bc_num_d(BcNum *a, BcNum *b, BcNum *c, size_t scale) {

  BcStatus s = BC_STATUS_SUCCESS;
  signed char *n, *p, q;
  size_t len, end, i;
  BcNum cp;
  int zero = 1;

  if (!b->len) return bc_vm_err(BC_ERROR_MATH_DIVIDE_BY_ZERO);
  if (!a->len) {
    bc_num_setToZero(c, scale);
    return BC_STATUS_SUCCESS;
  }
  if (BC_NUM_ONE(b)) {
    bc_num_copy(c, a);
    bc_num_retireMul(c, scale, a->neg, b->neg);
    return BC_STATUS_SUCCESS;
  }

  bc_num_init(&cp, bc_num_mulReq(a, b, scale));
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
    for (i = 0; zero && i < len; ++i) zero = !b->num[len - i - 1];
    len -= i - 1;
  }

  if (cp.cap == cp.len) bc_num_expand(&cp, cp.len + 1);

  // We want an extra zero in front to make things simpler.
  cp.num[cp.len++] = 0;
  end = cp.len - len;

  bc_num_expand(c, cp.len);

  memset(c->num + end, 0, c->cap - end);
  c->rdx = cp.rdx;
  c->len = cp.len;
  p = b->num;

  for (i = end - 1; !TT.sig && !s && i < end; --i) {
    n = cp.num + i;
    for (q = 0; !s && (n[len] || bc_num_compare(n, p, len) >= 0); ++q)
      s = bc_num_subArrays(n, p, len);
    c->num[i] = q;
  }

  if (!s) bc_num_retireMul(c, scale, a->neg, b->neg);
  bc_num_free(&cp);

  return s;
}

static BcStatus bc_num_r(BcNum *a, BcNum *b, BcNum *c, BcNum *d, size_t scale,
                  size_t ts)
{
  BcStatus s;
  BcNum temp;
  int neg;

  if (!b->len) return bc_vm_err(BC_ERROR_MATH_DIVIDE_BY_ZERO);
  if (!a->len) {
    bc_num_setToZero(c, ts);
    bc_num_setToZero(d, ts);
    return BC_STATUS_SUCCESS;
  }

  bc_num_init(&temp, d->cap);
  bc_num_d(a, b, c, scale);

  if (scale) scale = ts;

  s = bc_num_m(c, b, &temp, scale);
  if (s) goto err;
  s = bc_num_sub(a, &temp, d, scale);
  if (s) goto err;

  if (ts > d->rdx && d->len) bc_num_extend(d, ts - d->rdx);

  neg = d->neg;
  bc_num_retireMul(d, ts, a->neg, b->neg);
  d->neg = neg;

err:
  bc_num_free(&temp);
  return s;
}

static BcStatus bc_num_rem(BcNum *a, BcNum *b, BcNum *c, size_t scale) {

  BcStatus s;
  BcNum c1;
  size_t ts = maxof(scale + b->rdx, a->rdx), len = bc_num_mulReq(a, b, ts);

  bc_num_init(&c1, len);
  s = bc_num_r(a, b, &c1, c, scale, ts);
  bc_num_free(&c1);

  return s;
}

static BcStatus bc_num_p(BcNum *a, BcNum *b, BcNum *c, size_t scale) {

  BcStatus s = BC_STATUS_SUCCESS;
  BcNum copy;
  unsigned long pow = 0;
  size_t i, powrdx, resrdx;
  int neg, zero;

  if (b->rdx) return bc_vm_err(BC_ERROR_MATH_NON_INTEGER);

  if (!b->len) {
    bc_num_one(c);
    return BC_STATUS_SUCCESS;
  }
  if (!a->len) {
    if (b->neg) return bc_vm_err(BC_ERROR_MATH_DIVIDE_BY_ZERO);
    bc_num_setToZero(c, scale);
    return BC_STATUS_SUCCESS;
  }
  if (BC_NUM_ONE(b)) {
    if (!b->neg) bc_num_copy(c, a);
    else s = bc_num_inv(a, c, scale);
    return s;
  }

  neg = b->neg;
  b->neg = 0;
  s = bc_num_ulong(b, &pow);
  b->neg = neg;
  if (s) return s;

  bc_num_createCopy(&copy, a);

  if (!neg) scale = minof(a->rdx * pow, maxof(scale, a->rdx));

  for (powrdx = a->rdx; !TT.sig && !(pow & 1); pow >>= 1) {
    powrdx <<= 1;
    s = bc_num_mul(&copy, &copy, &copy, powrdx);
    if (s) goto err;
  }

  if (TT.sig) {
    s = BC_STATUS_SIGNAL;
    goto err;
  }

  bc_num_copy(c, &copy);
  resrdx = powrdx;

  while (!TT.sig && (pow >>= 1)) {

    powrdx <<= 1;
    s = bc_num_mul(&copy, &copy, &copy, powrdx);
    if (s) goto err;

    if (pow & 1) {
      resrdx += powrdx;
      s = bc_num_mul(c, &copy, c, resrdx);
      if (s) goto err;
    }
  }

  if (neg) {
    s = bc_num_inv(c, c, scale);
    if (s) goto err;
  }

  if (TT.sig) {
    s = BC_STATUS_SIGNAL;
    goto err;
  }

  if (c->rdx > scale) bc_num_truncate(c, c->rdx - scale);

  // We can't use bc_num_clean() here.
  for (zero = 1, i = 0; zero && i < c->len; ++i) zero = !c->num[i];
  if (zero) bc_num_setToZero(c, scale);

err:
  bc_num_free(&copy);
  return s;
}

static BcStatus bc_num_binary(BcNum *a, BcNum *b, BcNum *c, size_t scale,
                              BcNumBinaryOp op, size_t req)
{
  BcStatus s;
  BcNum num2, *ptr_a, *ptr_b;
  int init = 0;

  if (c == a) {
    ptr_a = &num2;
    memcpy(ptr_a, c, sizeof(BcNum));
    init = 1;
  }
  else ptr_a = a;

  if (c == b) {
    ptr_b = &num2;
    if (c != a) {
      memcpy(ptr_b, c, sizeof(BcNum));
      init = 1;
    }
  }
  else ptr_b = b;

  if (init) bc_num_init(c, req);
  else bc_num_expand(c, req);

  s = op(ptr_a, ptr_b, c, scale);

  if (init) bc_num_free(&num2);

  return s;
}

static unsigned long bc_num_parseChar(char c, size_t base_t) {

  if (isupper(c)) {
    c += 10 - 'A';
    if (c >= base_t) c = base_t - 1;
  } else c -= '0';

  return c;
}

static BcStatus bc_num_parseBase(BcNum *n, char *val,
                                 BcNum *base, size_t base_t)
{
  BcStatus s = BC_STATUS_SUCCESS;
  BcNum temp, mult, result;
  signed char c = 0;
  int zero = 1;
  unsigned long v;
  size_t i, digits, len = strlen(val);

  for (i = 0; zero && i < len; ++i) zero = (val[i] == '.' || val[i] == '0');
  if (zero) return BC_STATUS_SUCCESS;

  bc_num_init(&temp, BC_NUM_LONG_LOG10);
  bc_num_init(&mult, BC_NUM_LONG_LOG10);

  for (i = 0; i < len && (c = val[i]) && c != '.'; ++i) {

    v = bc_num_parseChar(c, base_t);

    s = bc_num_mul(n, base, &mult, 0);
    if (s) goto int_err;
    bc_num_ulong2num(&temp, v);
    s = bc_num_add(&mult, &temp, n, 0);
    if (s) goto int_err;
  }

  if (i == len && !(c = val[i])) goto int_err;

  bc_num_init(&result, base->len);
  bc_num_one(&mult);

  for (i += 1, digits = 0; i < len && (c = val[i]); ++i, ++digits) {

    v = bc_num_parseChar(c, base_t);

    s = bc_num_mul(&result, base, &result, 0);
    if (s) goto err;
    bc_num_ulong2num(&temp, v);
    s = bc_num_add(&result, &temp, &result, 0);
    if (s) goto err;
    s = bc_num_mul(&mult, base, &mult, 0);
    if (s) goto err;
  }

  s = bc_num_div(&result, &mult, &result, digits);
  if (s) goto err;
  s = bc_num_add(n, &result, n, digits);
  if (s) goto err;

  if (n->len) {
    if (n->rdx < digits) bc_num_extend(n, digits - n->rdx);
  }
  else bc_num_setToZero(n, 0);


err:
  bc_num_free(&result);
int_err:
  bc_num_free(&mult);
  bc_num_free(&temp);
  return s;
}

static void bc_num_printNewline() {
  if (TT.nchars >= TT.line_len - 1) {
    putchar('\\');
    putchar('\n');
    TT.nchars = 0;
  }
}

static void bc_num_printDigits(size_t n, size_t len, int rdx) {

  size_t exp, pow;

  bc_num_printNewline();
  putchar(rdx ? '.' : ' ');
  ++TT.nchars;

  bc_num_printNewline();
  for (exp = 0, pow = 1; exp < len - 1; ++exp, pow *= 10);

  for (exp = 0; exp < len; pow /= 10, ++TT.nchars, ++exp) {
    size_t dig;
    bc_num_printNewline();
    dig = n / pow;
    n -= dig * pow;
    putchar(((char) dig) + '0');
  }
}

static void bc_num_printHex(size_t n, size_t len, int rdx) {

  if (rdx) {
    bc_num_printNewline();
    putchar('.');
    TT.nchars += 1;
  }

  bc_num_printNewline();
  putchar(bc_num_hex_digits[n]);
  TT.nchars += len;
}

static void bc_num_printDecimal(BcNum *n) {

  size_t i, rdx = n->rdx - 1;

  if (n->neg) putchar('-');
  TT.nchars += n->neg;

  for (i = n->len - 1; i < n->len; --i)
    bc_num_printHex((size_t) n->num[i], 1, i == rdx);
}

static BcStatus bc_num_printNum(BcNum *n, BcNum *base,
                                size_t len, BcNumDigitOp print)
{
  BcStatus s;
  BcVec stack;
  BcNum intp, fracp, digit, frac_len;
  unsigned long dig, *ptr;
  size_t i;
  int radix;

  if (!n->len) {
    print(0, len, 0);
    return BC_STATUS_SUCCESS;
  }

  bc_vec_init(&stack, sizeof(unsigned long), NULL);
  bc_num_init(&fracp, n->rdx);
  bc_num_init(&digit, len);
  bc_num_init(&frac_len, BC_NUM_INT(n));
  bc_num_one(&frac_len);
  bc_num_createCopy(&intp, n);

  bc_num_truncate(&intp, intp.rdx);
  s = bc_num_sub(n, &intp, &fracp, 0);
  if (s) goto err;

  while (intp.len) {
    s = bc_num_divmod(&intp, base, &intp, &digit, 0);
    if (s) goto err;
    s = bc_num_ulong(&digit, &dig);
    if (s) goto err;
    bc_vec_push(&stack, &dig);
  }

  for (i = 0; i < stack.len; ++i) {
    ptr = bc_vec_item_rev(&stack, i);
    print(*ptr, len, 0);
  }

  if (!n->rdx) goto err;

  for (radix = 1; frac_len.len <= n->rdx; radix = 0) {
    s = bc_num_mul(&fracp, base, &fracp, n->rdx);
    if (s) goto err;
    s = bc_num_ulong(&fracp, &dig);
    if (s) goto err;
    bc_num_ulong2num(&intp, dig);
    s = bc_num_sub(&fracp, &intp, &fracp, 0);
    if (s) goto err;
    print(dig, len, radix);
    s = bc_num_mul(&frac_len, base, &frac_len, 0);
    if (s) goto err;
  }

err:
  bc_num_free(&frac_len);
  bc_num_free(&digit);
  bc_num_free(&fracp);
  bc_num_free(&intp);
  bc_vec_free(&stack);
  return s;
}

static BcStatus bc_num_printBase(BcNum *n, BcNum *base, size_t base_t) {

  BcStatus s;
  size_t width;
  BcNumDigitOp print;
  int neg = n->neg;

  if (neg) putchar('-');
  TT.nchars += neg;

  n->neg = 0;

  if (base_t <= 16) {
    width = 1;
    print = bc_num_printHex;
  } else {
    width = bc_num_log10(base_t - 1) - 1;
    print = bc_num_printDigits;
  }

  s = bc_num_printNum(n, base, width, print);
  n->neg = neg;

  return s;
}

void bc_num_setup(BcNum *n, signed char *num, size_t cap) {
  n->num = num;
  n->cap = cap;
  n->rdx = n->len = 0;
  n->neg = 0;
}

void bc_num_init(BcNum *n, size_t req) {
  req = req >= BC_NUM_DEF_SIZE ? req : BC_NUM_DEF_SIZE;
  bc_num_setup(n, xmalloc(req), req);
}

void bc_num_expand(BcNum *n, size_t req) {
  req = req >= BC_NUM_DEF_SIZE ? req : BC_NUM_DEF_SIZE;
  if (req > n->cap) {
    n->num = xrealloc(n->num, req);
    n->cap = req;
  }
}

void bc_num_free(void *num) {
  free(((BcNum*) num)->num);
}

void bc_num_copy(BcNum *d, BcNum *s) {
  if (d == s) return;
  bc_num_expand(d, s->len);
  d->len = s->len;
  d->neg = s->neg;
  d->rdx = s->rdx;
  memcpy(d->num, s->num, d->len);
}

void bc_num_createCopy(BcNum *d, BcNum *s) {
  bc_num_init(d, s->len);
  bc_num_copy(d, s);
}

void bc_num_createFromUlong(BcNum *n, unsigned long val) {
  bc_num_init(n, BC_NUM_LONG_LOG10);
  bc_num_ulong2num(n, val);
}

BcStatus bc_num_parse(BcNum *n, char *val,
                      BcNum *base, size_t base_t, int letter)
{
  BcStatus s = BC_STATUS_SUCCESS;

  if (letter) bc_num_ulong2num(n, bc_num_parseChar(val[0], 'Z'+11));
  else if (base_t == 10) {
    size_t len, i;
    char *ptr;
    int zero = 1;

    while (*val == '0') val++;

    len = strlen(val);
    if (len) {
      for (i = 0; zero && i < len; ++i) zero = (val[i] == '0') || val[i] == '.';
      bc_num_expand(n, len);
    }
    ptr = strchr(val, '.');
    n->rdx = ptr ? (val + len) - (ptr + 1) : 0;

    if (!zero) {
      for (i = len - 1; i < len; ++n->len, --i) {

        char c = val[i];

        if (c == '.') n->len -= 1;
        else {
          if (isupper(c)) c = '9';
          n->num[n->len] = c - '0';
        }
      }
    }
  } else s = bc_num_parseBase(n, val, base, base_t);

  return s;
}

BcStatus bc_num_ulong(BcNum *n, unsigned long *result) {

  size_t i;
  unsigned long r;

  *result = 0;

  if (n->neg) return bc_vm_err(BC_ERROR_MATH_NEGATIVE);

  for (r = 0, i = n->len; i > n->rdx;) {

    unsigned long prev = r * 10;

    if (prev == SIZE_MAX || prev / 10 != r)
      return bc_vm_err(BC_ERROR_MATH_OVERFLOW);

    r = prev + ((char) n->num[--i]);

    if (r == SIZE_MAX || r < prev) return bc_vm_err(BC_ERROR_MATH_OVERFLOW);
  }

  *result = r;

  return BC_STATUS_SUCCESS;
}

void bc_num_ulong2num(BcNum *n, unsigned long val) {

  size_t len;
  signed char *ptr;
  unsigned long i;

  bc_num_setToZero(n, 0);

  if (!val) return;

  len = bc_num_log10(ULONG_MAX);
  bc_num_expand(n, len);
  for (ptr = n->num, i = 0; val; ++i, ++n->len, val /= 10) ptr[i] = val % 10;
}

size_t bc_num_addReq(BcNum *a, BcNum *b, size_t scale) {
  return maxof(a->rdx, b->rdx) + maxof(BC_NUM_INT(a), BC_NUM_INT(b)) + 1;
}

size_t bc_num_mulReq(BcNum *a, BcNum *b, size_t scale) {
  return BC_NUM_INT(a) + BC_NUM_INT(b) + maxof(scale, a->rdx + b->rdx) + 1;
}

size_t bc_num_powReq(BcNum *a, BcNum *b, size_t scale) {
  return a->len + b->len + 1;
}

BcStatus bc_num_add(BcNum *a, BcNum *b, BcNum *c, size_t scale) {
  BcNumBinaryOp op = (!a->neg == !b->neg) ? bc_num_a : bc_num_s;
  return bc_num_binary(a, b, c, 0, op, bc_num_addReq(a, b, scale));
}

BcStatus bc_num_sub(BcNum *a, BcNum *b, BcNum *c, size_t scale) {
  BcNumBinaryOp op = (!a->neg == !b->neg) ? bc_num_s : bc_num_a;
  return bc_num_binary(a, b, c, 1, op, bc_num_addReq(a, b, scale));
}

BcStatus bc_num_mul(BcNum *a, BcNum *b, BcNum *c, size_t scale) {
  return bc_num_binary(a, b, c, scale, bc_num_m, bc_num_mulReq(a, b, scale));
}

BcStatus bc_num_div(BcNum *a, BcNum *b, BcNum *c, size_t scale) {
  return bc_num_binary(a, b, c, scale, bc_num_d, bc_num_mulReq(a, b, scale));
}

BcStatus bc_num_mod(BcNum *a, BcNum *b, BcNum *c, size_t scale) {
  return bc_num_binary(a, b, c, scale, bc_num_rem, bc_num_mulReq(a, b, scale));
}

BcStatus bc_num_pow(BcNum *a, BcNum *b, BcNum *c, size_t scale) {
  return bc_num_binary(a, b, c, scale, bc_num_p, a->len + b->len + 1);
}

BcStatus bc_num_sqrt(BcNum *a, BcNum *b, size_t scale) {

  BcStatus s = BC_STATUS_SUCCESS;
  BcNum num1, num2, half, f, fprime, *x0, *x1, *temp;
  size_t pow, len, digs, digs1, resrdx, times = 0;
  ssize_t cmp = 1, cmp1 = SSIZE_MAX, cmp2 = SSIZE_MAX;
  signed char half_digs[2];

  bc_num_init(b, maxof(scale, a->rdx) + ((BC_NUM_INT(a) + 1) >> 1) + 1);

  if (!a->len) {
    bc_num_setToZero(b, scale);
    return BC_STATUS_SUCCESS;
  }
  if (a->neg) return bc_vm_err(BC_ERROR_MATH_NEGATIVE);
  if (BC_NUM_ONE(a)) {
    bc_num_one(b);
    bc_num_extend(b, scale);
    return BC_STATUS_SUCCESS;
  }

  scale = maxof(scale, a->rdx) + 1;
  len = a->len + scale;

  bc_num_init(&num1, len);
  bc_num_init(&num2, len);
  bc_num_setup(&half, half_digs, sizeof(half_digs));

  bc_num_one(&half);
  half.num[0] = 5;
  half.rdx = 1;

  bc_num_init(&f, len);
  bc_num_init(&fprime, len);

  x0 = &num1;
  x1 = &num2;

  bc_num_one(x0);
  pow = BC_NUM_INT(a);

  if (pow) {

    if (pow & 1) x0->num[0] = 2;
    else x0->num[0] = 6;

    pow -= 2 - (pow & 1);

    bc_num_extend(x0, pow);

    // Make sure to move the radix back.
    x0->rdx -= pow;
  }

  x0->rdx = digs = digs1 = 0;
  resrdx = scale + 2;
  len = BC_NUM_INT(x0) + resrdx - 1;

  while (!TT.sig && (cmp || digs < len)) {

    s = bc_num_div(a, x0, &f, resrdx);
    if (s) goto err;
    s = bc_num_add(x0, &f, &fprime, resrdx);
    if (s) goto err;
    s = bc_num_mul(&fprime, &half, x1, resrdx);
    if (s) goto err;

    cmp = bc_num_cmp(x1, x0);
    digs = x1->len - (unsigned long long) llabs(cmp);

    if (cmp == cmp2 && digs == digs1) times += 1;
    else times = 0;

    resrdx += times > 4;

    cmp2 = cmp1;
    cmp1 = cmp;
    digs1 = digs;

    temp = x0;
    x0 = x1;
    x1 = temp;
  }

  if (TT.sig) {
    s = BC_STATUS_SIGNAL;
    goto err;
  }

  bc_num_copy(b, x0);
  scale -= 1;
  if (b->rdx > scale) bc_num_truncate(b, b->rdx - scale);

err:
  bc_num_free(&fprime);
  bc_num_free(&f);
  bc_num_free(&num2);
  bc_num_free(&num1);
  return s;
}

BcStatus bc_num_divmod(BcNum *a, BcNum *b, BcNum *c, BcNum *d, size_t scale) {

  BcStatus s;
  BcNum num2, *ptr_a;
  int init = 0;
  size_t ts = maxof(scale + b->rdx, a->rdx), len = bc_num_mulReq(a, b, ts);

  if (c == a) {
    memcpy(&num2, c, sizeof(BcNum));
    ptr_a = &num2;
    bc_num_init(c, len);
    init = 1;
  }
  else {
    ptr_a = a;
    bc_num_expand(c, len);
  }

  s = bc_num_r(ptr_a, b, c, d, scale, ts);

  if (init) bc_num_free(&num2);

  return s;
}

int bc_id_cmp(struct str_len *e1, struct str_len *e2) {
  return strcmp(e1->str, e2->str);
}

void bc_id_free(void *id) {
  free(((struct str_len *)id)->str);
}

void bc_string_free(void *string) {
  free(*((char**) string));
}

BcStatus bc_func_insert(BcFunc *f, char *name, BcType type, size_t line) {

  struct str_len a;
  size_t i;

  for (i = 0; i < f->autos.len; ++i) {
    struct str_len *id = bc_vec_item(&f->autos, i);
    if (!strcmp(name, id->str) && type == (BcType) id->len)
      return bc_vm_error(BC_ERROR_PARSE_DUP_LOCAL, line, name);
  }

  a.len = type;
  a.str = name;

  bc_vec_push(&f->autos, &a);

  return BC_STATUS_SUCCESS;
}

void bc_func_init(BcFunc *f, char *name) {
  bc_vec_init(&f->code, sizeof(char), NULL);
  bc_vec_init(&f->strs, sizeof(char*), bc_string_free);
  bc_vec_init(&f->consts, sizeof(char*), bc_string_free);
  bc_vec_init(&f->autos, sizeof(struct str_len), bc_id_free);
  bc_vec_init(&f->labels, sizeof(size_t), NULL);
  f->nparams = 0;
  f->voidfn = 0;
  f->name = name;
}

void bc_func_reset(BcFunc *f) {
  bc_vec_npop(&f->code, f->code.len);
  bc_vec_npop(&f->strs, f->strs.len);
  bc_vec_npop(&f->consts, f->consts.len);
  bc_vec_npop(&f->autos, f->autos.len);
  bc_vec_npop(&f->labels, f->labels.len);
  f->nparams = 0;
  f->voidfn = 0;
}

void bc_func_free(void *func) {
  BcFunc *f = (BcFunc*) func;
  bc_vec_free(&f->code);
  bc_vec_free(&f->strs);
  bc_vec_free(&f->consts);
  bc_vec_free(&f->autos);
  bc_vec_free(&f->labels);
}

void bc_array_init(BcVec *a, int nums) {
  if (nums) bc_vec_init(a, sizeof(BcNum), bc_num_free);
  else bc_vec_init(a, sizeof(BcVec), bc_vec_free);
  bc_array_expand(a, 1);
}

void bc_array_copy(BcVec *d, BcVec *s) {

  size_t i;

  bc_vec_npop(d, d->len);
  bc_vec_expand(d, s->cap);
  d->len = s->len;

  for (i = 0; i < s->len; ++i) {
    BcNum *dnum = bc_vec_item(d, i), *snum = bc_vec_item(s, i);
    bc_num_createCopy(dnum, snum);
  }
}

void bc_array_expand(BcVec *a, size_t len) {

  if (a->size == sizeof(BcNum) && a->dtor == bc_num_free) {
    BcNum n;
    while (len > a->len) {
      bc_num_init(&n, BC_NUM_DEF_SIZE);
      bc_vec_push(a, &n);
    }
  }
  else {
    BcVec v;
    while (len > a->len) {
      bc_array_init(&v, 1);
      bc_vec_push(a, &v);
    }
  }
}

void bc_result_free(void *result) {

  BcResult *r = (BcResult*) result;

  switch (r->t) {

    case BC_RESULT_TEMP:
    case BC_RESULT_IBASE:
    case BC_RESULT_SCALE:
    case BC_RESULT_OBASE:
    {
      bc_num_free(&r->d.n);
      break;
    }

    case BC_RESULT_VAR:
    case BC_RESULT_ARRAY:
    case BC_RESULT_ARRAY_ELEM:
    {
      free(r->d.id.str);
      break;
    }

    case BC_RESULT_STR:
    case BC_RESULT_CONSTANT:
    case BC_RESULT_VOID:
    case BC_RESULT_ONE:
    case BC_RESULT_LAST:
    {
      // Do nothing.
      break;
    }
  }
}

BcStatus bc_lex_invalidChar(BcLex *l, char c) {
  l->t = BC_LEX_INVALID;
  return bc_lex_verr(l, BC_ERROR_PARSE_CHAR, c);
}

void bc_lex_lineComment(BcLex *l) {
  l->t = BC_LEX_WHITESPACE;
  while (l->i < l->len && l->buf[l->i] != '\n') ++l->i;
}

BcStatus bc_lex_comment(BcLex *l) {

  size_t i, nlines = 0;
  char *buf = l->buf;
  int end = 0;
  char c;

  l->t = BC_LEX_WHITESPACE;

  for (i = ++l->i; !end; i += !end) {

    for (; (c = buf[i]) && c != '*'; ++i) nlines += (c == '\n');

    if (!c || buf[i + 1] == '\0') {
      l->i = i;
      return bc_lex_err(l, BC_ERROR_PARSE_COMMENT);
    }

    end = buf[i + 1] == '/';
  }

  l->i = i + 2;
  l->line += nlines;

  return BC_STATUS_SUCCESS;
}

void bc_lex_whitespace(BcLex *l) {
  char c;
  l->t = BC_LEX_WHITESPACE;
  for (c = l->buf[l->i]; c != '\n' && isspace(c); c = l->buf[++l->i]);
}

BcStatus bc_lex_number(BcLex *l, char start) {

  char *buf = l->buf + l->i;
  size_t i;
  char last_valid, c;
  int last_pt, pt = (start == '.');

  l->t = BC_LEX_NUMBER;
  last_valid = 'Z';

  bc_vec_npop(&l->str, l->str.len);
  bc_vec_push(&l->str, &start);

  for (i = 0; (c = buf[i]) && (BC_LEX_NUM_CHAR(c, last_valid, pt) ||
                               (c == '\\' && buf[i + 1] == '\n')); ++i)
  {
    if (c == '\\') {

      if (buf[i + 1] == '\n') {

        i += 2;

        // Make sure to eat whitespace at the beginning of the line.
        while(isspace(buf[i]) && buf[i] != '\n') ++i;

        c = buf[i];

        if (!BC_LEX_NUM_CHAR(c, last_valid, pt)) break;
      }
      else break;
    }

    last_pt = (c == '.');
    if (pt && last_pt) break;
    pt = pt || last_pt;

    bc_vec_push(&l->str, &c);
  }

  if (l->str.len - pt > BC_MAX_NUM)
    return bc_lex_verr(l, BC_ERROR_EXEC_NUM_LEN, BC_MAX_NUM);

  bc_vec_pushByte(&l->str, '\0');
  l->i += i;

  return BC_STATUS_SUCCESS;
}

BcStatus bc_lex_name(BcLex *l) {

  size_t i = 0;
  char *buf = l->buf + l->i - 1;
  char c = buf[i];

  l->t = BC_LEX_NAME;

  while ((c >= 'a' && c <= 'z') || isdigit(c) || c == '_') c = buf[++i];

  if (i > BC_MAX_NAME)
    return bc_lex_verr(l, BC_ERROR_EXEC_NAME_LEN, BC_MAX_NAME);

  bc_vec_string(&l->str, i, buf);

  // Increment the index. We minus 1 because it has already been incremented.
  l->i += i - 1;

  return BC_STATUS_SUCCESS;
}

void bc_lex_init(BcLex *l) {
  bc_vec_init(&l->str, sizeof(char), NULL);
}

void bc_lex_file(BcLex *l, char *file) {
  l->line = 1;
  TT.file = file;
}

BcStatus bc_lex_next(BcLex *l) {

  BcStatus s;

  l->last = l->t;
  l->line += (l->i != 0 && l->buf[l->i - 1] == '\n');

  if (l->last == BC_LEX_EOF) return bc_lex_err(l, BC_ERROR_PARSE_EOF);

  l->t = BC_LEX_EOF;

  if (l->i == l->len) return BC_STATUS_SUCCESS;

  // Loop until failure or we don't have whitespace. This
  // is so the parser doesn't get inundated with whitespace.
  do {
    s = bc_lex_token(l);
  } while (!s && l->t == BC_LEX_WHITESPACE);

  return s;
}

BcStatus bc_lex_text(BcLex *l, char *text) {
  l->buf = text;
  l->i = 0;
  l->len = strlen(text);
  l->t = l->last = BC_LEX_INVALID;
  return bc_lex_next(l);
}

static BcStatus bc_lex_identifier(BcLex *l) {

  BcStatus s;
  size_t i;
  char *buf = l->buf + l->i - 1;

  for (i = 0; i < bc_lex_kws_len; ++i) {

    BcLexKeyword *kw = bc_lex_kws + i;
    size_t len = BC_LEX_KW_LEN(kw);

    if (!strncmp(buf, kw->name, len) && !isalnum(buf[len]) && buf[len] != '_')
    {
      l->t = BC_LEX_KEY_AUTO + (BcLexType) i;

      if (!BC_LEX_KW_POSIX(kw)) {
        s = bc_lex_vposixErr(l, BC_ERROR_POSIX_KW, kw->name);
        if (s) return s;
      }

      // We minus 1 because the index has already been incremented.
      l->i += len - 1;
      return BC_STATUS_SUCCESS;
    }
  }

  s = bc_lex_name(l);
  if (s) return s;

  if (l->str.len - 1 > 1) s = bc_lex_vposixErr(l, BC_ERROR_POSIX_NAME_LEN, buf);

  return s;
}

static BcStatus bc_lex_string(BcLex *l) {

  size_t len, nlines = 0, i = l->i;
  char *buf = l->buf;
  char c;

  l->t = BC_LEX_STR;

  for (; (c = buf[i]) && c != '"'; ++i) nlines += c == '\n';

  if (c == '\0') {
    l->i = i;
    return bc_lex_err(l, BC_ERROR_PARSE_STRING);
  }

  len = i - l->i;

  if (len > BC_MAX_STRING)
    return bc_lex_verr(l, BC_ERROR_EXEC_STRING_LEN, BC_MAX_STRING);

  bc_vec_string(&l->str, len, l->buf + l->i);

  l->i = i + 1;
  l->line += nlines;

  return BC_STATUS_SUCCESS;
}

static void bc_lex_assign(BcLex *l, BcLexType with, BcLexType without) {
  if (l->buf[l->i] == '=') {
    ++l->i;
    l->t = with;
  }
  else l->t = without;
}

BcStatus bc_lex_token(BcLex *l) {

  BcStatus s = BC_STATUS_SUCCESS;
  char c = l->buf[l->i++], c2;

  // This is the workhorse of the lexer.
  switch (c) {

    case '\0':
    case '\n':
    {
      l->t = !c ? BC_LEX_EOF : BC_LEX_NLINE;
      break;
    }

    case '\t':
    case '\v':
    case '\f':
    case '\r':
    case ' ':
    {
      bc_lex_whitespace(l);
      break;
    }

    case '!':
    {
      bc_lex_assign(l, BC_LEX_OP_REL_NE, BC_LEX_OP_BOOL_NOT);

      if (l->t == BC_LEX_OP_BOOL_NOT) {
        s = bc_lex_vposixErr(l, BC_ERROR_POSIX_BOOL, "!");
        if (s) return s;
      }

      break;
    }

    case '"':
    {
      s = bc_lex_string(l);
      break;
    }

    case '#':
    {
      s = bc_lex_posixErr(l, BC_ERROR_POSIX_COMMENT);
      if (s) return s;

      bc_lex_lineComment(l);

      break;
    }

    case '%':
    {
      bc_lex_assign(l, BC_LEX_OP_ASSIGN_MODULUS, BC_LEX_OP_MODULUS);
      break;
    }

    case '&':
    {
      c2 = l->buf[l->i];
      if (c2 == '&') {

        s = bc_lex_vposixErr(l, BC_ERROR_POSIX_BOOL, "&&");
        if (s) return s;

        ++l->i;
        l->t = BC_LEX_OP_BOOL_AND;
      }
      else s = bc_lex_invalidChar(l, c);

      break;
    }
    case '(':
    case ')':
    {
      l->t = (BcLexType) (c - '(' + BC_LEX_LPAREN);
      break;
    }

    case '*':
    {
      bc_lex_assign(l, BC_LEX_OP_ASSIGN_MULTIPLY, BC_LEX_OP_MULTIPLY);
      break;
    }

    case '+':
    {
      c2 = l->buf[l->i];
      if (c2 == '+') {
        ++l->i;
        l->t = BC_LEX_OP_INC;
      }
      else bc_lex_assign(l, BC_LEX_OP_ASSIGN_PLUS, BC_LEX_OP_PLUS);
      break;
    }

    case ',':
    {
      l->t = BC_LEX_COMMA;
      break;
    }

    case '-':
    {
      c2 = l->buf[l->i];
      if (c2 == '-') {
        ++l->i;
        l->t = BC_LEX_OP_DEC;
      }
      else bc_lex_assign(l, BC_LEX_OP_ASSIGN_MINUS, BC_LEX_OP_MINUS);
      break;
    }

    case '.':
    {
      c2 = l->buf[l->i];
      if (BC_LEX_NUM_CHAR(c2, 'Z', 1)) s = bc_lex_number(l, c);
      else {
        l->t = BC_LEX_KEY_LAST;
        s = bc_lex_posixErr(l, BC_ERROR_POSIX_DOT);
      }
      break;
    }

    case '/':
    {
      c2 = l->buf[l->i];
      if (c2 =='*') s = bc_lex_comment(l);
      else bc_lex_assign(l, BC_LEX_OP_ASSIGN_DIVIDE, BC_LEX_OP_DIVIDE);
      break;
    }

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
    // Apparently, GNU bc (and maybe others) allows any uppercase letter as a
    // number. When single digits, they act like the ones above. When multi-
    // digit, any letter above the input base is automatically set to the
    // biggest allowable digit in the input base.
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
    {
      s = bc_lex_number(l, c);
      break;
    }

    case ';':
    {
      l->t = BC_LEX_SCOLON;
      break;
    }

    case '<':
    {
      bc_lex_assign(l, BC_LEX_OP_REL_LE, BC_LEX_OP_REL_LT);
      break;
    }

    case '=':
    {
      bc_lex_assign(l, BC_LEX_OP_REL_EQ, BC_LEX_OP_ASSIGN);
      break;
    }

    case '>':
    {
      bc_lex_assign(l, BC_LEX_OP_REL_GE, BC_LEX_OP_REL_GT);
      break;
    }

    case '[':
    case ']':
    {
      l->t = (BcLexType) (c - '[' + BC_LEX_LBRACKET);
      break;
    }

    case '\\':
    {
      if (l->buf[l->i] == '\n') {
        l->t = BC_LEX_WHITESPACE;
        ++l->i;
      }
      else s = bc_lex_invalidChar(l, c);
      break;
    }

    case '^':
    {
      bc_lex_assign(l, BC_LEX_OP_ASSIGN_POWER, BC_LEX_OP_POWER);
      break;
    }

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
    {
      s = bc_lex_identifier(l);
      break;
    }

    case '{':
    case '}':
    {
      l->t = (BcLexType) (c - '{' + BC_LEX_LBRACE);
      break;
    }

    case '|':
    {
      c2 = l->buf[l->i];

      if (c2 == '|') {

        s = bc_lex_vposixErr(l, BC_ERROR_POSIX_BOOL, "||");
        if (s) return s;

        ++l->i;
        l->t = BC_LEX_OP_BOOL_OR;
      }
      else s = bc_lex_invalidChar(l, c);

      break;
    }

    default:
    {
      s = bc_lex_invalidChar(l, c);
      break;
    }
  }

  return s;
}

void bc_parse_updateFunc(BcParse *p, size_t fidx) {
  p->fidx = fidx;
  p->func = bc_vec_item(&p->prog->fns, fidx);
}

void bc_parse_pushName(BcParse *p, char *name) {
  bc_vec_npush(&p->func->code, strlen(name), name);
  bc_parse_push(p, UCHAR_MAX);
}

void bc_parse_pushIndex(BcParse *p, size_t idx) {
  bc_vec_pushIndex(&p->func->code, idx);
}

void bc_parse_addId(BcParse *p, char inst) {

  BcFunc *f = p->func;
  BcVec *v = inst == BC_INST_NUM ? &f->consts : &f->strs;
  size_t idx = v->len;
  char *str = xstrdup(p->l.str.v);

  bc_vec_push(v, &str);
  bc_parse_updateFunc(p, p->fidx);
  bc_parse_push(p, inst);
  bc_parse_pushIndex(p, idx);
}

BcStatus bc_parse_text(BcParse *p, char *text) {
  // Make sure the pointer isn't invalidated.
  p->func = bc_vec_item(&p->prog->fns, p->fidx);
  return bc_lex_text(&p->l, text);
}

BcStatus bc_parse_reset(BcParse *p, BcStatus s) {

  if (p->fidx != BC_PROG_MAIN) {
    bc_func_reset(p->func);
    bc_parse_updateFunc(p, BC_PROG_MAIN);
  }

  p->l.i = p->l.len;
  p->l.t = BC_LEX_EOF;
  p->auto_part = 0;

  bc_vec_npop(&p->flags, p->flags.len - 1);
  bc_vec_npop(&p->exits, p->exits.len);
  bc_vec_npop(&p->conds, p->conds.len);
  bc_vec_npop(&p->ops, p->ops.len);

  return bc_program_reset(p->prog, s);
}

void bc_parse_free(BcParse *p) {
  bc_vec_free(&p->flags);
  bc_vec_free(&p->exits);
  bc_vec_free(&p->conds);
  bc_vec_free(&p->ops);
  bc_vec_free(&p->l.str);
}

void bc_parse_init(BcParse *p, BcProgram *prog, size_t func)
{
  uint16_t flag = 0;
  bc_vec_init(&p->flags, sizeof(uint16_t), NULL);
  bc_vec_push(&p->flags, &flag);
  bc_vec_init(&p->exits, sizeof(BcInstPtr), NULL);
  bc_vec_init(&p->conds, sizeof(size_t), NULL);
  bc_vec_init(&p->ops, sizeof(BcLexType), NULL);

  bc_lex_init(&p->l);

  p->prog = prog;
  p->auto_part = 0;
  bc_parse_updateFunc(p, func);
}

static BcStatus bc_parse_else(BcParse *p);
static BcStatus bc_parse_stmt(BcParse *p);
static BcStatus bc_parse_expr_err(BcParse *p, uint8_t flags, BcParseNext next);

static int bc_parse_inst_isLeaf(BcInst t) {
  return (t >= BC_INST_NUM && t <= BC_INST_ABS) ||
          t == BC_INST_INC_POST || t == BC_INST_DEC_POST;
}

static int bc_parse_isDelimiter(BcParse *p) {

  BcLexType t = p->l.t;
  int good = 0;

  if (BC_PARSE_DELIMITER(t)) return 1;

  if (t == BC_LEX_KEY_ELSE) {

    size_t i;
    uint16_t *fptr = NULL, flags = BC_PARSE_FLAG_ELSE;

    for (i = 0; i < p->flags.len && BC_PARSE_BLOCK_STMT(flags); ++i) {
      fptr = bc_vec_item_rev(&p->flags, i);
      flags = *fptr;
      if ((flags & BC_PARSE_FLAG_BRACE) && p->l.last != BC_LEX_RBRACE)
        return 0;
    }

    good = ((flags & BC_PARSE_FLAG_IF) != 0);
  }
  else if (t == BC_LEX_RBRACE) {

    size_t i;

    for (i = 0; !good && i < p->flags.len; ++i) {
      uint16_t *fptr = bc_vec_item_rev(&p->flags, i);
      good = (((*fptr) & BC_PARSE_FLAG_BRACE) != 0);
    }
  }

  return good;
}

static void bc_parse_setLabel(BcParse *p) {

  BcFunc *func = p->func;
  BcInstPtr *ip = bc_vec_top(&p->exits);
  size_t *label;

  label = bc_vec_item(&func->labels, ip->idx);
  *label = func->code.len;

  bc_vec_pop(&p->exits);
}

static void bc_parse_createLabel(BcParse *p, size_t idx) {
  bc_vec_push(&p->func->labels, &idx);
}

static void bc_parse_createCondLabel(BcParse *p, size_t idx) {
  bc_parse_createLabel(p, p->func->code.len);
  bc_vec_push(&p->conds, &idx);
}

static void bc_parse_createExitLabel(BcParse *p, size_t idx, int loop) {

  BcInstPtr ip;

  ip.func = loop;
  ip.idx = idx;
  ip.len = 0;

  bc_vec_push(&p->exits, &ip);
  bc_parse_createLabel(p, SIZE_MAX);
}

static size_t bc_parse_addFunc(BcParse *p, char *name) {

  size_t idx = bc_program_insertFunc(p->prog, name);

  // Make sure that this pointer was not invalidated.
  p->func = bc_vec_item(&p->prog->fns, p->fidx);

  return idx;
}

static void bc_parse_operator(BcParse *p, BcLexType type,
                              size_t start, size_t *nexprs)
{
  BcLexType t;
  char l, r = BC_PARSE_OP_PREC(type);
  char left = BC_PARSE_OP_LEFT(type);

  while (p->ops.len > start) {

    t = BC_PARSE_TOP_OP(p);
    if (t == BC_LEX_LPAREN) break;

    l = BC_PARSE_OP_PREC(t);
    if (l >= r && (l != r || !left)) break;

    bc_parse_push(p, BC_PARSE_TOKEN_INST(t));
    bc_vec_pop(&p->ops);
    *nexprs -= !BC_PARSE_OP_PREFIX(t);
  }

  bc_vec_push(&p->ops, &type);
}

static BcStatus bc_parse_rightParen(BcParse *p, size_t ops_bgn, size_t *nexs) {

  BcLexType top;

  if (p->ops.len <= ops_bgn) return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

  while ((top = BC_PARSE_TOP_OP(p)) != BC_LEX_LPAREN) {

    bc_parse_push(p, BC_PARSE_TOKEN_INST(top));

    bc_vec_pop(&p->ops);
    *nexs -= !BC_PARSE_OP_PREFIX(top);

    if (p->ops.len <= ops_bgn) return bc_parse_err(p, BC_ERROR_PARSE_EXPR);
  }

  bc_vec_pop(&p->ops);

  return bc_lex_next(&p->l);
}

static BcStatus bc_parse_params(BcParse *p, uint8_t flags) {

  BcStatus s;
  int comma = 0;
  size_t nparams;

  s = bc_lex_next(&p->l);
  if (s) return s;

  for (nparams = 0; p->l.t != BC_LEX_RPAREN; ++nparams) {

    flags = (flags & ~(BC_PARSE_PRINT | BC_PARSE_REL)) | BC_PARSE_ARRAY;
    s = bc_parse_expr_status(p, flags, bc_parse_next_param);
    if (s) return s;

    comma = p->l.t == BC_LEX_COMMA;
    if (comma) {
      s = bc_lex_next(&p->l);
      if (s) return s;
    }
  }

  if (comma) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
  bc_parse_push(p, BC_INST_CALL);
  bc_parse_pushIndex(p, nparams);

  return BC_STATUS_SUCCESS;
}

static BcStatus bc_parse_call(BcParse *p, char *name, uint8_t flags) {

  BcStatus s;
  struct str_len id;
  size_t idx;

  id.str = name;

  s = bc_parse_params(p, flags);
  if (s) goto err;

  if (p->l.t != BC_LEX_RPAREN) {
    s = bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
    goto err;
  }

  idx = bc_map_index(&p->prog->fn_map, &id);

  if (idx == SIZE_MAX) {
    bc_parse_addFunc(p, name);
    idx = bc_map_index(&p->prog->fn_map, &id);
  } else free(name);

  bc_parse_pushIndex(p,
    ((struct str_len *)bc_vec_item(&p->prog->fn_map, idx))->len);

  return bc_lex_next(&p->l);

err:
  free(name);
  return s;
}

static BcStatus bc_parse_name(BcParse *p, BcInst *type, uint8_t flags) {

  BcStatus s;
  char *name;

  name = xstrdup(p->l.str.v);
  s = bc_lex_next(&p->l);
  if (s) goto err;

  if (p->l.t == BC_LEX_LBRACKET) {

    s = bc_lex_next(&p->l);
    if (s) goto err;

    if (p->l.t == BC_LEX_RBRACKET) {

      if (!(flags & BC_PARSE_ARRAY)) {
        s = bc_parse_err(p, BC_ERROR_PARSE_EXPR);
        goto err;
      }

      *type = BC_INST_ARRAY;
    }
    else {

      *type = BC_INST_ARRAY_ELEM;

      flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
      s = bc_parse_expr_status(p, flags, bc_parse_next_elem);
      if (s) goto err;

      if (p->l.t != BC_LEX_RBRACKET) {
        s = bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
        goto err;
      }
    }

    s = bc_lex_next(&p->l);
    if (s) goto err;

    bc_parse_push(p, *type);
    bc_parse_pushName(p, name);
  }
  else if (p->l.t == BC_LEX_LPAREN) {

    if (flags & BC_PARSE_NOCALL) {
      s = bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
      goto err;
    }

    *type = BC_INST_CALL;

    // Return early because bc_parse_call() frees the name.
    return bc_parse_call(p, name, flags);
  }
  else {
    *type = BC_INST_VAR;
    bc_parse_push(p, BC_INST_VAR);
    bc_parse_pushName(p, name);
  }

err:
  free(name);
  return s;
}

static BcStatus bc_parse_read(BcParse *p) {

  BcStatus s;

  s = bc_lex_next(&p->l);
  if (s) return s;
  if (p->l.t != BC_LEX_LPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  s = bc_lex_next(&p->l);
  if (s) return s;
  if (p->l.t != BC_LEX_RPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  bc_parse_push(p, BC_INST_READ);

  return bc_lex_next(&p->l);
}

static BcStatus bc_parse_builtin(BcParse *p, BcLexType type,
                                 uint8_t flags, BcInst *prev)
{
  BcStatus s;

  s = bc_lex_next(&p->l);
  if (s) return s;
  if (p->l.t != BC_LEX_LPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  s = bc_lex_next(&p->l);
  if (s) return s;

  flags = (flags & ~(BC_PARSE_PRINT | BC_PARSE_REL));
  if (type == BC_LEX_KEY_LENGTH) flags |= BC_PARSE_ARRAY;

  s = bc_parse_expr_status(p, flags, bc_parse_next_rel);
  if (s) return s;

  if (p->l.t != BC_LEX_RPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  *prev = type - BC_LEX_KEY_LENGTH + BC_INST_LENGTH;
  bc_parse_push(p, *prev);

  return bc_lex_next(&p->l);
}

static BcStatus bc_parse_scale(BcParse *p, BcInst *type, uint8_t flags) {

  BcStatus s;

  s = bc_lex_next(&p->l);
  if (s) return s;

  if (p->l.t != BC_LEX_LPAREN) {
    *type = BC_INST_SCALE;
    bc_parse_push(p, BC_INST_SCALE);
    return BC_STATUS_SUCCESS;
  }

  *type = BC_INST_SCALE_FUNC;
  flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);

  s = bc_lex_next(&p->l);
  if (s) return s;

  s = bc_parse_expr_status(p, flags, bc_parse_next_rel);
  if (s) return s;
  if (p->l.t != BC_LEX_RPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  bc_parse_push(p, BC_INST_SCALE_FUNC);

  return bc_lex_next(&p->l);
}

static BcStatus bc_parse_incdec(BcParse *p, BcInst *prev,
                                size_t *nexs, uint8_t flags)
{
  BcStatus s;
  BcLexType type;
  char inst;
  BcInst etype = *prev;
  BcLexType last = p->l.last;

  if (last == BC_LEX_OP_INC || last == BC_LEX_OP_DEC || last == BC_LEX_RPAREN)
    return s = bc_parse_err(p, BC_ERROR_PARSE_ASSIGN);

  if (BC_PARSE_INST_VAR(etype)) {
    *prev = inst = BC_INST_INC_POST + (p->l.t != BC_LEX_OP_INC);
    bc_parse_push(p, inst);
    s = bc_lex_next(&p->l);
  }
  else {

    *prev = inst = BC_INST_INC_PRE + (p->l.t != BC_LEX_OP_INC);

    s = bc_lex_next(&p->l);
    if (s) return s;
    type = p->l.t;

    // Because we parse the next part of the expression
    // right here, we need to increment this.
    *nexs = *nexs + 1;

    if (type == BC_LEX_NAME)
      s = bc_parse_name(p, prev, flags | BC_PARSE_NOCALL);
    else if (type >= BC_LEX_KEY_LAST && type <= BC_LEX_KEY_OBASE) {
      bc_parse_push(p, type - BC_LEX_KEY_LAST + BC_INST_LAST);
      s = bc_lex_next(&p->l);
    }
    else if (type == BC_LEX_KEY_SCALE) {
      s = bc_lex_next(&p->l);
      if (s) return s;
      if (p->l.t == BC_LEX_LPAREN) s = bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
      else bc_parse_push(p, BC_INST_SCALE);
    }
    else s = bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

    if (!s) bc_parse_push(p, inst);
  }

  return s;
}

static BcStatus bc_parse_minus(BcParse *p, BcInst *prev, size_t ops_bgn,
                               int rparen, int bin_last, size_t *nexprs)
{
  BcStatus s;
  BcLexType type;

  s = bc_lex_next(&p->l);
  if (s) return s;

  type = BC_PARSE_LEAF(*prev, bin_last, rparen) ? BC_LEX_OP_MINUS : BC_LEX_NEG;
  *prev = BC_PARSE_TOKEN_INST(type);

  // We can just push onto the op stack because this is the largest
  // precedence operator that gets pushed. Inc/dec does not.
  if (type != BC_LEX_OP_MINUS) bc_vec_push(&p->ops, &type);
  else bc_parse_operator(p, type, ops_bgn, nexprs);

  return s;
}

static BcStatus bc_parse_str(BcParse *p, char inst) {
  bc_parse_string(p);
  bc_parse_push(p, inst);
  return bc_lex_next(&p->l);
}

static BcStatus bc_parse_print(BcParse *p) {

  BcStatus s;
  BcLexType t;
  int comma = 0;

  s = bc_lex_next(&p->l);
  if (s) return s;

  t = p->l.t;

  if (bc_parse_isDelimiter(p)) return bc_parse_err(p, BC_ERROR_PARSE_PRINT);

  do {
    if (t == BC_LEX_STR) s = bc_parse_str(p, BC_INST_PRINT_POP);
    else {
      s = bc_parse_expr_status(p, 0, bc_parse_next_print);
      if (!s) bc_parse_push(p, BC_INST_PRINT_POP);
    }

    if (s) return s;

    comma = (p->l.t == BC_LEX_COMMA);

    if (comma) s = bc_lex_next(&p->l);
    else {
      if (!bc_parse_isDelimiter(p))
        return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
      else break;
    }

    t = p->l.t;
  } while (!s);

  if (s) return s;
  if (comma) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  return s;
}

static BcStatus bc_parse_return(BcParse *p) {

  BcStatus s;
  BcLexType t;
  int paren;
  char inst = BC_INST_RET0;

  if (!BC_PARSE_FUNC(p)) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  if (p->func->voidfn) inst = BC_INST_RET_VOID;

  s = bc_lex_next(&p->l);
  if (s) return s;

  t = p->l.t;
  paren = t == BC_LEX_LPAREN;

  if (bc_parse_isDelimiter(p)) bc_parse_push(p, inst);
  else {

    s = bc_parse_expr_err(p, 0, bc_parse_next_expr);
    if (s && s != BC_STATUS_EMPTY_EXPR) return s;
    else if (s == BC_STATUS_EMPTY_EXPR) {
      bc_parse_push(p, inst);
      s = bc_lex_next(&p->l);
      if (s) return s;
    }

    if (!paren || p->l.last != BC_LEX_RPAREN) {
      s = bc_parse_posixErr(p, BC_ERROR_POSIX_RET);
      if (s) return s;
    }
    else if (p->func->voidfn)
      return bc_parse_verr(p, BC_ERROR_PARSE_RET_VOID, p->func->name);

    bc_parse_push(p, BC_INST_RET);
  }

  return s;
}

static BcStatus bc_parse_endBody(BcParse *p, int brace) {

  BcStatus s = BC_STATUS_SUCCESS;
  int has_brace, new_else = 0;

  if (p->flags.len <= 1) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  if (brace) {
    if (p->l.t == BC_LEX_RBRACE) {
      s = bc_lex_next(&p->l);
      if (s) return s;
      if (!bc_parse_isDelimiter(p))
        return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
    }
    else return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
  }

  has_brace = (BC_PARSE_BRACE(p) != 0);

  do {
    size_t len = p->flags.len;
    int loop;

    if (has_brace && !brace) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

    loop = BC_PARSE_LOOP_INNER(p) != 0;

    if (loop || BC_PARSE_ELSE(p)) {

      if (loop) {

        size_t *label = bc_vec_top(&p->conds);

        bc_parse_push(p, BC_INST_JUMP);
        bc_parse_pushIndex(p, *label);

        bc_vec_pop(&p->conds);
      }

      bc_parse_setLabel(p);
      bc_vec_pop(&p->flags);
    }
    else if (BC_PARSE_FUNC_INNER(p)) {
      BcInst inst = (p->func->voidfn ? BC_INST_RET_VOID : BC_INST_RET0);
      bc_parse_push(p, inst);
      bc_parse_updateFunc(p, BC_PROG_MAIN);
      bc_vec_pop(&p->flags);
    }
    else if (BC_PARSE_BRACE(p) && !BC_PARSE_IF(p)) bc_vec_pop(&p->flags);

    // This needs to be last to parse nested if's properly.
    if (BC_PARSE_IF(p) && (len == p->flags.len || !BC_PARSE_BRACE(p))) {

      while (p->l.t == BC_LEX_NLINE) {
        s = bc_lex_next(&p->l);
        if (s) return s;
      }

      bc_vec_pop(&p->flags);
      *(BC_PARSE_TOP_FLAG_PTR(p)) |= BC_PARSE_FLAG_IF_END;

      new_else = (p->l.t == BC_LEX_KEY_ELSE);
      if (new_else) s = bc_parse_else(p);
      else if (!has_brace && (!BC_PARSE_IF_END(p) || brace))
        bc_parse_noElse(p);
    }

    if (brace && has_brace) brace = 0;

  } while (p->flags.len > 1 && !new_else && (!BC_PARSE_IF_END(p) || brace) &&
           !(has_brace = (BC_PARSE_BRACE(p) != 0)));

  if (!s && p->flags.len == 1 && brace)
    s = bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
  else if (brace && BC_PARSE_BRACE(p)) {
    uint16_t flags = BC_PARSE_TOP_FLAG(p);
    if (!(flags & (BC_PARSE_FLAG_FUNC_INNER | BC_PARSE_FLAG_LOOP_INNER)) &&
        !(flags & (BC_PARSE_FLAG_IF | BC_PARSE_FLAG_ELSE)) &&
        !(flags & (BC_PARSE_FLAG_IF_END)))
    {
      bc_vec_pop(&p->flags);
    }
  }

  return s;
}

static void bc_parse_startBody(BcParse *p, uint16_t flags) {
  flags |= (BC_PARSE_TOP_FLAG(p) & (BC_PARSE_FLAG_FUNC | BC_PARSE_FLAG_LOOP));
  flags |= BC_PARSE_FLAG_BODY;
  bc_vec_push(&p->flags, &flags);
}

void bc_parse_noElse(BcParse *p) {
  uint16_t *flag_ptr = BC_PARSE_TOP_FLAG_PTR(p);
  *flag_ptr = (*flag_ptr & ~(BC_PARSE_FLAG_IF_END));
  bc_parse_setLabel(p);
}

static BcStatus bc_parse_if(BcParse *p) {

  BcStatus s;
  size_t idx;

  s = bc_lex_next(&p->l);
  if (s) return s;
  if (p->l.t != BC_LEX_LPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  s = bc_lex_next(&p->l);
  if (s) return s;
  s = bc_parse_expr_status(p, BC_PARSE_REL, bc_parse_next_rel);
  if (s) return s;
  if (p->l.t != BC_LEX_RPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  s = bc_lex_next(&p->l);
  if (s) return s;
  bc_parse_push(p, BC_INST_JUMP_ZERO);

  idx = p->func->labels.len;

  bc_parse_pushIndex(p, idx);
  bc_parse_createExitLabel(p, idx, 0);
  bc_parse_startBody(p, BC_PARSE_FLAG_IF);

  return BC_STATUS_SUCCESS;
}

static BcStatus bc_parse_else(BcParse *p) {

  size_t idx = p->func->labels.len;

  if (!BC_PARSE_IF_END(p)) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  bc_parse_push(p, BC_INST_JUMP);
  bc_parse_pushIndex(p, idx);

  bc_parse_noElse(p);

  bc_parse_createExitLabel(p, idx, 0);
  bc_parse_startBody(p, BC_PARSE_FLAG_ELSE);

  return bc_lex_next(&p->l);
}

static BcStatus bc_parse_while(BcParse *p) {

  BcStatus s;
  size_t idx;

  s = bc_lex_next(&p->l);
  if (s) return s;
  if (p->l.t != BC_LEX_LPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
  s = bc_lex_next(&p->l);
  if (s) return s;

  bc_parse_createCondLabel(p, p->func->labels.len);

  idx = p->func->labels.len;

  bc_parse_createExitLabel(p, idx, 1);

  s = bc_parse_expr_status(p, BC_PARSE_REL, bc_parse_next_rel);
  if (s) return s;
  if (p->l.t != BC_LEX_RPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
  s = bc_lex_next(&p->l);
  if (s) return s;

  bc_parse_push(p, BC_INST_JUMP_ZERO);
  bc_parse_pushIndex(p, idx);
  bc_parse_startBody(p, BC_PARSE_FLAG_LOOP | BC_PARSE_FLAG_LOOP_INNER);

  return BC_STATUS_SUCCESS;
}

static BcStatus bc_parse_for(BcParse *p) {

  BcStatus s;
  size_t cond_idx, exit_idx, body_idx, update_idx;

  s = bc_lex_next(&p->l);
  if (s) return s;
  if (p->l.t != BC_LEX_LPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
  s = bc_lex_next(&p->l);
  if (s) return s;

  if (p->l.t != BC_LEX_SCOLON) {
    s = bc_parse_expr_status(p, 0, bc_parse_next_for);
    if (!s) bc_parse_push(p, BC_INST_POP);
  }
  else s = bc_parse_posixErr(p, BC_ERROR_POSIX_FOR1);

  if (s) return s;
  if (p->l.t != BC_LEX_SCOLON) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
  s = bc_lex_next(&p->l);
  if (s) return s;

  cond_idx = p->func->labels.len;
  update_idx = cond_idx + 1;
  body_idx = update_idx + 1;
  exit_idx = body_idx + 1;

  bc_parse_createLabel(p, p->func->code.len);

  if (p->l.t != BC_LEX_SCOLON)
    s = bc_parse_expr_status(p, BC_PARSE_REL, bc_parse_next_for);
  else {

    // Set this for the next call to bc_parse_number.
    // This is safe to set because the current token
    // is a semicolon, which has no string requirement.
    bc_vec_string(&p->l.str, strlen(bc_parse_const1), bc_parse_const1);
    bc_parse_number(p);

    s = bc_parse_posixErr(p, BC_ERROR_POSIX_FOR2);
  }

  if (s) return s;
  if (p->l.t != BC_LEX_SCOLON) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  s = bc_lex_next(&p->l);
  if (s) return s;

  bc_parse_push(p, BC_INST_JUMP_ZERO);
  bc_parse_pushIndex(p, exit_idx);
  bc_parse_push(p, BC_INST_JUMP);
  bc_parse_pushIndex(p, body_idx);

  bc_parse_createCondLabel(p, update_idx);

  if (p->l.t != BC_LEX_RPAREN) {
    s = bc_parse_expr_status(p, 0, bc_parse_next_rel);
    if (!s) bc_parse_push(p, BC_INST_POP);
  }
  else s = bc_parse_posixErr(p, BC_ERROR_POSIX_FOR3);

  if (s) return s;

  if (p->l.t != BC_LEX_RPAREN) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
  bc_parse_push(p, BC_INST_JUMP);
  bc_parse_pushIndex(p, cond_idx);
  bc_parse_createLabel(p, p->func->code.len);

  bc_parse_createExitLabel(p, exit_idx, 1);
  s = bc_lex_next(&p->l);
  if (!s) bc_parse_startBody(p, BC_PARSE_FLAG_LOOP | BC_PARSE_FLAG_LOOP_INNER);

  return s;
}

static BcStatus bc_parse_loopExit(BcParse *p, BcLexType type) {

  size_t i;
  BcInstPtr *ip;

  if (!BC_PARSE_LOOP(p)) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  if (type == BC_LEX_KEY_BREAK) {

    if (!p->exits.len) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

    i = p->exits.len - 1;
    ip = bc_vec_item(&p->exits, i);

    while (!ip->func && i < p->exits.len) ip = bc_vec_item(&p->exits, i--);
    if (i >= p->exits.len && !ip->func)
      return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

    i = ip->idx;
  }
  else i = *((size_t*) bc_vec_top(&p->conds));

  bc_parse_push(p, BC_INST_JUMP);
  bc_parse_pushIndex(p, i);

  return bc_lex_next(&p->l);
}

static BcStatus bc_parse_func(BcParse *p) {

  BcStatus s;
  int comma = 0, voidfn;
  uint16_t flags;
  char *name;
  size_t idx;

  s = bc_lex_next(&p->l);
  if (s) return s;

  if (p->l.t != BC_LEX_NAME) return bc_parse_err(p, BC_ERROR_PARSE_FUNC);

  voidfn = (!FLAG(s) && !FLAG(w) && p->l.t == BC_LEX_NAME &&
            !strcmp(p->l.str.v, "void"));

  s = bc_lex_next(&p->l);
  if (s) return s;

  voidfn = (voidfn && p->l.t == BC_LEX_NAME);

  if (voidfn) {
    s = bc_lex_next(&p->l);
    if (s) return s;
  }

  if (p->l.t != BC_LEX_LPAREN) return bc_parse_err(p, BC_ERROR_PARSE_FUNC);

  name = xstrdup(p->l.str.v);
  idx = bc_program_insertFunc(p->prog, name);
  bc_parse_updateFunc(p, idx);
  p->func->voidfn = voidfn;

  s = bc_lex_next(&p->l);
  if (s) return s;

  while (p->l.t != BC_LEX_RPAREN) {

    BcType t = BC_TYPE_VAR;

    if (p->l.t != BC_LEX_NAME) return bc_parse_err(p, BC_ERROR_PARSE_FUNC);

    ++p->func->nparams;

    name = xstrdup(p->l.str.v);
    s = bc_lex_next(&p->l);
    if (s) goto err;

    if (p->l.t == BC_LEX_LBRACKET) {

      if (t == BC_TYPE_VAR) t = BC_TYPE_ARRAY;

      s = bc_lex_next(&p->l);
      if (s) goto err;

      if (p->l.t != BC_LEX_RBRACKET) {
        s = bc_parse_err(p, BC_ERROR_PARSE_FUNC);
        goto err;
      }

      s = bc_lex_next(&p->l);
      if (s) goto err;
    }

    comma = p->l.t == BC_LEX_COMMA;
    if (comma) {
      s = bc_lex_next(&p->l);
      if (s) goto err;
    }

    s = bc_func_insert(p->func, name, t, p->l.line);
    if (s) goto err;
  }

  if (comma) return bc_parse_err(p, BC_ERROR_PARSE_FUNC);

  flags = BC_PARSE_FLAG_FUNC | BC_PARSE_FLAG_FUNC_INNER | BC_PARSE_FLAG_BODY;
  bc_parse_startBody(p, flags);

  s = bc_lex_next(&p->l);
  if (s) return s;

  if (p->l.t != BC_LEX_LBRACE) s = bc_parse_posixErr(p, BC_ERROR_POSIX_BRACE);

  return s;

err:
  free(name);
  return s;
}

static BcStatus bc_parse_auto(BcParse *p) {

  BcStatus s;
  int comma, one;
  char *name;

  if (!p->auto_part) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
  s = bc_lex_next(&p->l);
  if (s) return s;

  p->auto_part = comma = 0;
  one = p->l.t == BC_LEX_NAME;

  while (p->l.t == BC_LEX_NAME) {

    BcType t;

    name = xstrdup(p->l.str.v);
    s = bc_lex_next(&p->l);
    if (s) goto err;

    if (p->l.t == BC_LEX_LBRACKET) {

      t = BC_TYPE_ARRAY;

      s = bc_lex_next(&p->l);
      if (s) goto err;

      if (p->l.t != BC_LEX_RBRACKET) {
        s = bc_parse_err(p, BC_ERROR_PARSE_FUNC);
        goto err;
      }

      s = bc_lex_next(&p->l);
      if (s) goto err;
    }
    else t = BC_TYPE_VAR;

    comma = p->l.t == BC_LEX_COMMA;
    if (comma) {
      s = bc_lex_next(&p->l);
      if (s) goto err;
    }

    s = bc_func_insert(p->func, name, t, p->l.line);
    if (s) goto err;
  }

  if (comma) return bc_parse_err(p, BC_ERROR_PARSE_FUNC);
  if (!one) return bc_parse_err(p, BC_ERROR_PARSE_NO_AUTO);
  if (!bc_parse_isDelimiter(p)) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

  return s;

err:
  free(name);
  return s;
}

static BcStatus bc_parse_body(BcParse *p, int brace) {

  BcStatus s = BC_STATUS_SUCCESS;
  uint16_t *flag_ptr = BC_PARSE_TOP_FLAG_PTR(p);

  *flag_ptr &= ~(BC_PARSE_FLAG_BODY);

  if (*flag_ptr & BC_PARSE_FLAG_FUNC_INNER) {

    if (!brace) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);

    p->auto_part = p->l.t != BC_LEX_KEY_AUTO;

    if (!p->auto_part) {

      // Make sure this is 1 to not get a parse error.
      p->auto_part = 1;

      s = bc_parse_auto(p);
      if (s) return s;
    }

    if (p->l.t == BC_LEX_NLINE) s = bc_lex_next(&p->l);
  }
  else {
    size_t len = p->flags.len;
    s = bc_parse_stmt(p);
    if (!s && !brace && !BC_PARSE_BODY(p) && len <= p->flags.len)
      s = bc_parse_endBody(p, 0);
  }

  return s;
}

static BcStatus bc_parse_stmt(BcParse *p) {

  BcStatus s = BC_STATUS_SUCCESS;
  size_t len;
  uint16_t flags;
  BcLexType type = p->l.t;

  if (type == BC_LEX_NLINE) return bc_lex_next(&p->l);
  if (type == BC_LEX_KEY_AUTO) return bc_parse_auto(p);

  p->auto_part = 0;

  if (type != BC_LEX_KEY_ELSE) {

    if (BC_PARSE_IF_END(p)) {
      bc_parse_noElse(p);
      if (p->flags.len > 1 && !BC_PARSE_BRACE(p))
        s = bc_parse_endBody(p, 0);
      return s;
    }
    else if (type == BC_LEX_LBRACE) {

      if (!BC_PARSE_BODY(p)) {
        bc_parse_startBody(p, BC_PARSE_FLAG_BRACE);
        s = bc_lex_next(&p->l);
      }
      else {
        *(BC_PARSE_TOP_FLAG_PTR(p)) |= BC_PARSE_FLAG_BRACE;
        s = bc_lex_next(&p->l);
        if (!s) s = bc_parse_body(p, 1);
      }

      return s;
    }
    else if (BC_PARSE_BODY(p) && !BC_PARSE_BRACE(p))
      return bc_parse_body(p, 0);
  }

  len = p->flags.len;
  flags = BC_PARSE_TOP_FLAG(p);

  switch (type) {

    case BC_LEX_OP_INC:
    case BC_LEX_OP_DEC:
    case BC_LEX_OP_MINUS:
    case BC_LEX_OP_BOOL_NOT:
    case BC_LEX_LPAREN:
    case BC_LEX_NAME:
    case BC_LEX_NUMBER:
    case BC_LEX_KEY_IBASE:
    case BC_LEX_KEY_LAST:
    case BC_LEX_KEY_LENGTH:
    case BC_LEX_KEY_OBASE:
    case BC_LEX_KEY_READ:
    case BC_LEX_KEY_SCALE:
    case BC_LEX_KEY_SQRT:
    case BC_LEX_KEY_ABS:
    {
      s = bc_parse_expr_status(p, BC_PARSE_PRINT, bc_parse_next_expr);
      break;
    }

    case BC_LEX_KEY_ELSE:
    {
      s = bc_parse_else(p);
      break;
    }

    case BC_LEX_SCOLON:
    {
      // Do nothing.
      break;
    }

    case BC_LEX_RBRACE:
    {
      s = bc_parse_endBody(p, 1);
      break;
    }

    case BC_LEX_STR:
    {
      s = bc_parse_str(p, BC_INST_PRINT_STR);
      break;
    }

    case BC_LEX_KEY_BREAK:
    case BC_LEX_KEY_CONTINUE:
    {
      s = bc_parse_loopExit(p, p->l.t);
      break;
    }

    case BC_LEX_KEY_FOR:
    {
      s = bc_parse_for(p);
      break;
    }

    case BC_LEX_KEY_HALT:
    {
      bc_parse_push(p, BC_INST_HALT);
      s = bc_lex_next(&p->l);
      break;
    }

    case BC_LEX_KEY_IF:
    {
      s = bc_parse_if(p);
      break;
    }

    case BC_LEX_KEY_LIMITS:
    {
      printf("BC_BASE_MAX     = %lu\n", BC_MAX_OBASE);
      printf("BC_DIM_MAX      = %lu\n", BC_MAX_DIM);
      printf("BC_SCALE_MAX    = %lu\n", BC_MAX_SCALE);
      printf("BC_STRING_MAX   = %lu\n", BC_MAX_STRING);
      printf("BC_NAME_MAX     = %lu\n", BC_MAX_NAME);
      printf("BC_NUM_MAX      = %lu\n", BC_MAX_NUM);
      printf("MAX Exponent    = %lu\n", BC_MAX_EXP);
      printf("Number of vars  = %lu\n", BC_MAX_VARS);

      s = bc_lex_next(&p->l);

      break;
    }

    case BC_LEX_KEY_PRINT:
    {
      s = bc_parse_print(p);
      break;
    }

    case BC_LEX_KEY_QUIT:
    {
      // Quit is a compile-time command. We don't exit directly,
      // so the vm can clean up. Limits do the same thing.
      s = BC_STATUS_QUIT;
      break;
    }

    case BC_LEX_KEY_RETURN:
    {
      s = bc_parse_return(p);
      break;
    }

    case BC_LEX_KEY_WHILE:
    {
      s = bc_parse_while(p);
      break;
    }

    default:
    {
      s = bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
      break;
    }
  }

  if (!s && len == p->flags.len && flags == BC_PARSE_TOP_FLAG(p)) {
    if (!bc_parse_isDelimiter(p)) s = bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
  }

  // Make sure semicolons are eaten.
  while (!s && p->l.t == BC_LEX_SCOLON) s = bc_lex_next(&p->l);

  return s;
}

BcStatus bc_parse_parse(BcParse *p) {

  BcStatus s;

  if (p->l.t == BC_LEX_EOF) s = bc_parse_err(p, BC_ERROR_PARSE_EOF);
  else if (p->l.t == BC_LEX_KEY_DEFINE) {
    if (BC_PARSE_NO_EXEC(p)) return bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
    s = bc_parse_func(p);
  }
  else s = bc_parse_stmt(p);

  if ((s && s != BC_STATUS_QUIT) || TT.sig) s = bc_parse_reset(p, s);

  return s;
}

static BcStatus bc_parse_expr_err(BcParse *p, uint8_t flags, BcParseNext next) {
  BcStatus s = BC_STATUS_SUCCESS;
  BcInst prev = BC_INST_PRINT;
  BcLexType top, t = p->l.t;
  size_t nexprs = 0, ops_bgn = p->ops.len;
  uint32_t i, nparens, nrelops;
  int pfirst, rprn, done, get_token, assign, bin_last, incdec;
  char valid[] = {0xfc, 0xff, 0xff, 0x67, 0xc0, 0x00, 0x7c, 0x0b};

  pfirst = p->l.t == BC_LEX_LPAREN;
  nparens = nrelops = 0;
  rprn = done = get_token = assign = incdec = 0;
  bin_last = 1;

  // We want to eat newlines if newlines are not a valid ending token.
  // This is for spacing in things like for loop headers.
  while (!s && (t = p->l.t) == BC_LEX_NLINE) s = bc_lex_next(&p->l);

  // Loop checking if token is valid in this expression
  for (; !TT.sig && !s && !done && (valid[t>>3] & (1<<(t&7))); t = p->l.t) {

    switch (t) {

      case BC_LEX_OP_INC:
      case BC_LEX_OP_DEC:
      {
        if (incdec) return bc_parse_err(p, BC_ERROR_PARSE_ASSIGN);
        s = bc_parse_incdec(p, &prev, &nexprs, flags);
        rprn = get_token = bin_last = 0;
        incdec = 1;
        break;
      }

      case BC_LEX_OP_MINUS:
      {
        s = bc_parse_minus(p, &prev, ops_bgn, rprn, bin_last, &nexprs);
        rprn = get_token = 0;
        bin_last = (prev == BC_INST_MINUS);
        if (bin_last) incdec = 0;
        break;
      }

      case BC_LEX_OP_ASSIGN_POWER:
      case BC_LEX_OP_ASSIGN_MULTIPLY:
      case BC_LEX_OP_ASSIGN_DIVIDE:
      case BC_LEX_OP_ASSIGN_MODULUS:
      case BC_LEX_OP_ASSIGN_PLUS:
      case BC_LEX_OP_ASSIGN_MINUS:
      case BC_LEX_OP_ASSIGN:
      {
        if (!BC_PARSE_INST_VAR(prev)) {
          s = bc_parse_err(p, BC_ERROR_PARSE_ASSIGN);
          break;
        }
      }
      // Fallthrough.
      case BC_LEX_OP_POWER:
      case BC_LEX_OP_MULTIPLY:
      case BC_LEX_OP_DIVIDE:
      case BC_LEX_OP_MODULUS:
      case BC_LEX_OP_PLUS:
      case BC_LEX_OP_REL_EQ:
      case BC_LEX_OP_REL_LE:
      case BC_LEX_OP_REL_GE:
      case BC_LEX_OP_REL_NE:
      case BC_LEX_OP_REL_LT:
      case BC_LEX_OP_REL_GT:
      case BC_LEX_OP_BOOL_NOT:
      case BC_LEX_OP_BOOL_OR:
      case BC_LEX_OP_BOOL_AND:
      {
        if (BC_PARSE_OP_PREFIX(t)) {
          if (!bin_last && !BC_PARSE_OP_PREFIX(p->l.last))
            return bc_parse_err(p, BC_ERROR_PARSE_EXPR);
        }
        else if (BC_PARSE_PREV_PREFIX(prev) || bin_last)
          return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

        nrelops += (t >= BC_LEX_OP_REL_EQ && t <= BC_LEX_OP_REL_GT);
        prev = BC_PARSE_TOKEN_INST(t);
        bc_parse_operator(p, t, ops_bgn, &nexprs);
        rprn = incdec = 0;
        get_token = 1;
        bin_last = !BC_PARSE_OP_PREFIX(t);

        break;
      }

      case BC_LEX_LPAREN:
      {
        if (BC_PARSE_LEAF(prev, bin_last, rprn))
          return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

        ++nparens;
        rprn = incdec = 0;
        get_token = 1;
        bc_vec_push(&p->ops, &t);

        break;
      }

      case BC_LEX_RPAREN:
      {
        // This needs to be a status. The error
        // is handled in bc_parse_expr_status().
        if (p->l.last == BC_LEX_LPAREN) return BC_STATUS_EMPTY_EXPR;

        if (bin_last || BC_PARSE_PREV_PREFIX(prev))
          return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

        if (!nparens) {
          s = BC_STATUS_SUCCESS;
          done = 1;
          get_token = 0;
          break;
        }

        --nparens;
        rprn = 1;
        get_token = bin_last = incdec = 0;

        s = bc_parse_rightParen(p, ops_bgn, &nexprs);

        break;
      }

      case BC_LEX_NAME:
      {
        if (BC_PARSE_LEAF(prev, bin_last, rprn))
          return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

        get_token = bin_last = 0;
        s = bc_parse_name(p, &prev, flags & ~BC_PARSE_NOCALL);
        rprn = (prev == BC_INST_CALL);
        ++nexprs;

        break;
      }

      case BC_LEX_NUMBER:
      {
        if (BC_PARSE_LEAF(prev, bin_last, rprn))
          return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

        bc_parse_number(p);
        nexprs += 1;
        prev = BC_INST_NUM;
        get_token = 1;
        rprn = bin_last = 0;

        break;
      }

      case BC_LEX_KEY_IBASE:
      case BC_LEX_KEY_LAST:
      case BC_LEX_KEY_OBASE:
      {
        if (BC_PARSE_LEAF(prev, bin_last, rprn))
          return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

        prev = (char) (t - BC_LEX_KEY_LAST + BC_INST_LAST);
        bc_parse_push(p, (char) prev);

        get_token = 1;
        rprn = bin_last = 0;
        ++nexprs;

        break;
      }

      case BC_LEX_KEY_LENGTH:
      case BC_LEX_KEY_SQRT:
      case BC_LEX_KEY_ABS:
      {
        if (BC_PARSE_LEAF(prev, bin_last, rprn))
          return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

        s = bc_parse_builtin(p, t, flags, &prev);
        rprn = get_token = bin_last = incdec = 0;
        ++nexprs;

        break;
      }

      case BC_LEX_KEY_READ:
      {
        if (BC_PARSE_LEAF(prev, bin_last, rprn))
          return bc_parse_err(p, BC_ERROR_PARSE_EXPR);
        else if (flags & BC_PARSE_NOREAD)
          s = bc_parse_err(p, BC_ERROR_EXEC_REC_READ);
        else s = bc_parse_read(p);

        rprn = get_token = bin_last = incdec = 0;
        ++nexprs;
        prev = BC_INST_READ;

        break;
      }

      case BC_LEX_KEY_SCALE:
      {
        if (BC_PARSE_LEAF(prev, bin_last, rprn))
          return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

        s = bc_parse_scale(p, &prev, flags);
        rprn = get_token = bin_last = 0;
        ++nexprs;

        break;
      }

      default:
      {
        s = bc_parse_err(p, BC_ERROR_PARSE_TOKEN);
        break;
      }
    }

    if (!s && get_token) s = bc_lex_next(&p->l);
  }

  if (s) return s;
  if (TT.sig) return BC_STATUS_SIGNAL;

  while (p->ops.len > ops_bgn) {

    top = BC_PARSE_TOP_OP(p);
    assign = top >= BC_LEX_OP_ASSIGN_POWER && top <= BC_LEX_OP_ASSIGN;

    if (top == BC_LEX_LPAREN || top == BC_LEX_RPAREN)
      return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

    bc_parse_push(p, BC_PARSE_TOKEN_INST(top));

    nexprs -= !BC_PARSE_OP_PREFIX(top);
    bc_vec_pop(&p->ops);
  }

  if (nexprs != 1) return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

  for (i = 0; i < next.len && t != next.tokens[i]; ++i);
  if (i == next.len && !bc_parse_isDelimiter(p))
    return bc_parse_err(p, BC_ERROR_PARSE_EXPR);

  if (!(flags & BC_PARSE_REL) && nrelops) {
    s = bc_parse_posixErr(p, BC_ERROR_POSIX_REL_POS);
    if (s) return s;
  }
  else if ((flags & BC_PARSE_REL) && nrelops > 1) {
    s = bc_parse_posixErr(p, BC_ERROR_POSIX_MULTIREL);
    if (s) return s;
  }

  if (flags & BC_PARSE_PRINT) {
    if (pfirst || !assign) bc_parse_push(p, BC_INST_PRINT);
    bc_parse_push(p, BC_INST_POP);
  }

  // We want to eat newlines if newlines are not a valid ending token.
  // This is for spacing in things like for loop headers.
  for (incdec = 1, i = 0; i < next.len && incdec; ++i)
    incdec = (next.tokens[i] != BC_LEX_NLINE);
  if (incdec) {
    while (!s && p->l.t == BC_LEX_NLINE) s = bc_lex_next(&p->l);
  }

  return s;
}

BcStatus bc_parse_expr_status(BcParse *p, uint8_t flags, BcParseNext next) {

  BcStatus s = bc_parse_expr_err(p, flags, next);

  if (s == BC_STATUS_EMPTY_EXPR) s = bc_parse_err(p, BC_ERROR_PARSE_EMPTY_EXPR);

  return s;
}

static BcStatus bc_program_type_num(BcResult *r, BcNum *n) {
  if (!BC_PROG_NUM(r, n)) return bc_vm_err(BC_ERROR_EXEC_TYPE);
  return BC_STATUS_SUCCESS;
}

static BcStatus bc_program_type_match(BcResult *r, BcType t) {
  if ((r->t != BC_RESULT_ARRAY) != (!t)) return bc_vm_err(BC_ERROR_EXEC_TYPE);
  return BC_STATUS_SUCCESS;
}

static char *bc_program_str(BcProgram *p, size_t idx, int str) {

  BcFunc *f;
  BcVec *v;
  size_t i;

  BcInstPtr *ip = bc_vec_item_rev(&p->stack, 0);
  i = ip->func;

  f = bc_vec_item(&p->fns, i);
  v = str ? &f->strs : &f->consts;

  return *((char**) bc_vec_item(v, idx));
}

static size_t bc_program_index(char *code, size_t *bgn) {

  char amt = (char) code[(*bgn)++], i = 0;
  size_t res = 0;

  for (; i < amt; ++i, ++(*bgn)) {
    size_t temp = ((size_t) ((int) (char) code[*bgn]) & UCHAR_MAX);
    res |= (temp << (i * CHAR_BIT));
  }

  return res;
}

static char *bc_program_name(char *code, size_t *bgn) {

  size_t i;
  char c;
  char *s;
  char *str = code + *bgn, *ptr = strchr(str, UCHAR_MAX);

  s = xmalloc(((unsigned long) ptr) - ((unsigned long) str) + 1);

  for (i = 0; (c = (char) code[(*bgn)++]) && c != UCHAR_MAX; ++i)
    s[i] = (char) c;

  s[i] = '\0';

  return s;
}

static BcVec* bc_program_search(BcProgram *p, char *id, BcType type) {

  struct str_len e, *ptr;
  BcVec *v, *map;
  size_t i;
  BcResultData data;
  int new, var = (type == BC_TYPE_VAR);

  v = var ? &p->vars : &p->arrs;
  map = var ? &p->var_map : &p->arr_map;

  e.str = id;
  e.len = v->len;
  new = bc_map_insert(map, &e, &i);

  if (new) {
    bc_array_init(&data.v, var);
    bc_vec_push(v, &data.v);
  }

  ptr = bc_vec_item(map, i);
  if (new) ptr->str = xstrdup(e.str);

  return bc_vec_item(v, ptr->len);
}

static BcStatus bc_program_num(BcProgram *p, BcResult *r, BcNum **num) {

  BcStatus s = BC_STATUS_SUCCESS;
  BcNum *n = &r->d.n;

  switch (r->t) {

    case BC_RESULT_CONSTANT:
    {
      char *str = bc_program_str(p, r->d.id.len, 0);
      size_t len = strlen(str);

      bc_num_init(n, len);

      s = bc_num_parse(n, str, &p->ib, p->ib_t, len == 1);

      if (s) {
        bc_num_free(n);
        return s;
      }

      r->t = BC_RESULT_TEMP;
    }
    // Fallthrough.
    case BC_RESULT_STR:
    case BC_RESULT_TEMP:
    case BC_RESULT_IBASE:
    case BC_RESULT_SCALE:
    case BC_RESULT_OBASE:
    {
      *num = n;
      break;
    }

    case BC_RESULT_VAR:
    case BC_RESULT_ARRAY:
    case BC_RESULT_ARRAY_ELEM:
    {
      BcVec *v;
      BcType type = (r->t == BC_RESULT_VAR) ? BC_TYPE_VAR : BC_TYPE_ARRAY;

      v = bc_program_search(p, r->d.id.str, type);

      if (r->t == BC_RESULT_ARRAY_ELEM) {

        size_t idx = r->d.id.len;

        v = bc_vec_top(v);

        if (v->len <= idx) bc_array_expand(v, idx + 1);
        *num = bc_vec_item(v, idx);
      }
      else *num = bc_vec_top(v);

      break;
    }

    case BC_RESULT_LAST:
    {
      *num = &p->last;
      break;
    }

    case BC_RESULT_ONE:
    {
      *num = &p->one;
      break;
    }

    case BC_RESULT_VOID:
    {
      s = bc_vm_err(BC_ERROR_EXEC_VOID_VAL);
      break;
    }
  }

  return s;
}

static BcStatus bc_program_operand(BcProgram *p, BcResult **r,
                                   BcNum **n, size_t idx)
{

  *r = bc_vec_item_rev(&p->results, idx);

  return bc_program_num(p, *r, n);
}

static BcStatus bc_program_binPrep(BcProgram *p, BcResult **l, BcNum **ln,
                                   BcResult **r, BcNum **rn)
{
  BcStatus s;
  BcResultType lt;

  s = bc_program_operand(p, l, ln, 1);
  if (s) return s;
  s = bc_program_operand(p, r, rn, 0);
  if (s) return s;

  lt = (*l)->t;

  // We run this again under these conditions in case any vector has been
  // reallocated out from under the BcNums or arrays we had.
  if (lt == (*r)->t && (lt == BC_RESULT_VAR || lt == BC_RESULT_ARRAY_ELEM))
    s = bc_program_num(p, *l, ln);

  if (lt == BC_RESULT_STR) return bc_vm_err(BC_ERROR_EXEC_TYPE);

  return s;
}

static BcStatus bc_program_binOpPrep(BcProgram *p, BcResult **l, BcNum **ln,
                                     BcResult **r, BcNum **rn)
{
  BcStatus s;

  s = bc_program_binPrep(p, l, ln, r, rn);
  if (s) return s;

  s = bc_program_type_num(*l, *ln);
  if (s) return s;

  return bc_program_type_num(*r, *rn);
}

static BcStatus bc_program_assignPrep(BcProgram *p, BcResult **l, BcNum **ln,
                                      BcResult **r, BcNum **rn)
{
  BcStatus s;
  int good = 0;
  BcResultType lt;

  s = bc_program_binPrep(p, l, ln, r, rn);
  if (s) return s;

  lt = (*l)->t;

  if (lt == BC_RESULT_CONSTANT || lt == BC_RESULT_TEMP ||lt == BC_RESULT_ARRAY)
    return bc_vm_err(BC_ERROR_EXEC_TYPE);

  if (lt == BC_RESULT_ONE) return bc_vm_err(BC_ERROR_EXEC_TYPE);

  if (!good) s = bc_program_type_num(*r, *rn);

  return s;
}

static void bc_program_binOpRetire(BcProgram *p, BcResult *r) {
  r->t = BC_RESULT_TEMP;
  bc_vec_pop(&p->results);
  bc_vec_pop(&p->results);
  bc_vec_push(&p->results, r);
}

static BcStatus bc_program_prep(BcProgram *p, BcResult **r, BcNum **n) {

  BcStatus s;

  s = bc_program_operand(p, r, n, 0);
  if (s) return s;

  return bc_program_type_num(*r, *n);
}

static void bc_program_retire(BcProgram *p, BcResult *r, BcResultType t) {
  r->t = t;
  bc_vec_pop(&p->results);
  bc_vec_push(&p->results, r);
}

static BcStatus bc_program_op(BcProgram *p, char inst) {

  BcStatus s;
  BcResult *opd1, *opd2, res;
  BcNum *n1 = NULL, *n2 = NULL;
  size_t idx = inst - BC_INST_POWER;

  s = bc_program_binOpPrep(p, &opd1, &n1, &opd2, &n2);
  if (s) return s;
  bc_num_init(&res.d.n, bc_program_opReqs[idx](n1, n2, p->scale));

  s = bc_program_ops[idx](n1, n2, &res.d.n, p->scale);
  if (s) goto err;
  bc_program_binOpRetire(p, &res);

  return s;

err:
  bc_num_free(&res.d.n);
  return s;
}

static BcStatus bc_program_read(BcProgram *p) {

  BcStatus s;
  BcParse parse;
  BcVec buf;
  BcInstPtr ip;
  size_t i;
  char *file;
  BcFunc *f = bc_vec_item(&p->fns, BC_PROG_READ);

  for (i = 0; i < p->stack.len; ++i) {
    BcInstPtr *ip_ptr = bc_vec_item(&p->stack, i);
    if (ip_ptr->func == BC_PROG_READ)
      return bc_vm_err(BC_ERROR_EXEC_REC_READ);
  }

  file = TT.file;
  bc_lex_file(&parse.l, "<stdin>");
  bc_vec_npop(&f->code, f->code.len);
  bc_vec_init(&buf, sizeof(char), NULL);

  s = bc_read_line(&buf, "read> ");
  if (s) {
    if (s == BC_STATUS_EOF) s = bc_vm_err(BC_ERROR_EXEC_READ_EXPR);
    goto io_err;
  }

  bc_parse_init(&parse, p, BC_PROG_READ);

  s = bc_parse_text(&parse, buf.v);
  if (s) goto exec_err;
  s = bc_parse_expr_status(&parse, BC_PARSE_NOREAD, bc_parse_next_read);
  if (s) goto exec_err;

  if (parse.l.t != BC_LEX_NLINE && parse.l.t != BC_LEX_EOF) {
    s = bc_vm_err(BC_ERROR_EXEC_READ_EXPR);
    goto exec_err;
  }

  ip.func = BC_PROG_READ;
  ip.idx = 0;
  ip.len = p->results.len;

  // Update this pointer, just in case.
  f = bc_vec_item(&p->fns, BC_PROG_READ);

  bc_vec_pushByte(&f->code, BC_INST_RET);
  bc_vec_push(&p->stack, &ip);

exec_err:
  bc_parse_free(&parse);
io_err:
  bc_vec_free(&buf);
  TT.file = file;
  return s;
}

static void bc_program_printChars(char *str) {
  char *nl;
  TT.nchars += printf("%s", str);
  nl = strrchr(str, '\n');
  if (nl) TT.nchars = strlen(nl + 1);
}

// Output, substituting escape sequences, see also unescape() in lib/
static void bc_program_printString(char *str)
{
  int i, c, idx;

  for (i = 0; str[i]; ++i, ++TT.nchars) {
    if ((c = str[i]) == '\\' && str[i+1]) {
      if ((idx = stridx("ab\\efnqrt", c = str[i+1])) >= 0) {
        if (c == 'n') TT.nchars = SIZE_MAX;
        c = "\a\b\\\\\f\n\"\r\t"[idx];
      }
      else putchar('\\');
      i++;
    }

    putchar(c);
  }
}

static BcStatus bc_program_print(BcProgram *p, char inst, size_t idx) {

  BcStatus s = BC_STATUS_SUCCESS;
  BcResult *r;
  char *str;
  BcNum *n = NULL;
  int pop = inst != BC_INST_PRINT;

  r = bc_vec_item_rev(&p->results, idx);

  if (r->t == BC_RESULT_VOID) {
    if (pop) return bc_vm_err(BC_ERROR_EXEC_VOID_VAL);
    return s;
  }

  s = bc_program_num(p, r, &n);
  if (s) return s;

  if (BC_PROG_NUM(r, n)) {
    bc_num_printNewline();

    if (!n->len) bc_num_printHex(0, 1, 0);
    else if (p->ob_t == 10) bc_num_printDecimal(n);
    else s = bc_num_printBase(n, &p->ob, p->ob_t);

    if (!s && !pop) {
      putchar('\n');
      TT.nchars = 0;
    }
    if (!s) bc_num_copy(&p->last, n);
  } else {

    size_t i = (r->t == BC_RESULT_STR) ? r->d.id.len : n->rdx;

    str = bc_program_str(p, i, 1);

    if (inst == BC_INST_PRINT_STR) bc_program_printChars(str);
    else {
      bc_program_printString(str);
      if (inst == BC_INST_PRINT) {
        putchar('\n');
        TT.nchars = 0;
      }
    }
  }

  if (!s && pop) bc_vec_pop(&p->results);

  return s;
}

void bc_program_negate(BcResult *r, BcNum *n) {
  BcNum *rn = &r->d.n;
  bc_num_copy(rn, n);
  if (rn->len) rn->neg = !rn->neg;
}

void bc_program_not(BcResult *r, BcNum *n) {
  if (!BC_NUM_CMP_ZERO(n)) bc_num_one(&r->d.n);
}

static BcStatus bc_program_unary(BcProgram *p, char inst) {

  BcStatus s;
  BcResult res, *ptr;
  BcNum *num = NULL;

  s = bc_program_prep(p, &ptr, &num);
  if (s) return s;

  bc_num_init(&res.d.n, num->len);
  bc_program_unarys[inst - BC_INST_NEG](&res, num);
  bc_program_retire(p, &res, BC_RESULT_TEMP);

  return s;
}

static BcStatus bc_program_logical(BcProgram *p, char inst) {

  BcStatus s;
  BcResult *opd1, *opd2, res;
  BcNum *n1 = NULL, *n2 = NULL;
  int cond = 0;
  ssize_t cmp;

  s = bc_program_binOpPrep(p, &opd1, &n1, &opd2, &n2);
  if (s) return s;
  bc_num_init(&res.d.n, BC_NUM_DEF_SIZE);

  if (inst == BC_INST_BOOL_AND)
    cond = BC_NUM_CMP_ZERO(n1) && BC_NUM_CMP_ZERO(n2);
  else if (inst == BC_INST_BOOL_OR)
    cond = BC_NUM_CMP_ZERO(n1) || BC_NUM_CMP_ZERO(n2);
  else {

    cmp = bc_num_cmp(n1, n2);

    switch (inst) {

      case BC_INST_REL_EQ:
      {
        cond = cmp == 0;
        break;
      }

      case BC_INST_REL_LE:
      {
        cond = cmp <= 0;
        break;
      }

      case BC_INST_REL_GE:
      {
        cond = cmp >= 0;
        break;
      }

      case BC_INST_REL_NE:
      {
        cond = cmp != 0;
        break;
      }

      case BC_INST_REL_LT:
      {
        cond = cmp < 0;
        break;
      }

      case BC_INST_REL_GT:
      {
        cond = cmp > 0;
        break;
      }
    }
  }

  if (cond) bc_num_one(&res.d.n);

  bc_program_binOpRetire(p, &res);

  return s;
}

static BcStatus bc_program_copyToVar(BcProgram *p, char *name,
                                     BcType t, int last)
{
  BcStatus s = BC_STATUS_SUCCESS;
  BcResult *ptr, r;
  BcVec *vec;
  BcNum *n = NULL;
  int var = (t == BC_TYPE_VAR);

  if (!last) {

    ptr = bc_vec_top(&p->results);

    if (ptr->t == BC_RESULT_VAR || ptr->t == BC_RESULT_ARRAY) {
      BcVec *v = bc_program_search(p, ptr->d.id.str, t);
      n = bc_vec_item_rev(v, 1);
    }
    else s = bc_program_num(p, ptr, &n);
  }
  else s = bc_program_operand(p, &ptr, &n, 0);

  if (s) return s;

  s = bc_program_type_match(ptr, t);
  if (s) return s;

  vec = bc_program_search(p, name, t);

  // Do this once more to make sure that pointers were not invalidated.
  vec = bc_program_search(p, name, t);

  if (var) bc_num_createCopy(&r.d.n, n);
  else {
    bc_array_init(&r.d.v, 1);
    bc_array_copy(&r.d.v, (BcVec *)n);
  }

  bc_vec_push(vec, &r.d);
  bc_vec_pop(&p->results);

  return s;
}

static BcStatus bc_program_assign(BcProgram *p, char inst) {

  BcStatus s;
  BcResult *left, *right, res;
  BcNum *l = NULL, *r = NULL;
  int ib, sc;

  s = bc_program_assignPrep(p, &left, &l, &right, &r);
  if (s) return s;

  ib = (left->t == BC_RESULT_IBASE);
  sc = (left->t == BC_RESULT_SCALE);

  if (inst == BC_INST_ASSIGN) bc_num_copy(l, r);
  else {
    s = bc_program_ops[inst - BC_INST_ASSIGN_POWER](l, r, l, p->scale);
    if (s) return s;
  }

  if (ib || sc || left->t == BC_RESULT_OBASE) {

    size_t *ptr;
    unsigned long val, max, min;
    BcError e;

    s = bc_num_ulong(l, &val);
    if (s) return s;
    e = left->t - BC_RESULT_IBASE + BC_ERROR_EXEC_IBASE;

    if (sc) {
      max = BC_MAX_SCALE;
      min = 0;
      ptr = &p->scale;
    }
    else {
      max = ib ? TT.max_ibase : BC_MAX_OBASE;
      min = 2;
      ptr = ib ? &p->ib_t : &p->ob_t;
    }

    if (val > max || val < min) return bc_vm_verr(e, min, max);
    if (!sc) bc_num_ulong2num(ib ? &p->ib : &p->ob, (unsigned long) val);

    *ptr = (size_t) val;
    s = BC_STATUS_SUCCESS;
  }

  bc_num_createCopy(&res.d.n, l);
  bc_program_binOpRetire(p, &res);

  return s;
}

static BcStatus bc_program_pushVar(BcProgram *p, char *code, size_t *bgn) {

  BcStatus s = BC_STATUS_SUCCESS;
  BcResult r;
  char *name = bc_program_name(code, bgn);

  r.t = BC_RESULT_VAR;
  r.d.id.str = name;

  bc_vec_push(&p->results, &r);

  return s;
}

static BcStatus bc_program_pushArray(BcProgram *p, char *code,
                                     size_t *bgn, char inst)
{
  BcStatus s = BC_STATUS_SUCCESS;
  BcResult r;
  BcNum *num = NULL;

  r.d.id.str = bc_program_name(code, bgn);

  if (inst == BC_INST_ARRAY) {
    r.t = BC_RESULT_ARRAY;
    bc_vec_push(&p->results, &r);
  }
  else {

    BcResult *operand;
    unsigned long temp;

    s = bc_program_prep(p, &operand, &num);
    if (s) goto err;
    s = bc_num_ulong(num, &temp);
    if (s) goto err;

    if (temp > BC_MAX_DIM) {
      s = bc_vm_verr(BC_ERROR_EXEC_ARRAY_LEN, BC_MAX_DIM);
      goto err;
    }

    r.d.id.len = temp;
    bc_program_retire(p, &r, BC_RESULT_ARRAY_ELEM);
  }

err:
  if (s) free(r.d.id.str);
  return s;
}

static BcStatus bc_program_incdec(BcProgram *p, char inst) {

  BcStatus s;
  BcResult *ptr, res, copy;
  BcNum *num = NULL;
  char inst2;

  s = bc_program_prep(p, &ptr, &num);
  if (s) return s;

  if (inst == BC_INST_INC_POST || inst == BC_INST_DEC_POST) {
    copy.t = BC_RESULT_TEMP;
    bc_num_createCopy(&copy.d.n, num);
  }

  res.t = BC_RESULT_ONE;
  inst2 = BC_INST_ASSIGN_PLUS + (inst & 0x01);

  bc_vec_push(&p->results, &res);
  bc_program_assign(p, inst2);

  if (inst == BC_INST_INC_POST || inst == BC_INST_DEC_POST) {
    bc_vec_pop(&p->results);
    bc_vec_push(&p->results, &copy);
  }

  return s;
}

static BcStatus bc_program_call(BcProgram *p, char *code,
                                size_t *idx)
{
  BcStatus s = BC_STATUS_SUCCESS;
  BcInstPtr ip;
  size_t i, nparams = bc_program_index(code, idx);
  BcFunc *f;
  BcVec *v;
  struct str_len *a;
  BcResultData param;
  BcResult *arg;

  ip.idx = 0;
  ip.func = bc_program_index(code, idx);
  f = bc_vec_item(&p->fns, ip.func);

  if (!f->code.len) return bc_vm_verr(BC_ERROR_EXEC_UNDEF_FUNC, f->name);
  if (nparams != f->nparams)
    return bc_vm_verr(BC_ERROR_EXEC_PARAMS, f->nparams, nparams);
  ip.len = p->results.len - nparams;

  for (i = 0; i < nparams; ++i) {

    size_t j;
    int last = 1;

    a = bc_vec_item(&f->autos, nparams - 1 - i);
    arg = bc_vec_top(&p->results);

    // If I have already pushed to a var, I need to make sure I
    // get the previous version, not the already pushed one.
    if (arg->t == BC_RESULT_VAR || arg->t == BC_RESULT_ARRAY) {
      for (j = 0; j < i && last; ++j) {
        struct str_len *id = bc_vec_item(&f->autos, nparams - 1 - j);
        last = strcmp(arg->d.id.str, id->str) ||
               (!id->len) != (arg->t == BC_RESULT_VAR);
      }
    }

    s = bc_program_copyToVar(p, a->str, a->len, last);
    if (s) return s;
  }

  for (; i < f->autos.len; ++i) {

    a = bc_vec_item(&f->autos, i);
    v = bc_program_search(p, a->str, a->len);

    if (a->len == BC_TYPE_VAR) {
      bc_num_init(&param.n, BC_NUM_DEF_SIZE);
      bc_vec_push(v, &param.n);
    }
    else {
      bc_array_init(&param.v, 1);
      bc_vec_push(v, &param.v);
    }
  }

  bc_vec_push(&p->stack, &ip);

  return BC_STATUS_SUCCESS;
}

static BcStatus bc_program_return(BcProgram *p, char inst) {

  BcStatus s;
  BcResult res;
  BcFunc *f;
  size_t i;
  BcInstPtr *ip = bc_vec_top(&p->stack);

  f = bc_vec_item(&p->fns, ip->func);
  res.t = BC_RESULT_TEMP;

  if (inst == BC_INST_RET) {

    BcNum *num = NULL;
    BcResult *operand;

    s = bc_program_operand(p, &operand, &num, 0);
    if (s) return s;

    bc_num_createCopy(&res.d.n, num);
  }
  else if (inst == BC_INST_RET_VOID) res.t = BC_RESULT_VOID;
  else bc_num_init(&res.d.n, BC_NUM_DEF_SIZE);

  // We need to pop arguments as well, so this takes that into account.
  for (i = 0; i < f->autos.len; ++i) {

    BcVec *v;
    struct str_len *a = bc_vec_item(&f->autos, i);

    v = bc_program_search(p, a->str, a->len);
    bc_vec_pop(v);
  }

  bc_vec_npop(&p->results, p->results.len - ip->len);
  bc_vec_push(&p->results, &res);
  bc_vec_pop(&p->stack);

  return BC_STATUS_SUCCESS;
}

unsigned long bc_program_scale(BcNum *n) {
  return (unsigned long) n->rdx;
}

unsigned long bc_program_len(BcNum *n) {

  unsigned long len = n->len;
  size_t i;

  if (n->rdx != n->len) return len;
  for (i = n->len - 1; i < n->len && !n->num[i]; --len, --i);

  return len;
}

static BcStatus bc_program_builtin(BcProgram *p, char inst) {

  BcStatus s;
  BcResult *opnd;
  BcResult res;
  BcNum *num = NULL, *resn = &res.d.n;
  int len = (inst == BC_INST_LENGTH);

  s = bc_program_operand(p, &opnd, &num, 0);
  if (s) return s;

  if (inst == BC_INST_SQRT) s = bc_num_sqrt(num, resn, p->scale);
  else if (inst == BC_INST_ABS) {
    bc_num_createCopy(resn, num);
    resn->neg = 0;
  }
  else {

    unsigned long val = 0;

    if (len) {
      if (opnd->t == BC_RESULT_ARRAY)
        val = (unsigned long) ((BcVec*) num)->len;
      else val = bc_program_len(num);
    }
    else val = bc_program_scale(num);

    bc_num_createFromUlong(resn, val);
  }

  bc_program_retire(p, &res, BC_RESULT_TEMP);

  return s;
}

static void bc_program_pushGlobal(BcProgram *p, char inst) {

  BcResult res;
  unsigned long val;

  res.t = inst - BC_INST_IBASE + BC_RESULT_IBASE;
  if (inst == BC_INST_IBASE) val = (unsigned long) p->ib_t;
  else if (inst == BC_INST_SCALE) val = (unsigned long) p->scale;
  else val = (unsigned long) p->ob_t;

  bc_num_createFromUlong(&res.d.n, val);
  bc_vec_push(&p->results, &res);
}

void bc_program_free(BcProgram *p) {
  bc_vec_free(&p->fns);
  bc_vec_free(&p->fn_map);
  bc_vec_free(&p->vars);
  bc_vec_free(&p->var_map);
  bc_vec_free(&p->arrs);
  bc_vec_free(&p->arr_map);
  bc_vec_free(&p->results);
  bc_vec_free(&p->stack);
  bc_num_free(&p->last);
}

void bc_program_init(BcProgram *p) {

  BcInstPtr ip;

  memset(p, 0, sizeof(BcProgram));
  memset(&ip, 0, sizeof(BcInstPtr));

  bc_num_setup(&p->ib, p->ib_num, BC_NUM_LONG_LOG10);
  bc_num_ten(&p->ib);
  p->ib_t = 10;

  bc_num_setup(&p->ob, p->ob_num, BC_NUM_LONG_LOG10);
  bc_num_ten(&p->ob);
  p->ob_t = 10;

  bc_num_setup(&p->one, p->one_num, BC_PROG_ONE_CAP);
  bc_num_one(&p->one);
  bc_num_init(&p->last, BC_NUM_DEF_SIZE);

  bc_vec_init(&p->fns, sizeof(BcFunc), bc_func_free);
  bc_vec_init(&p->fn_map, sizeof(struct str_len), bc_id_free);
  bc_program_insertFunc(p, xstrdup(bc_func_main));
  bc_program_insertFunc(p, xstrdup(bc_func_read));

  bc_vec_init(&p->vars, sizeof(BcVec), bc_vec_free);
  bc_vec_init(&p->var_map, sizeof(struct str_len), bc_id_free);

  bc_vec_init(&p->arrs, sizeof(BcVec), bc_vec_free);
  bc_vec_init(&p->arr_map, sizeof(struct str_len), bc_id_free);

  bc_vec_init(&p->results, sizeof(BcResult), bc_result_free);
  bc_vec_init(&p->stack, sizeof(BcInstPtr), NULL);
  bc_vec_push(&p->stack, &ip);
}

void bc_program_addFunc(BcProgram *p, BcFunc *f, char *name) {
  bc_func_init(f, name);
  bc_vec_push(&p->fns, f);
}

size_t bc_program_insertFunc(BcProgram *p, char *name) {

  struct str_len id;
  BcFunc f;
  int new;
  size_t idx;

  id.str = name;
  id.len = p->fns.len;

  new = bc_map_insert(&p->fn_map, &id, &idx);
  idx = ((struct BcVec *)bc_vec_item(&p->fn_map, idx))->len;

  if (!new) {
    BcFunc *func = bc_vec_item(&p->fns, idx);
    bc_func_reset(func);
    free(name);
  } else bc_program_addFunc(p, &f, name);

  return idx;
}

BcStatus bc_program_reset(BcProgram *p, BcStatus s) {

  BcFunc *f;
  BcInstPtr *ip;

  bc_vec_npop(&p->stack, p->stack.len - 1);
  bc_vec_npop(&p->results, p->results.len);

  f = bc_vec_item(&p->fns, 0);
  ip = bc_vec_top(&p->stack);
  ip->idx = f->code.len;

  if (TT.sig == SIGTERM || TT.sig == SIGQUIT ||
      (!s && TT.sig == SIGINT && FLAG(i))) return BC_STATUS_QUIT;
  TT.sig = 0;

  if (!s || s == BC_STATUS_SIGNAL) {
    if (BC_TTYIN) {
      fputs(bc_program_ready_msg, stderr);
      fflush(stderr);
      s = BC_STATUS_SUCCESS;
    }
    else s = BC_STATUS_QUIT;
  }

  return s;
}

BcStatus bc_program_exec(BcProgram *p) {

  BcStatus s = BC_STATUS_SUCCESS;
  size_t idx;
  BcResult r, *ptr;
  BcInstPtr *ip = bc_vec_top(&p->stack);
  BcFunc *func = bc_vec_item(&p->fns, ip->func);
  char *code = func->code.v;
  int cond = 0;
  BcNum *num;

  while (!s && ip->idx < func->code.len) {

    char inst = (char) code[(ip->idx)++];

    switch (inst) {

      case BC_INST_JUMP_ZERO:
      {
        s = bc_program_prep(p, &ptr, &num);
        if (s) return s;
        cond = !BC_NUM_CMP_ZERO(num);
        bc_vec_pop(&p->results);
      }
      // Fallthrough.
      case BC_INST_JUMP:
      {
        size_t *addr;
        idx = bc_program_index(code, &ip->idx);
        addr = bc_vec_item(&func->labels, idx);
        if (inst == BC_INST_JUMP || cond) ip->idx = *addr;
        break;
      }

      case BC_INST_CALL:
      {
        s = bc_program_call(p, code, &ip->idx);
        break;
      }

      case BC_INST_INC_PRE:
      case BC_INST_DEC_PRE:
      case BC_INST_INC_POST:
      case BC_INST_DEC_POST:
      {
        s = bc_program_incdec(p, inst);
        break;
      }

      case BC_INST_HALT:
      {
        s = BC_STATUS_QUIT;
        break;
      }

      case BC_INST_RET:
      case BC_INST_RET0:
      case BC_INST_RET_VOID:
      {
        s = bc_program_return(p, inst);
        break;
      }

      case BC_INST_LAST:
      {
        r.t = BC_RESULT_LAST;
        bc_vec_push(&p->results, &r);
        break;
      }

      case BC_INST_BOOL_OR:
      case BC_INST_BOOL_AND:
      case BC_INST_REL_EQ:
      case BC_INST_REL_LE:
      case BC_INST_REL_GE:
      case BC_INST_REL_NE:
      case BC_INST_REL_LT:
      case BC_INST_REL_GT:
      {
        s = bc_program_logical(p, inst);
        break;
      }

      case BC_INST_READ:
      {
        s = bc_program_read(p);
        break;
      }

      case BC_INST_VAR:
      {
        s = bc_program_pushVar(p, code, &ip->idx);
        break;
      }

      case BC_INST_ARRAY_ELEM:
      case BC_INST_ARRAY:
      {
        s = bc_program_pushArray(p, code, &ip->idx, inst);
        break;
      }

      case BC_INST_IBASE:
      case BC_INST_SCALE:
      case BC_INST_OBASE:
      {
        bc_program_pushGlobal(p, inst);
        break;
      }

      case BC_INST_LENGTH:
      case BC_INST_SCALE_FUNC:
      case BC_INST_SQRT:
      case BC_INST_ABS:
      {
        s = bc_program_builtin(p, inst);
        break;
      }

      case BC_INST_NUM:
      {
        r.t = BC_RESULT_CONSTANT;
        r.d.id.len = bc_program_index(code, &ip->idx);
        bc_vec_push(&p->results, &r);
        break;
      }

      case BC_INST_POP:
      {
        bc_vec_pop(&p->results);
        break;
      }

      case BC_INST_PRINT:
      case BC_INST_PRINT_POP:
      case BC_INST_PRINT_STR:
      {
        s = bc_program_print(p, inst, 0);
        break;
      }

      case BC_INST_STR:
      {
        r.t = BC_RESULT_STR;
        r.d.id.len = bc_program_index(code, &ip->idx);
        bc_vec_push(&p->results, &r);
        break;
      }

      case BC_INST_POWER:
      case BC_INST_MULTIPLY:
      case BC_INST_DIVIDE:
      case BC_INST_MODULUS:
      case BC_INST_PLUS:
      case BC_INST_MINUS:
      {
        s = bc_program_op(p, inst);
        break;
      }

      case BC_INST_NEG:
      case BC_INST_BOOL_NOT:
      {
        s = bc_program_unary(p, inst);
        break;
      }

      case BC_INST_ASSIGN_POWER:
      case BC_INST_ASSIGN_MULTIPLY:
      case BC_INST_ASSIGN_DIVIDE:
      case BC_INST_ASSIGN_MODULUS:
      case BC_INST_ASSIGN_PLUS:
      case BC_INST_ASSIGN_MINUS:
      case BC_INST_ASSIGN:
      {
        s = bc_program_assign(p, inst);
        break;
      }
    }

    if ((s && s != BC_STATUS_QUIT) || TT.sig) s = bc_program_reset(p, s);

    // If the stack has changed, pointers may be invalid.
    ip = bc_vec_top(&p->stack);
    func = bc_vec_item(&p->fns, ip->func);
    code = func->code.v;
  }

  return s;
}

static void bc_vm_sig(int sig) {
  int err = errno;

  // If you run bc 2>/dev/full ctrl-C is ignored. Why? No idea.
  if (sig == SIGINT) {
    size_t len = strlen(bc_sig_msg);
    if (write(2, bc_sig_msg, len) != len) sig = 0;
  }
  TT.sig = sig;
  errno = err;
}

void bc_vm_info(void) {
  printf("%s %s\n", toys.which->name, "1.1.0");
  fputs(bc_copyright, stdout);
}

static void bc_vm_printError(BcError e, char *fmt,
                             size_t line, va_list args)
{
  // Make sure all of stdout is written first.
  fflush(stdout);

  fprintf(stderr, fmt, bc_errs[(size_t) bc_err_ids[e]]);
  vfprintf(stderr, bc_err_msgs[e], args);

  // This is the condition for parsing vs runtime.
  // If line is not 0, it is parsing.
  if (line) {
    fprintf(stderr, "\n    %s", TT.file);
    fprintf(stderr, bc_err_line, line);
  }
  else {
    BcInstPtr *ip = bc_vec_item_rev(&BC_VM->prog.stack, 0);
    BcFunc *f = bc_vec_item(&BC_VM->prog.fns, ip->func);
    fprintf(stderr, "\n    Function: %s", f->name);
  }

  fputs("\n\n", stderr);
  fflush(stderr);
}

BcStatus bc_vm_error(BcError e, size_t line, ...) {

  va_list args;

  va_start(args, line);
  bc_vm_printError(e, bc_err_fmt, line, args);
  va_end(args);

  return BC_STATUS_ERROR;
}

BcStatus bc_vm_posixError(BcError e, size_t line, ...) {

  va_list args;

  if (!(FLAG(s) || FLAG(w))) return BC_STATUS_SUCCESS;

  va_start(args, line);
  bc_vm_printError(e, FLAG(s) ? bc_err_fmt : bc_warn_fmt, line, args);
  va_end(args);

  return FLAG(s) ? BC_STATUS_ERROR : BC_STATUS_SUCCESS;
}

static void bc_vm_clean()
{
  BcProgram *prog = &BC_VM->prog;
  BcFunc *f = bc_vec_item(&prog->fns, BC_PROG_MAIN);
  BcInstPtr *ip = bc_vec_item(&prog->stack, 0);

  // If this condition is 1, we can get rid of strings,
  // constants, and code. This is an idea from busybox.
  if (!BC_PARSE_NO_EXEC(&BC_VM->prs) && prog->stack.len == 1 &&
      !prog->results.len && ip->idx == f->code.len)
  {
    bc_vec_npop(&f->labels, f->labels.len);
    bc_vec_npop(&f->strs, f->strs.len);
    bc_vec_npop(&f->consts, f->consts.len);
    bc_vec_npop(&f->code, f->code.len);
    ip->idx = 0;
  }
}

static BcStatus bc_vm_process(char *text, int is_stdin) {

  BcStatus s;
  uint16_t *flags;

  s = bc_parse_text(&BC_VM->prs, text);
  if (s) goto err;

  while (BC_VM->prs.l.t != BC_LEX_EOF) {
    s = bc_parse_parse(&BC_VM->prs);
    if (s) goto err;
  }

  flags = BC_PARSE_TOP_FLAG_PTR(&BC_VM->prs);

  if (!is_stdin && BC_VM->prs.flags.len == 1 && *flags == BC_PARSE_FLAG_IF_END)
    bc_parse_noElse(&BC_VM->prs);

  if (BC_PARSE_NO_EXEC(&BC_VM->prs)) goto err;

  s = bc_program_exec(&BC_VM->prog);
  if (FLAG(i)) fflush(stdout);

err:
  if (s || TT.sig) s = bc_program_reset(&BC_VM->prog, s);
  bc_vm_clean();
  return s == BC_STATUS_QUIT || !FLAG(i) || !is_stdin ? s : BC_STATUS_SUCCESS;
}

static BcStatus bc_vm_file(char *file) {

  BcStatus s;
  char *data;

  bc_lex_file(&BC_VM->prs.l, file);
  s = bc_read_file(file, &data);
  if (s) return s;

  s = bc_vm_process(data, 0);
  if (s) goto err;

  if (BC_PARSE_NO_EXEC(&BC_VM->prs))
    s = bc_parse_err(&BC_VM->prs, BC_ERROR_PARSE_BLOCK);

err:
  free(data);
  return s;
}

static BcStatus bc_vm_stdin(void) {

  BcStatus s = BC_STATUS_SUCCESS;
  BcVec buf, buffer;
  size_t string = 0;
  int comment = 0, done = 0;

  bc_lex_file(&BC_VM->prs.l, "<stdin>");

  bc_vec_init(&buffer, sizeof(char), NULL);
  bc_vec_init(&buf, sizeof(char), NULL);
  bc_vec_pushByte(&buffer, '\0');

  // This loop is complex because the vm tries not to send any lines that end
  // with a backslash to the parser, which
  // treats a backslash+newline combo as whitespace per the bc spec. In that
  // case, and for strings and comments, the parser will expect more stuff.
  while (!done && (s = bc_read_line(&buf, ">>> ")) != BC_STATUS_ERROR &&
         buf.len > 1 && !TT.sig && s != BC_STATUS_SIGNAL)
  {
    char c2, *str = buf.v;
    size_t i, len = buf.len - 1;

    done = (s == BC_STATUS_EOF);

    if (len >= 2 && str[len - 1] == '\n' && str[len - 2] == '\\') {
      bc_vec_concat(&buffer, buf.v);
      continue;
    }

    for (i = 0; i < len; ++i) {

      int notend = len > i + 1;
      char c = (char) str[i];

      if (!comment && (i - 1 > len || str[i - 1] != '\\')) string ^= c == '"';

      if (!string && notend) {

        c2 = str[i + 1];

        if (c == '/' && !comment && c2 == '*') {
          comment = 1;
          ++i;
        }
        else if (c == '*' && comment && c2 == '/') {
          comment = 0;
          ++i;
        }
      }
    }

    bc_vec_concat(&buffer, buf.v);

    if (string || comment) continue;
    if (len >= 2 && str[len - 2] == '\\' && str[len - 1] == '\n') continue;

    s = bc_vm_process(buffer.v, 1);
    if (s) goto err;

    bc_vec_empty(&buffer);
  }

  if (s && s != BC_STATUS_EOF) goto err;
  else if (TT.sig && !s) s = BC_STATUS_SIGNAL;
  else if (s != BC_STATUS_ERROR) {
    if (comment) s = bc_parse_err(&BC_VM->prs, BC_ERROR_PARSE_COMMENT);
    else if (string) s = bc_parse_err(&BC_VM->prs, BC_ERROR_PARSE_STRING);
    else if (BC_PARSE_NO_EXEC(&BC_VM->prs))
      s = bc_parse_err(&BC_VM->prs, BC_ERROR_PARSE_BLOCK);
  }

err:
  bc_vec_free(&buf);
  bc_vec_free(&buffer);
  return s;
}

void bc_main(void)
{
  BcStatus s = 0;
  char *ss;

  struct sigaction sa;

  sigemptyset(&sa.sa_mask);
  sa.sa_handler = bc_vm_sig;
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);

  TT.line_len = 69;
  ss = getenv("BC_LINE_LENGTH");
  if (ss) {
    int len = atoi(ss);
    if (len>=2 && len <= UINT16_MAX) TT.line_len = len;
  }

  TT.vm = xzalloc(sizeof(BcVm));
  bc_program_init(&BC_VM->prog);
  bc_parse_init(&BC_VM->prs, &BC_VM->prog, BC_PROG_MAIN);

  if (getenv("POSIXLY_CORRECT")) toys.optflags |= FLAG_s;
  toys.optflags |= isatty(0) ? BC_FLAG_TTYIN : 0;
  toys.optflags |= BC_TTYIN && isatty(1) ? FLAG_i : 0;

  TT.max_ibase = !FLAG(s) && !FLAG(w) ? 16 : 36;

  if (FLAG(i) && !(toys.optflags & FLAG_q)) bc_vm_info();

  // load -l library (if any)
  if (FLAG(l)) {
    bc_lex_file(&BC_VM->prs.l, bc_lib_name);
    s = bc_parse_text(&BC_VM->prs, bc_lib);

    while (!s && BC_VM->prs.l.t != BC_LEX_EOF) s = bc_parse_parse(&BC_VM->prs);
  }

  // parse command line argument files, then stdin
  if (!s) {
    int i;

    for (i = 0; !s && i < toys.optc; ++i) s = bc_vm_file(toys.optargs[i]);
    if (!s) s = bc_vm_stdin();
  }

  if (CFG_TOYBOX_FREE) {
    bc_program_free(&BC_VM->prog);
    bc_parse_free(&BC_VM->prs);
    free(TT.vm);
  }

  toys.exitval = s == BC_STATUS_ERROR;
}
