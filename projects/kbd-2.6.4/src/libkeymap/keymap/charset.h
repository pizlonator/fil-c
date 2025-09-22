/**
 * @file charset.h
 * @brief Functions for charset manipulation.
 */
#ifndef LK_CHARSET_H
#define LK_CHARSET_H

#include <keymap/context.h>

/** Prints into the FILE a list of supported charsets.
 * @param fp is a stream.
 *
 * @return nothing.
 */
void lk_list_charsets(FILE *fp);

/** Returns the current name of the charset used by the library.
 * @param ctx is a keymap library context.
 *
 * @return pointer to null-terminated string (Do not pass this pointer to free(3)).
 */
const char *lk_get_charset(struct lk_ctx *ctx);

/** Sets the charset which will be used by the library.
 * @param ctx is a keymap library context.
 * @param name is a name of charset.
 *
 * @return zero if the charset was found and successfully changed. On error, 1 is returned.
 */
int lk_set_charset(struct lk_ctx *ctx, const char *name);

#endif /* LK_CHARSET_H */
