// SPDX-License-Identifier: LGPL-2.0-or-later
/*
 * Copyright (C) 2020 Alexey Gladkov <gladkov.alexey@gmail.com>
 */
#include <sysexits.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>

#include "paths.h"
#include "kfontP.h"

/* search for the map file in these directories (with trailing /) */
static const char *const mapdirpath[]  = {
	DATADIR "/" TRANSDIR "/",
	NULL
};

static const char *const mapsuffixes[] = {
	"",
	".trans",
	"_to_uni.trans",
	".acm",
	NULL
};

/* search for the font in these directories (with trailing /) */
static const char *const fontdirpath[]  = {
	DATADIR "/" FONTDIR "/",
	NULL
};
static char const *const fontsuffixes[] = {
	"",
	".psfu",
	".psf",
	".cp",
	".fnt",
	NULL
};

static const char *const unidirpath[]  = {
	DATADIR "/" UNIMAPDIR "/",
	NULL
};
static const char *const unisuffixes[] = {
	"",
	".uni",
	".sfm",
	NULL
};

/* hide partial fonts a bit - loading a single one is a bad idea */
const char *const partfontdirpath[]  = {
	DATADIR "/" FONTDIR "/" PARTIALDIR "/",
	NULL
};
char const *const partfontsuffixes[] = {
	"",
	NULL
};

void
kfont_set_option(struct kfont_context *ctx, enum kfont_option opt)
{
	ctx->options |= 1U << opt;
}

void
kfont_unset_option(struct kfont_context *ctx, enum kfont_option opt)
{
	ctx->options &= ~(1U << opt);
}

int
kfont_get_verbosity(struct kfont_context *ctx)
{
	return ctx->verbose;
}

void
kfont_inc_verbosity(struct kfont_context *ctx)
{
	ctx->verbose++;
}

void
kfont_set_logger(struct kfont_context *ctx, kfont_logger_t fn)
{
	ctx->log_fn = fn;
}

void
logger(struct kfont_context *ctx, int priority, const char *file,
		int line, const char *fn,
		const char *fmt, ...)
{
	va_list args;
	if (ctx->log_fn == NULL)
		return;
	switch(ctx->verbose) {
		case 0:
			if (priority > LOG_ERR)
				return;
			break;
		case 1:
			if (priority > LOG_INFO)
				return;
			break;
	}
	va_start(args, fmt);
	ctx->log_fn(ctx, priority, file, line, fn, fmt, args);
	va_end(args);
}


void
log_stderr(struct kfont_context *ctx, int priority, const char *file,
		const int line, const char *fn,
		const char *format, va_list args)
{
	char buf[16];
	const char *priname;

	switch (priority) {
		case LOG_EMERG:   priname = "EMERGENCY"; break;
		case LOG_ALERT:   priname = "ALERT";     break;
		case LOG_CRIT:    priname = "CRITICAL";  break;
		case LOG_ERR:     priname = "ERROR";     break;
		case LOG_WARNING: priname = "WARNING";   break;
		case LOG_NOTICE:  priname = "NOTICE";    break;
		case LOG_INFO:    priname = "INFO";      break;
		case LOG_DEBUG:   priname = "DEBUG";     break;
		default:
			snprintf(buf, sizeof(buf), "L:%d", priority);
			priname = buf;
	}

	if (ctx->progname)
		fprintf(stderr, "%s: ", ctx->progname);
	fprintf(stderr, "%s %s:%d %s: ", priname, file, line, fn);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");

	fflush(stderr);
}

int
kfont_init(const char *prefix, struct kfont_context **ctx)
{
	struct kfont_context *p;

	if (!(p = calloc(1, sizeof(*p))))
		return -EX_OSERR;

	p->progname = prefix;
	p->verbose = 0;
	p->options = 0;
	p->log_fn = log_stderr;
	p->mapdirpath = mapdirpath;
	p->mapsuffixes = mapsuffixes;
	p->fontdirpath = fontdirpath;
	p->fontsuffixes = fontsuffixes;
	p->partfontdirpath = partfontdirpath;
	p->partfontsuffixes = partfontsuffixes;
	p->unidirpath = unidirpath;
	p->unisuffixes = unisuffixes;

	*ctx = p;

	return 0;
}

void
kfont_free(struct kfont_context *ctx)
{
	if (ctx)
		free(ctx);
}
