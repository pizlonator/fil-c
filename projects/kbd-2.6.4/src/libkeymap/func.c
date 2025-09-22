/* func.c
 *
 * This file is part of kbd project.
 * Copyright (C) 2014  Alexey Gladkov <gladkov.alexey@gmail.com>
 *
 * This file is covered by the GNU General Public License,
 * which should be included with kbd as the file COPYING.
 */
#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "keymap.h"

#include "libcommon.h"
#include "contextP.h"

int lk_func_exists(struct lk_ctx *ctx, int index)
{
	return (lk_array_get_ptr(ctx->func_table, index) != NULL);
}

int lk_get_func(struct lk_ctx *ctx, struct kbsentry *kbs)
{
	char *s;

	s = lk_array_get_ptr(ctx->func_table, kbs->kb_func);
	if (!s) {
		ERR(ctx, _("func %d not allocated"), kbs->kb_func);
		return -1;
	}

	strncpy((char *)kbs->kb_string, s, sizeof(kbs->kb_string));
	kbs->kb_string[sizeof(kbs->kb_string) - 1] = 0;

	return 0;
}

int lk_add_func(struct lk_ctx *ctx, struct kbsentry *kbs)
{
	char *s;

	s = lk_array_get_ptr(ctx->func_table, kbs->kb_func);
	if (s)
		free(s);

	s = strdup((char *)kbs->kb_string);

	if (lk_array_set(ctx->func_table, kbs->kb_func, &s) < 0) {
		free(s);
		ERR(ctx, _("out of memory"));
		return -1;
	}

	return 0;
}

int lk_del_func(struct lk_ctx *ctx, int index)
{
	if (lk_array_unset(ctx->func_table, index) < 0) {
		ERR(ctx, _("Unable to remove item from the list of functions"));
		return -1;
	}

	return 0;
}
