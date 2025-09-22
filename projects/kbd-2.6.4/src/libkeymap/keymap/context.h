/**
 * @file context.h
 * @brief Header contains flags, keywords and context structure.
 */

#ifndef LK_CONTEXT_H
#define LK_CONTEXT_H

#include <linux/kd.h>
#include <linux/keyboard.h>
#include <keymap/array.h>

/**
 * @brief Parser flags that are set outside the library.
 */
typedef enum {
	LK_FLAG_UNICODE_MODE   = (1 << 1), /**< Unicode mode */
	LK_FLAG_CLEAR_COMPOSE  = (1 << 2), /**< Compose */
	LK_FLAG_CLEAR_STRINGS  = (1 << 3), /**< Strings */
	LK_FLAG_PREFER_UNICODE = (1 << 4)  /**< Prefer unicode */
} lk_flags;

/**
 * @brief Keywords used in keymap files.
 */
typedef enum {
	LK_KEYWORD_KEYMAPS    = (1 << 1), /**< 'Keymaps' keyword */
	LK_KEYWORD_ALTISMETA  = (1 << 2), /**< 'Alt-is-meta' keyword */
	LK_KEYWORD_CHARSET    = (1 << 3), /**< 'Charset' keyword */
	LK_KEYWORD_STRASUSUAL = (1 << 4)  /**< 'String as usual' keyword */
} lk_keywords;

/**
 * @brief Copy of struct kbdiacruc.
 */
struct lk_kbdiacr {
	unsigned int diacr, base, result;
};

/**
 * @brief Opaque object representing the library context.
 */
struct lk_ctx;

#endif /* LK_CONTEXT_H */
