/* vi: set sw=4 ts=4: */
/*
 * xxd implementation for busybox
 *
 * Copyright (c) 2017 Denys Vlasenko <vda.linux@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config XXD
//config:	bool "xxd (11 kb)"
//config:	default y
//config:	help
//config:	The xxd utility is used to display binary data in a readable
//config:	way that is comparable to the output from most hex editors.

//applet:IF_XXD(APPLET_NOEXEC(xxd, xxd, BB_DIR_USR_BIN, BB_SUID_DROP, xxd))

//kbuild:lib-$(CONFIG_XXD) += hexdump_xxd.o

// $ xxd --version
// xxd V1.10 27oct98 by Juergen Weigert
// $ xxd --help
// Usage:
//       xxd [options] [infile [outfile]]
//    or
//       xxd -r [-s [-]offset] [-c cols] [-ps] [infile [outfile]]
// Options:
//    -a          toggle autoskip: A single '*' replaces nul-lines. Default off.
//    -b          binary digit dump (incompatible with -ps,-i,-r). Default hex.
//    -c cols     format <cols> octets per line. Default 16 (-i: 12, -ps: 30).
//    -E          show characters in EBCDIC. Default ASCII.
//    -e          little-endian dump (incompatible with -ps,-i,-r).
//    -g          number of octets per group in normal output. Default 2 (-e: 4).
//    -i          output in C include file style.
//    -l len      stop after <len> octets.
//    -o off      add <off> to the displayed file position.
//    -ps         output in postscript plain hexdump style.
//    -r          reverse operation: convert (or patch) hexdump into binary.
//    -r -s off   revert with <off> added to file positions found in hexdump.
//    -s [+][-]seek  start at <seek> bytes abs. (or +: rel.) infile offset.
//    -u          use upper case hex letters.

//usage:#define xxd_trivial_usage
//usage:       "[-pri] [-g N] [-c N] [-l LEN] [-s OFS] [-o OFS] [FILE]"
//usage:#define xxd_full_usage "\n\n"
//usage:       "Hex dump FILE (or stdin)\n"
//usage:     "\n	-g N		Bytes per group"
//usage:     "\n	-c N		Bytes per line"
//usage:     "\n	-p		Show only hex bytes, assumes -c30"
//usage:     "\n	-i		C include file style"
// exactly the same help text lines in hexdump and xxd:
//usage:     "\n	-l LENGTH	Show only first LENGTH bytes"
//usage:     "\n	-s OFFSET	Skip OFFSET bytes"
//usage:     "\n	-o OFFSET	Add OFFSET to displayed offset"
//usage:     "\n	-r		Reverse (with -p, assumes no offsets in input)"

#include "libbb.h"
#include "common_bufsiz.h"
#include "dump.h"

/* This is a NOEXEC applet. Be very careful! */

#define OPT_l (1 << 0)
#define OPT_s (1 << 1)
#define OPT_a (1 << 2)
#define OPT_p (1 << 3)
#define OPT_i (1 << 4)
#define OPT_r (1 << 5)
#define OPT_g (1 << 6)
#define OPT_c (1 << 7)
#define OPT_o (1 << 8)

#define fillbuf bb_common_bufsiz1

static void write_zeros(off_t count)
{
	errno = 0;
	do {
		unsigned sz = count < COMMON_BUFSIZE ? (unsigned)count : COMMON_BUFSIZE;
		if (fwrite(fillbuf, 1, sz, stdout) != sz)
			bb_simple_perror_msg_and_die("write error");
		count -= sz;
	} while (count != 0);
}

static void reverse(unsigned opt, const char *filename, char *opt_s)
{
	FILE *fp;
	char *buf;
	off_t cur, opt_s_ofs;

	memset(fillbuf, 0, COMMON_BUFSIZE);
	opt_s_ofs = cur = 0;
	if (opt_s) {
		opt_s_ofs = BB_STRTOOFF(opt_s, NULL, 0);
		if (errno || opt_s_ofs < 0)
			bb_error_msg_and_die("invalid number '%s'", opt_s);
	}

	fp = filename ? xfopen_for_read(filename) : stdin;

 get_new_line:
	while ((buf = xmalloc_fgetline(fp)) != NULL) {
		char *p;

		p = buf;
		if (!(opt & OPT_p)) {
			char *end;
			off_t ofs;
 skip_address:
			p = skip_whitespace(p);
			ofs = BB_STRTOOFF(p, &end, 16);
			if ((errno && errno != EINVAL)
			 || ofs < 0
			/* -s SEEK value should be added before seeking */
			 || (ofs += opt_s_ofs) < 0
			) {
				bb_error_msg_and_die("invalid number '%s'", p);
			}
			if (ofs != cur) {
				if (fseeko(stdout, ofs, SEEK_SET) != 0) {
					if (ofs < cur)
						bb_simple_perror_msg_and_die("cannot seek");
					write_zeros(ofs - cur);
				}
				cur = ofs;
			}
			p = end;
			/* NB: for xxd -r, first hex portion is address even without colon */
			/* But if colon is there, skip it: */
			if (*p == ':')
				p++;
		}

		/* Process hex bytes optionally separated by whitespace */
		for (;;) {
			uint8_t val, c;
			int badchar = 0;
 nibble1:
			if (opt & OPT_p)
				p = skip_whitespace(p);
			c = *p++;
			if (isdigit(c))
				val = c - '0';
			else if ((c|0x20) >= 'a' && (c|0x20) <= 'f')
				val = (c|0x20) - ('a' - 10);
			else {
				/* xxd V1.10 allows one non-hexnum char:
				 *  echo -e "31 !3 0a 0a" | xxd -r -p
				 * is "10<a0>" (no <cr>) - "!" is ignored,
				 * but stops for more than one:
				 *  echo -e "31 !!343434\n30 0a" | xxd -r -p
				 * is "10<cr>" - "!!" drops rest of the line.
				 * Note: this also covers whitespace chars:
				 * xxxxxxxx: 3031 3233 3435 3637 3839 3a3b 3c3d 3e3f  0123456789:;<=>?
				 *  detects this ^ - skips this one space
				 * xxxxxxxx: 3031 3233 3435 3637 3839 3a3b 3c3d 3e3f  0123456789:;<=>?
				 *                                     detects this ^^ - skips the rest
				 */
				if (c == '\0' || badchar)
					break;
				badchar++;
				goto nibble1;
			}
			val <<= 4;

 nibble2:
			if (opt & OPT_p) {
				/* Works the same with xxd V1.10:
				 *  echo "31 09 32 0a" | xxd -r -p
				 *  echo "31 0 9 32 0a" | xxd -r -p
				 * thus allow whitespace (even multiple chars)
				 * after byte's 1st char:
				 */
				p = skip_whitespace(p);
			}

			c = *p++;
			if (isdigit(c))
				val |= c - '0';
			else if ((c|0x20) >= 'a' && (c|0x20) <= 'f')
				val |= (c|0x20) - ('a' - 10);
			else {
				if (c != '\0') {
					/* "...3<not_hex_char>...": ignore "3",
					 * skip everything up to next hexchar or newline:
					 */
					while (!isxdigit(*p)) {
						if (*p == '\0') {
							free(buf);
							goto get_new_line;
						}
						p++;
					}
					goto nibble1;
				}
				/* Nibbles can join even through newline:
				 * echo -e "31 3\n2 0a" | xxd -r -p
				 * is "12<cr>".
				 */
				free(buf);
				p = buf = xmalloc_fgetline(fp);
				if (!buf)
					break;
				if (!(opt & OPT_p)) /* -p and !-p: different behavior */
					goto skip_address;
				goto nibble2;
			}
			putchar(val);
			cur++;
		} /* for(;;) */
		free(buf);
	}
	//fclose(fp);
	fflush_stdout_and_exit_SUCCESS();
}

static void print_C_style(const char *p, const char *hdr)
{
	printf(hdr, isdigit(p[0]) ? "__" : "");
	while (*p) {
		bb_putchar(isalnum(*p) ? *p : '_');
		p++;
	}
}

int xxd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int xxd_main(int argc UNUSED_PARAM, char **argv)
{
	char buf[80];
	dumper_t *dumper;
	char *opt_l, *opt_o;
	char *opt_s = NULL;
	unsigned bytes = 2;
	unsigned cols = 0;
	unsigned opt;
	int r;

	setup_common_bufsiz();

	dumper = alloc_dumper();

	opt = getopt32(argv, "^" "l:s:apirg:+c:+o:" "\0" "?1" /* 1 argument max */,
			&opt_l, &opt_s, &bytes, &cols, &opt_o
	);
	argv += optind;

	dumper->dump_vflag = ALL;
//	if (opt & OPT_a)
//		dumper->dump_vflag = SKIPNUL; ..does not exist
	if (opt & OPT_l) {
		dumper->dump_length = xstrtou_range(
				opt_l,
				/*base:*/ 0,
				/*lo:*/ 0, /*hi:*/ INT_MAX
		);
	}
	if (opt & OPT_s) {
		dumper->dump_skip = xstrtoull_range(
				opt_s,
				/*base:*/ 0,
				/*lo:*/ 0, /*hi:*/ OFF_T_MAX
		);
		//BUGGY for /proc/version (unseekable?)
	}

	if (opt & OPT_r) {
		reverse(opt, argv[0], opt_s);
	}

	if (opt & OPT_o) {
		/* -o accepts negative numbers too */
		dumper->xxd_displayoff = xstrtoll(opt_o, /*base:*/ 0);
	}

	if (opt & OPT_p) {
		if (cols == 0)
			cols = 30;
		bytes = cols; /* -p ignores -gN */
	} else {
		if (cols == 0)
			cols = (opt & OPT_i) ? 12 : 16;
		if (opt & OPT_i) {
			bytes = 1; // -i ignores -gN
			// output is "  0xXX, 0xXX, 0xXX...", add leading space
			bb_dump_add(dumper, "\" \"");
		} else
			bb_dump_add(dumper, "\"%08_ax: \""); // "address: "
	}

	if (bytes < 1 || bytes >= cols) {
		sprintf(buf, "%u/1 \"%%02x\"", cols); // cols * "XX"
		bb_dump_add(dumper, buf);
	}
	else if (bytes == 1) {
		if (opt & OPT_i)
			sprintf(buf, "%u/1 \" 0x%%02x,\"", cols); // cols * " 0xXX,"
//TODO: compat: omit the last comma after the very last byte
		else
			sprintf(buf, "%u/1 \"%%02x \"", cols); // cols * "XX "
		bb_dump_add(dumper, buf);
	}
	else {
/* Format "print byte" with and without trailing space */
#define BS "/1 \"%02x \""
#define B  "/1 \"%02x\""
		unsigned i;
		char *bigbuf = xmalloc(cols * (sizeof(BS)-1));
		char *p = bigbuf;
		for (i = 1; i <= cols; i++) {
			if (i == cols || i % bytes)
				p = stpcpy(p, B);
			else
				p = stpcpy(p, BS);
		}
		// for -g3, this results in B B BS B B BS... B = "xxxxxx xxxxxx .....xx"
		// todo: can be more clever and use
		// one 'bytes-1/1 "%02x"' format instead of many "B B B..." formats
		//bb_error_msg("ADDED:'%s'", bigbuf);
		bb_dump_add(dumper, bigbuf);
		free(bigbuf);
	}

	if (!(opt & (OPT_p|OPT_i))) {
		sprintf(buf, "\"  \"%u/1 \"%%_p\"\"\n\"", cols); // "  ASCII\n"
		bb_dump_add(dumper, buf);
	} else {
		bb_dump_add(dumper, "\"\n\"");
		dumper->xxd_eofstring = "\n";
	}

	if ((opt & OPT_i) && argv[0]) {
		print_C_style(argv[0], "unsigned char %s");
		printf("[] = {\n");
	}
	r = bb_dump_dump(dumper, argv);
	if (r == 0 && (opt & OPT_i) && argv[0]) {
		print_C_style(argv[0], "};\nunsigned int %s");
		printf("_len = %"OFF_FMT"u;\n", dumper->address);
	}
	return r;
}
