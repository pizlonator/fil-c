/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * mount(8) -- mount a filesystem
 *
 * Copyright (C) 2011 Red Hat, Inc. All rights reserved.
 * Written by Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <libmount.h>
#include <ctype.h>

#include "nls.h"
#include "c.h"
#include "env.h"
#include "strutils.h"
#include "closestream.h"
#include "canonicalize.h"
#include "pathnames.h"

#define XALLOC_EXIT_CODE MNT_EX_SYSERR
#include "xalloc.h"

#define OPTUTILS_EXIT_CODE MNT_EX_USAGE
#include "optutils.h"

static struct ul_env_list *envs_removed;

static int mk_exit_code(struct libmnt_context *cxt, int rc);

static void suid_drop(struct libmnt_context *cxt)
{
	const uid_t ruid = getuid();
	const uid_t euid = geteuid();

	if (ruid != 0 && euid == 0 && drop_permissions() != 0)
		err(MNT_EX_FAIL, _("drop permissions failed"));

	/* be paranoid and check it, setuid(0) has to fail */
	if (ruid != 0 && setuid(0) == 0)
		errx(MNT_EX_FAIL, _("drop permissions failed."));

	mnt_context_force_unrestricted(cxt);

	/* restore "bad" environment variables */
	if (envs_removed) {
		env_list_setenv(envs_removed);
		env_list_free(envs_removed);
		envs_removed = NULL;
	}
}

static void __attribute__((__noreturn__)) mount_print_version(void)
{
	const char *ver = NULL;
	const char **features = NULL, **p;

	mnt_get_library_version(&ver);
	mnt_get_library_features(&features);

	printf(_("%s from %s (libmount %s"),
			program_invocation_short_name,
			PACKAGE_STRING,
			ver);
	p = features;
	while (p && *p) {
		fputs(p == features ? ": " : ", ", stdout);
		fputs(*p++, stdout);
	}
	fputs(")\n", stdout);
	exit(MNT_EX_SUCCESS);
}

static int table_parser_errcb(struct libmnt_table *tb __attribute__((__unused__)),
			const char *filename, int line)
{
	if (filename)
		warnx(_("%s: parse error at line %d -- ignored"), filename, line);
	return 1;
}

/*
 * Replace control chars with '?' to be compatible with coreutils. For more
 * robust solution use findmnt(1) where we use \x?? hex encoding.
 */
static void safe_fputs(const char *data)
{
	const char *p;

	for (p = data; p && *p; p++) {
		if (iscntrl((unsigned char) *p))
			fputc('?', stdout);
		else
			fputc(*p, stdout);
	}
}

static void print_all(struct libmnt_context *cxt, char *pattern, int show_label)
{
	struct libmnt_table *tb;
	struct libmnt_iter *itr = NULL;
	struct libmnt_fs *fs;
	struct libmnt_cache *cache = NULL;

	mnt_context_enable_noautofs(cxt, 1);

	if (mnt_context_get_mtab(cxt, &tb))
		err(MNT_EX_SYSERR, _("failed to read mtab"));

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		err(MNT_EX_SYSERR, _("failed to initialize libmount iterator"));
	if (show_label)
		cache = mnt_new_cache();

	while (mnt_table_next_fs(tb, itr, &fs) == 0) {
		const char *type = mnt_fs_get_fstype(fs);
		const char *src = mnt_fs_get_source(fs);
		const char *optstr = mnt_fs_get_options(fs);
		char *xsrc = NULL;

		if (type && pattern && !mnt_match_fstype(type, pattern))
			continue;

		if (mnt_fs_is_regularfs(fs))
			xsrc = mnt_pretty_path(src, cache);
		printf ("%s on ", xsrc ? xsrc : src);
		safe_fputs(mnt_fs_get_target(fs));

		if (type)
			printf (" type %s", type);
		if (optstr)
			printf (" (%s)", optstr);
		if (show_label && src) {
			char *lb = mnt_cache_find_tag_value(cache, src, "LABEL");
			if (lb)
				printf (" [%s]", lb);
		}
		fputc('\n', stdout);
		free(xsrc);
	}

	mnt_unref_cache(cache);
	mnt_free_iter(itr);
}

/*
 * mount -a [-F]
 */
static int mount_all(struct libmnt_context *cxt)
{
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;
	int mntrc, ignored, rc = MNT_EX_SUCCESS;

	int nsucc = 0, nerrs = 0;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr) {
		warn(_("failed to initialize libmount iterator"));
		return MNT_EX_SYSERR;
	}

	while (mnt_context_next_mount(cxt, itr, &fs, &mntrc, &ignored) == 0) {

		const char *tgt = mnt_fs_get_target(fs);

		if (ignored) {
			if (mnt_context_is_verbose(cxt))
				printf(ignored == 1 ? _("%-25s: ignored\n") :
						      _("%-25s: already mounted\n"),
						tgt);
		} else if (mnt_context_is_fork(cxt)) {
			if (mnt_context_is_verbose(cxt))
				printf("%-25s: mount successfully forked\n", tgt);
		} else {
			if (mk_exit_code(cxt, mntrc) == MNT_EX_SUCCESS) {
				nsucc++;

				/* Note that MNT_EX_SUCCESS return code does
				 * not mean that FS has been really mounted
				 * (e.g. nofail option) */
				if (mnt_context_get_status(cxt)
				    && mnt_context_is_verbose(cxt))
					printf("%-25s: successfully mounted\n", tgt);
			} else
				nerrs++;
		}
	}

	if (mnt_context_is_parent(cxt)) {
		/* wait for mount --fork children */
		int nchildren = 0;

		nerrs = 0, nsucc = 0;

		rc = mnt_context_wait_for_children(cxt, &nchildren, &nerrs);
		if (!rc && nchildren)
			nsucc = nchildren - nerrs;
	}

	if (nerrs == 0)
		rc = MNT_EX_SUCCESS;		/* all success */
	else if (nsucc == 0)
		rc = MNT_EX_FAIL;		/* all failed */
	else
		rc = MNT_EX_SOMEOK;		/* some success, some failed */

	mnt_free_iter(itr);
	return rc;
}


/*
 * mount -a -o remount
 */
static int remount_all(struct libmnt_context *cxt)
{
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;
	int mntrc, ignored, rc = MNT_EX_SUCCESS;

	int nsucc = 0, nerrs = 0;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr) {
		warn(_("failed to initialize libmount iterator"));
		return MNT_EX_SYSERR;
	}

	while (mnt_context_next_remount(cxt, itr, &fs, &mntrc, &ignored) == 0) {

		const char *tgt = mnt_fs_get_target(fs);

		if (ignored) {
			if (mnt_context_is_verbose(cxt))
				printf(_("%-25s: ignored\n"), tgt);
		} else {
			if (mk_exit_code(cxt, mntrc) == MNT_EX_SUCCESS) {
				nsucc++;

				/* Note that MNT_EX_SUCCESS return code does
				 * not mean that FS has been really mounted
				 * (e.g. nofail option) */
				if (mnt_context_get_status(cxt)
				    && mnt_context_is_verbose(cxt))
					printf("%-25s: successfully remounted\n", tgt);
			} else
				nerrs++;
		}
	}

	if (nerrs == 0)
		rc = MNT_EX_SUCCESS;		/* all success */
	else if (nsucc == 0)
		rc = MNT_EX_FAIL;		/* all failed */
	else
		rc = MNT_EX_SOMEOK;		/* some success, some failed */

	mnt_free_iter(itr);
	return rc;
}

static void success_message(struct libmnt_context *cxt)
{
	unsigned long mflags = 0;
	const char *tgt, *src, *pr = program_invocation_short_name;

	if (mnt_context_helper_executed(cxt)
	    || mnt_context_get_status(cxt) != 1)
		return;

	mnt_context_get_mflags(cxt, &mflags);
	tgt = mnt_context_get_target(cxt);
	src = mnt_context_get_source(cxt);

	if (mflags & MS_MOVE)
		printf(_("%s: %s moved to %s.\n"), pr, src, tgt);
	else if (mflags & MS_BIND)
		printf(_("%s: %s bound on %s.\n"), pr, src, tgt);
	else if (mflags & MS_PROPAGATION) {
		if (src && strcmp(src, "none") != 0 && tgt)
			printf(_("%s: %s mounted on %s.\n"), pr, src, tgt);

		printf(_("%s: %s propagation flags changed.\n"), pr, tgt);
	} else
		printf(_("%s: %s mounted on %s.\n"), pr, src, tgt);
}

#if defined(HAVE_LIBSELINUX) && defined(HAVE_SECURITY_GET_INITIAL_CONTEXT)
# include <selinux/selinux.h>
# include <selinux/context.h>

static void selinux_warning(struct libmnt_context *cxt, const char *tgt)
{

	if (tgt && mnt_context_is_verbose(cxt) && is_selinux_enabled() > 0) {
		char *raw = NULL, *def = NULL;

		if (getfilecon(tgt, &raw) > 0
		    && security_get_initial_context("file", &def) == 0) {

		if (!selinux_file_context_cmp(raw, def))
			printf(_(
	"mount: %s does not contain SELinux labels.\n"
	"       You just mounted a file system that supports labels which does not\n"
	"       contain labels, onto an SELinux box. It is likely that confined\n"
	"       applications will generate AVC messages and not be allowed access to\n"
	"       this file system.  For more details see restorecon(8) and mount(8).\n"),
				tgt);
		}
		freecon(raw);
		freecon(def);
	}
}
#else
# define selinux_warning(_x, _y)
#endif


#ifdef USE_SYSTEMD
static void systemd_hint(void)
{
	static int fstab_check_done = 0;

	if (fstab_check_done == 0) {
		struct stat a, b;

		if (isatty(STDERR_FILENO) &&
		    stat(_PATH_SD_UNITSLOAD, &a) == 0 &&
		    stat(_PATH_MNTTAB, &b) == 0 &&
		    cmp_stat_mtime(&a, &b, <))
			printf(_(
	"mount: (hint) your fstab has been modified, but systemd still uses\n"
	"       the old version; use 'systemctl daemon-reload' to reload.\n"));

		fstab_check_done = 1;
	}
}
#else
# define systemd_hint()
#endif


/*
 * Returns exit status (MNT_EX_*) and/or prints error message.
 */
static int mk_exit_code(struct libmnt_context *cxt, int rc)
{
	const char *tgt;
	char buf[BUFSIZ] = { 0 };

	rc = mnt_context_get_excode(cxt, rc, buf, sizeof(buf));
	tgt = mnt_context_get_target(cxt);

	if (*buf) {
		const char *spec = tgt;
		if (!spec)
			spec = mnt_context_get_source(cxt);
		if (!spec)
			spec = "???";
		warnx("%s: %s.", spec, buf);

		if (mnt_context_syscall_called(cxt) &&
		    mnt_context_get_syscall_errno(cxt) != 0)
			fprintf(stderr, _("       dmesg(1) may have more information after failed mount system call.\n"));
	}

	if (rc == MNT_EX_SUCCESS && mnt_context_get_status(cxt) == 1) {
		selinux_warning(cxt, tgt);
	}

	systemd_hint();

	return rc;
}

static struct libmnt_table *append_fstab(struct libmnt_context *cxt,
					 struct libmnt_table *fstab,
					 const char *path)
{

	if (!fstab) {
		fstab = mnt_new_table();
		if (!fstab)
			err(MNT_EX_SYSERR, _("failed to initialize libmount table"));

		mnt_table_set_parser_errcb(fstab, table_parser_errcb);
		mnt_context_set_fstab(cxt, fstab);

		mnt_unref_table(fstab);	/* reference is handled by @cxt now */
	}

	if (mnt_table_parse_fstab(fstab, path))
		errx(MNT_EX_USAGE,_("%s: failed to parse"), path);

	return fstab;
}

/*
 * Check source and target paths -- non-root user should not be able to
 * resolve paths which are unreadable for them.
 */
static int sanitize_paths(struct libmnt_context *cxt)
{
	const char *p;
	struct libmnt_fs *fs = mnt_context_get_fs(cxt);

	if (!fs)
		return 0;

	p = mnt_fs_get_target(fs);
	if (p) {
		char *np = canonicalize_path_restricted(p);
		if (!np)
			return -EPERM;
		mnt_fs_set_target(fs, np);
		free(np);
	}

	p = mnt_fs_get_srcpath(fs);
	if (p) {
		char *np = canonicalize_path_restricted(p);
		if (!np)
			return -EPERM;
		mnt_fs_set_source(fs, np);
		free(np);
	}
	return 0;
}

static void append_option(struct libmnt_context *cxt, const char *opt, const char *arg)
{
	char *o = NULL;

	if (opt && (*opt == '=' || *opt == '\'' || *opt == '\"' || isblank(*opt)))
		errx(MNT_EX_USAGE, _("unsupported option format: %s"), opt);

	if (arg && *arg)
		xasprintf(&o, "%s=\"%s\"", opt, arg);

	if (mnt_context_append_options(cxt, o ? : opt))
		err(MNT_EX_SYSERR, _("failed to append option '%s'"), o ? : opt);

	free(o);
}

static int has_remount_flag(struct libmnt_context *cxt)
{
	unsigned long mflags = 0;

	if (mnt_context_get_mflags(cxt, &mflags))
		return 0;

	return mflags & MS_REMOUNT;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(
		" %1$s [-lhV]\n"
		" %1$s -a [options]\n"
		" %1$s [options] [--source] <source> | [--target] <directory>\n"
		" %1$s [options] <source> <directory>\n"
		" %1$s <operation> <mountpoint> [<target>]\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Mount a filesystem.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --all               mount all filesystems mentioned in fstab\n"), out);
	fputs(_(" -c, --no-canonicalize   don't canonicalize paths\n"), out);
	fputs(_(" -f, --fake              dry run; skip the mount(2) syscall\n"), out);
	fputs(_(" -F, --fork              fork off for each device (use with -a)\n"), out);
	fputs(_(" -T, --fstab <path>      alternative file to /etc/fstab\n"), out);
	fputs(_(" -i, --internal-only     don't call the mount.<type> helpers\n"), out);
	fputs(_(" -l, --show-labels       show also filesystem labels\n"), out);
	fputs(_("     --map-groups <inner>:<outer>:<count>\n"
		"                         add the specified GID map to an ID-mapped mount\n"), out);
	fputs(_("     --map-users <inner>:<outer>:<count>\n"
		"                         add the specified UID map to an ID-mapped mount\n"), out);
	fputs(_("     --map-users /proc/<pid>/ns/user\n"
		"                         specify the user namespace for an ID-mapped mount\n"), out);
	fputs(_(" -m, --mkdir[=<mode>]    alias to '-o X-mount.mkdir[=<mode>]'\n"), out);
	fputs(_(" -n, --no-mtab           don't write to /etc/mtab\n"), out);
	fputs(_("     --options-mode <mode>\n"
		"                         what to do with options loaded from fstab\n"), out);
	fputs(_("     --options-source <source>\n"
		"                         mount options source\n"), out);
	fputs(_("     --options-source-force\n"
		"                         force use of options from fstab/mtab\n"), out);
	fputs(_("     --onlyonce          check if filesystem is already mounted\n"), out);
	fputs(_(" -o, --options <list>    comma-separated list of mount options\n"), out);
	fputs(_(" -O, --test-opts <list>  limit the set of filesystems (use with -a)\n"), out);
	fputs(_(" -r, --read-only         mount the filesystem read-only (same as -o ro)\n"), out);
	fputs(_(" -t, --types <list>      limit the set of filesystem types\n"), out);
	fputs(_("     --source <src>      explicitly specifies source (path, label, uuid)\n"), out);
	fputs(_("     --target <target>   explicitly specifies mountpoint\n"), out);
	fputs(_("     --target-prefix <path>\n"
		"                         specifies path used for all mountpoints\n"), out);
	fputs(_(" -v, --verbose           say what is being done\n"), out);
	fputs(_(" -w, --rw, --read-write  mount the filesystem read-write (default)\n"), out);
	fputs(_(" -N, --namespace <ns>    perform mount in another namespace\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(25));

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Source:\n"), out);
	fputs(_(" -L, --label <label>     synonym for LABEL=<label>\n"), out);
	fputs(_(" -U, --uuid <uuid>       synonym for UUID=<uuid>\n"), out);
	fputs(_(" LABEL=<label>           specifies device by filesystem label\n"), out);
	fputs(_(" UUID=<uuid>             specifies device by filesystem UUID\n"), out);
	fputs(_(" PARTLABEL=<label>       specifies device by partition label\n"), out);
	fputs(_(" PARTUUID=<uuid>         specifies device by partition UUID\n"), out);
	fputs(_(" ID=<id>                 specifies device by udev hardware ID\n"), out);
	fputs(_(" <device>                specifies device by path\n"), out);
	fputs(_(" <directory>             mountpoint for bind mounts (see --bind/rbind)\n"), out);
	fputs(_(" <file>                  regular file for loopdev setup\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Operations:\n"), out);
	fputs(_(" -B, --bind              mount a subtree somewhere else (same as -o bind)\n"), out);
	fputs(_(" -M, --move              move a subtree to some other place\n"), out);
	fputs(_(" -R, --rbind             mount a subtree and all submounts somewhere else\n"), out);
	fputs(_(" --make-shared           mark a subtree as shared\n"), out);
	fputs(_(" --make-slave            mark a subtree as slave\n"), out);
	fputs(_(" --make-private          mark a subtree as private\n"), out);
	fputs(_(" --make-unbindable       mark a subtree as unbindable\n"), out);
	fputs(_(" --make-rshared          recursively mark a whole subtree as shared\n"), out);
	fputs(_(" --make-rslave           recursively mark a whole subtree as slave\n"), out);
	fputs(_(" --make-rprivate         recursively mark a whole subtree as private\n"), out);
	fputs(_(" --make-runbindable      recursively mark a whole subtree as unbindable\n"), out);

	fprintf(out, USAGE_MAN_TAIL("mount(8)"));

	exit(MNT_EX_SUCCESS);
}

struct flag_str {
	int value;
	char *str;
};

static int omode2mask(const char *str)
{
	size_t i;

	static const struct flag_str flags[] = {
		{ MNT_OMODE_IGNORE, "ignore" },
		{ MNT_OMODE_APPEND, "append" },
		{ MNT_OMODE_PREPEND, "prepend" },
		{ MNT_OMODE_REPLACE, "replace" },
	};

	for (i = 0; i < ARRAY_SIZE(flags); i++) {
		if (!strcmp(str, flags[i].str))
			return flags[i].value;
	}
	return -EINVAL;
}

static long osrc2mask(const char *str, size_t len)
{
	size_t i;

	static const struct flag_str flags[] = {
		{ MNT_OMODE_FSTAB, "fstab" },
		{ MNT_OMODE_MTAB, "mtab" },
		{ MNT_OMODE_NOTAB, "disable" },
	};

	for (i = 0; i < ARRAY_SIZE(flags); i++) {
		if (!strncmp(str, flags[i].str, len) && !flags[i].str[len])
			return flags[i].value;
	}
	return -EINVAL;
}

static pid_t parse_pid(const char *str)
{
	char *end;
	pid_t ret;

	errno = 0;
	ret = strtoul(str, &end, 10);

	if (ret < 0 || errno || end == str || (end && *end))
		return 0;
	return ret;
}

int main(int argc, char **argv)
{
	int c, rc = MNT_EX_SUCCESS, all = 0, show_labels = 0;
	struct libmnt_context *cxt;
	struct libmnt_table *fstab = NULL;
	char *idmap = NULL;
	char *srcbuf = NULL;
	char *types = NULL;
	int oper = 0, is_move = 0;
	int propa = 0;
	int optmode = 0, optmode_mode = 0, optmode_src = 0;

	enum {
		MOUNT_OPT_SHARED = CHAR_MAX + 1,
		MOUNT_OPT_SLAVE,
		MOUNT_OPT_PRIVATE,
		MOUNT_OPT_UNBINDABLE,
		MOUNT_OPT_RSHARED,
		MOUNT_OPT_RSLAVE,
		MOUNT_OPT_RPRIVATE,
		MOUNT_OPT_RUNBINDABLE,
		MOUNT_OPT_MAP_GROUPS,
		MOUNT_OPT_MAP_USERS,
		MOUNT_OPT_TARGET,
		MOUNT_OPT_TARGET_PREFIX,
		MOUNT_OPT_SOURCE,
		MOUNT_OPT_OPTMODE,
		MOUNT_OPT_OPTSRC,
		MOUNT_OPT_OPTSRC_FORCE,
		MOUNT_OPT_ONLYONCE
	};

	static const struct option longopts[] = {
		{ "all",              no_argument,       NULL, 'a'                   },
		{ "fake",             no_argument,       NULL, 'f'                   },
		{ "fstab",            required_argument, NULL, 'T'                   },
		{ "fork",             no_argument,       NULL, 'F'                   },
		{ "help",             no_argument,       NULL, 'h'                   },
		{ "no-mtab",          no_argument,       NULL, 'n'                   },
		{ "read-only",        no_argument,       NULL, 'r'                   },
		{ "ro",               no_argument,       NULL, 'r'                   },
		{ "verbose",          no_argument,       NULL, 'v'                   },
		{ "version",          no_argument,       NULL, 'V'                   },
		{ "read-write",       no_argument,       NULL, 'w'                   },
		{ "rw",               no_argument,       NULL, 'w'                   },
		{ "options",          required_argument, NULL, 'o'                   },
		{ "test-opts",        required_argument, NULL, 'O'                   },
		{ "types",            required_argument, NULL, 't'                   },
		{ "uuid",             required_argument, NULL, 'U'                   },
		{ "label",            required_argument, NULL, 'L'                   },
		{ "bind",             no_argument,       NULL, 'B'                   },
		{ "move",             no_argument,       NULL, 'M'                   },
		{ "rbind",            no_argument,       NULL, 'R'                   },
		{ "make-shared",      no_argument,       NULL, MOUNT_OPT_SHARED      },
		{ "make-slave",       no_argument,       NULL, MOUNT_OPT_SLAVE       },
		{ "make-private",     no_argument,       NULL, MOUNT_OPT_PRIVATE     },
		{ "make-unbindable",  no_argument,       NULL, MOUNT_OPT_UNBINDABLE  },
		{ "make-rshared",     no_argument,       NULL, MOUNT_OPT_RSHARED     },
		{ "make-rslave",      no_argument,       NULL, MOUNT_OPT_RSLAVE      },
		{ "make-rprivate",    no_argument,       NULL, MOUNT_OPT_RPRIVATE    },
		{ "make-runbindable", no_argument,       NULL, MOUNT_OPT_RUNBINDABLE },
		{ "map-groups",       required_argument, NULL, MOUNT_OPT_MAP_GROUPS  },
		{ "map-users",        required_argument, NULL, MOUNT_OPT_MAP_USERS   },
		{ "mkdir",            optional_argument, NULL, 'm'                   },
		{ "no-canonicalize",  no_argument,       NULL, 'c'                   },
		{ "internal-only",    no_argument,       NULL, 'i'                   },
		{ "show-labels",      no_argument,       NULL, 'l'                   },
		{ "target",           required_argument, NULL, MOUNT_OPT_TARGET      },
		{ "target-prefix",    required_argument, NULL, MOUNT_OPT_TARGET_PREFIX },
		{ "source",           required_argument, NULL, MOUNT_OPT_SOURCE      },
		{ "onlyonce",         no_argument,       NULL, MOUNT_OPT_ONLYONCE    },
		{ "options-mode",     required_argument, NULL, MOUNT_OPT_OPTMODE     },
		{ "options-source",   required_argument, NULL, MOUNT_OPT_OPTSRC      },
		{ "options-source-force",   no_argument, NULL, MOUNT_OPT_OPTSRC_FORCE},
		{ "namespace",        required_argument, NULL, 'N'                   },
		{ NULL, 0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'B','M','R' },			/* bind,move,rbind */
		{ 'L','U', MOUNT_OPT_SOURCE },	/* label,uuid,source */
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	__sanitize_env(&envs_removed);
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	strutils_set_exitcode(MNT_EX_USAGE);

	mnt_init_debug(0);
	cxt = mnt_new_context();
	if (!cxt)
		err(MNT_EX_SYSERR, _("libmount context allocation failed"));

	mnt_context_set_tables_errcb(cxt, table_parser_errcb);

	while ((c = getopt_long(argc, argv, "aBcfFhilL:m::Mno:O:rRsU:vVwt:T:N:",
					longopts, NULL)) != -1) {

		/* only few options are allowed for non-root users */
		if (mnt_context_is_restricted(cxt) &&
		    !strchr("hlLUVvrist", c) &&
		    c != MOUNT_OPT_TARGET &&
		    c != MOUNT_OPT_SOURCE)
			suid_drop(cxt);

		err_exclusive_options(c, longopts, excl, excl_st);

		switch(c) {
		case 'a':
			all = 1;
			break;
		case 'c':
			mnt_context_disable_canonicalize(cxt, TRUE);
			break;
		case 'f':
			mnt_context_enable_fake(cxt, TRUE);
			break;
		case 'F':
			mnt_context_enable_fork(cxt, TRUE);
			break;
		case 'i':
			mnt_context_disable_helpers(cxt, TRUE);
			break;
		case 'n':
			mnt_context_disable_mtab(cxt, TRUE);
			break;
		case 'r':
			append_option(cxt, "ro", NULL);
			mnt_context_enable_rwonly_mount(cxt, FALSE);
			break;
		case 'v':
			mnt_context_enable_verbose(cxt, TRUE);
			break;
		case 'w':
			append_option(cxt, "rw", NULL);
			mnt_context_enable_rwonly_mount(cxt, TRUE);
			break;
		case 'o':
			/* "move" is not supported as option string in libmount
			 * to avoid use in fstab */
			if (mnt_optstr_get_option(optarg, "move", NULL, 0) == 0) {
				char *o = xstrdup(optarg);

				mnt_optstr_remove_option(&o, "move");
				if (o && *o)
					append_option(cxt, o, NULL);
				oper = is_move = 1;
				free(o);
			} else
				append_option(cxt, optarg, NULL);
			break;
		case 'O':
			if (mnt_context_set_options_pattern(cxt, optarg))
				err(MNT_EX_SYSERR, _("failed to set options pattern"));
			break;
		case 'L':
			xasprintf(&srcbuf, "LABEL=\"%s\"", optarg);
			mnt_context_disable_swapmatch(cxt, 1);
			mnt_context_set_source(cxt, srcbuf);
			free(srcbuf);
			break;
		case 'U':
			xasprintf(&srcbuf, "UUID=\"%s\"", optarg);
			mnt_context_disable_swapmatch(cxt, 1);
			mnt_context_set_source(cxt, srcbuf);
			free(srcbuf);
			break;
		case 'l':
			show_labels = 1;
			break;
		case 't':
			types = optarg;
			break;
		case 'T':
			fstab = append_fstab(cxt, fstab, optarg);
			break;
		case 's':
			mnt_context_enable_sloppy(cxt, TRUE);
			break;
		case 'B':
			oper = 1;
			append_option(cxt, "bind", NULL);
			break;
		case 'M':
			oper = 1;
			is_move = 1;
			break;
		case 'm':
			if (optarg && *optarg == '=')
				optarg++;
			append_option(cxt, "X-mount.mkdir", optarg);
			break;
		case 'R':
			oper = 1;
			append_option(cxt, "rbind", NULL);
			break;
		case 'N':
		{
			char path[PATH_MAX];
			pid_t pid = parse_pid(optarg);

			if (pid)
				snprintf(path, sizeof(path), "/proc/%i/ns/mnt", pid);

			if (mnt_context_set_target_ns(cxt, pid ? path : optarg))
				err(MNT_EX_SYSERR, _("failed to set target namespace to %s"), pid ? path : optarg);
			break;
		}
		case MOUNT_OPT_SHARED:
			append_option(cxt, "shared", NULL);
			propa = 1;
			break;
		case MOUNT_OPT_SLAVE:
			append_option(cxt, "slave", NULL);
			propa = 1;
			break;
		case MOUNT_OPT_PRIVATE:
			append_option(cxt, "private", NULL);
			propa = 1;
			break;
		case MOUNT_OPT_UNBINDABLE:
			append_option(cxt, "unbindable", NULL);
			propa = 1;
			break;
		case MOUNT_OPT_RSHARED:
			append_option(cxt, "rshared", NULL);
			propa = 1;
			break;
		case MOUNT_OPT_RSLAVE:
			append_option(cxt, "rslave", NULL);
			propa = 1;
			break;
		case MOUNT_OPT_RPRIVATE:
			append_option(cxt, "rprivate", NULL);
			propa = 1;
			break;
		case MOUNT_OPT_RUNBINDABLE:
			append_option(cxt, "runbindable", NULL);
			propa = 1;
			break;
		case MOUNT_OPT_MAP_GROUPS:
		case MOUNT_OPT_MAP_USERS:
			if (*optarg == '=')
				optarg++;
			if (idmap && (*idmap == '/' || *optarg == '/')) {
				warnx(_("bad usage"));
				errtryhelp(MNT_EX_USAGE);
			} else if (*optarg == '/') {
				idmap = xstrdup(optarg);
			} else {
				char *tmp;
				xasprintf(&tmp, "%s%s%s%s", idmap ? idmap : "", idmap ? " " : "",
					c == MOUNT_OPT_MAP_GROUPS ? "g:" : "u:", optarg);
				free(idmap);
				idmap = tmp;
			}
			break;
		case MOUNT_OPT_TARGET:
			mnt_context_disable_swapmatch(cxt, 1);
			mnt_context_set_target(cxt, optarg);
			break;
		case MOUNT_OPT_TARGET_PREFIX:
			mnt_context_set_target_prefix(cxt, optarg);
			break;
		case MOUNT_OPT_SOURCE:
			mnt_context_disable_swapmatch(cxt, 1);
			mnt_context_set_source(cxt, optarg);
			break;
		case MOUNT_OPT_OPTMODE:
			optmode_mode = omode2mask(optarg);
			if (optmode_mode == -EINVAL) {
				warnx(_("bad usage"));
				errtryhelp(MNT_EX_USAGE);
			}
			break;
		case MOUNT_OPT_OPTSRC:
		{
			unsigned long tmp = 0;
			if (string_to_bitmask(optarg, &tmp, osrc2mask)) {
				warnx(_("bad usage"));
				errtryhelp(MNT_EX_USAGE);
			}
			optmode_src = tmp;
			break;
		}
		case MOUNT_OPT_OPTSRC_FORCE:
			optmode |= MNT_OMODE_FORCE;
			break;
		case MOUNT_OPT_ONLYONCE:
			mnt_context_enable_onlyonce(cxt, 1);
			break;
		case 'h':
			mnt_free_context(cxt);
			usage();
		case 'V':
			mnt_free_context(cxt);
			mount_print_version();
		default:
			errtryhelp(MNT_EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (idmap)
		append_option(cxt, "X-mount.idmap", idmap);

	optmode |= optmode_mode | optmode_src;
	if (optmode) {
		if (!optmode_mode)
			optmode |= MNT_OMODE_PREPEND;
		if (!optmode_src)
			optmode |= MNT_OMODE_FSTAB | MNT_OMODE_MTAB;
		mnt_context_set_optsmode(cxt, optmode);
	}

	if (fstab && !mnt_context_is_nocanonicalize(cxt)) {
		/*
		 * We have external (context independent) fstab instance, let's
		 * make a connection between the fstab and the canonicalization
		 * cache.
		 */
		mnt_table_set_cache(fstab, mnt_context_get_cache(cxt));
	}

	if (!mnt_context_get_source(cxt) &&
	    !mnt_context_get_target(cxt) &&
	    !argc &&
	    !all) {
		if (oper || mnt_context_get_options(cxt)) {
			warnx(_("bad usage"));
			errtryhelp(MNT_EX_USAGE);
		}
		print_all(cxt, types, show_labels);
		goto done;
	}

	/* Non-root users are allowed to use -t to print_all(),
	   but not to mount */
	if (mnt_context_is_restricted(cxt) && types)
		suid_drop(cxt);

	if (oper && (types || all || mnt_context_get_source(cxt))) {
		warnx(_("bad usage"));
		errtryhelp(MNT_EX_USAGE);
	}

	if (types && (all || strchr(types, ',') ||
			     strncmp(types, "no", 2) == 0))
		mnt_context_set_fstype_pattern(cxt, types);
	else if (types)
		mnt_context_set_fstype(cxt, types);

	if (all) {
		/*
		 * A) Mount all
		 */
		if (has_remount_flag(cxt))
			rc = remount_all(cxt);
		else
			rc = mount_all(cxt);
		goto done;

	} else if (argc == 0 && (mnt_context_get_source(cxt) ||
				 mnt_context_get_target(cxt))) {
		/*
		 * B) mount -L|-U|--source|--target
		 *
		 * non-root may specify source *or* target, but not both
		 */
		if (mnt_context_is_restricted(cxt) &&
		    mnt_context_get_source(cxt) &&
		    mnt_context_get_target(cxt))
			suid_drop(cxt);

	} else if (argc == 1 && (!mnt_context_get_source(cxt) ||
				 !mnt_context_get_target(cxt))) {
		/*
		 * C) mount [-L|-U|--source] <target>
		 *    mount [--target <dir>] <source>
		 *    mount <source|target>
		 *
		 * non-root may specify source *or* target, but not both
		 *
		 * It does not matter for libmount if we set source or target
		 * here (the library is able to swap it), but it matters for
		 * sanitize_paths().
		 */
		int istag = mnt_tag_is_valid(argv[0]);

		if (istag && mnt_context_get_source(cxt))
			/* -L, -U or --source together with LABEL= or UUID= */
			errx(MNT_EX_USAGE, _("source specified more than once"));
		else if (istag || mnt_context_get_target(cxt))
			mnt_context_set_source(cxt, argv[0]);
		else
			mnt_context_set_target(cxt, argv[0]);

		if (mnt_context_is_restricted(cxt) &&
		    mnt_context_get_source(cxt) &&
		    mnt_context_get_target(cxt))
			suid_drop(cxt);

	} else if (argc == 2 && !mnt_context_get_source(cxt)
			     && !mnt_context_get_target(cxt)) {
		/*
		 * D) mount <source> <target>
		 */
		if (mnt_context_is_restricted(cxt))
			suid_drop(cxt);

		mnt_context_set_source(cxt, argv[0]);
		mnt_context_set_target(cxt, argv[1]);

	} else {
		warnx(_("bad usage"));
		errtryhelp(MNT_EX_USAGE);
	}

	if (mnt_context_is_restricted(cxt) && sanitize_paths(cxt) != 0)
		suid_drop(cxt);

	if (is_move)
		/* "move" as option string is not supported by libmount */
		mnt_context_set_mflags(cxt, MS_MOVE);

	if ((oper && !has_remount_flag(cxt)) || propa)
		/* For --make-* or --bind is fstab/mtab unnecessary */
		mnt_context_set_optsmode(cxt, MNT_OMODE_NOTAB);

	rc = mnt_context_mount(cxt);

	if (rc == -EPERM
	    && mnt_context_is_restricted(cxt)
	    && !mnt_context_syscall_called(cxt)) {
		/* Try it again without permissions */
		suid_drop(cxt);
		rc = mnt_context_mount(cxt);
	}
	rc = mk_exit_code(cxt, rc);

	if (rc == MNT_EX_SUCCESS && mnt_context_is_verbose(cxt))
		success_message(cxt);
done:
	mnt_free_context(cxt);
	env_list_free(envs_removed);
	return rc;
}
