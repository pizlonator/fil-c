/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2001, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include "defines.h"
#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include <utime.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>

#include "alloc.h"
#include "memzero.h"
#include "nscd.h"
#include "sssd.h"
#ifdef WITH_TCB
#include <tcb.h>
#endif				/* WITH_TCB */
#include "prototypes.h"
#include "commonio.h"
#include "shadowlog_internal.h"
#include "string/sprintf.h"


/* local function prototypes */
static int lrename (const char *, const char *);
static int check_link_count (const char *file, bool log);
static int do_lock_file (const char *file, const char *lock, bool log);
static /*@null@*/ /*@dependent@*/FILE *fopen_set_perms (
	const char *name,
	const char *mode,
	const struct stat *sb);
static int create_backup (const char *, FILE *);
static void free_linked_list (struct commonio_db *);
static void add_one_entry (
	struct commonio_db *db,
	/*@owned@*/struct commonio_entry *p);
static bool name_is_nis (const char *name);
static int write_all (const struct commonio_db *);
static /*@dependent@*/ /*@null@*/struct commonio_entry *find_entry_by_name (
	struct commonio_db *,
	const char *);
static /*@dependent@*/ /*@null@*/struct commonio_entry *next_entry_by_name (
	struct commonio_db *,
	/*@null@*/struct commonio_entry *pos,
	const char *);

static int lock_count = 0;
static bool nscd_need_reload = false;

/*
 * Simple rename(P) alternative that attempts to rename to symlink
 * target.
 */
int lrename (const char *old, const char *new)
{
	int res;
	char *r = NULL;

#ifndef __GLIBC__
	char resolved_path[PATH_MAX];
#endif				/* !__GLIBC__ */
	struct stat sb;
	if (lstat (new, &sb) == 0 && S_ISLNK (sb.st_mode)) {
#ifdef __GLIBC__ /* now a POSIX.1-2008 feature */
		r = realpath (new, NULL);
#else				/* !__GLIBC__ */
		r = realpath (new, resolved_path);
#endif				/* !__GLIBC__ */
		if (NULL == r) {
			perror ("realpath in lrename()");
		} else {
			new = r;
		}
	}

	res = rename (old, new);

#ifdef __GLIBC__
	free (r);
#endif				/* __GLIBC__ */

	return res;
}

static int check_link_count (const char *file, bool log)
{
	struct stat sb;

	if (stat (file, &sb) != 0) {
		if (log) {
			(void) fprintf (shadow_logfd,
			                "%s: %s file stat error: %s\n",
			                shadow_progname, file, strerror (errno));
		}
		return 0;
	}

	if (sb.st_nlink != 2) {
		if (log) {
			fprintf(shadow_logfd,
			        "%s: %s: lock file already used (nlink: %ju)\n",
			        shadow_progname, file, (uintmax_t) sb.st_nlink);
		}
		return 0;
	}

	return 1;
}


static int do_lock_file (const char *file, const char *lock, bool log)
{
	int      fd;
	int      retval;
	char     buf[32];
	pid_t    pid;
	ssize_t  len;

	fd = open (file, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	if (-1 == fd) {
		if (log) {
			(void) fprintf (shadow_logfd,
			                "%s: %s: %s\n",
			                shadow_progname, file, strerror (errno));
		}
		return 0;
	}

	pid = getpid ();
	SNPRINTF(buf, "%lu", (unsigned long) pid);
	len = (ssize_t) strlen (buf) + 1;
	if (write_full(fd, buf, len) == -1) {
		if (log) {
			(void) fprintf (shadow_logfd,
			                "%s: %s file write error: %s\n",
			                shadow_progname, file, strerror (errno));
		}
		(void) close (fd);
		unlink (file);
		return 0;
	}
	if (fdatasync (fd) == -1) {
		if (log) {
			(void) fprintf (shadow_logfd,
			                "%s: %s file sync error: %s\n",
			                shadow_progname, file, strerror (errno));
		}
		(void) close (fd);
		unlink (file);
		return 0;
	}
	close (fd);

	if (link (file, lock) == 0) {
		retval = check_link_count (file, log);
		unlink (file);
		return retval;
	}

	fd = open (lock, O_RDWR);
	if (-1 == fd) {
		if (log) {
			(void) fprintf (shadow_logfd,
			                "%s: %s: %s\n",
			                shadow_progname, lock, strerror (errno));
		}
		unlink (file);
		errno = EINVAL;
		return 0;
	}
	len = read (fd, buf, sizeof (buf) - 1);
	close (fd);
	if (len <= 0) {
		if (log) {
			(void) fprintf (shadow_logfd,
			                "%s: existing lock file %s without a PID\n",
			                shadow_progname, lock);
		}
		unlink (file);
		errno = EINVAL;
		return 0;
	}
	buf[len] = '\0';
	if (get_pid(buf, &pid) == -1) {
		if (log) {
			(void) fprintf (shadow_logfd,
			                "%s: existing lock file %s with an invalid PID '%s'\n",
			                shadow_progname, lock, buf);
		}
		unlink (file);
		errno = EINVAL;
		return 0;
	}
	if (kill (pid, 0) == 0) {
		if (log) {
			(void) fprintf (shadow_logfd,
			                "%s: lock %s already used by PID %lu\n",
			                shadow_progname, lock, (unsigned long) pid);
		}
		unlink (file);
		errno = EEXIST;
		return 0;
	}
	if (unlink (lock) != 0) {
		if (log) {
			(void) fprintf (shadow_logfd,
			                "%s: cannot get lock %s: %s\n",
			                shadow_progname, lock, strerror (errno));
		}
		unlink (file);
		return 0;
	}

	retval = 0;
	if (link (file, lock) == 0) {
		retval = check_link_count (file, log);
	} else {
		if (log) {
			(void) fprintf (shadow_logfd,
			                "%s: cannot get lock %s: %s\n",
			                shadow_progname, lock, strerror (errno));
		}
	}

	unlink (file);
	return retval;
}


static /*@null@*/ /*@dependent@*/FILE *fopen_set_perms (
	const char *name,
	const char *mode,
	const struct stat *sb)
{
	FILE *fp;
	mode_t mask;

	mask = umask (0777);
	fp = fopen (name, mode);
	(void) umask (mask);
	if (NULL == fp) {
		return NULL;
	}

	if (fchown (fileno (fp), sb->st_uid, sb->st_gid) != 0) {
		goto fail;
	}
	if (fchmod (fileno (fp), sb->st_mode & 0664) != 0) {
		goto fail;
	}

	return fp;

      fail:
	(void) fclose (fp);
	/* fopen_set_perms is used for intermediate files */
	(void) unlink (name);
	return NULL;
}


static int create_backup (const char *backup, FILE * fp)
{
	struct stat sb;
	struct utimbuf ub;
	FILE *bkfp;
	int c;

	if (fstat (fileno (fp), &sb) != 0) {
		return -1;
	}

	bkfp = fopen_set_perms (backup, "w", &sb);
	if (NULL == bkfp) {
		return -1;
	}

	/* TODO: faster copy, not one-char-at-a-time.  --marekm */
	c = 0;
	if (fseek (fp, 0, SEEK_SET) == 0) {
		while ((c = getc (fp)) != EOF) {
			if (putc (c, bkfp) == EOF) {
				break;
			}
		}
	}
	if ((c != EOF) || (ferror (fp) != 0) || (fflush (bkfp) != 0)) {
		(void) fclose (bkfp);
		/* FIXME: unlink the backup file? */
		return -1;
	}
	if (fsync (fileno (bkfp)) != 0) {
		(void) fclose (bkfp);
		/* FIXME: unlink the backup file? */
		return -1;
	}
	if (fclose (bkfp) != 0) {
		/* FIXME: unlink the backup file? */
		return -1;
	}

	ub.actime = sb.st_atime;
	ub.modtime = sb.st_mtime;
	(void) utime (backup, &ub);
	return 0;
}


static void free_linked_list (struct commonio_db *db)
{
	struct commonio_entry *p;

	while (NULL != db->head) {
		p = db->head;
		db->head = p->next;

		free (p->line);

		if (NULL != p->eptr) {
			db->ops->free (p->eptr);
		}

		free (p);
	}
	db->tail = NULL;
}


int commonio_setname (struct commonio_db *db, const char *name)
{
	SNPRINTF(db->filename, "%s", name);
	db->setname = true;
	return 1;
}


bool commonio_present (const struct commonio_db *db)
{
	return (access (db->filename, F_OK) == 0);
}


int commonio_lock_nowait (struct commonio_db *db, bool log)
{
	int   err = 0;
	char  *file = NULL;
	char  *lock = NULL;

	if (db->locked) {
		return 1;
	}

	if (asprintf(&file, "%s.%ju", db->filename, (uintmax_t) getpid()) == -1)
		goto cleanup_ENOMEM;
	if (asprintf(&lock, "%s.lock", db->filename) == -1)
		goto cleanup_ENOMEM;

	if (do_lock_file (file, lock, log) != 0) {
		db->locked = true;
		lock_count++;
		err = 1;
	}

cleanup_ENOMEM:
	free(file);
	free(lock);
	return err;
}


int commonio_lock (struct commonio_db *db)
{
	int i;

#ifdef HAVE_LCKPWDF
	/*
	 * Only if the system libc has a real lckpwdf() - the one from
	 * lockpw.c calls us and would cause infinite recursion!
	 * It is also not used with the prefix option.
	 */
	if (!db->setname) {
		/*
		 * Call lckpwdf() on the first lock.
		 * If it succeeds, call *_lock() only once
		 * (no retries, it should always succeed).
		 */
		if (0 == lock_count) {
			if (lckpwdf () == -1) {
				if (geteuid () != 0) {
					(void) fprintf (shadow_logfd,
					                "%s: Permission denied.\n",
					                shadow_progname);
				}
				return 0;	/* failure */
			}
		}

		if (commonio_lock_nowait (db, true) != 0) {
			return 1;	/* success */
		}

		ulckpwdf ();
		return 0;		/* failure */
	}
#endif				/* !HAVE_LCKPWDF */

	/*
	 * lckpwdf() not used - do it the old way.
	 */
#ifndef LOCK_TRIES
#define LOCK_TRIES 15
#endif

#ifndef LOCK_SLEEP
#define LOCK_SLEEP 1
#endif
	for (i = 0; i < LOCK_TRIES; i++) {
		if (i > 0) {
			sleep (LOCK_SLEEP);	/* delay between retries */
		}
		if (commonio_lock_nowait (db, i==LOCK_TRIES-1) != 0) {
			return 1;	/* success */
		}
		/* no unnecessary retries on "permission denied" errors */
		if (geteuid () != 0) {
			(void) fprintf (shadow_logfd, "%s: Permission denied.\n",
			                shadow_progname);
			return 0;
		}
	}
	return 0;		/* failure */
}

static void dec_lock_count (void)
{
	if (lock_count > 0) {
		lock_count--;
		if (lock_count == 0) {
			/* Tell nscd when lock count goes to zero,
			   if any of the files were changed.  */
			if (nscd_need_reload) {
				nscd_flush_cache ("passwd");
				nscd_flush_cache ("group");
				sssd_flush_cache (SSSD_DB_PASSWD | SSSD_DB_GROUP);
				nscd_need_reload = false;
			}
#ifdef HAVE_LCKPWDF
			ulckpwdf ();
#endif				/* HAVE_LCKPWDF */
		}
	}
}


int commonio_unlock (struct commonio_db *db)
{
	char  lock[1029];

	if (db->isopen) {
		db->readonly = true;
		if (commonio_close (db) == 0) {
			if (db->locked) {
				dec_lock_count ();
			}
			return 0;
		}
	}
	if (db->locked) {
		/*
		 * Unlock in reverse order: remove the lock file,
		 * then call ulckpwdf() (if used) on last unlock.
		 */
		db->locked = false;
		SNPRINTF(lock, "%s.lock", db->filename);
		unlink (lock);
		dec_lock_count ();
		return 1;
	}
	return 0;
}


/*
 * Add an entry at the end.
 *
 * defines p->next, p->prev
 * (unfortunately, owned special are not supported)
 */
static void add_one_entry (struct commonio_db *db,
                           /*@owned@*/struct commonio_entry *p)
{
	/*@-mustfreeonly@*/
	p->next = NULL;
	p->prev = db->tail;
	/*@=mustfreeonly@*/
	if (NULL == db->head) {
		db->head = p;
	}
	if (NULL != db->tail) {
		db->tail->next = p;
	}
	db->tail = p;
}


static bool name_is_nis (const char *name)
{
	return (('+' == name[0]) || ('-' == name[0]));
}


/*
 * New entries are inserted before the first NIS entry.  Order is preserved
 * when db is written out.
 */
#ifndef KEEP_NIS_AT_END
#define KEEP_NIS_AT_END 1
#endif

#if KEEP_NIS_AT_END
static void add_one_entry_nis (struct commonio_db *db,
                               /*@owned@*/struct commonio_entry *newp);

/*
 * Insert an entry between the regular entries, and the NIS entries.
 *
 * defines newp->next, newp->prev
 * (unfortunately, owned special are not supported)
 */
static void add_one_entry_nis (struct commonio_db *db,
                               /*@owned@*/struct commonio_entry *newp)
{
	struct commonio_entry *p;

	for (p = db->head; NULL != p; p = p->next) {
		if (name_is_nis (p->eptr ? db->ops->getname (p->eptr)
		                         : p->line)) {
			/*@-mustfreeonly@*/
			newp->next = p;
			newp->prev = p->prev;
			/*@=mustfreeonly@*/
			if (NULL != p->prev) {
				p->prev->next = newp;
			} else {
				db->head = newp;
			}
			p->prev = newp;
			return;
		}
	}
	add_one_entry (db, newp);
}
#endif				/* KEEP_NIS_AT_END */

/* Initial buffer size, as well as increment if not sufficient
   (for reading very long lines in group files).  */
#define BUFLEN 4096

int commonio_open (struct commonio_db *db, int mode)
{
	char *buf;
	char *cp;
	char *line;
	struct commonio_entry *p;
	void *eptr = NULL;
	int flags = mode;
	size_t buflen;
	int fd;
	int saved_errno;

	mode &= ~O_CREAT;

	if (   db->isopen
	    || (   (O_RDONLY != mode)
	        && (O_RDWR != mode))) {
		errno = EINVAL;
		return 0;
	}
	db->readonly = (mode == O_RDONLY);
	if (!db->readonly && !db->locked) {
		errno = EACCES;
		return 0;
	}

	db->head = NULL;
	db->tail = NULL;
	db->cursor = NULL;
	db->changed = false;

	fd = open (db->filename,
	             (db->readonly ? O_RDONLY : O_RDWR)
	           | O_NOCTTY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
	saved_errno = errno;
	db->fp = NULL;
	if (fd >= 0) {
#ifdef WITH_TCB
		if (tcb_is_suspect (fd) != 0) {
			(void) close (fd);
			errno = EINVAL;
			return 0;
		}
#endif				/* WITH_TCB */
		db->fp = fdopen (fd, db->readonly ? "r" : "r+");
		saved_errno = errno;
		if (NULL == db->fp) {
			(void) close (fd);
		}
	}
	errno = saved_errno;

	/*
	 * If O_CREAT was specified and the file didn't exist, it will be
	 * created by commonio_close().  We have no entries to read yet.  --marekm
	 */
	if (NULL == db->fp) {
		if (((flags & O_CREAT) != 0) && (ENOENT == errno)) {
			db->isopen = true;
			return 1;
		}
		return 0;
	}

	buflen = BUFLEN;
	buf = MALLOC(buflen, char);
	if (NULL == buf) {
		goto cleanup_ENOMEM;
	}

	while (db->ops->fgets (buf, buflen, db->fp) == buf) {
		while (   (strrchr (buf, '\n') == NULL)
		       && (feof (db->fp) == 0)) {
			size_t len;

			buflen += BUFLEN;
			cp = REALLOC(buf, buflen, char);
			if (NULL == cp) {
				goto cleanup_buf;
			}
			buf = cp;
			len = strlen (buf);
			if (db->ops->fgets (buf + len,
			                    (int) (buflen - len),
			                    db->fp) == NULL) {
				goto cleanup_buf;
			}
		}
		cp = strrchr (buf, '\n');
		if (NULL != cp) {
			*cp = '\0';
		}

		line = strdup (buf);
		if (NULL == line) {
			goto cleanup_buf;
		}

		if (name_is_nis (line)) {
			eptr = NULL;
		} else {
			eptr = db->ops->parse (line);
			if (NULL != eptr) {
				eptr = db->ops->dup (eptr);
				if (NULL == eptr) {
					goto cleanup_line;
				}
			}
		}

		p = MALLOC(1, struct commonio_entry);
		if (NULL == p) {
			goto cleanup_entry;
		}

		p->eptr = eptr;
		p->line = line;
		p->changed = false;

		add_one_entry (db, p);
	}

	free (buf);

	if (ferror (db->fp) != 0) {
		goto cleanup_errno;
	}

	if ((NULL != db->ops->open_hook) && (db->ops->open_hook () == 0)) {
		goto cleanup_errno;
	}

	db->isopen = true;
	return 1;

      cleanup_entry:
	if (NULL != eptr) {
		db->ops->free (eptr);
	}
      cleanup_line:
	free (line);
      cleanup_buf:
	free (buf);
      cleanup_ENOMEM:
	errno = ENOMEM;
      cleanup_errno:
	saved_errno = errno;
	free_linked_list (db);
	fclose (db->fp);
	db->fp = NULL;
	errno = saved_errno;
	return 0;
}

/*
 * Sort given db according to cmp function (usually compares uids)
 */
int
commonio_sort (struct commonio_db *db, int (*cmp) (const void *, const void *))
{
	struct commonio_entry **entries, *ptr;
	size_t n = 0, i;
#if KEEP_NIS_AT_END
	struct commonio_entry *nis = NULL;
#endif

	for (ptr = db->head;
	        (NULL != ptr)
#if KEEP_NIS_AT_END
	     && ((NULL == ptr->line)
	         || (('+' != ptr->line[0])
	             && ('-' != ptr->line[0])))
#endif
	     ;
	     ptr = ptr->next) {
		n++;
	}
#if KEEP_NIS_AT_END
	if (NULL != ptr) {
		nis = ptr;
	}
#endif

	if (n <= 1) {
		return 0;
	}

	entries = MALLOC(n, struct commonio_entry *);
	if (entries == NULL) {
		return -1;
	}

	n = 0;
	for (ptr = db->head;
#if KEEP_NIS_AT_END
	     nis != ptr;
#else
	     NULL != ptr;
#endif
/*@ -nullderef @*/
	     ptr = ptr->next
/*@ +nullderef @*/
	    ) {
		entries[n] = ptr;
		n++;
	}
	qsort (entries, n, sizeof (struct commonio_entry *), cmp);

	/* Take care of the head and tail separately */
	db->head = entries[0];
	n--;
#if KEEP_NIS_AT_END
	if (NULL == nis)
#endif
	{
		db->tail = entries[n];
	}
	db->head->prev = NULL;
	db->head->next = entries[1];
	entries[n]->prev = entries[n - 1];
#if KEEP_NIS_AT_END
	entries[n]->next = nis;
#else
	entries[n]->next = NULL;
#endif

	/* Now other elements have prev and next entries */
	for (i = 1; i < n; i++) {
		entries[i]->prev = entries[i - 1];
		entries[i]->next = entries[i + 1];
	}

	free (entries);
	db->changed = true;

	return 0;
}

/*
 * Sort entries in db according to order in another.
 */
int commonio_sort_wrt (struct commonio_db *shadow,
                       const struct commonio_db *passwd)
{
	struct commonio_entry *head = NULL, *pw_ptr, *spw_ptr;
	const char *name;

	if ((NULL == shadow) || (NULL == shadow->head)) {
		return 0;
	}

	for (pw_ptr = passwd->head; NULL != pw_ptr; pw_ptr = pw_ptr->next) {
		if (NULL == pw_ptr->eptr) {
			continue;
		}
		name = passwd->ops->getname (pw_ptr->eptr);
		for (spw_ptr = shadow->head;
		     NULL != spw_ptr;
		     spw_ptr = spw_ptr->next) {
			if (NULL == spw_ptr->eptr) {
				continue;
			}
			if (strcmp (name, shadow->ops->getname (spw_ptr->eptr))
			    == 0) {
				break;
			}
		}
		if (NULL == spw_ptr) {
			continue;
		}
		commonio_del_entry (shadow, spw_ptr);
		spw_ptr->next = head;
		head = spw_ptr;
	}

	for (spw_ptr = head; NULL != spw_ptr; spw_ptr = head) {
		head = head->next;

		if (NULL != shadow->head) {
			shadow->head->prev = spw_ptr;
		}
		spw_ptr->next = shadow->head;
		shadow->head = spw_ptr;
	}

	shadow->head->prev = NULL;
	shadow->changed = true;

	return 0;
}

/*
 * write_all - Write the database to its file.
 *
 * It returns 0 if all the entries could be written correctly.
 */
static int write_all (const struct commonio_db *db)
	/*@requires notnull db->fp@*/
{
	const struct commonio_entry *p;
	void *eptr;

	for (p = db->head; NULL != p; p = p->next) {
		if (p->changed) {
			eptr = p->eptr;
			assert (NULL != eptr);
			if (db->ops->put (eptr, db->fp) != 0) {
				return -1;
			}
		} else if (NULL != p->line) {
			if (db->ops->fputs (p->line, db->fp) == EOF) {
				return -1;
			}
			if (putc ('\n', db->fp) == EOF) {
				return -1;
			}
		}
	}
	return 0;
}


int commonio_close (struct commonio_db *db)
{
	int          errors = 0;
	char         buf[1024];
	struct stat  sb;

	if (!db->isopen) {
		errno = EINVAL;
		return 0;
	}
	db->isopen = false;

	if (!db->changed || db->readonly) {
		if (NULL != db->fp) {
			(void) fclose (db->fp);
			db->fp = NULL;
		}
		goto success;
	}

	if ((NULL != db->ops->close_hook) && (db->ops->close_hook () == 0)) {
		goto fail;
	}

	memzero (&sb, sizeof sb);
	if (NULL != db->fp) {
		if (fstat (fileno (db->fp), &sb) != 0) {
			(void) fclose (db->fp);
			db->fp = NULL;
			goto fail;
		}

		/*
		 * Create backup file.
		 */
		if (SNPRINTF(buf, "%s-", db->filename) == -1) {
			(void) fclose (db->fp);
			db->fp = NULL;
			goto fail;
		}

#ifdef WITH_SELINUX
		if (set_selinux_file_context (db->filename, S_IFREG) != 0) {
			errors++;
		}
#endif
		if (create_backup (buf, db->fp) != 0) {
			errors++;
		}

		if (fclose (db->fp) != 0) {
			errors++;
		}

		db->fp = NULL;

#ifdef WITH_SELINUX
		if (reset_selinux_file_context () != 0) {
			errors++;
		}
#endif
		if (errors != 0)
			goto fail;
	} else {
		/*
		 * Default permissions for new [g]shadow files.
		 */
		sb.st_mode = db->st_mode;
		sb.st_uid = db->st_uid;
		sb.st_gid = db->st_gid;
	}

	if (SNPRINTF(buf, "%s+", db->filename) == -1)
		goto fail;

#ifdef WITH_SELINUX
	if (set_selinux_file_context (db->filename, S_IFREG) != 0) {
		errors++;
	}
#endif

	db->fp = fopen_set_perms (buf, "w", &sb);
	if (NULL == db->fp) {
		goto fail;
	}

	if (write_all (db) != 0) {
		errors++;
	}

	if (fflush (db->fp) != 0) {
		errors++;
	}

	if (fsync (fileno (db->fp)) != 0) {
		errors++;
	}

	if (fclose (db->fp) != 0) {
		errors++;
	}

	db->fp = NULL;

	if (errors != 0) {
		unlink (buf);
		goto fail;
	}

	if (lrename (buf, db->filename) != 0) {
		goto fail;
	}

#ifdef WITH_SELINUX
	if (reset_selinux_file_context () != 0) {
		goto fail;
	}
#endif

	nscd_need_reload = true;
	goto success;
      fail:
	errors++;
      success:

	free_linked_list (db);
	return errors == 0;
}

static /*@dependent@*/ /*@null@*/struct commonio_entry *next_entry_by_name (
	struct commonio_db *db,
	/*@null@*/struct commonio_entry *pos,
	const char *name)
{
	struct commonio_entry *p;
	void *ep;

	if (NULL == pos) {
		return NULL;
	}

	for (p = pos; NULL != p; p = p->next) {
		ep = p->eptr;
		if (   (NULL != ep)
		    && (strcmp (db->ops->getname (ep), name) == 0)) {
			break;
		}
	}
	return p;
}

static /*@dependent@*/ /*@null@*/struct commonio_entry *find_entry_by_name (
	struct commonio_db *db,
	const char *name)
{
	return next_entry_by_name (db, db->head, name);
}


int commonio_update (struct commonio_db *db, const void *eptr)
{
	struct commonio_entry *p;
	void *nentry;

	if (!db->isopen || db->readonly) {
		errno = EINVAL;
		return 0;
	}
	nentry = db->ops->dup (eptr);
	if (NULL == nentry) {
		errno = ENOMEM;
		return 0;
	}
	p = find_entry_by_name (db, db->ops->getname (eptr));
	if (NULL != p) {
		if (next_entry_by_name (db, p->next, db->ops->getname (eptr)) != NULL) {
			fprintf (shadow_logfd, _("Multiple entries named '%s' in %s. Please fix this with pwck or grpck.\n"), db->ops->getname (eptr), db->filename);
			db->ops->free (nentry);
			return 0;
		}
		db->ops->free (p->eptr);
		p->eptr = nentry;
		p->changed = true;
		db->cursor = p;

		db->changed = true;
		return 1;
	}
	/* not found, new entry */
	p = MALLOC(1, struct commonio_entry);
	if (NULL == p) {
		db->ops->free (nentry);
		errno = ENOMEM;
		return 0;
	}

	p->eptr = nentry;
	p->line = NULL;
	p->changed = true;

#if KEEP_NIS_AT_END
	add_one_entry_nis (db, p);
#else				/* !KEEP_NIS_AT_END */
	add_one_entry (db, p);
#endif				/* !KEEP_NIS_AT_END */

	db->changed = true;
	return 1;
}

#ifdef ENABLE_SUBIDS
int commonio_append (struct commonio_db *db, const void *eptr)
{
	struct commonio_entry *p;
	void *nentry;

	if (!db->isopen || db->readonly) {
		errno = EINVAL;
		return 0;
	}
	nentry = db->ops->dup (eptr);
	if (NULL == nentry) {
		errno = ENOMEM;
		return 0;
	}
	/* new entry */
	p = MALLOC(1, struct commonio_entry);
	if (NULL == p) {
		db->ops->free (nentry);
		errno = ENOMEM;
		return 0;
	}

	p->eptr = nentry;
	p->line = NULL;
	p->changed = true;
	add_one_entry (db, p);

	db->changed = true;
	return 1;
}
#endif				/* ENABLE_SUBIDS */

void commonio_del_entry (struct commonio_db *db, const struct commonio_entry *p)
{
	if (p == db->cursor) {
		db->cursor = p->next;
	}

	if (NULL != p->prev) {
		p->prev->next = p->next;
	} else {
		db->head = p->next;
	}

	if (NULL != p->next) {
		p->next->prev = p->prev;
	} else {
		db->tail = p->prev;
	}

	db->changed = true;
}

/*
 * commonio_remove - Remove the entry of the given name from the database.
 */
int commonio_remove (struct commonio_db *db, const char *name)
{
	struct commonio_entry *p;

	if (!db->isopen || db->readonly) {
		errno = EINVAL;
		return 0;
	}
	p = find_entry_by_name (db, name);
	if (NULL == p) {
		errno = ENOENT;
		return 0;
	}
	if (next_entry_by_name (db, p->next, name) != NULL) {
		fprintf (shadow_logfd, _("Multiple entries named '%s' in %s. Please fix this with pwck or grpck.\n"), name, db->filename);
		return 0;
	}

	commonio_del_entry (db, p);

	free (p->line);

	if (NULL != p->eptr) {
		db->ops->free (p->eptr);
	}

	free(p);

	return 1;
}

/*
 * commonio_locate - Find the first entry with the specified name in
 *                   the database.
 *
 *	If found, it returns the entry and set the cursor of the database to
 *	that entry.
 *
 *	Otherwise, it returns NULL.
 */
/*@observer@*/ /*@null@*/const void *commonio_locate (struct commonio_db *db, const char *name)
{
	struct commonio_entry *p;

	if (!db->isopen) {
		errno = EINVAL;
		return NULL;
	}
	p = find_entry_by_name (db, name);
	if (NULL == p) {
		errno = ENOENT;
		return NULL;
	}
	db->cursor = p;
	return p->eptr;
}

/*
 * commonio_rewind - Restore the database cursor to the first entry.
 *
 * It returns 0 on error, 1 on success.
 */
int commonio_rewind (struct commonio_db *db)
{
	if (!db->isopen) {
		errno = EINVAL;
		return 0;
	}
	db->cursor = NULL;
	return 1;
}

/*
 * commonio_next - Return the next entry of the specified database
 *
 * It returns the next entry, or NULL if no other entries could be found.
 */
/*@observer@*/ /*@null@*/const void *commonio_next (struct commonio_db *db)
{
	void *eptr;

	if (!db->isopen) {
		errno = EINVAL;
		return 0;
	}
	if (NULL == db->cursor) {
		db->cursor = db->head;
	} else {
		db->cursor = db->cursor->next;
	}

	while (NULL != db->cursor) {
		eptr = db->cursor->eptr;
		if (NULL != eptr) {
			return eptr;
		}

		db->cursor = db->cursor->next;
	}
	return NULL;
}

