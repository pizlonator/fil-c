/**
 * @file common.h
 * @brief Functions for initialization and release of resources as well
 * as functions to handle parameters.
 */
#ifndef LK_COMMON_H
#define LK_COMMON_H

#include <stdarg.h>

#include <keymap/context.h>
#include <keymap/logging.h>

/** Initializes the structures necessary to read and/or parse keymap.
 *
 * @return a pointer to keymap library context or NULL.
 */
struct lk_ctx *lk_init(void);

/** Free keymap resources.
 * @param ctx is a keymap library context.
 *
 * @return 0 on success, -1 on error
 */
int lk_free(struct lk_ctx *ctx);

/** Get the parser flags.
 * @param ctx is a keymap library context.
 *
 * @return the current parser flags.
 */
lk_flags lk_get_parser_flags(struct lk_ctx *ctx);

/** Set the parser flags.
 * @param ctx is a keymap library context.
 * @param flags the new value of the flags.
 *
 * @return 0 on success, -1 on error.
 */
int lk_set_parser_flags(struct lk_ctx *ctx, lk_flags flags);

/** Get the current logging priority.
 * @param ctx is a keymap library context.
 *
 * @return the current logging priority or -1 on error.
 */
int lk_get_log_priority(struct lk_ctx *ctx);

/** Set the current logging priority.
 * The value controls which messages get logged.
 * @param ctx is a keymap library context.
 *
 * @return the current logging priority.
 */
int lk_set_log_priority(struct lk_ctx *ctx, int priority);

/** The built-in logging writes to stderr. It can be
 * overridden by a custom function to plug log messages
 * into the user's logging functionality.
 * @param ctx keymap library context
 * @param log_fn function to be called for logging messages
 * @param data data to pass to log function
 *
 * @return 0 on success, -1 on error.
 */
int lk_set_log_fn(struct lk_ctx *ctx, lk_logger_t log_fn, const void *data);

#endif /* LK_COMMON_H */
