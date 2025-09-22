#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "kbdfile.h"

#include "libcommon.h"
#include "contextP.h"

void
kbdfile_log(struct kbdfile_ctx *ctx, int priority,
       const char *file, int line, const char *fn,
       const char *fmt, ...)
{
	va_list args;
	if (ctx->log_fn == NULL)
		return;
	va_start(args, fmt);
	ctx->log_fn(ctx->log_data, priority, file, line, fn, fmt, args);
	va_end(args);
}

#ifndef DEBUG
#define log_unused __attribute__((unused))
#else
#define log_unused
#endif

static void __attribute__((format(printf, 6, 0)))
log_file(void *data,
         int priority log_unused,
         const char *file log_unused,
         const int line log_unused,
         const char *fn log_unused,
         const char *format, va_list args)
{
	FILE *fp = data;
#ifdef DEBUG
	char buf[16];
	const char *priname;

	switch (priority) {
		case LOG_EMERG:
			priname = "EMERGENCY";
			break;
		case LOG_ALERT:
			priname = "ALERT";
			break;
		case LOG_CRIT:
			priname = "CRITICAL";
			break;
		case LOG_ERR:
			priname = "ERROR";
			break;
		case LOG_WARNING:
			priname = "WARNING";
			break;
		case LOG_NOTICE:
			priname = "NOTICE";
			break;
		case LOG_INFO:
			priname = "INFO";
			break;
		case LOG_DEBUG:
			priname = "DEBUG";
			break;
		default:
			snprintf(buf, sizeof(buf), "L:%d", priority);
			priname = buf;
	}
	fprintf(fp, "libkbdfile: %s %s:%d %s: ", priname, file, line, fn);
#endif
	vfprintf(fp, format, args);
	fprintf(fp, "\n");
}

#undef log_unused

kbdfile_logger_t
kbdfile_get_log_fn(struct kbdfile_ctx *ctx)
{
	if (!ctx)
		return NULL;

	return ctx->log_fn;
}

int
kbdfile_set_log_fn(struct kbdfile_ctx *ctx, kbdfile_logger_t log_fn, const void *data)
{
	if (!ctx)
		return -1;

	ctx->log_fn   = log_fn;
	ctx->log_data = (void *)data;

	return 0;
}

void *
kbdfile_get_log_data(struct kbdfile_ctx *ctx)
{
	if (!ctx)
		return NULL;

	return ctx->log_data;
}

int
kbdfile_set_log_data(struct kbdfile_ctx *ctx, const void *data)
{
	if (!ctx)
		return -1;

	ctx->log_data = (void *)data;
	return 0;
}

int kbdfile_get_log_priority(struct kbdfile_ctx *ctx)
{
	if (!ctx)
		return -1;

	return ctx->log_priority;
}

int kbdfile_set_log_priority(struct kbdfile_ctx *ctx, int priority)
{
	if (!ctx)
		return -1;

	ctx->log_priority = priority;
	return 0;
}

struct kbdfile_ctx *
kbdfile_context_new(void)
{
	struct kbdfile_ctx *ctx;

	ctx = calloc(1, sizeof(struct kbdfile_ctx));
	if (!ctx)
		return NULL;

	kbdfile_set_log_fn(ctx, log_file, stderr);
	kbdfile_set_log_priority(ctx, LOG_ERR);

	return ctx;
}

void *
kbdfile_context_free(struct kbdfile_ctx *ctx)
{
	if (!ctx)
		return NULL;

	free(ctx);
	return NULL;
}
