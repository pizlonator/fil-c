// SPDX-License-Identifier: LGPL-2.0-or-later
/*
 * Copyright (C) 2020 Alexey Gladkov <gladkov.alexey@gmail.com>
 *
 * Originally written by Andries Brouwer
 */
#ifndef _KFONT_H_
#define _KFONT_H_

#ifndef __GNUC__
#undef  __attribute__
#define __attribute__(x) /*NOTHING*/
#endif

/*
 * Format of a psf font file:
 *
 * 1. The header
 * 2. The font
 * 3. Unicode information
 */

/*
 * Format of the Unicode information:
 *
 * For each font position <uc>*<seq>*<term>
 * where <uc> is a 2-byte little endian Unicode value,
 * <seq> = <ss><uc><uc>*, <ss> = psf1 ? 0xFFFE : 0xFE,
 * <term> = psf1 ? 0xFFFF : 0xFF.
 * and * denotes zero or more occurrences of the preceding item.
 *
 * Semantics:
 * The leading <uc>* part gives Unicode symbols that are all
 * represented by this font position. The following sequences
 * are sequences of Unicode symbols - probably a symbol
 * together with combining accents - also represented by
 * this font position.
 *
 * Example:
 * At the font position for a capital A-ring glyph, we
 * may have:
 *	00C5,212B,FFFE,0041,030A,FFFF
 * Some font positions may be described by sequences only,
 * namely when there is no precomposed Unicode value for the glyph.
 */
#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

#define PSF1_MODE512 0x01
#define PSF1_MODEHASTAB 0x02
#define PSF1_MODEHASSEQ 0x04
#define PSF1_MAXMODE 0x05

#define PSF1_SEPARATOR 0xFFFF
#define PSF1_STARTSEQ 0xFFFE

struct psf1_header {
	unsigned char magic[2]; /* Magic number */
	unsigned char mode;     /* PSF font mode */
	unsigned char charsize; /* Character size */
};

/*
 * Format and semantics of psf2 version 0 are as psf (with PSF_MAXMODE == 5).
 * However, this allows one to specify the length.
 * It turns out to be very useful to be able to work with fonts
 * with a few symbols or even only one (like the Euro), and
 * with very large fonts (like several thousand Unicode symbols
 * done in the same style).
 * Following hpa's suggestion, psf2 uses UTF-8 rather than UCS-2,
 * and has 32-bit magic 0x864ab572.
 * The integers here are little endian 4-byte integers.
 */

#define PSF2_MAGIC0 0x72
#define PSF2_MAGIC1 0xb5
#define PSF2_MAGIC2 0x4a
#define PSF2_MAGIC3 0x86

struct psf2_header {
	unsigned char magic[4];
	unsigned int version;
	unsigned int headersize; /* offset of bitmaps in file */
	unsigned int flags;
	unsigned int length;        /* number of glyphs */
	unsigned int charsize;      /* number of bytes for each character */
	unsigned int height, width; /* max dimensions of glyphs */
	                            /* charsize = height * ((width + 7) / 8) */
};

/* bits used in flags */
#define PSF2_HAS_UNICODE_TABLE 0x01

/* max version recognized so far */
#define PSF2_MAXVERSION 0

/* UTF8 separators */
#define PSF2_SEPARATOR 0xFF
#define PSF2_STARTSEQ 0xFE

#define PSF1_MAGIC_OK(x) ((x)[0] == PSF1_MAGIC0 && (x)[1] == PSF1_MAGIC1)
#define PSF2_MAGIC_OK(x) ((x)[0] == PSF2_MAGIC0 && (x)[1] == PSF2_MAGIC1 && (x)[2] == PSF2_MAGIC2 && (x)[3] == PSF2_MAGIC3)

struct kfont_context;

/* unicode.c */

#include <stdint.h>

typedef int32_t unicode;
struct unicode_list;

#include <stdarg.h>

#define MAXIFILES 256

int kfont_init(const char *prefix, struct kfont_context **ctx);
void kfont_free(struct kfont_context *ctx);

typedef void (*kfont_logger_t)(struct kfont_context *, int, const char *, int,
		const char *, const char *, va_list)
	__attribute__((nonnull(1)))
	__attribute__((format(printf, 6, 0)));

enum kfont_option {
	kfont_force,
	kfont_double_size,
};

int kfont_get_verbosity(struct kfont_context *ctx)
	__attribute__((nonnull(1)));

void kfont_inc_verbosity(struct kfont_context *ctx)
	__attribute__((nonnull(1)));

void kfont_set_logger(struct kfont_context *ctx, kfont_logger_t fn)
	__attribute__((nonnull(1)));

void kfont_set_option(struct kfont_context *ctx, enum kfont_option opt)
	__attribute__((nonnull(1)));

void kfont_unset_option(struct kfont_context *ctx, enum kfont_option opt)
	__attribute__((nonnull(1)));

/* mapscrn.c */

int kfont_load_consolemap(struct kfont_context *ctx, int consolefd,
		const char *filename)
	__attribute__((nonnull(1)));

int kfont_save_consolemap(struct kfont_context *ctx, int consolefd,
		const char *filename)
	__attribute__((nonnull(1,3)));

/* loadunimap.c */

/* save humanly readable */
int kfont_save_unicodemap(struct kfont_context *ctx, int consolefd,
		const char *filename)
	__attribute__((nonnull(1,3)));

int kfont_load_unicodemap(struct kfont_context *ctx, int consolefd,
		const char *filename)
	__attribute__((nonnull(1,3)));

/* kdfontop.c */

/*
 * Read kernel font into BUF with room for COUNT 32x32 glyphs.
 * Return 0 on success -1 on failure.
 * Sets number of glyphs in COUNT, glyph size in WIDTH and HEIGHT.
 */
int kfont_get_font(struct kfont_context *ctx, int consolefd, unsigned char *buf,
		unsigned int *count, unsigned int *width, unsigned int *height,
		unsigned int *vpitch)
	__attribute__((nonnull(1)));

/*
 * Load kernel font of width WIDTH and pointsize HEIGHT from BUF
 * with length COUNT.
 * Return 0 on success, -1 on failure.
 */
int kfont_put_font(struct kfont_context *ctx, int consolefd, unsigned char *buf,
		unsigned int count, unsigned int width, unsigned int height,
		unsigned int vpitch)
	__attribute__((nonnull(1)));

/*
 * Find the size of the kernel font.
 */
unsigned int kfont_get_fontsize(struct kfont_context *ctx, int consolefd)
	__attribute__((nonnull(1)));

/*
 * Restore font (doesn't work).
 */
int kfont_restore_font(struct kfont_context *ctx, int fd)
	__attribute__((nonnull(1)));

/* kdmapop.c */

int kfont_get_uniscrnmap(struct kfont_context *ctx, int consolefd,
		unsigned short *map)
	__attribute__((nonnull(1,3)));

int kfont_put_uniscrnmap(struct kfont_context *ctx, int consolefd,
		unsigned short *map)
	__attribute__((nonnull(1,3)));

#include <linux/kd.h>

int kfont_get_unicodemap(struct kfont_context *ctx, int consolefd,
		struct unimapdesc *ud)
	__attribute__((nonnull(1,3)));

int kfont_put_unicodemap(struct kfont_context *ctx, int consolefd,
		struct unimapinit *ui, struct unimapdesc *ud)
	__attribute__((nonnull(1)));

/* setfont.c */

int kfont_save_font(struct kfont_context *ctx, int consolefd,
		const char *filename, int with_unicodemap)
	__attribute__((nonnull(1,3)));

int kfont_load_font(struct kfont_context *ctx,
		int consolefd, const char *filename,
		unsigned int iunit, unsigned int hwunit, int no_m, int no_u)
	__attribute__((nonnull(1)));

int kfont_load_fonts(struct kfont_context *ctx,
		int consolefd, const char *const *files, int filect,
		unsigned int iunit, unsigned int hwunit, int no_m, int no_u)
	__attribute__((nonnull(1)));

void kfont_activatemap(int fd);
void kfont_disactivatemap(int fd);

/* psffontop.c */

#include <stdio.h>

/* Maximum font size that we try to handle */
#define MAXFONTSIZE (512*64*128)

/**
 * readpsffont reads a PSF font.
 *
 * The font is read either from a file (when fontf is non-NULL) or from memory
 * (namely from @p *allbufp of size @p *allszp). In the former case, if
 * @p allbufp is non-NULL, a pointer to the entire fontfile contents (possibly
 * read from pipe) is returned in @p *allbufp, and the size in @p allszp, where
 * this buffer was allocated using malloc().
 *
 * In @p fontbufp, @p fontszp the subinterval of @p allbufp containing the font
 * data is given.
 *
 * The font width is stored in @p fontwidthp.
 *
 * The number of glyphs is stored in @p fontlenp.
 *
 * The unicode table is stored in @p uclistheadsp (when non-NULL), with
 * fontpositions counted from @p fontpos0 (so that calling this several times
 * can achieve font merging).
 *
 * @returns >= 0 on success and -1 on failure. Failure means that the font was
 * not psf (but has been read). > 0 means that the Unicode table contains
 * sequences.
 */
int kfont_read_psffont(struct kfont_context *ctx,
		FILE *fontf, unsigned char **allbufp, unsigned int *allszp,
		unsigned char **fontbufp, unsigned int *fontszp,
		unsigned int *fontwidthp, unsigned int *fontheightp,
		unsigned int *fontlenp, unsigned int fontpos0,
		struct unicode_list **uclistheadsp)
	__attribute__((nonnull(1)));

int kfont_write_psffont(struct kfont_context *ctx,
		FILE *ofil, unsigned char *fontbuf,
		unsigned int width, unsigned int height, unsigned int fontlen,
		int psftype, struct unicode_list *uclistheads)
	__attribute__((nonnull(1,2,3)));

/* psfxtable.c */

int kfont_read_unicodetable(struct kfont_context *ctx, FILE *file,
		unsigned int fontlen,
		struct unicode_list **uclistheads)
	__attribute__((nonnull(1,2,4)));

int kfont_write_unicodetable(struct kfont_context *ctx, FILE *file,
		unsigned int fontlen,
		struct unicode_list *uclistheads)
	__attribute__((nonnull(1,2,4)));

#endif /* _KFONT_H_ */
