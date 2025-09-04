/* vi: set sw=4 ts=4: */
/*
 * Copyright 2003, Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * Based on specification from
 * http://www.opengroup.org/onlinepubs/007904975/utilities/uuencode.html
 *
 * Bugs: the spec doesn't mention anything about "`\n`\n" prior to the
 * "end" line
 */
//config:config UUDECODE
//config:	bool "uudecode (5.9 kb)"
//config:	default y
//config:	help
//config:	uudecode is used to decode a uuencoded file.

//applet:IF_UUDECODE(APPLET(uudecode, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_UUDECODE) += uudecode.o

//usage:#define uudecode_trivial_usage
//usage:       "[-o OUTFILE] [INFILE]"
//usage:#define uudecode_full_usage "\n\n"
//usage:       "Uudecode a file\n"
//usage:       "Finds OUTFILE in uuencoded source unless -o is given"
//usage:
//usage:#define uudecode_example_usage
//usage:       "$ uudecode -o busybox busybox.uu\n"
//usage:       "$ ls -l busybox\n"
//usage:       "-rwxr-xr-x   1 ams      ams        245264 Jun  7 21:35 busybox\n"

#include "libbb.h"

#if ENABLE_UUDECODE
static void FAST_FUNC read_stduu(FILE *src_stream, FILE *dst_stream, int flags UNUSED_PARAM)
{
	char *line;

	for (;;) {
		int encoded_len, str_len;
		char *line_ptr, *dst;
		size_t line_len;

		line_len = 64 * 1024;
		line = xmalloc_fgets_str_len(src_stream, "\n", &line_len);
		if (!line)
			break;
		/* Handle both Unix and MSDOS text.
		 * Note: space should not be trimmed, some encoders use it instead of "`"
		 * for padding of last incomplete 4-char block.
		 */
		str_len = line_len;
		while (--str_len >= 0
		 && (line[str_len] == '\n' || line[str_len] == '\r')
		) {
			line[str_len] = '\0';
		}

		if (strcmp(line, "end") == 0) {
			return; /* the only non-error exit */
		}

		line_ptr = line;
		while (*line_ptr) {
			*line_ptr = (*line_ptr - 0x20) & 0x3f;
			line_ptr++;
		}
		str_len = line_ptr - line;

		encoded_len = line[0] * 4 / 3;
		/* Check that line is not too short. (we tolerate
		 * overly _long_ line to accommodate possible extra "`").
		 * Empty line case is also caught here. */
		if (str_len <= encoded_len) {
			break; /* go to bb_error_msg_and_die("short file"); */
		}
		if (encoded_len <= 0) {
			/* Ignore the "`\n" line, why is it even in the encode file ? */
			free(line);
			continue;
		}
		if (encoded_len > 60) {
			bb_simple_error_msg_and_die("line too long");
		}

		dst = line;
		line_ptr = line + 1;
		do {
			/* Merge four 6 bit chars to three 8 bit chars */
			*dst++ = line_ptr[0] << 2 | line_ptr[1] >> 4;
			encoded_len--;
			if (encoded_len == 0) {
				break;
			}

			*dst++ = line_ptr[1] << 4 | line_ptr[2] >> 2;
			encoded_len--;
			if (encoded_len == 0) {
				break;
			}

			*dst++ = line_ptr[2] << 6 | line_ptr[3];
			line_ptr += 4;
			encoded_len -= 2;
		} while (encoded_len > 0);
		fwrite(line, 1, dst - line, dst_stream);
		free(line);
	}
	bb_simple_error_msg_and_die("short file");
}

int uudecode_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int uudecode_main(int argc UNUSED_PARAM, char **argv)
{
	FILE *src_stream;
	char *outname = NULL;
	char *line;

	getopt32(argv, "^" "o:" "\0" "?1"/* 1 arg max*/, &outname);
	argv += optind;

	if (!argv[0])
		*--argv = (char*)"-";
	src_stream = xfopen_stdin(argv[0]);

	/* Search for the start of the encoding */
	while ((line = xmalloc_fgetline(src_stream)) != NULL) {
		void FAST_FUNC (*decode_fn_ptr)(FILE *src, FILE *dst, int flags);
		char *line_ptr;
		FILE *dst_stream;
		int mode;

		if (is_prefixed_with(line, "begin-base64 ")) {
			line_ptr = line + 13;
			decode_fn_ptr = read_base64;
		} else if (is_prefixed_with(line, "begin ")) {
			line_ptr = line + 6;
			decode_fn_ptr = read_stduu;
		} else {
			free(line);
			continue;
		}

		/* begin line found. decode and exit */
		mode = bb_strtou(line_ptr, NULL, 8);
		if (outname == NULL) {
			outname = strchr(line_ptr, ' ');
			if (!outname)
				break;
			outname++;
			trim(outname); /* remove trailing space (and '\r' for DOS text) */
			if (!outname[0])
				break;
		}
		dst_stream = stdout;
		if (NOT_LONE_DASH(outname)
/* https://pubs.opengroup.org/onlinepubs/9699919799/utilities/uudecode.html
 * https://pubs.opengroup.org/onlinepubs/9699919799/utilities/uuencode.html
 * The above says that output file name specified in input file
 * or overridden by -o OUTFILE can be special "/dev/stdout" string.
 * This usually works "implicitly": many systems have /dev/stdout.
 * If ENABLE_DESKTOP, support that explicitly:
 */
		 && (!ENABLE_DESKTOP || strcmp(outname, "/dev/stdout") != 0)
		) {
			dst_stream = xfopen_for_write(outname);
			fchmod(fileno(dst_stream), mode & (S_IRWXU | S_IRWXG | S_IRWXO));
		}
		free(line);
		decode_fn_ptr(src_stream, dst_stream, /*flags:*/ BASE64_FLAG_UU_STOP + BASE64_FLAG_NO_STOP_CHAR);
		/* fclose_if_not_stdin(src_stream); - redundant */
		return EXIT_SUCCESS;
	}
	bb_simple_error_msg_and_die("no 'begin' line");
}
#endif

//config:config BASE32
//config:	bool "base32 (5.5 kb)"
//config:	default y
//config:	help
//config:	Base32 encode and decode

//config:config BASE64
//config:	bool "base64 (5.3 kb)"
//config:	default y
//config:	help
//config:	Base64 encode and decode

//usage:#define base32_trivial_usage
//usage:	"[-d] [-w COL] [FILE]"
//usage:#define base32_full_usage "\n\n"
//usage:       "Base32 encode or decode FILE to standard output\n"
//usage:     "\n	-d	Decode data"
//usage:     "\n	-w COL	Wrap lines at COL (default 76, 0 disables)"
////usage:     "\n	-i	When decoding, ignore non-alphabet characters"

//usage:#define base64_trivial_usage
//usage:	"[-d] [-w COL] [FILE]"
//usage:#define base64_full_usage "\n\n"
//usage:       "Base64 encode or decode FILE to standard output\n"
//usage:     "\n	-d	Decode data"
//usage:     "\n	-w COL	Wrap lines at COL (default 76, 0 disables)"
///////:     "\n	-i	When decoding, ignore non-alphabet characters"
// -i is accepted but has no effect: currently, decode_base32/64() functions
// (called via read_base64()) skip invalid chars unconditionally.

//                 APPLET_ODDNAME:name    main     location    suid_type     help
//applet:IF_BASE32(APPLET_ODDNAME(base32, baseNUM, BB_DIR_BIN, BB_SUID_DROP, base32))
//applet:IF_BASE64(APPLET_ODDNAME(base64, baseNUM, BB_DIR_BIN, BB_SUID_DROP, base64))

//kbuild:lib-$(CONFIG_BASE64) += uudecode.o
//kbuild:lib-$(CONFIG_BASE32) += uudecode.o

#if ENABLE_BASE32 || ENABLE_BASE64

# if ENABLE_BASE32
static void bb_b32encode(char *p, const void *src, int length)
{
#define tbl bb_uuenc_tbl_base32
	const unsigned char *s = src;

	/* Transform 5x8 bits to 8x5 bits */
	while (length > 0) {
		unsigned cur, next;

		length--;
		cur = *s++;
		*p++ = tbl[cur >> 3];			// xxxxx--- -------- -------- -------- --------
		cur &= 7;

		next = 0;
		if (--length >= 0)
			next = *s++;
		*p++ = tbl[(cur << 2) + (next >> 6)];	// -----xxx xx------ -------- -------- --------
		cur = next & 0x3f;

		*p++ = tbl[cur >> 1];			// -------- --xxxxx- -------- -------- --------
		cur &= 1;

		next = 0;
		if (--length >= 0)
			next = *s++;
		*p++ = tbl[(cur << 4) + (next >> 4)];	// -------- -------x xxxx---- -------- --------
		cur = next & 0xf;

		next = 0;
		if (--length >= 0)
			next = *s++;
		*p++ = tbl[(cur << 1) + (next >> 7)];	// -------- -------- ----xxxx x------- --------
		cur = next & 0x7f;

		*p++ = tbl[cur >> 2];			// -------- -------- -------- -xxxxx-- --------
		cur &= 3;

		next = 0;
		if (--length >= 0)
			next = *s++;
		*p++ = tbl[(cur << 3) + (next >> 5)];	// -------- -------- -------- ------xx xxx-----
		cur = next & 0x1f;

		*p++ = tbl[cur];			// -------- -------- -------- -------- ---xxxxx
	}
#undef tbl
	/* Zero-terminate */
	*p = '\0';
	/* Pad as necessary */
	length = ((-length) * 3) >> 1; /* -4 => 6 pad chars, -3 => 4, -2 => 3, -1 => 1 */
	while (length--) {
		*--p = '=';
	}
}
# else
void bb_b32encode(char *p, const void *src, int length); /* undefined */
# endif

int baseNUM_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int baseNUM_main(int argc UNUSED_PARAM, char **argv)
{
	FILE *src_stream;
	unsigned opts;
	unsigned col = 76;

	opts = getopt32(argv, "^" "diw:+" "\0" "?1"/* 1 arg max*/, &col);
	argv += optind;

	if (!argv[0])
		*--argv = (char*)"-";
	src_stream = xfopen_stdin(argv[0]);
	if (opts & 1) {
		/* -d: decode */
		int flags = (unsigned char)EOF;
		if (ENABLE_BASE32 && (!ENABLE_BASE64 || applet_name[4] == '3'))
			flags = ((unsigned char)EOF) | BASE64_32;
		read_base64(src_stream, stdout, flags);
	} else {
		enum {
			SRC_BUF_SIZE = 3 * 5 * 32, /* this *MUST* be a multiple of 3 and 5 */
			DST_BUF_SIZE = 8 * ((SRC_BUF_SIZE + 4) / 5), /* max growth on encode (base32 case) */
		};
		/* Use one buffer for both input and output:
		 * encoding reads input "left-to-right",
		 * it's safe to place source at the end of the buffer and
		 * overwrite it while encoding, just be careful to have a gap.
		 */
		char dst_buf[((DST_BUF_SIZE + /*gap:*/ 16) /*round up to 16:*/ | 0xf) + 1];
#define src_buf (dst_buf + sizeof(dst_buf) - SRC_BUF_SIZE)
		int src_fd, rem;

		src_fd = fileno(src_stream);
		rem = 0;
		while (1) {
			size_t size = full_read(src_fd, src_buf, SRC_BUF_SIZE);
			if ((ssize_t)size < 0)
				bb_simple_perror_msg_and_die(bb_msg_read_error);
			if (size == 0) {
				if (rem != 0) bb_putchar('\n');
				break;
			}

			/* Encode the buffer we just read in */
			if (ENABLE_BASE32 && (!ENABLE_BASE64 || applet_name[4] == '3')) {
				bb_b32encode(dst_buf, src_buf, size);
				size = 8 * ((size + 4) / 5);
			} else {
				bb_uuencode(dst_buf, src_buf, size, bb_uuenc_tbl_base64);
				size = 4 * ((size + 2) / 3);
			}

			if (col == 0) {
				fputs_stdout(dst_buf);
			} else {
				char *result = dst_buf;
				if (rem == 0)
					rem = col;
				while (1) {
					int out = size < rem ? size : rem;
					rem -= out;
					printf(rem != 0 ? "%.*s" : "%.*s\n", out, result);
					if (rem != 0)
						break;
					size -= out;
					if (size == 0)
						break;
					result += out;
					rem = col;
				}
			}
		}
#undef src_buf
	}

	fflush_stdout_and_exit_SUCCESS();
}
#endif

/* Test script.
Put this into an empty dir with busybox binary, an run.

#!/bin/sh
test -x busybox || { echo "No ./busybox?"; exit; }
ln -sf busybox uudecode
ln -sf busybox uuencode
>A_null
echo -n A >A
echo -n AB >AB
echo -n ABC >ABC
echo -n ABCD >ABCD
echo -n ABCDE >ABCDE
echo -n ABCDEF >ABCDEF
cat busybox >A_bbox
for f in A*; do
    echo uuencode $f
    ./uuencode    $f <$f >u_$f
    ./uuencode -m $f <$f >m_$f
done
mkdir unpk_u unpk_m 2>/dev/null
for f in u_*; do
    ./uudecode <$f -o unpk_u/${f:2}
    diff -a ${f:2} unpk_u/${f:2} >/dev/null 2>&1
    echo uudecode $f: $?
done
for f in m_*; do
    ./uudecode <$f -o unpk_m/${f:2}
    diff -a ${f:2} unpk_m/${f:2} >/dev/null 2>&1
    echo uudecode $f: $?
done
*/
