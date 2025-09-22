/**
 * @file kmap.h
 * @brief Functions for keymaps manipulation (add/delete keys).
 */
#ifndef LK_KMAP_H
#define LK_KMAP_H

#include <keymap/context.h>

#ifndef __GNUC__
#undef  __attribute__
#define __attribute__(x) /*NOTHING*/
#endif

int lk_add_map(struct lk_ctx *ctx, int k_table) __attribute__((nonnull(1)));
int lk_map_exists(struct lk_ctx *ctx, int k_table) __attribute__((nonnull(1)));

int lk_get_keys_total(struct lk_ctx *ctx, int k_table) __attribute__((nonnull(1)));

int lk_add_key(struct lk_ctx *ctx, int k_table, int k_index, int keycode) __attribute__((nonnull(1)));
int lk_del_key(struct lk_ctx *ctx, int k_table, int k_index) __attribute__((nonnull(1)));
int lk_get_key(struct lk_ctx *ctx, int k_table, int k_index) __attribute__((nonnull(1)));
int lk_key_exists(struct lk_ctx *ctx, int k_table, int k_index) __attribute__((nonnull(1)));

/* Functions for key string manipulations */
int lk_get_func(struct lk_ctx *ctx, struct kbsentry *kbs) __attribute__((nonnull(1, 2)));
int lk_add_func(struct lk_ctx *ctx, struct kbsentry *kbs) __attribute__((nonnull(1, 2)));
int lk_del_func(struct lk_ctx *ctx, int index) __attribute__((nonnull(1)));
int lk_func_exists(struct lk_ctx *ctx, int index) __attribute__((nonnull(1)));

/* Functions for manipulations with diacritical table */
int lk_get_diacr(struct lk_ctx *ctx, int index, struct lk_kbdiacr *dcr) __attribute__((nonnull(1, 3)));
int lk_add_diacr(struct lk_ctx *ctx, int index, struct lk_kbdiacr *dcr) __attribute__((nonnull(1, 3)));
int lk_del_diacr(struct lk_ctx *ctx, int index) __attribute__((nonnull(1)));
int lk_diacr_exists(struct lk_ctx *ctx, int index) __attribute__((nonnull(1)));
int lk_append_diacr(struct lk_ctx *ctx, struct lk_kbdiacr *dcr) __attribute__((nonnull(1, 2)));
int lk_append_compose(struct lk_ctx *ctx, struct lk_kbdiacr *dcr) __attribute__((nonnull(1, 2)));

int lk_add_constants(struct lk_ctx *ctx) __attribute__((nonnull(1)));

#include <kbdfile.h>

int lk_parse_keymap(struct lk_ctx *ctx, struct kbdfile *f) __attribute__((nonnull(1, 2)));
int lk_load_keymap(struct lk_ctx *ctx, int fd, int kbd_mode) __attribute__((nonnull(1)));

#endif /* LK_KMAP_H */
