#ifndef UTIL_LINUX_CAREFULPUTC_H
#define UTIL_LINUX_CAREFULPUTC_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_WIDECHAR
#include <wctype.h>
#include <wchar.h>
#endif
#include <stdbool.h>

#include "cctype.h"

/*
 * A puts() for use in write and wall (that sometimes are sgid tty).
 * It avoids control and invalid characters.
 * The locale of the recipient is nominally unknown,
 * but it's a solid bet that it's compatible with the author's.
 * Use soft_width=0 to disable wrapping.
 */
static inline int fputs_careful(const char * s, FILE *fp, const char ctrl, bool cr_lf, int soft_width)
{
	int ret = 0, col = 0;

	for (size_t slen = strlen(s); *s; ++s, --slen) {
		if (*s == '\t')
			col += (7 - (col % 8)) - 1;
		else if (*s == '\r')
			col = -1;
		else if (*s == '\a')
			--col;

		if ((soft_width && col >= soft_width) || *s == '\n') {
			if (soft_width) {
				fprintf(fp, "%*s", soft_width - col, "");
				col = 0;
			}
			ret = fputs(cr_lf ? "\r\n" : "\n", fp);
			if (*s == '\n' || ret < 0)
				goto wrote;
		}

		if (isprint(*s) || *s == '\a' || *s == '\t' || *s == '\r') {
			ret = putc(*s, fp);
			++col;
		} else if (!c_isascii(*s)) {
#ifdef HAVE_WIDECHAR
			wchar_t w;
			size_t clen = mbtowc(&w, s, slen);
			switch(clen) {
				case (size_t)-2:  // incomplete
				case (size_t)-1:  // EILSEQ
					mbtowc(NULL, NULL, 0);
				nonprint:
					col += ret = fprintf(fp, "\\%3hho", *s);
					break;
				default:
					if(!iswprint(w))
						goto nonprint;
					ret = fwrite(s, 1, clen, fp);
					if (soft_width)
						col += wcwidth(w);
					s += clen - 1;
					slen -= clen - 1;
					break;
			}
#else
			col += ret = fprintf(fp, "\\%3hho", *s);
#endif
		} else {
			ret = fputs((char[]){ ctrl, *s ^ 0x40, '\0' }, fp);
			col += 2;
		}

	wrote:
		if (ret < 0)
			return EOF;
	}
	return 0;
}

static inline void fputs_quoted_case(const char *data, FILE *out, int dir)
{
	const char *p;

	fputc('"', out);
	for (p = data; p && *p; p++) {
		if ((unsigned char) *p == 0x22 ||		/* " */
		    (unsigned char) *p == 0x5c ||		/* \ */
		    (unsigned char) *p == 0x60 ||		/* ` */
		    (unsigned char) *p == 0x24 ||		/* $ */
		    !isprint((unsigned char) *p) ||
		    iscntrl((unsigned char) *p)) {

			fprintf(out, "\\x%02x", (unsigned char) *p);
		} else
			fputc(dir ==  1 ? toupper(*p) :
			      dir == -1 ? tolower(*p) :
			      *p, out);
	}
	fputc('"', out);
}

#define fputs_quoted(_d, _o)		fputs_quoted_case(_d, _o, 0)
#define fputs_quoted_upper(_d, _o)	fputs_quoted_case(_d, _o, 1)
#define fputs_quoted_lower(_d, _o)	fputs_quoted_case(_d, _o, -1)

static inline void fputs_nonblank(const char *data, FILE *out)
{
	const char *p;

	for (p = data; p && *p; p++) {
		if (isblank((unsigned char) *p) ||
		    (unsigned char) *p == 0x5c ||		/* \ */
		    !isprint((unsigned char) *p) ||
		    iscntrl((unsigned char) *p)) {

			fprintf(out, "\\x%02x", (unsigned char) *p);

		} else
			fputc(*p, out);
	}
}

#endif  /*  _CAREFULPUTC_H  */
