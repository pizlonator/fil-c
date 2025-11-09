/* A Bison parser, made by Lrama 0.5.12.  */

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
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* First part of user prologue.  */
#line 14 "parse.y"


#if !YYPURE
# error needs pure parser
#endif
#define YYDEBUG 1
#define YYERROR_VERBOSE 1
#define YYSTACK_USE_ALLOCA 0
#define YYLTYPE rb_code_location_t
#define YYLTYPE_IS_DECLARED 1

/* For Ripper */
#ifdef RUBY_EXTCONF_H
# include RUBY_EXTCONF_H
#endif

#include "ruby/internal/config.h"

#include <errno.h>

#ifdef UNIVERSAL_PARSER

#include "internal/ruby_parser.h"
#include "parser_node.h"
#include "universal_parser.c"

#ifdef RIPPER
#undef T_NODE
#define T_NODE 0x1b
#define STATIC_ID2SYM p->config->static_id2sym
#define rb_str_coderange_scan_restartable p->config->str_coderange_scan_restartable
#endif

#else

#include "internal.h"
#include "internal/compile.h"
#include "internal/compilers.h"
#include "internal/complex.h"
#include "internal/encoding.h"
#include "internal/error.h"
#include "internal/hash.h"
#include "internal/imemo.h"
#include "internal/io.h"
#include "internal/numeric.h"
#include "internal/parse.h"
#include "internal/rational.h"
#include "internal/re.h"
#include "internal/ruby_parser.h"
#include "internal/symbol.h"
#include "internal/thread.h"
#include "internal/variable.h"
#include "node.h"
#include "parser_node.h"
#include "probes.h"
#include "regenc.h"
#include "ruby/encoding.h"
#include "ruby/regex.h"
#include "ruby/ruby.h"
#include "ruby/st.h"
#include "ruby/util.h"
#include "ruby/ractor.h"
#include "symbol.h"

#ifndef RIPPER
static void
bignum_negate(VALUE b)
{
    BIGNUM_NEGATE(b);
}

static void
rational_set_num(VALUE r, VALUE n)
{
    RATIONAL_SET_NUM(r, n);
}

static VALUE
rational_get_num(VALUE obj)
{
    return RRATIONAL(obj)->num;
}

static void
rcomplex_set_real(VALUE cmp, VALUE r)
{
    RCOMPLEX_SET_REAL(cmp, r);
}

static VALUE
rcomplex_get_real(VALUE obj)
{
    return RCOMPLEX(obj)->real;
}

static void
rcomplex_set_imag(VALUE cmp, VALUE i)
{
    RCOMPLEX_SET_IMAG(cmp, i);
}

static VALUE
rcomplex_get_imag(VALUE obj)
{
    return RCOMPLEX(obj)->imag;
}

static bool
hash_literal_key_p(VALUE k)
{
    switch (OBJ_BUILTIN_TYPE(k)) {
      case T_NODE:
        return false;
      default:
        return true;
    }
}

static int
literal_cmp(VALUE val, VALUE lit)
{
    if (val == lit) return 0;
    if (!hash_literal_key_p(val) || !hash_literal_key_p(lit)) return -1;
    return rb_iseq_cdhash_cmp(val, lit);
}

static st_index_t
literal_hash(VALUE a)
{
    if (!hash_literal_key_p(a)) return (st_index_t)a;
    return rb_iseq_cdhash_hash(a);
}

static VALUE
syntax_error_new(void)
{
    return rb_class_new_instance(0, 0, rb_eSyntaxError);
}

static NODE *reg_named_capture_assign(struct parser_params* p, VALUE regexp, const YYLTYPE *loc);
#endif /* !RIPPER */

#define compile_callback rb_suppress_tracing
VALUE rb_io_gets_internal(VALUE io);

VALUE rb_node_case_when_optimizable_literal(const NODE *const node);
#endif /* !UNIVERSAL_PARSER */

static inline int
parse_isascii(int c)
{
    return '\0' <= c && c <= '\x7f';
}

#undef ISASCII
#define ISASCII parse_isascii

static inline int
parse_isspace(int c)
{
    return c == ' ' || ('\t' <= c && c <= '\r');
}

#undef ISSPACE
#define ISSPACE parse_isspace

static inline int
parse_iscntrl(int c)
{
    return ('\0' <= c && c < ' ') || c == '\x7f';
}

#undef ISCNTRL
#define ISCNTRL(c) parse_iscntrl(c)

static inline int
parse_isupper(int c)
{
    return 'A' <= c && c <= 'Z';
}

static inline int
parse_islower(int c)
{
    return 'a' <= c && c <= 'z';
}

static inline int
parse_isalpha(int c)
{
    return parse_isupper(c) || parse_islower(c);
}

#undef ISALPHA
#define ISALPHA(c) parse_isalpha(c)

static inline int
parse_isdigit(int c)
{
    return '0' <= c && c <= '9';
}

#undef ISDIGIT
#define ISDIGIT(c) parse_isdigit(c)

static inline int
parse_isalnum(int c)
{
    return parse_isalpha(c) || parse_isdigit(c);
}

#undef ISALNUM
#define ISALNUM(c) parse_isalnum(c)

static inline int
parse_isxdigit(int c)
{
    return parse_isdigit(c) || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}

#undef ISXDIGIT
#define ISXDIGIT(c) parse_isxdigit(c)

#include "parser_st.h"

#undef STRCASECMP
#define STRCASECMP rb_parser_st_locale_insensitive_strcasecmp

#undef STRNCASECMP
#define STRNCASECMP rb_parser_st_locale_insensitive_strncasecmp

#ifdef RIPPER
#include "ripper_init.h"
#endif

enum shareability {
    shareable_none,
    shareable_literal,
    shareable_copy,
    shareable_everything,
};

enum rescue_context {
    before_rescue,
    after_rescue,
    after_else,
    after_ensure,
};

struct lex_context {
    unsigned int in_defined: 1;
    unsigned int in_kwarg: 1;
    unsigned int in_argdef: 1;
    unsigned int in_def: 1;
    unsigned int in_class: 1;
    BITFIELD(enum shareability, shareable_constant_value, 2);
    BITFIELD(enum rescue_context, in_rescue, 2);
};

typedef struct RNode_DEF_TEMP rb_node_def_temp_t;
typedef struct RNode_EXITS rb_node_exits_t;

#if defined(__GNUC__) && !defined(__clang__)
// Suppress "parameter passing for argument of type 'struct
// lex_context' changed" notes.  `struct lex_context` is file scope,
// and has no ABI compatibility issue.
RBIMPL_WARNING_PUSH()
RBIMPL_WARNING_IGNORED(-Wpsabi)
RBIMPL_WARNING_POP()
// Not sure why effective even after popped.
#endif

#include "parse.h"

#define NO_LEX_CTXT (struct lex_context){0}

#define AREF(ary, i) RARRAY_AREF(ary, i)

#ifndef WARN_PAST_SCOPE
# define WARN_PAST_SCOPE 0
#endif

#define TAB_WIDTH 8

#define yydebug (p->debug)	/* disable the global variable definition */

#define YYMALLOC(size)		rb_parser_malloc(p, (size))
#define YYREALLOC(ptr, size)	rb_parser_realloc(p, (ptr), (size))
#define YYCALLOC(nelem, size)	rb_parser_calloc(p, (nelem), (size))
#define YYFREE(ptr)		rb_parser_free(p, (ptr))
#define YYFPRINTF(out, ...)	rb_parser_printf(p, __VA_ARGS__)
#define YY_LOCATION_PRINT(File, loc, p) \
     rb_parser_printf(p, "%d.%d-%d.%d", \
                      (loc).beg_pos.lineno, (loc).beg_pos.column,\
                      (loc).end_pos.lineno, (loc).end_pos.column)
#define YYLLOC_DEFAULT(Current, Rhs, N)					\
    do									\
      if (N)								\
        {								\
          (Current).beg_pos = YYRHSLOC(Rhs, 1).beg_pos;			\
          (Current).end_pos = YYRHSLOC(Rhs, N).end_pos;			\
        }								\
      else								\
        {                                                               \
          (Current).beg_pos = YYRHSLOC(Rhs, 0).end_pos;                 \
          (Current).end_pos = YYRHSLOC(Rhs, 0).end_pos;                 \
        }                                                               \
    while (0)
#define YY_(Msgid) \
    (((Msgid)[0] == 'm') && (strcmp((Msgid), "memory exhausted") == 0) ? \
     "nesting too deep" : (Msgid))

#define RUBY_SET_YYLLOC_FROM_STRTERM_HEREDOC(Current)			\
    rb_parser_set_location_from_strterm_heredoc(p, &p->lex.strterm->u.heredoc, &(Current))
#define RUBY_SET_YYLLOC_OF_DELAYED_TOKEN(Current)			\
    rb_parser_set_location_of_delayed_token(p, &(Current))
#define RUBY_SET_YYLLOC_OF_HEREDOC_END(Current)				\
    rb_parser_set_location_of_heredoc_end(p, &(Current))
#define RUBY_SET_YYLLOC_OF_DUMMY_END(Current)				\
    rb_parser_set_location_of_dummy_end(p, &(Current))
#define RUBY_SET_YYLLOC_OF_NONE(Current)				\
    rb_parser_set_location_of_none(p, &(Current))
#define RUBY_SET_YYLLOC(Current)					\
    rb_parser_set_location(p, &(Current))
#define RUBY_INIT_YYLLOC() \
    { \
        {p->ruby_sourceline, (int)(p->lex.ptok - p->lex.pbeg)}, \
        {p->ruby_sourceline, (int)(p->lex.pcur - p->lex.pbeg)}, \
    }

#define IS_lex_state_for(x, ls)	((x) & (ls))
#define IS_lex_state_all_for(x, ls) (((x) & (ls)) == (ls))
#define IS_lex_state(ls)	IS_lex_state_for(p->lex.state, (ls))
#define IS_lex_state_all(ls)	IS_lex_state_all_for(p->lex.state, (ls))

# define SET_LEX_STATE(ls) \
    parser_set_lex_state(p, ls, __LINE__)
static inline enum lex_state_e parser_set_lex_state(struct parser_params *p, enum lex_state_e ls, int line);

typedef uintptr_t stack_type;

static const rb_code_location_t NULL_LOC = { {0, -1}, {0, -1} };

# define SHOW_BITSTACK(stack, name) (p->debug ? rb_parser_show_bitstack(p, stack, name, __LINE__) : (void)0)
# define BITSTACK_PUSH(stack, n) (((p->stack) = ((p->stack)<<1)|((n)&1)), SHOW_BITSTACK(p->stack, #stack"(push)"))
# define BITSTACK_POP(stack)	 (((p->stack) = (p->stack) >> 1), SHOW_BITSTACK(p->stack, #stack"(pop)"))
# define BITSTACK_SET_P(stack)	 (SHOW_BITSTACK(p->stack, #stack), (p->stack)&1)
# define BITSTACK_SET(stack, n)	 ((p->stack)=(n), SHOW_BITSTACK(p->stack, #stack"(set)"))

/* A flag to identify keyword_do_cond, "do" keyword after condition expression.
   Examples: `while ... do`, `until ... do`, and `for ... in ... do` */
#define COND_PUSH(n)	BITSTACK_PUSH(cond_stack, (n))
#define COND_POP()	BITSTACK_POP(cond_stack)
#define COND_P()	BITSTACK_SET_P(cond_stack)
#define COND_SET(n)	BITSTACK_SET(cond_stack, (n))

/* A flag to identify keyword_do_block; "do" keyword after command_call.
   Example: `foo 1, 2 do`. */
#define CMDARG_PUSH(n)	BITSTACK_PUSH(cmdarg_stack, (n))
#define CMDARG_POP()	BITSTACK_POP(cmdarg_stack)
#define CMDARG_P()	BITSTACK_SET_P(cmdarg_stack)
#define CMDARG_SET(n)	BITSTACK_SET(cmdarg_stack, (n))

struct vtable {
    ID *tbl;
    int pos;
    int capa;
    struct vtable *prev;
};

struct local_vars {
    struct vtable *args;
    struct vtable *vars;
    struct vtable *used;
# if WARN_PAST_SCOPE
    struct vtable *past;
# endif
    struct local_vars *prev;
# ifndef RIPPER
    struct {
        NODE *outer, *inner, *current;
    } numparam;
# endif
};

enum {
    ORDINAL_PARAM = -1,
    NO_PARAM = 0,
    NUMPARAM_MAX = 9,
};

#define DVARS_INHERIT ((void*)1)
#define DVARS_TOPSCOPE NULL
#define DVARS_TERMINAL_P(tbl) ((tbl) == DVARS_INHERIT || (tbl) == DVARS_TOPSCOPE)

typedef struct token_info {
    const char *token;
    rb_code_position_t beg;
    int indent;
    int nonspc;
    struct token_info *next;
} token_info;

/*
    Structure of Lexer Buffer:

 lex.pbeg     lex.ptok     lex.pcur     lex.pend
    |            |            |            |
    |------------+------------+------------|
                 |<---------->|
                     token
*/
struct parser_params {
    rb_imemo_tmpbuf_t *heap;

    YYSTYPE *lval;
    YYLTYPE *yylloc;

    struct {
        rb_strterm_t *strterm;
        VALUE (*gets)(struct parser_params*,VALUE);
        VALUE input;
        VALUE lastline;
        VALUE nextline;
        const char *pbeg;
        const char *pcur;
        const char *pend;
        const char *ptok;
        union {
            long ptr;
            VALUE (*call)(VALUE, int);
        } gets_;
        enum lex_state_e state;
        /* track the nest level of any parens "()[]{}" */
        int paren_nest;
        /* keep p->lex.paren_nest at the beginning of lambda "->" to detect tLAMBEG and keyword_do_LAMBDA */
        int lpar_beg;
        /* track the nest level of only braces "{}" */
        int brace_nest;
    } lex;
    stack_type cond_stack;
    stack_type cmdarg_stack;
    int tokidx;
    int toksiz;
    int heredoc_end;
    int heredoc_indent;
    int heredoc_line_indent;
    char *tokenbuf;
    struct local_vars *lvtbl;
    st_table *pvtbl;
    st_table *pktbl;
    int line_count;
    int ruby_sourceline;	/* current line no. */
    const char *ruby_sourcefile; /* current source file */
    VALUE ruby_sourcefile_string;
    rb_encoding *enc;
    token_info *token_info;
    VALUE case_labels;
    rb_node_exits_t *exits;

    VALUE debug_buffer;
    VALUE debug_output;

    struct {
        VALUE token;
        int beg_line;
        int beg_col;
        int end_line;
        int end_col;
    } delayed;

    ID cur_arg;

    rb_ast_t *ast;
    int node_id;

    int max_numparam;

    struct lex_context ctxt;

#ifdef UNIVERSAL_PARSER
    rb_parser_config_t *config;
#endif
    /* compile_option */
    signed int frozen_string_literal:2; /* -1: not specified, 0: false, 1: true */

    unsigned int command_start:1;
    unsigned int eofp: 1;
    unsigned int ruby__end__seen: 1;
    unsigned int debug: 1;
    unsigned int has_shebang: 1;
    unsigned int token_seen: 1;
    unsigned int token_info_enabled: 1;
# if WARN_PAST_SCOPE
    unsigned int past_scope_enabled: 1;
# endif
    unsigned int error_p: 1;
    unsigned int cr_seen: 1;

#ifndef RIPPER
    /* Ruby core only */

    unsigned int do_print: 1;
    unsigned int do_loop: 1;
    unsigned int do_chomp: 1;
    unsigned int do_split: 1;
    unsigned int error_tolerant: 1;
    unsigned int keep_tokens: 1;

    NODE *eval_tree_begin;
    NODE *eval_tree;
    VALUE error_buffer;
    VALUE debug_lines;
    const struct rb_iseq_struct *parent_iseq;
    /* store specific keyword locations to generate dummy end token */
    VALUE end_expect_token_locations;
    /* id for terms */
    int token_id;
    /* Array for term tokens */
    VALUE tokens;
#else
    /* Ripper only */

    VALUE value;
    VALUE result;
    VALUE parsing_thread;
#endif
};

#define NUMPARAM_ID_P(id) numparam_id_p(p, id)
#define NUMPARAM_ID_TO_IDX(id) (unsigned int)(((id) >> ID_SCOPE_SHIFT) - (tNUMPARAM_1 - 1))
#define NUMPARAM_IDX_TO_ID(idx) TOKEN2LOCALID((tNUMPARAM_1 - 1 + (idx)))
static int
numparam_id_p(struct parser_params *p, ID id)
{
    if (!is_local_id(id) || id < (tNUMPARAM_1 << ID_SCOPE_SHIFT)) return 0;
    unsigned int idx = NUMPARAM_ID_TO_IDX(id);
    return idx > 0 && idx <= NUMPARAM_MAX;
}
static void numparam_name(struct parser_params *p, ID id);


#define intern_cstr(n,l,en) rb_intern3(n,l,en)

#define STR_NEW(ptr,len) rb_enc_str_new((ptr),(len),p->enc)
#define STR_NEW0() rb_enc_str_new(0,0,p->enc)
#define STR_NEW2(ptr) rb_enc_str_new((ptr),strlen(ptr),p->enc)
#define STR_NEW3(ptr,len,e,func) parser_str_new(p, (ptr),(len),(e),(func),p->enc)
#define TOK_INTERN() intern_cstr(tok(p), toklen(p), p->enc)
#define VALID_SYMNAME_P(s, l, enc, type) (rb_enc_symname_type(s, l, enc, (1U<<(type))) == (int)(type))

static inline bool
end_with_newline_p(struct parser_params *p, VALUE str)
{
    return RSTRING_LEN(str) > 0 && RSTRING_END(str)[-1] == '\n';
}

static void
pop_pvtbl(struct parser_params *p, st_table *tbl)
{
    st_free_table(p->pvtbl);
    p->pvtbl = tbl;
}

static void
pop_pktbl(struct parser_params *p, st_table *tbl)
{
    if (p->pktbl) st_free_table(p->pktbl);
    p->pktbl = tbl;
}

#ifndef RIPPER
static void flush_debug_buffer(struct parser_params *p, VALUE out, VALUE str);

static void
debug_end_expect_token_locations(struct parser_params *p, const char *name)
{
    if(p->debug) {
        VALUE mesg = rb_sprintf("%s: ", name);
        rb_str_catf(mesg, " %"PRIsVALUE"\n", p->end_expect_token_locations);
        flush_debug_buffer(p, p->debug_output, mesg);
    }
}

static void
push_end_expect_token_locations(struct parser_params *p, const rb_code_position_t *pos)
{
    if(NIL_P(p->end_expect_token_locations)) return;
    rb_ary_push(p->end_expect_token_locations, rb_ary_new_from_args(2, INT2NUM(pos->lineno), INT2NUM(pos->column)));
    debug_end_expect_token_locations(p, "push_end_expect_token_locations");
}

static void
pop_end_expect_token_locations(struct parser_params *p)
{
    if(NIL_P(p->end_expect_token_locations)) return;
    rb_ary_pop(p->end_expect_token_locations);
    debug_end_expect_token_locations(p, "pop_end_expect_token_locations");
}

static VALUE
peek_end_expect_token_locations(struct parser_params *p)
{
    if(NIL_P(p->end_expect_token_locations)) return Qnil;
    return rb_ary_last(0, 0, p->end_expect_token_locations);
}

static ID
parser_token2id(struct parser_params *p, enum yytokentype tok)
{
    switch ((int) tok) {
#define TOKEN2ID(tok) case tok: return rb_intern(#tok);
#define TOKEN2ID2(tok, name) case tok: return rb_intern(name);
      TOKEN2ID2(' ', "words_sep")
      TOKEN2ID2('!', "!")
      TOKEN2ID2('%', "%");
      TOKEN2ID2('&', "&");
      TOKEN2ID2('*', "*");
      TOKEN2ID2('+', "+");
      TOKEN2ID2('-', "-");
      TOKEN2ID2('/', "/");
      TOKEN2ID2('<', "<");
      TOKEN2ID2('=', "=");
      TOKEN2ID2('>', ">");
      TOKEN2ID2('?', "?");
      TOKEN2ID2('^', "^");
      TOKEN2ID2('|', "|");
      TOKEN2ID2('~', "~");
      TOKEN2ID2(':', ":");
      TOKEN2ID2(',', ",");
      TOKEN2ID2('.', ".");
      TOKEN2ID2(';', ";");
      TOKEN2ID2('`', "`");
      TOKEN2ID2('\n', "nl");
      TOKEN2ID2('{', "{");
      TOKEN2ID2('}', "}");
      TOKEN2ID2('[', "[");
      TOKEN2ID2(']', "]");
      TOKEN2ID2('(', "(");
      TOKEN2ID2(')', ")");
      TOKEN2ID2('\\', "backslash");
      TOKEN2ID(keyword_class);
      TOKEN2ID(keyword_module);
      TOKEN2ID(keyword_def);
      TOKEN2ID(keyword_undef);
      TOKEN2ID(keyword_begin);
      TOKEN2ID(keyword_rescue);
      TOKEN2ID(keyword_ensure);
      TOKEN2ID(keyword_end);
      TOKEN2ID(keyword_if);
      TOKEN2ID(keyword_unless);
      TOKEN2ID(keyword_then);
      TOKEN2ID(keyword_elsif);
      TOKEN2ID(keyword_else);
      TOKEN2ID(keyword_case);
      TOKEN2ID(keyword_when);
      TOKEN2ID(keyword_while);
      TOKEN2ID(keyword_until);
      TOKEN2ID(keyword_for);
      TOKEN2ID(keyword_break);
      TOKEN2ID(keyword_next);
      TOKEN2ID(keyword_redo);
      TOKEN2ID(keyword_retry);
      TOKEN2ID(keyword_in);
      TOKEN2ID(keyword_do);
      TOKEN2ID(keyword_do_cond);
      TOKEN2ID(keyword_do_block);
      TOKEN2ID(keyword_do_LAMBDA);
      TOKEN2ID(keyword_return);
      TOKEN2ID(keyword_yield);
      TOKEN2ID(keyword_super);
      TOKEN2ID(keyword_self);
      TOKEN2ID(keyword_nil);
      TOKEN2ID(keyword_true);
      TOKEN2ID(keyword_false);
      TOKEN2ID(keyword_and);
      TOKEN2ID(keyword_or);
      TOKEN2ID(keyword_not);
      TOKEN2ID(modifier_if);
      TOKEN2ID(modifier_unless);
      TOKEN2ID(modifier_while);
      TOKEN2ID(modifier_until);
      TOKEN2ID(modifier_rescue);
      TOKEN2ID(keyword_alias);
      TOKEN2ID(keyword_defined);
      TOKEN2ID(keyword_BEGIN);
      TOKEN2ID(keyword_END);
      TOKEN2ID(keyword__LINE__);
      TOKEN2ID(keyword__FILE__);
      TOKEN2ID(keyword__ENCODING__);
      TOKEN2ID(tIDENTIFIER);
      TOKEN2ID(tFID);
      TOKEN2ID(tGVAR);
      TOKEN2ID(tIVAR);
      TOKEN2ID(tCONSTANT);
      TOKEN2ID(tCVAR);
      TOKEN2ID(tLABEL);
      TOKEN2ID(tINTEGER);
      TOKEN2ID(tFLOAT);
      TOKEN2ID(tRATIONAL);
      TOKEN2ID(tIMAGINARY);
      TOKEN2ID(tCHAR);
      TOKEN2ID(tNTH_REF);
      TOKEN2ID(tBACK_REF);
      TOKEN2ID(tSTRING_CONTENT);
      TOKEN2ID(tREGEXP_END);
      TOKEN2ID(tDUMNY_END);
      TOKEN2ID(tSP);
      TOKEN2ID(tUPLUS);
      TOKEN2ID(tUMINUS);
      TOKEN2ID(tPOW);
      TOKEN2ID(tCMP);
      TOKEN2ID(tEQ);
      TOKEN2ID(tEQQ);
      TOKEN2ID(tNEQ);
      TOKEN2ID(tGEQ);
      TOKEN2ID(tLEQ);
      TOKEN2ID(tANDOP);
      TOKEN2ID(tOROP);
      TOKEN2ID(tMATCH);
      TOKEN2ID(tNMATCH);
      TOKEN2ID(tDOT2);
      TOKEN2ID(tDOT3);
      TOKEN2ID(tBDOT2);
      TOKEN2ID(tBDOT3);
      TOKEN2ID(tAREF);
      TOKEN2ID(tASET);
      TOKEN2ID(tLSHFT);
      TOKEN2ID(tRSHFT);
      TOKEN2ID(tANDDOT);
      TOKEN2ID(tCOLON2);
      TOKEN2ID(tCOLON3);
      TOKEN2ID(tOP_ASGN);
      TOKEN2ID(tASSOC);
      TOKEN2ID(tLPAREN);
      TOKEN2ID(tLPAREN_ARG);
      TOKEN2ID(tRPAREN);
      TOKEN2ID(tLBRACK);
      TOKEN2ID(tLBRACE);
      TOKEN2ID(tLBRACE_ARG);
      TOKEN2ID(tSTAR);
      TOKEN2ID(tDSTAR);
      TOKEN2ID(tAMPER);
      TOKEN2ID(tLAMBDA);
      TOKEN2ID(tSYMBEG);
      TOKEN2ID(tSTRING_BEG);
      TOKEN2ID(tXSTRING_BEG);
      TOKEN2ID(tREGEXP_BEG);
      TOKEN2ID(tWORDS_BEG);
      TOKEN2ID(tQWORDS_BEG);
      TOKEN2ID(tSYMBOLS_BEG);
      TOKEN2ID(tQSYMBOLS_BEG);
      TOKEN2ID(tSTRING_END);
      TOKEN2ID(tSTRING_DEND);
      TOKEN2ID(tSTRING_DBEG);
      TOKEN2ID(tSTRING_DVAR);
      TOKEN2ID(tLAMBEG);
      TOKEN2ID(tLABEL_END);
      TOKEN2ID(tIGNORED_NL);
      TOKEN2ID(tCOMMENT);
      TOKEN2ID(tEMBDOC_BEG);
      TOKEN2ID(tEMBDOC);
      TOKEN2ID(tEMBDOC_END);
      TOKEN2ID(tHEREDOC_BEG);
      TOKEN2ID(tHEREDOC_END);
      TOKEN2ID(k__END__);
      TOKEN2ID(tLOWEST);
      TOKEN2ID(tUMINUS_NUM);
      TOKEN2ID(tLAST_TOKEN);
#undef TOKEN2ID
#undef TOKEN2ID2
    }

    rb_bug("parser_token2id: unknown token %d", tok);

    UNREACHABLE_RETURN(0);
}

#endif

RBIMPL_ATTR_NONNULL((1, 2, 3))
static int parser_yyerror(struct parser_params*, const YYLTYPE *yylloc, const char*);
RBIMPL_ATTR_NONNULL((1, 2))
static int parser_yyerror0(struct parser_params*, const char*);
#define yyerror0(msg) parser_yyerror0(p, (msg))
#define yyerror1(loc, msg) parser_yyerror(p, (loc), (msg))
#define yyerror(yylloc, p, msg) parser_yyerror(p, yylloc, msg)
#define token_flush(ptr) ((ptr)->lex.ptok = (ptr)->lex.pcur)
#define lex_goto_eol(p) ((p)->lex.pcur = (p)->lex.pend)
#define lex_eol_p(p) lex_eol_n_p(p, 0)
#define lex_eol_n_p(p,n) lex_eol_ptr_n_p(p, (p)->lex.pcur, n)
#define lex_eol_ptr_p(p,ptr) lex_eol_ptr_n_p(p,ptr,0)
#define lex_eol_ptr_n_p(p,ptr,n) ((ptr)+(n) >= (p)->lex.pend)

static void token_info_setup(token_info *ptinfo, const char *ptr, const rb_code_location_t *loc);
static void token_info_push(struct parser_params*, const char *token, const rb_code_location_t *loc);
static void token_info_pop(struct parser_params*, const char *token, const rb_code_location_t *loc);
static void token_info_warn(struct parser_params *p, const char *token, token_info *ptinfo_beg, int same, const rb_code_location_t *loc);
static void token_info_drop(struct parser_params *p, const char *token, rb_code_position_t beg_pos);

#ifdef RIPPER
#define compile_for_eval	(0)
#else
#define compile_for_eval	(p->parent_iseq != 0)
#endif

#define token_column		((int)(p->lex.ptok - p->lex.pbeg))

#define CALL_Q_P(q) ((q) == TOKEN2VAL(tANDDOT))
#define NEW_QCALL(q,r,m,a,loc) (CALL_Q_P(q) ? NEW_QCALL0(r,m,a,loc) : NEW_CALL(r,m,a,loc))

#define lambda_beginning_p() (p->lex.lpar_beg == p->lex.paren_nest)

static enum yytokentype yylex(YYSTYPE*, YYLTYPE*, struct parser_params*);

#ifndef RIPPER
static inline void
rb_discard_node(struct parser_params *p, NODE *n)
{
    rb_ast_delete_node(p->ast, n);
}
#endif

#ifdef RIPPER
static inline VALUE
add_mark_object(struct parser_params *p, VALUE obj)
{
    if (!SPECIAL_CONST_P(obj)
        && !RB_TYPE_P(obj, T_NODE) /* Ripper jumbles NODE objects and other objects... */
    ) {
        rb_ast_add_mark_object(p->ast, obj);
    }
    return obj;
}

static rb_node_ripper_t *rb_node_ripper_new(struct parser_params *p, ID a, VALUE b, VALUE c, const YYLTYPE *loc);
static rb_node_ripper_values_t *rb_node_ripper_values_new(struct parser_params *p, VALUE a, VALUE b, VALUE c, const YYLTYPE *loc);
#define NEW_RIPPER(a,b,c,loc) (VALUE)rb_node_ripper_new(p,a,b,c,loc)
#define NEW_RIPPER_VALUES(a,b,c,loc) (VALUE)rb_node_ripper_values_new(p,a,b,c,loc)

#else
static rb_node_scope_t *rb_node_scope_new(struct parser_params *p, rb_node_args_t *nd_args, NODE *nd_body, const YYLTYPE *loc);
static rb_node_scope_t *rb_node_scope_new2(struct parser_params *p, rb_ast_id_table_t *nd_tbl, rb_node_args_t *nd_args, NODE *nd_body, const YYLTYPE *loc);
static rb_node_block_t *rb_node_block_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc);
static rb_node_if_t *rb_node_if_new(struct parser_params *p, NODE *nd_cond, NODE *nd_body, NODE *nd_else, const YYLTYPE *loc);
static rb_node_unless_t *rb_node_unless_new(struct parser_params *p, NODE *nd_cond, NODE *nd_body, NODE *nd_else, const YYLTYPE *loc);
static rb_node_case_t *rb_node_case_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, const YYLTYPE *loc);
static rb_node_case2_t *rb_node_case2_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc);
static rb_node_case3_t *rb_node_case3_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, const YYLTYPE *loc);
static rb_node_when_t *rb_node_when_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, NODE *nd_next, const YYLTYPE *loc);
static rb_node_in_t *rb_node_in_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, NODE *nd_next, const YYLTYPE *loc);
static rb_node_while_t *rb_node_while_new(struct parser_params *p, NODE *nd_cond, NODE *nd_body, long nd_state, const YYLTYPE *loc);
static rb_node_until_t *rb_node_until_new(struct parser_params *p, NODE *nd_cond, NODE *nd_body, long nd_state, const YYLTYPE *loc);
static rb_node_iter_t *rb_node_iter_new(struct parser_params *p, rb_node_args_t *nd_args, NODE *nd_body, const YYLTYPE *loc);
static rb_node_for_t *rb_node_for_new(struct parser_params *p, NODE *nd_iter, NODE *nd_body, const YYLTYPE *loc);
static rb_node_for_masgn_t *rb_node_for_masgn_new(struct parser_params *p, NODE *nd_var, const YYLTYPE *loc);
static rb_node_retry_t *rb_node_retry_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_begin_t *rb_node_begin_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc);
static rb_node_rescue_t *rb_node_rescue_new(struct parser_params *p, NODE *nd_head, NODE *nd_resq, NODE *nd_else, const YYLTYPE *loc);
static rb_node_resbody_t *rb_node_resbody_new(struct parser_params *p, NODE *nd_args, NODE *nd_body, NODE *nd_head, const YYLTYPE *loc);
static rb_node_ensure_t *rb_node_ensure_new(struct parser_params *p, NODE *nd_head, NODE *nd_ensr, const YYLTYPE *loc);
static rb_node_and_t *rb_node_and_new(struct parser_params *p, NODE *nd_1st, NODE *nd_2nd, const YYLTYPE *loc);
static rb_node_or_t *rb_node_or_new(struct parser_params *p, NODE *nd_1st, NODE *nd_2nd, const YYLTYPE *loc);
static rb_node_masgn_t *rb_node_masgn_new(struct parser_params *p, NODE *nd_head, NODE *nd_args, const YYLTYPE *loc);
static rb_node_lasgn_t *rb_node_lasgn_new(struct parser_params *p, ID nd_vid, NODE *nd_value, const YYLTYPE *loc);
static rb_node_dasgn_t *rb_node_dasgn_new(struct parser_params *p, ID nd_vid, NODE *nd_value, const YYLTYPE *loc);
static rb_node_gasgn_t *rb_node_gasgn_new(struct parser_params *p, ID nd_vid, NODE *nd_value, const YYLTYPE *loc);
static rb_node_iasgn_t *rb_node_iasgn_new(struct parser_params *p, ID nd_vid, NODE *nd_value, const YYLTYPE *loc);
static rb_node_cdecl_t *rb_node_cdecl_new(struct parser_params *p, ID nd_vid, NODE *nd_value, NODE *nd_else, const YYLTYPE *loc);
static rb_node_cvasgn_t *rb_node_cvasgn_new(struct parser_params *p, ID nd_vid, NODE *nd_value, const YYLTYPE *loc);
static rb_node_op_asgn1_t *rb_node_op_asgn1_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *index, NODE *rvalue, const YYLTYPE *loc);
static rb_node_op_asgn2_t *rb_node_op_asgn2_new(struct parser_params *p, NODE *nd_recv, NODE *nd_value, ID nd_vid, ID nd_mid, bool nd_aid, const YYLTYPE *loc);
static rb_node_op_asgn_or_t *rb_node_op_asgn_or_new(struct parser_params *p, NODE *nd_head, NODE *nd_value, const YYLTYPE *loc);
static rb_node_op_asgn_and_t *rb_node_op_asgn_and_new(struct parser_params *p, NODE *nd_head, NODE *nd_value, const YYLTYPE *loc);
static rb_node_op_cdecl_t *rb_node_op_cdecl_new(struct parser_params *p, NODE *nd_head, NODE *nd_value, ID nd_aid, const YYLTYPE *loc);
static rb_node_call_t *rb_node_call_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *nd_args, const YYLTYPE *loc);
static rb_node_opcall_t *rb_node_opcall_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *nd_args, const YYLTYPE *loc);
static rb_node_fcall_t *rb_node_fcall_new(struct parser_params *p, ID nd_mid, NODE *nd_args, const YYLTYPE *loc);
static rb_node_vcall_t *rb_node_vcall_new(struct parser_params *p, ID nd_mid, const YYLTYPE *loc);
static rb_node_qcall_t *rb_node_qcall_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *nd_args, const YYLTYPE *loc);
static rb_node_super_t *rb_node_super_new(struct parser_params *p, NODE *nd_args, const YYLTYPE *loc);
static rb_node_zsuper_t * rb_node_zsuper_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_list_t *rb_node_list_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc);
static rb_node_list_t *rb_node_list_new2(struct parser_params *p, NODE *nd_head, long nd_alen, NODE *nd_next, const YYLTYPE *loc);
static rb_node_zlist_t *rb_node_zlist_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_hash_t *rb_node_hash_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc);
static rb_node_return_t *rb_node_return_new(struct parser_params *p, NODE *nd_stts, const YYLTYPE *loc);
static rb_node_yield_t *rb_node_yield_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc);
static rb_node_lvar_t *rb_node_lvar_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc);
static rb_node_dvar_t *rb_node_dvar_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc);
static rb_node_gvar_t *rb_node_gvar_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc);
static rb_node_ivar_t *rb_node_ivar_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc);
static rb_node_const_t *rb_node_const_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc);
static rb_node_cvar_t *rb_node_cvar_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc);
static rb_node_nth_ref_t *rb_node_nth_ref_new(struct parser_params *p, long nd_nth, const YYLTYPE *loc);
static rb_node_back_ref_t *rb_node_back_ref_new(struct parser_params *p, long nd_nth, const YYLTYPE *loc);
static rb_node_match2_t *rb_node_match2_new(struct parser_params *p, NODE *nd_recv, NODE *nd_value, const YYLTYPE *loc);
static rb_node_match3_t *rb_node_match3_new(struct parser_params *p, NODE *nd_recv, NODE *nd_value, const YYLTYPE *loc);
static rb_node_lit_t *rb_node_lit_new(struct parser_params *p, VALUE nd_lit, const YYLTYPE *loc);
static rb_node_str_t *rb_node_str_new(struct parser_params *p, VALUE nd_lit, const YYLTYPE *loc);
static rb_node_dstr_t *rb_node_dstr_new0(struct parser_params *p, VALUE nd_lit, long nd_alen, NODE *nd_next, const YYLTYPE *loc);
static rb_node_dstr_t *rb_node_dstr_new(struct parser_params *p, VALUE nd_lit, const YYLTYPE *loc);
static rb_node_xstr_t *rb_node_xstr_new(struct parser_params *p, VALUE nd_lit, const YYLTYPE *loc);
static rb_node_dxstr_t *rb_node_dxstr_new(struct parser_params *p, VALUE nd_lit, long nd_alen, NODE *nd_next, const YYLTYPE *loc);
static rb_node_evstr_t *rb_node_evstr_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc);
static rb_node_once_t *rb_node_once_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc);
static rb_node_args_t *rb_node_args_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_args_aux_t *rb_node_args_aux_new(struct parser_params *p, ID nd_pid, long nd_plen, const YYLTYPE *loc);
static rb_node_opt_arg_t *rb_node_opt_arg_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc);
static rb_node_kw_arg_t *rb_node_kw_arg_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc);
static rb_node_postarg_t *rb_node_postarg_new(struct parser_params *p, NODE *nd_1st, NODE *nd_2nd, const YYLTYPE *loc);
static rb_node_argscat_t *rb_node_argscat_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, const YYLTYPE *loc);
static rb_node_argspush_t *rb_node_argspush_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, const YYLTYPE *loc);
static rb_node_splat_t *rb_node_splat_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc);
static rb_node_block_pass_t *rb_node_block_pass_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc);
static rb_node_defn_t *rb_node_defn_new(struct parser_params *p, ID nd_mid, NODE *nd_defn, const YYLTYPE *loc);
static rb_node_defs_t *rb_node_defs_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *nd_defn, const YYLTYPE *loc);
static rb_node_alias_t *rb_node_alias_new(struct parser_params *p, NODE *nd_1st, NODE *nd_2nd, const YYLTYPE *loc);
static rb_node_valias_t *rb_node_valias_new(struct parser_params *p, ID nd_alias, ID nd_orig, const YYLTYPE *loc);
static rb_node_undef_t *rb_node_undef_new(struct parser_params *p, NODE *nd_undef, const YYLTYPE *loc);
static rb_node_class_t *rb_node_class_new(struct parser_params *p, NODE *nd_cpath, NODE *nd_body, NODE *nd_super, const YYLTYPE *loc);
static rb_node_module_t *rb_node_module_new(struct parser_params *p, NODE *nd_cpath, NODE *nd_body, const YYLTYPE *loc);
static rb_node_sclass_t *rb_node_sclass_new(struct parser_params *p, NODE *nd_recv, NODE *nd_body, const YYLTYPE *loc);
static rb_node_colon2_t *rb_node_colon2_new(struct parser_params *p, NODE *nd_head, ID nd_mid, const YYLTYPE *loc);
static rb_node_colon3_t *rb_node_colon3_new(struct parser_params *p, ID nd_mid, const YYLTYPE *loc);
static rb_node_dot2_t *rb_node_dot2_new(struct parser_params *p, NODE *nd_beg, NODE *nd_end, const YYLTYPE *loc);
static rb_node_dot3_t *rb_node_dot3_new(struct parser_params *p, NODE *nd_beg, NODE *nd_end, const YYLTYPE *loc);
static rb_node_self_t *rb_node_self_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_nil_t *rb_node_nil_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_true_t *rb_node_true_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_false_t *rb_node_false_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_errinfo_t *rb_node_errinfo_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_defined_t *rb_node_defined_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc);
static rb_node_postexe_t *rb_node_postexe_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc);
static rb_node_dsym_t *rb_node_dsym_new(struct parser_params *p, VALUE nd_lit, long nd_alen, NODE *nd_next, const YYLTYPE *loc);
static rb_node_attrasgn_t *rb_node_attrasgn_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *nd_args, const YYLTYPE *loc);
static rb_node_lambda_t *rb_node_lambda_new(struct parser_params *p, rb_node_args_t *nd_args, NODE *nd_body, const YYLTYPE *loc);
static rb_node_aryptn_t *rb_node_aryptn_new(struct parser_params *p, NODE *pre_args, NODE *rest_arg, NODE *post_args, const YYLTYPE *loc);
static rb_node_hshptn_t *rb_node_hshptn_new(struct parser_params *p, NODE *nd_pconst, NODE *nd_pkwargs, NODE *nd_pkwrestarg, const YYLTYPE *loc);
static rb_node_fndptn_t *rb_node_fndptn_new(struct parser_params *p, NODE *pre_rest_arg, NODE *args, NODE *post_rest_arg, const YYLTYPE *loc);
static rb_node_error_t *rb_node_error_new(struct parser_params *p, const YYLTYPE *loc);

#define NEW_SCOPE(a,b,loc) (NODE *)rb_node_scope_new(p,a,b,loc)
#define NEW_SCOPE2(t,a,b,loc) (NODE *)rb_node_scope_new2(p,t,a,b,loc)
#define NEW_BLOCK(a,loc) (NODE *)rb_node_block_new(p,a,loc)
#define NEW_IF(c,t,e,loc) (NODE *)rb_node_if_new(p,c,t,e,loc)
#define NEW_UNLESS(c,t,e,loc) (NODE *)rb_node_unless_new(p,c,t,e,loc)
#define NEW_CASE(h,b,loc) (NODE *)rb_node_case_new(p,h,b,loc)
#define NEW_CASE2(b,loc) (NODE *)rb_node_case2_new(p,b,loc)
#define NEW_CASE3(h,b,loc) (NODE *)rb_node_case3_new(p,h,b,loc)
#define NEW_WHEN(c,t,e,loc) (NODE *)rb_node_when_new(p,c,t,e,loc)
#define NEW_IN(c,t,e,loc) (NODE *)rb_node_in_new(p,c,t,e,loc)
#define NEW_WHILE(c,b,n,loc) (NODE *)rb_node_while_new(p,c,b,n,loc)
#define NEW_UNTIL(c,b,n,loc) (NODE *)rb_node_until_new(p,c,b,n,loc)
#define NEW_ITER(a,b,loc) (NODE *)rb_node_iter_new(p,a,b,loc)
#define NEW_FOR(i,b,loc) (NODE *)rb_node_for_new(p,i,b,loc)
#define NEW_FOR_MASGN(v,loc) (NODE *)rb_node_for_masgn_new(p,v,loc)
#define NEW_RETRY(loc) (NODE *)rb_node_retry_new(p,loc)
#define NEW_BEGIN(b,loc) (NODE *)rb_node_begin_new(p,b,loc)
#define NEW_RESCUE(b,res,e,loc) (NODE *)rb_node_rescue_new(p,b,res,e,loc)
#define NEW_RESBODY(a,ex,n,loc) (NODE *)rb_node_resbody_new(p,a,ex,n,loc)
#define NEW_ENSURE(b,en,loc) (NODE *)rb_node_ensure_new(p,b,en,loc)
#define NEW_AND(f,s,loc) (NODE *)rb_node_and_new(p,f,s,loc)
#define NEW_OR(f,s,loc) (NODE *)rb_node_or_new(p,f,s,loc)
#define NEW_MASGN(l,r,loc)   rb_node_masgn_new(p,l,r,loc)
#define NEW_LASGN(v,val,loc) (NODE *)rb_node_lasgn_new(p,v,val,loc)
#define NEW_DASGN(v,val,loc) (NODE *)rb_node_dasgn_new(p,v,val,loc)
#define NEW_GASGN(v,val,loc) (NODE *)rb_node_gasgn_new(p,v,val,loc)
#define NEW_IASGN(v,val,loc) (NODE *)rb_node_iasgn_new(p,v,val,loc)
#define NEW_CDECL(v,val,path,loc) (NODE *)rb_node_cdecl_new(p,v,val,path,loc)
#define NEW_CVASGN(v,val,loc) (NODE *)rb_node_cvasgn_new(p,v,val,loc)
#define NEW_OP_ASGN1(r,id,idx,rval,loc) (NODE *)rb_node_op_asgn1_new(p,r,id,idx,rval,loc)
#define NEW_OP_ASGN2(r,t,i,o,val,loc) (NODE *)rb_node_op_asgn2_new(p,r,val,i,o,t,loc)
#define NEW_OP_ASGN_OR(i,val,loc) (NODE *)rb_node_op_asgn_or_new(p,i,val,loc)
#define NEW_OP_ASGN_AND(i,val,loc) (NODE *)rb_node_op_asgn_and_new(p,i,val,loc)
#define NEW_OP_CDECL(v,op,val,loc) (NODE *)rb_node_op_cdecl_new(p,v,val,op,loc)
#define NEW_CALL(r,m,a,loc) (NODE *)rb_node_call_new(p,r,m,a,loc)
#define NEW_OPCALL(r,m,a,loc) (NODE *)rb_node_opcall_new(p,r,m,a,loc)
#define NEW_FCALL(m,a,loc) rb_node_fcall_new(p,m,a,loc)
#define NEW_VCALL(m,loc) (NODE *)rb_node_vcall_new(p,m,loc)
#define NEW_QCALL0(r,m,a,loc) (NODE *)rb_node_qcall_new(p,r,m,a,loc)
#define NEW_SUPER(a,loc) (NODE *)rb_node_super_new(p,a,loc)
#define NEW_ZSUPER(loc) (NODE *)rb_node_zsuper_new(p,loc)
#define NEW_LIST(a,loc) (NODE *)rb_node_list_new(p,a,loc)
#define NEW_LIST2(h,l,n,loc) (NODE *)rb_node_list_new2(p,h,l,n,loc)
#define NEW_ZLIST(loc) (NODE *)rb_node_zlist_new(p,loc)
#define NEW_HASH(a,loc) (NODE *)rb_node_hash_new(p,a,loc)
#define NEW_RETURN(s,loc) (NODE *)rb_node_return_new(p,s,loc)
#define NEW_YIELD(a,loc) (NODE *)rb_node_yield_new(p,a,loc)
#define NEW_LVAR(v,loc) (NODE *)rb_node_lvar_new(p,v,loc)
#define NEW_DVAR(v,loc) (NODE *)rb_node_dvar_new(p,v,loc)
#define NEW_GVAR(v,loc) (NODE *)rb_node_gvar_new(p,v,loc)
#define NEW_IVAR(v,loc) (NODE *)rb_node_ivar_new(p,v,loc)
#define NEW_CONST(v,loc) (NODE *)rb_node_const_new(p,v,loc)
#define NEW_CVAR(v,loc) (NODE *)rb_node_cvar_new(p,v,loc)
#define NEW_NTH_REF(n,loc)  (NODE *)rb_node_nth_ref_new(p,n,loc)
#define NEW_BACK_REF(n,loc) (NODE *)rb_node_back_ref_new(p,n,loc)
#define NEW_MATCH2(n1,n2,loc) (NODE *)rb_node_match2_new(p,n1,n2,loc)
#define NEW_MATCH3(r,n2,loc) (NODE *)rb_node_match3_new(p,r,n2,loc)
#define NEW_LIT(l,loc) (NODE *)rb_node_lit_new(p,l,loc)
#define NEW_STR(s,loc) (NODE *)rb_node_str_new(p,s,loc)
#define NEW_DSTR0(s,l,n,loc) (NODE *)rb_node_dstr_new0(p,s,l,n,loc)
#define NEW_DSTR(s,loc) (NODE *)rb_node_dstr_new(p,s,loc)
#define NEW_XSTR(s,loc) (NODE *)rb_node_xstr_new(p,s,loc)
#define NEW_DXSTR(s,l,n,loc) (NODE *)rb_node_dxstr_new(p,s,l,n,loc)
#define NEW_EVSTR(n,loc) (NODE *)rb_node_evstr_new(p,n,loc)
#define NEW_ONCE(b,loc) (NODE *)rb_node_once_new(p,b,loc)
#define NEW_ARGS(loc) rb_node_args_new(p,loc)
#define NEW_ARGS_AUX(r,b,loc) rb_node_args_aux_new(p,r,b,loc)
#define NEW_OPT_ARG(v,loc) rb_node_opt_arg_new(p,v,loc)
#define NEW_KW_ARG(v,loc) rb_node_kw_arg_new(p,v,loc)
#define NEW_POSTARG(i,v,loc) (NODE *)rb_node_postarg_new(p,i,v,loc)
#define NEW_ARGSCAT(a,b,loc) (NODE *)rb_node_argscat_new(p,a,b,loc)
#define NEW_ARGSPUSH(a,b,loc) (NODE *)rb_node_argspush_new(p,a,b,loc)
#define NEW_SPLAT(a,loc) (NODE *)rb_node_splat_new(p,a,loc)
#define NEW_BLOCK_PASS(b,loc) rb_node_block_pass_new(p,b,loc)
#define NEW_DEFN(i,s,loc) (NODE *)rb_node_defn_new(p,i,s,loc)
#define NEW_DEFS(r,i,s,loc) (NODE *)rb_node_defs_new(p,r,i,s,loc)
#define NEW_ALIAS(n,o,loc) (NODE *)rb_node_alias_new(p,n,o,loc)
#define NEW_VALIAS(n,o,loc) (NODE *)rb_node_valias_new(p,n,o,loc)
#define NEW_UNDEF(i,loc) (NODE *)rb_node_undef_new(p,i,loc)
#define NEW_CLASS(n,b,s,loc) (NODE *)rb_node_class_new(p,n,b,s,loc)
#define NEW_MODULE(n,b,loc) (NODE *)rb_node_module_new(p,n,b,loc)
#define NEW_SCLASS(r,b,loc) (NODE *)rb_node_sclass_new(p,r,b,loc)
#define NEW_COLON2(c,i,loc) (NODE *)rb_node_colon2_new(p,c,i,loc)
#define NEW_COLON3(i,loc) (NODE *)rb_node_colon3_new(p,i,loc)
#define NEW_DOT2(b,e,loc) (NODE *)rb_node_dot2_new(p,b,e,loc)
#define NEW_DOT3(b,e,loc) (NODE *)rb_node_dot3_new(p,b,e,loc)
#define NEW_SELF(loc) (NODE *)rb_node_self_new(p,loc)
#define NEW_NIL(loc) (NODE *)rb_node_nil_new(p,loc)
#define NEW_TRUE(loc) (NODE *)rb_node_true_new(p,loc)
#define NEW_FALSE(loc) (NODE *)rb_node_false_new(p,loc)
#define NEW_ERRINFO(loc) (NODE *)rb_node_errinfo_new(p,loc)
#define NEW_DEFINED(e,loc) (NODE *)rb_node_defined_new(p,e,loc)
#define NEW_POSTEXE(b,loc) (NODE *)rb_node_postexe_new(p,b,loc)
#define NEW_DSYM(s,l,n,loc) (NODE *)rb_node_dsym_new(p,s,l,n,loc)
#define NEW_ATTRASGN(r,m,a,loc) (NODE *)rb_node_attrasgn_new(p,r,m,a,loc)
#define NEW_LAMBDA(a,b,loc) (NODE *)rb_node_lambda_new(p,a,b,loc)
#define NEW_ARYPTN(pre,r,post,loc) (NODE *)rb_node_aryptn_new(p,pre,r,post,loc)
#define NEW_HSHPTN(c,kw,kwrest,loc) (NODE *)rb_node_hshptn_new(p,c,kw,kwrest,loc)
#define NEW_FNDPTN(pre,a,post,loc) (NODE *)rb_node_fndptn_new(p,pre,a,post,loc)
#define NEW_ERROR(loc) (NODE *)rb_node_error_new(p,loc)

#endif

enum internal_node_type {
    NODE_INTERNAL_ONLY = NODE_LAST,
    NODE_DEF_TEMP,
    NODE_EXITS,
    NODE_INTERNAL_LAST
};

static const char *
parser_node_name(int node)
{
    switch (node) {
      case NODE_DEF_TEMP:
        return "NODE_DEF_TEMP";
      case NODE_EXITS:
        return "NODE_EXITS";
      default:
        return ruby_node_name(node);
    }
}

/* This node is parse.y internal */
struct RNode_DEF_TEMP {
    NODE node;

    /* for NODE_DEFN/NODE_DEFS */
#ifndef RIPPER
    struct RNode *nd_def;
    ID nd_mid;
#else
    VALUE nd_recv;
    VALUE nd_mid;
    VALUE dot_or_colon;
#endif

    struct {
        ID cur_arg;
        int max_numparam;
        NODE *numparam_save;
        struct lex_context ctxt;
    } save;
};

#define RNODE_DEF_TEMP(node) ((struct RNode_DEF_TEMP *)(node))

static rb_node_break_t *rb_node_break_new(struct parser_params *p, NODE *nd_stts, const YYLTYPE *loc);
static rb_node_next_t *rb_node_next_new(struct parser_params *p, NODE *nd_stts, const YYLTYPE *loc);
static rb_node_redo_t *rb_node_redo_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_def_temp_t *rb_node_def_temp_new(struct parser_params *p, const YYLTYPE *loc);
static rb_node_def_temp_t *def_head_save(struct parser_params *p, rb_node_def_temp_t *n);

#define NEW_BREAK(s,loc) (NODE *)rb_node_break_new(p,s,loc)
#define NEW_NEXT(s,loc) (NODE *)rb_node_next_new(p,s,loc)
#define NEW_REDO(loc) (NODE *)rb_node_redo_new(p,loc)
#define NEW_DEF_TEMP(loc) rb_node_def_temp_new(p,loc)

/* Make a new internal node, which should not be appeared in the
 * result AST and does not have node_id and location. */
static NODE* node_new_internal(struct parser_params *p, enum node_type type, size_t size, size_t alignment);
#define NODE_NEW_INTERNAL(ndtype, type) (type *)node_new_internal(p, (enum node_type)(ndtype), sizeof(type), RUBY_ALIGNOF(type))

static NODE *nd_set_loc(NODE *nd, const YYLTYPE *loc);

static int
parser_get_node_id(struct parser_params *p)
{
    int node_id = p->node_id;
    p->node_id++;
    return node_id;
}

static void
anddot_multiple_assignment_check(struct parser_params* p, const YYLTYPE *loc, ID id)
{
    if (id == tANDDOT) {
	yyerror1(loc, "&. inside multiple assignment destination");
    }
}

#ifndef RIPPER
static inline void
set_line_body(NODE *body, int line)
{
    if (!body) return;
    switch (nd_type(body)) {
      case NODE_RESCUE:
      case NODE_ENSURE:
        nd_set_line(body, line);
    }
}

static void
set_embraced_location(NODE *node, const rb_code_location_t *beg, const rb_code_location_t *end)
{
    RNODE_ITER(node)->nd_body->nd_loc = code_loc_gen(beg, end);
    nd_set_line(node, beg->end_pos.lineno);
}

static NODE *
last_expr_node(NODE *expr)
{
    while (expr) {
        if (nd_type_p(expr, NODE_BLOCK)) {
            expr = RNODE_BLOCK(RNODE_BLOCK(expr)->nd_end)->nd_head;
        }
        else if (nd_type_p(expr, NODE_BEGIN)) {
            expr = RNODE_BEGIN(expr)->nd_body;
        }
        else {
            break;
        }
    }
    return expr;
}

#define yyparse ruby_yyparse

static NODE* cond(struct parser_params *p, NODE *node, const YYLTYPE *loc);
static NODE* method_cond(struct parser_params *p, NODE *node, const YYLTYPE *loc);
#define new_nil(loc) NEW_NIL(loc)
static NODE *new_nil_at(struct parser_params *p, const rb_code_position_t *pos);
static NODE *new_if(struct parser_params*,NODE*,NODE*,NODE*,const YYLTYPE*);
static NODE *new_unless(struct parser_params*,NODE*,NODE*,NODE*,const YYLTYPE*);
static NODE *logop(struct parser_params*,ID,NODE*,NODE*,const YYLTYPE*,const YYLTYPE*);

static NODE *newline_node(NODE*);
static void fixpos(NODE*,NODE*);

static int value_expr_gen(struct parser_params*,NODE*);
static void void_expr(struct parser_params*,NODE*);
static NODE *remove_begin(NODE*);
#define value_expr(node) value_expr_gen(p, (node))
static NODE *void_stmts(struct parser_params*,NODE*);
static void reduce_nodes(struct parser_params*,NODE**);
static void block_dup_check(struct parser_params*,NODE*,NODE*);

static NODE *block_append(struct parser_params*,NODE*,NODE*);
static NODE *list_append(struct parser_params*,NODE*,NODE*);
static NODE *list_concat(NODE*,NODE*);
static NODE *arg_append(struct parser_params*,NODE*,NODE*,const YYLTYPE*);
static NODE *last_arg_append(struct parser_params *p, NODE *args, NODE *last_arg, const YYLTYPE *loc);
static NODE *rest_arg_append(struct parser_params *p, NODE *args, NODE *rest_arg, const YYLTYPE *loc);
static NODE *literal_concat(struct parser_params*,NODE*,NODE*,const YYLTYPE*);
static NODE *new_evstr(struct parser_params*,NODE*,const YYLTYPE*);
static NODE *new_dstr(struct parser_params*,NODE*,const YYLTYPE*);
static NODE *str2dstr(struct parser_params*,NODE*);
static NODE *evstr2dstr(struct parser_params*,NODE*);
static NODE *splat_array(NODE*);
static void mark_lvar_used(struct parser_params *p, NODE *rhs);

static NODE *call_bin_op(struct parser_params*,NODE*,ID,NODE*,const YYLTYPE*,const YYLTYPE*);
static NODE *call_uni_op(struct parser_params*,NODE*,ID,const YYLTYPE*,const YYLTYPE*);
static NODE *new_qcall(struct parser_params* p, ID atype, NODE *recv, ID mid, NODE *args, const YYLTYPE *op_loc, const YYLTYPE *loc);
static NODE *new_command_qcall(struct parser_params* p, ID atype, NODE *recv, ID mid, NODE *args, NODE *block, const YYLTYPE *op_loc, const YYLTYPE *loc);
static NODE *method_add_block(struct parser_params*p, NODE *m, NODE *b, const YYLTYPE *loc) {RNODE_ITER(b)->nd_iter = m; b->nd_loc = *loc; return b;}

static bool args_info_empty_p(struct rb_args_info *args);
static rb_node_args_t *new_args(struct parser_params*,rb_node_args_aux_t*,rb_node_opt_arg_t*,ID,rb_node_args_aux_t*,rb_node_args_t*,const YYLTYPE*);
static rb_node_args_t *new_args_tail(struct parser_params*,rb_node_kw_arg_t*,ID,ID,const YYLTYPE*);
static NODE *new_array_pattern(struct parser_params *p, NODE *constant, NODE *pre_arg, NODE *aryptn, const YYLTYPE *loc);
static NODE *new_array_pattern_tail(struct parser_params *p, NODE *pre_args, int has_rest, NODE *rest_arg, NODE *post_args, const YYLTYPE *loc);
static NODE *new_find_pattern(struct parser_params *p, NODE *constant, NODE *fndptn, const YYLTYPE *loc);
static NODE *new_find_pattern_tail(struct parser_params *p, NODE *pre_rest_arg, NODE *args, NODE *post_rest_arg, const YYLTYPE *loc);
static NODE *new_hash_pattern(struct parser_params *p, NODE *constant, NODE *hshptn, const YYLTYPE *loc);
static NODE *new_hash_pattern_tail(struct parser_params *p, NODE *kw_args, ID kw_rest_arg, const YYLTYPE *loc);

static rb_node_kw_arg_t *new_kw_arg(struct parser_params *p, NODE *k, const YYLTYPE *loc);
static rb_node_args_t *args_with_numbered(struct parser_params*,rb_node_args_t*,int);

static VALUE negate_lit(struct parser_params*, VALUE);
static NODE *ret_args(struct parser_params*,NODE*);
static NODE *arg_blk_pass(NODE*,rb_node_block_pass_t*);
static NODE *new_yield(struct parser_params*,NODE*,const YYLTYPE*);
static NODE *dsym_node(struct parser_params*,NODE*,const YYLTYPE*);

static NODE *gettable(struct parser_params*,ID,const YYLTYPE*);
static NODE *assignable(struct parser_params*,ID,NODE*,const YYLTYPE*);

static NODE *aryset(struct parser_params*,NODE*,NODE*,const YYLTYPE*);
static NODE *attrset(struct parser_params*,NODE*,ID,ID,const YYLTYPE*);

static void rb_backref_error(struct parser_params*,NODE*);
static NODE *node_assign(struct parser_params*,NODE*,NODE*,struct lex_context,const YYLTYPE*);

static NODE *new_op_assign(struct parser_params *p, NODE *lhs, ID op, NODE *rhs, struct lex_context, const YYLTYPE *loc);
static NODE *new_ary_op_assign(struct parser_params *p, NODE *ary, NODE *args, ID op, NODE *rhs, const YYLTYPE *args_loc, const YYLTYPE *loc);
static NODE *new_attr_op_assign(struct parser_params *p, NODE *lhs, ID atype, ID attr, ID op, NODE *rhs, const YYLTYPE *loc);
static NODE *new_const_op_assign(struct parser_params *p, NODE *lhs, ID op, NODE *rhs, struct lex_context, const YYLTYPE *loc);
static NODE *new_bodystmt(struct parser_params *p, NODE *head, NODE *rescue, NODE *rescue_else, NODE *ensure, const YYLTYPE *loc);

static NODE *const_decl(struct parser_params *p, NODE* path, const YYLTYPE *loc);

static rb_node_opt_arg_t *opt_arg_append(rb_node_opt_arg_t*, rb_node_opt_arg_t*);
static rb_node_kw_arg_t *kwd_append(rb_node_kw_arg_t*, rb_node_kw_arg_t*);

static NODE *new_hash(struct parser_params *p, NODE *hash, const YYLTYPE *loc);
static NODE *new_unique_key_hash(struct parser_params *p, NODE *hash, const YYLTYPE *loc);

static NODE *new_defined(struct parser_params *p, NODE *expr, const YYLTYPE *loc);

static NODE *new_regexp(struct parser_params *, NODE *, int, const YYLTYPE *);

#define make_list(list, loc) ((list) ? (nd_set_loc(list, loc), list) : NEW_ZLIST(loc))

static NODE *new_xstring(struct parser_params *, NODE *, const YYLTYPE *loc);

static NODE *symbol_append(struct parser_params *p, NODE *symbols, NODE *symbol);

static NODE *match_op(struct parser_params*,NODE*,NODE*,const YYLTYPE*,const YYLTYPE*);

static rb_ast_id_table_t *local_tbl(struct parser_params*);

static VALUE reg_compile(struct parser_params*, VALUE, int);
static void reg_fragment_setenc(struct parser_params*, VALUE, int);
static int reg_fragment_check(struct parser_params*, VALUE, int);

static int literal_concat0(struct parser_params *p, VALUE head, VALUE tail);
static NODE *heredoc_dedent(struct parser_params*,NODE*);

static void check_literal_when(struct parser_params *p, NODE *args, const YYLTYPE *loc);

#define get_id(id) (id)
#define get_value(val) (val)
#define get_num(num) (num)
#else  /* RIPPER */

static inline int ripper_is_node_yylval(struct parser_params *p, VALUE n);

static inline VALUE
ripper_new_yylval(struct parser_params *p, ID a, VALUE b, VALUE c)
{
    if (ripper_is_node_yylval(p, c)) c = RNODE_RIPPER(c)->nd_cval;
    add_mark_object(p, b);
    add_mark_object(p, c);
    return NEW_RIPPER(a, b, c, &NULL_LOC);
}

static inline VALUE
ripper_new_yylval2(struct parser_params *p, VALUE a, VALUE b, VALUE c)
{
    add_mark_object(p, a);
    add_mark_object(p, b);
    add_mark_object(p, c);
    return NEW_RIPPER_VALUES(a, b, c, &NULL_LOC);
}

static inline int
ripper_is_node_yylval(struct parser_params *p, VALUE n)
{
    return RB_TYPE_P(n, T_NODE) && nd_type_p(RNODE(n), NODE_RIPPER);
}

#define value_expr(node) ((void)(node))
#define remove_begin(node) (node)
#define void_stmts(p,x) (x)
#undef rb_dvar_defined
#define rb_dvar_defined(id, base) 0
#undef rb_local_defined
#define rb_local_defined(id, base) 0
#define get_id(id) ripper_get_id(id)
#define get_value(val) ripper_get_value(val)
#define get_num(num) (int)get_id(num)
static VALUE assignable(struct parser_params*,VALUE);
static int id_is_var(struct parser_params *p, ID id);

#define method_cond(p,node,loc) (node)
#define call_bin_op(p, recv,id,arg1,op_loc,loc) dispatch3(binary, (recv), STATIC_ID2SYM(id), (arg1))
#define match_op(p,node1,node2,op_loc,loc) call_bin_op(0, (node1), idEqTilde, (node2), op_loc, loc)
#define call_uni_op(p, recv,id,op_loc,loc) dispatch2(unary, STATIC_ID2SYM(id), (recv))
#define logop(p,id,node1,node2,op_loc,loc) call_bin_op(0, (node1), (id), (node2), op_loc, loc)

#define new_nil(loc) Qnil

static VALUE new_regexp(struct parser_params *, VALUE, VALUE, const YYLTYPE *);

static VALUE const_decl(struct parser_params *p, VALUE path);

static VALUE var_field(struct parser_params *p, VALUE a);
static VALUE assign_error(struct parser_params *p, const char *mesg, VALUE a);

static VALUE parser_reg_compile(struct parser_params*, VALUE, int, VALUE *);

static VALUE backref_error(struct parser_params*, NODE *, VALUE);
#endif /* !RIPPER */

RUBY_SYMBOL_EXPORT_BEGIN
VALUE rb_parser_reg_compile(struct parser_params* p, VALUE str, int options);
int rb_reg_fragment_setenc(struct parser_params*, VALUE, int);
enum lex_state_e rb_parser_trace_lex_state(struct parser_params *, enum lex_state_e, enum lex_state_e, int);
VALUE rb_parser_lex_state_name(struct parser_params *p, enum lex_state_e state);
void rb_parser_show_bitstack(struct parser_params *, stack_type, const char *, int);
PRINTF_ARGS(void rb_parser_fatal(struct parser_params *p, const char *fmt, ...), 2, 3);
YYLTYPE *rb_parser_set_location_from_strterm_heredoc(struct parser_params *p, rb_strterm_heredoc_t *here, YYLTYPE *yylloc);
YYLTYPE *rb_parser_set_location_of_delayed_token(struct parser_params *p, YYLTYPE *yylloc);
YYLTYPE *rb_parser_set_location_of_heredoc_end(struct parser_params *p, YYLTYPE *yylloc);
YYLTYPE *rb_parser_set_location_of_dummy_end(struct parser_params *p, YYLTYPE *yylloc);
YYLTYPE *rb_parser_set_location_of_none(struct parser_params *p, YYLTYPE *yylloc);
YYLTYPE *rb_parser_set_location(struct parser_params *p, YYLTYPE *yylloc);
RUBY_SYMBOL_EXPORT_END

static void error_duplicate_pattern_variable(struct parser_params *p, ID id, const YYLTYPE *loc);
static void error_duplicate_pattern_key(struct parser_params *p, ID id, const YYLTYPE *loc);
#ifndef RIPPER
static ID formal_argument(struct parser_params*, ID);
#else
static ID formal_argument(struct parser_params*, VALUE);
#endif
static ID shadowing_lvar(struct parser_params*,ID);
static void new_bv(struct parser_params*,ID);

static void local_push(struct parser_params*,int);
static void local_pop(struct parser_params*);
static void local_var(struct parser_params*, ID);
static void arg_var(struct parser_params*, ID);
static int  local_id(struct parser_params *p, ID id);
static int  local_id_ref(struct parser_params*, ID, ID **);
#ifndef RIPPER
static ID   internal_id(struct parser_params*);
static NODE *new_args_forward_call(struct parser_params*, NODE*, const YYLTYPE*, const YYLTYPE*);
#endif
static int check_forwarding_args(struct parser_params*);
static void add_forwarding_args(struct parser_params *p);
static void forwarding_arg_check(struct parser_params *p, ID arg, ID all, const char *var);

static const struct vtable *dyna_push(struct parser_params *);
static void dyna_pop(struct parser_params*, const struct vtable *);
static int dyna_in_block(struct parser_params*);
#define dyna_var(p, id) local_var(p, id)
static int dvar_defined(struct parser_params*, ID);
static int dvar_defined_ref(struct parser_params*, ID, ID**);
static int dvar_curr(struct parser_params*,ID);

static int lvar_defined(struct parser_params*, ID);

static NODE *numparam_push(struct parser_params *p);
static void numparam_pop(struct parser_params *p, NODE *prev_inner);

#ifdef RIPPER
# define METHOD_NOT idNOT
#else
# define METHOD_NOT '!'
#endif

#define idFWD_REST   '*'
#define idFWD_KWREST idPow /* Use simple "**", as tDSTAR is "**arg" */
#define idFWD_BLOCK  '&'
#define idFWD_ALL    idDot3
#ifdef RIPPER
#define arg_FWD_BLOCK Qnone
#else
#define arg_FWD_BLOCK idFWD_BLOCK
#endif
#define FORWARD_ARGS_WITH_RUBY2_KEYWORDS

#define RE_OPTION_ONCE (1<<16)
#define RE_OPTION_ENCODING_SHIFT 8
#define RE_OPTION_ENCODING(e) (((e)&0xff)<<RE_OPTION_ENCODING_SHIFT)
#define RE_OPTION_ENCODING_IDX(o) (((o)>>RE_OPTION_ENCODING_SHIFT)&0xff)
#define RE_OPTION_ENCODING_NONE(o) ((o)&RE_OPTION_ARG_ENCODING_NONE)
#define RE_OPTION_MASK  0xff
#define RE_OPTION_ARG_ENCODING_NONE 32

#define yytnamerr(yyres, yystr) (YYSIZE_T)rb_yytnamerr(p, yyres, yystr)
size_t rb_yytnamerr(struct parser_params *p, char *yyres, const char *yystr);

#define TOKEN2ID(tok) ( \
    tTOKEN_LOCAL_BEGIN<(tok)&&(tok)<tTOKEN_LOCAL_END ? TOKEN2LOCALID(tok) : \
    tTOKEN_INSTANCE_BEGIN<(tok)&&(tok)<tTOKEN_INSTANCE_END ? TOKEN2INSTANCEID(tok) : \
    tTOKEN_GLOBAL_BEGIN<(tok)&&(tok)<tTOKEN_GLOBAL_END ? TOKEN2GLOBALID(tok) : \
    tTOKEN_CONST_BEGIN<(tok)&&(tok)<tTOKEN_CONST_END ? TOKEN2CONSTID(tok) : \
    tTOKEN_CLASS_BEGIN<(tok)&&(tok)<tTOKEN_CLASS_END ? TOKEN2CLASSID(tok) : \
    tTOKEN_ATTRSET_BEGIN<(tok)&&(tok)<tTOKEN_ATTRSET_END ? TOKEN2ATTRSETID(tok) : \
    ((tok) / ((tok)<tPRESERVED_ID_END && ((tok)>=128 || rb_ispunct(tok)))))

/****** Ripper *******/

#ifdef RIPPER

#include "eventids1.h"
#include "eventids2.h"

extern const struct ripper_parser_ids ripper_parser_ids;

static VALUE ripper_dispatch0(struct parser_params*,ID);
static VALUE ripper_dispatch1(struct parser_params*,ID,VALUE);
static VALUE ripper_dispatch2(struct parser_params*,ID,VALUE,VALUE);
static VALUE ripper_dispatch3(struct parser_params*,ID,VALUE,VALUE,VALUE);
static VALUE ripper_dispatch4(struct parser_params*,ID,VALUE,VALUE,VALUE,VALUE);
static VALUE ripper_dispatch5(struct parser_params*,ID,VALUE,VALUE,VALUE,VALUE,VALUE);
static VALUE ripper_dispatch7(struct parser_params*,ID,VALUE,VALUE,VALUE,VALUE,VALUE,VALUE,VALUE);
void ripper_error(struct parser_params *p);

#define dispatch0(n)            ripper_dispatch0(p, TOKEN_PASTE(ripper_id_, n))
#define dispatch1(n,a)          ripper_dispatch1(p, TOKEN_PASTE(ripper_id_, n), (a))
#define dispatch2(n,a,b)        ripper_dispatch2(p, TOKEN_PASTE(ripper_id_, n), (a), (b))
#define dispatch3(n,a,b,c)      ripper_dispatch3(p, TOKEN_PASTE(ripper_id_, n), (a), (b), (c))
#define dispatch4(n,a,b,c,d)    ripper_dispatch4(p, TOKEN_PASTE(ripper_id_, n), (a), (b), (c), (d))
#define dispatch5(n,a,b,c,d,e)  ripper_dispatch5(p, TOKEN_PASTE(ripper_id_, n), (a), (b), (c), (d), (e))
#define dispatch7(n,a,b,c,d,e,f,g) ripper_dispatch7(p, TOKEN_PASTE(ripper_id_, n), (a), (b), (c), (d), (e), (f), (g))

#define yyparse ripper_yyparse

#define ID2VAL(id) STATIC_ID2SYM(id)
#define TOKEN2VAL(t) ID2VAL(TOKEN2ID(t))
#define KWD2EID(t, v) ripper_new_yylval(p, keyword_##t, get_value(v), 0)

#define params_new(pars, opts, rest, pars2, kws, kwrest, blk) \
        dispatch7(params, (pars), (opts), (rest), (pars2), (kws), (kwrest), (blk))

static inline VALUE
new_args(struct parser_params *p, VALUE pre_args, VALUE opt_args, VALUE rest_arg, VALUE post_args, VALUE tail, YYLTYPE *loc)
{
    struct RNode_RIPPER_VALUES *t = RNODE_RIPPER_VALUES(tail);
    VALUE kw_args = t->nd_val1, kw_rest_arg = t->nd_val2, block = t->nd_val3;
    return params_new(pre_args, opt_args, rest_arg, post_args, kw_args, kw_rest_arg, block);
}

static inline VALUE
new_args_tail(struct parser_params *p, VALUE kw_args, VALUE kw_rest_arg, VALUE block, YYLTYPE *loc)
{
    return ripper_new_yylval2(p, kw_args, kw_rest_arg, block);
}

static inline VALUE
args_with_numbered(struct parser_params *p, VALUE args, int max_numparam)
{
    return args;
}

static VALUE
new_array_pattern(struct parser_params *p, VALUE constant, VALUE pre_arg, VALUE aryptn, const YYLTYPE *loc)
{
    struct RNode_RIPPER_VALUES *t = RNODE_RIPPER_VALUES(aryptn);
    VALUE pre_args = t->nd_val1, rest_arg = t->nd_val2, post_args = t->nd_val3;

    if (!NIL_P(pre_arg)) {
        if (!NIL_P(pre_args)) {
            rb_ary_unshift(pre_args, pre_arg);
        }
        else {
            pre_args = rb_ary_new_from_args(1, pre_arg);
        }
    }
    return dispatch4(aryptn, constant, pre_args, rest_arg, post_args);
}

static VALUE
new_array_pattern_tail(struct parser_params *p, VALUE pre_args, VALUE has_rest, VALUE rest_arg, VALUE post_args, const YYLTYPE *loc)
{
    return ripper_new_yylval2(p, pre_args, rest_arg, post_args);
}

static VALUE
new_find_pattern(struct parser_params *p, VALUE constant, VALUE fndptn, const YYLTYPE *loc)
{
    struct RNode_RIPPER_VALUES *t = RNODE_RIPPER_VALUES(fndptn);
    VALUE pre_rest_arg = t->nd_val1, args = t->nd_val2, post_rest_arg = t->nd_val3;

    return dispatch4(fndptn, constant, pre_rest_arg, args, post_rest_arg);
}

static VALUE
new_find_pattern_tail(struct parser_params *p, VALUE pre_rest_arg, VALUE args, VALUE post_rest_arg, const YYLTYPE *loc)
{
    return ripper_new_yylval2(p, pre_rest_arg, args, post_rest_arg);
}

#define new_hash(p,h,l) rb_ary_new_from_args(0)

static VALUE
new_unique_key_hash(struct parser_params *p, VALUE ary, const YYLTYPE *loc)
{
    return ary;
}

static VALUE
new_hash_pattern(struct parser_params *p, VALUE constant, VALUE hshptn, const YYLTYPE *loc)
{
    struct RNode_RIPPER_VALUES *t = RNODE_RIPPER_VALUES(hshptn);
    VALUE kw_args = t->nd_val1, kw_rest_arg = t->nd_val2;
    return dispatch3(hshptn, constant, kw_args, kw_rest_arg);
}

static VALUE
new_hash_pattern_tail(struct parser_params *p, VALUE kw_args, VALUE kw_rest_arg, const YYLTYPE *loc)
{
    if (kw_rest_arg) {
        kw_rest_arg = dispatch1(var_field, kw_rest_arg);
    }
    else {
        kw_rest_arg = Qnil;
    }
    return ripper_new_yylval2(p, kw_args, kw_rest_arg, Qnil);
}

#define new_defined(p,expr,loc) dispatch1(defined, (expr))

static VALUE heredoc_dedent(struct parser_params*,VALUE);

#else
#define ID2VAL(id) (id)
#define TOKEN2VAL(t) ID2VAL(t)
#define KWD2EID(t, v) keyword_##t

static NODE *
new_scope_body(struct parser_params *p, rb_node_args_t *args, NODE *body, const YYLTYPE *loc)
{
    body = remove_begin(body);
    reduce_nodes(p, &body);
    NODE *n = NEW_SCOPE(args, body, loc);
    nd_set_line(n, loc->end_pos.lineno);
    set_line_body(body, loc->beg_pos.lineno);
    return n;
}

static NODE *
rescued_expr(struct parser_params *p, NODE *arg, NODE *rescue,
             const YYLTYPE *arg_loc, const YYLTYPE *mod_loc, const YYLTYPE *res_loc)
{
    YYLTYPE loc = code_loc_gen(mod_loc, res_loc);
    rescue = NEW_RESBODY(0, remove_begin(rescue), 0, &loc);
    loc.beg_pos = arg_loc->beg_pos;
    return NEW_RESCUE(arg, rescue, 0, &loc);
}

#endif /* RIPPER */

static NODE *add_block_exit(struct parser_params *p, NODE *node);
static rb_node_exits_t *init_block_exit(struct parser_params *p);
static rb_node_exits_t *allow_block_exit(struct parser_params *p);
static void restore_block_exit(struct parser_params *p, rb_node_exits_t *exits);
static void clear_block_exit(struct parser_params *p, bool error);

static void
next_rescue_context(struct lex_context *next, const struct lex_context *outer, enum rescue_context def)
{
    next->in_rescue = outer->in_rescue == after_rescue ? after_rescue : def;
}

static void
restore_defun(struct parser_params *p, rb_node_def_temp_t *temp)
{
    /* See: def_name action */
    struct lex_context ctxt = temp->save.ctxt;
    p->cur_arg = temp->save.cur_arg;
    p->ctxt.in_def = ctxt.in_def;
    p->ctxt.shareable_constant_value = ctxt.shareable_constant_value;
    p->ctxt.in_rescue = ctxt.in_rescue;
    p->max_numparam = temp->save.max_numparam;
    numparam_pop(p, temp->save.numparam_save);
    clear_block_exit(p, true);
}

static void
endless_method_name(struct parser_params *p, ID mid, const YYLTYPE *loc)
{
    if (is_attrset_id(mid)) {
        yyerror1(loc, "setter method cannot be defined in an endless method definition");
    }
    token_info_drop(p, "def", loc->beg_pos);
}

#define debug_token_line(p, name, line) do { \
        if (p->debug) { \
            const char *const pcur = p->lex.pcur; \
            const char *const ptok = p->lex.ptok; \
            rb_parser_printf(p, name ":%d (%d: %"PRIdPTRDIFF"|%"PRIdPTRDIFF"|%"PRIdPTRDIFF")\n", \
                             line, p->ruby_sourceline, \
                             ptok - p->lex.pbeg, pcur - ptok, p->lex.pend - pcur); \
        } \
    } while (0)

#define begin_definition(k, loc_beg, loc_end) \
    do { \
        if (!(p->ctxt.in_class = (k)[0] != 0)) { \
            p->ctxt.in_def = 0; \
        } \
        else if (p->ctxt.in_def) { \
            YYLTYPE loc = code_loc_gen(loc_beg, loc_end); \
            yyerror1(&loc, k " definition in method body"); \
        } \
        local_push(p, 0); \
    } while (0)

#ifndef RIPPER
# define Qnone 0
# define Qnull 0
# define ifndef_ripper(x) (x)
#else
# define Qnone Qnil
# define Qnull Qundef
# define ifndef_ripper(x)
#endif

# define rb_warn0(fmt)         WARN_CALL(WARN_ARGS(fmt, 1))
# define rb_warn1(fmt,a)       WARN_CALL(WARN_ARGS(fmt, 2), (a))
# define rb_warn2(fmt,a,b)     WARN_CALL(WARN_ARGS(fmt, 3), (a), (b))
# define rb_warn3(fmt,a,b,c)   WARN_CALL(WARN_ARGS(fmt, 4), (a), (b), (c))
# define rb_warn4(fmt,a,b,c,d) WARN_CALL(WARN_ARGS(fmt, 5), (a), (b), (c), (d))
# define rb_warning0(fmt)         WARNING_CALL(WARNING_ARGS(fmt, 1))
# define rb_warning1(fmt,a)       WARNING_CALL(WARNING_ARGS(fmt, 2), (a))
# define rb_warning2(fmt,a,b)     WARNING_CALL(WARNING_ARGS(fmt, 3), (a), (b))
# define rb_warning3(fmt,a,b,c)   WARNING_CALL(WARNING_ARGS(fmt, 4), (a), (b), (c))
# define rb_warning4(fmt,a,b,c,d) WARNING_CALL(WARNING_ARGS(fmt, 5), (a), (b), (c), (d))
# define rb_warn0L(l,fmt)         WARN_CALL(WARN_ARGS_L(l, fmt, 1))
# define rb_warn1L(l,fmt,a)       WARN_CALL(WARN_ARGS_L(l, fmt, 2), (a))
# define rb_warn2L(l,fmt,a,b)     WARN_CALL(WARN_ARGS_L(l, fmt, 3), (a), (b))
# define rb_warn3L(l,fmt,a,b,c)   WARN_CALL(WARN_ARGS_L(l, fmt, 4), (a), (b), (c))
# define rb_warn4L(l,fmt,a,b,c,d) WARN_CALL(WARN_ARGS_L(l, fmt, 5), (a), (b), (c), (d))
# define rb_warning0L(l,fmt)         WARNING_CALL(WARNING_ARGS_L(l, fmt, 1))
# define rb_warning1L(l,fmt,a)       WARNING_CALL(WARNING_ARGS_L(l, fmt, 2), (a))
# define rb_warning2L(l,fmt,a,b)     WARNING_CALL(WARNING_ARGS_L(l, fmt, 3), (a), (b))
# define rb_warning3L(l,fmt,a,b,c)   WARNING_CALL(WARNING_ARGS_L(l, fmt, 4), (a), (b), (c))
# define rb_warning4L(l,fmt,a,b,c,d) WARNING_CALL(WARNING_ARGS_L(l, fmt, 5), (a), (b), (c), (d))
#ifdef RIPPER
extern const ID id_warn, id_warning, id_gets, id_assoc;
# define ERR_MESG() STR_NEW2(mesg) /* to bypass Ripper DSL */
# define WARN_S_L(s,l) STR_NEW(s,l)
# define WARN_S(s) STR_NEW2(s)
# define WARN_I(i) INT2NUM(i)
# define WARN_ID(i) rb_id2str(i)
# define WARN_IVAL(i) i
# define PRIsWARN "s"
# define rb_warn0L_experimental(l,fmt)         WARN_CALL(WARN_ARGS_L(l, fmt, 1))
# define WARN_ARGS(fmt,n) p->value, id_warn, n, rb_usascii_str_new_lit(fmt)
# define WARN_ARGS_L(l,fmt,n) WARN_ARGS(fmt,n)
# ifdef HAVE_VA_ARGS_MACRO
# define WARN_CALL(...) rb_funcall(__VA_ARGS__)
# else
# define WARN_CALL rb_funcall
# endif
# define WARNING_ARGS(fmt,n) p->value, id_warning, n, rb_usascii_str_new_lit(fmt)
# define WARNING_ARGS_L(l, fmt,n) WARNING_ARGS(fmt,n)
# ifdef HAVE_VA_ARGS_MACRO
# define WARNING_CALL(...) rb_funcall(__VA_ARGS__)
# else
# define WARNING_CALL rb_funcall
# endif
# define compile_error ripper_compile_error
#else
# define WARN_S_L(s,l) s
# define WARN_S(s) s
# define WARN_I(i) i
# define WARN_ID(i) rb_id2name(i)
# define WARN_IVAL(i) NUM2INT(i)
# define PRIsWARN PRIsVALUE
# define WARN_ARGS(fmt,n) WARN_ARGS_L(p->ruby_sourceline,fmt,n)
# define WARN_ARGS_L(l,fmt,n) p->ruby_sourcefile, (l), (fmt)
# define WARN_CALL rb_compile_warn
# define rb_warn0L_experimental(l,fmt) rb_category_compile_warn(RB_WARN_CATEGORY_EXPERIMENTAL, WARN_ARGS_L(l, fmt, 1))
# define WARNING_ARGS(fmt,n) WARN_ARGS(fmt,n)
# define WARNING_ARGS_L(l,fmt,n) WARN_ARGS_L(l,fmt,n)
# define WARNING_CALL rb_compile_warning
PRINTF_ARGS(static void parser_compile_error(struct parser_params*, const rb_code_location_t *loc, const char *fmt, ...), 3, 4);
# define compile_error(p, ...) parser_compile_error(p, NULL, __VA_ARGS__)
#endif

struct RNode_EXITS {
    NODE node;

    NODE *nd_chain; /* Assume NODE_BREAK, NODE_NEXT, NODE_REDO have nd_chain here */
    NODE *nd_end;
};

#define RNODE_EXITS(node) ((rb_node_exits_t*)(node))

static NODE *
add_block_exit(struct parser_params *p, NODE *node)
{
    if (!node) {
        compile_error(p, "unexpected null node");
        return 0;
    }
    switch (nd_type(node)) {
      case NODE_BREAK: case NODE_NEXT: case NODE_REDO: break;
      default:
        compile_error(p, "unexpected node: %s", parser_node_name(nd_type(node)));
        return node;
    }
    if (!p->ctxt.in_defined) {
        rb_node_exits_t *exits = p->exits;
        if (exits) {
            RNODE_EXITS(exits->nd_end)->nd_chain = node;
            exits->nd_end = node;
        }
    }
    return node;
}

static rb_node_exits_t *
init_block_exit(struct parser_params *p)
{
    rb_node_exits_t *old = p->exits;
    rb_node_exits_t *exits = NODE_NEW_INTERNAL(NODE_EXITS, rb_node_exits_t);
    exits->nd_chain = 0;
    exits->nd_end = RNODE(exits);
    p->exits = exits;
    return old;
}

static rb_node_exits_t *
allow_block_exit(struct parser_params *p)
{
    rb_node_exits_t *exits = p->exits;
    p->exits = 0;
    return exits;
}

static void
restore_block_exit(struct parser_params *p, rb_node_exits_t *exits)
{
    p->exits = exits;
}

static void
clear_block_exit(struct parser_params *p, bool error)
{
    rb_node_exits_t *exits = p->exits;
    if (!exits) return;
    if (error && !compile_for_eval) {
        for (NODE *e = RNODE(exits); (e = RNODE_EXITS(e)->nd_chain) != 0; ) {
            switch (nd_type(e)) {
              case NODE_BREAK:
                yyerror1(&e->nd_loc, "Invalid break");
                break;
              case NODE_NEXT:
                yyerror1(&e->nd_loc, "Invalid next");
                break;
              case NODE_REDO:
                yyerror1(&e->nd_loc, "Invalid redo");
                break;
              default:
                yyerror1(&e->nd_loc, "unexpected node");
                goto end_checks; /* no nd_chain */
            }
        }
      end_checks:;
    }
    exits->nd_end = RNODE(exits);
    exits->nd_chain = 0;
}

#define WARN_EOL(tok) \
    (looking_at_eol_p(p) ? \
     (void)rb_warning0("`" tok "' at the end of line without an expression") : \
     (void)0)
static int looking_at_eol_p(struct parser_params *p);

#ifndef RIPPER
static NODE *
get_nd_value(struct parser_params *p, NODE *node)
{
    switch (nd_type(node)) {
      case NODE_GASGN:
        return RNODE_GASGN(node)->nd_value;
      case NODE_IASGN:
        return RNODE_IASGN(node)->nd_value;
      case NODE_LASGN:
        return RNODE_LASGN(node)->nd_value;
      case NODE_DASGN:
        return RNODE_DASGN(node)->nd_value;
      case NODE_MASGN:
        return RNODE_MASGN(node)->nd_value;
      case NODE_CVASGN:
        return RNODE_CVASGN(node)->nd_value;
      case NODE_CDECL:
        return RNODE_CDECL(node)->nd_value;
      default:
        compile_error(p, "unexpected node: %s", parser_node_name(nd_type(node)));
        return 0;
    }
}

static void
set_nd_value(struct parser_params *p, NODE *node, NODE *rhs)
{
    switch (nd_type(node)) {
      case NODE_CDECL:
        RNODE_CDECL(node)->nd_value = rhs;
        break;
      case NODE_GASGN:
        RNODE_GASGN(node)->nd_value = rhs;
        break;
      case NODE_IASGN:
        RNODE_IASGN(node)->nd_value = rhs;
        break;
      case NODE_LASGN:
        RNODE_LASGN(node)->nd_value = rhs;
        break;
      case NODE_DASGN:
        RNODE_DASGN(node)->nd_value = rhs;
        break;
      case NODE_MASGN:
        RNODE_MASGN(node)->nd_value = rhs;
        break;
      case NODE_CVASGN:
        RNODE_CVASGN(node)->nd_value = rhs;
        break;
      default:
        compile_error(p, "unexpected node: %s", parser_node_name(nd_type(node)));
        break;
    }
}

static ID
get_nd_vid(struct parser_params *p, NODE *node)
{
    switch (nd_type(node)) {
      case NODE_CDECL:
        return RNODE_CDECL(node)->nd_vid;
      case NODE_GASGN:
        return RNODE_GASGN(node)->nd_vid;
      case NODE_IASGN:
        return RNODE_IASGN(node)->nd_vid;
      case NODE_LASGN:
        return RNODE_LASGN(node)->nd_vid;
      case NODE_DASGN:
        return RNODE_DASGN(node)->nd_vid;
      case NODE_CVASGN:
        return RNODE_CVASGN(node)->nd_vid;
      default:
        compile_error(p, "unexpected node: %s", parser_node_name(nd_type(node)));
        return 0;
    }
}

static NODE *
get_nd_args(struct parser_params *p, NODE *node)
{
    switch (nd_type(node)) {
      case NODE_CALL:
        return RNODE_CALL(node)->nd_args;
      case NODE_OPCALL:
        return RNODE_OPCALL(node)->nd_args;
      case NODE_FCALL:
        return RNODE_FCALL(node)->nd_args;
      case NODE_QCALL:
        return RNODE_QCALL(node)->nd_args;
      case NODE_VCALL:
      case NODE_SUPER:
      case NODE_ZSUPER:
      case NODE_YIELD:
      case NODE_RETURN:
      case NODE_BREAK:
      case NODE_NEXT:
        return 0;
      default:
        compile_error(p, "unexpected node: %s", parser_node_name(nd_type(node)));
        return 0;
    }
}
#endif

#line 1967 "parse.c"

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

#include "parse.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end-of-input"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_keyword_class = 3,              /* "`class'"  */
  YYSYMBOL_keyword_module = 4,             /* "`module'"  */
  YYSYMBOL_keyword_def = 5,                /* "`def'"  */
  YYSYMBOL_keyword_undef = 6,              /* "`undef'"  */
  YYSYMBOL_keyword_begin = 7,              /* "`begin'"  */
  YYSYMBOL_keyword_rescue = 8,             /* "`rescue'"  */
  YYSYMBOL_keyword_ensure = 9,             /* "`ensure'"  */
  YYSYMBOL_keyword_end = 10,               /* "`end'"  */
  YYSYMBOL_keyword_if = 11,                /* "`if'"  */
  YYSYMBOL_keyword_unless = 12,            /* "`unless'"  */
  YYSYMBOL_keyword_then = 13,              /* "`then'"  */
  YYSYMBOL_keyword_elsif = 14,             /* "`elsif'"  */
  YYSYMBOL_keyword_else = 15,              /* "`else'"  */
  YYSYMBOL_keyword_case = 16,              /* "`case'"  */
  YYSYMBOL_keyword_when = 17,              /* "`when'"  */
  YYSYMBOL_keyword_while = 18,             /* "`while'"  */
  YYSYMBOL_keyword_until = 19,             /* "`until'"  */
  YYSYMBOL_keyword_for = 20,               /* "`for'"  */
  YYSYMBOL_keyword_break = 21,             /* "`break'"  */
  YYSYMBOL_keyword_next = 22,              /* "`next'"  */
  YYSYMBOL_keyword_redo = 23,              /* "`redo'"  */
  YYSYMBOL_keyword_retry = 24,             /* "`retry'"  */
  YYSYMBOL_keyword_in = 25,                /* "`in'"  */
  YYSYMBOL_keyword_do = 26,                /* "`do'"  */
  YYSYMBOL_keyword_do_cond = 27,           /* "`do' for condition"  */
  YYSYMBOL_keyword_do_block = 28,          /* "`do' for block"  */
  YYSYMBOL_keyword_do_LAMBDA = 29,         /* "`do' for lambda"  */
  YYSYMBOL_keyword_return = 30,            /* "`return'"  */
  YYSYMBOL_keyword_yield = 31,             /* "`yield'"  */
  YYSYMBOL_keyword_super = 32,             /* "`super'"  */
  YYSYMBOL_keyword_self = 33,              /* "`self'"  */
  YYSYMBOL_keyword_nil = 34,               /* "`nil'"  */
  YYSYMBOL_keyword_true = 35,              /* "`true'"  */
  YYSYMBOL_keyword_false = 36,             /* "`false'"  */
  YYSYMBOL_keyword_and = 37,               /* "`and'"  */
  YYSYMBOL_keyword_or = 38,                /* "`or'"  */
  YYSYMBOL_keyword_not = 39,               /* "`not'"  */
  YYSYMBOL_modifier_if = 40,               /* "`if' modifier"  */
  YYSYMBOL_modifier_unless = 41,           /* "`unless' modifier"  */
  YYSYMBOL_modifier_while = 42,            /* "`while' modifier"  */
  YYSYMBOL_modifier_until = 43,            /* "`until' modifier"  */
  YYSYMBOL_modifier_rescue = 44,           /* "`rescue' modifier"  */
  YYSYMBOL_keyword_alias = 45,             /* "`alias'"  */
  YYSYMBOL_keyword_defined = 46,           /* "`defined?'"  */
  YYSYMBOL_keyword_BEGIN = 47,             /* "`BEGIN'"  */
  YYSYMBOL_keyword_END = 48,               /* "`END'"  */
  YYSYMBOL_keyword__LINE__ = 49,           /* "`__LINE__'"  */
  YYSYMBOL_keyword__FILE__ = 50,           /* "`__FILE__'"  */
  YYSYMBOL_keyword__ENCODING__ = 51,       /* "`__ENCODING__'"  */
  YYSYMBOL_tIDENTIFIER = 52,               /* "local variable or method"  */
  YYSYMBOL_tFID = 53,                      /* "method"  */
  YYSYMBOL_tGVAR = 54,                     /* "global variable"  */
  YYSYMBOL_tIVAR = 55,                     /* "instance variable"  */
  YYSYMBOL_tCONSTANT = 56,                 /* "constant"  */
  YYSYMBOL_tCVAR = 57,                     /* "class variable"  */
  YYSYMBOL_tLABEL = 58,                    /* "label"  */
  YYSYMBOL_tINTEGER = 59,                  /* "integer literal"  */
  YYSYMBOL_tFLOAT = 60,                    /* "float literal"  */
  YYSYMBOL_tRATIONAL = 61,                 /* "rational literal"  */
  YYSYMBOL_tIMAGINARY = 62,                /* "imaginary literal"  */
  YYSYMBOL_tCHAR = 63,                     /* "char literal"  */
  YYSYMBOL_tNTH_REF = 64,                  /* "numbered reference"  */
  YYSYMBOL_tBACK_REF = 65,                 /* "back reference"  */
  YYSYMBOL_tSTRING_CONTENT = 66,           /* "literal content"  */
  YYSYMBOL_tREGEXP_END = 67,               /* tREGEXP_END  */
  YYSYMBOL_tDUMNY_END = 68,                /* "dummy end"  */
  YYSYMBOL_69_ = 69,                       /* '.'  */
  YYSYMBOL_70_backslash_ = 70,             /* "backslash"  */
  YYSYMBOL_tSP = 71,                       /* "escaped space"  */
  YYSYMBOL_72_escaped_horizontal_tab_ = 72, /* "escaped horizontal tab"  */
  YYSYMBOL_73_escaped_form_feed_ = 73,     /* "escaped form feed"  */
  YYSYMBOL_74_escaped_carriage_return_ = 74, /* "escaped carriage return"  */
  YYSYMBOL_75_escaped_vertical_tab_ = 75,  /* "escaped vertical tab"  */
  YYSYMBOL_tUPLUS = 76,                    /* "unary+"  */
  YYSYMBOL_tUMINUS = 77,                   /* "unary-"  */
  YYSYMBOL_tPOW = 78,                      /* "**"  */
  YYSYMBOL_tCMP = 79,                      /* "<=>"  */
  YYSYMBOL_tEQ = 80,                       /* "=="  */
  YYSYMBOL_tEQQ = 81,                      /* "==="  */
  YYSYMBOL_tNEQ = 82,                      /* "!="  */
  YYSYMBOL_tGEQ = 83,                      /* ">="  */
  YYSYMBOL_tLEQ = 84,                      /* "<="  */
  YYSYMBOL_tANDOP = 85,                    /* "&&"  */
  YYSYMBOL_tOROP = 86,                     /* "||"  */
  YYSYMBOL_tMATCH = 87,                    /* "=~"  */
  YYSYMBOL_tNMATCH = 88,                   /* "!~"  */
  YYSYMBOL_tDOT2 = 89,                     /* ".."  */
  YYSYMBOL_tDOT3 = 90,                     /* "..."  */
  YYSYMBOL_tBDOT2 = 91,                    /* "(.."  */
  YYSYMBOL_tBDOT3 = 92,                    /* "(..."  */
  YYSYMBOL_tAREF = 93,                     /* "[]"  */
  YYSYMBOL_tASET = 94,                     /* "[]="  */
  YYSYMBOL_tLSHFT = 95,                    /* "<<"  */
  YYSYMBOL_tRSHFT = 96,                    /* ">>"  */
  YYSYMBOL_tANDDOT = 97,                   /* "&."  */
  YYSYMBOL_tCOLON2 = 98,                   /* "::"  */
  YYSYMBOL_tCOLON3 = 99,                   /* ":: at EXPR_BEG"  */
  YYSYMBOL_tOP_ASGN = 100,                 /* "operator-assignment"  */
  YYSYMBOL_tASSOC = 101,                   /* "=>"  */
  YYSYMBOL_tLPAREN = 102,                  /* "("  */
  YYSYMBOL_tLPAREN_ARG = 103,              /* "( arg"  */
  YYSYMBOL_tRPAREN = 104,                  /* ")"  */
  YYSYMBOL_tLBRACK = 105,                  /* "["  */
  YYSYMBOL_tLBRACE = 106,                  /* "{"  */
  YYSYMBOL_tLBRACE_ARG = 107,              /* "{ arg"  */
  YYSYMBOL_tSTAR = 108,                    /* "*"  */
  YYSYMBOL_tDSTAR = 109,                   /* "**arg"  */
  YYSYMBOL_tAMPER = 110,                   /* "&"  */
  YYSYMBOL_tLAMBDA = 111,                  /* "->"  */
  YYSYMBOL_tSYMBEG = 112,                  /* "symbol literal"  */
  YYSYMBOL_tSTRING_BEG = 113,              /* "string literal"  */
  YYSYMBOL_tXSTRING_BEG = 114,             /* "backtick literal"  */
  YYSYMBOL_tREGEXP_BEG = 115,              /* "regexp literal"  */
  YYSYMBOL_tWORDS_BEG = 116,               /* "word list"  */
  YYSYMBOL_tQWORDS_BEG = 117,              /* "verbatim word list"  */
  YYSYMBOL_tSYMBOLS_BEG = 118,             /* "symbol list"  */
  YYSYMBOL_tQSYMBOLS_BEG = 119,            /* "verbatim symbol list"  */
  YYSYMBOL_tSTRING_END = 120,              /* "terminator"  */
  YYSYMBOL_tSTRING_DEND = 121,             /* "'}'"  */
  YYSYMBOL_tSTRING_DBEG = 122,             /* tSTRING_DBEG  */
  YYSYMBOL_tSTRING_DVAR = 123,             /* tSTRING_DVAR  */
  YYSYMBOL_tLAMBEG = 124,                  /* tLAMBEG  */
  YYSYMBOL_tLABEL_END = 125,               /* tLABEL_END  */
  YYSYMBOL_tIGNORED_NL = 126,              /* tIGNORED_NL  */
  YYSYMBOL_tCOMMENT = 127,                 /* tCOMMENT  */
  YYSYMBOL_tEMBDOC_BEG = 128,              /* tEMBDOC_BEG  */
  YYSYMBOL_tEMBDOC = 129,                  /* tEMBDOC  */
  YYSYMBOL_tEMBDOC_END = 130,              /* tEMBDOC_END  */
  YYSYMBOL_tHEREDOC_BEG = 131,             /* tHEREDOC_BEG  */
  YYSYMBOL_tHEREDOC_END = 132,             /* tHEREDOC_END  */
  YYSYMBOL_k__END__ = 133,                 /* k__END__  */
  YYSYMBOL_tLOWEST = 134,                  /* tLOWEST  */
  YYSYMBOL_135_ = 135,                     /* '='  */
  YYSYMBOL_136_ = 136,                     /* '?'  */
  YYSYMBOL_137_ = 137,                     /* ':'  */
  YYSYMBOL_138_ = 138,                     /* '>'  */
  YYSYMBOL_139_ = 139,                     /* '<'  */
  YYSYMBOL_140_ = 140,                     /* '|'  */
  YYSYMBOL_141_ = 141,                     /* '^'  */
  YYSYMBOL_142_ = 142,                     /* '&'  */
  YYSYMBOL_143_ = 143,                     /* '+'  */
  YYSYMBOL_144_ = 144,                     /* '-'  */
  YYSYMBOL_145_ = 145,                     /* '*'  */
  YYSYMBOL_146_ = 146,                     /* '/'  */
  YYSYMBOL_147_ = 147,                     /* '%'  */
  YYSYMBOL_tUMINUS_NUM = 148,              /* tUMINUS_NUM  */
  YYSYMBOL_149_ = 149,                     /* '!'  */
  YYSYMBOL_150_ = 150,                     /* '~'  */
  YYSYMBOL_tLAST_TOKEN = 151,              /* tLAST_TOKEN  */
  YYSYMBOL_152_ = 152,                     /* '{'  */
  YYSYMBOL_153_ = 153,                     /* '}'  */
  YYSYMBOL_154_ = 154,                     /* '['  */
  YYSYMBOL_155_ = 155,                     /* ','  */
  YYSYMBOL_156_ = 156,                     /* '`'  */
  YYSYMBOL_157_ = 157,                     /* '('  */
  YYSYMBOL_158_ = 158,                     /* ')'  */
  YYSYMBOL_159_ = 159,                     /* ']'  */
  YYSYMBOL_160_ = 160,                     /* ';'  */
  YYSYMBOL_161_ = 161,                     /* ' '  */
  YYSYMBOL_162_n_ = 162,                   /* '\n'  */
  YYSYMBOL_YYACCEPT = 163,                 /* $accept  */
  YYSYMBOL_program = 164,                  /* program  */
  YYSYMBOL_165_1 = 165,                    /* $@1  */
  YYSYMBOL_top_compstmt = 166,             /* top_compstmt  */
  YYSYMBOL_top_stmts = 167,                /* top_stmts  */
  YYSYMBOL_top_stmt = 168,                 /* top_stmt  */
  YYSYMBOL_block_open = 169,               /* block_open  */
  YYSYMBOL_begin_block = 170,              /* begin_block  */
  YYSYMBOL_bodystmt = 171,                 /* bodystmt  */
  YYSYMBOL_172_2 = 172,                    /* $@2  */
  YYSYMBOL_173_3 = 173,                    /* $@3  */
  YYSYMBOL_174_4 = 174,                    /* $@4  */
  YYSYMBOL_compstmt = 175,                 /* compstmt  */
  YYSYMBOL_stmts = 176,                    /* stmts  */
  YYSYMBOL_stmt_or_begin = 177,            /* stmt_or_begin  */
  YYSYMBOL_178_5 = 178,                    /* $@5  */
  YYSYMBOL_allow_exits = 179,              /* allow_exits  */
  YYSYMBOL_k_END = 180,                    /* k_END  */
  YYSYMBOL_stmt = 181,                     /* stmt  */
  YYSYMBOL_182_6 = 182,                    /* $@6  */
  YYSYMBOL_command_asgn = 183,             /* command_asgn  */
  YYSYMBOL_endless_command = 184,          /* endless_command  */
  YYSYMBOL_command_rhs = 185,              /* command_rhs  */
  YYSYMBOL_expr = 186,                     /* expr  */
  YYSYMBOL_187_7 = 187,                    /* $@7  */
  YYSYMBOL_188_8 = 188,                    /* $@8  */
  YYSYMBOL_def_name = 189,                 /* def_name  */
  YYSYMBOL_defn_head = 190,                /* defn_head  */
  YYSYMBOL_defs_head = 191,                /* defs_head  */
  YYSYMBOL_192_9 = 192,                    /* $@9  */
  YYSYMBOL_expr_value = 193,               /* expr_value  */
  YYSYMBOL_expr_value_do = 194,            /* expr_value_do  */
  YYSYMBOL_195_10 = 195,                   /* $@10  */
  YYSYMBOL_196_11 = 196,                   /* $@11  */
  YYSYMBOL_command_call = 197,             /* command_call  */
  YYSYMBOL_block_command = 198,            /* block_command  */
  YYSYMBOL_cmd_brace_block = 199,          /* cmd_brace_block  */
  YYSYMBOL_fcall = 200,                    /* fcall  */
  YYSYMBOL_command = 201,                  /* command  */
  YYSYMBOL_mlhs = 202,                     /* mlhs  */
  YYSYMBOL_mlhs_inner = 203,               /* mlhs_inner  */
  YYSYMBOL_mlhs_basic = 204,               /* mlhs_basic  */
  YYSYMBOL_mlhs_item = 205,                /* mlhs_item  */
  YYSYMBOL_mlhs_head = 206,                /* mlhs_head  */
  YYSYMBOL_mlhs_post = 207,                /* mlhs_post  */
  YYSYMBOL_mlhs_node = 208,                /* mlhs_node  */
  YYSYMBOL_lhs = 209,                      /* lhs  */
  YYSYMBOL_cname = 210,                    /* cname  */
  YYSYMBOL_cpath = 211,                    /* cpath  */
  YYSYMBOL_fname = 212,                    /* fname  */
  YYSYMBOL_fitem = 213,                    /* fitem  */
  YYSYMBOL_undef_list = 214,               /* undef_list  */
  YYSYMBOL_215_12 = 215,                   /* $@12  */
  YYSYMBOL_op = 216,                       /* op  */
  YYSYMBOL_reswords = 217,                 /* reswords  */
  YYSYMBOL_arg = 218,                      /* arg  */
  YYSYMBOL_endless_arg = 219,              /* endless_arg  */
  YYSYMBOL_relop = 220,                    /* relop  */
  YYSYMBOL_rel_expr = 221,                 /* rel_expr  */
  YYSYMBOL_lex_ctxt = 222,                 /* lex_ctxt  */
  YYSYMBOL_begin_defined = 223,            /* begin_defined  */
  YYSYMBOL_after_rescue = 224,             /* after_rescue  */
  YYSYMBOL_arg_value = 225,                /* arg_value  */
  YYSYMBOL_aref_args = 226,                /* aref_args  */
  YYSYMBOL_arg_rhs = 227,                  /* arg_rhs  */
  YYSYMBOL_paren_args = 228,               /* paren_args  */
  YYSYMBOL_opt_paren_args = 229,           /* opt_paren_args  */
  YYSYMBOL_opt_call_args = 230,            /* opt_call_args  */
  YYSYMBOL_call_args = 231,                /* call_args  */
  YYSYMBOL_command_args = 232,             /* command_args  */
  YYSYMBOL_233_13 = 233,                   /* $@13  */
  YYSYMBOL_block_arg = 234,                /* block_arg  */
  YYSYMBOL_opt_block_arg = 235,            /* opt_block_arg  */
  YYSYMBOL_args = 236,                     /* args  */
  YYSYMBOL_arg_splat = 237,                /* arg_splat  */
  YYSYMBOL_mrhs_arg = 238,                 /* mrhs_arg  */
  YYSYMBOL_mrhs = 239,                     /* mrhs  */
  YYSYMBOL_primary = 240,                  /* primary  */
  YYSYMBOL_241_14 = 241,                   /* $@14  */
  YYSYMBOL_242_15 = 242,                   /* $@15  */
  YYSYMBOL_243_16 = 243,                   /* @16  */
  YYSYMBOL_244_17 = 244,                   /* @17  */
  YYSYMBOL_245_18 = 245,                   /* $@18  */
  YYSYMBOL_246_19 = 246,                   /* $@19  */
  YYSYMBOL_247_20 = 247,                   /* $@20  */
  YYSYMBOL_248_21 = 248,                   /* $@21  */
  YYSYMBOL_249_22 = 249,                   /* $@22  */
  YYSYMBOL_primary_value = 250,            /* primary_value  */
  YYSYMBOL_k_begin = 251,                  /* k_begin  */
  YYSYMBOL_k_if = 252,                     /* k_if  */
  YYSYMBOL_k_unless = 253,                 /* k_unless  */
  YYSYMBOL_k_while = 254,                  /* k_while  */
  YYSYMBOL_k_until = 255,                  /* k_until  */
  YYSYMBOL_k_case = 256,                   /* k_case  */
  YYSYMBOL_k_for = 257,                    /* k_for  */
  YYSYMBOL_k_class = 258,                  /* k_class  */
  YYSYMBOL_k_module = 259,                 /* k_module  */
  YYSYMBOL_k_def = 260,                    /* k_def  */
  YYSYMBOL_k_do = 261,                     /* k_do  */
  YYSYMBOL_k_do_block = 262,               /* k_do_block  */
  YYSYMBOL_k_rescue = 263,                 /* k_rescue  */
  YYSYMBOL_k_ensure = 264,                 /* k_ensure  */
  YYSYMBOL_k_when = 265,                   /* k_when  */
  YYSYMBOL_k_else = 266,                   /* k_else  */
  YYSYMBOL_k_elsif = 267,                  /* k_elsif  */
  YYSYMBOL_k_end = 268,                    /* k_end  */
  YYSYMBOL_k_return = 269,                 /* k_return  */
  YYSYMBOL_k_yield = 270,                  /* k_yield  */
  YYSYMBOL_then = 271,                     /* then  */
  YYSYMBOL_do = 272,                       /* do  */
  YYSYMBOL_if_tail = 273,                  /* if_tail  */
  YYSYMBOL_opt_else = 274,                 /* opt_else  */
  YYSYMBOL_for_var = 275,                  /* for_var  */
  YYSYMBOL_f_marg = 276,                   /* f_marg  */
  YYSYMBOL_f_marg_list = 277,              /* f_marg_list  */
  YYSYMBOL_f_margs = 278,                  /* f_margs  */
  YYSYMBOL_f_rest_marg = 279,              /* f_rest_marg  */
  YYSYMBOL_f_any_kwrest = 280,             /* f_any_kwrest  */
  YYSYMBOL_f_eq = 281,                     /* f_eq  */
  YYSYMBOL_282_23 = 282,                   /* $@23  */
  YYSYMBOL_block_args_tail = 283,          /* block_args_tail  */
  YYSYMBOL_opt_block_args_tail = 284,      /* opt_block_args_tail  */
  YYSYMBOL_excessed_comma = 285,           /* excessed_comma  */
  YYSYMBOL_block_param = 286,              /* block_param  */
  YYSYMBOL_opt_block_param = 287,          /* opt_block_param  */
  YYSYMBOL_block_param_def = 288,          /* block_param_def  */
  YYSYMBOL_opt_bv_decl = 289,              /* opt_bv_decl  */
  YYSYMBOL_bv_decls = 290,                 /* bv_decls  */
  YYSYMBOL_bvar = 291,                     /* bvar  */
  YYSYMBOL_max_numparam = 292,             /* max_numparam  */
  YYSYMBOL_numparam = 293,                 /* numparam  */
  YYSYMBOL_lambda = 294,                   /* lambda  */
  YYSYMBOL_295_24 = 295,                   /* @24  */
  YYSYMBOL_296_25 = 296,                   /* $@25  */
  YYSYMBOL_f_larglist = 297,               /* f_larglist  */
  YYSYMBOL_lambda_body = 298,              /* lambda_body  */
  YYSYMBOL_299_26 = 299,                   /* $@26  */
  YYSYMBOL_do_block = 300,                 /* do_block  */
  YYSYMBOL_block_call = 301,               /* block_call  */
  YYSYMBOL_method_call = 302,              /* method_call  */
  YYSYMBOL_brace_block = 303,              /* brace_block  */
  YYSYMBOL_brace_body = 304,               /* brace_body  */
  YYSYMBOL_305_27 = 305,                   /* @27  */
  YYSYMBOL_do_body = 306,                  /* do_body  */
  YYSYMBOL_307_28 = 307,                   /* @28  */
  YYSYMBOL_case_args = 308,                /* case_args  */
  YYSYMBOL_case_body = 309,                /* case_body  */
  YYSYMBOL_cases = 310,                    /* cases  */
  YYSYMBOL_p_pvtbl = 311,                  /* p_pvtbl  */
  YYSYMBOL_p_pktbl = 312,                  /* p_pktbl  */
  YYSYMBOL_p_in_kwarg = 313,               /* p_in_kwarg  */
  YYSYMBOL_p_case_body = 314,              /* p_case_body  */
  YYSYMBOL_315_29 = 315,                   /* $@29  */
  YYSYMBOL_p_cases = 316,                  /* p_cases  */
  YYSYMBOL_p_top_expr = 317,               /* p_top_expr  */
  YYSYMBOL_p_top_expr_body = 318,          /* p_top_expr_body  */
  YYSYMBOL_p_expr = 319,                   /* p_expr  */
  YYSYMBOL_p_as = 320,                     /* p_as  */
  YYSYMBOL_p_alt = 321,                    /* p_alt  */
  YYSYMBOL_p_lparen = 322,                 /* p_lparen  */
  YYSYMBOL_p_lbracket = 323,               /* p_lbracket  */
  YYSYMBOL_p_expr_basic = 324,             /* p_expr_basic  */
  YYSYMBOL_325_30 = 325,                   /* $@30  */
  YYSYMBOL_p_args = 326,                   /* p_args  */
  YYSYMBOL_p_args_head = 327,              /* p_args_head  */
  YYSYMBOL_p_args_tail = 328,              /* p_args_tail  */
  YYSYMBOL_p_find = 329,                   /* p_find  */
  YYSYMBOL_p_rest = 330,                   /* p_rest  */
  YYSYMBOL_p_args_post = 331,              /* p_args_post  */
  YYSYMBOL_p_arg = 332,                    /* p_arg  */
  YYSYMBOL_p_kwargs = 333,                 /* p_kwargs  */
  YYSYMBOL_p_kwarg = 334,                  /* p_kwarg  */
  YYSYMBOL_p_kw = 335,                     /* p_kw  */
  YYSYMBOL_p_kw_label = 336,               /* p_kw_label  */
  YYSYMBOL_p_kwrest = 337,                 /* p_kwrest  */
  YYSYMBOL_p_kwnorest = 338,               /* p_kwnorest  */
  YYSYMBOL_p_any_kwrest = 339,             /* p_any_kwrest  */
  YYSYMBOL_p_value = 340,                  /* p_value  */
  YYSYMBOL_p_primitive = 341,              /* p_primitive  */
  YYSYMBOL_p_variable = 342,               /* p_variable  */
  YYSYMBOL_p_var_ref = 343,                /* p_var_ref  */
  YYSYMBOL_p_expr_ref = 344,               /* p_expr_ref  */
  YYSYMBOL_p_const = 345,                  /* p_const  */
  YYSYMBOL_opt_rescue = 346,               /* opt_rescue  */
  YYSYMBOL_exc_list = 347,                 /* exc_list  */
  YYSYMBOL_exc_var = 348,                  /* exc_var  */
  YYSYMBOL_opt_ensure = 349,               /* opt_ensure  */
  YYSYMBOL_literal = 350,                  /* literal  */
  YYSYMBOL_strings = 351,                  /* strings  */
  YYSYMBOL_string = 352,                   /* string  */
  YYSYMBOL_string1 = 353,                  /* string1  */
  YYSYMBOL_xstring = 354,                  /* xstring  */
  YYSYMBOL_regexp = 355,                   /* regexp  */
  YYSYMBOL_words_sep = 356,                /* words_sep  */
  YYSYMBOL_words = 357,                    /* words  */
  YYSYMBOL_word_list = 358,                /* word_list  */
  YYSYMBOL_word = 359,                     /* word  */
  YYSYMBOL_symbols = 360,                  /* symbols  */
  YYSYMBOL_symbol_list = 361,              /* symbol_list  */
  YYSYMBOL_qwords = 362,                   /* qwords  */
  YYSYMBOL_qsymbols = 363,                 /* qsymbols  */
  YYSYMBOL_qword_list = 364,               /* qword_list  */
  YYSYMBOL_qsym_list = 365,                /* qsym_list  */
  YYSYMBOL_string_contents = 366,          /* string_contents  */
  YYSYMBOL_xstring_contents = 367,         /* xstring_contents  */
  YYSYMBOL_regexp_contents = 368,          /* regexp_contents  */
  YYSYMBOL_string_content = 369,           /* string_content  */
  YYSYMBOL_370_31 = 370,                   /* @31  */
  YYSYMBOL_371_32 = 371,                   /* @32  */
  YYSYMBOL_372_33 = 372,                   /* @33  */
  YYSYMBOL_373_34 = 373,                   /* @34  */
  YYSYMBOL_string_dend = 374,              /* string_dend  */
  YYSYMBOL_string_dvar = 375,              /* string_dvar  */
  YYSYMBOL_symbol = 376,                   /* symbol  */
  YYSYMBOL_ssym = 377,                     /* ssym  */
  YYSYMBOL_sym = 378,                      /* sym  */
  YYSYMBOL_dsym = 379,                     /* dsym  */
  YYSYMBOL_numeric = 380,                  /* numeric  */
  YYSYMBOL_simple_numeric = 381,           /* simple_numeric  */
  YYSYMBOL_nonlocal_var = 382,             /* nonlocal_var  */
  YYSYMBOL_user_variable = 383,            /* user_variable  */
  YYSYMBOL_keyword_variable = 384,         /* keyword_variable  */
  YYSYMBOL_var_ref = 385,                  /* var_ref  */
  YYSYMBOL_var_lhs = 386,                  /* var_lhs  */
  YYSYMBOL_backref = 387,                  /* backref  */
  YYSYMBOL_superclass = 388,               /* superclass  */
  YYSYMBOL_389_35 = 389,                   /* $@35  */
  YYSYMBOL_f_opt_paren_args = 390,         /* f_opt_paren_args  */
  YYSYMBOL_f_paren_args = 391,             /* f_paren_args  */
  YYSYMBOL_f_arglist = 392,                /* f_arglist  */
  YYSYMBOL_393_36 = 393,                   /* @36  */
  YYSYMBOL_args_tail = 394,                /* args_tail  */
  YYSYMBOL_opt_args_tail = 395,            /* opt_args_tail  */
  YYSYMBOL_f_args = 396,                   /* f_args  */
  YYSYMBOL_args_forward = 397,             /* args_forward  */
  YYSYMBOL_f_bad_arg = 398,                /* f_bad_arg  */
  YYSYMBOL_f_norm_arg = 399,               /* f_norm_arg  */
  YYSYMBOL_f_arg_asgn = 400,               /* f_arg_asgn  */
  YYSYMBOL_f_arg_item = 401,               /* f_arg_item  */
  YYSYMBOL_f_arg = 402,                    /* f_arg  */
  YYSYMBOL_f_label = 403,                  /* f_label  */
  YYSYMBOL_f_kw = 404,                     /* f_kw  */
  YYSYMBOL_f_block_kw = 405,               /* f_block_kw  */
  YYSYMBOL_f_block_kwarg = 406,            /* f_block_kwarg  */
  YYSYMBOL_f_kwarg = 407,                  /* f_kwarg  */
  YYSYMBOL_kwrest_mark = 408,              /* kwrest_mark  */
  YYSYMBOL_f_no_kwarg = 409,               /* f_no_kwarg  */
  YYSYMBOL_f_kwrest = 410,                 /* f_kwrest  */
  YYSYMBOL_f_opt = 411,                    /* f_opt  */
  YYSYMBOL_f_block_opt = 412,              /* f_block_opt  */
  YYSYMBOL_f_block_optarg = 413,           /* f_block_optarg  */
  YYSYMBOL_f_optarg = 414,                 /* f_optarg  */
  YYSYMBOL_restarg_mark = 415,             /* restarg_mark  */
  YYSYMBOL_f_rest_arg = 416,               /* f_rest_arg  */
  YYSYMBOL_blkarg_mark = 417,              /* blkarg_mark  */
  YYSYMBOL_f_block_arg = 418,              /* f_block_arg  */
  YYSYMBOL_opt_f_block_arg = 419,          /* opt_f_block_arg  */
  YYSYMBOL_singleton = 420,                /* singleton  */
  YYSYMBOL_421_37 = 421,                   /* $@37  */
  YYSYMBOL_assoc_list = 422,               /* assoc_list  */
  YYSYMBOL_assocs = 423,                   /* assocs  */
  YYSYMBOL_assoc = 424,                    /* assoc  */
  YYSYMBOL_operation = 425,                /* operation  */
  YYSYMBOL_operation2 = 426,               /* operation2  */
  YYSYMBOL_operation3 = 427,               /* operation3  */
  YYSYMBOL_dot_or_colon = 428,             /* dot_or_colon  */
  YYSYMBOL_call_op = 429,                  /* call_op  */
  YYSYMBOL_call_op2 = 430,                 /* call_op2  */
  YYSYMBOL_opt_terms = 431,                /* opt_terms  */
  YYSYMBOL_opt_nl = 432,                   /* opt_nl  */
  YYSYMBOL_rparen = 433,                   /* rparen  */
  YYSYMBOL_rbracket = 434,                 /* rbracket  */
  YYSYMBOL_rbrace = 435,                   /* rbrace  */
  YYSYMBOL_trailer = 436,                  /* trailer  */
  YYSYMBOL_term = 437,                     /* term  */
  YYSYMBOL_terms = 438,                    /* terms  */
  YYSYMBOL_none = 439                      /* none  */
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
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

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
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   15486

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  163
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  277
/* YYNRULES -- Number of rules.  */
#define YYNRULES  783
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  1341

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   362


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
       0,     2,     2,     2,     2,     2,     2,     2,     2,    72,
     162,    75,    73,    74,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,   161,   149,     2,     2,     2,   147,   142,     2,
     157,   158,   145,   143,   155,   144,    69,   146,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   137,   160,
     139,   135,   138,   136,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   154,    70,   159,   141,     2,   156,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   152,   140,   153,   150,     2,    89,    90,
      91,    92,    76,    77,    78,    79,    95,    96,    84,    83,
      80,    81,    82,    87,    88,    93,    94,    98,    85,    86,
      97,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
      65,    66,    67,    68,    71,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   148,   151
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,  2193,  2193,  2193,  2221,  2227,  2234,  2241,  2250,  2255,
    2261,  2263,  2279,  2284,  2275,  2297,  2294,  2309,  2315,  2322,
    2329,  2338,  2343,  2342,  2352,  2354,  2360,  2360,  2367,  2374,
    2384,  2393,  2400,  2408,  2416,  2429,  2442,  2453,  2468,  2469,
    2477,  2484,  2497,  2504,  2505,  2514,  2521,  2528,  2536,  2543,
    2550,  2558,  2565,  2578,  2591,  2601,  2602,  2610,  2616,  2621,
    2631,  2634,  2635,  2639,  2643,  2647,  2652,  2651,  2667,  2666,
    2681,  2684,  2696,  2709,  2708,  2728,  2733,  2741,  2741,  2741,
    2747,  2748,  2751,  2752,  2761,  2770,  2779,  2788,  2799,  2806,
    2813,  2820,  2827,  2835,  2843,  2851,  2858,  2867,  2878,  2879,
    2888,  2889,  2898,  2905,  2912,  2919,  2926,  2933,  2940,  2947,
    2954,  2961,  2970,  2971,  2980,  2987,  2996,  3003,  3012,  3019,
    3026,  3033,  3041,  3048,  3056,  3063,  3070,  3080,  3087,  3094,
    3101,  3108,  3115,  3122,  3129,  3136,  3146,  3154,  3157,  3164,
    3171,  3180,  3181,  3182,  3183,  3188,  3191,  3198,  3201,  3208,
    3208,  3218,  3219,  3220,  3221,  3222,  3223,  3224,  3225,  3226,
    3227,  3228,  3229,  3230,  3231,  3232,  3233,  3234,  3235,  3236,
    3237,  3238,  3239,  3240,  3241,  3242,  3243,  3244,  3245,  3246,
    3247,  3250,  3250,  3250,  3251,  3251,  3252,  3252,  3252,  3253,
    3253,  3253,  3253,  3254,  3254,  3254,  3254,  3255,  3255,  3255,
    3256,  3256,  3256,  3256,  3257,  3257,  3257,  3257,  3258,  3258,
    3258,  3258,  3259,  3259,  3259,  3259,  3260,  3260,  3260,  3260,
    3261,  3261,  3264,  3271,  3278,  3285,  3292,  3299,  3306,  3314,
    3322,  3330,  3339,  3348,  3356,  3364,  3372,  3380,  3384,  3388,
    3392,  3396,  3400,  3404,  3408,  3412,  3416,  3420,  3424,  3428,
    3432,  3433,  3437,  3441,  3445,  3449,  3453,  3457,  3461,  3465,
    3469,  3473,  3477,  3482,  3491,  3504,  3517,  3523,  3524,  3532,
    3538,  3539,  3540,  3541,  3544,  3548,  3555,  3561,  3568,  3575,
    3582,  3583,  3587,  3594,  3603,  3608,  3619,  3626,  3638,  3652,
    3653,  3656,  3657,  3658,  3662,  3669,  3678,  3686,  3693,  3701,
    3709,  3713,  3713,  3750,  3757,  3767,  3771,  3778,  3785,  3792,
    3799,  3809,  3813,  3824,  3825,  3829,  3836,  3843,  3852,  3853,
    3854,  3855,  3856,  3857,  3858,  3859,  3860,  3861,  3862,  3870,
    3869,  3884,  3884,  3892,  3900,  3907,  3914,  3921,  3929,  3936,
    3943,  3950,  3957,  3962,  3966,  3970,  3977,  3978,  3986,  3987,
    3998,  4009,  4020,  4032,  4031,  4048,  4047,  4062,  4071,  4116,
    4115,  4134,  4133,  4154,  4153,  4173,  4171,  4192,  4190,  4209,
    4214,  4219,  4224,  4241,  4248,  4257,  4277,  4286,  4296,  4306,
    4315,  4325,  4336,  4347,  4355,  4364,  4373,  4381,  4388,  4394,
    4409,  4416,  4423,  4429,  4436,  4443,  4444,  4445,  4448,  4449,
    4452,  4453,  4465,  4466,  4475,  4476,  4479,  4487,  4496,  4503,
    4512,  4519,  4526,  4533,  4540,  4549,  4557,  4566,  4567,  4570,
    4570,  4572,  4576,  4580,  4584,  4590,  4595,  4600,  4610,  4614,
    4618,  4622,  4626,  4630,  4635,  4639,  4643,  4647,  4651,  4655,
    4659,  4663,  4667,  4673,  4674,  4680,  4691,  4704,  4708,  4717,
    4719,  4723,  4728,  4734,  4740,  4746,  4754,  4745,  4780,  4789,
    4800,  4806,  4805,  4817,  4826,  4840,  4847,  4854,  4863,  4872,
    4880,  4888,  4895,  4903,  4911,  4918,  4925,  4935,  4942,  4951,
    4951,  4968,  4968,  4989,  4997,  5004,  5012,  5021,  5033,  5034,
    5037,  5038,  5040,  5051,  5048,  5066,  5067,  5070,  5071,  5079,
    5089,  5090,  5095,  5103,  5107,  5111,  5117,  5120,  5129,  5132,
    5139,  5142,  5143,  5145,  5146,  5147,  5156,  5165,  5174,  5179,
    5188,  5197,  5206,  5211,  5215,  5219,  5225,  5224,  5234,  5239,
    5246,  5255,  5259,  5268,  5272,  5276,  5279,  5283,  5292,  5296,
    5302,  5309,  5317,  5326,  5327,  5336,  5345,  5349,  5353,  5357,
    5363,  5365,  5374,  5382,  5396,  5397,  5420,  5424,  5430,  5436,
    5437,  5440,  5441,  5450,  5459,  5467,  5475,  5476,  5477,  5478,
    5486,  5496,  5497,  5498,  5499,  5500,  5501,  5502,  5503,  5504,
    5511,  5514,  5524,  5535,  5544,  5553,  5560,  5567,  5576,  5600,
    5603,  5610,  5617,  5620,  5624,  5627,  5635,  5638,  5639,  5642,
    5659,  5660,  5661,  5670,  5680,  5689,  5695,  5696,  5699,  5709,
    5715,  5724,  5726,  5735,  5745,  5751,  5760,  5769,  5779,  5785,
    5795,  5801,  5811,  5821,  5840,  5846,  5856,  5866,  5907,  5910,
    5909,  5926,  5935,  5939,  5925,  5960,  5961,  5964,  5971,  5974,
    5975,  5978,  5988,  5989,  5992,  6002,  6003,  6013,  6014,  6015,
    6016,  6019,  6020,  6021,  6024,  6025,  6026,  6029,  6030,  6031,
    6032,  6033,  6034,  6035,  6038,  6051,  6060,  6067,  6076,  6077,
    6081,  6080,  6090,  6098,  6099,  6107,  6119,  6120,  6120,  6136,
    6140,  6144,  6148,  6152,  6162,  6167,  6172,  6176,  6180,  6184,
    6188,  6192,  6196,  6200,  6204,  6208,  6212,  6216,  6220,  6224,
    6229,  6235,  6248,  6257,  6266,  6275,  6286,  6287,  6295,  6304,
    6312,  6333,  6335,  6348,  6358,  6367,  6378,  6386,  6396,  6403,
    6413,  6420,  6429,  6430,  6433,  6441,  6449,  6459,  6470,  6481,
    6488,  6497,  6504,  6513,  6514,  6517,  6525,  6535,  6536,  6539,
    6547,  6557,  6561,  6567,  6572,  6572,  6598,  6599,  6608,  6610,
    6633,  6644,  6651,  6660,  6668,  6685,  6696,  6697,  6698,  6701,
    6702,  6705,  6706,  6707,  6710,  6711,  6714,  6715,  6718,  6719,
    6722,  6723,  6726,  6727,  6730,  6733,  6736,  6739,  6740,  6743,
    6744,  6751,  6752,  6756
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
  "\"end-of-input\"", "error", "\"invalid token\"", "\"`class'\"",
  "\"`module'\"", "\"`def'\"", "\"`undef'\"", "\"`begin'\"",
  "\"`rescue'\"", "\"`ensure'\"", "\"`end'\"", "\"`if'\"", "\"`unless'\"",
  "\"`then'\"", "\"`elsif'\"", "\"`else'\"", "\"`case'\"", "\"`when'\"",
  "\"`while'\"", "\"`until'\"", "\"`for'\"", "\"`break'\"", "\"`next'\"",
  "\"`redo'\"", "\"`retry'\"", "\"`in'\"", "\"`do'\"",
  "\"`do' for condition\"", "\"`do' for block\"", "\"`do' for lambda\"",
  "\"`return'\"", "\"`yield'\"", "\"`super'\"", "\"`self'\"", "\"`nil'\"",
  "\"`true'\"", "\"`false'\"", "\"`and'\"", "\"`or'\"", "\"`not'\"",
  "\"`if' modifier\"", "\"`unless' modifier\"", "\"`while' modifier\"",
  "\"`until' modifier\"", "\"`rescue' modifier\"", "\"`alias'\"",
  "\"`defined?'\"", "\"`BEGIN'\"", "\"`END'\"", "\"`__LINE__'\"",
  "\"`__FILE__'\"", "\"`__ENCODING__'\"", "\"local variable or method\"",
  "\"method\"", "\"global variable\"", "\"instance variable\"",
  "\"constant\"", "\"class variable\"", "\"label\"", "\"integer literal\"",
  "\"float literal\"", "\"rational literal\"", "\"imaginary literal\"",
  "\"char literal\"", "\"numbered reference\"", "\"back reference\"",
  "\"literal content\"", "tREGEXP_END", "\"dummy end\"", "'.'",
  "\"backslash\"", "\"escaped space\"", "\"escaped horizontal tab\"",
  "\"escaped form feed\"", "\"escaped carriage return\"",
  "\"escaped vertical tab\"", "\"unary+\"", "\"unary-\"", "\"**\"",
  "\"<=>\"", "\"==\"", "\"===\"", "\"!=\"", "\">=\"", "\"<=\"", "\"&&\"",
  "\"||\"", "\"=~\"", "\"!~\"", "\"..\"", "\"...\"", "\"(..\"", "\"(...\"",
  "\"[]\"", "\"[]=\"", "\"<<\"", "\">>\"", "\"&.\"", "\"::\"",
  "\":: at EXPR_BEG\"", "\"operator-assignment\"", "\"=>\"", "\"(\"",
  "\"( arg\"", "\")\"", "\"[\"", "\"{\"", "\"{ arg\"", "\"*\"",
  "\"**arg\"", "\"&\"", "\"->\"", "\"symbol literal\"",
  "\"string literal\"", "\"backtick literal\"", "\"regexp literal\"",
  "\"word list\"", "\"verbatim word list\"", "\"symbol list\"",
  "\"verbatim symbol list\"", "\"terminator\"", "\"'}'\"", "tSTRING_DBEG",
  "tSTRING_DVAR", "tLAMBEG", "tLABEL_END", "tIGNORED_NL", "tCOMMENT",
  "tEMBDOC_BEG", "tEMBDOC", "tEMBDOC_END", "tHEREDOC_BEG", "tHEREDOC_END",
  "k__END__", "tLOWEST", "'='", "'?'", "':'", "'>'", "'<'", "'|'", "'^'",
  "'&'", "'+'", "'-'", "'*'", "'/'", "'%'", "tUMINUS_NUM", "'!'", "'~'",
  "tLAST_TOKEN", "'{'", "'}'", "'['", "','", "'`'", "'('", "')'", "']'",
  "';'", "' '", "'\\n'", "$accept", "program", "$@1", "top_compstmt",
  "top_stmts", "top_stmt", "block_open", "begin_block", "bodystmt", "$@2",
  "$@3", "$@4", "compstmt", "stmts", "stmt_or_begin", "$@5", "allow_exits",
  "k_END", "stmt", "$@6", "command_asgn", "endless_command", "command_rhs",
  "expr", "$@7", "$@8", "def_name", "defn_head", "defs_head", "$@9",
  "expr_value", "expr_value_do", "$@10", "$@11", "command_call",
  "block_command", "cmd_brace_block", "fcall", "command", "mlhs",
  "mlhs_inner", "mlhs_basic", "mlhs_item", "mlhs_head", "mlhs_post",
  "mlhs_node", "lhs", "cname", "cpath", "fname", "fitem", "undef_list",
  "$@12", "op", "reswords", "arg", "endless_arg", "relop", "rel_expr",
  "lex_ctxt", "begin_defined", "after_rescue", "arg_value", "aref_args",
  "arg_rhs", "paren_args", "opt_paren_args", "opt_call_args", "call_args",
  "command_args", "$@13", "block_arg", "opt_block_arg", "args",
  "arg_splat", "mrhs_arg", "mrhs", "primary", "$@14", "$@15", "@16", "@17",
  "$@18", "$@19", "$@20", "$@21", "$@22", "primary_value", "k_begin",
  "k_if", "k_unless", "k_while", "k_until", "k_case", "k_for", "k_class",
  "k_module", "k_def", "k_do", "k_do_block", "k_rescue", "k_ensure",
  "k_when", "k_else", "k_elsif", "k_end", "k_return", "k_yield", "then",
  "do", "if_tail", "opt_else", "for_var", "f_marg", "f_marg_list",
  "f_margs", "f_rest_marg", "f_any_kwrest", "f_eq", "$@23",
  "block_args_tail", "opt_block_args_tail", "excessed_comma",
  "block_param", "opt_block_param", "block_param_def", "opt_bv_decl",
  "bv_decls", "bvar", "max_numparam", "numparam", "lambda", "@24", "$@25",
  "f_larglist", "lambda_body", "$@26", "do_block", "block_call",
  "method_call", "brace_block", "brace_body", "@27", "do_body", "@28",
  "case_args", "case_body", "cases", "p_pvtbl", "p_pktbl", "p_in_kwarg",
  "p_case_body", "$@29", "p_cases", "p_top_expr", "p_top_expr_body",
  "p_expr", "p_as", "p_alt", "p_lparen", "p_lbracket", "p_expr_basic",
  "$@30", "p_args", "p_args_head", "p_args_tail", "p_find", "p_rest",
  "p_args_post", "p_arg", "p_kwargs", "p_kwarg", "p_kw", "p_kw_label",
  "p_kwrest", "p_kwnorest", "p_any_kwrest", "p_value", "p_primitive",
  "p_variable", "p_var_ref", "p_expr_ref", "p_const", "opt_rescue",
  "exc_list", "exc_var", "opt_ensure", "literal", "strings", "string",
  "string1", "xstring", "regexp", "words_sep", "words", "word_list",
  "word", "symbols", "symbol_list", "qwords", "qsymbols", "qword_list",
  "qsym_list", "string_contents", "xstring_contents", "regexp_contents",
  "string_content", "@31", "@32", "@33", "@34", "string_dend",
  "string_dvar", "symbol", "ssym", "sym", "dsym", "numeric",
  "simple_numeric", "nonlocal_var", "user_variable", "keyword_variable",
  "var_ref", "var_lhs", "backref", "superclass", "$@35",
  "f_opt_paren_args", "f_paren_args", "f_arglist", "@36", "args_tail",
  "opt_args_tail", "f_args", "args_forward", "f_bad_arg", "f_norm_arg",
  "f_arg_asgn", "f_arg_item", "f_arg", "f_label", "f_kw", "f_block_kw",
  "f_block_kwarg", "f_kwarg", "kwrest_mark", "f_no_kwarg", "f_kwrest",
  "f_opt", "f_block_opt", "f_block_optarg", "f_optarg", "restarg_mark",
  "f_rest_arg", "blkarg_mark", "f_block_arg", "opt_f_block_arg",
  "singleton", "$@37", "assoc_list", "assocs", "assoc", "operation",
  "operation2", "operation3", "dot_or_colon", "call_op", "call_op2",
  "opt_terms", "opt_nl", "rparen", "rbracket", "rbrace", "trailer", "term",
  "terms", "none", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-1088)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-784)

#define yytable_value_is_error(Yyn) \
  ((Yyn) == YYTABLE_NINF)

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
   -1088,   141,  4978, -1088, -1088, -1088, -1088, -1088, 10235, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, 11055, 11055, -1088, -1088,
   -1088, -1088,  6200, -1088, -1088, -1088, -1088,   527, 10081,    50,
     222, -1088, -1088, -1088, -1088,  5576,  6356, -1088, -1088,  5732,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, 12507, 12507,
   12507, 12507,   427,  8462,  8622, 11539, 11781, 10537, -1088,  9927,
   -1088, -1088, -1088,   336,   336,   336,   336,  1285, 12628, 12507,
   -1088,   299, -1088, -1088,  1317, -1088,   485,    37,    37, -1088,
   -1088,   142,   480,   398, -1088,   406, 13112, -1088,   444,  1944,
     957,    57,   659, -1088, 10934, 10934, -1088, -1088,  9096, 13231,
   13350, 13469,  9772, 11055,  6824, -1088,   593,    97, -1088, -1088,
     476, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088, -1088, -1088,    85,   561, -1088,   495,   645, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088,   446, -1088, -1088,
   -1088, -1088, -1088, -1088,   496, 12507,   581,  8622, 12507, 12507,
   12507, -1088, 12507,    37,    37, -1088,   562,  5702,   599, -1088,
   -1088,   556, -1088,   763,    69,    71,   632,   108,   610, -1088,
   -1088, 11176, -1088, -1088, 11055,  9651, -1088, 12749,   823, -1088,
     606, -1088,  8782, -1088, -1088, -1088, -1088, -1088,   616,   142,
   -1088,   769, -1088,   626,   762,  5390,  5390,   667, -1088,  8462,
     661,   299, -1088,  1317,    50,   710, -1088, -1088,   688,   240,
     369, -1088,   599,   699,   369, -1088,    50,   794,  1285, 13588,
     702,   702,   718, -1088,   793,   830,   843,   853, -1088, -1088,
      73, -1088, -1088,   687,   858,   852, -1088,   724,   724,   724,
     724,   801, -1088, -1088, -1088, -1088, -1088, -1088, -1088,  9379,
     786, 10934, 10934, 10934, 10934, -1088, 12749, 12749,  2194,   819,
     835, -1088,  2194, -1088,   836, -1088, -1088, -1088, -1088,   866,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088,  8462, 10675,   834,
   -1088, -1088, 12507, 12507, 12507, 12507, 12507, -1088, -1088, 12507,
   12507, 12507, 12507, 12507, 12507, 12507, 12507, -1088, 12507, -1088,
   -1088, 12507, 12507, 12507, 12507, 12507, 12507, 12507, 12507, 12507,
   12507, -1088, -1088,  4666, 11055,  5066,  7608, -1088,   485,   169,
     169,  8340, 10934,  8340,   299, -1088,   832,   942, -1088, -1088,
     879,   979,    91,   113,   127,   697,   732, 10934,   493, -1088,
     871,   888, -1088, -1088, -1088, -1088,   119,   347,   371,   378,
     466,   569,   576,   693,   700, -1088, -1088, -1088, -1088,   767,
   -1088,  9530, -1088, -1088, -1088, 15330, -1088, -1088, -1088, -1088,
   -1088, -1088,   318, -1088, -1088, -1088,   698,   870,   878, -1088,
   12507, 11297, -1088, -1088, 13944, 11055, 14043, -1088, -1088, 11660,
   -1088, 12507,    50, -1088,   863,    50,   868, -1088, -1088,    83,
     896, -1088, -1088, -1088, -1088, -1088, 10235, -1088, -1088, 12507,
     875, 14142, 14043, -1088,   222,    50, -1088, -1088,  9218,   882,
     897, -1088, 11539, -1088, -1088, 11781, -1088, -1088, -1088,   606,
     914, -1088, -1088,   906, -1088, 13588, 14241, 11055, 14340, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088,   930,   117,   953,   331, 12507, -1088, -1088,  8942, -1088,
   -1088, -1088, -1088, -1088, 10813, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088, -1088, -1088, -1088,  1150, -1088, -1088, -1088, -1088,
   -1088,   908, -1088, -1088,    50, -1088, -1088, -1088,   945, -1088,
     910, 12507, -1088,   927,   158, -1088, -1088, -1088,   931,  1033,
     932,  1041, -1088, 12870,  7608,   299, 12870,  7608,   941, -1088,
   -1088, -1088,   156, -1088,   156, 11902,    50, 13588,   944, -1088,
   11902, -1088,   762,  4760,  4760,  4760,  4760,  5858,  4273,  4760,
    4760,  5390,  5390,   631,   631, -1088,  5546,   642,   642,  1120,
     326,   326,   762,   762,   762,  1458,  1458,  6980,  5888,  7292,
    6044, -1088, -1088,   616, -1088,    50,   955,   808, -1088,   929,
   -1088, -1088,  6512,   156, -1088, -1088,  7730,  1084,  8096,   156,
     320,   156,  1081,  1094,   136, 14439, 11055, 14538, -1088, -1088,
   -1088,   914, -1088, -1088, -1088, 14637, 11055, 14736,  7608, 12749,
   -1088, -1088, -1088,    50, -1088, -1088, -1088,  1852, 12628, 12628,
   10235, 12507, 12991, 12991, 12507, -1088,   599, -1088, -1088,   610,
    5420,  6668,    50,   363,   412, 12507, 12507, -1088, -1088, 11418,
   -1088, 11660, -1088, -1088, -1088, 12749,  5702, -1088,   416,   616,
     616, 12507, -1088,   595, -1088, -1088,   369, 13588,   906,   486,
     657,    50,   264,   552, -1088, -1088,  1132, -1088,    67, -1088,
     336, -1088, -1088,    67,   336, -1088,   762,   959, -1088,  1150,
    1274, -1088,   961,    50,   963, -1088,    36, -1088, -1088, -1088,
   12507,   989,  2194, -1088, -1088,   461, -1088, -1088, -1088,  2194,
   -1088, -1088,  2479, -1088, -1088,   527,  1070, -1088,  5702,  1082,
     156, -1088,  1070,  1082,   156, -1088, -1088,   972, -1088, -1088,
   -1088, -1088, -1088, 12507, -1088,   973,   974,  1092, -1088, -1088,
     906, 13588, -1088, -1088,  1096,  1008,  2581, -1088, -1088, -1088,
     934,   428, -1088, -1088,  1009, -1088, -1088, -1088, -1088,   866,
     992,   943, 11297, -1088, -1088, -1088, -1088,   866, -1088, -1088,
    1140,   671, -1088,  1141, -1088, -1088, -1088, -1088, -1088, -1088,
    1094,   156, -1088, 12023,   156,   233,   254,    50,   166,   175,
    8340,   299, 10934,  7608,   807,   657, -1088,    50,   156,    83,
   10389, -1088,    97,   480, -1088,  6947, -1088, -1088, -1088, -1088,
   -1088,   527, -1088, -1088, -1088,   492, -1088, -1088,    50,  1002,
      83, -1088, -1088, -1088,   563,  1615, -1088, -1088, -1088, -1088,
     724, -1088,   724,   724,   724, -1088,    50, -1088,  1150, -1088,
    1188, -1088, -1088, -1088, -1088, -1088,  1004,  1006, -1088,  1111,
     908,  1016, -1088,  1017, -1088,  1016, 12870, -1088, -1088, -1088,
   -1088, -1088, -1088, -1088,  1018, 12144, -1088,   906, -1088, -1088,
   -1088, 14835, 11055, 14934, -1088, -1088, 12507, 12628, 12628,  1024,
   -1088, -1088, -1088, 12628, 12628, -1088, -1088, 12265,  1141, -1088,
   -1088, -1088,  8340, 10934,   156, -1088, -1088,   156, -1088, -1088,
     156, -1088, 12507, -1088,   109, -1088,   214,   156,  7608,   299,
     156, -1088, -1088, -1088, -1088, -1088, -1088, 12991, 12507, 12507,
   -1088, 12507, 12507, -1088, 11660, -1088,  2194, -1088, -1088,  5294,
   -1088, -1088,  1026,  1029,  2194, -1088,  2479, -1088, -1088,  2479,
   -1088,  2479, -1088, -1088,  1070,  1082, 12507, 12507,  1045,  1045,
   12507,  1036, 10813, 10813, 12628, 12507,  7136,  7448,    50,   500,
     504,  4476,  4476,  5702, -1088, -1088, -1088, -1088, -1088, 12628,
   -1088, -1088, -1088, -1088,   973, -1088,  1091, -1088,  1185, -1088,
   -1088,   169, -1088, -1088, -1088, -1088, -1088, 12386,  7852, -1088,
     156, -1088, -1088, 12507,    50,    98,    95,  1188,  1188,  1016,
    1044,  1016,  1016,  5702,  5702,  1874,  8942, -1088, -1088,  7608,
    1018, -1088, -1088,  5702,   511, -1088, -1088, -1088,  2448,  2448,
     741, -1088,  4166,    14,  1148, -1088,  1078, -1088, -1088,   309,
   -1088,  1063, -1088, -1088, -1088,  1053, -1088,  1055, -1088,  1686,
   -1088, -1088, -1088, -1088,   682, -1088, -1088, -1088,    45, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,   447, -1088,
   -1088, -1088, 13707,   169, -1088, -1088,  8340, -1088, -1088,  8218,
    7730,  4476, 12507, -1088,   608, -1088,  1056,  1058, -1088,  8942,
   -1088, -1088, -1088, -1088,  1029, -1088,  2479, -1088, -1088, -1088,
     908, -1088,    50,  1087,   945,  1074, 13826, -1088,  1077, -1088,
    1080,  1099, -1088, -1088, -1088, -1088, -1088, -1088, -1088,  1686,
     324,    50, 13900, -1088,    50,  1100, -1088, -1088,  1086, -1088,
   -1088,   902, -1088, 10934, -1088,  1197, 13900,  1686,  1686,   958,
    1156,  2448,  2448,   741,   274,   739,  4476,  4476, -1088,  1195,
   -1088,   948,   188,   192,   200,  7608, -1088, -1088,   671,   169,
     831, -1088, -1088, -1088, -1088, -1088,  1280,  7608,  1115,  1016,
   -1088,  1129, -1088, 13826,  1539, -1088, -1088,  1217,   951,   461,
   -1088,  1539, -1088,  1472, -1088,    35, -1088,  1156,  1121,  1123,
   -1088, -1088, -1088, -1088, -1088,    50, -1088, -1088,  1124, -1088,
    1128, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088,    50,    50,    50,    50,    50,    50,   218, 15033,
   11055, 15132,  1140,  1185, -1088, -1088, 10934, 10934, -1088,   467,
   -1088, -1088,   156, -1088, -1088, -1088,   951, -1088,  1134,  1137,
   -1088, 15231, -1088,   908,  1138, -1088,  1146,  1138, -1088,  1686,
   -1088,   958, -1088,  1686, 13900,   758, -1088, -1088, -1088, -1088,
   -1088, -1088,   121,   201,    50,   223,   238, -1088, -1088,  7974,
   -1088, -1088,  1280, -1088, -1088,  1539, -1088,  1472, -1088,  1118,
    1130, -1088,  1472, -1088,  1472, -1088, -1088,  1149,    50,  1149,
   -1088, -1088,   253,   417, -1088,  1138,  1155,  1138,  1138,  1686,
   -1088, -1088, -1088, -1088, -1088,  1472, -1088, -1088, -1088,  1138,
   -1088
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       2,     0,     0,     1,    44,   381,   382,   383,     0,   374,
     375,   376,   379,    24,    24,    24,   369,   370,   371,   372,
     393,   394,   301,   658,   657,   659,   660,   772,     0,   772,
       0,   783,   662,   661,   663,   756,   758,   652,   651,   757,
     653,   647,   648,   649,   650,   600,   668,   669,     0,     0,
       0,     0,     0,     0,     0,   783,   783,   110,   455,   622,
     622,   624,   626,     0,     0,     0,     0,     0,     0,     0,
       3,   770,     6,    24,     8,    38,    43,   677,   677,    61,
      81,   301,    80,     0,    98,     0,   102,   112,     0,    70,
     250,   266,     0,   329,     0,     0,    77,    77,     0,     0,
       0,     0,     0,   338,   301,   348,    82,   346,   318,   319,
     599,   601,   320,   321,   322,   324,   323,   325,   598,   639,
     640,   597,   645,   656,   664,   665,   326,     0,   327,    85,
       5,   191,   202,   192,   215,   188,   208,   198,   197,   218,
     219,   213,   196,   195,   190,   216,   220,   221,   200,   189,
     203,   207,   209,   201,   194,   210,   217,   212,   211,   204,
     214,   199,   187,   206,   205,   186,   193,   184,   185,   181,
     182,   183,   141,   143,   142,   176,   177,   172,   154,   155,
     156,   163,   160,   162,   157,   158,   178,   179,   164,   165,
     169,   173,   159,   161,   151,   152,   153,   166,   167,   168,
     170,   171,   174,   175,   180,   146,   148,    31,   144,   145,
     147,   377,   378,   380,     0,   752,     0,     0,   312,   755,
     304,   622,     0,   677,   677,   296,     0,   279,   307,    96,
     300,   783,   308,     0,   664,   665,     0,   327,   783,   748,
      97,   783,   474,    93,     0,   772,   773,     0,     0,    26,
     783,    10,     0,     9,    25,   276,   369,   370,   475,     0,
     244,     0,   338,   341,   245,   235,   236,   335,    22,     0,
       0,   770,    19,    21,   772,   100,    18,   331,     0,   772,
     772,   280,     0,     0,   772,   746,   772,     0,     0,     0,
     677,   677,   108,   373,     0,   118,   119,   126,   453,   642,
       0,   641,   643,     0,     0,     0,   606,   609,   618,   614,
     620,   646,    65,   256,   257,   779,   780,     4,   781,     0,
       0,     0,     0,     0,     0,   783,     0,     0,   700,     0,
     676,   365,   700,   674,     0,   367,   384,   479,   468,    86,
     481,   345,   385,   481,   464,   783,   114,     0,   106,   103,
     783,    68,     0,     0,     0,     0,     0,   272,   273,     0,
       0,     0,     0,   233,   234,     0,     0,    66,     0,   270,
     271,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   766,   767,     0,   783,     0,     0,    76,    75,     0,
       0,     0,     0,     0,   770,   355,   771,     0,   405,   404,
       0,     0,   664,   665,   327,   136,   137,     0,     0,   139,
     672,     0,   664,   665,   327,   363,   211,   204,   214,   199,
     181,   182,   183,   141,   142,   744,    72,    71,   743,     0,
      95,   772,    94,   769,   768,     0,   347,   602,   783,   783,
     149,   751,   335,   311,   754,   303,     0,     0,     0,   783,
       0,     0,   297,   306,     0,   783,     0,   783,   783,     0,
     298,   701,   772,   292,   783,   772,   783,   291,   302,   772,
       0,   344,    64,    28,    30,    29,     0,   783,   277,     0,
       0,     0,     0,   783,     0,   772,   333,    17,     0,    99,
       0,   336,   778,   777,   281,   778,   283,   337,   747,     0,
     125,   646,   116,   111,   676,     0,     0,   783,     0,   454,
     628,   644,   631,   629,   623,   603,   604,   625,   605,   627,
     607,     0,     0,     0,     0,     0,   782,     7,     0,    32,
      33,    34,    35,   278,     0,    62,    63,   707,   704,   703,
     702,   705,   713,   722,   701,     0,   734,   723,   738,   737,
     733,   783,   724,   699,   772,   683,   706,   708,   709,   711,
     685,   715,   720,   783,   726,   418,   417,   731,   685,   736,
     685,   740,   682,     0,     0,     0,     0,     0,     0,   453,
     479,    87,     0,   453,     0,     0,   772,     0,   104,   115,
       0,   492,   242,   249,   251,   252,   253,   260,   261,   254,
     255,   231,   232,   258,   259,   492,   772,   246,   247,   248,
     237,   238,   239,   240,   241,   274,   275,   756,   758,   757,
     760,   473,   759,   301,   471,   772,   783,   756,   758,   757,
     760,   472,   301,     0,   783,   396,     0,   395,     0,     0,
       0,     0,   353,     0,   335,     0,   783,     0,    77,   361,
     136,   137,   138,   670,   359,     0,   783,     0,     0,     0,
     764,   765,    73,   772,   340,   756,   757,   301,     0,     0,
       0,     0,     0,     0,     0,   750,   309,   305,   310,   783,
     756,   757,   772,   756,   757,     0,     0,   749,   286,   293,
     288,   295,   343,   774,    27,     0,   262,    11,   334,     0,
     783,     0,    23,   101,    20,   332,   772,     0,   109,   761,
     124,   772,   756,   757,    24,   632,     0,   608,     0,   611,
       0,   616,   613,     0,     0,   617,   243,     0,    36,     0,
     416,   408,   410,   772,   413,   406,     0,   681,   742,   675,
       0,     0,     0,   692,   714,     0,   680,   558,   725,     0,
     695,   735,     0,   697,   739,   772,    52,    55,   267,   264,
       0,   678,    53,   265,     0,   477,   454,     0,   391,   392,
     478,   454,   463,   312,    39,   314,     0,    42,   313,   113,
     107,     0,    60,    45,    58,     0,   284,   307,   222,    40,
       0,   327,   490,   490,     0,   783,   783,   479,   470,    90,
       0,   476,   293,   783,   783,   290,   469,    88,   289,   330,
     783,   783,   397,   783,   351,   399,    78,   398,   352,   492,
       0,     0,   388,     0,     0,   761,   334,   772,   756,   757,
       0,     0,     0,     0,   136,   137,   140,   772,     0,   772,
       0,   339,   465,    83,    46,   284,   223,    54,   230,   150,
     753,   772,   299,   783,   783,   476,   783,   783,   772,   783,
     772,   229,   282,   117,   476,   700,   633,   630,   637,   638,
     610,   612,   619,   615,   621,    37,   772,   415,     0,   710,
       0,   741,   727,   420,   684,   712,   685,   685,   721,   726,
     783,   685,   732,   685,   709,   685,     0,   783,   783,   366,
     368,    24,    84,    24,   317,     0,   783,   105,   783,   783,
     783,     0,   783,     0,   491,   491,     0,     0,     0,     0,
      91,   775,   783,     0,     0,    89,   386,   783,    15,   589,
     390,   389,     0,     0,     0,   400,   402,     0,    79,   490,
       0,   357,     0,   483,     0,   356,   476,     0,     0,     0,
       0,   476,   364,   745,    74,   466,   467,     0,     0,     0,
     783,     0,     0,   287,   294,   342,   700,   456,   459,     0,
     407,   409,   411,   414,     0,   688,     0,   690,   679,     0,
     696,     0,   693,   698,    57,   269,     0,     0,   783,   783,
     312,   315,     0,     0,     0,     0,   756,   757,   772,   756,
     757,     0,     0,   263,    51,   227,    50,   228,    92,     0,
      48,   225,    49,   226,   590,   591,   783,   592,   783,    12,
     403,     0,   349,   350,   491,   354,   484,     0,     0,   358,
       0,   671,   360,     0,   772,     0,     0,     0,     0,   685,
     685,   685,   685,    56,   268,   772,     0,   444,   443,     0,
     316,    41,    59,   285,   476,   581,   587,   554,     0,     0,
       0,   491,   772,   491,   542,   622,     0,   580,    69,   500,
     506,   508,   510,   504,   503,   538,   505,   547,   550,   553,
     559,   560,   549,   513,   561,   514,   566,   567,   568,   571,
     572,   573,   574,   575,   577,   576,   578,   579,   557,    67,
      47,   224,     0,     0,   594,   387,     0,    16,   596,     0,
       0,     0,     0,   485,   783,   362,     0,   447,   461,     0,
     457,   636,   635,   634,   412,   689,     0,   686,   691,   694,
     783,   442,   772,     0,   709,   426,   717,   718,   783,   729,
     426,   426,   424,   480,   482,   569,   570,   137,   585,     0,
     530,   772,   531,   535,   772,     0,   525,   783,     0,   528,
     541,     0,   582,     0,   583,     0,   501,     0,     0,   548,
     552,   564,   565,     0,   491,   491,     0,     0,   556,     0,
     593,     0,   664,   665,   327,     0,   595,    13,   783,     0,
     497,   486,   488,   489,   487,   458,     0,     0,     0,   685,
     423,     0,   445,     0,   427,   435,   433,     0,   716,     0,
     422,     0,   438,     0,   440,   772,   523,   545,   533,   532,
     524,   536,   526,   776,   555,   772,   507,   502,   538,   509,
     539,   543,   622,   551,   546,   562,   563,   586,   512,   522,
     511,   518,   772,   772,   772,   772,   772,   772,   335,     0,
     783,     0,   783,   783,   401,   493,     0,     0,   451,   772,
     449,   452,     0,   460,   687,   446,   728,   425,   426,   426,
     335,     0,   719,   783,   426,   730,   426,   426,   529,     0,
     537,     0,   584,     0,     0,     0,   515,   516,   517,   519,
     520,   521,   761,   334,   772,   756,   757,   588,    14,     0,
     498,   499,     0,   448,   462,     0,   430,     0,   432,   761,
     334,   421,     0,   439,     0,   436,   441,   534,   772,   539,
     540,   544,   476,   783,   450,   426,   426,   426,   426,     0,
     527,   495,   496,   494,   431,     0,   428,   434,   437,   426,
     429
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
   -1088, -1088, -1088,  1028, -1088,   976, -1088,   812,  -540, -1088,
   -1088, -1088,   -40, -1088,   817, -1088,     8, -1088,     9, -1088,
     -49,  -510,  -494,   -26, -1088, -1088,   471,  2739,  3102, -1088,
     -89,   -62, -1088, -1088,   -60, -1088,  -620,  1245,   -14,  1213,
    -140,    -7,   -37, -1088,  -425,    44,  3555,  -392,  1214,   -28,
     -10, -1088, -1088,     2, -1088,  4276,  -500,  1226, -1088,    99,
     840,   340,  1165, -1088,   617,   -17,   653,  -372,    46,   -39,
   -1088,  -381,  -212,    12,  -401, -1088,  -551,   -13, -1088, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088,  1125, -1088, -1088,
   -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088, -1088,
   -1088, -1088, -1088,   394, -1088,    32,  1883,  2367,  -363, -1088,
     137,  -789, -1088,  -766,  -767,   604,   463,  -900,   190, -1088,
     304,   131, -1088, -1088,   361, -1088,  -894, -1088,    49,  -419,
      33,  1221, -1088, -1088, -1088, -1088, -1088,   509, -1088, -1088,
     -82,  -478, -1088,  1011, -1088, -1088,  -741, -1088,  -662,  -838,
    -495,    39, -1088, -1088, -1088,  -885,  -659, -1088, -1088, -1088,
   -1088,   196, -1088,  -256, -1088,  -637,  -656,  -968,  -245, -1024,
    -721, -1088,   195, -1088, -1088,  -854,   198, -1088,  -468,   203,
   -1088, -1088, -1088,   122, -1088, -1088,   112,  1427,  1787, -1088,
    1261,  1866,  2213,    28,  2585, -1088,   850,  2724, -1088,  2820,
    2902, -1088, -1088,   -56, -1088, -1088,  -248, -1088, -1088, -1088,
   -1088, -1088, -1088,    25, -1088, -1088, -1088, -1088,    18,   -52,
    3202,    -2,  1273,  3772,  2516, -1088, -1088,    62,   641,    41,
   -1088,  -287,   267,  -285,  -187,  -817,  -336,  -311,  -699,  -599,
    -370,   633,   172, -1088, -1088,  -663, -1088,  -709,  -641, -1087,
     178,   637, -1088,  -617, -1088,   110,  -531, -1088, -1088, -1088,
       3,  -399,   170,  -345, -1088, -1088,   -91, -1088,   -27,   433,
     262,   276,    68,  -177,   -23,    43,   103
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,     2,    70,    71,    72,   252,   253,   633,  1109,
    1253,  1018,   634,   271,   272,   484,   211,    73,   273,   476,
      75,   756,   783,    76,   605,   591,   426,   223,   224,   840,
     389,   391,   392,   938,    79,    80,   581,   259,    82,    83,
     274,    84,    85,    86,   503,    87,   226,   409,   410,   205,
     206,   207,   670,   620,   209,    89,   759,   379,    90,   533,
     479,   534,   228,   278,   788,   621,   806,   462,   463,   243,
     244,   230,   452,   626,   232,   777,   778,    91,   386,   490,
     820,   643,   833,   831,   658,   574,   577,   261,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   340,   343,
     927,  1106,   823,   932,   933,   770,   262,   263,   636,   816,
     934,   935,   401,   731,   732,   733,   734,   551,   740,   741,
    1267,  1205,  1206,  1132,  1046,  1047,  1116,  1259,  1260,   509,
     714,   105,   298,  1035,   967,  1120,  1197,   344,   106,   107,
     341,   578,   579,   582,   583,   944,   824,  1194,   914,  1001,
     792,   821,  1299,  1333,  1189,  1068,  1217,  1070,  1071,  1176,
    1177,  1072,  1281,  1151,  1152,  1153,  1074,  1075,  1230,  1155,
    1076,  1077,  1078,  1079,  1080,   552,  1082,  1083,  1084,  1085,
    1086,  1087,  1088,   928,  1016,  1103,  1107,   108,   109,   110,
     111,   112,   113,   307,   114,   521,   718,   115,   523,   116,
     117,   522,   524,   300,   304,   305,   514,   716,   715,   866,
     969,  1123,   867,   118,   119,   301,   120,   121,   122,   123,
     234,   235,   126,   236,   237,   654,   832,   329,   330,   331,
     332,   884,   743,   554,   555,   556,   557,   894,   559,   560,
     561,   562,  1137,  1138,   563,   564,   565,   566,   567,  1139,
    1140,   568,   569,   570,   571,   572,   737,   429,   659,   283,
     466,   239,   129,   699,   624,   662,   657,   435,   317,   470,
     471,   801,  1159,   494,   637,   396,   255
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     125,   385,   225,   225,   303,   242,   390,   302,   312,   394,
     208,    74,   625,   270,   277,   434,   652,   558,   249,   238,
     238,   558,   212,   213,   937,   436,   460,   638,   231,   231,
     208,   299,   746,   210,   760,   393,   890,   764,   623,   789,
     632,   553,   339,   885,   293,   553,   275,   575,   318,   349,
     678,   125,   125,   210,   465,   296,   517,   519,   280,   284,
     687,   208,   229,   240,   338,   432,   762,   279,   388,   388,
     677,   395,   388,   293,   427,   318,   763,  1002,   677,   940,
     708,   320,   889,   682,   296,   311,   293,   293,   293,   225,
     667,   678,   308,   309,   310,  1121,   687,   403,   413,   413,
     413,   292,   767,   496,   208,   130,   238,   498,   892,   623,
     793,   632,   971,   973,   319,   231,  -127,  1099,   838,   335,
     326,   327,   635,   336,  1275,   887,  -373,  1118,  1219,   485,
     254,   915,   893,   510,  -131,   711,  1165,   700,  -128,   510,
     334,     3,   456,  1173,  1231,  1130,   548,  1081,  1081,   430,
     891,  1133,  -135,   895,  -373,  -373,   276,   276,   281,   285,
     766,  -134,   780,   700,   771,   446,   768,  -772,   336,  -666,
     482,  -667,  -783,   763,   844,   847,   246,   270,   549,   920,
     333,   333,   635,   720,  1218,  -666,  1111,   925,  -658,   512,
     513,  -130,   747,   511,   328,   512,   513,   246,  1228,  1174,
    -132,  -127,  1175,   508,  -127,  -128,  -128,   586,   458,   735,
     748,  -373,   246,  -135,  -133,   125,  1122,  -658,  1275,   469,
    -127,   472,  1119,  1149,   769,  1157,  1190,   225,   306,   270,
     225,  -134,   529,   530,   531,   532,  -130,   721,  1201,  -129,
    -118,   242,   338,  -135,   487,   246,  -118,   238,   318,   337,
     125,  -132,   502,   464,   460,  1231,   231,  1081,  -131,  1231,
    1321,    74,   275,   836,  1027,   335,  -129,   125,  -119,   315,
    1124,   316,   971,   719,   827,   719,   293,  1024,  -756,  -133,
     885,  -131,  -126,  -131,   837,   447,   448,   296,   678,  -121,
     468,  -125,   687,   950,   337,   388,   388,   388,   388,   241,
     535,   536,   700,   640,  1130,  1321,   501,   270,   677,   647,
     677,  1130,   700,  1130,   488,  1081,  1320,   125,   649,   919,
     276,  -121,  1081,  1081,   939,  1192,   333,   333,    74,   315,
    -123,   316,   335,   892,   453,   293,  1238,  1240,  1098,  1098,
     275,   453,  1069,  1069,   467,   125,   296,   815,  -127,   478,
    -127,   639,  -128,   641,  -128,   130,   907,  1040,  -757,   978,
    -135,  -133,  -135,  -133,  1073,  1073,   388,   642,   631,  -120,
     225,   318,   276,  1193,   251,  1039,  1015,  1041,  -134,  1261,
    -134,   388,  1042,  -130,   125,  -130,   984,   630,  -122,   125,
    -756,   125,   588,   735,   877,   492,   985,   724,  -132,  -121,
    -132,   678,   246,  1150,   352,  1130,  1154,  1130,  1030,  -124,
    1165,  -757,  1130,  -129,  1130,  -129,  -657,   225,   483,  -121,
    1170,   677,  -121,  1004,  1006,  1165,  -121,  1081,  1141,  1010,
    1012,   558,   931,  -772,   238,  1130,   246,   630,   558,   631,
    -659,   225,   819,   231,   585,  -657,  1135,  -660,  1098,   590,
     276,   725,  1069,  -134,   679,  1244,  1247,   985,   630,   315,
     247,   316,   250,   856,  1166,   631,   694,   852,   502,  -659,
     871,   376,   377,   378,  1073,   871,  -660,   663,   208,  -545,
     315,   747,   316,   267,   630,  1261,   125,   467,   727,   276,
    1215,   631,   293,   225,   276,   706,   276,   306,  -130,  1178,
    1273,   210,   858,   296,   678,   885,  1098,  1150,   342,  1144,
     630,  -122,   857,  1098,  1098,  1100,   854,  1150,  1150,   542,
    1243,  1246,   326,   327,   495,   774,   125,  1199,   439,   862,
     784,   246,   125,   345,  1331,  -662,   489,   668,   669,   543,
     998,   782,   735,   728,   735,   650,   889,  -132,   674,   651,
     502,  -133,   761,   622,   558,   622,   685,   686,   467,   757,
    1318,   346,   757,  -135,  -662,   687,   623,   453,   632,   453,
     547,   548,   125,  -757,   293,   125,   478,  -123,   553,   350,
     968,  1028,   701,   677,   799,   296,   830,  1269,  -120,    60,
    1145,  1146,   960,   807,  1276,   438,   811,   776,   813,  1200,
     803,   440,   776,   549,   804,   622,   798,  1210,   784,   784,
     467,   922,  1274,   885,  1277,   805,   772,   817,  1098,   782,
     782,  -122,  1302,   931,   622,   822,   622,  -129,   843,   246,
     631,   276,   225,   839,   125,  -130,   125,   442,  -661,  -132,
     631,  -122,   225,  -756,  -122,  -663,  -129,   852,  -122,   630,
     805,   622,   622,   245,   738,   558,   125,  1262,  1110,   630,
     849,  -667,   381,   558,   868,   809,   738,  -661,  1148,   860,
     863,   814,   208,   818,  -663,  1136,   622,   276,   622,   553,
     276,  1034,   798,   805,   245,   930,   931,  -123,  1326,   246,
     382,   433,   859,   664,   293,   210,  -128,   449,  -120,   913,
     450,   735,   735,  1235,  1236,   296,  1325,  -123,  1327,   352,
    -123,   451,   493,   493,  -123,  1328,  -119,   493,  -120,   499,
     352,  -120,   865,  -756,   688,  -120,  -334,   690,   381,   453,
     -99,   692,   457,   810,  1134,   808,  1339,   365,   366,   276,
    1185,   276,  1311,   949,   502,   439,   870,   703,   872,   467,
    -113,   873,   874,   510,  -334,  -334,   382,   383,  -757,   467,
     955,   276,  -654,   477,   510,   459,  -654,   483,   293,  -655,
     808,  1171,  1172,   241,   374,   375,   376,   377,   378,   296,
    -135,  1237,   453,   431,   373,   374,   375,   376,   377,   378,
     947,  -654,   899,   650,  -654,  -654,   900,  1147,  -655,   901,
    -126,  -655,  -134,   808,   903,   859,   388,   515,   948,   512,
     513,  -334,   427,   384,  -757,   622,   739,   622,   515,   486,
     512,   513,  -125,   671,   510,   622,  1255,   622,   125,  -655,
    -655,   125,   381,  -761,  1136,   750,   660,   753,   381,  1136,
     352,  1136,   208,  1136,  1021,   -98,   881,   491,   779,  -756,
     500,  -654,   497,   941,  -756,   881,   945,   784,   784,   328,
     382,   454,   381,   784,   784,   661,   382,   481,   782,   782,
     952,  1256,  1257,   505,   782,   782,  -761,   473,  1294,   525,
     512,   513,   757,  1224,  -757,   520,  -655,   474,   475,  -757,
     382,   506,  1020,  1134,   917,   918,   631,  -772,   225,  -664,
    1134,   246,   923,   924,  -761,  -761,   700,   388,   803,   988,
    1227,   989,  -665,   929,   936,   630,   936,   455,   510,   518,
    1242,  1245,  -327,   455,   510,   841,  1031,  -664,  -664,  1036,
     125,   504,   504,   276,   784,  1136,   276,  1136,   528,   776,
    -665,  -665,  1136,  -130,  1136,   782,   125,   507,   381,   784,
    -327,  -327,   958,   959,   573,   961,   962,   381,   855,  -761,
     782,  -761,   453,  -121,  -756,  1136,  1022,   125,   510,  1023,
    -673,   576,  1025,   580,   512,   513,   382,   645,   516,  1029,
     512,   513,  1032,  -335,  -664,   382,   655,   864,  1114,   589,
     125,   125,   526,   738,  1134,   879,   510,  -665,   644,  1097,
    1097,  1051,  1052,   381,   648,   672,  1143,  -327,   994,  1161,
     653,  -335,  -335,   673,  1164,   467,  1057,   381,   689,   510,
     381,  1009,   515,   691,   512,   513,   125,  1224,   697,   804,
    1017,   382,   911,   646,  1317,   276,   543,  -113,  1319,   794,
     357,   358,   656,   922,   125,   382,  1249,   125,   382,  1271,
     717,   276,   512,   513,   693,   705,  1097,  1097,   800,  1033,
    1097,   707,  1115,   736,  -132,   742,  1186,   547,  -335,  1187,
    1188,  1232,   276,   722,  1225,   512,   513,  1097,  -129,  1198,
    -419,   622,   745,   622,  -123,   751,   749,   752,   912,   293,
    1251,  1048,  1048,   754,   765,   369,   370,   812,  -120,   781,
    1183,   953,  1250,   946,   125,   656,   819,   125,   125,  1097,
     802,   822,   875,   951,   897,   800,   878,   125,   880,  1104,
     963,  1108,   965,   293,   883,   902,   898,    92,  -307,   905,
    1162,   276,    37,    38,   413,    40,   906,   388,   970,   493,
     908,   233,   233,   909,   800,  1252,   916,  1097,   926,   276,
    1097,   921,   276,   975,   977,  1142,   931,   964,   980,   974,
     982,   976,   983,   748,  1097,  1097,  1097,  1300,  1301,  1097,
    1097,   979,   981,  -311,  1097,  1097,  1285,  1008,    92,    92,
    1163,  1037,   294,   125,  1038,  1045,    37,    38,   896,    40,
     293,  -309,  1102,   233,  1105,   125,    46,    47,   352,  1126,
    1160,   413,   537,  1167,   538,   539,   540,   541,  1168,   276,
    1169,   294,   276,   276,  1195,   365,   366,   936,  1196,   233,
     233,   282,   276,   233,   400,   411,   411,  1202,   233,  1204,
     388,   388,  1209,   738,   631,  1211,   225,   986,   987,  1223,
     537,   738,   538,   539,   540,   541,   992,    81,   993,  1055,
     995,  1248,   729,   630,  1213,  1221,  1222,  1165,   730,  1323,
     800,    81,    81,   374,   375,   376,   377,   378,  1263,  1265,
     800,  1212,  1214,  1270,  1054,  -756,  1279,  1097,  1280,  1283,
     480,  1097,  1097,  1284,   957,   846,   848,  -757,   276,  1305,
     729,   936,  1307,  1312,  1304,   527,   702,   125,    81,    81,
     276,  1314,   846,   848,  1329,   704,  1125,  1127,  1128,  1129,
    1335,   954,   398,    81,  1142,   415,   380,   695,   861,   881,
     842,  1142,  1019,  1142,  1203,  1254,   537,  1097,   538,   539,
     540,   541,  1258,   876,   538,   539,   540,   541,  1156,    81,
      81,   972,    92,    81,    41,    42,    43,    44,    81,  1131,
    1049,  1324,   956,   467,   584,   929,  1108,   321,   322,   323,
     324,   325,  1332,  1229,  1233,  1298,   233,  1234,  1226,   233,
     233,   437,   233,   723,  1297,   428,   738,    92,   888,   886,
     441,  1272,  1268,   443,   444,   445,  1330,     0,     0,     0,
       0,     0,     0,     0,    92,     0,     0,     0,     0,  1306,
    1308,     0,   276,     0,     0,  1313,     0,  1315,  1316,     0,
       0,     0,     0,     0,   294,  1142,     0,  1142,     0,   622,
       0,   622,  1142,     0,  1142,     0,   936,  1216,     0,     0,
    1220,   800,     0,     0,     0,     0,     0,  1241,     0,     0,
       0,   622,     0,     0,    92,  1142,   233,   233,   233,   233,
    1239,   233,   233,     0,     0,     0,  1334,  1336,  1337,  1338,
       0,     0,    81,     0,     0,     0,  1264,  1117,     0,     0,
    1340,     0,    92,   294,     0,     0,     0,  1278,  1117,     0,
       0,     0,     0,     0,     0,     0,    81,  1282,     0,    81,
      81,     0,    81,     0,     0,   800,  1158,    81,     0,     0,
       0,     0,     0,     0,  1286,  1287,  1288,     0,     0,   233,
       0,    92,     0,     0,    81,     0,    92,   233,    92,     0,
       0,  1289,  1290,  1291,   537,     0,   538,   539,   540,   541,
     542,     0,   233,     0,  1005,  1007,   352,     0,     0,     0,
    1011,  1013,     0,     0,     0,     0,     0,     0,     0,     0,
     543,     0,     0,   365,   366,     0,   233,     0,     0,     0,
       0,     0,     0,     0,    81,  1117,    81,    81,    81,    81,
    1322,    81,    81,     0,   545,  1005,  1007,     0,  1011,  1013,
     233,   547,   548,     0,   800,     0,     0,   800,     0,     0,
       0,   537,    81,   538,   539,   540,   541,   542,   371,   372,
     373,   374,   375,   376,   377,   378,     0,   800,     0,     0,
       0,     0,     0,    92,   549,   675,   676,   543,     0,     0,
       0,     0,     0,     0,   282,     0,  1101,     0,     0,    81,
     294,    81,   233,     0,     0,     0,    81,    81,    81,     0,
       0,   545,     0,     0,     0,     0,     0,   546,   547,   548,
    1101,     0,    81,    92,     0,     0,     0,   676,     0,    92,
     282,     0,     0,     0,     0,     0,     0,   537,     0,   538,
     539,   540,   541,   542,     0,     0,    81,     0,   800,   800,
     800,   549,     0,     0,   550,     0,     0,     0,     0,     0,
       0,     0,  1303,   543,     0,     0,     0,     0,   233,    92,
      81,   233,    92,     0,     0,     0,     0,   544,     0,     0,
     233,     0,   294,     0,     0,   790,     0,   545,     0,    23,
      24,    25,    26,   546,   547,   548,   744,   800,     0,     0,
       0,     0,     0,    81,     0,    32,    33,    34,  1055,     0,
       0,     0,  1056,     0,     0,    41,    42,    43,    44,    45,
     775,  1158,    81,     0,     0,   787,     0,   549,     0,     0,
     550,    92,     0,    92,     0,     0,     0,     0,     0,     0,
       0,   233,   966,    81,     0,     0,     0,  1058,  1059,    81,
       0,   233,     0,    92,   233,  1060,     0,     0,  1061,     0,
       0,  1062,  1063,   790,   790,     0,     0,    58,    59,    60,
      61,    62,    63,    64,    65,    66,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    81,    81,
     233,    81,    81,     0,     0,     0,     0,  1066,     0,     0,
      81,     0,   294,     0,   288,    81,   850,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -783,     0,   676,     0,   282,     0,     0,     0,
    -783,  -783,  -783,     0,     0,  -783,  -783,  -783,     0,  -783,
       0,     0,     0,     0,     0,     0,     0,  -783,  -783,  -783,
       0,    81,     0,    81,     0,   103,     0,     0,     0,  -783,
    -783,    81,  -783,  -783,  -783,  -783,  -783,     0,     0,   103,
     103,    81,     0,    81,    81,   882,   294,     0,     0,     0,
       0,     0,     0,    81,    81,     0,     0,     0,     0,     0,
    -783,  -783,     0,     0,     0,     0,   537,     0,   538,   539,
     540,   541,   542,     0,     0,     0,   103,   103,   904,     0,
      81,     0,     0,     0,     0,     0,     0,     0,     0,  -783,
    -783,   103,   543,     0,     0,    92,     0,   233,    92,     0,
       0,     0,     0,     0,     0,     0,     0,   676,     0,   351,
       0,     0,     0,  -783,     0,     0,   545,   103,   103,     0,
       0,   103,   546,   547,   548,     0,   103,     0,   943,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -783,  -783,     0,     0,     0,   241,
    -783,     0,  -783,     0,  -783,     0,   549,     0,     0,   550,
       0,   233,   352,   353,   354,   355,   356,   357,   358,   359,
     360,   361,   362,   363,   364,     0,   246,   233,     0,   365,
     366,     0,   790,   790,     0,   367,     0,     0,   790,   790,
       0,     0,     0,     0,     0,     0,     0,    92,   233,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     991,     0,     0,    92,     0,    81,     0,    81,    81,     0,
     368,     0,   369,   370,   371,   372,   373,   374,   375,   376,
     377,   378,  1014,     0,    92,     0,     0,     0,     0,     0,
     103,     0,     0,     0,     0,     0,     0,  1026,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    92,    92,   790,
       0,     0,     0,     0,   103,     0,     0,   103,   103,   282,
     103,     0,     0,     0,   790,   103,     0,     0,     0,     0,
       0,    81,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   103,    92,     0,  1050,     0,    81,     0,     0,
       0,     0,    81,    81,     0,     0,     0,     0,    81,    81,
       0,    92,     0,     0,    92,     0,     0,    81,    81,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  1113,    81,     0,     0,     0,     0,     0,     0,
       0,     0,   103,     0,   103,   103,   103,   103,     0,   103,
     103,     0,     0,     0,    81,     0,     0,     0,     0,     0,
       0,     0,  1067,  1067,     0,     0,     0,  1181,     0,     0,
     103,    92,     0,     0,    92,    92,     0,    81,    81,    81,
       0,     0,     0,     0,    92,     0,   537,     0,   538,   539,
     540,   541,   542,     0,    81,     0,     0,     0,     0,     0,
       0,  1208,     0,     0,     0,     0,     0,   103,     0,   103,
       0,     0,   543,    81,   103,   103,   103,  1191,     0,  1067,
    1067,     0,     0,  1067,     0,     0,   544,     0,   233,     0,
     103,    81,     0,     0,    81,     0,   545,     0,     0,     0,
    1067,     0,   546,   547,   548,     0,     0,     0,     0,     0,
      92,     0,     0,     0,   103,     0,     0,     0,     0,     0,
       0,     0,    92,     0,     0,     0,     0,     0,  1266,     0,
       0,     0,  1067,     0,     0,     0,   549,     0,   103,   550,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    81,     0,     0,    81,    81,     0,     0,     0,     0,
       0,     0,     0,     0,    81,     0,     0,     0,     0,   104,
    1067,   103,     0,  1067,     0,   233,     0,     0,     0,     0,
       0,   233,   233,   104,   104,     0,     0,  1067,  1067,  1067,
     103,     0,  1067,  1067,     0,     0,     0,  1067,  1067,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    81,     0,
       0,   103,     0,     0,     0,     0,     0,   103,     0,     0,
     104,   104,     0,     0,    92,     0,     0,     0,  1089,  1089,
      81,     0,     0,     0,     0,   104,     0,     0,     0,     0,
       0,     0,    81,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   103,   103,     0,   103,
     103,   104,   104,     0,     0,   104,     0,     0,   103,     0,
     104,     0,     0,   103,     0,     0,     0,     0,     0,     0,
       0,    23,    24,    25,    26,  1089,  1089,     0,     0,  1089,
       0,     0,     0,     0,     0,    81,     0,    32,    33,    34,
    1067,    81,    81,     0,  1067,  1067,  1089,    41,    42,    43,
      44,    45,     0,     0,     0,     0,     0,     0,   128,   103,
       0,   103,     0,     0,     0,     0,     0,     0,     0,   103,
       0,   537,     0,   538,   539,   540,   541,   542,  1089,   103,
       0,   103,   103,     0,    81,     0,     0,     0,     0,     0,
    1067,   103,   103,     0,     0,     0,     0,   543,     0,    58,
      59,    60,    61,    62,    63,    64,    65,    66,     0,   128,
     128,   544,     0,   297,     0,     0,  1089,     0,   103,  1089,
       0,   545,     0,     0,   104,     0,     0,     0,   547,   548,
       0,     0,     0,  1089,  1089,  1089,   288,     0,  1089,  1089,
       0,     0,   297,  1089,  1089,     0,     0,     0,   104,     0,
       0,   104,   104,     0,   104,   404,   414,   414,     0,   104,
       0,   549,     0,     0,     0,   910,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   104,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   352,
     353,   354,   355,   356,   357,   358,   359,   360,   361,   362,
     363,   364,     0,     0,     0,     0,   365,   366,     0,     0,
       0,     0,     0,     0,     0,     0,   104,     0,   104,   104,
     104,   104,     0,   104,   104,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  1089,     0,     0,     0,
    1089,  1089,     0,   103,   104,   103,   103,   368,     0,   369,
     370,   371,   372,   373,   374,   375,   376,   377,   378,     0,
       0,     0,     0,   128,     0,     0,  -279,     0,     0,     0,
       0,    77,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   104,     0,   104,     0,     0,  1089,     0,   104,   104,
     104,     0,     0,     0,     0,     0,     0,     0,   128,     0,
       0,     0,     0,     0,   104,     0,     0,     0,     0,   103,
       0,     0,     0,     0,     0,   128,     0,     0,  1090,  1090,
       0,     0,    77,    77,     0,   103,   290,     0,   104,     0,
     103,   103,     0,     0,     0,   297,   103,   103,     0,     0,
       0,     0,     0,     0,     0,   103,   103,     0,     0,     0,
       0,     0,   104,     0,     0,   290,     0,     0,     0,     0,
       0,   103,     0,     0,     0,   128,     0,     0,   290,   290,
     290,     0,     0,     0,     0,  1090,  1090,     0,     0,  1090,
       0,     0,   103,     0,     0,   104,     0,     0,     0,     0,
       0,     0,     0,   128,   297,     0,  1090,  1091,  1091,     0,
       0,     0,     0,     0,   104,   103,   103,   103,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   103,     0,     0,   104,     0,     0,  1090,     0,
       0,   104,   128,     0,     0,     0,     0,   128,     0,   128,
       0,   103,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  1091,  1091,     0,     0,  1091,   103,
       0,     0,   103,     0,     0,     0,  1090,     0,     0,  1090,
     104,   104,     0,   104,   104,  1091,     0,     0,     0,     0,
       0,     0,   104,  1090,  1090,  1090,    77,   104,  1090,  1090,
       0,     0,     0,  1090,  1090,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  1091,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   103,
       0,    77,   103,   103,     0,     0,     0,     0,     0,     0,
       0,     0,   103,   104,   128,   104,     0,     0,    77,     0,
       0,     0,     0,   104,     0,  1091,     0,     0,  1091,     0,
       0,   297,     0,   104,     0,   104,   104,     0,   290,     0,
       0,     0,  1091,  1091,  1091,   104,   104,  1091,  1091,     0,
       0,     0,  1091,  1091,   128,     0,   103,     0,     0,     0,
     128,     0,     0,     0,     0,     0,     0,     0,    77,     0,
       0,     0,   104,     0,     0,     0,  1090,     0,   103,     0,
    1090,  1090,     0,     0,     0,     0,     0,     0,     0,     0,
     103,     0,     0,     0,     0,     0,    77,   290,     0,     0,
     128,     0,     0,   128,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   297,    78,     0,   791,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  1090,     0,     0,     0,
       0,     0,     0,     0,     0,    77,     0,     0,     0,     0,
      77,     0,    77,   103,     0,     0,     0,     0,     0,   103,
     103,     0,     0,     0,     0,  1091,     0,     0,     0,  1091,
    1091,     0,   128,     0,   128,    78,    78,     0,     0,   291,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   128,     0,     0,     0,     0,     0,
       0,     0,   103,     0,   791,   791,     0,     0,   291,     0,
       0,     0,     0,     0,     0,  1091,     0,   104,     0,   104,
     104,   291,   291,   291,   124,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  1092,  1092,     0,     0,     0,     0,
       0,     0,     0,   297,     0,     0,     0,    77,     0,     0,
       0,     0,   869,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   290,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   124,   124,     0,     0,   295,
       0,     0,     0,   104,     0,     0,     0,    77,     0,     0,
       0,  1092,  1092,    77,     0,  1092,     0,     0,     0,   104,
       0,     0,     0,     0,   104,   104,     0,     0,   295,     0,
     104,   104,  1092,     0,     0,     0,     0,   297,     0,   104,
     104,   402,   412,   412,   412,     0,     0,     0,     0,     0,
       0,     0,     0,    77,     0,   104,    77,     0,     0,    78,
       0,     0,     0,     0,  1092,     0,   290,     0,     0,    77,
       0,     0,     0,     0,     0,     0,   104,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   128,     0,     0,   128,
       0,     0,     0,     0,    78,     0,     0,     0,     0,   104,
     104,   104,  1092,     0,     0,  1092,     0,     0,     0,     0,
       0,    78,     0,     0,     0,    77,   104,    77,     0,  1092,
    1092,  1092,     0,     0,  1092,  1092,     0,     0,     0,  1092,
    1092,   291,     0,     0,     0,   104,     0,    77,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    77,    77,     0,
       0,     0,     0,   104,     0,     0,   104,     0,     0,   124,
       0,    78,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   791,   791,     0,     0,     0,     0,   791,
     791,     0,     0,     0,     0,     0,   290,     0,   128,    78,
     291,     0,     0,     0,   124,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   128,     0,     0,     0,     0,     0,
       0,   124,     0,   104,     0,     0,   104,   104,     0,     0,
       0,     0,     0,     0,     0,   128,   104,     0,    78,     0,
       0,   295,  1092,    78,     0,    78,  1092,  1092,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   128,   128,
     791,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     290,   124,     0,     0,     0,   791,     0,     0,     0,     0,
     104,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  1092,     0,   128,     0,     0,     0,     0,   124,
     295,     0,   104,     0,     0,     0,     0,    88,     0,     0,
       0,     0,   128,     0,   104,   128,     0,     0,     0,    77,
       0,     0,    77,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  1093,  1093,   124,     0,
      78,     0,     0,   124,     0,   124,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   291,    88,    88,
       0,     0,     0,     0,     0,     0,     0,   104,  1184,     0,
       0,     0,   128,   104,   104,   128,   128,     0,     0,     0,
      78,     0,     0,     0,     0,   128,    78,     0,     0,     0,
       0,     0,     0,  1093,  1093,     0,     0,  1093,     0,     0,
       0,     0,   414,     0,   399,     0,    77,    77,     0,     0,
       0,     0,    77,    77,  1093,     0,   104,     0,     0,     0,
       0,    77,     0,     0,     0,     0,    78,     0,     0,    78,
       0,     0,     0,     0,     0,     0,     0,    77,     0,   291,
     124,     0,    78,     0,     0,     0,  1093,     0,     0,     0,
       0,   128,     0,     0,     0,     0,     0,   295,    77,     0,
       0,     0,     0,   128,     0,     0,     0,     0,     0,   414,
       0,     0,     0,     0,     0,  1094,  1094,     0,     0,     0,
     124,    77,    77,    77,  1093,     0,   124,  1093,    78,     0,
      78,     0,     0,     0,     0,     0,     0,     0,    77,     0,
       0,  1093,  1093,  1093,     0,     0,  1093,  1093,     0,     0,
      78,  1093,  1093,     0,     0,     0,     0,    77,     0,     0,
      78,    78,    88,     0,   127,     0,   124,     0,     0,   124,
       0,     0,  1094,  1094,     0,    77,  1094,     0,    77,   295,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  1094,     0,     0,     0,    88,     0,   291,
       0,     0,     0,     0,     0,   128,     0,     0,     0,     0,
       0,  1095,  1095,     0,    88,   127,   127,     0,     0,     0,
       0,     0,     0,     0,     0,  1094,     0,     0,   124,     0,
     124,   290,     0,     0,     0,    77,     0,     0,    77,    77,
       0,     0,     0,     0,     0,     0,     0,     0,    77,     0,
     124,     0,     0,     0,  1093,     0,     0,     0,  1093,  1093,
       0,     0,     0,  1094,    88,   290,  1094,     0,  1095,  1095,
       0,     0,  1095,   291,     0,     0,     0,     0,     0,     0,
    1094,  1094,  1094,     0,     0,  1094,  1094,     0,     0,  1095,
    1094,  1094,    88,  1096,  1096,     0,     0,     0,     0,   295,
       0,     0,     0,     0,  1093,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    77,     0,     0,     0,     0,     0,
       0,  1095,    78,     0,     0,    78,    77,     0,     0,     0,
       0,    88,   290,     0,     0,     0,    88,     0,    88,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    1096,  1096,     0,     0,  1096,     0,     0,     0,     0,  1095,
       0,     0,  1095,     0,     0,     0,     0,     0,     0,     0,
       0,  1096,     0,   295,     0,     0,  1095,  1095,  1095,   127,
       0,  1095,  1095,     0,     0,     0,  1095,  1095,     0,     0,
       0,     0,     0,  1094,     0,     0,     0,  1094,  1094,     0,
       0,     0,     0,  1096,     0,     0,     0,     0,     0,    78,
      78,     0,     0,     0,   127,    78,    78,     0,     0,     0,
       0,     0,   124,     0,    78,   124,     0,     0,    77,     0,
       0,   127,     0,    88,     0,     0,     0,     0,     0,     0,
      78,  1096,     0,  1094,  1096,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  1096,  1096,
    1096,    78,     0,  1096,  1096,     0,     0,     0,  1096,  1096,
       0,     0,     0,    88,     0,     0,     0,     0,     0,    88,
       0,   127,     0,     0,    78,    78,    78,     0,     0,  1095,
       0,     0,     0,  1095,  1095,     0,     0,     0,     0,     0,
       0,    78,     0,     0,     0,     0,     0,     0,     0,   127,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    88,
      78,     0,    88,     0,   124,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   785,     0,     0,    78,  1095,
     124,    78,     0,     0,     0,     0,     0,     0,   127,     0,
       0,     0,     0,   127,     0,   127,     0,     0,     0,     0,
       0,   124,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  1096,     0,     0,     0,  1096,  1096,     0,     0,     0,
       0,    88,     0,    88,   124,   124,     0,     0,     0,    23,
      24,    25,    26,     0,   291,     0,     0,     0,    78,     0,
       0,    78,    78,    88,     0,    32,    33,    34,  1055,     0,
       0,    78,  1056,   785,   785,    41,    42,    43,    44,    45,
     124,  1096,     0,     0,     0,     0,     0,     0,   291,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   124,     0,
       0,   124,     0,     0,     0,     0,     0,  1058,  1059,     0,
     127,     0,     0,     0,     0,  1060,     0,     0,  1061,     0,
       0,  1062,  1063,     0,  1064,     0,     0,    58,    59,    60,
      61,    62,    63,    64,    65,    66,     0,    78,     0,     0,
       0,     0,   227,   227,     0,     0,     0,     0,     0,    78,
     127,     0,     0,     0,  1182,   291,   127,  1066,   124,     0,
       0,   124,   124,     0,   288,     0,     0,     0,     0,     0,
       0,   124,     0,     0,   260,   264,   265,   266,   246,     0,
       0,   227,   227,     0,     0,     0,     0,     0,   412,     0,
       0,     0,     0,     0,   313,   314,   127,     0,     0,   127,
       0,   352,   353,   354,   355,   356,   357,   358,   359,     0,
     361,   362,   127,     0,     0,     0,     0,     0,   365,   366,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   227,
       0,     0,     0,     0,     0,    88,     0,   124,    88,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   124,
       0,    78,     0,     0,     0,   412,     0,     0,   127,     0,
     127,   369,   370,   371,   372,   373,   374,   375,   376,   377,
     378,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     127,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     127,   127,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   785,   785,     0,     0,     0,     0,   785,   785,
       0,     0,     0,     0,     0,     0,     0,    88,     0,     0,
       0,   227,     0,     0,   227,   227,   227,     0,   313,     0,
       0,   124,     0,    88,     0,     0,     0,     0,     0,    23,
      24,    25,    26,     0,     0,     0,     0,   227,     0,     0,
     227,     0,     0,     0,    88,    32,    33,    34,  1055,     0,
       0,     0,  1056,     0,  1057,    41,    42,    43,    44,    45,
       0,     0,     0,     0,     0,     0,     0,    88,    88,   785,
       0,     0,     0,     0,   543,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   785,     0,     0,  1058,  1059,     0,
       0,     0,     0,     0,     0,  1060,     0,     0,  1061,     0,
       0,  1062,  1063,    88,  1064,   547,     0,    58,    59,  1065,
      61,    62,    63,    64,    65,    66,     0,     0,     0,     0,
       0,    88,   127,     0,    88,   127,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  1066,     0,     0,
       0,     0,     0,     0,   288,     0,     0,     0,   592,   593,
     594,   595,   596,     0,     0,   597,   598,   599,   600,   601,
     602,   603,   604,     0,   606,     0,     0,   607,   608,   609,
     610,   611,   612,   613,   614,   615,   616,  1180,     0,     0,
     227,    88,     0,     0,    88,    88,     0,     0,     0,     0,
       0,     0,     0,     0,    88,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   127,
     127,     0,     0,     0,     0,   127,   127,     0,     0,     0,
       0,     0,     0,     0,   127,     0,     0,   227,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   617,   618,
     127,     0,   619,     0,     0,     0,   227,   227,     0,     0,
       0,   227,     0,     0,     0,   227,     0,   266,     0,     0,
      88,   127,   175,   176,   177,   178,   179,   180,   181,   182,
     183,     0,    88,   184,   185,   696,     0,     0,     0,   186,
     187,   188,   189,     0,   127,   127,   127,     0,   227,     0,
       0,   227,     0,     0,   190,   191,     0,     0,     0,     0,
       0,   127,     0,   227,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     127,   726,     0,     0,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,     0,   202,   203,     0,   127,     0,
       0,   127,   204,   241,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   227,   352,  -784,
    -784,  -784,  -784,   357,   358,     0,     0,  -784,  -784,   758,
       0,     0,   758,     0,    88,   365,   366,     0,     0,     0,
       0,   227,     0,     0,     0,     0,   786,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   127,     0,
       0,   127,   127,     0,     0,     0,     0,     0,     0,     0,
       0,   127,     0,     0,     0,     0,     0,     0,   369,   370,
     371,   372,   373,   374,   375,   376,   377,   378,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   227,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   227,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   845,   845,     0,   227,   758,   758,
     845,     0,     0,     0,     0,     0,     0,   127,     0,     0,
       0,   845,   845,     0,     0,   227,     0,   227,     0,   127,
       0,     0,     0,     0,     0,     0,     0,   845,  -783,     4,
       0,     5,     6,     7,     8,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,   227,    27,     0,     0,
       0,     0,     0,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,     0,    41,    42,    43,
      44,    45,    46,    47,     0,     0,     0,     0,     0,   227,
       0,     0,     0,     0,    48,    49,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    50,
      51,   127,     0,     0,     0,     0,     0,    52,   227,     0,
      53,    54,     0,    55,    56,     0,    57,     0,     0,    58,
      59,    60,    61,    62,    63,    64,    65,    66,     0,   227,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   627,   628,
       0,     0,   629,     0,     0,     0,    67,    68,    69,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -783,     0,
    -783,     0,   175,   176,   177,   178,   179,   180,   181,   182,
     183,     0,     0,   184,   185,     0,     0,     0,     0,   186,
     187,   188,   189,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   758,     0,   190,   191,     0,     0,     0,     0,
       0,   227,     0,     0,     0,     0,     0,     0,   227,     0,
       0,     0,  1003,   845,   845,     0,     0,     0,     0,   845,
     845,     0,     0,   227,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,     0,   202,   203,     0,   227,     0,
       0,     0,   204,   241,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   758,   845,   845,     0,   845,   845,     0,
     227,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  1043,  1044,     0,     0,   227,     0,     0,     0,
     845,  1053,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   845,     0,     0,     0,     0,
       0,     0,     0,     0,  -783,     4,     0,     5,     6,     7,
       8,     9,     0,   227,     0,    10,    11,     0,     0,   845,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,     0,    28,
      29,   268,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,     0,    41,    42,    43,    44,    45,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      48,    49,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    50,    51,     0,   227,     0,
       0,     0,     0,    52,     0,     0,    53,    54,     0,    55,
      56,     0,    57,     0,     0,    58,    59,    60,    61,    62,
      63,    64,    65,    66,     0,  -783,     0,     0,     0,     0,
    -761,     0,     0,     0,     0,     0,     0,     0,  -761,  -761,
    -761,     0,     0,  -761,  -761,  -761,     0,  -761,     0,     0,
       0,     0,    67,    68,    69,  -761,  -761,  -761,  -761,  -761,
       0,     0,     0,     0,  -783,     0,  -783,  -761,  -761,     0,
    -761,  -761,  -761,  -761,  -761,     0,     0,     0,   352,   353,
     354,   355,   356,   357,   358,   359,   360,   361,   362,  -784,
    -784,     0,     0,     0,     0,   365,   366,     0,  -761,  -761,
       0,     0,     0,     0,     0,     0,     0,     0,  -761,  -761,
    -761,  -761,  -761,  -761,  -761,  -761,  -761,  -761,  -761,  -761,
    -761,     0,     0,     0,     0,  -761,  -761,  -761,  -761,     0,
     853,  -761,     0,     0,     0,     0,   227,  -761,   369,   370,
     371,   372,   373,   374,   375,   376,   377,   378,     0,     0,
       0,  -761,     0,     0,  -761,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -131,  -761,  -761,  -761,  -761,
    -761,  -761,  -761,  -761,  -761,  -761,  -761,  -761,     0,     0,
       0,     0,  -761,  -761,  -761,  -761,  -654,     0,  -761,  -761,
    -761,     0,  -761,     0,  -654,  -654,  -654,     0,     0,  -654,
    -654,  -654,     0,  -654,     0,     0,     0,     0,     0,     0,
       0,  -654,     0,  -654,  -654,  -654,     0,     0,     0,     0,
       0,     0,     0,  -654,  -654,     0,  -654,  -654,  -654,  -654,
    -654,     0,     0,     0,   352,   353,   354,   355,   356,   357,
     358,   359,   360,   361,   362,   363,   364,     0,     0,     0,
       0,   365,   366,     0,  -654,  -654,     0,     0,     0,     0,
       0,     0,     0,     0,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,  -654,  -654,  -654,  -654,     0,     0,     0,
       0,  -654,  -654,  -654,  -654,     0,  -654,  -654,     0,     0,
       0,     0,   368,  -654,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,     0,     0,     0,  -654,     0,     0,
    -654,     0,     0,     0,     0,     0,     0,     0,   246,     0,
       0,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,  -654,     0,     0,     0,     0,     0,  -654,
    -654,  -654,  -655,     0,  -654,  -654,  -654,     0,  -654,     0,
    -655,  -655,  -655,     0,     0,  -655,  -655,  -655,     0,  -655,
       0,     0,     0,     0,     0,     0,     0,  -655,     0,  -655,
    -655,  -655,     0,     0,     0,     0,     0,     0,     0,  -655,
    -655,     0,  -655,  -655,  -655,  -655,  -655,     0,     0,     0,
     352,   353,   354,   355,   356,   357,   358,   359,   360,   361,
     362,   363,   364,     0,     0,     0,     0,   365,   366,     0,
    -655,  -655,     0,     0,     0,     0,     0,     0,     0,     0,
    -655,  -655,  -655,  -655,  -655,  -655,  -655,  -655,  -655,  -655,
    -655,  -655,  -655,     0,     0,     0,     0,  -655,  -655,  -655,
    -655,     0,  -655,  -655,     0,     0,     0,     0,   368,  -655,
     369,   370,   371,   372,   373,   374,   375,   376,   377,   378,
       0,     0,     0,  -655,     0,     0,  -655,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -655,  -655,  -655,
    -655,  -655,  -655,  -655,  -655,  -655,  -655,  -655,  -655,  -655,
       0,     0,     0,     0,     0,  -655,  -655,  -655,  -762,     0,
    -655,  -655,  -655,     0,  -655,     0,  -762,  -762,  -762,     0,
       0,  -762,  -762,  -762,     0,  -762,     0,     0,     0,     0,
       0,     0,     0,  -762,  -762,  -762,  -762,  -762,     0,     0,
       0,     0,     0,     0,     0,  -762,  -762,     0,  -762,  -762,
    -762,  -762,  -762,     0,     0,     0,   352,   353,   354,   355,
     356,   357,   358,     0,     0,   361,   362,     0,     0,     0,
       0,     0,     0,   365,   366,     0,  -762,  -762,     0,     0,
       0,     0,     0,     0,     0,     0,  -762,  -762,  -762,  -762,
    -762,  -762,  -762,  -762,  -762,  -762,  -762,  -762,  -762,     0,
       0,     0,     0,  -762,  -762,  -762,  -762,     0,     0,  -762,
       0,     0,     0,     0,     0,  -762,   369,   370,   371,   372,
     373,   374,   375,   376,   377,   378,     0,     0,     0,  -762,
       0,     0,  -762,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -762,  -762,  -762,  -762,  -762,  -762,
    -762,  -762,  -762,  -762,  -762,  -762,     0,     0,     0,     0,
    -762,  -762,  -762,  -762,  -763,     0,  -762,  -762,  -762,     0,
    -762,     0,  -763,  -763,  -763,     0,     0,  -763,  -763,  -763,
       0,  -763,     0,     0,     0,     0,     0,     0,     0,  -763,
    -763,  -763,  -763,  -763,     0,     0,     0,     0,     0,     0,
       0,  -763,  -763,     0,  -763,  -763,  -763,  -763,  -763,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -763,  -763,     0,     0,     0,     0,     0,     0,
       0,     0,  -763,  -763,  -763,  -763,  -763,  -763,  -763,  -763,
    -763,  -763,  -763,  -763,  -763,     0,     0,     0,     0,  -763,
    -763,  -763,  -763,     0,     0,  -763,     0,     0,     0,     0,
       0,  -763,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -763,     0,     0,  -763,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -763,  -763,  -763,  -763,  -763,  -763,  -763,  -763,  -763,  -763,
    -763,  -763,     0,     0,     0,     0,  -763,  -763,  -763,  -763,
    -475,     0,  -763,  -763,  -763,     0,  -763,     0,  -475,  -475,
    -475,     0,     0,  -475,  -475,  -475,     0,  -475,     0,     0,
       0,     0,     0,     0,     0,  -475,  -475,  -475,  -475,     0,
       0,     0,     0,     0,     0,     0,     0,  -475,  -475,     0,
    -475,  -475,  -475,  -475,  -475,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -475,  -475,
       0,     0,     0,     0,     0,     0,     0,     0,  -475,  -475,
    -475,  -475,  -475,  -475,  -475,  -475,  -475,  -475,  -475,  -475,
    -475,     0,     0,     0,     0,  -475,  -475,  -475,  -475,     0,
       0,  -475,     0,     0,     0,     0,     0,  -475,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -475,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -475,     0,  -475,  -475,
    -475,  -475,  -475,  -475,  -475,  -475,  -475,  -475,     0,     0,
       0,     0,  -475,  -475,  -475,  -475,  -328,   241,  -475,  -475,
    -475,     0,  -475,     0,  -328,  -328,  -328,     0,     0,  -328,
    -328,  -328,     0,  -328,     0,     0,     0,     0,     0,     0,
       0,  -328,     0,  -328,  -328,  -328,     0,     0,     0,     0,
       0,     0,     0,  -328,  -328,     0,  -328,  -328,  -328,  -328,
    -328,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -328,  -328,     0,     0,     0,     0,
       0,     0,     0,     0,  -328,  -328,  -328,  -328,  -328,  -328,
    -328,  -328,  -328,  -328,  -328,  -328,  -328,     0,     0,     0,
       0,  -328,  -328,  -328,  -328,     0,     0,  -328,     0,     0,
       0,     0,     0,  -328,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -328,     0,     0,
    -328,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -328,  -328,  -328,  -328,  -328,  -328,  -328,  -328,
    -328,  -328,  -328,  -328,     0,     0,     0,     0,     0,  -328,
    -328,  -328,  -783,     0,  -328,  -328,  -328,     0,  -328,     0,
    -783,  -783,  -783,     0,     0,  -783,  -783,  -783,     0,  -783,
       0,     0,     0,     0,     0,     0,     0,  -783,  -783,  -783,
    -783,     0,     0,     0,     0,     0,     0,     0,     0,  -783,
    -783,     0,  -783,  -783,  -783,  -783,  -783,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -783,  -783,     0,     0,     0,     0,     0,     0,     0,     0,
    -783,  -783,  -783,  -783,  -783,  -783,  -783,  -783,  -783,  -783,
    -783,  -783,  -783,     0,     0,     0,     0,  -783,  -783,  -783,
    -783,     0,     0,  -783,     0,     0,     0,     0,     0,  -783,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -783,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -783,     0,
    -783,  -783,  -783,  -783,  -783,  -783,  -783,  -783,  -783,  -783,
       0,     0,     0,     0,  -783,  -783,  -783,  -783,  -334,   241,
    -783,  -783,  -783,     0,  -783,     0,  -334,  -334,  -334,     0,
       0,  -334,  -334,  -334,     0,  -334,     0,     0,     0,     0,
       0,     0,     0,  -334,     0,  -334,  -334,     0,     0,     0,
       0,     0,     0,     0,     0,  -334,  -334,     0,  -334,  -334,
    -334,  -334,  -334,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -334,  -334,     0,     0,
       0,     0,     0,     0,     0,     0,  -334,  -334,  -334,  -334,
    -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,     0,
       0,     0,     0,  -334,  -334,  -334,  -334,     0,   854,  -334,
       0,     0,     0,     0,     0,  -334,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -334,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -133,  -334,     0,  -334,  -334,  -334,  -334,
    -334,  -334,  -334,  -334,  -334,  -334,     0,     0,     0,     0,
     797,  -334,  -334,  -334,  -341,     0,  -334,  -334,  -334,     0,
    -334,     0,  -341,  -341,  -341,     0,     0,  -341,  -341,  -341,
       0,  -341,     0,     0,     0,     0,     0,     0,     0,  -341,
       0,  -341,  -341,     0,     0,     0,     0,     0,     0,     0,
       0,  -341,  -341,     0,  -341,  -341,  -341,  -341,  -341,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -341,  -341,     0,     0,     0,     0,     0,     0,
       0,     0,  -341,  -341,  -341,  -341,  -341,  -341,  -341,  -341,
    -341,  -341,  -341,  -341,  -341,     0,     0,     0,     0,  -341,
    -341,  -341,  -341,     0,     0,  -341,     0,     0,     0,     0,
       0,  -341,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -341,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -341,     0,  -341,  -341,  -341,  -341,  -341,  -341,  -341,  -341,
    -341,  -341,     0,     0,     0,     0,     0,  -341,  -341,  -341,
    -761,   431,  -341,  -341,  -341,     0,  -341,     0,  -761,  -761,
    -761,   910,     0,     0,  -761,  -761,     0,  -761,     0,     0,
       0,     0,     0,     0,     0,  -761,  -761,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -761,  -761,     0,
    -761,  -761,  -761,  -761,  -761,   352,   353,   354,   355,   356,
     357,   358,   359,   360,   361,   362,   363,   364,     0,     0,
       0,     0,   365,   366,     0,     0,     0,     0,  -761,  -761,
       0,     0,     0,     0,     0,     0,     0,     0,  -761,  -761,
    -761,  -761,  -761,  -761,  -761,  -761,  -761,  -761,  -761,  -761,
    -761,     0,     0,     0,     0,  -761,  -761,  -761,  -761,     0,
     795,  -761,     0,   368,     0,   369,   370,   371,   372,   373,
     374,   375,   376,   377,   378,     0,     0,     0,     0,     0,
       0,  -761,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -131,  -761,     0,  -761,  -761,
    -761,  -761,  -761,  -761,  -761,  -761,  -761,  -761,     0,     0,
       0,     0,  -761,  -761,  -761,  -122,  -761,     0,  -761,     0,
    -761,     0,  -761,     0,  -761,  -761,  -761,     0,     0,     0,
    -761,  -761,     0,  -761,     0,     0,     0,     0,     0,     0,
       0,  -761,  -761,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -761,  -761,     0,  -761,  -761,  -761,  -761,
    -761,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -761,  -761,     0,     0,     0,     0,
       0,     0,     0,     0,  -761,  -761,  -761,  -761,  -761,  -761,
    -761,  -761,  -761,  -761,  -761,  -761,  -761,     0,     0,     0,
       0,  -761,  -761,  -761,  -761,     0,   795,  -761,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -761,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -131,  -761,     0,  -761,  -761,  -761,  -761,  -761,  -761,
    -761,  -761,  -761,  -761,     0,     0,     0,     0,  -761,  -761,
    -761,  -761,  -334,     0,  -761,     0,  -761,     0,  -761,     0,
    -334,  -334,  -334,     0,     0,     0,  -334,  -334,     0,  -334,
       0,     0,     0,     0,     0,     0,     0,  -334,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -334,
    -334,     0,  -334,  -334,  -334,  -334,  -334,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -334,  -334,     0,     0,     0,     0,     0,     0,     0,     0,
    -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,
    -334,  -334,  -334,     0,     0,     0,     0,  -334,  -334,  -334,
    -334,     0,   796,  -334,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -334,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -133,  -334,     0,
    -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,
       0,     0,     0,     0,   797,  -334,  -334,  -124,  -334,     0,
    -334,     0,  -334,     0,  -334,     0,  -334,  -334,  -334,     0,
       0,     0,  -334,  -334,     0,  -334,     0,     0,     0,     0,
       0,     0,     0,  -334,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -334,  -334,     0,  -334,  -334,
    -334,  -334,  -334,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -334,  -334,     0,     0,
       0,     0,     0,     0,     0,     0,  -334,  -334,  -334,  -334,
    -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,     0,
       0,     0,     0,  -334,  -334,  -334,  -334,     0,   796,  -334,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -334,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -133,  -334,     0,  -334,  -334,  -334,  -334,
    -334,  -334,  -334,  -334,  -334,  -334,     0,     0,     0,     0,
     797,  -334,  -334,  -334,     0,     0,  -334,     0,  -334,     4,
    -334,     5,     6,     7,     8,     9,  -783,  -783,  -783,    10,
      11,     0,     0,  -783,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,    28,    29,   268,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,     0,    41,    42,    43,
      44,    45,    46,    47,     0,     0,  -783,     0,     0,     0,
       0,     0,     0,     0,    48,    49,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    50,
      51,     0,     0,     0,     0,     0,     0,    52,     0,     0,
      53,    54,     0,    55,    56,     0,    57,     0,     0,    58,
      59,    60,    61,    62,    63,    64,    65,    66,     0,     0,
       0,     4,     0,     5,     6,     7,     8,     9,     0,     0,
    -783,    10,    11,     0,  -783,  -783,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,    67,    68,    69,     0,
      20,    21,    22,    23,    24,    25,    26,     0,  -783,    27,
    -783,     0,     0,     0,     0,    28,    29,   268,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,     0,    41,
      42,    43,    44,    45,    46,    47,     0,     0,  -783,     0,
       0,     0,     0,     0,     0,     0,    48,    49,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    50,    51,     0,     0,     0,     0,     0,     0,    52,
       0,     0,    53,    54,     0,    55,    56,     0,    57,     0,
       0,    58,    59,    60,    61,    62,    63,    64,    65,    66,
       0,     0,     0,     4,     0,     5,     6,     7,     8,     9,
       0,     0,  -783,    10,    11,     0,     0,  -783,    12,  -783,
      13,    14,    15,    16,    17,    18,    19,     0,    67,    68,
      69,     0,    20,    21,    22,    23,    24,    25,    26,     0,
    -783,    27,  -783,     0,     0,     0,     0,    28,    29,   268,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,    43,    44,    45,    46,    47,     0,     0,
    -783,     0,     0,     0,     0,     0,     0,     0,    48,    49,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    50,    51,     0,     0,     0,     0,     0,
       0,    52,     0,     0,    53,    54,     0,    55,    56,     0,
      57,     0,     0,    58,    59,    60,    61,    62,    63,    64,
      65,    66,     0,     0,     0,     4,     0,     5,     6,     7,
       8,     9,     0,     0,  -783,    10,    11,     0,     0,  -783,
      12,     0,    13,    14,    15,    16,    17,    18,    19,  -783,
      67,    68,    69,     0,    20,    21,    22,    23,    24,    25,
      26,     0,  -783,    27,  -783,     0,     0,     0,     0,    28,
      29,   268,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,     0,    41,    42,    43,    44,    45,    46,    47,
       0,     0,  -783,     0,     0,     0,     0,     0,     0,     0,
      48,    49,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    50,    51,     0,     0,     0,
       0,     0,     0,    52,     0,     0,    53,    54,     0,    55,
      56,     0,    57,     0,     0,    58,    59,    60,    61,    62,
      63,    64,    65,    66,     0,     0,     0,     4,     0,     5,
       6,     7,     8,     9,     0,     0,  -783,    10,    11,     0,
       0,  -783,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,    67,    68,    69,     0,    20,    21,    22,    23,
      24,    25,    26,     0,  -783,    27,  -783,     0,     0,     0,
       0,    28,    29,   268,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,     0,    41,    42,    43,    44,    45,
      46,    47,     0,     0,  -783,     0,     0,     0,     0,     0,
       0,     0,    48,    49,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    50,    51,     0,
       0,     0,     0,     0,     0,    52,     0,     0,    53,    54,
       0,    55,    56,     0,    57,     0,     0,    58,    59,    60,
      61,    62,    63,    64,    65,    66,     0,     0,     0,     4,
       0,     5,     6,     7,     8,     9,     0,  -783,  -783,    10,
      11,     0,     0,     0,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,    67,    68,    69,     0,    20,    21,
      22,    23,    24,    25,    26,     0,  -783,    27,  -783,     0,
       0,     0,     0,    28,    29,   268,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,     0,    41,    42,    43,
      44,    45,    46,    47,     0,     0,  -783,     0,     0,     0,
       0,     0,     0,     0,    48,    49,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    50,
      51,     0,     0,     0,     0,     0,     0,    52,     0,     0,
      53,    54,     0,    55,    56,     0,    57,     0,     0,    58,
      59,    60,    61,    62,    63,    64,    65,    66,     0,     0,
       0,     4,     0,     5,     6,     7,     8,     9,     0,     0,
    -783,    10,    11,     0,     0,     0,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,    67,    68,    69,     0,
      20,    21,    22,    23,    24,    25,    26,     0,  -783,    27,
    -783,     0,     0,     0,     0,    28,    29,   268,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,     0,    41,
      42,    43,    44,    45,    46,    47,     0,     0,  -783,     0,
       0,     0,     0,     0,     0,     0,    48,    49,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    50,    51,     0,     0,     0,     0,     0,     0,    52,
       0,     0,    53,    54,     0,    55,    56,     0,    57,     0,
       0,    58,    59,    60,    61,    62,    63,    64,    65,    66,
       0,     0,     0,     4,     0,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,    67,    68,
      69,     0,    20,    21,    22,    23,    24,    25,    26,     0,
    -783,    27,  -783,     0,     0,     0,     0,    28,    29,   268,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,    43,    44,    45,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,    49,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    50,    51,     0,     0,     0,     0,     0,
       0,    52,     0,     0,   269,    54,     0,    55,    56,     0,
      57,     0,     0,    58,    59,    60,    61,    62,    63,    64,
      65,    66,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      67,    68,    69,     0,     0,     0,     0,     0,     0,     0,
    -783,     0,  -783,     4,  -783,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,    28,    29,   268,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,    43,    44,    45,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,    49,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    50,    51,     0,     0,     0,     0,     0,
       0,    52,     0,     0,    53,    54,     0,    55,    56,     0,
      57,     0,     0,    58,    59,    60,    61,    62,    63,    64,
      65,    66,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      67,    68,    69,     0,     0,     0,     0,     0,     0,     0,
    -783,     0,  -783,     4,  -783,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,    43,    44,    45,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,    49,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    50,    51,     0,     0,     0,     0,     0,
       0,    52,     0,     0,    53,    54,     0,    55,    56,     0,
      57,     0,     0,    58,    59,    60,    61,    62,    63,    64,
      65,    66,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      67,    68,    69,     0,     0,  -783,     0,     0,     0,     0,
       0,     0,  -783,     4,  -783,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,    28,    29,   268,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,    43,    44,    45,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,    49,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    50,    51,     0,     0,     0,     0,     0,
       0,    52,     0,     0,    53,    54,     0,    55,    56,     0,
      57,     0,     0,    58,    59,    60,    61,    62,    63,    64,
      65,    66,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      67,    68,    69,     0,     0,  -783,     0,   387,     0,     5,
       6,     7,  -783,     9,  -783,     0,     0,    10,    11,     0,
       0,     0,    12,  -770,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,     0,    41,    42,    43,    44,    45,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    48,    49,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    50,    51,     0,
       0,     0,     0,     0,     0,   216,     0,     0,   217,    54,
       0,    55,    56,     0,     0,     0,     0,    58,    59,    60,
      61,    62,    63,    64,    65,    66,     0,     0,  -771,     4,
       0,     5,     6,     7,     8,     9,  -771,  -771,  -771,    10,
      11,     0,  -771,  -771,    12,  -771,    13,    14,    15,    16,
      17,    18,    19,  -771,    67,    68,    69,     0,    20,    21,
      22,    23,    24,    25,    26,     0,   315,    27,   316,     0,
       0,     0,     0,    28,    29,   268,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,     0,    41,    42,    43,
      44,    45,    46,    47,     0,     0,  -771,     0,     0,     0,
       0,     0,     0,     0,    48,    49,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    50,
      51,     0,     0,     0,     0,     0,     0,    52,     0,     0,
      53,    54,     0,    55,    56,     0,    57,     0,     0,    58,
      59,    60,    61,    62,    63,    64,    65,    66,     0,  -771,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    67,    68,    69,     0,
       0,  -771,     0,     0,     0,     0,  -771,     0,   526,  -771,
       4,     0,     5,     6,     7,     8,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,     0,    41,    42,
      43,    44,    45,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    48,    49,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      50,    51,     0,     0,     0,     0,     0,     0,    52,     0,
       0,    53,    54,     0,    55,    56,     0,    57,     0,     0,
      58,    59,    60,    61,    62,    63,    64,    65,    66,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    67,    68,    69,
       0,     0,  -771,     5,     6,     7,     0,     9,     0,   526,
       0,    10,    11,     0,     0,     0,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,   214,
       0,     0,     0,     0,     0,     0,    29,     0,     0,    32,
      33,    34,    35,    36,    37,    38,    39,    40,   215,    41,
      42,    43,    44,    45,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    48,    49,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    50,    51,     0,     0,     0,     0,     0,     0,   216,
       0,     0,   217,    54,     0,    55,    56,     0,   218,   219,
     220,    58,    59,   221,    61,    62,    63,    64,    65,    66,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,    67,   222,
      69,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,   246,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,     0,
      41,    42,    43,    44,    45,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,    49,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    50,    51,     0,     0,     0,     0,     0,     0,
     216,     0,     0,   217,    54,     0,    55,    56,     0,     0,
       0,     0,    58,    59,    60,    61,    62,    63,    64,    65,
      66,     0,     0,     0,     0,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,    67,
      68,    69,   155,   156,   157,   416,   417,   418,   419,   162,
     163,   164,     0,   246,     0,     0,     0,   165,   166,   167,
     168,   420,   421,   422,   423,   173,    37,    38,   424,    40,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   175,   176,
     177,   178,   179,   180,   181,   182,   183,     0,     0,   184,
     185,     0,     0,     0,     0,   186,   187,   188,   189,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     190,   191,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
       0,   202,   203,     0,     0,     0,     0,     0,   204,   425,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,   153,   154,     0,     0,     0,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,     0,     0,     0,
       0,     0,   165,   166,   167,   168,   169,   170,   171,   172,
     173,    37,    38,   174,    40,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   175,   176,   177,   178,   179,   180,   181,
     182,   183,     0,     0,   184,   185,     0,     0,     0,     0,
     186,   187,   188,   189,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   190,   191,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   192,   193,   194,   195,   196,
     197,   198,   199,   200,   201,     0,   202,   203,     0,     0,
       0,     0,     0,   204,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,     0,     0,
       0,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,     0,     0,     0,     0,     0,   165,   166,   167,   168,
     169,   170,   171,   172,   173,   248,     0,   174,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   175,   176,   177,
     178,   179,   180,   181,   182,   183,     0,     0,   184,   185,
       0,     0,     0,     0,   186,   187,   188,   189,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   190,
     191,     0,     0,    59,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,     0,
     202,   203,     0,     0,     0,     0,     0,   204,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
     153,   154,     0,     0,     0,   155,   156,   157,   158,   159,
     160,   161,   162,   163,   164,     0,     0,     0,     0,     0,
     165,   166,   167,   168,   169,   170,   171,   172,   173,     0,
       0,   174,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   175,   176,   177,   178,   179,   180,   181,   182,   183,
       0,     0,   184,   185,     0,     0,     0,     0,   186,   187,
     188,   189,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   190,   191,     0,     0,    59,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   192,   193,   194,   195,   196,   197,   198,
     199,   200,   201,     0,   202,   203,     0,     0,     0,     0,
       0,   204,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   148,
     149,   150,   151,   152,   153,   154,     0,     0,     0,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,     0,
       0,     0,     0,     0,   165,   166,   167,   168,   169,   170,
     171,   172,   173,     0,     0,   174,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   175,   176,   177,   178,   179,
     180,   181,   182,   183,     0,     0,   184,   185,     0,     0,
       0,     0,   186,   187,   188,   189,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   190,   191,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,     0,   202,   203,
       5,     6,     7,     0,     9,   204,     0,     0,    10,    11,
       0,     0,     0,    12,     0,    13,    14,    15,   256,   257,
      18,    19,     0,     0,     0,     0,     0,    20,    21,   258,
      23,    24,    25,    26,     0,     0,   214,     0,     0,     0,
       0,     0,     0,   286,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,     0,    41,    42,    43,    44,
      45,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   287,     0,     0,   217,
      54,     0,    55,    56,     0,     0,     0,     0,    58,    59,
      60,    61,    62,    63,    64,    65,    66,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,   288,    10,    11,     0,     0,
       0,    12,   289,    13,    14,    15,   256,   257,    18,    19,
       0,     0,     0,     0,     0,    20,    21,   258,    23,    24,
      25,    26,     0,     0,   214,     0,     0,     0,     0,     0,
       0,   286,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,     0,    41,    42,    43,    44,    45,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   287,     0,     0,   217,    54,     0,
      55,    56,     0,     0,     0,     0,    58,    59,    60,    61,
      62,    63,    64,    65,    66,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     4,     0,     5,     6,     7,     8,
       9,     0,     0,   288,    10,    11,     0,     0,     0,    12,
     587,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
       0,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,     0,    41,    42,    43,    44,    45,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    48,
      49,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    50,    51,     0,     0,     0,     0,
       0,     0,    52,     0,     0,    53,    54,     0,    55,    56,
       0,    57,     0,     0,    58,    59,    60,    61,    62,    63,
      64,    65,    66,     0,     0,   387,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,     0,     0,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,    67,    68,    69,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,     0,     0,
      29,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,     0,    41,    42,    43,    44,    45,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      48,    49,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    50,    51,     0,     0,     0,
       0,     0,     0,   216,     0,     0,   217,    54,     0,    55,
      56,     0,     0,     0,     0,    58,    59,    60,    61,    62,
      63,    64,    65,    66,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,     0,     0,
       0,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,    67,    68,    69,    20,    21,    22,    23,    24,
      25,    26,     0,     0,   214,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,   215,    41,    42,    43,    44,    45,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    48,    49,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    50,    51,     0,     0,
       0,     0,     0,     0,   216,     0,     0,   217,    54,     0,
      55,    56,     0,   218,   219,   220,    58,    59,   221,    61,
      62,    63,    64,    65,    66,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,     0,
       0,     0,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,     0,    67,   222,    69,    20,    21,    22,    23,
      24,    25,    26,     0,     0,   214,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,   215,    41,    42,    43,    44,    45,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    48,    49,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    50,   461,     0,
       0,     0,     0,     0,     0,   216,     0,     0,   217,    54,
       0,    55,    56,     0,   218,   219,   220,    58,    59,   221,
      61,    62,    63,    64,    65,    66,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,     0,    13,    14,    15,   256,   257,
      18,    19,     0,     0,    67,   222,    69,    20,    21,   258,
      23,    24,    25,    26,     0,     0,   214,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,   215,    41,    42,    43,    44,
      45,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    48,    49,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    50,    51,
       0,     0,     0,     0,     0,     0,   216,     0,     0,   217,
      54,     0,    55,    56,     0,   218,   219,   220,    58,    59,
     221,    61,    62,    63,    64,    65,    66,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,     0,    13,    14,    15,   256,
     257,    18,    19,     0,     0,    67,   222,    69,    20,    21,
     258,    23,    24,    25,    26,     0,     0,   214,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,   215,    41,    42,    43,
      44,    45,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    48,    49,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    50,
     461,     0,     0,     0,     0,     0,     0,   216,     0,     0,
     217,    54,     0,    55,    56,     0,   218,   219,   220,    58,
      59,   221,    61,    62,    63,    64,    65,    66,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,     0,    13,    14,    15,
     256,   257,    18,    19,     0,     0,    67,   222,    69,    20,
      21,   258,    23,    24,    25,    26,     0,     0,   214,     0,
       0,     0,     0,     0,     0,    29,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,   215,    41,    42,
      43,    44,    45,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    48,    49,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      50,    51,     0,     0,     0,     0,     0,     0,   216,     0,
       0,   217,    54,     0,    55,    56,     0,   218,   219,     0,
      58,    59,   221,    61,    62,    63,    64,    65,    66,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,     0,    13,    14,
      15,   256,   257,    18,    19,     0,     0,    67,   222,    69,
      20,    21,   258,    23,    24,    25,    26,     0,     0,   214,
       0,     0,     0,     0,     0,     0,    29,     0,     0,    32,
      33,    34,    35,    36,    37,    38,    39,    40,   215,    41,
      42,    43,    44,    45,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    48,    49,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    50,    51,     0,     0,     0,     0,     0,     0,   216,
       0,     0,   217,    54,     0,    55,    56,     0,     0,   219,
     220,    58,    59,   221,    61,    62,    63,    64,    65,    66,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,     0,    13,
      14,    15,   256,   257,    18,    19,     0,     0,    67,   222,
      69,    20,    21,   258,    23,    24,    25,    26,     0,     0,
     214,     0,     0,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,   215,
      41,    42,    43,    44,    45,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,    49,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    50,    51,     0,     0,     0,     0,     0,     0,
     216,     0,     0,   217,    54,     0,    55,    56,     0,     0,
     219,     0,    58,    59,   221,    61,    62,    63,    64,    65,
      66,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,    67,
     222,    69,    20,    21,    22,    23,    24,    25,    26,     0,
       0,   214,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,    43,    44,    45,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,    49,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    50,    51,     0,     0,     0,     0,     0,
       0,   216,     0,     0,   217,    54,     0,    55,    56,     0,
     773,     0,     0,    58,    59,    60,    61,    62,    63,    64,
      65,    66,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,     0,     0,     0,    12,
       0,    13,    14,    15,   256,   257,    18,    19,     0,     0,
      67,   222,    69,    20,    21,   258,    23,    24,    25,    26,
       0,     0,   214,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,     0,    41,    42,    43,    44,    45,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    48,
      49,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    50,    51,     0,     0,     0,     0,
       0,     0,   216,     0,     0,   217,    54,     0,    55,    56,
       0,   942,     0,     0,    58,    59,    60,    61,    62,    63,
      64,    65,    66,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,     0,     0,
      12,     0,    13,    14,    15,   256,   257,    18,    19,     0,
       0,    67,   222,    69,    20,    21,   258,    23,    24,    25,
      26,     0,     0,   214,     0,     0,     0,     0,     0,     0,
      29,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,     0,    41,    42,    43,    44,    45,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      48,    49,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    50,    51,     0,     0,     0,
       0,     0,     0,   216,     0,     0,   217,    54,     0,    55,
      56,     0,   990,     0,     0,    58,    59,    60,    61,    62,
      63,    64,    65,    66,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,     0,     0,
       0,    12,     0,    13,    14,    15,   256,   257,    18,    19,
       0,     0,    67,   222,    69,    20,    21,   258,    23,    24,
      25,    26,     0,     0,   214,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,     0,    41,    42,    43,    44,    45,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    48,    49,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    50,    51,     0,     0,
       0,     0,     0,     0,   216,     0,     0,   217,    54,     0,
      55,    56,     0,   773,     0,     0,    58,    59,    60,    61,
      62,    63,    64,    65,    66,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,     0,
       0,     0,    12,     0,    13,    14,    15,   256,   257,    18,
      19,     0,     0,    67,   222,    69,    20,    21,   258,    23,
      24,    25,    26,     0,     0,   214,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,     0,    41,    42,    43,    44,    45,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    48,    49,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    50,    51,     0,
       0,     0,     0,     0,     0,   216,     0,     0,   217,    54,
       0,    55,    56,     0,  1112,     0,     0,    58,    59,    60,
      61,    62,    63,    64,    65,    66,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,     0,    13,    14,    15,   256,   257,
      18,    19,     0,     0,    67,   222,    69,    20,    21,   258,
      23,    24,    25,    26,     0,     0,   214,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,     0,    41,    42,    43,    44,
      45,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    48,    49,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    50,    51,
       0,     0,     0,     0,     0,     0,   216,     0,     0,   217,
      54,     0,    55,    56,     0,     0,     0,     0,    58,    59,
      60,    61,    62,    63,    64,    65,    66,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,    67,   222,    69,    20,    21,
      22,    23,    24,    25,    26,     0,     0,   214,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,     0,    41,    42,    43,
      44,    45,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    48,    49,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    50,
      51,     0,     0,     0,     0,     0,     0,   216,     0,     0,
     217,    54,     0,    55,    56,     0,     0,     0,     0,    58,
      59,    60,    61,    62,    63,    64,    65,    66,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,    67,   222,    69,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,     0,    29,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,     0,    41,    42,
      43,    44,    45,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    48,    49,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      50,    51,     0,     0,     0,     0,     0,     0,   216,     0,
       0,   217,    54,     0,    55,    56,     0,     0,     0,     0,
      58,    59,    60,    61,    62,    63,    64,    65,    66,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,    67,    68,    69,
      20,    21,    22,    23,    24,    25,    26,     0,     0,   755,
       0,     0,     0,     0,     0,     0,    29,     0,     0,    32,
      33,    34,    35,    36,    37,    38,    39,    40,     0,    41,
      42,    43,    44,    45,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    48,    49,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    50,    51,     0,     0,     0,     0,     0,     0,   216,
       0,     0,   217,    54,     0,    55,    56,     0,     0,     0,
       0,    58,    59,    60,    61,    62,    63,    64,    65,    66,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,     0,    13,
      14,    15,   256,   257,    18,    19,     0,     0,    67,   222,
      69,    20,    21,   258,    23,    24,    25,    26,     0,     0,
     851,     0,     0,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,     0,
      41,    42,    43,    44,    45,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,    49,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    50,    51,     0,     0,     0,     0,     0,     0,
     216,     0,     0,   217,    54,     0,    55,    56,     0,     0,
       0,     0,    58,    59,    60,    61,    62,    63,    64,    65,
      66,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,   256,   257,    18,    19,     0,     0,    67,
     222,    69,    20,    21,   258,    23,    24,    25,    26,     0,
       0,   214,     0,     0,     0,     0,     0,     0,   286,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,    43,    44,    45,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   287,     0,     0,   347,    54,     0,    55,    56,     0,
     348,     0,     0,    58,    59,    60,    61,    62,    63,    64,
      65,    66,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,     0,    13,
      14,    15,   256,   257,    18,    19,     0,     0,     0,     0,
     288,    20,    21,   258,    23,    24,    25,    26,     0,     0,
     214,     0,     0,     0,     0,     0,     0,   286,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,     0,
      41,    42,    43,    44,    45,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     397,     0,     0,    53,    54,     0,    55,    56,     0,    57,
       0,     0,    58,    59,    60,    61,    62,    63,    64,    65,
      66,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,     0,    13,    14,
      15,   256,   257,    18,    19,     0,     0,     0,     0,   288,
      20,    21,   258,    23,    24,    25,    26,     0,     0,   214,
       0,     0,     0,     0,     0,     0,   286,     0,     0,    32,
      33,    34,   405,    36,    37,    38,   406,    40,     0,    41,
      42,    43,    44,    45,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   407,     0,     0,     0,   408,
       0,     0,   217,    54,     0,    55,    56,     0,     0,     0,
       0,    58,    59,    60,    61,    62,    63,    64,    65,    66,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,     0,    13,    14,    15,
     256,   257,    18,    19,     0,     0,     0,     0,   288,    20,
      21,   258,    23,    24,    25,    26,     0,     0,   214,     0,
       0,     0,     0,     0,     0,   286,     0,     0,    32,    33,
      34,   405,    36,    37,    38,   406,    40,     0,    41,    42,
      43,    44,    45,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   408,     0,
       0,   217,    54,     0,    55,    56,     0,     0,     0,     0,
      58,    59,    60,    61,    62,    63,    64,    65,    66,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,     0,    13,    14,    15,   256,
     257,    18,    19,     0,     0,     0,     0,   288,    20,    21,
     258,    23,    24,    25,    26,     0,     0,   214,     0,     0,
       0,     0,     0,     0,   286,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,     0,    41,    42,    43,
      44,    45,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   287,     0,     0,
     347,    54,     0,    55,    56,     0,     0,     0,     0,    58,
      59,    60,    61,    62,    63,    64,    65,    66,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,     0,    13,    14,    15,   256,   257,
      18,    19,     0,     0,     0,     0,   288,    20,    21,   258,
      23,    24,    25,    26,     0,     0,   214,     0,     0,     0,
       0,     0,     0,   286,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,     0,    41,    42,    43,    44,
      45,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  1179,     0,     0,   217,
      54,     0,    55,    56,     0,     0,     0,     0,    58,    59,
      60,    61,    62,    63,    64,    65,    66,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,     0,
       0,     0,    12,     0,    13,    14,    15,   256,   257,    18,
      19,     0,     0,     0,     0,   288,    20,    21,   258,    23,
      24,    25,    26,     0,     0,   214,     0,     0,     0,     0,
       0,     0,   286,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,     0,    41,    42,    43,    44,    45,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  1207,     0,     0,   217,    54,
       0,    55,    56,    23,    24,    25,    26,    58,    59,    60,
      61,    62,    63,    64,    65,    66,     0,     0,     0,    32,
      33,    34,  1055,     0,     0,     0,  1056,     0,     0,    41,
      42,    43,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   288,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  1058,  1059,     0,     0,     0,   680,   618,     0,  1060,
     681,     0,  1061,     0,     0,  1062,  1063,     0,  1064,     0,
       0,    58,    59,    60,    61,    62,    63,    64,    65,    66,
     175,   176,   177,   178,   179,   180,   181,   182,   183,     0,
       0,   184,   185,     0,     0,     0,     0,   186,   187,   188,
     189,  1066,     0,     0,     0,     0,     0,     0,   288,     0,
       0,     0,   190,   191,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,     0,   202,   203,   683,   628,     0,     0,   684,
     204,   241,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   175,
     176,   177,   178,   179,   180,   181,   182,   183,     0,     0,
     184,   185,     0,     0,     0,     0,   186,   187,   188,   189,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   190,   191,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   192,   193,   194,   195,   196,   197,   198,   199,   200,
     201,     0,   202,   203,   680,   618,     0,     0,   698,   204,
     241,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   175,   176,
     177,   178,   179,   180,   181,   182,   183,     0,     0,   184,
     185,     0,     0,     0,     0,   186,   187,   188,   189,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     190,   191,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
       0,   202,   203,   709,   618,     0,     0,   710,   204,   241,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   175,   176,   177,
     178,   179,   180,   181,   182,   183,     0,     0,   184,   185,
       0,     0,     0,     0,   186,   187,   188,   189,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   190,
     191,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,     0,
     202,   203,   712,   628,     0,     0,   713,   204,   241,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   175,   176,   177,   178,
     179,   180,   181,   182,   183,     0,     0,   184,   185,     0,
       0,     0,     0,   186,   187,   188,   189,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   190,   191,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,     0,   202,
     203,   825,   618,     0,     0,   826,   204,   241,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   175,   176,   177,   178,   179,
     180,   181,   182,   183,     0,     0,   184,   185,     0,     0,
       0,     0,   186,   187,   188,   189,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   190,   191,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,     0,   202,   203,
     828,   628,     0,     0,   829,   204,   241,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   175,   176,   177,   178,   179,   180,
     181,   182,   183,     0,     0,   184,   185,     0,     0,     0,
       0,   186,   187,   188,   189,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   190,   191,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   192,   193,   194,   195,
     196,   197,   198,   199,   200,   201,     0,   202,   203,   834,
     618,     0,     0,   835,   204,   241,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   175,   176,   177,   178,   179,   180,   181,
     182,   183,     0,     0,   184,   185,     0,     0,     0,     0,
     186,   187,   188,   189,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   190,   191,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   192,   193,   194,   195,   196,
     197,   198,   199,   200,   201,     0,   202,   203,   665,   628,
       0,     0,   666,   204,   241,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   175,   176,   177,   178,   179,   180,   181,   182,
     183,     0,     0,   184,   185,     0,     0,     0,     0,   186,
     187,   188,   189,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   190,   191,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,     0,   202,   203,   996,   618,     0,
       0,   997,   204,   241,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   175,   176,   177,   178,   179,   180,   181,   182,   183,
       0,     0,   184,   185,     0,     0,     0,     0,   186,   187,
     188,   189,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   190,   191,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   192,   193,   194,   195,   196,   197,   198,
     199,   200,   201,     0,   202,   203,   999,   628,     0,     0,
    1000,   204,   241,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     175,   176,   177,   178,   179,   180,   181,   182,   183,     0,
       0,   184,   185,     0,     0,     0,     0,   186,   187,   188,
     189,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   190,   191,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,     0,   202,   203,  1292,   618,     0,     0,  1293,
     204,   241,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   175,
     176,   177,   178,   179,   180,   181,   182,   183,     0,     0,
     184,   185,     0,     0,     0,     0,   186,   187,   188,   189,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   190,   191,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   192,   193,   194,   195,   196,   197,   198,   199,   200,
     201,     0,   202,   203,  1295,   628,     0,     0,  1296,   204,
     241,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   175,   176,
     177,   178,   179,   180,   181,   182,   183,     0,     0,   184,
     185,     0,     0,     0,     0,   186,   187,   188,   189,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     190,   191,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
       0,   202,   203,  1309,   618,     0,     0,  1310,   204,   241,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   175,   176,   177,
     178,   179,   180,   181,   182,   183,     0,     0,   184,   185,
       0,     0,     0,     0,   186,   187,   188,   189,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   190,
     191,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,     0,
     202,   203,   665,   628,     0,     0,   666,   204,   241,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   175,   176,   177,   178,
     179,   180,   181,   182,   183,     0,     0,   184,   185,     0,
       0,     0,     0,   186,   187,   188,   189,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   190,   191,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,     0,   202,
     203,     0,     0,     0,     0,     0,   204
};

static const yytype_int16 yycheck[] =
{
       2,    92,    16,    17,    60,    22,    95,    59,    68,    98,
       8,     2,   384,    53,    54,   106,   408,   328,    28,    16,
      17,   332,    14,    15,   813,   107,   238,   390,    16,    17,
      28,    59,   563,     8,   574,    97,   745,   577,   383,   590,
     385,   328,    81,   742,    57,   332,    53,   332,    71,    86,
     451,    53,    54,    28,   241,    57,   304,   305,    55,    56,
     459,    59,    16,    17,    81,   104,   576,    55,    94,    95,
     451,    98,    98,    86,   102,    98,   576,   915,   459,   820,
     505,    73,   745,   455,    86,    67,    99,   100,   101,   103,
     435,   492,    64,    65,    66,     0,   495,    99,   100,   101,
     102,    57,   580,   280,   102,     2,   103,   284,   749,   454,
     605,   456,   878,   880,    71,   103,    25,  1002,   658,    78,
      37,    38,    13,    26,  1211,   742,    69,    29,  1152,   269,
      31,   793,   749,    66,    13,   507,   101,   482,    25,    66,
      78,     0,   233,    98,  1168,  1045,   110,  1001,  1002,   103,
     749,  1045,    25,   752,    97,    98,    53,    54,    55,    56,
     579,    25,   587,   508,   583,   221,    10,   153,    26,   100,
     261,   100,   135,   673,   668,   669,   162,   217,   142,   799,
      77,    78,    13,    66,  1152,   100,  1024,   807,    69,   122,
     123,    25,    34,   120,   157,   122,   123,   162,  1166,   154,
      25,    13,   157,   294,   135,    13,   135,   347,   100,   545,
      52,   154,   162,    13,    13,   217,   121,    98,  1305,   245,
     135,   247,   124,  1061,    68,  1063,  1111,   241,   161,   269,
     244,    13,   321,   322,   323,   324,    13,   120,  1132,    25,
     155,   258,   259,   135,   271,   162,   155,   244,   271,   152,
     252,    13,   289,   241,   466,  1279,   244,  1111,    25,  1283,
    1284,   252,   269,   655,   155,   224,    13,   269,   155,   160,
    1037,   162,  1038,   521,   646,   523,   289,   939,   157,    25,
     979,   160,   155,   162,   656,   223,   224,   289,   689,    25,
     244,   155,   691,   833,   152,   321,   322,   323,   324,   157,
     326,   327,   647,   392,  1204,  1329,   288,   347,   689,   400,
     691,  1211,   657,  1213,   271,  1169,  1284,   319,   407,   797,
     217,   155,  1176,  1177,   819,  1114,   223,   224,   319,   160,
     155,   162,   291,   974,   231,   348,  1174,  1175,  1001,  1002,
     347,   238,  1001,  1002,   241,   347,   348,    27,   160,   250,
     162,   391,   160,   393,   162,   252,   781,   974,   157,   890,
     160,   160,   162,   162,  1001,  1002,   392,   394,   385,   155,
     384,   394,   269,  1114,   152,   974,   927,   976,   160,  1196,
     162,   407,   981,   160,   386,   162,   896,   385,   155,   391,
     157,   393,   348,   729,   730,   155,   896,    66,   160,   135,
     162,   802,   162,  1062,    78,  1305,  1062,  1307,   948,   155,
     101,   157,  1312,   160,  1314,   162,    69,   431,   100,   155,
    1079,   802,   158,   917,   918,   101,   162,  1281,  1045,   923,
     924,   742,    15,   159,   431,  1335,   162,   435,   749,   456,
      69,   455,    25,   431,   345,    98,  1045,    69,  1111,   350,
     347,   120,  1111,   135,   451,  1176,  1177,   957,   456,   160,
      27,   162,    29,   100,   155,   482,   476,   679,   505,    98,
     718,   145,   146,   147,  1111,   723,    98,   431,   476,   155,
     160,    34,   162,    56,   482,  1302,   488,   384,   528,   386,
    1149,   508,   505,   507,   391,   492,   393,   161,   135,    52,
    1209,   476,   689,   505,   905,  1204,  1169,  1166,    28,  1049,
     508,    25,   100,  1176,  1177,  1009,   100,  1176,  1177,    58,
    1176,  1177,    37,    38,   155,   585,   528,  1126,   100,   706,
     590,   162,   534,   135,  1323,    69,   274,   438,   439,    78,
     912,   590,   878,   534,   880,    52,  1209,   135,   449,    56,
     587,   135,   575,   383,   865,   385,   457,   458,   455,   573,
    1281,   155,   576,   135,    98,   964,   911,   464,   913,   466,
     109,   110,   574,   157,   587,   577,   477,    25,   865,   135,
     865,   944,   483,   964,   623,   587,   648,  1204,    25,   113,
    1058,  1059,   100,   632,  1211,   100,   636,   585,   638,  1130,
     100,   155,   590,   142,   100,   435,   623,  1138,   668,   669,
     507,   100,  1211,  1312,  1213,   632,   584,   640,  1281,   668,
     669,   135,   155,    15,   454,    17,   456,   135,   667,   162,
     647,   528,   646,   659,   636,   135,   638,    56,    69,   135,
     657,   155,   656,   157,   158,    69,   135,   859,   162,   647,
     667,   481,   482,   157,   551,   966,   658,  1197,  1021,   657,
     670,   100,    69,   974,   716,   633,   563,    98,  1060,   695,
     707,   639,   670,   641,    98,  1045,   506,   574,   508,   966,
     577,   966,   699,   700,   157,    14,    15,   135,  1305,   162,
      97,    98,   689,   431,   707,   670,   135,   135,   135,   790,
     101,  1037,  1038,  1171,  1172,   707,  1305,   155,  1307,    78,
     158,   155,   279,   280,   162,  1314,   155,   284,   155,   286,
      78,   158,   714,    26,   462,   162,    69,   465,    69,   626,
     135,   469,   100,   634,  1045,   632,  1335,    95,    96,   636,
    1103,   638,  1273,   832,   781,   100,   718,   485,   720,   646,
     155,   723,   724,    66,    97,    98,    97,    98,    26,   656,
     842,   658,    69,   157,    66,   155,    69,   100,   781,    69,
     667,    89,    90,   157,   143,   144,   145,   146,   147,   781,
     135,  1173,   679,   157,   142,   143,   144,   145,   146,   147,
     830,    98,   760,    52,    97,    98,   764,    56,    98,   766,
     155,    69,   135,   700,   771,   802,   832,   120,   831,   122,
     123,   154,   840,   154,   157,   645,   554,   647,   120,   158,
     122,   123,   155,   125,    66,   655,  1189,   657,   830,    97,
      98,   833,    69,    26,  1204,   568,    69,   570,    69,  1209,
      78,  1211,   840,  1213,   933,   135,   736,   159,   586,   152,
      56,   154,   153,   821,   157,   745,   824,   917,   918,   157,
      97,    98,    69,   923,   924,    98,    97,    98,   917,   918,
     838,    40,    41,   155,   923,   924,    69,    54,  1250,    78,
     122,   123,   896,   125,   152,   161,   154,    64,    65,   157,
      97,    98,   932,  1204,   795,   796,   913,   158,   912,    69,
    1211,   162,   803,   804,    97,    98,  1251,   933,   100,   901,
    1166,   903,    69,   810,   811,   913,   813,   154,    66,    67,
    1176,  1177,    69,   154,    66,   663,   949,    97,    98,   969,
     932,   290,   291,   830,   994,  1305,   833,  1307,   152,   927,
      97,    98,  1312,   135,  1314,   994,   948,   154,    69,  1009,
      97,    98,   853,   854,   135,   856,   857,    69,   682,   152,
    1009,   154,   859,   155,   157,  1335,   934,   969,    66,   937,
     135,   135,   940,   107,   122,   123,    97,    98,   120,   947,
     122,   123,   950,    69,   154,    97,    98,   711,  1028,   155,
     992,   993,   160,   890,  1305,   733,    66,   154,    56,  1001,
    1002,   992,   993,    69,    25,   135,  1046,   154,   909,  1065,
     139,    97,    98,   135,  1066,   912,    58,    69,   155,    66,
      69,   922,   120,   155,   122,   123,  1028,   125,   153,   100,
     927,    97,    98,   154,  1279,   932,    78,   155,  1283,   606,
      83,    84,   154,   100,  1046,    97,    98,  1049,    97,    98,
     120,   948,   122,   123,   158,   158,  1058,  1059,   625,   960,
    1062,   155,  1030,   155,   135,   155,  1106,   109,   154,  1109,
    1110,   113,   969,   120,  1163,   122,   123,  1079,   135,  1119,
     135,   911,   155,   913,   155,    52,   155,   155,   154,  1102,
    1181,   988,   989,    52,   153,   138,   139,    13,   155,   155,
    1102,   839,   154,   827,  1106,   154,    25,  1109,  1110,  1111,
     155,    17,   153,   837,    44,   682,   155,  1119,   155,  1016,
     858,  1018,   860,  1136,   135,   153,    44,     2,   155,   155,
      52,  1028,    54,    55,  1136,    57,    44,  1163,   876,   706,
      44,    16,    17,   135,   711,  1185,   137,  1149,     8,  1046,
    1152,   159,  1049,   886,   887,  1045,    15,   155,   891,   155,
     893,   155,   895,    52,  1166,  1167,  1168,  1256,  1257,  1171,
    1172,   155,   155,   155,  1176,  1177,  1232,   153,    53,    54,
     102,   155,    57,  1185,   155,   140,    54,    55,   755,    57,
    1203,   155,   101,    68,     9,  1197,    64,    65,    78,   155,
      52,  1203,    52,   140,    54,    55,    56,    57,   155,  1106,
     155,    86,  1109,  1110,   158,    95,    96,  1114,   160,    94,
      95,    56,  1119,    98,    99,   100,   101,   140,   103,   155,
    1256,  1257,   155,  1130,  1251,   155,  1250,   897,   898,   153,
      52,  1138,    54,    55,    56,    57,   906,     2,   908,    52,
     910,    56,   102,  1251,   155,   155,  1157,   101,   108,  1299,
     827,    16,    17,   143,   144,   145,   146,   147,   153,   140,
     837,  1140,  1141,    56,   998,   157,   155,  1279,   155,   155,
     252,  1283,  1284,   155,   851,   668,   669,   157,  1185,   155,
     102,  1188,   155,   155,  1262,   319,   484,  1299,    53,    54,
    1197,   155,   685,   686,   155,   488,  1039,  1040,  1041,  1042,
     155,   840,    99,    68,  1204,   101,    90,   477,   701,  1209,
     667,  1211,   928,  1213,  1134,  1188,    52,  1329,    54,    55,
      56,    57,    52,   729,    54,    55,    56,    57,  1062,    94,
      95,   878,   217,    98,    59,    60,    61,    62,   103,  1045,
     989,  1302,   843,  1250,   343,  1252,  1253,    40,    41,    42,
      43,    44,  1323,  1167,  1169,  1253,   241,  1169,  1165,   244,
     245,   110,   247,   523,  1252,   102,  1273,   252,   745,   742,
     215,  1209,  1204,   218,   219,   220,  1318,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   269,    -1,    -1,    -1,    -1,  1268,
    1269,    -1,  1299,    -1,    -1,  1274,    -1,  1276,  1277,    -1,
      -1,    -1,    -1,    -1,   289,  1305,    -1,  1307,    -1,  1249,
      -1,  1251,  1312,    -1,  1314,    -1,  1323,  1151,    -1,    -1,
    1154,   998,    -1,    -1,    -1,    -1,    -1,  1175,    -1,    -1,
      -1,  1271,    -1,    -1,   319,  1335,   321,   322,   323,   324,
    1174,   326,   327,    -1,    -1,    -1,  1325,  1326,  1327,  1328,
      -1,    -1,   217,    -1,    -1,    -1,  1199,  1034,    -1,    -1,
    1339,    -1,   347,   348,    -1,    -1,    -1,  1215,  1045,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   241,  1225,    -1,   244,
     245,    -1,   247,    -1,    -1,  1062,  1063,   252,    -1,    -1,
      -1,    -1,    -1,    -1,  1242,  1243,  1244,    -1,    -1,   384,
      -1,   386,    -1,    -1,   269,    -1,   391,   392,   393,    -1,
      -1,  1245,  1246,  1247,    52,    -1,    54,    55,    56,    57,
      58,    -1,   407,    -1,   917,   918,    78,    -1,    -1,    -1,
     923,   924,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      78,    -1,    -1,    95,    96,    -1,   431,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   319,  1132,   321,   322,   323,   324,
    1294,   326,   327,    -1,   102,   958,   959,    -1,   961,   962,
     455,   109,   110,    -1,  1151,    -1,    -1,  1154,    -1,    -1,
      -1,    52,   347,    54,    55,    56,    57,    58,   140,   141,
     142,   143,   144,   145,   146,   147,    -1,  1174,    -1,    -1,
      -1,    -1,    -1,   488,   142,   450,   451,    78,    -1,    -1,
      -1,    -1,    -1,    -1,   459,    -1,  1009,    -1,    -1,   384,
     505,   386,   507,    -1,    -1,    -1,   391,   392,   393,    -1,
      -1,   102,    -1,    -1,    -1,    -1,    -1,   108,   109,   110,
    1033,    -1,   407,   528,    -1,    -1,    -1,   492,    -1,   534,
     495,    -1,    -1,    -1,    -1,    -1,    -1,    52,    -1,    54,
      55,    56,    57,    58,    -1,    -1,   431,    -1,  1245,  1246,
    1247,   142,    -1,    -1,   145,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,  1259,    78,    -1,    -1,    -1,    -1,   573,   574,
     455,   576,   577,    -1,    -1,    -1,    -1,    92,    -1,    -1,
     585,    -1,   587,    -1,    -1,   590,    -1,   102,    -1,    33,
      34,    35,    36,   108,   109,   110,   561,  1294,    -1,    -1,
      -1,    -1,    -1,   488,    -1,    49,    50,    51,    52,    -1,
      -1,    -1,    56,    -1,    -1,    59,    60,    61,    62,    63,
     585,  1318,   507,    -1,    -1,   590,    -1,   142,    -1,    -1,
     145,   636,    -1,   638,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   646,   157,   528,    -1,    -1,    -1,    91,    92,   534,
      -1,   656,    -1,   658,   659,    99,    -1,    -1,   102,    -1,
      -1,   105,   106,   668,   669,    -1,    -1,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   573,   574,
     695,   576,   577,    -1,    -1,    -1,    -1,   141,    -1,    -1,
     585,    -1,   707,    -1,   148,   590,   671,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     0,    -1,   689,    -1,   691,    -1,    -1,    -1,
       8,     9,    10,    -1,    -1,    13,    14,    15,    -1,    17,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    26,    27,
      -1,   636,    -1,   638,    -1,     2,    -1,    -1,    -1,    37,
      38,   646,    40,    41,    42,    43,    44,    -1,    -1,    16,
      17,   656,    -1,   658,   659,   740,   781,    -1,    -1,    -1,
      -1,    -1,    -1,   668,   669,    -1,    -1,    -1,    -1,    -1,
      68,    69,    -1,    -1,    -1,    -1,    52,    -1,    54,    55,
      56,    57,    58,    -1,    -1,    -1,    53,    54,   773,    -1,
     695,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    97,
      98,    68,    78,    -1,    -1,   830,    -1,   832,   833,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   802,    -1,    25,
      -1,    -1,    -1,   121,    -1,    -1,   102,    94,    95,    -1,
      -1,    98,   108,   109,   110,    -1,   103,    -1,   823,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   152,   153,    -1,    -1,    -1,   157,
     158,    -1,   160,    -1,   162,    -1,   142,    -1,    -1,   145,
      -1,   896,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    -1,   162,   912,    -1,    95,
      96,    -1,   917,   918,    -1,   101,    -1,    -1,   923,   924,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   932,   933,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     905,    -1,    -1,   948,    -1,   830,    -1,   832,   833,    -1,
     136,    -1,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   927,    -1,   969,    -1,    -1,    -1,    -1,    -1,
     217,    -1,    -1,    -1,    -1,    -1,    -1,   942,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   992,   993,   994,
      -1,    -1,    -1,    -1,   241,    -1,    -1,   244,   245,   964,
     247,    -1,    -1,    -1,  1009,   252,    -1,    -1,    -1,    -1,
      -1,   896,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   269,  1028,    -1,   990,    -1,   912,    -1,    -1,
      -1,    -1,   917,   918,    -1,    -1,    -1,    -1,   923,   924,
      -1,  1046,    -1,    -1,  1049,    -1,    -1,   932,   933,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,  1027,   948,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   319,    -1,   321,   322,   323,   324,    -1,   326,
     327,    -1,    -1,    -1,   969,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,  1001,  1002,    -1,    -1,    -1,  1102,    -1,    -1,
     347,  1106,    -1,    -1,  1109,  1110,    -1,   992,   993,   994,
      -1,    -1,    -1,    -1,  1119,    -1,    52,    -1,    54,    55,
      56,    57,    58,    -1,  1009,    -1,    -1,    -1,    -1,    -1,
      -1,  1136,    -1,    -1,    -1,    -1,    -1,   384,    -1,   386,
      -1,    -1,    78,  1028,   391,   392,   393,  1112,    -1,  1058,
    1059,    -1,    -1,  1062,    -1,    -1,    92,    -1,  1163,    -1,
     407,  1046,    -1,    -1,  1049,    -1,   102,    -1,    -1,    -1,
    1079,    -1,   108,   109,   110,    -1,    -1,    -1,    -1,    -1,
    1185,    -1,    -1,    -1,   431,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,  1197,    -1,    -1,    -1,    -1,    -1,  1203,    -1,
      -1,    -1,  1111,    -1,    -1,    -1,   142,    -1,   455,   145,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,  1106,    -1,    -1,  1109,  1110,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,  1119,    -1,    -1,    -1,    -1,     2,
    1149,   488,    -1,  1152,    -1,  1250,    -1,    -1,    -1,    -1,
      -1,  1256,  1257,    16,    17,    -1,    -1,  1166,  1167,  1168,
     507,    -1,  1171,  1172,    -1,    -1,    -1,  1176,  1177,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,  1163,    -1,
      -1,   528,    -1,    -1,    -1,    -1,    -1,   534,    -1,    -1,
      53,    54,    -1,    -1,  1299,    -1,    -1,    -1,  1001,  1002,
    1185,    -1,    -1,    -1,    -1,    68,    -1,    -1,    -1,    -1,
      -1,    -1,  1197,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   573,   574,    -1,   576,
     577,    94,    95,    -1,    -1,    98,    -1,    -1,   585,    -1,
     103,    -1,    -1,   590,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    33,    34,    35,    36,  1058,  1059,    -1,    -1,  1062,
      -1,    -1,    -1,    -1,    -1,  1250,    -1,    49,    50,    51,
    1279,  1256,  1257,    -1,  1283,  1284,  1079,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,     2,   636,
      -1,   638,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   646,
      -1,    52,    -1,    54,    55,    56,    57,    58,  1111,   656,
      -1,   658,   659,    -1,  1299,    -1,    -1,    -1,    -1,    -1,
    1329,   668,   669,    -1,    -1,    -1,    -1,    78,    -1,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,    53,
      54,    92,    -1,    57,    -1,    -1,  1149,    -1,   695,  1152,
      -1,   102,    -1,    -1,   217,    -1,    -1,    -1,   109,   110,
      -1,    -1,    -1,  1166,  1167,  1168,   148,    -1,  1171,  1172,
      -1,    -1,    86,  1176,  1177,    -1,    -1,    -1,   241,    -1,
      -1,   244,   245,    -1,   247,    99,   100,   101,    -1,   252,
      -1,   142,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   269,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    -1,    -1,    -1,    -1,    95,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   319,    -1,   321,   322,
     323,   324,    -1,   326,   327,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,  1279,    -1,    -1,    -1,
    1283,  1284,    -1,   830,   347,   832,   833,   136,    -1,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,    -1,
      -1,    -1,    -1,   217,    -1,    -1,   155,    -1,    -1,    -1,
      -1,     2,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   384,    -1,   386,    -1,    -1,  1329,    -1,   391,   392,
     393,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   252,    -1,
      -1,    -1,    -1,    -1,   407,    -1,    -1,    -1,    -1,   896,
      -1,    -1,    -1,    -1,    -1,   269,    -1,    -1,  1001,  1002,
      -1,    -1,    53,    54,    -1,   912,    57,    -1,   431,    -1,
     917,   918,    -1,    -1,    -1,   289,   923,   924,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   932,   933,    -1,    -1,    -1,
      -1,    -1,   455,    -1,    -1,    86,    -1,    -1,    -1,    -1,
      -1,   948,    -1,    -1,    -1,   319,    -1,    -1,    99,   100,
     101,    -1,    -1,    -1,    -1,  1058,  1059,    -1,    -1,  1062,
      -1,    -1,   969,    -1,    -1,   488,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   347,   348,    -1,  1079,  1001,  1002,    -1,
      -1,    -1,    -1,    -1,   507,   992,   993,   994,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,  1009,    -1,    -1,   528,    -1,    -1,  1111,    -1,
      -1,   534,   386,    -1,    -1,    -1,    -1,   391,    -1,   393,
      -1,  1028,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,  1058,  1059,    -1,    -1,  1062,  1046,
      -1,    -1,  1049,    -1,    -1,    -1,  1149,    -1,    -1,  1152,
     573,   574,    -1,   576,   577,  1079,    -1,    -1,    -1,    -1,
      -1,    -1,   585,  1166,  1167,  1168,   217,   590,  1171,  1172,
      -1,    -1,    -1,  1176,  1177,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,  1111,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,  1106,
      -1,   252,  1109,  1110,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,  1119,   636,   488,   638,    -1,    -1,   269,    -1,
      -1,    -1,    -1,   646,    -1,  1149,    -1,    -1,  1152,    -1,
      -1,   505,    -1,   656,    -1,   658,   659,    -1,   289,    -1,
      -1,    -1,  1166,  1167,  1168,   668,   669,  1171,  1172,    -1,
      -1,    -1,  1176,  1177,   528,    -1,  1163,    -1,    -1,    -1,
     534,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   319,    -1,
      -1,    -1,   695,    -1,    -1,    -1,  1279,    -1,  1185,    -1,
    1283,  1284,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    1197,    -1,    -1,    -1,    -1,    -1,   347,   348,    -1,    -1,
     574,    -1,    -1,   577,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   587,     2,    -1,   590,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,  1329,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   386,    -1,    -1,    -1,    -1,
     391,    -1,   393,  1250,    -1,    -1,    -1,    -1,    -1,  1256,
    1257,    -1,    -1,    -1,    -1,  1279,    -1,    -1,    -1,  1283,
    1284,    -1,   636,    -1,   638,    53,    54,    -1,    -1,    57,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   658,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,  1299,    -1,   668,   669,    -1,    -1,    86,    -1,
      -1,    -1,    -1,    -1,    -1,  1329,    -1,   830,    -1,   832,
     833,    99,   100,   101,     2,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,  1001,  1002,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   707,    -1,    -1,    -1,   488,    -1,    -1,
      -1,    -1,   716,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   505,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    53,    54,    -1,    -1,    57,
      -1,    -1,    -1,   896,    -1,    -1,    -1,   528,    -1,    -1,
      -1,  1058,  1059,   534,    -1,  1062,    -1,    -1,    -1,   912,
      -1,    -1,    -1,    -1,   917,   918,    -1,    -1,    86,    -1,
     923,   924,  1079,    -1,    -1,    -1,    -1,   781,    -1,   932,
     933,    99,   100,   101,   102,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   574,    -1,   948,   577,    -1,    -1,   217,
      -1,    -1,    -1,    -1,  1111,    -1,   587,    -1,    -1,   590,
      -1,    -1,    -1,    -1,    -1,    -1,   969,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   830,    -1,    -1,   833,
      -1,    -1,    -1,    -1,   252,    -1,    -1,    -1,    -1,   992,
     993,   994,  1149,    -1,    -1,  1152,    -1,    -1,    -1,    -1,
      -1,   269,    -1,    -1,    -1,   636,  1009,   638,    -1,  1166,
    1167,  1168,    -1,    -1,  1171,  1172,    -1,    -1,    -1,  1176,
    1177,   289,    -1,    -1,    -1,  1028,    -1,   658,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   668,   669,    -1,
      -1,    -1,    -1,  1046,    -1,    -1,  1049,    -1,    -1,   217,
      -1,   319,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   917,   918,    -1,    -1,    -1,    -1,   923,
     924,    -1,    -1,    -1,    -1,    -1,   707,    -1,   932,   347,
     348,    -1,    -1,    -1,   252,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   948,    -1,    -1,    -1,    -1,    -1,
      -1,   269,    -1,  1106,    -1,    -1,  1109,  1110,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   969,  1119,    -1,   386,    -1,
      -1,   289,  1279,   391,    -1,   393,  1283,  1284,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   992,   993,
     994,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     781,   319,    -1,    -1,    -1,  1009,    -1,    -1,    -1,    -1,
    1163,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,  1329,    -1,  1028,    -1,    -1,    -1,    -1,   347,
     348,    -1,  1185,    -1,    -1,    -1,    -1,     2,    -1,    -1,
      -1,    -1,  1046,    -1,  1197,  1049,    -1,    -1,    -1,   830,
      -1,    -1,   833,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,  1001,  1002,   386,    -1,
     488,    -1,    -1,   391,    -1,   393,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   505,    53,    54,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,  1250,  1102,    -1,
      -1,    -1,  1106,  1256,  1257,  1109,  1110,    -1,    -1,    -1,
     528,    -1,    -1,    -1,    -1,  1119,   534,    -1,    -1,    -1,
      -1,    -1,    -1,  1058,  1059,    -1,    -1,  1062,    -1,    -1,
      -1,    -1,  1136,    -1,    99,    -1,   917,   918,    -1,    -1,
      -1,    -1,   923,   924,  1079,    -1,  1299,    -1,    -1,    -1,
      -1,   932,    -1,    -1,    -1,    -1,   574,    -1,    -1,   577,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   948,    -1,   587,
     488,    -1,   590,    -1,    -1,    -1,  1111,    -1,    -1,    -1,
      -1,  1185,    -1,    -1,    -1,    -1,    -1,   505,   969,    -1,
      -1,    -1,    -1,  1197,    -1,    -1,    -1,    -1,    -1,  1203,
      -1,    -1,    -1,    -1,    -1,  1001,  1002,    -1,    -1,    -1,
     528,   992,   993,   994,  1149,    -1,   534,  1152,   636,    -1,
     638,    -1,    -1,    -1,    -1,    -1,    -1,    -1,  1009,    -1,
      -1,  1166,  1167,  1168,    -1,    -1,  1171,  1172,    -1,    -1,
     658,  1176,  1177,    -1,    -1,    -1,    -1,  1028,    -1,    -1,
     668,   669,   217,    -1,     2,    -1,   574,    -1,    -1,   577,
      -1,    -1,  1058,  1059,    -1,  1046,  1062,    -1,  1049,   587,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,  1079,    -1,    -1,    -1,   252,    -1,   707,
      -1,    -1,    -1,    -1,    -1,  1299,    -1,    -1,    -1,    -1,
      -1,  1001,  1002,    -1,   269,    53,    54,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,  1111,    -1,    -1,   636,    -1,
     638,  1102,    -1,    -1,    -1,  1106,    -1,    -1,  1109,  1110,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,  1119,    -1,
     658,    -1,    -1,    -1,  1279,    -1,    -1,    -1,  1283,  1284,
      -1,    -1,    -1,  1149,   319,  1136,  1152,    -1,  1058,  1059,
      -1,    -1,  1062,   781,    -1,    -1,    -1,    -1,    -1,    -1,
    1166,  1167,  1168,    -1,    -1,  1171,  1172,    -1,    -1,  1079,
    1176,  1177,   347,  1001,  1002,    -1,    -1,    -1,    -1,   707,
      -1,    -1,    -1,    -1,  1329,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,  1185,    -1,    -1,    -1,    -1,    -1,
      -1,  1111,   830,    -1,    -1,   833,  1197,    -1,    -1,    -1,
      -1,   386,  1203,    -1,    -1,    -1,   391,    -1,   393,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    1058,  1059,    -1,    -1,  1062,    -1,    -1,    -1,    -1,  1149,
      -1,    -1,  1152,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,  1079,    -1,   781,    -1,    -1,  1166,  1167,  1168,   217,
      -1,  1171,  1172,    -1,    -1,    -1,  1176,  1177,    -1,    -1,
      -1,    -1,    -1,  1279,    -1,    -1,    -1,  1283,  1284,    -1,
      -1,    -1,    -1,  1111,    -1,    -1,    -1,    -1,    -1,   917,
     918,    -1,    -1,    -1,   252,   923,   924,    -1,    -1,    -1,
      -1,    -1,   830,    -1,   932,   833,    -1,    -1,  1299,    -1,
      -1,   269,    -1,   488,    -1,    -1,    -1,    -1,    -1,    -1,
     948,  1149,    -1,  1329,  1152,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,  1166,  1167,
    1168,   969,    -1,  1171,  1172,    -1,    -1,    -1,  1176,  1177,
      -1,    -1,    -1,   528,    -1,    -1,    -1,    -1,    -1,   534,
      -1,   319,    -1,    -1,   992,   993,   994,    -1,    -1,  1279,
      -1,    -1,    -1,  1283,  1284,    -1,    -1,    -1,    -1,    -1,
      -1,  1009,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   347,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   574,
    1028,    -1,   577,    -1,   932,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   590,    -1,    -1,  1046,  1329,
     948,  1049,    -1,    -1,    -1,    -1,    -1,    -1,   386,    -1,
      -1,    -1,    -1,   391,    -1,   393,    -1,    -1,    -1,    -1,
      -1,   969,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,  1279,    -1,    -1,    -1,  1283,  1284,    -1,    -1,    -1,
      -1,   636,    -1,   638,   992,   993,    -1,    -1,    -1,    33,
      34,    35,    36,    -1,  1102,    -1,    -1,    -1,  1106,    -1,
      -1,  1109,  1110,   658,    -1,    49,    50,    51,    52,    -1,
      -1,  1119,    56,   668,   669,    59,    60,    61,    62,    63,
    1028,  1329,    -1,    -1,    -1,    -1,    -1,    -1,  1136,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,  1046,    -1,
      -1,  1049,    -1,    -1,    -1,    -1,    -1,    91,    92,    -1,
     488,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,    -1,
      -1,   105,   106,    -1,   108,    -1,    -1,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,  1185,    -1,    -1,
      -1,    -1,    16,    17,    -1,    -1,    -1,    -1,    -1,  1197,
     528,    -1,    -1,    -1,  1102,  1203,   534,   141,  1106,    -1,
      -1,  1109,  1110,    -1,   148,    -1,    -1,    -1,    -1,    -1,
      -1,  1119,    -1,    -1,    48,    49,    50,    51,   162,    -1,
      -1,    55,    56,    -1,    -1,    -1,    -1,    -1,  1136,    -1,
      -1,    -1,    -1,    -1,    68,    69,   574,    -1,    -1,   577,
      -1,    78,    79,    80,    81,    82,    83,    84,    85,    -1,
      87,    88,   590,    -1,    -1,    -1,    -1,    -1,    95,    96,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   103,
      -1,    -1,    -1,    -1,    -1,   830,    -1,  1185,   833,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,  1197,
      -1,  1299,    -1,    -1,    -1,  1203,    -1,    -1,   636,    -1,
     638,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     658,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     668,   669,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   917,   918,    -1,    -1,    -1,    -1,   923,   924,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   932,    -1,    -1,
      -1,   215,    -1,    -1,   218,   219,   220,    -1,   222,    -1,
      -1,  1299,    -1,   948,    -1,    -1,    -1,    -1,    -1,    33,
      34,    35,    36,    -1,    -1,    -1,    -1,   241,    -1,    -1,
     244,    -1,    -1,    -1,   969,    49,    50,    51,    52,    -1,
      -1,    -1,    56,    -1,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   992,   993,   994,
      -1,    -1,    -1,    -1,    78,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,  1009,    -1,    -1,    91,    92,    -1,
      -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,    -1,
      -1,   105,   106,  1028,   108,   109,    -1,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,
      -1,  1046,   830,    -1,  1049,   833,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   141,    -1,    -1,
      -1,    -1,    -1,    -1,   148,    -1,    -1,    -1,   352,   353,
     354,   355,   356,    -1,    -1,   359,   360,   361,   362,   363,
     364,   365,   366,    -1,   368,    -1,    -1,   371,   372,   373,
     374,   375,   376,   377,   378,   379,   380,  1102,    -1,    -1,
     384,  1106,    -1,    -1,  1109,  1110,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,  1119,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   917,
     918,    -1,    -1,    -1,    -1,   923,   924,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   932,    -1,    -1,   431,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    52,    53,
     948,    -1,    56,    -1,    -1,    -1,   450,   451,    -1,    -1,
      -1,   455,    -1,    -1,    -1,   459,    -1,   461,    -1,    -1,
    1185,   969,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    -1,  1197,    87,    88,   479,    -1,    -1,    -1,    93,
      94,    95,    96,    -1,   992,   993,   994,    -1,   492,    -1,
      -1,   495,    -1,    -1,   108,   109,    -1,    -1,    -1,    -1,
      -1,  1009,    -1,   507,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    1028,   525,    -1,    -1,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,    -1,   149,   150,    -1,  1046,    -1,
      -1,  1049,   156,   157,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   561,    78,    79,
      80,    81,    82,    83,    84,    -1,    -1,    87,    88,   573,
      -1,    -1,   576,    -1,  1299,    95,    96,    -1,    -1,    -1,
      -1,   585,    -1,    -1,    -1,    -1,   590,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,  1106,    -1,
      -1,  1109,  1110,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,  1119,    -1,    -1,    -1,    -1,    -1,    -1,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   646,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   656,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   668,   669,    -1,   671,   672,   673,
     674,    -1,    -1,    -1,    -1,    -1,    -1,  1185,    -1,    -1,
      -1,   685,   686,    -1,    -1,   689,    -1,   691,    -1,  1197,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   701,     0,     1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,   740,    39,    -1,    -1,
      -1,    -1,    -1,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    61,
      62,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,   773,
      -1,    -1,    -1,    -1,    76,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      92,  1299,    -1,    -1,    -1,    -1,    -1,    99,   802,    -1,
     102,   103,    -1,   105,   106,    -1,   108,    -1,    -1,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,   823,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    52,    53,
      -1,    -1,    56,    -1,    -1,    -1,   148,   149,   150,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   160,    -1,
     162,    -1,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    -1,    -1,    87,    88,    -1,    -1,    -1,    -1,    93,
      94,    95,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   896,    -1,   108,   109,    -1,    -1,    -1,    -1,
      -1,   905,    -1,    -1,    -1,    -1,    -1,    -1,   912,    -1,
      -1,    -1,   916,   917,   918,    -1,    -1,    -1,    -1,   923,
     924,    -1,    -1,   927,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,    -1,   149,   150,    -1,   942,    -1,
      -1,    -1,   156,   157,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   957,   958,   959,    -1,   961,   962,    -1,
     964,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   986,   987,    -1,    -1,   990,    -1,    -1,    -1,
     994,   995,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,  1009,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     0,     1,    -1,     3,     4,     5,
       6,     7,    -1,  1027,    -1,    11,    12,    -1,    -1,  1033,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    61,    62,    63,    64,    65,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      76,    77,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    91,    92,    -1,  1112,    -1,
      -1,    -1,    -1,    99,    -1,    -1,   102,   103,    -1,   105,
     106,    -1,   108,    -1,    -1,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,   121,    -1,    -1,    -1,    -1,
       0,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     8,     9,
      10,    -1,    -1,    13,    14,    15,    -1,    17,    -1,    -1,
      -1,    -1,   148,   149,   150,    25,    26,    27,    28,    29,
      -1,    -1,    -1,    -1,   160,    -1,   162,    37,    38,    -1,
      40,    41,    42,    43,    44,    -1,    -1,    -1,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    -1,    -1,    -1,    -1,    95,    96,    -1,    68,    69,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    -1,    -1,    -1,    -1,    95,    96,    97,    98,    -1,
     100,   101,    -1,    -1,    -1,    -1,  1250,   107,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,    -1,    -1,
      -1,   121,    -1,    -1,   124,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,    -1,    -1,
      -1,    -1,   152,   153,   154,   155,     0,    -1,   158,   159,
     160,    -1,   162,    -1,     8,     9,    10,    -1,    -1,    13,
      14,    15,    -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    25,    -1,    27,    28,    29,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    -1,    40,    41,    42,    43,
      44,    -1,    -1,    -1,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    -1,    -1,    -1,
      -1,    95,    96,    -1,    68,    69,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    -1,    -1,    -1,
      -1,    95,    96,    97,    98,    -1,   100,   101,    -1,    -1,
      -1,    -1,   136,   107,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,    -1,    -1,    -1,   121,    -1,    -1,
     124,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   162,    -1,
      -1,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,    -1,    -1,    -1,    -1,    -1,   153,
     154,   155,     0,    -1,   158,   159,   160,    -1,   162,    -1,
       8,     9,    10,    -1,    -1,    13,    14,    15,    -1,    17,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,    27,
      28,    29,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    -1,    40,    41,    42,    43,    44,    -1,    -1,    -1,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    -1,    -1,    -1,    -1,    95,    96,    -1,
      68,    69,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    -1,    -1,    -1,    -1,    95,    96,    97,
      98,    -1,   100,   101,    -1,    -1,    -1,    -1,   136,   107,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
      -1,    -1,    -1,   121,    -1,    -1,   124,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
      -1,    -1,    -1,    -1,    -1,   153,   154,   155,     0,    -1,
     158,   159,   160,    -1,   162,    -1,     8,     9,    10,    -1,
      -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    25,    26,    27,    28,    29,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    43,    44,    -1,    -1,    -1,    78,    79,    80,    81,
      82,    83,    84,    -1,    -1,    87,    88,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    96,    -1,    68,    69,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    -1,
      -1,    -1,    -1,    95,    96,    97,    98,    -1,    -1,   101,
      -1,    -1,    -1,    -1,    -1,   107,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,    -1,    -1,    -1,   121,
      -1,    -1,   124,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,    -1,    -1,    -1,    -1,
     152,   153,   154,   155,     0,    -1,   158,   159,   160,    -1,
     162,    -1,     8,     9,    10,    -1,    -1,    13,    14,    15,
      -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,
      26,    27,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    43,    44,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    68,    69,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    -1,    -1,    -1,    -1,    95,
      96,    97,    98,    -1,    -1,   101,    -1,    -1,    -1,    -1,
      -1,   107,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   121,    -1,    -1,   124,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,    -1,    -1,    -1,    -1,   152,   153,   154,   155,
       0,    -1,   158,   159,   160,    -1,   162,    -1,     8,     9,
      10,    -1,    -1,    13,    14,    15,    -1,    17,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    25,    26,    27,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,
      40,    41,    42,    43,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    -1,    -1,    -1,    -1,    95,    96,    97,    98,    -1,
      -1,   101,    -1,    -1,    -1,    -1,    -1,   107,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   121,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   136,    -1,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,    -1,    -1,
      -1,    -1,   152,   153,   154,   155,     0,   157,   158,   159,
     160,    -1,   162,    -1,     8,     9,    10,    -1,    -1,    13,
      14,    15,    -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    25,    -1,    27,    28,    29,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    -1,    40,    41,    42,    43,
      44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    68,    69,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    -1,    -1,    -1,
      -1,    95,    96,    97,    98,    -1,    -1,   101,    -1,    -1,
      -1,    -1,    -1,   107,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   121,    -1,    -1,
     124,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,    -1,    -1,    -1,    -1,    -1,   153,
     154,   155,     0,    -1,   158,   159,   160,    -1,   162,    -1,
       8,     9,    10,    -1,    -1,    13,    14,    15,    -1,    17,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    26,    27,
      28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    -1,    40,    41,    42,    43,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      68,    69,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    -1,    -1,    -1,    -1,    95,    96,    97,
      98,    -1,    -1,   101,    -1,    -1,    -1,    -1,    -1,   107,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   121,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   136,    -1,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
      -1,    -1,    -1,    -1,   152,   153,   154,   155,     0,   157,
     158,   159,   160,    -1,   162,    -1,     8,     9,    10,    -1,
      -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    25,    -1,    27,    28,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    -1,
      -1,    -1,    -1,    95,    96,    97,    98,    -1,   100,   101,
      -1,    -1,    -1,    -1,    -1,   107,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   121,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   135,   136,    -1,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,    -1,    -1,    -1,    -1,
     152,   153,   154,   155,     0,    -1,   158,   159,   160,    -1,
     162,    -1,     8,     9,    10,    -1,    -1,    13,    14,    15,
      -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,
      -1,    27,    28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    43,    44,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    68,    69,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    -1,    -1,    -1,    -1,    95,
      96,    97,    98,    -1,    -1,   101,    -1,    -1,    -1,    -1,
      -1,   107,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   121,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     136,    -1,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,    -1,    -1,    -1,    -1,    -1,   153,   154,   155,
       0,   157,   158,   159,   160,    -1,   162,    -1,     8,     9,
      10,    44,    -1,    -1,    14,    15,    -1,    17,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    25,    26,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,
      40,    41,    42,    43,    44,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    -1,    -1,
      -1,    -1,    95,    96,    -1,    -1,    -1,    -1,    68,    69,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    -1,    -1,    -1,    -1,    95,    96,    97,    98,    -1,
     100,   101,    -1,   136,    -1,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,    -1,    -1,    -1,    -1,    -1,
      -1,   121,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   135,   136,    -1,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,    -1,    -1,
      -1,    -1,   152,   153,   154,   155,     0,    -1,   158,    -1,
     160,    -1,   162,    -1,     8,     9,    10,    -1,    -1,    -1,
      14,    15,    -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    25,    26,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    -1,    40,    41,    42,    43,
      44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    68,    69,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    -1,    -1,    -1,
      -1,    95,    96,    97,    98,    -1,   100,   101,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   121,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   135,   136,    -1,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,    -1,    -1,    -1,    -1,   152,   153,
     154,   155,     0,    -1,   158,    -1,   160,    -1,   162,    -1,
       8,     9,    10,    -1,    -1,    -1,    14,    15,    -1,    17,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    -1,    40,    41,    42,    43,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      68,    69,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    -1,    -1,    -1,    -1,    95,    96,    97,
      98,    -1,   100,   101,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   121,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   135,   136,    -1,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
      -1,    -1,    -1,    -1,   152,   153,   154,   155,     0,    -1,
     158,    -1,   160,    -1,   162,    -1,     8,     9,    10,    -1,
      -1,    -1,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    25,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    -1,
      -1,    -1,    -1,    95,    96,    97,    98,    -1,   100,   101,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   121,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   135,   136,    -1,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,    -1,    -1,    -1,    -1,
     152,   153,   154,   155,    -1,    -1,   158,    -1,   160,     1,
     162,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    -1,    -1,    15,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    61,
      62,    63,    64,    65,    -1,    -1,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    76,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      92,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,
     102,   103,    -1,   105,   106,    -1,   108,    -1,    -1,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,    -1,
      -1,     1,    -1,     3,     4,     5,     6,     7,    -1,    -1,
      10,    11,    12,    -1,    14,    15,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,   148,   149,   150,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,   160,    39,
     162,    -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    61,    62,    63,    64,    65,    -1,    -1,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    91,    92,    -1,    -1,    -1,    -1,    -1,    -1,    99,
      -1,    -1,   102,   103,    -1,   105,   106,    -1,   108,    -1,
      -1,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    -1,    -1,     1,    -1,     3,     4,     5,     6,     7,
      -1,    -1,    10,    11,    12,    -1,    -1,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    -1,   148,   149,
     150,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
     160,    39,   162,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    61,    62,    63,    64,    65,    -1,    -1,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,
      -1,    99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,
     108,    -1,    -1,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,    -1,     1,    -1,     3,     4,     5,
       6,     7,    -1,    -1,    10,    11,    12,    -1,    -1,    15,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    25,
     148,   149,   150,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,   160,    39,   162,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    61,    62,    63,    64,    65,
      -1,    -1,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      76,    77,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    91,    92,    -1,    -1,    -1,
      -1,    -1,    -1,    99,    -1,    -1,   102,   103,    -1,   105,
     106,    -1,   108,    -1,    -1,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,    -1,    -1,     1,    -1,     3,
       4,     5,     6,     7,    -1,    -1,    10,    11,    12,    -1,
      -1,    15,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,   148,   149,   150,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,   160,    39,   162,    -1,    -1,    -1,
      -1,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    61,    62,    63,
      64,    65,    -1,    -1,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    76,    77,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    92,    -1,
      -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,   103,
      -1,   105,   106,    -1,   108,    -1,    -1,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,    -1,    -1,     1,
      -1,     3,     4,     5,     6,     7,    -1,     9,    10,    11,
      12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,   148,   149,   150,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,   160,    39,   162,    -1,
      -1,    -1,    -1,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    61,
      62,    63,    64,    65,    -1,    -1,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    76,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      92,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,
     102,   103,    -1,   105,   106,    -1,   108,    -1,    -1,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,    -1,
      -1,     1,    -1,     3,     4,     5,     6,     7,    -1,    -1,
      10,    11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,   148,   149,   150,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,   160,    39,
     162,    -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    61,    62,    63,    64,    65,    -1,    -1,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    91,    92,    -1,    -1,    -1,    -1,    -1,    -1,    99,
      -1,    -1,   102,   103,    -1,   105,   106,    -1,   108,    -1,
      -1,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    -1,    -1,     1,    -1,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,   148,   149,
     150,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
     160,    39,   162,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    61,    62,    63,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,
      -1,    99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,
     108,    -1,    -1,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     148,   149,   150,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     158,    -1,   160,     1,   162,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    61,    62,    63,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,
      -1,    99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,
     108,    -1,    -1,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     148,   149,   150,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     158,    -1,   160,     1,   162,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    61,    62,    63,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,
      -1,    99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,
     108,    -1,    -1,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     148,   149,   150,    -1,    -1,   153,    -1,    -1,    -1,    -1,
      -1,    -1,   160,     1,   162,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    61,    62,    63,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,
      -1,    99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,
     108,    -1,    -1,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     148,   149,   150,    -1,    -1,   153,    -1,     1,    -1,     3,
       4,     5,   160,     7,   162,    -1,    -1,    11,    12,    -1,
      -1,    -1,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    61,    62,    63,
      64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    76,    77,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    92,    -1,
      -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,   103,
      -1,   105,   106,    -1,    -1,    -1,    -1,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,    -1,     0,     1,
      -1,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    -1,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,   148,   149,   150,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,   160,    39,   162,    -1,
      -1,    -1,    -1,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    61,
      62,    63,    64,    65,    -1,    -1,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    76,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      92,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,
     102,   103,    -1,   105,   106,    -1,   108,    -1,    -1,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,   121,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   148,   149,   150,    -1,
      -1,   153,    -1,    -1,    -1,    -1,   158,    -1,   160,     0,
       1,    -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      61,    62,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    76,    77,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      91,    92,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,
      -1,   102,   103,    -1,   105,   106,    -1,   108,    -1,    -1,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   148,   149,   150,
      -1,    -1,   153,     3,     4,     5,    -1,     7,    -1,   160,
      -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    91,    92,    -1,    -1,    -1,    -1,    -1,    -1,    99,
      -1,    -1,   102,   103,    -1,   105,   106,    -1,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,   148,   149,
     150,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,   162,    -1,    -1,    -1,    -1,    46,    -1,    -1,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    61,    62,    63,    64,    65,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,    -1,
      99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,    -1,
      -1,    -1,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,   148,
     149,   150,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    -1,   162,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    -1,    -1,    87,
      88,    -1,    -1,    -1,    -1,    93,    94,    95,    96,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     108,   109,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
      -1,   149,   150,    -1,    -1,    -1,    -1,    -1,   156,   157,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    -1,    -1,    -1,
      -1,    -1,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    -1,    -1,    87,    88,    -1,    -1,    -1,    -1,
      93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   108,   109,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,    -1,   149,   150,    -1,    -1,
      -1,    -1,    -1,   156,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    -1,    56,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    -1,    -1,    87,    88,
      -1,    -1,    -1,    -1,    93,    94,    95,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,
     109,    -1,    -1,   112,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,    -1,
     149,   150,    -1,    -1,    -1,    -1,    -1,   156,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    -1,
      -1,    56,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      -1,    -1,    87,    88,    -1,    -1,    -1,    -1,    93,    94,
      95,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   108,   109,    -1,    -1,   112,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,    -1,   149,   150,    -1,    -1,    -1,    -1,
      -1,   156,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    -1,    -1,    56,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    -1,    -1,    87,    88,    -1,    -1,
      -1,    -1,    93,    94,    95,    96,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,   109,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,    -1,   149,   150,
       3,     4,     5,    -1,     7,   156,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    61,    62,
      63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,
     103,    -1,   105,   106,    -1,    -1,    -1,    -1,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,   148,    11,    12,    -1,    -1,
      -1,    16,   155,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    46,    -1,    -1,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    61,    62,    63,    64,
      65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    99,    -1,    -1,   102,   103,    -1,
     105,   106,    -1,    -1,    -1,    -1,   111,   112,   113,   114,
     115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     1,    -1,     3,     4,     5,     6,
       7,    -1,    -1,   148,    11,    12,    -1,    -1,    -1,    16,
     155,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    61,    62,    63,    64,    65,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,
      77,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    91,    92,    -1,    -1,    -1,    -1,
      -1,    -1,    99,    -1,    -1,   102,   103,    -1,   105,   106,
      -1,   108,    -1,    -1,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    -1,     1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,   148,   149,   150,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    -1,    -1,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    61,    62,    63,    64,    65,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      76,    77,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    91,    92,    -1,    -1,    -1,
      -1,    -1,    -1,    99,    -1,    -1,   102,   103,    -1,   105,
     106,    -1,    -1,    -1,    -1,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,
      -1,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,   148,   149,   150,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    46,    -1,    -1,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    76,    77,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    91,    92,    -1,    -1,
      -1,    -1,    -1,    -1,    99,    -1,    -1,   102,   103,    -1,
     105,   106,    -1,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,
      -1,    -1,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,   148,   149,   150,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    76,    77,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    92,    -1,
      -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,   103,
      -1,   105,   106,    -1,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,   148,   149,   150,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    76,    77,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    92,
      -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,
     103,    -1,   105,   106,    -1,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,   148,   149,   150,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    76,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      92,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,
     102,   103,    -1,   105,   106,    -1,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,   148,   149,   150,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    76,    77,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      91,    92,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,
      -1,   102,   103,    -1,   105,   106,    -1,   108,   109,    -1,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,   148,   149,   150,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    91,    92,    -1,    -1,    -1,    -1,    -1,    -1,    99,
      -1,    -1,   102,   103,    -1,   105,   106,    -1,    -1,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,   148,   149,
     150,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,    -1,
      99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,    -1,
     109,    -1,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,   148,
     149,   150,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,
      -1,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    61,    62,    63,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,
      -1,    99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,
     108,    -1,    -1,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
     148,   149,   150,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      -1,    -1,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    61,    62,    63,    64,    65,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,
      77,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    91,    92,    -1,    -1,    -1,    -1,
      -1,    -1,    99,    -1,    -1,   102,   103,    -1,   105,   106,
      -1,   108,    -1,    -1,   111,   112,   113,   114,   115,   116,
     117,   118,   119,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,   148,   149,   150,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    -1,    -1,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    61,    62,    63,    64,    65,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      76,    77,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    91,    92,    -1,    -1,    -1,
      -1,    -1,    -1,    99,    -1,    -1,   102,   103,    -1,   105,
     106,    -1,   108,    -1,    -1,   111,   112,   113,   114,   115,
     116,   117,   118,   119,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,
      -1,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,   148,   149,   150,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    46,    -1,    -1,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    61,    62,    63,    64,
      65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    76,    77,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    91,    92,    -1,    -1,
      -1,    -1,    -1,    -1,    99,    -1,    -1,   102,   103,    -1,
     105,   106,    -1,   108,    -1,    -1,   111,   112,   113,   114,
     115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,
      -1,    -1,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,   148,   149,   150,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    61,    62,    63,
      64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    76,    77,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    92,    -1,
      -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,   103,
      -1,   105,   106,    -1,   108,    -1,    -1,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,   148,   149,   150,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    61,    62,
      63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    76,    77,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    92,
      -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,
     103,    -1,   105,   106,    -1,    -1,    -1,    -1,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,   148,   149,   150,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    61,
      62,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    76,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      92,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,
     102,   103,    -1,   105,   106,    -1,    -1,    -1,    -1,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,   148,   149,   150,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      61,    62,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    76,    77,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      91,    92,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,
      -1,   102,   103,    -1,   105,   106,    -1,    -1,    -1,    -1,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,   148,   149,   150,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    61,    62,    63,    64,    65,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    91,    92,    -1,    -1,    -1,    -1,    -1,    -1,    99,
      -1,    -1,   102,   103,    -1,   105,   106,    -1,    -1,    -1,
      -1,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,   148,   149,
     150,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    61,    62,    63,    64,    65,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,    -1,
      99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,    -1,
      -1,    -1,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,   148,
     149,   150,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,
      -1,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    61,    62,    63,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,
     108,    -1,    -1,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
     148,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    61,    62,    63,    64,    65,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      99,    -1,    -1,   102,   103,    -1,   105,   106,    -1,   108,
      -1,    -1,   111,   112,   113,   114,   115,   116,   117,   118,
     119,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,   148,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    61,    62,    63,    64,    65,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    95,    -1,    -1,    -1,    99,
      -1,    -1,   102,   103,    -1,   105,   106,    -1,    -1,    -1,
      -1,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,   148,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      61,    62,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,
      -1,   102,   103,    -1,   105,   106,    -1,    -1,    -1,    -1,
     111,   112,   113,   114,   115,   116,   117,   118,   119,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,   148,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    61,
      62,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,
     102,   103,    -1,   105,   106,    -1,    -1,    -1,    -1,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,   148,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    61,    62,
      63,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,
     103,    -1,   105,   106,    -1,    -1,    -1,    -1,   111,   112,
     113,   114,   115,   116,   117,   118,   119,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,
      -1,    -1,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,   148,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    61,    62,    63,
      64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    99,    -1,    -1,   102,   103,
      -1,   105,   106,    33,    34,    35,    36,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,    -1,    -1,    49,
      50,    51,    52,    -1,    -1,    -1,    56,    -1,    -1,    59,
      60,    61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   148,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    91,    92,    -1,    -1,    -1,    52,    53,    -1,    99,
      56,    -1,   102,    -1,    -1,   105,   106,    -1,   108,    -1,
      -1,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    -1,
      -1,    87,    88,    -1,    -1,    -1,    -1,    93,    94,    95,
      96,   141,    -1,    -1,    -1,    -1,    -1,    -1,   148,    -1,
      -1,    -1,   108,   109,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,    -1,   149,   150,    52,    53,    -1,    -1,    56,
     156,   157,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    -1,    -1,
      87,    88,    -1,    -1,    -1,    -1,    93,    94,    95,    96,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   108,   109,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,    -1,   149,   150,    52,    53,    -1,    -1,    56,   156,
     157,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    -1,    -1,    87,
      88,    -1,    -1,    -1,    -1,    93,    94,    95,    96,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     108,   109,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
      -1,   149,   150,    52,    53,    -1,    -1,    56,   156,   157,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    -1,    -1,    87,    88,
      -1,    -1,    -1,    -1,    93,    94,    95,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,
     109,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,    -1,
     149,   150,    52,    53,    -1,    -1,    56,   156,   157,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    -1,    -1,    87,    88,    -1,
      -1,    -1,    -1,    93,    94,    95,    96,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,   109,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,    -1,   149,
     150,    52,    53,    -1,    -1,    56,   156,   157,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    -1,    -1,    87,    88,    -1,    -1,
      -1,    -1,    93,    94,    95,    96,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,   109,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,    -1,   149,   150,
      52,    53,    -1,    -1,    56,   156,   157,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    -1,    -1,    87,    88,    -1,    -1,    -1,
      -1,    93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   108,   109,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,    -1,   149,   150,    52,
      53,    -1,    -1,    56,   156,   157,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    -1,    -1,    87,    88,    -1,    -1,    -1,    -1,
      93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   108,   109,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,    -1,   149,   150,    52,    53,
      -1,    -1,    56,   156,   157,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    -1,    -1,    87,    88,    -1,    -1,    -1,    -1,    93,
      94,    95,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   108,   109,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,    -1,   149,   150,    52,    53,    -1,
      -1,    56,   156,   157,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      -1,    -1,    87,    88,    -1,    -1,    -1,    -1,    93,    94,
      95,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   108,   109,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,    -1,   149,   150,    52,    53,    -1,    -1,
      56,   156,   157,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    -1,
      -1,    87,    88,    -1,    -1,    -1,    -1,    93,    94,    95,
      96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   108,   109,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,    -1,   149,   150,    52,    53,    -1,    -1,    56,
     156,   157,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    -1,    -1,
      87,    88,    -1,    -1,    -1,    -1,    93,    94,    95,    96,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   108,   109,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,    -1,   149,   150,    52,    53,    -1,    -1,    56,   156,
     157,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    -1,    -1,    87,
      88,    -1,    -1,    -1,    -1,    93,    94,    95,    96,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     108,   109,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
      -1,   149,   150,    52,    53,    -1,    -1,    56,   156,   157,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    -1,    -1,    87,    88,
      -1,    -1,    -1,    -1,    93,    94,    95,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,
     109,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,    -1,
     149,   150,    52,    53,    -1,    -1,    56,   156,   157,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    -1,    -1,    87,    88,    -1,
      -1,    -1,    -1,    93,    94,    95,    96,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,   109,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,    -1,   149,
     150,    -1,    -1,    -1,    -1,    -1,   156
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int16 yystos[] =
{
       0,   164,   165,     0,     1,     3,     4,     5,     6,     7,
      11,    12,    16,    18,    19,    20,    21,    22,    23,    24,
      30,    31,    32,    33,    34,    35,    36,    39,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    59,    60,    61,    62,    63,    64,    65,    76,    77,
      91,    92,    99,   102,   103,   105,   106,   108,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   148,   149,   150,
     166,   167,   168,   180,   181,   183,   186,   190,   191,   197,
     198,   200,   201,   202,   204,   205,   206,   208,   209,   218,
     221,   240,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,   260,   269,   270,   294,   301,   302,   350,   351,
     352,   353,   354,   355,   357,   360,   362,   363,   376,   377,
     379,   380,   381,   382,   383,   384,   385,   386,   387,   425,
     439,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    56,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    87,    88,    93,    94,    95,    96,
     108,   109,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   149,   150,   156,   212,   213,   214,   216,   217,
     376,   179,   179,   179,    39,    58,    99,   102,   108,   109,
     110,   113,   149,   190,   191,   201,   209,   218,   225,   231,
     234,   236,   237,   250,   383,   384,   386,   387,   423,   424,
     231,   157,   228,   232,   233,   157,   162,   432,    54,   213,
     432,   152,   169,   170,   222,   439,    21,    22,    32,   200,
     218,   250,   269,   270,   218,   218,   218,    56,    47,   102,
     175,   176,   177,   181,   203,   204,   439,   175,   226,   236,
     423,   439,   225,   422,   423,   439,    46,    99,   148,   155,
     190,   191,   208,   240,   250,   383,   384,   387,   295,   212,
     366,   378,   382,   366,   367,   368,   161,   356,   356,   356,
     356,   381,   197,   218,   218,   160,   162,   431,   437,   438,
     179,    40,    41,    42,    43,    44,    37,    38,   157,   390,
     391,   392,   393,   439,   390,   392,    26,   152,   228,   232,
     261,   303,    28,   262,   300,   135,   155,   102,   108,   205,
     135,    25,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    95,    96,   101,   136,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   220,
     220,    69,    97,    98,   154,   429,   241,     1,   186,   193,
     193,   194,   195,   194,   193,   431,   438,    99,   202,   209,
     250,   275,   383,   384,   387,    52,    56,    95,    99,   210,
     211,   250,   383,   384,   387,   211,    33,    34,    35,    36,
      49,    50,    51,    52,    56,   157,   189,   212,   385,   420,
     231,   157,   232,    98,   429,   430,   303,   353,   100,   100,
     155,   225,    56,   225,   225,   225,   366,   390,   390,   135,
     101,   155,   235,   439,    98,   154,   429,   100,   100,   155,
     235,    92,   230,   231,   236,   397,   423,   439,   231,   186,
     432,   433,   186,    54,    64,    65,   182,   157,   222,   223,
     166,    98,   429,   100,   178,   203,   158,   431,   438,   433,
     242,   159,   155,   432,   436,   155,   436,   153,   436,   432,
      56,   381,   205,   207,   391,   155,    98,   154,   429,   292,
      66,   120,   122,   123,   369,   120,   120,   369,    67,   369,
     161,   358,   364,   361,   365,    78,   160,   168,   152,   193,
     193,   193,   193,   222,   224,   186,   186,    52,    54,    55,
      56,    57,    58,    78,    92,   102,   108,   109,   110,   142,
     145,   280,   338,   394,   396,   397,   398,   399,   400,   401,
     402,   403,   404,   407,   408,   409,   410,   411,   414,   415,
     416,   417,   418,   135,   248,   396,   135,   249,   304,   305,
     107,   199,   306,   307,   306,   222,   203,   155,   208,   155,
     222,   188,   218,   218,   218,   218,   218,   218,   218,   218,
     218,   218,   218,   218,   218,   187,   218,   218,   218,   218,
     218,   218,   218,   218,   218,   218,   218,    52,    53,    56,
     216,   228,   425,   426,   427,   230,   236,    52,    53,    56,
     216,   228,   426,   171,   175,    13,   271,   437,   271,   175,
     193,   175,   431,   244,    56,    98,   154,   429,    25,   193,
      52,    56,   210,   139,   388,    98,   154,   429,   247,   421,
      69,    98,   428,   231,   433,    52,    56,   426,   222,   222,
     215,   125,   135,   135,   222,   225,   225,   234,   237,   423,
      52,    56,   230,    52,    56,   222,   222,   424,   433,   155,
     433,   155,   433,   158,   213,   223,   218,   153,    56,   426,
     426,   222,   170,   433,   177,   158,   423,   155,   207,    52,
      56,   230,    52,    56,   293,   371,   370,   120,   359,   369,
      66,   120,   120,   359,    66,   120,   218,   175,   181,   102,
     108,   276,   277,   278,   279,   399,   155,   419,   439,   433,
     281,   282,   155,   395,   225,   155,   419,    34,    52,   155,
     395,    52,   155,   395,    52,    39,   184,   201,   218,   219,
     171,   437,   184,   219,   171,   153,   292,   304,    10,    68,
     268,   292,   268,   108,   197,   225,   236,   238,   239,   433,
     207,   155,   183,   185,   197,   209,   218,   225,   227,   239,
     250,   387,   313,   313,   432,   100,   100,   152,   228,   232,
     432,   434,   155,   100,   100,   228,   229,   232,   439,   268,
     222,   175,    13,   175,   268,    27,   272,   437,   268,    25,
     243,   314,    17,   265,   309,    52,    56,   230,    52,    56,
     194,   246,   389,   245,    52,    56,   210,   230,   171,   186,
     192,   433,   229,   232,   185,   218,   227,   185,   227,   213,
     225,    39,   235,   100,   100,   434,   100,   100,   397,   423,
     186,   227,   436,   205,   434,   179,   372,   375,   382,   387,
     356,   369,   356,   356,   356,   153,   278,   399,   155,   433,
     155,   418,   225,   135,   394,   401,   414,   416,   404,   408,
     410,   402,   411,   416,   400,   402,   432,    44,    44,   268,
     268,   293,   153,   293,   225,   155,    44,   207,    44,   135,
      44,    98,   154,   429,   311,   311,   137,   222,   222,   304,
     199,   159,   100,   222,   222,   199,     8,   263,   346,   439,
      14,    15,   266,   267,   273,   274,   439,   274,   196,   313,
     309,   268,   108,   225,   308,   268,   434,   175,   437,   193,
     171,   434,   268,   433,   189,   303,   300,   432,   222,   222,
     100,   222,   222,   433,   155,   433,   157,   297,   396,   373,
     433,   276,   279,   277,   155,   395,   155,   395,   419,   155,
     395,   155,   395,   395,   184,   219,   224,   224,   179,   179,
     108,   225,   224,   224,   222,   224,    52,    56,   230,    52,
      56,   312,   312,   218,   185,   227,   185,   227,   153,   222,
     185,   227,   185,   227,   225,   239,   347,   439,   174,   266,
     175,   193,   268,   268,   311,   268,   225,   155,   271,   268,
     171,   437,   268,   222,   396,   296,   175,   155,   155,   402,
     416,   402,   402,   218,   218,   140,   287,   288,   439,   287,
     225,   181,   181,   218,   434,    52,    56,    58,    91,    92,
      99,   102,   105,   106,   108,   113,   141,   294,   318,   319,
     320,   321,   324,   328,   329,   330,   333,   334,   335,   336,
     337,   338,   339,   340,   341,   342,   343,   344,   345,   350,
     351,   354,   355,   357,   360,   362,   363,   384,   408,   318,
     185,   227,   101,   348,   439,     9,   264,   349,   439,   172,
     271,   312,   108,   225,   175,   268,   289,   432,    29,   124,
     298,     0,   121,   374,   277,   395,   155,   395,   395,   395,
     280,   283,   286,   289,   400,   402,   403,   405,   406,   412,
     413,   416,   418,   175,   171,   341,   341,    56,   210,   312,
     319,   326,   327,   328,   329,   332,   434,   312,   432,   435,
      52,   366,    52,   102,   382,   101,   155,   140,   155,   155,
     319,    89,    90,    98,   154,   157,   322,   323,    52,    99,
     209,   250,   383,   384,   387,   271,   175,   175,   175,   317,
     318,   225,   274,   309,   310,   158,   160,   299,   175,   402,
     419,   289,   140,   281,   155,   284,   285,    99,   250,   155,
     419,   155,   284,   155,   284,   319,   434,   319,   330,   332,
     434,   155,   222,   153,   125,   193,   342,   326,   330,   324,
     331,   332,   113,   335,   339,   341,   341,   210,   312,   434,
     312,   433,   326,   329,   333,   326,   329,   333,    56,    98,
     154,   429,   175,   173,   273,   271,    40,    41,    52,   290,
     291,   398,   171,   153,   395,   140,   250,   283,   413,   416,
      56,    98,   405,   410,   402,   412,   416,   402,   433,   155,
     155,   325,   433,   155,   155,   366,   433,   433,   433,   434,
     434,   434,    52,    56,   230,    52,    56,   346,   349,   315,
     193,   193,   155,   432,   268,   155,   284,   155,   284,    52,
      56,   419,   155,   284,   155,   284,   284,   331,   333,   331,
     330,   332,   434,   175,   291,   402,   416,   402,   402,   155,
     435,   274,   314,   316,   284,   155,   284,   284,   284,   402,
     284
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int16 yyr1[] =
{
       0,   163,   165,   164,   166,   167,   167,   167,   168,   168,
     169,   170,   172,   173,   171,   174,   171,   175,   176,   176,
     176,   177,   178,   177,   179,   180,   182,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   183,   183,   183,   183,   183,
     183,   183,   183,   183,   183,   184,   184,   184,   185,   185,
     185,   186,   186,   186,   186,   186,   187,   186,   188,   186,
     186,   189,   190,   192,   191,   193,   193,   195,   196,   194,
     197,   197,   198,   198,   199,   200,   201,   201,   201,   201,
     201,   201,   201,   201,   201,   201,   201,   201,   202,   202,
     203,   203,   204,   204,   204,   204,   204,   204,   204,   204,
     204,   204,   205,   205,   206,   206,   207,   207,   208,   208,
     208,   208,   208,   208,   208,   208,   208,   209,   209,   209,
     209,   209,   209,   209,   209,   209,   210,   210,   211,   211,
     211,   212,   212,   212,   212,   212,   213,   213,   214,   215,
     214,   216,   216,   216,   216,   216,   216,   216,   216,   216,
     216,   216,   216,   216,   216,   216,   216,   216,   216,   216,
     216,   216,   216,   216,   216,   216,   216,   216,   216,   216,
     216,   217,   217,   217,   217,   217,   217,   217,   217,   217,
     217,   217,   217,   217,   217,   217,   217,   217,   217,   217,
     217,   217,   217,   217,   217,   217,   217,   217,   217,   217,
     217,   217,   217,   217,   217,   217,   217,   217,   217,   217,
     217,   217,   218,   218,   218,   218,   218,   218,   218,   218,
     218,   218,   218,   218,   218,   218,   218,   218,   218,   218,
     218,   218,   218,   218,   218,   218,   218,   218,   218,   218,
     218,   218,   218,   218,   218,   218,   218,   218,   218,   218,
     218,   218,   218,   218,   218,   218,   218,   219,   219,   219,
     220,   220,   220,   220,   221,   221,   222,   223,   224,   225,
     226,   226,   226,   226,   227,   227,   228,   228,   228,   229,
     229,   230,   230,   230,   230,   230,   231,   231,   231,   231,
     231,   233,   232,   234,   234,   235,   235,   236,   236,   236,
     236,   237,   237,   238,   238,   239,   239,   239,   240,   240,
     240,   240,   240,   240,   240,   240,   240,   240,   240,   241,
     240,   242,   240,   240,   240,   240,   240,   240,   240,   240,
     240,   240,   240,   240,   240,   240,   240,   240,   240,   240,
     240,   240,   240,   243,   240,   244,   240,   240,   240,   245,
     240,   246,   240,   247,   240,   248,   240,   249,   240,   240,
     240,   240,   240,   250,   251,   252,   253,   254,   255,   256,
     257,   258,   259,   260,   261,   262,   263,   264,   265,   266,
     267,   268,   268,   269,   270,   271,   271,   271,   272,   272,
     273,   273,   274,   274,   275,   275,   276,   276,   277,   277,
     278,   278,   278,   278,   278,   279,   279,   280,   280,   282,
     281,   283,   283,   283,   283,   284,   284,   285,   286,   286,
     286,   286,   286,   286,   286,   286,   286,   286,   286,   286,
     286,   286,   286,   287,   287,   288,   288,   289,   289,   290,
     290,   291,   291,   292,   293,   295,   296,   294,   297,   297,
     298,   299,   298,   300,   301,   301,   301,   301,   302,   302,
     302,   302,   302,   302,   302,   302,   302,   303,   303,   305,
     304,   307,   306,   308,   308,   308,   308,   309,   310,   310,
     311,   312,   313,   315,   314,   316,   316,   317,   317,   317,
     318,   318,   318,   318,   318,   318,   319,   320,   320,   321,
     321,   322,   323,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   325,   324,   324,   324,
     326,   326,   326,   326,   326,   326,   327,   327,   328,   328,
     329,   330,   330,   331,   331,   332,   333,   333,   333,   333,
     334,   334,   335,   335,   336,   336,   337,   337,   338,   339,
     339,   340,   340,   340,   340,   340,   340,   340,   340,   340,
     340,   341,   341,   341,   341,   341,   341,   341,   341,   341,
     341,   342,   343,   343,   344,   345,   345,   345,   346,   346,
     347,   347,   347,   348,   348,   349,   349,   350,   350,   351,
     352,   352,   352,   353,   354,   355,   356,   356,   357,   358,
     358,   359,   359,   360,   361,   361,   362,   363,   364,   364,
     365,   365,   366,   366,   367,   367,   368,   368,   369,   370,
     369,   371,   372,   373,   369,   374,   374,   375,   375,   376,
     376,   377,   378,   378,   379,   380,   380,   381,   381,   381,
     381,   382,   382,   382,   383,   383,   383,   384,   384,   384,
     384,   384,   384,   384,   385,   385,   386,   386,   387,   387,
     389,   388,   388,   390,   390,   391,   392,   393,   392,   394,
     394,   394,   394,   394,   395,   395,   396,   396,   396,   396,
     396,   396,   396,   396,   396,   396,   396,   396,   396,   396,
     396,   397,   398,   398,   398,   398,   399,   399,   400,   401,
     401,   402,   402,   403,   404,   404,   405,   405,   406,   406,
     407,   407,   408,   408,   409,   410,   410,   411,   412,   413,
     413,   414,   414,   415,   415,   416,   416,   417,   417,   418,
     418,   419,   419,   420,   421,   420,   422,   422,   423,   423,
     424,   424,   424,   424,   424,   424,   425,   425,   425,   426,
     426,   427,   427,   427,   428,   428,   429,   429,   430,   430,
     431,   431,   432,   432,   433,   434,   435,   436,   436,   437,
     437,   438,   438,   439
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     2,     2,     1,     1,     3,     1,     2,
       1,     3,     0,     0,     8,     0,     5,     2,     1,     1,
       3,     1,     0,     3,     0,     2,     0,     4,     3,     3,
       3,     2,     3,     3,     3,     3,     4,     5,     1,     4,
       4,     7,     4,     1,     1,     4,     4,     7,     6,     6,
       6,     6,     4,     4,     4,     1,     4,     3,     1,     4,
       1,     1,     3,     3,     3,     2,     0,     7,     0,     7,
       1,     1,     2,     0,     5,     1,     1,     0,     0,     4,
       1,     1,     1,     4,     3,     1,     2,     3,     4,     5,
       4,     5,     6,     2,     2,     2,     2,     2,     1,     3,
       1,     3,     1,     2,     3,     5,     2,     4,     2,     4,
       1,     3,     1,     3,     2,     3,     1,     3,     1,     1,
       4,     3,     3,     3,     3,     2,     1,     1,     1,     4,
       3,     3,     3,     3,     2,     1,     1,     1,     2,     1,
       3,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       4,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     4,     4,     7,     6,     6,     6,     6,     5,
       4,     3,     3,     2,     2,     2,     2,     3,     3,     3,
       3,     3,     3,     4,     2,     2,     3,     3,     3,     3,
       1,     3,     3,     3,     3,     3,     2,     2,     3,     3,
       3,     3,     4,     6,     4,     4,     1,     1,     4,     3,
       1,     1,     1,     1,     3,     3,     1,     1,     1,     1,
       1,     2,     4,     2,     1,     4,     3,     5,     3,     1,
       1,     1,     1,     2,     4,     2,     1,     2,     2,     4,
       1,     0,     2,     2,     1,     2,     1,     1,     1,     3,
       3,     2,     1,     1,     1,     3,     4,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       4,     0,     4,     3,     3,     2,     3,     3,     1,     4,
       3,     1,     6,     4,     3,     2,     1,     2,     1,     6,
       6,     4,     4,     0,     6,     0,     5,     5,     6,     0,
       6,     0,     7,     0,     5,     0,     5,     0,     5,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     1,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     1,     1,
       1,     5,     1,     2,     1,     1,     1,     3,     1,     3,
       1,     3,     5,     1,     3,     2,     1,     1,     1,     0,
       2,     4,     2,     2,     1,     2,     0,     1,     6,     8,
       4,     6,     4,     2,     6,     2,     4,     6,     2,     4,
       2,     4,     1,     1,     1,     3,     4,     1,     4,     1,
       3,     1,     1,     0,     0,     0,     0,     8,     4,     1,
       3,     0,     4,     3,     2,     4,     5,     5,     2,     4,
       4,     3,     3,     3,     2,     1,     4,     3,     3,     0,
       6,     0,     6,     1,     2,     3,     4,     5,     1,     1,
       0,     0,     0,     0,     9,     1,     1,     1,     3,     3,
       1,     2,     3,     1,     1,     1,     1,     3,     1,     3,
       1,     2,     2,     1,     1,     4,     4,     4,     3,     4,
       4,     4,     3,     3,     3,     2,     0,     6,     2,     4,
       1,     1,     2,     2,     4,     1,     2,     3,     1,     3,
       5,     2,     1,     1,     3,     1,     3,     1,     2,     1,
       1,     3,     2,     1,     1,     3,     2,     1,     2,     1,
       1,     1,     3,     3,     2,     2,     1,     1,     1,     2,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     2,     4,     2,     3,     1,     6,     1,
       1,     1,     1,     2,     1,     2,     1,     1,     1,     1,
       1,     1,     2,     3,     3,     3,     1,     2,     4,     0,
       3,     1,     2,     4,     0,     3,     4,     4,     0,     3,
       0,     3,     0,     2,     0,     2,     0,     2,     1,     0,
       3,     0,     0,     0,     6,     1,     1,     1,     1,     1,
       1,     2,     1,     1,     3,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       0,     4,     0,     1,     1,     3,     1,     0,     3,     4,
       2,     2,     1,     1,     2,     0,     6,     8,     4,     6,
       4,     6,     2,     4,     6,     2,     4,     2,     4,     1,
       0,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     1,     3,     1,     2,     1,     2,     1,     1,     3,
       1,     3,     1,     1,     1,     2,     1,     3,     3,     1,
       3,     1,     3,     1,     1,     2,     1,     1,     1,     2,
       1,     2,     1,     1,     0,     4,     1,     2,     1,     3,
       3,     2,     1,     4,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       0,     1,     0,     1,     2,     2,     2,     1,     1,     1,
       1,     1,     2,     0
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
        yyerror (&yylloc, p, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


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


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc, p)  YY_LOCATION_PRINT(File, *(Loc), p)

#  elif defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc, p)  YYLOCATION_PRINT(File, &(Loc), p)

#  else

#   define YYLOCATION_PRINT(File, Loc, p) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location, p) \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, p);          \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct parser_params *p)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (p);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
switch (yykind)
    {
    case YYSYMBOL_tIDENTIFIER: /* "local variable or method"  */
#line 1926 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, rb_id2str(((*yyvaluep).id)));
#else
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_RIPPER(((*yyvaluep).id))->nd_rval);
#endif
}
#line 6973 "parse.c"
        break;

    case YYSYMBOL_tFID: /* "method"  */
#line 1926 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, rb_id2str(((*yyvaluep).id)));
#else
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_RIPPER(((*yyvaluep).id))->nd_rval);
#endif
}
#line 6985 "parse.c"
        break;

    case YYSYMBOL_tGVAR: /* "global variable"  */
#line 1926 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, rb_id2str(((*yyvaluep).id)));
#else
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_RIPPER(((*yyvaluep).id))->nd_rval);
#endif
}
#line 6997 "parse.c"
        break;

    case YYSYMBOL_tIVAR: /* "instance variable"  */
#line 1926 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, rb_id2str(((*yyvaluep).id)));
#else
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_RIPPER(((*yyvaluep).id))->nd_rval);
#endif
}
#line 7009 "parse.c"
        break;

    case YYSYMBOL_tCONSTANT: /* "constant"  */
#line 1926 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, rb_id2str(((*yyvaluep).id)));
#else
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_RIPPER(((*yyvaluep).id))->nd_rval);
#endif
}
#line 7021 "parse.c"
        break;

    case YYSYMBOL_tCVAR: /* "class variable"  */
#line 1926 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, rb_id2str(((*yyvaluep).id)));
#else
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_RIPPER(((*yyvaluep).id))->nd_rval);
#endif
}
#line 7033 "parse.c"
        break;

    case YYSYMBOL_tLABEL: /* "label"  */
#line 1926 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, rb_id2str(((*yyvaluep).id)));
#else
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_RIPPER(((*yyvaluep).id))->nd_rval);
#endif
}
#line 7045 "parse.c"
        break;

    case YYSYMBOL_tINTEGER: /* "integer literal"  */
#line 1933 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_LIT(((*yyvaluep).node))->nd_lit);
#else
    rb_parser_printf(p, "%"PRIsVALUE, get_value(((*yyvaluep).node)));
#endif
}
#line 7057 "parse.c"
        break;

    case YYSYMBOL_tFLOAT: /* "float literal"  */
#line 1933 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_LIT(((*yyvaluep).node))->nd_lit);
#else
    rb_parser_printf(p, "%"PRIsVALUE, get_value(((*yyvaluep).node)));
#endif
}
#line 7069 "parse.c"
        break;

    case YYSYMBOL_tRATIONAL: /* "rational literal"  */
#line 1933 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_LIT(((*yyvaluep).node))->nd_lit);
#else
    rb_parser_printf(p, "%"PRIsVALUE, get_value(((*yyvaluep).node)));
#endif
}
#line 7081 "parse.c"
        break;

    case YYSYMBOL_tIMAGINARY: /* "imaginary literal"  */
#line 1933 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_LIT(((*yyvaluep).node))->nd_lit);
#else
    rb_parser_printf(p, "%"PRIsVALUE, get_value(((*yyvaluep).node)));
#endif
}
#line 7093 "parse.c"
        break;

    case YYSYMBOL_tCHAR: /* "char literal"  */
#line 1933 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_LIT(((*yyvaluep).node))->nd_lit);
#else
    rb_parser_printf(p, "%"PRIsVALUE, get_value(((*yyvaluep).node)));
#endif
}
#line 7105 "parse.c"
        break;

    case YYSYMBOL_tNTH_REF: /* "numbered reference"  */
#line 1940 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "$%ld", RNODE_NTH_REF(((*yyvaluep).node))->nd_nth);
#else
    rb_parser_printf(p, "%"PRIsVALUE, ((*yyvaluep).node));
#endif
}
#line 7117 "parse.c"
        break;

    case YYSYMBOL_tBACK_REF: /* "back reference"  */
#line 1947 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "$%c", (int)RNODE_BACK_REF(((*yyvaluep).node))->nd_nth);
#else
    rb_parser_printf(p, "%"PRIsVALUE, ((*yyvaluep).node));
#endif
}
#line 7129 "parse.c"
        break;

    case YYSYMBOL_tSTRING_CONTENT: /* "literal content"  */
#line 1933 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_LIT(((*yyvaluep).node))->nd_lit);
#else
    rb_parser_printf(p, "%"PRIsVALUE, get_value(((*yyvaluep).node)));
#endif
}
#line 7141 "parse.c"
        break;

    case YYSYMBOL_tOP_ASGN: /* "operator-assignment"  */
#line 1926 "parse.y"
         {
#ifndef RIPPER
    rb_parser_printf(p, "%"PRIsVALUE, rb_id2str(((*yyvaluep).id)));
#else
    rb_parser_printf(p, "%"PRIsVALUE, RNODE_RIPPER(((*yyvaluep).id))->nd_rval);
#endif
}
#line 7153 "parse.c"
        break;

    case YYSYMBOL_top_compstmt: /* top_compstmt  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7169 "parse.c"
        break;

    case YYSYMBOL_top_stmts: /* top_stmts  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7185 "parse.c"
        break;

    case YYSYMBOL_top_stmt: /* top_stmt  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7201 "parse.c"
        break;

    case YYSYMBOL_begin_block: /* begin_block  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7217 "parse.c"
        break;

    case YYSYMBOL_bodystmt: /* bodystmt  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7233 "parse.c"
        break;

    case YYSYMBOL_compstmt: /* compstmt  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7249 "parse.c"
        break;

    case YYSYMBOL_stmts: /* stmts  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7265 "parse.c"
        break;

    case YYSYMBOL_stmt_or_begin: /* stmt_or_begin  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7281 "parse.c"
        break;

    case YYSYMBOL_stmt: /* stmt  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7297 "parse.c"
        break;

    case YYSYMBOL_command_asgn: /* command_asgn  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7313 "parse.c"
        break;

    case YYSYMBOL_endless_command: /* endless_command  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7329 "parse.c"
        break;

    case YYSYMBOL_command_rhs: /* command_rhs  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7345 "parse.c"
        break;

    case YYSYMBOL_expr: /* expr  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7361 "parse.c"
        break;

    case YYSYMBOL_expr_value: /* expr_value  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7377 "parse.c"
        break;

    case YYSYMBOL_expr_value_do: /* expr_value_do  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7393 "parse.c"
        break;

    case YYSYMBOL_command_call: /* command_call  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7409 "parse.c"
        break;

    case YYSYMBOL_block_command: /* block_command  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7425 "parse.c"
        break;

    case YYSYMBOL_cmd_brace_block: /* cmd_brace_block  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7441 "parse.c"
        break;

    case YYSYMBOL_fcall: /* fcall  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_fcall) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_fcall)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_fcall)))));
    }
#else
#endif
}
#line 7457 "parse.c"
        break;

    case YYSYMBOL_command: /* command  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7473 "parse.c"
        break;

    case YYSYMBOL_mlhs_item: /* mlhs_item  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7489 "parse.c"
        break;

    case YYSYMBOL_mlhs_head: /* mlhs_head  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7505 "parse.c"
        break;

    case YYSYMBOL_mlhs_post: /* mlhs_post  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7521 "parse.c"
        break;

    case YYSYMBOL_mlhs_node: /* mlhs_node  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7537 "parse.c"
        break;

    case YYSYMBOL_lhs: /* lhs  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7553 "parse.c"
        break;

    case YYSYMBOL_cpath: /* cpath  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7569 "parse.c"
        break;

    case YYSYMBOL_fitem: /* fitem  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7585 "parse.c"
        break;

    case YYSYMBOL_undef_list: /* undef_list  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7601 "parse.c"
        break;

    case YYSYMBOL_arg: /* arg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7617 "parse.c"
        break;

    case YYSYMBOL_endless_arg: /* endless_arg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7633 "parse.c"
        break;

    case YYSYMBOL_rel_expr: /* rel_expr  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7649 "parse.c"
        break;

    case YYSYMBOL_arg_value: /* arg_value  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7665 "parse.c"
        break;

    case YYSYMBOL_aref_args: /* aref_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7681 "parse.c"
        break;

    case YYSYMBOL_arg_rhs: /* arg_rhs  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7697 "parse.c"
        break;

    case YYSYMBOL_paren_args: /* paren_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7713 "parse.c"
        break;

    case YYSYMBOL_opt_paren_args: /* opt_paren_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7729 "parse.c"
        break;

    case YYSYMBOL_opt_call_args: /* opt_call_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7745 "parse.c"
        break;

    case YYSYMBOL_call_args: /* call_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7761 "parse.c"
        break;

    case YYSYMBOL_command_args: /* command_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7777 "parse.c"
        break;

    case YYSYMBOL_block_arg: /* block_arg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_block_pass) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_block_pass)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_block_pass)))));
    }
#else
#endif
}
#line 7793 "parse.c"
        break;

    case YYSYMBOL_opt_block_arg: /* opt_block_arg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_block_pass) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_block_pass)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_block_pass)))));
    }
#else
#endif
}
#line 7809 "parse.c"
        break;

    case YYSYMBOL_args: /* args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7825 "parse.c"
        break;

    case YYSYMBOL_arg_splat: /* arg_splat  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7841 "parse.c"
        break;

    case YYSYMBOL_mrhs_arg: /* mrhs_arg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7857 "parse.c"
        break;

    case YYSYMBOL_mrhs: /* mrhs  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7873 "parse.c"
        break;

    case YYSYMBOL_primary: /* primary  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7889 "parse.c"
        break;

    case YYSYMBOL_primary_value: /* primary_value  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7905 "parse.c"
        break;

    case YYSYMBOL_if_tail: /* if_tail  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7921 "parse.c"
        break;

    case YYSYMBOL_opt_else: /* opt_else  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7937 "parse.c"
        break;

    case YYSYMBOL_for_var: /* for_var  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7953 "parse.c"
        break;

    case YYSYMBOL_f_marg: /* f_marg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7969 "parse.c"
        break;

    case YYSYMBOL_f_marg_list: /* f_marg_list  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 7985 "parse.c"
        break;

    case YYSYMBOL_f_rest_marg: /* f_rest_marg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8001 "parse.c"
        break;

    case YYSYMBOL_block_args_tail: /* block_args_tail  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 8017 "parse.c"
        break;

    case YYSYMBOL_opt_block_args_tail: /* opt_block_args_tail  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 8033 "parse.c"
        break;

    case YYSYMBOL_block_param: /* block_param  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 8049 "parse.c"
        break;

    case YYSYMBOL_opt_block_param: /* opt_block_param  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 8065 "parse.c"
        break;

    case YYSYMBOL_block_param_def: /* block_param_def  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 8081 "parse.c"
        break;

    case YYSYMBOL_opt_bv_decl: /* opt_bv_decl  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8097 "parse.c"
        break;

    case YYSYMBOL_bv_decls: /* bv_decls  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8113 "parse.c"
        break;

    case YYSYMBOL_bvar: /* bvar  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8129 "parse.c"
        break;

    case YYSYMBOL_numparam: /* numparam  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8145 "parse.c"
        break;

    case YYSYMBOL_lambda: /* lambda  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8161 "parse.c"
        break;

    case YYSYMBOL_f_larglist: /* f_larglist  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 8177 "parse.c"
        break;

    case YYSYMBOL_lambda_body: /* lambda_body  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8193 "parse.c"
        break;

    case YYSYMBOL_do_block: /* do_block  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8209 "parse.c"
        break;

    case YYSYMBOL_block_call: /* block_call  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8225 "parse.c"
        break;

    case YYSYMBOL_method_call: /* method_call  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8241 "parse.c"
        break;

    case YYSYMBOL_brace_block: /* brace_block  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8257 "parse.c"
        break;

    case YYSYMBOL_brace_body: /* brace_body  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8273 "parse.c"
        break;

    case YYSYMBOL_do_body: /* do_body  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8289 "parse.c"
        break;

    case YYSYMBOL_case_args: /* case_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8305 "parse.c"
        break;

    case YYSYMBOL_case_body: /* case_body  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8321 "parse.c"
        break;

    case YYSYMBOL_cases: /* cases  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8337 "parse.c"
        break;

    case YYSYMBOL_p_case_body: /* p_case_body  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8353 "parse.c"
        break;

    case YYSYMBOL_p_cases: /* p_cases  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8369 "parse.c"
        break;

    case YYSYMBOL_p_top_expr: /* p_top_expr  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8385 "parse.c"
        break;

    case YYSYMBOL_p_top_expr_body: /* p_top_expr_body  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8401 "parse.c"
        break;

    case YYSYMBOL_p_expr: /* p_expr  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8417 "parse.c"
        break;

    case YYSYMBOL_p_as: /* p_as  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8433 "parse.c"
        break;

    case YYSYMBOL_p_alt: /* p_alt  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8449 "parse.c"
        break;

    case YYSYMBOL_p_expr_basic: /* p_expr_basic  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8465 "parse.c"
        break;

    case YYSYMBOL_p_args: /* p_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8481 "parse.c"
        break;

    case YYSYMBOL_p_args_head: /* p_args_head  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8497 "parse.c"
        break;

    case YYSYMBOL_p_args_tail: /* p_args_tail  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8513 "parse.c"
        break;

    case YYSYMBOL_p_find: /* p_find  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8529 "parse.c"
        break;

    case YYSYMBOL_p_rest: /* p_rest  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8545 "parse.c"
        break;

    case YYSYMBOL_p_args_post: /* p_args_post  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8561 "parse.c"
        break;

    case YYSYMBOL_p_arg: /* p_arg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8577 "parse.c"
        break;

    case YYSYMBOL_p_kwargs: /* p_kwargs  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8593 "parse.c"
        break;

    case YYSYMBOL_p_kwarg: /* p_kwarg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8609 "parse.c"
        break;

    case YYSYMBOL_p_kw: /* p_kw  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8625 "parse.c"
        break;

    case YYSYMBOL_p_value: /* p_value  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8641 "parse.c"
        break;

    case YYSYMBOL_p_primitive: /* p_primitive  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8657 "parse.c"
        break;

    case YYSYMBOL_p_variable: /* p_variable  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8673 "parse.c"
        break;

    case YYSYMBOL_p_var_ref: /* p_var_ref  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8689 "parse.c"
        break;

    case YYSYMBOL_p_expr_ref: /* p_expr_ref  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8705 "parse.c"
        break;

    case YYSYMBOL_p_const: /* p_const  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8721 "parse.c"
        break;

    case YYSYMBOL_opt_rescue: /* opt_rescue  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8737 "parse.c"
        break;

    case YYSYMBOL_exc_list: /* exc_list  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8753 "parse.c"
        break;

    case YYSYMBOL_exc_var: /* exc_var  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8769 "parse.c"
        break;

    case YYSYMBOL_opt_ensure: /* opt_ensure  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8785 "parse.c"
        break;

    case YYSYMBOL_literal: /* literal  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8801 "parse.c"
        break;

    case YYSYMBOL_strings: /* strings  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8817 "parse.c"
        break;

    case YYSYMBOL_string: /* string  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8833 "parse.c"
        break;

    case YYSYMBOL_string1: /* string1  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8849 "parse.c"
        break;

    case YYSYMBOL_xstring: /* xstring  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8865 "parse.c"
        break;

    case YYSYMBOL_regexp: /* regexp  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8881 "parse.c"
        break;

    case YYSYMBOL_words: /* words  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8897 "parse.c"
        break;

    case YYSYMBOL_word_list: /* word_list  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8913 "parse.c"
        break;

    case YYSYMBOL_word: /* word  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8929 "parse.c"
        break;

    case YYSYMBOL_symbols: /* symbols  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8945 "parse.c"
        break;

    case YYSYMBOL_symbol_list: /* symbol_list  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8961 "parse.c"
        break;

    case YYSYMBOL_qwords: /* qwords  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8977 "parse.c"
        break;

    case YYSYMBOL_qsymbols: /* qsymbols  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 8993 "parse.c"
        break;

    case YYSYMBOL_qword_list: /* qword_list  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9009 "parse.c"
        break;

    case YYSYMBOL_qsym_list: /* qsym_list  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9025 "parse.c"
        break;

    case YYSYMBOL_string_contents: /* string_contents  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9041 "parse.c"
        break;

    case YYSYMBOL_xstring_contents: /* xstring_contents  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9057 "parse.c"
        break;

    case YYSYMBOL_regexp_contents: /* regexp_contents  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9073 "parse.c"
        break;

    case YYSYMBOL_string_content: /* string_content  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9089 "parse.c"
        break;

    case YYSYMBOL_string_dvar: /* string_dvar  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9105 "parse.c"
        break;

    case YYSYMBOL_symbol: /* symbol  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9121 "parse.c"
        break;

    case YYSYMBOL_ssym: /* ssym  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9137 "parse.c"
        break;

    case YYSYMBOL_dsym: /* dsym  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9153 "parse.c"
        break;

    case YYSYMBOL_numeric: /* numeric  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9169 "parse.c"
        break;

    case YYSYMBOL_simple_numeric: /* simple_numeric  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9185 "parse.c"
        break;

    case YYSYMBOL_var_ref: /* var_ref  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9201 "parse.c"
        break;

    case YYSYMBOL_var_lhs: /* var_lhs  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9217 "parse.c"
        break;

    case YYSYMBOL_backref: /* backref  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9233 "parse.c"
        break;

    case YYSYMBOL_superclass: /* superclass  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9249 "parse.c"
        break;

    case YYSYMBOL_f_opt_paren_args: /* f_opt_paren_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 9265 "parse.c"
        break;

    case YYSYMBOL_f_paren_args: /* f_paren_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 9281 "parse.c"
        break;

    case YYSYMBOL_f_arglist: /* f_arglist  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 9297 "parse.c"
        break;

    case YYSYMBOL_args_tail: /* args_tail  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 9313 "parse.c"
        break;

    case YYSYMBOL_opt_args_tail: /* opt_args_tail  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 9329 "parse.c"
        break;

    case YYSYMBOL_f_args: /* f_args  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args)))));
    }
#else
#endif
}
#line 9345 "parse.c"
        break;

    case YYSYMBOL_f_arg_item: /* f_arg_item  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args_aux) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args_aux)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args_aux)))));
    }
#else
#endif
}
#line 9361 "parse.c"
        break;

    case YYSYMBOL_f_arg: /* f_arg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_args_aux) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_args_aux)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_args_aux)))));
    }
#else
#endif
}
#line 9377 "parse.c"
        break;

    case YYSYMBOL_f_kw: /* f_kw  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_kw_arg) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_kw_arg)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_kw_arg)))));
    }
#else
#endif
}
#line 9393 "parse.c"
        break;

    case YYSYMBOL_f_block_kw: /* f_block_kw  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_kw_arg) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_kw_arg)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_kw_arg)))));
    }
#else
#endif
}
#line 9409 "parse.c"
        break;

    case YYSYMBOL_f_block_kwarg: /* f_block_kwarg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_kw_arg) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_kw_arg)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_kw_arg)))));
    }
#else
#endif
}
#line 9425 "parse.c"
        break;

    case YYSYMBOL_f_kwarg: /* f_kwarg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_kw_arg) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_kw_arg)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_kw_arg)))));
    }
#else
#endif
}
#line 9441 "parse.c"
        break;

    case YYSYMBOL_f_opt: /* f_opt  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_opt_arg) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_opt_arg)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_opt_arg)))));
    }
#else
#endif
}
#line 9457 "parse.c"
        break;

    case YYSYMBOL_f_block_opt: /* f_block_opt  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_opt_arg) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_opt_arg)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_opt_arg)))));
    }
#else
#endif
}
#line 9473 "parse.c"
        break;

    case YYSYMBOL_f_block_optarg: /* f_block_optarg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_opt_arg) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_opt_arg)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_opt_arg)))));
    }
#else
#endif
}
#line 9489 "parse.c"
        break;

    case YYSYMBOL_f_optarg: /* f_optarg  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node_opt_arg) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node_opt_arg)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node_opt_arg)))));
    }
#else
#endif
}
#line 9505 "parse.c"
        break;

    case YYSYMBOL_singleton: /* singleton  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9521 "parse.c"
        break;

    case YYSYMBOL_assoc_list: /* assoc_list  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9537 "parse.c"
        break;

    case YYSYMBOL_assocs: /* assocs  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9553 "parse.c"
        break;

    case YYSYMBOL_assoc: /* assoc  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9569 "parse.c"
        break;

    case YYSYMBOL_none: /* none  */
#line 1915 "parse.y"
         {
#ifndef RIPPER
    if ((NODE *)((*yyvaluep).node) == (NODE *)-1) {
        rb_parser_printf(p, "NODE_SPECIAL");
    }
    else if (((*yyvaluep).node)) {
        rb_parser_printf(p, "%s", parser_node_name(nd_type(RNODE(((*yyvaluep).node)))));
    }
#else
#endif
}
#line 9585 "parse.c"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct parser_params *p)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp, p);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, p);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop, struct parser_params *p)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top, p)     \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top), p);    \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule, struct parser_params *p)
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
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]), p);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule, p) \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, p); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
#ifndef yydebug
int yydebug;
#endif
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location, p)
# define YY_STACK_PRINT(Bottom, Top, p)
# define YY_REDUCE_PRINT(Rule, p)
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
  YYLTYPE *yylloc;
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
                const yypcontext_t *yyctx, struct parser_params *p)
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
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, struct parser_params *p)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (p);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp, p);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (struct parser_params *p)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static const YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static const YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs = 0;
    YY_USE (yynerrs); /* Silence compiler warning.  */

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

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */


        /* User initialization code.  */
#line 1958 "parse.y"
        {
    RUBY_SET_YYLLOC_OF_NONE(yylloc);
}

#line 10083 "parse.c"

  yylsp[0] = yylloc;
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
  YY_STACK_PRINT (yyss, yyssp, p);

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
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
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
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

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
      yychar = yylex (&yylval, &yylloc, p);
    }

  if (yychar <= END_OF_INPUT)
    {
      yychar = END_OF_INPUT;
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
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc, p);
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
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc, p);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

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

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn, p);
  switch (yyn)
    {
  case 2: /* $@1: %empty  */
#line 2193 "parse.y"
          {
                        SET_LEX_STATE(EXPR_BEG);
                        local_push(p, ifndef_ripper(1)+0);
                        /* jumps are possible in the top-level loop. */
                        if (!ifndef_ripper(p->do_loop) + 0) init_block_exit(p);
                    }
#line 10301 "parse.c"
    break;

  case 3: /* program: $@1 top_compstmt  */
#line 2200 "parse.y"
                  {
                    /*%%%*/
                        if ((yyvsp[0].node) && !compile_for_eval) {
                            NODE *node = (yyvsp[0].node);
                            /* last expression should not be void */
                            if (nd_type_p(node, NODE_BLOCK)) {
                                while (RNODE_BLOCK(node)->nd_next) {
                                    node = RNODE_BLOCK(node)->nd_next;
                                }
                                node = RNODE_BLOCK(node)->nd_head;
                            }
                            node = remove_begin(node);
                            void_expr(p, node);
                        }
                        p->eval_tree = NEW_SCOPE(0, block_append(p, p->eval_tree, (yyvsp[0].node)), &(yyloc));
                    /*% %*/
                    /*% ripper[final]: program!($2) %*/
                        local_pop(p);
                    }
#line 10325 "parse.c"
    break;

  case 4: /* top_compstmt: top_stmts opt_terms  */
#line 2222 "parse.y"
                  {
                        (yyval.node) = void_stmts(p, (yyvsp[-1].node));
                    }
#line 10333 "parse.c"
    break;

  case 5: /* top_stmts: none  */
#line 2228 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_BEGIN(0, &(yyloc));
                    /*% %*/
                    /*% ripper: stmts_add!(stmts_new!, void_stmt!) %*/
                    }
#line 10344 "parse.c"
    break;

  case 6: /* top_stmts: top_stmt  */
#line 2235 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = newline_node((yyvsp[0].node));
                    /*% %*/
                    /*% ripper: stmts_add!(stmts_new!, $1) %*/
                    }
#line 10355 "parse.c"
    break;

  case 7: /* top_stmts: top_stmts terms top_stmt  */
#line 2242 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = block_append(p, (yyvsp[-2].node), newline_node((yyvsp[0].node)));
                    /*% %*/
                    /*% ripper: stmts_add!($1, $3) %*/
                    }
#line 10366 "parse.c"
    break;

  case 8: /* top_stmt: stmt  */
#line 2251 "parse.y"
                  {
                        clear_block_exit(p, true);
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 10375 "parse.c"
    break;

  case 9: /* top_stmt: "`BEGIN'" begin_block  */
#line 2256 "parse.y"
                  {
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 10383 "parse.c"
    break;

  case 10: /* block_open: '{'  */
#line 2261 "parse.y"
               {(yyval.node_exits) = init_block_exit(p);}
#line 10389 "parse.c"
    break;

  case 11: /* begin_block: block_open top_compstmt '}'  */
#line 2264 "parse.y"
                  {
                        restore_block_exit(p, (yyvsp[-2].node_exits));
                    /*%%%*/
                        p->eval_tree_begin = block_append(p, p->eval_tree_begin,
                                                          NEW_BEGIN((yyvsp[-1].node), &(yyloc)));
                        (yyval.node) = NEW_BEGIN(0, &(yyloc));
                    /*% %*/
                    /*% ripper: BEGIN!($2) %*/
                    }
#line 10403 "parse.c"
    break;

  case 12: /* $@2: %empty  */
#line 2279 "parse.y"
                  {
                        if (!(yyvsp[-1].node)) yyerror1(&(yylsp[0]), "else without rescue is useless");
                        next_rescue_context(&p->ctxt, &(yyvsp[-2].ctxt), after_else);
                    }
#line 10412 "parse.c"
    break;

  case 13: /* $@3: %empty  */
#line 2284 "parse.y"
                  {
                        next_rescue_context(&p->ctxt, &(yyvsp[-4].ctxt), after_ensure);
                    }
#line 10420 "parse.c"
    break;

  case 14: /* bodystmt: compstmt lex_ctxt opt_rescue k_else $@2 compstmt $@3 opt_ensure  */
#line 2288 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_bodystmt(p, (yyvsp[-7].node), (yyvsp[-5].node), (yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: bodystmt!($body, $opt_rescue, $elsebody, $opt_ensure) %*/
                    }
#line 10431 "parse.c"
    break;

  case 15: /* $@4: %empty  */
#line 2297 "parse.y"
                  {
                        next_rescue_context(&p->ctxt, &(yyvsp[-1].ctxt), after_ensure);
                    }
#line 10439 "parse.c"
    break;

  case 16: /* bodystmt: compstmt lex_ctxt opt_rescue $@4 opt_ensure  */
#line 2301 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_bodystmt(p, (yyvsp[-4].node), (yyvsp[-2].node), 0, (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: bodystmt!($body, $opt_rescue, Qnil, $opt_ensure) %*/
                    }
#line 10450 "parse.c"
    break;

  case 17: /* compstmt: stmts opt_terms  */
#line 2310 "parse.y"
                  {
                        (yyval.node) = void_stmts(p, (yyvsp[-1].node));
                    }
#line 10458 "parse.c"
    break;

  case 18: /* stmts: none  */
#line 2316 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_BEGIN(0, &(yyloc));
                    /*% %*/
                    /*% ripper: stmts_add!(stmts_new!, void_stmt!) %*/
                    }
#line 10469 "parse.c"
    break;

  case 19: /* stmts: stmt_or_begin  */
#line 2323 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = newline_node((yyvsp[0].node));
                    /*% %*/
                    /*% ripper: stmts_add!(stmts_new!, $1) %*/
                    }
#line 10480 "parse.c"
    break;

  case 20: /* stmts: stmts terms stmt_or_begin  */
#line 2330 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = block_append(p, (yyvsp[-2].node), newline_node((yyvsp[0].node)));
                    /*% %*/
                    /*% ripper: stmts_add!($1, $3) %*/
                    }
#line 10491 "parse.c"
    break;

  case 21: /* stmt_or_begin: stmt  */
#line 2339 "parse.y"
                  {
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 10499 "parse.c"
    break;

  case 22: /* $@5: %empty  */
#line 2343 "parse.y"
                  {
                        yyerror1(&(yylsp[0]), "BEGIN is permitted only at toplevel");
                    }
#line 10507 "parse.c"
    break;

  case 23: /* stmt_or_begin: "`BEGIN'" $@5 begin_block  */
#line 2347 "parse.y"
                  {
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 10515 "parse.c"
    break;

  case 24: /* allow_exits: %empty  */
#line 2352 "parse.y"
            {(yyval.node_exits) = allow_block_exit(p);}
#line 10521 "parse.c"
    break;

  case 25: /* k_END: "`END'" lex_ctxt  */
#line 2355 "parse.y"
                  {
                        (yyval.ctxt) = (yyvsp[0].ctxt);
                        p->ctxt.in_rescue = before_rescue;
                    }
#line 10530 "parse.c"
    break;

  case 26: /* $@6: %empty  */
#line 2360 "parse.y"
                          {SET_LEX_STATE(EXPR_FNAME|EXPR_FITEM);}
#line 10536 "parse.c"
    break;

  case 27: /* stmt: "`alias'" fitem $@6 fitem  */
#line 2361 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_ALIAS((yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: alias!($2, $4) %*/
                    }
#line 10547 "parse.c"
    break;

  case 28: /* stmt: "`alias'" "global variable" "global variable"  */
#line 2368 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_VALIAS((yyvsp[-1].id), (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: var_alias!($2, $3) %*/
                    }
#line 10558 "parse.c"
    break;

  case 29: /* stmt: "`alias'" "global variable" "back reference"  */
#line 2375 "parse.y"
                  {
                    /*%%%*/
                        char buf[2];
                        buf[0] = '$';
                        buf[1] = (char)RNODE_BACK_REF((yyvsp[0].node))->nd_nth;
                        (yyval.node) = NEW_VALIAS((yyvsp[-1].id), rb_intern2(buf, 2), &(yyloc));
                    /*% %*/
                    /*% ripper: var_alias!($2, $3) %*/
                    }
#line 10572 "parse.c"
    break;

  case 30: /* stmt: "`alias'" "global variable" "numbered reference"  */
#line 2385 "parse.y"
                  {
                        static const char mesg[] = "can't make alias for the number variables";
                    /*%%%*/
                        yyerror1(&(yylsp[0]), mesg);
                        (yyval.node) = NEW_ERROR(&(yyloc));
                    /*% %*/
                    /*% ripper[error]: alias_error!(ERR_MESG(), $3) %*/
                    }
#line 10585 "parse.c"
    break;

  case 31: /* stmt: "`undef'" undef_list  */
#line 2394 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[0].node);
                    /*% %*/
                    /*% ripper: undef!($2) %*/
                    }
#line 10596 "parse.c"
    break;

  case 32: /* stmt: stmt "`if' modifier" expr_value  */
#line 2401 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_if(p, (yyvsp[0].node), remove_begin((yyvsp[-2].node)), 0, &(yyloc));
                        fixpos((yyval.node), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: if_mod!($3, $1) %*/
                    }
#line 10608 "parse.c"
    break;

  case 33: /* stmt: stmt "`unless' modifier" expr_value  */
#line 2409 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_unless(p, (yyvsp[0].node), remove_begin((yyvsp[-2].node)), 0, &(yyloc));
                        fixpos((yyval.node), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: unless_mod!($3, $1) %*/
                    }
#line 10620 "parse.c"
    break;

  case 34: /* stmt: stmt "`while' modifier" expr_value  */
#line 2417 "parse.y"
                  {
                        clear_block_exit(p, false);
                    /*%%%*/
                        if ((yyvsp[-2].node) && nd_type_p((yyvsp[-2].node), NODE_BEGIN)) {
                            (yyval.node) = NEW_WHILE(cond(p, (yyvsp[0].node), &(yylsp[0])), RNODE_BEGIN((yyvsp[-2].node))->nd_body, 0, &(yyloc));
                        }
                        else {
                            (yyval.node) = NEW_WHILE(cond(p, (yyvsp[0].node), &(yylsp[0])), (yyvsp[-2].node), 1, &(yyloc));
                        }
                    /*% %*/
                    /*% ripper: while_mod!($3, $1) %*/
                    }
#line 10637 "parse.c"
    break;

  case 35: /* stmt: stmt "`until' modifier" expr_value  */
#line 2430 "parse.y"
                  {
                        clear_block_exit(p, false);
                    /*%%%*/
                        if ((yyvsp[-2].node) && nd_type_p((yyvsp[-2].node), NODE_BEGIN)) {
                            (yyval.node) = NEW_UNTIL(cond(p, (yyvsp[0].node), &(yylsp[0])), RNODE_BEGIN((yyvsp[-2].node))->nd_body, 0, &(yyloc));
                        }
                        else {
                            (yyval.node) = NEW_UNTIL(cond(p, (yyvsp[0].node), &(yylsp[0])), (yyvsp[-2].node), 1, &(yyloc));
                        }
                    /*% %*/
                    /*% ripper: until_mod!($3, $1) %*/
                    }
#line 10654 "parse.c"
    break;

  case 36: /* stmt: stmt "`rescue' modifier" after_rescue stmt  */
#line 2443 "parse.y"
                  {
                        p->ctxt.in_rescue = (yyvsp[-1].ctxt).in_rescue;
                    /*%%%*/
                        NODE *resq;
                        YYLTYPE loc = code_loc_gen(&(yylsp[-2]), &(yylsp[0]));
                        resq = NEW_RESBODY(0, remove_begin((yyvsp[0].node)), 0, &loc);
                        (yyval.node) = NEW_RESCUE(remove_begin((yyvsp[-3].node)), resq, 0, &(yyloc));
                    /*% %*/
                    /*% ripper: rescue_mod!($1, $4) %*/
                    }
#line 10669 "parse.c"
    break;

  case 37: /* stmt: k_END allow_exits '{' compstmt '}'  */
#line 2454 "parse.y"
                  {
                        if (p->ctxt.in_def) {
                            rb_warn0("END in method; use at_exit");
                        }
                        restore_block_exit(p, (yyvsp[-3].node_exits));
                        p->ctxt = (yyvsp[-4].ctxt);
                    /*%%%*/
                        {
                            NODE *scope = NEW_SCOPE2(0 /* tbl */, 0 /* args */, (yyvsp[-1].node) /* body */, &(yyloc));
                            (yyval.node) = NEW_POSTEXE(scope, &(yyloc));
                        }
                    /*% %*/
                    /*% ripper: END!($compstmt) %*/
                    }
#line 10688 "parse.c"
    break;

  case 39: /* stmt: mlhs '=' lex_ctxt command_call  */
#line 2470 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[0].node));
                        (yyval.node) = node_assign(p, (NODE *)(yyvsp[-3].node_masgn), (yyvsp[0].node), (yyvsp[-1].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: massign!($1, $4) %*/
                    }
#line 10700 "parse.c"
    break;

  case 40: /* stmt: lhs '=' lex_ctxt mrhs  */
#line 2478 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = node_assign(p, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-1].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: assign!($1, $4) %*/
                    }
#line 10711 "parse.c"
    break;

  case 41: /* stmt: mlhs '=' lex_ctxt mrhs_arg "`rescue' modifier" after_rescue stmt  */
#line 2486 "parse.y"
                  {
                        p->ctxt.in_rescue = (yyvsp[-4].ctxt).in_rescue;
                    /*%%%*/
                        YYLTYPE loc = code_loc_gen(&(yylsp[-2]), &(yylsp[0]));
                        (yyvsp[0].node) = NEW_RESBODY(0, remove_begin((yyvsp[0].node)), 0, &loc);
                        loc.beg_pos = (yylsp[-3]).beg_pos;
                        (yyvsp[-3].node) = NEW_RESCUE((yyvsp[-3].node), (yyvsp[0].node), 0, &loc);
                        (yyval.node) = node_assign(p, (NODE *)(yyvsp[-6].node_masgn), (yyvsp[-3].node), (yyvsp[-4].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: massign!($1, rescue_mod!($4, $7)) %*/
                    }
#line 10727 "parse.c"
    break;

  case 42: /* stmt: mlhs '=' lex_ctxt mrhs_arg  */
#line 2498 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = node_assign(p, (NODE *)(yyvsp[-3].node_masgn), (yyvsp[0].node), (yyvsp[-1].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: massign!($1, $4) %*/
                    }
#line 10738 "parse.c"
    break;

  case 44: /* stmt: error  */
#line 2506 "parse.y"
                  {
                        (void)yynerrs;
                    /*%%%*/
                        (yyval.node) = NEW_ERROR(&(yyloc));
                    /*% %*/
                    }
#line 10749 "parse.c"
    break;

  case 45: /* command_asgn: lhs '=' lex_ctxt command_rhs  */
#line 2515 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = node_assign(p, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-1].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: assign!($1, $4) %*/
                    }
#line 10760 "parse.c"
    break;

  case 46: /* command_asgn: var_lhs "operator-assignment" lex_ctxt command_rhs  */
#line 2522 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_op_assign(p, (yyvsp[-3].node), (yyvsp[-2].id), (yyvsp[0].node), (yyvsp[-1].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!($1, $2, $4) %*/
                    }
#line 10771 "parse.c"
    break;

  case 47: /* command_asgn: primary_value '[' opt_call_args rbracket "operator-assignment" lex_ctxt command_rhs  */
#line 2529 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_ary_op_assign(p, (yyvsp[-6].node), (yyvsp[-4].node), (yyvsp[-2].id), (yyvsp[0].node), &(yylsp[-4]), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(aref_field!($1, $3), $5, $7) %*/

                    }
#line 10783 "parse.c"
    break;

  case 48: /* command_asgn: primary_value call_op "local variable or method" "operator-assignment" lex_ctxt command_rhs  */
#line 2537 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_attr_op_assign(p, (yyvsp[-5].node), (yyvsp[-4].id), (yyvsp[-3].id), (yyvsp[-2].id), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(field!($1, $2, $3), $4, $6) %*/
                    }
#line 10794 "parse.c"
    break;

  case 49: /* command_asgn: primary_value call_op "constant" "operator-assignment" lex_ctxt command_rhs  */
#line 2544 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_attr_op_assign(p, (yyvsp[-5].node), (yyvsp[-4].id), (yyvsp[-3].id), (yyvsp[-2].id), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(field!($1, $2, $3), $4, $6) %*/
                    }
#line 10805 "parse.c"
    break;

  case 50: /* command_asgn: primary_value "::" "constant" "operator-assignment" lex_ctxt command_rhs  */
#line 2551 "parse.y"
                  {
                    /*%%%*/
                        YYLTYPE loc = code_loc_gen(&(yylsp[-5]), &(yylsp[-3]));
                        (yyval.node) = new_const_op_assign(p, NEW_COLON2((yyvsp[-5].node), (yyvsp[-3].id), &loc), (yyvsp[-2].id), (yyvsp[0].node), (yyvsp[-1].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(const_path_field!($1, $3), $4, $6) %*/
                    }
#line 10817 "parse.c"
    break;

  case 51: /* command_asgn: primary_value "::" "local variable or method" "operator-assignment" lex_ctxt command_rhs  */
#line 2559 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_attr_op_assign(p, (yyvsp[-5].node), ID2VAL(idCOLON2), (yyvsp[-3].id), (yyvsp[-2].id), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(field!($1, $2, $3), $4, $6) %*/
                    }
#line 10828 "parse.c"
    break;

  case 52: /* command_asgn: defn_head f_opt_paren_args '=' endless_command  */
#line 2566 "parse.y"
                  {
                        endless_method_name(p, get_id((yyvsp[-3].node_def_temp)->nd_mid), &(yylsp[-3]));
                        restore_defun(p, (yyvsp[-3].node_def_temp));
                    /*%%%*/
                        (yyvsp[0].node) = new_scope_body(p, (yyvsp[-2].node_args), (yyvsp[0].node), &(yyloc));
                        ((yyval.node) = (yyvsp[-3].node_def_temp)->nd_def)->nd_loc = (yyloc);
                        RNODE_DEFN((yyval.node))->nd_defn = (yyvsp[0].node);
                    /*% %*/
                    /*% ripper[$bodystmt]: bodystmt!($bodystmt, Qnil, Qnil, Qnil) %*/
                    /*% ripper: def!($head->nd_mid, $args, $bodystmt) %*/
                        local_pop(p);
                    }
#line 10845 "parse.c"
    break;

  case 53: /* command_asgn: defs_head f_opt_paren_args '=' endless_command  */
#line 2579 "parse.y"
                  {
                        endless_method_name(p, get_id((yyvsp[-3].node_def_temp)->nd_mid), &(yylsp[-3]));
                        restore_defun(p, (yyvsp[-3].node_def_temp));
                    /*%%%*/
                        (yyvsp[0].node) = new_scope_body(p, (yyvsp[-2].node_args), (yyvsp[0].node), &(yyloc));
                        ((yyval.node) = (yyvsp[-3].node_def_temp)->nd_def)->nd_loc = (yyloc);
                        RNODE_DEFS((yyval.node))->nd_defn = (yyvsp[0].node);
                    /*% %*/
                    /*% ripper[$bodystmt]: bodystmt!($bodystmt, Qnil, Qnil, Qnil) %*/
                    /*% ripper: defs!($head->nd_recv, $head->dot_or_colon, $head->nd_mid, $args, $bodystmt) %*/
                        local_pop(p);
                    }
#line 10862 "parse.c"
    break;

  case 54: /* command_asgn: backref "operator-assignment" lex_ctxt command_rhs  */
#line 2592 "parse.y"
                  {
                    /*%%%*/
                        rb_backref_error(p, (yyvsp[-3].node));
                        (yyval.node) = NEW_ERROR(&(yyloc));
                    /*% %*/
                    /*% ripper[error]: backref_error(p, RNODE($1), assign!(var_field(p, $1), $4)) %*/
                    }
#line 10874 "parse.c"
    break;

  case 56: /* endless_command: endless_command "`rescue' modifier" after_rescue arg  */
#line 2603 "parse.y"
                  {
                        p->ctxt.in_rescue = (yyvsp[-1].ctxt).in_rescue;
                    /*%%%*/
                        (yyval.node) = rescued_expr(p, (yyvsp[-3].node), (yyvsp[0].node), &(yylsp[-3]), &(yylsp[-2]), &(yylsp[0]));
                    /*% %*/
                    /*% ripper: rescue_mod!($1, $4) %*/
                    }
#line 10886 "parse.c"
    break;

  case 57: /* endless_command: "`not'" opt_nl endless_command  */
#line 2611 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, method_cond(p, (yyvsp[0].node), &(yylsp[0])), METHOD_NOT, &(yylsp[-2]), &(yyloc));
                    }
#line 10894 "parse.c"
    break;

  case 58: /* command_rhs: command_call  */
#line 2617 "parse.y"
                  {
                        value_expr((yyvsp[0].node));
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 10903 "parse.c"
    break;

  case 59: /* command_rhs: command_call "`rescue' modifier" after_rescue stmt  */
#line 2622 "parse.y"
                  {
                        p->ctxt.in_rescue = (yyvsp[-1].ctxt).in_rescue;
                    /*%%%*/
                        YYLTYPE loc = code_loc_gen(&(yylsp[-2]), &(yylsp[0]));
                        value_expr((yyvsp[-3].node));
                        (yyval.node) = NEW_RESCUE((yyvsp[-3].node), NEW_RESBODY(0, remove_begin((yyvsp[0].node)), 0, &loc), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: rescue_mod!($1, $4) %*/
                    }
#line 10917 "parse.c"
    break;

  case 62: /* expr: expr "`and'" expr  */
#line 2636 "parse.y"
                  {
                        (yyval.node) = logop(p, idAND, (yyvsp[-2].node), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 10925 "parse.c"
    break;

  case 63: /* expr: expr "`or'" expr  */
#line 2640 "parse.y"
                  {
                        (yyval.node) = logop(p, idOR, (yyvsp[-2].node), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 10933 "parse.c"
    break;

  case 64: /* expr: "`not'" opt_nl expr  */
#line 2644 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, method_cond(p, (yyvsp[0].node), &(yylsp[0])), METHOD_NOT, &(yylsp[-2]), &(yyloc));
                    }
#line 10941 "parse.c"
    break;

  case 65: /* expr: '!' command_call  */
#line 2648 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, method_cond(p, (yyvsp[0].node), &(yylsp[0])), '!', &(yylsp[-1]), &(yyloc));
                    }
#line 10949 "parse.c"
    break;

  case 66: /* $@7: %empty  */
#line 2652 "parse.y"
                  {
                        value_expr((yyvsp[-1].node));
                    }
#line 10957 "parse.c"
    break;

  case 67: /* expr: arg "=>" $@7 p_in_kwarg p_pvtbl p_pktbl p_top_expr_body  */
#line 2657 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-1].tbl));
                        pop_pvtbl(p, (yyvsp[-2].tbl));
                        p->ctxt.in_kwarg = (yyvsp[-3].ctxt).in_kwarg;
                    /*%%%*/
                        (yyval.node) = NEW_CASE3((yyvsp[-6].node), NEW_IN((yyvsp[0].node), 0, 0, &(yylsp[0])), &(yyloc));
                    /*% %*/
                    /*% ripper: case!($arg, in!($body, Qnil, Qnil)) %*/
                    }
#line 10971 "parse.c"
    break;

  case 68: /* $@8: %empty  */
#line 2667 "parse.y"
                  {
                        value_expr((yyvsp[-1].node));
                    }
#line 10979 "parse.c"
    break;

  case 69: /* expr: arg "`in'" $@8 p_in_kwarg p_pvtbl p_pktbl p_top_expr_body  */
#line 2672 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-1].tbl));
                        pop_pvtbl(p, (yyvsp[-2].tbl));
                        p->ctxt.in_kwarg = (yyvsp[-3].ctxt).in_kwarg;
                    /*%%%*/
                        (yyval.node) = NEW_CASE3((yyvsp[-6].node), NEW_IN((yyvsp[0].node), NEW_TRUE(&(yylsp[0])), NEW_FALSE(&(yylsp[0])), &(yylsp[0])), &(yyloc));
                    /*% %*/
                    /*% ripper: case!($arg, in!($body, Qnil, Qnil)) %*/
                    }
#line 10993 "parse.c"
    break;

  case 71: /* def_name: fname  */
#line 2685 "parse.y"
                  {
                        ID fname = get_id((yyvsp[0].id));
                        numparam_name(p, fname);
                        local_push(p, 0);
                        p->cur_arg = 0;
                        p->ctxt.in_def = 1;
                        p->ctxt.in_rescue = before_rescue;
                        (yyval.id) = (yyvsp[0].id);
                    }
#line 11007 "parse.c"
    break;

  case 72: /* defn_head: k_def def_name  */
#line 2697 "parse.y"
                  {
                        (yyval.node_def_temp) = def_head_save(p, (yyvsp[-1].node_def_temp));
                        (yyval.node_def_temp)->nd_mid = (yyvsp[0].id);
                    /*%%%*/
                        (yyval.node_def_temp)->nd_def = NEW_DEFN((yyvsp[0].id), 0, &(yyloc));
                    /*%
                        add_mark_object(p, $def_name);
                    %*/
                    }
#line 11021 "parse.c"
    break;

  case 73: /* $@9: %empty  */
#line 2709 "parse.y"
                  {
                        SET_LEX_STATE(EXPR_FNAME);
                        p->ctxt.in_argdef = 1;
                    }
#line 11030 "parse.c"
    break;

  case 74: /* defs_head: k_def singleton dot_or_colon $@9 def_name  */
#line 2714 "parse.y"
                  {
                        SET_LEX_STATE(EXPR_ENDFN|EXPR_LABEL); /* force for args */
                        (yyval.node_def_temp) = def_head_save(p, (yyvsp[-4].node_def_temp));
                        (yyval.node_def_temp)->nd_mid = (yyvsp[0].id);
                    /*%%%*/
                        (yyval.node_def_temp)->nd_def = NEW_DEFS((yyvsp[-3].node), (yyvsp[0].id), 0, &(yyloc));
                    /*%
                        add_mark_object(p, $def_name);
                        $$->nd_recv = add_mark_object(p, $singleton);
                        $$->dot_or_colon = add_mark_object(p, $dot_or_colon);
                    %*/
                    }
#line 11047 "parse.c"
    break;

  case 75: /* expr_value: expr  */
#line 2729 "parse.y"
                  {
                        value_expr((yyvsp[0].node));
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 11056 "parse.c"
    break;

  case 76: /* expr_value: error  */
#line 2734 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_ERROR(&(yyloc));
                    /*% %*/
                    }
#line 11066 "parse.c"
    break;

  case 77: /* $@10: %empty  */
#line 2741 "parse.y"
              {COND_PUSH(1);}
#line 11072 "parse.c"
    break;

  case 78: /* $@11: %empty  */
#line 2741 "parse.y"
                                            {COND_POP();}
#line 11078 "parse.c"
    break;

  case 79: /* expr_value_do: $@10 expr_value do $@11  */
#line 2742 "parse.y"
                  {
                        (yyval.node) = (yyvsp[-2].node);
                    }
#line 11086 "parse.c"
    break;

  case 83: /* block_command: block_call call_op2 operation2 command_args  */
#line 2753 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_qcall(p, (yyvsp[-2].id), (yyvsp[-3].node), (yyvsp[-1].id), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    /*% %*/
                    /*% ripper: method_add_arg!(call!($1, $2, $3), $4) %*/
                    }
#line 11097 "parse.c"
    break;

  case 84: /* cmd_brace_block: "{ arg" brace_body '}'  */
#line 2762 "parse.y"
                  {
                        (yyval.node) = (yyvsp[-1].node);
                    /*%%%*/
                        set_embraced_location((yyval.node), &(yylsp[-2]), &(yylsp[0]));
                    /*% %*/
                    }
#line 11108 "parse.c"
    break;

  case 85: /* fcall: operation  */
#line 2771 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_fcall) = NEW_FCALL((yyvsp[0].id), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: $1 %*/
                    }
#line 11119 "parse.c"
    break;

  case 86: /* command: fcall command_args  */
#line 2780 "parse.y"
                  {
                    /*%%%*/
                        (yyvsp[-1].node_fcall)->nd_args = (yyvsp[0].node);
                        nd_set_last_loc((yyvsp[-1].node_fcall), (yylsp[0]).end_pos);
                        (yyval.node) = (NODE *)(yyvsp[-1].node_fcall);
                    /*% %*/
                    /*% ripper: command!($1, $2) %*/
                    }
#line 11132 "parse.c"
    break;

  case 87: /* command: fcall command_args cmd_brace_block  */
#line 2789 "parse.y"
                  {
                    /*%%%*/
                        block_dup_check(p, (yyvsp[-1].node), (yyvsp[0].node));
                        (yyvsp[-2].node_fcall)->nd_args = (yyvsp[-1].node);
                        (yyval.node) = method_add_block(p, (NODE *)(yyvsp[-2].node_fcall), (yyvsp[0].node), &(yyloc));
                        fixpos((yyval.node), RNODE((yyvsp[-2].node_fcall)));
                        nd_set_last_loc((yyvsp[-2].node_fcall), (yylsp[-1]).end_pos);
                    /*% %*/
                    /*% ripper: method_add_block!(command!($1, $2), $3) %*/
                    }
#line 11147 "parse.c"
    break;

  case 88: /* command: primary_value call_op operation2 command_args  */
#line 2800 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_command_qcall(p, (yyvsp[-2].id), (yyvsp[-3].node), (yyvsp[-1].id), (yyvsp[0].node), Qnull, &(yylsp[-1]), &(yyloc));
                    /*% %*/
                    /*% ripper: command_call!($1, $2, $3, $4) %*/
                    }
#line 11158 "parse.c"
    break;

  case 89: /* command: primary_value call_op operation2 command_args cmd_brace_block  */
#line 2807 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_command_qcall(p, (yyvsp[-3].id), (yyvsp[-4].node), (yyvsp[-2].id), (yyvsp[-1].node), (yyvsp[0].node), &(yylsp[-2]), &(yyloc));
                    /*% %*/
                    /*% ripper: method_add_block!(command_call!($1, $2, $3, $4), $5) %*/
                    }
#line 11169 "parse.c"
    break;

  case 90: /* command: primary_value "::" operation2 command_args  */
#line 2814 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_command_qcall(p, ID2VAL(idCOLON2), (yyvsp[-3].node), (yyvsp[-1].id), (yyvsp[0].node), Qnull, &(yylsp[-1]), &(yyloc));
                    /*% %*/
                    /*% ripper: command_call!($1, $2, $3, $4) %*/
                    }
#line 11180 "parse.c"
    break;

  case 91: /* command: primary_value "::" operation2 command_args cmd_brace_block  */
#line 2821 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_command_qcall(p, ID2VAL(idCOLON2), (yyvsp[-4].node), (yyvsp[-2].id), (yyvsp[-1].node), (yyvsp[0].node), &(yylsp[-2]), &(yyloc));
                    /*% %*/
                    /*% ripper: method_add_block!(command_call!($1, $2, $3, $4), $5) %*/
                   }
#line 11191 "parse.c"
    break;

  case 92: /* command: primary_value "::" "constant" '{' brace_body '}'  */
#line 2828 "parse.y"
                  {
                    /*%%%*/
                        set_embraced_location((yyvsp[-1].node), &(yylsp[-2]), &(yylsp[0]));
                        (yyval.node) = new_command_qcall(p, ID2VAL(idCOLON2), (yyvsp[-5].node), (yyvsp[-3].id), Qnull, (yyvsp[-1].node), &(yylsp[-3]), &(yyloc));
                    /*% %*/
                    /*% ripper: method_add_block!(command_call!($1, $2, $3, Qnull), $5) %*/
                   }
#line 11203 "parse.c"
    break;

  case 93: /* command: "`super'" command_args  */
#line 2836 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_SUPER((yyvsp[0].node), &(yyloc));
                        fixpos((yyval.node), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: super!($2) %*/
                    }
#line 11215 "parse.c"
    break;

  case 94: /* command: k_yield command_args  */
#line 2844 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_yield(p, (yyvsp[0].node), &(yyloc));
                        fixpos((yyval.node), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: yield!($2) %*/
                    }
#line 11227 "parse.c"
    break;

  case 95: /* command: k_return call_args  */
#line 2852 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_RETURN(ret_args(p, (yyvsp[0].node)), &(yyloc));
                    /*% %*/
                    /*% ripper: return!($2) %*/
                    }
#line 11238 "parse.c"
    break;

  case 96: /* command: "`break'" call_args  */
#line 2859 "parse.y"
                  {
                        NODE *args = 0;
                    /*%%%*/
                        args = ret_args(p, (yyvsp[0].node));
                    /*% %*/
                        (yyval.node) = add_block_exit(p, NEW_BREAK(args, &(yyloc)));
                    /*% ripper: break!($2) %*/
                    }
#line 11251 "parse.c"
    break;

  case 97: /* command: "`next'" call_args  */
#line 2868 "parse.y"
                  {
                        NODE *args = 0;
                    /*%%%*/
                        args = ret_args(p, (yyvsp[0].node));
                    /*% %*/
                        (yyval.node) = add_block_exit(p, NEW_NEXT(args, &(yyloc)));
                    /*% ripper: next!($2) %*/
                    }
#line 11264 "parse.c"
    break;

  case 99: /* mlhs: "(" mlhs_inner rparen  */
#line 2880 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = (yyvsp[-1].node_masgn);
                    /*% %*/
                    /*% ripper: mlhs_paren!($2) %*/
                    }
#line 11275 "parse.c"
    break;

  case 101: /* mlhs_inner: "(" mlhs_inner rparen  */
#line 2890 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN(NEW_LIST((NODE *)(yyvsp[-1].node_masgn), &(yyloc)), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_paren!($2) %*/
                    }
#line 11286 "parse.c"
    break;

  case 102: /* mlhs_basic: mlhs_head  */
#line 2899 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN((yyvsp[0].node), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: $1 %*/
                    }
#line 11297 "parse.c"
    break;

  case 103: /* mlhs_basic: mlhs_head mlhs_item  */
#line 2906 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN(list_append(p, (yyvsp[-1].node), (yyvsp[0].node)), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add!($1, $2) %*/
                    }
#line 11308 "parse.c"
    break;

  case 104: /* mlhs_basic: mlhs_head "*" mlhs_node  */
#line 2913 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN((yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_star!($1, $3) %*/
                    }
#line 11319 "parse.c"
    break;

  case 105: /* mlhs_basic: mlhs_head "*" mlhs_node ',' mlhs_post  */
#line 2920 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN((yyvsp[-4].node), NEW_POSTARG((yyvsp[-2].node),(yyvsp[0].node),&(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_post!(mlhs_add_star!($1, $3), $5) %*/
                    }
#line 11330 "parse.c"
    break;

  case 106: /* mlhs_basic: mlhs_head "*"  */
#line 2927 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN((yyvsp[-1].node), NODE_SPECIAL_NO_NAME_REST, &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_star!($1, Qnil) %*/
                    }
#line 11341 "parse.c"
    break;

  case 107: /* mlhs_basic: mlhs_head "*" ',' mlhs_post  */
#line 2934 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN((yyvsp[-3].node), NEW_POSTARG(NODE_SPECIAL_NO_NAME_REST, (yyvsp[0].node), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_post!(mlhs_add_star!($1, Qnil), $4) %*/
                    }
#line 11352 "parse.c"
    break;

  case 108: /* mlhs_basic: "*" mlhs_node  */
#line 2941 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN(0, (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_star!(mlhs_new!, $2) %*/
                    }
#line 11363 "parse.c"
    break;

  case 109: /* mlhs_basic: "*" mlhs_node ',' mlhs_post  */
#line 2948 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN(0, NEW_POSTARG((yyvsp[-2].node),(yyvsp[0].node),&(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_post!(mlhs_add_star!(mlhs_new!, $2), $4) %*/
                    }
#line 11374 "parse.c"
    break;

  case 110: /* mlhs_basic: "*"  */
#line 2955 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN(0, NODE_SPECIAL_NO_NAME_REST, &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_star!(mlhs_new!, Qnil) %*/
                    }
#line 11385 "parse.c"
    break;

  case 111: /* mlhs_basic: "*" ',' mlhs_post  */
#line 2962 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN(0, NEW_POSTARG(NODE_SPECIAL_NO_NAME_REST, (yyvsp[0].node), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_post!(mlhs_add_star!(mlhs_new!, Qnil), $3) %*/
                    }
#line 11396 "parse.c"
    break;

  case 113: /* mlhs_item: "(" mlhs_inner rparen  */
#line 2972 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (NODE *)(yyvsp[-1].node_masgn);
                    /*% %*/
                    /*% ripper: mlhs_paren!($2) %*/
                    }
#line 11407 "parse.c"
    break;

  case 114: /* mlhs_head: mlhs_item ','  */
#line 2981 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_LIST((yyvsp[-1].node), &(yylsp[-1]));
                    /*% %*/
                    /*% ripper: mlhs_add!(mlhs_new!, $1) %*/
                    }
#line 11418 "parse.c"
    break;

  case 115: /* mlhs_head: mlhs_head mlhs_item ','  */
#line 2988 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = list_append(p, (yyvsp[-2].node), (yyvsp[-1].node));
                    /*% %*/
                    /*% ripper: mlhs_add!($1, $2) %*/
                    }
#line 11429 "parse.c"
    break;

  case 116: /* mlhs_post: mlhs_item  */
#line 2997 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_LIST((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add!(mlhs_new!, $1) %*/
                    }
#line 11440 "parse.c"
    break;

  case 117: /* mlhs_post: mlhs_post ',' mlhs_item  */
#line 3004 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = list_append(p, (yyvsp[-2].node), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: mlhs_add!($1, $3) %*/
                    }
#line 11451 "parse.c"
    break;

  case 118: /* mlhs_node: user_variable  */
#line 3013 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = assignable(p, (yyvsp[0].id), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: assignable(p, var_field(p, $1)) %*/
                    }
#line 11462 "parse.c"
    break;

  case 119: /* mlhs_node: keyword_variable  */
#line 3020 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = assignable(p, (yyvsp[0].id), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: assignable(p, var_field(p, $1)) %*/
                    }
#line 11473 "parse.c"
    break;

  case 120: /* mlhs_node: primary_value '[' opt_call_args rbracket  */
#line 3027 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = aryset(p, (yyvsp[-3].node), (yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: aref_field!($1, $3) %*/
                    }
#line 11484 "parse.c"
    break;

  case 121: /* mlhs_node: primary_value call_op "local variable or method"  */
#line 3034 "parse.y"
                  {
                        anddot_multiple_assignment_check(p, &(yylsp[-1]), (yyvsp[-1].id));
                    /*%%%*/
                        (yyval.node) = attrset(p, (yyvsp[-2].node), (yyvsp[-1].id), (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: field!($1, $2, $3) %*/
                    }
#line 11496 "parse.c"
    break;

  case 122: /* mlhs_node: primary_value "::" "local variable or method"  */
#line 3042 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = attrset(p, (yyvsp[-2].node), idCOLON2, (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: const_path_field!($1, $3) %*/
                    }
#line 11507 "parse.c"
    break;

  case 123: /* mlhs_node: primary_value call_op "constant"  */
#line 3049 "parse.y"
                  {
                        anddot_multiple_assignment_check(p, &(yylsp[-1]), (yyvsp[-1].id));
                    /*%%%*/
                        (yyval.node) = attrset(p, (yyvsp[-2].node), (yyvsp[-1].id), (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: field!($1, $2, $3) %*/
                    }
#line 11519 "parse.c"
    break;

  case 124: /* mlhs_node: primary_value "::" "constant"  */
#line 3057 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = const_decl(p, NEW_COLON2((yyvsp[-2].node), (yyvsp[0].id), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: const_decl(p, const_path_field!($1, $3)) %*/
                    }
#line 11530 "parse.c"
    break;

  case 125: /* mlhs_node: ":: at EXPR_BEG" "constant"  */
#line 3064 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = const_decl(p, NEW_COLON3((yyvsp[0].id), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: const_decl(p, top_const_field!($2)) %*/
                    }
#line 11541 "parse.c"
    break;

  case 126: /* mlhs_node: backref  */
#line 3071 "parse.y"
                  {
                    /*%%%*/
                        rb_backref_error(p, (yyvsp[0].node));
                        (yyval.node) = NEW_ERROR(&(yyloc));
                    /*% %*/
                    /*% ripper[error]: backref_error(p, RNODE($1), var_field(p, $1)) %*/
                    }
#line 11553 "parse.c"
    break;

  case 127: /* lhs: user_variable  */
#line 3081 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = assignable(p, (yyvsp[0].id), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: assignable(p, var_field(p, $1)) %*/
                    }
#line 11564 "parse.c"
    break;

  case 128: /* lhs: keyword_variable  */
#line 3088 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = assignable(p, (yyvsp[0].id), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: assignable(p, var_field(p, $1)) %*/
                    }
#line 11575 "parse.c"
    break;

  case 129: /* lhs: primary_value '[' opt_call_args rbracket  */
#line 3095 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = aryset(p, (yyvsp[-3].node), (yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: aref_field!($1, $3) %*/
                    }
#line 11586 "parse.c"
    break;

  case 130: /* lhs: primary_value call_op "local variable or method"  */
#line 3102 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = attrset(p, (yyvsp[-2].node), (yyvsp[-1].id), (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: field!($1, $2, $3) %*/
                    }
#line 11597 "parse.c"
    break;

  case 131: /* lhs: primary_value "::" "local variable or method"  */
#line 3109 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = attrset(p, (yyvsp[-2].node), idCOLON2, (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: field!($1, $2, $3) %*/
                    }
#line 11608 "parse.c"
    break;

  case 132: /* lhs: primary_value call_op "constant"  */
#line 3116 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = attrset(p, (yyvsp[-2].node), (yyvsp[-1].id), (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: field!($1, $2, $3) %*/
                    }
#line 11619 "parse.c"
    break;

  case 133: /* lhs: primary_value "::" "constant"  */
#line 3123 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = const_decl(p, NEW_COLON2((yyvsp[-2].node), (yyvsp[0].id), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: const_decl(p, const_path_field!($1, $3)) %*/
                    }
#line 11630 "parse.c"
    break;

  case 134: /* lhs: ":: at EXPR_BEG" "constant"  */
#line 3130 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = const_decl(p, NEW_COLON3((yyvsp[0].id), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: const_decl(p, top_const_field!($2)) %*/
                    }
#line 11641 "parse.c"
    break;

  case 135: /* lhs: backref  */
#line 3137 "parse.y"
                  {
                    /*%%%*/
                        rb_backref_error(p, (yyvsp[0].node));
                        (yyval.node) = NEW_ERROR(&(yyloc));
                    /*% %*/
                    /*% ripper[error]: backref_error(p, RNODE($1), var_field(p, $1)) %*/
                    }
#line 11653 "parse.c"
    break;

  case 136: /* cname: "local variable or method"  */
#line 3147 "parse.y"
                  {
                        static const char mesg[] = "class/module name must be CONSTANT";
                    /*%%%*/
                        yyerror1(&(yylsp[0]), mesg);
                    /*% %*/
                    /*% ripper[error]: class_name_error!(ERR_MESG(), $1) %*/
                    }
#line 11665 "parse.c"
    break;

  case 138: /* cpath: ":: at EXPR_BEG" cname  */
#line 3158 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_COLON3((yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: top_const_ref!($2) %*/
                    }
#line 11676 "parse.c"
    break;

  case 139: /* cpath: cname  */
#line 3165 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_COLON2(0, (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: const_ref!($1) %*/
                    }
#line 11687 "parse.c"
    break;

  case 140: /* cpath: primary_value "::" cname  */
#line 3172 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_COLON2((yyvsp[-2].node), (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: const_path_ref!($1, $3) %*/
                    }
#line 11698 "parse.c"
    break;

  case 144: /* fname: op  */
#line 3184 "parse.y"
                  {
                        SET_LEX_STATE(EXPR_ENDFN);
                        (yyval.id) = (yyvsp[0].id);
                    }
#line 11707 "parse.c"
    break;

  case 146: /* fitem: fname  */
#line 3192 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_LIT(ID2SYM((yyvsp[0].id)), &(yyloc));
                    /*% %*/
                    /*% ripper: symbol_literal!($1) %*/
                    }
#line 11718 "parse.c"
    break;

  case 148: /* undef_list: fitem  */
#line 3202 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_UNDEF((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: rb_ary_new3(1, get_value($1)) %*/
                    }
#line 11729 "parse.c"
    break;

  case 149: /* $@12: %empty  */
#line 3208 "parse.y"
                               {SET_LEX_STATE(EXPR_FNAME|EXPR_FITEM);}
#line 11735 "parse.c"
    break;

  case 150: /* undef_list: undef_list ',' $@12 fitem  */
#line 3209 "parse.y"
                  {
                    /*%%%*/
                        NODE *undef = NEW_UNDEF((yyvsp[0].node), &(yylsp[0]));
                        (yyval.node) = block_append(p, (yyvsp[-3].node), undef);
                    /*% %*/
                    /*% ripper: rb_ary_push($1, get_value($4)) %*/
                    }
#line 11747 "parse.c"
    break;

  case 151: /* op: '|'  */
#line 3218 "parse.y"
         { ifndef_ripper((yyval.id) = '|'); }
#line 11753 "parse.c"
    break;

  case 152: /* op: '^'  */
#line 3219 "parse.y"
                     { ifndef_ripper((yyval.id) = '^'); }
#line 11759 "parse.c"
    break;

  case 153: /* op: '&'  */
#line 3220 "parse.y"
                     { ifndef_ripper((yyval.id) = '&'); }
#line 11765 "parse.c"
    break;

  case 154: /* op: "<=>"  */
#line 3221 "parse.y"
                      { ifndef_ripper((yyval.id) = tCMP); }
#line 11771 "parse.c"
    break;

  case 155: /* op: "=="  */
#line 3222 "parse.y"
                     { ifndef_ripper((yyval.id) = tEQ); }
#line 11777 "parse.c"
    break;

  case 156: /* op: "==="  */
#line 3223 "parse.y"
                      { ifndef_ripper((yyval.id) = tEQQ); }
#line 11783 "parse.c"
    break;

  case 157: /* op: "=~"  */
#line 3224 "parse.y"
                       { ifndef_ripper((yyval.id) = tMATCH); }
#line 11789 "parse.c"
    break;

  case 158: /* op: "!~"  */
#line 3225 "parse.y"
                        { ifndef_ripper((yyval.id) = tNMATCH); }
#line 11795 "parse.c"
    break;

  case 159: /* op: '>'  */
#line 3226 "parse.y"
                     { ifndef_ripper((yyval.id) = '>'); }
#line 11801 "parse.c"
    break;

  case 160: /* op: ">="  */
#line 3227 "parse.y"
                      { ifndef_ripper((yyval.id) = tGEQ); }
#line 11807 "parse.c"
    break;

  case 161: /* op: '<'  */
#line 3228 "parse.y"
                     { ifndef_ripper((yyval.id) = '<'); }
#line 11813 "parse.c"
    break;

  case 162: /* op: "<="  */
#line 3229 "parse.y"
                      { ifndef_ripper((yyval.id) = tLEQ); }
#line 11819 "parse.c"
    break;

  case 163: /* op: "!="  */
#line 3230 "parse.y"
                      { ifndef_ripper((yyval.id) = tNEQ); }
#line 11825 "parse.c"
    break;

  case 164: /* op: "<<"  */
#line 3231 "parse.y"
                       { ifndef_ripper((yyval.id) = tLSHFT); }
#line 11831 "parse.c"
    break;

  case 165: /* op: ">>"  */
#line 3232 "parse.y"
                       { ifndef_ripper((yyval.id) = tRSHFT); }
#line 11837 "parse.c"
    break;

  case 166: /* op: '+'  */
#line 3233 "parse.y"
                     { ifndef_ripper((yyval.id) = '+'); }
#line 11843 "parse.c"
    break;

  case 167: /* op: '-'  */
#line 3234 "parse.y"
                     { ifndef_ripper((yyval.id) = '-'); }
#line 11849 "parse.c"
    break;

  case 168: /* op: '*'  */
#line 3235 "parse.y"
                     { ifndef_ripper((yyval.id) = '*'); }
#line 11855 "parse.c"
    break;

  case 169: /* op: "*"  */
#line 3236 "parse.y"
                       { ifndef_ripper((yyval.id) = '*'); }
#line 11861 "parse.c"
    break;

  case 170: /* op: '/'  */
#line 3237 "parse.y"
                     { ifndef_ripper((yyval.id) = '/'); }
#line 11867 "parse.c"
    break;

  case 171: /* op: '%'  */
#line 3238 "parse.y"
                     { ifndef_ripper((yyval.id) = '%'); }
#line 11873 "parse.c"
    break;

  case 172: /* op: "**"  */
#line 3239 "parse.y"
                      { ifndef_ripper((yyval.id) = tPOW); }
#line 11879 "parse.c"
    break;

  case 173: /* op: "**arg"  */
#line 3240 "parse.y"
                       { ifndef_ripper((yyval.id) = tDSTAR); }
#line 11885 "parse.c"
    break;

  case 174: /* op: '!'  */
#line 3241 "parse.y"
                     { ifndef_ripper((yyval.id) = '!'); }
#line 11891 "parse.c"
    break;

  case 175: /* op: '~'  */
#line 3242 "parse.y"
                     { ifndef_ripper((yyval.id) = '~'); }
#line 11897 "parse.c"
    break;

  case 176: /* op: "unary+"  */
#line 3243 "parse.y"
                       { ifndef_ripper((yyval.id) = tUPLUS); }
#line 11903 "parse.c"
    break;

  case 177: /* op: "unary-"  */
#line 3244 "parse.y"
                        { ifndef_ripper((yyval.id) = tUMINUS); }
#line 11909 "parse.c"
    break;

  case 178: /* op: "[]"  */
#line 3245 "parse.y"
                       { ifndef_ripper((yyval.id) = tAREF); }
#line 11915 "parse.c"
    break;

  case 179: /* op: "[]="  */
#line 3246 "parse.y"
                       { ifndef_ripper((yyval.id) = tASET); }
#line 11921 "parse.c"
    break;

  case 180: /* op: '`'  */
#line 3247 "parse.y"
                     { ifndef_ripper((yyval.id) = '`'); }
#line 11927 "parse.c"
    break;

  case 222: /* arg: lhs '=' lex_ctxt arg_rhs  */
#line 3265 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = node_assign(p, (yyvsp[-3].node), (yyvsp[0].node), (yyvsp[-1].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: assign!($1, $4) %*/
                    }
#line 11938 "parse.c"
    break;

  case 223: /* arg: var_lhs "operator-assignment" lex_ctxt arg_rhs  */
#line 3272 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_op_assign(p, (yyvsp[-3].node), (yyvsp[-2].id), (yyvsp[0].node), (yyvsp[-1].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!($1, $2, $4) %*/
                    }
#line 11949 "parse.c"
    break;

  case 224: /* arg: primary_value '[' opt_call_args rbracket "operator-assignment" lex_ctxt arg_rhs  */
#line 3279 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_ary_op_assign(p, (yyvsp[-6].node), (yyvsp[-4].node), (yyvsp[-2].id), (yyvsp[0].node), &(yylsp[-4]), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(aref_field!($1, $3), $5, $7) %*/
                    }
#line 11960 "parse.c"
    break;

  case 225: /* arg: primary_value call_op "local variable or method" "operator-assignment" lex_ctxt arg_rhs  */
#line 3286 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_attr_op_assign(p, (yyvsp[-5].node), (yyvsp[-4].id), (yyvsp[-3].id), (yyvsp[-2].id), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(field!($1, $2, $3), $4, $6) %*/
                    }
#line 11971 "parse.c"
    break;

  case 226: /* arg: primary_value call_op "constant" "operator-assignment" lex_ctxt arg_rhs  */
#line 3293 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_attr_op_assign(p, (yyvsp[-5].node), (yyvsp[-4].id), (yyvsp[-3].id), (yyvsp[-2].id), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(field!($1, $2, $3), $4, $6) %*/
                    }
#line 11982 "parse.c"
    break;

  case 227: /* arg: primary_value "::" "local variable or method" "operator-assignment" lex_ctxt arg_rhs  */
#line 3300 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_attr_op_assign(p, (yyvsp[-5].node), ID2VAL(idCOLON2), (yyvsp[-3].id), (yyvsp[-2].id), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(field!($1, $2, $3), $4, $6) %*/
                    }
#line 11993 "parse.c"
    break;

  case 228: /* arg: primary_value "::" "constant" "operator-assignment" lex_ctxt arg_rhs  */
#line 3307 "parse.y"
                  {
                    /*%%%*/
                        YYLTYPE loc = code_loc_gen(&(yylsp[-5]), &(yylsp[-3]));
                        (yyval.node) = new_const_op_assign(p, NEW_COLON2((yyvsp[-5].node), (yyvsp[-3].id), &loc), (yyvsp[-2].id), (yyvsp[0].node), (yyvsp[-1].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(const_path_field!($1, $3), $4, $6) %*/
                    }
#line 12005 "parse.c"
    break;

  case 229: /* arg: ":: at EXPR_BEG" "constant" "operator-assignment" lex_ctxt arg_rhs  */
#line 3315 "parse.y"
                  {
                    /*%%%*/
                        YYLTYPE loc = code_loc_gen(&(yylsp[-4]), &(yylsp[-3]));
                        (yyval.node) = new_const_op_assign(p, NEW_COLON3((yyvsp[-3].id), &loc), (yyvsp[-2].id), (yyvsp[0].node), (yyvsp[-1].ctxt), &(yyloc));
                    /*% %*/
                    /*% ripper: opassign!(top_const_field!($2), $3, $5) %*/
                    }
#line 12017 "parse.c"
    break;

  case 230: /* arg: backref "operator-assignment" lex_ctxt arg_rhs  */
#line 3323 "parse.y"
                  {
                    /*%%%*/
                        rb_backref_error(p, (yyvsp[-3].node));
                        (yyval.node) = NEW_ERROR(&(yyloc));
                    /*% %*/
                    /*% ripper[error]: backref_error(p, RNODE($1), opassign!(var_field(p, $1), $2, $4)) %*/
                    }
#line 12029 "parse.c"
    break;

  case 231: /* arg: arg ".." arg  */
#line 3331 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[-2].node));
                        value_expr((yyvsp[0].node));
                        (yyval.node) = NEW_DOT2((yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: dot2!($1, $3) %*/
                    }
#line 12042 "parse.c"
    break;

  case 232: /* arg: arg "..." arg  */
#line 3340 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[-2].node));
                        value_expr((yyvsp[0].node));
                        (yyval.node) = NEW_DOT3((yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: dot3!($1, $3) %*/
                    }
#line 12055 "parse.c"
    break;

  case 233: /* arg: arg ".."  */
#line 3349 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[-1].node));
                        (yyval.node) = NEW_DOT2((yyvsp[-1].node), new_nil_at(p, &(yylsp[0]).end_pos), &(yyloc));
                    /*% %*/
                    /*% ripper: dot2!($1, Qnil) %*/
                    }
#line 12067 "parse.c"
    break;

  case 234: /* arg: arg "..."  */
#line 3357 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[-1].node));
                        (yyval.node) = NEW_DOT3((yyvsp[-1].node), new_nil_at(p, &(yylsp[0]).end_pos), &(yyloc));
                    /*% %*/
                    /*% ripper: dot3!($1, Qnil) %*/
                    }
#line 12079 "parse.c"
    break;

  case 235: /* arg: "(.." arg  */
#line 3365 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[0].node));
                        (yyval.node) = NEW_DOT2(new_nil_at(p, &(yylsp[-1]).beg_pos), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: dot2!(Qnil, $2) %*/
                    }
#line 12091 "parse.c"
    break;

  case 236: /* arg: "(..." arg  */
#line 3373 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[0].node));
                        (yyval.node) = NEW_DOT3(new_nil_at(p, &(yylsp[-1]).beg_pos), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: dot3!(Qnil, $2) %*/
                    }
#line 12103 "parse.c"
    break;

  case 237: /* arg: arg '+' arg  */
#line 3381 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), '+', (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12111 "parse.c"
    break;

  case 238: /* arg: arg '-' arg  */
#line 3385 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), '-', (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12119 "parse.c"
    break;

  case 239: /* arg: arg '*' arg  */
#line 3389 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), '*', (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12127 "parse.c"
    break;

  case 240: /* arg: arg '/' arg  */
#line 3393 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), '/', (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12135 "parse.c"
    break;

  case 241: /* arg: arg '%' arg  */
#line 3397 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), '%', (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12143 "parse.c"
    break;

  case 242: /* arg: arg "**" arg  */
#line 3401 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), idPow, (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12151 "parse.c"
    break;

  case 243: /* arg: tUMINUS_NUM simple_numeric "**" arg  */
#line 3405 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, call_bin_op(p, (yyvsp[-2].node), idPow, (yyvsp[0].node), &(yylsp[-2]), &(yyloc)), idUMinus, &(yylsp[-3]), &(yyloc));
                    }
#line 12159 "parse.c"
    break;

  case 244: /* arg: "unary+" arg  */
#line 3409 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, (yyvsp[0].node), idUPlus, &(yylsp[-1]), &(yyloc));
                    }
#line 12167 "parse.c"
    break;

  case 245: /* arg: "unary-" arg  */
#line 3413 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, (yyvsp[0].node), idUMinus, &(yylsp[-1]), &(yyloc));
                    }
#line 12175 "parse.c"
    break;

  case 246: /* arg: arg '|' arg  */
#line 3417 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), '|', (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12183 "parse.c"
    break;

  case 247: /* arg: arg '^' arg  */
#line 3421 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), '^', (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12191 "parse.c"
    break;

  case 248: /* arg: arg '&' arg  */
#line 3425 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), '&', (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12199 "parse.c"
    break;

  case 249: /* arg: arg "<=>" arg  */
#line 3429 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), idCmp, (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12207 "parse.c"
    break;

  case 251: /* arg: arg "==" arg  */
#line 3434 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), idEq, (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12215 "parse.c"
    break;

  case 252: /* arg: arg "===" arg  */
#line 3438 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), idEqq, (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12223 "parse.c"
    break;

  case 253: /* arg: arg "!=" arg  */
#line 3442 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), idNeq, (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12231 "parse.c"
    break;

  case 254: /* arg: arg "=~" arg  */
#line 3446 "parse.y"
                  {
                        (yyval.node) = match_op(p, (yyvsp[-2].node), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12239 "parse.c"
    break;

  case 255: /* arg: arg "!~" arg  */
#line 3450 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), idNeqTilde, (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12247 "parse.c"
    break;

  case 256: /* arg: '!' arg  */
#line 3454 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, method_cond(p, (yyvsp[0].node), &(yylsp[0])), '!', &(yylsp[-1]), &(yyloc));
                    }
#line 12255 "parse.c"
    break;

  case 257: /* arg: '~' arg  */
#line 3458 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, (yyvsp[0].node), '~', &(yylsp[-1]), &(yyloc));
                    }
#line 12263 "parse.c"
    break;

  case 258: /* arg: arg "<<" arg  */
#line 3462 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), idLTLT, (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12271 "parse.c"
    break;

  case 259: /* arg: arg ">>" arg  */
#line 3466 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), idGTGT, (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12279 "parse.c"
    break;

  case 260: /* arg: arg "&&" arg  */
#line 3470 "parse.y"
                  {
                        (yyval.node) = logop(p, idANDOP, (yyvsp[-2].node), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12287 "parse.c"
    break;

  case 261: /* arg: arg "||" arg  */
#line 3474 "parse.y"
                  {
                        (yyval.node) = logop(p, idOROP, (yyvsp[-2].node), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12295 "parse.c"
    break;

  case 262: /* arg: "`defined?'" opt_nl begin_defined arg  */
#line 3478 "parse.y"
                  {
                        p->ctxt.in_defined = (yyvsp[-1].ctxt).in_defined;
                        (yyval.node) = new_defined(p, (yyvsp[0].node), &(yyloc));
                    }
#line 12304 "parse.c"
    break;

  case 263: /* arg: arg '?' arg opt_nl ':' arg  */
#line 3483 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[-5].node));
                        (yyval.node) = new_if(p, (yyvsp[-5].node), (yyvsp[-3].node), (yyvsp[0].node), &(yyloc));
                        fixpos((yyval.node), (yyvsp[-5].node));
                    /*% %*/
                    /*% ripper: ifop!($1, $3, $6) %*/
                    }
#line 12317 "parse.c"
    break;

  case 264: /* arg: defn_head f_opt_paren_args '=' endless_arg  */
#line 3492 "parse.y"
                  {
                        endless_method_name(p, get_id((yyvsp[-3].node_def_temp)->nd_mid), &(yylsp[-3]));
                        restore_defun(p, (yyvsp[-3].node_def_temp));
                    /*%%%*/
                        (yyvsp[0].node) = new_scope_body(p, (yyvsp[-2].node_args), (yyvsp[0].node), &(yyloc));
                        ((yyval.node) = (yyvsp[-3].node_def_temp)->nd_def)->nd_loc = (yyloc);
                        RNODE_DEFN((yyval.node))->nd_defn = (yyvsp[0].node);
                    /*% %*/
                    /*% ripper[$bodystmt]: bodystmt!($bodystmt, Qnil, Qnil, Qnil) %*/
                    /*% ripper: def!($head->nd_mid, $args, $bodystmt) %*/
                        local_pop(p);
                    }
#line 12334 "parse.c"
    break;

  case 265: /* arg: defs_head f_opt_paren_args '=' endless_arg  */
#line 3505 "parse.y"
                  {
                        endless_method_name(p, get_id((yyvsp[-3].node_def_temp)->nd_mid), &(yylsp[-3]));
                        restore_defun(p, (yyvsp[-3].node_def_temp));
                    /*%%%*/
                        (yyvsp[0].node) = new_scope_body(p, (yyvsp[-2].node_args), (yyvsp[0].node), &(yyloc));
                        ((yyval.node) = (yyvsp[-3].node_def_temp)->nd_def)->nd_loc = (yyloc);
                        RNODE_DEFS((yyval.node))->nd_defn = (yyvsp[0].node);
                    /*% %*/
                    /*% ripper[$bodystmt]: bodystmt!($bodystmt, Qnil, Qnil, Qnil) %*/
                    /*% ripper: defs!($head->nd_recv, $head->dot_or_colon, $head->nd_mid, $args, $bodystmt) %*/
                        local_pop(p);
                    }
#line 12351 "parse.c"
    break;

  case 266: /* arg: primary  */
#line 3518 "parse.y"
                  {
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 12359 "parse.c"
    break;

  case 268: /* endless_arg: endless_arg "`rescue' modifier" after_rescue arg  */
#line 3525 "parse.y"
                  {
                        p->ctxt.in_rescue = (yyvsp[-1].ctxt).in_rescue;
                    /*%%%*/
                        (yyval.node) = rescued_expr(p, (yyvsp[-3].node), (yyvsp[0].node), &(yylsp[-3]), &(yylsp[-2]), &(yylsp[0]));
                    /*% %*/
                    /*% ripper: rescue_mod!($1, $4) %*/
                    }
#line 12371 "parse.c"
    break;

  case 269: /* endless_arg: "`not'" opt_nl endless_arg  */
#line 3533 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, method_cond(p, (yyvsp[0].node), &(yylsp[0])), METHOD_NOT, &(yylsp[-2]), &(yyloc));
                    }
#line 12379 "parse.c"
    break;

  case 270: /* relop: '>'  */
#line 3538 "parse.y"
            {(yyval.id) = '>';}
#line 12385 "parse.c"
    break;

  case 271: /* relop: '<'  */
#line 3539 "parse.y"
                     {(yyval.id) = '<';}
#line 12391 "parse.c"
    break;

  case 272: /* relop: ">="  */
#line 3540 "parse.y"
                     {(yyval.id) = idGE;}
#line 12397 "parse.c"
    break;

  case 273: /* relop: "<="  */
#line 3541 "parse.y"
                     {(yyval.id) = idLE;}
#line 12403 "parse.c"
    break;

  case 274: /* rel_expr: arg relop arg  */
#line 3545 "parse.y"
                  {
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), (yyvsp[-1].id), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12411 "parse.c"
    break;

  case 275: /* rel_expr: rel_expr relop arg  */
#line 3549 "parse.y"
                  {
                        rb_warning1("comparison '%s' after comparison", WARN_ID((yyvsp[-1].id)));
                        (yyval.node) = call_bin_op(p, (yyvsp[-2].node), (yyvsp[-1].id), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    }
#line 12420 "parse.c"
    break;

  case 276: /* lex_ctxt: none  */
#line 3556 "parse.y"
                  {
                        (yyval.ctxt) = p->ctxt;
                    }
#line 12428 "parse.c"
    break;

  case 277: /* begin_defined: lex_ctxt  */
#line 3562 "parse.y"
                  {
                        p->ctxt.in_defined = 1;
                        (yyval.ctxt) = (yyvsp[0].ctxt);
                    }
#line 12437 "parse.c"
    break;

  case 278: /* after_rescue: lex_ctxt  */
#line 3569 "parse.y"
                  {
                        p->ctxt.in_rescue = after_rescue;
                        (yyval.ctxt) = (yyvsp[0].ctxt);
                    }
#line 12446 "parse.c"
    break;

  case 279: /* arg_value: arg  */
#line 3576 "parse.y"
                  {
                        value_expr((yyvsp[0].node));
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 12455 "parse.c"
    break;

  case 281: /* aref_args: args trailer  */
#line 3584 "parse.y"
                  {
                        (yyval.node) = (yyvsp[-1].node);
                    }
#line 12463 "parse.c"
    break;

  case 282: /* aref_args: args ',' assocs trailer  */
#line 3588 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[-1].node) ? arg_append(p, (yyvsp[-3].node), new_hash(p, (yyvsp[-1].node), &(yylsp[-1])), &(yyloc)) : (yyvsp[-3].node);
                    /*% %*/
                    /*% ripper: args_add!($1, bare_assoc_hash!($3)) %*/
                    }
#line 12474 "parse.c"
    break;

  case 283: /* aref_args: assocs trailer  */
#line 3595 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[-1].node) ? NEW_LIST(new_hash(p, (yyvsp[-1].node), &(yylsp[-1])), &(yyloc)) : 0;
                    /*% %*/
                    /*% ripper: args_add!(args_new!, bare_assoc_hash!($1)) %*/
                    }
#line 12485 "parse.c"
    break;

  case 284: /* arg_rhs: arg  */
#line 3604 "parse.y"
                  {
                        value_expr((yyvsp[0].node));
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 12494 "parse.c"
    break;

  case 285: /* arg_rhs: arg "`rescue' modifier" after_rescue arg  */
#line 3609 "parse.y"
                  {
                        p->ctxt.in_rescue = (yyvsp[-1].ctxt).in_rescue;
                    /*%%%*/
                        value_expr((yyvsp[-3].node));
                        (yyval.node) = rescued_expr(p, (yyvsp[-3].node), (yyvsp[0].node), &(yylsp[-3]), &(yylsp[-2]), &(yylsp[0]));
                    /*% %*/
                    /*% ripper: rescue_mod!($1, $4) %*/
                    }
#line 12507 "parse.c"
    break;

  case 286: /* paren_args: '(' opt_call_args rparen  */
#line 3620 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[-1].node);
                    /*% %*/
                    /*% ripper: arg_paren!($2) %*/
                    }
#line 12518 "parse.c"
    break;

  case 287: /* paren_args: '(' args ',' args_forward rparen  */
#line 3627 "parse.y"
                  {
                        if (!check_forwarding_args(p)) {
                            (yyval.node) = Qnone;
                        }
                        else {
                        /*%%%*/
                            (yyval.node) = new_args_forward_call(p, (yyvsp[-3].node), &(yylsp[-1]), &(yyloc));
                        /*% %*/
                        /*% ripper: arg_paren!(args_add!($2, $4)) %*/
                        }
                    }
#line 12534 "parse.c"
    break;

  case 288: /* paren_args: '(' args_forward rparen  */
#line 3639 "parse.y"
                  {
                        if (!check_forwarding_args(p)) {
                            (yyval.node) = Qnone;
                        }
                        else {
                        /*%%%*/
                            (yyval.node) = new_args_forward_call(p, 0, &(yylsp[-1]), &(yyloc));
                        /*% %*/
                        /*% ripper: arg_paren!($2) %*/
                        }
                    }
#line 12550 "parse.c"
    break;

  case 293: /* opt_call_args: args ','  */
#line 3659 "parse.y"
                  {
                      (yyval.node) = (yyvsp[-1].node);
                    }
#line 12558 "parse.c"
    break;

  case 294: /* opt_call_args: args ',' assocs ','  */
#line 3663 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[-1].node) ? arg_append(p, (yyvsp[-3].node), new_hash(p, (yyvsp[-1].node), &(yylsp[-1])), &(yyloc)) : (yyvsp[-3].node);
                    /*% %*/
                    /*% ripper: args_add!($1, bare_assoc_hash!($3)) %*/
                    }
#line 12569 "parse.c"
    break;

  case 295: /* opt_call_args: assocs ','  */
#line 3670 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[-1].node) ? NEW_LIST(new_hash(p, (yyvsp[-1].node), &(yylsp[-1])), &(yylsp[-1])) : 0;
                    /*% %*/
                    /*% ripper: args_add!(args_new!, bare_assoc_hash!($1)) %*/
                    }
#line 12580 "parse.c"
    break;

  case 296: /* call_args: command  */
#line 3679 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[0].node));
                        (yyval.node) = NEW_LIST((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: args_add!(args_new!, $1) %*/
                    }
#line 12592 "parse.c"
    break;

  case 297: /* call_args: args opt_block_arg  */
#line 3687 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = arg_blk_pass((yyvsp[-1].node), (yyvsp[0].node_block_pass));
                    /*% %*/
                    /*% ripper: args_add_block!($1, $2) %*/
                    }
#line 12603 "parse.c"
    break;

  case 298: /* call_args: assocs opt_block_arg  */
#line 3694 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[-1].node) ? NEW_LIST(new_hash(p, (yyvsp[-1].node), &(yylsp[-1])), &(yylsp[-1])) : 0;
                        (yyval.node) = arg_blk_pass((yyval.node), (yyvsp[0].node_block_pass));
                    /*% %*/
                    /*% ripper: args_add_block!(args_add!(args_new!, bare_assoc_hash!($1)), $2) %*/
                    }
#line 12615 "parse.c"
    break;

  case 299: /* call_args: args ',' assocs opt_block_arg  */
#line 3702 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[-1].node) ? arg_append(p, (yyvsp[-3].node), new_hash(p, (yyvsp[-1].node), &(yylsp[-1])), &(yyloc)) : (yyvsp[-3].node);
                        (yyval.node) = arg_blk_pass((yyval.node), (yyvsp[0].node_block_pass));
                    /*% %*/
                    /*% ripper: args_add_block!(args_add!($1, bare_assoc_hash!($3)), $4) %*/
                    }
#line 12627 "parse.c"
    break;

  case 301: /* $@13: %empty  */
#line 3713 "parse.y"
               {
                        /* If call_args starts with a open paren '(' or '[',
                         * look-ahead reading of the letters calls CMDARG_PUSH(0),
                         * but the push must be done after CMDARG_PUSH(1).
                         * So this code makes them consistent by first cancelling
                         * the premature CMDARG_PUSH(0), doing CMDARG_PUSH(1),
                         * and finally redoing CMDARG_PUSH(0).
                         */
                        int lookahead = 0;
                        switch (yychar) {
                          case '(': case tLPAREN: case tLPAREN_ARG: case '[': case tLBRACK:
                            lookahead = 1;
                        }
                        if (lookahead) CMDARG_POP();
                        CMDARG_PUSH(1);
                        if (lookahead) CMDARG_PUSH(0);
                    }
#line 12649 "parse.c"
    break;

  case 302: /* command_args: $@13 call_args  */
#line 3731 "parse.y"
                  {
                        /* call_args can be followed by tLBRACE_ARG (that does CMDARG_PUSH(0) in the lexer)
                         * but the push must be done after CMDARG_POP() in the parser.
                         * So this code does CMDARG_POP() to pop 0 pushed by tLBRACE_ARG,
                         * CMDARG_POP() to pop 1 pushed by command_args,
                         * and CMDARG_PUSH(0) to restore back the flag set by tLBRACE_ARG.
                         */
                        int lookahead = 0;
                        switch (yychar) {
                          case tLBRACE_ARG:
                            lookahead = 1;
                        }
                        if (lookahead) CMDARG_POP();
                        CMDARG_POP();
                        if (lookahead) CMDARG_PUSH(0);
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 12671 "parse.c"
    break;

  case 303: /* block_arg: "&" arg_value  */
#line 3751 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_block_pass) = NEW_BLOCK_PASS((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: $2 %*/
                    }
#line 12682 "parse.c"
    break;

  case 304: /* block_arg: "&"  */
#line 3758 "parse.y"
                  {
                        forwarding_arg_check(p, idFWD_BLOCK, 0, "block");
                    /*%%%*/
                        (yyval.node_block_pass) = NEW_BLOCK_PASS(NEW_LVAR(idFWD_BLOCK, &(yylsp[0])), &(yyloc));
                    /*% %*/
                    /*% ripper: Qnil %*/
                    }
#line 12694 "parse.c"
    break;

  case 305: /* opt_block_arg: ',' block_arg  */
#line 3768 "parse.y"
                  {
                        (yyval.node_block_pass) = (yyvsp[0].node_block_pass);
                    }
#line 12702 "parse.c"
    break;

  case 306: /* opt_block_arg: none  */
#line 3772 "parse.y"
                  {
                        (yyval.node_block_pass) = 0;
                    }
#line 12710 "parse.c"
    break;

  case 307: /* args: arg_value  */
#line 3779 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_LIST((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: args_add!(args_new!, $1) %*/
                    }
#line 12721 "parse.c"
    break;

  case 308: /* args: arg_splat  */
#line 3786 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_SPLAT((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: args_add_star!(args_new!, $arg_splat) %*/
                    }
#line 12732 "parse.c"
    break;

  case 309: /* args: args ',' arg_value  */
#line 3793 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = last_arg_append(p, (yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: args_add!($1, $3) %*/
                    }
#line 12743 "parse.c"
    break;

  case 310: /* args: args ',' arg_splat  */
#line 3800 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = rest_arg_append(p, (yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: args_add_star!($1, $3) %*/
                    }
#line 12754 "parse.c"
    break;

  case 311: /* arg_splat: "*" arg_value  */
#line 3810 "parse.y"
                  {
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 12762 "parse.c"
    break;

  case 312: /* arg_splat: "*"  */
#line 3814 "parse.y"
                  {
                        forwarding_arg_check(p, idFWD_REST, idFWD_ALL, "rest");
                    /*%%%*/
                        (yyval.node) = NEW_LVAR(idFWD_REST, &(yylsp[0]));
                    /*% %*/
                    /*% ripper: Qnil %*/
                    }
#line 12774 "parse.c"
    break;

  case 315: /* mrhs: args ',' arg_value  */
#line 3830 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = last_arg_append(p, (yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: mrhs_add!(mrhs_new_from_args!($1), $3) %*/
                    }
#line 12785 "parse.c"
    break;

  case 316: /* mrhs: args ',' "*" arg_value  */
#line 3837 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = rest_arg_append(p, (yyvsp[-3].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: mrhs_add_star!(mrhs_new_from_args!($1), $4) %*/
                    }
#line 12796 "parse.c"
    break;

  case 317: /* mrhs: "*" arg_value  */
#line 3844 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_SPLAT((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: mrhs_add_star!(mrhs_new!, $2) %*/
                    }
#line 12807 "parse.c"
    break;

  case 328: /* primary: "method"  */
#line 3863 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (NODE *)NEW_FCALL((yyvsp[0].id), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: method_add_arg!(fcall!($1), args_new!) %*/
                    }
#line 12818 "parse.c"
    break;

  case 329: /* $@14: %empty  */
#line 3870 "parse.y"
                  {
                        CMDARG_PUSH(0);
                    }
#line 12826 "parse.c"
    break;

  case 330: /* primary: k_begin $@14 bodystmt k_end  */
#line 3875 "parse.y"
                  {
                        CMDARG_POP();
                    /*%%%*/
                        set_line_body((yyvsp[-1].node), (yylsp[-3]).end_pos.lineno);
                        (yyval.node) = NEW_BEGIN((yyvsp[-1].node), &(yyloc));
                        nd_set_line((yyval.node), (yylsp[-3]).end_pos.lineno);
                    /*% %*/
                    /*% ripper: begin!($3) %*/
                    }
#line 12840 "parse.c"
    break;

  case 331: /* $@15: %empty  */
#line 3884 "parse.y"
                                     {SET_LEX_STATE(EXPR_ENDARG);}
#line 12846 "parse.c"
    break;

  case 332: /* primary: "( arg" compstmt $@15 ')'  */
#line 3885 "parse.y"
                  {
                    /*%%%*/
                        if (nd_type_p((yyvsp[-2].node), NODE_SELF)) RNODE_SELF((yyvsp[-2].node))->nd_state = 0;
                        (yyval.node) = (yyvsp[-2].node);
                    /*% %*/
                    /*% ripper: paren!($2) %*/
                    }
#line 12858 "parse.c"
    break;

  case 333: /* primary: "(" compstmt ')'  */
#line 3893 "parse.y"
                  {
                    /*%%%*/
                        if (nd_type_p((yyvsp[-1].node), NODE_SELF)) RNODE_SELF((yyvsp[-1].node))->nd_state = 0;
                        (yyval.node) = NEW_BLOCK((yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: paren!($2) %*/
                    }
#line 12870 "parse.c"
    break;

  case 334: /* primary: primary_value "::" "constant"  */
#line 3901 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_COLON2((yyvsp[-2].node), (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: const_path_ref!($1, $3) %*/
                    }
#line 12881 "parse.c"
    break;

  case 335: /* primary: ":: at EXPR_BEG" "constant"  */
#line 3908 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_COLON3((yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: top_const_ref!($2) %*/
                    }
#line 12892 "parse.c"
    break;

  case 336: /* primary: "[" aref_args ']'  */
#line 3915 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = make_list((yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: array!($2) %*/
                    }
#line 12903 "parse.c"
    break;

  case 337: /* primary: "{" assoc_list '}'  */
#line 3922 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_hash(p, (yyvsp[-1].node), &(yyloc));
                        RNODE_HASH((yyval.node))->nd_brace = TRUE;
                    /*% %*/
                    /*% ripper: hash!($2) %*/
                    }
#line 12915 "parse.c"
    break;

  case 338: /* primary: k_return  */
#line 3930 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_RETURN(0, &(yyloc));
                    /*% %*/
                    /*% ripper: return0! %*/
                    }
#line 12926 "parse.c"
    break;

  case 339: /* primary: k_yield '(' call_args rparen  */
#line 3937 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_yield(p, (yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: yield!(paren!($3)) %*/
                    }
#line 12937 "parse.c"
    break;

  case 340: /* primary: k_yield '(' rparen  */
#line 3944 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_YIELD(0, &(yyloc));
                    /*% %*/
                    /*% ripper: yield!(paren!(args_new!)) %*/
                    }
#line 12948 "parse.c"
    break;

  case 341: /* primary: k_yield  */
#line 3951 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_YIELD(0, &(yyloc));
                    /*% %*/
                    /*% ripper: yield0! %*/
                    }
#line 12959 "parse.c"
    break;

  case 342: /* primary: "`defined?'" opt_nl '(' begin_defined expr rparen  */
#line 3958 "parse.y"
                  {
                        p->ctxt.in_defined = (yyvsp[-2].ctxt).in_defined;
                        (yyval.node) = new_defined(p, (yyvsp[-1].node), &(yyloc));
                    }
#line 12968 "parse.c"
    break;

  case 343: /* primary: "`not'" '(' expr rparen  */
#line 3963 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, method_cond(p, (yyvsp[-1].node), &(yylsp[-1])), METHOD_NOT, &(yylsp[-3]), &(yyloc));
                    }
#line 12976 "parse.c"
    break;

  case 344: /* primary: "`not'" '(' rparen  */
#line 3967 "parse.y"
                  {
                        (yyval.node) = call_uni_op(p, method_cond(p, new_nil(&(yylsp[-1])), &(yylsp[-1])), METHOD_NOT, &(yylsp[-2]), &(yyloc));
                    }
#line 12984 "parse.c"
    break;

  case 345: /* primary: fcall brace_block  */
#line 3971 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = method_add_block(p, (NODE *)(yyvsp[-1].node_fcall), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: method_add_block!(method_add_arg!(fcall!($1), args_new!), $2) %*/
                    }
#line 12995 "parse.c"
    break;

  case 347: /* primary: method_call brace_block  */
#line 3979 "parse.y"
                  {
                    /*%%%*/
                        block_dup_check(p, get_nd_args(p, (yyvsp[-1].node)), (yyvsp[0].node));
                        (yyval.node) = method_add_block(p, (yyvsp[-1].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: method_add_block!($1, $2) %*/
                    }
#line 13007 "parse.c"
    break;

  case 349: /* primary: k_if expr_value then compstmt if_tail k_end  */
#line 3991 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_if(p, (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), &(yyloc));
                        fixpos((yyval.node), (yyvsp[-4].node));
                    /*% %*/
                    /*% ripper: if!($2, $4, $5) %*/
                    }
#line 13019 "parse.c"
    break;

  case 350: /* primary: k_unless expr_value then compstmt opt_else k_end  */
#line 4002 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_unless(p, (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), &(yyloc));
                        fixpos((yyval.node), (yyvsp[-4].node));
                    /*% %*/
                    /*% ripper: unless!($2, $4, $5) %*/
                    }
#line 13031 "parse.c"
    break;

  case 351: /* primary: k_while expr_value_do compstmt k_end  */
#line 4012 "parse.y"
                  {
                        restore_block_exit(p, (yyvsp[-3].node_exits));
                    /*%%%*/
                        (yyval.node) = NEW_WHILE(cond(p, (yyvsp[-2].node), &(yylsp[-2])), (yyvsp[-1].node), 1, &(yyloc));
                        fixpos((yyval.node), (yyvsp[-2].node));
                    /*% %*/
                    /*% ripper: while!($2, $3) %*/
                    }
#line 13044 "parse.c"
    break;

  case 352: /* primary: k_until expr_value_do compstmt k_end  */
#line 4023 "parse.y"
                  {
                        restore_block_exit(p, (yyvsp[-3].node_exits));
                    /*%%%*/
                        (yyval.node) = NEW_UNTIL(cond(p, (yyvsp[-2].node), &(yylsp[-2])), (yyvsp[-1].node), 1, &(yyloc));
                        fixpos((yyval.node), (yyvsp[-2].node));
                    /*% %*/
                    /*% ripper: until!($2, $3) %*/
                    }
#line 13057 "parse.c"
    break;

  case 353: /* @16: %empty  */
#line 4032 "parse.y"
                  {
                        (yyval.val) = p->case_labels;
                        p->case_labels = Qnil;
                    }
#line 13066 "parse.c"
    break;

  case 354: /* primary: k_case expr_value opt_terms @16 case_body k_end  */
#line 4038 "parse.y"
                  {
                        if (RTEST(p->case_labels)) rb_hash_clear(p->case_labels);
                        p->case_labels = (yyvsp[-2].val);
                    /*%%%*/
                        (yyval.node) = NEW_CASE((yyvsp[-4].node), (yyvsp[-1].node), &(yyloc));
                        fixpos((yyval.node), (yyvsp[-4].node));
                    /*% %*/
                    /*% ripper: case!($2, $5) %*/
                    }
#line 13080 "parse.c"
    break;

  case 355: /* @17: %empty  */
#line 4048 "parse.y"
                  {
                        (yyval.val) = p->case_labels;
                        p->case_labels = 0;
                    }
#line 13089 "parse.c"
    break;

  case 356: /* primary: k_case opt_terms @17 case_body k_end  */
#line 4054 "parse.y"
                  {
                        if (RTEST(p->case_labels)) rb_hash_clear(p->case_labels);
                        p->case_labels = (yyvsp[-2].val);
                    /*%%%*/
                        (yyval.node) = NEW_CASE2((yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: case!(Qnil, $4) %*/
                    }
#line 13102 "parse.c"
    break;

  case 357: /* primary: k_case expr_value opt_terms p_case_body k_end  */
#line 4065 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_CASE3((yyvsp[-3].node), (yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: case!($2, $4) %*/
                    }
#line 13113 "parse.c"
    break;

  case 358: /* primary: k_for for_var "`in'" expr_value_do compstmt k_end  */
#line 4074 "parse.y"
                  {
                        restore_block_exit(p, (yyvsp[-5].node_exits));
                    /*%%%*/
                        /*
                         *  for a, b, c in e
                         *  #=>
                         *  e.each{|*x| a, b, c = x}
                         *
                         *  for a in e
                         *  #=>
                         *  e.each{|x| a, = x}
                         */
                        ID id = internal_id(p);
                        rb_node_args_aux_t *m = NEW_ARGS_AUX(0, 0, &NULL_LOC);
                        rb_node_args_t *args;
                        NODE *scope, *internal_var = NEW_DVAR(id, &(yylsp[-4]));
                        rb_ast_id_table_t *tbl = rb_ast_new_local_table(p->ast, 1);
                        tbl->ids[0] = id; /* internal id */

                        switch (nd_type((yyvsp[-4].node))) {
                          case NODE_LASGN:
                          case NODE_DASGN: /* e.each {|internal_var| a = internal_var; ... } */
                            set_nd_value(p, (yyvsp[-4].node), internal_var);
                            id = 0;
                            m->nd_plen = 1;
                            m->nd_next = (yyvsp[-4].node);
                            break;
                          case NODE_MASGN: /* e.each {|*internal_var| a, b, c = (internal_var.length == 1 && Array === (tmp = internal_var[0]) ? tmp : internal_var); ... } */
                            m->nd_next = node_assign(p, (yyvsp[-4].node), NEW_FOR_MASGN(internal_var, &(yylsp[-4])), NO_LEX_CTXT, &(yylsp[-4]));
                            break;
                          default: /* e.each {|*internal_var| @a, B, c[1], d.attr = internal_val; ... } */
                            m->nd_next = node_assign(p, (NODE *)NEW_MASGN(NEW_LIST((yyvsp[-4].node), &(yylsp[-4])), 0, &(yylsp[-4])), internal_var, NO_LEX_CTXT, &(yylsp[-4]));
                        }
                        /* {|*internal_id| <m> = internal_id; ... } */
                        args = new_args(p, m, 0, id, 0, new_args_tail(p, 0, 0, 0, &(yylsp[-4])), &(yylsp[-4]));
                        scope = NEW_SCOPE2(tbl, args, (yyvsp[-1].node), &(yyloc));
                        (yyval.node) = NEW_FOR((yyvsp[-2].node), scope, &(yyloc));
                        fixpos((yyval.node), (yyvsp[-4].node));
                    /*% %*/
                    /*% ripper: for!($2, $4, $5) %*/
                    }
#line 13159 "parse.c"
    break;

  case 359: /* $@18: %empty  */
#line 4116 "parse.y"
                  {
                        begin_definition("class", &(yylsp[-2]), &(yylsp[-1]));
                    }
#line 13167 "parse.c"
    break;

  case 360: /* primary: k_class cpath superclass $@18 bodystmt k_end  */
#line 4121 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_CLASS((yyvsp[-4].node), (yyvsp[-1].node), (yyvsp[-3].node), &(yyloc));
                        nd_set_line(RNODE_CLASS((yyval.node))->nd_body, (yylsp[0]).end_pos.lineno);
                        set_line_body((yyvsp[-1].node), (yylsp[-3]).end_pos.lineno);
                        nd_set_line((yyval.node), (yylsp[-3]).end_pos.lineno);
                    /*% %*/
                    /*% ripper: class!($cpath, $superclass, $bodystmt) %*/
                        local_pop(p);
                        p->ctxt.in_class = (yyvsp[-5].ctxt).in_class;
                        p->ctxt.shareable_constant_value = (yyvsp[-5].ctxt).shareable_constant_value;
                    }
#line 13184 "parse.c"
    break;

  case 361: /* $@19: %empty  */
#line 4134 "parse.y"
                  {
                        begin_definition("", &(yylsp[-2]), &(yylsp[-1]));
                    }
#line 13192 "parse.c"
    break;

  case 362: /* primary: k_class "<<" expr_value $@19 term bodystmt k_end  */
#line 4140 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_SCLASS((yyvsp[-4].node), (yyvsp[-1].node), &(yyloc));
                        nd_set_line(RNODE_SCLASS((yyval.node))->nd_body, (yylsp[0]).end_pos.lineno);
                        set_line_body((yyvsp[-1].node), nd_line((yyvsp[-4].node)));
                        fixpos((yyval.node), (yyvsp[-4].node));
                    /*% %*/
                    /*% ripper: sclass!($expr_value, $bodystmt) %*/
                        local_pop(p);
                        p->ctxt.in_def = (yyvsp[-6].ctxt).in_def;
                        p->ctxt.in_class = (yyvsp[-6].ctxt).in_class;
                        p->ctxt.shareable_constant_value = (yyvsp[-6].ctxt).shareable_constant_value;
                    }
#line 13210 "parse.c"
    break;

  case 363: /* $@20: %empty  */
#line 4154 "parse.y"
                  {
                        begin_definition("module", &(yylsp[-1]), &(yylsp[0]));
                    }
#line 13218 "parse.c"
    break;

  case 364: /* primary: k_module cpath $@20 bodystmt k_end  */
#line 4159 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_MODULE((yyvsp[-3].node), (yyvsp[-1].node), &(yyloc));
                        nd_set_line(RNODE_MODULE((yyval.node))->nd_body, (yylsp[0]).end_pos.lineno);
                        set_line_body((yyvsp[-1].node), (yylsp[-3]).end_pos.lineno);
                        nd_set_line((yyval.node), (yylsp[-3]).end_pos.lineno);
                    /*% %*/
                    /*% ripper: module!($cpath, $bodystmt) %*/
                        local_pop(p);
                        p->ctxt.in_class = (yyvsp[-4].ctxt).in_class;
                        p->ctxt.shareable_constant_value = (yyvsp[-4].ctxt).shareable_constant_value;
                    }
#line 13235 "parse.c"
    break;

  case 365: /* $@21: %empty  */
#line 4173 "parse.y"
                  {
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[-1]).beg_pos);
                    /*% %*/
                    }
#line 13245 "parse.c"
    break;

  case 366: /* primary: defn_head f_arglist $@21 bodystmt k_end  */
#line 4180 "parse.y"
                  {
                        restore_defun(p, (yyvsp[-4].node_def_temp));
                    /*%%%*/
                        (yyvsp[-1].node) = new_scope_body(p, (yyvsp[-3].node_args), (yyvsp[-1].node), &(yyloc));
                        ((yyval.node) = (yyvsp[-4].node_def_temp)->nd_def)->nd_loc = (yyloc);
                        RNODE_DEFN((yyval.node))->nd_defn = (yyvsp[-1].node);
                    /*% %*/
                    /*% ripper: def!($head->nd_mid, $args, $bodystmt) %*/
                        local_pop(p);
                    }
#line 13260 "parse.c"
    break;

  case 367: /* $@22: %empty  */
#line 4192 "parse.y"
                  {
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[-1]).beg_pos);
                    /*% %*/
                    }
#line 13270 "parse.c"
    break;

  case 368: /* primary: defs_head f_arglist $@22 bodystmt k_end  */
#line 4199 "parse.y"
                  {
                        restore_defun(p, (yyvsp[-4].node_def_temp));
                    /*%%%*/
                        (yyvsp[-1].node) = new_scope_body(p, (yyvsp[-3].node_args), (yyvsp[-1].node), &(yyloc));
                        ((yyval.node) = (yyvsp[-4].node_def_temp)->nd_def)->nd_loc = (yyloc);
                        RNODE_DEFS((yyval.node))->nd_defn = (yyvsp[-1].node);
                    /*% %*/
                    /*% ripper: defs!($head->nd_recv, $head->dot_or_colon, $head->nd_mid, $args, $bodystmt) %*/
                        local_pop(p);
                    }
#line 13285 "parse.c"
    break;

  case 369: /* primary: "`break'"  */
#line 4210 "parse.y"
                  {
                        (yyval.node) = add_block_exit(p, NEW_BREAK(0, &(yyloc)));
                    /*% ripper: break!(args_new!) %*/
                    }
#line 13294 "parse.c"
    break;

  case 370: /* primary: "`next'"  */
#line 4215 "parse.y"
                  {
                        (yyval.node) = add_block_exit(p, NEW_NEXT(0, &(yyloc)));
                    /*% ripper: next!(args_new!) %*/
                    }
#line 13303 "parse.c"
    break;

  case 371: /* primary: "`redo'"  */
#line 4220 "parse.y"
                  {
                        (yyval.node) = add_block_exit(p, NEW_REDO(&(yyloc)));
                    /*% ripper: redo! %*/
                    }
#line 13312 "parse.c"
    break;

  case 372: /* primary: "`retry'"  */
#line 4225 "parse.y"
                  {
                        if (!p->ctxt.in_defined) {
                            switch (p->ctxt.in_rescue) {
                              case before_rescue: yyerror1(&(yylsp[0]), "Invalid retry without rescue"); break;
                              case after_rescue: /* ok */ break;
                              case after_else: yyerror1(&(yylsp[0]), "Invalid retry after else"); break;
                              case after_ensure: yyerror1(&(yylsp[0]), "Invalid retry after ensure"); break;
                            }
                        }
                    /*%%%*/
                        (yyval.node) = NEW_RETRY(&(yyloc));
                    /*% %*/
                    /*% ripper: retry! %*/
                    }
#line 13331 "parse.c"
    break;

  case 373: /* primary_value: primary  */
#line 4242 "parse.y"
                  {
                        value_expr((yyvsp[0].node));
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 13340 "parse.c"
    break;

  case 374: /* k_begin: "`begin'"  */
#line 4249 "parse.y"
                  {
                        token_info_push(p, "begin", &(yyloc));
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[0]).beg_pos);
                    /*% %*/
                    }
#line 13351 "parse.c"
    break;

  case 375: /* k_if: "`if'"  */
#line 4258 "parse.y"
                  {
                        WARN_EOL("if");
                        token_info_push(p, "if", &(yyloc));
                        if (p->token_info && p->token_info->nonspc &&
                            p->token_info->next && !strcmp(p->token_info->next->token, "else")) {
                            const char *tok = p->lex.ptok - rb_strlen_lit("if");
                            const char *beg = p->lex.pbeg + p->token_info->next->beg.column;
                            beg += rb_strlen_lit("else");
                            while (beg < tok && ISSPACE(*beg)) beg++;
                            if (beg == tok) {
                                p->token_info->nonspc = 0;
                            }
                        }
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[0]).beg_pos);
                    /*% %*/
                    }
#line 13373 "parse.c"
    break;

  case 376: /* k_unless: "`unless'"  */
#line 4278 "parse.y"
                  {
                        token_info_push(p, "unless", &(yyloc));
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[0]).beg_pos);
                    /*% %*/
                    }
#line 13384 "parse.c"
    break;

  case 377: /* k_while: "`while'" allow_exits  */
#line 4287 "parse.y"
                  {
                        (yyval.node_exits) = (yyvsp[0].node_exits);
                        token_info_push(p, "while", &(yyloc));
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[-1]).beg_pos);
                    /*% %*/
                    }
#line 13396 "parse.c"
    break;

  case 378: /* k_until: "`until'" allow_exits  */
#line 4297 "parse.y"
                  {
                        (yyval.node_exits) = (yyvsp[0].node_exits);
                        token_info_push(p, "until", &(yyloc));
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[-1]).beg_pos);
                    /*% %*/
                    }
#line 13408 "parse.c"
    break;

  case 379: /* k_case: "`case'"  */
#line 4307 "parse.y"
                  {
                        token_info_push(p, "case", &(yyloc));
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[0]).beg_pos);
                    /*% %*/
                    }
#line 13419 "parse.c"
    break;

  case 380: /* k_for: "`for'" allow_exits  */
#line 4316 "parse.y"
                  {
                        (yyval.node_exits) = (yyvsp[0].node_exits);
                        token_info_push(p, "for", &(yyloc));
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[-1]).beg_pos);
                    /*% %*/
                    }
#line 13431 "parse.c"
    break;

  case 381: /* k_class: "`class'"  */
#line 4326 "parse.y"
                  {
                        token_info_push(p, "class", &(yyloc));
                        (yyval.ctxt) = p->ctxt;
                        p->ctxt.in_rescue = before_rescue;
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[0]).beg_pos);
                    /*% %*/
                    }
#line 13444 "parse.c"
    break;

  case 382: /* k_module: "`module'"  */
#line 4337 "parse.y"
                  {
                        token_info_push(p, "module", &(yyloc));
                        (yyval.ctxt) = p->ctxt;
                        p->ctxt.in_rescue = before_rescue;
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[0]).beg_pos);
                    /*% %*/
                    }
#line 13457 "parse.c"
    break;

  case 383: /* k_def: "`def'"  */
#line 4348 "parse.y"
                  {
                        token_info_push(p, "def", &(yyloc));
                        (yyval.node_def_temp) = NEW_DEF_TEMP(&(yyloc));
                        p->ctxt.in_argdef = 1;
                    }
#line 13467 "parse.c"
    break;

  case 384: /* k_do: "`do'"  */
#line 4356 "parse.y"
                  {
                        token_info_push(p, "do", &(yyloc));
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[0]).beg_pos);
                    /*% %*/
                    }
#line 13478 "parse.c"
    break;

  case 385: /* k_do_block: "`do' for block"  */
#line 4365 "parse.y"
                  {
                        token_info_push(p, "do", &(yyloc));
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[0]).beg_pos);
                    /*% %*/
                    }
#line 13489 "parse.c"
    break;

  case 386: /* k_rescue: "`rescue'"  */
#line 4374 "parse.y"
                  {
                        token_info_warn(p, "rescue", p->token_info, 1, &(yyloc));
                        (yyval.ctxt) = p->ctxt;
                        p->ctxt.in_rescue = after_rescue;
                    }
#line 13499 "parse.c"
    break;

  case 387: /* k_ensure: "`ensure'"  */
#line 4382 "parse.y"
                  {
                        token_info_warn(p, "ensure", p->token_info, 1, &(yyloc));
                        (yyval.ctxt) = p->ctxt;
                    }
#line 13508 "parse.c"
    break;

  case 388: /* k_when: "`when'"  */
#line 4389 "parse.y"
                  {
                        token_info_warn(p, "when", p->token_info, 0, &(yyloc));
                    }
#line 13516 "parse.c"
    break;

  case 389: /* k_else: "`else'"  */
#line 4395 "parse.y"
                  {
                        token_info *ptinfo_beg = p->token_info;
                        int same = ptinfo_beg && strcmp(ptinfo_beg->token, "case") != 0;
                        token_info_warn(p, "else", p->token_info, same, &(yyloc));
                        if (same) {
                            token_info e;
                            e.next = ptinfo_beg->next;
                            e.token = "else";
                            token_info_setup(&e, p->lex.pbeg, &(yyloc));
                            if (!e.nonspc) *ptinfo_beg = e;
                        }
                    }
#line 13533 "parse.c"
    break;

  case 390: /* k_elsif: "`elsif'"  */
#line 4410 "parse.y"
                  {
                        WARN_EOL("elsif");
                        token_info_warn(p, "elsif", p->token_info, 1, &(yyloc));
                    }
#line 13542 "parse.c"
    break;

  case 391: /* k_end: "`end'"  */
#line 4417 "parse.y"
                  {
                        token_info_pop(p, "end", &(yyloc));
                    /*%%%*/
                        pop_end_expect_token_locations(p);
                    /*% %*/
                    }
#line 13553 "parse.c"
    break;

  case 392: /* k_end: "dummy end"  */
#line 4424 "parse.y"
                  {
                        compile_error(p, "syntax error, unexpected end-of-input");
                    }
#line 13561 "parse.c"
    break;

  case 393: /* k_return: "`return'"  */
#line 4430 "parse.y"
                  {
                        if (p->ctxt.in_class && !p->ctxt.in_def && !dyna_in_block(p))
                            yyerror1(&(yylsp[0]), "Invalid return in class/module body");
                    }
#line 13570 "parse.c"
    break;

  case 394: /* k_yield: "`yield'"  */
#line 4437 "parse.y"
                  {
                        if (!p->ctxt.in_defined && !p->ctxt.in_def && !compile_for_eval)
                            yyerror1(&(yylsp[0]), "Invalid yield");
                    }
#line 13579 "parse.c"
    break;

  case 401: /* if_tail: k_elsif expr_value then compstmt if_tail  */
#line 4456 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_if(p, (yyvsp[-3].node), (yyvsp[-1].node), (yyvsp[0].node), &(yyloc));
                        fixpos((yyval.node), (yyvsp[-3].node));
                    /*% %*/
                    /*% ripper: elsif!($2, $4, $5) %*/
                    }
#line 13591 "parse.c"
    break;

  case 403: /* opt_else: k_else compstmt  */
#line 4467 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[0].node);
                    /*% %*/
                    /*% ripper: else!($2) %*/
                    }
#line 13602 "parse.c"
    break;

  case 406: /* f_marg: f_norm_arg  */
#line 4480 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = assignable(p, (yyvsp[0].id), 0, &(yyloc));
                        mark_lvar_used(p, (yyval.node));
                    /*% %*/
                    /*% ripper: assignable(p, $1) %*/
                    }
#line 13614 "parse.c"
    break;

  case 407: /* f_marg: "(" f_margs rparen  */
#line 4488 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (NODE *)(yyvsp[-1].node_masgn);
                    /*% %*/
                    /*% ripper: mlhs_paren!($2) %*/
                    }
#line 13625 "parse.c"
    break;

  case 408: /* f_marg_list: f_marg  */
#line 4497 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_LIST((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add!(mlhs_new!, $1) %*/
                    }
#line 13636 "parse.c"
    break;

  case 409: /* f_marg_list: f_marg_list ',' f_marg  */
#line 4504 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = list_append(p, (yyvsp[-2].node), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: mlhs_add!($1, $3) %*/
                    }
#line 13647 "parse.c"
    break;

  case 410: /* f_margs: f_marg_list  */
#line 4513 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN((yyvsp[0].node), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: $1 %*/
                    }
#line 13658 "parse.c"
    break;

  case 411: /* f_margs: f_marg_list ',' f_rest_marg  */
#line 4520 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN((yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_star!($1, $3) %*/
                    }
#line 13669 "parse.c"
    break;

  case 412: /* f_margs: f_marg_list ',' f_rest_marg ',' f_marg_list  */
#line 4527 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN((yyvsp[-4].node), NEW_POSTARG((yyvsp[-2].node), (yyvsp[0].node), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_post!(mlhs_add_star!($1, $3), $5) %*/
                    }
#line 13680 "parse.c"
    break;

  case 413: /* f_margs: f_rest_marg  */
#line 4534 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN(0, (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_star!(mlhs_new!, $1) %*/
                    }
#line 13691 "parse.c"
    break;

  case 414: /* f_margs: f_rest_marg ',' f_marg_list  */
#line 4541 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_masgn) = NEW_MASGN(0, NEW_POSTARG((yyvsp[-2].node), (yyvsp[0].node), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: mlhs_add_post!(mlhs_add_star!(mlhs_new!, $1), $3) %*/
                    }
#line 13702 "parse.c"
    break;

  case 415: /* f_rest_marg: "*" f_norm_arg  */
#line 4550 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = assignable(p, (yyvsp[0].id), 0, &(yyloc));
                        mark_lvar_used(p, (yyval.node));
                    /*% %*/
                    /*% ripper: assignable(p, $2) %*/
                    }
#line 13714 "parse.c"
    break;

  case 416: /* f_rest_marg: "*"  */
#line 4558 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NODE_SPECIAL_NO_NAME_REST;
                    /*% %*/
                    /*% ripper: Qnil %*/
                    }
#line 13725 "parse.c"
    break;

  case 418: /* f_any_kwrest: f_no_kwarg  */
#line 4567 "parse.y"
                           {(yyval.id) = ID2VAL(idNil);}
#line 13731 "parse.c"
    break;

  case 419: /* $@23: %empty  */
#line 4570 "parse.y"
      {p->ctxt.in_argdef = 0;}
#line 13737 "parse.c"
    break;

  case 421: /* block_args_tail: f_block_kwarg ',' f_kwrest opt_f_block_arg  */
#line 4573 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, (yyvsp[-3].node_kw_arg), (yyvsp[-1].id), (yyvsp[0].id), &(yylsp[-1]));
                    }
#line 13745 "parse.c"
    break;

  case 422: /* block_args_tail: f_block_kwarg opt_f_block_arg  */
#line 4577 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, (yyvsp[-1].node_kw_arg), Qnone, (yyvsp[0].id), &(yylsp[-1]));
                    }
#line 13753 "parse.c"
    break;

  case 423: /* block_args_tail: f_any_kwrest opt_f_block_arg  */
#line 4581 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, Qnone, (yyvsp[-1].id), (yyvsp[0].id), &(yylsp[-1]));
                    }
#line 13761 "parse.c"
    break;

  case 424: /* block_args_tail: f_block_arg  */
#line 4585 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, Qnone, Qnone, (yyvsp[0].id), &(yylsp[0]));
                    }
#line 13769 "parse.c"
    break;

  case 425: /* opt_block_args_tail: ',' block_args_tail  */
#line 4591 "parse.y"
                  {
                        (yyval.node_args) = (yyvsp[0].node_args);
                    }
#line 13777 "parse.c"
    break;

  case 426: /* opt_block_args_tail: %empty  */
#line 4595 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, Qnone, Qnone, Qnone, &(yylsp[0]));
                    }
#line 13785 "parse.c"
    break;

  case 427: /* excessed_comma: ','  */
#line 4601 "parse.y"
                  {
                        /* magic number for rest_id in iseq_set_arguments() */
                    /*%%%*/
                        (yyval.id) = NODE_SPECIAL_EXCESSIVE_COMMA;
                    /*% %*/
                    /*% ripper: excessed_comma! %*/
                    }
#line 13797 "parse.c"
    break;

  case 428: /* block_param: f_arg ',' f_block_optarg ',' f_rest_arg opt_block_args_tail  */
#line 4611 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-5].node_args_aux), (yyvsp[-3].node_opt_arg), (yyvsp[-1].id), Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 13805 "parse.c"
    break;

  case 429: /* block_param: f_arg ',' f_block_optarg ',' f_rest_arg ',' f_arg opt_block_args_tail  */
#line 4615 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-7].node_args_aux), (yyvsp[-5].node_opt_arg), (yyvsp[-3].id), (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 13813 "parse.c"
    break;

  case 430: /* block_param: f_arg ',' f_block_optarg opt_block_args_tail  */
#line 4619 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-3].node_args_aux), (yyvsp[-1].node_opt_arg), Qnone, Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 13821 "parse.c"
    break;

  case 431: /* block_param: f_arg ',' f_block_optarg ',' f_arg opt_block_args_tail  */
#line 4623 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-5].node_args_aux), (yyvsp[-3].node_opt_arg), Qnone, (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 13829 "parse.c"
    break;

  case 432: /* block_param: f_arg ',' f_rest_arg opt_block_args_tail  */
#line 4627 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-3].node_args_aux), Qnone, (yyvsp[-1].id), Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 13837 "parse.c"
    break;

  case 433: /* block_param: f_arg excessed_comma  */
#line 4631 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, Qnone, Qnone, Qnone, &(yylsp[0]));
                        (yyval.node_args) = new_args(p, (yyvsp[-1].node_args_aux), Qnone, (yyvsp[0].id), Qnone, (yyval.node_args), &(yyloc));
                    }
#line 13846 "parse.c"
    break;

  case 434: /* block_param: f_arg ',' f_rest_arg ',' f_arg opt_block_args_tail  */
#line 4636 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-5].node_args_aux), Qnone, (yyvsp[-3].id), (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 13854 "parse.c"
    break;

  case 435: /* block_param: f_arg opt_block_args_tail  */
#line 4640 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-1].node_args_aux), Qnone, Qnone, Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 13862 "parse.c"
    break;

  case 436: /* block_param: f_block_optarg ',' f_rest_arg opt_block_args_tail  */
#line 4644 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, (yyvsp[-3].node_opt_arg), (yyvsp[-1].id), Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 13870 "parse.c"
    break;

  case 437: /* block_param: f_block_optarg ',' f_rest_arg ',' f_arg opt_block_args_tail  */
#line 4648 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, (yyvsp[-5].node_opt_arg), (yyvsp[-3].id), (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 13878 "parse.c"
    break;

  case 438: /* block_param: f_block_optarg opt_block_args_tail  */
#line 4652 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, (yyvsp[-1].node_opt_arg), Qnone, Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 13886 "parse.c"
    break;

  case 439: /* block_param: f_block_optarg ',' f_arg opt_block_args_tail  */
#line 4656 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, (yyvsp[-3].node_opt_arg), Qnone, (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 13894 "parse.c"
    break;

  case 440: /* block_param: f_rest_arg opt_block_args_tail  */
#line 4660 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, Qnone, (yyvsp[-1].id), Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 13902 "parse.c"
    break;

  case 441: /* block_param: f_rest_arg ',' f_arg opt_block_args_tail  */
#line 4664 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, Qnone, (yyvsp[-3].id), (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 13910 "parse.c"
    break;

  case 442: /* block_param: block_args_tail  */
#line 4668 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, Qnone, Qnone, Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 13918 "parse.c"
    break;

  case 444: /* opt_block_param: block_param_def  */
#line 4675 "parse.y"
                  {
                        p->command_start = TRUE;
                    }
#line 13926 "parse.c"
    break;

  case 445: /* block_param_def: '|' opt_bv_decl '|'  */
#line 4681 "parse.y"
                  {
                        p->cur_arg = 0;
                        p->max_numparam = ORDINAL_PARAM;
                        p->ctxt.in_argdef = 0;
                    /*%%%*/
                        (yyval.node_args) = 0;
                    /*% %*/
                    /*% ripper: params!(Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil) %*/
                    /*% ripper: block_var!($$, $2) %*/
                    }
#line 13941 "parse.c"
    break;

  case 446: /* block_param_def: '|' block_param opt_bv_decl '|'  */
#line 4692 "parse.y"
                  {
                        p->cur_arg = 0;
                        p->max_numparam = ORDINAL_PARAM;
                        p->ctxt.in_argdef = 0;
                    /*%%%*/
                        (yyval.node_args) = (yyvsp[-2].node_args);
                    /*% %*/
                    /*% ripper: block_var!($2, $3) %*/
                    }
#line 13955 "parse.c"
    break;

  case 447: /* opt_bv_decl: opt_nl  */
#line 4705 "parse.y"
                  {
                        (yyval.node) = 0;
                    }
#line 13963 "parse.c"
    break;

  case 448: /* opt_bv_decl: opt_nl ';' bv_decls opt_nl  */
#line 4709 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = 0;
                    /*% %*/
                    /*% ripper: $3 %*/
                    }
#line 13974 "parse.c"
    break;

  case 451: /* bvar: "local variable or method"  */
#line 4724 "parse.y"
                  {
                        new_bv(p, get_id((yyvsp[0].id)));
                    /*% ripper: get_value($1) %*/
                    }
#line 13983 "parse.c"
    break;

  case 452: /* bvar: f_bad_arg  */
#line 4729 "parse.y"
                  {
                        (yyval.node) = 0;
                    }
#line 13991 "parse.c"
    break;

  case 453: /* max_numparam: %empty  */
#line 4734 "parse.y"
               {
                        (yyval.num) = p->max_numparam;
                        p->max_numparam = 0;
                    }
#line 14000 "parse.c"
    break;

  case 454: /* numparam: %empty  */
#line 4740 "parse.y"
           {
                        (yyval.node) = numparam_push(p);
                    }
#line 14008 "parse.c"
    break;

  case 455: /* @24: %empty  */
#line 4746 "parse.y"
                  {
                        token_info_push(p, "->", &(yylsp[0]));
                        (yyvsp[0].vars) = dyna_push(p);
                        (yyval.num) = p->lex.lpar_beg;
                        p->lex.lpar_beg = p->lex.paren_nest;
                    }
#line 14019 "parse.c"
    break;

  case 456: /* $@25: %empty  */
#line 4754 "parse.y"
                  {
                        CMDARG_PUSH(0);
                    }
#line 14027 "parse.c"
    break;

  case 457: /* lambda: "->" @24 max_numparam numparam allow_exits f_larglist $@25 lambda_body  */
#line 4758 "parse.y"
                  {
                        int max_numparam = p->max_numparam;
                        p->lex.lpar_beg = (yyvsp[-6].num);
                        p->max_numparam = (yyvsp[-5].num);
                        restore_block_exit(p, (yyvsp[-3].node_exits));
                        CMDARG_POP();
                        (yyvsp[-2].node_args) = args_with_numbered(p, (yyvsp[-2].node_args), max_numparam);
                    /*%%%*/
                        {
                            YYLTYPE loc = code_loc_gen(&(yylsp[-2]), &(yylsp[0]));
                            (yyval.node) = NEW_LAMBDA((yyvsp[-2].node_args), (yyvsp[0].node), &loc);
                            nd_set_line(RNODE_LAMBDA((yyval.node))->nd_body, (yylsp[0]).end_pos.lineno);
                            nd_set_line((yyval.node), (yylsp[-2]).end_pos.lineno);
                            nd_set_first_loc((yyval.node), (yylsp[-7]).beg_pos);
                        }
                    /*% %*/
                    /*% ripper: lambda!($args, $body) %*/
                        numparam_pop(p, (yyvsp[-4].node));
                        dyna_pop(p, (yyvsp[-7].vars));
                    }
#line 14052 "parse.c"
    break;

  case 458: /* f_larglist: '(' f_args opt_bv_decl ')'  */
#line 4781 "parse.y"
                  {
                        p->ctxt.in_argdef = 0;
                    /*%%%*/
                        (yyval.node_args) = (yyvsp[-2].node_args);
                        p->max_numparam = ORDINAL_PARAM;
                    /*% %*/
                    /*% ripper: paren!($2) %*/
                    }
#line 14065 "parse.c"
    break;

  case 459: /* f_larglist: f_args  */
#line 4790 "parse.y"
                  {
                        p->ctxt.in_argdef = 0;
                    /*%%%*/
                        if (!args_info_empty_p(&(yyvsp[0].node_args)->nd_ainfo))
                            p->max_numparam = ORDINAL_PARAM;
                    /*% %*/
                        (yyval.node_args) = (yyvsp[0].node_args);
                    }
#line 14078 "parse.c"
    break;

  case 460: /* lambda_body: tLAMBEG compstmt '}'  */
#line 4801 "parse.y"
                  {
                        token_info_pop(p, "}", &(yylsp[0]));
                        (yyval.node) = (yyvsp[-1].node);
                    }
#line 14087 "parse.c"
    break;

  case 461: /* $@26: %empty  */
#line 4806 "parse.y"
                  {
                    /*%%%*/
                        push_end_expect_token_locations(p, &(yylsp[0]).beg_pos);
                    /*% %*/
                    }
#line 14097 "parse.c"
    break;

  case 462: /* lambda_body: "`do' for lambda" $@26 bodystmt k_end  */
#line 4812 "parse.y"
                  {
                        (yyval.node) = (yyvsp[-1].node);
                    }
#line 14105 "parse.c"
    break;

  case 463: /* do_block: k_do_block do_body k_end  */
#line 4818 "parse.y"
                  {
                        (yyval.node) = (yyvsp[-1].node);
                    /*%%%*/
                        set_embraced_location((yyval.node), &(yylsp[-2]), &(yylsp[0]));
                    /*% %*/
                    }
#line 14116 "parse.c"
    break;

  case 464: /* block_call: command do_block  */
#line 4827 "parse.y"
                  {
                    /*%%%*/
                        if (nd_type_p((yyvsp[-1].node), NODE_YIELD)) {
                            compile_error(p, "block given to yield");
                        }
                        else {
                            block_dup_check(p, get_nd_args(p, (yyvsp[-1].node)), (yyvsp[0].node));
                        }
                        (yyval.node) = method_add_block(p, (yyvsp[-1].node), (yyvsp[0].node), &(yyloc));
                        fixpos((yyval.node), (yyvsp[-1].node));
                    /*% %*/
                    /*% ripper: method_add_block!($1, $2) %*/
                    }
#line 14134 "parse.c"
    break;

  case 465: /* block_call: block_call call_op2 operation2 opt_paren_args  */
#line 4841 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_qcall(p, (yyvsp[-2].id), (yyvsp[-3].node), (yyvsp[-1].id), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                    /*% %*/
                    /*% ripper: opt_event(:method_add_arg!, call!($1, $2, $3), $4) %*/
                    }
#line 14145 "parse.c"
    break;

  case 466: /* block_call: block_call call_op2 operation2 opt_paren_args brace_block  */
#line 4848 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_command_qcall(p, (yyvsp[-3].id), (yyvsp[-4].node), (yyvsp[-2].id), (yyvsp[-1].node), (yyvsp[0].node), &(yylsp[-2]), &(yyloc));
                    /*% %*/
                    /*% ripper: opt_event(:method_add_block!, command_call!($1, $2, $3, $4), $5) %*/
                    }
#line 14156 "parse.c"
    break;

  case 467: /* block_call: block_call call_op2 operation2 command_args do_block  */
#line 4855 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_command_qcall(p, (yyvsp[-3].id), (yyvsp[-4].node), (yyvsp[-2].id), (yyvsp[-1].node), (yyvsp[0].node), &(yylsp[-2]), &(yyloc));
                    /*% %*/
                    /*% ripper: method_add_block!(command_call!($1, $2, $3, $4), $5) %*/
                    }
#line 14167 "parse.c"
    break;

  case 468: /* method_call: fcall paren_args  */
#line 4864 "parse.y"
                  {
                    /*%%%*/
                        (yyvsp[-1].node_fcall)->nd_args = (yyvsp[0].node);
                        (yyval.node) = (NODE *)(yyvsp[-1].node_fcall);
                        nd_set_last_loc((yyvsp[-1].node_fcall), (yylsp[0]).end_pos);
                    /*% %*/
                    /*% ripper: method_add_arg!(fcall!($1), $2) %*/
                    }
#line 14180 "parse.c"
    break;

  case 469: /* method_call: primary_value call_op operation2 opt_paren_args  */
#line 4873 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_qcall(p, (yyvsp[-2].id), (yyvsp[-3].node), (yyvsp[-1].id), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                        nd_set_line((yyval.node), (yylsp[-1]).end_pos.lineno);
                    /*% %*/
                    /*% ripper: opt_event(:method_add_arg!, call!($1, $2, $3), $4) %*/
                    }
#line 14192 "parse.c"
    break;

  case 470: /* method_call: primary_value "::" operation2 paren_args  */
#line 4881 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_qcall(p, ID2VAL(idCOLON2), (yyvsp[-3].node), (yyvsp[-1].id), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                        nd_set_line((yyval.node), (yylsp[-1]).end_pos.lineno);
                    /*% %*/
                    /*% ripper: method_add_arg!(call!($1, $2, $3), $4) %*/
                    }
#line 14204 "parse.c"
    break;

  case 471: /* method_call: primary_value "::" operation3  */
#line 4889 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_qcall(p, ID2VAL(idCOLON2), (yyvsp[-2].node), (yyvsp[0].id), Qnull, &(yylsp[0]), &(yyloc));
                    /*% %*/
                    /*% ripper: call!($1, $2, $3) %*/
                    }
#line 14215 "parse.c"
    break;

  case 472: /* method_call: primary_value call_op paren_args  */
#line 4896 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_qcall(p, (yyvsp[-1].id), (yyvsp[-2].node), ID2VAL(idCall), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                        nd_set_line((yyval.node), (yylsp[-1]).end_pos.lineno);
                    /*% %*/
                    /*% ripper: method_add_arg!(call!($1, $2, ID2VAL(idCall)), $3) %*/
                    }
#line 14227 "parse.c"
    break;

  case 473: /* method_call: primary_value "::" paren_args  */
#line 4904 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_qcall(p, ID2VAL(idCOLON2), (yyvsp[-2].node), ID2VAL(idCall), (yyvsp[0].node), &(yylsp[-1]), &(yyloc));
                        nd_set_line((yyval.node), (yylsp[-1]).end_pos.lineno);
                    /*% %*/
                    /*% ripper: method_add_arg!(call!($1, $2, ID2VAL(idCall)), $3) %*/
                    }
#line 14239 "parse.c"
    break;

  case 474: /* method_call: "`super'" paren_args  */
#line 4912 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_SUPER((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: super!($2) %*/
                    }
#line 14250 "parse.c"
    break;

  case 475: /* method_call: "`super'"  */
#line 4919 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_ZSUPER(&(yyloc));
                    /*% %*/
                    /*% ripper: zsuper! %*/
                    }
#line 14261 "parse.c"
    break;

  case 476: /* method_call: primary_value '[' opt_call_args rbracket  */
#line 4926 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_CALL((yyvsp[-3].node), tAREF, (yyvsp[-1].node), &(yyloc));
                        fixpos((yyval.node), (yyvsp[-3].node));
                    /*% %*/
                    /*% ripper: aref!($1, $3) %*/
                    }
#line 14273 "parse.c"
    break;

  case 477: /* brace_block: '{' brace_body '}'  */
#line 4936 "parse.y"
                  {
                        (yyval.node) = (yyvsp[-1].node);
                    /*%%%*/
                        set_embraced_location((yyval.node), &(yylsp[-2]), &(yylsp[0]));
                    /*% %*/
                    }
#line 14284 "parse.c"
    break;

  case 478: /* brace_block: k_do do_body k_end  */
#line 4943 "parse.y"
                  {
                        (yyval.node) = (yyvsp[-1].node);
                    /*%%%*/
                        set_embraced_location((yyval.node), &(yylsp[-2]), &(yylsp[0]));
                    /*% %*/
                    }
#line 14295 "parse.c"
    break;

  case 479: /* @27: %empty  */
#line 4951 "parse.y"
           {(yyval.vars) = dyna_push(p);}
#line 14301 "parse.c"
    break;

  case 480: /* brace_body: @27 max_numparam numparam allow_exits opt_block_param compstmt  */
#line 4954 "parse.y"
                  {
                        int max_numparam = p->max_numparam;
                        p->max_numparam = (yyvsp[-4].num);
                        (yyvsp[-1].node_args) = args_with_numbered(p, (yyvsp[-1].node_args), max_numparam);
                    /*%%%*/
                        (yyval.node) = NEW_ITER((yyvsp[-1].node_args), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: brace_block!($args, $compstmt) %*/
                        restore_block_exit(p, (yyvsp[-2].node_exits));
                        numparam_pop(p, (yyvsp[-3].node));
                        dyna_pop(p, (yyvsp[-5].vars));
                    }
#line 14318 "parse.c"
    break;

  case 481: /* @28: %empty  */
#line 4968 "parse.y"
           {
                        (yyval.vars) = dyna_push(p);
                        CMDARG_PUSH(0);
                    }
#line 14327 "parse.c"
    break;

  case 482: /* do_body: @28 max_numparam numparam allow_exits opt_block_param bodystmt  */
#line 4974 "parse.y"
                  {
                        int max_numparam = p->max_numparam;
                        p->max_numparam = (yyvsp[-4].num);
                        (yyvsp[-1].node_args) = args_with_numbered(p, (yyvsp[-1].node_args), max_numparam);
                    /*%%%*/
                        (yyval.node) = NEW_ITER((yyvsp[-1].node_args), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: do_block!($args, $bodystmt) %*/
                        CMDARG_POP();
                        restore_block_exit(p, (yyvsp[-2].node_exits));
                        numparam_pop(p, (yyvsp[-3].node));
                        dyna_pop(p, (yyvsp[-5].vars));
                    }
#line 14345 "parse.c"
    break;

  case 483: /* case_args: arg_value  */
#line 4990 "parse.y"
                  {
                    /*%%%*/
                        check_literal_when(p, (yyvsp[0].node), &(yylsp[0]));
                        (yyval.node) = NEW_LIST((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: args_add!(args_new!, $1) %*/
                    }
#line 14357 "parse.c"
    break;

  case 484: /* case_args: "*" arg_value  */
#line 4998 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_SPLAT((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: args_add_star!(args_new!, $2) %*/
                    }
#line 14368 "parse.c"
    break;

  case 485: /* case_args: case_args ',' arg_value  */
#line 5005 "parse.y"
                  {
                    /*%%%*/
                        check_literal_when(p, (yyvsp[0].node), &(yylsp[0]));
                        (yyval.node) = last_arg_append(p, (yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: args_add!($1, $3) %*/
                    }
#line 14380 "parse.c"
    break;

  case 486: /* case_args: case_args ',' "*" arg_value  */
#line 5013 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = rest_arg_append(p, (yyvsp[-3].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: args_add_star!($1, $4) %*/
                    }
#line 14391 "parse.c"
    break;

  case 487: /* case_body: k_when case_args then compstmt cases  */
#line 5024 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_WHEN((yyvsp[-3].node), (yyvsp[-1].node), (yyvsp[0].node), &(yyloc));
                        fixpos((yyval.node), (yyvsp[-3].node));
                    /*% %*/
                    /*% ripper: when!($2, $4, $5) %*/
                    }
#line 14403 "parse.c"
    break;

  case 490: /* p_pvtbl: %empty  */
#line 5037 "parse.y"
         {(yyval.tbl) = p->pvtbl; p->pvtbl = st_init_numtable();}
#line 14409 "parse.c"
    break;

  case 491: /* p_pktbl: %empty  */
#line 5038 "parse.y"
         {(yyval.tbl) = p->pktbl; p->pktbl = 0;}
#line 14415 "parse.c"
    break;

  case 492: /* p_in_kwarg: %empty  */
#line 5040 "parse.y"
             {
                        (yyval.ctxt) = p->ctxt;
                        SET_LEX_STATE(EXPR_BEG|EXPR_LABEL);
                        p->command_start = FALSE;
                        p->ctxt.in_kwarg = 1;
                    }
#line 14426 "parse.c"
    break;

  case 493: /* $@29: %empty  */
#line 5051 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-2].tbl));
                        pop_pvtbl(p, (yyvsp[-3].tbl));
                        p->ctxt.in_kwarg = (yyvsp[-4].ctxt).in_kwarg;
                    }
#line 14436 "parse.c"
    break;

  case 494: /* p_case_body: "`in'" p_in_kwarg p_pvtbl p_pktbl p_top_expr then $@29 compstmt p_cases  */
#line 5058 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_IN((yyvsp[-4].node), (yyvsp[-1].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: in!($expr, $compstmt, $cases) %*/
                    }
#line 14447 "parse.c"
    break;

  case 498: /* p_top_expr: p_top_expr_body "`if' modifier" expr_value  */
#line 5072 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_if(p, (yyvsp[0].node), (yyvsp[-2].node), 0, &(yyloc));
                        fixpos((yyval.node), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: if_mod!($3, $1) %*/
                    }
#line 14459 "parse.c"
    break;

  case 499: /* p_top_expr: p_top_expr_body "`unless' modifier" expr_value  */
#line 5080 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_unless(p, (yyvsp[0].node), (yyvsp[-2].node), 0, &(yyloc));
                        fixpos((yyval.node), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: unless_mod!($3, $1) %*/
                    }
#line 14471 "parse.c"
    break;

  case 501: /* p_top_expr_body: p_expr ','  */
#line 5091 "parse.y"
                  {
                        (yyval.node) = new_array_pattern_tail(p, Qnone, 1, Qnone, Qnone, &(yyloc));
                        (yyval.node) = new_array_pattern(p, Qnone, get_value((yyvsp[-1].node)), (yyval.node), &(yyloc));
                    }
#line 14480 "parse.c"
    break;

  case 502: /* p_top_expr_body: p_expr ',' p_args  */
#line 5096 "parse.y"
                  {
                        (yyval.node) = new_array_pattern(p, Qnone, get_value((yyvsp[-2].node)), (yyvsp[0].node), &(yyloc));
                    /*%%%*/
                        nd_set_first_loc((yyval.node), (yylsp[-2]).beg_pos);
                    /*%
                    %*/
                    }
#line 14492 "parse.c"
    break;

  case 503: /* p_top_expr_body: p_find  */
#line 5104 "parse.y"
                  {
                        (yyval.node) = new_find_pattern(p, Qnone, (yyvsp[0].node), &(yyloc));
                    }
#line 14500 "parse.c"
    break;

  case 504: /* p_top_expr_body: p_args_tail  */
#line 5108 "parse.y"
                  {
                        (yyval.node) = new_array_pattern(p, Qnone, Qnone, (yyvsp[0].node), &(yyloc));
                    }
#line 14508 "parse.c"
    break;

  case 505: /* p_top_expr_body: p_kwargs  */
#line 5112 "parse.y"
                  {
                        (yyval.node) = new_hash_pattern(p, Qnone, (yyvsp[0].node), &(yyloc));
                    }
#line 14516 "parse.c"
    break;

  case 507: /* p_as: p_expr "=>" p_variable  */
#line 5121 "parse.y"
                  {
                    /*%%%*/
                        NODE *n = NEW_LIST((yyvsp[-2].node), &(yyloc));
                        n = list_append(p, n, (yyvsp[0].node));
                        (yyval.node) = new_hash(p, n, &(yyloc));
                    /*% %*/
                    /*% ripper: binary!($1, STATIC_ID2SYM((id_assoc)), $3) %*/
                    }
#line 14529 "parse.c"
    break;

  case 509: /* p_alt: p_alt '|' p_expr_basic  */
#line 5133 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_OR((yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: binary!($1, STATIC_ID2SYM(idOr), $3) %*/
                    }
#line 14540 "parse.c"
    break;

  case 511: /* p_lparen: '(' p_pktbl  */
#line 5142 "parse.y"
                     { (yyval.tbl) = (yyvsp[0].tbl);}
#line 14546 "parse.c"
    break;

  case 512: /* p_lbracket: '[' p_pktbl  */
#line 5143 "parse.y"
                       { (yyval.tbl) = (yyvsp[0].tbl);}
#line 14552 "parse.c"
    break;

  case 515: /* p_expr_basic: p_const p_lparen p_args rparen  */
#line 5148 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-2].tbl));
                        (yyval.node) = new_array_pattern(p, (yyvsp[-3].node), Qnone, (yyvsp[-1].node), &(yyloc));
                    /*%%%*/
                        nd_set_first_loc((yyval.node), (yylsp[-3]).beg_pos);
                    /*%
                    %*/
                    }
#line 14565 "parse.c"
    break;

  case 516: /* p_expr_basic: p_const p_lparen p_find rparen  */
#line 5157 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-2].tbl));
                        (yyval.node) = new_find_pattern(p, (yyvsp[-3].node), (yyvsp[-1].node), &(yyloc));
                    /*%%%*/
                        nd_set_first_loc((yyval.node), (yylsp[-3]).beg_pos);
                    /*%
                    %*/
                    }
#line 14578 "parse.c"
    break;

  case 517: /* p_expr_basic: p_const p_lparen p_kwargs rparen  */
#line 5166 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-2].tbl));
                        (yyval.node) = new_hash_pattern(p, (yyvsp[-3].node), (yyvsp[-1].node), &(yyloc));
                    /*%%%*/
                        nd_set_first_loc((yyval.node), (yylsp[-3]).beg_pos);
                    /*%
                    %*/
                    }
#line 14591 "parse.c"
    break;

  case 518: /* p_expr_basic: p_const '(' rparen  */
#line 5175 "parse.y"
                  {
                        (yyval.node) = new_array_pattern_tail(p, Qnone, 0, Qnone, Qnone, &(yyloc));
                        (yyval.node) = new_array_pattern(p, (yyvsp[-2].node), Qnone, (yyval.node), &(yyloc));
                    }
#line 14600 "parse.c"
    break;

  case 519: /* p_expr_basic: p_const p_lbracket p_args rbracket  */
#line 5180 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-2].tbl));
                        (yyval.node) = new_array_pattern(p, (yyvsp[-3].node), Qnone, (yyvsp[-1].node), &(yyloc));
                    /*%%%*/
                        nd_set_first_loc((yyval.node), (yylsp[-3]).beg_pos);
                    /*%
                    %*/
                    }
#line 14613 "parse.c"
    break;

  case 520: /* p_expr_basic: p_const p_lbracket p_find rbracket  */
#line 5189 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-2].tbl));
                        (yyval.node) = new_find_pattern(p, (yyvsp[-3].node), (yyvsp[-1].node), &(yyloc));
                    /*%%%*/
                        nd_set_first_loc((yyval.node), (yylsp[-3]).beg_pos);
                    /*%
                    %*/
                    }
#line 14626 "parse.c"
    break;

  case 521: /* p_expr_basic: p_const p_lbracket p_kwargs rbracket  */
#line 5198 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-2].tbl));
                        (yyval.node) = new_hash_pattern(p, (yyvsp[-3].node), (yyvsp[-1].node), &(yyloc));
                    /*%%%*/
                        nd_set_first_loc((yyval.node), (yylsp[-3]).beg_pos);
                    /*%
                    %*/
                    }
#line 14639 "parse.c"
    break;

  case 522: /* p_expr_basic: p_const '[' rbracket  */
#line 5207 "parse.y"
                  {
                        (yyval.node) = new_array_pattern_tail(p, Qnone, 0, Qnone, Qnone, &(yyloc));
                        (yyval.node) = new_array_pattern(p, (yyvsp[-2].node), Qnone, (yyval.node), &(yyloc));
                    }
#line 14648 "parse.c"
    break;

  case 523: /* p_expr_basic: "[" p_args rbracket  */
#line 5212 "parse.y"
                  {
                        (yyval.node) = new_array_pattern(p, Qnone, Qnone, (yyvsp[-1].node), &(yyloc));
                    }
#line 14656 "parse.c"
    break;

  case 524: /* p_expr_basic: "[" p_find rbracket  */
#line 5216 "parse.y"
                  {
                        (yyval.node) = new_find_pattern(p, Qnone, (yyvsp[-1].node), &(yyloc));
                    }
#line 14664 "parse.c"
    break;

  case 525: /* p_expr_basic: "[" rbracket  */
#line 5220 "parse.y"
                  {
                        (yyval.node) = new_array_pattern_tail(p, Qnone, 0, Qnone, Qnone, &(yyloc));
                        (yyval.node) = new_array_pattern(p, Qnone, Qnone, (yyval.node), &(yyloc));
                    }
#line 14673 "parse.c"
    break;

  case 526: /* $@30: %empty  */
#line 5225 "parse.y"
                  {
                        p->ctxt.in_kwarg = 0;
                    }
#line 14681 "parse.c"
    break;

  case 527: /* p_expr_basic: "{" p_pktbl lex_ctxt $@30 p_kwargs rbrace  */
#line 5229 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-4].tbl));
                        p->ctxt.in_kwarg = (yyvsp[-3].ctxt).in_kwarg;
                        (yyval.node) = new_hash_pattern(p, Qnone, (yyvsp[-1].node), &(yyloc));
                    }
#line 14691 "parse.c"
    break;

  case 528: /* p_expr_basic: "{" rbrace  */
#line 5235 "parse.y"
                  {
                        (yyval.node) = new_hash_pattern_tail(p, Qnone, 0, &(yyloc));
                        (yyval.node) = new_hash_pattern(p, Qnone, (yyval.node), &(yyloc));
                    }
#line 14700 "parse.c"
    break;

  case 529: /* p_expr_basic: "(" p_pktbl p_expr rparen  */
#line 5240 "parse.y"
                  {
                        pop_pktbl(p, (yyvsp[-2].tbl));
                        (yyval.node) = (yyvsp[-1].node);
                    }
#line 14709 "parse.c"
    break;

  case 530: /* p_args: p_expr  */
#line 5247 "parse.y"
                  {
                    /*%%%*/
                        NODE *pre_args = NEW_LIST((yyvsp[0].node), &(yyloc));
                        (yyval.node) = new_array_pattern_tail(p, pre_args, 0, Qnone, Qnone, &(yyloc));
                    /*%
                        $$ = new_array_pattern_tail(p, rb_ary_new_from_args(1, get_value($1)), 0, Qnone, Qnone, &@$);
                    %*/
                    }
#line 14722 "parse.c"
    break;

  case 531: /* p_args: p_args_head  */
#line 5256 "parse.y"
                  {
                        (yyval.node) = new_array_pattern_tail(p, (yyvsp[0].node), 1, Qnone, Qnone, &(yyloc));
                    }
#line 14730 "parse.c"
    break;

  case 532: /* p_args: p_args_head p_arg  */
#line 5260 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_array_pattern_tail(p, list_concat((yyvsp[-1].node), (yyvsp[0].node)), 0, Qnone, Qnone, &(yyloc));
                    /*%
                        VALUE pre_args = rb_ary_concat($1, get_value($2));
                        $$ = new_array_pattern_tail(p, pre_args, 0, Qnone, Qnone, &@$);
                    %*/
                    }
#line 14743 "parse.c"
    break;

  case 533: /* p_args: p_args_head p_rest  */
#line 5269 "parse.y"
                  {
                        (yyval.node) = new_array_pattern_tail(p, (yyvsp[-1].node), 1, (yyvsp[0].node), Qnone, &(yyloc));
                    }
#line 14751 "parse.c"
    break;

  case 534: /* p_args: p_args_head p_rest ',' p_args_post  */
#line 5273 "parse.y"
                  {
                        (yyval.node) = new_array_pattern_tail(p, (yyvsp[-3].node), 1, (yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    }
#line 14759 "parse.c"
    break;

  case 536: /* p_args_head: p_arg ','  */
#line 5280 "parse.y"
                  {
                        (yyval.node) = (yyvsp[-1].node);
                    }
#line 14767 "parse.c"
    break;

  case 537: /* p_args_head: p_args_head p_arg ','  */
#line 5284 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = list_concat((yyvsp[-2].node), (yyvsp[-1].node));
                    /*% %*/
                    /*% ripper: rb_ary_concat($1, get_value($2)) %*/
                    }
#line 14778 "parse.c"
    break;

  case 538: /* p_args_tail: p_rest  */
#line 5293 "parse.y"
                  {
                        (yyval.node) = new_array_pattern_tail(p, Qnone, 1, (yyvsp[0].node), Qnone, &(yyloc));
                    }
#line 14786 "parse.c"
    break;

  case 539: /* p_args_tail: p_rest ',' p_args_post  */
#line 5297 "parse.y"
                  {
                        (yyval.node) = new_array_pattern_tail(p, Qnone, 1, (yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    }
#line 14794 "parse.c"
    break;

  case 540: /* p_find: p_rest ',' p_args_post ',' p_rest  */
#line 5303 "parse.y"
                  {
                        (yyval.node) = new_find_pattern_tail(p, (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    }
#line 14802 "parse.c"
    break;

  case 541: /* p_rest: "*" "local variable or method"  */
#line 5310 "parse.y"
                  {
                    /*%%%*/
                        error_duplicate_pattern_variable(p, (yyvsp[0].id), &(yylsp[0]));
                        (yyval.node) = assignable(p, (yyvsp[0].id), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: assignable(p, var_field(p, $2)) %*/
                    }
#line 14814 "parse.c"
    break;

  case 542: /* p_rest: "*"  */
#line 5318 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = 0;
                    /*% %*/
                    /*% ripper: var_field(p, Qnil) %*/
                    }
#line 14825 "parse.c"
    break;

  case 544: /* p_args_post: p_args_post ',' p_arg  */
#line 5328 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = list_concat((yyvsp[-2].node), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: rb_ary_concat($1, get_value($3)) %*/
                    }
#line 14836 "parse.c"
    break;

  case 545: /* p_arg: p_expr  */
#line 5337 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_LIST((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: rb_ary_new_from_args(1, get_value($1)) %*/
                    }
#line 14847 "parse.c"
    break;

  case 546: /* p_kwargs: p_kwarg ',' p_any_kwrest  */
#line 5346 "parse.y"
                  {
                        (yyval.node) =  new_hash_pattern_tail(p, new_unique_key_hash(p, (yyvsp[-2].node), &(yyloc)), (yyvsp[0].id), &(yyloc));
                    }
#line 14855 "parse.c"
    break;

  case 547: /* p_kwargs: p_kwarg  */
#line 5350 "parse.y"
                  {
                        (yyval.node) =  new_hash_pattern_tail(p, new_unique_key_hash(p, (yyvsp[0].node), &(yyloc)), 0, &(yyloc));
                    }
#line 14863 "parse.c"
    break;

  case 548: /* p_kwargs: p_kwarg ','  */
#line 5354 "parse.y"
                  {
                        (yyval.node) =  new_hash_pattern_tail(p, new_unique_key_hash(p, (yyvsp[-1].node), &(yyloc)), 0, &(yyloc));
                    }
#line 14871 "parse.c"
    break;

  case 549: /* p_kwargs: p_any_kwrest  */
#line 5358 "parse.y"
                  {
                        (yyval.node) =  new_hash_pattern_tail(p, new_hash(p, Qnone, &(yyloc)), (yyvsp[0].id), &(yyloc));
                    }
#line 14879 "parse.c"
    break;

  case 551: /* p_kwarg: p_kwarg ',' p_kw  */
#line 5366 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = list_concat((yyvsp[-2].node), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: rb_ary_push($1, $3) %*/
                    }
#line 14890 "parse.c"
    break;

  case 552: /* p_kw: p_kw_label p_expr  */
#line 5375 "parse.y"
                  {
                        error_duplicate_pattern_key(p, get_id((yyvsp[-1].id)), &(yylsp[-1]));
                    /*%%%*/
                        (yyval.node) = list_append(p, NEW_LIST(NEW_LIT(ID2SYM((yyvsp[-1].id)), &(yylsp[-1])), &(yyloc)), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: rb_ary_new_from_args(2, get_value($1), get_value($2)) %*/
                    }
#line 14902 "parse.c"
    break;

  case 553: /* p_kw: p_kw_label  */
#line 5383 "parse.y"
                  {
                        error_duplicate_pattern_key(p, get_id((yyvsp[0].id)), &(yylsp[0]));
                        if ((yyvsp[0].id) && !is_local_id(get_id((yyvsp[0].id)))) {
                            yyerror1(&(yylsp[0]), "key must be valid as local variables");
                        }
                        error_duplicate_pattern_variable(p, get_id((yyvsp[0].id)), &(yylsp[0]));
                    /*%%%*/
                        (yyval.node) = list_append(p, NEW_LIST(NEW_LIT(ID2SYM((yyvsp[0].id)), &(yyloc)), &(yyloc)), assignable(p, (yyvsp[0].id), 0, &(yyloc)));
                    /*% %*/
                    /*% ripper: rb_ary_new_from_args(2, get_value(assignable(p, $1)), Qnil) %*/
                    }
#line 14918 "parse.c"
    break;

  case 555: /* p_kw_label: "string literal" string_contents tLABEL_END  */
#line 5398 "parse.y"
                  {
                        YYLTYPE loc = code_loc_gen(&(yylsp[-2]), &(yylsp[0]));
                    /*%%%*/
                        if (!(yyvsp[-1].node) || nd_type_p((yyvsp[-1].node), NODE_STR)) {
                            NODE *node = dsym_node(p, (yyvsp[-1].node), &loc);
                            (yyval.id) = SYM2ID(RNODE_LIT(node)->nd_lit);
                        }
                    /*%
                        if (ripper_is_node_yylval(p, $2) && RNODE_RIPPER($2)->nd_cval) {
                            VALUE label = RNODE_RIPPER($2)->nd_cval;
                            VALUE rval = RNODE_RIPPER($2)->nd_rval;
                            $$ = ripper_new_yylval(p, rb_intern_str(label), rval, label);
                            RNODE($$)->nd_loc = loc;
                        }
                    %*/
                        else {
                            yyerror1(&loc, "symbol literal with interpolation is not allowed");
                            (yyval.id) = 0;
                        }
                    }
#line 14943 "parse.c"
    break;

  case 556: /* p_kwrest: kwrest_mark "local variable or method"  */
#line 5421 "parse.y"
                  {
                        (yyval.id) = (yyvsp[0].id);
                    }
#line 14951 "parse.c"
    break;

  case 557: /* p_kwrest: kwrest_mark  */
#line 5425 "parse.y"
                  {
                        (yyval.id) = 0;
                    }
#line 14959 "parse.c"
    break;

  case 558: /* p_kwnorest: kwrest_mark "`nil'"  */
#line 5431 "parse.y"
                  {
                        (yyval.id) = 0;
                    }
#line 14967 "parse.c"
    break;

  case 560: /* p_any_kwrest: p_kwnorest  */
#line 5437 "parse.y"
                           {(yyval.id) = ID2VAL(idNil);}
#line 14973 "parse.c"
    break;

  case 562: /* p_value: p_primitive ".." p_primitive  */
#line 5442 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[-2].node));
                        value_expr((yyvsp[0].node));
                        (yyval.node) = NEW_DOT2((yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: dot2!($1, $3) %*/
                    }
#line 14986 "parse.c"
    break;

  case 563: /* p_value: p_primitive "..." p_primitive  */
#line 5451 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[-2].node));
                        value_expr((yyvsp[0].node));
                        (yyval.node) = NEW_DOT3((yyvsp[-2].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: dot3!($1, $3) %*/
                    }
#line 14999 "parse.c"
    break;

  case 564: /* p_value: p_primitive ".."  */
#line 5460 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[-1].node));
                        (yyval.node) = NEW_DOT2((yyvsp[-1].node), new_nil_at(p, &(yylsp[0]).end_pos), &(yyloc));
                    /*% %*/
                    /*% ripper: dot2!($1, Qnil) %*/
                    }
#line 15011 "parse.c"
    break;

  case 565: /* p_value: p_primitive "..."  */
#line 5468 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[-1].node));
                        (yyval.node) = NEW_DOT3((yyvsp[-1].node), new_nil_at(p, &(yylsp[0]).end_pos), &(yyloc));
                    /*% %*/
                    /*% ripper: dot3!($1, Qnil) %*/
                    }
#line 15023 "parse.c"
    break;

  case 569: /* p_value: "(.." p_primitive  */
#line 5479 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[0].node));
                        (yyval.node) = NEW_DOT2(new_nil_at(p, &(yylsp[-1]).beg_pos), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: dot2!(Qnil, $2) %*/
                    }
#line 15035 "parse.c"
    break;

  case 570: /* p_value: "(..." p_primitive  */
#line 5487 "parse.y"
                  {
                    /*%%%*/
                        value_expr((yyvsp[0].node));
                        (yyval.node) = NEW_DOT3(new_nil_at(p, &(yylsp[-1]).beg_pos), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: dot3!(Qnil, $2) %*/
                    }
#line 15047 "parse.c"
    break;

  case 579: /* p_primitive: keyword_variable  */
#line 5505 "parse.y"
                  {
                    /*%%%*/
                        if (!((yyval.node) = gettable(p, (yyvsp[0].id), &(yyloc)))) (yyval.node) = NEW_ERROR(&(yyloc));
                    /*% %*/
                    /*% ripper: var_ref!($1) %*/
                    }
#line 15058 "parse.c"
    break;

  case 581: /* p_variable: "local variable or method"  */
#line 5515 "parse.y"
                  {
                    /*%%%*/
                        error_duplicate_pattern_variable(p, (yyvsp[0].id), &(yylsp[0]));
                        (yyval.node) = assignable(p, (yyvsp[0].id), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: assignable(p, var_field(p, $1)) %*/
                    }
#line 15070 "parse.c"
    break;

  case 582: /* p_var_ref: '^' "local variable or method"  */
#line 5525 "parse.y"
                  {
                    /*%%%*/
                        NODE *n = gettable(p, (yyvsp[0].id), &(yyloc));
                        if (!(nd_type_p(n, NODE_LVAR) || nd_type_p(n, NODE_DVAR))) {
                            compile_error(p, "%"PRIsVALUE": no such local variable", rb_id2str((yyvsp[0].id)));
                        }
                        (yyval.node) = n;
                    /*% %*/
                    /*% ripper: var_ref!($2) %*/
                    }
#line 15085 "parse.c"
    break;

  case 583: /* p_var_ref: '^' nonlocal_var  */
#line 5536 "parse.y"
                  {
                    /*%%%*/
                        if (!((yyval.node) = gettable(p, (yyvsp[0].id), &(yyloc)))) (yyval.node) = NEW_BEGIN(0, &(yyloc));
                    /*% %*/
                    /*% ripper: var_ref!($2) %*/
                    }
#line 15096 "parse.c"
    break;

  case 584: /* p_expr_ref: '^' "(" expr_value rparen  */
#line 5545 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_BLOCK((yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: begin!($3) %*/
                    }
#line 15107 "parse.c"
    break;

  case 585: /* p_const: ":: at EXPR_BEG" cname  */
#line 5554 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_COLON3((yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: top_const_ref!($2) %*/
                    }
#line 15118 "parse.c"
    break;

  case 586: /* p_const: p_const "::" cname  */
#line 5561 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_COLON2((yyvsp[-2].node), (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: const_path_ref!($1, $3) %*/
                    }
#line 15129 "parse.c"
    break;

  case 587: /* p_const: "constant"  */
#line 5568 "parse.y"
                 {
                    /*%%%*/
                        (yyval.node) = gettable(p, (yyvsp[0].id), &(yyloc));
                    /*% %*/
                    /*% ripper: var_ref!($1) %*/
                   }
#line 15140 "parse.c"
    break;

  case 588: /* opt_rescue: k_rescue exc_list exc_var then compstmt opt_rescue  */
#line 5579 "parse.y"
                  {
                    /*%%%*/
                        NODE *body = (yyvsp[-1].node);
                        if ((yyvsp[-3].node)) {
                            NODE *err = NEW_ERRINFO(&(yylsp[-3]));
                            err = node_assign(p, (yyvsp[-3].node), err, NO_LEX_CTXT, &(yylsp[-3]));
                            body = block_append(p, err, body);
                        }
                        (yyval.node) = NEW_RESBODY((yyvsp[-4].node), body, (yyvsp[0].node), &(yyloc));
                        if ((yyvsp[-4].node)) {
                            fixpos((yyval.node), (yyvsp[-4].node));
                        }
                        else if ((yyvsp[-3].node)) {
                            fixpos((yyval.node), (yyvsp[-3].node));
                        }
                        else {
                            fixpos((yyval.node), (yyvsp[-1].node));
                        }
                    /*% %*/
                    /*% ripper: rescue!($2, $3, $5, $6) %*/
                    }
#line 15166 "parse.c"
    break;

  case 590: /* exc_list: arg_value  */
#line 5604 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = NEW_LIST((yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: rb_ary_new3(1, get_value($1)) %*/
                    }
#line 15177 "parse.c"
    break;

  case 591: /* exc_list: mrhs  */
#line 5611 "parse.y"
                  {
                    /*%%%*/
                        if (!((yyval.node) = splat_array((yyvsp[0].node)))) (yyval.node) = (yyvsp[0].node);
                    /*% %*/
                    /*% ripper: $1 %*/
                    }
#line 15188 "parse.c"
    break;

  case 593: /* exc_var: "=>" lhs  */
#line 5621 "parse.y"
                  {
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 15196 "parse.c"
    break;

  case 595: /* opt_ensure: k_ensure compstmt  */
#line 5628 "parse.y"
                  {
                        p->ctxt.in_rescue = (yyvsp[-1].ctxt).in_rescue;
                    /*%%%*/
                        (yyval.node) = (yyvsp[0].node);
                    /*% %*/
                    /*% ripper: ensure!($2) %*/
                    }
#line 15208 "parse.c"
    break;

  case 599: /* strings: string  */
#line 5643 "parse.y"
                  {
                    /*%%%*/
                        NODE *node = (yyvsp[0].node);
                        if (!node) {
                            node = NEW_STR(STR_NEW0(), &(yyloc));
                            RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_STR(node)->nd_lit);
                        }
                        else {
                            node = evstr2dstr(p, node);
                        }
                        (yyval.node) = node;
                    /*% %*/
                    /*% ripper: $1 %*/
                    }
#line 15227 "parse.c"
    break;

  case 602: /* string: string string1  */
#line 5662 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = literal_concat(p, (yyvsp[-1].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: string_concat!($1, $2) %*/
                    }
#line 15238 "parse.c"
    break;

  case 603: /* string1: "string literal" string_contents "terminator"  */
#line 5671 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = heredoc_dedent(p, (yyvsp[-1].node));
                        if ((yyval.node)) nd_set_loc((yyval.node), &(yyloc));
                    /*% %*/
                    /*% ripper: string_literal!(heredoc_dedent(p, $2)) %*/
                    }
#line 15250 "parse.c"
    break;

  case 604: /* xstring: "backtick literal" xstring_contents "terminator"  */
#line 5681 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = new_xstring(p, heredoc_dedent(p, (yyvsp[-1].node)), &(yyloc));
                    /*% %*/
                    /*% ripper: xstring_literal!(heredoc_dedent(p, $2)) %*/
                    }
#line 15261 "parse.c"
    break;

  case 605: /* regexp: "regexp literal" regexp_contents tREGEXP_END  */
#line 5690 "parse.y"
                  {
                        (yyval.node) = new_regexp(p, (yyvsp[-1].node), (yyvsp[0].num), &(yyloc));
                    }
#line 15269 "parse.c"
    break;

  case 606: /* words_sep: ' '  */
#line 5695 "parse.y"
              {}
#line 15275 "parse.c"
    break;

  case 608: /* words: "word list" words_sep word_list "terminator"  */
#line 5700 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = make_list((yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: array!($3) %*/
                    }
#line 15286 "parse.c"
    break;

  case 609: /* word_list: %empty  */
#line 5709 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = 0;
                    /*% %*/
                    /*% ripper: words_new! %*/
                    }
#line 15297 "parse.c"
    break;

  case 610: /* word_list: word_list word words_sep  */
#line 5716 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = list_append(p, (yyvsp[-2].node), evstr2dstr(p, (yyvsp[-1].node)));
                    /*% %*/
                    /*% ripper: words_add!($1, $2) %*/
                    }
#line 15308 "parse.c"
    break;

  case 612: /* word: word string_content  */
#line 5727 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = literal_concat(p, (yyvsp[-1].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: word_add!($1, $2) %*/
                    }
#line 15319 "parse.c"
    break;

  case 613: /* symbols: "symbol list" words_sep symbol_list "terminator"  */
#line 5736 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = make_list((yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: array!($3) %*/
                    }
#line 15330 "parse.c"
    break;

  case 614: /* symbol_list: %empty  */
#line 5745 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = 0;
                    /*% %*/
                    /*% ripper: symbols_new! %*/
                    }
#line 15341 "parse.c"
    break;

  case 615: /* symbol_list: symbol_list word words_sep  */
#line 5752 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = symbol_append(p, (yyvsp[-2].node), evstr2dstr(p, (yyvsp[-1].node)));
                    /*% %*/
                    /*% ripper: symbols_add!($1, $2) %*/
                    }
#line 15352 "parse.c"
    break;

  case 616: /* qwords: "verbatim word list" words_sep qword_list "terminator"  */
#line 5761 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = make_list((yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: array!($3) %*/
                    }
#line 15363 "parse.c"
    break;

  case 617: /* qsymbols: "verbatim symbol list" words_sep qsym_list "terminator"  */
#line 5770 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = make_list((yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: array!($3) %*/
                    }
#line 15374 "parse.c"
    break;

  case 618: /* qword_list: %empty  */
#line 5779 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = 0;
                    /*% %*/
                    /*% ripper: qwords_new! %*/
                    }
#line 15385 "parse.c"
    break;

  case 619: /* qword_list: qword_list "literal content" words_sep  */
#line 5786 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = list_append(p, (yyvsp[-2].node), (yyvsp[-1].node));
                    /*% %*/
                    /*% ripper: qwords_add!($1, $2) %*/
                    }
#line 15396 "parse.c"
    break;

  case 620: /* qsym_list: %empty  */
#line 5795 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = 0;
                    /*% %*/
                    /*% ripper: qsymbols_new! %*/
                    }
#line 15407 "parse.c"
    break;

  case 621: /* qsym_list: qsym_list "literal content" words_sep  */
#line 5802 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = symbol_append(p, (yyvsp[-2].node), (yyvsp[-1].node));
                    /*% %*/
                    /*% ripper: qsymbols_add!($1, $2) %*/
                    }
#line 15418 "parse.c"
    break;

  case 622: /* string_contents: %empty  */
#line 5811 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = 0;
                    /*% %*/
                    /*% ripper: string_content! %*/
                    /*%%%*/
                    /*%
                        $$ = ripper_new_yylval(p, 0, $$, 0);
                    %*/
                    }
#line 15433 "parse.c"
    break;

  case 623: /* string_contents: string_contents string_content  */
#line 5822 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = literal_concat(p, (yyvsp[-1].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: string_add!($1, $2) %*/
                    /*%%%*/
                    /*%
                        if (ripper_is_node_yylval(p, $1) && ripper_is_node_yylval(p, $2) &&
                            !RNODE_RIPPER($1)->nd_cval) {
                            RNODE_RIPPER($1)->nd_cval = RNODE_RIPPER($2)->nd_cval;
                            RNODE_RIPPER($1)->nd_rval = add_mark_object(p, $$);
                            $$ = $1;
                        }
                    %*/
                    }
#line 15453 "parse.c"
    break;

  case 624: /* xstring_contents: %empty  */
#line 5840 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = 0;
                    /*% %*/
                    /*% ripper: xstring_new! %*/
                    }
#line 15464 "parse.c"
    break;

  case 625: /* xstring_contents: xstring_contents string_content  */
#line 5847 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = literal_concat(p, (yyvsp[-1].node), (yyvsp[0].node), &(yyloc));
                    /*% %*/
                    /*% ripper: xstring_add!($1, $2) %*/
                    }
#line 15475 "parse.c"
    break;

  case 626: /* regexp_contents: %empty  */
#line 5856 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = 0;
                    /*% %*/
                    /*% ripper: regexp_new! %*/
                    /*%%%*/
                    /*%
                        $$ = ripper_new_yylval(p, 0, $$, 0);
                    %*/
                    }
#line 15490 "parse.c"
    break;

  case 627: /* regexp_contents: regexp_contents string_content  */
#line 5867 "parse.y"
                  {
                    /*%%%*/
                        NODE *head = (yyvsp[-1].node), *tail = (yyvsp[0].node);
                        if (!head) {
                            (yyval.node) = tail;
                        }
                        else if (!tail) {
                            (yyval.node) = head;
                        }
                        else {
                            switch (nd_type(head)) {
                              case NODE_STR:
                                head = str2dstr(p, head);
                                break;
                              case NODE_DSTR:
                                break;
                              default:
                                head = list_append(p, NEW_DSTR(Qnil, &(yyloc)), head);
                                break;
                            }
                            (yyval.node) = list_append(p, head, tail);
                        }
                    /*%
                        VALUE s1 = 1, s2 = 0, n1 = $1, n2 = $2;
                        if (ripper_is_node_yylval(p, n1)) {
                            s1 = RNODE_RIPPER(n1)->nd_cval;
                            n1 = RNODE_RIPPER(n1)->nd_rval;
                        }
                        if (ripper_is_node_yylval(p, n2)) {
                            s2 = RNODE_RIPPER(n2)->nd_cval;
                            n2 = RNODE_RIPPER(n2)->nd_rval;
                        }
                        $$ = dispatch2(regexp_add, n1, n2);
                        if (!s1 && s2) {
                            $$ = ripper_new_yylval(p, 0, $$, s2);
                        }
                    %*/
                    }
#line 15533 "parse.c"
    break;

  case 629: /* @31: %empty  */
#line 5910 "parse.y"
                  {
                        /* need to backup p->lex.strterm so that a string literal `%&foo,#$&,bar&` can be parsed */
                        (yyval.strterm) = p->lex.strterm;
                        p->lex.strterm = 0;
                        SET_LEX_STATE(EXPR_BEG);
                    }
#line 15544 "parse.c"
    break;

  case 630: /* string_content: tSTRING_DVAR @31 string_dvar  */
#line 5917 "parse.y"
                  {
                        p->lex.strterm = (yyvsp[-1].strterm);
                    /*%%%*/
                        (yyval.node) = NEW_EVSTR((yyvsp[0].node), &(yyloc));
                        nd_set_line((yyval.node), (yylsp[0]).end_pos.lineno);
                    /*% %*/
                    /*% ripper: string_dvar!($3) %*/
                    }
#line 15557 "parse.c"
    break;

  case 631: /* @32: %empty  */
#line 5926 "parse.y"
                  {
                        CMDARG_PUSH(0);
                        COND_PUSH(0);
                        /* need to backup p->lex.strterm so that a string literal `%!foo,#{ !0 },bar!` can be parsed */
                        (yyvsp[0].strterm) = p->lex.strterm;
                        p->lex.strterm = 0;
                        (yyval.num) = p->lex.state;
                        SET_LEX_STATE(EXPR_BEG);
                    }
#line 15571 "parse.c"
    break;

  case 632: /* @33: %empty  */
#line 5935 "parse.y"
                  {
                        (yyval.num) = p->lex.brace_nest;
                        p->lex.brace_nest = 0;
                    }
#line 15580 "parse.c"
    break;

  case 633: /* @34: %empty  */
#line 5939 "parse.y"
                  {
                        (yyval.num) = p->heredoc_indent;
                        p->heredoc_indent = 0;
                    }
#line 15589 "parse.c"
    break;

  case 634: /* string_content: tSTRING_DBEG @32 @33 @34 compstmt string_dend  */
#line 5944 "parse.y"
                  {
                        COND_POP();
                        CMDARG_POP();
                        p->lex.strterm = (yyvsp[-5].strterm);
                        SET_LEX_STATE((yyvsp[-4].num));
                        p->lex.brace_nest = (yyvsp[-3].num);
                        p->heredoc_indent = (yyvsp[-2].num);
                        p->heredoc_line_indent = -1;
                    /*%%%*/
                        if ((yyvsp[-1].node)) nd_unset_fl_newline((yyvsp[-1].node));
                        (yyval.node) = new_evstr(p, (yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: string_embexpr!($compstmt) %*/
                    }
#line 15608 "parse.c"
    break;

  case 637: /* string_dvar: nonlocal_var  */
#line 5965 "parse.y"
                  {
                    /*%%%*/
                        if (!((yyval.node) = gettable(p, (yyvsp[0].id), &(yyloc)))) (yyval.node) = NEW_ERROR(&(yyloc));
                    /*% %*/
                    /*% ripper: var_ref!($1) %*/
                    }
#line 15619 "parse.c"
    break;

  case 641: /* ssym: "symbol literal" sym  */
#line 5979 "parse.y"
                  {
                        SET_LEX_STATE(EXPR_END);
                    /*%%%*/
                        (yyval.node) = NEW_LIT(ID2SYM((yyvsp[0].id)), &(yyloc));
                    /*% %*/
                    /*% ripper: symbol_literal!(symbol!($2)) %*/
                    }
#line 15631 "parse.c"
    break;

  case 644: /* dsym: "symbol literal" string_contents "terminator"  */
#line 5993 "parse.y"
                  {
                        SET_LEX_STATE(EXPR_END);
                    /*%%%*/
                        (yyval.node) = dsym_node(p, (yyvsp[-1].node), &(yyloc));
                    /*% %*/
                    /*% ripper: dyna_symbol!($2) %*/
                    }
#line 15643 "parse.c"
    break;

  case 646: /* numeric: tUMINUS_NUM simple_numeric  */
#line 6004 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[0].node);
                        RB_OBJ_WRITE(p->ast, &RNODE_LIT((yyval.node))->nd_lit, negate_lit(p, RNODE_LIT((yyval.node))->nd_lit));
                    /*% %*/
                    /*% ripper: unary!(ID2VAL(idUMinus), $2) %*/
                    }
#line 15655 "parse.c"
    break;

  case 657: /* keyword_variable: "`nil'"  */
#line 6029 "parse.y"
                            {(yyval.id) = KWD2EID(nil, (yyvsp[0].id));}
#line 15661 "parse.c"
    break;

  case 658: /* keyword_variable: "`self'"  */
#line 6030 "parse.y"
                             {(yyval.id) = KWD2EID(self, (yyvsp[0].id));}
#line 15667 "parse.c"
    break;

  case 659: /* keyword_variable: "`true'"  */
#line 6031 "parse.y"
                             {(yyval.id) = KWD2EID(true, (yyvsp[0].id));}
#line 15673 "parse.c"
    break;

  case 660: /* keyword_variable: "`false'"  */
#line 6032 "parse.y"
                              {(yyval.id) = KWD2EID(false, (yyvsp[0].id));}
#line 15679 "parse.c"
    break;

  case 661: /* keyword_variable: "`__FILE__'"  */
#line 6033 "parse.y"
                                {(yyval.id) = KWD2EID(_FILE__, (yyvsp[0].id));}
#line 15685 "parse.c"
    break;

  case 662: /* keyword_variable: "`__LINE__'"  */
#line 6034 "parse.y"
                                {(yyval.id) = KWD2EID(_LINE__, (yyvsp[0].id));}
#line 15691 "parse.c"
    break;

  case 663: /* keyword_variable: "`__ENCODING__'"  */
#line 6035 "parse.y"
                                    {(yyval.id) = KWD2EID(_ENCODING__, (yyvsp[0].id));}
#line 15697 "parse.c"
    break;

  case 664: /* var_ref: user_variable  */
#line 6039 "parse.y"
                  {
                    /*%%%*/
                        if (!((yyval.node) = gettable(p, (yyvsp[0].id), &(yyloc)))) (yyval.node) = NEW_BEGIN(0, &(yyloc));
                    /*%
                        if (id_is_var(p, get_id($1))) {
                            $$ = dispatch1(var_ref, $1);
                        }
                        else {
                            $$ = dispatch1(vcall, $1);
                        }
                    %*/
                    }
#line 15714 "parse.c"
    break;

  case 665: /* var_ref: keyword_variable  */
#line 6052 "parse.y"
                  {
                    /*%%%*/
                        if (!((yyval.node) = gettable(p, (yyvsp[0].id), &(yyloc)))) (yyval.node) = NEW_ERROR(&(yyloc));
                    /*% %*/
                    /*% ripper: var_ref!($1) %*/
                    }
#line 15725 "parse.c"
    break;

  case 666: /* var_lhs: user_variable  */
#line 6061 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = assignable(p, (yyvsp[0].id), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: assignable(p, var_field(p, $1)) %*/
                    }
#line 15736 "parse.c"
    break;

  case 667: /* var_lhs: keyword_variable  */
#line 6068 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = assignable(p, (yyvsp[0].id), 0, &(yyloc));
                    /*% %*/
                    /*% ripper: assignable(p, var_field(p, $1)) %*/
                    }
#line 15747 "parse.c"
    break;

  case 670: /* $@35: %empty  */
#line 6081 "parse.y"
                  {
                        SET_LEX_STATE(EXPR_BEG);
                        p->command_start = TRUE;
                    }
#line 15756 "parse.c"
    break;

  case 671: /* superclass: '<' $@35 expr_value term  */
#line 6086 "parse.y"
                  {
                        (yyval.node) = (yyvsp[-1].node);
                    }
#line 15764 "parse.c"
    break;

  case 672: /* superclass: %empty  */
#line 6090 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = 0;
                    /*% %*/
                    /*% ripper: Qnil %*/
                    }
#line 15775 "parse.c"
    break;

  case 674: /* f_opt_paren_args: none  */
#line 6100 "parse.y"
                  {
                        p->ctxt.in_argdef = 0;
                        (yyval.node_args) = new_args_tail(p, Qnone, Qnone, Qnone, &(yylsp[-1]));
                        (yyval.node_args) = new_args(p, Qnone, Qnone, Qnone, Qnone, (yyval.node_args), &(yylsp[-1]));
                    }
#line 15785 "parse.c"
    break;

  case 675: /* f_paren_args: '(' f_args rparen  */
#line 6108 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_args) = (yyvsp[-1].node_args);
                    /*% %*/
                    /*% ripper: paren!($2) %*/
                        SET_LEX_STATE(EXPR_BEG);
                        p->command_start = TRUE;
                        p->ctxt.in_argdef = 0;
                    }
#line 15799 "parse.c"
    break;

  case 677: /* @36: %empty  */
#line 6120 "parse.y"
                  {
                        (yyval.ctxt) = p->ctxt;
                        p->ctxt.in_kwarg = 1;
                        p->ctxt.in_argdef = 1;
                        SET_LEX_STATE(p->lex.state|EXPR_LABEL); /* force for args */
                    }
#line 15810 "parse.c"
    break;

  case 678: /* f_arglist: @36 f_args term  */
#line 6127 "parse.y"
                  {
                        p->ctxt.in_kwarg = (yyvsp[-2].ctxt).in_kwarg;
                        p->ctxt.in_argdef = 0;
                        (yyval.node_args) = (yyvsp[-1].node_args);
                        SET_LEX_STATE(EXPR_BEG);
                        p->command_start = TRUE;
                    }
#line 15822 "parse.c"
    break;

  case 679: /* args_tail: f_kwarg ',' f_kwrest opt_f_block_arg  */
#line 6137 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, (yyvsp[-3].node_kw_arg), (yyvsp[-1].id), (yyvsp[0].id), &(yylsp[-1]));
                    }
#line 15830 "parse.c"
    break;

  case 680: /* args_tail: f_kwarg opt_f_block_arg  */
#line 6141 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, (yyvsp[-1].node_kw_arg), Qnone, (yyvsp[0].id), &(yylsp[-1]));
                    }
#line 15838 "parse.c"
    break;

  case 681: /* args_tail: f_any_kwrest opt_f_block_arg  */
#line 6145 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, Qnone, (yyvsp[-1].id), (yyvsp[0].id), &(yylsp[-1]));
                    }
#line 15846 "parse.c"
    break;

  case 682: /* args_tail: f_block_arg  */
#line 6149 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, Qnone, Qnone, (yyvsp[0].id), &(yylsp[0]));
                    }
#line 15854 "parse.c"
    break;

  case 683: /* args_tail: args_forward  */
#line 6153 "parse.y"
                  {
                        add_forwarding_args(p);
                        (yyval.node_args) = new_args_tail(p, Qnone, (yyvsp[0].id), arg_FWD_BLOCK, &(yylsp[0]));
                    /*%%%*/
                        (yyval.node_args)->nd_ainfo.forwarding = 1;
                    /*% %*/
                    }
#line 15866 "parse.c"
    break;

  case 684: /* opt_args_tail: ',' args_tail  */
#line 6163 "parse.y"
                  {
                        (yyval.node_args) = (yyvsp[0].node_args);
                    }
#line 15874 "parse.c"
    break;

  case 685: /* opt_args_tail: %empty  */
#line 6167 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, Qnone, Qnone, Qnone, &(yylsp[0]));
                    }
#line 15882 "parse.c"
    break;

  case 686: /* f_args: f_arg ',' f_optarg ',' f_rest_arg opt_args_tail  */
#line 6173 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-5].node_args_aux), (yyvsp[-3].node_opt_arg), (yyvsp[-1].id), Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 15890 "parse.c"
    break;

  case 687: /* f_args: f_arg ',' f_optarg ',' f_rest_arg ',' f_arg opt_args_tail  */
#line 6177 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-7].node_args_aux), (yyvsp[-5].node_opt_arg), (yyvsp[-3].id), (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 15898 "parse.c"
    break;

  case 688: /* f_args: f_arg ',' f_optarg opt_args_tail  */
#line 6181 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-3].node_args_aux), (yyvsp[-1].node_opt_arg), Qnone, Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 15906 "parse.c"
    break;

  case 689: /* f_args: f_arg ',' f_optarg ',' f_arg opt_args_tail  */
#line 6185 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-5].node_args_aux), (yyvsp[-3].node_opt_arg), Qnone, (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 15914 "parse.c"
    break;

  case 690: /* f_args: f_arg ',' f_rest_arg opt_args_tail  */
#line 6189 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-3].node_args_aux), Qnone, (yyvsp[-1].id), Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 15922 "parse.c"
    break;

  case 691: /* f_args: f_arg ',' f_rest_arg ',' f_arg opt_args_tail  */
#line 6193 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-5].node_args_aux), Qnone, (yyvsp[-3].id), (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 15930 "parse.c"
    break;

  case 692: /* f_args: f_arg opt_args_tail  */
#line 6197 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, (yyvsp[-1].node_args_aux), Qnone, Qnone, Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 15938 "parse.c"
    break;

  case 693: /* f_args: f_optarg ',' f_rest_arg opt_args_tail  */
#line 6201 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, (yyvsp[-3].node_opt_arg), (yyvsp[-1].id), Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 15946 "parse.c"
    break;

  case 694: /* f_args: f_optarg ',' f_rest_arg ',' f_arg opt_args_tail  */
#line 6205 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, (yyvsp[-5].node_opt_arg), (yyvsp[-3].id), (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 15954 "parse.c"
    break;

  case 695: /* f_args: f_optarg opt_args_tail  */
#line 6209 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, (yyvsp[-1].node_opt_arg), Qnone, Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 15962 "parse.c"
    break;

  case 696: /* f_args: f_optarg ',' f_arg opt_args_tail  */
#line 6213 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, (yyvsp[-3].node_opt_arg), Qnone, (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 15970 "parse.c"
    break;

  case 697: /* f_args: f_rest_arg opt_args_tail  */
#line 6217 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, Qnone, (yyvsp[-1].id), Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 15978 "parse.c"
    break;

  case 698: /* f_args: f_rest_arg ',' f_arg opt_args_tail  */
#line 6221 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, Qnone, (yyvsp[-3].id), (yyvsp[-1].node_args_aux), (yyvsp[0].node_args), &(yyloc));
                    }
#line 15986 "parse.c"
    break;

  case 699: /* f_args: args_tail  */
#line 6225 "parse.y"
                  {
                        (yyval.node_args) = new_args(p, Qnone, Qnone, Qnone, Qnone, (yyvsp[0].node_args), &(yyloc));
                    }
#line 15994 "parse.c"
    break;

  case 700: /* f_args: %empty  */
#line 6229 "parse.y"
                  {
                        (yyval.node_args) = new_args_tail(p, Qnone, Qnone, Qnone, &(yylsp[0]));
                        (yyval.node_args) = new_args(p, Qnone, Qnone, Qnone, Qnone, (yyval.node_args), &(yylsp[0]));
                    }
#line 16003 "parse.c"
    break;

  case 701: /* args_forward: "(..."  */
#line 6236 "parse.y"
                  {
                    /*%%%*/
#ifdef FORWARD_ARGS_WITH_RUBY2_KEYWORDS
                        (yyval.id) = 0;
#else
                        (yyval.id) = idFWD_KWREST;
#endif
                    /*% %*/
                    /*% ripper: args_forward! %*/
                    }
#line 16018 "parse.c"
    break;

  case 702: /* f_bad_arg: "constant"  */
#line 6249 "parse.y"
                  {
                        static const char mesg[] = "formal argument cannot be a constant";
                    /*%%%*/
                        yyerror1(&(yylsp[0]), mesg);
                        (yyval.id) = 0;
                    /*% %*/
                    /*% ripper[error]: param_error!(ERR_MESG(), $1) %*/
                    }
#line 16031 "parse.c"
    break;

  case 703: /* f_bad_arg: "instance variable"  */
#line 6258 "parse.y"
                  {
                        static const char mesg[] = "formal argument cannot be an instance variable";
                    /*%%%*/
                        yyerror1(&(yylsp[0]), mesg);
                        (yyval.id) = 0;
                    /*% %*/
                    /*% ripper[error]: param_error!(ERR_MESG(), $1) %*/
                    }
#line 16044 "parse.c"
    break;

  case 704: /* f_bad_arg: "global variable"  */
#line 6267 "parse.y"
                  {
                        static const char mesg[] = "formal argument cannot be a global variable";
                    /*%%%*/
                        yyerror1(&(yylsp[0]), mesg);
                        (yyval.id) = 0;
                    /*% %*/
                    /*% ripper[error]: param_error!(ERR_MESG(), $1) %*/
                    }
#line 16057 "parse.c"
    break;

  case 705: /* f_bad_arg: "class variable"  */
#line 6276 "parse.y"
                  {
                        static const char mesg[] = "formal argument cannot be a class variable";
                    /*%%%*/
                        yyerror1(&(yylsp[0]), mesg);
                        (yyval.id) = 0;
                    /*% %*/
                    /*% ripper[error]: param_error!(ERR_MESG(), $1) %*/
                    }
#line 16070 "parse.c"
    break;

  case 707: /* f_norm_arg: "local variable or method"  */
#line 6288 "parse.y"
                  {
                        formal_argument(p, (yyvsp[0].id));
                        p->max_numparam = ORDINAL_PARAM;
                        (yyval.id) = (yyvsp[0].id);
                    }
#line 16080 "parse.c"
    break;

  case 708: /* f_arg_asgn: f_norm_arg  */
#line 6296 "parse.y"
                  {
                        ID id = get_id((yyvsp[0].id));
                        arg_var(p, id);
                        p->cur_arg = id;
                        (yyval.id) = (yyvsp[0].id);
                    }
#line 16091 "parse.c"
    break;

  case 709: /* f_arg_item: f_arg_asgn  */
#line 6305 "parse.y"
                  {
                        p->cur_arg = 0;
                    /*%%%*/
                        (yyval.node_args_aux) = NEW_ARGS_AUX((yyvsp[0].id), 1, &NULL_LOC);
                    /*% %*/
                    /*% ripper: get_value($1) %*/
                    }
#line 16103 "parse.c"
    break;

  case 710: /* f_arg_item: "(" f_margs rparen  */
#line 6313 "parse.y"
                  {
                    /*%%%*/
                        ID tid = internal_id(p);
                        YYLTYPE loc;
                        loc.beg_pos = (yylsp[-1]).beg_pos;
                        loc.end_pos = (yylsp[-1]).beg_pos;
                        arg_var(p, tid);
                        if (dyna_in_block(p)) {
                            (yyvsp[-1].node_masgn)->nd_value = NEW_DVAR(tid, &loc);
                        }
                        else {
                            (yyvsp[-1].node_masgn)->nd_value = NEW_LVAR(tid, &loc);
                        }
                        (yyval.node_args_aux) = NEW_ARGS_AUX(tid, 1, &NULL_LOC);
                        (yyval.node_args_aux)->nd_next = (NODE *)(yyvsp[-1].node_masgn);
                    /*% %*/
                    /*% ripper: mlhs_paren!($2) %*/
                    }
#line 16126 "parse.c"
    break;

  case 712: /* f_arg: f_arg ',' f_arg_item  */
#line 6336 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_args_aux) = (yyvsp[-2].node_args_aux);
                        (yyval.node_args_aux)->nd_plen++;
                        (yyval.node_args_aux)->nd_next = block_append(p, (yyval.node_args_aux)->nd_next, (yyvsp[0].node_args_aux)->nd_next);
                        rb_discard_node(p, (NODE *)(yyvsp[0].node_args_aux));
                    /*% %*/
                    /*% ripper: rb_ary_push($1, get_value($3)) %*/
                    }
#line 16140 "parse.c"
    break;

  case 713: /* f_label: "label"  */
#line 6349 "parse.y"
                  {
                        arg_var(p, formal_argument(p, (yyvsp[0].id)));
                        p->cur_arg = get_id((yyvsp[0].id));
                        p->max_numparam = ORDINAL_PARAM;
                        p->ctxt.in_argdef = 0;
                        (yyval.id) = (yyvsp[0].id);
                    }
#line 16152 "parse.c"
    break;

  case 714: /* f_kw: f_label arg_value  */
#line 6359 "parse.y"
                  {
                        p->cur_arg = 0;
                        p->ctxt.in_argdef = 1;
                    /*%%%*/
                        (yyval.node_kw_arg) = new_kw_arg(p, assignable(p, (yyvsp[-1].id), (yyvsp[0].node), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: rb_assoc_new(get_value(assignable(p, $1)), get_value($2)) %*/
                    }
#line 16165 "parse.c"
    break;

  case 715: /* f_kw: f_label  */
#line 6368 "parse.y"
                  {
                        p->cur_arg = 0;
                        p->ctxt.in_argdef = 1;
                    /*%%%*/
                        (yyval.node_kw_arg) = new_kw_arg(p, assignable(p, (yyvsp[0].id), NODE_SPECIAL_REQUIRED_KEYWORD, &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: rb_assoc_new(get_value(assignable(p, $1)), 0) %*/
                    }
#line 16178 "parse.c"
    break;

  case 716: /* f_block_kw: f_label primary_value  */
#line 6379 "parse.y"
                  {
                        p->ctxt.in_argdef = 1;
                    /*%%%*/
                        (yyval.node_kw_arg) = new_kw_arg(p, assignable(p, (yyvsp[-1].id), (yyvsp[0].node), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: rb_assoc_new(get_value(assignable(p, $1)), get_value($2)) %*/
                    }
#line 16190 "parse.c"
    break;

  case 717: /* f_block_kw: f_label  */
#line 6387 "parse.y"
                  {
                        p->ctxt.in_argdef = 1;
                    /*%%%*/
                        (yyval.node_kw_arg) = new_kw_arg(p, assignable(p, (yyvsp[0].id), NODE_SPECIAL_REQUIRED_KEYWORD, &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: rb_assoc_new(get_value(assignable(p, $1)), 0) %*/
                    }
#line 16202 "parse.c"
    break;

  case 718: /* f_block_kwarg: f_block_kw  */
#line 6397 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_kw_arg) = (yyvsp[0].node_kw_arg);
                    /*% %*/
                    /*% ripper: rb_ary_new3(1, get_value($1)) %*/
                    }
#line 16213 "parse.c"
    break;

  case 719: /* f_block_kwarg: f_block_kwarg ',' f_block_kw  */
#line 6404 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_kw_arg) = kwd_append((yyvsp[-2].node_kw_arg), (yyvsp[0].node_kw_arg));
                    /*% %*/
                    /*% ripper: rb_ary_push($1, get_value($3)) %*/
                    }
#line 16224 "parse.c"
    break;

  case 720: /* f_kwarg: f_kw  */
#line 6414 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_kw_arg) = (yyvsp[0].node_kw_arg);
                    /*% %*/
                    /*% ripper: rb_ary_new3(1, get_value($1)) %*/
                    }
#line 16235 "parse.c"
    break;

  case 721: /* f_kwarg: f_kwarg ',' f_kw  */
#line 6421 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_kw_arg) = kwd_append((yyvsp[-2].node_kw_arg), (yyvsp[0].node_kw_arg));
                    /*% %*/
                    /*% ripper: rb_ary_push($1, get_value($3)) %*/
                    }
#line 16246 "parse.c"
    break;

  case 724: /* f_no_kwarg: p_kwnorest  */
#line 6434 "parse.y"
                  {
                    /*%%%*/
                    /*% %*/
                    /*% ripper: nokw_param!(Qnil) %*/
                    }
#line 16256 "parse.c"
    break;

  case 725: /* f_kwrest: kwrest_mark "local variable or method"  */
#line 6442 "parse.y"
                  {
                        arg_var(p, shadowing_lvar(p, get_id((yyvsp[0].id))));
                    /*%%%*/
                        (yyval.id) = (yyvsp[0].id);
                    /*% %*/
                    /*% ripper: kwrest_param!($2) %*/
                    }
#line 16268 "parse.c"
    break;

  case 726: /* f_kwrest: kwrest_mark  */
#line 6450 "parse.y"
                  {
                        arg_var(p, idFWD_KWREST);
                    /*%%%*/
                        (yyval.id) = idFWD_KWREST;
                    /*% %*/
                    /*% ripper: kwrest_param!(Qnil) %*/
                    }
#line 16280 "parse.c"
    break;

  case 727: /* f_opt: f_arg_asgn f_eq arg_value  */
#line 6460 "parse.y"
                  {
                        p->cur_arg = 0;
                        p->ctxt.in_argdef = 1;
                    /*%%%*/
                        (yyval.node_opt_arg) = NEW_OPT_ARG(assignable(p, (yyvsp[-2].id), (yyvsp[0].node), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: rb_assoc_new(get_value(assignable(p, $1)), get_value($3)) %*/
                    }
#line 16293 "parse.c"
    break;

  case 728: /* f_block_opt: f_arg_asgn f_eq primary_value  */
#line 6471 "parse.y"
                  {
                        p->cur_arg = 0;
                        p->ctxt.in_argdef = 1;
                    /*%%%*/
                        (yyval.node_opt_arg) = NEW_OPT_ARG(assignable(p, (yyvsp[-2].id), (yyvsp[0].node), &(yyloc)), &(yyloc));
                    /*% %*/
                    /*% ripper: rb_assoc_new(get_value(assignable(p, $1)), get_value($3)) %*/
                    }
#line 16306 "parse.c"
    break;

  case 729: /* f_block_optarg: f_block_opt  */
#line 6482 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_opt_arg) = (yyvsp[0].node_opt_arg);
                    /*% %*/
                    /*% ripper: rb_ary_new3(1, get_value($1)) %*/
                    }
#line 16317 "parse.c"
    break;

  case 730: /* f_block_optarg: f_block_optarg ',' f_block_opt  */
#line 6489 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_opt_arg) = opt_arg_append((yyvsp[-2].node_opt_arg), (yyvsp[0].node_opt_arg));
                    /*% %*/
                    /*% ripper: rb_ary_push($1, get_value($3)) %*/
                    }
#line 16328 "parse.c"
    break;

  case 731: /* f_optarg: f_opt  */
#line 6498 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_opt_arg) = (yyvsp[0].node_opt_arg);
                    /*% %*/
                    /*% ripper: rb_ary_new3(1, get_value($1)) %*/
                    }
#line 16339 "parse.c"
    break;

  case 732: /* f_optarg: f_optarg ',' f_opt  */
#line 6505 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node_opt_arg) = opt_arg_append((yyvsp[-2].node_opt_arg), (yyvsp[0].node_opt_arg));
                    /*% %*/
                    /*% ripper: rb_ary_push($1, get_value($3)) %*/
                    }
#line 16350 "parse.c"
    break;

  case 735: /* f_rest_arg: restarg_mark "local variable or method"  */
#line 6518 "parse.y"
                  {
                        arg_var(p, shadowing_lvar(p, get_id((yyvsp[0].id))));
                    /*%%%*/
                        (yyval.id) = (yyvsp[0].id);
                    /*% %*/
                    /*% ripper: rest_param!($2) %*/
                    }
#line 16362 "parse.c"
    break;

  case 736: /* f_rest_arg: restarg_mark  */
#line 6526 "parse.y"
                  {
                        arg_var(p, idFWD_REST);
                    /*%%%*/
                        (yyval.id) = idFWD_REST;
                    /*% %*/
                    /*% ripper: rest_param!(Qnil) %*/
                    }
#line 16374 "parse.c"
    break;

  case 739: /* f_block_arg: blkarg_mark "local variable or method"  */
#line 6540 "parse.y"
                  {
                        arg_var(p, shadowing_lvar(p, get_id((yyvsp[0].id))));
                    /*%%%*/
                        (yyval.id) = (yyvsp[0].id);
                    /*% %*/
                    /*% ripper: blockarg!($2) %*/
                    }
#line 16386 "parse.c"
    break;

  case 740: /* f_block_arg: blkarg_mark  */
#line 6548 "parse.y"
                  {
                        arg_var(p, idFWD_BLOCK);
                    /*%%%*/
                        (yyval.id) = idFWD_BLOCK;
                    /*% %*/
                    /*% ripper: blockarg!(Qnil) %*/
                    }
#line 16398 "parse.c"
    break;

  case 741: /* opt_f_block_arg: ',' f_block_arg  */
#line 6558 "parse.y"
                  {
                        (yyval.id) = (yyvsp[0].id);
                    }
#line 16406 "parse.c"
    break;

  case 742: /* opt_f_block_arg: none  */
#line 6562 "parse.y"
                  {
                        (yyval.id) = Qnull;
                    }
#line 16414 "parse.c"
    break;

  case 743: /* singleton: var_ref  */
#line 6568 "parse.y"
                  {
                        value_expr((yyvsp[0].node));
                        (yyval.node) = (yyvsp[0].node);
                    }
#line 16423 "parse.c"
    break;

  case 744: /* $@37: %empty  */
#line 6572 "parse.y"
                    {SET_LEX_STATE(EXPR_BEG);}
#line 16429 "parse.c"
    break;

  case 745: /* singleton: '(' $@37 expr rparen  */
#line 6573 "parse.y"
                  {
                    /*%%%*/
                        NODE *expr = last_expr_node((yyvsp[-1].node));
                        switch (nd_type(expr)) {
                          case NODE_STR:
                          case NODE_DSTR:
                          case NODE_XSTR:
                          case NODE_DXSTR:
                          case NODE_DREGX:
                          case NODE_LIT:
                          case NODE_DSYM:
                          case NODE_LIST:
                          case NODE_ZLIST:
                            yyerror1(&expr->nd_loc, "can't define singleton method for literals");
                            break;
                          default:
                            value_expr((yyvsp[-1].node));
                            break;
                        }
                        (yyval.node) = (yyvsp[-1].node);
                    /*% %*/
                    /*% ripper: paren!($3) %*/
                    }
#line 16457 "parse.c"
    break;

  case 747: /* assoc_list: assocs trailer  */
#line 6600 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = (yyvsp[-1].node);
                    /*% %*/
                    /*% ripper: assoclist_from_args!($1) %*/
                    }
#line 16468 "parse.c"
    break;

  case 749: /* assocs: assocs ',' assoc  */
#line 6611 "parse.y"
                  {
                    /*%%%*/
                        NODE *assocs = (yyvsp[-2].node);
                        NODE *tail = (yyvsp[0].node);
                        if (!assocs) {
                            assocs = tail;
                        }
                        else if (tail) {
                            if (RNODE_LIST(assocs)->nd_head &&
                                !RNODE_LIST(tail)->nd_head && nd_type_p(RNODE_LIST(tail)->nd_next, NODE_LIST) &&
                                nd_type_p(RNODE_LIST(RNODE_LIST(tail)->nd_next)->nd_head, NODE_HASH)) {
                                /* DSTAR */
                                tail = RNODE_HASH(RNODE_LIST(RNODE_LIST(tail)->nd_next)->nd_head)->nd_head;
                            }
                            assocs = list_concat(assocs, tail);
                        }
                        (yyval.node) = assocs;
                    /*% %*/
                    /*% ripper: rb_ary_push($1, get_value($3)) %*/
                    }
#line 16493 "parse.c"
    break;

  case 750: /* assoc: arg_value "=>" arg_value  */
#line 6634 "parse.y"
                  {
                    /*%%%*/
                        if (nd_type_p((yyvsp[-2].node), NODE_STR)) {
                            nd_set_type((yyvsp[-2].node), NODE_LIT);
                            RB_OBJ_WRITE(p->ast, &RNODE_LIT((yyvsp[-2].node))->nd_lit, rb_fstring(RNODE_LIT((yyvsp[-2].node))->nd_lit));
                        }
                        (yyval.node) = list_append(p, NEW_LIST((yyvsp[-2].node), &(yyloc)), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: assoc_new!($1, $3) %*/
                    }
#line 16508 "parse.c"
    break;

  case 751: /* assoc: "label" arg_value  */
#line 6645 "parse.y"
                  {
                    /*%%%*/
                        (yyval.node) = list_append(p, NEW_LIST(NEW_LIT(ID2SYM((yyvsp[-1].id)), &(yylsp[-1])), &(yyloc)), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: assoc_new!($1, $2) %*/
                    }
#line 16519 "parse.c"
    break;

  case 752: /* assoc: "label"  */
#line 6652 "parse.y"
                  {
                    /*%%%*/
                        NODE *val = gettable(p, (yyvsp[0].id), &(yyloc));
                        if (!val) val = NEW_ERROR(&(yyloc));
                        (yyval.node) = list_append(p, NEW_LIST(NEW_LIT(ID2SYM((yyvsp[0].id)), &(yylsp[0])), &(yyloc)), val);
                    /*% %*/
                    /*% ripper: assoc_new!($1, Qnil) %*/
                    }
#line 16532 "parse.c"
    break;

  case 753: /* assoc: "string literal" string_contents tLABEL_END arg_value  */
#line 6661 "parse.y"
                  {
                    /*%%%*/
                        YYLTYPE loc = code_loc_gen(&(yylsp[-3]), &(yylsp[-1]));
                        (yyval.node) = list_append(p, NEW_LIST(dsym_node(p, (yyvsp[-2].node), &loc), &loc), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: assoc_new!(dyna_symbol!($2), $4) %*/
                    }
#line 16544 "parse.c"
    break;

  case 754: /* assoc: "**arg" arg_value  */
#line 6669 "parse.y"
                  {
                    /*%%%*/
                        if (nd_type_p((yyvsp[0].node), NODE_HASH) &&
                            !(RNODE_HASH((yyvsp[0].node))->nd_head && RNODE_LIST(RNODE_HASH((yyvsp[0].node))->nd_head)->as.nd_alen)) {
                            static VALUE empty_hash;
                            if (!empty_hash) {
                                empty_hash = rb_obj_freeze(rb_hash_new());
                                rb_gc_register_mark_object(empty_hash);
                            }
                            (yyval.node) = list_append(p, NEW_LIST(0, &(yyloc)), NEW_LIT(empty_hash, &(yyloc)));
                        }
                        else
                            (yyval.node) = list_append(p, NEW_LIST(0, &(yyloc)), (yyvsp[0].node));
                    /*% %*/
                    /*% ripper: assoc_splat!($2) %*/
                    }
#line 16565 "parse.c"
    break;

  case 755: /* assoc: "**arg"  */
#line 6686 "parse.y"
                  {
                        forwarding_arg_check(p, idFWD_KWREST, idFWD_ALL, "keyword rest");
                    /*%%%*/
                        (yyval.node) = list_append(p, NEW_LIST(0, &(yyloc)),
                                         NEW_LVAR(idFWD_KWREST, &(yyloc)));
                    /*% %*/
                    /*% ripper: assoc_splat!(Qnil) %*/
                    }
#line 16578 "parse.c"
    break;

  case 779: /* term: ';'  */
#line 6743 "parse.y"
          {yyerrok;token_flush(p);}
#line 16584 "parse.c"
    break;

  case 780: /* term: '\n'  */
#line 6745 "parse.y"
                  {
                        (yyloc).end_pos = (yyloc).beg_pos;
                        token_flush(p);
                    }
#line 16593 "parse.c"
    break;

  case 782: /* terms: terms ';'  */
#line 6752 "parse.y"
                          {yyerrok;}
#line 16599 "parse.c"
    break;

  case 783: /* none: %empty  */
#line 6756 "parse.y"
                  {
                        (yyval.node) = Qnull;
                    }
#line 16607 "parse.c"
    break;


#line 16611 "parse.c"

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
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc, p);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

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
          = {yyssp, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx, p);
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
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx, p);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (&yylloc, p, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= END_OF_INPUT)
        {
          /* Return failure if at end of input.  */
          if (yychar == END_OF_INPUT)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, p);
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
  YY_STACK_PRINT (yyss, yyssp, p);
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

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, p);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp, p);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp, p);

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
  yyerror (&yylloc, p, YY_("memory exhausted"));
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
                  yytoken, &yylval, &yylloc, p);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp, p);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, p);
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

#line 6760 "parse.y"

# undef p
# undef yylex
# undef yylval
# define yylval  (*p->lval)

static int regx_options(struct parser_params*);
static int tokadd_string(struct parser_params*,int,int,int,long*,rb_encoding**,rb_encoding**);
static void tokaddmbc(struct parser_params *p, int c, rb_encoding *enc);
static enum yytokentype parse_string(struct parser_params*,rb_strterm_literal_t*);
static enum yytokentype here_document(struct parser_params*,rb_strterm_heredoc_t*);

#ifndef RIPPER
# define set_yylval_node(x) {				\
  YYLTYPE _cur_loc;					\
  rb_parser_set_location(p, &_cur_loc);			\
  yylval.node = (x);					\
}
# define set_yylval_str(x) \
do { \
  set_yylval_node(NEW_STR(x, &_cur_loc)); \
  RB_OBJ_WRITTEN(p->ast, Qnil, x); \
} while(0)
# define set_yylval_literal(x) \
do { \
  set_yylval_node(NEW_LIT(x, &_cur_loc)); \
  RB_OBJ_WRITTEN(p->ast, Qnil, x); \
} while(0)
# define set_yylval_num(x) (yylval.num = (x))
# define set_yylval_id(x)  (yylval.id = (x))
# define set_yylval_name(x)  (yylval.id = (x))
# define yylval_id() (yylval.id)
#else
static inline VALUE
ripper_yylval_id(struct parser_params *p, ID x)
{
    return ripper_new_yylval(p, x, ID2SYM(x), 0);
}
# define set_yylval_str(x) (yylval.val = add_mark_object(p, (x)))
# define set_yylval_num(x) (yylval.val = ripper_new_yylval(p, (x), 0, 0))
# define set_yylval_id(x)  (void)(x)
# define set_yylval_name(x) (void)(yylval.val = ripper_yylval_id(p, x))
# define set_yylval_literal(x) add_mark_object(p, (x))
# define set_yylval_node(x) (yylval.val = ripper_new_yylval(p, 0, 0, STR_NEW(p->lex.ptok, p->lex.pcur-p->lex.ptok)))
# define yylval_id() yylval.id
# define _cur_loc NULL_LOC /* dummy */
#endif

#define set_yylval_noname() set_yylval_id(keyword_nil)
#define has_delayed_token(p) (!NIL_P(p->delayed.token))

#ifndef RIPPER
#define literal_flush(p, ptr) ((p)->lex.ptok = (ptr))
#define dispatch_scan_event(p, t) parser_dispatch_scan_event(p, t, __LINE__)

static bool
parser_has_token(struct parser_params *p)
{
    const char *const pcur = p->lex.pcur;
    const char *const ptok = p->lex.ptok;
    if (p->keep_tokens && (pcur < ptok)) {
        rb_bug("lex.pcur < lex.ptok. (line: %d) %"PRIdPTRDIFF"|%"PRIdPTRDIFF"|%"PRIdPTRDIFF"",
               p->ruby_sourceline, ptok - p->lex.pbeg, pcur - ptok, p->lex.pend - pcur);
    }
    return pcur > ptok;
}

static VALUE
code_loc_to_ary(struct parser_params *p, const rb_code_location_t *loc)
{
    VALUE ary = rb_ary_new_from_args(4,
        INT2NUM(loc->beg_pos.lineno), INT2NUM(loc->beg_pos.column),
        INT2NUM(loc->end_pos.lineno), INT2NUM(loc->end_pos.column));
    rb_obj_freeze(ary);

    return ary;
}

static void
parser_append_tokens(struct parser_params *p, VALUE str, enum yytokentype t, int line)
{
    VALUE ary;
    int token_id;

    ary = rb_ary_new2(4);
    token_id = p->token_id;
    rb_ary_push(ary, INT2FIX(token_id));
    rb_ary_push(ary, ID2SYM(parser_token2id(p, t)));
    rb_ary_push(ary, str);
    rb_ary_push(ary, code_loc_to_ary(p, p->yylloc));
    rb_obj_freeze(ary);
    rb_ary_push(p->tokens, ary);
    p->token_id++;

    if (p->debug) {
        rb_parser_printf(p, "Append tokens (line: %d) %"PRIsVALUE"\n", line, ary);
    }
}

static void
parser_dispatch_scan_event(struct parser_params *p, enum yytokentype t, int line)
{
    debug_token_line(p, "parser_dispatch_scan_event", line);

    if (!parser_has_token(p)) return;

    RUBY_SET_YYLLOC(*p->yylloc);

    if (p->keep_tokens) {
        VALUE str = STR_NEW(p->lex.ptok, p->lex.pcur - p->lex.ptok);
        parser_append_tokens(p, str, t, line);
    }

    token_flush(p);
}

#define dispatch_delayed_token(p, t) parser_dispatch_delayed_token(p, t, __LINE__)
static void
parser_dispatch_delayed_token(struct parser_params *p, enum yytokentype t, int line)
{
    debug_token_line(p, "parser_dispatch_delayed_token", line);

    if (!has_delayed_token(p)) return;

    RUBY_SET_YYLLOC_OF_DELAYED_TOKEN(*p->yylloc);

    if (p->keep_tokens) {
        parser_append_tokens(p, p->delayed.token, t, line);
    }

    p->delayed.token = Qnil;
}
#else
#define literal_flush(p, ptr) ((void)(ptr))

#define yylval_rval (*(RB_TYPE_P(yylval.val, T_NODE) ? &RNODE_RIPPER(yylval.node)->nd_rval : &yylval.val))

static int
ripper_has_scan_event(struct parser_params *p)
{
    if (p->lex.pcur < p->lex.ptok) rb_raise(rb_eRuntimeError, "lex.pcur < lex.ptok");
    return p->lex.pcur > p->lex.ptok;
}

static VALUE
ripper_scan_event_val(struct parser_params *p, enum yytokentype t)
{
    VALUE str = STR_NEW(p->lex.ptok, p->lex.pcur - p->lex.ptok);
    VALUE rval = ripper_dispatch1(p, ripper_token2eventid(t), str);
    RUBY_SET_YYLLOC(*p->yylloc);
    token_flush(p);
    return rval;
}

static void
ripper_dispatch_scan_event(struct parser_params *p, enum yytokentype t)
{
    if (!ripper_has_scan_event(p)) return;
    add_mark_object(p, yylval_rval = ripper_scan_event_val(p, t));
}
#define dispatch_scan_event(p, t) ripper_dispatch_scan_event(p, t)

static void
ripper_dispatch_delayed_token(struct parser_params *p, enum yytokentype t)
{
    /* save and adjust the location to delayed token for callbacks */
    int saved_line = p->ruby_sourceline;
    const char *saved_tokp = p->lex.ptok;

    if (!has_delayed_token(p)) return;
    p->ruby_sourceline = p->delayed.beg_line;
    p->lex.ptok = p->lex.pbeg + p->delayed.beg_col;
    add_mark_object(p, yylval_rval = ripper_dispatch1(p, ripper_token2eventid(t), p->delayed.token));
    p->delayed.token = Qnil;
    p->ruby_sourceline = saved_line;
    p->lex.ptok = saved_tokp;
}
#define dispatch_delayed_token(p, t) ripper_dispatch_delayed_token(p, t)
#endif /* RIPPER */

static inline int
is_identchar(struct parser_params *p, const char *ptr, const char *MAYBE_UNUSED(ptr_end), rb_encoding *enc)
{
    return rb_enc_isalnum((unsigned char)*ptr, enc) || *ptr == '_' || !ISASCII(*ptr);
}

static inline int
parser_is_identchar(struct parser_params *p)
{
    return !(p)->eofp && is_identchar(p, p->lex.pcur-1, p->lex.pend, p->enc);
}

static inline int
parser_isascii(struct parser_params *p)
{
    return ISASCII(*(p->lex.pcur-1));
}

static void
token_info_setup(token_info *ptinfo, const char *ptr, const rb_code_location_t *loc)
{
    int column = 1, nonspc = 0, i;
    for (i = 0; i < loc->beg_pos.column; i++, ptr++) {
        if (*ptr == '\t') {
            column = (((column - 1) / TAB_WIDTH) + 1) * TAB_WIDTH;
        }
        column++;
        if (*ptr != ' ' && *ptr != '\t') {
            nonspc = 1;
        }
    }

    ptinfo->beg = loc->beg_pos;
    ptinfo->indent = column;
    ptinfo->nonspc = nonspc;
}

static void
token_info_push(struct parser_params *p, const char *token, const rb_code_location_t *loc)
{
    token_info *ptinfo;

    if (!p->token_info_enabled) return;
    ptinfo = ALLOC(token_info);
    ptinfo->token = token;
    ptinfo->next = p->token_info;
    token_info_setup(ptinfo, p->lex.pbeg, loc);

    p->token_info = ptinfo;
}

static void
token_info_pop(struct parser_params *p, const char *token, const rb_code_location_t *loc)
{
    token_info *ptinfo_beg = p->token_info;

    if (!ptinfo_beg) return;
    p->token_info = ptinfo_beg->next;

    /* indentation check of matched keywords (begin..end, if..end, etc.) */
    token_info_warn(p, token, ptinfo_beg, 1, loc);
    ruby_sized_xfree(ptinfo_beg, sizeof(*ptinfo_beg));
}

static void
token_info_drop(struct parser_params *p, const char *token, rb_code_position_t beg_pos)
{
    token_info *ptinfo_beg = p->token_info;

    if (!ptinfo_beg) return;
    p->token_info = ptinfo_beg->next;

    if (ptinfo_beg->beg.lineno != beg_pos.lineno ||
        ptinfo_beg->beg.column != beg_pos.column ||
        strcmp(ptinfo_beg->token, token)) {
        compile_error(p, "token position mismatch: %d:%d:%s expected but %d:%d:%s",
                      beg_pos.lineno, beg_pos.column, token,
                      ptinfo_beg->beg.lineno, ptinfo_beg->beg.column,
                      ptinfo_beg->token);
    }

    ruby_sized_xfree(ptinfo_beg, sizeof(*ptinfo_beg));
}

static void
token_info_warn(struct parser_params *p, const char *token, token_info *ptinfo_beg, int same, const rb_code_location_t *loc)
{
    token_info ptinfo_end_body, *ptinfo_end = &ptinfo_end_body;
    if (!p->token_info_enabled) return;
    if (!ptinfo_beg) return;
    token_info_setup(ptinfo_end, p->lex.pbeg, loc);
    if (ptinfo_beg->beg.lineno == ptinfo_end->beg.lineno) return; /* ignore one-line block */
    if (ptinfo_beg->nonspc || ptinfo_end->nonspc) return; /* ignore keyword in the middle of a line */
    if (ptinfo_beg->indent == ptinfo_end->indent) return; /* the indents are matched */
    if (!same && ptinfo_beg->indent < ptinfo_end->indent) return;
    rb_warn3L(ptinfo_end->beg.lineno,
              "mismatched indentations at '%s' with '%s' at %d",
              WARN_S(token), WARN_S(ptinfo_beg->token), WARN_I(ptinfo_beg->beg.lineno));
}

static int
parser_precise_mbclen(struct parser_params *p, const char *ptr)
{
    int len = rb_enc_precise_mbclen(ptr, p->lex.pend, p->enc);
    if (!MBCLEN_CHARFOUND_P(len)) {
        compile_error(p, "invalid multibyte char (%s)", rb_enc_name(p->enc));
        return -1;
    }
    return len;
}

#ifndef RIPPER
static void ruby_show_error_line(struct parser_params *p, VALUE errbuf, const YYLTYPE *yylloc, int lineno, VALUE str);

static inline void
parser_show_error_line(struct parser_params *p, const YYLTYPE *yylloc)
{
    VALUE str;
    int lineno = p->ruby_sourceline;
    if (!yylloc) {
        return;
    }
    else if (yylloc->beg_pos.lineno == lineno) {
        str = p->lex.lastline;
    }
    else {
        return;
    }
    ruby_show_error_line(p, p->error_buffer, yylloc, lineno, str);
}

static int
parser_yyerror(struct parser_params *p, const rb_code_location_t *yylloc, const char *msg)
{
#if 0
    YYLTYPE current;

    if (!yylloc) {
        yylloc = RUBY_SET_YYLLOC(current);
    }
    else if ((p->ruby_sourceline != yylloc->beg_pos.lineno &&
              p->ruby_sourceline != yylloc->end_pos.lineno)) {
        yylloc = 0;
    }
#endif
    parser_compile_error(p, yylloc, "%s", msg);
    parser_show_error_line(p, yylloc);
    return 0;
}

static int
parser_yyerror0(struct parser_params *p, const char *msg)
{
    YYLTYPE current;
    return parser_yyerror(p, RUBY_SET_YYLLOC(current), msg);
}

static void
ruby_show_error_line(struct parser_params *p, VALUE errbuf, const YYLTYPE *yylloc, int lineno, VALUE str)
{
    VALUE mesg;
    const int max_line_margin = 30;
    const char *ptr, *ptr_end, *pt, *pb;
    const char *pre = "", *post = "", *pend;
    const char *code = "", *caret = "";
    const char *lim;
    const char *const pbeg = RSTRING_PTR(str);
    char *buf;
    long len;
    int i;

    if (!yylloc) return;
    pend = RSTRING_END(str);
    if (pend > pbeg && pend[-1] == '\n') {
        if (--pend > pbeg && pend[-1] == '\r') --pend;
    }

    pt = pend;
    if (lineno == yylloc->end_pos.lineno &&
        (pend - pbeg) > yylloc->end_pos.column) {
        pt = pbeg + yylloc->end_pos.column;
    }

    ptr = ptr_end = pt;
    lim = ptr - pbeg > max_line_margin ? ptr - max_line_margin : pbeg;
    while ((lim < ptr) && (*(ptr-1) != '\n')) ptr--;

    lim = pend - ptr_end > max_line_margin ? ptr_end + max_line_margin : pend;
    while ((ptr_end < lim) && (*ptr_end != '\n') && (*ptr_end != '\r')) ptr_end++;

    len = ptr_end - ptr;
    if (len > 4) {
        if (ptr > pbeg) {
            ptr = rb_enc_prev_char(pbeg, ptr, pt, rb_enc_get(str));
            if (ptr > pbeg) pre = "...";
        }
        if (ptr_end < pend) {
            ptr_end = rb_enc_prev_char(pt, ptr_end, pend, rb_enc_get(str));
            if (ptr_end < pend) post = "...";
        }
    }
    pb = pbeg;
    if (lineno == yylloc->beg_pos.lineno) {
        pb += yylloc->beg_pos.column;
        if (pb > pt) pb = pt;
    }
    if (pb < ptr) pb = ptr;
    if (len <= 4 && yylloc->beg_pos.lineno == yylloc->end_pos.lineno) {
        return;
    }
    if (RTEST(errbuf)) {
        mesg = rb_attr_get(errbuf, idMesg);
        if (RSTRING_LEN(mesg) > 0 && *(RSTRING_END(mesg)-1) != '\n')
            rb_str_cat_cstr(mesg, "\n");
    }
    else {
        mesg = rb_enc_str_new(0, 0, rb_enc_get(str));
    }
    if (!errbuf && rb_stderr_tty_p()) {
#define CSI_BEGIN "\033["
#define CSI_SGR "m"
        rb_str_catf(mesg,
                    CSI_BEGIN""CSI_SGR"%s" /* pre */
                    CSI_BEGIN"1"CSI_SGR"%.*s"
                    CSI_BEGIN"1;4"CSI_SGR"%.*s"
                    CSI_BEGIN";1"CSI_SGR"%.*s"
                    CSI_BEGIN""CSI_SGR"%s" /* post */
                    "\n",
                    pre,
                    (int)(pb - ptr), ptr,
                    (int)(pt - pb), pb,
                    (int)(ptr_end - pt), pt,
                    post);
    }
    else {
        char *p2;

        len = ptr_end - ptr;
        lim = pt < pend ? pt : pend;
        i = (int)(lim - ptr);
        buf = ALLOCA_N(char, i+2);
        code = ptr;
        caret = p2 = buf;
        if (ptr <= pb) {
            while (ptr < pb) {
                *p2++ = *ptr++ == '\t' ? '\t' : ' ';
            }
            *p2++ = '^';
            ptr++;
        }
        if (lim > ptr) {
            memset(p2, '~', (lim - ptr));
            p2 += (lim - ptr);
        }
        *p2 = '\0';
        rb_str_catf(mesg, "%s%.*s%s\n""%s%s\n",
                    pre, (int)len, code, post,
                    pre, caret);
    }
    if (!errbuf) rb_write_error_str(mesg);
}
#else
static int
parser_yyerror(struct parser_params *p, const YYLTYPE *yylloc, const char *msg)
{
    const char *pcur = 0, *ptok = 0;
    if (p->ruby_sourceline == yylloc->beg_pos.lineno &&
        p->ruby_sourceline == yylloc->end_pos.lineno) {
        pcur = p->lex.pcur;
        ptok = p->lex.ptok;
        p->lex.ptok = p->lex.pbeg + yylloc->beg_pos.column;
        p->lex.pcur = p->lex.pbeg + yylloc->end_pos.column;
    }
    parser_yyerror0(p, msg);
    if (pcur) {
        p->lex.ptok = ptok;
        p->lex.pcur = pcur;
    }
    return 0;
}

static int
parser_yyerror0(struct parser_params *p, const char *msg)
{
    dispatch1(parse_error, STR_NEW2(msg));
    ripper_error(p);
    return 0;
}

static inline void
parser_show_error_line(struct parser_params *p, const YYLTYPE *yylloc)
{
}
#endif /* !RIPPER */

#ifndef RIPPER
static int
vtable_size(const struct vtable *tbl)
{
    if (!DVARS_TERMINAL_P(tbl)) {
        return tbl->pos;
    }
    else {
        return 0;
    }
}
#endif

static struct vtable *
vtable_alloc_gen(struct parser_params *p, int line, struct vtable *prev)
{
    struct vtable *tbl = ALLOC(struct vtable);
    tbl->pos = 0;
    tbl->capa = 8;
    tbl->tbl = ALLOC_N(ID, tbl->capa);
    tbl->prev = prev;
#ifndef RIPPER
    if (p->debug) {
        rb_parser_printf(p, "vtable_alloc:%d: %p\n", line, (void *)tbl);
    }
#endif
    return tbl;
}
#define vtable_alloc(prev) vtable_alloc_gen(p, __LINE__, prev)

static void
vtable_free_gen(struct parser_params *p, int line, const char *name,
                struct vtable *tbl)
{
#ifndef RIPPER
    if (p->debug) {
        rb_parser_printf(p, "vtable_free:%d: %s(%p)\n", line, name, (void *)tbl);
    }
#endif
    if (!DVARS_TERMINAL_P(tbl)) {
        if (tbl->tbl) {
            ruby_sized_xfree(tbl->tbl, tbl->capa * sizeof(ID));
        }
        ruby_sized_xfree(tbl, sizeof(*tbl));
    }
}
#define vtable_free(tbl) vtable_free_gen(p, __LINE__, #tbl, tbl)

static void
vtable_add_gen(struct parser_params *p, int line, const char *name,
               struct vtable *tbl, ID id)
{
#ifndef RIPPER
    if (p->debug) {
        rb_parser_printf(p, "vtable_add:%d: %s(%p), %s\n",
                         line, name, (void *)tbl, rb_id2name(id));
    }
#endif
    if (DVARS_TERMINAL_P(tbl)) {
        rb_parser_fatal(p, "vtable_add: vtable is not allocated (%p)", (void *)tbl);
        return;
    }
    if (tbl->pos == tbl->capa) {
        tbl->capa = tbl->capa * 2;
        SIZED_REALLOC_N(tbl->tbl, ID, tbl->capa, tbl->pos);
    }
    tbl->tbl[tbl->pos++] = id;
}
#define vtable_add(tbl, id) vtable_add_gen(p, __LINE__, #tbl, tbl, id)

#ifndef RIPPER
static void
vtable_pop_gen(struct parser_params *p, int line, const char *name,
               struct vtable *tbl, int n)
{
    if (p->debug) {
        rb_parser_printf(p, "vtable_pop:%d: %s(%p), %d\n",
                         line, name, (void *)tbl, n);
    }
    if (tbl->pos < n) {
        rb_parser_fatal(p, "vtable_pop: unreachable (%d < %d)", tbl->pos, n);
        return;
    }
    tbl->pos -= n;
}
#define vtable_pop(tbl, n) vtable_pop_gen(p, __LINE__, #tbl, tbl, n)
#endif

static int
vtable_included(const struct vtable * tbl, ID id)
{
    int i;

    if (!DVARS_TERMINAL_P(tbl)) {
        for (i = 0; i < tbl->pos; i++) {
            if (tbl->tbl[i] == id) {
                return i+1;
            }
        }
    }
    return 0;
}

static void parser_prepare(struct parser_params *p);

#ifndef RIPPER
static NODE *parser_append_options(struct parser_params *p, NODE *node);

static int
e_option_supplied(struct parser_params *p)
{
    return strcmp(p->ruby_sourcefile, "-e") == 0;
}

static VALUE
yycompile0(VALUE arg)
{
    int n;
    NODE *tree;
    struct parser_params *p = (struct parser_params *)arg;
    int cov = FALSE;

    if (!compile_for_eval && !NIL_P(p->ruby_sourcefile_string)) {
        if (p->debug_lines && p->ruby_sourceline > 0) {
            VALUE str = rb_default_rs;
            n = p->ruby_sourceline;
            do {
                rb_ary_push(p->debug_lines, str);
            } while (--n);
        }

        if (!e_option_supplied(p)) {
            cov = TRUE;
        }
    }

    if (p->debug_lines) {
        RB_OBJ_WRITE(p->ast, &p->ast->body.script_lines, p->debug_lines);
    }

    parser_prepare(p);
#define RUBY_DTRACE_PARSE_HOOK(name) \
    if (RUBY_DTRACE_PARSE_##name##_ENABLED()) { \
        RUBY_DTRACE_PARSE_##name(p->ruby_sourcefile, p->ruby_sourceline); \
    }
    RUBY_DTRACE_PARSE_HOOK(BEGIN);
    n = yyparse(p);
    RUBY_DTRACE_PARSE_HOOK(END);
    p->debug_lines = 0;

    p->lex.strterm = 0;
    p->lex.pcur = p->lex.pbeg = p->lex.pend = 0;
    if (n || p->error_p) {
        VALUE mesg = p->error_buffer;
        if (!mesg) {
            mesg = syntax_error_new();
        }
        if (!p->error_tolerant) {
            rb_set_errinfo(mesg);
            return FALSE;
        }
    }
    tree = p->eval_tree;
    if (!tree) {
        tree = NEW_NIL(&NULL_LOC);
    }
    else {
        VALUE tokens = p->tokens;
        NODE *prelude;
        NODE *body = parser_append_options(p, RNODE_SCOPE(tree)->nd_body);
        prelude = block_append(p, p->eval_tree_begin, body);
        RNODE_SCOPE(tree)->nd_body = prelude;
        p->ast->body.frozen_string_literal = p->frozen_string_literal;
        p->ast->body.coverage_enabled = cov;
        if (p->keep_tokens) {
            rb_obj_freeze(tokens);
            rb_ast_set_tokens(p->ast, tokens);
        }
    }
    p->ast->body.root = tree;
    if (!p->ast->body.script_lines) p->ast->body.script_lines = INT2FIX(p->line_count);
    return (VALUE)TRUE;
}

static rb_ast_t *
yycompile(struct parser_params *p, VALUE fname, int line)
{
    rb_ast_t *ast;
    if (NIL_P(fname)) {
        p->ruby_sourcefile_string = Qnil;
        p->ruby_sourcefile = "(none)";
    }
    else {
        p->ruby_sourcefile_string = rb_fstring(fname);
        p->ruby_sourcefile = StringValueCStr(fname);
    }
    p->ruby_sourceline = line - 1;

    p->lvtbl = NULL;

    p->ast = ast = rb_ast_new();
    compile_callback(yycompile0, (VALUE)p);
    p->ast = 0;

    while (p->lvtbl) {
        local_pop(p);
    }

    return ast;
}
#endif /* !RIPPER */

static rb_encoding *
must_be_ascii_compatible(struct parser_params *p, VALUE s)
{
    rb_encoding *enc = rb_enc_get(s);
    if (!rb_enc_asciicompat(enc)) {
        rb_raise(rb_eArgError, "invalid source encoding");
    }
    return enc;
}

static VALUE
lex_get_str(struct parser_params *p, VALUE s)
{
    char *beg, *end, *start;
    long len;

    beg = RSTRING_PTR(s);
    len = RSTRING_LEN(s);
    start = beg;
    if (p->lex.gets_.ptr) {
        if (len == p->lex.gets_.ptr) return Qnil;
        beg += p->lex.gets_.ptr;
        len -= p->lex.gets_.ptr;
    }
    end = memchr(beg, '\n', len);
    if (end) len = ++end - beg;
    p->lex.gets_.ptr += len;
    return rb_str_subseq(s, beg - start, len);
}

static VALUE
lex_getline(struct parser_params *p)
{
    VALUE line = (*p->lex.gets)(p, p->lex.input);
    if (NIL_P(line)) return line;
    must_be_ascii_compatible(p, line);
    if (RB_OBJ_FROZEN(line)) line = rb_str_dup(line); // needed for RubyVM::AST.of because script_lines in iseq is deep-frozen
    p->line_count++;
    return line;
}

#ifndef RIPPER
static rb_ast_t*
parser_compile_string(rb_parser_t *p, VALUE fname, VALUE s, int line)
{
    p->lex.gets = lex_get_str;
    p->lex.gets_.ptr = 0;
    p->lex.input = rb_str_new_frozen(s);
    p->lex.pbeg = p->lex.pcur = p->lex.pend = 0;

    return yycompile(p, fname, line);
}

rb_ast_t*
rb_ruby_parser_compile_string_path(rb_parser_t *p, VALUE f, VALUE s, int line)
{
    must_be_ascii_compatible(p, s);
    return parser_compile_string(p, f, s, line);
}

rb_ast_t*
rb_ruby_parser_compile_string(rb_parser_t *p, const char *f, VALUE s, int line)
{
    return rb_ruby_parser_compile_string_path(p, rb_filesystem_str_new_cstr(f), s, line);
}

static VALUE
lex_io_gets(struct parser_params *p, VALUE io)
{
    return rb_io_gets_internal(io);
}

rb_ast_t*
rb_ruby_parser_compile_file_path(rb_parser_t *p, VALUE fname, VALUE file, int start)
{
    p->lex.gets = lex_io_gets;
    p->lex.input = file;
    p->lex.pbeg = p->lex.pcur = p->lex.pend = 0;

    return yycompile(p, fname, start);
}

static VALUE
lex_generic_gets(struct parser_params *p, VALUE input)
{
    return (*p->lex.gets_.call)(input, p->line_count);
}

rb_ast_t*
rb_ruby_parser_compile_generic(rb_parser_t *p, VALUE (*lex_gets)(VALUE, int), VALUE fname, VALUE input, int start)
{
    p->lex.gets = lex_generic_gets;
    p->lex.gets_.call = lex_gets;
    p->lex.input = input;
    p->lex.pbeg = p->lex.pcur = p->lex.pend = 0;

    return yycompile(p, fname, start);
}
#endif  /* !RIPPER */

#define STR_FUNC_ESCAPE 0x01
#define STR_FUNC_EXPAND 0x02
#define STR_FUNC_REGEXP 0x04
#define STR_FUNC_QWORDS 0x08
#define STR_FUNC_SYMBOL 0x10
#define STR_FUNC_INDENT 0x20
#define STR_FUNC_LABEL  0x40
#define STR_FUNC_LIST   0x4000
#define STR_FUNC_TERM   0x8000

enum string_type {
    str_label  = STR_FUNC_LABEL,
    str_squote = (0),
    str_dquote = (STR_FUNC_EXPAND),
    str_xquote = (STR_FUNC_EXPAND),
    str_regexp = (STR_FUNC_REGEXP|STR_FUNC_ESCAPE|STR_FUNC_EXPAND),
    str_sword  = (STR_FUNC_QWORDS|STR_FUNC_LIST),
    str_dword  = (STR_FUNC_QWORDS|STR_FUNC_EXPAND|STR_FUNC_LIST),
    str_ssym   = (STR_FUNC_SYMBOL),
    str_dsym   = (STR_FUNC_SYMBOL|STR_FUNC_EXPAND)
};

static VALUE
parser_str_new(struct parser_params *p, const char *ptr, long len, rb_encoding *enc, int func, rb_encoding *enc0)
{
    VALUE str;

    str = rb_enc_str_new(ptr, len, enc);
    if (!(func & STR_FUNC_REGEXP) && rb_enc_asciicompat(enc)) {
        if (is_ascii_string(str)) {
        }
        else if (rb_is_usascii_enc((void *)enc0) && enc != rb_utf8_encoding()) {
            rb_enc_associate(str, rb_ascii8bit_encoding());
        }
    }

    return str;
}

static int
strterm_is_heredoc(rb_strterm_t *strterm)
{
    return strterm->flags & STRTERM_HEREDOC;
}

static rb_strterm_t *
new_strterm(struct parser_params *p, int func, int term, int paren)
{
    rb_strterm_t *strterm = ZALLOC(rb_strterm_t);
    strterm->u.literal.func = func;
    strterm->u.literal.term = term;
    strterm->u.literal.paren = paren;
    return strterm;
}

static rb_strterm_t *
new_heredoc(struct parser_params *p)
{
    rb_strterm_t *strterm = ZALLOC(rb_strterm_t);
    strterm->flags |= STRTERM_HEREDOC;
    return strterm;
}

#define peek(p,c) peek_n(p, (c), 0)
#define peek_n(p,c,n) (!lex_eol_n_p(p, n) && (c) == (unsigned char)(p)->lex.pcur[n])
#define peekc(p) peekc_n(p, 0)
#define peekc_n(p,n) (lex_eol_n_p(p, n) ? -1 : (unsigned char)(p)->lex.pcur[n])

static void
add_delayed_token(struct parser_params *p, const char *tok, const char *end, int line)
{
#ifndef RIPPER
    debug_token_line(p, "add_delayed_token", line);
#endif

    if (tok < end) {
        if (has_delayed_token(p)) {
            bool next_line = end_with_newline_p(p, p->delayed.token);
            int end_line = (next_line ? 1 : 0) + p->delayed.end_line;
            int end_col = (next_line ? 0 : p->delayed.end_col);
            if (end_line != p->ruby_sourceline || end_col != tok - p->lex.pbeg) {
                dispatch_delayed_token(p, tSTRING_CONTENT);
            }
        }
        if (!has_delayed_token(p)) {
            p->delayed.token = rb_str_buf_new(end - tok);
            rb_enc_associate(p->delayed.token, p->enc);
            p->delayed.beg_line = p->ruby_sourceline;
            p->delayed.beg_col = rb_long2int(tok - p->lex.pbeg);
        }
        rb_str_buf_cat(p->delayed.token, tok, end - tok);
        p->delayed.end_line = p->ruby_sourceline;
        p->delayed.end_col = rb_long2int(end - p->lex.pbeg);
        p->lex.ptok = end;
    }
}

static void
set_lastline(struct parser_params *p, VALUE v)
{
    p->lex.pbeg = p->lex.pcur = RSTRING_PTR(v);
    p->lex.pend = p->lex.pcur + RSTRING_LEN(v);
    p->lex.lastline = v;
}

static int
nextline(struct parser_params *p, int set_encoding)
{
    VALUE v = p->lex.nextline;
    p->lex.nextline = 0;
    if (!v) {
        if (p->eofp)
            return -1;

        if (!lex_eol_ptr_p(p, p->lex.pbeg) && *(p->lex.pend-1) != '\n') {
            goto end_of_input;
        }

        if (!p->lex.input || NIL_P(v = lex_getline(p))) {
          end_of_input:
            p->eofp = 1;
            lex_goto_eol(p);
            return -1;
        }
#ifndef RIPPER
        if (p->debug_lines) {
            if (set_encoding) rb_enc_associate(v, p->enc);
            rb_ary_push(p->debug_lines, v);
        }
#endif
        p->cr_seen = FALSE;
    }
    else if (NIL_P(v)) {
        /* after here-document without terminator */
        goto end_of_input;
    }
    add_delayed_token(p, p->lex.ptok, p->lex.pend, __LINE__);
    if (p->heredoc_end > 0) {
        p->ruby_sourceline = p->heredoc_end;
        p->heredoc_end = 0;
    }
    p->ruby_sourceline++;
    set_lastline(p, v);
    token_flush(p);
    return 0;
}

static int
parser_cr(struct parser_params *p, int c)
{
    if (peek(p, '\n')) {
        p->lex.pcur++;
        c = '\n';
    }
    return c;
}

static inline int
nextc0(struct parser_params *p, int set_encoding)
{
    int c;

    if (UNLIKELY(lex_eol_p(p) || p->eofp || RTEST(p->lex.nextline))) {
        if (nextline(p, set_encoding)) return -1;
    }
    c = (unsigned char)*p->lex.pcur++;
    if (UNLIKELY(c == '\r')) {
        c = parser_cr(p, c);
    }

    return c;
}
#define nextc(p) nextc0(p, TRUE)

static void
pushback(struct parser_params *p, int c)
{
    if (c == -1) return;
    p->eofp = 0;
    p->lex.pcur--;
    if (p->lex.pcur > p->lex.pbeg && p->lex.pcur[0] == '\n' && p->lex.pcur[-1] == '\r') {
        p->lex.pcur--;
    }
}

#define was_bol(p) ((p)->lex.pcur == (p)->lex.pbeg + 1)

#define tokfix(p) ((p)->tokenbuf[(p)->tokidx]='\0')
#define tok(p) (p)->tokenbuf
#define toklen(p) (p)->tokidx

static int
looking_at_eol_p(struct parser_params *p)
{
    const char *ptr = p->lex.pcur;
    while (!lex_eol_ptr_p(p, ptr)) {
        int c = (unsigned char)*ptr++;
        int eol = (c == '\n' || c == '#');
        if (eol || !ISSPACE(c)) {
            return eol;
        }
    }
    return TRUE;
}

static char*
newtok(struct parser_params *p)
{
    p->tokidx = 0;
    if (!p->tokenbuf) {
        p->toksiz = 60;
        p->tokenbuf = ALLOC_N(char, 60);
    }
    if (p->toksiz > 4096) {
        p->toksiz = 60;
        REALLOC_N(p->tokenbuf, char, 60);
    }
    return p->tokenbuf;
}

static char *
tokspace(struct parser_params *p, int n)
{
    p->tokidx += n;

    if (p->tokidx >= p->toksiz) {
        do {p->toksiz *= 2;} while (p->toksiz < p->tokidx);
        REALLOC_N(p->tokenbuf, char, p->toksiz);
    }
    return &p->tokenbuf[p->tokidx-n];
}

static void
tokadd(struct parser_params *p, int c)
{
    p->tokenbuf[p->tokidx++] = (char)c;
    if (p->tokidx >= p->toksiz) {
        p->toksiz *= 2;
        REALLOC_N(p->tokenbuf, char, p->toksiz);
    }
}

static int
tok_hex(struct parser_params *p, size_t *numlen)
{
    int c;

    c = (int)ruby_scan_hex(p->lex.pcur, 2, numlen);
    if (!*numlen) {
        yyerror0("invalid hex escape");
        dispatch_scan_event(p, tSTRING_CONTENT);
        return 0;
    }
    p->lex.pcur += *numlen;
    return c;
}

#define tokcopy(p, n) memcpy(tokspace(p, n), (p)->lex.pcur - (n), (n))

static int
escaped_control_code(int c)
{
    int c2 = 0;
    switch (c) {
      case ' ':
        c2 = 's';
        break;
      case '\n':
        c2 = 'n';
        break;
      case '\t':
        c2 = 't';
        break;
      case '\v':
        c2 = 'v';
        break;
      case '\r':
        c2 = 'r';
        break;
      case '\f':
        c2 = 'f';
        break;
    }
    return c2;
}

#define WARN_SPACE_CHAR(c, prefix) \
    rb_warn1("invalid character syntax; use "prefix"\\%c", WARN_I(c2))

static int
tokadd_codepoint(struct parser_params *p, rb_encoding **encp,
                 int regexp_literal, int wide)
{
    size_t numlen;
    int codepoint = (int)ruby_scan_hex(p->lex.pcur, wide ? p->lex.pend - p->lex.pcur : 4, &numlen);
    p->lex.pcur += numlen;
    if (p->lex.strterm == NULL ||
        strterm_is_heredoc(p->lex.strterm) ||
        (p->lex.strterm->u.literal.func != str_regexp)) {
        if (wide ? (numlen == 0 || numlen > 6) : (numlen < 4))  {
            literal_flush(p, p->lex.pcur);
            yyerror0("invalid Unicode escape");
            return wide && numlen > 0;
        }
        if (codepoint > 0x10ffff) {
            literal_flush(p, p->lex.pcur);
            yyerror0("invalid Unicode codepoint (too large)");
            return wide;
        }
        if ((codepoint & 0xfffff800) == 0xd800) {
            literal_flush(p, p->lex.pcur);
            yyerror0("invalid Unicode codepoint");
            return wide;
        }
    }
    if (regexp_literal) {
        tokcopy(p, (int)numlen);
    }
    else if (codepoint >= 0x80) {
        rb_encoding *utf8 = rb_utf8_encoding();
        if (*encp && utf8 != *encp) {
            YYLTYPE loc = RUBY_INIT_YYLLOC();
            compile_error(p, "UTF-8 mixed within %s source", rb_enc_name(*encp));
            parser_show_error_line(p, &loc);
            return wide;
        }
        *encp = utf8;
        tokaddmbc(p, codepoint, *encp);
    }
    else {
        tokadd(p, codepoint);
    }
    return TRUE;
}

static int tokadd_mbchar(struct parser_params *p, int c);

static int
tokskip_mbchar(struct parser_params *p)
{
    int len = parser_precise_mbclen(p, p->lex.pcur-1);
    if (len > 0) {
        p->lex.pcur += len - 1;
    }
    return len;
}

/* return value is for ?\u3042 */
static void
tokadd_utf8(struct parser_params *p, rb_encoding **encp,
            int term, int symbol_literal, int regexp_literal)
{
    /*
     * If `term` is not -1, then we allow multiple codepoints in \u{}
     * upto `term` byte, otherwise we're parsing a character literal.
     * And then add the codepoints to the current token.
     */
    static const char multiple_codepoints[] = "Multiple codepoints at single character literal";

    const int open_brace = '{', close_brace = '}';

    if (regexp_literal) { tokadd(p, '\\'); tokadd(p, 'u'); }

    if (peek(p, open_brace)) {  /* handle \u{...} form */
        if (regexp_literal && p->lex.strterm->u.literal.func == str_regexp) {
            /*
             * Skip parsing validation code and copy bytes as-is until term or
             * closing brace, in order to correctly handle extended regexps where
             * invalid unicode escapes are allowed in comments. The regexp parser
             * does its own validation and will catch any issues.
             */
            tokadd(p, open_brace);
            while (!lex_eol_ptr_p(p, ++p->lex.pcur)) {
                int c = peekc(p);
                if (c == close_brace) {
                    tokadd(p, c);
                    ++p->lex.pcur;
                    break;
                }
                else if (c == term) {
                    break;
                }
                if (c == '\\' && !lex_eol_n_p(p, 1)) {
                    tokadd(p, c);
                    c = *++p->lex.pcur;
                }
                tokadd_mbchar(p, c);
            }
        }
        else {
            const char *second = NULL;
            int c, last = nextc(p);
            if (lex_eol_p(p)) goto unterminated;
            while (ISSPACE(c = peekc(p)) && !lex_eol_ptr_p(p, ++p->lex.pcur));
            while (c != close_brace) {
                if (c == term) goto unterminated;
                if (second == multiple_codepoints)
                    second = p->lex.pcur;
                if (regexp_literal) tokadd(p, last);
                if (!tokadd_codepoint(p, encp, regexp_literal, TRUE)) {
                    break;
                }
                while (ISSPACE(c = peekc(p))) {
                    if (lex_eol_ptr_p(p, ++p->lex.pcur)) goto unterminated;
                    last = c;
                }
                if (term == -1 && !second)
                    second = multiple_codepoints;
            }

            if (c != close_brace) {
              unterminated:
                token_flush(p);
                yyerror0("unterminated Unicode escape");
                return;
            }
            if (second && second != multiple_codepoints) {
                const char *pcur = p->lex.pcur;
                p->lex.pcur = second;
                dispatch_scan_event(p, tSTRING_CONTENT);
                token_flush(p);
                p->lex.pcur = pcur;
                yyerror0(multiple_codepoints);
                token_flush(p);
            }

            if (regexp_literal) tokadd(p, close_brace);
            nextc(p);
        }
    }
    else {			/* handle \uxxxx form */
        if (!tokadd_codepoint(p, encp, regexp_literal, FALSE)) {
            token_flush(p);
            return;
        }
    }
}

#define ESCAPE_CONTROL 1
#define ESCAPE_META    2

static int
read_escape(struct parser_params *p, int flags)
{
    int c;
    size_t numlen;

    switch (c = nextc(p)) {
      case '\\':	/* Backslash */
        return c;

      case 'n':	/* newline */
        return '\n';

      case 't':	/* horizontal tab */
        return '\t';

      case 'r':	/* carriage-return */
        return '\r';

      case 'f':	/* form-feed */
        return '\f';

      case 'v':	/* vertical tab */
        return '\13';

      case 'a':	/* alarm(bell) */
        return '\007';

      case 'e':	/* escape */
        return 033;

      case '0': case '1': case '2': case '3': /* octal constant */
      case '4': case '5': case '6': case '7':
        pushback(p, c);
        c = (int)ruby_scan_oct(p->lex.pcur, 3, &numlen);
        p->lex.pcur += numlen;
        return c;

      case 'x':	/* hex constant */
        c = tok_hex(p, &numlen);
        if (numlen == 0) return 0;
        return c;

      case 'b':	/* backspace */
        return '\010';

      case 's':	/* space */
        return ' ';

      case 'M':
        if (flags & ESCAPE_META) goto eof;
        if ((c = nextc(p)) != '-') {
            goto eof;
        }
        if ((c = nextc(p)) == '\\') {
            switch (peekc(p)) {
              case 'u': case 'U':
                nextc(p);
                goto eof;
            }
            return read_escape(p, flags|ESCAPE_META) | 0x80;
        }
        else if (c == -1) goto eof;
        else if (!ISASCII(c)) {
            tokskip_mbchar(p);
            goto eof;
        }
        else {
            int c2 = escaped_control_code(c);
            if (c2) {
                if (ISCNTRL(c) || !(flags & ESCAPE_CONTROL)) {
                    WARN_SPACE_CHAR(c2, "\\M-");
                }
                else {
                    WARN_SPACE_CHAR(c2, "\\C-\\M-");
                }
            }
            else if (ISCNTRL(c)) goto eof;
            return ((c & 0xff) | 0x80);
        }

      case 'C':
        if ((c = nextc(p)) != '-') {
            goto eof;
        }
      case 'c':
        if (flags & ESCAPE_CONTROL) goto eof;
        if ((c = nextc(p))== '\\') {
            switch (peekc(p)) {
              case 'u': case 'U':
                nextc(p);
                goto eof;
            }
            c = read_escape(p, flags|ESCAPE_CONTROL);
        }
        else if (c == '?')
            return 0177;
        else if (c == -1) goto eof;
        else if (!ISASCII(c)) {
            tokskip_mbchar(p);
            goto eof;
        }
        else {
            int c2 = escaped_control_code(c);
            if (c2) {
                if (ISCNTRL(c)) {
                    if (flags & ESCAPE_META) {
                        WARN_SPACE_CHAR(c2, "\\M-");
                    }
                    else {
                        WARN_SPACE_CHAR(c2, "");
                    }
                }
                else {
                    if (flags & ESCAPE_META) {
                        WARN_SPACE_CHAR(c2, "\\M-\\C-");
                    }
                    else {
                        WARN_SPACE_CHAR(c2, "\\C-");
                    }
                }
            }
            else if (ISCNTRL(c)) goto eof;
        }
        return c & 0x9f;

      eof:
      case -1:
        yyerror0("Invalid escape character syntax");
        dispatch_scan_event(p, tSTRING_CONTENT);
        return '\0';

      default:
        return c;
    }
}

static void
tokaddmbc(struct parser_params *p, int c, rb_encoding *enc)
{
    int len = rb_enc_codelen(c, enc);
    rb_enc_mbcput(c, tokspace(p, len), enc);
}

static int
tokadd_escape(struct parser_params *p)
{
    int c;
    size_t numlen;

    switch (c = nextc(p)) {
      case '\n':
        return 0;		/* just ignore */

      case '0': case '1': case '2': case '3': /* octal constant */
      case '4': case '5': case '6': case '7':
        {
            ruby_scan_oct(--p->lex.pcur, 3, &numlen);
            if (numlen == 0) goto eof;
            p->lex.pcur += numlen;
            tokcopy(p, (int)numlen + 1);
        }
        return 0;

      case 'x':	/* hex constant */
        {
            tok_hex(p, &numlen);
            if (numlen == 0) return -1;
            tokcopy(p, (int)numlen + 2);
        }
        return 0;

      eof:
      case -1:
        yyerror0("Invalid escape character syntax");
        token_flush(p);
        return -1;

      default:
        tokadd(p, '\\');
        tokadd(p, c);
    }
    return 0;
}

static int
regx_options(struct parser_params *p)
{
    int kcode = 0;
    int kopt = 0;
    int options = 0;
    int c, opt, kc;

    newtok(p);
    while (c = nextc(p), ISALPHA(c)) {
        if (c == 'o') {
            options |= RE_OPTION_ONCE;
        }
        else if (rb_char_to_option_kcode(c, &opt, &kc)) {
            if (kc >= 0) {
                if (kc != rb_ascii8bit_encindex()) kcode = c;
                kopt = opt;
            }
            else {
                options |= opt;
            }
        }
        else {
            tokadd(p, c);
        }
    }
    options |= kopt;
    pushback(p, c);
    if (toklen(p)) {
        YYLTYPE loc = RUBY_INIT_YYLLOC();
        tokfix(p);
        compile_error(p, "unknown regexp option%s - %*s",
                      toklen(p) > 1 ? "s" : "", toklen(p), tok(p));
        parser_show_error_line(p, &loc);
    }
    return options | RE_OPTION_ENCODING(kcode);
}

static int
tokadd_mbchar(struct parser_params *p, int c)
{
    int len = parser_precise_mbclen(p, p->lex.pcur-1);
    if (len < 0) return -1;
    tokadd(p, c);
    p->lex.pcur += --len;
    if (len > 0) tokcopy(p, len);
    return c;
}

static inline int
simple_re_meta(int c)
{
    switch (c) {
      case '$': case '*': case '+': case '.':
      case '?': case '^': case '|':
      case ')': case ']': case '}': case '>':
        return TRUE;
      default:
        return FALSE;
    }
}

static int
parser_update_heredoc_indent(struct parser_params *p, int c)
{
    if (p->heredoc_line_indent == -1) {
        if (c == '\n') p->heredoc_line_indent = 0;
    }
    else {
        if (c == ' ') {
            p->heredoc_line_indent++;
            return TRUE;
        }
        else if (c == '\t') {
            int w = (p->heredoc_line_indent / TAB_WIDTH) + 1;
            p->heredoc_line_indent = w * TAB_WIDTH;
            return TRUE;
        }
        else if (c != '\n') {
            if (p->heredoc_indent > p->heredoc_line_indent) {
                p->heredoc_indent = p->heredoc_line_indent;
            }
            p->heredoc_line_indent = -1;
        }
    }
    return FALSE;
}

static void
parser_mixed_error(struct parser_params *p, rb_encoding *enc1, rb_encoding *enc2)
{
    YYLTYPE loc = RUBY_INIT_YYLLOC();
    const char *n1 = rb_enc_name(enc1), *n2 = rb_enc_name(enc2);
    compile_error(p, "%s mixed within %s source", n1, n2);
    parser_show_error_line(p, &loc);
}

static void
parser_mixed_escape(struct parser_params *p, const char *beg, rb_encoding *enc1, rb_encoding *enc2)
{
    const char *pos = p->lex.pcur;
    p->lex.pcur = beg;
    parser_mixed_error(p, enc1, enc2);
    p->lex.pcur = pos;
}

static inline char
nibble_char_upper(unsigned int c)
{
    c &= 0xf;
    return c + (c < 10 ? '0' : 'A' - 10);
}

static int
tokadd_string(struct parser_params *p,
              int func, int term, int paren, long *nest,
              rb_encoding **encp, rb_encoding **enc)
{
    int c;
    bool erred = false;
#ifdef RIPPER
    const int heredoc_end = (p->heredoc_end ? p->heredoc_end + 1 : 0);
    int top_of_line = FALSE;
#endif

#define mixed_error(enc1, enc2) \
    (void)(erred || (parser_mixed_error(p, enc1, enc2), erred = true))
#define mixed_escape(beg, enc1, enc2) \
    (void)(erred || (parser_mixed_escape(p, beg, enc1, enc2), erred = true))

    while ((c = nextc(p)) != -1) {
        if (p->heredoc_indent > 0) {
            parser_update_heredoc_indent(p, c);
        }
#ifdef RIPPER
        if (top_of_line && heredoc_end == p->ruby_sourceline) {
            pushback(p, c);
            break;
        }
#endif

        if (paren && c == paren) {
            ++*nest;
        }
        else if (c == term) {
            if (!nest || !*nest) {
                pushback(p, c);
                break;
            }
            --*nest;
        }
        else if ((func & STR_FUNC_EXPAND) && c == '#' && !lex_eol_p(p)) {
            unsigned char c2 = *p->lex.pcur;
            if (c2 == '$' || c2 == '@' || c2 == '{') {
                pushback(p, c);
                break;
            }
        }
        else if (c == '\\') {
            c = nextc(p);
            switch (c) {
              case '\n':
                if (func & STR_FUNC_QWORDS) break;
                if (func & STR_FUNC_EXPAND) {
                    if (!(func & STR_FUNC_INDENT) || (p->heredoc_indent < 0))
                        continue;
                    if (c == term) {
                        c = '\\';
                        goto terminate;
                    }
                }
                tokadd(p, '\\');
                break;

              case '\\':
                if (func & STR_FUNC_ESCAPE) tokadd(p, c);
                break;

              case 'u':
                if ((func & STR_FUNC_EXPAND) == 0) {
                    tokadd(p, '\\');
                    break;
                }
                tokadd_utf8(p, enc, term,
                            func & STR_FUNC_SYMBOL,
                            func & STR_FUNC_REGEXP);
                continue;

              default:
                if (c == -1) return -1;
                if (!ISASCII(c)) {
                    if ((func & STR_FUNC_EXPAND) == 0) tokadd(p, '\\');
                    goto non_ascii;
                }
                if (func & STR_FUNC_REGEXP) {
                    switch (c) {
                      case 'c':
                      case 'C':
                      case 'M': {
                        pushback(p, c);
                        c = read_escape(p, 0);

                        char *t = tokspace(p, rb_strlen_lit("\\x00"));
                        *t++ = '\\';
                        *t++ = 'x';
                        *t++ = nibble_char_upper(c >> 4);
                        *t++ = nibble_char_upper(c);
                        continue;
                      }
                    }

                    if (c == term && !simple_re_meta(c)) {
                        tokadd(p, c);
                        continue;
                    }
                    pushback(p, c);
                    if ((c = tokadd_escape(p)) < 0)
                        return -1;
                    if (*enc && *enc != *encp) {
                        mixed_escape(p->lex.ptok+2, *enc, *encp);
                    }
                    continue;
                }
                else if (func & STR_FUNC_EXPAND) {
                    pushback(p, c);
                    if (func & STR_FUNC_ESCAPE) tokadd(p, '\\');
                    c = read_escape(p, 0);
                }
                else if ((func & STR_FUNC_QWORDS) && ISSPACE(c)) {
                    /* ignore backslashed spaces in %w */
                }
                else if (c != term && !(paren && c == paren)) {
                    tokadd(p, '\\');
                    pushback(p, c);
                    continue;
                }
            }
        }
        else if (!parser_isascii(p)) {
          non_ascii:
            if (!*enc) {
                *enc = *encp;
            }
            else if (*enc != *encp) {
                mixed_error(*enc, *encp);
                continue;
            }
            if (tokadd_mbchar(p, c) == -1) return -1;
            continue;
        }
        else if ((func & STR_FUNC_QWORDS) && ISSPACE(c)) {
            pushback(p, c);
            break;
        }
        if (c & 0x80) {
            if (!*enc) {
                *enc = *encp;
            }
            else if (*enc != *encp) {
                mixed_error(*enc, *encp);
                continue;
            }
        }
        tokadd(p, c);
#ifdef RIPPER
        top_of_line = (c == '\n');
#endif
    }
  terminate:
    if (*enc) *encp = *enc;
    return c;
}

#define NEW_STRTERM(func, term, paren) new_strterm(p, func, term, paren)

#ifdef RIPPER
static void
flush_string_content(struct parser_params *p, rb_encoding *enc)
{
    VALUE content = yylval.val;
    if (!ripper_is_node_yylval(p, content))
        content = ripper_new_yylval(p, 0, 0, content);
    if (has_delayed_token(p)) {
        ptrdiff_t len = p->lex.pcur - p->lex.ptok;
        if (len > 0) {
            rb_enc_str_buf_cat(p->delayed.token, p->lex.ptok, len, enc);
        }
        dispatch_delayed_token(p, tSTRING_CONTENT);
        p->lex.ptok = p->lex.pcur;
        RNODE_RIPPER(content)->nd_rval = yylval.val;
    }
    dispatch_scan_event(p, tSTRING_CONTENT);
    if (yylval.val != content)
        RNODE_RIPPER(content)->nd_rval = yylval.val;
    yylval.val = content;
}
#else
static void
flush_string_content(struct parser_params *p, rb_encoding *enc)
{
    if (has_delayed_token(p)) {
        ptrdiff_t len = p->lex.pcur - p->lex.ptok;
        if (len > 0) {
            rb_enc_str_buf_cat(p->delayed.token, p->lex.ptok, len, enc);
            p->delayed.end_line = p->ruby_sourceline;
            p->delayed.end_col = rb_long2int(p->lex.pcur - p->lex.pbeg);
        }
        dispatch_delayed_token(p, tSTRING_CONTENT);
        p->lex.ptok = p->lex.pcur;
    }
    dispatch_scan_event(p, tSTRING_CONTENT);
}
#endif

RUBY_FUNC_EXPORTED const uint_least32_t ruby_global_name_punct_bits[(0x7e - 0x20 + 31) / 32];
/* this can be shared with ripper, since it's independent from struct
 * parser_params. */
#ifndef RIPPER
#define BIT(c, idx) (((c) / 32 - 1 == idx) ? (1U << ((c) % 32)) : 0)
#define SPECIAL_PUNCT(idx) ( \
        BIT('~', idx) | BIT('*', idx) | BIT('$', idx) | BIT('?', idx) | \
        BIT('!', idx) | BIT('@', idx) | BIT('/', idx) | BIT('\\', idx) | \
        BIT(';', idx) | BIT(',', idx) | BIT('.', idx) | BIT('=', idx) | \
        BIT(':', idx) | BIT('<', idx) | BIT('>', idx) | BIT('\"', idx) | \
        BIT('&', idx) | BIT('`', idx) | BIT('\'', idx) | BIT('+', idx) | \
        BIT('0', idx))
const uint_least32_t ruby_global_name_punct_bits[] = {
    SPECIAL_PUNCT(0),
    SPECIAL_PUNCT(1),
    SPECIAL_PUNCT(2),
};
#undef BIT
#undef SPECIAL_PUNCT
#endif

static enum yytokentype
parser_peek_variable_name(struct parser_params *p)
{
    int c;
    const char *ptr = p->lex.pcur;

    if (lex_eol_ptr_n_p(p, ptr, 1)) return 0;
    c = *ptr++;
    switch (c) {
      case '$':
        if ((c = *ptr) == '-') {
            if (lex_eol_ptr_p(p, ++ptr)) return 0;
            c = *ptr;
        }
        else if (is_global_name_punct(c) || ISDIGIT(c)) {
            return tSTRING_DVAR;
        }
        break;
      case '@':
        if ((c = *ptr) == '@') {
            if (lex_eol_ptr_p(p, ++ptr)) return 0;
            c = *ptr;
        }
        break;
      case '{':
        p->lex.pcur = ptr;
        p->command_start = TRUE;
        return tSTRING_DBEG;
      default:
        return 0;
    }
    if (!ISASCII(c) || c == '_' || ISALPHA(c))
        return tSTRING_DVAR;
    return 0;
}

#define IS_ARG() IS_lex_state(EXPR_ARG_ANY)
#define IS_END() IS_lex_state(EXPR_END_ANY)
#define IS_BEG() (IS_lex_state(EXPR_BEG_ANY) || IS_lex_state_all(EXPR_ARG|EXPR_LABELED))
#define IS_SPCARG(c) (IS_ARG() && space_seen && !ISSPACE(c))
#define IS_LABEL_POSSIBLE() (\
        (IS_lex_state(EXPR_LABEL|EXPR_ENDFN) && !cmd_state) || \
        IS_ARG())
#define IS_LABEL_SUFFIX(n) (peek_n(p, ':',(n)) && !peek_n(p, ':', (n)+1))
#define IS_AFTER_OPERATOR() IS_lex_state(EXPR_FNAME | EXPR_DOT)

static inline enum yytokentype
parser_string_term(struct parser_params *p, int func)
{
    xfree(p->lex.strterm);
    p->lex.strterm = 0;
    if (func & STR_FUNC_REGEXP) {
        set_yylval_num(regx_options(p));
        dispatch_scan_event(p, tREGEXP_END);
        SET_LEX_STATE(EXPR_END);
        return tREGEXP_END;
    }
    if ((func & STR_FUNC_LABEL) && IS_LABEL_SUFFIX(0)) {
        nextc(p);
        SET_LEX_STATE(EXPR_ARG|EXPR_LABELED);
        return tLABEL_END;
    }
    SET_LEX_STATE(EXPR_END);
    return tSTRING_END;
}

static enum yytokentype
parse_string(struct parser_params *p, rb_strterm_literal_t *quote)
{
    int func = quote->func;
    int term = quote->term;
    int paren = quote->paren;
    int c, space = 0;
    rb_encoding *enc = p->enc;
    rb_encoding *base_enc = 0;
    VALUE lit;

    if (func & STR_FUNC_TERM) {
        if (func & STR_FUNC_QWORDS) nextc(p); /* delayed term */
        SET_LEX_STATE(EXPR_END);
        xfree(p->lex.strterm);
        p->lex.strterm = 0;
        return func & STR_FUNC_REGEXP ? tREGEXP_END : tSTRING_END;
    }
    c = nextc(p);
    if ((func & STR_FUNC_QWORDS) && ISSPACE(c)) {
        while (c != '\n' && ISSPACE(c = nextc(p)));
        space = 1;
    }
    if (func & STR_FUNC_LIST) {
        quote->func &= ~STR_FUNC_LIST;
        space = 1;
    }
    if (c == term && !quote->nest) {
        if (func & STR_FUNC_QWORDS) {
            quote->func |= STR_FUNC_TERM;
            pushback(p, c); /* dispatch the term at tSTRING_END */
            add_delayed_token(p, p->lex.ptok, p->lex.pcur, __LINE__);
            return ' ';
        }
        return parser_string_term(p, func);
    }
    if (space) {
        if (!ISSPACE(c)) pushback(p, c);
        add_delayed_token(p, p->lex.ptok, p->lex.pcur, __LINE__);
        return ' ';
    }
    newtok(p);
    if ((func & STR_FUNC_EXPAND) && c == '#') {
        enum yytokentype t = parser_peek_variable_name(p);
        if (t) return t;
        tokadd(p, '#');
        c = nextc(p);
    }
    pushback(p, c);
    if (tokadd_string(p, func, term, paren, &quote->nest,
                      &enc, &base_enc) == -1) {
        if (p->eofp) {
#ifndef RIPPER
# define unterminated_literal(mesg) yyerror0(mesg)
#else
# define unterminated_literal(mesg) compile_error(p,  mesg)
#endif
            literal_flush(p, p->lex.pcur);
            if (func & STR_FUNC_QWORDS) {
                /* no content to add, bailing out here */
                unterminated_literal("unterminated list meets end of file");
                xfree(p->lex.strterm);
                p->lex.strterm = 0;
                return tSTRING_END;
            }
            if (func & STR_FUNC_REGEXP) {
                unterminated_literal("unterminated regexp meets end of file");
            }
            else {
                unterminated_literal("unterminated string meets end of file");
            }
            quote->func |= STR_FUNC_TERM;
        }
    }

    tokfix(p);
    lit = STR_NEW3(tok(p), toklen(p), enc, func);
    set_yylval_str(lit);
    flush_string_content(p, enc);

    return tSTRING_CONTENT;
}

static enum yytokentype
heredoc_identifier(struct parser_params *p)
{
    /*
     * term_len is length of `<<"END"` except `END`,
     * in this case term_len is 4 (<, <, " and ").
     */
    long len, offset = p->lex.pcur - p->lex.pbeg;
    int c = nextc(p), term, func = 0, quote = 0;
    enum yytokentype token = tSTRING_BEG;
    int indent = 0;

    if (c == '-') {
        c = nextc(p);
        func = STR_FUNC_INDENT;
        offset++;
    }
    else if (c == '~') {
        c = nextc(p);
        func = STR_FUNC_INDENT;
        offset++;
        indent = INT_MAX;
    }
    switch (c) {
      case '\'':
        func |= str_squote; goto quoted;
      case '"':
        func |= str_dquote; goto quoted;
      case '`':
        token = tXSTRING_BEG;
        func |= str_xquote; goto quoted;

      quoted:
        quote++;
        offset++;
        term = c;
        len = 0;
        while ((c = nextc(p)) != term) {
            if (c == -1 || c == '\r' || c == '\n') {
                yyerror0("unterminated here document identifier");
                return -1;
            }
        }
        break;

      default:
        if (!parser_is_identchar(p)) {
            pushback(p, c);
            if (func & STR_FUNC_INDENT) {
                pushback(p, indent > 0 ? '~' : '-');
            }
            return 0;
        }
        func |= str_dquote;
        do {
            int n = parser_precise_mbclen(p, p->lex.pcur-1);
            if (n < 0) return 0;
            p->lex.pcur += --n;
        } while ((c = nextc(p)) != -1 && parser_is_identchar(p));
        pushback(p, c);
        break;
    }

    len = p->lex.pcur - (p->lex.pbeg + offset) - quote;
    if ((unsigned long)len >= HERETERM_LENGTH_MAX)
        yyerror0("too long here document identifier");
    dispatch_scan_event(p, tHEREDOC_BEG);
    lex_goto_eol(p);

    p->lex.strterm = new_heredoc(p);
    rb_strterm_heredoc_t *here = &p->lex.strterm->u.heredoc;
    here->offset = offset;
    here->sourceline = p->ruby_sourceline;
    here->length = (unsigned)len;
    here->quote = quote;
    here->func = func;
    here->lastline = p->lex.lastline;
    rb_ast_add_mark_object(p->ast, p->lex.lastline);

    token_flush(p);
    p->heredoc_indent = indent;
    p->heredoc_line_indent = 0;
    return token;
}

static void
heredoc_restore(struct parser_params *p, rb_strterm_heredoc_t *here)
{
    VALUE line;
    rb_strterm_t *term = p->lex.strterm;

    p->lex.strterm = 0;
    line = here->lastline;
    p->lex.lastline = line;
    p->lex.pbeg = RSTRING_PTR(line);
    p->lex.pend = p->lex.pbeg + RSTRING_LEN(line);
    p->lex.pcur = p->lex.pbeg + here->offset + here->length + here->quote;
    p->lex.ptok = p->lex.pbeg + here->offset - here->quote;
    p->heredoc_end = p->ruby_sourceline;
    p->ruby_sourceline = (int)here->sourceline;
    if (p->eofp) p->lex.nextline = Qnil;
    p->eofp = 0;
    xfree(term);
    rb_ast_delete_mark_object(p->ast, line);
}

static int
dedent_string(struct parser_params *p, VALUE string, int width)
{
    char *str;
    long len;
    int i, col = 0;

    RSTRING_GETMEM(string, str, len);
    for (i = 0; i < len && col < width; i++) {
        if (str[i] == ' ') {
            col++;
        }
        else if (str[i] == '\t') {
            int n = TAB_WIDTH * (col / TAB_WIDTH + 1);
            if (n > width) break;
            col = n;
        }
        else {
            break;
        }
    }
    if (!i) return 0;
    rb_str_modify(string);
    str = RSTRING_PTR(string);
    if (RSTRING_LEN(string) != len)
        rb_fatal("literal string changed: %"PRIsVALUE, string);
    MEMMOVE(str, str + i, char, len - i);
    rb_str_set_len(string, len - i);
    return i;
}

#ifndef RIPPER
static NODE *
heredoc_dedent(struct parser_params *p, NODE *root)
{
    NODE *node, *str_node, *prev_node;
    int indent = p->heredoc_indent;
    VALUE prev_lit = 0;

    if (indent <= 0) return root;
    p->heredoc_indent = 0;
    if (!root) return root;

    prev_node = node = str_node = root;
    if (nd_type_p(root, NODE_LIST)) str_node = RNODE_LIST(root)->nd_head;

    while (str_node) {
        VALUE lit = RNODE_LIT(str_node)->nd_lit;
        if (nd_fl_newline(str_node)) {
            dedent_string(p, lit, indent);
        }
        if (!prev_lit) {
            prev_lit = lit;
        }
        else if (!literal_concat0(p, prev_lit, lit)) {
            return 0;
        }
        else {
            NODE *end = RNODE_LIST(node)->as.nd_end;
            node = RNODE_LIST(prev_node)->nd_next = RNODE_LIST(node)->nd_next;
            if (!node) {
                if (nd_type_p(prev_node, NODE_DSTR))
                    nd_set_type(prev_node, NODE_STR);
                break;
            }
            RNODE_LIST(node)->as.nd_end = end;
            goto next_str;
        }

        str_node = 0;
        while ((nd_type_p(node, NODE_LIST) || nd_type_p(node, NODE_DSTR)) && (node = RNODE_LIST(prev_node = node)->nd_next) != 0) {
          next_str:
            if (!nd_type_p(node, NODE_LIST)) break;
            if ((str_node = RNODE_LIST(node)->nd_head) != 0) {
                enum node_type type = nd_type(str_node);
                if (type == NODE_STR || type == NODE_DSTR) break;
                prev_lit = 0;
                str_node = 0;
            }
        }
    }
    return root;
}
#else /* RIPPER */
static VALUE
heredoc_dedent(struct parser_params *p, VALUE array)
{
    int indent = p->heredoc_indent;

    if (indent <= 0) return array;
    p->heredoc_indent = 0;
    dispatch2(heredoc_dedent, array, INT2NUM(indent));
    return array;
}
#endif

static int
whole_match_p(struct parser_params *p, const char *eos, long len, int indent)
{
    const char *beg = p->lex.pbeg;
    const char *ptr = p->lex.pend;

    if (ptr - beg < len) return FALSE;
    if (ptr > beg && ptr[-1] == '\n') {
        if (--ptr > beg && ptr[-1] == '\r') --ptr;
        if (ptr - beg < len) return FALSE;
    }
    if (strncmp(eos, ptr -= len, len)) return FALSE;
    if (indent) {
        while (beg < ptr && ISSPACE(*beg)) beg++;
    }
    return beg == ptr;
}

static int
word_match_p(struct parser_params *p, const char *word, long len)
{
    if (strncmp(p->lex.pcur, word, len)) return 0;
    if (lex_eol_n_p(p, len)) return 1;
    int c = (unsigned char)p->lex.pcur[len];
    if (ISSPACE(c)) return 1;
    switch (c) {
      case '\0': case '\004': case '\032': return 1;
    }
    return 0;
}

#define NUM_SUFFIX_R   (1<<0)
#define NUM_SUFFIX_I   (1<<1)
#define NUM_SUFFIX_ALL 3

static int
number_literal_suffix(struct parser_params *p, int mask)
{
    int c, result = 0;
    const char *lastp = p->lex.pcur;

    while ((c = nextc(p)) != -1) {
        if ((mask & NUM_SUFFIX_I) && c == 'i') {
            result |= (mask & NUM_SUFFIX_I);
            mask &= ~NUM_SUFFIX_I;
            /* r after i, rational of complex is disallowed */
            mask &= ~NUM_SUFFIX_R;
            continue;
        }
        if ((mask & NUM_SUFFIX_R) && c == 'r') {
            result |= (mask & NUM_SUFFIX_R);
            mask &= ~NUM_SUFFIX_R;
            continue;
        }
        if (!ISASCII(c) || ISALPHA(c) || c == '_') {
            p->lex.pcur = lastp;
            literal_flush(p, p->lex.pcur);
            return 0;
        }
        pushback(p, c);
        break;
    }
    return result;
}

static enum yytokentype
set_number_literal(struct parser_params *p, VALUE v,
                   enum yytokentype type, int suffix)
{
    if (suffix & NUM_SUFFIX_I) {
        v = rb_complex_raw(INT2FIX(0), v);
        type = tIMAGINARY;
    }
    set_yylval_literal(v);
    SET_LEX_STATE(EXPR_END);
    return type;
}

static enum yytokentype
set_integer_literal(struct parser_params *p, VALUE v, int suffix)
{
    enum yytokentype type = tINTEGER;
    if (suffix & NUM_SUFFIX_R) {
        v = rb_rational_raw1(v);
        type = tRATIONAL;
    }
    return set_number_literal(p, v, type, suffix);
}

#ifdef RIPPER
static void
dispatch_heredoc_end(struct parser_params *p)
{
    VALUE str;
    if (has_delayed_token(p))
        dispatch_delayed_token(p, tSTRING_CONTENT);
    str = STR_NEW(p->lex.ptok, p->lex.pend - p->lex.ptok);
    ripper_dispatch1(p, ripper_token2eventid(tHEREDOC_END), str);
    RUBY_SET_YYLLOC_FROM_STRTERM_HEREDOC(*p->yylloc);
    lex_goto_eol(p);
    token_flush(p);
}

#else
#define dispatch_heredoc_end(p) parser_dispatch_heredoc_end(p, __LINE__)
static void
parser_dispatch_heredoc_end(struct parser_params *p, int line)
{
    if (has_delayed_token(p))
        dispatch_delayed_token(p, tSTRING_CONTENT);

    if (p->keep_tokens) {
        VALUE str = STR_NEW(p->lex.ptok, p->lex.pend - p->lex.ptok);
        RUBY_SET_YYLLOC_OF_HEREDOC_END(*p->yylloc);
        parser_append_tokens(p, str, tHEREDOC_END, line);
    }

    RUBY_SET_YYLLOC_FROM_STRTERM_HEREDOC(*p->yylloc);
    lex_goto_eol(p);
    token_flush(p);
}
#endif

static enum yytokentype
here_document(struct parser_params *p, rb_strterm_heredoc_t *here)
{
    int c, func, indent = 0;
    const char *eos, *ptr, *ptr_end;
    long len;
    VALUE str = 0;
    rb_encoding *enc = p->enc;
    rb_encoding *base_enc = 0;
    int bol;

    eos = RSTRING_PTR(here->lastline) + here->offset;
    len = here->length;
    indent = (func = here->func) & STR_FUNC_INDENT;

    if ((c = nextc(p)) == -1) {
      error:
#ifdef RIPPER
        if (!has_delayed_token(p)) {
            dispatch_scan_event(p, tSTRING_CONTENT);
        }
        else {
            if ((len = p->lex.pcur - p->lex.ptok) > 0) {
                if (!(func & STR_FUNC_REGEXP) && rb_enc_asciicompat(enc)) {
                    int cr = ENC_CODERANGE_UNKNOWN;
                    rb_str_coderange_scan_restartable(p->lex.ptok, p->lex.pcur, enc, &cr);
                    if (cr != ENC_CODERANGE_7BIT &&
                        rb_is_usascii_enc(p->enc) &&
                        enc != rb_utf8_encoding()) {
                        enc = rb_ascii8bit_encoding();
                    }
                }
                rb_enc_str_buf_cat(p->delayed.token, p->lex.ptok, len, enc);
            }
            dispatch_delayed_token(p, tSTRING_CONTENT);
        }
        lex_goto_eol(p);
#endif
        heredoc_restore(p, &p->lex.strterm->u.heredoc);
        compile_error(p, "can't find string \"%.*s\" anywhere before EOF",
                      (int)len, eos);
        token_flush(p);
        SET_LEX_STATE(EXPR_END);
        return tSTRING_END;
    }
    bol = was_bol(p);
    if (!bol) {
        /* not beginning of line, cannot be the terminator */
    }
    else if (p->heredoc_line_indent == -1) {
        /* `heredoc_line_indent == -1` means
         * - "after an interpolation in the same line", or
         * - "in a continuing line"
         */
        p->heredoc_line_indent = 0;
    }
    else if (whole_match_p(p, eos, len, indent)) {
        dispatch_heredoc_end(p);
      restore:
        heredoc_restore(p, &p->lex.strterm->u.heredoc);
        token_flush(p);
        SET_LEX_STATE(EXPR_END);
        return tSTRING_END;
    }

    if (!(func & STR_FUNC_EXPAND)) {
        do {
            ptr = RSTRING_PTR(p->lex.lastline);
            ptr_end = p->lex.pend;
            if (ptr_end > ptr) {
                switch (ptr_end[-1]) {
                  case '\n':
                    if (--ptr_end == ptr || ptr_end[-1] != '\r') {
                        ptr_end++;
                        break;
                    }
                  case '\r':
                    --ptr_end;
                }
            }

            if (p->heredoc_indent > 0) {
                long i = 0;
                while (ptr + i < ptr_end && parser_update_heredoc_indent(p, ptr[i]))
                    i++;
                p->heredoc_line_indent = 0;
            }

            if (str)
                rb_str_cat(str, ptr, ptr_end - ptr);
            else
                str = STR_NEW(ptr, ptr_end - ptr);
            if (!lex_eol_ptr_p(p, ptr_end)) rb_str_cat(str, "\n", 1);
            lex_goto_eol(p);
            if (p->heredoc_indent > 0) {
                goto flush_str;
            }
            if (nextc(p) == -1) {
                if (str) {
                    str = 0;
                }
                goto error;
            }
        } while (!whole_match_p(p, eos, len, indent));
    }
    else {
        /*	int mb = ENC_CODERANGE_7BIT, *mbp = &mb;*/
        newtok(p);
        if (c == '#') {
            enum yytokentype t = parser_peek_variable_name(p);
            if (p->heredoc_line_indent != -1) {
                if (p->heredoc_indent > p->heredoc_line_indent) {
                    p->heredoc_indent = p->heredoc_line_indent;
                }
                p->heredoc_line_indent = -1;
            }
            if (t) return t;
            tokadd(p, '#');
            c = nextc(p);
        }
        do {
            pushback(p, c);
            enc = p->enc;
            if ((c = tokadd_string(p, func, '\n', 0, NULL, &enc, &base_enc)) == -1) {
                if (p->eofp) goto error;
                goto restore;
            }
            if (c != '\n') {
                if (c == '\\') p->heredoc_line_indent = -1;
              flush:
                str = STR_NEW3(tok(p), toklen(p), enc, func);
              flush_str:
                set_yylval_str(str);
#ifndef RIPPER
                if (bol) nd_set_fl_newline(yylval.node);
#endif
                flush_string_content(p, enc);
                return tSTRING_CONTENT;
            }
            tokadd(p, nextc(p));
            if (p->heredoc_indent > 0) {
                lex_goto_eol(p);
                goto flush;
            }
            /*	    if (mbp && mb == ENC_CODERANGE_UNKNOWN) mbp = 0;*/
            if ((c = nextc(p)) == -1) goto error;
        } while (!whole_match_p(p, eos, len, indent));
        str = STR_NEW3(tok(p), toklen(p), enc, func);
    }
    dispatch_heredoc_end(p);
#ifdef RIPPER
    str = ripper_new_yylval(p, ripper_token2eventid(tSTRING_CONTENT),
                            yylval.val, str);
#endif
    heredoc_restore(p, &p->lex.strterm->u.heredoc);
    token_flush(p);
    p->lex.strterm = NEW_STRTERM(func | STR_FUNC_TERM, 0, 0);
    set_yylval_str(str);
#ifndef RIPPER
    if (bol) nd_set_fl_newline(yylval.node);
#endif
    return tSTRING_CONTENT;
}

#include "lex.c"

static int
arg_ambiguous(struct parser_params *p, char c)
{
#ifndef RIPPER
    if (c == '/') {
        rb_warning1("ambiguity between regexp and two divisions: wrap regexp in parentheses or add a space after `%c' operator", WARN_I(c));
    }
    else {
        rb_warning1("ambiguous first argument; put parentheses or a space even after `%c' operator", WARN_I(c));
    }
#else
    dispatch1(arg_ambiguous, rb_usascii_str_new(&c, 1));
#endif
    return TRUE;
}

static ID
#ifndef RIPPER
formal_argument(struct parser_params *p, ID lhs)
#else
formal_argument(struct parser_params *p, VALUE lhs)
#endif
{
    ID id = get_id(lhs);

    switch (id_type(id)) {
      case ID_LOCAL:
        break;
#ifndef RIPPER
# define ERR(mesg) yyerror0(mesg)
#else
# define ERR(mesg) (dispatch2(param_error, WARN_S(mesg), lhs), ripper_error(p))
#endif
      case ID_CONST:
        ERR("formal argument cannot be a constant");
        return 0;
      case ID_INSTANCE:
        ERR("formal argument cannot be an instance variable");
        return 0;
      case ID_GLOBAL:
        ERR("formal argument cannot be a global variable");
        return 0;
      case ID_CLASS:
        ERR("formal argument cannot be a class variable");
        return 0;
      default:
        ERR("formal argument must be local variable");
        return 0;
#undef ERR
    }
    shadowing_lvar(p, id);
    return lhs;
}

static int
lvar_defined(struct parser_params *p, ID id)
{
    return (dyna_in_block(p) && dvar_defined(p, id)) || local_id(p, id);
}

/* emacsen -*- hack */
static long
parser_encode_length(struct parser_params *p, const char *name, long len)
{
    long nlen;

    if (len > 5 && name[nlen = len - 5] == '-') {
        if (rb_memcicmp(name + nlen + 1, "unix", 4) == 0)
            return nlen;
    }
    if (len > 4 && name[nlen = len - 4] == '-') {
        if (rb_memcicmp(name + nlen + 1, "dos", 3) == 0)
            return nlen;
        if (rb_memcicmp(name + nlen + 1, "mac", 3) == 0 &&
            !(len == 8 && rb_memcicmp(name, "utf8-mac", len) == 0))
            /* exclude UTF8-MAC because the encoding named "UTF8" doesn't exist in Ruby */
            return nlen;
    }
    return len;
}

static void
parser_set_encode(struct parser_params *p, const char *name)
{
    int idx = rb_enc_find_index(name);
    rb_encoding *enc;
    VALUE excargs[3];

    if (idx < 0) {
        excargs[1] = rb_sprintf("unknown encoding name: %s", name);
      error:
        excargs[0] = rb_eArgError;
        excargs[2] = rb_make_backtrace();
        rb_ary_unshift(excargs[2], rb_sprintf("%"PRIsVALUE":%d", p->ruby_sourcefile_string, p->ruby_sourceline));
        rb_exc_raise(rb_make_exception(3, excargs));
    }
    enc = rb_enc_from_index(idx);
    if (!rb_enc_asciicompat(enc)) {
        excargs[1] = rb_sprintf("%s is not ASCII compatible", rb_enc_name(enc));
        goto error;
    }
    p->enc = enc;
#ifndef RIPPER
    if (p->debug_lines) {
        VALUE lines = p->debug_lines;
        long i, n = RARRAY_LEN(lines);
        for (i = 0; i < n; ++i) {
            rb_enc_associate_index(RARRAY_AREF(lines, i), idx);
        }
    }
#endif
}

static int
comment_at_top(struct parser_params *p)
{
    const char *ptr = p->lex.pbeg, *ptr_end = p->lex.pcur - 1;
    if (p->line_count != (p->has_shebang ? 2 : 1)) return 0;
    while (ptr < ptr_end) {
        if (!ISSPACE(*ptr)) return 0;
        ptr++;
    }
    return 1;
}

typedef long (*rb_magic_comment_length_t)(struct parser_params *p, const char *name, long len);
typedef void (*rb_magic_comment_setter_t)(struct parser_params *p, const char *name, const char *val);

static int parser_invalid_pragma_value(struct parser_params *p, const char *name, const char *val);

static void
magic_comment_encoding(struct parser_params *p, const char *name, const char *val)
{
    if (!comment_at_top(p)) {
        return;
    }
    parser_set_encode(p, val);
}

static int
parser_get_bool(struct parser_params *p, const char *name, const char *val)
{
    switch (*val) {
      case 't': case 'T':
        if (STRCASECMP(val, "true") == 0) {
            return TRUE;
        }
        break;
      case 'f': case 'F':
        if (STRCASECMP(val, "false") == 0) {
            return FALSE;
        }
        break;
    }
    return parser_invalid_pragma_value(p, name, val);
}

static int
parser_invalid_pragma_value(struct parser_params *p, const char *name, const char *val)
{
    rb_warning2("invalid value for %s: %s", WARN_S(name), WARN_S(val));
    return -1;
}

static void
parser_set_token_info(struct parser_params *p, const char *name, const char *val)
{
    int b = parser_get_bool(p, name, val);
    if (b >= 0) p->token_info_enabled = b;
}

static void
parser_set_frozen_string_literal(struct parser_params *p, const char *name, const char *val)
{
    int b;

    if (p->token_seen) {
        rb_warning1("`%s' is ignored after any tokens", WARN_S(name));
        return;
    }

    b = parser_get_bool(p, name, val);
    if (b < 0) return;

    p->frozen_string_literal = b;
}

static void
parser_set_shareable_constant_value(struct parser_params *p, const char *name, const char *val)
{
    for (const char *s = p->lex.pbeg, *e = p->lex.pcur; s < e; ++s) {
        if (*s == ' ' || *s == '\t') continue;
        if (*s == '#') break;
        rb_warning1("`%s' is ignored unless in comment-only line", WARN_S(name));
        return;
    }

    switch (*val) {
      case 'n': case 'N':
        if (STRCASECMP(val, "none") == 0) {
            p->ctxt.shareable_constant_value = shareable_none;
            return;
        }
        break;
      case 'l': case 'L':
        if (STRCASECMP(val, "literal") == 0) {
            p->ctxt.shareable_constant_value = shareable_literal;
            return;
        }
        break;
      case 'e': case 'E':
        if (STRCASECMP(val, "experimental_copy") == 0) {
            p->ctxt.shareable_constant_value = shareable_copy;
            return;
        }
        if (STRCASECMP(val, "experimental_everything") == 0) {
            p->ctxt.shareable_constant_value = shareable_everything;
            return;
        }
        break;
    }
    parser_invalid_pragma_value(p, name, val);
}

# if WARN_PAST_SCOPE
static void
parser_set_past_scope(struct parser_params *p, const char *name, const char *val)
{
    int b = parser_get_bool(p, name, val);
    if (b >= 0) p->past_scope_enabled = b;
}
# endif

struct magic_comment {
    const char *name;
    rb_magic_comment_setter_t func;
    rb_magic_comment_length_t length;
};

static const struct magic_comment magic_comments[] = {
    {"coding", magic_comment_encoding, parser_encode_length},
    {"encoding", magic_comment_encoding, parser_encode_length},
    {"frozen_string_literal", parser_set_frozen_string_literal},
    {"shareable_constant_value", parser_set_shareable_constant_value},
    {"warn_indent", parser_set_token_info},
# if WARN_PAST_SCOPE
    {"warn_past_scope", parser_set_past_scope},
# endif
};

static const char *
magic_comment_marker(const char *str, long len)
{
    long i = 2;

    while (i < len) {
        switch (str[i]) {
          case '-':
            if (str[i-1] == '*' && str[i-2] == '-') {
                return str + i + 1;
            }
            i += 2;
            break;
          case '*':
            if (i + 1 >= len) return 0;
            if (str[i+1] != '-') {
                i += 4;
            }
            else if (str[i-1] != '-') {
                i += 2;
            }
            else {
                return str + i + 2;
            }
            break;
          default:
            i += 3;
            break;
        }
    }
    return 0;
}

static int
parser_magic_comment(struct parser_params *p, const char *str, long len)
{
    int indicator = 0;
    VALUE name = 0, val = 0;
    const char *beg, *end, *vbeg, *vend;
#define str_copy(_s, _p, _n) ((_s) \
        ? (void)(rb_str_resize((_s), (_n)), \
           MEMCPY(RSTRING_PTR(_s), (_p), char, (_n)), (_s)) \
        : (void)((_s) = STR_NEW((_p), (_n))))

    if (len <= 7) return FALSE;
    if (!!(beg = magic_comment_marker(str, len))) {
        if (!(end = magic_comment_marker(beg, str + len - beg)))
            return FALSE;
        indicator = TRUE;
        str = beg;
        len = end - beg - 3;
    }

    /* %r"([^\\s\'\":;]+)\\s*:\\s*(\"(?:\\\\.|[^\"])*\"|[^\"\\s;]+)[\\s;]*" */
    while (len > 0) {
        const struct magic_comment *mc = magic_comments;
        char *s;
        int i;
        long n = 0;

        for (; len > 0 && *str; str++, --len) {
            switch (*str) {
              case '\'': case '"': case ':': case ';':
                continue;
            }
            if (!ISSPACE(*str)) break;
        }
        for (beg = str; len > 0; str++, --len) {
            switch (*str) {
              case '\'': case '"': case ':': case ';':
                break;
              default:
                if (ISSPACE(*str)) break;
                continue;
            }
            break;
        }
        for (end = str; len > 0 && ISSPACE(*str); str++, --len);
        if (!len) break;
        if (*str != ':') {
            if (!indicator) return FALSE;
            continue;
        }

        do str++; while (--len > 0 && ISSPACE(*str));
        if (!len) break;
        if (*str == '"') {
            for (vbeg = ++str; --len > 0 && *str != '"'; str++) {
                if (*str == '\\') {
                    --len;
                    ++str;
                }
            }
            vend = str;
            if (len) {
                --len;
                ++str;
            }
        }
        else {
            for (vbeg = str; len > 0 && *str != '"' && *str != ';' && !ISSPACE(*str); --len, str++);
            vend = str;
        }
        if (indicator) {
            while (len > 0 && (*str == ';' || ISSPACE(*str))) --len, str++;
        }
        else {
            while (len > 0 && (ISSPACE(*str))) --len, str++;
            if (len) return FALSE;
        }

        n = end - beg;
        str_copy(name, beg, n);
        s = RSTRING_PTR(name);
        for (i = 0; i < n; ++i) {
            if (s[i] == '-') s[i] = '_';
        }
        do {
            if (STRNCASECMP(mc->name, s, n) == 0 && !mc->name[n]) {
                n = vend - vbeg;
                if (mc->length) {
                    n = (*mc->length)(p, vbeg, n);
                }
                str_copy(val, vbeg, n);
                (*mc->func)(p, mc->name, RSTRING_PTR(val));
                break;
            }
        } while (++mc < magic_comments + numberof(magic_comments));
#ifdef RIPPER
        str_copy(val, vbeg, vend - vbeg);
        dispatch2(magic_comment, name, val);
#endif
    }

    return TRUE;
}

static void
set_file_encoding(struct parser_params *p, const char *str, const char *send)
{
    int sep = 0;
    const char *beg = str;
    VALUE s;

    for (;;) {
        if (send - str <= 6) return;
        switch (str[6]) {
          case 'C': case 'c': str += 6; continue;
          case 'O': case 'o': str += 5; continue;
          case 'D': case 'd': str += 4; continue;
          case 'I': case 'i': str += 3; continue;
          case 'N': case 'n': str += 2; continue;
          case 'G': case 'g': str += 1; continue;
          case '=': case ':':
            sep = 1;
            str += 6;
            break;
          default:
            str += 6;
            if (ISSPACE(*str)) break;
            continue;
        }
        if (STRNCASECMP(str-6, "coding", 6) == 0) break;
        sep = 0;
    }
    for (;;) {
        do {
            if (++str >= send) return;
        } while (ISSPACE(*str));
        if (sep) break;
        if (*str != '=' && *str != ':') return;
        sep = 1;
        str++;
    }
    beg = str;
    while ((*str == '-' || *str == '_' || ISALNUM(*str)) && ++str < send);
    s = rb_str_new(beg, parser_encode_length(p, beg, str - beg));
    parser_set_encode(p, RSTRING_PTR(s));
    rb_str_resize(s, 0);
}

static void
parser_prepare(struct parser_params *p)
{
    int c = nextc0(p, FALSE);
    p->token_info_enabled = !compile_for_eval && RTEST(ruby_verbose);
    switch (c) {
      case '#':
        if (peek(p, '!')) p->has_shebang = 1;
        break;
      case 0xef:		/* UTF-8 BOM marker */
        if (!lex_eol_n_p(p, 2) &&
            (unsigned char)p->lex.pcur[0] == 0xbb &&
            (unsigned char)p->lex.pcur[1] == 0xbf) {
            p->enc = rb_utf8_encoding();
            p->lex.pcur += 2;
#ifndef RIPPER
            if (p->debug_lines) {
                rb_enc_associate(p->lex.lastline, p->enc);
            }
#endif
            p->lex.pbeg = p->lex.pcur;
            token_flush(p);
            return;
        }
        break;
      case EOF:
        return;
    }
    pushback(p, c);
    p->enc = rb_enc_get(p->lex.lastline);
}

#ifndef RIPPER
#define ambiguous_operator(tok, op, syn) ( \
    rb_warning0("`"op"' after local variable or literal is interpreted as binary operator"), \
    rb_warning0("even though it seems like "syn""))
#else
#define ambiguous_operator(tok, op, syn) \
    dispatch2(operator_ambiguous, TOKEN2VAL(tok), rb_str_new_cstr(syn))
#endif
#define warn_balanced(tok, op, syn) ((void) \
    (!IS_lex_state_for(last_state, EXPR_CLASS|EXPR_DOT|EXPR_FNAME|EXPR_ENDFN) && \
     space_seen && !ISSPACE(c) && \
     (ambiguous_operator(tok, op, syn), 0)), \
     (enum yytokentype)(tok))

static VALUE
parse_rational(struct parser_params *p, char *str, int len, int seen_point)
{
    VALUE v;
    char *point = &str[seen_point];
    size_t fraclen = len-seen_point-1;
    memmove(point, point+1, fraclen+1);
    v = rb_cstr_to_inum(str, 10, FALSE);
    return rb_rational_new(v, rb_int_positive_pow(10, fraclen));
}

static enum yytokentype
no_digits(struct parser_params *p)
{
    yyerror0("numeric literal without digits");
    if (peek(p, '_')) nextc(p);
    /* dummy 0, for tUMINUS_NUM at numeric */
    return set_integer_literal(p, INT2FIX(0), 0);
}

static enum yytokentype
parse_numeric(struct parser_params *p, int c)
{
    int is_float, seen_point, seen_e, nondigit;
    int suffix;

    is_float = seen_point = seen_e = nondigit = 0;
    SET_LEX_STATE(EXPR_END);
    newtok(p);
    if (c == '-' || c == '+') {
        tokadd(p, c);
        c = nextc(p);
    }
    if (c == '0') {
        int start = toklen(p);
        c = nextc(p);
        if (c == 'x' || c == 'X') {
            /* hexadecimal */
            c = nextc(p);
            if (c != -1 && ISXDIGIT(c)) {
                do {
                    if (c == '_') {
                        if (nondigit) break;
                        nondigit = c;
                        continue;
                    }
                    if (!ISXDIGIT(c)) break;
                    nondigit = 0;
                    tokadd(p, c);
                } while ((c = nextc(p)) != -1);
            }
            pushback(p, c);
            tokfix(p);
            if (toklen(p) == start) {
                return no_digits(p);
            }
            else if (nondigit) goto trailing_uc;
            suffix = number_literal_suffix(p, NUM_SUFFIX_ALL);
            return set_integer_literal(p, rb_cstr_to_inum(tok(p), 16, FALSE), suffix);
        }
        if (c == 'b' || c == 'B') {
            /* binary */
            c = nextc(p);
            if (c == '0' || c == '1') {
                do {
                    if (c == '_') {
                        if (nondigit) break;
                        nondigit = c;
                        continue;
                    }
                    if (c != '0' && c != '1') break;
                    nondigit = 0;
                    tokadd(p, c);
                } while ((c = nextc(p)) != -1);
            }
            pushback(p, c);
            tokfix(p);
            if (toklen(p) == start) {
                return no_digits(p);
            }
            else if (nondigit) goto trailing_uc;
            suffix = number_literal_suffix(p, NUM_SUFFIX_ALL);
            return set_integer_literal(p, rb_cstr_to_inum(tok(p), 2, FALSE), suffix);
        }
        if (c == 'd' || c == 'D') {
            /* decimal */
            c = nextc(p);
            if (c != -1 && ISDIGIT(c)) {
                do {
                    if (c == '_') {
                        if (nondigit) break;
                        nondigit = c;
                        continue;
                    }
                    if (!ISDIGIT(c)) break;
                    nondigit = 0;
                    tokadd(p, c);
                } while ((c = nextc(p)) != -1);
            }
            pushback(p, c);
            tokfix(p);
            if (toklen(p) == start) {
                return no_digits(p);
            }
            else if (nondigit) goto trailing_uc;
            suffix = number_literal_suffix(p, NUM_SUFFIX_ALL);
            return set_integer_literal(p, rb_cstr_to_inum(tok(p), 10, FALSE), suffix);
        }
        if (c == '_') {
            /* 0_0 */
            goto octal_number;
        }
        if (c == 'o' || c == 'O') {
            /* prefixed octal */
            c = nextc(p);
            if (c == -1 || c == '_' || !ISDIGIT(c)) {
                return no_digits(p);
            }
        }
        if (c >= '0' && c <= '7') {
            /* octal */
          octal_number:
            do {
                if (c == '_') {
                    if (nondigit) break;
                    nondigit = c;
                    continue;
                }
                if (c < '0' || c > '9') break;
                if (c > '7') goto invalid_octal;
                nondigit = 0;
                tokadd(p, c);
            } while ((c = nextc(p)) != -1);
            if (toklen(p) > start) {
                pushback(p, c);
                tokfix(p);
                if (nondigit) goto trailing_uc;
                suffix = number_literal_suffix(p, NUM_SUFFIX_ALL);
                return set_integer_literal(p, rb_cstr_to_inum(tok(p), 8, FALSE), suffix);
            }
            if (nondigit) {
                pushback(p, c);
                goto trailing_uc;
            }
        }
        if (c > '7' && c <= '9') {
          invalid_octal:
            yyerror0("Invalid octal digit");
        }
        else if (c == '.' || c == 'e' || c == 'E') {
            tokadd(p, '0');
        }
        else {
            pushback(p, c);
            suffix = number_literal_suffix(p, NUM_SUFFIX_ALL);
            return set_integer_literal(p, INT2FIX(0), suffix);
        }
    }

    for (;;) {
        switch (c) {
          case '0': case '1': case '2': case '3': case '4':
          case '5': case '6': case '7': case '8': case '9':
            nondigit = 0;
            tokadd(p, c);
            break;

          case '.':
            if (nondigit) goto trailing_uc;
            if (seen_point || seen_e) {
                goto decode_num;
            }
            else {
                int c0 = nextc(p);
                if (c0 == -1 || !ISDIGIT(c0)) {
                    pushback(p, c0);
                    goto decode_num;
                }
                c = c0;
            }
            seen_point = toklen(p);
            tokadd(p, '.');
            tokadd(p, c);
            is_float++;
            nondigit = 0;
            break;

          case 'e':
          case 'E':
            if (nondigit) {
                pushback(p, c);
                c = nondigit;
                goto decode_num;
            }
            if (seen_e) {
                goto decode_num;
            }
            nondigit = c;
            c = nextc(p);
            if (c != '-' && c != '+' && !ISDIGIT(c)) {
                pushback(p, c);
                c = nondigit;
                nondigit = 0;
                goto decode_num;
            }
            tokadd(p, nondigit);
            seen_e++;
            is_float++;
            tokadd(p, c);
            nondigit = (c == '-' || c == '+') ? c : 0;
            break;

          case '_':	/* `_' in number just ignored */
            if (nondigit) goto decode_num;
            nondigit = c;
            break;

          default:
            goto decode_num;
        }
        c = nextc(p);
    }

  decode_num:
    pushback(p, c);
    if (nondigit) {
      trailing_uc:
        literal_flush(p, p->lex.pcur - 1);
        YYLTYPE loc = RUBY_INIT_YYLLOC();
        compile_error(p, "trailing `%c' in number", nondigit);
        parser_show_error_line(p, &loc);
    }
    tokfix(p);
    if (is_float) {
        enum yytokentype type = tFLOAT;
        VALUE v;

        suffix = number_literal_suffix(p, seen_e ? NUM_SUFFIX_I : NUM_SUFFIX_ALL);
        if (suffix & NUM_SUFFIX_R) {
            type = tRATIONAL;
            v = parse_rational(p, tok(p), toklen(p), seen_point);
        }
        else {
            double d = strtod(tok(p), 0);
            if (errno == ERANGE) {
                rb_warning1("Float %s out of range", WARN_S(tok(p)));
                errno = 0;
            }
            v = DBL2NUM(d);
        }
        return set_number_literal(p, v, type, suffix);
    }
    suffix = number_literal_suffix(p, NUM_SUFFIX_ALL);
    return set_integer_literal(p, rb_cstr_to_inum(tok(p), 10, FALSE), suffix);
}

static enum yytokentype
parse_qmark(struct parser_params *p, int space_seen)
{
    rb_encoding *enc;
    register int c;
    VALUE lit;
    const char *start = p->lex.pcur;

    if (IS_END()) {
        SET_LEX_STATE(EXPR_VALUE);
        return '?';
    }
    c = nextc(p);
    if (c == -1) {
        compile_error(p, "incomplete character syntax");
        return 0;
    }
    if (rb_enc_isspace(c, p->enc)) {
        if (!IS_ARG()) {
            int c2 = escaped_control_code(c);
            if (c2) {
                WARN_SPACE_CHAR(c2, "?");
            }
        }
      ternary:
        pushback(p, c);
        SET_LEX_STATE(EXPR_VALUE);
        return '?';
    }
    newtok(p);
    enc = p->enc;
    int w = parser_precise_mbclen(p, start);
    if (is_identchar(p, start, p->lex.pend, p->enc) &&
        !(lex_eol_ptr_n_p(p, start, w) || !is_identchar(p, start + w, p->lex.pend, p->enc))) {
        if (space_seen) {
            const char *ptr = start;
            do {
                int n = parser_precise_mbclen(p, ptr);
                if (n < 0) return -1;
                ptr += n;
            } while (!lex_eol_ptr_p(p, ptr) && is_identchar(p, ptr, p->lex.pend, p->enc));
            rb_warn2("`?' just followed by `%.*s' is interpreted as" \
                     " a conditional operator, put a space after `?'",
                     WARN_I((int)(ptr - start)), WARN_S_L(start, (ptr - start)));
        }
        goto ternary;
    }
    else if (c == '\\') {
        if (peek(p, 'u')) {
            nextc(p);
            enc = rb_utf8_encoding();
            tokadd_utf8(p, &enc, -1, 0, 0);
        }
        else if (!ISASCII(c = peekc(p))) {
            nextc(p);
            if (tokadd_mbchar(p, c) == -1) return 0;
        }
        else {
            c = read_escape(p, 0);
            tokadd(p, c);
        }
    }
    else {
        if (tokadd_mbchar(p, c) == -1) return 0;
    }
    tokfix(p);
    lit = STR_NEW3(tok(p), toklen(p), enc, 0);
    set_yylval_str(lit);
    SET_LEX_STATE(EXPR_END);
    return tCHAR;
}

static enum yytokentype
parse_percent(struct parser_params *p, const int space_seen, const enum lex_state_e last_state)
{
    register int c;
    const char *ptok = p->lex.pcur;

    if (IS_BEG()) {
        int term;
        int paren;

        c = nextc(p);
      quotation:
        if (c == -1) goto unterminated;
        if (!ISALNUM(c)) {
            term = c;
            if (!ISASCII(c)) goto unknown;
            c = 'Q';
        }
        else {
            term = nextc(p);
            if (rb_enc_isalnum(term, p->enc) || !parser_isascii(p)) {
              unknown:
                pushback(p, term);
                c = parser_precise_mbclen(p, p->lex.pcur);
                if (c < 0) return 0;
                p->lex.pcur += c;
                yyerror0("unknown type of %string");
                return 0;
            }
        }
        if (term == -1) {
          unterminated:
            compile_error(p, "unterminated quoted string meets end of file");
            return 0;
        }
        paren = term;
        if (term == '(') term = ')';
        else if (term == '[') term = ']';
        else if (term == '{') term = '}';
        else if (term == '<') term = '>';
        else paren = 0;

        p->lex.ptok = ptok-1;
        switch (c) {
          case 'Q':
            p->lex.strterm = NEW_STRTERM(str_dquote, term, paren);
            return tSTRING_BEG;

          case 'q':
            p->lex.strterm = NEW_STRTERM(str_squote, term, paren);
            return tSTRING_BEG;

          case 'W':
            p->lex.strterm = NEW_STRTERM(str_dword, term, paren);
            return tWORDS_BEG;

          case 'w':
            p->lex.strterm = NEW_STRTERM(str_sword, term, paren);
            return tQWORDS_BEG;

          case 'I':
            p->lex.strterm = NEW_STRTERM(str_dword, term, paren);
            return tSYMBOLS_BEG;

          case 'i':
            p->lex.strterm = NEW_STRTERM(str_sword, term, paren);
            return tQSYMBOLS_BEG;

          case 'x':
            p->lex.strterm = NEW_STRTERM(str_xquote, term, paren);
            return tXSTRING_BEG;

          case 'r':
            p->lex.strterm = NEW_STRTERM(str_regexp, term, paren);
            return tREGEXP_BEG;

          case 's':
            p->lex.strterm = NEW_STRTERM(str_ssym, term, paren);
            SET_LEX_STATE(EXPR_FNAME|EXPR_FITEM);
            return tSYMBEG;

          default:
            yyerror0("unknown type of %string");
            return 0;
        }
    }
    if ((c = nextc(p)) == '=') {
        set_yylval_id('%');
        SET_LEX_STATE(EXPR_BEG);
        return tOP_ASGN;
    }
    if (IS_SPCARG(c) || (IS_lex_state(EXPR_FITEM) && c == 's')) {
        goto quotation;
    }
    SET_LEX_STATE(IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG);
    pushback(p, c);
    return warn_balanced('%', "%%", "string literal");
}

static int
tokadd_ident(struct parser_params *p, int c)
{
    do {
        if (tokadd_mbchar(p, c) == -1) return -1;
        c = nextc(p);
    } while (parser_is_identchar(p));
    pushback(p, c);
    return 0;
}

static ID
tokenize_ident(struct parser_params *p)
{
    ID ident = TOK_INTERN();

    set_yylval_name(ident);

    return ident;
}

static int
parse_numvar(struct parser_params *p)
{
    size_t len;
    int overflow;
    unsigned long n = ruby_scan_digits(tok(p)+1, toklen(p)-1, 10, &len, &overflow);
    const unsigned long nth_ref_max =
        ((FIXNUM_MAX < INT_MAX) ? FIXNUM_MAX : INT_MAX) >> 1;
    /* NTH_REF is left-shifted to be ORed with back-ref flag and
     * turned into a Fixnum, in compile.c */

    if (overflow || n > nth_ref_max) {
        /* compile_error()? */
        rb_warn1("`%s' is too big for a number variable, always nil", WARN_S(tok(p)));
        return 0;		/* $0 is $PROGRAM_NAME, not NTH_REF */
    }
    else {
        return (int)n;
    }
}

static enum yytokentype
parse_gvar(struct parser_params *p, const enum lex_state_e last_state)
{
    const char *ptr = p->lex.pcur;
    register int c;

    SET_LEX_STATE(EXPR_END);
    p->lex.ptok = ptr - 1; /* from '$' */
    newtok(p);
    c = nextc(p);
    switch (c) {
      case '_':		/* $_: last read line string */
        c = nextc(p);
        if (parser_is_identchar(p)) {
            tokadd(p, '$');
            tokadd(p, '_');
            break;
        }
        pushback(p, c);
        c = '_';
        /* fall through */
      case '~': 	/* $~: match-data */
      case '*': 	/* $*: argv */
      case '$': 	/* $$: pid */
      case '?': 	/* $?: last status */
      case '!': 	/* $!: error string */
      case '@': 	/* $@: error position */
      case '/': 	/* $/: input record separator */
      case '\\':	/* $\: output record separator */
      case ';': 	/* $;: field separator */
      case ',': 	/* $,: output field separator */
      case '.': 	/* $.: last read line number */
      case '=': 	/* $=: ignorecase */
      case ':': 	/* $:: load path */
      case '<': 	/* $<: reading filename */
      case '>': 	/* $>: default output handle */
      case '\"':	/* $": already loaded files */
        tokadd(p, '$');
        tokadd(p, c);
        goto gvar;

      case '-':
        tokadd(p, '$');
        tokadd(p, c);
        c = nextc(p);
        if (parser_is_identchar(p)) {
            if (tokadd_mbchar(p, c) == -1) return 0;
        }
        else {
            pushback(p, c);
            pushback(p, '-');
            return '$';
        }
      gvar:
        set_yylval_name(TOK_INTERN());
        return tGVAR;

      case '&': 	/* $&: last match */
      case '`': 	/* $`: string before last match */
      case '\'':	/* $': string after last match */
      case '+': 	/* $+: string matches last paren. */
        if (IS_lex_state_for(last_state, EXPR_FNAME)) {
            tokadd(p, '$');
            tokadd(p, c);
            goto gvar;
        }
        set_yylval_node(NEW_BACK_REF(c, &_cur_loc));
        return tBACK_REF;

      case '1': case '2': case '3':
      case '4': case '5': case '6':
      case '7': case '8': case '9':
        tokadd(p, '$');
        do {
            tokadd(p, c);
            c = nextc(p);
        } while (c != -1 && ISDIGIT(c));
        pushback(p, c);
        if (IS_lex_state_for(last_state, EXPR_FNAME)) goto gvar;
        tokfix(p);
        c = parse_numvar(p);
        set_yylval_node(NEW_NTH_REF(c, &_cur_loc));
        return tNTH_REF;

      default:
        if (!parser_is_identchar(p)) {
            YYLTYPE loc = RUBY_INIT_YYLLOC();
            if (c == -1 || ISSPACE(c)) {
                compile_error(p, "`$' without identifiers is not allowed as a global variable name");
            }
            else {
                pushback(p, c);
                compile_error(p, "`$%c' is not allowed as a global variable name", c);
            }
            parser_show_error_line(p, &loc);
            set_yylval_noname();
            return tGVAR;
        }
        /* fall through */
      case '0':
        tokadd(p, '$');
    }

    if (tokadd_ident(p, c)) return 0;
    SET_LEX_STATE(EXPR_END);
    if (VALID_SYMNAME_P(tok(p), toklen(p), p->enc, ID_GLOBAL)) {
        tokenize_ident(p);
    }
    else {
        compile_error(p, "`%.*s' is not allowed as a global variable name", toklen(p), tok(p));
        set_yylval_noname();
    }
    return tGVAR;
}

#ifndef RIPPER
static bool
parser_numbered_param(struct parser_params *p, int n)
{
    if (n < 0) return false;

    if (DVARS_TERMINAL_P(p->lvtbl->args) || DVARS_TERMINAL_P(p->lvtbl->args->prev)) {
        return false;
    }
    if (p->max_numparam == ORDINAL_PARAM) {
        compile_error(p, "ordinary parameter is defined");
        return false;
    }
    struct vtable *args = p->lvtbl->args;
    if (p->max_numparam < n) {
        p->max_numparam = n;
    }
    while (n > args->pos) {
        vtable_add(args, NUMPARAM_IDX_TO_ID(args->pos+1));
    }
    return true;
}
#endif

static enum yytokentype
parse_atmark(struct parser_params *p, const enum lex_state_e last_state)
{
    const char *ptr = p->lex.pcur;
    enum yytokentype result = tIVAR;
    register int c = nextc(p);
    YYLTYPE loc;

    p->lex.ptok = ptr - 1; /* from '@' */
    newtok(p);
    tokadd(p, '@');
    if (c == '@') {
        result = tCVAR;
        tokadd(p, '@');
        c = nextc(p);
    }
    SET_LEX_STATE(IS_lex_state_for(last_state, EXPR_FNAME) ? EXPR_ENDFN : EXPR_END);
    if (c == -1 || !parser_is_identchar(p)) {
        pushback(p, c);
        RUBY_SET_YYLLOC(loc);
        if (result == tIVAR) {
            compile_error(p, "`@' without identifiers is not allowed as an instance variable name");
        }
        else {
            compile_error(p, "`@@' without identifiers is not allowed as a class variable name");
        }
        parser_show_error_line(p, &loc);
        set_yylval_noname();
        SET_LEX_STATE(EXPR_END);
        return result;
    }
    else if (ISDIGIT(c)) {
        pushback(p, c);
        RUBY_SET_YYLLOC(loc);
        if (result == tIVAR) {
            compile_error(p, "`@%c' is not allowed as an instance variable name", c);
        }
        else {
            compile_error(p, "`@@%c' is not allowed as a class variable name", c);
        }
        parser_show_error_line(p, &loc);
        set_yylval_noname();
        SET_LEX_STATE(EXPR_END);
        return result;
    }

    if (tokadd_ident(p, c)) return 0;
    tokenize_ident(p);
    return result;
}

static enum yytokentype
parse_ident(struct parser_params *p, int c, int cmd_state)
{
    enum yytokentype result;
    int mb = ENC_CODERANGE_7BIT;
    const enum lex_state_e last_state = p->lex.state;
    ID ident;
    int enforce_keyword_end = 0;

    do {
        if (!ISASCII(c)) mb = ENC_CODERANGE_UNKNOWN;
        if (tokadd_mbchar(p, c) == -1) return 0;
        c = nextc(p);
    } while (parser_is_identchar(p));
    if ((c == '!' || c == '?') && !peek(p, '=')) {
        result = tFID;
        tokadd(p, c);
    }
    else if (c == '=' && IS_lex_state(EXPR_FNAME) &&
             (!peek(p, '~') && !peek(p, '>') && (!peek(p, '=') || (peek_n(p, '>', 1))))) {
        result = tIDENTIFIER;
        tokadd(p, c);
    }
    else {
        result = tCONSTANT;	/* assume provisionally */
        pushback(p, c);
    }
    tokfix(p);

    if (IS_LABEL_POSSIBLE()) {
        if (IS_LABEL_SUFFIX(0)) {
            SET_LEX_STATE(EXPR_ARG|EXPR_LABELED);
            nextc(p);
            set_yylval_name(TOK_INTERN());
            return tLABEL;
        }
    }

#ifndef RIPPER
    if (!NIL_P(peek_end_expect_token_locations(p))) {
        VALUE end_loc;
        int lineno, column;
        int beg_pos = (int)(p->lex.ptok - p->lex.pbeg);

        end_loc = peek_end_expect_token_locations(p);
        lineno = NUM2INT(rb_ary_entry(end_loc, 0));
        column = NUM2INT(rb_ary_entry(end_loc, 1));

        if (p->debug) {
            rb_parser_printf(p, "enforce_keyword_end check. current: (%d, %d), peek: (%d, %d)\n",
                                p->ruby_sourceline, beg_pos, lineno, column);
        }

        if ((p->ruby_sourceline > lineno) && (beg_pos <= column)) {
            const struct kwtable *kw;

            if ((IS_lex_state(EXPR_DOT)) && (kw = rb_reserved_word(tok(p), toklen(p))) && (kw && kw->id[0] == keyword_end)) {
                if (p->debug) rb_parser_printf(p, "enforce_keyword_end is enabled\n");
                enforce_keyword_end = 1;
            }
        }
    }
#endif

    if (mb == ENC_CODERANGE_7BIT && (!IS_lex_state(EXPR_DOT) || enforce_keyword_end)) {
        const struct kwtable *kw;

        /* See if it is a reserved word.  */
        kw = rb_reserved_word(tok(p), toklen(p));
        if (kw) {
            enum lex_state_e state = p->lex.state;
            if (IS_lex_state_for(state, EXPR_FNAME)) {
                SET_LEX_STATE(EXPR_ENDFN);
                set_yylval_name(rb_intern2(tok(p), toklen(p)));
                return kw->id[0];
            }
            SET_LEX_STATE(kw->state);
            if (IS_lex_state(EXPR_BEG)) {
                p->command_start = TRUE;
            }
            if (kw->id[0] == keyword_do) {
                if (lambda_beginning_p()) {
                    p->lex.lpar_beg = -1; /* make lambda_beginning_p() == FALSE in the body of "-> do ... end" */
                    return keyword_do_LAMBDA;
                }
                if (COND_P()) return keyword_do_cond;
                if (CMDARG_P() && !IS_lex_state_for(state, EXPR_CMDARG))
                    return keyword_do_block;
                return keyword_do;
            }
            if (IS_lex_state_for(state, (EXPR_BEG | EXPR_LABELED | EXPR_CLASS)))
                return kw->id[0];
            else {
                if (kw->id[0] != kw->id[1])
                    SET_LEX_STATE(EXPR_BEG | EXPR_LABEL);
                return kw->id[1];
            }
        }
    }

    if (IS_lex_state(EXPR_BEG_ANY | EXPR_ARG_ANY | EXPR_DOT)) {
        if (cmd_state) {
            SET_LEX_STATE(EXPR_CMDARG);
        }
        else {
            SET_LEX_STATE(EXPR_ARG);
        }
    }
    else if (p->lex.state == EXPR_FNAME) {
        SET_LEX_STATE(EXPR_ENDFN);
    }
    else {
        SET_LEX_STATE(EXPR_END);
    }

    ident = tokenize_ident(p);
    if (result == tCONSTANT && is_local_id(ident)) result = tIDENTIFIER;
    if (!IS_lex_state_for(last_state, EXPR_DOT|EXPR_FNAME) &&
        (result == tIDENTIFIER) && /* not EXPR_FNAME, not attrasgn */
        (lvar_defined(p, ident) || NUMPARAM_ID_P(ident))) {
        SET_LEX_STATE(EXPR_END|EXPR_LABEL);
    }
    return result;
}

static void
warn_cr(struct parser_params *p)
{
    if (!p->cr_seen) {
        p->cr_seen = TRUE;
        /* carried over with p->lex.nextline for nextc() */
        rb_warn0("encountered \\r in middle of line, treated as a mere space");
    }
}

static enum yytokentype
parser_yylex(struct parser_params *p)
{
    register int c;
    int space_seen = 0;
    int cmd_state;
    int label;
    enum lex_state_e last_state;
    int fallthru = FALSE;
    int token_seen = p->token_seen;

    if (p->lex.strterm) {
        if (strterm_is_heredoc(p->lex.strterm)) {
            token_flush(p);
            return here_document(p, &p->lex.strterm->u.heredoc);
        }
        else {
            token_flush(p);
            return parse_string(p, &p->lex.strterm->u.literal);
        }
    }
    cmd_state = p->command_start;
    p->command_start = FALSE;
    p->token_seen = TRUE;
#ifndef RIPPER
    token_flush(p);
#endif
  retry:
    last_state = p->lex.state;
    switch (c = nextc(p)) {
      case '\0':		/* NUL */
      case '\004':		/* ^D */
      case '\032':		/* ^Z */
      case -1:			/* end of script. */
        p->eofp  = 1;
#ifndef RIPPER
        if (!NIL_P(p->end_expect_token_locations) && RARRAY_LEN(p->end_expect_token_locations) > 0) {
            pop_end_expect_token_locations(p);
            RUBY_SET_YYLLOC_OF_DUMMY_END(*p->yylloc);
            return tDUMNY_END;
        }
#endif
        /* Set location for end-of-input because dispatch_scan_event is not called. */
        RUBY_SET_YYLLOC(*p->yylloc);
        return END_OF_INPUT;

        /* white spaces */
      case '\r':
        warn_cr(p);
        /* fall through */
      case ' ': case '\t': case '\f':
      case '\13': /* '\v' */
        space_seen = 1;
        while ((c = nextc(p))) {
            switch (c) {
              case '\r':
                warn_cr(p);
                /* fall through */
              case ' ': case '\t': case '\f':
              case '\13': /* '\v' */
                break;
              default:
                goto outofloop;
            }
        }
      outofloop:
        pushback(p, c);
        dispatch_scan_event(p, tSP);
#ifndef RIPPER
        token_flush(p);
#endif
        goto retry;

      case '#':		/* it's a comment */
        p->token_seen = token_seen;
        /* no magic_comment in shebang line */
        if (!parser_magic_comment(p, p->lex.pcur, p->lex.pend - p->lex.pcur)) {
            if (comment_at_top(p)) {
                set_file_encoding(p, p->lex.pcur, p->lex.pend);
            }
        }
        lex_goto_eol(p);
        dispatch_scan_event(p, tCOMMENT);
        fallthru = TRUE;
        /* fall through */
      case '\n':
        p->token_seen = token_seen;
        VALUE prevline = p->lex.lastline;
        c = (IS_lex_state(EXPR_BEG|EXPR_CLASS|EXPR_FNAME|EXPR_DOT) &&
             !IS_lex_state(EXPR_LABELED));
        if (c || IS_lex_state_all(EXPR_ARG|EXPR_LABELED)) {
            if (!fallthru) {
                dispatch_scan_event(p, tIGNORED_NL);
            }
            fallthru = FALSE;
            if (!c && p->ctxt.in_kwarg) {
                goto normal_newline;
            }
            goto retry;
        }
        while (1) {
            switch (c = nextc(p)) {
              case ' ': case '\t': case '\f': case '\r':
              case '\13': /* '\v' */
                space_seen = 1;
                break;
              case '#':
                pushback(p, c);
                if (space_seen) {
                    dispatch_scan_event(p, tSP);
                    token_flush(p);
                }
                goto retry;
              case '&':
              case '.': {
                dispatch_delayed_token(p, tIGNORED_NL);
                if (peek(p, '.') == (c == '&')) {
                    pushback(p, c);
                    dispatch_scan_event(p, tSP);
                    goto retry;
                }
              }
              default:
                p->ruby_sourceline--;
                p->lex.nextline = p->lex.lastline;
                set_lastline(p, prevline);
              case -1:		/* EOF no decrement*/
                lex_goto_eol(p);
                if (c != -1) {
                    token_flush(p);
                    RUBY_SET_YYLLOC(*p->yylloc);
                }
                goto normal_newline;
            }
        }
      normal_newline:
        p->command_start = TRUE;
        SET_LEX_STATE(EXPR_BEG);
        return '\n';

      case '*':
        if ((c = nextc(p)) == '*') {
            if ((c = nextc(p)) == '=') {
                set_yylval_id(idPow);
                SET_LEX_STATE(EXPR_BEG);
                return tOP_ASGN;
            }
            pushback(p, c);
            if (IS_SPCARG(c)) {
                rb_warning0("`**' interpreted as argument prefix");
                c = tDSTAR;
            }
            else if (IS_BEG()) {
                c = tDSTAR;
            }
            else {
                c = warn_balanced((enum ruby_method_ids)tPOW, "**", "argument prefix");
            }
        }
        else {
            if (c == '=') {
                set_yylval_id('*');
                SET_LEX_STATE(EXPR_BEG);
                return tOP_ASGN;
            }
            pushback(p, c);
            if (IS_SPCARG(c)) {
                rb_warning0("`*' interpreted as argument prefix");
                c = tSTAR;
            }
            else if (IS_BEG()) {
                c = tSTAR;
            }
            else {
                c = warn_balanced('*', "*", "argument prefix");
            }
        }
        SET_LEX_STATE(IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG);
        return c;

      case '!':
        c = nextc(p);
        if (IS_AFTER_OPERATOR()) {
            SET_LEX_STATE(EXPR_ARG);
            if (c == '@') {
                return '!';
            }
        }
        else {
            SET_LEX_STATE(EXPR_BEG);
        }
        if (c == '=') {
            return tNEQ;
        }
        if (c == '~') {
            return tNMATCH;
        }
        pushback(p, c);
        return '!';

      case '=':
        if (was_bol(p)) {
            /* skip embedded rd document */
            if (word_match_p(p, "begin", 5)) {
                int first_p = TRUE;

                lex_goto_eol(p);
                dispatch_scan_event(p, tEMBDOC_BEG);
                for (;;) {
                    lex_goto_eol(p);
                    if (!first_p) {
                        dispatch_scan_event(p, tEMBDOC);
                    }
                    first_p = FALSE;
                    c = nextc(p);
                    if (c == -1) {
                        compile_error(p, "embedded document meets end of file");
                        return END_OF_INPUT;
                    }
                    if (c == '=' && word_match_p(p, "end", 3)) {
                        break;
                    }
                    pushback(p, c);
                }
                lex_goto_eol(p);
                dispatch_scan_event(p, tEMBDOC_END);
                goto retry;
            }
        }

        SET_LEX_STATE(IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG);
        if ((c = nextc(p)) == '=') {
            if ((c = nextc(p)) == '=') {
                return tEQQ;
            }
            pushback(p, c);
            return tEQ;
        }
        if (c == '~') {
            return tMATCH;
        }
        else if (c == '>') {
            return tASSOC;
        }
        pushback(p, c);
        return '=';

      case '<':
        c = nextc(p);
        if (c == '<' &&
            !IS_lex_state(EXPR_DOT | EXPR_CLASS) &&
            !IS_END() &&
            (!IS_ARG() || IS_lex_state(EXPR_LABELED) || space_seen)) {
            enum  yytokentype token = heredoc_identifier(p);
            if (token) return token < 0 ? 0 : token;
        }
        if (IS_AFTER_OPERATOR()) {
            SET_LEX_STATE(EXPR_ARG);
        }
        else {
            if (IS_lex_state(EXPR_CLASS))
                p->command_start = TRUE;
            SET_LEX_STATE(EXPR_BEG);
        }
        if (c == '=') {
            if ((c = nextc(p)) == '>') {
                return tCMP;
            }
            pushback(p, c);
            return tLEQ;
        }
        if (c == '<') {
            if ((c = nextc(p)) == '=') {
                set_yylval_id(idLTLT);
                SET_LEX_STATE(EXPR_BEG);
                return tOP_ASGN;
            }
            pushback(p, c);
            return warn_balanced((enum ruby_method_ids)tLSHFT, "<<", "here document");
        }
        pushback(p, c);
        return '<';

      case '>':
        SET_LEX_STATE(IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG);
        if ((c = nextc(p)) == '=') {
            return tGEQ;
        }
        if (c == '>') {
            if ((c = nextc(p)) == '=') {
                set_yylval_id(idGTGT);
                SET_LEX_STATE(EXPR_BEG);
                return tOP_ASGN;
            }
            pushback(p, c);
            return tRSHFT;
        }
        pushback(p, c);
        return '>';

      case '"':
        label = (IS_LABEL_POSSIBLE() ? str_label : 0);
        p->lex.strterm = NEW_STRTERM(str_dquote | label, '"', 0);
        p->lex.ptok = p->lex.pcur-1;
        return tSTRING_BEG;

      case '`':
        if (IS_lex_state(EXPR_FNAME)) {
            SET_LEX_STATE(EXPR_ENDFN);
            return c;
        }
        if (IS_lex_state(EXPR_DOT)) {
            if (cmd_state)
                SET_LEX_STATE(EXPR_CMDARG);
            else
                SET_LEX_STATE(EXPR_ARG);
            return c;
        }
        p->lex.strterm = NEW_STRTERM(str_xquote, '`', 0);
        return tXSTRING_BEG;

      case '\'':
        label = (IS_LABEL_POSSIBLE() ? str_label : 0);
        p->lex.strterm = NEW_STRTERM(str_squote | label, '\'', 0);
        p->lex.ptok = p->lex.pcur-1;
        return tSTRING_BEG;

      case '?':
        return parse_qmark(p, space_seen);

      case '&':
        if ((c = nextc(p)) == '&') {
            SET_LEX_STATE(EXPR_BEG);
            if ((c = nextc(p)) == '=') {
                set_yylval_id(idANDOP);
                SET_LEX_STATE(EXPR_BEG);
                return tOP_ASGN;
            }
            pushback(p, c);
            return tANDOP;
        }
        else if (c == '=') {
            set_yylval_id('&');
            SET_LEX_STATE(EXPR_BEG);
            return tOP_ASGN;
        }
        else if (c == '.') {
            set_yylval_id(idANDDOT);
            SET_LEX_STATE(EXPR_DOT);
            return tANDDOT;
        }
        pushback(p, c);
        if (IS_SPCARG(c)) {
            if ((c != ':') ||
                (c = peekc_n(p, 1)) == -1 ||
                !(c == '\'' || c == '"' ||
                  is_identchar(p, (p->lex.pcur+1), p->lex.pend, p->enc))) {
                rb_warning0("`&' interpreted as argument prefix");
            }
            c = tAMPER;
        }
        else if (IS_BEG()) {
            c = tAMPER;
        }
        else {
            c = warn_balanced('&', "&", "argument prefix");
        }
        SET_LEX_STATE(IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG);
        return c;

      case '|':
        if ((c = nextc(p)) == '|') {
            SET_LEX_STATE(EXPR_BEG);
            if ((c = nextc(p)) == '=') {
                set_yylval_id(idOROP);
                SET_LEX_STATE(EXPR_BEG);
                return tOP_ASGN;
            }
            pushback(p, c);
            if (IS_lex_state_for(last_state, EXPR_BEG)) {
                c = '|';
                pushback(p, '|');
                return c;
            }
            return tOROP;
        }
        if (c == '=') {
            set_yylval_id('|');
            SET_LEX_STATE(EXPR_BEG);
            return tOP_ASGN;
        }
        SET_LEX_STATE(IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG|EXPR_LABEL);
        pushback(p, c);
        return '|';

      case '+':
        c = nextc(p);
        if (IS_AFTER_OPERATOR()) {
            SET_LEX_STATE(EXPR_ARG);
            if (c == '@') {
                return tUPLUS;
            }
            pushback(p, c);
            return '+';
        }
        if (c == '=') {
            set_yylval_id('+');
            SET_LEX_STATE(EXPR_BEG);
            return tOP_ASGN;
        }
        if (IS_BEG() || (IS_SPCARG(c) && arg_ambiguous(p, '+'))) {
            SET_LEX_STATE(EXPR_BEG);
            pushback(p, c);
            if (c != -1 && ISDIGIT(c)) {
                return parse_numeric(p, '+');
            }
            return tUPLUS;
        }
        SET_LEX_STATE(EXPR_BEG);
        pushback(p, c);
        return warn_balanced('+', "+", "unary operator");

      case '-':
        c = nextc(p);
        if (IS_AFTER_OPERATOR()) {
            SET_LEX_STATE(EXPR_ARG);
            if (c == '@') {
                return tUMINUS;
            }
            pushback(p, c);
            return '-';
        }
        if (c == '=') {
            set_yylval_id('-');
            SET_LEX_STATE(EXPR_BEG);
            return tOP_ASGN;
        }
        if (c == '>') {
            SET_LEX_STATE(EXPR_ENDFN);
            return tLAMBDA;
        }
        if (IS_BEG() || (IS_SPCARG(c) && arg_ambiguous(p, '-'))) {
            SET_LEX_STATE(EXPR_BEG);
            pushback(p, c);
            if (c != -1 && ISDIGIT(c)) {
                return tUMINUS_NUM;
            }
            return tUMINUS;
        }
        SET_LEX_STATE(EXPR_BEG);
        pushback(p, c);
        return warn_balanced('-', "-", "unary operator");

      case '.': {
        int is_beg = IS_BEG();
        SET_LEX_STATE(EXPR_BEG);
        if ((c = nextc(p)) == '.') {
            if ((c = nextc(p)) == '.') {
                if (p->ctxt.in_argdef) {
                    SET_LEX_STATE(EXPR_ENDARG);
                    return tBDOT3;
                }
                if (p->lex.paren_nest == 0 && looking_at_eol_p(p)) {
                    rb_warn0("... at EOL, should be parenthesized?");
                }
                else if (p->lex.lpar_beg >= 0 && p->lex.lpar_beg+1 == p->lex.paren_nest) {
                    if (IS_lex_state_for(last_state, EXPR_LABEL))
                        return tDOT3;
                }
                return is_beg ? tBDOT3 : tDOT3;
            }
            pushback(p, c);
            return is_beg ? tBDOT2 : tDOT2;
        }
        pushback(p, c);
        if (c != -1 && ISDIGIT(c)) {
            char prev = p->lex.pcur-1 > p->lex.pbeg ? *(p->lex.pcur-2) : 0;
            parse_numeric(p, '.');
            if (ISDIGIT(prev)) {
                yyerror0("unexpected fraction part after numeric literal");
            }
            else {
                yyerror0("no .<digit> floating literal anymore; put 0 before dot");
            }
            SET_LEX_STATE(EXPR_END);
            p->lex.ptok = p->lex.pcur;
            goto retry;
        }
        set_yylval_id('.');
        SET_LEX_STATE(EXPR_DOT);
        return '.';
      }

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        return parse_numeric(p, c);

      case ')':
        COND_POP();
        CMDARG_POP();
        SET_LEX_STATE(EXPR_ENDFN);
        p->lex.paren_nest--;
        return c;

      case ']':
        COND_POP();
        CMDARG_POP();
        SET_LEX_STATE(EXPR_END);
        p->lex.paren_nest--;
        return c;

      case '}':
        /* tSTRING_DEND does COND_POP and CMDARG_POP in the yacc's rule */
        if (!p->lex.brace_nest--) return tSTRING_DEND;
        COND_POP();
        CMDARG_POP();
        SET_LEX_STATE(EXPR_END);
        p->lex.paren_nest--;
        return c;

      case ':':
        c = nextc(p);
        if (c == ':') {
            if (IS_BEG() || IS_lex_state(EXPR_CLASS) || IS_SPCARG(-1)) {
                SET_LEX_STATE(EXPR_BEG);
                return tCOLON3;
            }
            set_yylval_id(idCOLON2);
            SET_LEX_STATE(EXPR_DOT);
            return tCOLON2;
        }
        if (IS_END() || ISSPACE(c) || c == '#') {
            pushback(p, c);
            c = warn_balanced(':', ":", "symbol literal");
            SET_LEX_STATE(EXPR_BEG);
            return c;
        }
        switch (c) {
          case '\'':
            p->lex.strterm = NEW_STRTERM(str_ssym, c, 0);
            break;
          case '"':
            p->lex.strterm = NEW_STRTERM(str_dsym, c, 0);
            break;
          default:
            pushback(p, c);
            break;
        }
        SET_LEX_STATE(EXPR_FNAME);
        return tSYMBEG;

      case '/':
        if (IS_BEG()) {
            p->lex.strterm = NEW_STRTERM(str_regexp, '/', 0);
            return tREGEXP_BEG;
        }
        if ((c = nextc(p)) == '=') {
            set_yylval_id('/');
            SET_LEX_STATE(EXPR_BEG);
            return tOP_ASGN;
        }
        pushback(p, c);
        if (IS_SPCARG(c)) {
            arg_ambiguous(p, '/');
            p->lex.strterm = NEW_STRTERM(str_regexp, '/', 0);
            return tREGEXP_BEG;
        }
        SET_LEX_STATE(IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG);
        return warn_balanced('/', "/", "regexp literal");

      case '^':
        if ((c = nextc(p)) == '=') {
            set_yylval_id('^');
            SET_LEX_STATE(EXPR_BEG);
            return tOP_ASGN;
        }
        SET_LEX_STATE(IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG);
        pushback(p, c);
        return '^';

      case ';':
        SET_LEX_STATE(EXPR_BEG);
        p->command_start = TRUE;
        return ';';

      case ',':
        SET_LEX_STATE(EXPR_BEG|EXPR_LABEL);
        return ',';

      case '~':
        if (IS_AFTER_OPERATOR()) {
            if ((c = nextc(p)) != '@') {
                pushback(p, c);
            }
            SET_LEX_STATE(EXPR_ARG);
        }
        else {
            SET_LEX_STATE(EXPR_BEG);
        }
        return '~';

      case '(':
        if (IS_BEG()) {
            c = tLPAREN;
        }
        else if (!space_seen) {
            /* foo( ... ) => method call, no ambiguity */
        }
        else if (IS_ARG() || IS_lex_state_all(EXPR_END|EXPR_LABEL)) {
            c = tLPAREN_ARG;
        }
        else if (IS_lex_state(EXPR_ENDFN) && !lambda_beginning_p()) {
            rb_warning0("parentheses after method name is interpreted as "
                        "an argument list, not a decomposed argument");
        }
        p->lex.paren_nest++;
        COND_PUSH(0);
        CMDARG_PUSH(0);
        SET_LEX_STATE(EXPR_BEG|EXPR_LABEL);
        return c;

      case '[':
        p->lex.paren_nest++;
        if (IS_AFTER_OPERATOR()) {
            if ((c = nextc(p)) == ']') {
                p->lex.paren_nest--;
                SET_LEX_STATE(EXPR_ARG);
                if ((c = nextc(p)) == '=') {
                    return tASET;
                }
                pushback(p, c);
                return tAREF;
            }
            pushback(p, c);
            SET_LEX_STATE(EXPR_ARG|EXPR_LABEL);
            return '[';
        }
        else if (IS_BEG()) {
            c = tLBRACK;
        }
        else if (IS_ARG() && (space_seen || IS_lex_state(EXPR_LABELED))) {
            c = tLBRACK;
        }
        SET_LEX_STATE(EXPR_BEG|EXPR_LABEL);
        COND_PUSH(0);
        CMDARG_PUSH(0);
        return c;

      case '{':
        ++p->lex.brace_nest;
        if (lambda_beginning_p())
            c = tLAMBEG;
        else if (IS_lex_state(EXPR_LABELED))
            c = tLBRACE;      /* hash */
        else if (IS_lex_state(EXPR_ARG_ANY | EXPR_END | EXPR_ENDFN))
            c = '{';          /* block (primary) */
        else if (IS_lex_state(EXPR_ENDARG))
            c = tLBRACE_ARG;  /* block (expr) */
        else
            c = tLBRACE;      /* hash */
        if (c != tLBRACE) {
            p->command_start = TRUE;
            SET_LEX_STATE(EXPR_BEG);
        }
        else {
            SET_LEX_STATE(EXPR_BEG|EXPR_LABEL);
        }
        ++p->lex.paren_nest;  /* after lambda_beginning_p() */
        COND_PUSH(0);
        CMDARG_PUSH(0);
        return c;

      case '\\':
        c = nextc(p);
        if (c == '\n') {
            space_seen = 1;
            dispatch_scan_event(p, tSP);
            goto retry; /* skip \\n */
        }
        if (c == ' ') return tSP;
        if (ISSPACE(c)) return c;
        pushback(p, c);
        return '\\';

      case '%':
        return parse_percent(p, space_seen, last_state);

      case '$':
        return parse_gvar(p, last_state);

      case '@':
        return parse_atmark(p, last_state);

      case '_':
        if (was_bol(p) && whole_match_p(p, "__END__", 7, 0)) {
            p->ruby__end__seen = 1;
            p->eofp = 1;
#ifdef RIPPER
            lex_goto_eol(p);
            dispatch_scan_event(p, k__END__);
#endif
            return END_OF_INPUT;
        }
        newtok(p);
        break;

      default:
        if (!parser_is_identchar(p)) {
            compile_error(p, "Invalid char `\\x%02X' in expression", c);
            token_flush(p);
            goto retry;
        }

        newtok(p);
        break;
    }

    return parse_ident(p, c, cmd_state);
}

static enum yytokentype
yylex(YYSTYPE *lval, YYLTYPE *yylloc, struct parser_params *p)
{
    enum yytokentype t;

    p->lval = lval;
    lval->val = Qundef;
    p->yylloc = yylloc;

    t = parser_yylex(p);

    if (has_delayed_token(p))
        dispatch_delayed_token(p, t);
    else if (t != END_OF_INPUT)
        dispatch_scan_event(p, t);

    return t;
}

#define LVAR_USED ((ID)1 << (sizeof(ID) * CHAR_BIT - 1))

static NODE*
node_new_internal(struct parser_params *p, enum node_type type, size_t size, size_t alignment)
{
    NODE *n = rb_ast_newnode(p->ast, type, size, alignment);

    rb_node_init(n, type);
    return n;
}

static NODE *
nd_set_loc(NODE *nd, const YYLTYPE *loc)
{
    nd->nd_loc = *loc;
    nd_set_line(nd, loc->beg_pos.lineno);
    return nd;
}

static NODE*
node_newnode(struct parser_params *p, enum node_type type, size_t size, size_t alignment, const rb_code_location_t *loc)
{
    NODE *n = node_new_internal(p, type, size, alignment);

    nd_set_loc(n, loc);
    nd_set_node_id(n, parser_get_node_id(p));
    return n;
}

#define NODE_NEWNODE(node_type, type, loc) (type *)(node_newnode(p, node_type, sizeof(type), RUBY_ALIGNOF(type), loc))

#ifndef RIPPER

static rb_node_scope_t *
rb_node_scope_new(struct parser_params *p, rb_node_args_t *nd_args, NODE *nd_body, const YYLTYPE *loc)
{
    rb_ast_id_table_t *nd_tbl;
    nd_tbl = local_tbl(p);
    rb_node_scope_t *n = NODE_NEWNODE(NODE_SCOPE, rb_node_scope_t, loc);
    n->nd_tbl = nd_tbl;
    n->nd_body = nd_body;
    n->nd_args = nd_args;

    return n;
}

static rb_node_scope_t *
rb_node_scope_new2(struct parser_params *p, rb_ast_id_table_t *nd_tbl, rb_node_args_t *nd_args, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_scope_t *n = NODE_NEWNODE(NODE_SCOPE, rb_node_scope_t, loc);
    n->nd_tbl = nd_tbl;
    n->nd_body = nd_body;
    n->nd_args = nd_args;

    return n;
}

static rb_node_defn_t *
rb_node_defn_new(struct parser_params *p, ID nd_mid, NODE *nd_defn, const YYLTYPE *loc)
{
    rb_node_defn_t *n = NODE_NEWNODE(NODE_DEFN, rb_node_defn_t, loc);
    n->nd_mid = nd_mid;
    n->nd_defn = nd_defn;

    return n;
}

static rb_node_defs_t *
rb_node_defs_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *nd_defn, const YYLTYPE *loc)
{
    rb_node_defs_t *n = NODE_NEWNODE(NODE_DEFS, rb_node_defs_t, loc);
    n->nd_recv = nd_recv;
    n->nd_mid = nd_mid;
    n->nd_defn = nd_defn;

    return n;
}

static rb_node_block_t *
rb_node_block_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc)
{
    rb_node_block_t *n = NODE_NEWNODE(NODE_BLOCK, rb_node_block_t, loc);
    n->nd_head = nd_head;
    n->nd_end = (NODE *)n;
    n->nd_next = 0;

    return n;
}

static rb_node_for_t *
rb_node_for_new(struct parser_params *p, NODE *nd_iter, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_for_t *n = NODE_NEWNODE(NODE_FOR, rb_node_for_t, loc);
    n->nd_body = nd_body;
    n->nd_iter = nd_iter;

    return n;
}

static rb_node_for_masgn_t *
rb_node_for_masgn_new(struct parser_params *p, NODE *nd_var, const YYLTYPE *loc)
{
    rb_node_for_masgn_t *n = NODE_NEWNODE(NODE_FOR_MASGN, rb_node_for_masgn_t, loc);
    n->nd_var = nd_var;

    return n;
}

static rb_node_retry_t *
rb_node_retry_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_retry_t *n = NODE_NEWNODE(NODE_RETRY, rb_node_retry_t, loc);

    return n;
}

static rb_node_begin_t *
rb_node_begin_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_begin_t *n = NODE_NEWNODE(NODE_BEGIN, rb_node_begin_t, loc);
    n->nd_body = nd_body;

    return n;
}

static rb_node_rescue_t *
rb_node_rescue_new(struct parser_params *p, NODE *nd_head, NODE *nd_resq, NODE *nd_else, const YYLTYPE *loc)
{
    rb_node_rescue_t *n = NODE_NEWNODE(NODE_RESCUE, rb_node_rescue_t, loc);
    n->nd_head = nd_head;
    n->nd_resq = nd_resq;
    n->nd_else = nd_else;

    return n;
}

static rb_node_resbody_t *
rb_node_resbody_new(struct parser_params *p, NODE *nd_args, NODE *nd_body, NODE *nd_head, const YYLTYPE *loc)
{
    rb_node_resbody_t *n = NODE_NEWNODE(NODE_RESBODY, rb_node_resbody_t, loc);
    n->nd_head = nd_head;
    n->nd_body = nd_body;
    n->nd_args = nd_args;

    return n;
}

static rb_node_ensure_t *
rb_node_ensure_new(struct parser_params *p, NODE *nd_head, NODE *nd_ensr, const YYLTYPE *loc)
{
    rb_node_ensure_t *n = NODE_NEWNODE(NODE_ENSURE, rb_node_ensure_t, loc);
    n->nd_head = nd_head;
    n->nd_resq = 0;
    n->nd_ensr = nd_ensr;

    return n;
}

static rb_node_and_t *
rb_node_and_new(struct parser_params *p, NODE *nd_1st, NODE *nd_2nd, const YYLTYPE *loc)
{
    rb_node_and_t *n = NODE_NEWNODE(NODE_AND, rb_node_and_t, loc);
    n->nd_1st = nd_1st;
    n->nd_2nd = nd_2nd;

    return n;
}

static rb_node_or_t *
rb_node_or_new(struct parser_params *p, NODE *nd_1st, NODE *nd_2nd, const YYLTYPE *loc)
{
    rb_node_or_t *n = NODE_NEWNODE(NODE_OR, rb_node_or_t, loc);
    n->nd_1st = nd_1st;
    n->nd_2nd = nd_2nd;

    return n;
}

static rb_node_return_t *
rb_node_return_new(struct parser_params *p, NODE *nd_stts, const YYLTYPE *loc)
{
    rb_node_return_t *n = NODE_NEWNODE(NODE_RETURN, rb_node_return_t, loc);
    n->nd_stts = nd_stts;
    return n;
}

static rb_node_yield_t *
rb_node_yield_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc)
{
    rb_node_yield_t *n = NODE_NEWNODE(NODE_YIELD, rb_node_yield_t, loc);
    n->nd_head = nd_head;

    return n;
}

static rb_node_if_t *
rb_node_if_new(struct parser_params *p, NODE *nd_cond, NODE *nd_body, NODE *nd_else, const YYLTYPE *loc)
{
    rb_node_if_t *n = NODE_NEWNODE(NODE_IF, rb_node_if_t, loc);
    n->nd_cond = nd_cond;
    n->nd_body = nd_body;
    n->nd_else = nd_else;

    return n;
}

static rb_node_unless_t *
rb_node_unless_new(struct parser_params *p, NODE *nd_cond, NODE *nd_body, NODE *nd_else, const YYLTYPE *loc)
{
    rb_node_unless_t *n = NODE_NEWNODE(NODE_UNLESS, rb_node_unless_t, loc);
    n->nd_cond = nd_cond;
    n->nd_body = nd_body;
    n->nd_else = nd_else;

    return n;
}

static rb_node_class_t *
rb_node_class_new(struct parser_params *p, NODE *nd_cpath, NODE *nd_body, NODE *nd_super, const YYLTYPE *loc)
{
    /* Keep the order of node creation */
    NODE *scope = NEW_SCOPE(0, nd_body, loc);
    rb_node_class_t *n = NODE_NEWNODE(NODE_CLASS, rb_node_class_t, loc);
    n->nd_cpath = nd_cpath;
    n->nd_body = scope;
    n->nd_super = nd_super;

    return n;
}

static rb_node_sclass_t *
rb_node_sclass_new(struct parser_params *p, NODE *nd_recv, NODE *nd_body, const YYLTYPE *loc)
{
    /* Keep the order of node creation */
    NODE *scope = NEW_SCOPE(0, nd_body, loc);
    rb_node_sclass_t *n = NODE_NEWNODE(NODE_SCLASS, rb_node_sclass_t, loc);
    n->nd_recv = nd_recv;
    n->nd_body = scope;

    return n;
}

static rb_node_module_t *
rb_node_module_new(struct parser_params *p, NODE *nd_cpath, NODE *nd_body, const YYLTYPE *loc)
{
    /* Keep the order of node creation */
    NODE *scope = NEW_SCOPE(0, nd_body, loc);
    rb_node_module_t *n = NODE_NEWNODE(NODE_MODULE, rb_node_module_t, loc);
    n->nd_cpath = nd_cpath;
    n->nd_body = scope;

    return n;
}

static rb_node_iter_t *
rb_node_iter_new(struct parser_params *p, rb_node_args_t *nd_args, NODE *nd_body, const YYLTYPE *loc)
{
    /* Keep the order of node creation */
    NODE *scope = NEW_SCOPE(nd_args, nd_body, loc);
    rb_node_iter_t *n = NODE_NEWNODE(NODE_ITER, rb_node_iter_t, loc);
    n->nd_body = scope;
    n->nd_iter = 0;

    return n;
}

static rb_node_lambda_t *
rb_node_lambda_new(struct parser_params *p, rb_node_args_t *nd_args, NODE *nd_body, const YYLTYPE *loc)
{
    /* Keep the order of node creation */
    NODE *scope = NEW_SCOPE(nd_args, nd_body, loc);
    rb_node_lambda_t *n = NODE_NEWNODE(NODE_LAMBDA, rb_node_lambda_t, loc);
    n->nd_body = scope;

    return n;
}

static rb_node_case_t *
rb_node_case_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_case_t *n = NODE_NEWNODE(NODE_CASE, rb_node_case_t, loc);
    n->nd_head = nd_head;
    n->nd_body = nd_body;

    return n;
}

static rb_node_case2_t *
rb_node_case2_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_case2_t *n = NODE_NEWNODE(NODE_CASE2, rb_node_case2_t, loc);
    n->nd_head = 0;
    n->nd_body = nd_body;

    return n;
}

static rb_node_case3_t *
rb_node_case3_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_case3_t *n = NODE_NEWNODE(NODE_CASE3, rb_node_case3_t, loc);
    n->nd_head = nd_head;
    n->nd_body = nd_body;

    return n;
}

static rb_node_when_t *
rb_node_when_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, NODE *nd_next, const YYLTYPE *loc)
{
    rb_node_when_t *n = NODE_NEWNODE(NODE_WHEN, rb_node_when_t, loc);
    n->nd_head = nd_head;
    n->nd_body = nd_body;
    n->nd_next = nd_next;

    return n;
}

static rb_node_in_t *
rb_node_in_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, NODE *nd_next, const YYLTYPE *loc)
{
    rb_node_in_t *n = NODE_NEWNODE(NODE_IN, rb_node_in_t, loc);
    n->nd_head = nd_head;
    n->nd_body = nd_body;
    n->nd_next = nd_next;

    return n;
}

static rb_node_while_t *
rb_node_while_new(struct parser_params *p, NODE *nd_cond, NODE *nd_body, long nd_state, const YYLTYPE *loc)
{
    rb_node_while_t *n = NODE_NEWNODE(NODE_WHILE, rb_node_while_t, loc);
    n->nd_cond = nd_cond;
    n->nd_body = nd_body;
    n->nd_state = nd_state;

    return n;
}

static rb_node_until_t *
rb_node_until_new(struct parser_params *p, NODE *nd_cond, NODE *nd_body, long nd_state, const YYLTYPE *loc)
{
    rb_node_until_t *n = NODE_NEWNODE(NODE_UNTIL, rb_node_until_t, loc);
    n->nd_cond = nd_cond;
    n->nd_body = nd_body;
    n->nd_state = nd_state;

    return n;
}

static rb_node_colon2_t *
rb_node_colon2_new(struct parser_params *p, NODE *nd_head, ID nd_mid, const YYLTYPE *loc)
{
    rb_node_colon2_t *n = NODE_NEWNODE(NODE_COLON2, rb_node_colon2_t, loc);
    n->nd_head = nd_head;
    n->nd_mid = nd_mid;

    return n;
}

static rb_node_colon3_t *
rb_node_colon3_new(struct parser_params *p, ID nd_mid, const YYLTYPE *loc)
{
    rb_node_colon3_t *n = NODE_NEWNODE(NODE_COLON3, rb_node_colon3_t, loc);
    n->nd_mid = nd_mid;

    return n;
}

static rb_node_dot2_t *
rb_node_dot2_new(struct parser_params *p, NODE *nd_beg, NODE *nd_end, const YYLTYPE *loc)
{
    rb_node_dot2_t *n = NODE_NEWNODE(NODE_DOT2, rb_node_dot2_t, loc);
    n->nd_beg = nd_beg;
    n->nd_end = nd_end;

    return n;
}

static rb_node_dot3_t *
rb_node_dot3_new(struct parser_params *p, NODE *nd_beg, NODE *nd_end, const YYLTYPE *loc)
{
    rb_node_dot3_t *n = NODE_NEWNODE(NODE_DOT3, rb_node_dot3_t, loc);
    n->nd_beg = nd_beg;
    n->nd_end = nd_end;

    return n;
}

static rb_node_self_t *
rb_node_self_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_self_t *n = NODE_NEWNODE(NODE_SELF, rb_node_self_t, loc);
    n->nd_state = 1;

    return n;
}

static rb_node_nil_t *
rb_node_nil_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_nil_t *n = NODE_NEWNODE(NODE_NIL, rb_node_nil_t, loc);

    return n;
}

static rb_node_true_t *
rb_node_true_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_true_t *n = NODE_NEWNODE(NODE_TRUE, rb_node_true_t, loc);

    return n;
}

static rb_node_false_t *
rb_node_false_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_false_t *n = NODE_NEWNODE(NODE_FALSE, rb_node_false_t, loc);

    return n;
}

static rb_node_super_t *
rb_node_super_new(struct parser_params *p, NODE *nd_args, const YYLTYPE *loc)
{
    rb_node_super_t *n = NODE_NEWNODE(NODE_SUPER, rb_node_super_t, loc);
    n->nd_args = nd_args;

    return n;
}

static rb_node_zsuper_t *
rb_node_zsuper_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_zsuper_t *n = NODE_NEWNODE(NODE_ZSUPER, rb_node_zsuper_t, loc);

    return n;
}

static rb_node_match2_t *
rb_node_match2_new(struct parser_params *p, NODE *nd_recv, NODE *nd_value, const YYLTYPE *loc)
{
    rb_node_match2_t *n = NODE_NEWNODE(NODE_MATCH2, rb_node_match2_t, loc);
    n->nd_recv = nd_recv;
    n->nd_value = nd_value;
    n->nd_args = 0;

    return n;
}

static rb_node_match3_t *
rb_node_match3_new(struct parser_params *p, NODE *nd_recv, NODE *nd_value, const YYLTYPE *loc)
{
    rb_node_match3_t *n = NODE_NEWNODE(NODE_MATCH3, rb_node_match3_t, loc);
    n->nd_recv = nd_recv;
    n->nd_value = nd_value;

    return n;
}

/* TODO: Use union for NODE_LIST2 */
static rb_node_list_t *
rb_node_list_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc)
{
    rb_node_list_t *n = NODE_NEWNODE(NODE_LIST, rb_node_list_t, loc);
    n->nd_head = nd_head;
    n->as.nd_alen = 1;
    n->nd_next = 0;

    return n;
}

static rb_node_list_t *
rb_node_list_new2(struct parser_params *p, NODE *nd_head, long nd_alen, NODE *nd_next, const YYLTYPE *loc)
{
    rb_node_list_t *n = NODE_NEWNODE(NODE_LIST, rb_node_list_t, loc);
    n->nd_head = nd_head;
    n->as.nd_alen = nd_alen;
    n->nd_next = nd_next;

    return n;
}

static rb_node_zlist_t *
rb_node_zlist_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_zlist_t *n = NODE_NEWNODE(NODE_ZLIST, rb_node_zlist_t, loc);

    return n;
}

static rb_node_hash_t *
rb_node_hash_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc)
{
    rb_node_hash_t *n = NODE_NEWNODE(NODE_HASH, rb_node_hash_t, loc);
    n->nd_head = nd_head;
    n->nd_brace = 0;

    return n;
}

static rb_node_masgn_t *
rb_node_masgn_new(struct parser_params *p, NODE *nd_head, NODE *nd_args, const YYLTYPE *loc)
{
    rb_node_masgn_t *n = NODE_NEWNODE(NODE_MASGN, rb_node_masgn_t, loc);
    n->nd_head = nd_head;
    n->nd_value = 0;
    n->nd_args = nd_args;

    return n;
}

static rb_node_gasgn_t *
rb_node_gasgn_new(struct parser_params *p, ID nd_vid, NODE *nd_value, const YYLTYPE *loc)
{
    rb_node_gasgn_t *n = NODE_NEWNODE(NODE_GASGN, rb_node_gasgn_t, loc);
    n->nd_vid = nd_vid;
    n->nd_value = nd_value;

    return n;
}

static rb_node_lasgn_t *
rb_node_lasgn_new(struct parser_params *p, ID nd_vid, NODE *nd_value, const YYLTYPE *loc)
{
    rb_node_lasgn_t *n = NODE_NEWNODE(NODE_LASGN, rb_node_lasgn_t, loc);
    n->nd_vid = nd_vid;
    n->nd_value = nd_value;

    return n;
}

static rb_node_dasgn_t *
rb_node_dasgn_new(struct parser_params *p, ID nd_vid, NODE *nd_value, const YYLTYPE *loc)
{
    rb_node_dasgn_t *n = NODE_NEWNODE(NODE_DASGN, rb_node_dasgn_t, loc);
    n->nd_vid = nd_vid;
    n->nd_value = nd_value;

    return n;
}

static rb_node_iasgn_t *
rb_node_iasgn_new(struct parser_params *p, ID nd_vid, NODE *nd_value, const YYLTYPE *loc)
{
    rb_node_iasgn_t *n = NODE_NEWNODE(NODE_IASGN, rb_node_iasgn_t, loc);
    n->nd_vid = nd_vid;
    n->nd_value = nd_value;

    return n;
}

static rb_node_cvasgn_t *
rb_node_cvasgn_new(struct parser_params *p, ID nd_vid, NODE *nd_value, const YYLTYPE *loc)
{
    rb_node_cvasgn_t *n = NODE_NEWNODE(NODE_CVASGN, rb_node_cvasgn_t, loc);
    n->nd_vid = nd_vid;
    n->nd_value = nd_value;

    return n;
}

static rb_node_op_asgn1_t *
rb_node_op_asgn1_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *index, NODE *rvalue, const YYLTYPE *loc)
{
    rb_node_op_asgn1_t *n = NODE_NEWNODE(NODE_OP_ASGN1, rb_node_op_asgn1_t, loc);
    n->nd_recv = nd_recv;
    n->nd_mid = nd_mid;
    n->nd_index = index;
    n->nd_rvalue = rvalue;

    return n;
}

static rb_node_op_asgn2_t *
rb_node_op_asgn2_new(struct parser_params *p, NODE *nd_recv, NODE *nd_value, ID nd_vid, ID nd_mid, bool nd_aid, const YYLTYPE *loc)
{
    rb_node_op_asgn2_t *n = NODE_NEWNODE(NODE_OP_ASGN2, rb_node_op_asgn2_t, loc);
    n->nd_recv = nd_recv;
    n->nd_value = nd_value;
    n->nd_vid = nd_vid;
    n->nd_mid = nd_mid;
    n->nd_aid = nd_aid;

    return n;
}

static rb_node_op_asgn_or_t *
rb_node_op_asgn_or_new(struct parser_params *p, NODE *nd_head, NODE *nd_value, const YYLTYPE *loc)
{
    rb_node_op_asgn_or_t *n = NODE_NEWNODE(NODE_OP_ASGN_OR, rb_node_op_asgn_or_t, loc);
    n->nd_head = nd_head;
    n->nd_value = nd_value;

    return n;
}

static rb_node_op_asgn_and_t *
rb_node_op_asgn_and_new(struct parser_params *p, NODE *nd_head, NODE *nd_value, const YYLTYPE *loc)
{
    rb_node_op_asgn_and_t *n = NODE_NEWNODE(NODE_OP_ASGN_AND, rb_node_op_asgn_and_t, loc);
    n->nd_head = nd_head;
    n->nd_value = nd_value;

    return n;
}

static rb_node_gvar_t *
rb_node_gvar_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc)
{
    rb_node_gvar_t *n = NODE_NEWNODE(NODE_GVAR, rb_node_gvar_t, loc);
    n->nd_vid = nd_vid;

    return n;
}

static rb_node_lvar_t *
rb_node_lvar_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc)
{
    rb_node_lvar_t *n = NODE_NEWNODE(NODE_LVAR, rb_node_lvar_t, loc);
    n->nd_vid = nd_vid;

    return n;
}

static rb_node_dvar_t *
rb_node_dvar_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc)
{
    rb_node_dvar_t *n = NODE_NEWNODE(NODE_DVAR, rb_node_dvar_t, loc);
    n->nd_vid = nd_vid;

    return n;
}

static rb_node_ivar_t *
rb_node_ivar_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc)
{
    rb_node_ivar_t *n = NODE_NEWNODE(NODE_IVAR, rb_node_ivar_t, loc);
    n->nd_vid = nd_vid;

    return n;
}

static rb_node_const_t *
rb_node_const_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc)
{
    rb_node_const_t *n = NODE_NEWNODE(NODE_CONST, rb_node_const_t, loc);
    n->nd_vid = nd_vid;

    return n;
}

static rb_node_cvar_t *
rb_node_cvar_new(struct parser_params *p, ID nd_vid, const YYLTYPE *loc)
{
    rb_node_cvar_t *n = NODE_NEWNODE(NODE_CVAR, rb_node_cvar_t, loc);
    n->nd_vid = nd_vid;

    return n;
}

static rb_node_nth_ref_t *
rb_node_nth_ref_new(struct parser_params *p, long nd_nth, const YYLTYPE *loc)
{
    rb_node_nth_ref_t *n = NODE_NEWNODE(NODE_NTH_REF, rb_node_nth_ref_t, loc);
    n->nd_nth = nd_nth;

    return n;
}

static rb_node_back_ref_t *
rb_node_back_ref_new(struct parser_params *p, long nd_nth, const YYLTYPE *loc)
{
    rb_node_back_ref_t *n = NODE_NEWNODE(NODE_BACK_REF, rb_node_back_ref_t, loc);
    n->nd_nth = nd_nth;

    return n;
}

static rb_node_lit_t *
rb_node_lit_new(struct parser_params *p, VALUE nd_lit, const YYLTYPE *loc)
{
    rb_node_lit_t *n = NODE_NEWNODE(NODE_LIT, rb_node_lit_t, loc);
    n->nd_lit = nd_lit;

    return n;
}

static rb_node_str_t *
rb_node_str_new(struct parser_params *p, VALUE nd_lit, const YYLTYPE *loc)
{
    rb_node_str_t *n = NODE_NEWNODE(NODE_STR, rb_node_str_t, loc);
    n->nd_lit = nd_lit;

    return n;
}

/* TODO; Use union for NODE_DSTR2 */
static rb_node_dstr_t *
rb_node_dstr_new0(struct parser_params *p, VALUE nd_lit, long nd_alen, NODE *nd_next, const YYLTYPE *loc)
{
    rb_node_dstr_t *n = NODE_NEWNODE(NODE_DSTR, rb_node_dstr_t, loc);
    n->nd_lit = nd_lit;
    n->as.nd_alen = nd_alen;
    n->nd_next = (rb_node_list_t *)nd_next;

    return n;
}

static rb_node_dstr_t *
rb_node_dstr_new(struct parser_params *p, VALUE nd_lit, const YYLTYPE *loc)
{
    return rb_node_dstr_new0(p, nd_lit, 1, 0, loc);
}

static rb_node_xstr_t *
rb_node_xstr_new(struct parser_params *p, VALUE nd_lit, const YYLTYPE *loc)
{
    rb_node_xstr_t *n = NODE_NEWNODE(NODE_XSTR, rb_node_xstr_t, loc);
    n->nd_lit = nd_lit;

    return n;
}

static rb_node_dxstr_t *
rb_node_dxstr_new(struct parser_params *p, VALUE nd_lit, long nd_alen, NODE *nd_next, const YYLTYPE *loc)
{
    rb_node_dxstr_t *n = NODE_NEWNODE(NODE_DXSTR, rb_node_dxstr_t, loc);
    n->nd_lit = nd_lit;
    n->nd_alen = nd_alen;
    n->nd_next = (rb_node_list_t *)nd_next;

    return n;
}

static rb_node_dsym_t *
rb_node_dsym_new(struct parser_params *p, VALUE nd_lit, long nd_alen, NODE *nd_next, const YYLTYPE *loc)
{
    rb_node_dsym_t *n = NODE_NEWNODE(NODE_DSYM, rb_node_dsym_t, loc);
    n->nd_lit = nd_lit;
    n->nd_alen = nd_alen;
    n->nd_next = (rb_node_list_t *)nd_next;

    return n;
}

static rb_node_evstr_t *
rb_node_evstr_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_evstr_t *n = NODE_NEWNODE(NODE_EVSTR, rb_node_evstr_t, loc);
    n->nd_body = nd_body;

    return n;
}

static rb_node_call_t *
rb_node_call_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *nd_args, const YYLTYPE *loc)
{
    rb_node_call_t *n = NODE_NEWNODE(NODE_CALL, rb_node_call_t, loc);
    n->nd_recv = nd_recv;
    n->nd_mid = nd_mid;
    n->nd_args = nd_args;

    return n;
}

static rb_node_opcall_t *
rb_node_opcall_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *nd_args, const YYLTYPE *loc)
{
    rb_node_opcall_t *n = NODE_NEWNODE(NODE_OPCALL, rb_node_opcall_t, loc);
    n->nd_recv = nd_recv;
    n->nd_mid = nd_mid;
    n->nd_args = nd_args;

    return n;
}

static rb_node_fcall_t *
rb_node_fcall_new(struct parser_params *p, ID nd_mid, NODE *nd_args, const YYLTYPE *loc)
{
    rb_node_fcall_t *n = NODE_NEWNODE(NODE_FCALL, rb_node_fcall_t, loc);
    n->nd_mid = nd_mid;
    n->nd_args = nd_args;

    return n;
}

static rb_node_qcall_t *
rb_node_qcall_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *nd_args, const YYLTYPE *loc)
{
    rb_node_qcall_t *n = NODE_NEWNODE(NODE_QCALL, rb_node_qcall_t, loc);
    n->nd_recv = nd_recv;
    n->nd_mid = nd_mid;
    n->nd_args = nd_args;

    return n;
}

static rb_node_vcall_t *
rb_node_vcall_new(struct parser_params *p, ID nd_mid, const YYLTYPE *loc)
{
    rb_node_vcall_t *n = NODE_NEWNODE(NODE_VCALL, rb_node_vcall_t, loc);
    n->nd_mid = nd_mid;

    return n;
}

static rb_node_once_t *
rb_node_once_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_once_t *n = NODE_NEWNODE(NODE_ONCE, rb_node_once_t, loc);
    n->nd_body = nd_body;

    return n;
}

static rb_node_args_t *
rb_node_args_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_args_t *n = NODE_NEWNODE(NODE_ARGS, rb_node_args_t, loc);
    MEMZERO(&n->nd_ainfo, struct rb_args_info, 1);

    return n;
}

static rb_node_args_aux_t *
rb_node_args_aux_new(struct parser_params *p, ID nd_pid, long nd_plen, const YYLTYPE *loc)
{
    rb_node_args_aux_t *n = NODE_NEWNODE(NODE_ARGS_AUX, rb_node_args_aux_t, loc);
    n->nd_pid = nd_pid;
    n->nd_plen = nd_plen;
    n->nd_next = 0;

    return n;
}

static rb_node_opt_arg_t *
rb_node_opt_arg_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_opt_arg_t *n = NODE_NEWNODE(NODE_OPT_ARG, rb_node_opt_arg_t, loc);
    n->nd_body = nd_body;
    n->nd_next = 0;

    return n;
}

static rb_node_kw_arg_t *
rb_node_kw_arg_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_kw_arg_t *n = NODE_NEWNODE(NODE_KW_ARG, rb_node_kw_arg_t, loc);
    n->nd_body = nd_body;
    n->nd_next = 0;

    return n;
}

static rb_node_postarg_t *
rb_node_postarg_new(struct parser_params *p, NODE *nd_1st, NODE *nd_2nd, const YYLTYPE *loc)
{
    rb_node_postarg_t *n = NODE_NEWNODE(NODE_POSTARG, rb_node_postarg_t, loc);
    n->nd_1st = nd_1st;
    n->nd_2nd = nd_2nd;

    return n;
}

static rb_node_argscat_t *
rb_node_argscat_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_argscat_t *n = NODE_NEWNODE(NODE_ARGSCAT, rb_node_argscat_t, loc);
    n->nd_head = nd_head;
    n->nd_body = nd_body;

    return n;
}

static rb_node_argspush_t *
rb_node_argspush_new(struct parser_params *p, NODE *nd_head, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_argspush_t *n = NODE_NEWNODE(NODE_ARGSPUSH, rb_node_argspush_t, loc);
    n->nd_head = nd_head;
    n->nd_body = nd_body;

    return n;
}

static rb_node_splat_t *
rb_node_splat_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc)
{
    rb_node_splat_t *n = NODE_NEWNODE(NODE_SPLAT, rb_node_splat_t, loc);
    n->nd_head = nd_head;

    return n;
}

static rb_node_block_pass_t *
rb_node_block_pass_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_block_pass_t *n = NODE_NEWNODE(NODE_BLOCK_PASS, rb_node_block_pass_t, loc);
    n->nd_head = 0;
    n->nd_body = nd_body;

    return n;
}

static rb_node_alias_t *
rb_node_alias_new(struct parser_params *p, NODE *nd_1st, NODE *nd_2nd, const YYLTYPE *loc)
{
    rb_node_alias_t *n = NODE_NEWNODE(NODE_ALIAS, rb_node_alias_t, loc);
    n->nd_1st = nd_1st;
    n->nd_2nd = nd_2nd;

    return n;
}

static rb_node_valias_t *
rb_node_valias_new(struct parser_params *p, ID nd_alias, ID nd_orig, const YYLTYPE *loc)
{
    rb_node_valias_t *n = NODE_NEWNODE(NODE_VALIAS, rb_node_valias_t, loc);
    n->nd_alias = nd_alias;
    n->nd_orig = nd_orig;

    return n;
}

static rb_node_undef_t *
rb_node_undef_new(struct parser_params *p, NODE *nd_undef, const YYLTYPE *loc)
{
    rb_node_undef_t *n = NODE_NEWNODE(NODE_UNDEF, rb_node_undef_t, loc);
    n->nd_undef = nd_undef;

    return n;
}

static rb_node_errinfo_t *
rb_node_errinfo_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_errinfo_t *n = NODE_NEWNODE(NODE_ERRINFO, rb_node_errinfo_t, loc);

    return n;
}

static rb_node_defined_t *
rb_node_defined_new(struct parser_params *p, NODE *nd_head, const YYLTYPE *loc)
{
    rb_node_defined_t *n = NODE_NEWNODE(NODE_DEFINED, rb_node_defined_t, loc);
    n->nd_head = nd_head;

    return n;
}

static rb_node_postexe_t *
rb_node_postexe_new(struct parser_params *p, NODE *nd_body, const YYLTYPE *loc)
{
    rb_node_postexe_t *n = NODE_NEWNODE(NODE_POSTEXE, rb_node_postexe_t, loc);
    n->nd_body = nd_body;

    return n;
}

static rb_node_attrasgn_t *
rb_node_attrasgn_new(struct parser_params *p, NODE *nd_recv, ID nd_mid, NODE *nd_args, const YYLTYPE *loc)
{
    rb_node_attrasgn_t *n = NODE_NEWNODE(NODE_ATTRASGN, rb_node_attrasgn_t, loc);
    n->nd_recv = nd_recv;
    n->nd_mid = nd_mid;
    n->nd_args = nd_args;

    return n;
}

static rb_node_aryptn_t *
rb_node_aryptn_new(struct parser_params *p, NODE *pre_args, NODE *rest_arg, NODE *post_args, const YYLTYPE *loc)
{
    rb_node_aryptn_t *n = NODE_NEWNODE(NODE_ARYPTN, rb_node_aryptn_t, loc);
    n->nd_pconst = 0;
    n->pre_args = pre_args;
    n->rest_arg = rest_arg;
    n->post_args = post_args;

    return n;
}

static rb_node_hshptn_t *
rb_node_hshptn_new(struct parser_params *p, NODE *nd_pconst, NODE *nd_pkwargs, NODE *nd_pkwrestarg, const YYLTYPE *loc)
{
    rb_node_hshptn_t *n = NODE_NEWNODE(NODE_HSHPTN, rb_node_hshptn_t, loc);
    n->nd_pconst = nd_pconst;
    n->nd_pkwargs = nd_pkwargs;
    n->nd_pkwrestarg = nd_pkwrestarg;

    return n;
}

static rb_node_fndptn_t *
rb_node_fndptn_new(struct parser_params *p, NODE *pre_rest_arg, NODE *args, NODE *post_rest_arg, const YYLTYPE *loc)
{
    rb_node_fndptn_t *n = NODE_NEWNODE(NODE_FNDPTN, rb_node_fndptn_t, loc);
    n->nd_pconst = 0;
    n->pre_rest_arg = pre_rest_arg;
    n->args = args;
    n->post_rest_arg = post_rest_arg;

    return n;
}

static rb_node_cdecl_t *
rb_node_cdecl_new(struct parser_params *p, ID nd_vid, NODE *nd_value, NODE *nd_else, const YYLTYPE *loc)
{
    rb_node_cdecl_t *n = NODE_NEWNODE(NODE_CDECL, rb_node_cdecl_t, loc);
    n->nd_vid = nd_vid;
    n->nd_value = nd_value;
    n->nd_else = nd_else;

    return n;
}

static rb_node_op_cdecl_t *
rb_node_op_cdecl_new(struct parser_params *p, NODE *nd_head, NODE *nd_value, ID nd_aid, const YYLTYPE *loc)
{
    rb_node_op_cdecl_t *n = NODE_NEWNODE(NODE_OP_CDECL, rb_node_op_cdecl_t, loc);
    n->nd_head = nd_head;
    n->nd_value = nd_value;
    n->nd_aid = nd_aid;

    return n;
}

static rb_node_error_t *
rb_node_error_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_error_t *n = NODE_NEWNODE(NODE_ERROR, rb_node_error_t, loc);

    return n;
}

#else

static rb_node_ripper_t *
rb_node_ripper_new(struct parser_params *p, ID nd_vid, VALUE nd_rval, VALUE nd_cval, const YYLTYPE *loc)
{
    rb_node_ripper_t *n = NODE_NEWNODE(NODE_RIPPER, rb_node_ripper_t, loc);
    n->nd_vid = nd_vid;
    n->nd_rval = nd_rval;
    n->nd_cval = nd_cval;

    return n;
}

static rb_node_ripper_values_t *
rb_node_ripper_values_new(struct parser_params *p, VALUE nd_val1, VALUE nd_val2, VALUE nd_val3, const YYLTYPE *loc)
{
    rb_node_ripper_values_t *n = NODE_NEWNODE(NODE_RIPPER_VALUES, rb_node_ripper_values_t, loc);
    n->nd_val1 = nd_val1;
    n->nd_val2 = nd_val2;
    n->nd_val3 = nd_val3;

    return n;
}

#endif

static rb_node_break_t *
rb_node_break_new(struct parser_params *p, NODE *nd_stts, const YYLTYPE *loc)
{
    rb_node_break_t *n = NODE_NEWNODE(NODE_BREAK, rb_node_break_t, loc);
    n->nd_stts = nd_stts;
    n->nd_chain = 0;

    return n;
}

static rb_node_next_t *
rb_node_next_new(struct parser_params *p, NODE *nd_stts, const YYLTYPE *loc)
{
    rb_node_next_t *n = NODE_NEWNODE(NODE_NEXT, rb_node_next_t, loc);
    n->nd_stts = nd_stts;
    n->nd_chain = 0;

    return n;
}

static rb_node_redo_t *
rb_node_redo_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_redo_t *n = NODE_NEWNODE(NODE_REDO, rb_node_redo_t, loc);
    n->nd_chain = 0;

    return n;
}

static rb_node_def_temp_t *
rb_node_def_temp_new(struct parser_params *p, const YYLTYPE *loc)
{
    rb_node_def_temp_t *n = NODE_NEWNODE((enum node_type)NODE_DEF_TEMP, rb_node_def_temp_t, loc);
    n->save.cur_arg = p->cur_arg;
    n->save.numparam_save = 0;
    n->save.max_numparam = 0;
    n->save.ctxt = p->ctxt;
#ifdef RIPPER
    n->nd_recv = Qnil;
    n->nd_mid = Qnil;
    n->dot_or_colon = Qnil;
#else
    n->nd_def = 0;
    n->nd_mid = 0;
#endif

    return n;
}

static rb_node_def_temp_t *
def_head_save(struct parser_params *p, rb_node_def_temp_t *n)
{
    n->save.numparam_save = numparam_push(p);
    n->save.max_numparam = p->max_numparam;
    return n;
}

#ifndef RIPPER
static enum node_type
nodetype(NODE *node)			/* for debug */
{
    return (enum node_type)nd_type(node);
}

static int
nodeline(NODE *node)
{
    return nd_line(node);
}

static NODE*
newline_node(NODE *node)
{
    if (node) {
        node = remove_begin(node);
        nd_set_fl_newline(node);
    }
    return node;
}

static void
fixpos(NODE *node, NODE *orig)
{
    if (!node) return;
    if (!orig) return;
    nd_set_line(node, nd_line(orig));
}

static void
parser_warning(struct parser_params *p, NODE *node, const char *mesg)
{
    rb_compile_warning(p->ruby_sourcefile, nd_line(node), "%s", mesg);
}

static void
parser_warn(struct parser_params *p, NODE *node, const char *mesg)
{
    rb_compile_warn(p->ruby_sourcefile, nd_line(node), "%s", mesg);
}

static NODE*
block_append(struct parser_params *p, NODE *head, NODE *tail)
{
    NODE *end, *h = head, *nd;

    if (tail == 0) return head;

    if (h == 0) return tail;
    switch (nd_type(h)) {
      default:
        h = end = NEW_BLOCK(head, &head->nd_loc);
        head = end;
        break;
      case NODE_BLOCK:
        end = RNODE_BLOCK(h)->nd_end;
        break;
    }

    nd = RNODE_BLOCK(end)->nd_head;
    switch (nd_type(nd)) {
      case NODE_RETURN:
      case NODE_BREAK:
      case NODE_NEXT:
      case NODE_REDO:
      case NODE_RETRY:
        if (RTEST(ruby_verbose)) {
            parser_warning(p, tail, "statement not reached");
        }
        break;

      default:
        break;
    }

    if (!nd_type_p(tail, NODE_BLOCK)) {
        tail = NEW_BLOCK(tail, &tail->nd_loc);
    }
    RNODE_BLOCK(end)->nd_next = tail;
    RNODE_BLOCK(h)->nd_end = RNODE_BLOCK(tail)->nd_end;
    nd_set_last_loc(head, nd_last_loc(tail));
    return head;
}

/* append item to the list */
static NODE*
list_append(struct parser_params *p, NODE *list, NODE *item)
{
    NODE *last;

    if (list == 0) return NEW_LIST(item, &item->nd_loc);
    if (RNODE_LIST(list)->nd_next) {
        last = RNODE_LIST(RNODE_LIST(list)->nd_next)->as.nd_end;
    }
    else {
        last = list;
    }

    RNODE_LIST(list)->as.nd_alen += 1;
    RNODE_LIST(last)->nd_next = NEW_LIST(item, &item->nd_loc);
    RNODE_LIST(RNODE_LIST(list)->nd_next)->as.nd_end = RNODE_LIST(last)->nd_next;

    nd_set_last_loc(list, nd_last_loc(item));

    return list;
}

/* concat two lists */
static NODE*
list_concat(NODE *head, NODE *tail)
{
    NODE *last;

    if (RNODE_LIST(head)->nd_next) {
        last = RNODE_LIST(RNODE_LIST(head)->nd_next)->as.nd_end;
    }
    else {
        last = head;
    }

    RNODE_LIST(head)->as.nd_alen += RNODE_LIST(tail)->as.nd_alen;
    RNODE_LIST(last)->nd_next = tail;
    if (RNODE_LIST(tail)->nd_next) {
        RNODE_LIST(RNODE_LIST(head)->nd_next)->as.nd_end = RNODE_LIST(RNODE_LIST(tail)->nd_next)->as.nd_end;
    }
    else {
        RNODE_LIST(RNODE_LIST(head)->nd_next)->as.nd_end = tail;
    }

    nd_set_last_loc(head, nd_last_loc(tail));

    return head;
}

static int
literal_concat0(struct parser_params *p, VALUE head, VALUE tail)
{
    if (NIL_P(tail)) return 1;
    if (!rb_enc_compatible(head, tail)) {
        compile_error(p, "string literal encodings differ (%s / %s)",
                      rb_enc_name(rb_enc_get(head)),
                      rb_enc_name(rb_enc_get(tail)));
        rb_str_resize(head, 0);
        rb_str_resize(tail, 0);
        return 0;
    }
    rb_str_buf_append(head, tail);
    return 1;
}

static VALUE
string_literal_head(struct parser_params *p, enum node_type htype, NODE *head)
{
    if (htype != NODE_DSTR) return Qfalse;
    if (RNODE_DSTR(head)->nd_next) {
        head = RNODE_LIST(RNODE_LIST(RNODE_DSTR(head)->nd_next)->as.nd_end)->nd_head;
        if (!head || !nd_type_p(head, NODE_STR)) return Qfalse;
    }
    const VALUE lit = RNODE_DSTR(head)->nd_lit;
    ASSUME(lit != Qfalse);
    return lit;
}

/* concat two string literals */
static NODE *
literal_concat(struct parser_params *p, NODE *head, NODE *tail, const YYLTYPE *loc)
{
    enum node_type htype;
    VALUE lit;

    if (!head) return tail;
    if (!tail) return head;

    htype = nd_type(head);
    if (htype == NODE_EVSTR) {
        head = new_dstr(p, head, loc);
        htype = NODE_DSTR;
    }
    if (p->heredoc_indent > 0) {
        switch (htype) {
          case NODE_STR:
            head = str2dstr(p, head);
          case NODE_DSTR:
            return list_append(p, head, tail);
          default:
            break;
        }
    }
    switch (nd_type(tail)) {
      case NODE_STR:
        if ((lit = string_literal_head(p, htype, head)) != Qfalse) {
            htype = NODE_STR;
        }
        else {
            lit = RNODE_DSTR(head)->nd_lit;
        }
        if (htype == NODE_STR) {
            if (!literal_concat0(p, lit, RNODE_STR(tail)->nd_lit)) {
              error:
                rb_discard_node(p, head);
                rb_discard_node(p, tail);
                return 0;
            }
            rb_discard_node(p, tail);
        }
        else {
            list_append(p, head, tail);
        }
        break;

      case NODE_DSTR:
        if (htype == NODE_STR) {
            if (!literal_concat0(p, RNODE_STR(head)->nd_lit, RNODE_DSTR(tail)->nd_lit))
                goto error;
            RNODE_DSTR(tail)->nd_lit = RNODE_STR(head)->nd_lit;
            rb_discard_node(p, head);
            head = tail;
        }
        else if (NIL_P(RNODE_DSTR(tail)->nd_lit)) {
          append:
            RNODE_DSTR(head)->as.nd_alen += RNODE_DSTR(tail)->as.nd_alen - 1;
            if (!RNODE_DSTR(head)->nd_next) {
                RNODE_DSTR(head)->nd_next = RNODE_DSTR(tail)->nd_next;
            }
            else if (RNODE_DSTR(tail)->nd_next) {
                RNODE_DSTR(RNODE_DSTR(RNODE_DSTR(head)->nd_next)->as.nd_end)->nd_next = RNODE_DSTR(tail)->nd_next;
                RNODE_DSTR(RNODE_DSTR(head)->nd_next)->as.nd_end = RNODE_DSTR(RNODE_DSTR(tail)->nd_next)->as.nd_end;
            }
            rb_discard_node(p, tail);
        }
        else if ((lit = string_literal_head(p, htype, head)) != Qfalse) {
            if (!literal_concat0(p, lit, RNODE_DSTR(tail)->nd_lit))
                goto error;
            RNODE_DSTR(tail)->nd_lit = Qnil;
            goto append;
        }
        else {
            list_concat(head, NEW_LIST2(NEW_STR(RNODE_DSTR(tail)->nd_lit, loc), RNODE_DSTR(tail)->as.nd_alen, (NODE *)RNODE_DSTR(tail)->nd_next, loc));
        }
        break;

      case NODE_EVSTR:
        if (htype == NODE_STR) {
            head = str2dstr(p, head);
            RNODE_DSTR(head)->as.nd_alen = 1;
        }
        list_append(p, head, tail);
        break;
    }
    return head;
}

static void
nd_copy_flag(NODE *new_node, NODE *old_node)
{
    if (nd_fl_newline(old_node)) nd_set_fl_newline(new_node);
    nd_set_line(new_node, nd_line(old_node));
    new_node->nd_loc = old_node->nd_loc;
    new_node->node_id = old_node->node_id;
}

static NODE *
str2dstr(struct parser_params *p, NODE *node)
{
    NODE *new_node = (NODE *)NODE_NEW_INTERNAL(NODE_DSTR, rb_node_dstr_t);
    nd_copy_flag(new_node, node);
    RNODE_DSTR(new_node)->nd_lit = RNODE_STR(node)->nd_lit;
    RNODE_DSTR(new_node)->as.nd_alen = 0;
    RNODE_DSTR(new_node)->nd_next = 0;
    RNODE_STR(node)->nd_lit = 0;

    return new_node;
}

static NODE *
evstr2dstr(struct parser_params *p, NODE *node)
{
    if (nd_type_p(node, NODE_EVSTR)) {
        node = new_dstr(p, node, &node->nd_loc);
    }
    return node;
}

static NODE *
new_evstr(struct parser_params *p, NODE *node, const YYLTYPE *loc)
{
    NODE *head = node;

    if (node) {
        switch (nd_type(node)) {
          case NODE_STR:
            return str2dstr(p, node);
          case NODE_DSTR:
            break;
          case NODE_EVSTR:
            return node;
        }
    }
    return NEW_EVSTR(head, loc);
}

static NODE *
new_dstr(struct parser_params *p, NODE *node, const YYLTYPE *loc)
{
    VALUE lit = STR_NEW0();
    NODE *dstr = NEW_DSTR(lit, loc);
    RB_OBJ_WRITTEN(p->ast, Qnil, lit);
    return list_append(p, dstr, node);
}

static NODE *
call_bin_op(struct parser_params *p, NODE *recv, ID id, NODE *arg1,
                const YYLTYPE *op_loc, const YYLTYPE *loc)
{
    NODE *expr;
    value_expr(recv);
    value_expr(arg1);
    expr = NEW_OPCALL(recv, id, NEW_LIST(arg1, &arg1->nd_loc), loc);
    nd_set_line(expr, op_loc->beg_pos.lineno);
    return expr;
}

static NODE *
call_uni_op(struct parser_params *p, NODE *recv, ID id, const YYLTYPE *op_loc, const YYLTYPE *loc)
{
    NODE *opcall;
    value_expr(recv);
    opcall = NEW_OPCALL(recv, id, 0, loc);
    nd_set_line(opcall, op_loc->beg_pos.lineno);
    return opcall;
}

static NODE *
new_qcall(struct parser_params* p, ID atype, NODE *recv, ID mid, NODE *args, const YYLTYPE *op_loc, const YYLTYPE *loc)
{
    NODE *qcall = NEW_QCALL(atype, recv, mid, args, loc);
    nd_set_line(qcall, op_loc->beg_pos.lineno);
    return qcall;
}

static NODE*
new_command_qcall(struct parser_params* p, ID atype, NODE *recv, ID mid, NODE *args, NODE *block, const YYLTYPE *op_loc, const YYLTYPE *loc)
{
    NODE *ret;
    if (block) block_dup_check(p, args, block);
    ret = new_qcall(p, atype, recv, mid, args, op_loc, loc);
    if (block) ret = method_add_block(p, ret, block, loc);
    fixpos(ret, recv);
    return ret;
}

#define nd_once_body(node) (nd_type_p((node), NODE_ONCE) ? RNODE_ONCE(node)->nd_body : node)

static NODE*
last_expr_once_body(NODE *node)
{
    if (!node) return 0;
    return nd_once_body(node);
}

static NODE*
match_op(struct parser_params *p, NODE *node1, NODE *node2, const YYLTYPE *op_loc, const YYLTYPE *loc)
{
    NODE *n;
    int line = op_loc->beg_pos.lineno;

    value_expr(node1);
    value_expr(node2);

    if ((n = last_expr_once_body(node1)) != 0) {
        switch (nd_type(n)) {
          case NODE_DREGX:
            {
                NODE *match = NEW_MATCH2(node1, node2, loc);
                nd_set_line(match, line);
                return match;
            }

          case NODE_LIT:
            if (RB_TYPE_P(RNODE_LIT(n)->nd_lit, T_REGEXP)) {
                const VALUE lit = RNODE_LIT(n)->nd_lit;
                NODE *match = NEW_MATCH2(node1, node2, loc);
                RNODE_MATCH2(match)->nd_args = reg_named_capture_assign(p, lit, loc);
                nd_set_line(match, line);
                return match;
            }
        }
    }

    if ((n = last_expr_once_body(node2)) != 0) {
        NODE *match3;

        switch (nd_type(n)) {
          case NODE_LIT:
            if (!RB_TYPE_P(RNODE_LIT(n)->nd_lit, T_REGEXP)) break;
            /* fallthru */
          case NODE_DREGX:
            match3 = NEW_MATCH3(node2, node1, loc);
            return match3;
        }
    }

    n = NEW_CALL(node1, tMATCH, NEW_LIST(node2, &node2->nd_loc), loc);
    nd_set_line(n, line);
    return n;
}

# if WARN_PAST_SCOPE
static int
past_dvar_p(struct parser_params *p, ID id)
{
    struct vtable *past = p->lvtbl->past;
    while (past) {
        if (vtable_included(past, id)) return 1;
        past = past->prev;
    }
    return 0;
}
# endif

static int
numparam_nested_p(struct parser_params *p)
{
    struct local_vars *local = p->lvtbl;
    NODE *outer = local->numparam.outer;
    NODE *inner = local->numparam.inner;
    if (outer || inner) {
        NODE *used = outer ? outer : inner;
        compile_error(p, "numbered parameter is already used in\n"
                      "%s:%d: %s block here",
                      p->ruby_sourcefile, nd_line(used),
                      outer ? "outer" : "inner");
        parser_show_error_line(p, &used->nd_loc);
        return 1;
    }
    return 0;
}

static NODE*
gettable(struct parser_params *p, ID id, const YYLTYPE *loc)
{
    ID *vidp = NULL;
    NODE *node;
    switch (id) {
      case keyword_self:
        return NEW_SELF(loc);
      case keyword_nil:
        return NEW_NIL(loc);
      case keyword_true:
        return NEW_TRUE(loc);
      case keyword_false:
        return NEW_FALSE(loc);
      case keyword__FILE__:
        {
            VALUE file = p->ruby_sourcefile_string;
            if (NIL_P(file))
                file = rb_str_new(0, 0);
            else
                file = rb_str_dup(file);
            node = NEW_STR(file, loc);
            RB_OBJ_WRITTEN(p->ast, Qnil, file);
        }
        return node;
      case keyword__LINE__:
        return NEW_LIT(INT2FIX(loc->beg_pos.lineno), loc);
      case keyword__ENCODING__:
        node = NEW_LIT(rb_enc_from_encoding(p->enc), loc);
        RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_LIT(node)->nd_lit);
        return node;

    }
    switch (id_type(id)) {
      case ID_LOCAL:
        if (dyna_in_block(p) && dvar_defined_ref(p, id, &vidp)) {
            if (NUMPARAM_ID_P(id) && numparam_nested_p(p)) return 0;
            if (id == p->cur_arg) {
                compile_error(p, "circular argument reference - %"PRIsWARN, rb_id2str(id));
                return 0;
            }
            if (vidp) *vidp |= LVAR_USED;
            node = NEW_DVAR(id, loc);
            return node;
        }
        if (local_id_ref(p, id, &vidp)) {
            if (id == p->cur_arg) {
                compile_error(p, "circular argument reference - %"PRIsWARN, rb_id2str(id));
                return 0;
            }
            if (vidp) *vidp |= LVAR_USED;
            node = NEW_LVAR(id, loc);
            return node;
        }
        if (dyna_in_block(p) && NUMPARAM_ID_P(id) &&
            parser_numbered_param(p, NUMPARAM_ID_TO_IDX(id))) {
            if (numparam_nested_p(p)) return 0;
            node = NEW_DVAR(id, loc);
            struct local_vars *local = p->lvtbl;
            if (!local->numparam.current) local->numparam.current = node;
            return node;
        }
# if WARN_PAST_SCOPE
        if (!p->ctxt.in_defined && RTEST(ruby_verbose) && past_dvar_p(p, id)) {
            rb_warning1("possible reference to past scope - %"PRIsWARN, rb_id2str(id));
        }
# endif
        /* method call without arguments */
        if (dyna_in_block(p) && id == rb_intern("it")
            && !(DVARS_TERMINAL_P(p->lvtbl->args) || DVARS_TERMINAL_P(p->lvtbl->args->prev))
            && p->max_numparam != ORDINAL_PARAM) {
            rb_warn0("`it` calls without arguments will refer to the first block param in Ruby 3.4; use it() or self.it");
        }
        return NEW_VCALL(id, loc);
      case ID_GLOBAL:
        return NEW_GVAR(id, loc);
      case ID_INSTANCE:
        return NEW_IVAR(id, loc);
      case ID_CONST:
        return NEW_CONST(id, loc);
      case ID_CLASS:
        return NEW_CVAR(id, loc);
    }
    compile_error(p, "identifier %"PRIsVALUE" is not valid to get", rb_id2str(id));
    return 0;
}

static rb_node_opt_arg_t *
opt_arg_append(rb_node_opt_arg_t *opt_list, rb_node_opt_arg_t *opt)
{
    rb_node_opt_arg_t *opts = opt_list;
    RNODE(opts)->nd_loc.end_pos = RNODE(opt)->nd_loc.end_pos;

    while (opts->nd_next) {
        opts = opts->nd_next;
        RNODE(opts)->nd_loc.end_pos = RNODE(opt)->nd_loc.end_pos;
    }
    opts->nd_next = opt;

    return opt_list;
}

static rb_node_kw_arg_t *
kwd_append(rb_node_kw_arg_t *kwlist, rb_node_kw_arg_t *kw)
{
    if (kwlist) {
        /* Assume rb_node_kw_arg_t and rb_node_opt_arg_t has same structure */
        opt_arg_append(RNODE_OPT_ARG(kwlist), RNODE_OPT_ARG(kw));
    }
    return kwlist;
}

static NODE *
new_defined(struct parser_params *p, NODE *expr, const YYLTYPE *loc)
{
    NODE *n = expr;
    while (n) {
        if (nd_type_p(n, NODE_BEGIN)) {
            n = RNODE_BEGIN(n)->nd_body;
        }
        else if (nd_type_p(n, NODE_BLOCK) && RNODE_BLOCK(n)->nd_end == n) {
            n = RNODE_BLOCK(n)->nd_head;
        }
        else {
            break;
        }
    }
    return NEW_DEFINED(n, loc);
}

static VALUE
str_to_sym_check(struct parser_params *p, VALUE lit, const YYLTYPE *loc)
{
    if (rb_enc_str_coderange(lit) == ENC_CODERANGE_BROKEN) {
        yyerror1(loc, "invalid symbol");
        lit = STR_NEW0();
    }

    return lit;
}

static NODE*
symbol_append(struct parser_params *p, NODE *symbols, NODE *symbol)
{
    VALUE lit;

    enum node_type type = nd_type(symbol);
    switch (type) {
      case NODE_DSTR:
        nd_set_type(symbol, NODE_DSYM);
        break;
      case NODE_STR:
        nd_set_type(symbol, NODE_LIT);
        lit = str_to_sym_check(p, RNODE_LIT(symbol)->nd_lit, &RNODE(symbol)->nd_loc);
        RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_LIT(symbol)->nd_lit = rb_str_intern(lit));
        break;
      default:
        compile_error(p, "unexpected node as symbol: %s", parser_node_name(type));
    }
    return list_append(p, symbols, symbol);
}

static NODE *
new_regexp(struct parser_params *p, NODE *node, int options, const YYLTYPE *loc)
{
    struct RNode_LIST *list;
    NODE *prev;
    VALUE lit;

    if (!node) {
        node = NEW_LIT(reg_compile(p, STR_NEW0(), options), loc);
        RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_LIT(node)->nd_lit);
        return node;
    }
    switch (nd_type(node)) {
      case NODE_STR:
        {
            VALUE src = RNODE_STR(node)->nd_lit;
            nd_set_type(node, NODE_LIT);
            nd_set_loc(node, loc);
            RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_LIT(node)->nd_lit = reg_compile(p, src, options));
        }
        break;
      default:
        lit = STR_NEW0();
        node = NEW_DSTR0(lit, 1, NEW_LIST(node, loc), loc);
        RB_OBJ_WRITTEN(p->ast, Qnil, lit);
        /* fall through */
      case NODE_DSTR:
        nd_set_type(node, NODE_DREGX);
        nd_set_loc(node, loc);
        RNODE_DREGX(node)->nd_cflag = options & RE_OPTION_MASK;
        if (!NIL_P(RNODE_DREGX(node)->nd_lit)) reg_fragment_check(p, RNODE_DREGX(node)->nd_lit, options);
        for (list = RNODE_DREGX(prev = node)->nd_next; list; list = RNODE_LIST(list->nd_next)) {
            NODE *frag = list->nd_head;
            enum node_type type = nd_type(frag);
            if (type == NODE_STR || (type == NODE_DSTR && !RNODE_DSTR(frag)->nd_next)) {
                VALUE tail = RNODE_STR(frag)->nd_lit;
                if (reg_fragment_check(p, tail, options) && prev && !NIL_P(RNODE_DREGX(prev)->nd_lit)) {
                    VALUE lit = prev == node ? RNODE_DREGX(prev)->nd_lit : RNODE_LIT(RNODE_LIST(prev)->nd_head)->nd_lit;
                    if (!literal_concat0(p, lit, tail)) {
                        return NEW_NIL(loc); /* dummy node on error */
                    }
                    rb_str_resize(tail, 0);
                    RNODE_LIST(prev)->nd_next = list->nd_next;
                    rb_discard_node(p, list->nd_head);
                    rb_discard_node(p, (NODE *)list);
                    list = RNODE_LIST(prev);
                }
                else {
                    prev = (NODE *)list;
                }
            }
            else {
                prev = 0;
            }
        }
        if (!RNODE_DREGX(node)->nd_next) {
            VALUE src = RNODE_DREGX(node)->nd_lit;
            VALUE re = reg_compile(p, src, options);
            RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_DREGX(node)->nd_lit = re);
        }
        if (options & RE_OPTION_ONCE) {
            node = NEW_ONCE(node, loc);
        }
        break;
    }
    return node;
}

static rb_node_kw_arg_t *
new_kw_arg(struct parser_params *p, NODE *k, const YYLTYPE *loc)
{
    if (!k) return 0;
    return NEW_KW_ARG((k), loc);
}

static NODE *
new_xstring(struct parser_params *p, NODE *node, const YYLTYPE *loc)
{
    if (!node) {
        VALUE lit = STR_NEW0();
        NODE *xstr = NEW_XSTR(lit, loc);
        RB_OBJ_WRITTEN(p->ast, Qnil, lit);
        return xstr;
    }
    switch (nd_type(node)) {
      case NODE_STR:
        nd_set_type(node, NODE_XSTR);
        nd_set_loc(node, loc);
        break;
      case NODE_DSTR:
        nd_set_type(node, NODE_DXSTR);
        nd_set_loc(node, loc);
        break;
      default:
        node = NEW_DXSTR(Qnil, 1, NEW_LIST(node, loc), loc);
        break;
    }
    return node;
}

static void
check_literal_when(struct parser_params *p, NODE *arg, const YYLTYPE *loc)
{
    VALUE lit;

    if (!arg || !p->case_labels) return;

    lit = rb_node_case_when_optimizable_literal(arg);
    if (UNDEF_P(lit)) return;
    if (nd_type_p(arg, NODE_STR)) {
        RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_STR(arg)->nd_lit = lit);
    }

    if (NIL_P(p->case_labels)) {
        p->case_labels = rb_obj_hide(rb_hash_new());
    }
    else {
        VALUE line = rb_hash_lookup(p->case_labels, lit);
        if (!NIL_P(line)) {
            rb_warning1("duplicated `when' clause with line %d is ignored",
                        WARN_IVAL(line));
            return;
        }
    }
    rb_hash_aset(p->case_labels, lit, INT2NUM(p->ruby_sourceline));
}

#else  /* !RIPPER */
static int
id_is_var(struct parser_params *p, ID id)
{
    if (is_notop_id(id)) {
        switch (id & ID_SCOPE_MASK) {
          case ID_GLOBAL: case ID_INSTANCE: case ID_CONST: case ID_CLASS:
            return 1;
          case ID_LOCAL:
            if (dyna_in_block(p)) {
                if (NUMPARAM_ID_P(id) || dvar_defined(p, id)) return 1;
            }
            if (local_id(p, id)) return 1;
            /* method call without arguments */
            return 0;
        }
    }
    compile_error(p, "identifier %"PRIsVALUE" is not valid to get", rb_id2str(id));
    return 0;
}

static VALUE
new_regexp(struct parser_params *p, VALUE re, VALUE opt, const YYLTYPE *loc)
{
    VALUE src = 0, err = 0;
    int options = 0;
    if (ripper_is_node_yylval(p, re)) {
        src = RNODE_RIPPER(re)->nd_cval;
        re = RNODE_RIPPER(re)->nd_rval;
    }
    if (ripper_is_node_yylval(p, opt)) {
        options = (int)RNODE_RIPPER(opt)->nd_vid;
        opt = RNODE_RIPPER(opt)->nd_rval;
    }
    if (src && NIL_P(parser_reg_compile(p, src, options, &err))) {
        compile_error(p, "%"PRIsVALUE, err);
    }
    return dispatch2(regexp_literal, re, opt);
}
#endif /* !RIPPER */

static inline enum lex_state_e
parser_set_lex_state(struct parser_params *p, enum lex_state_e ls, int line)
{
    if (p->debug) {
        ls = rb_parser_trace_lex_state(p, p->lex.state, ls, line);
    }
    return p->lex.state = ls;
}

#ifndef RIPPER
static const char rb_parser_lex_state_names[][8] = {
    "BEG",    "END",    "ENDARG", "ENDFN",  "ARG",
    "CMDARG", "MID",    "FNAME",  "DOT",    "CLASS",
    "LABEL",  "LABELED","FITEM",
};

static VALUE
append_lex_state_name(struct parser_params *p, enum lex_state_e state, VALUE buf)
{
    int i, sep = 0;
    unsigned int mask = 1;
    static const char none[] = "NONE";

    for (i = 0; i < EXPR_MAX_STATE; ++i, mask <<= 1) {
        if ((unsigned)state & mask) {
            if (sep) {
                rb_str_cat(buf, "|", 1);
            }
            sep = 1;
            rb_str_cat_cstr(buf, rb_parser_lex_state_names[i]);
        }
    }
    if (!sep) {
        rb_str_cat(buf, none, sizeof(none)-1);
    }
    return buf;
}

static void
flush_debug_buffer(struct parser_params *p, VALUE out, VALUE str)
{
    VALUE mesg = p->debug_buffer;

    if (!NIL_P(mesg) && RSTRING_LEN(mesg)) {
        p->debug_buffer = Qnil;
        rb_io_puts(1, &mesg, out);
    }
    if (!NIL_P(str) && RSTRING_LEN(str)) {
        rb_io_write(p->debug_output, str);
    }
}

enum lex_state_e
rb_parser_trace_lex_state(struct parser_params *p, enum lex_state_e from,
                          enum lex_state_e to, int line)
{
    VALUE mesg;
    mesg = rb_str_new_cstr("lex_state: ");
    append_lex_state_name(p, from, mesg);
    rb_str_cat_cstr(mesg, " -> ");
    append_lex_state_name(p, to, mesg);
    rb_str_catf(mesg, " at line %d\n", line);
    flush_debug_buffer(p, p->debug_output, mesg);
    return to;
}

VALUE
rb_parser_lex_state_name(struct parser_params *p, enum lex_state_e state)
{
    return rb_fstring(append_lex_state_name(p, state, rb_str_new(0, 0)));
}

static void
append_bitstack_value(struct parser_params *p, stack_type stack, VALUE mesg)
{
    if (stack == 0) {
        rb_str_cat_cstr(mesg, "0");
    }
    else {
        stack_type mask = (stack_type)1U << (CHAR_BIT * sizeof(stack_type) - 1);
        for (; mask && !(stack & mask); mask >>= 1) continue;
        for (; mask; mask >>= 1) rb_str_cat(mesg, stack & mask ? "1" : "0", 1);
    }
}

void
rb_parser_show_bitstack(struct parser_params *p, stack_type stack,
                        const char *name, int line)
{
    VALUE mesg = rb_sprintf("%s: ", name);
    append_bitstack_value(p, stack, mesg);
    rb_str_catf(mesg, " at line %d\n", line);
    flush_debug_buffer(p, p->debug_output, mesg);
}

void
rb_parser_fatal(struct parser_params *p, const char *fmt, ...)
{
    va_list ap;
    VALUE mesg = rb_str_new_cstr("internal parser error: ");

    va_start(ap, fmt);
    rb_str_vcatf(mesg, fmt, ap);
    va_end(ap);
    yyerror0(RSTRING_PTR(mesg));
    RB_GC_GUARD(mesg);

    mesg = rb_str_new(0, 0);
    append_lex_state_name(p, p->lex.state, mesg);
    compile_error(p, "lex.state: %"PRIsVALUE, mesg);
    rb_str_resize(mesg, 0);
    append_bitstack_value(p, p->cond_stack, mesg);
    compile_error(p, "cond_stack: %"PRIsVALUE, mesg);
    rb_str_resize(mesg, 0);
    append_bitstack_value(p, p->cmdarg_stack, mesg);
    compile_error(p, "cmdarg_stack: %"PRIsVALUE, mesg);
    if (p->debug_output == rb_ractor_stdout())
        p->debug_output = rb_ractor_stderr();
    p->debug = TRUE;
}

static YYLTYPE *
rb_parser_set_pos(YYLTYPE *yylloc, int sourceline, int beg_pos, int end_pos)
{
    yylloc->beg_pos.lineno = sourceline;
    yylloc->beg_pos.column = beg_pos;
    yylloc->end_pos.lineno = sourceline;
    yylloc->end_pos.column = end_pos;
    return yylloc;
}

YYLTYPE *
rb_parser_set_location_from_strterm_heredoc(struct parser_params *p, rb_strterm_heredoc_t *here, YYLTYPE *yylloc)
{
    int sourceline = here->sourceline;
    int beg_pos = (int)here->offset - here->quote
        - (rb_strlen_lit("<<-") - !(here->func & STR_FUNC_INDENT));
    int end_pos = (int)here->offset + here->length + here->quote;

    return rb_parser_set_pos(yylloc, sourceline, beg_pos, end_pos);
}

YYLTYPE *
rb_parser_set_location_of_delayed_token(struct parser_params *p, YYLTYPE *yylloc)
{
    yylloc->beg_pos.lineno = p->delayed.beg_line;
    yylloc->beg_pos.column = p->delayed.beg_col;
    yylloc->end_pos.lineno = p->delayed.end_line;
    yylloc->end_pos.column = p->delayed.end_col;

    return yylloc;
}

YYLTYPE *
rb_parser_set_location_of_heredoc_end(struct parser_params *p, YYLTYPE *yylloc)
{
    int sourceline = p->ruby_sourceline;
    int beg_pos = (int)(p->lex.ptok - p->lex.pbeg);
    int end_pos = (int)(p->lex.pend - p->lex.pbeg);
    return rb_parser_set_pos(yylloc, sourceline, beg_pos, end_pos);
}

YYLTYPE *
rb_parser_set_location_of_dummy_end(struct parser_params *p, YYLTYPE *yylloc)
{
    yylloc->end_pos = yylloc->beg_pos;

    return yylloc;
}

YYLTYPE *
rb_parser_set_location_of_none(struct parser_params *p, YYLTYPE *yylloc)
{
    int sourceline = p->ruby_sourceline;
    int beg_pos = (int)(p->lex.ptok - p->lex.pbeg);
    int end_pos = (int)(p->lex.ptok - p->lex.pbeg);
    return rb_parser_set_pos(yylloc, sourceline, beg_pos, end_pos);
}

YYLTYPE *
rb_parser_set_location(struct parser_params *p, YYLTYPE *yylloc)
{
    int sourceline = p->ruby_sourceline;
    int beg_pos = (int)(p->lex.ptok - p->lex.pbeg);
    int end_pos = (int)(p->lex.pcur - p->lex.pbeg);
    return rb_parser_set_pos(yylloc, sourceline, beg_pos, end_pos);
}
#endif /* !RIPPER */

static int
assignable0(struct parser_params *p, ID id, const char **err)
{
    if (!id) return -1;
    switch (id) {
      case keyword_self:
        *err = "Can't change the value of self";
        return -1;
      case keyword_nil:
        *err = "Can't assign to nil";
        return -1;
      case keyword_true:
        *err = "Can't assign to true";
        return -1;
      case keyword_false:
        *err = "Can't assign to false";
        return -1;
      case keyword__FILE__:
        *err = "Can't assign to __FILE__";
        return -1;
      case keyword__LINE__:
        *err = "Can't assign to __LINE__";
        return -1;
      case keyword__ENCODING__:
        *err = "Can't assign to __ENCODING__";
        return -1;
    }
    switch (id_type(id)) {
      case ID_LOCAL:
        if (dyna_in_block(p)) {
            if (p->max_numparam > NO_PARAM && NUMPARAM_ID_P(id)) {
                compile_error(p, "Can't assign to numbered parameter _%d",
                              NUMPARAM_ID_TO_IDX(id));
                return -1;
            }
            if (dvar_curr(p, id)) return NODE_DASGN;
            if (dvar_defined(p, id)) return NODE_DASGN;
            if (local_id(p, id)) return NODE_LASGN;
            dyna_var(p, id);
            return NODE_DASGN;
        }
        else {
            if (!local_id(p, id)) local_var(p, id);
            return NODE_LASGN;
        }
        break;
      case ID_GLOBAL: return NODE_GASGN;
      case ID_INSTANCE: return NODE_IASGN;
      case ID_CONST:
        if (!p->ctxt.in_def) return NODE_CDECL;
        *err = "dynamic constant assignment";
        return -1;
      case ID_CLASS: return NODE_CVASGN;
      default:
        compile_error(p, "identifier %"PRIsVALUE" is not valid to set", rb_id2str(id));
    }
    return -1;
}

#ifndef RIPPER
static NODE*
assignable(struct parser_params *p, ID id, NODE *val, const YYLTYPE *loc)
{
    const char *err = 0;
    int node_type = assignable0(p, id, &err);
    switch (node_type) {
      case NODE_DASGN: return NEW_DASGN(id, val, loc);
      case NODE_LASGN: return NEW_LASGN(id, val, loc);
      case NODE_GASGN: return NEW_GASGN(id, val, loc);
      case NODE_IASGN: return NEW_IASGN(id, val, loc);
      case NODE_CDECL: return NEW_CDECL(id, val, 0, loc);
      case NODE_CVASGN: return NEW_CVASGN(id, val, loc);
    }
    if (err) yyerror1(loc, err);
    return NEW_ERROR(loc);
}
#else
static VALUE
assignable(struct parser_params *p, VALUE lhs)
{
    const char *err = 0;
    assignable0(p, get_id(lhs), &err);
    if (err) lhs = assign_error(p, err, lhs);
    return lhs;
}
#endif

static int
is_private_local_id(struct parser_params *p, ID name)
{
    VALUE s;
    if (name == idUScore) return 1;
    if (!is_local_id(name)) return 0;
    s = rb_id2str(name);
    if (!s) return 0;
    return RSTRING_PTR(s)[0] == '_';
}

static int
shadowing_lvar_0(struct parser_params *p, ID name)
{
    if (dyna_in_block(p)) {
        if (dvar_curr(p, name)) {
            if (is_private_local_id(p, name)) return 1;
            yyerror0("duplicated argument name");
        }
        else if (dvar_defined(p, name) || local_id(p, name)) {
            vtable_add(p->lvtbl->vars, name);
            if (p->lvtbl->used) {
                vtable_add(p->lvtbl->used, (ID)p->ruby_sourceline | LVAR_USED);
            }
            return 0;
        }
    }
    else {
        if (local_id(p, name)) {
            if (is_private_local_id(p, name)) return 1;
            yyerror0("duplicated argument name");
        }
    }
    return 1;
}

static ID
shadowing_lvar(struct parser_params *p, ID name)
{
    shadowing_lvar_0(p, name);
    return name;
}

static void
new_bv(struct parser_params *p, ID name)
{
    if (!name) return;
    if (!is_local_id(name)) {
        compile_error(p, "invalid local variable - %"PRIsVALUE,
                      rb_id2str(name));
        return;
    }
    if (!shadowing_lvar_0(p, name)) return;
    dyna_var(p, name);
}

#ifndef RIPPER
static NODE *
aryset(struct parser_params *p, NODE *recv, NODE *idx, const YYLTYPE *loc)
{
    return NEW_ATTRASGN(recv, tASET, idx, loc);
}

static void
block_dup_check(struct parser_params *p, NODE *node1, NODE *node2)
{
    if (node2 && node1 && nd_type_p(node1, NODE_BLOCK_PASS)) {
        compile_error(p, "both block arg and actual block given");
    }
}

static NODE *
attrset(struct parser_params *p, NODE *recv, ID atype, ID id, const YYLTYPE *loc)
{
    if (!CALL_Q_P(atype)) id = rb_id_attrset(id);
    return NEW_ATTRASGN(recv, id, 0, loc);
}

static void
rb_backref_error(struct parser_params *p, NODE *node)
{
    switch (nd_type(node)) {
      case NODE_NTH_REF:
        compile_error(p, "Can't set variable $%ld", RNODE_NTH_REF(node)->nd_nth);
        break;
      case NODE_BACK_REF:
        compile_error(p, "Can't set variable $%c", (int)RNODE_BACK_REF(node)->nd_nth);
        break;
    }
}
#else
static VALUE
backref_error(struct parser_params *p, NODE *ref, VALUE expr)
{
    VALUE mesg = rb_str_new_cstr("Can't set variable ");
    rb_str_append(mesg, RNODE_RIPPER(ref)->nd_cval);
    return dispatch2(assign_error, mesg, expr);
}
#endif

#ifndef RIPPER
static NODE *
arg_append(struct parser_params *p, NODE *node1, NODE *node2, const YYLTYPE *loc)
{
    if (!node1) return NEW_LIST(node2, &node2->nd_loc);
    switch (nd_type(node1))  {
      case NODE_LIST:
        return list_append(p, node1, node2);
      case NODE_BLOCK_PASS:
        RNODE_BLOCK_PASS(node1)->nd_head = arg_append(p, RNODE_BLOCK_PASS(node1)->nd_head, node2, loc);
        node1->nd_loc.end_pos = RNODE_BLOCK_PASS(node1)->nd_head->nd_loc.end_pos;
        return node1;
      case NODE_ARGSPUSH:
        RNODE_ARGSPUSH(node1)->nd_body = list_append(p, NEW_LIST(RNODE_ARGSPUSH(node1)->nd_body, &RNODE_ARGSPUSH(node1)->nd_body->nd_loc), node2);
        node1->nd_loc.end_pos = RNODE_ARGSPUSH(node1)->nd_body->nd_loc.end_pos;
        nd_set_type(node1, NODE_ARGSCAT);
        return node1;
      case NODE_ARGSCAT:
        if (!nd_type_p(RNODE_ARGSCAT(node1)->nd_body, NODE_LIST)) break;
        RNODE_ARGSCAT(node1)->nd_body = list_append(p, RNODE_ARGSCAT(node1)->nd_body, node2);
        node1->nd_loc.end_pos = RNODE_ARGSCAT(node1)->nd_body->nd_loc.end_pos;
        return node1;
    }
    return NEW_ARGSPUSH(node1, node2, loc);
}

static NODE *
arg_concat(struct parser_params *p, NODE *node1, NODE *node2, const YYLTYPE *loc)
{
    if (!node2) return node1;
    switch (nd_type(node1)) {
      case NODE_BLOCK_PASS:
        if (RNODE_BLOCK_PASS(node1)->nd_head)
            RNODE_BLOCK_PASS(node1)->nd_head = arg_concat(p, RNODE_BLOCK_PASS(node1)->nd_head, node2, loc);
        else
            RNODE_LIST(node1)->nd_head = NEW_LIST(node2, loc);
        return node1;
      case NODE_ARGSPUSH:
        if (!nd_type_p(node2, NODE_LIST)) break;
        RNODE_ARGSPUSH(node1)->nd_body = list_concat(NEW_LIST(RNODE_ARGSPUSH(node1)->nd_body, loc), node2);
        nd_set_type(node1, NODE_ARGSCAT);
        return node1;
      case NODE_ARGSCAT:
        if (!nd_type_p(node2, NODE_LIST) ||
            !nd_type_p(RNODE_ARGSCAT(node1)->nd_body, NODE_LIST)) break;
        RNODE_ARGSCAT(node1)->nd_body = list_concat(RNODE_ARGSCAT(node1)->nd_body, node2);
        return node1;
    }
    return NEW_ARGSCAT(node1, node2, loc);
}

static NODE *
last_arg_append(struct parser_params *p, NODE *args, NODE *last_arg, const YYLTYPE *loc)
{
    NODE *n1;
    if ((n1 = splat_array(args)) != 0) {
        return list_append(p, n1, last_arg);
    }
    return arg_append(p, args, last_arg, loc);
}

static NODE *
rest_arg_append(struct parser_params *p, NODE *args, NODE *rest_arg, const YYLTYPE *loc)
{
    NODE *n1;
    if ((nd_type_p(rest_arg, NODE_LIST)) && (n1 = splat_array(args)) != 0) {
        return list_concat(n1, rest_arg);
    }
    return arg_concat(p, args, rest_arg, loc);
}

static NODE *
splat_array(NODE* node)
{
    if (nd_type_p(node, NODE_SPLAT)) node = RNODE_SPLAT(node)->nd_head;
    if (nd_type_p(node, NODE_LIST)) return node;
    return 0;
}

static void
mark_lvar_used(struct parser_params *p, NODE *rhs)
{
    ID *vidp = NULL;
    if (!rhs) return;
    switch (nd_type(rhs)) {
      case NODE_LASGN:
        if (local_id_ref(p, RNODE_LASGN(rhs)->nd_vid, &vidp)) {
            if (vidp) *vidp |= LVAR_USED;
        }
        break;
      case NODE_DASGN:
        if (dvar_defined_ref(p, RNODE_DASGN(rhs)->nd_vid, &vidp)) {
            if (vidp) *vidp |= LVAR_USED;
        }
        break;
#if 0
      case NODE_MASGN:
        for (rhs = rhs->nd_head; rhs; rhs = rhs->nd_next) {
            mark_lvar_used(p, rhs->nd_head);
        }
        break;
#endif
    }
}

static NODE *
const_decl_path(struct parser_params *p, NODE *dest)
{
    NODE *n = dest;
    if (!nd_type_p(n, NODE_CALL)) {
        const YYLTYPE *loc = &n->nd_loc;
        VALUE path;
        if (RNODE_CDECL(n)->nd_vid) {
             path = rb_id2str(RNODE_CDECL(n)->nd_vid);
        }
        else {
            n = RNODE_CDECL(n)->nd_else;
            path = rb_ary_new();
            for (; n && nd_type_p(n, NODE_COLON2); n = RNODE_COLON2(n)->nd_head) {
                rb_ary_push(path, rb_id2str(RNODE_COLON2(n)->nd_mid));
            }
            if (n && nd_type_p(n, NODE_CONST)) {
                // Const::Name
                rb_ary_push(path, rb_id2str(RNODE_CONST(n)->nd_vid));
            }
            else if (n && nd_type_p(n, NODE_COLON3)) {
                // ::Const::Name
                rb_ary_push(path, rb_str_new(0, 0));
            }
            else {
                // expression::Name
                rb_ary_push(path, rb_str_new_cstr("..."));
            }
            path = rb_ary_join(rb_ary_reverse(path), rb_str_new_cstr("::"));
            path = rb_fstring(path);
        }
        n = NEW_LIT(path, loc);
        RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_LIT(n)->nd_lit);
    }
    return n;
}

static NODE *
make_shareable_node(struct parser_params *p, NODE *value, bool copy, const YYLTYPE *loc)
{
    NODE *fcore = NEW_LIT(rb_mRubyVMFrozenCore, loc);

    if (copy) {
        return NEW_CALL(fcore, rb_intern("make_shareable_copy"),
                        NEW_LIST(value, loc), loc);
    }
    else {
        return NEW_CALL(fcore, rb_intern("make_shareable"),
                        NEW_LIST(value, loc), loc);
    }
}

static NODE *
ensure_shareable_node(struct parser_params *p, NODE **dest, NODE *value, const YYLTYPE *loc)
{
    NODE *fcore = NEW_LIT(rb_mRubyVMFrozenCore, loc);
    NODE *args = NEW_LIST(value, loc);
    args = list_append(p, args, const_decl_path(p, *dest));
    return NEW_CALL(fcore, rb_intern("ensure_shareable"), args, loc);
}

static int is_static_content(NODE *node);

static VALUE
shareable_literal_value(struct parser_params *p, NODE *node)
{
    if (!node) return Qnil;
    enum node_type type = nd_type(node);
    switch (type) {
      case NODE_TRUE:
        return Qtrue;
      case NODE_FALSE:
        return Qfalse;
      case NODE_NIL:
        return Qnil;
      case NODE_LIT:
        return RNODE_LIT(node)->nd_lit;
      default:
        return Qundef;
    }
}

#ifndef SHAREABLE_BARE_EXPRESSION
#define SHAREABLE_BARE_EXPRESSION 1
#endif

static NODE *
shareable_literal_constant(struct parser_params *p, enum shareability shareable,
                           NODE **dest, NODE *value, const YYLTYPE *loc, size_t level)
{
# define shareable_literal_constant_next(n) \
    shareable_literal_constant(p, shareable, dest, (n), &(n)->nd_loc, level+1)
    VALUE lit = Qnil;

    if (!value) return 0;
    enum node_type type = nd_type(value);
    switch (type) {
      case NODE_TRUE:
      case NODE_FALSE:
      case NODE_NIL:
      case NODE_LIT:
        return value;

      case NODE_DSTR:
        if (shareable == shareable_literal) {
            value = NEW_CALL(value, idUMinus, 0, loc);
        }
        return value;

      case NODE_STR:
        lit = rb_fstring(RNODE_STR(value)->nd_lit);
        nd_set_type(value, NODE_LIT);
        RB_OBJ_WRITE(p->ast, &RNODE_LIT(value)->nd_lit, lit);
        return value;

      case NODE_ZLIST:
        lit = rb_ary_new();
        OBJ_FREEZE_RAW(lit);
        NODE *n = NEW_LIT(lit, loc);
        RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_LIT(n)->nd_lit);
        return n;

      case NODE_LIST:
        lit = rb_ary_new();
        for (NODE *n = value; n; n = RNODE_LIST(n)->nd_next) {
            NODE *elt = RNODE_LIST(n)->nd_head;
            if (elt) {
                elt = shareable_literal_constant_next(elt);
                if (elt) {
                    RNODE_LIST(n)->nd_head = elt;
                }
                else if (RTEST(lit)) {
                    rb_ary_clear(lit);
                    lit = Qfalse;
                }
            }
            if (RTEST(lit)) {
                VALUE e = shareable_literal_value(p, elt);
                if (!UNDEF_P(e)) {
                    rb_ary_push(lit, e);
                }
                else {
                    rb_ary_clear(lit);
                    lit = Qnil;	/* make shareable at runtime */
                }
            }
        }
        break;

      case NODE_HASH:
        if (!RNODE_HASH(value)->nd_brace) return 0;
        lit = rb_hash_new();
        for (NODE *n = RNODE_HASH(value)->nd_head; n; n = RNODE_LIST(RNODE_LIST(n)->nd_next)->nd_next) {
            NODE *key = RNODE_LIST(n)->nd_head;
            NODE *val = RNODE_LIST(RNODE_LIST(n)->nd_next)->nd_head;
            if (key) {
                key = shareable_literal_constant_next(key);
                if (key) {
                    RNODE_LIST(n)->nd_head = key;
                }
                else if (RTEST(lit)) {
                    rb_hash_clear(lit);
                    lit = Qfalse;
                }
            }
            if (val) {
                val = shareable_literal_constant_next(val);
                if (val) {
                    RNODE_LIST(RNODE_LIST(n)->nd_next)->nd_head = val;
                }
                else if (RTEST(lit)) {
                    rb_hash_clear(lit);
                    lit = Qfalse;
                }
            }
            if (RTEST(lit)) {
                VALUE k = shareable_literal_value(p, key);
                VALUE v = shareable_literal_value(p, val);
                if (!UNDEF_P(k) && !UNDEF_P(v)) {
                    rb_hash_aset(lit, k, v);
                }
                else {
                    rb_hash_clear(lit);
                    lit = Qnil;	/* make shareable at runtime */
                }
            }
        }
        break;

      default:
        if (shareable == shareable_literal &&
            (SHAREABLE_BARE_EXPRESSION || level > 0)) {
            return ensure_shareable_node(p, dest, value, loc);
        }
        return 0;
    }

    /* Array or Hash */
    if (!lit) return 0;
    if (NIL_P(lit)) {
        // if shareable_literal, all elements should have been ensured
        // as shareable
        value = make_shareable_node(p, value, false, loc);
    }
    else {
        value = NEW_LIT(rb_ractor_make_shareable(lit), loc);
        RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_LIT(value)->nd_lit);
    }

    return value;
# undef shareable_literal_constant_next
}

static NODE *
shareable_constant_value(struct parser_params *p, enum shareability shareable,
                         NODE *lhs, NODE *value, const YYLTYPE *loc)
{
    if (!value) return 0;
    switch (shareable) {
      case shareable_none:
        return value;

      case shareable_literal:
        {
            NODE *lit = shareable_literal_constant(p, shareable, &lhs, value, loc, 0);
            if (lit) return lit;
            return value;
        }
        break;

      case shareable_copy:
      case shareable_everything:
        {
            NODE *lit = shareable_literal_constant(p, shareable, &lhs, value, loc, 0);
            if (lit) return lit;
            return make_shareable_node(p, value, shareable == shareable_copy, loc);
        }
        break;

      default:
        UNREACHABLE_RETURN(0);
    }
}

static NODE *
node_assign(struct parser_params *p, NODE *lhs, NODE *rhs, struct lex_context ctxt, const YYLTYPE *loc)
{
    if (!lhs) return 0;

    switch (nd_type(lhs)) {
      case NODE_CDECL:
        rhs = shareable_constant_value(p, ctxt.shareable_constant_value, lhs, rhs, loc);
        /* fallthru */

      case NODE_GASGN:
      case NODE_IASGN:
      case NODE_LASGN:
      case NODE_DASGN:
      case NODE_MASGN:
      case NODE_CVASGN:
        set_nd_value(p, lhs, rhs);
        nd_set_loc(lhs, loc);
        break;

      case NODE_ATTRASGN:
        RNODE_ATTRASGN(lhs)->nd_args = arg_append(p, RNODE_ATTRASGN(lhs)->nd_args, rhs, loc);
        nd_set_loc(lhs, loc);
        break;

      default:
        /* should not happen */
        break;
    }

    return lhs;
}

static NODE *
value_expr_check(struct parser_params *p, NODE *node)
{
    NODE *void_node = 0, *vn;

    if (!node) {
        rb_warning0("empty expression");
    }
    while (node) {
        switch (nd_type(node)) {
          case NODE_RETURN:
          case NODE_BREAK:
          case NODE_NEXT:
          case NODE_REDO:
          case NODE_RETRY:
            return void_node ? void_node : node;

          case NODE_CASE3:
            if (!RNODE_CASE3(node)->nd_body || !nd_type_p(RNODE_CASE3(node)->nd_body, NODE_IN)) {
                compile_error(p, "unexpected node");
                return NULL;
            }
            if (RNODE_IN(RNODE_CASE3(node)->nd_body)->nd_body) {
                return NULL;
            }
            /* single line pattern matching with "=>" operator */
            return void_node ? void_node : node;

          case NODE_BLOCK:
            while (RNODE_BLOCK(node)->nd_next) {
                node = RNODE_BLOCK(node)->nd_next;
            }
            node = RNODE_BLOCK(node)->nd_head;
            break;

          case NODE_BEGIN:
            node = RNODE_BEGIN(node)->nd_body;
            break;

          case NODE_IF:
          case NODE_UNLESS:
            if (!RNODE_IF(node)->nd_body) {
                return NULL;
            }
            else if (!RNODE_IF(node)->nd_else) {
                return NULL;
            }
            vn = value_expr_check(p, RNODE_IF(node)->nd_body);
            if (!vn) return NULL;
            if (!void_node) void_node = vn;
            node = RNODE_IF(node)->nd_else;
            break;

          case NODE_AND:
          case NODE_OR:
            node = RNODE_AND(node)->nd_1st;
            break;

          case NODE_LASGN:
          case NODE_DASGN:
          case NODE_MASGN:
            mark_lvar_used(p, node);
            return NULL;

          default:
            return NULL;
        }
    }

    return NULL;
}

static int
value_expr_gen(struct parser_params *p, NODE *node)
{
    NODE *void_node = value_expr_check(p, node);
    if (void_node) {
        yyerror1(&void_node->nd_loc, "void value expression");
        /* or "control never reach"? */
        return FALSE;
    }
    return TRUE;
}

static void
void_expr(struct parser_params *p, NODE *node)
{
    const char *useless = 0;

    if (!RTEST(ruby_verbose)) return;

    if (!node || !(node = nd_once_body(node))) return;
    switch (nd_type(node)) {
      case NODE_OPCALL:
        switch (RNODE_OPCALL(node)->nd_mid) {
          case '+':
          case '-':
          case '*':
          case '/':
          case '%':
          case tPOW:
          case tUPLUS:
          case tUMINUS:
          case '|':
          case '^':
          case '&':
          case tCMP:
          case '>':
          case tGEQ:
          case '<':
          case tLEQ:
          case tEQ:
          case tNEQ:
            useless = rb_id2name(RNODE_OPCALL(node)->nd_mid);
            break;
        }
        break;

      case NODE_LVAR:
      case NODE_DVAR:
      case NODE_GVAR:
      case NODE_IVAR:
      case NODE_CVAR:
      case NODE_NTH_REF:
      case NODE_BACK_REF:
        useless = "a variable";
        break;
      case NODE_CONST:
        useless = "a constant";
        break;
      case NODE_LIT:
      case NODE_STR:
      case NODE_DSTR:
      case NODE_DREGX:
        useless = "a literal";
        break;
      case NODE_COLON2:
      case NODE_COLON3:
        useless = "::";
        break;
      case NODE_DOT2:
        useless = "..";
        break;
      case NODE_DOT3:
        useless = "...";
        break;
      case NODE_SELF:
        useless = "self";
        break;
      case NODE_NIL:
        useless = "nil";
        break;
      case NODE_TRUE:
        useless = "true";
        break;
      case NODE_FALSE:
        useless = "false";
        break;
      case NODE_DEFINED:
        useless = "defined?";
        break;
    }

    if (useless) {
        rb_warn1L(nd_line(node), "possibly useless use of %s in void context", WARN_S(useless));
    }
}

static NODE *
void_stmts(struct parser_params *p, NODE *node)
{
    NODE *const n = node;
    if (!RTEST(ruby_verbose)) return n;
    if (!node) return n;
    if (!nd_type_p(node, NODE_BLOCK)) return n;

    while (RNODE_BLOCK(node)->nd_next) {
        void_expr(p, RNODE_BLOCK(node)->nd_head);
        node = RNODE_BLOCK(node)->nd_next;
    }
    return n;
}

static NODE *
remove_begin(NODE *node)
{
    NODE **n = &node, *n1 = node;
    while (n1 && nd_type_p(n1, NODE_BEGIN) && RNODE_BEGIN(n1)->nd_body) {
        *n = n1 = RNODE_BEGIN(n1)->nd_body;
    }
    return node;
}

static void
reduce_nodes(struct parser_params *p, NODE **body)
{
    NODE *node = *body;

    if (!node) {
        *body = NEW_NIL(&NULL_LOC);
        return;
    }
#define subnodes(type, n1, n2) \
    ((!type(node)->n1) ? (type(node)->n2 ? (body = &type(node)->n2, 1) : 0) : \
     (!type(node)->n2) ? (body = &type(node)->n1, 1) : \
     (reduce_nodes(p, &type(node)->n1), body = &type(node)->n2, 1))

    while (node) {
        int newline = (int)(nd_fl_newline(node));
        switch (nd_type(node)) {
          end:
          case NODE_NIL:
            *body = 0;
            return;
          case NODE_RETURN:
            *body = node = RNODE_RETURN(node)->nd_stts;
            if (newline && node) nd_set_fl_newline(node);
            continue;
          case NODE_BEGIN:
            *body = node = RNODE_BEGIN(node)->nd_body;
            if (newline && node) nd_set_fl_newline(node);
            continue;
          case NODE_BLOCK:
            body = &RNODE_BLOCK(RNODE_BLOCK(node)->nd_end)->nd_head;
            break;
          case NODE_IF:
          case NODE_UNLESS:
            if (subnodes(RNODE_IF, nd_body, nd_else)) break;
            return;
          case NODE_CASE:
            body = &RNODE_CASE(node)->nd_body;
            break;
          case NODE_WHEN:
            if (!subnodes(RNODE_WHEN, nd_body, nd_next)) goto end;
            break;
          case NODE_ENSURE:
            if (!subnodes(RNODE_ENSURE, nd_head, nd_resq)) goto end;
            break;
          case NODE_RESCUE:
            newline = 0; // RESBODY should not be a NEWLINE
            if (RNODE_RESCUE(node)->nd_else) {
                body = &RNODE_RESCUE(node)->nd_resq;
                break;
            }
            if (!subnodes(RNODE_RESCUE, nd_head, nd_resq)) goto end;
            break;
          default:
            return;
        }
        node = *body;
        if (newline && node) nd_set_fl_newline(node);
    }

#undef subnodes
}

static int
is_static_content(NODE *node)
{
    if (!node) return 1;
    switch (nd_type(node)) {
      case NODE_HASH:
        if (!(node = RNODE_HASH(node)->nd_head)) break;
      case NODE_LIST:
        do {
            if (!is_static_content(RNODE_LIST(node)->nd_head)) return 0;
        } while ((node = RNODE_LIST(node)->nd_next) != 0);
      case NODE_LIT:
      case NODE_STR:
      case NODE_NIL:
      case NODE_TRUE:
      case NODE_FALSE:
      case NODE_ZLIST:
        break;
      default:
        return 0;
    }
    return 1;
}

static int
assign_in_cond(struct parser_params *p, NODE *node)
{
    switch (nd_type(node)) {
      case NODE_MASGN:
      case NODE_LASGN:
      case NODE_DASGN:
      case NODE_GASGN:
      case NODE_IASGN:
      case NODE_CVASGN:
      case NODE_CDECL:
        break;

      default:
        return 0;
    }

    if (!get_nd_value(p, node)) return 1;
    if (is_static_content(get_nd_value(p, node))) {
        /* reports always */
        parser_warn(p, get_nd_value(p, node), "found `= literal' in conditional, should be ==");
    }
    return 1;
}

enum cond_type {
    COND_IN_OP,
    COND_IN_COND,
    COND_IN_FF
};

#define SWITCH_BY_COND_TYPE(t, w, arg) do { \
    switch (t) { \
      case COND_IN_OP: break; \
      case COND_IN_COND: rb_##w##0(arg "literal in condition"); break; \
      case COND_IN_FF: rb_##w##0(arg "literal in flip-flop"); break; \
    } \
} while (0)

static NODE *cond0(struct parser_params*,NODE*,enum cond_type,const YYLTYPE*,bool);

static NODE*
range_op(struct parser_params *p, NODE *node, const YYLTYPE *loc)
{
    enum node_type type;

    if (node == 0) return 0;

    type = nd_type(node);
    value_expr(node);
    if (type == NODE_LIT && FIXNUM_P(RNODE_LIT(node)->nd_lit)) {
        if (!e_option_supplied(p)) parser_warn(p, node, "integer literal in flip-flop");
        ID lineno = rb_intern("$.");
        return NEW_CALL(node, tEQ, NEW_LIST(NEW_GVAR(lineno, loc), loc), loc);
    }
    return cond0(p, node, COND_IN_FF, loc, true);
}

static NODE*
cond0(struct parser_params *p, NODE *node, enum cond_type type, const YYLTYPE *loc, bool top)
{
    if (node == 0) return 0;
    if (!(node = nd_once_body(node))) return 0;
    assign_in_cond(p, node);

    switch (nd_type(node)) {
      case NODE_BEGIN:
        RNODE_BEGIN(node)->nd_body = cond0(p, RNODE_BEGIN(node)->nd_body, type, loc, top);
        break;

      case NODE_DSTR:
      case NODE_EVSTR:
      case NODE_STR:
        SWITCH_BY_COND_TYPE(type, warn, "string ");
        break;

      case NODE_DREGX:
        if (!e_option_supplied(p)) SWITCH_BY_COND_TYPE(type, warning, "regex ");

        return NEW_MATCH2(node, NEW_GVAR(idLASTLINE, loc), loc);

      case NODE_BLOCK:
        {
            NODE *end = RNODE_BLOCK(node)->nd_end;
            NODE **expr = &RNODE_BLOCK(end)->nd_head;
            if (top) top = node == end;
            *expr = cond0(p, *expr, type, loc, top);
        }
        break;

      case NODE_AND:
      case NODE_OR:
        RNODE_AND(node)->nd_1st = cond0(p, RNODE_AND(node)->nd_1st, COND_IN_COND, loc, true);
        RNODE_AND(node)->nd_2nd = cond0(p, RNODE_AND(node)->nd_2nd, COND_IN_COND, loc, true);
        break;

      case NODE_DOT2:
      case NODE_DOT3:
        if (!top) break;
        RNODE_DOT2(node)->nd_beg = range_op(p, RNODE_DOT2(node)->nd_beg, loc);
        RNODE_DOT2(node)->nd_end = range_op(p, RNODE_DOT2(node)->nd_end, loc);
        if (nd_type_p(node, NODE_DOT2)) nd_set_type(node,NODE_FLIP2);
        else if (nd_type_p(node, NODE_DOT3)) nd_set_type(node, NODE_FLIP3);
        break;

      case NODE_DSYM:
      warn_symbol:
        SWITCH_BY_COND_TYPE(type, warning, "symbol ");
        break;

      case NODE_LIT:
        if (RB_TYPE_P(RNODE_LIT(node)->nd_lit, T_REGEXP)) {
            if (!e_option_supplied(p)) SWITCH_BY_COND_TYPE(type, warn, "regex ");
            nd_set_type(node, NODE_MATCH);
        }
        else if (RNODE_LIT(node)->nd_lit == Qtrue ||
                 RNODE_LIT(node)->nd_lit == Qfalse) {
            /* booleans are OK, e.g., while true */
        }
        else if (SYMBOL_P(RNODE_LIT(node)->nd_lit)) {
            goto warn_symbol;
        }
        else {
            SWITCH_BY_COND_TYPE(type, warning, "");
        }
      default:
        break;
    }
    return node;
}

static NODE*
cond(struct parser_params *p, NODE *node, const YYLTYPE *loc)
{
    if (node == 0) return 0;
    return cond0(p, node, COND_IN_COND, loc, true);
}

static NODE*
method_cond(struct parser_params *p, NODE *node, const YYLTYPE *loc)
{
    if (node == 0) return 0;
    return cond0(p, node, COND_IN_OP, loc, true);
}

static NODE*
new_nil_at(struct parser_params *p, const rb_code_position_t *pos)
{
    YYLTYPE loc = {*pos, *pos};
    return NEW_NIL(&loc);
}

static NODE*
new_if(struct parser_params *p, NODE *cc, NODE *left, NODE *right, const YYLTYPE *loc)
{
    if (!cc) return right;
    cc = cond0(p, cc, COND_IN_COND, loc, true);
    return newline_node(NEW_IF(cc, left, right, loc));
}

static NODE*
new_unless(struct parser_params *p, NODE *cc, NODE *left, NODE *right, const YYLTYPE *loc)
{
    if (!cc) return right;
    cc = cond0(p, cc, COND_IN_COND, loc, true);
    return newline_node(NEW_UNLESS(cc, left, right, loc));
}

#define NEW_AND_OR(type, f, s, loc) (type == NODE_AND ? NEW_AND(f,s,loc) : NEW_OR(f,s,loc))

static NODE*
logop(struct parser_params *p, ID id, NODE *left, NODE *right,
          const YYLTYPE *op_loc, const YYLTYPE *loc)
{
    enum node_type type = id == idAND || id == idANDOP ? NODE_AND : NODE_OR;
    NODE *op;
    value_expr(left);
    if (left && nd_type_p(left, type)) {
        NODE *node = left, *second;
        while ((second = RNODE_AND(node)->nd_2nd) != 0 && nd_type_p(second, type)) {
            node = second;
        }
        RNODE_AND(node)->nd_2nd = NEW_AND_OR(type, second, right, loc);
        nd_set_line(RNODE_AND(node)->nd_2nd, op_loc->beg_pos.lineno);
        left->nd_loc.end_pos = loc->end_pos;
        return left;
    }
    op = NEW_AND_OR(type, left, right, loc);
    nd_set_line(op, op_loc->beg_pos.lineno);
    return op;
}

#undef NEW_AND_OR

static void
no_blockarg(struct parser_params *p, NODE *node)
{
    if (nd_type_p(node, NODE_BLOCK_PASS)) {
        compile_error(p, "block argument should not be given");
    }
}

static NODE *
ret_args(struct parser_params *p, NODE *node)
{
    if (node) {
        no_blockarg(p, node);
        if (nd_type_p(node, NODE_LIST) && !RNODE_LIST(node)->nd_next) {
            node = RNODE_LIST(node)->nd_head;
        }
    }
    return node;
}

static NODE *
new_yield(struct parser_params *p, NODE *node, const YYLTYPE *loc)
{
    if (node) no_blockarg(p, node);

    return NEW_YIELD(node, loc);
}

static VALUE
negate_lit(struct parser_params *p, VALUE lit)
{
    if (FIXNUM_P(lit)) {
        return LONG2FIX(-FIX2LONG(lit));
    }
    if (SPECIAL_CONST_P(lit)) {
#if USE_FLONUM
        if (FLONUM_P(lit)) {
            return DBL2NUM(-RFLOAT_VALUE(lit));
        }
#endif
        goto unknown;
    }
    switch (BUILTIN_TYPE(lit)) {
      case T_BIGNUM:
        bignum_negate(lit);
        lit = rb_big_norm(lit);
        break;
      case T_RATIONAL:
        rational_set_num(lit, negate_lit(p, rational_get_num(lit)));
        break;
      case T_COMPLEX:
        rcomplex_set_real(lit, negate_lit(p, rcomplex_get_real(lit)));
        rcomplex_set_imag(lit, negate_lit(p, rcomplex_get_imag(lit)));
        break;
      case T_FLOAT:
        lit = DBL2NUM(-RFLOAT_VALUE(lit));
        break;
      unknown:
      default:
        rb_parser_fatal(p, "unknown literal type (%s) passed to negate_lit",
                        rb_builtin_class_name(lit));
        break;
    }
    return lit;
}

static NODE *
arg_blk_pass(NODE *node1, rb_node_block_pass_t *node2)
{
    if (node2) {
        if (!node1) return (NODE *)node2;
        node2->nd_head = node1;
        nd_set_first_lineno(node2, nd_first_lineno(node1));
        nd_set_first_column(node2, nd_first_column(node1));
        return (NODE *)node2;
    }
    return node1;
}

static bool
args_info_empty_p(struct rb_args_info *args)
{
    if (args->pre_args_num) return false;
    if (args->post_args_num) return false;
    if (args->rest_arg) return false;
    if (args->opt_args) return false;
    if (args->block_arg) return false;
    if (args->kw_args) return false;
    if (args->kw_rest_arg) return false;
    return true;
}

static rb_node_args_t *
new_args(struct parser_params *p, rb_node_args_aux_t *pre_args, rb_node_opt_arg_t *opt_args, ID rest_arg, rb_node_args_aux_t *post_args, rb_node_args_t *tail, const YYLTYPE *loc)
{
    struct rb_args_info *args = &tail->nd_ainfo;

    if (args->forwarding) {
        if (rest_arg) {
            yyerror1(&RNODE(tail)->nd_loc, "... after rest argument");
            return tail;
        }
        rest_arg = idFWD_REST;
    }

    args->pre_args_num   = pre_args ? rb_long2int(pre_args->nd_plen) : 0;
    args->pre_init       = pre_args ? pre_args->nd_next : 0;

    args->post_args_num  = post_args ? rb_long2int(post_args->nd_plen) : 0;
    args->post_init      = post_args ? post_args->nd_next : 0;
    args->first_post_arg = post_args ? post_args->nd_pid : 0;

    args->rest_arg       = rest_arg;

    args->opt_args       = opt_args;

#ifdef FORWARD_ARGS_WITH_RUBY2_KEYWORDS
    args->ruby2_keywords = args->forwarding;
#else
    args->ruby2_keywords = 0;
#endif

    nd_set_loc(RNODE(tail), loc);

    return tail;
}

static rb_node_args_t *
new_args_tail(struct parser_params *p, rb_node_kw_arg_t *kw_args, ID kw_rest_arg, ID block, const YYLTYPE *kw_rest_loc)
{
    rb_node_args_t *node = NEW_ARGS(&NULL_LOC);
    struct rb_args_info *args = &node->nd_ainfo;
    if (p->error_p) return node;

    args->block_arg      = block;
    args->kw_args        = kw_args;

    if (kw_args) {
        /*
         * def foo(k1: 1, kr1:, k2: 2, **krest, &b)
         * variable order: k1, kr1, k2, &b, internal_id, krest
         * #=> <reorder>
         * variable order: kr1, k1, k2, internal_id, krest, &b
         */
        ID kw_bits = internal_id(p), *required_kw_vars, *kw_vars;
        struct vtable *vtargs = p->lvtbl->args;
        rb_node_kw_arg_t *kwn = kw_args;

        if (block) block = vtargs->tbl[vtargs->pos-1];
        vtable_pop(vtargs, !!block + !!kw_rest_arg);
        required_kw_vars = kw_vars = &vtargs->tbl[vtargs->pos];
        while (kwn) {
            if (!NODE_REQUIRED_KEYWORD_P(get_nd_value(p, kwn->nd_body)))
                --kw_vars;
            --required_kw_vars;
            kwn = kwn->nd_next;
        }

        for (kwn = kw_args; kwn; kwn = kwn->nd_next) {
            ID vid = get_nd_vid(p, kwn->nd_body);
            if (NODE_REQUIRED_KEYWORD_P(get_nd_value(p, kwn->nd_body))) {
                *required_kw_vars++ = vid;
            }
            else {
                *kw_vars++ = vid;
            }
        }

        arg_var(p, kw_bits);
        if (kw_rest_arg) arg_var(p, kw_rest_arg);
        if (block) arg_var(p, block);

        args->kw_rest_arg = NEW_DVAR(kw_rest_arg, kw_rest_loc);
    }
    else if (kw_rest_arg == idNil) {
        args->no_kwarg = 1;
    }
    else if (kw_rest_arg) {
        args->kw_rest_arg = NEW_DVAR(kw_rest_arg, kw_rest_loc);
    }

    return node;
}

static rb_node_args_t *
args_with_numbered(struct parser_params *p, rb_node_args_t *args, int max_numparam)
{
    if (max_numparam > NO_PARAM) {
        if (!args) {
            YYLTYPE loc = RUBY_INIT_YYLLOC();
            args = new_args_tail(p, 0, 0, 0, 0);
            nd_set_loc(RNODE(args), &loc);
        }
        args->nd_ainfo.pre_args_num = max_numparam;
    }
    return args;
}

static NODE*
new_array_pattern(struct parser_params *p, NODE *constant, NODE *pre_arg, NODE *aryptn, const YYLTYPE *loc)
{
    RNODE_ARYPTN(aryptn)->nd_pconst = constant;

    if (pre_arg) {
        NODE *pre_args = NEW_LIST(pre_arg, loc);
        if (RNODE_ARYPTN(aryptn)->pre_args) {
            RNODE_ARYPTN(aryptn)->pre_args = list_concat(pre_args, RNODE_ARYPTN(aryptn)->pre_args);
        }
        else {
            RNODE_ARYPTN(aryptn)->pre_args = pre_args;
        }
    }
    return aryptn;
}

static NODE*
new_array_pattern_tail(struct parser_params *p, NODE *pre_args, int has_rest, NODE *rest_arg, NODE *post_args, const YYLTYPE *loc)
{
    if (has_rest) {
        rest_arg = rest_arg ? rest_arg : NODE_SPECIAL_NO_NAME_REST;
    }
    else {
        rest_arg = NULL;
    }
    NODE *node = NEW_ARYPTN(pre_args, rest_arg, post_args, loc);

    return node;
}

static NODE*
new_find_pattern(struct parser_params *p, NODE *constant, NODE *fndptn, const YYLTYPE *loc)
{
    RNODE_FNDPTN(fndptn)->nd_pconst = constant;

    return fndptn;
}

static NODE*
new_find_pattern_tail(struct parser_params *p, NODE *pre_rest_arg, NODE *args, NODE *post_rest_arg, const YYLTYPE *loc)
{
    pre_rest_arg = pre_rest_arg ? pre_rest_arg : NODE_SPECIAL_NO_NAME_REST;
    post_rest_arg = post_rest_arg ? post_rest_arg : NODE_SPECIAL_NO_NAME_REST;
    NODE *node = NEW_FNDPTN(pre_rest_arg, args, post_rest_arg, loc);

    return node;
}

static NODE*
new_hash_pattern(struct parser_params *p, NODE *constant, NODE *hshptn, const YYLTYPE *loc)
{
    RNODE_HSHPTN(hshptn)->nd_pconst = constant;
    return hshptn;
}

static NODE*
new_hash_pattern_tail(struct parser_params *p, NODE *kw_args, ID kw_rest_arg, const YYLTYPE *loc)
{
    NODE *node, *kw_rest_arg_node;

    if (kw_rest_arg == idNil) {
        kw_rest_arg_node = NODE_SPECIAL_NO_REST_KEYWORD;
    }
    else if (kw_rest_arg) {
        kw_rest_arg_node = assignable(p, kw_rest_arg, 0, loc);
    }
    else {
        kw_rest_arg_node = NULL;
    }

    node = NEW_HSHPTN(0, kw_args, kw_rest_arg_node, loc);

    return node;
}

static NODE*
dsym_node(struct parser_params *p, NODE *node, const YYLTYPE *loc)
{
    VALUE lit;

    if (!node) {
        return NEW_LIT(ID2SYM(idNULL), loc);
    }

    switch (nd_type(node)) {
      case NODE_DSTR:
        nd_set_type(node, NODE_DSYM);
        nd_set_loc(node, loc);
        break;
      case NODE_STR:
        lit = str_to_sym_check(p, RNODE_STR(node)->nd_lit, &RNODE(node)->nd_loc);
        RB_OBJ_WRITTEN(p->ast, Qnil, RNODE_STR(node)->nd_lit = ID2SYM(rb_intern_str(lit)));
        nd_set_type(node, NODE_LIT);
        nd_set_loc(node, loc);
        break;
      default:
        node = NEW_DSYM(Qnil, 1, NEW_LIST(node, loc), loc);
        break;
    }
    return node;
}

static int
append_literal_keys(st_data_t k, st_data_t v, st_data_t h)
{
    NODE *node = (NODE *)v;
    NODE **result = (NODE **)h;
    RNODE_LIST(node)->as.nd_alen = 2;
    RNODE_LIST(RNODE_LIST(node)->nd_next)->as.nd_end = RNODE_LIST(node)->nd_next;
    RNODE_LIST(RNODE_LIST(node)->nd_next)->nd_next = 0;
    if (*result)
        list_concat(*result, node);
    else
        *result = node;
    return ST_CONTINUE;
}

static NODE *
remove_duplicate_keys(struct parser_params *p, NODE *hash)
{
    struct st_hash_type literal_type = {
        literal_cmp,
        literal_hash,
    };

    st_table *literal_keys = st_init_table_with_size(&literal_type, RNODE_LIST(hash)->as.nd_alen / 2);
    NODE *result = 0;
    NODE *last_expr = 0;
    rb_code_location_t loc = hash->nd_loc;
    while (hash && RNODE_LIST(hash)->nd_next) {
        NODE *head = RNODE_LIST(hash)->nd_head;
        NODE *value = RNODE_LIST(hash)->nd_next;
        NODE *next = RNODE_LIST(value)->nd_next;
        st_data_t key = (st_data_t)head;
        st_data_t data;
        RNODE_LIST(value)->nd_next = 0;
        if (!head) {
            key = (st_data_t)value;
        }
        else if (nd_type_p(head, NODE_LIT) &&
                 st_delete(literal_keys, (key = (st_data_t)RNODE_LIT(head)->nd_lit, &key), &data)) {
            NODE *dup_value = (RNODE_LIST((NODE *)data))->nd_next;
            rb_compile_warn(p->ruby_sourcefile, nd_line((NODE *)data),
                            "key %"PRIsVALUE" is duplicated and overwritten on line %d",
                            RNODE_LIT(head)->nd_lit, nd_line(head));
            if (dup_value == last_expr) {
                RNODE_LIST(value)->nd_head = block_append(p, RNODE_LIST(dup_value)->nd_head, RNODE_LIST(value)->nd_head);
            }
            else {
                RNODE_LIST(last_expr)->nd_head = block_append(p, RNODE_LIST(dup_value)->nd_head, RNODE_LIST(last_expr)->nd_head);
            }
        }
        st_insert(literal_keys, (st_data_t)key, (st_data_t)hash);
        last_expr = !head || nd_type_p(head, NODE_LIT) ? value : head;
        hash = next;
    }
    st_foreach(literal_keys, append_literal_keys, (st_data_t)&result);
    st_free_table(literal_keys);
    if (hash) {
        if (!result) result = hash;
        else list_concat(result, hash);
    }
    result->nd_loc = loc;
    return result;
}

static NODE *
new_hash(struct parser_params *p, NODE *hash, const YYLTYPE *loc)
{
    if (hash) hash = remove_duplicate_keys(p, hash);
    return NEW_HASH(hash, loc);
}
#endif

static void
error_duplicate_pattern_variable(struct parser_params *p, ID id, const YYLTYPE *loc)
{
    if (is_private_local_id(p, id)) {
        return;
    }
    if (st_is_member(p->pvtbl, (st_data_t)id)) {
        yyerror1(loc, "duplicated variable name");
    }
    else {
        st_insert(p->pvtbl, (st_data_t)id, 0);
    }
}

static void
error_duplicate_pattern_key(struct parser_params *p, ID key, const YYLTYPE *loc)
{
    if (!p->pktbl) {
        p->pktbl = st_init_numtable();
    }
    else if (st_is_member(p->pktbl, (st_data_t)key)) {
        yyerror1(loc, "duplicated key name");
        return;
    }
    st_insert(p->pktbl, (st_data_t)key, 0);
}

#ifndef RIPPER
static NODE *
new_unique_key_hash(struct parser_params *p, NODE *hash, const YYLTYPE *loc)
{
    return NEW_HASH(hash, loc);
}
#endif /* !RIPPER */

#ifndef RIPPER
static NODE *
new_op_assign(struct parser_params *p, NODE *lhs, ID op, NODE *rhs, struct lex_context ctxt, const YYLTYPE *loc)
{
    NODE *asgn;

    if (lhs) {
        ID vid = get_nd_vid(p, lhs);
        YYLTYPE lhs_loc = lhs->nd_loc;
        int shareable = ctxt.shareable_constant_value;
        if (shareable) {
            switch (nd_type(lhs)) {
              case NODE_CDECL:
              case NODE_COLON2:
              case NODE_COLON3:
                break;
              default:
                shareable = 0;
                break;
            }
        }
        if (op == tOROP) {
            rhs = shareable_constant_value(p, shareable, lhs, rhs, &rhs->nd_loc);
            set_nd_value(p, lhs, rhs);
            nd_set_loc(lhs, loc);
            asgn = NEW_OP_ASGN_OR(gettable(p, vid, &lhs_loc), lhs, loc);
        }
        else if (op == tANDOP) {
            if (shareable) {
                rhs = shareable_constant_value(p, shareable, lhs, rhs, &rhs->nd_loc);
            }
            set_nd_value(p, lhs, rhs);
            nd_set_loc(lhs, loc);
            asgn = NEW_OP_ASGN_AND(gettable(p, vid, &lhs_loc), lhs, loc);
        }
        else {
            asgn = lhs;
            rhs = NEW_CALL(gettable(p, vid, &lhs_loc), op, NEW_LIST(rhs, &rhs->nd_loc), loc);
            if (shareable) {
                rhs = shareable_constant_value(p, shareable, lhs, rhs, &rhs->nd_loc);
            }
            set_nd_value(p, asgn, rhs);
            nd_set_loc(asgn, loc);
        }
    }
    else {
        asgn = NEW_ERROR(loc);
    }
    return asgn;
}

static NODE *
new_ary_op_assign(struct parser_params *p, NODE *ary,
                  NODE *args, ID op, NODE *rhs, const YYLTYPE *args_loc, const YYLTYPE *loc)
{
    NODE *asgn;

    args = make_list(args, args_loc);
    asgn = NEW_OP_ASGN1(ary, op, args, rhs, loc);
    fixpos(asgn, ary);
    return asgn;
}

static NODE *
new_attr_op_assign(struct parser_params *p, NODE *lhs,
                   ID atype, ID attr, ID op, NODE *rhs, const YYLTYPE *loc)
{
    NODE *asgn;

    asgn = NEW_OP_ASGN2(lhs, CALL_Q_P(atype), attr, op, rhs, loc);
    fixpos(asgn, lhs);
    return asgn;
}

static NODE *
new_const_op_assign(struct parser_params *p, NODE *lhs, ID op, NODE *rhs, struct lex_context ctxt, const YYLTYPE *loc)
{
    NODE *asgn;

    if (lhs) {
        rhs = shareable_constant_value(p, ctxt.shareable_constant_value, lhs, rhs, loc);
        asgn = NEW_OP_CDECL(lhs, op, rhs, loc);
    }
    else {
        asgn = NEW_ERROR(loc);
    }
    fixpos(asgn, lhs);
    return asgn;
}

static NODE *
const_decl(struct parser_params *p, NODE *path, const YYLTYPE *loc)
{
    if (p->ctxt.in_def) {
        yyerror1(loc, "dynamic constant assignment");
    }
    return NEW_CDECL(0, 0, (path), loc);
}
#else
static VALUE
const_decl(struct parser_params *p, VALUE path)
{
    if (p->ctxt.in_def) {
        path = assign_error(p, "dynamic constant assignment", path);
    }
    return path;
}

static VALUE
assign_error(struct parser_params *p, const char *mesg, VALUE a)
{
    a = dispatch2(assign_error, ERR_MESG(), a);
    ripper_error(p);
    return a;
}

static VALUE
var_field(struct parser_params *p, VALUE a)
{
    return ripper_new_yylval(p, get_id(a), dispatch1(var_field, a), 0);
}
#endif

#ifndef RIPPER
static NODE *
new_bodystmt(struct parser_params *p, NODE *head, NODE *rescue, NODE *rescue_else, NODE *ensure, const YYLTYPE *loc)
{
    NODE *result = head;
    if (rescue) {
        NODE *tmp = rescue_else ? rescue_else : rescue;
        YYLTYPE rescue_loc = code_loc_gen(&head->nd_loc, &tmp->nd_loc);

        result = NEW_RESCUE(head, rescue, rescue_else, &rescue_loc);
        nd_set_line(result, rescue->nd_loc.beg_pos.lineno);
    }
    else if (rescue_else) {
        result = block_append(p, result, rescue_else);
    }
    if (ensure) {
        result = NEW_ENSURE(result, ensure, loc);
    }
    fixpos(result, head);
    return result;
}
#endif

static void
warn_unused_var(struct parser_params *p, struct local_vars *local)
{
    int cnt;

    if (!local->used) return;
    cnt = local->used->pos;
    if (cnt != local->vars->pos) {
        rb_parser_fatal(p, "local->used->pos != local->vars->pos");
    }
#ifndef RIPPER
    ID *v = local->vars->tbl;
    ID *u = local->used->tbl;
    for (int i = 0; i < cnt; ++i) {
        if (!v[i] || (u[i] & LVAR_USED)) continue;
        if (is_private_local_id(p, v[i])) continue;
        rb_warn1L((int)u[i], "assigned but unused variable - %"PRIsWARN, rb_id2str(v[i]));
    }
#endif
}

static void
local_push(struct parser_params *p, int toplevel_scope)
{
    struct local_vars *local;
    int inherits_dvars = toplevel_scope && compile_for_eval;
    int warn_unused_vars = RTEST(ruby_verbose);

    local = ALLOC(struct local_vars);
    local->prev = p->lvtbl;
    local->args = vtable_alloc(0);
    local->vars = vtable_alloc(inherits_dvars ? DVARS_INHERIT : DVARS_TOPSCOPE);
#ifndef RIPPER
    if (toplevel_scope && compile_for_eval) warn_unused_vars = 0;
    if (toplevel_scope && e_option_supplied(p)) warn_unused_vars = 0;
    local->numparam.outer = 0;
    local->numparam.inner = 0;
    local->numparam.current = 0;
#endif
    local->used = warn_unused_vars ? vtable_alloc(0) : 0;

# if WARN_PAST_SCOPE
    local->past = 0;
# endif
    CMDARG_PUSH(0);
    COND_PUSH(0);
    p->lvtbl = local;
}

static void
vtable_chain_free(struct parser_params *p, struct vtable *table)
{
    while (!DVARS_TERMINAL_P(table)) {
        struct vtable *cur_table = table;
        table = cur_table->prev;
        vtable_free(cur_table);
    }
}

static void
local_free(struct parser_params *p, struct local_vars *local)
{
    vtable_chain_free(p, local->used);

# if WARN_PAST_SCOPE
    vtable_chain_free(p, local->past);
# endif

    vtable_chain_free(p, local->args);
    vtable_chain_free(p, local->vars);

    ruby_sized_xfree(local, sizeof(struct local_vars));
}

static void
local_pop(struct parser_params *p)
{
    struct local_vars *local = p->lvtbl->prev;
    if (p->lvtbl->used) {
        warn_unused_var(p, p->lvtbl);
    }

    local_free(p, p->lvtbl);
    p->lvtbl = local;

    CMDARG_POP();
    COND_POP();
}

#ifndef RIPPER
static rb_ast_id_table_t *
local_tbl(struct parser_params *p)
{
    int cnt_args = vtable_size(p->lvtbl->args);
    int cnt_vars = vtable_size(p->lvtbl->vars);
    int cnt = cnt_args + cnt_vars;
    int i, j;
    rb_ast_id_table_t *tbl;

    if (cnt <= 0) return 0;
    tbl = rb_ast_new_local_table(p->ast, cnt);
    MEMCPY(tbl->ids, p->lvtbl->args->tbl, ID, cnt_args);
    /* remove IDs duplicated to warn shadowing */
    for (i = 0, j = cnt_args; i < cnt_vars; ++i) {
        ID id = p->lvtbl->vars->tbl[i];
        if (!vtable_included(p->lvtbl->args, id)) {
            tbl->ids[j++] = id;
        }
    }
    if (j < cnt) {
        tbl = rb_ast_resize_latest_local_table(p->ast, j);
    }

    return tbl;
}

#endif

static void
numparam_name(struct parser_params *p, ID id)
{
    if (!NUMPARAM_ID_P(id)) return;
    compile_error(p, "_%d is reserved for numbered parameter",
        NUMPARAM_ID_TO_IDX(id));
}

static void
arg_var(struct parser_params *p, ID id)
{
    numparam_name(p, id);
    vtable_add(p->lvtbl->args, id);
}

static void
local_var(struct parser_params *p, ID id)
{
    numparam_name(p, id);
    vtable_add(p->lvtbl->vars, id);
    if (p->lvtbl->used) {
        vtable_add(p->lvtbl->used, (ID)p->ruby_sourceline);
    }
}

static int
local_id_ref(struct parser_params *p, ID id, ID **vidrefp)
{
    struct vtable *vars, *args, *used;

    vars = p->lvtbl->vars;
    args = p->lvtbl->args;
    used = p->lvtbl->used;

    while (vars && !DVARS_TERMINAL_P(vars->prev)) {
        vars = vars->prev;
        args = args->prev;
        if (used) used = used->prev;
    }

    if (vars && vars->prev == DVARS_INHERIT) {
        return rb_local_defined(id, p->parent_iseq);
    }
    else if (vtable_included(args, id)) {
        return 1;
    }
    else {
        int i = vtable_included(vars, id);
        if (i && used && vidrefp) *vidrefp = &used->tbl[i-1];
        return i != 0;
    }
}

static int
local_id(struct parser_params *p, ID id)
{
    return local_id_ref(p, id, NULL);
}

static int
check_forwarding_args(struct parser_params *p)
{
    if (local_id(p, idFWD_ALL)) return TRUE;
    compile_error(p, "unexpected ...");
    return FALSE;
}

static void
add_forwarding_args(struct parser_params *p)
{
    arg_var(p, idFWD_REST);
#ifndef FORWARD_ARGS_WITH_RUBY2_KEYWORDS
    arg_var(p, idFWD_KWREST);
#endif
    arg_var(p, idFWD_BLOCK);
    arg_var(p, idFWD_ALL);
}

static void
forwarding_arg_check(struct parser_params *p, ID arg, ID all, const char *var)
{
    bool conflict = false;

    struct vtable *vars, *args;

    vars = p->lvtbl->vars;
    args = p->lvtbl->args;

    while (vars && !DVARS_TERMINAL_P(vars->prev)) {
        conflict |= (vtable_included(args, arg) && !(all && vtable_included(args, all)));
        vars = vars->prev;
        args = args->prev;
    }

    bool found = false;
    if (vars && vars->prev == DVARS_INHERIT && !found) {
        found = (rb_local_defined(arg, p->parent_iseq) &&
                 !(all && rb_local_defined(all, p->parent_iseq)));
    }
    else {
        found = (vtable_included(args, arg) &&
                 !(all && vtable_included(args, all)));
    }

    if (!found) {
        compile_error(p, "no anonymous %s parameter", var);
    }
    else if (conflict) {
        compile_error(p, "anonymous %s parameter is also used within block", var);
    }
}

#ifndef RIPPER
static NODE *
new_args_forward_call(struct parser_params *p, NODE *leading, const YYLTYPE *loc, const YYLTYPE *argsloc)
{
    NODE *rest = NEW_LVAR(idFWD_REST, loc);
#ifndef FORWARD_ARGS_WITH_RUBY2_KEYWORDS
    NODE *kwrest = list_append(p, NEW_LIST(0, loc), NEW_LVAR(idFWD_KWREST, loc));
#endif
    rb_node_block_pass_t *block = NEW_BLOCK_PASS(NEW_LVAR(idFWD_BLOCK, loc), loc);
    NODE *args = leading ? rest_arg_append(p, leading, rest, argsloc) : NEW_SPLAT(rest, loc);
#ifndef FORWARD_ARGS_WITH_RUBY2_KEYWORDS
    args = arg_append(p, args, new_hash(p, kwrest, loc), loc);
#endif
    return arg_blk_pass(args, block);
}
#endif

static NODE *
numparam_push(struct parser_params *p)
{
#ifndef RIPPER
    struct local_vars *local = p->lvtbl;
    NODE *inner = local->numparam.inner;
    if (!local->numparam.outer) {
        local->numparam.outer = local->numparam.current;
    }
    local->numparam.inner = 0;
    local->numparam.current = 0;
    return inner;
#else
    return 0;
#endif
}

static void
numparam_pop(struct parser_params *p, NODE *prev_inner)
{
#ifndef RIPPER
    struct local_vars *local = p->lvtbl;
    if (prev_inner) {
        /* prefer first one */
        local->numparam.inner = prev_inner;
    }
    else if (local->numparam.current) {
        /* current and inner are exclusive */
        local->numparam.inner = local->numparam.current;
    }
    if (p->max_numparam > NO_PARAM) {
        /* current and outer are exclusive */
        local->numparam.current = local->numparam.outer;
        local->numparam.outer = 0;
    }
    else {
        /* no numbered parameter */
        local->numparam.current = 0;
    }
#endif
}

static const struct vtable *
dyna_push(struct parser_params *p)
{
    p->lvtbl->args = vtable_alloc(p->lvtbl->args);
    p->lvtbl->vars = vtable_alloc(p->lvtbl->vars);
    if (p->lvtbl->used) {
        p->lvtbl->used = vtable_alloc(p->lvtbl->used);
    }
    return p->lvtbl->args;
}

static void
dyna_pop_vtable(struct parser_params *p, struct vtable **vtblp)
{
    struct vtable *tmp = *vtblp;
    *vtblp = tmp->prev;
# if WARN_PAST_SCOPE
    if (p->past_scope_enabled) {
        tmp->prev = p->lvtbl->past;
        p->lvtbl->past = tmp;
        return;
    }
# endif
    vtable_free(tmp);
}

static void
dyna_pop_1(struct parser_params *p)
{
    struct vtable *tmp;

    if ((tmp = p->lvtbl->used) != 0) {
        warn_unused_var(p, p->lvtbl);
        p->lvtbl->used = p->lvtbl->used->prev;
        vtable_free(tmp);
    }
    dyna_pop_vtable(p, &p->lvtbl->args);
    dyna_pop_vtable(p, &p->lvtbl->vars);
}

static void
dyna_pop(struct parser_params *p, const struct vtable *lvargs)
{
    while (p->lvtbl->args != lvargs) {
        dyna_pop_1(p);
        if (!p->lvtbl->args) {
            struct local_vars *local = p->lvtbl->prev;
            ruby_sized_xfree(p->lvtbl, sizeof(*p->lvtbl));
            p->lvtbl = local;
        }
    }
    dyna_pop_1(p);
}

static int
dyna_in_block(struct parser_params *p)
{
    return !DVARS_TERMINAL_P(p->lvtbl->vars) && p->lvtbl->vars->prev != DVARS_TOPSCOPE;
}

static int
dvar_defined_ref(struct parser_params *p, ID id, ID **vidrefp)
{
    struct vtable *vars, *args, *used;
    int i;

    args = p->lvtbl->args;
    vars = p->lvtbl->vars;
    used = p->lvtbl->used;

    while (!DVARS_TERMINAL_P(vars)) {
        if (vtable_included(args, id)) {
            return 1;
        }
        if ((i = vtable_included(vars, id)) != 0) {
            if (used && vidrefp) *vidrefp = &used->tbl[i-1];
            return 1;
        }
        args = args->prev;
        vars = vars->prev;
        if (!vidrefp) used = 0;
        if (used) used = used->prev;
    }

    if (vars == DVARS_INHERIT && !NUMPARAM_ID_P(id)) {
        return rb_dvar_defined(id, p->parent_iseq);
    }

    return 0;
}

static int
dvar_defined(struct parser_params *p, ID id)
{
    return dvar_defined_ref(p, id, NULL);
}

static int
dvar_curr(struct parser_params *p, ID id)
{
    return (vtable_included(p->lvtbl->args, id) ||
            vtable_included(p->lvtbl->vars, id));
}

static void
reg_fragment_enc_error(struct parser_params* p, VALUE str, int c)
{
    compile_error(p,
        "regexp encoding option '%c' differs from source encoding '%s'",
        c, rb_enc_name(rb_enc_get(str)));
}

#ifndef RIPPER
int
rb_reg_fragment_setenc(struct parser_params* p, VALUE str, int options)
{
    int c = RE_OPTION_ENCODING_IDX(options);

    if (c) {
        int opt, idx;
        rb_char_to_option_kcode(c, &opt, &idx);
        if (idx != ENCODING_GET(str) &&
            !is_ascii_string(str)) {
            goto error;
        }
        ENCODING_SET(str, idx);
    }
    else if (RE_OPTION_ENCODING_NONE(options)) {
        if (!ENCODING_IS_ASCII8BIT(str) &&
            !is_ascii_string(str)) {
            c = 'n';
            goto error;
        }
        rb_enc_associate(str, rb_ascii8bit_encoding());
    }
    else if (rb_is_usascii_enc(p->enc)) {
        if (!is_ascii_string(str)) {
            /* raise in re.c */
            rb_enc_associate(str, rb_usascii_encoding());
        }
        else {
            rb_enc_associate(str, rb_ascii8bit_encoding());
        }
    }
    return 0;

  error:
    return c;
}

static void
reg_fragment_setenc(struct parser_params* p, VALUE str, int options)
{
    int c = rb_reg_fragment_setenc(p, str, options);
    if (c) reg_fragment_enc_error(p, str, c);
}

static int
reg_fragment_check(struct parser_params* p, VALUE str, int options)
{
    VALUE err;
    reg_fragment_setenc(p, str, options);
    err = rb_reg_check_preprocess(str);
    if (err != Qnil) {
        err = rb_obj_as_string(err);
        compile_error(p, "%"PRIsVALUE, err);
        return 0;
    }
    return 1;
}

#ifndef UNIVERSAL_PARSER
typedef struct {
    struct parser_params* parser;
    rb_encoding *enc;
    NODE *succ_block;
    const YYLTYPE *loc;
} reg_named_capture_assign_t;

static int
reg_named_capture_assign_iter(const OnigUChar *name, const OnigUChar *name_end,
          int back_num, int *back_refs, OnigRegex regex, void *arg0)
{
    reg_named_capture_assign_t *arg = (reg_named_capture_assign_t*)arg0;
    struct parser_params* p = arg->parser;
    rb_encoding *enc = arg->enc;
    long len = name_end - name;
    const char *s = (const char *)name;

    return rb_reg_named_capture_assign_iter_impl(p, s, len, enc, &arg->succ_block, arg->loc);
}

static NODE *
reg_named_capture_assign(struct parser_params* p, VALUE regexp, const YYLTYPE *loc)
{
    reg_named_capture_assign_t arg;

    arg.parser = p;
    arg.enc = rb_enc_get(regexp);
    arg.succ_block = 0;
    arg.loc = loc;
    onig_foreach_name(RREGEXP_PTR(regexp), reg_named_capture_assign_iter, &arg);

    if (!arg.succ_block) return 0;
    return RNODE_BLOCK(arg.succ_block)->nd_next;
}
#endif

int
rb_reg_named_capture_assign_iter_impl(struct parser_params *p, const char *s, long len,
          rb_encoding *enc, NODE **succ_block, const rb_code_location_t *loc)
{
    ID var;
    NODE *node, *succ;

    if (!len) return ST_CONTINUE;
    if (!VALID_SYMNAME_P(s, len, enc, ID_LOCAL))
        return ST_CONTINUE;

    var = intern_cstr(s, len, enc);
    if (len < MAX_WORD_LENGTH && rb_reserved_word(s, (int)len)) {
        if (!lvar_defined(p, var)) return ST_CONTINUE;
    }
    node = node_assign(p, assignable(p, var, 0, loc), NEW_LIT(ID2SYM(var), loc), NO_LEX_CTXT, loc);
    succ = *succ_block;
    if (!succ) succ = NEW_ERROR(loc);
    succ = block_append(p, succ, node);
    *succ_block = succ;
    return ST_CONTINUE;
}

static VALUE
parser_reg_compile(struct parser_params* p, VALUE str, int options)
{
    reg_fragment_setenc(p, str, options);
    return rb_parser_reg_compile(p, str, options);
}

VALUE
rb_parser_reg_compile(struct parser_params* p, VALUE str, int options)
{
    return rb_reg_compile(str, options & RE_OPTION_MASK, p->ruby_sourcefile, p->ruby_sourceline);
}

static VALUE
reg_compile(struct parser_params* p, VALUE str, int options)
{
    VALUE re;
    VALUE err;

    err = rb_errinfo();
    re = parser_reg_compile(p, str, options);
    if (NIL_P(re)) {
        VALUE m = rb_attr_get(rb_errinfo(), idMesg);
        rb_set_errinfo(err);
        compile_error(p, "%"PRIsVALUE, m);
        return Qnil;
    }
    return re;
}
#else
static VALUE
parser_reg_compile(struct parser_params* p, VALUE str, int options, VALUE *errmsg)
{
    VALUE err = rb_errinfo();
    VALUE re;
    str = ripper_is_node_yylval(p, str) ? RNODE_RIPPER(str)->nd_cval : str;
    int c = rb_reg_fragment_setenc(p, str, options);
    if (c) reg_fragment_enc_error(p, str, c);
    re = rb_parser_reg_compile(p, str, options);
    if (NIL_P(re)) {
        *errmsg = rb_attr_get(rb_errinfo(), idMesg);
        rb_set_errinfo(err);
    }
    return re;
}
#endif

#ifndef RIPPER
void
rb_ruby_parser_set_options(struct parser_params *p, int print, int loop, int chomp, int split)
{
    p->do_print = print;
    p->do_loop = loop;
    p->do_chomp = chomp;
    p->do_split = split;
}

static NODE *
parser_append_options(struct parser_params *p, NODE *node)
{
    static const YYLTYPE default_location = {{1, 0}, {1, 0}};
    const YYLTYPE *const LOC = &default_location;

    if (p->do_print) {
        NODE *print = (NODE *)NEW_FCALL(rb_intern("print"),
                                NEW_LIST(NEW_GVAR(idLASTLINE, LOC), LOC),
                                LOC);
        node = block_append(p, node, print);
    }

    if (p->do_loop) {
        NODE *irs = NEW_LIST(NEW_GVAR(rb_intern("$/"), LOC), LOC);

        if (p->do_split) {
            ID ifs = rb_intern("$;");
            ID fields = rb_intern("$F");
            NODE *args = NEW_LIST(NEW_GVAR(ifs, LOC), LOC);
            NODE *split = NEW_GASGN(fields,
                                    NEW_CALL(NEW_GVAR(idLASTLINE, LOC),
                                             rb_intern("split"), args, LOC),
                                    LOC);
            node = block_append(p, split, node);
        }
        if (p->do_chomp) {
            NODE *chomp = NEW_LIT(ID2SYM(rb_intern("chomp")), LOC);
            chomp = list_append(p, NEW_LIST(chomp, LOC), NEW_TRUE(LOC));
            irs = list_append(p, irs, NEW_HASH(chomp, LOC));
        }

        node = NEW_WHILE((NODE *)NEW_FCALL(idGets, irs, LOC), node, 1, LOC);
    }

    return node;
}

void
rb_init_parse(void)
{
    /* just to suppress unused-function warnings */
    (void)nodetype;
    (void)nodeline;
}

static ID
internal_id(struct parser_params *p)
{
    return rb_make_temporary_id(vtable_size(p->lvtbl->args) + vtable_size(p->lvtbl->vars));
}
#endif /* !RIPPER */

static void
parser_initialize(struct parser_params *p)
{
    /* note: we rely on TypedData_Make_Struct to set most fields to 0 */
    p->command_start = TRUE;
    p->ruby_sourcefile_string = Qnil;
    p->lex.lpar_beg = -1; /* make lambda_beginning_p() == FALSE at first */
    p->node_id = 0;
    p->delayed.token = Qnil;
    p->frozen_string_literal = -1; /* not specified */
#ifdef RIPPER
    p->result = Qnil;
    p->parsing_thread = Qnil;
#else
    p->error_buffer = Qfalse;
    p->end_expect_token_locations = Qnil;
    p->token_id = 0;
    p->tokens = Qnil;
#endif
    p->debug_buffer = Qnil;
    p->debug_output = rb_ractor_stdout();
    p->enc = rb_utf8_encoding();
    p->exits = 0;
}

#ifdef RIPPER
#define rb_ruby_parser_mark ripper_parser_mark
#define rb_ruby_parser_free ripper_parser_free
#define rb_ruby_parser_memsize ripper_parser_memsize
#endif

void
rb_ruby_parser_mark(void *ptr)
{
    struct parser_params *p = (struct parser_params*)ptr;

    rb_gc_mark(p->lex.input);
    rb_gc_mark(p->lex.lastline);
    rb_gc_mark(p->lex.nextline);
    rb_gc_mark(p->ruby_sourcefile_string);
    rb_gc_mark((VALUE)p->ast);
    rb_gc_mark(p->case_labels);
    rb_gc_mark(p->delayed.token);
#ifndef RIPPER
    rb_gc_mark(p->debug_lines);
    rb_gc_mark(p->error_buffer);
    rb_gc_mark(p->end_expect_token_locations);
    rb_gc_mark(p->tokens);
#else
    rb_gc_mark(p->value);
    rb_gc_mark(p->result);
    rb_gc_mark(p->parsing_thread);
#endif
    rb_gc_mark(p->debug_buffer);
    rb_gc_mark(p->debug_output);
#ifdef YYMALLOC
    rb_gc_mark((VALUE)p->heap);
#endif
}

void
rb_ruby_parser_free(void *ptr)
{
    struct parser_params *p = (struct parser_params*)ptr;
    struct local_vars *local, *prev;
#ifdef UNIVERSAL_PARSER
    rb_parser_config_t *config = p->config;
#endif

    if (p->tokenbuf) {
        ruby_sized_xfree(p->tokenbuf, p->toksiz);
    }

    for (local = p->lvtbl; local; local = prev) {
        prev = local->prev;
        local_free(p, local);
    }

    {
        token_info *ptinfo;
        while ((ptinfo = p->token_info) != 0) {
            p->token_info = ptinfo->next;
            xfree(ptinfo);
        }
    }

    xfree(p->lex.strterm);
    p->lex.strterm = 0;

    xfree(ptr);

#ifdef UNIVERSAL_PARSER
    config->counter--;
    if (config->counter <= 0) {
        rb_ruby_parser_config_free(config);
    }
#endif
}

size_t
rb_ruby_parser_memsize(const void *ptr)
{
    struct parser_params *p = (struct parser_params*)ptr;
    struct local_vars *local;
    size_t size = sizeof(*p);

    size += p->toksiz;
    for (local = p->lvtbl; local; local = local->prev) {
        size += sizeof(*local);
        if (local->vars) size += local->vars->capa * sizeof(ID);
    }
    return size;
}

#ifdef UNIVERSAL_PARSER
rb_parser_config_t *
rb_ruby_parser_config_new(void *(*malloc)(size_t size))
{
    return (rb_parser_config_t *)malloc(sizeof(rb_parser_config_t));
}

void
rb_ruby_parser_config_free(rb_parser_config_t *config)
{
    config->free(config);
}
#endif

#ifndef UNIVERSAL_PARSER
#ifndef RIPPER
static const rb_data_type_t parser_data_type = {
    "parser",
    {
        rb_ruby_parser_mark,
        rb_ruby_parser_free,
        rb_ruby_parser_memsize,
    },
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};
#endif
#endif

#ifndef RIPPER
#undef rb_reserved_word

const struct kwtable *
rb_reserved_word(const char *str, unsigned int len)
{
    return reserved_word(str, len);
}

#ifdef UNIVERSAL_PARSER
rb_parser_t *
rb_ruby_parser_allocate(rb_parser_config_t *config)
{
    /* parser_initialize expects fields to be set to 0 */
    rb_parser_t *p = (rb_parser_t *)config->calloc(1, sizeof(rb_parser_t));
    p->config = config;
    p->config->counter++;
    return p;
}

rb_parser_t *
rb_ruby_parser_new(rb_parser_config_t *config)
{
    /* parser_initialize expects fields to be set to 0 */
    rb_parser_t *p = rb_ruby_parser_allocate(config);
    parser_initialize(p);
    return p;
}
#endif

rb_parser_t *
rb_ruby_parser_set_context(rb_parser_t *p, const struct rb_iseq_struct *base, int main)
{
    p->error_buffer = main ? Qfalse : Qnil;
    p->parent_iseq = base;
    return p;
}

void
rb_ruby_parser_set_script_lines(rb_parser_t *p, VALUE lines)
{
    if (!RTEST(lines)) {
        lines = Qfalse;
    }
    else if (lines == Qtrue) {
        lines = rb_ary_new();
    }
    else {
        Check_Type(lines, T_ARRAY);
        rb_ary_modify(lines);
    }
    p->debug_lines = lines;
}

void
rb_ruby_parser_error_tolerant(rb_parser_t *p)
{
    p->error_tolerant = 1;
    // TODO
    p->end_expect_token_locations = rb_ary_new();
}

void
rb_ruby_parser_keep_tokens(rb_parser_t *p)
{
    p->keep_tokens = 1;
    // TODO
    p->tokens = rb_ary_new();
}

#ifndef UNIVERSAL_PARSER
rb_ast_t*
rb_parser_compile_file_path(VALUE vparser, VALUE fname, VALUE file, int start)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    RB_GC_GUARD(vparser); /* prohibit tail call optimization */
    return rb_ruby_parser_compile_file_path(p, fname, file, start);
}

rb_ast_t*
rb_parser_compile_generic(VALUE vparser, VALUE (*lex_gets)(VALUE, int), VALUE fname, VALUE input, int start)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    RB_GC_GUARD(vparser); /* prohibit tail call optimization */
    return rb_ruby_parser_compile_generic(p, lex_gets, fname, input, start);
}

rb_ast_t*
rb_parser_compile_string(VALUE vparser, const char *f, VALUE s, int line)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    RB_GC_GUARD(vparser); /* prohibit tail call optimization */
    return rb_ruby_parser_compile_string(p, f, s, line);
}

rb_ast_t*
rb_parser_compile_string_path(VALUE vparser, VALUE f, VALUE s, int line)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    RB_GC_GUARD(vparser); /* prohibit tail call optimization */
    return rb_ruby_parser_compile_string_path(p, f, s, line);
}

VALUE
rb_parser_encoding(VALUE vparser)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    return rb_ruby_parser_encoding(p);
}

VALUE
rb_parser_end_seen_p(VALUE vparser)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    return RBOOL(rb_ruby_parser_end_seen_p(p));
}

void
rb_parser_error_tolerant(VALUE vparser)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    rb_ruby_parser_error_tolerant(p);
}

void
rb_parser_set_script_lines(VALUE vparser, VALUE lines)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    rb_ruby_parser_set_script_lines(p, lines);
}

void
rb_parser_keep_tokens(VALUE vparser)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    rb_ruby_parser_keep_tokens(p);
}

VALUE
rb_parser_new(void)
{
    struct parser_params *p;
    VALUE parser = TypedData_Make_Struct(0, struct parser_params,
                                         &parser_data_type, p);
    parser_initialize(p);
    return parser;
}

VALUE
rb_parser_set_context(VALUE vparser, const struct rb_iseq_struct *base, int main)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    rb_ruby_parser_set_context(p, base, main);
    return vparser;
}

void
rb_parser_set_options(VALUE vparser, int print, int loop, int chomp, int split)
{
    struct parser_params *p;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, p);
    rb_ruby_parser_set_options(p, print, loop, chomp, split);
}

VALUE
rb_parser_set_yydebug(VALUE self, VALUE flag)
{
    struct parser_params *p;

    TypedData_Get_Struct(self, struct parser_params, &parser_data_type, p);
    rb_ruby_parser_set_yydebug(p, RTEST(flag));
    return flag;
}
#endif /* !UNIVERSAL_PARSER */

VALUE
rb_ruby_parser_encoding(rb_parser_t *p)
{
    return rb_enc_from_encoding(p->enc);
}

int
rb_ruby_parser_end_seen_p(rb_parser_t *p)
{
    return p->ruby__end__seen;
}

int
rb_ruby_parser_set_yydebug(rb_parser_t *p, int flag)
{
    p->debug = flag;
    return flag;
}
#endif /* !RIPPER */

#ifdef RIPPER
int
rb_ruby_parser_get_yydebug(rb_parser_t *p)
{
    return p->debug;
}

void
rb_ruby_parser_set_value(rb_parser_t *p, VALUE value)
{
    p->value = value;
}

int
rb_ruby_parser_error_p(rb_parser_t *p)
{
    return p->error_p;
}

VALUE
rb_ruby_parser_debug_output(rb_parser_t *p)
{
    return p->debug_output;
}

void
rb_ruby_parser_set_debug_output(rb_parser_t *p, VALUE output)
{
    p->debug_output = output;
}

VALUE
rb_ruby_parser_parsing_thread(rb_parser_t *p)
{
    return p->parsing_thread;
}

void
rb_ruby_parser_set_parsing_thread(rb_parser_t *p, VALUE parsing_thread)
{
    p->parsing_thread = parsing_thread;
}

void
rb_ruby_parser_ripper_initialize(rb_parser_t *p, VALUE (*gets)(struct parser_params*,VALUE), VALUE input, VALUE sourcefile_string, const char *sourcefile, int sourceline)
{
    p->lex.gets = gets;
    p->lex.input = input;
    p->eofp = 0;
    p->ruby_sourcefile_string = sourcefile_string;
    p->ruby_sourcefile = sourcefile;
    p->ruby_sourceline = sourceline;
}

VALUE
rb_ruby_parser_result(rb_parser_t *p)
{
    return p->result;
}

rb_encoding *
rb_ruby_parser_enc(rb_parser_t *p)
{
    return p->enc;
}

VALUE
rb_ruby_parser_ruby_sourcefile_string(rb_parser_t *p)
{
    return p->ruby_sourcefile_string;
}

int
rb_ruby_parser_ruby_sourceline(rb_parser_t *p)
{
    return p->ruby_sourceline;
}

int
rb_ruby_parser_lex_state(rb_parser_t *p)
{
    return p->lex.state;
}

void
rb_ruby_ripper_parse0(rb_parser_t *p)
{
    parser_prepare(p);
    p->ast = rb_ast_new();
    ripper_yyparse((void*)p);
    rb_ast_dispose(p->ast);
    p->ast = 0;
}

int
rb_ruby_ripper_dedent_string(rb_parser_t *p, VALUE string, int width)
{
    return dedent_string(p, string, width);
}

VALUE
rb_ruby_ripper_lex_get_str(rb_parser_t *p, VALUE s)
{
    return lex_get_str(p, s);
}

int
rb_ruby_ripper_initialized_p(rb_parser_t *p)
{
    return p->lex.input != 0;
}

void
rb_ruby_ripper_parser_initialize(rb_parser_t *p)
{
    parser_initialize(p);
}

long
rb_ruby_ripper_column(rb_parser_t *p)
{
    return p->lex.ptok - p->lex.pbeg;
}

long
rb_ruby_ripper_token_len(rb_parser_t *p)
{
    return p->lex.pcur - p->lex.ptok;
}

VALUE
rb_ruby_ripper_lex_lastline(rb_parser_t *p)
{
    return p->lex.lastline;
}

VALUE
rb_ruby_ripper_lex_state_name(struct parser_params *p, int state)
{
    return rb_parser_lex_state_name(p, (enum lex_state_e)state);
}

struct parser_params*
rb_ruby_ripper_parser_allocate(void)
{
    return (struct parser_params *)ruby_xcalloc(1, sizeof(struct parser_params));
}
#endif /* RIPPER */

#ifndef RIPPER
#ifdef YYMALLOC
#define HEAPCNT(n, size) ((n) * (size) / sizeof(YYSTYPE))
/* Keep the order; NEWHEAP then xmalloc and ADD2HEAP to get rid of
 * potential memory leak */
#define NEWHEAP() rb_imemo_tmpbuf_parser_heap(0, p->heap, 0)
#define ADD2HEAP(new, cnt, ptr) ((p->heap = (new))->ptr = (ptr), \
                           (new)->cnt = (cnt), (ptr))

void *
rb_parser_malloc(struct parser_params *p, size_t size)
{
    size_t cnt = HEAPCNT(1, size);
    rb_imemo_tmpbuf_t *n = NEWHEAP();
    void *ptr = xmalloc(size);

    return ADD2HEAP(n, cnt, ptr);
}

void *
rb_parser_calloc(struct parser_params *p, size_t nelem, size_t size)
{
    size_t cnt = HEAPCNT(nelem, size);
    rb_imemo_tmpbuf_t *n = NEWHEAP();
    void *ptr = xcalloc(nelem, size);

    return ADD2HEAP(n, cnt, ptr);
}

void *
rb_parser_realloc(struct parser_params *p, void *ptr, size_t size)
{
    rb_imemo_tmpbuf_t *n;
    size_t cnt = HEAPCNT(1, size);

    if (ptr && (n = p->heap) != NULL) {
        do {
            if (n->ptr == ptr) {
                n->ptr = ptr = xrealloc(ptr, size);
                if (n->cnt) n->cnt = cnt;
                return ptr;
            }
        } while ((n = n->next) != NULL);
    }
    n = NEWHEAP();
    ptr = xrealloc(ptr, size);
    return ADD2HEAP(n, cnt, ptr);
}

void
rb_parser_free(struct parser_params *p, void *ptr)
{
    rb_imemo_tmpbuf_t **prev = &p->heap, *n;

    while ((n = *prev) != NULL) {
        if (n->ptr == ptr) {
            *prev = n->next;
            break;
        }
        prev = &n->next;
    }
}
#endif

void
rb_parser_printf(struct parser_params *p, const char *fmt, ...)
{
    va_list ap;
    VALUE mesg = p->debug_buffer;

    if (NIL_P(mesg)) p->debug_buffer = mesg = rb_str_new(0, 0);
    va_start(ap, fmt);
    rb_str_vcatf(mesg, fmt, ap);
    va_end(ap);
    if (end_with_newline_p(p, mesg)) {
        rb_io_write(p->debug_output, mesg);
        p->debug_buffer = Qnil;
    }
}

static void
parser_compile_error(struct parser_params *p, const rb_code_location_t *loc, const char *fmt, ...)
{
    va_list ap;
    int lineno, column;

    if (loc) {
        lineno = loc->end_pos.lineno;
        column = loc->end_pos.column;
    }
    else {
        lineno = p->ruby_sourceline;
        column = rb_long2int(p->lex.pcur - p->lex.pbeg);
    }

    rb_io_flush(p->debug_output);
    p->error_p = 1;
    va_start(ap, fmt);
    p->error_buffer =
        rb_syntax_error_append(p->error_buffer,
                               p->ruby_sourcefile_string,
                               lineno, column,
                               p->enc, fmt, ap);
    va_end(ap);
}

static size_t
count_char(const char *str, int c)
{
    int n = 0;
    while (str[n] == c) ++n;
    return n;
}

/*
 * strip enclosing double-quotes, same as the default yytnamerr except
 * for that single-quotes matching back-quotes do not stop stripping.
 *
 *  "\"`class' keyword\"" => "`class' keyword"
 */
RUBY_FUNC_EXPORTED size_t
rb_yytnamerr(struct parser_params *p, char *yyres, const char *yystr)
{
    if (*yystr == '"') {
        size_t yyn = 0, bquote = 0;
        const char *yyp = yystr;

        while (*++yyp) {
            switch (*yyp) {
              case '`':
                if (!bquote) {
                    bquote = count_char(yyp+1, '`') + 1;
                    if (yyres) memcpy(&yyres[yyn], yyp, bquote);
                    yyn += bquote;
                    yyp += bquote - 1;
                    break;
                }
                goto default_char;

              case '\'':
                if (bquote && count_char(yyp+1, '\'') + 1 == bquote) {
                    if (yyres) memcpy(yyres + yyn, yyp, bquote);
                    yyn += bquote;
                    yyp += bquote - 1;
                    bquote = 0;
                    break;
                }
                if (yyp[1] && yyp[1] != '\'' && yyp[2] == '\'') {
                    if (yyres) memcpy(yyres + yyn, yyp, 3);
                    yyn += 3;
                    yyp += 2;
                    break;
                }
                goto do_not_strip_quotes;

              case ',':
                goto do_not_strip_quotes;

              case '\\':
                if (*++yyp != '\\')
                    goto do_not_strip_quotes;
                /* Fall through.  */
              default_char:
              default:
                if (yyres)
                    yyres[yyn] = *yyp;
                yyn++;
                break;

              case '"':
              case '\0':
                if (yyres)
                    yyres[yyn] = '\0';
                return yyn;
            }
        }
      do_not_strip_quotes: ;
    }

    if (!yyres) return strlen(yystr);

    return (YYSIZE_T)(yystpcpy(yyres, yystr) - yyres);
}
#endif

#ifdef RIPPER
#ifdef RIPPER_DEBUG
/* :nodoc: */
static VALUE
ripper_validate_object(VALUE self, VALUE x)
{
    if (x == Qfalse) return x;
    if (x == Qtrue) return x;
    if (NIL_P(x)) return x;
    if (UNDEF_P(x))
        rb_raise(rb_eArgError, "Qundef given");
    if (FIXNUM_P(x)) return x;
    if (SYMBOL_P(x)) return x;
    switch (BUILTIN_TYPE(x)) {
      case T_STRING:
      case T_OBJECT:
      case T_ARRAY:
      case T_BIGNUM:
      case T_FLOAT:
      case T_COMPLEX:
      case T_RATIONAL:
        break;
      case T_NODE:
        if (!nd_type_p((NODE *)x, NODE_RIPPER)) {
            rb_raise(rb_eArgError, "NODE given: %p", (void *)x);
        }
        x = ((NODE *)x)->nd_rval;
        break;
      default:
        rb_raise(rb_eArgError, "wrong type of ruby object: %p (%s)",
                 (void *)x, rb_obj_classname(x));
    }
    if (!RBASIC_CLASS(x)) {
        rb_raise(rb_eArgError, "hidden ruby object: %p (%s)",
                 (void *)x, rb_builtin_type_name(TYPE(x)));
    }
    return x;
}
#endif

#define validate(x) ((x) = get_value(x))

static VALUE
ripper_dispatch0(struct parser_params *p, ID mid)
{
    return rb_funcall(p->value, mid, 0);
}

static VALUE
ripper_dispatch1(struct parser_params *p, ID mid, VALUE a)
{
    validate(a);
    return rb_funcall(p->value, mid, 1, a);
}

static VALUE
ripper_dispatch2(struct parser_params *p, ID mid, VALUE a, VALUE b)
{
    validate(a);
    validate(b);
    return rb_funcall(p->value, mid, 2, a, b);
}

static VALUE
ripper_dispatch3(struct parser_params *p, ID mid, VALUE a, VALUE b, VALUE c)
{
    validate(a);
    validate(b);
    validate(c);
    return rb_funcall(p->value, mid, 3, a, b, c);
}

static VALUE
ripper_dispatch4(struct parser_params *p, ID mid, VALUE a, VALUE b, VALUE c, VALUE d)
{
    validate(a);
    validate(b);
    validate(c);
    validate(d);
    return rb_funcall(p->value, mid, 4, a, b, c, d);
}

static VALUE
ripper_dispatch5(struct parser_params *p, ID mid, VALUE a, VALUE b, VALUE c, VALUE d, VALUE e)
{
    validate(a);
    validate(b);
    validate(c);
    validate(d);
    validate(e);
    return rb_funcall(p->value, mid, 5, a, b, c, d, e);
}

static VALUE
ripper_dispatch7(struct parser_params *p, ID mid, VALUE a, VALUE b, VALUE c, VALUE d, VALUE e, VALUE f, VALUE g)
{
    validate(a);
    validate(b);
    validate(c);
    validate(d);
    validate(e);
    validate(f);
    validate(g);
    return rb_funcall(p->value, mid, 7, a, b, c, d, e, f, g);
}

void
ripper_error(struct parser_params *p)
{
    p->error_p = TRUE;
}

VALUE
ripper_value(struct parser_params *p)
{
    (void)yystpcpy; /* may not used in newer bison */

    return p->value;
}

#endif /* RIPPER */
/*
 * Local variables:
 * mode: c
 * c-file-style: "ruby"
 * End:
 */
