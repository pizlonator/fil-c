#include "config.h"

#include <linux/keyboard.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "keymap.h"

#include "contextP.h"
#include "ksyms.h"
#include "libcommon.h"

#include "syms.cp1250.h"
#include "syms.ethiopic.h"
#include "syms.iso8859_15.h"
#include "syms.iso8859_5.h"
#include "syms.iso8859_7.h"
#include "syms.iso8859_8.h"
#include "syms.iso8859_9.h"
#include "syms.koi8.h"
#include "syms.latin1.h"
#include "syms.latin2.h"
#include "syms.latin3.h"
#include "syms.latin4.h"
#include "syms.mazovia.h"
#include "syms.sami.h"
#include "syms.thai.h"

#include "syms.synonyms.h"

#include "syms.ktyp.h"

#define E(x)                                \
	{                                   \
		x, sizeof(x) / sizeof(x[0]) \
	}

const syms_entry syms[] = {
	E(iso646_syms), /* KT_LATIN */
	E(fn_syms),     /* KT_FN */
	E(spec_syms),   /* KT_SPEC */
	E(pad_syms),    /* KT_PAD */
	E(dead_syms),   /* KT_DEAD */
	E(cons_syms),   /* KT_CONS */
	E(cur_syms),    /* KT_CUR */
	E(shift_syms),  /* KT_SHIFT */
	{ 0, 0 },       /* KT_META */
	E(ascii_syms),  /* KT_ASCII */
	E(lock_syms),   /* KT_LOCK */
	{ 0, 0 },       /* KT_LETTER */
	E(sticky_syms), /* KT_SLOCK */
	{ 0, 0 },       /*  */
	E(brl_syms)     /* KT_BRL */
};

#undef E

const int syms_size = sizeof(syms) / sizeof(syms_entry);
const int syn_size  = sizeof(synonyms) / sizeof(synonyms[0]);

const struct cs {
	const char *charset;
	const sym *charnames;
	const unsigned short start;
} charsets[] = {
	{ "iso-8859-1", latin1_syms, 160 },
	{ "iso-8859-2", latin2_syms, 160 },
	{ "iso-8859-3", latin3_syms, 160 },
	{ "iso-8859-4", latin4_syms, 160 },
	{ "iso-8859-5", iso8859_5_syms, 160 },
	{ "iso-8859-7", iso8859_7_syms, 160 },
	{ "iso-8859-8", iso8859_8_syms, 160 },
	{ "iso-8859-9", iso8859_9_syms, 160 },
	{ "iso-8859-10", latin6_syms, 160 },
	{ "iso-8859-15", iso8859_15_syms, 160 },
	{ "mazovia", mazovia_syms, 128 },
	{ "cp-1250", cp1250_syms, 128 },
	{ "koi8-r", koi8_syms, 128 },
	{ "koi8-u", koi8_syms, 128 },
	{ "tis-620", tis_620_syms, 160 },          /* thai */
	{ "iso-10646-18", iso10646_18_syms, 159 }, /* ethiopic */
	{ "iso-ir-197", iso_ir_197_syms, 160 },    /* sami */
	{ "iso-ir-209", iso_ir_209_syms, 160 },    /* sami */
};

static const unsigned int charsets_size = sizeof(charsets) / sizeof(charsets[0]);

/* Functions for both dumpkeys and loadkeys. */

void lk_list_charsets(FILE *f)
{
	size_t lth;
	int ct;
	unsigned int i, j;
	const char *mm[] = { "iso-8859-", "koi8-" };

	for (j = 0; j < sizeof(mm) / sizeof(mm[0]); j++) {
		if (j)
			fprintf(f, ",");
		fprintf(f, "%s{", mm[j]);
		ct  = 0;
		lth = strlen(mm[j]);
		for (i = 1; i < charsets_size; i++) {
			if (!strncmp(charsets[i].charset, mm[j], lth)) {
				if (ct++)
					fprintf(f, ",");
				fprintf(f, "%s", charsets[i].charset + lth);
			}
		}
		fprintf(f, "}");
	}
	for (i = 0; i < charsets_size; i++) {
		for (j = 0; j < sizeof(mm) / sizeof(mm[0]); j++) {
			lth = strlen(mm[j]);
			if (!strncmp(charsets[i].charset, mm[j], lth))
				goto nxti;
		}
		fprintf(f, ",%s", charsets[i].charset);
	nxti:;
	}
	fprintf(f, "\n");
}

const char *
lk_get_charset(struct lk_ctx *ctx)
{
	if (!ctx || ctx->charset >= charsets_size)
		return NULL;

	return charsets[ctx->charset].charset;
}

int lk_set_charset(struct lk_ctx *ctx, const char *charset)
{
	unsigned short i;

	for (i = 0; i < charsets_size; i++) {
		if (!strcasecmp(charsets[i].charset, charset)) {
			ctx->charset = i;
			return 0;
		}
	}
	return 1;
}

int
get_sym_size(struct lk_ctx *ctx, int ktype)
{
	if (ktype >= syms_size) {
		ERR(ctx, _("unable to get symbol by wrong type: %d"), ktype);
		return 0;
	}

	return syms[ktype].size;
}

const char *
get_sym(struct lk_ctx *ctx, int ktype, int index)
{
	if (!get_sym_size(ctx, ktype))
		return NULL;

	if (index >= syms[ktype].size) {
		ERR(ctx, _("unable to get symbol of %d type by wrong index: %d"), ktype, index);
		return NULL;
	}

	return syms[ktype].table[index];
}

char *
lk_get_sym(struct lk_ctx *ctx, int ktype, int index)
{
	const char *ksym = get_sym(ctx, ktype, index);
	return (ksym ? strdup(ksym) : NULL);
}

const char *
codetoksym(struct lk_ctx *ctx, int code)
{
	unsigned int i;
	int j;
	sym *p;

	if (code < 0)
		return NULL;

	if (code < 0x1000) { /* "traditional" keysym */
		if (code < 0x80)
			return get_sym(ctx, KT_LATIN, code);

		if (KTYP(code) == KT_META)
			return NULL;

		if (KTYP(code) == KT_LETTER)
			code = K(KT_LATIN, KVAL(code));

		if (KTYP(code) > KT_LATIN)
			return get_sym(ctx, KTYP(code), KVAL(code));

		i = ctx->charset;
		p = (sym *)charsets[i].charnames;
		if (p && (KVAL(code) >= charsets[i].start)) {
			p += KVAL(code) - charsets[i].start;
			if (p->name[0])
				return p->name;
		}
	}

	else { /* Unicode keysym */
		code ^= 0xf000;

		if (code < 0x80)
			return get_sym(ctx, KT_LATIN, code);

		for (i = 0; i < charsets_size; i++) {
			p = (sym *)charsets[i].charnames;
			if (p) {
				for (j = charsets[i].start; j < 256; j++, p++) {
					if (p->uni == code && p->name[0])
						return p->name;
				}
			}
		}
	}

	return NULL;
}

char *
lk_code_to_ksym(struct lk_ctx *ctx, int code)
{
	const char *s;

	s = codetoksym(ctx, code);
	if (!s)
		return NULL;

	return strdup(s);
}

/* Functions for loadkeys. */

static int
kt_latin(struct lk_ctx *ctx, const char *s, int direction)
{
	unsigned short i, max;

	sym *p = (sym *)charsets[ctx->charset].charnames;

	max = (direction == TO_UNICODE ? 128 : 256); // TODO(dmage): is 256 valid for ethiopic charset?

	for (i = charsets[ctx->charset].start; i < max; i++, p++) {
		if (p->name[0] && !strcmp(s, p->name))
			return K(KT_LATIN, i);
	}

	max = (direction == TO_UNICODE ? 128 : syms[KT_LATIN].size);

	for (i = 0; i < max; i++) {
		if (!strcmp(s, get_sym(ctx, KT_LATIN, i)))
			return K(KT_LATIN, i);
	}

	return -1;
}

int ksymtocode(struct lk_ctx *ctx, const char *s, int direction)
{
	unsigned short i, j;
	int n;
	int keycode;
	sym *p;

	if (direction == TO_AUTO)
		direction = (ctx->flags & LK_FLAG_PREFER_UNICODE)
		                ? TO_UNICODE
		                : TO_8BIT;

	if (!strncmp(s, "Meta_", 5)) {
		keycode = ksymtocode(ctx, s + 5, TO_8BIT);
		if (KTYP(keycode) == KT_LATIN)
			return K(KT_META, KVAL(keycode));

		/* Avoid error messages for Meta_acute with UTF-8 */
		else if (direction == TO_UNICODE)
			return (0);

		/* fall through to error printf */
	}

	if ((n = kt_latin(ctx, s, direction)) >= 0) {
		return n;
	}

	for (i = 1; i < syms_size; i++) {
		for (j = 0; j < syms[i].size; j++) {
			const char *ksym = get_sym(ctx, i, j);
			if (ksym && !strcmp(s, ksym))
				return K(i, j);
		}
	}

	for (i = 0; i < syn_size; i++)
		if (!strcmp(s, synonyms[i].synonym))
			return ksymtocode(ctx, synonyms[i].official_name, direction);

	if (direction == TO_UNICODE) {
		i = ctx->charset;
		p = (sym *)charsets[i].charnames;
		if (p) {
			for (j = charsets[i].start; j < 256; j++, p++) {
				if (!strcmp(s, p->name))
					return (p->uni ^ 0xf000);
			}
		}

		/* not found in the current charset, maybe we'll have good luck in others? */
		for (i = 0; i < charsets_size; i++) {
			if (i == ctx->charset) {
				continue;
			}
			p = (sym *)charsets[i].charnames;
			if (p) {
				for (j = charsets[i].start; j < 256; j++, p++) {
					if (!strcmp(s, p->name))
						return (p->uni ^ 0xf000);
				}
			}
		}
	} else /* if (!chosen_charset[0]) */ {
		/* note: some keymaps use latin1 but with euro,
		   so set_charset() would fail */
		/* note: some keymaps with charset line still use
		   symbols from more than one character set,
		   so we cannot have the  `if (!chosen_charset[0])'  here */

		for (i = 0; i < 256 - 160; i++)
			if (!strcmp(s, latin1_syms[i].name)) {
				INFO(ctx, _("assuming %s %s"), "iso-8859-1", s);
				return K(KT_LATIN, 160 + i);
			}

		for (i = 0; i < 256 - 160; i++)
			if (!strcmp(s, iso8859_15_syms[i].name)) {
				INFO(ctx, _("assuming %s %s"), "iso-8859-15", s);
				return K(KT_LATIN, 160 + i);
			}

		for (i = 0; i < 256 - 160; i++)
			if (!strcmp(s, latin2_syms[i].name)) {
				INFO(ctx, _("assuming %s %s"), "iso-8859-2", s);
				return K(KT_LATIN, 160 + i);
			}

		for (i = 0; i < 256 - 160; i++)
			if (!strcmp(s, latin3_syms[i].name)) {
				INFO(ctx, _("assuming %s %s"), "iso-8859-3", s);
				return K(KT_LATIN, 160 + i);
			}

		for (i = 0; i < 256 - 160; i++)
			if (!strcmp(s, latin4_syms[i].name)) {
				INFO(ctx, _("assuming %s %s"), "iso-8859-4", s);
				return K(KT_LATIN, 160 + i);
			}
	}

	ERR(ctx, _("unknown keysym '%s'"), s);

	return CODE_FOR_UNKNOWN_KSYM;
}

int lk_ksym_to_unicode(struct lk_ctx *ctx, const char *s)
{
	return ksymtocode(ctx, s, TO_UNICODE);
}

int convert_code(struct lk_ctx *ctx, int code, int direction)
{
	const char *ksym;
	int unicode_forced   = (direction == TO_UNICODE);
	int input_is_unicode = (code >= 0x1000);
	int result;

	if (direction == TO_AUTO)
		direction = (ctx->flags & LK_FLAG_PREFER_UNICODE)
		                ? TO_UNICODE
		                : TO_8BIT;

	if (KTYP(code) == KT_META)
		return code;
	else if (!input_is_unicode && code < 0x80)
		/* basic ASCII is fine in every situation */
		return code;
	else if (input_is_unicode && (code ^ 0xf000) < 0x80)
		/* so is Unicode "Basic Latin" */
		return code ^ 0xf000;
	else if ((input_is_unicode && direction == TO_UNICODE) ||
	         (!input_is_unicode && direction == TO_8BIT))
		/* no conversion necessary */
		result = code;
	else {
		/* depending on direction, this will give us either an 8-bit
		 * K(KTYP, KVAL) or a Unicode keysym xor 0xf000 */
		ksym = codetoksym(ctx, code);
		if (ksym)
			result = ksymtocode(ctx, ksym, direction);
		else
			result = code;
		if (direction == TO_UNICODE && KTYP(code) == KT_LETTER && (result ^ 0xf000) < 0x100) {
			/* Unicode Latin-1 Supplement */
			result = K(KT_LETTER, result ^ 0xf000);
		}
	}

	/* if direction was TO_UNICODE from the beginning, we return the true
	 * Unicode value (without the 0xf000 mask) */
	if (unicode_forced && result >= 0x1000)
		return result ^ 0xf000;
	else
		return result;
}

int add_capslock(struct lk_ctx *ctx, int code)
{
	if (KTYP(code) == KT_LATIN && (!(ctx->flags & LK_FLAG_PREFER_UNICODE) || code < 0x80))
		return K(KT_LETTER, KVAL(code));
	else if ((code ^ 0xf000) < 0x100)
		/* Unicode Latin-1 Supplement */
		/* a bit dirty to use KT_LETTER here, but it should work */
		return K(KT_LETTER, code ^ 0xf000);
	else
		return convert_code(ctx, code, TO_AUTO);
}
