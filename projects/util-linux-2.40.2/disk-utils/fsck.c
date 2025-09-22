/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * fsck --- A generic, parallelizing front-end for the fsck program.
 * It will automatically try to run fsck programs in parallel if the
 * devices are on separate spindles.  It is based on the same ideas as
 * the generic front end for fsck by David Engel and Fred van Kempen,
 * but it has been completely rewritten from scratch to support
 * parallel execution.
 *
 * Written by Theodore Ts'o, <tytso@mit.edu>
 *
 * Miquel van Smoorenburg (miquels@drinkel.ow.org) 20-Oct-1994:
 *   o Changed -t fstype to behave like with mount when -A (all file
 *     systems) or -M (like mount) is specified.
 *   o fsck looks if it can find the fsck.type program to decide
 *     if it should ignore the fs type. This way more fsck programs
 *     can be added without changing this front-end.
 *   o -R flag skip root file system.
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *	         2001, 2002, 2003, 2004, 2005 by  Theodore Ts'o.
 *
 * Copyright (C) 2009-2014 Karel Zak <kzak@redhat.com>
 */
#define _XOPEN_SOURCE 600 /* for inclusion of sa_handler in Solaris */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <paths.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <blkid.h>
#include <libmount.h>

#include "nls.h"
#include "pathnames.h"
#include "exitcodes.h"
#include "c.h"
#include "fileutils.h"
#include "monotonic.h"
#include "strutils.h"

#define XALLOC_EXIT_CODE	FSCK_EX_ERROR
#include "xalloc.h"

#define CLOSE_EXIT_CODE		FSCK_EX_ERROR
#include "closestream.h"

#ifndef DEFAULT_FSTYPE
# define DEFAULT_FSTYPE	"ext2"
#endif

#define MAX_DEVICES 32
#define MAX_ARGS 32

#define FSCK_RUNTIME_DIRNAME	"/run/fsck"

static const char *ignored_types[] = {
	"ignore",
	"iso9660",
	"sw",
	NULL
};

static const char *really_wanted[] = {
	"minix",
	"ext2",
	"ext3",
	"ext4",
	"ext4dev",
	"jfs",
	"reiserfs"
};

/*
 * Internal structure for mount table entries.
 */
struct fsck_fs_data
{
	const char	*device;
	dev_t		disk;
	unsigned int	stacked:1,
			done:1,
			eval_device:1;
};

/*
 * Structure to allow exit codes to be stored
 */
struct fsck_instance {
	int	pid;
	int	flags;		/* FLAG_{DONE|PROGRESS} */

	int	lock;		/* flock()ed lockpath file descriptor or -1 */
	char	*lockpath;	/* /run/fsck/<diskname>.lock or NULL */

	int	exit_status;
	struct timeval start_time;
	struct timeval end_time;
	char *	prog;
	char *	type;

	struct rusage rusage;
	struct libmnt_fs *fs;
	struct fsck_instance *next;
};

#define FLAG_DONE 1
#define FLAG_PROGRESS 2

/*
 * Global variables for options
 */
static char *devices[MAX_DEVICES];
static char *args[MAX_ARGS];
static int num_devices, num_args;

static int lockdisk;
static int verbose;
static int doall;
static int noexecute;
static int serialize;
static int skip_root;
static int ignore_mounted;
static int notitle;
static int parallel_root;
static int progress;
static int progress_fd;
static int force_all_parallel;
static int report_stats;
static FILE *report_stats_file;

static int num_running;
static int max_running;

static volatile sig_atomic_t cancel_requested;
static int kill_sent;
static char *fstype;
static struct fsck_instance *instance_list;

#define FSCK_DEFAULT_PATH "/sbin"
static char *fsck_path;


/* parsed fstab and mtab */
static struct libmnt_table *fstab, *mtab;
static struct libmnt_cache *mntcache;

static int count_slaves(dev_t disk);

static int string_to_int(const char *s)
{
	long l;
	char *p;

	errno = 0;
	l = strtol(s, &p, 0);
	if (errno || *p || l == LONG_MIN || l == LONG_MAX || l < 0 || l > INT_MAX)
		return -1;

	return (int) l;
}

/* Do we really really want to check this fs? */
static int fs_check_required(const char *type)
{
	size_t i;

	for(i = 0; i < ARRAY_SIZE(really_wanted); i++) {
		if (strcmp(type, really_wanted[i]) == 0)
			return 1;
	}

	return 0;
}

static int is_mounted(struct libmnt_fs *fs)
{
	int rc;
	const char *src;

	src = mnt_fs_get_source(fs);
	if (!src)
		return 0;
	if (!mntcache)
		mntcache = mnt_new_cache();
	if (!mtab) {
		mtab = mnt_new_table();
		if (!mtab)
			err(FSCK_EX_ERROR, ("failed to initialize libmount table"));
		mnt_table_set_cache(mtab, mntcache);
		mnt_table_parse_mtab(mtab, NULL);
	}

	rc = mnt_table_find_source(mtab, src, MNT_ITER_BACKWARD) ? 1 : 0;
	if (verbose) {
		if (rc)
			printf(_("%s is mounted\n"), src);
		else
			printf(_("%s is not mounted\n"), src);
	}
	return rc;
}

static int ignore(struct libmnt_fs *);

static struct fsck_fs_data *fs_create_data(struct libmnt_fs *fs)
{
	struct fsck_fs_data *data = mnt_fs_get_userdata(fs);

	if (!data) {
		data = xcalloc(1, sizeof(*data));
		mnt_fs_set_userdata(fs, data);
	}
	return data;
}

/*
 * fs from fstab might contains real device name as well as symlink,
 * LABEL or UUID, this function returns canonicalized result.
 */
static const char *fs_get_device(struct libmnt_fs *fs)
{
	struct fsck_fs_data *data = mnt_fs_get_userdata(fs);

	if (!data || !data->eval_device) {
		const char *spec = mnt_fs_get_source(fs);

		if (!data)
			data = fs_create_data(fs);

		data->eval_device = 1;
		data->device = mnt_resolve_spec(spec, mnt_table_get_cache(fstab));
		if (!data->device)
			data->device = xstrdup(spec);
	}

	return data->device;
}

static dev_t fs_get_disk(struct libmnt_fs *fs, int check)
{
	struct fsck_fs_data *data;
	const char *device;
	struct stat st;

	data = mnt_fs_get_userdata(fs);
	if (data && data->disk)
		return data->disk;

	if (!check)
		return 0;

	if (mnt_fs_is_netfs(fs) || mnt_fs_is_pseudofs(fs))
		return 0;

	device = fs_get_device(fs);
	if (!device)
		return 0;

	data = fs_create_data(fs);

	if (!stat(device, &st) &&
	    !blkid_devno_to_wholedisk(st.st_rdev, NULL, 0, &data->disk)) {

		if (data->disk)
			data->stacked = count_slaves(data->disk) > 0 ? 1 : 0;
		return data->disk;
	}
	return 0;
}

static int fs_is_stacked(struct libmnt_fs *fs)
{
	struct fsck_fs_data *data = mnt_fs_get_userdata(fs);
	return data ? data->stacked : 0;
}

static int fs_is_done(struct libmnt_fs *fs)
{
	struct fsck_fs_data *data = mnt_fs_get_userdata(fs);
	return data ? data->done : 0;
}

static void fs_set_done(struct libmnt_fs *fs)
{
	struct fsck_fs_data *data = fs_create_data(fs);

	if (data)
		data->done = 1;
}

static int is_irrotational_disk(dev_t disk)
{
	char path[PATH_MAX];
	FILE *f;
	int rc, x;


	rc = snprintf(path, sizeof(path),
			"/sys/dev/block/%d:%d/queue/rotational",
			major(disk), minor(disk));

	if (rc < 0 || (unsigned int) rc >= sizeof(path))
		return 0;

	f = fopen(path, "r");
	if (!f)
		return 0;

	rc = fscanf(f, "%d", &x);
	if (rc != 1) {
		if (ferror(f))
			warn(_("cannot read %s"), path);
		else
			warnx(_("parse error: %s"), path);
	}
	fclose(f);

	return rc == 1 ? !x : 0;
}

static void lock_disk(struct fsck_instance *inst)
{
	dev_t disk = fs_get_disk(inst->fs, 1);
	char *diskpath = NULL, *diskname;

	inst->lock = -1;

	if (!disk || is_irrotational_disk(disk))
		goto done;

	diskpath = blkid_devno_to_devname(disk);
	if (!diskpath)
		goto done;

	if (access(FSCK_RUNTIME_DIRNAME, F_OK) != 0) {
		int rc = mkdir(FSCK_RUNTIME_DIRNAME,
				    S_IWUSR|
				    S_IRUSR|S_IRGRP|S_IROTH|
				    S_IXUSR|S_IXGRP|S_IXOTH);
		if (rc && errno != EEXIST) {
			warn(_("cannot create directory %s"),
					FSCK_RUNTIME_DIRNAME);
			goto done;
		}
	}

	diskname = stripoff_last_component(diskpath);
	if (!diskname)
		diskname = diskpath;

	xasprintf(&inst->lockpath, FSCK_RUNTIME_DIRNAME "/%s.lock", diskname);

	if (verbose)
		printf(_("Locking disk by %s ... "), inst->lockpath);

	inst->lock = open(inst->lockpath, O_RDONLY|O_CREAT|O_CLOEXEC,
				    S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
	if (inst->lock >= 0) {
		int rc = -1;

		/* inform users that we're waiting on the lock */
		if (verbose &&
		    (rc = flock(inst->lock, LOCK_EX | LOCK_NB)) != 0 &&
		    errno == EWOULDBLOCK)
			printf(_("(waiting) "));

		if (rc != 0 && flock(inst->lock, LOCK_EX) != 0) {
			close(inst->lock);			/* failed */
			inst->lock = -1;
		}
	}

	if (verbose)
		/* TRANSLATORS: These are followups to "Locking disk...". */
		printf("%s.\n", inst->lock >= 0 ? _("succeeded") : _("failed"));


done:
	if (inst->lock < 0) {
		free(inst->lockpath);
		inst->lockpath = NULL;
	}
	free(diskpath);
}

static void unlock_disk(struct fsck_instance *inst)
{
	if (inst->lock < 0)
		return;

	if (verbose)
		printf(_("Unlocking %s.\n"), inst->lockpath);

	close(inst->lock);			/* unlock */

	free(inst->lockpath);

	inst->lock = -1;
	inst->lockpath = NULL;
}

static void free_instance(struct fsck_instance *i)
{
	if (lockdisk)
		unlock_disk(i);
	free(i->prog);
	free(i->lockpath);
	mnt_unref_fs(i->fs);
	free(i);
}

static struct libmnt_fs *add_dummy_fs(const char *device)
{
	struct libmnt_fs *fs = mnt_new_fs();

	if (fs && mnt_fs_set_source(fs, device) == 0 &&
				  mnt_table_add_fs(fstab, fs) == 0) {
		mnt_unref_fs(fs);
		return fs;
	}

	mnt_unref_fs(fs);
	err(FSCK_EX_ERROR, _("failed to setup description for %s"), device);
}

static void fs_interpret_type(struct libmnt_fs *fs)
{
	const char *device;
	const char *type = mnt_fs_get_fstype(fs);

	if (type && strcmp(type, "auto") != 0)
		return;

	mnt_fs_set_fstype(fs, NULL);

	device = fs_get_device(fs);
	if (device) {
		int ambi = 0;
		char *tp;
		struct libmnt_cache *cache = mnt_table_get_cache(fstab);

		tp = mnt_get_fstype(device, &ambi, cache);
		if (!ambi)
			mnt_fs_set_fstype(fs, tp);
		if (!cache)
			free(tp);
	}
}

static int parser_errcb(struct libmnt_table *tb __attribute__ ((__unused__)),
			const char *filename, int line)
{
	warnx(_("%s: parse error at line %d -- ignored"), filename, line);
	return 1;
}

/*
 * Load the filesystem database from /etc/fstab
 */
static void load_fs_info(void)
{
	const char *path;

	fstab = mnt_new_table();
	if (!fstab)
		err(FSCK_EX_ERROR, ("failed to initialize libmount table"));

	mnt_table_set_parser_errcb(fstab, parser_errcb);
	mnt_table_set_cache(fstab, mntcache);

	errno = 0;

	/*
	 * Let's follow libmount defaults if $FSTAB_FILE is not specified
	 */
	path = getenv("FSTAB_FILE");

	if (mnt_table_parse_fstab(fstab, path)) {
		if (!path)
			path = mnt_get_fstab_path();

		/* don't print error when there is no fstab at all */
		if (access(path, F_OK) == 0) {
			if (errno)
				warn(_("%s: failed to parse fstab"), path);
			else
				warnx(_("%s: failed to parse fstab"), path);
		}
	}
}

/*
 * Lookup filesys in /etc/fstab and return the corresponding entry.
 * The @path has to be real path (no TAG) by mnt_resolve_spec().
 */
static struct libmnt_fs *lookup(char *path)
{
	struct libmnt_fs *fs;

	if (!path)
		return NULL;

	fs = mnt_table_find_srcpath(fstab, path, MNT_ITER_FORWARD);
	if (!fs) {
		/*
		 * Maybe mountpoint has been specified on fsck command line.
		 * Yeah, crazy feature...
		 *
		 * Note that mnt_table_find_target() may canonicalize paths in
		 * the fstab to support symlinks. This is really unwanted,
		 * because readlink() on mountpoints may trigger automounts.
		 *
		 * So, disable the cache and compare the paths as strings
		 * without care about symlinks...
		 */
		mnt_table_set_cache(fstab, NULL);
		fs = mnt_table_find_target(fstab, path, MNT_ITER_FORWARD);
		mnt_table_set_cache(fstab, mntcache);
	}
	return fs;
}

/* Find fsck program for a given fs type. */
static int find_fsck(const char *type, char **progpath)
{
	char *s;
	const char *tpl;
	char *prog = NULL;
	char *p = xstrdup(fsck_path);
	int rc;

	/* Are we looking for a program or just a type? */
	tpl = (strncmp(type, "fsck.", 5) ? "%s/fsck.%s" : "%s/%s");

	for(s = strtok(p, ":"); s; s = strtok(NULL, ":")) {
		xasprintf(&prog, tpl, s, type);
		if (access(prog, X_OK) == 0)
			break;
		free(prog);
		prog = NULL;
	}

	free(p);
	rc = prog ? 1 : 0;

	if (progpath)
		*progpath = prog;
	else
		free(prog);

	return rc;
}

static int progress_active(void)
{
	struct fsck_instance *inst;

	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->flags & FLAG_DONE)
			continue;
		if (inst->flags & FLAG_PROGRESS)
			return 1;
	}
	return 0;
}

/*
 * Process run statistics for finished fsck instances.
 *
 * If report_stats is 0, do nothing, otherwise print a selection of
 * interesting rusage statistics as well as elapsed wallclock time.
 */
static void print_stats(struct fsck_instance *inst)
{
	struct timeval delta = { 0 };

	if (!inst || !report_stats || noexecute)
		return;

	timersub(&inst->end_time, &inst->start_time, &delta);

	if (report_stats_file)
		fprintf(report_stats_file, "%s %d %ld"
				   " %"PRId64".%06"PRId64
				   " %"PRId64".%06"PRId64
				   " %"PRId64".%06"PRId64"\n",
			fs_get_device(inst->fs),
			inst->exit_status,
			inst->rusage.ru_maxrss,
			(int64_t)delta.tv_sec, (int64_t)delta.tv_usec,
			(int64_t)inst->rusage.ru_utime.tv_sec,
			(int64_t)inst->rusage.ru_utime.tv_usec,
			(int64_t)inst->rusage.ru_stime.tv_sec,
			(int64_t)inst->rusage.ru_stime.tv_usec);
	else
		fprintf(stdout, "%s: status %d, rss %ld, "
				"real %"PRId64".%06"PRId64", "
				"user %"PRId64".%06"PRId64", "
				"sys %"PRId64".%06"PRId64"\n",
			fs_get_device(inst->fs),
			inst->exit_status,
			inst->rusage.ru_maxrss,
			(int64_t)delta.tv_sec, (int64_t)delta.tv_usec,
			(int64_t)inst->rusage.ru_utime.tv_sec,
			(int64_t)inst->rusage.ru_utime.tv_usec,
			(int64_t)inst->rusage.ru_stime.tv_sec,
			(int64_t)inst->rusage.ru_stime.tv_usec);
}

/*
 * Execute a particular fsck program, and link it into the list of
 * child processes we are waiting for.
 */
static int execute(const char *progname, const char *progpath,
		   const char *type, struct libmnt_fs *fs, int interactive)
{
	char *argv[80];
	int  argc, i;
	struct fsck_instance *inst, *p;
	pid_t	pid;

	inst = xcalloc(1, sizeof(*inst));

	argv[0] = xstrdup(progname);
	argc = 1;

	for (i=0; i <num_args; i++)
		argv[argc++] = xstrdup(args[i]);

	if (progress &&
	       ((strcmp(type, "ext2") == 0) ||
		(strcmp(type, "ext3") == 0) ||
		(strcmp(type, "ext4") == 0) ||
		(strcmp(type, "ext4dev") == 0))) {

		char tmp[80];
		tmp[0] = 0;
		if (!progress_active()) {
			snprintf(tmp, 80, "-C%d", progress_fd);
			inst->flags |= FLAG_PROGRESS;
		} else if (progress_fd)
			snprintf(tmp, 80, "-C%d", progress_fd * -1);
		if (tmp[0])
			argv[argc++] = xstrdup(tmp);
	}

	argv[argc++] = xstrdup(fs_get_device(fs));
	argv[argc] = NULL;

	if (verbose || noexecute) {
		const char *tgt = mnt_fs_get_target(fs);

		if (!tgt)
			tgt = fs_get_device(fs);
		printf("[%s (%d) -- %s] ", progpath, num_running, tgt);
		for (i=0; i < argc; i++)
			printf("%s ", argv[i]);
		printf("\n");
	}

	mnt_ref_fs(fs);
	inst->fs = fs;
	inst->lock = -1;

	if (lockdisk)
		lock_disk(inst);

	/* Fork and execute the correct program. */
	if (noexecute)
		pid = -1;
	else if ((pid = fork()) < 0) {
		warn(_("fork failed"));
		free_instance(inst);
		return errno;
	} else if (pid == 0) {
		if (!interactive)
			close(0);
		execv(progpath, argv);
		err(FSCK_EX_ERROR, _("%s: execute failed"), progpath);
	}

	for (i=0; i < argc; i++)
		free(argv[i]);

	inst->pid = pid;
	inst->prog = xstrdup(progname);
	inst->type = xstrdup(type);
	gettime_monotonic(&inst->start_time);
	inst->next = NULL;

	/*
	 * Find the end of the list, so we add the instance on at the end.
	 */
	for (p = instance_list; p && p->next; p = p->next);

	if (p)
		p->next = inst;
	else
		instance_list = inst;

	return 0;
}

/*
 * Send a signal to all outstanding fsck child processes
 */
static int kill_all(int signum)
{
	struct fsck_instance *inst;
	int	n = 0;

	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->flags & FLAG_DONE)
			continue;
		if (inst->pid <= 0)
			continue;
		kill(inst->pid, signum);
		n++;
	}
	return n;
}

/*
 * Wait for one child process to exit; when it does, unlink it from
 * the list of executing child processes, and return it.
 */
static struct fsck_instance *wait_one(int flags)
{
	int	status = 0;
	int	sig;
	struct fsck_instance *inst, *inst2, *prev;
	pid_t	pid;
	struct rusage rusage;

	if (!instance_list)
		return NULL;

	if (noexecute) {
		inst = instance_list;
		prev = NULL;
#ifdef RANDOM_DEBUG
		while (inst->next && (random() & 1)) {
			prev = inst;
			inst = inst->next;
		}
#endif
		inst->exit_status = 0;
		goto ret_inst;
	}

	/*
	 * gcc -Wall fails saving throw against stupidity
	 * (inst and prev are thought to be uninitialized variables)
	 */
	inst = prev = NULL;

	do {
		pid = wait4(-1, &status, flags, &rusage);
		if (cancel_requested && !kill_sent) {
			kill_all(SIGTERM);
			kill_sent++;
		}
		if ((pid == 0) && (flags & WNOHANG))
			return NULL;
		if (pid < 0) {
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;
			if (errno == ECHILD) {
				warnx(_("wait: no more child process?!?"));
				return NULL;
			}
			warn(_("waitpid failed"));
			continue;
		}
		for (prev = NULL, inst = instance_list;
		     inst;
		     prev = inst, inst = inst->next) {
			if (inst->pid == pid)
				break;
		}
	} while (!inst);

	if (WIFEXITED(status))
		status = WEXITSTATUS(status);
	else if (WIFSIGNALED(status)) {
		sig = WTERMSIG(status);
		if (sig == SIGINT) {
			status = FSCK_EX_UNCORRECTED;
		} else {
			warnx(_("Warning... %s for device %s exited "
			       "with signal %d."),
			       inst->prog, fs_get_device(inst->fs), sig);
			status = FSCK_EX_ERROR;
		}
	} else {
		warnx(_("%s %s: status is %x, should never happen."),
		       inst->prog, fs_get_device(inst->fs), status);
		status = FSCK_EX_ERROR;
	}

	inst->exit_status = status;
	inst->flags |= FLAG_DONE;
	gettime_monotonic(&inst->end_time);
	memcpy(&inst->rusage, &rusage, sizeof(struct rusage));

	if (progress && (inst->flags & FLAG_PROGRESS) &&
	    !progress_active()) {
		for (inst2 = instance_list; inst2; inst2 = inst2->next) {
			if (inst2->flags & FLAG_DONE)
				continue;
			if (strcmp(inst2->type, "ext2") != 0 &&
			    strcmp(inst2->type, "ext3") &&
			    strcmp(inst2->type, "ext4") != 0 &&
			    strcmp(inst2->type, "ext4dev") != 0)
				continue;
			/*
			 * If we've just started the fsck, wait a tiny
			 * bit before sending the kill, to give it
			 * time to set up the signal handler
			 */
			if (inst2->start_time.tv_sec < time(NULL) + 2) {
				if (fork() == 0) {
					sleep(1);
					kill(inst2->pid, SIGUSR1);
					exit(FSCK_EX_OK);
				}
			} else
				kill(inst2->pid, SIGUSR1);
			inst2->flags |= FLAG_PROGRESS;
			break;
		}
	}
ret_inst:
	if (prev)
		prev->next = inst->next;
	else
		instance_list = inst->next;

	print_stats(inst);

	if (verbose > 1)
		printf(_("Finished with %s (exit status %d)\n"),
		       fs_get_device(inst->fs), inst->exit_status);
	num_running--;
	return inst;
}

#define FLAG_WAIT_ALL		0
#define FLAG_WAIT_ATLEAST_ONE	1
/*
 * Wait until all executing child processes have exited; return the
 * logical OR of all of their exit code values.
 */
static int wait_many(int flags)
{
	struct fsck_instance *inst;
	int	global_status = 0;
	int	wait_flags = 0;

	while ((inst = wait_one(wait_flags))) {
		global_status |= inst->exit_status;
		free_instance(inst);
#ifdef RANDOM_DEBUG
		if (noexecute && (flags & WNOHANG) && !(random() % 3))
			break;
#endif
		if (flags & FLAG_WAIT_ATLEAST_ONE)
			wait_flags = WNOHANG;
	}
	return global_status;
}

/*
 * Run the fsck program on a particular device
 *
 * If the type is specified using -t, and it isn't prefixed with "no"
 * (as in "noext2") and only one filesystem type is specified, then
 * use that type regardless of what is specified in /etc/fstab.
 *
 * If the type isn't specified by the user, then use either the type
 * specified in /etc/fstab, or DEFAULT_FSTYPE.
 */
static int fsck_device(struct libmnt_fs *fs, int interactive)
{
	char *progname, *progpath;
	const char *type;
	int retval;

	fs_interpret_type(fs);

	type = mnt_fs_get_fstype(fs);

	if (type && strcmp(type, "auto") != 0)
		;
	else if (fstype && strncmp(fstype, "no", 2) != 0 &&
	    strncmp(fstype, "opts=", 5) != 0 && strncmp(fstype, "loop", 4) != 0 &&
	    !strchr(fstype, ','))
		type = fstype;
	else
		type = DEFAULT_FSTYPE;

	xasprintf(&progname, "fsck.%s", type);

	if (!find_fsck(progname, &progpath)) {
		free(progname);
		if (fs_check_required(type)) {
			retval = ENOENT;
			goto err;
		}
		return 0;
	}

	num_running++;
	retval = execute(progname, progpath, type, fs, interactive);
	free(progname);
	free(progpath);
	if (retval) {
		num_running--;
		goto err;
	}
	return 0;
err:
	warnx(_("error %d (%s) while executing fsck.%s for %s"),
			retval, strerror(errno), type, fs_get_device(fs));
	return FSCK_EX_ERROR;
}


/*
 * Deal with the fsck -t argument.
 */
static struct fs_type_compile {
	char **list;
	int *type;
	int  negate;
} fs_type_compiled;

#define FS_TYPE_NORMAL	0
#define FS_TYPE_OPT	1
#define FS_TYPE_NEGOPT	2

static void compile_fs_type(char *fs_type, struct fs_type_compile *cmp)
{
	char	*cp, *list, *s;
	int	num = 2;
	int	negate, first_negate = 1;

	if (fs_type) {
		for (cp=fs_type; *cp; cp++) {
			if (*cp == ',')
				num++;
		}
	}

	cmp->list = xcalloc(num, sizeof(char *));
	cmp->type = xcalloc(num, sizeof(int));
	cmp->negate = 0;

	if (!fs_type)
		return;

	list = xstrdup(fs_type);
	num = 0;
	s = strtok(list, ",");
	while(s) {
		negate = 0;
		if (strncmp(s, "no", 2) == 0) {
			s += 2;
			negate = 1;
		} else if (*s == '!') {
			s++;
			negate = 1;
		}
		if (strcmp(s, "loop") == 0)
			/* loop is really short-hand for opts=loop */
			goto loop_special_case;
		else if (strncmp(s, "opts=", 5) == 0) {
			s += 5;
		loop_special_case:
			cmp->type[num] = negate ? FS_TYPE_NEGOPT : FS_TYPE_OPT;
		} else {
			if (first_negate) {
				cmp->negate = negate;
				first_negate = 0;
			}
			if ((negate && !cmp->negate) ||
			    (!negate && cmp->negate)) {
				errx(FSCK_EX_USAGE,
					_("Either all or none of the filesystem types passed to -t must be prefixed\n"
					  "with 'no' or '!'."));
			}
		}

		cmp->list[num++] = xstrdup(s);
		s = strtok(NULL, ",");
	}
	free(list);
}

/*
 * This function returns true if a particular option appears in a
 * comma-delimited options list
 */
static int opt_in_list(const char *opt, const char *optlist)
{
	char	*list, *s;

	if (!optlist)
		return 0;
	list = xstrdup(optlist);

	s = strtok(list, ",");
	while(s) {
		if (strcmp(s, opt) == 0) {
			free(list);
			return 1;
		}
		s = strtok(NULL, ",");
	}
	free(list);
	return 0;
}

/* See if the filesystem matches the criteria given by the -t option */
static int fs_match(struct libmnt_fs *fs, struct fs_type_compile *cmp)
{
	int n, ret = 0, checked_type = 0;
	char *cp;

	if (cmp->list == NULL || cmp->list[0] == NULL)
		return 1;

	for (n=0; (cp = cmp->list[n]); n++) {
		switch (cmp->type[n]) {
		case FS_TYPE_NORMAL:
		{
			const char *type = mnt_fs_get_fstype(fs);

			checked_type++;
			if (type && strcmp(cp, type) == 0)
				ret = 1;
			break;
		}
		case FS_TYPE_NEGOPT:
			if (opt_in_list(cp, mnt_fs_get_options(fs)))
				return 0;
			break;
		case FS_TYPE_OPT:
			if (!opt_in_list(cp, mnt_fs_get_options(fs)))
				return 0;
			break;
		}
	}
	if (checked_type == 0)
		return 1;
	return (cmp->negate ? !ret : ret);
}

/*
 * Check if a device exists
 */
static int device_exists(const char *device)
{
	struct stat st;

	if (stat(device, &st) == -1)
		return 0;
	if (!S_ISBLK(st.st_mode))
		return 0;
	return 1;
}

static int fs_ignored_type(struct libmnt_fs *fs)
{
	const char **ip, *type;

	if (!mnt_fs_is_regularfs(fs))
		return 1;

	type = mnt_fs_get_fstype(fs);

	for(ip = ignored_types; type && *ip; ip++) {
		if (strcmp(type, *ip) == 0)
			return 1;
	}

	return 0;
}

/* Check if we should ignore this filesystem. */
static int ignore(struct libmnt_fs *fs)
{
	const char *type;

	/*
	 * If the pass number is 0, ignore it.
	 */
	if (mnt_fs_get_passno(fs) == 0)
		return 1;

	/*
	 * If this is a bind mount, ignore it.
	 */
	if (opt_in_list("bind", mnt_fs_get_options(fs))) {
		warnx(_("%s: skipping bad line in /etc/fstab: "
			"bind mount with nonzero fsck pass number"),
			mnt_fs_get_target(fs));
		return 1;
	}

	/*
	 * ignore devices that don't exist and have the "nofail" mount option
	 */
	if (!device_exists(fs_get_device(fs))) {
		if (opt_in_list("nofail", mnt_fs_get_options(fs))) {
			if (verbose)
				printf(_("%s: skipping nonexistent device\n"),
							fs_get_device(fs));
			return 1;
		}
		if (verbose)
			printf(_("%s: nonexistent device (\"nofail\" fstab "
				 "option may be used to skip this device)\n"),
				fs_get_device(fs));
	}

	fs_interpret_type(fs);

	/*
	 * If a specific fstype is specified, and it doesn't match,
	 * ignore it.
	 */
	if (!fs_match(fs, &fs_type_compiled))
		return 1;

	type = mnt_fs_get_fstype(fs);
	if (!type) {
		if (verbose)
			printf(_("%s: skipping unknown filesystem type\n"),
				fs_get_device(fs));
		return 1;
	}

	/* Are we ignoring this type? */
	if (fs_ignored_type(fs))
		return 1;



	/* See if the <fsck.fs> program is available. */
	if (!find_fsck(type, NULL)) {
		if (fs_check_required(type))
			warnx(_("cannot check %s: fsck.%s not found"),
				fs_get_device(fs), type);
		return 1;
	}

	/* We can and want to check this file system type. */
	return 0;
}

static int count_slaves(dev_t disk)
{
	DIR *dir;
	struct dirent *dp;
	char dirname[PATH_MAX];
	int count = 0;

	snprintf(dirname, sizeof(dirname),
			"/sys/dev/block/%u:%u/slaves/",
			major(disk), minor(disk));

	if (!(dir = opendir(dirname)))
		return -1;

	while ((dp = readdir(dir)) != NULL) {
#ifdef _DIRENT_HAVE_D_TYPE
		if (dp->d_type != DT_UNKNOWN && dp->d_type != DT_LNK)
			continue;
#endif
		if (dp->d_name[0] == '.' &&
		    ((dp->d_name[1] == 0) ||
		     ((dp->d_name[1] == '.') && (dp->d_name[2] == 0))))
			continue;

		count++;
	}

	closedir(dir);
	return count;
}

/*
 * Returns TRUE if a partition on the same disk is already being
 * checked.
 */
static int disk_already_active(struct libmnt_fs *fs)
{
	struct fsck_instance *inst;
	dev_t disk;

	if (force_all_parallel)
		return 0;

	if (instance_list && fs_is_stacked(instance_list->fs))
		/* any instance for a stacked device is already running */
		return 1;

	disk = fs_get_disk(fs, 1);

	/*
	 * If we don't know the base device, assume that the device is
	 * already active if there are any fsck instances running.
	 *
	 * Don't check a stacked device with any other disk too.
	 */
	if (!disk || fs_is_stacked(fs))
		return (instance_list != NULL);

	for (inst = instance_list; inst; inst = inst->next) {
		dev_t idisk = fs_get_disk(inst->fs, 0);

		if (!idisk || disk == idisk)
			return 1;
	}

	return 0;
}

/* Check all file systems, using the /etc/fstab table. */
static int check_all(void)
{
	int not_done_yet = 1;
	int passno = 1;
	int pass_done;
	int status = FSCK_EX_OK;

	struct libmnt_fs *fs;
	struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_FORWARD);

	if (!itr)
		err(FSCK_EX_ERROR, _("failed to allocate iterator"));

	/*
	 * Do an initial scan over the filesystem; mark filesystems
	 * which should be ignored as done, and resolve any "auto"
	 * filesystem types (done as a side-effect of calling ignore()).
	 */
	while (mnt_table_next_fs(fstab, itr, &fs) == 0) {
		if (ignore(fs)) {
			fs_set_done(fs);
			continue;
		}
	}

	if (verbose)
		fputs(_("Checking all file systems.\n"), stdout);

	/*
	 * Find and check the root filesystem.
	 */
	if (!parallel_root) {
		fs = mnt_table_find_target(fstab, "/", MNT_ITER_FORWARD);
		if (fs) {
			if (!skip_root &&
			    !fs_is_done(fs) &&
			    !(ignore_mounted && is_mounted(fs))) {
				status |= fsck_device(fs, 1);
				status |= wait_many(FLAG_WAIT_ALL);
				if (status > FSCK_EX_NONDESTRUCT) {
					mnt_free_iter(itr);
					return status;
				}
			}
			fs_set_done(fs);
		}
	}

	/*
	 * This is for the bone-headed user who enters the root
	 * filesystem twice.  Skip root will skip all root entries.
	 */
	if (skip_root) {
		mnt_reset_iter(itr, MNT_ITER_FORWARD);

		while(mnt_table_next_fs(fstab, itr, &fs) == 0) {
			const char *tgt = mnt_fs_get_target(fs);

			if (tgt && strcmp(tgt, "/") == 0)
				fs_set_done(fs);
		}
	}

	while (not_done_yet) {
		not_done_yet = 0;
		pass_done = 1;

		mnt_reset_iter(itr, MNT_ITER_FORWARD);

		while(mnt_table_next_fs(fstab, itr, &fs) == 0) {

			if (cancel_requested)
				break;
			if (fs_is_done(fs))
				continue;
			/*
			 * If the filesystem's pass number is higher
			 * than the current pass number, then we don't
			 * do it yet.
			 */
			if (mnt_fs_get_passno(fs) > passno) {
				not_done_yet++;
				continue;
			}
			if (ignore_mounted && is_mounted(fs)) {
				fs_set_done(fs);
				continue;
			}
			/*
			 * If a filesystem on a particular device has
			 * already been spawned, then we need to defer
			 * this to another pass.
			 */
			if (disk_already_active(fs)) {
				pass_done = 0;
				continue;
			}
			/*
			 * Spawn off the fsck process
			 */
			status |= fsck_device(fs, serialize);
			fs_set_done(fs);

			/*
			 * Only do one filesystem at a time, or if we
			 * have a limit on the number of fsck's extant
			 * at one time, apply that limit.
			 */
			if (serialize ||
			    (max_running && (num_running >= max_running))) {
				pass_done = 0;
				break;
			}
		}
		if (cancel_requested)
			break;
		if (verbose > 1)
			printf(_("--waiting-- (pass %d)\n"), passno);

		status |= wait_many(pass_done ? FLAG_WAIT_ALL :
				    FLAG_WAIT_ATLEAST_ONE);
		if (pass_done) {
			if (verbose > 1)
				printf("----------------------------------\n");
			passno++;
		} else
			not_done_yet++;
	}

	if (cancel_requested && !kill_sent) {
		kill_all(SIGTERM);
		kill_sent++;
	}

	status |= wait_many(FLAG_WAIT_ATLEAST_ONE);
	mnt_free_iter(itr);
	return status;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] -- [fs-options] [<filesystem> ...]\n"),
			 program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Check and repair a Linux filesystem.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -A         check all filesystems\n"), out);
	fputs(_(" -C [<fd>]  display progress bar; file descriptor is for GUIs\n"), out);
	fputs(_(" -l         lock the device to guarantee exclusive access\n"), out);
	fputs(_(" -M         do not check mounted filesystems\n"), out);
	fputs(_(" -N         do not execute, just show what would be done\n"), out);
	fputs(_(" -P         check filesystems in parallel, including root\n"), out);
	fputs(_(" -R         skip root filesystem; useful only with '-A'\n"), out);
	fputs(_(" -r [<fd>]  report statistics for each device checked;\n"
		"            file descriptor is for GUIs\n"), out);
	fputs(_(" -s         serialize the checking operations\n"), out);
	fputs(_(" -T         do not show the title on startup\n"), out);
	fputs(_(" -t <type>  specify filesystem types to be checked;\n"
		"            <type> is allowed to be a comma-separated list\n"), out);
	fputs(_(" -V         explain what is being done\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, " -?, --help     %s\n", USAGE_OPTSTR_HELP);
	fprintf(out, "     --version  %s\n", USAGE_OPTSTR_VERSION);
	fputs(USAGE_SEPARATOR, out);
	fputs(_("See the specific fsck.* commands for available fs-options."), out);
	fprintf(out, USAGE_MAN_TAIL("fsck(8)"));
	exit(FSCK_EX_OK);
}

static void signal_cancel(int sig __attribute__((__unused__)))
{
	cancel_requested = 1;
}

static void parse_argv(int argc, char *argv[])
{
	int	i, j;
	char	*arg, *dev, *tmp = NULL;
	char	options[128];
	int	opt = 0;
	int     opts_for_fsck = 0;
	struct sigaction	sa;
	int	report_stats_fd = -1;

	/*
	 * Set up signal action
	 */
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = signal_cancel;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	num_devices = 0;
	num_args = 0;
	instance_list = NULL;

	for (i=1; i < argc; i++) {
		arg = argv[i];
		if (!arg)
			continue;

		/* the only two longopts to satisfy UL standards */
		if (!opts_for_fsck && !strcmp(arg, "--help"))
			usage();
		if (!opts_for_fsck && !strcmp(arg, "--version"))
			print_version(FSCK_EX_OK);

		if ((arg[0] == '/' && !opts_for_fsck) || strchr(arg, '=')) {
			if (num_devices >= MAX_DEVICES)
				errx(FSCK_EX_ERROR, _("too many devices"));

			dev = mnt_resolve_spec(arg, mntcache);

			if (!dev && strchr(arg, '=')) {
				/*
				 * Check to see if we failed because
				 * /proc/partitions isn't found.
				 */
				if (access(_PATH_PROC_PARTITIONS, R_OK) < 0) {
					warn(_("cannot open %s"),
						_PATH_PROC_PARTITIONS);
					errx(FSCK_EX_ERROR, _("Is /proc mounted?"));
				}
				/*
				 * Check to see if this is because
				 * we're not running as root
				 */
				if (geteuid())
					errx(FSCK_EX_ERROR,
						_("must be root to scan for matching filesystems: %s"),
						arg);
				else
					errx(FSCK_EX_ERROR,
						_("couldn't find matching filesystem: %s"),
						arg);
			}
			devices[num_devices++] = dev ? dev : xstrdup(arg);
			continue;
		}
		if (arg[0] != '-' || opts_for_fsck) {
			if (num_args >= MAX_ARGS)
				errx(FSCK_EX_ERROR, _("too many arguments"));
			args[num_args++] = xstrdup(arg);
			continue;
		}
		for (j=1; arg[j]; j++) {
			if (opts_for_fsck) {
				options[++opt] = arg[j];
				continue;
			}
			switch (arg[j]) {
			case 'A':
				doall = 1;
				break;
			case 'C':
				progress = 1;
				if (arg[j+1]) {					/* -C<fd> */
					progress_fd = string_to_int(arg+j+1);
					if (progress_fd < 0)
						progress_fd = 0;
					else
						goto next_arg;
				} else if (i+1 < argc && *argv[i+1] != '-') {	/* -C <fd> */
					progress_fd = string_to_int(argv[i+1]);
					if (progress_fd < 0)
						progress_fd = 0;
					else {
						++i;
						goto next_arg;
					}
				}
				break;
			case 'l':
				lockdisk = 1;
				break;
			case 'V':
				verbose++;
				break;
			case 'N':
				noexecute = 1;
				break;
			case 'R':
				skip_root = 1;
				break;
			case 'T':
				notitle = 1;
				break;
			case 'M':
				ignore_mounted = 1;
				break;
			case 'P':
				parallel_root = 1;
				break;
			case 'r':
				report_stats = 1;
				if (arg[j+1]) {					/* -r<fd> */
					report_stats_fd = strtou32_or_err(arg+j+1, _("invalid argument of -r"));
					goto next_arg;
				} else if (i+1 < argc && *argv[i+1] >= '0' && *argv[i+1] <= '9') {	/* -r <fd> */
					report_stats_fd = strtou32_or_err(argv[i+1], _("invalid argument of -r"));
					++i;
					goto next_arg;
				}
				break;
			case 's':
				serialize = 1;
				break;
			case 't':
				tmp = NULL;
				if (fstype)
					errx(FSCK_EX_USAGE,
						_("option '%s' may be specified only once"), "-t");
				if (arg[j+1])
					tmp = arg+j+1;
				else if ((i+1) < argc)
					tmp = argv[++i];
				else
					errx(FSCK_EX_USAGE,
						_("option '%s' requires an argument"), "-t");
				fstype = xstrdup(tmp);
				compile_fs_type(fstype, &fs_type_compiled);
				goto next_arg;
			case '-':
				opts_for_fsck++;
				break;
			case '?':
				usage();
				break;
			default:
				options[++opt] = arg[j];
				break;
			}
		}
	next_arg:
		if (opt) {
			options[0] = '-';
			options[++opt] = '\0';
			if (num_args >= MAX_ARGS)
				errx(FSCK_EX_ERROR, _("too many arguments"));
			args[num_args++] = xstrdup(options);
			opt = 0;
		}
	}

	/* Validate the report stats file descriptor to avoid disasters */
	if (report_stats_fd >= 0) {
		report_stats_file = fdopen(report_stats_fd, "w");
		if (!report_stats_file)
			err(FSCK_EX_ERROR,
				_("invalid argument of -r: %d"),
				report_stats_fd);
	}

	if (getenv("FSCK_FORCE_ALL_PARALLEL"))
		force_all_parallel++;
	if (ul_strtos32(getenv("FSCK_MAX_INST"), &max_running, 10) != 0)
		max_running = 0;
}

int main(int argc, char *argv[])
{
	int i, status = 0;
	int interactive = 0;
	struct libmnt_fs *fs;
	const char *path = getenv("PATH");

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	setvbuf(stderr, NULL, _IONBF, BUFSIZ);

	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	strutils_set_exitcode(FSCK_EX_USAGE);
	mnt_init_debug(0);		/* init libmount debug mask */
	mntcache = mnt_new_cache();	/* no fatal error if failed */

	if (mntcache)
		/* Force libblkid to accept also filesystems with bad
		 * checksums. This feature is helpful for "fsck /dev/foo," but
		 * if it evaluates LABEL/UUIDs from fstab, then libmount may
		 * use cached data from udevd and udev accepts only properly
		 * detected filesystems.
		 */
		mnt_cache_set_sbprobe(mntcache, BLKID_SUBLKS_BADCSUM);


	parse_argv(argc, argv);

	if (!notitle)
		printf(UTIL_LINUX_VERSION);

	signal(SIGCHLD, SIG_DFL);	/* clear any inherited settings */

	load_fs_info();

	fsck_path = xstrdup(path && *path ? path : FSCK_DEFAULT_PATH);

	if ((num_devices == 1) || (serialize))
		interactive = 1;

	if (lockdisk && (doall || num_devices > 1)) {
		warnx(_("the -l option can be used with one "
				  "device only -- ignore"));
		lockdisk = 0;
	}

	/* If -A was specified ("check all"), do that! */
	if (doall)
		return check_all();

	if (num_devices == 0) {
		serialize++;
		interactive++;
		return check_all();
	}
	for (i = 0 ; i < num_devices; i++) {
		if (cancel_requested) {
			if (!kill_sent) {
				kill_all(SIGTERM);
				kill_sent++;
			}
			break;
		}
		fs = lookup(devices[i]);
		if (!fs)
			fs = add_dummy_fs(devices[i]);
		else if (fs_ignored_type(fs))
			continue;
		if (ignore_mounted && is_mounted(fs))
			continue;
		status |= fsck_device(fs, interactive);
		if (serialize ||
		    (max_running && (num_running >= max_running))) {
			struct fsck_instance *inst;

			inst = wait_one(0);
			if (inst) {
				status |= inst->exit_status;
				free_instance(inst);
			}
			if (verbose > 1)
				printf("----------------------------------\n");
		}
	}
	status |= wait_many(FLAG_WAIT_ALL);
	free(fsck_path);
	mnt_unref_cache(mntcache);
	mnt_unref_table(fstab);
	mnt_unref_table(mtab);
	return status;
}
