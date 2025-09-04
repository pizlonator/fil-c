/* vi: set sw=4 ts=4: */
/*
 * Adapted from ash applet code
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1997-2005 Herbert Xu <herbert@gondor.apana.org.au>
 * was re-ported from NetBSD and debianized.
 *
 * Copyright (c) 2010 Denys Vlasenko
 * Split from ash.c
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "shell_common.h"

const char defifsvar[] ALIGN1 = "IFS= \t\n";
const char defoptindvar[] ALIGN1 = "OPTIND=1";

/* Compare two strings up to the first '=' or '\0'. */
int FAST_FUNC varcmp(const char *p, const char *q)
{
	int c, d;

	while ((c = *p) == (d = *q)) {
		if (c == '\0' || c == '=')
			goto out;
		p++;
		q++;
	}
	if (c == '=')
		c = '\0';
	if (d == '=')
		d = '\0';
 out:
	return c - d;
}

/* read builtin */

/* Needs to be interruptible: shell must handle traps and shell-special signals
 * while inside read. To implement this, be sure to not loop on EINTR
 * and return errno == EINTR reliably.
 */
//TODO: use more efficient setvar() which takes a pointer to malloced "VAR=VAL"
//string. hush naturally has it, and ash has setvareq().
//Here we can simply store "VAR=" at buffer start and store read data directly
//after "=", then pass buffer to setvar() to consume.
const char* FAST_FUNC
shell_builtin_read(struct builtin_read_params *params)
{
	struct pollfd pfd[1];
#define fd (pfd[0].fd) /* -u FD */
	unsigned err;
	unsigned end_ms; /* -t TIMEOUT */
	int nchars; /* -n NUM */
	char **pp;
	char *buffer;
	char delim;
	struct termios tty, old_tty;
	const char *retval;
	int bufpos; /* need to be able to hold -1 */
	int startword;
	smallint backslash;
	char **argv;
	const char *ifs;
	int read_flags;

	errno = err = 0;

	argv = params->argv;
	pp = argv;
	while (*pp) {
		if (!*pp[0] || endofname(*pp)[0] != '\0') {
			/* Mimic bash message */
			bb_error_msg("read: '%s': bad variable name", *pp);
			return (const char *)(uintptr_t)1;
		}
		pp++;
	}

	nchars = 0; /* if != 0, -n is in effect */
	if (params->opt_n) {
		nchars = bb_strtou(params->opt_n, NULL, 10);
		if (nchars < 0 || errno)
			return "invalid count";
		/* note: "-n 0": off (bash 3.2 does this too) */
	}

	end_ms = 0;
	if (params->opt_t && !ENABLE_FEATURE_SH_READ_FRAC) {
		end_ms = bb_strtou(params->opt_t, NULL, 10);
		if (errno)
			return "invalid timeout";
		if (end_ms > UINT_MAX / 2048) /* be safely away from overflow */
			end_ms = UINT_MAX / 2048;
		end_ms *= 1000;
	}
	if (params->opt_t && ENABLE_FEATURE_SH_READ_FRAC) {
		/* bash 4.3 (maybe earlier) supports -t N.NNNNNN */
		char *p;
		/* Eat up to three fractional digits */
		int frac_digits = 3 + 1;

		end_ms = bb_strtou(params->opt_t, &p, 10);
		if (end_ms > UINT_MAX / 2048) /* be safely away from overflow */
			end_ms = UINT_MAX / 2048;

		if (errno) {
			/* EINVAL = number is ok, but not NUL terminated */
			if (errno != EINVAL || *p != '.')
				return "invalid timeout";
			/* Do not check the rest: bash allows "0.123456xyz" */
			while (*++p && --frac_digits) {
				end_ms *= 10;
				end_ms += (*p - '0');
				if ((unsigned char)(*p - '0') > 9)
					return "invalid timeout";
			}
		}
		while (--frac_digits > 0) {
			end_ms *= 10;
		}
	}

	fd = STDIN_FILENO;
	if (params->opt_u) {
		fd = bb_strtou(params->opt_u, NULL, 10);
		if (fd < 0 || errno)
			return "invalid file descriptor";
	}

	if (params->opt_t && end_ms == 0) {
		/* "If timeout is 0, read returns immediately, without trying
		 * to read any data. The exit status is 0 if input is available
		 * on the specified file descriptor, non-zero otherwise."
		 * bash seems to ignore -p PROMPT for this use case.
		 */
		int r;
		pfd[0].events = POLLIN;
		r = poll(pfd, 1, /*timeout:*/ 0);
		/* Return 0 only if poll returns 1 ("one fd ready"), else return 1: */
		return (const char *)(uintptr_t)(r <= 0);
	}

	if (params->opt_p && isatty(fd)) {
		fputs(params->opt_p, stderr);
		fflush_all();
	}

	ifs = params->ifs;
	if (ifs == NULL)
		ifs = defifs;

	read_flags = params->read_flags;
	if (nchars || (read_flags & BUILTIN_READ_SILENT)) {
		tcgetattr(fd, &tty);
		old_tty = tty;
		if (nchars) {
			tty.c_lflag &= ~ICANON;
			// Setting it to more than 1 breaks poll():
			// it blocks even if there's data. !??
			//tty.c_cc[VMIN] = nchars < 256 ? nchars : 255;
			/* reads will block only if < 1 char is available */
			tty.c_cc[VMIN] = 1;
			/* no timeout (reads block forever) */
			tty.c_cc[VTIME] = 0;
		}
		if (read_flags & BUILTIN_READ_SILENT) {
			tty.c_lflag &= ~(ECHO | ECHOK | ECHONL);
		}
		/* This forces execution of "restoring" tcgetattr later */
		read_flags |= BUILTIN_READ_SILENT;
		/* if tcgetattr failed, tcsetattr will fail too.
		 * Ignoring, it's harmless. */
		tcsetattr(fd, TCSANOW, &tty);
	}

	retval = (const char *)(uintptr_t)0;
	startword = 1;
	backslash = 0;
	if (params->opt_t)
		end_ms += (unsigned)monotonic_ms();
	buffer = NULL;
	bufpos = 0;
	delim = params->opt_d ? params->opt_d[0] : '\n';
	do {
		char c;
		int timeout;

		if ((bufpos & 0xff) == 0)
			buffer = xrealloc(buffer, bufpos + 0x101);

		timeout = -1;
		if (params->opt_t) {
			timeout = end_ms - (unsigned)monotonic_ms();
			/* ^^^^^^^^^^^^^ all values are unsigned,
			 * wrapping math is used here, good even if
			 * 32-bit unix time wrapped (year 2038+).
			 */
			if (timeout <= 0) { /* already late? */
				retval = (const char *)(uintptr_t)1;
				goto ret;
			}
		}

		/* We must poll even if timeout is -1:
		 * we want to be interrupted if signal arrives,
		 * regardless of SA_RESTART-ness of that signal!
		 */
		errno = 0;
		pfd[0].events = POLLIN;
//TODO race with a signal arriving just before the poll!
		if (poll(pfd, 1, timeout) <= 0) {
			/* timed out, or EINTR */
			err = errno;
			retval = (const char *)(uintptr_t)1;
			goto ret;
		}
		if (read(fd, &buffer[bufpos], 1) != 1) {
			err = errno;
			retval = (const char *)(uintptr_t)1;
			break;
		}

		c = buffer[bufpos];
		if (!(read_flags & BUILTIN_READ_RAW)) {
			if (backslash) {
				backslash = 0;
				if (c != '\n')
					goto put;
				continue;
			}
			if (c == '\\') {
				backslash = 1;
				continue;
			}
		}
		if (c == delim) /* '\n' or -d CHAR */
			break;
		if (c == '\0')
			continue;

		/* $IFS splitting. NOT done if we run "read"
		 * without variable names (bash compat).
		 * Thus, "read" and "read REPLY" are not the same.
		 */
		if (argv[0]) {
/* http://www.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_06_05 */
			const char *is_ifs = strchr(ifs, c);
			if (startword && is_ifs) {
				if (isspace(c))
					continue;
				/* it is a non-space ifs char */
				startword--;
				if (startword == 1) /* first one? */
					continue; /* yes, it is not next word yet */
			}
			startword = 0;
			if (argv[1] != NULL && is_ifs) {
				buffer[bufpos] = '\0';
				bufpos = 0;
				params->setvar(*argv, buffer);
				argv++;
				/* can we skip one non-space ifs char? (2: yes) */
				startword = isspace(c) ? 2 : 1;
				continue;
			}
		}
 put:
		bufpos++;
	} while (--nchars);

	if (argv[0]) {
		/* Remove trailing space $IFS chars */
		while (--bufpos >= 0
		 && isspace(buffer[bufpos])
		 && strchr(ifs, buffer[bufpos]) != NULL
		) {
			continue;
		}
		buffer[bufpos + 1] = '\0';

		/* Last variable takes the entire remainder with delimiters
		 * (sans trailing whitespace $IFS),
		 * but ***only "if there are fewer vars than fields"(c)***!
		 * The "X:Y:" case below: there are two fields,
		 * and therefore last delimiter (:) is eaten:
		 * IFS=": "
		 * echo "X:Y:Z:"  | (read x y; echo "|$x|$y|") # |X|Y:Z:|
		 * echo "X:Y:Z"   | (read x y; echo "|$x|$y|") # |X|Y:Z|
		 * echo "X:Y:"    | (read x y; echo "|$x|$y|") # |X|Y|, not |X|Y:|
		 * echo "X:Y  : " | (read x y; echo "|$x|$y|") # |X|Y|
		 */
		if (bufpos >= 0
		 && strchr(ifs, buffer[bufpos]) != NULL
		) {
			/* There _is_ a non-whitespace IFS char */
			/* Skip whitespace IFS char before it */
			while (--bufpos >= 0
			 && isspace(buffer[bufpos])
			 && strchr(ifs, buffer[bufpos]) != NULL
			) {
				continue;
			}
			/* Are there $IFS chars? */
			if (strcspn(buffer, ifs) >= ++bufpos) {
				/* No: last var takes one field, not more */
				/* So, drop trailing IFS delims */
				buffer[bufpos] = '\0';
			}
		}

		/* Use the remainder as a value for the next variable */
		params->setvar(*argv, buffer);
		/* Set the rest to "" */
		while (*++argv)
			params->setvar(*argv, "");
	} else {
		/* Note: no $IFS removal */
		buffer[bufpos] = '\0';
		params->setvar("REPLY", buffer);
	}

 ret:
	free(buffer);
	if (read_flags & BUILTIN_READ_SILENT)
		tcsetattr(fd, TCSANOW, &old_tty);

	errno = err;
	return retval;
#undef fd
}

/* ulimit builtin */

struct limits {
	uint8_t cmd;            /* RLIMIT_xxx fit into it */
	uint8_t factor_shift;   /* shift by to get rlim_{cur,max} values */
};

/* Order of entries matches order in which bash prints "ulimit -a" */
static const struct limits limits_tbl[] ALIGN2 = {
	{ RLIMIT_CORE,		9,	}, // -c
	{ RLIMIT_DATA,		10,	}, // -d
#ifdef RLIMIT_NICE
	{ RLIMIT_NICE,		0,	}, // -e
#define LIMIT_F_IDX     3
#else
/* for example, Hurd */
#define LIMIT_F_IDX     2
#endif
	{ RLIMIT_FSIZE,		9,	}, // -f
#ifdef RLIMIT_SIGPENDING
	{ RLIMIT_SIGPENDING,	0,	}, // -i
#endif
#ifdef RLIMIT_MEMLOCK
	{ RLIMIT_MEMLOCK,	10,	}, // -l
#endif
#ifdef RLIMIT_RSS
	{ RLIMIT_RSS,		10,	}, // -m
#endif
#ifdef RLIMIT_NOFILE
	{ RLIMIT_NOFILE,	0,	}, // -n
#endif
#ifdef RLIMIT_MSGQUEUE
	{ RLIMIT_MSGQUEUE,	0,	}, // -q
#endif
#ifdef RLIMIT_RTPRIO
	{ RLIMIT_RTPRIO,	0,	}, // -r
#endif
#ifdef RLIMIT_STACK
	{ RLIMIT_STACK,		10,	}, // -s
#endif
#ifdef RLIMIT_CPU
	{ RLIMIT_CPU,		0,	}, // -t
#endif
#ifdef RLIMIT_NPROC
	{ RLIMIT_NPROC,		0,	}, // -u
#endif
#ifdef RLIMIT_AS
	{ RLIMIT_AS,		10,	}, // -v
#endif
#ifdef RLIMIT_LOCKS
	{ RLIMIT_LOCKS,		0,	}, // -x
#endif
};
// 1) bash also shows:
//pipe size            (512 bytes, -p) 8
// 2) RLIMIT_RTTIME ("timeout for RT tasks in us") is not in the table

static const char limits_help[] ALIGN1 =
	"core file size (blocks)"          // -c
	"\0""data seg size (kb)"           // -d
#ifdef RLIMIT_NICE
	"\0""scheduling priority"          // -e
#endif
	"\0""file size (blocks)"           // -f
#ifdef RLIMIT_SIGPENDING
	"\0""pending signals"              // -i
#endif
#ifdef RLIMIT_MEMLOCK
	"\0""max locked memory (kb)"       // -l
#endif
#ifdef RLIMIT_RSS
	"\0""max memory size (kb)"         // -m
#endif
#ifdef RLIMIT_NOFILE
	"\0""open files"                   // -n
#endif
#ifdef RLIMIT_MSGQUEUE
	"\0""POSIX message queues (bytes)" // -q
#endif
#ifdef RLIMIT_RTPRIO
	"\0""real-time priority"           // -r
#endif
#ifdef RLIMIT_STACK
	"\0""stack size (kb)"              // -s
#endif
#ifdef RLIMIT_CPU
	"\0""cpu time (seconds)"           // -t
#endif
#ifdef RLIMIT_NPROC
	"\0""max user processes"           // -u
#endif
#ifdef RLIMIT_AS
	"\0""virtual memory (kb)"          // -v
#endif
#ifdef RLIMIT_LOCKS
	"\0""file locks"                   // -x
#endif
;

static const char limit_chars[] ALIGN1 =
			"c"
			"d"
#ifdef RLIMIT_NICE
			"e"
#endif
			"f"
#ifdef RLIMIT_SIGPENDING
			"i"
#endif
#ifdef RLIMIT_MEMLOCK
			"l"
#endif
#ifdef RLIMIT_RSS
			"m"
#endif
#ifdef RLIMIT_NOFILE
			"n"
#endif
#ifdef RLIMIT_MSGQUEUE
			"q"
#endif
#ifdef RLIMIT_RTPRIO
			"r"
#endif
#ifdef RLIMIT_STACK
			"s"
#endif
#ifdef RLIMIT_CPU
			"t"
#endif
#ifdef RLIMIT_NPROC
			"u"
#endif
#ifdef RLIMIT_AS
			"v"
#endif
#ifdef RLIMIT_LOCKS
			"x"
#endif
;

/* "-": treat args as parameters of option with ASCII code 1 */
static const char ulimit_opt_string[] ALIGN1 = "-HSa"
			"c::"
			"d::"
#ifdef RLIMIT_NICE
			"e::"
#endif
			"f::"
#ifdef RLIMIT_SIGPENDING
			"i::"
#endif
#ifdef RLIMIT_MEMLOCK
			"l::"
#endif
#ifdef RLIMIT_RSS
			"m::"
#endif
#ifdef RLIMIT_NOFILE
			"n::"
#endif
#ifdef RLIMIT_MSGQUEUE
			"q::"
#endif
#ifdef RLIMIT_RTPRIO
			"r::"
#endif
#ifdef RLIMIT_STACK
			"s::"
#endif
#ifdef RLIMIT_CPU
			"t::"
#endif
#ifdef RLIMIT_NPROC
			"u::"
#endif
#ifdef RLIMIT_AS
			"v::"
#endif
#ifdef RLIMIT_LOCKS
			"x::"
#endif
;

enum {
	OPT_hard = (1 << 0),
	OPT_soft = (1 << 1),
	OPT_all  = (1 << 2),
};

static void printlim(unsigned opts, const struct rlimit *limit,
			const struct limits *l)
{
	rlim_t val;

	val = limit->rlim_max;
	if (opts & OPT_soft)
		val = limit->rlim_cur;

	if (val == RLIM_INFINITY)
		puts("unlimited");
	else {
		val >>= l->factor_shift;
		printf("%llu\n", (long long) val);
	}
}

int FAST_FUNC
shell_builtin_ulimit(char **argv)
{
	struct rlimit limit;
	unsigned opt_cnt;
	unsigned opts;
	unsigned argc;
	unsigned i;

	/* We can't use getopt32: need to handle commands like
	 * ulimit 123 -c2 -l 456
	 */

	/* In case getopt() was already called:
	 * reset libc getopt() internal state.
	 */
	GETOPT_RESET();

// bash 4.4.23:
//
// -H and/or -S change meaning even of options *before* them: ulimit -f 2000 -H
// sets hard limit, ulimit -a -H prints hard limits.
//
// -a is equivalent for requesting all limits to be shown.
//
// If -a is specified, attempts to set limits are ignored:
//  ulimit -m 1000; ulimit -m 2000 -a
// shows 1000, not 2000. HOWEVER, *implicit* -f form "ulimit 2000 -a"
// DOES set -f limit [we don't implement this quirk], "ulimit -a 2000" does not.
// Options are still parsed: ulimit -az complains about unknown -z opt.
//
// -a is not cumulative: "ulimit -a -a" = "ulimit -a -f -m" = "ulimit -a"
//
// -HSa can be combined in one argument and with one other option (example: -Sm),
// but other options can't: limit value is an optional argument,
// thus "-mf" means "-m f", f is the parameter of -m.
//
// Limit can be set and then printed: ulimit -m 2000 -m
// If set more than once, they are set and printed in order:
// try ulimit -m -m 1000 -m -m 2000 -m -m 3000 -m
//
// Limits are shown in the order of options given:
// ulimit -m -f is not the same as ulimit -f -m.
//
// If both -S and -H are given, show soft limit.
//
// Short printout (limit value only) is printed only if just one option
// is given: ulimit -m. ulimit -f -m prints verbose lines.
// ulimit -f -f prints same verbose line twice.
// ulimit -m 10000 -f prints verbose line for -f.

	argc = string_array_len(argv);

	/* First pass over options: detect -H/-S/-a status,
	 * and "bare ulimit" and "only one option" cases
	 * by counting other opts.
	 */
	opt_cnt = 0;
	opts = 0;
	while (1) {
		int opt_char = getopt(argc, argv, ulimit_opt_string);

		if (opt_char == -1)
			break;
		if (opt_char == 'H') {
			opts |= OPT_hard;
			continue;
		}
		if (opt_char == 'S') {
			opts |= OPT_soft;
			continue;
		}
		if (opt_char == 'a') {
			opts |= OPT_all;
			continue;
		}
		if (opt_char == '?') {
			/* bad option. getopt already complained. */
			return EXIT_FAILURE;
		}
		opt_cnt++;
	} /* while (there are options) */

	if (!(opts & (OPT_hard | OPT_soft)))
		opts |= (OPT_hard | OPT_soft);
	if (opts & OPT_all) {
		const char *help = limits_help;
		for (i = 0; i < ARRAY_SIZE(limits_tbl); i++) {
			getrlimit(limits_tbl[i].cmd, &limit);
			printf("%-32s(-%c) ", help, limit_chars[i]);
			printlim(opts, &limit, &limits_tbl[i]);
			help += strlen(help) + 1;
		}
		return EXIT_SUCCESS;
	}

	/* Second pass: set or print limits, in order */
	GETOPT_RESET();
	while (1) {
		char *val_str;
		int opt_char = getopt(argc, argv, ulimit_opt_string);

		if (opt_char == -1)
			break;
		if (opt_char == 'H')
			continue;
		if (opt_char == 'S')
			continue;
		//if (opt_char == 'a') - impossible

		if (opt_char == 1) /* if "ulimit NNN", -f is assumed */
			opt_char = 'f';
		i = strchrnul(limit_chars, opt_char) - limit_chars;
		//if (i >= ARRAY_SIZE(limits_tbl)) - bad option, impossible

		val_str = optarg;
		if (!val_str && argv[optind] && argv[optind][0] != '-')
			val_str = argv[optind++]; /* ++ skips NN in "-c NN" case */

		getrlimit(limits_tbl[i].cmd, &limit);
		if (!val_str) {
			if (opt_cnt > 1)
				printf("%-32s(-%c) ", nth_string(limits_help, i), limit_chars[i]);
			printlim(opts, &limit, &limits_tbl[i]);
		} else {
			rlim_t val = RLIM_INFINITY;
			if (strcmp(val_str, "unlimited") != 0) {
				if (sizeof(val) == sizeof(int))
					val = bb_strtou(val_str, NULL, 10);
				else if (sizeof(val) == sizeof(long))
					val = bb_strtoul(val_str, NULL, 10);
				else
					val = bb_strtoull(val_str, NULL, 10);
				if (errno) {
					bb_error_msg("invalid number '%s'", val_str);
					return EXIT_FAILURE;
				}
				val <<= limits_tbl[i].factor_shift;
			}
//bb_error_msg("opt %c val_str:'%s' val:%lld", opt_char, val_str, (long long)val);
			/* from man bash: "If neither -H nor -S
			 * is specified, both the soft and hard
			 * limits are set. */
			if (opts & OPT_hard)
				limit.rlim_max = val;
			if (opts & OPT_soft)
				limit.rlim_cur = val;
//bb_error_msg("setrlimit(%d, %lld, %lld)", limits_tbl[i].cmd, (long long)limit.rlim_cur, (long long)limit.rlim_max);
			if (setrlimit(limits_tbl[i].cmd, &limit) < 0) {
				bb_simple_perror_msg("error setting limit");
				return EXIT_FAILURE;
			}
		}
	} /* while (there are options) */

	if (opt_cnt == 0) {
		/* "bare ulimit": treat it as if it was -f */
		getrlimit(RLIMIT_FSIZE, &limit);
		printlim(opts, &limit, &limits_tbl[LIMIT_F_IDX]);
	}

	return EXIT_SUCCESS;
}
