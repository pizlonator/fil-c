/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2009-2018 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

/**
 * SECTION: lock
 * @title: Locking
 * @short_description: locking methods for utab or another libmount files
 *
 * Since v2.39 libmount does nto support classic mtab locking. Now all is based
 * on flock only.
 *
 */
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/file.h>

#include "strutils.h"
#include "closestream.h"
#include "pathnames.h"
#include "mountP.h"
#include "monotonic.h"

/*
 * lock handler
 */
struct libmnt_lock {
	int	refcount;	/* reference counter */
	char	*lockfile;	/* path to lock file (e.g. /etc/mtab~) */
	int	lockfile_fd;	/* lock file descriptor */

	unsigned int	locked :1,	/* do we own the lock? */
			sigblock :1;	/* block signals when locked */

	sigset_t oldsigmask;
};


/**
 * mnt_new_lock:
 * @datafile: the file that should be covered by the lock
 * @id: ignored by library
 *
 * Returns: newly allocated lock handler or NULL on case of error.
 */
struct libmnt_lock *mnt_new_lock(const char *datafile, pid_t id __attribute__((__unused__)))
{
	struct libmnt_lock *ml = NULL;
	char *lo = NULL;
	size_t losz;

	if (!datafile)
		return NULL;

	losz = strlen(datafile) + sizeof(".lock");
	lo = malloc(losz);
	if (!lo)
		goto err;

	snprintf(lo, losz, "%s.lock", datafile);

	ml = calloc(1, sizeof(*ml) );
	if (!ml)
		goto err;

	ml->refcount = 1;
	ml->lockfile_fd = -1;
	ml->lockfile = lo;

	DBG(LOCKS, ul_debugobj(ml, "alloc: lockfile=%s", lo));
	return ml;
err:
	free(lo);
	free(ml);
	return NULL;
}


/**
 * mnt_free_lock:
 * @ml: struct libmnt_lock handler
 *
 * Deallocates libmnt_lock. This function does not care about reference count. Don't
 * use this function directly -- it's better to use mnt_unref_lock().
 *
 * The reference counting is supported since util-linux v2.40.
 */
void mnt_free_lock(struct libmnt_lock *ml)
{
	if (!ml)
		return;

	DBG(LOCKS, ul_debugobj(ml, "free%s [refcount=%d]",
					ml->locked ? " !!! LOCKED !!!" : "",
					ml->refcount));
	free(ml->lockfile);
	free(ml);
}

/**
 * mnt_ref_lock:
 * @ml: lock pointer
 *
 * Increments reference counter.
 *
 * Since: 2.40
 */
void mnt_ref_lock(struct libmnt_lock *ml)
{
	if (ml) {
		ml->refcount++;
		/*DBG(FS, ul_debugobj(fs, "ref=%d", ml->refcount));*/
	}
}

/**
 * mnt_unref_lock:
 * @ml: lock pointer
 *
 * De-increments reference counter, on zero the @ml is automatically
 * deallocated by mnt_free_lock).
 */
void mnt_unref_lock(struct libmnt_lock *ml)
{
	if (ml) {
		ml->refcount--;
		/*DBG(FS, ul_debugobj(fs, "unref=%d", ml->refcount));*/
		if (ml->refcount <= 0)
			mnt_free_lock(ml);
	}
}

/**
 * mnt_lock_block_signals:
 * @ml: struct libmnt_lock handler
 * @enable: TRUE/FALSE
 *
 * Block/unblock signals when the lock is locked, the signals are not blocked
 * by default.
 *
 * Returns: <0 on error, 0 on success.
 */
int mnt_lock_block_signals(struct libmnt_lock *ml, int enable)
{
	if (!ml)
		return -EINVAL;
	DBG(LOCKS, ul_debugobj(ml, "signals: %s", enable ? "BLOCKED" : "UNBLOCKED"));
	ml->sigblock = enable ? 1 : 0;
	return 0;
}

/*
 * Returns path to lockfile.
 */
static const char *mnt_lock_get_lockfile(struct libmnt_lock *ml)
{
	return ml ? ml->lockfile : NULL;
}

/*
 * Simple flocking
 */
static void unlock_simplelock(struct libmnt_lock *ml)
{
	assert(ml);

	if (ml->lockfile_fd >= 0) {
		DBG(LOCKS, ul_debugobj(ml, "%s: unflocking",
					mnt_lock_get_lockfile(ml)));
		close(ml->lockfile_fd);
	}
}

static int lock_simplelock(struct libmnt_lock *ml)
{
	const char *lfile;
	int rc;
	struct stat sb;
	const mode_t lock_mask = S_IRUSR|S_IWUSR;

	assert(ml);

	lfile = mnt_lock_get_lockfile(ml);

	DBG(LOCKS, ul_debugobj(ml, "%s: locking", lfile));

	if (ml->sigblock) {
		sigset_t sigs;
		sigemptyset(&ml->oldsigmask);
		sigfillset(&sigs);
		sigprocmask(SIG_BLOCK, &sigs, &ml->oldsigmask);
	}

	ml->lockfile_fd = open(lfile, O_RDONLY|O_CREAT|O_CLOEXEC, lock_mask);
	if (ml->lockfile_fd < 0) {
		rc = -errno;
		goto err;
	}

	rc = fstat(ml->lockfile_fd, &sb);
	if (rc < 0) {
		rc = -errno;
		goto err;
	}

	if ((sb.st_mode & lock_mask) != lock_mask) {
		rc = fchmod(ml->lockfile_fd, lock_mask);
		if (rc < 0) {
			rc = -errno;
			goto err;
		}
	}

	while (flock(ml->lockfile_fd, LOCK_EX) < 0) {
		int errsv;
		if ((errno == EAGAIN) || (errno == EINTR))
			continue;
		errsv = errno;
		close(ml->lockfile_fd);
		ml->lockfile_fd = -1;
		rc = -errsv;
		goto err;
	}
	ml->locked = 1;
	return 0;
err:
	if (ml->sigblock)
		sigprocmask(SIG_SETMASK, &ml->oldsigmask, NULL);
	return rc;
}

/**
 * mnt_lock_file
 * @ml: pointer to struct libmnt_lock instance
 *
 * Creates a lock file.
*
 * Note that when the lock is used by mnt_update_table() interface then libmount
 * uses flock() for private library file /run/mount/utab.
 *
 * Returns: 0 on success or negative number in case of error (-ETIMEOUT is case
 * of stale lock file).
 */
int mnt_lock_file(struct libmnt_lock *ml)
{
	if (!ml)
		return -EINVAL;

	return lock_simplelock(ml);
}

/**
 * mnt_unlock_file:
 * @ml: lock struct
 *
 * Unlocks the file. The function could be called independently of the
 * lock status (for example from exit(3)).
 */
void mnt_unlock_file(struct libmnt_lock *ml)
{
	if (!ml)
		return;

	DBG(LOCKS, ul_debugobj(ml, "(%d) %s", getpid(),
			ml->locked ? "unlocking" : "cleaning"));

	unlock_simplelock(ml);

	ml->locked = 0;
	ml->lockfile_fd = -1;

	if (ml->sigblock) {
		DBG(LOCKS, ul_debugobj(ml, "restoring sigmask"));
		sigprocmask(SIG_SETMASK, &ml->oldsigmask, NULL);
	}
}

#ifdef TEST_PROGRAM

static struct libmnt_lock *lock;

/*
 * read number from @filename, increment the number and
 * write the number back to the file
 */
static void increment_data(const char *filename, int verbose, int loopno)
{
	long num;
	FILE *f;
	char buf[256];

	if (!(f = fopen(filename, "r" UL_CLOEXECSTR)))
		err(EXIT_FAILURE, "%d: failed to open: %s", getpid(), filename);

	if (!fgets(buf, sizeof(buf), f))
		err(EXIT_FAILURE, "%d failed read: %s", getpid(), filename);

	fclose(f);
	num = atol(buf) + 1;

	if (!(f = fopen(filename, "w" UL_CLOEXECSTR)))
		err(EXIT_FAILURE, "%d: failed to open: %s", getpid(), filename);

	fprintf(f, "%ld", num);

	if (close_stream(f) != 0)
		err(EXIT_FAILURE, "write failed: %s", filename);

	if (verbose)
		fprintf(stderr, "%d: %s: %ld --> %ld (loop=%d)\n", getpid(),
				filename, num - 1, num, loopno);
}

static void clean_lock(void)
{
	if (!lock)
		return;
	mnt_unlock_file(lock);
	mnt_unref_lock(lock);
}

static void __attribute__((__noreturn__)) sig_handler(int sig)
{
	errx(EXIT_FAILURE, "\n%d: catch signal: %s\n", getpid(), strsignal(sig));
}

static int test_lock(struct libmnt_test *ts __attribute__((unused)),
		     int argc, char *argv[])
{
	time_t synctime = 0;
	unsigned int usecs;
	const char *datafile = NULL;
	int verbose = 0, loops = 0, l, idx = 1;

	if (argc < 3)
		return -EINVAL;

	if (strcmp(argv[idx], "--synctime") == 0) {
		synctime = (time_t) atol(argv[idx + 1]);
		idx += 2;
	}
	if (idx < argc && strcmp(argv[idx], "--verbose") == 0) {
		verbose = 1;
		idx++;
	}

	if (idx < argc)
		datafile = argv[idx++];
	if (idx < argc)
		loops = atoi(argv[idx++]);

	if (!datafile || !loops)
		return -EINVAL;

	if (verbose)
		fprintf(stderr, "%d: start: synctime=%u, datafile=%s, loops=%d\n",
			 getpid(), (int) synctime, datafile, loops);

	atexit(clean_lock);

	/* be paranoid and call exit() (=clean_lock()) for all signals */
	{
		int sig = 0;
		struct sigaction sa;

		sa.sa_handler = sig_handler;
		sa.sa_flags = 0;
		sigfillset(&sa.sa_mask);

		while (sigismember(&sa.sa_mask, ++sig) != -1 && sig != SIGCHLD)
			sigaction (sig, &sa, (struct sigaction *) 0);
	}

	/* start the test in exactly defined time */
	if (synctime) {
		struct timeval tv;

		gettimeofday(&tv, NULL);
		if (synctime && synctime - tv.tv_sec > 1) {
			usecs = ((synctime - tv.tv_sec) * 1000000UL) -
						(1000000UL - tv.tv_usec);
			xusleep(usecs);
		}
	}

	for (l = 0; l < loops; l++) {
		lock = mnt_new_lock(datafile, 0);
		if (!lock)
			return -1;

		if (mnt_lock_file(lock) != 0) {
			fprintf(stderr, "%d: failed to lock %s file\n",
					getpid(), datafile);
			return -1;
		}

		increment_data(datafile, verbose, l);

		mnt_unlock_file(lock);
		mnt_unref_lock(lock);
		lock = NULL;

		/* The mount command usually finishes after a mtab update. We
		 * simulate this via short sleep -- it's also enough to make
		 * concurrent processes happy.
		 */
		if (synctime)
			xusleep(25000);
	}

	return 0;
}

/*
 * Note that this test should be executed from a script that creates many
 * parallel processes, otherwise this test does not make sense.
 */
int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
	{ "--lock", test_lock,  " [--synctime <time_t>] [--verbose] <datafile> <loops> "
				"increment a number in datafile" },
	{ NULL }
	};

	return mnt_run_test(tss, argc, argv);
}

#endif /* TEST_PROGRAM */
