/* -*- Mode: C -*-
 * GObject introspection: C lexer
 *
 * Copyright (c) 1997 Sandro Sigala  <ssigala@globalnet.it>
 * Copyright (c) 2007-2008 Jürg Billeter  <j@bitron.ch>
 * Copyright (c) 2010 Andreas Rottmann <a.rottmann@gmx.at>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

%{
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <limits.h>
#else
#include <io.h>
#endif

#include <glib.h>
#include "sourcescanner.h"
#include "scannerparser.h"

int lineno;
char linebuf[2000];

#undef YY_BUF_SIZE
#define YY_BUF_SIZE 1048576

extern int yylex (GISourceScanner *scanner);
#define YY_DECL int yylex (GISourceScanner *scanner)
static int yywrap (void);
static void parse_comment (GISourceScanner *scanner);
static void parse_trigraph (GISourceScanner *scanner);
static void process_linemarks (GISourceScanner *scanner, gboolean has_line);
static int check_identifier (GISourceScanner *scanner, const char *);
static int parse_ignored_macro (void);
static void print_error (GISourceScanner *scanner);

#if (YY_FLEX_MAJOR_VERSION > 2) \
  || ((YY_FLEX_MAJOR_VERSION == 2) && (YY_FLEX_MINOR_VERSION > 6)) \
  || ((YY_FLEX_MAJOR_VERSION == 2) && (YY_FLEX_MINOR_VERSION == 6) && (YY_FLEX_SUBMINOR_VERSION >= 1))
#define IS_EOF 0
#else
#define IS_EOF EOF
#endif

%}

%option nounput

intsuffix				([uU][lL]?[lL]?)|([lL][lL]?[uU]?)
fracconst				([0-9]*\.[0-9]+)|([0-9]+\.)
exppart					[eE][-+]?[0-9]+
floatsuffix				[fFlL]
chartext				([^\\\'])|(\\.)
stringtext				([^\\\"])|(\\.)

%%

\n.*					{ strncpy(linebuf, yytext+1, sizeof(linebuf)); /* save the next line */
						linebuf[sizeof(linebuf)-1]='\0';
						/* printf("%4d:%s\n",lineno,linebuf); */
						yyless(1);      /* give back all but the \n to rescan */
						++lineno;
					}
"\\\n"					{ ++lineno; }

[\t\f\v\r ]+				{ /* Ignore whitespace. */ }

"/*"					{ parse_comment(scanner); }
"/*"[\t ]?<[\t ,=A-Za-z0-9_]+>[\t ]?"*/" { parse_trigraph(scanner); }
"//".*					{ /* Ignore C++ style comments. */ }

"#define "[a-zA-Z_][a-zA-Z_0-9]*"("	{ yyless (yyleng - 1); return FUNCTION_MACRO; }
"#define "[a-zA-Z_][a-zA-Z_0-9]*	{ return OBJECT_MACRO; }
"#ifdef"[\t ]+"__GI_SCANNER__"[\t ]?.*"\n" { ++lineno; return IFDEF_GI_SCANNER; }
"#ifndef"[\t ]+"__GI_SCANNER__"[\t ]?.*"\n" { ++lineno; return IFNDEF_GI_SCANNER; }
"#ifndef ".*"\n"			{ ++lineno; return IFNDEF_COND; }
"#ifdef ".*"\n"				{ ++lineno; return IFDEF_COND; }
"#if ".*"\n"				{ ++lineno; return IF_COND; }
"#elif ".*"\n"				{ ++lineno; return ELIF_COND; }
"#else".*"\n"				{ ++lineno; return ELSE_COND; }
"#endif".*"\n"				{ ++lineno; return ENDIF_COND; }
"#pragma ".*"\n"			{ ++lineno; /* Ignore pragma. */ }

"# "[0-9]+" ".*"\n"			{ ++lineno; process_linemarks(scanner, FALSE); }
"#line "[0-9]+" ".*"\n"			{ ++lineno; process_linemarks(scanner, TRUE); }
"#"			                { }
"{"					{ return '{'; }
"<%"					{ return '{'; }
"}"					{ return '}'; }
"%>"					{ return '}'; }
"["					{ return '['; }
"<:"					{ return '['; }
"]"					{ return ']'; }
":>"					{ return ']'; }
"("					{ return '('; }
")"					{ return ')'; }
";"					{ return ';'; }
":"					{ return ':'; }
"..."					{ return ELLIPSIS; }
"?"					{ return '?'; }
"."					{ return '.'; }
"+"					{ return '+'; }
"-"					{ return '-'; }
"*"					{ return '*'; }
"/"					{ return '/'; }
"%"					{ return '%'; }
"^"					{ return '^'; }
"&"					{ return '&'; }
"|"					{ return '|'; }
"~"					{ return '~'; }
"!"					{ return '!'; }
"="					{ return '='; }
"<"					{ return '<'; }
">"					{ return '>'; }
"+="					{ return ADDEQ; }
"-="					{ return SUBEQ; }
"*="					{ return MULEQ; }
"/="					{ return DIVEQ; }
"%="					{ return MODEQ; }
"^="					{ return XOREQ; }
"&="					{ return ANDEQ; }
"|="					{ return OREQ; }
"<<"					{ return SL; }
">>"					{ return SR; }
"<<="					{ return SLEQ; }
">>="					{ return SREQ; }
"=="					{ return EQ; }
"!="					{ return NOTEQ; }
"<="					{ return LTEQ; }
">="					{ return GTEQ; }
"&&"					{ return ANDAND; }
"||"					{ return OROR; }
"++"					{ return PLUSPLUS; }
"--"					{ return MINUSMINUS; }
","					{ return ','; }
"->"					{ return ARROW; }

"__asm"[\t\f\v\r ]+"volatile"  	        { if (!parse_ignored_macro()) REJECT; }
"__asm__"[\t\f\v\r ]+"volatile"	        { if (!parse_ignored_macro()) REJECT; }
"__asm__"[\t\f\v\r ]+"__volatile__"	{ if (!parse_ignored_macro()) REJECT; }
"__asm" 	        	        { if (!parse_ignored_macro()) REJECT; }
"__asm__" 	        	        { if (!parse_ignored_macro()) REJECT; }
"__attribute__" 		        { if (!parse_ignored_macro()) REJECT; }
"__attribute" 		                { if (!parse_ignored_macro()) REJECT; }
"__const"                               { return CONST; }
"__extension__"                         { return EXTENSION; }
"__inline__"                            { return INLINE; }
"__inline"				{ return INLINE; }
"__nonnull" 			        { if (!parse_ignored_macro()) REJECT; }
"_Nonnull"				{ /* Ignore */ }
"_Nullable"				{ /* Ignore */ }
"_Null_unspecified"			{ /* Ignore */ }
"_Noreturn" 			        { /* Ignore */ }
"__pragma" 		                { if (!parse_ignored_macro()) REJECT; }
"__restrict"				{ return RESTRICT; }
"__restrict__"				{ return RESTRICT; }
"thread_local"				{ return THREAD_LOCAL; }
"_Thread_local"				{ return THREAD_LOCAL; }
"__typeof__"				{ if (parse_ignored_macro()) return VOID; else REJECT; }
"__typeof"				{ if (parse_ignored_macro()) return VOID; else REJECT; }
"__volatile"	        	        { return VOLATILE; }
"__volatile__"	        	        { return VOLATILE; }
"_Bool"					{ return BASIC_TYPE; }
"bool"					{ return BASIC_TYPE; }
"typedef char __static_assert_t".*"\n"	{ ++lineno; /* Ignore */ }
"__cdecl" 	        	        { /* Ignore */ }
"__declspec(deprecated(".*"))"		{ /* Ignore */ }
"__declspec"[\t ]*"("[a-z\t ]+")"	{ /* Ignore */ }
"__stdcall" 			        { /* ignore */ }
"__w64"					{ /* ignore */ }

"G_GINT64_CONSTANT"			{ return INTL_CONST; }
"G_GUINT64_CONSTANT"			{ return INTUL_CONST; }

"TRUE"					{ return BOOLEAN; }
"FALSE"					{ return BOOLEAN; }
"true"					{ return BOOLEAN; }
"false"					{ return BOOLEAN; }

[a-zA-Z_][a-zA-Z_0-9]*			{ if (scanner->macro_scan) return check_identifier(scanner, yytext); else REJECT; }

"asm"           		        { if (!parse_ignored_macro()) REJECT; }
"auto"					{ return AUTO; }
"break"					{ return BREAK; }
"case"					{ return CASE; }
"char"					{ return BASIC_TYPE; }
"const"					{ return CONST; }
"continue"				{ return CONTINUE; }
"default"				{ return DEFAULT; }
"do"					{ return DO; }
"double"				{ return BASIC_TYPE; }
"else"					{ return ELSE; }
"enum"					{ return ENUM; }
"extern"				{ return EXTERN; }
"float"					{ return BASIC_TYPE; }
"_Float16"                              { return BASIC_TYPE; }
"_Float32"                              { return BASIC_TYPE; }
"_Float64"                              { return BASIC_TYPE; }
"_Float128"                             { return BASIC_TYPE; }
"_Float32x"                             { return BASIC_TYPE; }
"_Float64x"                             { return BASIC_TYPE; }
"_Float128x"                            { return BASIC_TYPE; }
"__float80"                             { return BASIC_TYPE; }
"__float128"                            { return BASIC_TYPE; }
"for"					{ return FOR; }
"goto"					{ return GOTO; }
"if"					{ return IF; }
"inline"				{ return INLINE; }
"int"					{ return BASIC_TYPE; }
"__int64"				{ return BASIC_TYPE; }
"__uint128_t"				{ return BASIC_TYPE; }
"__int128_t"				{ return BASIC_TYPE; }
"__uint128"				{ return BASIC_TYPE; }
"__int128"				{ return BASIC_TYPE; }
"long"					{ return BASIC_TYPE; }
"register"				{ return REGISTER; }
"restrict"				{ return RESTRICT; }
"return"				{ return RETURN; }
"short"					{ return BASIC_TYPE; }
"signed"				{ return SIGNED; }
"__signed"                              { return SIGNED; }
"__signed__"				{ return SIGNED; }
"sizeof"				{ return SIZEOF; }
"static"				{ return STATIC; }
"struct"				{ return STRUCT; }
"switch"				{ return SWITCH; }
"typedef"				{ return TYPEDEF; }
"union"					{ return UNION; }
"unsigned"				{ return UNSIGNED; }
"void"					{ return VOID; }
"volatile"				{ return VOLATILE; }
"while"					{ return WHILE; }

[a-zA-Z_][a-zA-Z_0-9]*			{ return check_identifier(scanner, yytext); }

"0"[xX][0-9a-fA-F]+{intsuffix}?		{ return INTEGER; }
"0"[0-7]+{intsuffix}?			{ return INTEGER; }
[0-9]+{intsuffix}?			{ return INTEGER; }

{fracconst}{exppart}?{floatsuffix}?	{ return FLOATING; }
[0-9]+{exppart}{floatsuffix}?		{ return FLOATING; }

"'"{chartext}*"'"			{ return CHARACTER; }
"L'"{chartext}*"'"			{ return CHARACTER; }

"\""{stringtext}*"\""			{ return STRING; }
"L\""{stringtext}*"\""			{ return STRING; }

.					{ print_error(scanner); }

%%

static int
yywrap (void)
{
  return 1;
}

static void
parse_comment (GISourceScanner *scanner)
{
  int c1, c2;
  GString *string = NULL;
  GISourceComment *comment;
  int comment_lineno;
  int skip = FALSE;

  c1 = input();
  c2 = input();

  if (c2 != IS_EOF && (c1 == '*' && c2 != '*' && c2 != '/')) {
    /*
     * Store GTK-Doc comment blocks,
     * starts with one '/' followed by exactly two '*' and not followed by a '/'
     */
    if (!g_hash_table_contains (scanner->files, scanner->current_file)) {
        skip = TRUE;
    } else {
        string = g_string_new (yytext);
    }

    comment_lineno = lineno;

    while (c2 != IS_EOF && !(c1 == '*' && c2 == '/'))
      {
        if (!skip)
          g_string_append_c (string, c1);

        if (c1 == '\n')
          lineno++;

        c1 = c2;
        c2 = input();
      }

    if (skip) {
        return;
    }

    g_string_append (string, "*/");

    comment = g_slice_new (GISourceComment);
    comment->comment = g_string_free (string, FALSE);
    comment->line = comment_lineno;
    comment->filename = g_file_get_parse_name (scanner->current_file);

    gi_source_scanner_take_comment (scanner, comment);
  } else {
    /*
     * Ignore all other comment blocks
     */
    while (c2 != IS_EOF && !(c1 == '*' && c2 == '/'))
      {
        if (c1 == '\n')
          lineno++;

        c1 = c2;
        c2 = input();
      }

    return;
  }
}

static int
check_identifier (GISourceScanner *scanner,
		  const char  *s)
{
	/*
	 * This function checks if `s' is a type name or an
	 * identifier.
	 */

	if (gi_source_scanner_is_typedef (scanner, s)) {
		return TYPEDEF_NAME;
	} else if (strcmp (s, "__builtin_va_list") == 0) {
		return TYPEDEF_NAME;
	}

	return IDENTIFIER;
}

/* taken from glib/gfileutils.c */
#if defined(MAXPATHLEN)
#define G_PATH_LENGTH MAXPATHLEN
#elif defined(PATH_MAX)
#define G_PATH_LENGTH PATH_MAX
#elif defined(_PC_PATH_MAX)
#define G_PATH_LENGTH sysconf(_PC_PATH_MAX)
#else
#define G_PATH_LENGTH 2048
#endif

#ifdef _WIN32
/* We don't want to include <windows.h> as it clashes horribly
 * with token names from scannerparser.h. So just declare
 * GetFullPathNameA() here unless we already defined it, like
 * in giscanner.c.
 */
extern unsigned long __stdcall GetFullPathNameA(const char*, int, char*, char**);
#endif

static inline char *
_realpath (const char *path)
{
#ifndef _WIN32
  char buffer[G_PATH_LENGTH];

  return realpath (path, buffer) ? g_strdup (buffer) : NULL;
#else
  char *buffer;
  char dummy;
  int rc, len;

  rc = GetFullPathNameA (path, 1, &dummy, NULL);
  if (rc == 0)
    {
      /* Weird failure, so just return the input path as such */
      return g_strdup (path);
    }

  len = rc + 1;
  buffer = g_malloc (len);

  rc = GetFullPathNameA (path, len, buffer, NULL);
  if (rc == 0 || rc > len)
    {
      /* Weird failure again */
      g_free (buffer);
      return g_strdup (path);
    }
  return buffer;
#endif
}

/*
 * # linenum "filename" flags
 *  See http://gcc.gnu.org/onlinedocs/cpp/Preprocessor-Output.html
 **/

static void
process_linemarks (GISourceScanner *scanner, gboolean has_line)
{
	char escaped_filename[1025];
	char *filename;
	char *real;

	if (has_line)
		sscanf(yytext, "#line %d \"%1024[^\"]\"", &lineno, escaped_filename);
	else
		sscanf(yytext, "# %d \"%1024[^\"]\"", &lineno, escaped_filename);

	filename = g_strcompress (escaped_filename);

        real = _realpath (filename);
        if (real)
          {
            g_free (filename);
            filename = real;
          }

        if (scanner->current_file)
          g_object_unref (scanner->current_file);
	scanner->current_file = g_file_new_for_path (filename);
	g_free (filename);
}

/*
 * This parses a macro which is ignored, such as
 * __attribute__((x)) or __asm__ (x)
 */
static int
parse_ignored_macro (void)
{
	int c;
	int nest;

	while ((c = input ()) != IS_EOF && isspace (c))
		;
	if (c != '(')
		return FALSE;

	nest = 0;
	while ((c = input ()) != IS_EOF && (nest > 0 || c != ')')) {
		if (c == '(')
			nest++;
		else if (c == ')')
			nest--;
		else if (c == '"') {
			while ((c = input ()) != IS_EOF && c != '"') {
				if (c == '\\')
					c = input ();
			}
		} else if (c == '\'') {
			c = input ();
			if (c == '\\')
				c = input ();
			else if (c == '\'')
				return FALSE;
			c = input ();
			if (c != '\'')
				return FALSE;
		} else if (c == '\n')
			lineno++;
	}

	return TRUE;
}

static void
parse_trigraph (GISourceScanner *scanner)
{
	char **items;
	char *start, *end;
	int i;

	start = g_strstr_len (yytext, yyleng, "<");
	g_assert (start != NULL);
	end = g_strstr_len (yytext, yyleng, ">");
	g_assert (end != NULL);
	*end = '\0';
	items = g_strsplit (start + 1, ",", 0);
	for (i = 0; items[i] != NULL; i++) {
		char *item = items[i];
		g_strstrip (item);
		if (strcmp (item, "public") == 0)
			scanner->private = FALSE;
		else if (strcmp (item, "private") == 0)
			scanner->private = TRUE;
		else if (strcmp (item, "flags") == 0)
			scanner->flags = TRUE;
	}
	g_strfreev (items);
}

static void
print_error (GISourceScanner *scanner)
{
  if (yytext[0]) {
    char *filename = g_file_get_parse_name (scanner->current_file);
    gchar *error = g_strdup_printf ("%s:%d: unexpected character `%c'", filename, lineno, yytext[0]);
    g_ptr_array_add (scanner->errors, error);
    g_free (filename);
  }
}
