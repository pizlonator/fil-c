#ifndef KBDFILE_CONTEXTP_H
#define KBDFILE_CONTEXTP_H

#include <stdarg.h>

#include "libcommon.h"
#include "kbdfile.h"

/**
 * @brief Opaque object representing the library context.
 */
struct kbdfile_ctx {
	/**
	 * User defined logging function.
	 */
	kbdfile_logger_t log_fn;
	/**
	 * The data passed to the @ref log_fn logging function as the first argument.
	 */
	void *log_data;

	/**
	 * Logging priority used by @ref log_fn logging function.
	 */
	int log_priority;
};

struct kbdfile {
	struct kbdfile_ctx *ctx;
	int flags;
	FILE *fd;
	char pathname[MAXPATHLEN];
};

#define KBDFILE_CTX_INITIALIZED 0x01
#define KBDFILE_PIPE            0x02

#define kbdfile_log_cond(ctx, level, arg...)                                          \
	do {                                                                     \
		if (ctx->log_priority >= level)                                  \
			kbdfile_log(ctx, level, __FILE__, __LINE__, __func__, ##arg); \
	} while (0)

/**
 * Wrapper to output debug-level messages
 * @param ctx is a kbdfile library context.
 * @param arg is output message.
 */
#define DBG(ctx, arg...) kbdfile_log_cond(ctx, LOG_DEBUG, ##arg)

/**
 * Wrapper to output informational messages
 * @param ctx is a kbdfile library context.
 * @param arg is output message.
 */
#define INFO(ctx, arg...) kbdfile_log_cond(ctx, LOG_INFO, ##arg)

/**
 * Wrapper to output warning conditions
 * @param ctx is a kbdfile library context.
 * @param arg is output message.
 */
#define WARN(ctx, arg...) kbdfile_log_cond(ctx, LOG_WARNING, ##arg)

/**
 * Wrapper to output error conditions
 * @param ctx is a kbdfile library context.
 * @param arg is output message.
 */
#define ERR(ctx, arg...) kbdfile_log_cond(ctx, LOG_ERR, ##arg)

#endif /* KBDFILE_CONTEXTP_H */
