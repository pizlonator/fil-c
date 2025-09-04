/* vi: set sw=4 ts=4: */
/*
 * Mini cp implementation for busybox
 *
 * Copyright (C) 2000 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
 * SELinux support by Yuichi Nakamura <ynakam@hitachisoft.jp>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reduction.
 */
//config:config CP
//config:	bool "cp (10 kb)"
//config:	default y
//config:	help
//config:	cp is used to copy files and directories.
//config:
//config:config FEATURE_CP_LONG_OPTIONS
//config:	bool "Enable long options"
//config:	default y
//config:	depends on CP && LONG_OPTS
//config:	help
//config:	Enable long options.
//config:	Also add support for --parents option.
//config:
//config:config FEATURE_CP_REFLINK
//config:	bool "Enable --reflink[=auto]"
//config:	default y
//config:	depends on FEATURE_CP_LONG_OPTIONS

//applet:IF_CP(APPLET_NOEXEC(cp, cp, BB_DIR_BIN, BB_SUID_DROP, cp))
/* NOEXEC despite cases when it can be a "runner" (cp -r LARGE_DIR NEW_DIR) */

//kbuild:lib-$(CONFIG_CP) += cp.o

/* http://www.opengroup.org/onlinepubs/007904975/utilities/cp.html */

// Options of cp from GNU coreutils 6.10:
// -a, --archive
// -f, --force
// -i, --interactive
// -l, --link
// -L, --dereference
// -P, --no-dereference
// -R, -r, --recursive
// -s, --symbolic-link
// -v, --verbose
// -H	follow command-line symbolic links in SOURCE
// -d	same as --no-dereference --preserve=links
// -p	same as --preserve=mode,ownership,timestamps
// -c	same as --preserve=context
// -u, --update
//	copy only when the SOURCE file is newer than the destination
//	file or when the destination file is missing
// --remove-destination
//	remove each existing destination file before attempting to open
// --parents
//	use full source file name under DIRECTORY
// -T, --no-target-directory
//	treat DEST as a normal file
// NOT SUPPORTED IN BBOX:
// --backup[=CONTROL]
//	make a backup of each existing destination file
// -b	like --backup but does not accept an argument
// --copy-contents
//	copy contents of special files when recursive
// --preserve[=ATTR_LIST]
//	preserve attributes (default: mode,ownership,timestamps),
//	if possible additional attributes: security context,links,all
// --no-preserve=ATTR_LIST
// --sparse=WHEN
//	control creation of sparse files
// --strip-trailing-slashes
//	remove any trailing slashes from each SOURCE argument
// -S, --suffix=SUFFIX
//	override the usual backup suffix
// -t, --target-directory=DIRECTORY
//	copy all SOURCE arguments into DIRECTORY
// -x, --one-file-system
//	stay on this file system
// -Z, --context=CONTEXT
//	(SELinux) set SELinux security context of copy to CONTEXT

//usage:#define cp_trivial_usage
//usage:       "[-arPLHpfinlsTu] SOURCE DEST\n"
//usage:       "or: cp [-arPLHpfinlsu] SOURCE... { -t DIRECTORY | DIRECTORY }"
//usage:#define cp_full_usage "\n\n"
//usage:       "Copy SOURCEs to DEST\n"
//usage:     "\n	-a	Same as -dpR"
//usage:	IF_SELINUX(
//usage:     "\n	-c	Preserve security context"
//usage:	)
//usage:     "\n	-R,-r	Recurse"
//usage:     "\n	-d,-P	Preserve symlinks (default if -R)"
//usage:     "\n	-L	Follow all symlinks"
//usage:     "\n	-H	Follow symlinks on command line"
//usage:     "\n	-p	Preserve file attributes if possible"
//usage:     "\n	-f	Overwrite"
//usage:     "\n	-i	Prompt before overwrite"
//usage:     "\n	-n	Don't overwrite"
//usage:     "\n	-l,-s	Create (sym)links"
//usage:     "\n	-T	Refuse to copy if DEST is a directory"
//usage:     "\n	-t DIR	Copy all SOURCEs into DIR"
//usage:     "\n	-u	Copy only newer files"

#include "libbb.h"
#include "libcoreutils/coreutils.h"

/* This is a NOEXEC applet. Be very careful! */

int cp_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cp_main(int argc, char **argv)
{
	struct stat source_stat;
	struct stat dest_stat;
	const char *last;
	const char *dest;
	int s_flags;
	int d_flags;
	int flags;
	int status;
#if ENABLE_FEATURE_CP_LONG_OPTIONS
	enum {
		/*OPT_rmdest  = FILEUTILS_RMDEST = 1 << FILEUTILS_CP_OPTBITS */
		OPT_parents = 1 << (FILEUTILS_CP_OPTBITS+1),
		OPT_reflink = 1 << (FILEUTILS_CP_OPTBITS+2),
	};
# if ENABLE_FEATURE_CP_REFLINK
	char *reflink = NULL;
# endif
	flags = getopt32long(argv, "^"
		FILEUTILS_CP_OPTSTR
		"\0"
		// Need at least one argument. (Usually two+, but -t DIR can have only one)
		// Soft- and hardlinking doesn't mix
		// -P and -d are the same (-P is POSIX, -d is GNU)
		// -r and -R are the same
		// -R (and therefore -r) turns on -d (coreutils does this)
		// -a = -pdR
		// -i overrides -n and vice versa (last wins)
		"-1:l--s:s--l:Pd:rRd:Rd:apdR:i-n:n-i",
		"archive\0"        No_argument "a"
		"force\0"          No_argument "f"
		"interactive\0"    No_argument "i"
		"no-clobber\0"     No_argument "n"
		"link\0"           No_argument "l"
		"dereference\0"    No_argument "L"
		"no-dereference\0" No_argument "P"
		"recursive\0"      No_argument "R"
		"symbolic-link\0"  No_argument "s"
		"no-target-directory\0" No_argument "T"
		"target-directory\0" Required_argument "t"
		"verbose\0"        No_argument "v"
		"update\0"         No_argument "u"
		"remove-destination\0" No_argument "\xff"
		"parents\0"        No_argument "\xfe"
# if ENABLE_FEATURE_CP_REFLINK
		"reflink\0"        Optional_argument "\xfd"
# endif
		, &last
# if ENABLE_FEATURE_CP_REFLINK
		, &reflink
# endif
	);
# if ENABLE_FEATURE_CP_REFLINK
	BUILD_BUG_ON((int)OPT_reflink != (int)FILEUTILS_REFLINK);
	if (flags & FILEUTILS_REFLINK) {
		if (!reflink)
			flags |= FILEUTILS_REFLINK_ALWAYS;
		else if (strcmp(reflink, "always") == 0)
			flags |= FILEUTILS_REFLINK_ALWAYS;
		else if (strcmp(reflink, "auto") != 0)
			bb_show_usage();
	}
# endif
#else
	flags = getopt32(argv, "^"
		FILEUTILS_CP_OPTSTR
		"\0"
		"-1:l--s:s--l:Pd:rRd:Rd:apdR"
		, &last
	);
#endif
	argc -= optind;
	argv += optind;
	/* Reverse this bit. If there is -d, bit is not set: */
	flags ^= FILEUTILS_DEREFERENCE;
	/* coreutils 6.9 compat:
	 * by default, "cp" derefs symlinks (creates regular dest files),
	 * but "cp -R" does not. We switch off deref if -r or -R (see above).
	 * However, "cp -RL" must still deref symlinks: */
	if (flags & FILEUTILS_DEREF_SOFTLINK) /* -L */
		flags |= FILEUTILS_DEREFERENCE;

#if ENABLE_SELINUX
	if (flags & FILEUTILS_PRESERVE_SECURITY_CONTEXT) {
		selinux_or_die();
	}
#endif

	status = EXIT_SUCCESS;
	if (!(flags & FILEUTILS_TARGET_DIR)) {
		last = argv[argc - 1];
		if (argc < 2)
			bb_show_usage();
		if (argc != 2) {
			if (flags & FILEUTILS_NO_TARGET_DIR)
				bb_show_usage();
			/* "cp A B C... DIR" - target must be dir */
		} else /* argc == 2 */ {
			/* "cp A B" - only case where target can be not a dir */
			s_flags = cp_mv_stat2(*argv, &source_stat,
					(flags & FILEUTILS_DEREFERENCE) ? stat : lstat);
			if (s_flags < 0) /* error other than ENOENT */
				return EXIT_FAILURE;
			d_flags = cp_mv_stat(last, &dest_stat);
			if (d_flags < 0) /* error other than ENOENT */
				return EXIT_FAILURE;

			if (flags & FILEUTILS_NO_TARGET_DIR) { /* -T */
				if (!(s_flags & 2) && (d_flags & 2))
					/* cp -T NOTDIR DIR */
					bb_error_msg_and_die("'%s' is a directory", last);
			}

#if ENABLE_FEATURE_CP_LONG_OPTIONS
			//bb_error_msg("flags:%x FILEUTILS_RMDEST:%x OPT_parents:%x",
			//	flags, FILEUTILS_RMDEST, OPT_parents);
			if (flags & OPT_parents) {
				if (!(d_flags & 2)) {
					bb_simple_error_msg_and_die("with --parents, the destination must be a directory");
				}
			}
			if (flags & FILEUTILS_RMDEST) {
				flags |= FILEUTILS_FORCE;
			}
#endif

			/* ...if neither is a directory...  */
			if (!((s_flags | d_flags) & 2)
			    /* ...or: recursing, the 1st is a directory, and the 2nd doesn't exist... */
			 || ((flags & FILEUTILS_RECUR) && (s_flags & 2) && !d_flags)
			 || (flags & FILEUTILS_NO_TARGET_DIR)
			) {
				/* Do a simple copy */
				dest = last;
				goto DO_COPY; /* NB: argc==2 -> *++argv==last */
			}
		}
	}
	/* else: last is DIR from "-t DIR" */

	while (1) {
#if ENABLE_FEATURE_CP_LONG_OPTIONS
		if (flags & OPT_parents) {
			char *dest_dup;
			char *dest_dir;
			dest = concat_path_file(last, *argv);
			dest_dup = xstrdup(dest);
			dest_dir = dirname(dest_dup);
			if (bb_make_directory(dest_dir, -1, FILEUTILS_RECUR)) {
				return EXIT_FAILURE;
			}
			free(dest_dup);
			goto DO_COPY;
		}
#endif
		dest = concat_path_file(last, bb_get_last_path_component_strip(*argv));
 DO_COPY:
		if (copy_file(*argv, dest, flags) < 0) {
			status = EXIT_FAILURE;
		}
		if (!*++argv || *argv == last) {
			/* possibly leaking dest... */
			break;
		}
		/* don't move up: dest may be == last and not malloced! */
		free((void*)dest);
	}

	/* Exit. We are NOEXEC, not NOFORK. We do exit at the end of main() */
	return status;
}
