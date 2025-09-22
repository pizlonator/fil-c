/* diacr.c
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
#include "ksyms.h"

int lk_diacr_exists(struct lk_ctx *ctx, int index)
{
	return (lk_array_get_ptr(ctx->accent_table, index) != NULL);
}

int lk_get_diacr(struct lk_ctx *ctx, int index, struct lk_kbdiacr *dcr)
{
	struct lk_kbdiacr *ptr;

	ptr = lk_array_get_ptr(ctx->accent_table, index);
	if (!ptr) {
		ERR(ctx, _("Index %d in the accent table does not exist"), index);
		return -1;
	}

	dcr->diacr  = ptr->diacr;
	dcr->base   = ptr->base;
	dcr->result = ptr->result;

	return 0;
}

int lk_append_diacr(struct lk_ctx *ctx, struct lk_kbdiacr *dcr)
{
	struct lk_kbdiacr *ptr;

	ptr = malloc(sizeof(struct lk_kbdiacr));
	if (!ptr) {
		ERR(ctx, _("out of memory"));
		return -1;
	}

	ptr->diacr  = dcr->diacr;
	ptr->base   = dcr->base;
	ptr->result = dcr->result;

	lk_array_append(ctx->accent_table, &ptr);

	return 0;
}

int lk_add_diacr(struct lk_ctx *ctx, int index, struct lk_kbdiacr *dcr)
{
	struct lk_kbdiacr *ptr;

	ptr = malloc(sizeof(struct lk_kbdiacr));
	if (!ptr) {
		ERR(ctx, _("out of memory"));
		return -1;
	}

	ptr->diacr  = dcr->diacr;
	ptr->base   = dcr->base;
	ptr->result = dcr->result;

	lk_array_set(ctx->accent_table, index, &ptr);

	return 0;
}

int lk_del_diacr(struct lk_ctx *ctx, int index)
{
	int rc;
	rc = lk_array_unset(ctx->accent_table, index);
	if (rc) {
		ERR(ctx, _("Unable to remove item from the diacritical table"));
		return -1;
	}
	return 0;
}

int lk_append_compose(struct lk_ctx *ctx, struct lk_kbdiacr *dcr)
{
	struct lk_kbdiacr dcr0;
	int direction = TO_8BIT;

#ifdef KDSKBDIACRUC
	if (ctx->flags & LK_FLAG_PREFER_UNICODE)
		direction = TO_UNICODE;
#endif

	dcr0.diacr  = (unsigned int) convert_code(ctx, (int) dcr->diacr, direction);
	dcr0.base   = (unsigned int) convert_code(ctx, (int) dcr->base, direction);
	dcr0.result = (unsigned int) convert_code(ctx, (int) dcr->result, direction);

	return lk_append_diacr(ctx, &dcr0);
}
