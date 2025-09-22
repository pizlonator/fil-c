/* parser.y
 *
 * This file is part of kbd project.
 * Copyright (C) 1993  Risto Kankkunen.
 * Copyright (C) 1993  Eugene G. Crosser.
 * Copyright (C) 1994-2007  Andries E. Brouwer.
 * Copyright (C) 2007-2013  Alexey Gladkov <gladkov.alexey@gmail.com>
 *
 * This file is covered by the GNU General Public License,
 * which should be included with kbd as the file COPYING.
 */
%{
#include "config.h"
#include "libcommon.h"

#include "contextP.h"
#include "ksyms.h"
#include "modifiers.h"

#include "parser.h"
#include "analyze.h"
%}

%code requires {
#include <kbdfile.h>
#include "keymap.h"

#ifndef STRDATA_STRUCT
#define STRDATA_STRUCT
#define MAX_PARSER_STRING 512
struct strdata {
	unsigned long len;
	unsigned char data[MAX_PARSER_STRING];
};
#endif
}

%language "C"
%defines
%debug
%define parse.error verbose

/* Pure yylex.  */
%define api.pure
%lex-param { yyscan_t scanner }

/* Pure yyparse.  */
%parse-param { void *scanner }
%parse-param { struct lk_ctx *ctx }

%token EOL NUMBER LITERAL CHARSET KEYMAPS KEYCODE EQUALS
%token PLAIN SHIFT CONTROL ALT ALTGR SHIFTL SHIFTR CTRLL CTRLR CAPSSHIFT
%token COMMA DASH STRING STRLITERAL COMPOSE TO CCHAR ERROR PLUS
%token UNUMBER ALT_IS_META STRINGS AS USUAL ON FOR

%union {
	int num;
	struct strdata str;
}

%type <str>  STRLITERAL
%type <num>  CCHAR
%type <num>  LITERAL
%type <num>  NUMBER
%type <num>  UNUMBER
%type <num>  compsym
%type <num>  rvalue

%{
static int
yyerror(yyscan_t scanner __attribute__ ((unused)),
        struct lk_ctx *ctx, const char *s)
{
	ERR(ctx, "%s", s);
	return 0;
}

static int
strings_as_usual(struct lk_ctx *ctx)
{
	/*
	 * 26 strings, mostly inspired by the VT100 family
	 */
	const char *stringvalues[30] = {
		/* F1 .. F20 */
		"\033[[A",  "\033[[B",  "\033[[C",  "\033[[D",  "\033[[E",
		"\033[17~", "\033[18~", "\033[19~", "\033[20~", "\033[21~",
		"\033[23~", "\033[24~", "\033[25~", "\033[26~",
		"\033[28~", "\033[29~",
		"\033[31~", "\033[32~", "\033[33~", "\033[34~",
		/* Find,    Insert,     Remove,     Select,     Prior */
		"\033[1~",  "\033[2~",  "\033[3~",  "\033[4~",  "\033[5~",
		/* Next,    Macro,      Help,       Do,         Pause */
		"\033[6~",  0,          0,          0,          0
	};
	unsigned char i;

	for (i = 0; i < 30; i++) {
		if (stringvalues[i]) {
			struct kbsentry ke;
			ke.kb_func = i;
			strncpy((char *)ke.kb_string, stringvalues[i], sizeof(ke.kb_string));
			ke.kb_string[sizeof(ke.kb_string) - 1] = 0;

			if (lk_add_func(ctx, &ke) == -1)
				return -1;
		}
	}
	return 0;
}

static int
compose_as_usual(struct lk_ctx *ctx, char *charset)
{
	if (charset && strcmp(charset, "iso-8859-1")) {
		ERR(ctx, _("loadkeys: don't know how to compose for %s"), charset);
		return -1;

	} else {
		struct ccc {
			unsigned char c1, c2, c3;
		} def_latin1_composes[68] = {
			{ '`', 'A', 0300 }, { '`', 'a', 0340 },
			{ '\'', 'A', 0301 }, { '\'', 'a', 0341 },
			{ '^', 'A', 0302 }, { '^', 'a', 0342 },
			{ '~', 'A', 0303 }, { '~', 'a', 0343 },
			{ '"', 'A', 0304 }, { '"', 'a', 0344 },
			{ 'O', 'A', 0305 }, { 'o', 'a', 0345 },
			{ '0', 'A', 0305 }, { '0', 'a', 0345 },
			{ 'A', 'A', 0305 }, { 'a', 'a', 0345 },
			{ 'A', 'E', 0306 }, { 'a', 'e', 0346 },
			{ ',', 'C', 0307 }, { ',', 'c', 0347 },
			{ '`', 'E', 0310 }, { '`', 'e', 0350 },
			{ '\'', 'E', 0311 }, { '\'', 'e', 0351 },
			{ '^', 'E', 0312 }, { '^', 'e', 0352 },
			{ '"', 'E', 0313 }, { '"', 'e', 0353 },
			{ '`', 'I', 0314 }, { '`', 'i', 0354 },
			{ '\'', 'I', 0315 }, { '\'', 'i', 0355 },
			{ '^', 'I', 0316 }, { '^', 'i', 0356 },
			{ '"', 'I', 0317 }, { '"', 'i', 0357 },
			{ '-', 'D', 0320 }, { '-', 'd', 0360 },
			{ '~', 'N', 0321 }, { '~', 'n', 0361 },
			{ '`', 'O', 0322 }, { '`', 'o', 0362 },
			{ '\'', 'O', 0323 }, { '\'', 'o', 0363 },
			{ '^', 'O', 0324 }, { '^', 'o', 0364 },
			{ '~', 'O', 0325 }, { '~', 'o', 0365 },
			{ '"', 'O', 0326 }, { '"', 'o', 0366 },
			{ '/', 'O', 0330 }, { '/', 'o', 0370 },
			{ '`', 'U', 0331 }, { '`', 'u', 0371 },
			{ '\'', 'U', 0332 }, { '\'', 'u', 0372 },
			{ '^', 'U', 0333 }, { '^', 'u', 0373 },
			{ '"', 'U', 0334 }, { '"', 'u', 0374 },
			{ '\'', 'Y', 0335 }, { '\'', 'y', 0375 },
			{ 'T', 'H', 0336 }, { 't', 'h', 0376 },
			{ 's', 's', 0337 }, { '"', 'y', 0377 },
			{ 's', 'z', 0337 }, { 'i', 'j', 0377 }
		};
		int i;
		for (i = 0; i < 68; i++) {
			struct lk_kbdiacr ptr;
			struct ccc c = def_latin1_composes[i];

			ptr.diacr  = c.c1;
			ptr.base   = c.c2;
			ptr.result = c.c3;

			if (lk_append_compose(ctx, &ptr) == -1)
				return -1;
		}
	}
	return 0;
}

%}

%%
keytable	:
		| keytable line
		;
line		: EOL
		| charsetline
		| altismetaline
		| usualstringsline
		| usualcomposeline
		| keymapline
		| singleline
		| strline
                | compline
		;
charsetline	: CHARSET STRLITERAL EOL
			{
				if (lk_set_charset(ctx, (char *) $2.data)) {
					ERR(ctx,
						_("unknown charset %s - ignoring charset request\n"),
						(char *) $2.data);
					YYERROR;
				}
				ctx->keywords |= LK_KEYWORD_CHARSET;

				/* Unicode: The first 256 code points were made
				   identical to the content of ISO 8859-1 */
				if (ctx->flags & LK_FLAG_PREFER_UNICODE &&
				    !strcasecmp((char *) $2.data, "iso-8859-1"))
					ctx->flags ^= LK_FLAG_PREFER_UNICODE;
			}
		;
altismetaline	: ALT_IS_META EOL
			{
				ctx->keywords |= LK_KEYWORD_ALTISMETA;
			}
		;
usualstringsline: STRINGS AS USUAL EOL
			{
				if (strings_as_usual(ctx) == -1)
					YYERROR;
				ctx->keywords |= LK_KEYWORD_STRASUSUAL;
			}
		;
usualcomposeline: COMPOSE AS USUAL FOR STRLITERAL EOL
			{
				if (compose_as_usual(ctx, (char *) $5.data) == -1)
					YYERROR;
			}
		  | COMPOSE AS USUAL EOL
			{
				if (compose_as_usual(ctx, 0) == -1)
					YYERROR;
			}
		;
keymapline	: KEYMAPS range EOL
			{
				ctx->keywords |= LK_KEYWORD_KEYMAPS;
			}
		;
range		: range COMMA range0
		| range0
		;
range0		: NUMBER DASH NUMBER
			{
				int i;
				for (i = $1; i <= $3; i++) {
					if (lk_add_map(ctx, i) == -1)
						YYERROR;
				}
			}
		| NUMBER
			{
				if (lk_add_map(ctx, $1) == -1)
					YYERROR;
			}
		;
strline		: STRING LITERAL EQUALS STRLITERAL EOL
			{
				struct kbsentry ke;

				if (KTYP($2) != KT_FN) {
					ERR(ctx, _("'%s' is not a function key symbol"),
						get_sym(ctx, KTYP($2), KVAL($2)));
					YYERROR;
				}

				ke.kb_func = (unsigned char) KVAL($2);

				strncpy((char *) ke.kb_string,
				        (char *) $4.data,
				        sizeof(ke.kb_string));

				ke.kb_string[sizeof(ke.kb_string) - 1] = 0;

				if (lk_add_func(ctx, &ke) == -1)
					YYERROR;
			}
		;
compline        : COMPOSE compsym compsym TO CCHAR EOL
                        {
				struct lk_kbdiacr ptr;
				ptr.diacr  = (unsigned int) $2;
				ptr.base   = (unsigned int) $3;
				ptr.result = (unsigned int) $5;

				if (lk_append_compose(ctx, &ptr) == -1)
					YYERROR;
			}
		 | COMPOSE compsym compsym TO rvalue EOL
			{
				struct lk_kbdiacr ptr;
				ptr.diacr  = (unsigned int) $2;
				ptr.base   = (unsigned int) $3;
				ptr.result = (unsigned int) $5;

				if (lk_append_compose(ctx, &ptr) == -1)
					YYERROR;
			}
                ;
compsym		: CCHAR		{	$$ = $1;		}
		| UNUMBER	{	$$ = $1 ^ 0xf000;	}
		;
singleline	: KEYCODE NUMBER EQUALS rvalue0 EOL
			{
				int j, i, keycode;
				int *val;

				if (ctx->key_line->count == 1) {
					char one = 1;
					/* Some files do not have a keymaps line, and
					 * we have to wait until all input has been read
					 * before we know which maps to fill. */
					lk_array_set(ctx->key_constant, $2, &one);

					/* On the other hand, we now have include files,
					 * and it should be possible to override lines
					 * from an include file. So, kill old defs. */
					for (j = 0; j < ctx->keymap->total; j++) {
						if (!lk_map_exists(ctx, j))
							continue;

						if (lk_del_key(ctx, j, $2) < 0)
							YYERROR;
					}
				}

				if (ctx->keywords & LK_KEYWORD_KEYMAPS) {
					i = 0;

					for (j = 0; j < ctx->keymap->total; j++) {
						if (!lk_map_exists(ctx, j))
							continue;

						if (ctx->key_line->count != 1 || i == 0) {
							keycode = K_HOLE;

							if (i < ctx->key_line->count) {
								val = lk_array_get(ctx->key_line, i);
								keycode = *val;
							}

							if (lk_add_key(ctx, j, $2, keycode) < 0)
								YYERROR;
						}
						i++;
					}

					if (i < ctx->key_line->count) {
						ERR(ctx, _("too many (%ld) entries on one line"),
							ctx->key_line->count);
						YYERROR;
					}
				} else {
					for (i = 0; i < ctx->key_line->count; i++) {
						val = lk_array_get(ctx->key_line, i);

						if (lk_add_key(ctx, i, $2, *val) < 0)
							YYERROR;
					}
				}
			}

		| modifiers KEYCODE NUMBER EQUALS rvalue EOL
			{
				if (lk_add_key(ctx, ctx->mod, $3, $5) < 0)
					YYERROR;
				ctx->mod = 0;
			}
		| PLAIN KEYCODE NUMBER EQUALS rvalue EOL
			{
				if (lk_add_key(ctx, 0, $3, $5) < 0)
					YYERROR;
				ctx->mod = 0;
			}
		;
modifiers	: modifiers modifier
		| modifier
		;
modifier	: SHIFT		{ ctx->mod |= M_SHIFT;	}
		| CONTROL	{ ctx->mod |= M_CTRL;	}
		| ALT		{ ctx->mod |= M_ALT;		}
		| ALTGR		{ ctx->mod |= M_ALTGR;	}
		| SHIFTL	{ ctx->mod |= M_SHIFTL;	}
		| SHIFTR	{ ctx->mod |= M_SHIFTR;	}
		| CTRLL		{ ctx->mod |= M_CTRLL;	}
		| CTRLR		{ ctx->mod |= M_CTRLR;	}
		| CAPSSHIFT	{ ctx->mod |= M_CAPSSHIFT;	}
		;
		;

rvalue0		:
		| rvalue1 rvalue0
		;
rvalue1		: rvalue
			{
				int val = $1;
				lk_array_append(ctx->key_line, &val);
			}
		;
rvalue		: NUMBER	{ $$ = convert_code(ctx, $1, TO_AUTO);		}
                | PLUS NUMBER	{ $$ = add_capslock(ctx, $2);			}
		| UNUMBER	{ $$ = convert_code(ctx, $1^0xf000, TO_AUTO);	}
		| PLUS UNUMBER	{ $$ = add_capslock(ctx, $2^0xf000);		}
		| LITERAL	{ $$ = $1;					}
                | PLUS LITERAL	{ $$ = add_capslock(ctx, $2);			}
		;
%%

int
lk_parse_keymap(struct lk_ctx *ctx, struct kbdfile *fp)
{
	yyscan_t scanner;
	int rc = -1;

	ctx->mod = 0;

	yylex_init_extra(ctx, &scanner);

	INFO(ctx, _("Loading %s"), kbdfile_get_pathname(fp));

	if (stack_push(ctx, fp, scanner) == -1)
		goto fail;

	if (yyparse(scanner, ctx))
		goto fail;

	rc = 0;

fail:
	stack_pop(ctx, scanner);

	yylex_destroy(scanner);
	return rc;
}
