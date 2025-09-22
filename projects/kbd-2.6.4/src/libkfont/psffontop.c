// SPDX-License-Identifier: LGPL-2.0-or-later
/*
 * Copyright (C) 2007-2020 Alexey Gladkov <gladkov.alexey@gmail.com>
 * Copyright (C) 2020 Oleg Bulatov <oleg@bulatov.me>
 *
 * Originally written by Andries Brouwer
 */
#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <limits.h>

#include "libcommon.h"
#include "kfontP.h"
#include "utf8.h"

static uint32_t
assemble_uint32(unsigned char *ip)
{
	return (
		(uint32_t)ip[0] +
		((uint32_t)ip[1] << 8) +
		((uint32_t)ip[2] << 16) +
		((uint32_t)ip[3] << 24)
	);
}

static void
store_int_le(unsigned char *ip, unsigned int num)
{
	ip[0] = (unsigned char)(num & 0xff);
	ip[1] = (unsigned char)((num >> 8) & 0xff);
	ip[2] = (unsigned char)((num >> 16) & 0xff);
	ip[3] = (unsigned char)((num >> 24) & 0xff);
}

static unicode
assemble_ucs2(struct kfont_context *ctx, const unsigned char **inptr, long int cnt)
{
	int u1, u2;

	if (cnt < 2) {
		KFONT_ERR(ctx, _("short ucs2 unicode table"));
		return -EX_DATAERR;
	}

	u1 = *(*inptr)++;
	u2 = *(*inptr)++;

	return (u1 | (u2 << 8));
}

/* called with cnt > 0 and **inptr not 0xff or 0xfe */
static unicode
assemble_utf8(struct kfont_context *ctx, const unsigned char **inptr, long int cnt)
{
	int err;
	unicode uc;

	uc = from_utf8(inptr, cnt, &err);
	if (err) {
		switch (err) {
			case UTF8_SHORT:
				KFONT_ERR(ctx, _("short utf8 unicode table"));
				break;
			case UTF8_BAD:
				KFONT_ERR(ctx, _("bad utf8"));
				break;
			default:
				KFONT_ERR(ctx, _("unknown utf8 error"));
		}
		return -EX_DATAERR;
	}
	return uc;
}

/*
 * Read description of a single font position.
 */
static int
get_uni_entry(struct kfont_context *ctx,
              const unsigned char **inptr, const unsigned char **endptr,
              struct unicode_list *up, int utf8)
{
	unsigned char uc;
	unicode unichar;
	int inseq = 0;

	up->next = NULL;
	up->seq  = NULL;
	up->prev = up;

	while (1) {
		if (*endptr == *inptr) {
			KFONT_ERR(ctx, _("short unicode table"));
			return -EX_DATAERR;
		}
		if (utf8) {
			uc = *(*inptr)++;
			if (uc == PSF2_SEPARATOR)
				break;
			if (uc == PSF2_STARTSEQ) {
				inseq = 1;
				continue;
			}
			--(*inptr);
			unichar = assemble_utf8(ctx, inptr, *endptr - *inptr);
			if (unichar < 0)
				return unichar;
		} else {
			unichar = assemble_ucs2(ctx, inptr, *endptr - *inptr);
			if (unichar < 0)
				return unichar;
			if (unichar == PSF1_SEPARATOR)
				break;
			if (unichar == PSF1_STARTSEQ) {
				inseq = 1;
				continue;
			}
		}

		int ret;

		if (inseq < 2)
			ret = addpair(up, unichar);
		else
			ret = addseq(up, unichar);

		if (ret < 0) {
			KFONT_ERR(ctx, "unable to unichar: %s", strerror(-ret));
			return -EX_OSERR;
		}

		if (inseq)
			inseq++;
	}

	return 0;
}

static int
read_fontfile(struct kfont_context *ctx, FILE *fontf, unsigned char **inputbuf,
		unsigned int *inputlth)
{
	unsigned char *buf = NULL;
	unsigned int buflth = 0;
	unsigned int chunksz = MAXFONTSIZE / 4; /* random */
	int ret = 0;
	size_t n = 0;

	while(!feof(fontf)) {
		if (n >= buflth) {
			unsigned char *bufp;

			buflth += chunksz;

			bufp = realloc(buf, buflth);
			if (!bufp) {
				KFONT_ERR(ctx, "realloc: %m");
				ret = -EX_OSERR;
				goto end;
			}

			buf = bufp;
		}

		n += fread(buf + n, 1, buflth - n, fontf);

		if (ferror(fontf)) {
			KFONT_ERR(ctx, _("Error reading input font"));
			ret = -EX_DATAERR;
			goto end;
		}

		if (n > MAXFONTSIZE) {
			KFONT_ERR(ctx, _("Font is too big"));
			ret = -EX_DATAERR;
			goto end;
		}
	}

	*inputbuf = buf;
	*inputlth = (unsigned int) n;

	return 0;
end:
	free(buf);
	return ret;
}

int
kfont_read_psffont(struct kfont_context *ctx,
		FILE *fontf, unsigned char **allbufp, unsigned int *allszp,
		unsigned char **fontbufp, unsigned int *fontszp,
		unsigned int *fontwidthp, unsigned int *fontheightp,
		unsigned int *fontlenp, unsigned int fontpos0,
		struct unicode_list **uclistheadsp)
{
	unsigned char *inputbuf;
	unsigned int inputlth;
	int ret;

	/*
	 * We used to look at the length of the input file
	 * with stat(); now that we accept compressed files,
	 * just read the entire file.
	 */
	if (fontf) {
		if ((ret = read_fontfile(ctx, fontf, &inputbuf, &inputlth)) < 0)
			return ret;

		if (allbufp)
			*allbufp = inputbuf;
		if (allszp)
			*allszp = inputlth;
	} else {
		if (!allbufp || !allszp) {
			KFONT_ERR(ctx, _("Bad call of readpsffont"));
			return -EX_SOFTWARE;
		}
		inputbuf = *allbufp;
		inputlth = *allszp;
	}

	unsigned int fontlen, fontwidth, fontheight, charsize, hastable, ftoffset;
	int utf8;

	if (inputlth >= sizeof(struct psf1_header) && PSF1_MAGIC_OK(inputbuf)) {
		struct psf1_header *psfhdr;

		psfhdr = (struct psf1_header *)&inputbuf[0];

		if (psfhdr->mode > PSF1_MAXMODE) {
			KFONT_ERR(ctx, _("Unsupported psf file mode (%d)"), psfhdr->mode);
			return -EX_DATAERR;
		}
		fontlen   = ((psfhdr->mode & PSF1_MODE512) ? 512 : 256);
		charsize  = psfhdr->charsize;
		hastable  = (psfhdr->mode & (PSF1_MODEHASTAB | PSF1_MODEHASSEQ));
		ftoffset  = sizeof(struct psf1_header);
		fontwidth = 8;
		fontheight= 0;
		utf8      = 0;
	} else if (inputlth >= sizeof(struct psf2_header) && PSF2_MAGIC_OK(inputbuf)) {
		struct psf2_header psfhdr;
		unsigned int flags;

		memcpy(&psfhdr, inputbuf, sizeof(psfhdr));

		if (psfhdr.version > PSF2_MAXVERSION) {
			KFONT_ERR(ctx, _("Unsupported psf version (%d)"), psfhdr.version);
			return -EX_DATAERR;
		}
		fontlen   = assemble_uint32((unsigned char *)&psfhdr.length);
		charsize  = assemble_uint32((unsigned char *)&psfhdr.charsize);
		flags     = assemble_uint32((unsigned char *)&psfhdr.flags);
		hastable  = (flags & PSF2_HAS_UNICODE_TABLE);
		ftoffset  = assemble_uint32((unsigned char *)&psfhdr.headersize);
		fontwidth = assemble_uint32((unsigned char *)&psfhdr.width);
		fontheight= assemble_uint32((unsigned char *)&psfhdr.height);
		utf8      = 1;
	} else
		return -EX_DATAERR; /* not psf */

	/* tests required - we divide by these */
	if (fontlen == 0) {
		KFONT_ERR(ctx, _("zero input font length?"));
		return -EX_DATAERR;
	}
	if (charsize == 0) {
		KFONT_ERR(ctx, _("zero input character size?"));
		return -EX_DATAERR;
	}

	if (!fontheight) {
		unsigned int bytewidth;
		bytewidth = (fontwidth + 7) / 8;
		fontheight = (fontlen * charsize) / (bytewidth * fontlen);
	}

	unsigned int i = ftoffset + fontlen * charsize;

	if (i > inputlth || (!hastable && i != inputlth)) {
		KFONT_ERR(ctx, _("Input file: bad input length (%d)"), inputlth);
		return -EX_DATAERR;
	}

	if (fontbufp && allbufp)
		*fontbufp = *allbufp + ftoffset;
	if (fontszp)
		*fontszp = fontlen * charsize;
	if (fontlenp)
		*fontlenp = fontlen;
	if (fontwidthp)
		*fontwidthp = fontwidth;
	if (fontheightp)
		*fontheightp = fontheight;

	if (!uclistheadsp)
		return 0; /* got font, don't need unicode_list */

	struct unicode_list *ptr;

	ptr = realloc(*uclistheadsp, (fontpos0 + fontlen) * sizeof(*ptr));
	if (!ptr) {
		KFONT_ERR(ctx, "realloc: %m");
		return -EX_OSERR;
	}
	*uclistheadsp = ptr;

	if (hastable) {
		const unsigned char *inptr, *endptr;

		inptr  = inputbuf + ftoffset + fontlen * charsize;
		endptr = inputbuf + inputlth;

		for (i = 0; i < fontlen; i++) {
			ret = get_uni_entry(ctx, &inptr, &endptr,
					&(*uclistheadsp)[fontpos0 + i], utf8);
			if (ret < 0)
				return ret;
		}
		if (inptr != endptr) {
			KFONT_ERR(ctx, _("Input file: trailing garbage"));
			return -EX_DATAERR;
		}
	} else {
		for (i = 0; i < fontlen; i++)
			clear_uni_entry(&(*uclistheadsp)[fontpos0 + i]);
	}

	return 0; /* got psf font */
}

static int
has_sequences(struct unicode_list *uclistheads, unsigned int fontlen)
{
	struct unicode_list *ul;
	struct unicode_seq *us;
	unsigned int i;

	for (i = 0; i < fontlen; i++) {
		ul = uclistheads[i].next;
		while (ul) {
			us = ul->seq;
			if (us && us->next)
				return 1;
			ul = ul->next;
		}
	}
	return 0;
}

int
appendunicode(struct kfont_context *ctx, FILE *fp, int u, int utf8)
{
	unsigned int n = 6;
	unsigned char out[6];

	if (u < 0) {
		KFONT_ERR(ctx, _("illegal unicode %d"), u);
		return -EX_DATAERR;
	}

	unsigned int uc = (unsigned int)u;

	if (!utf8) {
		out[--n] = ((uc >> 8) & 0xff);
		out[--n] = (uc & 0xff);
	} else if (uc < 0x80) {
		out[--n] = (unsigned char)uc;
	} else {
		unsigned int mask = 0x3f;
		while (uc & ~mask) {
			out[--n] = (unsigned char)(0x80u + (uc & 0x3fu));
			uc >>= 6;
			mask >>= 1;
		}
		out[--n] = ((uc + ~mask + ~mask) & 0xff);
	}
	if (fwrite(out + n, 6 - n, 1, fp) != 1) {
		KFONT_ERR(ctx, "appendunimap: %m");
		return -EX_IOERR;
	}
	if (ctx->verbose > 0) {
		printf("(");
		if (!utf8)
			printf("U+");
		while (n < 6)
			printf("%02x ", out[n++]);
		printf(")");
	}

	return 0;
}

int
appendseparator(struct kfont_context *ctx, FILE *fp, int seq, int utf8)
{
	size_t n;

	if (utf8) {
		unsigned char u = (seq ? PSF2_STARTSEQ : PSF2_SEPARATOR);
		n = fwrite(&u, sizeof(u), 1, fp);
	} else {
		unsigned short u = (seq ? PSF1_STARTSEQ : PSF1_SEPARATOR);
		n = fwrite(&u, sizeof(u), 1, fp);
	}
	if (n != 1) {
		KFONT_ERR(ctx, "fwrite: %m");
		return -EX_IOERR;
	}
	return 0;
}

int
writepsffontheader(struct kfont_context *ctx,
		FILE *ofil, unsigned int width, unsigned int height,
		unsigned int fontlen, int *psftype, int flags)
{
	unsigned int bytewidth, charsize;
	size_t ret;

	bytewidth = (width + 7) / 8;
	charsize  = bytewidth * height;

	if ((fontlen != 256 && fontlen != 512) || width != 8 || height > 32)
		*psftype = 2;

	if (*psftype == 2) {
		struct psf2_header h;
		unsigned int flags2 = 0;

		if (flags & WPSFH_HASTAB)
			flags2 |= PSF2_HAS_UNICODE_TABLE;
		h.magic[0] = PSF2_MAGIC0;
		h.magic[1] = PSF2_MAGIC1;
		h.magic[2] = PSF2_MAGIC2;
		h.magic[3] = PSF2_MAGIC3;
		store_int_le((unsigned char *)&h.version, 0);
		store_int_le((unsigned char *)&h.headersize, sizeof(h));
		store_int_le((unsigned char *)&h.flags, flags2);
		store_int_le((unsigned char *)&h.length, fontlen);
		store_int_le((unsigned char *)&h.charsize, charsize);
		store_int_le((unsigned char *)&h.width, width);
		store_int_le((unsigned char *)&h.height, height);
		ret = fwrite(&h, sizeof(h), 1, ofil);
	} else {
		struct psf1_header h;

		h.magic[0] = PSF1_MAGIC0;
		h.magic[1] = PSF1_MAGIC1;
		h.mode     = 0;
		if (fontlen == 512)
			h.mode |= PSF1_MODE512;
		if (flags & WPSFH_HASSEQ)
			h.mode |= PSF1_MODEHASSEQ;
		else if (flags & WPSFH_HASTAB)
			h.mode |= PSF1_MODEHASTAB;
		h.charsize = (unsigned char) (charsize <= UCHAR_MAX ? charsize : UCHAR_MAX);
		ret = fwrite(&h, sizeof(h), 1, ofil);
	}

	if (ret != 1) {
		KFONT_ERR(ctx, _("Cannot write font file header"));
		return -EX_IOERR;
	}

	return 0;
}

int
kfont_write_psffont(struct kfont_context *ctx,
		FILE *ofil, unsigned char *fontbuf, unsigned int width,
		unsigned int height, unsigned int fontlen,
		int psftype, struct unicode_list *uclistheads)
{
	unsigned int bytewidth, charsize, i;
	int flags, utf8, ret;

	bytewidth = (width + 7) / 8;
	charsize  = bytewidth * height;
	flags     = 0;

	if (uclistheads) {
		flags |= WPSFH_HASTAB;
		if (has_sequences(uclistheads, fontlen))
			flags |= WPSFH_HASSEQ;
	}

	ret = writepsffontheader(ctx, ofil, width, height, fontlen, &psftype, flags);
	if (ret < 0)
		return ret;

	utf8 = (psftype == 2);

	if ((fwrite(fontbuf, charsize, fontlen, ofil)) != fontlen) {
		KFONT_ERR(ctx, _("Cannot write font file"));
		return -EX_IOERR;
	}

	/* unimaps: -1 => do nothing: caller will append map */
	if (uclistheads != NULL && uclistheads != (struct unicode_list *)-1) {
		struct unicode_list *ul;
		struct unicode_seq *us;

		for (i = 0; i < fontlen; i++) {
			ul = uclistheads[i].next;
			while (ul) {
				us = ul->seq;
				if (us && us->next) {
					ret = appendseparator(ctx, ofil, 1, utf8);
					if (ret < 0)
						return ret;
				}
				while (us) {
					ret = appendunicode(ctx, ofil, us->uc, utf8);
					if (ret < 0)
						return ret;
					us = us->next;
				}
				ul = ul->next;
			}
			ret = appendseparator(ctx, ofil, 0, utf8);
			if (ret < 0)
				return ret;
		}
	}
	return utf8;
}
