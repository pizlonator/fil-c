/*
 * SPDX-FileCopyrightText: 2001 Rafal Wojtczuk, Solar Designer
 * SPDX-License-Identifier: 0BSD
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tcb.h>
#include <unistd.h>

#include "config.h"

#include "defines.h"
#include "prototypes.h"
#include "getdef.h"
#include "shadowio.h"
#include "tcbfuncs.h"

#include "shadowlog_internal.h"

#define SHADOWTCB_HASH_BY 1000
#define SHADOWTCB_LOCK_SUFFIX ".lock"

static /*@null@*//*@only@*/char *stored_tcb_user = NULL;

shadowtcb_status shadowtcb_drop_priv (void)
{
	if (!getdef_bool ("USE_TCB")) {
		return SHADOWTCB_SUCCESS;
	}

	if (NULL != stored_tcb_user) {
		if (tcb_drop_priv (stored_tcb_user) == 0) {
			return SHADOWTCB_SUCCESS;
		}
	}

	return SHADOWTCB_FAILURE;
}

shadowtcb_status shadowtcb_gain_priv (void)
{
	if (!getdef_bool ("USE_TCB")) {
		return SHADOWTCB_SUCCESS;
	}

	return (tcb_gain_priv () == 0) ? SHADOWTCB_SUCCESS : SHADOWTCB_FAILURE;
}

/* In case something goes wrong, we return immediately, not polluting the
 * code with free(). All errors are fatal, so the application is expected
 * to exit soon.
 */
#define OUT_OF_MEMORY do { \
	fprintf (shadow_logfd, _("%s: out of memory\n"), shadow_progname); \
	(void) fflush (shadow_logfd); \
} while (false)

/* Returns user's tcb directory path relative to TCB_DIR. */
static /*@null@*/ char *shadowtcb_path_rel (const char *name, uid_t uid)
{
	char *ret;

	if (!getdef_bool ("TCB_SYMLINKS") || uid < SHADOWTCB_HASH_BY) {
		if (asprintf (&ret, "%s", name) == -1) {
			OUT_OF_MEMORY;
			return NULL;
		}
	} else if (uid < SHADOWTCB_HASH_BY * SHADOWTCB_HASH_BY) {
		if (asprintf (&ret, ":%dK/%s",
		              uid / SHADOWTCB_HASH_BY, name) == -1) {
			OUT_OF_MEMORY;
			return NULL;
		}
	} else {
		if (asprintf (&ret, ":%dM/:%dK/%s",
		              uid / (SHADOWTCB_HASH_BY * SHADOWTCB_HASH_BY),
		              (uid % (SHADOWTCB_HASH_BY * SHADOWTCB_HASH_BY)) / SHADOWTCB_HASH_BY,
			name) == -1) {
			OUT_OF_MEMORY;
			return NULL;
		}
	}
	return ret;
}

static /*@null@*/ char *shadowtcb_path_rel_existing (const char *name)
{
	char *path, *rval;
	struct stat st;
	char link[8192];
	ssize_t ret;

	if (asprintf (&path, TCB_DIR "/%s", name) == -1) {
		OUT_OF_MEMORY;
		return NULL;
	}
	if (lstat (path, &st) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot stat %s: %s\n"),
		         shadow_progname, path, strerror (errno));
		free (path);
		return NULL;
	}
	if (S_ISDIR (st.st_mode)) {
		free (path);
		rval = strdup (name);
		if (NULL == rval) {
			OUT_OF_MEMORY;
			return NULL;
		}
		return rval;
	}
	if (!S_ISLNK (st.st_mode)) {
		fprintf (shadow_logfd,
		         _("%s: %s is neither a directory, nor a symlink.\n"),
		         shadow_progname, path);
		free (path);
		return NULL;
	}
	ret = readlink (path, link, sizeof (link) - 1);
	if (-1 == ret) {
		fprintf (shadow_logfd,
		         _("%s: Cannot read symbolic link %s: %s\n"),
		         shadow_progname, path, strerror (errno));
		free (path);
		return NULL;
	}
	free (path);
	if ((size_t)ret >= sizeof(link) - 1) {
		link[sizeof(link) - 1] = '\0';
		fprintf (shadow_logfd,
		         _("%s: Suspiciously long symlink: %s\n"),
		         shadow_progname, link);
		return NULL;
	}
	link[ret] = '\0';
	rval = strdup (link);
	if (NULL == rval) {
		OUT_OF_MEMORY;
		return NULL;
	}
	return rval;
}

static /*@null@*/ char *shadowtcb_path (const char *name, uid_t uid)
{
	char *ret, *rel;

	rel = shadowtcb_path_rel (name, uid);
	if (NULL == rel) {
		return NULL;
	}
	if (asprintf (&ret, TCB_DIR "/%s", rel) == -1) {
		OUT_OF_MEMORY;
		free (rel);
		return NULL;
	}
	free (rel);
	return ret;
}

static /*@null@*/ char *shadowtcb_path_existing (const char *name)
{
	char *ret, *rel;

	rel = shadowtcb_path_rel_existing (name);
	if (NULL == rel) {
		return NULL;
	}
	if (asprintf (&ret, TCB_DIR "/%s", rel) == -1) {
		OUT_OF_MEMORY;
		free (rel);
		return NULL;
	}
	free (rel);
	return ret;
}

static shadowtcb_status mkdir_leading (const char *name, uid_t uid)
{
	char *ind, *dir, *ptr, *path = shadowtcb_path_rel (name, uid);
	struct stat st;

	if (NULL == path) {
		return SHADOWTCB_FAILURE;
	}
	ptr = path;
	if (stat (TCB_DIR, &st) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot stat %s: %s\n"),
		         shadow_progname, TCB_DIR, strerror (errno));
		goto out_free_path;
	}
	while ((ind = strchr (ptr, '/'))) {
		*ind = '\0';
		if (asprintf (&dir, TCB_DIR "/%s", path) == -1) {
			OUT_OF_MEMORY;
			return SHADOWTCB_FAILURE;
		}
		if ((mkdir (dir, 0700) != 0) && (errno != EEXIST)) {
			fprintf (shadow_logfd,
			         _("%s: Cannot create directory %s: %s\n"),
			         shadow_progname, dir, strerror (errno));
			goto out_free_dir;
		}
		if (chown (dir, 0, st.st_gid) != 0) {
			fprintf (shadow_logfd,
			         _("%s: Cannot change owner of %s: %s\n"),
			         shadow_progname, dir, strerror (errno));
			goto out_free_dir;
		}
		if (chmod (dir, 0711) != 0) {
			fprintf (shadow_logfd,
			         _("%s: Cannot change mode of %s: %s\n"),
			         shadow_progname, dir, strerror (errno));
			goto out_free_dir;
		}
		free (dir);
		*ind = '/';
		ptr = ind + 1;
	}
	free (path);
	return SHADOWTCB_SUCCESS;
out_free_dir:
	free (dir);
out_free_path:
	free (path);
	return SHADOWTCB_FAILURE;
}

static shadowtcb_status unlink_suffs (const char *user)
{
	static char *suffs[] = { "+", "-", SHADOWTCB_LOCK_SUFFIX };
	char *tmp;
	int i;

	for (i = 0; i < 3; i++) {
		if (asprintf (&tmp, TCB_FMT "%s", user, suffs[i]) == -1) {
			OUT_OF_MEMORY;
			return SHADOWTCB_FAILURE;
		}
		if ((unlink (tmp) != 0) && (errno != ENOENT)) {
			fprintf (shadow_logfd,
			         _("%s: unlink: %s: %s\n"),
			         shadow_progname, tmp, strerror (errno));
			free (tmp);
			return SHADOWTCB_FAILURE;
		}
		free (tmp);
	}

	return SHADOWTCB_SUCCESS;
}

/* path should be a relative existing tcb directory */
static shadowtcb_status rmdir_leading (char *path)
{
	char *ind, *dir;
	shadowtcb_status ret = SHADOWTCB_SUCCESS;
	while ((ind = strrchr (path, '/'))) {
		*ind = '\0';
		if (asprintf (&dir, TCB_DIR "/%s", path) == -1) {
			OUT_OF_MEMORY;
			return SHADOWTCB_FAILURE;
		}
		if (rmdir (dir) != 0) {
			if (errno != ENOTEMPTY) {
				fprintf (shadow_logfd,
				         _("%s: Cannot remove directory %s: %s\n"),
				         shadow_progname, dir, strerror (errno));
				ret = SHADOWTCB_FAILURE;
			}
			free (dir);
			break;
		}
		free (dir);
	}
	return ret;
}

static shadowtcb_status move_dir (const char *user_newname, uid_t user_newid)
{
	char *olddir = NULL, *newdir = NULL;
	char *real_old_dir = NULL, *real_new_dir = NULL;
	char *real_old_dir_rel = NULL, *real_new_dir_rel = NULL;
	uid_t old_uid, the_newid;
	struct stat oldmode;
	shadowtcb_status ret = SHADOWTCB_FAILURE;

	if (NULL == stored_tcb_user) {
		return SHADOWTCB_FAILURE;
	}
	if (asprintf (&olddir, TCB_DIR "/%s", stored_tcb_user) == -1) {
		goto out_free_nomem;
	}
	if (stat (olddir, &oldmode) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot stat %s: %s\n"),
		         shadow_progname, olddir, strerror (errno));
		goto out_free;
	}
	old_uid = oldmode.st_uid;
	the_newid = (user_newid == -1) ? old_uid : user_newid;
	real_old_dir = shadowtcb_path_existing (stored_tcb_user);
	if (NULL == real_old_dir) {
		goto out_free;
	}
	real_new_dir = shadowtcb_path (user_newname, the_newid);
	if (NULL == real_new_dir) {
		goto out_free;
	}
	if (strcmp (real_old_dir, real_new_dir) == 0) {
		ret = SHADOWTCB_SUCCESS;
		goto out_free;
	}
	real_old_dir_rel = shadowtcb_path_rel_existing (stored_tcb_user);
	if (NULL == real_old_dir_rel) {
		goto out_free;
	}
	if (mkdir_leading (user_newname, the_newid) == SHADOWTCB_FAILURE) {
		goto out_free;
	}
	if (rename (real_old_dir, real_new_dir) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot rename %s to %s: %s\n"),
		         shadow_progname, real_old_dir, real_new_dir, strerror (errno));
		goto out_free;
	}
	if (rmdir_leading (real_old_dir_rel) == SHADOWTCB_FAILURE) {
		goto out_free;
	}
	if ((unlink (olddir) != 0) && (errno != ENOENT)) {
		fprintf (shadow_logfd,
		         _("%s: Cannot remove %s: %s\n"),
		         shadow_progname, olddir, strerror (errno));
		goto out_free;
	}
	if (asprintf (&newdir, TCB_DIR "/%s", user_newname) == -1) {
		goto out_free_nomem;
	}
	real_new_dir_rel = shadowtcb_path_rel (user_newname, the_newid);
	if (NULL == real_new_dir_rel) {
		goto out_free;
	}
	if (   (strcmp (real_new_dir, newdir) != 0)
	    && (symlink (real_new_dir_rel, newdir) != 0)) {
		fprintf (shadow_logfd,
		         _("%s: Cannot create symbolic link %s: %s\n"),
		         shadow_progname, real_new_dir_rel, strerror (errno));
		goto out_free;
	}
	ret = SHADOWTCB_SUCCESS;
	goto out_free;
out_free_nomem:
	OUT_OF_MEMORY;
out_free:
	free (olddir);
	free (newdir);
	free (real_old_dir);
	free (real_new_dir);
	free (real_old_dir_rel);
	free (real_new_dir_rel);
	return ret;
}

shadowtcb_status shadowtcb_set_user (const char* name)
{
	char *buf;
	shadowtcb_status retval;

	if (!getdef_bool ("USE_TCB")) {
		return SHADOWTCB_SUCCESS;
	}

	free (stored_tcb_user);

	stored_tcb_user = strdup (name);
	if (NULL == stored_tcb_user) {
		OUT_OF_MEMORY;
		return SHADOWTCB_FAILURE;
	}
	if (asprintf (&buf, TCB_FMT, name) == -1) {
		OUT_OF_MEMORY;
		return SHADOWTCB_FAILURE;
	}

	retval = (spw_setdbname (buf) != 0) ? SHADOWTCB_SUCCESS : SHADOWTCB_FAILURE;
	free (buf);
	return retval;
}

/* tcb directory must be empty before shadowtcb_remove is called. */
shadowtcb_status shadowtcb_remove (const char *name)
{
	shadowtcb_status ret = SHADOWTCB_SUCCESS;
	char *path = shadowtcb_path_existing (name);
	char *rel = shadowtcb_path_rel_existing (name);
	if ((NULL == path) || (NULL == rel) || (rmdir (path) != 0)) {
		return SHADOWTCB_FAILURE;
	}
	if (rmdir_leading (rel) == SHADOWTCB_FAILURE) {
		return SHADOWTCB_FAILURE;
	}
	free (path);
	free (rel);
	if (asprintf (&path, TCB_DIR "/%s", name) == -1) {
		OUT_OF_MEMORY;
		return SHADOWTCB_FAILURE;
	}
	if ((unlink (path) != 0) && (errno != ENOENT)) {
		ret = SHADOWTCB_FAILURE;
	}
	free (path);
	return ret;
}

shadowtcb_status shadowtcb_move (/*@NULL@*/const char *user_newname, uid_t user_newid)
{
	struct stat dirmode, filemode;
	char *tcbdir, *shadow;
	shadowtcb_status ret = SHADOWTCB_FAILURE;

	if (!getdef_bool ("USE_TCB")) {
		return SHADOWTCB_SUCCESS;
	}
	if (NULL == stored_tcb_user) {
		return SHADOWTCB_FAILURE;
	}
	if (NULL == user_newname) {
		user_newname = stored_tcb_user;
	}
	if (move_dir (user_newname, user_newid) == SHADOWTCB_FAILURE) {
		return SHADOWTCB_FAILURE;
	}
	if (-1 == user_newid) {
		return SHADOWTCB_SUCCESS;
	}
	if (   (asprintf (&tcbdir, TCB_DIR "/%s", user_newname) == -1)
	    || (asprintf (&shadow, TCB_FMT, user_newname) == -1)) {
		OUT_OF_MEMORY;
		return SHADOWTCB_FAILURE;
	}
	if (stat (tcbdir, &dirmode) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot stat %s: %s\n"),
		         shadow_progname, tcbdir, strerror (errno));
		goto out_free;
	}
	if (chown (tcbdir, 0, 0) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot change owners of %s: %s\n"),
		         shadow_progname, tcbdir, strerror (errno));
		goto out_free;
	}
	if (chmod (tcbdir, 0700) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot change mode of %s: %s\n"),
		         shadow_progname, tcbdir, strerror (errno));
		goto out_free;
	}
	if (lstat (shadow, &filemode) != 0) {
		if (errno != ENOENT) {
			fprintf (shadow_logfd,
			         _("%s: Cannot lstat %s: %s\n"),
			         shadow_progname, shadow, strerror (errno));
			goto out_free;
		}
		fprintf (shadow_logfd,
		         _("%s: Warning, user %s has no tcb shadow file.\n"),
		         shadow_progname, user_newname);
	} else {
		if (!S_ISREG (filemode.st_mode) ||
			filemode.st_nlink != 1) {
			fprintf (shadow_logfd,
			         _("%s: Emergency: %s's tcb shadow is not a "
			           "regular file with st_nlink=1.\n"
			           "The account is left locked.\n"),
			         shadow_progname, user_newname);
			goto out_free;
		}
		if (chown (shadow, user_newid, filemode.st_gid) != 0) {
			fprintf (shadow_logfd,
			         _("%s: Cannot change owner of %s: %s\n"),
			         shadow_progname, shadow, strerror (errno));
			goto out_free;
		}
		if (chmod (shadow, filemode.st_mode & 07777) != 0) {
			fprintf (shadow_logfd,
			         _("%s: Cannot change mode of %s: %s\n"),
			         shadow_progname, shadow, strerror (errno));
			goto out_free;
		}
	}
	if (unlink_suffs (user_newname) == SHADOWTCB_FAILURE) {
		goto out_free;
	}
	if (chown (tcbdir, user_newid, dirmode.st_gid) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot change owner of %s: %s\n"),
		         shadow_progname, tcbdir, strerror (errno));
		goto out_free;
	}
	if (chmod (tcbdir, dirmode.st_mode & 07777) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot change mode of %s: %s\n"),
		         shadow_progname, tcbdir, strerror (errno));
		goto out_free;
	}
	ret = SHADOWTCB_SUCCESS;
out_free:
	free (tcbdir);
	free (shadow);
	return ret;
}

shadowtcb_status shadowtcb_create (const char *name, uid_t uid)
{
	char *dir, *shadow;
	struct stat tcbdir_stat;
	gid_t shadowgid, authgid;
	struct group *gr;
	int fd = -1;
	shadowtcb_status ret = SHADOWTCB_FAILURE;

	if (!getdef_bool ("USE_TCB")) {
		return SHADOWTCB_SUCCESS;
	}
	if (stat (TCB_DIR, &tcbdir_stat) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot stat %s: %s\n"),
		         shadow_progname, TCB_DIR, strerror (errno));
		return SHADOWTCB_FAILURE;
	}
	shadowgid = tcbdir_stat.st_gid;
	authgid = shadowgid;
	if (getdef_bool ("TCB_AUTH_GROUP")) {
		gr = getgrnam ("auth");
		if (NULL != gr) {
			authgid = gr->gr_gid;
		}
	}

	if (   (asprintf (&dir, TCB_DIR "/%s", name) == -1)
	    || (asprintf (&shadow, TCB_FMT, name) == -1)) {
		OUT_OF_MEMORY;
		return SHADOWTCB_FAILURE;
	}
	if (mkdir (dir, 0700) != 0) {
		fprintf (shadow_logfd,
		         _("%s: mkdir: %s: %s\n"),
		         shadow_progname, dir, strerror (errno));
		goto out_free;
	}
	fd = open (shadow, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd < 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot open %s: %s\n"),
		         shadow_progname, shadow, strerror (errno));
		goto out_free;
	}
	if (fchown (fd, 0, authgid) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot change owner of %s: %s\n"),
		         shadow_progname, shadow, strerror (errno));
		goto out_free;
	}
	if (fchmod (fd, (mode_t) ((authgid == shadowgid) ? 0600 : 0640)) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot change mode of %s: %s\n"),
		         shadow_progname, shadow, strerror (errno));
		goto out_free;
	}
	if (chown (dir, 0, authgid) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot change owner of %s: %s\n"),
		         shadow_progname, dir, strerror (errno));
		goto out_free;
	}
	if (chmod (dir, (mode_t) ((authgid == shadowgid) ? 02700 : 02710)) != 0) {
		fprintf (shadow_logfd,
		         _("%s: Cannot change mode of %s: %s\n"),
		         shadow_progname, dir, strerror (errno));
		goto out_free;
	}
	if (   (shadowtcb_set_user (name) == SHADOWTCB_FAILURE)
	    || (shadowtcb_move (NULL, uid) == SHADOWTCB_FAILURE)) {
		goto out_free;
	}
	ret = SHADOWTCB_SUCCESS;
out_free:
	if (fd != -1)
		close(fd);
	free (dir);
	free (shadow);
	return ret;
}

