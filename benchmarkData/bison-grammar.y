/* This is clanker generated */

/* mega_test.y - “bigger” Bison grammar stress test (parse-only; no %union).
 *
 * Goals:
 * - Lots of tokens + productions + precedence
 * - Multiple top-level constructs (modules, imports, types, traits, impls, funcs)
 * - Statements (control flow, match, try/catch/finally, defer)
 * - Expressions (ternary, coalesce, pipe, ranges, calls, indexing, member access)
 *
 * This is intentionally a syntactic stress test. It does not build an AST.
 */

%{
#include <stdio.h>
#include <stdlib.h>

int yylex(void);
static void yyerror(const char *s) { fprintf(stderr, "parse error: %s\n", s); }
%}

%define parse.error verbose
%define parse.trace true

/* -------------------- TOKENS -------------------- */

/* Identifiers + literals */
%token IDENT
%token INT_LIT FLOAT_LIT STRING_LIT CHAR_LIT
%token TRUE FALSE NULL_LIT

/* Keywords */
%token MODULE IMPORT EXPORT FROM AS
%token PUB PRIV
%token LET VAR CONST STATIC MUT
%token FN LAMBDA
%token TYPEDEF TYPE
%token STRUCT ENUM UNION INTERFACE TRAIT IMPL WHERE
%token EXTENDS IMPLEMENTS
%token IF ELSE
%token FOR WHILE DO
%token SWITCH CASE DEFAULT
%token MATCH
%token BREAK CONTINUE RETURN YIELD
%token TRY CATCH FINALLY THROW DEFER
%token NEW DELETE SIZEOF TYPEOF
%token IN IS AS_OP
%token AWAIT ASYNC

/* Punctuation */
%token LPAREN RPAREN LBRACE RBRACE LBRACK RBRACK
%token COMMA SEMI DOT COLON DCOLON QMARK ARROW FATARROW ELLIPSIS

/* Operators */
%token ASSIGN
%token PLUS MINUS STAR SLASH PERCENT
%token INC DEC
%token EQ NE LT LE GT GE
%token AND OR NOT
%token BITAND BITOR BITXOR BITNOT
%token SHL SHR
%token PLUS_ASSIGN MINUS_ASSIGN STAR_ASSIGN SLASH_ASSIGN PERCENT_ASSIGN
%token AND_ASSIGN OR_ASSIGN XOR_ASSIGN SHL_ASSIGN SHR_ASSIGN
%token COALESCE /* ?? */
%token PIPE     /* |> */
%token RANGE    /* .. */
%token RANGE_INCL /* ..= */

/* -------------------- PRECEDENCE -------------------- */

%right ASSIGN PLUS_ASSIGN MINUS_ASSIGN STAR_ASSIGN SLASH_ASSIGN PERCENT_ASSIGN
%right AND_ASSIGN OR_ASSIGN XOR_ASSIGN SHL_ASSIGN SHR_ASSIGN
%right QMARK COLON
%left OR
%left AND
%left BITOR
%left BITXOR
%left BITAND
%left EQ NE
%left LT LE GT GE IN IS
%left SHL SHR
%left PLUS MINUS
%left STAR SLASH PERCENT
%right COALESCE
%left PIPE
%right NOT BITNOT
%right UMINUS
%left INC DEC
%left DOT DCOLON LBRACK RBRACK LPAREN RPAREN

%start compilation_unit

%%

/* -------------------- TOP LEVEL -------------------- */

compilation_unit
  : opt_shebang opt_module_decl opt_imports opt_top_items
  ;

opt_shebang
  : /* empty */
  | STRING_LIT /* treat as shebang-like prelude */
  ;

opt_module_decl
  : /* empty */
  | MODULE path SEMI
  ;

opt_imports
  : /* empty */
  | opt_imports import_stmt
  ;

import_stmt
  : IMPORT import_spec SEMI
  | EXPORT IMPORT import_spec SEMI
  ;

import_spec
  : STRING_LIT
  | path
  | LBRACE import_list RBRACE FROM STRING_LIT
  | STAR FROM STRING_LIT
  ;

import_list
  : import_item
  | import_list COMMA import_item
  ;

import_item
  : IDENT
  | IDENT AS IDENT
  ;

opt_top_items
  : /* empty */
  | opt_top_items top_item
  ;

top_item
  : SEMI
  | attr_block opt_visibility item
  | item
  ;

opt_visibility
  : /* empty */
  | PUB
  | PRIV
  ;

item
  : type_alias
  | type_decl
  | trait_decl
  | impl_block
  | func_decl
  | global_decl
  ;

attr_block
  : /* empty */
  | attr_block attr
  ;

attr
  : AT_IDENT
  | AT_IDENT LPAREN opt_attr_args RPAREN
  ;

AT_IDENT
  : IDENT /* lexer can emit IDENT for '@foo' too; for stress, keep as IDENT */
  ;

opt_attr_args
  : /* empty */
  | attr_args
  ;

attr_args
  : attr_arg
  | attr_args COMMA attr_arg
  ;

attr_arg
  : IDENT
  | IDENT ASSIGN literal
  | literal
  ;

/* -------------------- TYPES -------------------- */

type_alias
  : TYPEDEF type_expr IDENT SEMI
  | TYPE IDENT ASSIGN type_expr SEMI
  ;

type_decl
  : STRUCT IDENT opt_type_params opt_where_clause LBRACE struct_fields RBRACE
  | ENUM IDENT opt_type_params opt_where_clause LBRACE enum_variants RBRACE
  | UNION IDENT opt_type_params opt_where_clause LBRACE union_fields RBRACE
  | INTERFACE IDENT opt_type_params opt_where_clause LBRACE iface_members RBRACE
  ;

trait_decl
  : TRAIT IDENT opt_type_params opt_where_clause LBRACE trait_members RBRACE
  ;

impl_block
  : IMPL opt_type_params type_expr opt_where_clause LBRACE impl_members RBRACE
  | IMPL opt_type_params type_expr FOR type_expr opt_where_clause LBRACE impl_members RBRACE
  ;

opt_type_params
  : /* empty */
  | LT type_param_list GT
  ;

type_param_list
  : type_param
  | type_param_list COMMA type_param
  ;

type_param
  : IDENT
  | IDENT COLON type_bound_list
  | IDENT ASSIGN type_expr
  ;

type_bound_list
  : type_bound
  | type_bound_list PLUS type_bound
  ;

type_bound
  : path
  | path LT type_arg_list GT
  ;

type_arg_list
  : type_expr
  | type_arg_list COMMA type_expr
  ;

opt_where_clause
  : /* empty */
  | WHERE where_list
  ;

where_list
  : where_item
  | where_list COMMA where_item
  ;

where_item
  : IDENT COLON type_bound_list
  | type_expr COLON type_bound_list
  ;

struct_fields
  : /* empty */
  | struct_fields struct_field
  ;

struct_field
  : attr_block opt_visibility type_expr IDENT SEMI
  | attr_block opt_visibility type_expr IDENT ASSIGN expr SEMI
  ;

union_fields
  : /* empty */
  | union_fields union_field
  ;

union_field
  : attr_block opt_visibility type_expr IDENT SEMI
  ;

enum_variants
  : /* empty */
  | enum_variants enum_variant
  ;

enum_variant
  : attr_block opt_visibility IDENT opt_variant_payload opt_variant_discriminant COMMA
  | attr_block opt_visibility IDENT opt_variant_payload opt_variant_discriminant
  ;

opt_variant_payload
  : /* empty */
  | LPAREN opt_type_list RPAREN
  | LBRACE struct_fields RBRACE
  ;

opt_variant_discriminant
  : /* empty */
  | ASSIGN expr
  ;

iface_members
  : /* empty */
  | iface_members iface_member
  ;

iface_member
  : func_sig SEMI
  | assoc_type SEMI
  ;

trait_members
  : /* empty */
  | trait_members trait_member
  ;

trait_member
  : func_sig SEMI
  | assoc_type SEMI
  | const_sig SEMI
  ;

assoc_type
  : TYPE IDENT
  | TYPE IDENT ASSIGN type_expr
  ;

const_sig
  : CONST type_expr IDENT
  | CONST type_expr IDENT ASSIGN expr
  ;

impl_members
  : /* empty */
  | impl_members impl_member
  ;

impl_member
  : func_decl
  | global_decl
  | type_alias
  ;

/* -------------------- FUNCTIONS -------------------- */

func_decl
  : opt_async FN IDENT opt_type_params LPAREN opt_params RPAREN opt_ret_type opt_where_clause block
  | opt_async FN IDENT opt_type_params LPAREN opt_params RPAREN opt_ret_type opt_where_clause SEMI
  ;

opt_async
  : /* empty */
  | ASYNC
  ;

func_sig
  : opt_async FN IDENT opt_type_params LPAREN opt_params RPAREN opt_ret_type opt_where_clause
  ;

opt_ret_type
  : /* empty */
  | ARROW type_expr
  ;

opt_params
  : /* empty */
  | param_list
  ;

param_list
  : param
  | param_list COMMA param
  ;

param
  : attr_block param_kind type_expr IDENT opt_param_default
  | attr_block param_kind type_expr IDENT LBRACK RBRACK opt_param_default
  | attr_block param_kind type_expr ELLIPSIS IDENT opt_param_default
  ;

param_kind
  : /* empty */
  | MUT
  ;

opt_param_default
  : /* empty */
  | ASSIGN expr
  ;

/* -------------------- DECLARATIONS -------------------- */

global_decl
  : storage decl_list SEMI
  ;

storage
  : LET
  | VAR
  | CONST
  | STATIC
  ;

decl_list
  : decl_item
  | decl_list COMMA decl_item
  ;

decl_item
  : pattern opt_type_annot opt_init
  ;

opt_type_annot
  : /* empty */
  | COLON type_expr
  ;

opt_init
  : /* empty */
  | ASSIGN expr
  ;

/* -------------------- STATEMENTS -------------------- */

block
  : LBRACE stmt_list RBRACE
  ;

stmt_list
  : /* empty */
  | stmt_list stmt
  ;

stmt
  : SEMI
  | block
  | labeled_stmt
  | decl_stmt
  | expr_stmt
  | if_stmt
  | while_stmt
  | do_while_stmt
  | for_stmt
  | switch_stmt
  | match_stmt
  | jump_stmt
  | try_stmt
  | throw_stmt
  | defer_stmt
  ;

labeled_stmt
  : IDENT COLON stmt
  ;

decl_stmt
  : storage decl_list SEMI
  ;

expr_stmt
  : expr SEMI
  ;

if_stmt
  : IF LPAREN expr RPAREN stmt
  | IF LPAREN expr RPAREN stmt ELSE stmt
  ;

while_stmt
  : WHILE LPAREN expr RPAREN stmt
  ;

do_while_stmt
  : DO stmt WHILE LPAREN expr RPAREN SEMI
  ;

for_stmt
  : FOR LPAREN opt_for_init SEMI opt_for_cond SEMI opt_for_step RPAREN stmt
  | FOR LPAREN pattern IN expr RPAREN stmt
  ;

opt_for_init
  : /* empty */
  | decl_stmt /* contains semicolon; but stress-case: accept it */
  | expr_list
  ;

opt_for_cond
  : /* empty */
  | expr
  ;

opt_for_step
  : /* empty */
  | expr_list
  ;

expr_list
  : expr
  | expr_list COMMA expr
  ;

switch_stmt
  : SWITCH LPAREN expr RPAREN LBRACE case_blocks opt_default RBRACE
  ;

case_blocks
  : /* empty */
  | case_blocks case_block
  ;

case_block
  : CASE case_value_list COLON stmt_list
  ;

case_value_list
  : expr
  | case_value_list COMMA expr
  ;

opt_default
  : /* empty */
  | DEFAULT COLON stmt_list
  ;

match_stmt
  : MATCH LPAREN expr RPAREN LBRACE match_arms RBRACE
  ;

match_arms
  : /* empty */
  | match_arms match_arm
  ;

match_arm
  : attr_block pattern opt_match_guard FATARROW match_arm_body COMMA
  | attr_block pattern opt_match_guard FATARROW match_arm_body
  ;

opt_match_guard
  : /* empty */
  | IF expr
  ;

match_arm_body
  : expr
  | block
  ;

jump_stmt
  : BREAK opt_label SEMI
  | CONTINUE opt_label SEMI
  | RETURN opt_expr SEMI
  | YIELD opt_expr SEMI
  ;

opt_label
  : /* empty */
  | IDENT
  ;

opt_expr
  : /* empty */
  | expr
  ;

try_stmt
  : TRY block catch_blocks opt_finally
  | TRY expr SEMI catch_blocks opt_finally
  ;

catch_blocks
  : CATCH LPAREN pattern RPAREN block
  | catch_blocks CATCH LPAREN pattern RPAREN block
  ;

opt_finally
  : /* empty */
  | FINALLY block
  ;

throw_stmt
  : THROW expr SEMI
  ;

defer_stmt
  : DEFER stmt
  ;

/* -------------------- PATTERNS -------------------- */

pattern
  : IDENT
  | UNDERSCORE
  | literal
  | tuple_pattern
  | array_pattern
  | struct_pattern
  | enum_pattern
  | pattern OR_PATTERN pattern
  ;

UNDERSCORE
  : IDENT /* lexer can map '_' to IDENT; for stress keep it */
  ;

OR_PATTERN
  : BITOR /* reuse | token */
  ;

tuple_pattern
  : LPAREN opt_pattern_list RPAREN
  ;

opt_pattern_list
  : /* empty */
  | pattern_list
  ;

pattern_list
  : pattern
  | pattern_list COMMA pattern
  ;

array_pattern
  : LBRACK opt_pattern_list RBRACK
  | LBRACK opt_pattern_list COMMA ELLIPSIS RBRACK
  ;

struct_pattern
  : path LBRACE struct_pat_fields RBRACE
  ;

struct_pat_fields
  : /* empty */
  | struct_pat_fields struct_pat_field
  ;

struct_pat_field
  : IDENT COLON pattern COMMA
  | IDENT COLON pattern
  | IDENT COMMA
  | IDENT
  | ELLIPSIS
  ;

enum_pattern
  : path LPAREN opt_pattern_list RPAREN
  | path LBRACE struct_pat_fields RBRACE
  ;

/* -------------------- EXPRESSIONS -------------------- */

expr
  : assignment
  ;

assignment
  : conditional
  | lvalue ASSIGN assignment
  | lvalue PLUS_ASSIGN assignment
  | lvalue MINUS_ASSIGN assignment
  | lvalue STAR_ASSIGN assignment
  | lvalue SLASH_ASSIGN assignment
  | lvalue PERCENT_ASSIGN assignment
  | lvalue AND_ASSIGN assignment
  | lvalue OR_ASSIGN assignment
  | lvalue XOR_ASSIGN assignment
  | lvalue SHL_ASSIGN assignment
  | lvalue SHR_ASSIGN assignment
  ;

conditional
  : pipe
  | pipe QMARK expr COLON conditional
  ;

pipe
  : coalesce
  | pipe PIPE coalesce
  ;

coalesce
  : logical_or
  | coalesce COALESCE logical_or
  ;

logical_or
  : logical_and
  | logical_or OR logical_and
  ;

logical_and
  : bit_or
  | logical_and AND bit_or
  ;

bit_or
  : bit_xor
  | bit_or BITOR bit_xor
  ;

bit_xor
  : bit_and
  | bit_xor BITXOR bit_and
  ;

bit_and
  : equality
  | bit_and BITAND equality
  ;

equality
  : relational
  | equality EQ relational
  | equality NE relational
  ;

relational
  : shift
  | relational LT shift
  | relational LE shift
  | relational GT shift
  | relational GE shift
  | relational IN shift
  | relational IS shift
  ;

shift
  : additive
  | shift SHL additive
  | shift SHR additive
  ;

additive
  : multiplicative
  | additive PLUS multiplicative
  | additive MINUS multiplicative
  ;

multiplicative
  : unary
  | multiplicative STAR unary
  | multiplicative SLASH unary
  | multiplicative PERCENT unary
  ;

unary
  : postfix
  | NOT unary
  | BITNOT unary
  | MINUS unary %prec UMINUS
  | INC unary
  | DEC unary
  | SIZEOF unary
  | TYPEOF unary
  | NEW type_expr
  | DELETE unary
  | AWAIT unary
  ;

postfix
  : primary
  | postfix INC
  | postfix DEC
  | postfix LBRACK opt_slice RBRACK
  | postfix LPAREN opt_args RPAREN
  | postfix DOT IDENT
  | postfix DCOLON IDENT
  ;

opt_slice
  : expr
  | opt_expr COLON opt_expr
  | opt_expr RANGE opt_expr
  | opt_expr RANGE_INCL opt_expr
  | /* empty */
  ;

opt_args
  : /* empty */
  | arg_list
  ;

arg_list
  : arg
  | arg_list COMMA arg
  ;

arg
  : expr
  | IDENT COLON expr
  ;

primary
  : literal
  | IDENT
  | path
  | LPAREN expr RPAREN
  | tuple_expr
  | array_expr
  | object_expr
  | lambda_expr
  | if_expr
  | match_expr
  | try_expr
  ;

literal
  : INT_LIT
  | FLOAT_LIT
  | STRING_LIT
  | CHAR_LIT
  | TRUE
  | FALSE
  | NULL_LIT
  ;

tuple_expr
  : LPAREN opt_expr_list RPAREN
  ;

opt_expr_list
  : /* empty */
  | expr_list
  ;

array_expr
  : LBRACK opt_array_items RBRACK
  ;

opt_array_items
  : /* empty */
  | array_items
  ;

array_items
  : array_item
  | array_items COMMA array_item
  ;

array_item
  : expr
  | expr ELLIPSIS expr
  ;

object_expr
  : LBRACE obj_fields RBRACE
  ;

obj_fields
  : /* empty */
  | obj_fields obj_field
  ;

obj_field
  : IDENT COLON expr COMMA
  | IDENT COLON expr
  | ELLIPSIS expr COMMA
  | ELLIPSIS expr
  ;

lambda_expr
  : LAMBDA opt_lambda_params FATARROW lambda_body
  | FN opt_type_params LPAREN opt_params RPAREN opt_ret_type opt_where_clause lambda_body
  ;

opt_lambda_params
  : /* empty */
  | lambda_params
  ;

lambda_params
  : pattern
  | lambda_params COMMA pattern
  ;

lambda_body
  : expr
  | block
  ;

if_expr
  : IF LPAREN expr RPAREN expr ELSE expr
  | IF LPAREN expr RPAREN block ELSE block
  ;

match_expr
  : MATCH LPAREN expr RPAREN LBRACE match_arms RBRACE
  ;

try_expr
  : TRY expr
  | TRY block
  ;

lvalue
  : IDENT
  | postfix
  ;

path
  : IDENT
  | path DCOLON IDENT
  | path LT type_arg_list GT
  ;

/* -------------------- TYPE EXPRESSIONS -------------------- */

type_expr
  : type_primary
  | type_expr STAR
  | type_expr AMP
  | type_expr QMARK
  | LPAREN type_expr RPAREN
  ;

type_primary
  : IDENT
  | path
  | tuple_type
  | array_type
  | fn_type
  ;

tuple_type
  : LPAREN opt_type_list RPAREN
  ;

opt_type_list
  : /* empty */
  | type_list
  ;

type_list
  : type_expr
  | type_list COMMA type_expr
  ;

array_type
  : LBRACK type_expr RBRACK
  | LBRACK type_expr SEMI expr RBRACK
  ;

fn_type
  : FN LPAREN opt_type_list RPAREN opt_ret_type
  ;

AMP
  : BITAND /* reuse token */
  ;

%%

/* End mega_test.y */

