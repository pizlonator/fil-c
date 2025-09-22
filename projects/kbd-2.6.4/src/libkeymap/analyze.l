%{
#include "config.h"

#include <stdlib.h>
#include <unistd.h> /* readlink */

#include <kbdfile.h>

#include "libcommon.h"
#include "contextP.h"
#include "ksyms.h"
#include "paths.h"

#include "parser.h"
%}

%top {
#include "keymap.h"
int stack_push(struct lk_ctx *ctx, struct kbdfile *fp, void *scanner);
int stack_pop(struct lk_ctx *ctx, void *scanner);
}

%option reentrant
%option bison-bridge
%option stack
%option never-interactive
%option noyywrap
%option nounput
%option noinput
%option noyy_top_state

%option extra-type="struct lk_ctx *"

%{
int
stack_push(struct lk_ctx *ctx, struct kbdfile *fp, void *scanner)
{
	int i = 0;

	while (ctx->stack[i]) i++;

	if (i == MAX_INCLUDE_DEPTH) {
		ERR(ctx, _("includes are nested too deeply"));
		return -1;
	}

	ctx->stack[i] = fp;

	yypush_buffer_state(yy_create_buffer(kbdfile_get_file(fp), YY_BUF_SIZE, scanner), scanner);
	return 0;
}

int
stack_pop(struct lk_ctx *ctx, void *scanner)
{
	int i = 0;

	while (ctx->stack[i]) i++;
	if (!i)
		return 0;
	i--;

	/*
	 * The top of stack is input file for library. No need to close it.
	 */
	if (i) {
		kbdfile_free(ctx->stack[i]);
	}
	ctx->stack[i] = NULL;

	yypop_buffer_state(scanner);
	return 0;
}

/*
 * Where shall we look for an include file?
 * Current strategy (undocumented, may change):
 *
 * 1. Look for a user-specified LOADKEYS_INCLUDE_PATH
 * 2. Try . and ../include and ../../include
 * 3. Try D and D/../include and D/../../include
 *    where D is the directory from where we are loading the current file.
 * 4. Try KD/include and KD/#/include where KD = DATADIR/KEYMAPDIR.
 *
 * Expected layout:
 * KD has subdirectories amiga, atari, i386, mac, sun, include
 * KD/include contains architecture-independent stuff
 * like strings and iso-8859-x compose tables.
 * KD/i386 has subdirectories qwerty, ... and include;
 * this latter include dir contains stuff with keycode=...
 *
 * (Of course, if the present setup turns out to be reasonable,
 * then later also the other architectures will grow and get
 * subdirectories, and the hard-coded i386 below will go again.)
 *
 * People that dislike a dozen lookups for loadkeys
 * can easily do "loadkeys file_with_includes; dumpkeys > my_keymap"
 * and afterwards use only "loadkeys /fullpath/mykeymap", where no
 * lookups are required.
 */
static const char *const include_dirpath0[] = {
	"",
	NULL
};
static const char *const include_dirpath1[] = {
	"../include/",
	"../../include/",
	NULL
};
static const char *const include_dirpath3[] = {
	DATADIR "/" KEYMAPDIR "/include/",
	DATADIR "/" KEYMAPDIR "/i386/include/",
	DATADIR "/" KEYMAPDIR "/mac/include/",
	NULL
};

static const char *const include_suffixes[] = {
	"",
	".inc",
	NULL
};

static int
find_incl_file_near_fn(struct lk_ctx *ctx, char *s, char *fn, struct kbdfile *fp)
{
	const char *include_dirpath2[] = { NULL, NULL, NULL, NULL };
	char *t, *te, *t1 = NULL, *t2 = NULL;
	size_t len;
	int rc = 1;

	if (!fn)
		return 1;

	t = strdup(fn);
	if (t == NULL)
		goto nomem;

	te = strrchr(t, '/');
	if (te) {
		te[1] = 0;
		len = strlen(t);
		include_dirpath2[0] = t;
		include_dirpath2[1] = t1 = malloc(len + 12);
		include_dirpath2[2] = t2 = malloc(len + 15);

		if (t1 == NULL || t2 == NULL)
			goto nomem;

		strcpy(t1, t);
		strcat(t1, "../include/");
		strcpy(t2, t);
		strcat(t2, "../../include/");
		rc = kbdfile_find(s, include_dirpath2, include_suffixes, fp);
		free(t1);
		free(t2);
	}
	free(t);
	return rc;

nomem:	ERR(ctx, _("out of memory"));
	if (t1) free(t1);
	if (t2) free(t2);
	if (t)  free(t);
	return -1;
}

static int
find_standard_incl_file(struct lk_ctx *ctx, char *s, struct kbdfile *fp)
{
	char *pathname;
	int rc = 1;
	int i = 0;

	while (ctx->stack[i]) i++;
	if (i == 0)
		return -1;
	i--;
	pathname = kbdfile_get_pathname(ctx->stack[i]);

	if (kbdfile_find(s, include_dirpath1, include_suffixes, fp)) {
		if ((rc = find_incl_file_near_fn(ctx, s, pathname, fp)) == -1)
			return rc;
	}

	/* If filename is a symlink, also look near its target. */
	if (rc) {
		char buf[MAXPATHLEN], path[MAXPATHLEN], *ptr;
		ssize_t n;

		n = readlink(pathname, buf, sizeof(buf));
		if (n > 0 && n < (ssize_t) sizeof(buf)) {
			buf[n] = 0;
			if (buf[0] == '/') {
				rc = find_incl_file_near_fn(ctx, s, buf, fp);

			} else if (strlen(pathname) + (size_t) n < sizeof(path)) {
				strcpy(path, pathname);
				path[sizeof(path) - 1] = 0;
				ptr = strrchr(path, '/');
				if (ptr)
					ptr[1] = 0;
				strcat(path, buf);
				rc = find_incl_file_near_fn(ctx, s, path, fp);
			}
		}
	}

	if (rc)
		rc = kbdfile_find(s, include_dirpath3, include_suffixes, fp);
	return rc;
}

static int
find_incl_file(struct lk_ctx *ctx, char *s, struct kbdfile *fp)
{
	char *ev;

	if (!s || !*s)
		return 1;

	if (*s == '/')		/* no path required */
		return (kbdfile_find(s, include_dirpath0, include_suffixes, fp));

	if ((ev = getenv("LOADKEYS_INCLUDE_PATH")) != NULL) {
		/* try user-specified path */
		const char *user_dir[2] = { NULL, NULL };
		while (ev) {
			int rc;
			char *t = strchr(ev, ':');
			char sv = 0;
			if (t) {
				sv = *t;
				*t = 0;
			}
			user_dir[0] = ev;
			if (*ev)
				rc = kbdfile_find(s, user_dir, include_suffixes, fp);
			else	/* empty string denotes system path */
				rc = find_standard_incl_file(ctx, s, fp);

			if (rc <= 0)
				return rc;
			if (t)
				*t++ = sv;
			ev = t;
		}
		return 1;
	}
	return find_standard_incl_file(ctx, s, fp);
}

static int
open_include(struct lk_ctx *ctx, char *s, yyscan_t scanner)
{
	int rc;
	struct kbdfile *fp;

	INFO(ctx, _("switching to %s"), s);

	fp = kbdfile_new(ctx->kbdfile_ctx);
	if (!fp) {
		ERR(ctx, _("out of memory"));
		return -1;
	}

	rc = find_incl_file(ctx, s, fp);
	if (rc > 0) {
		ERR(ctx, _("cannot open include file %s"), s);
		free(s);
		return -1;
	} else if (rc == -1) {
		free(s);
		return -1;
	}

	free(s);

	return stack_push(ctx, fp, scanner);
}

static int
parse_int(struct lk_ctx *ctx, char *text, char *value, int base, int *res)
{
	long v;

	errno = 0;
	v = strtol(value, NULL, base);

	if (errno) {
		ERR(ctx, _("unable to parse number: %s"), text);
		return -1;
	}

	if (v < 0) {
		ERR(ctx, _("value must be a positive number: %s"), text);
		return -1;
	}

	if (v > INT_MAX) {
		ERR(ctx, _("value must be less than %d: %s"), INT_MAX, text);
		return -1;
	}

	*res = (int) v;

	return 0;
}

%}
%s RVALUE
%x STR
%x INCLSTR
Comment			#|!
Continuation		\\\n
Eol			\n
Blank			[ \t]
Include			include[ \t]*
Decimal			[1-9][0-9]*
Octal			0[0-7]*
Hex			0[xX][0-9a-fA-F]+
Unicode			U\+([0-9a-fA-F]){4,6}
Literal			[a-zA-Z][a-zA-Z_0-9]*
Octa			([0-7]){1,3}
Charset			charset|Charset|CharSet|CHARSET
Keymaps			keymaps|Keymaps|KeyMaps|KEYMAPS
Keycode			keycode|Keycode|KeyCode|KEYCODE
String			string|String|STRING
Equals			=
Plain			plain|Plain|PLAIN
Shift			shift|Shift|SHIFT
Control			control|Control|CONTROL
Alt			alt|Alt|ALT
AltGr			altgr|Altgr|AltGr|ALTGR
ShiftL			shiftl|ShiftL|SHIFTL
ShiftR			shiftr|ShiftR|SHIFTR
CtrlL			ctrll|CtrlL|CTRLL
CtrlR			ctrlr|CtrlR|CTRLR
CapsShift		capsshift|Capsshift|CapsShift|CAPSSHIFT
AltIsMeta		[aA][lL][tT][-_][iI][sS][-_][mM][eE][tT][aA]
Strings			strings|Strings|STRINGS
Compose			compose|Compose|COMPOSE
As			as|As|AS
Usual			usual|Usual|USUAL
For			for|For|FOR
On			on|On|ON
To                      to|To|TO

%%

{Include}		{
				yy_push_state(INCLSTR, yyscanner);
			}
<INCLSTR>\"[^\"\n]+\"	{
				char *s = strndup(yytext+1, strlen(yytext)-2);
				if (s == NULL) {
					ERR(yyextra, _("out of memory"));
					return(ERROR);
				}

				if (open_include(yyextra, s, yyscanner) == -1)
					return(ERROR);

				while (((struct yyguts_t*)yyscanner)->yy_start_stack_ptr) {
					yy_pop_state(yyscanner);
				}
			}
<INCLSTR>[^"]|\"\"|\"[^"\n]*{Eol}	{
				ERR(yyextra, _("expected filename between quotes"));
				return(ERROR);
			}
<<EOF>>			{
				stack_pop(yyextra, yyscanner);
				if (!YY_CURRENT_BUFFER)
					yyterminate();
			}
{Continuation}		{
				yyset_lineno(yyget_lineno(yyscanner) + 1, yyscanner);
			}
{Eol}			{
				yyset_lineno(yyget_lineno(yyscanner) + 1, yyscanner);

				while (((struct yyguts_t*)yyscanner)->yy_start_stack_ptr) {
					yy_pop_state(yyscanner);
				}
				return(EOL);
			}
{Blank}+		; /* do nothing */
{Comment}.*/{Eol}	; /* do nothing */
{Equals}		{
				yy_push_state(RVALUE, yyscanner);
				lk_array_empty(yyextra->key_line);
				return(EQUALS);
			}
{String}		{
				yy_push_state(RVALUE, yyscanner);
				return(STRING);
			}
{To}			{
				yy_push_state(RVALUE, yyscanner);
				return(TO);
			}
{Unicode}		{
				if (parse_int(yyextra, yytext, yytext + 1, 16, &(yylval->num)) < 0)
					return(ERROR);

				if (yylval->num >= 0xf000) {
					ERR(yyextra, _("unicode keysym out of range: %s"),
						yytext);
					return(ERROR);
				}

				return(UNUMBER);
			}
{Decimal}|{Octal}|{Hex}	{
				if (parse_int(yyextra, yytext, yytext, 0, &(yylval->num)) < 0)
					return(ERROR);

				return(NUMBER);
			}
<RVALUE>{Literal}	{	return((yylval->num = ksymtocode(yyextra, yytext, TO_AUTO)) == -1 ? ERROR : LITERAL);	}
\-			{	return(DASH);		}
\,			{	return(COMMA);		}
\+			{	return(PLUS);		}
{Charset}		{	return(CHARSET);	}
{Keymaps}		{	return(KEYMAPS);	}
{Keycode}		{	return(KEYCODE);	}
{Plain}			{	return(PLAIN);		}
{Shift}			{	return(SHIFT);		}
{Control}		{	return(CONTROL);	}
{Alt}			{	return(ALT);		}
{AltGr}			{	return(ALTGR);		}
{ShiftL}		{	return(SHIFTL);		}
{ShiftR}		{	return(SHIFTR);		}
{CtrlL}			{	return(CTRLL);		}
{CtrlR}			{	return(CTRLR);		}
{CapsShift}		{	return(CAPSSHIFT);	}
{AltIsMeta}		{	return(ALT_IS_META);	}
{Strings}		{	return(STRINGS);	}
{Compose}		{	return(COMPOSE);	}
{As}			{	return(AS);		}
{Usual}			{	return(USUAL);		}
{On}			{	return(ON);		}
{For}			{	return(FOR);		}
'\\{Octa}'              {
				if (parse_int(yyextra, yytext, yytext + 2, 8, &(yylval->num)) < 0)
					return(ERROR);

				return(CCHAR);
			}
'\\.'                   {
				yylval->num = (unsigned char) yytext[2];
				return(CCHAR);
			}
'.'                     {
				yylval->num = (unsigned char) yytext[1];
				return(CCHAR);
			}
\"			{
				yylval->str.data[0] = '\0';
				yylval->str.len = 0;

				yy_push_state(STR, yyscanner);
			}
<STR>\\{Octa}		{
				long int i;
				if (yylval->str.len == MAX_PARSER_STRING) {
					ERR(yyextra, _("string too long"));
					return(ERROR);
				}

				i = strtol(yytext + 1, NULL, 8);

				if (i == LONG_MIN || i == LONG_MAX) {
					char buf[200];

					strerror_r(errno, buf, sizeof(buf));

					ERR(yyextra, "%s", buf);
					return(ERROR);
				}

				if (i > UCHAR_MAX) {
					ERR(yyextra, _("octal number too big"));
					return(ERROR);
				}

				yylval->str.data[yylval->str.len++] = (unsigned char) i;
			}
<STR>\\\"               {
				if (yylval->str.len == MAX_PARSER_STRING) {
					ERR(yyextra, _("string too long"));
					return(ERROR);
				}
				yylval->str.data[yylval->str.len++] = '"';
			}
<STR>\\\\               {
				if (yylval->str.len == MAX_PARSER_STRING) {
					ERR(yyextra, _("string too long"));
					return(ERROR);
				}
				yylval->str.data[yylval->str.len++] = '\\';
			}
<STR>\\n		{
				if (yylval->str.len == MAX_PARSER_STRING) {
					ERR(yyextra, _("string too long"));
					return(ERROR);
				}
				yylval->str.data[yylval->str.len++] = '\n';
			}
<STR>[^\"\\]*		{
				size_t len = strlen(yytext);

				if (yylval->str.len + len >= MAX_PARSER_STRING) {
					ERR(yyextra, _("string too long"));
					return(ERROR);
				}

				strcpy((char *) yylval->str.data + yylval->str.len, yytext);
				yylval->str.len += len;
			}
<STR>\"			{
				yylval->str.data[yylval->str.len] = '\0';
				while (((struct yyguts_t*)yyscanner)->yy_start_stack_ptr) {
					yy_pop_state(yyscanner);
				}
				return(STRLITERAL);
			}
.			{
				return(ERROR);
			}
%%
