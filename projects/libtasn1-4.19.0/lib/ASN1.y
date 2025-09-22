%{
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
%}

/* Prefix symbols and functions with _asn1_ */
/* %define parse.lac full */
%define parse.error verbose
%define api.prefix {_asn1_yy}

%union {
  unsigned int constant;
  char str[ASN1_MAX_NAME_SIZE+1];
  asn1_node node;
}


%token ASSIG "::="
%token <str> NUM
%token <str> IDENTIFIER
%token OPTIONAL
%token INTEGER
%token SIZE
%token OCTET
%token STRING
%token SEQUENCE
%token BIT
%token UNIVERSAL
%token PRIVATE
%token APPLICATION
%token DEFAULT
%token CHOICE
%token OF
%token OBJECT
%token STR_IDENTIFIER
%token BOOLEAN
%token ASN1_TRUE
%token ASN1_FALSE
%token TOKEN_NULL
%token ANY
%token DEFINED
%token BY
%token SET
%token EXPLICIT
%token IMPLICIT
%token DEFINITIONS
%token TAGS
%token BEGIN
%token END
%token UTCTime
%token GeneralizedTime
%token GeneralString
%token NumericString
%token IA5String
%token TeletexString
%token PrintableString
%token UniversalString
%token BMPString
%token UTF8String
%token VisibleString

%token FROM
%token IMPORTS
%token ENUMERATED

%type <node> octet_string_def constant constant_list type_assig_right
%type <node> integer_def type_assig type_assig_list sequence_def type_def
%type <node> bit_string_def default size_def choise_def object_def
%type <node> boolean_def any_def size_def2 obj_constant obj_constant_list
%type <node> constant_def type_constant type_constant_list definitions
%type <node> definitions_id Time bit_element bit_element_list set_def
%type <node> tag_type tag type_assig_right_tag generalstring_def
%type <node> numericstring_def ia5string_def printablestring_def universalstring_def
%type <node> bmpstring_def utf8string_def visiblestring_def teletexstring_def
%type <node> type_assig_right_tag_default enumerated_def
%type <str>  pos_num neg_num pos_neg_num pos_neg_identifier pos_neg_list
%type <str>  num_identifier
%type <str>  int_identifier
%type <constant> class explicit_implicit
%type <str> known_string

%%


definitions:   definitions_id
               DEFINITIONS explicit_implicit TAGS "::=" BEGIN  /* imports_def */
               type_constant_list END
                   {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_DEFINITIONS|$3);
                    _asn1_set_name($$,_asn1_get_name($1));
                    _asn1_set_name($1,"");
                    _asn1_set_right($1,$7);
                    _asn1_set_down($$,$1);

		    p_tree=$$;
		    }
;

pos_num :   NUM       {snprintf($$,sizeof($$),"%s",$1);}
          | '+' NUM   {snprintf($$,sizeof($$),"%s",$2);}
;

neg_num : '-' NUM     {SAFE_COPY($$,sizeof($$),"-%s",$2);}
;

pos_neg_num :  pos_num  {snprintf($$,sizeof($$),"%s",$1);}
             | neg_num  {snprintf($$,sizeof($$),"%s",$1);}
;

num_identifier :  NUM            {snprintf($$,sizeof($$),"%s",$1);}
                | IDENTIFIER     {snprintf($$,sizeof($$),"%s",$1);}
;

int_identifier :  NUM            {snprintf($$,sizeof($$),"%s",$1);}
                | '-' NUM        {SAFE_COPY($$,sizeof($$),"-%s",$2);}
                | IDENTIFIER     {snprintf($$,sizeof($$),"%s",$1);}
;

pos_neg_identifier :  pos_neg_num    {snprintf($$,sizeof($$),"%s",$1);}
                    | IDENTIFIER     {snprintf($$,sizeof($$),"%s",$1);}
;

constant: '(' pos_neg_num ')'         {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT);
                                       _asn1_set_value($$,$2,strlen($2)+1);}
        | IDENTIFIER'('pos_neg_num')' {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT);
	                               _asn1_set_name($$,$1);
                                       _asn1_set_value($$,$3,strlen($3)+1);}
;

constant_list:  constant                   {$$=$1;}
              | constant_list ',' constant {$$=$1;
                                            _asn1_set_right(_asn1_get_last_right($1),$3);}
;

obj_constant:  num_identifier     {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT);
                                   _asn1_set_value($$,$1,strlen($1)+1);}
             | IDENTIFIER'('NUM')' {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT);
	                            _asn1_set_name($$,$1);
                                    _asn1_set_value($$,$3,strlen($3)+1);}
;

obj_constant_list:  obj_constant                   {$$=$1;}
                  | obj_constant_list obj_constant {$$=$1;
                                                    _asn1_set_right(_asn1_get_last_right($1),$2);}
;

class :  UNIVERSAL    {$$=CONST_UNIVERSAL;}
       | PRIVATE      {$$=CONST_PRIVATE;}
       | APPLICATION  {$$=CONST_APPLICATION;}
;

tag_type :  '[' NUM ']'    {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_TAG);
                            _asn1_set_value($$,$2,strlen($2)+1);}
          | '[' class NUM ']'  {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_TAG | $2);
                                _asn1_set_value($$,$3,strlen($3)+1);}
;

tag :  tag_type           {$$=$1;}
     | tag_type EXPLICIT  {$$=_asn1_mod_type($1,CONST_EXPLICIT);}
     | tag_type IMPLICIT  {$$=_asn1_mod_type($1,CONST_IMPLICIT);}
;

default :  DEFAULT pos_neg_identifier {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_DEFAULT);
                                       _asn1_set_value($$,$2,strlen($2)+1);}
         | DEFAULT ASN1_TRUE           {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_DEFAULT|CONST_TRUE);}
         | DEFAULT ASN1_FALSE          {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_DEFAULT|CONST_FALSE);}
;


pos_neg_list:  pos_neg_num
            |  pos_neg_list '|' pos_neg_num
;


integer_def: INTEGER                    {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_INTEGER);}
           | INTEGER'{'constant_list'}' {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_INTEGER|CONST_LIST);
	                                 _asn1_set_down($$,$3);}
           | integer_def'(' pos_neg_list ')' {$$=$1;}
           | integer_def'('int_identifier'.''.'int_identifier')'
                                        {$$=$1;}
;

boolean_def: BOOLEAN   {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_BOOLEAN);}
;

Time:   UTCTime          {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_UTC_TIME);}
      | GeneralizedTime  {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_GENERALIZED_TIME);}
;

size_def2: SIZE'('num_identifier')'  {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_SIZE|CONST_1_PARAM);
	                              _asn1_set_value($$,$3,strlen($3)+1);}
        | SIZE'('num_identifier'.''.'num_identifier')'
                                     {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_SIZE|CONST_MIN_MAX);
	                              _asn1_set_value($$,$3,strlen($3)+1);
                                      _asn1_set_name($$,$6);}
;

size_def:   size_def2          {$$=$1;}
          | '(' size_def2 ')'  {$$=$2;}
;

generalstring_def: GeneralString {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_GENERALSTRING);}
                | GeneralString size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_GENERALSTRING|CONST_SIZE);
					  _asn1_set_down($$,$2);}
;

numericstring_def: NumericString {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_NUMERIC_STRING|CONST_UNIVERSAL);}
                | NumericString size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_NUMERIC_STRING|CONST_SIZE);
					  _asn1_set_down($$,$2);}
;

ia5string_def: IA5String {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_IA5_STRING);}
                | IA5String size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_IA5_STRING|CONST_SIZE);
					  _asn1_set_down($$,$2);}
;

teletexstring_def: TeletexString {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_TELETEX_STRING);}
                | TeletexString size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_TELETEX_STRING|CONST_SIZE);
					  _asn1_set_down($$,$2);}
;

printablestring_def: PrintableString {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_PRINTABLE_STRING);}
                | PrintableString size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_PRINTABLE_STRING|CONST_SIZE);
					  _asn1_set_down($$,$2);}
;

universalstring_def: UniversalString {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_UNIVERSAL_STRING);}
                | UniversalString size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_UNIVERSAL_STRING|CONST_SIZE);
					  _asn1_set_down($$,$2);}
;

bmpstring_def: BMPString {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_BMP_STRING);}
                | BMPString size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_BMP_STRING|CONST_SIZE);
					  _asn1_set_down($$,$2);}
;

utf8string_def: UTF8String {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_UTF8_STRING);}
                | UTF8String size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_UTF8_STRING|CONST_SIZE);
					  _asn1_set_down($$,$2);}
;

visiblestring_def: VisibleString {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_VISIBLE_STRING);}
                | VisibleString size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_VISIBLE_STRING|CONST_SIZE);
					  _asn1_set_down($$,$2);}
;

octet_string_def : OCTET STRING           {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_OCTET_STRING);}
                 | OCTET STRING size_def  {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_OCTET_STRING|CONST_SIZE);
                                           _asn1_set_down($$,$3);}
;

bit_element :  IDENTIFIER'('NUM')' {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT);
	                           _asn1_set_name($$,$1);
                                    _asn1_set_value($$,$3,strlen($3)+1);}
;

bit_element_list :  bit_element   {$$=$1;}
                  | bit_element_list ',' bit_element  {$$=$1;
                                                       _asn1_set_right(_asn1_get_last_right($1),$3);}
;

bit_string_def : BIT STRING    {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_BIT_STRING);}
               | BIT STRING size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_BIT_STRING|CONST_SIZE);
                                      _asn1_set_down($$,$3);}
               | BIT STRING'{'bit_element_list'}'
                               {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_BIT_STRING|CONST_LIST);
                                _asn1_set_down($$,$4);}
;

enumerated_def : ENUMERATED'{'bit_element_list'}'
                               {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_ENUMERATED|CONST_LIST);
                                _asn1_set_down($$,$3);}
;


object_def :  OBJECT STR_IDENTIFIER {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID);}
;

type_assig_right: IDENTIFIER          {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_IDENTIFIER);
                                       _asn1_set_value($$,$1,strlen($1)+1);}
                | IDENTIFIER size_def {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_IDENTIFIER|CONST_SIZE);
                                       _asn1_set_value($$,$1,strlen($1)+1);
                                       _asn1_set_down($$,$2);}
                | integer_def         {$$=$1;}
                | enumerated_def      {$$=$1;}
                | boolean_def         {$$=$1;}
                | Time
                | octet_string_def    {$$=$1;}
                | bit_string_def      {$$=$1;}
                | generalstring_def   {$$=$1;}
                | numericstring_def   {$$=$1;}
                | ia5string_def       {$$=$1;}
                | teletexstring_def   {$$=$1;}
                | printablestring_def {$$=$1;}
                | universalstring_def {$$=$1;}
                | bmpstring_def       {$$=$1;}
                | utf8string_def      {$$=$1;}
                | visiblestring_def   {$$=$1;}
                | sequence_def        {$$=$1;}
                | object_def          {$$=$1;}
                | choise_def          {$$=$1;}
                | any_def             {$$=$1;}
                | set_def             {$$=$1;}
                | TOKEN_NULL          {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_NULL);}
;

type_assig_right_tag :   type_assig_right     {$$=$1;}
                       | tag type_assig_right {$$=_asn1_mod_type($2,CONST_TAG);
                                               _asn1_set_right($1,_asn1_get_down($$));
                                               _asn1_set_down($$,$1);}
;

type_assig_right_tag_default : type_assig_right_tag   {$$=$1;}
                      | type_assig_right_tag default  {$$=_asn1_mod_type($1,CONST_DEFAULT);
                                                       _asn1_set_right($2,_asn1_get_down($$));
						       _asn1_set_down($$,$2);}
                      | type_assig_right_tag OPTIONAL {$$=_asn1_mod_type($1,CONST_OPTION);}
;

type_assig : IDENTIFIER type_assig_right_tag_default  {$$=_asn1_set_name($2,$1);}
;

type_assig_list : type_assig                   {$$=$1;}
                | type_assig_list','type_assig {$$=$1;
                                                _asn1_set_right(_asn1_get_last_right($1),$3);}
;

sequence_def : SEQUENCE'{'type_assig_list'}' {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_SEQUENCE);
                                              _asn1_set_down($$,$3);}
   | SEQUENCE OF type_assig_right            {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_SEQUENCE_OF);
                                              _asn1_set_down($$,$3);}
   | SEQUENCE size_def OF type_assig_right {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_SEQUENCE_OF|CONST_SIZE);
                                            _asn1_set_right($2,$4);
                                            _asn1_set_down($$,$2);}
;

set_def :  SET'{'type_assig_list'}' {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_SET);
                                     _asn1_set_down($$,$3);}
   | SET OF type_assig_right        {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_SET_OF);
                                     _asn1_set_down($$,$3);}
   | SET size_def OF type_assig_right {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_SET_OF|CONST_SIZE);
                                       _asn1_set_right($2,$4);
                                       _asn1_set_down($$,$2);}
;

choise_def :   CHOICE'{'type_assig_list'}'  {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_CHOICE);
                                             _asn1_set_down($$,$3);}
;

any_def :  ANY                         {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_ANY);}
         | ANY DEFINED BY IDENTIFIER   {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_ANY|CONST_DEFINED_BY);
                                        _asn1_set_down($$,_asn1_add_static_node(&e_list, ASN1_ETYPE_CONSTANT));
	                                _asn1_set_name(_asn1_get_down($$),$4);}
;

known_string: UTF8String { SAFE_COPY($$,sizeof($$),"%s",last_token); }
	 | VisibleString { SAFE_COPY($$,sizeof($$),"%s",last_token); }
	 | PrintableString { SAFE_COPY($$,sizeof($$),"%s",last_token); }
	 | UniversalString { SAFE_COPY($$,sizeof($$),"%s",last_token); }
	 | IA5String { SAFE_COPY($$,sizeof($$),"%s",last_token); }
	 | NumericString { SAFE_COPY($$,sizeof($$),"%s",last_token); }
	 | TeletexString { SAFE_COPY($$,sizeof($$),"%s",last_token); }
	 | BMPString { SAFE_COPY($$,sizeof($$),"%s",last_token); }

/* This matches build-in types which are redefined */
type_invalid : known_string "::=" '[' class NUM ']' IMPLICIT OCTET STRING {
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
	fprintf(stderr, "%s:%u: Warning: %s is a built-in ASN.1 type.\n", file_name, line_number, $1);
#endif
}
;

type_def : IDENTIFIER "::=" type_assig_right_tag  { $$=_asn1_set_name($3,$1);}
;

constant_def :  IDENTIFIER OBJECT STR_IDENTIFIER "::=" '{'obj_constant_list'}'
                        {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID|CONST_ASSIGN);
                         _asn1_set_name($$,$1);
                         _asn1_set_down($$,$6);}
              | IDENTIFIER IDENTIFIER "::=" '{' obj_constant_list '}'
                        {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID|CONST_ASSIGN|CONST_1_PARAM);
                         _asn1_set_name($$,$1);
                         _asn1_set_value($$,$2,strlen($2)+1);
                         _asn1_set_down($$,$5);}
              | IDENTIFIER INTEGER "::=" pos_neg_num
                        {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_INTEGER|CONST_ASSIGN);
                         _asn1_set_name($$,$1);
                         _asn1_set_value($$,$4,strlen($4)+1);}
;

type_constant:   type_def     {$$=$1;}
               | type_invalid {$$=NULL;}
               | constant_def {$$=$1;}
;

type_constant_list :   type_constant    {$$=$1;}
                     | type_constant_list type_constant  {if (!$1)
                                                            {
                                                              $$ = $2;
                                                            }
                                                          else
                                                            {
                                                              $$=$1;
                                                              if ($2) _asn1_set_right(_asn1_get_last_right($1),$2);
                                                            }}
;

definitions_id  :  IDENTIFIER  '{' obj_constant_list '}' {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID);
                                                          _asn1_set_down($$,$3);
                                                          _asn1_set_name($$,$1);}
                 | IDENTIFIER  '{' '}'                   {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID);
                                                          _asn1_set_name($$,$1);}
                 | IDENTIFIER                            {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID);
                                                          _asn1_set_name($$,$1);}
;

/*
identifier_list  :  IDENTIFIER  {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_IDENTIFIER);
                                 _asn1_set_name($$,$1);}
                  | identifier_list IDENTIFIER
                                {$$=$1;
                                 _asn1_set_right(_asn1_get_last_right($$),_asn1_add_static_node(&e_list, ASN1_ETYPE_IDENTIFIER));
                                 _asn1_set_name(_asn1_get_last_right($$),$2);}
;


imports_def :    empty   {$$=NULL;}
              | IMPORTS identifier_list FROM IDENTIFIER obj_constant_list
                        {$$=_asn1_add_static_node(&e_list, ASN1_ETYPE_IMPORTS);
                         _asn1_set_down($$,_asn1_add_static_node(&e_list, ASN1_ETYPE_OBJECT_ID));
                         _asn1_set_name(_asn1_get_down($$),$4);
                         _asn1_set_down(_asn1_get_down($$),$5);
                         _asn1_set_right($$,$2);}
;
*/

explicit_implicit :  EXPLICIT  {$$=CONST_EXPLICIT;}
                   | IMPLICIT  {$$=CONST_IMPLICIT;}
;


%%



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
