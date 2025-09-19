/*
 * Copyright © 2024 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util-strings.h"

static inline int
mkdir_p(const char *dir)
{
	char *path, *parent;
	int rc;

	if (streq(dir, "/"))
		return 0;

	path = safe_strdup(dir);
	parent = dirname(path);
	rc = mkdir_p(parent);
	free(path);

	if (rc < 0)
		return rc;

	rc = mkdir(dir, 0755);

	return (rc == -1 && errno != EEXIST) ? -errno : 0;
}

DEFINE_TRIVIAL_CLEANUP_FUNC(DIR *, closedir);

static inline int
rmdir_r(const char *dir)
{
	_cleanup_(closedirp) DIR *d = opendir(dir);
	if (!d)
		return -errno;

	struct dirent *entry;
	int rc = 0;

	while (rc >= 0 && (entry = readdir(d))) {
		if (streq(entry->d_name, ".") || streq(entry->d_name, ".."))
			continue;

		_autofree_ char *path = strdup_printf("%s/%s", dir, entry->d_name);

		struct stat st;
		if (stat(path, &st) < 0)
			return -errno;

		if (S_ISDIR(st.st_mode))
			rc = rmdir_r(path);
		else
			rc = unlink(path) < 0 ? -errno : 0;
	}
	rc = rmdir(dir) < 0 ? -errno : rc;

	return rc;
}

static inline void
xclose(int *fd)
{
	if (*fd > -1) {
		close(*fd);
		*fd = -1;
	}
}

/**
 * In the NULL-terminated list of directories
 * search for files with the given suffix and return
 * a filename-ordered NULL-terminated list of those
 * full paths.
 *
 * The directories are given in descending priority order.
 * Any file with a given filename shadows the same file
 * in another directory of lower sorting order.
 *
 * If nfiles is not NULL, it is set to the number of
 * files returned (not including the NULL terminator).
 */
char **
list_files(const char **directories, const char *suffix, size_t *nfiles);

struct tmpdir {
	char *path;
};

static inline void
tmpdir_destroy(struct tmpdir *tmpdir)
{
	/* String check so we can't accidentally rm -rf */
	if (tmpdir->path && strstr(tmpdir->path, "tmpdir-")) {
		rmdir_r(tmpdir->path);
		free(tmpdir->path);
	}
	free(tmpdir);
}

DEFINE_DESTROY_CLEANUP_FUNC(tmpdir);

static inline struct tmpdir *
tmpdir_create(const char *basedir)
{
	_destroy_(tmpdir) *tmpdir = zalloc(sizeof(*tmpdir));
	tmpdir->path = strdup_printf("%s/tmpdir-XXXXXX", basedir ? basedir : "/tmp");
	if (!mkdtemp(tmpdir->path))
		return NULL;

	return steal(&tmpdir);
}
