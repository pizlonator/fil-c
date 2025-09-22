/*
 * umount(8) -- mount a filesystem
 *
 * Copyright (C) 2011 Red Hat, Inc. All rights reserved.
 * Written by Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>

#include <libmount.h>

#include "nls.h"
#include "c.h"
#include "env.h"
#include "closestream.h"
#include "pathnames.h"
#include "canonicalize.h"

#define XALLOC_EXIT_CODE MNT_EX_SYSERR
#include "xalloc.h"

#define OPTUTILS_EXIT_CODE MNT_EX_USAGE
#include "optutils.h"

static int quiet;
static struct ul_env_list *envs_removed;

static int table_parser_errcb(struct libmnt_table *tb __attribute__((__unused__)),
			const char *filename, int line)
{
	if (filename)
		warnx(_("%s: parse error at line %d -- ignored"), filename, line);
	return 1;
}


static void __attribute__((__noreturn__)) umount_print_version(void)
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
static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(
		" %1$s [-hV]\n"
		" %1$s -a [options]\n"
		" %1$s [options] <source> | <directory>\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Unmount filesystems.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --all               unmount all filesystems\n"), out);
	fputs(_(" -A, --all-targets       unmount all mountpoints for the given device in the\n"
	        "                           current namespace\n"), out);
	fputs(_(" -c, --no-canonicalize   don't canonicalize paths\n"), out);
	fputs(_(" -d, --detach-loop       if mounted loop device, also free this loop device\n"), out);
	fputs(_("     --fake              dry run; skip the umount(2) syscall\n"), out);
	fputs(_(" -f, --force             force unmount (in case of an unreachable NFS system)\n"), out);
	fputs(_(" -i, --internal-only     don't call the umount.<type> helpers\n"), out);
	fputs(_(" -n, --no-mtab           don't write to /etc/mtab\n"), out);
	fputs(_(" -l, --lazy              detach the filesystem now, clean up things later\n"), out);
	fputs(_(" -O, --test-opts <list>  limit the set of filesystems (use with -a)\n"), out);
	fputs(_(" -R, --recursive         recursively unmount a target with all its children\n"), out);
	fputs(_(" -r, --read-only         in case unmounting fails, try to remount read-only\n"), out);
	fputs(_(" -t, --types <list>      limit the set of filesystem types\n"), out);
	fputs(_(" -v, --verbose           say what is being done\n"), out);
	fputs(_(" -q, --quiet             suppress 'not mounted' error messages\n"), out);
	fputs(_(" -N, --namespace <ns>    perform umount in another namespace\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(25));
	fprintf(out, USAGE_MAN_TAIL("umount(8)"));

	exit(MNT_EX_SUCCESS);
}

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

static void success_message(struct libmnt_context *cxt)
{
	const char *tgt, *src;

	if (mnt_context_helper_executed(cxt)
	    || mnt_context_get_status(cxt) != 1)
		return;

	tgt = mnt_context_get_target(cxt);
	if (!tgt)
		return;

	src = mnt_context_get_source(cxt);
	if (src)
		warnx(_("%s (%s) unmounted"), tgt, src);
	else
		warnx(_("%s unmounted"), tgt);
}

static int mk_exit_code(struct libmnt_context *cxt, int api_rc)
{
	char buf[BUFSIZ] = { 0 };
	int rc;

	rc = mnt_context_get_excode(cxt, api_rc, buf, sizeof(buf));

	/* suppress "not mounted" error message */
	if (quiet) {
		switch (rc) {
		case MNT_EX_USAGE:
			if (api_rc == -EPERM)	/* non-root user */
				return rc;
			break;
		case MNT_EX_FAIL:
			if (mnt_context_syscall_called(cxt) &&
			    mnt_context_get_syscall_errno(cxt) == EINVAL)
				return rc;
			break;
		}
	}

	/* print errors/warnings */
	if (*buf) {
		const char *spec = mnt_context_get_target(cxt);
		if (!spec)
			spec = mnt_context_get_source(cxt);
		if (!spec)
			spec = "???";
		warnx("%s: %s.", spec, buf);
	}
	return rc;
}

static int umount_all(struct libmnt_context *cxt)
{
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;
	int mntrc, ignored, rc = 0;

	itr = mnt_new_iter(MNT_ITER_BACKWARD);
	if (!itr) {
		warn(_("failed to initialize libmount iterator"));
		return MNT_EX_SYSERR;
	}

	while (mnt_context_next_umount(cxt, itr, &fs, &mntrc, &ignored) == 0) {

		const char *tgt = mnt_fs_get_target(fs);

		if (ignored) {
			if (mnt_context_is_verbose(cxt))
				printf(_("%-25s: ignored\n"), tgt);
		} else {
			int xrc = mk_exit_code(cxt, mntrc);

			if (xrc == MNT_EX_SUCCESS
			    && mnt_context_is_verbose(cxt))
				printf("%-25s: successfully unmounted\n", tgt);
			rc |= xrc;
		}
	}

	mnt_free_iter(itr);
	return rc;
}

static int umount_one(struct libmnt_context *cxt, const char *spec)
{
	int rc;

	if (!spec)
		return MNT_EX_SOFTWARE;

	if (mnt_context_set_target(cxt, spec))
		err(MNT_EX_SYSERR, _("failed to set umount target"));

	rc = mnt_context_umount(cxt);

	if (rc == -EPERM
	    && mnt_context_is_restricted(cxt)
	    && mnt_context_tab_applied(cxt)
	    && !mnt_context_syscall_called(cxt)) {
		/* Mountpoint exists, but failed something else in libmount,
		 * drop perms and try it again */
		suid_drop(cxt);
		rc = mnt_context_umount(cxt);
	}

	rc = mk_exit_code(cxt, rc);

	if (rc == MNT_EX_SUCCESS && mnt_context_is_verbose(cxt))
		success_message(cxt);

	mnt_reset_context(cxt);
	return rc;
}

static struct libmnt_table *new_mountinfo(struct libmnt_context *cxt)
{
	struct libmnt_table *tb;
	struct libmnt_ns *ns_old = mnt_context_switch_target_ns(cxt);

	if (!ns_old)
		err(MNT_EX_SYSERR, _("failed to switch namespace"));

	tb = mnt_new_table();
	if (!tb)
		err(MNT_EX_SYSERR, _("libmount table allocation failed"));

	mnt_table_set_parser_errcb(tb, table_parser_errcb);
	mnt_table_set_cache(tb, mnt_context_get_cache(cxt));

	if (mnt_table_parse_file(tb, _PATH_PROC_MOUNTINFO)) {
		warn(_("failed to parse %s"), _PATH_PROC_MOUNTINFO);
		mnt_unref_table(tb);
		tb = NULL;
	}

	if (!mnt_context_switch_ns(cxt, ns_old))
		err(MNT_EX_SYSERR, _("failed to switch namespace"));

	return tb;
}

/*
 * like umount_one() but does not return error is @spec not mounted
 */
static int umount_one_if_mounted(struct libmnt_context *cxt, const char *spec)
{
	int rc;
	struct libmnt_fs *fs;

	rc = mnt_context_find_umount_fs(cxt, spec, &fs);
	if (rc == 1) {
		rc = MNT_EX_SUCCESS;		/* already unmounted */
		mnt_reset_context(cxt);
	} else if (rc < 0) {
		rc = mk_exit_code(cxt, rc);	/* error */
		mnt_reset_context(cxt);
	} else
		rc = umount_one(cxt, mnt_fs_get_target(fs));

	return rc;
}

static int umount_do_recurse(struct libmnt_context *cxt,
		struct libmnt_table *tb, struct libmnt_fs *fs)
{
	struct libmnt_fs *child, *over = NULL;
	struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_BACKWARD);
	int rc;

	if (!itr)
		err(MNT_EX_SYSERR, _("libmount iterator allocation failed"));

	/* first try overmount */
	if (mnt_table_over_fs(tb, fs, &over) == 0 && over) {
		rc = umount_do_recurse(cxt, tb, over);
		if (rc != MNT_EX_SUCCESS)
			goto done;
	}

	/* umount all children */
	for (;;) {
		rc = mnt_table_next_child_fs(tb, itr, fs, &child);
		if (rc < 0) {
			warnx(_("failed to get child fs of %s"),
					mnt_fs_get_target(fs));
			rc = MNT_EX_SOFTWARE;
			goto done;
		} else if (rc == 1)
			break;		/* no more children */

		if (over && child == over)
			continue;

		rc = umount_do_recurse(cxt, tb, child);
		if (rc != MNT_EX_SUCCESS)
			goto done;
	}

	rc = umount_one_if_mounted(cxt, mnt_fs_get_target(fs));
done:
	mnt_free_iter(itr);
	return rc;
}

static int umount_recursive(struct libmnt_context *cxt, const char *spec)
{
	struct libmnt_table *tb;
	struct libmnt_fs *fs;
	int rc;

	tb = new_mountinfo(cxt);
	if (!tb)
		return MNT_EX_SOFTWARE;

	/* it's always real mountpoint, don't assume that the target maybe a device */
	mnt_context_disable_swapmatch(cxt, 1);

	fs = mnt_table_find_target(tb, spec, MNT_ITER_FORWARD);
	if (fs)
		rc = umount_do_recurse(cxt, tb, fs);
	else {
		rc = MNT_EX_USAGE;
		if (!quiet)
			warnx(access(spec, F_OK) == 0 ?
				_("%s: not mounted") :
				_("%s: not found"), spec);
	}

	mnt_unref_table(tb);
	return rc;
}

static int umount_alltargets(struct libmnt_context *cxt, const char *spec, int rec)
{
	struct libmnt_fs *fs;
	struct libmnt_table *tb;
	struct libmnt_iter *itr = NULL;
	dev_t devno = 0;
	int rc;

	/* Convert @spec to device name, Use the same logic like regular
	 * "umount <spec>".
	 */
	rc = mnt_context_find_umount_fs(cxt, spec, &fs);
	if (rc == 1) {
		rc = MNT_EX_USAGE;
		if (!quiet)
			warnx(access(spec, F_OK) == 0 ?
				_("%s: not mounted") :
				_("%s: not found"), spec);
		return rc;
	}
	if (rc < 0)
		return mk_exit_code(cxt, rc);		/* error */

	if (!mnt_fs_get_srcpath(fs) || !mnt_fs_get_devno(fs))
		errx(MNT_EX_USAGE, _("%s: failed to determine source "
				"(--all-targets is unsupported on systems with "
				"regular mtab file)."), spec);

	itr = mnt_new_iter(MNT_ITER_BACKWARD);
	if (!itr)
		err(MNT_EX_SYSERR, _("libmount iterator allocation failed"));

	/* get on @cxt independent mountinfo */
	tb = new_mountinfo(cxt);
	if (!tb) {
		rc = MNT_EX_SOFTWARE;
		goto done;
	}

	/* Note that @fs is from mount context and the context will be reset
	 * after each umount() call */
	devno = mnt_fs_get_devno(fs);
	fs = NULL;

	mnt_reset_context(cxt);

	while (mnt_table_next_fs(tb, itr, &fs) == 0) {
		if (mnt_fs_get_devno(fs) != devno)
			continue;
		mnt_context_disable_swapmatch(cxt, 1);
		if (rec)
			rc = umount_do_recurse(cxt, tb, fs);
		else
			rc = umount_one_if_mounted(cxt, mnt_fs_get_target(fs));

		if (rc != MNT_EX_SUCCESS)
			break;
	}

done:
	mnt_free_iter(itr);
	mnt_unref_table(tb);

	return rc;
}

/*
 * Check path -- non-root user should not be able to resolve path which is
 * unreadable for them.
 */
static char *sanitize_path(const char *path)
{
	char *p;

	if (!path)
		return NULL;

	p = canonicalize_path_restricted(path);
	if (!p)
		err(MNT_EX_USAGE, "%s", path);

	return p;
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
	int c, rc = 0, all = 0, recursive = 0, alltargets = 0;
	struct libmnt_context *cxt;
	char *types = NULL;

	enum {
		UMOUNT_OPT_FAKE = CHAR_MAX + 1,
	};

	static const struct option longopts[] = {
		{ "all",             no_argument,       NULL, 'a'             },
		{ "all-targets",     no_argument,       NULL, 'A'             },
		{ "detach-loop",     no_argument,       NULL, 'd'             },
		{ "fake",            no_argument,       NULL, UMOUNT_OPT_FAKE },
		{ "force",           no_argument,       NULL, 'f'             },
		{ "help",            no_argument,       NULL, 'h'             },
		{ "internal-only",   no_argument,       NULL, 'i'             },
		{ "lazy",            no_argument,       NULL, 'l'             },
		{ "no-canonicalize", no_argument,       NULL, 'c'             },
		{ "no-mtab",         no_argument,       NULL, 'n'             },
		{ "quiet",           no_argument,       NULL, 'q'             },
		{ "read-only",       no_argument,       NULL, 'r'             },
		{ "recursive",       no_argument,       NULL, 'R'             },
		{ "test-opts",       required_argument, NULL, 'O'             },
		{ "types",           required_argument, NULL, 't'             },
		{ "verbose",         no_argument,       NULL, 'v'             },
		{ "version",         no_argument,       NULL, 'V'             },
		{ "namespace",       required_argument, NULL, 'N'             },
		{ NULL, 0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'A','a' },			/* all-targets,all */
		{ 'R','a' },			/* recursive,all */
		{ 'O','R','t'},			/* options,recursive,types */
		{ 'R','r' },			/* recursive,read-only */
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	__sanitize_env(&envs_removed);
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	mnt_init_debug(0);
	cxt = mnt_new_context();
	if (!cxt)
		err(MNT_EX_SYSERR, _("libmount context allocation failed"));

	mnt_context_set_tables_errcb(cxt, table_parser_errcb);

	while ((c = getopt_long(argc, argv, "aAcdfhilnqRrO:t:vVN:",
					longopts, NULL)) != -1) {


		/* only few options are allowed for non-root users */
		if (mnt_context_is_restricted(cxt) && !strchr("hdilqVv", c)) {

			/* Silently ignore options without direct impact to the
			 * umount operation, but with security sensitive
			 * side-effects */
			if (strchr("c", c))
				continue;	/* ignore */

			/* drop permissions, continue as regular user */
			suid_drop(cxt);
		}

		err_exclusive_options(c, longopts, excl, excl_st);

		switch(c) {
		case 'a':
			all = 1;
			break;
		case 'A':
			alltargets = 1;
			break;
		case 'c':
			mnt_context_disable_canonicalize(cxt, TRUE);
			break;
		case 'd':
			mnt_context_enable_loopdel(cxt, TRUE);
			break;
		case UMOUNT_OPT_FAKE:
			mnt_context_enable_fake(cxt, TRUE);
			break;
		case 'f':
			mnt_context_enable_force(cxt, TRUE);
			break;
		case 'i':
			mnt_context_disable_helpers(cxt, TRUE);
			break;
		case 'l':
			mnt_context_enable_lazy(cxt, TRUE);
			break;
		case 'n':
			mnt_context_disable_mtab(cxt, TRUE);
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			mnt_context_enable_rdonly_umount(cxt, TRUE);
			break;
		case 'R':
			recursive = TRUE;
			break;
		case 'O':
			if (mnt_context_set_options_pattern(cxt, optarg))
				err(MNT_EX_SYSERR, _("failed to set options pattern"));
			break;
		case 't':
			types = optarg;
			break;
		case 'v':
			mnt_context_enable_verbose(cxt, TRUE);
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

		case 'h':
			mnt_free_context(cxt);
			usage();
		case 'V':
			mnt_free_context(cxt);
			umount_print_version();
		default:
			errtryhelp(MNT_EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (all) {
		if (argc) {
			warnx(_("unexpected number of arguments"));
			errtryhelp(MNT_EX_USAGE);
		}
		if (!types)
			types = "noproc,nodevfs,nodevpts,nosysfs,norpc_pipefs,nonfsd,noselinuxfs";

		mnt_context_set_fstype_pattern(cxt, types);
		rc = umount_all(cxt);

	} else if (argc < 1) {
		warnx(_("bad usage"));
		errtryhelp(MNT_EX_USAGE);

	} else if (alltargets) {
		while (argc--)
			rc += umount_alltargets(cxt, *argv++, recursive);
	} else if (recursive) {
		while (argc--)
			rc += umount_recursive(cxt, *argv++);
	} else {
		while (argc--) {
			char *path = *argv;

			if (mnt_context_is_restricted(cxt)
			    && !mnt_tag_is_valid(path))
				path = sanitize_path(path);

			rc += umount_one(cxt, path);

			if (path != *argv)
				free(path);
			argv++;
		}
	}

	mnt_free_context(cxt);
	env_list_free(envs_removed);

	return (rc < 256) ? rc : 255;
}
