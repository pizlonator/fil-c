#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "keymap.h"

#include "libcommon.h"
#include "contextP.h"
#include "ksyms.h"
#include "modifiers.h"

int lk_map_exists(struct lk_ctx *ctx, int k_table)
{
	return (lk_array_get_ptr(ctx->keymap, k_table) != NULL);
}

int lk_get_keys_total(struct lk_ctx *ctx, int k_table)
{
	struct lk_array *map;
	map = lk_array_get_ptr(ctx->keymap, k_table);
	if (!map) {
		return 0;
	}

	if (map->total >= NR_KEYS) {
		return -1;
	}

	return (int) map->total;
}

int lk_key_exists(struct lk_ctx *ctx, int k_table, int k_index)
{
	struct lk_array *map;
	unsigned int *key;

	map = lk_array_get_ptr(ctx->keymap, k_table);
	if (!map) {
		return 0;
	}

	key = lk_array_get(map, k_index);
	if (!key) {
		return 0;
	}

	return (*key > 0);
}

int lk_add_map(struct lk_ctx *ctx, int k_table)
{
	struct lk_array *keys;

	if (lk_map_exists(ctx, k_table)) {
		return 0;
	}

	keys = malloc(sizeof(struct lk_array));
	if (!keys) {
		ERR(ctx, _("out of memory"));
		return -1;
	}

	lk_array_init(keys, sizeof(unsigned int), 0);

	if (lk_array_set(ctx->keymap, k_table, &keys) < 0) {
		free(keys);
		ERR(ctx, _("out of memory"));
		return -1;
	}

	return 0;
}

int lk_get_key(struct lk_ctx *ctx, int k_table, int k_index)
{
	struct lk_array *map;
	int *key;

	map = lk_array_get_ptr(ctx->keymap, k_table);
	if (!map) {
		ERR(ctx, _("unable to get keymap %d"), k_table);
		return -1;
	}

	key = lk_array_get(map, k_index);
	if (!key || *key == 0) {
		return K_HOLE;
	}

	return (*key) - 1;
}

int lk_del_key(struct lk_ctx *ctx, int k_table, int k_index)
{
	struct lk_array *map;

	map = lk_array_get_ptr(ctx->keymap, k_table);
	if (!map) {
		ERR(ctx, _("unable to get keymap %d"), k_table);
		return -1;
	}

	if (!lk_array_exists(map, k_index))
		return 0;

	if (lk_array_unset(map, k_index) < 0) {
		ERR(ctx, _("unable to unset key %d for table %d"),
		    k_index, k_table);
		return -1;
	}

	return 0;
}

int lk_add_key(struct lk_ctx *ctx, int k_table, int k_index, int keycode)
{
	struct lk_array *map;
	int code = keycode + 1;

	if (keycode == CODE_FOR_UNKNOWN_KSYM) {
		/* is safer not to be silent in this case,
		 * it can be caused by coding errors as well. */
		ERR(ctx, _("lk_add_key called with bad keycode %d"), keycode);
		return -1;
	}

	map = lk_array_get_ptr(ctx->keymap, k_table);
	if (!map) {
		if (ctx->keywords & LK_KEYWORD_KEYMAPS) {
			ERR(ctx, _("adding map %d violates explicit keymaps line"),
			    k_table);
			return -1;
		}

		if (lk_add_map(ctx, k_table) < 0)
			return -1;
	}

	if ((ctx->keywords & LK_KEYWORD_ALTISMETA) && keycode == K_HOLE &&
	    lk_key_exists(ctx, k_table, k_index))
		return 0;

	map = lk_array_get_ptr(ctx->keymap, k_table);

	if (lk_array_set(map, k_index, &code) < 0) {
		ERR(ctx, _("unable to set key %d for table %d"),
		    k_index, k_table);
		return -1;
	}

	if (ctx->keywords & LK_KEYWORD_ALTISMETA) {
		int alttable = k_table | M_ALT;
		int type     = KTYP(keycode);
		int val      = KVAL(keycode);

		if (alttable != k_table && lk_map_exists(ctx, alttable) &&
		    !lk_key_exists(ctx, alttable, k_index) &&
		    (type == KT_LATIN || type == KT_LETTER) &&
		    val < 128) {
			if (lk_add_key(ctx, alttable, k_index, K(KT_META, val)) < 0)
				return -1;
		}
	}

	return 0;
}

static int
do_constant_key(struct lk_ctx *ctx, int i, int key)
{
	int j, typ, val;

	typ = KTYP(key);
	val = KVAL(key);

	if ((typ == KT_LATIN || typ == KT_LETTER) &&
	    ((val >= 'a' && val <= 'z') || (val >= 'A' && val <= 'Z'))) {
		int defs[16];

		defs[0] = K(KT_LETTER, val);
		defs[1] = K(KT_LETTER, val ^ 32);
		defs[2] = defs[0];
		defs[3] = defs[1];

		for (j = 4; j < 8; j++)
			defs[j] = K(KT_LATIN, val & ~96);

		for (j = 8; j < 16; j++)
			defs[j] = K(KT_META, KVAL(defs[j - 8]));

		for (j = 0; j < ctx->keymap->total; j++) {
			if (!lk_map_exists(ctx, j))
				continue;

			if (j > 0 && lk_key_exists(ctx, j, i))
				continue;

			if (lk_add_key(ctx, j, i, defs[j % 16]) < 0)
				return -1;
		}

	} else {
		/* do this also for keys like Escape,
		   as promised in the man page */
		for (j = 1; j < ctx->keymap->total; j++) {
			if (!lk_map_exists(ctx, j))
				continue;

			if (lk_key_exists(ctx, j, i))
				continue;

			if (lk_add_key(ctx, j, i, key) < 0)
				return -1;
		}
	}
	return 0;
}

int lk_add_constants(struct lk_ctx *ctx)
{
	int i, r0 = 0;

	if (ctx->keywords & LK_KEYWORD_KEYMAPS) {
		while (r0 < ctx->keymap->total && !lk_map_exists(ctx, r0))
			r0++;
	}

	for (i = 0; i < ctx->key_constant->total; i++) {
		char *constant;
		int key;

		constant = lk_array_get(ctx->key_constant, i);
		if (!constant || !(*constant))
			continue;

		if (!lk_map_exists(ctx, r0)) {
			ERR(ctx, _("impossible error in lk_add_constants"));
			return -1;
		}

		key = lk_get_key(ctx, r0, i);

		if (do_constant_key(ctx, i, key) < 0)
			return -1;
	}
	return 0;
}
