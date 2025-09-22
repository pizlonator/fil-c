/**
 * @file dump.h
 * @brief Functions for keymap output.
 */
#ifndef LK_DUMP_H
#define LK_DUMP_H

#include <stdio.h>

#include <keymap/context.h>

#ifndef __GNUC__
#undef  __attribute__
#define __attribute__(x) /*NOTHING*/
#endif

/**
 * @brief Flags controlling the output keymap.
 */
typedef enum {
	LK_SHAPE_DEFAULT        = (1 << 1),
	LK_SHAPE_FULL_TABLE     = (1 << 2), /**< one line for each keycode */
	LK_SHAPE_SEPARATE_LINES = (1 << 3), /**< one line for each (modifier,keycode) pair */
	LK_SHAPE_UNTIL_HOLE     = (1 << 4)  /**< one line for each keycode until 1st hole */
} lk_table_shape;

/**
 * @brief General information about the keymap.
 */
struct kmapinfo {
	lk_flags flags;          /**< Parser flags that are set outside the library */
	lk_keywords keywords;    /**< Keywords used in keymap files */
	ssize_t keymaps;         /**< Number of keymaps in actual use */
	ssize_t keymaps_alloced; /**< Number of keymaps dynamically allocated */
	ssize_t functions;       /**< Number of function keys */
	ssize_t composes;        /**< Number of compose definitions in actual use */

	ssize_t keymaps_total;
	ssize_t functions_total;
	ssize_t composes_total;
};

/**
 * Outputs a keymap in binary format.
 * @param ctx is a keymap library context.
 * @param fd is a FILE pointer for output.
 *
 * @return 0 on success, -1 on error.
 */
int lk_dump_bkeymap(struct lk_ctx *ctx, FILE *fd) __attribute__((nonnull(1)));

/**
 * Outputs a keymap in C format.
 * @param ctx is a keymap library context.
 * @param fd is a FILE pointer for output.
 *
 * @return 0 on success, -1 on error.
 */
int lk_dump_ctable(struct lk_ctx *ctx, FILE *fd) __attribute__((nonnull(1, 2)));

/**
 * Outputs whole keymap. This is a high-level function that calls @ref lk_dump_keys,
 * @ref lk_dump_keymaps, @ref lk_dump_funcs and @ref lk_dump_diacs.
 * @param ctx is a keymap library context.
 * @param fd is a FILE pointer for output.
 * @param table specifies the output format of the keycode table.
 * @param numeric indicates whether to output the keycodes in numerical form.
 */
void lk_dump_keymap(struct lk_ctx *ctx, FILE *fd, lk_table_shape table, char numeric) __attribute__((nonnull(1, 2)));

/**
 * Outputs keycodes.
 * @param ctx is a keymap library context.
 * @param fd is a FILE pointer for output.
 * @param table specifies the output format of the keycode table.
 * @param numeric indicates whether to output the keycodes in numerical form.
 */
void lk_dump_keys(struct lk_ctx *ctx, FILE *fd, lk_table_shape table, char numeric) __attribute__((nonnull(1, 2)));

/**
 * Outputs 'keymaps' line.
 * @param ctx is a keymap library context.
 * @param fd is a FILE pointer for output.
 */
void lk_dump_keymaps(struct lk_ctx *ctx, FILE *fd) __attribute__((nonnull(1, 2)));

/**
 * Outputs function keys.
 * @param ctx is a keymap library context.
 * @param fd is a FILE pointer for output.
 */
void lk_dump_funcs(struct lk_ctx *ctx, FILE *fd) __attribute__((nonnull(1, 2)));

/**
 * Outputs accent table.
 * @param ctx is a keymap library context.
 * @param fd is a FILE pointer for output.
 */
void lk_dump_diacs(struct lk_ctx *ctx, FILE *fd) __attribute__((nonnull(1, 2)));

/**
 * Converts a number to a string representation of the character.
 * @param ctx is a keymap library context.
 * @param code is a numeric representation of ksym.
 *
 * @return a string representation of the code.
 */
char *lk_code_to_ksym(struct lk_ctx *ctx, int code) __attribute__((nonnull(1)));

char *lk_get_sym(struct lk_ctx *ctx, int ktype, int index) __attribute__((nonnull(1)));

/**
 * Converts a string to a numeric representation of the character.
 * @param ctx is a keymap library context.
 * @param code is a string representation of ksym.
 *
 * @return a unicode representation of the code.
 */
int lk_ksym_to_unicode(struct lk_ctx *ctx, const char *code) __attribute__((nonnull(1, 2)));

int lk_get_kmapinfo(struct lk_ctx *ctx, struct kmapinfo *res) __attribute__((nonnull(1, 2)));
void lk_dump_summary(struct lk_ctx *ctx, FILE *fd, int console) __attribute__((nonnull(1, 2)));
void lk_dump_symbols(struct lk_ctx *ctx, FILE *fd) __attribute__((nonnull(1, 2)));

#endif /* LK_DUMP_H */
