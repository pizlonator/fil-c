// SPDX-License-Identifier: LGPL-2.0-or-later
/*
 * Copyright (C) 2020 Alexey Gladkov <gladkov.alexey@gmail.com>
 */
#ifndef _KFONT_PRIVATE_H_
#define _KFONT_PRIVATE_H_

#include "kfont.h"

struct kfont_context {
	const char *progname;
	int verbose;
	kfont_logger_t log_fn;

	unsigned int options;

	const char *const *mapdirpath;
	const char *const *mapsuffixes;

	const char *const *fontdirpath;
	const char *const *fontsuffixes;

	const char *const *partfontdirpath;
	const char *const *partfontsuffixes;

	const char *const *unidirpath;
	const char *const *unisuffixes;
};

void logger(struct kfont_context *ctx, int priority, const char *file,
		int line, const char *fn, const char *fmt, ...)
	__attribute__((format(printf, 6, 7)))
	__attribute__((nonnull(1)));

#include <syslog.h>

#define KFONT_DBG(ctx,  arg...) logger(ctx, LOG_DEBUG,   __FILE__, __LINE__, __func__, ##arg)
#define KFONT_INFO(ctx, arg...) logger(ctx, LOG_INFO,    __FILE__, __LINE__, __func__, ##arg)
#define KFONT_WARN(ctx, arg...) logger(ctx, LOG_WARNING, __FILE__, __LINE__, __func__, ##arg)
#define KFONT_ERR(ctx,  arg...) logger(ctx, LOG_ERR,     __FILE__, __LINE__, __func__, ##arg)

void log_stderr(struct kfont_context *ctx, int priority, const char *file,
		const int line, const char *fn, const char *format, va_list args)
	__attribute__((format(printf, 6, 0)))
	__attribute__((nonnull(1)));

/* unicode.c */

struct unicode_seq {
	struct unicode_seq *next;
	struct unicode_seq *prev;
	unicode uc;
};

struct unicode_list {
	struct unicode_list *next;
	struct unicode_list *prev;
	struct unicode_seq *seq;
};

int addpair(struct unicode_list *up, unicode uc);
int addseq(struct unicode_list *up, unicode uc);
void clear_uni_entry(struct unicode_list *up);

/* loadunimap.c */

int appendunicodemap(struct kfont_context *ctx, int fd, FILE *fp,
		unsigned int ct, int utf8)
	__attribute__((nonnull(1)));

/* kdfontop.c */

/*
 * Read kernel font into BUF with room for COUNT 32x32 glyphs.
 * Return 0 on success -1 on failure.
 * Sets number of glyphs in COUNT, glyph size in WIDTH and HEIGHT.
 */
int getfont(struct kfont_context *ctx, int fd, unsigned char *buf,
		unsigned int *count, unsigned int *width, unsigned int *height)
	__attribute__((nonnull(1)));

/*
 * Load kernel font of width WIDTH and pointsize HEIGHT from BUF
 * with length COUNT.
 * Return 0 on success, -1 on failure.
 */
int putfont(struct kfont_context *ctx, int fd, unsigned char *buf,
		unsigned int count, unsigned int width, unsigned int height)
	__attribute__((nonnull(1)));

/*
 * Find the maximum height of nonblank pixels
 * (in the ((WIDTH+7)/8)*32*COUNT bytes of BUF).
 */
unsigned int font_charheight(unsigned char *buf, unsigned int count,
		unsigned int width)
	__attribute__((nonnull(1)));

/* kdmapop.c */

int getscrnmap(struct kfont_context *ctx, int fd, unsigned char *map)
	__attribute__((nonnull(1)));

int loadscrnmap(struct kfont_context *ctx, int fd, unsigned char *map)
	__attribute__((nonnull(1)));

/* psffontop.c */

#define WPSFH_HASTAB 1
#define WPSFH_HASSEQ 2
int writepsffontheader(struct kfont_context *ctx, FILE *ofil,
		unsigned int width, unsigned int height, unsigned int fontlen,
		int *psftype, int flags);

int appendunicode(struct kfont_context *ctx, FILE *fp, unicode uc, int utf8);
int appendseparator(struct kfont_context *ctx, FILE *fp, int seq, int utf8);

#endif /* _KFONT_PRIVATE_H_ */
