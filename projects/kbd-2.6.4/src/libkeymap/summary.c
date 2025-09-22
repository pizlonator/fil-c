/* summary.c
 *
 * This file is part of kbd project.
 * Copyright (C) 2012-2013  Alexey Gladkov <gladkov.alexey@gmail.com>
 *
 * This file is covered by the GNU General Public License,
 * which should be included with kbd as the file COPYING.
 */
#include "config.h"
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "keymap.h"

#include "libcommon.h"
#include "contextP.h"
#include "ksyms.h"
#include "modifiers.h"

static char
valid_type(int fd, int t)
{
	struct kbentry ke;

	ke.kb_index = 0;
	ke.kb_table = 0;
	ke.kb_value = (unsigned short) K(t, 0);

	return (ioctl(fd, KDSKBENT, (unsigned long)&ke) == 0);
}

static unsigned char
maximum_val(int fd, int t)
{
	struct kbentry ke, ke0;
	int i;

	ke.kb_index = 0;
	ke.kb_table = 0;
	ke.kb_value = K_HOLE;
	ke0         = ke;

	ioctl(fd, KDGKBENT, (unsigned long)&ke0);

	for (i = 0; i < 256; i++) {
		ke.kb_value = (unsigned short) K(t, i);
		if (ioctl(fd, KDSKBENT, (unsigned long)&ke))
			break;
	}
	ke.kb_value = K_HOLE;
	ioctl(fd, KDSKBENT, (unsigned long)&ke0);

	return (unsigned char) (i - 1);
}

int lk_get_kmapinfo(struct lk_ctx *ctx, struct kmapinfo *res)
{
	int i;

	res->flags     = ctx->flags;
	res->keywords  = ctx->keywords;
	res->keymaps   = ctx->keymap->count;
	res->functions = ctx->func_table->count;
	res->composes  = ctx->accent_table->count;

	res->keymaps_total   = ctx->keymap->total;
	res->functions_total = ctx->func_table->total;
	res->composes_total  = ctx->accent_table->total;

	res->keymaps_alloced = 0;

	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		if (lk_map_exists(ctx, i) && lk_get_key(ctx, i, 0) == K_ALLOCATED) {
			res->keymaps_alloced++;
		}
	}

	return 0;
}

#define NR_TYPES 15

void lk_dump_summary(struct lk_ctx *ctx, FILE *fd, int console)
{
	int i;
	struct kmapinfo info;

	if (lk_get_kmapinfo(ctx, &info) < 0)
		return;

	fprintf(fd, _("keycode range supported by kernel:           1 - %d\n"),
	        NR_KEYS - 1);
	fprintf(fd, _("max number of actions bindable to a key:         %d\n"),
	        MAX_NR_KEYMAPS);
	fprintf(fd, _("number of keymaps in actual use:                 %u\n"),
	        (unsigned int)info.keymaps);

	fprintf(fd, _("of which %u dynamically allocated\n"),
	        (unsigned int)info.keymaps_alloced);

	fprintf(fd, _("ranges of action codes supported by kernel:\n"));

	for (i = 0; i < NR_TYPES && valid_type(console, i); i++)
		fprintf(fd, "	0x%04x - 0x%04x\n",
		        K(i, 0), K(i, maximum_val(console, i)));

	fprintf(fd, _("number of function keys supported by kernel: %d\n"),
	        MAX_NR_FUNC);
	fprintf(fd, _("max nr of compose definitions: %d\n"),
	        MAX_DIACR);
	fprintf(fd, _("nr of compose definitions in actual use: %u\n"),
	        (unsigned int)info.composes);
}

void lk_dump_symbols(struct lk_ctx *ctx, FILE *fd)
{
	int t, v;
	modifier_t *mod;
	const char *p;

	for (t = 0; t < syms_size; t++) {
		if (get_sym_size(ctx, t)) {
			for (v = 0; v < get_sym_size(ctx, t); v++) {
				if ((p = get_sym(ctx, t, v))[0])
					fprintf(fd, "0x%04x\t%s\n", K(t, v), p);
			}
		} else if (t == KT_META) {
			for (v = 0; v < get_sym_size(ctx, KT_LATIN) && v < 128; v++) {
				if ((p = get_sym(ctx, KT_LATIN, v))[0])
					fprintf(fd, "0x%04x\tMeta_%s\n", K(t, v), p);
			}
		}
	}

	fprintf(fd, _("\nThe following synonyms are recognized:\n\n"));

	for (t = 0; t < syn_size; t++) {
		fprintf(fd, _("%-15s for %s\n"),
		        synonyms[t].synonym, synonyms[t].official_name);
	}

	fprintf(fd, _("\nRecognized modifier names and their column numbers:\n"));

	mod = (modifier_t *)modifiers;
	while (mod->name) {
		fprintf(fd, "%s\t\t%3d\n", mod->name, 1 << mod->bit);
		mod++;
	}
}
