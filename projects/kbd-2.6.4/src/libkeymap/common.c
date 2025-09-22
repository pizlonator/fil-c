#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "keymap.h"

#include "libcommon.h"
#include "contextP.h"

void
lk_log(struct lk_ctx *ctx, int priority,
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
	fprintf(fp, "libkeymap: %s %s:%d %s: ", priname, file, line, fn);
#endif
	vfprintf(fp, format, args);
	fprintf(fp, "\n");
}

#undef log_unused

int lk_set_log_fn(struct lk_ctx *ctx, lk_logger_t log_fn, const void *data)
{
	if (!ctx)
		return -1;

	ctx->log_fn   = log_fn;
	ctx->log_data = (void *)data;

	return 0;
}

int lk_get_log_priority(struct lk_ctx *ctx)
{
	if (!ctx)
		return -1;

	return ctx->log_priority;
}

int lk_set_log_priority(struct lk_ctx *ctx, int priority)
{
	if (!ctx)
		return -1;

	ctx->log_priority = priority;
	return 0;
}

lk_flags
lk_get_parser_flags(struct lk_ctx *ctx)
{
	if (!ctx)
		return -1;

	return ctx->flags;
}

int lk_set_parser_flags(struct lk_ctx *ctx, lk_flags flags)
{
	if (!ctx)
		return -1;

	ctx->flags = flags;
	return 0;
}

static int
init_array(struct lk_ctx *ctx, struct lk_array **arr, ssize_t size)
{
	int rc;
	void *ptr;

	ptr = malloc(sizeof(struct lk_array));
	if (!ptr) {
		ERR(ctx, _("out of memory"));
		return -1;
	}

	rc = lk_array_init(ptr, size, 0);
	if (rc < 0) {
		ERR(ctx, _("unable to initialize array: %s"), strerror(rc));
		free(ptr);
		return -1;
	}

	*arr = ptr;

	return 0;
}

struct lk_ctx *
lk_init(void)
{
	struct lk_ctx *ctx;

	ctx = malloc(sizeof(struct lk_ctx));
	if (!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct lk_ctx));

	lk_set_log_fn(ctx, log_file, stderr);
	lk_set_log_priority(ctx, LOG_ERR);

	if (init_array(ctx, &ctx->keymap, sizeof(void *)) < 0 ||
	    init_array(ctx, &ctx->func_table, sizeof(void *)) < 0 ||
	    init_array(ctx, &ctx->accent_table, sizeof(void *)) < 0 ||
	    init_array(ctx, &ctx->key_constant, sizeof(char)) < 0 ||
	    init_array(ctx, &ctx->key_line, sizeof(int)) < 0) {
		lk_free(ctx);
		return NULL;
	}

	ctx->kbdfile_ctx = kbdfile_context_new();

	if (ctx->kbdfile_ctx == NULL) {
		lk_free(ctx);
		return NULL;
	}

	return ctx;
}

int lk_free(struct lk_ctx *ctx)
{
	unsigned int i; //, j;

	if (!ctx)
		return -1;

	if (ctx->keymap) {
		for (i = 0; i < ctx->keymap->total; i++) {
			struct lk_array *map;

			map = lk_array_get_ptr(ctx->keymap, i);
			if (!map)
				continue;

			lk_array_free(map);
			free(map);
		}
		lk_array_free(ctx->keymap);
		free(ctx->keymap);

		ctx->keymap = NULL;
	}

	if (ctx->func_table) {
		for (i = 0; i < ctx->func_table->total; i++) {
			char *ptr;

			ptr = lk_array_get_ptr(ctx->func_table, i);
			if (!ptr)
				continue;

			free(ptr);
		}
		lk_array_free(ctx->func_table);
		free(ctx->func_table);

		ctx->func_table = NULL;
	}

	if (ctx->accent_table) {
		for (i = 0; i < ctx->accent_table->total; i++) {
			struct lk_array *ptr;

			ptr = lk_array_get_ptr(ctx->accent_table, i);
			if (!ptr)
				continue;

			free(ptr);
		}
		lk_array_free(ctx->accent_table);
		free(ctx->accent_table);

		ctx->accent_table = NULL;
	}

	if (ctx->key_constant) {
		lk_array_free(ctx->key_constant);
		free(ctx->key_constant);
		ctx->key_constant = NULL;
	}

	if (ctx->key_line) {
		lk_array_free(ctx->key_line);
		free(ctx->key_line);
		ctx->key_line = NULL;
	}

	if (ctx->kbdfile_ctx != NULL)
		ctx->kbdfile_ctx = kbdfile_context_free(ctx->kbdfile_ctx);

	free(ctx);

	return 0;
}
