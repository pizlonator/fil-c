#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <kbdfile.h>

#include "libcommon.h"
#include "contextP.h"

static struct decompressor {
	const char *ext; /* starts with `.', has no other dots */
	const char *cmd;
} decompressors[] = {
	{ ".gz",  "gzip -d -c"    },
	{ ".bz2", "bzip2 -d -c"   },
	{ ".xz",  "xz -d -c"      },
	{ ".zst", "zstd -d -q -c" },
	{ 0, 0 }
};

struct kbdfile *
kbdfile_new(struct kbdfile_ctx *ctx)
{
	struct kbdfile *fp = calloc(1, sizeof(struct kbdfile));

	if (!fp)
		return NULL;

	fp->ctx = ctx;

	if (!fp->ctx) {
		fp->ctx = kbdfile_context_new();
		if (!fp->ctx) {
			free(fp);
			return NULL;
		}
		fp->flags |= KBDFILE_CTX_INITIALIZED;
	}

	return fp;
}

void
kbdfile_free(struct kbdfile *fp)
{
	if (!fp)
		return;
	if (fp->flags & KBDFILE_CTX_INITIALIZED)
		kbdfile_context_free(fp->ctx);
	kbdfile_close(fp);
	free(fp);
}

char *
kbdfile_get_pathname(struct kbdfile *fp)
{
	if (!fp)
		return NULL;
	return fp->pathname;
}

int
kbdfile_set_pathname(struct kbdfile *fp, const char *pathname)
{
	strncpy(fp->pathname, pathname, sizeof(fp->pathname) - 1);
	return 0;
}

FILE *
kbdfile_get_file(struct kbdfile *fp)
{
	if (!fp)
		return NULL;
	return fp->fd;
}

int
kbdfile_set_file(struct kbdfile *fp, FILE *x)
{
	fp->fd = x;
	return 0;
}

void
kbdfile_close(struct kbdfile *fp)
{
	if (!fp || !fp->fd)
		return;
	if (fp->flags & KBDFILE_PIPE)
		pclose(fp->fd);
	else
		fclose(fp->fd);
	fp->fd = NULL;
	fp->pathname[0] = '\0';
}

static int
pipe_open(const struct decompressor *dc, struct kbdfile *fp)
{
	char *pipe_cmd;

	pipe_cmd = malloc(strlen(dc->cmd) + strlen(fp->pathname) + 2);
	if (pipe_cmd == NULL)
		return -1;

	sprintf(pipe_cmd, "%s %s", dc->cmd, fp->pathname);

	fp->fd = popen(pipe_cmd, "r");
	fp->flags |= KBDFILE_PIPE;

	if (!(fp->fd)) {
		char buf[200];
		strerror_r(errno, buf, sizeof(buf));
		ERR(fp->ctx, "popen: %s: %s", pipe_cmd, buf);
		free(pipe_cmd);
		return -1;
	}

	free(pipe_cmd);
	return 0;
}

/* If a file PATHNAME exists, then open it.
   If is has a `compressed' extension, then open a pipe reading it */
static int
maybe_pipe_open(struct kbdfile *fp)
{
	char *t;
	struct stat st;
	struct decompressor *dc;

	if (stat(fp->pathname, &st) == -1 || !S_ISREG(st.st_mode) || access(fp->pathname, R_OK) == -1)
		return -1;

	t = strrchr(fp->pathname, '.');
	if (t) {
		for (dc = &decompressors[0]; dc->cmd; dc++) {
			if (strcmp(t, dc->ext) == 0)
				return pipe_open(dc, fp);
		}
	}

	fp->flags &= ~KBDFILE_PIPE;

	if ((fp->fd = fopen(fp->pathname, "r")) == NULL) {
		char buf[200];
		strerror_r(errno, buf, sizeof(buf));

		ERR(fp->ctx, "fopen: %s: %s", fp->pathname, buf);
		return -1;
	}

	return 0;
}

static int
findfile_by_fullname(const char *fnam, const char *const *suffixes, struct kbdfile *fp)
{
	int i;
	struct stat st;
	struct decompressor *dc;
	size_t fnam_len, sp_len;

	fp->flags &= ~KBDFILE_PIPE;
	fnam_len = strlen(fnam);

	for (i = 0; suffixes[i]; i++) {
		if (suffixes[i] == 0)
			continue; /* we tried it already */

		sp_len = strlen(suffixes[i]);

		if (fnam_len + sp_len + 1 > sizeof(fp->pathname))
			continue;

		snprintf(fp->pathname, sizeof(fp->pathname), "%s%s", fnam, suffixes[i]);

		if (stat(fp->pathname, &st) == 0 && S_ISREG(st.st_mode) && (fp->fd = fopen(fp->pathname, "r")) != NULL)
			return 0;

		for (dc = &decompressors[0]; dc->cmd; dc++) {
			if (fnam_len + sp_len + strlen(dc->ext) + 1 > sizeof(fp->pathname))
				continue;

			snprintf(fp->pathname, sizeof(fp->pathname), "%s%s%s", fnam, suffixes[i], dc->ext);

			if (stat(fp->pathname, &st) == 0 && S_ISREG(st.st_mode) && access(fp->pathname, R_OK) == 0)
				return pipe_open(dc, fp);
		}
	}

	return -1;
}

static int
filecmp(const char *fname, const char *name, const char *const *suf, unsigned int *index, struct decompressor **d)
{
	/* Does d_name start right? */
	const char *p = name;
	const char *q = fname;

	while (*p && *p == *q)
		p++, q++;

	if (*q)
		return 1;

	/* Does tail consist of a known suffix and possibly a compression suffix? */
	for (unsigned int i = 0; suf[i]; i++) {
		if (!strcmp(p, suf[i])) {
			if (i < *index) {
				*index = i;
				*d = NULL;
			}
			return 0;
		}

		size_t l = strlen(suf[i]);

		if (strncmp(p, suf[i], l))
			continue;

		for (struct decompressor *dc = &decompressors[0]; dc->cmd; dc++) {
			if (!strcmp(p + l, dc->ext)) {
				if (i < *index) {
					*index = i;
					*d = dc;
				}
				return 0;
			}
		}
	}

	return 1;
}

static int
findfile_in_dir(const char *fnam, const char *dir, const int recdepth, const char *const *suf, struct kbdfile *fp)
{
	char errbuf[200];
	char *ff, *fdir, *path;
	int rc = 1, secondpass = 0;
	size_t dir_len;

	fp->fd = NULL;
	fp->flags &= ~KBDFILE_PIPE;

	dir_len = strlen(dir);

	ff = strchr(fnam, '/');
	fdir = NULL;

	if (ff != NULL) {
		fdir = strndup(fnam, (size_t) (ff - fnam));

		if (fdir == NULL) {
			strerror_r(errno, errbuf, sizeof(errbuf));
			ERR(fp->ctx, "strndup: %s", errbuf);
			return -1;
		}
	}

	struct dirent **namelist = NULL;

	int dirents = scandir(dir, &namelist, NULL, alphasort);

	if (dirents < 0) {
		strerror_r(errno, errbuf, sizeof(errbuf));
		DBG(fp->ctx, "scandir: %s: %s", dir, errbuf);
		rc = -1;
		goto EndScan;
	}

	struct decompressor *dc = NULL;
	unsigned int index = UINT_MAX;

	// Scan the directory twice: first for files, then
	// for subdirectories, so that we do never search
	// a subdirectory when the directory itself already
	// contains the file we are looking for.
StartScan:
	for (int n = 0; n < dirents; n++) {
		struct stat st;

		size_t d_len = strlen(namelist[n]->d_name);

		if (d_len < 3) {
			if (!strcmp(namelist[n]->d_name, ".") || !strcmp(namelist[n]->d_name, ".."))
				continue;
		}

		if (dir_len + d_len + 2 > sizeof(fp->pathname))
			continue;

		int okdir = (ff && !strcmp(namelist[n]->d_name, fdir));

		if ((secondpass && recdepth) || okdir) {
			path = malloc(dir_len + d_len + 2);

			if (path == NULL) {
				rc = -1;
				goto EndScan;
			}

			sprintf(path, "%s/%s", dir, namelist[n]->d_name);

			if (!stat(path, &st) && S_ISDIR(st.st_mode)) {
				if (okdir) {
					rc = findfile_in_dir(ff + 1, path, 0, suf, fp);
				}

				if (rc && recdepth) {
					rc = findfile_in_dir(fnam, path, recdepth - 1, suf, fp);
				}
			}
			free(path);

			if (!rc) {
				goto EndScan;
			}
		}

		if (secondpass || ff)
			continue;

		snprintf(fp->pathname, sizeof(fp->pathname), "%s/%s", dir, namelist[n]->d_name);

		if (stat(fp->pathname, &st) || !S_ISREG(st.st_mode))
			continue;

		if (!filecmp(fnam, namelist[n]->d_name, suf, &index, &dc)) {
			rc = 0;
		}
	}

	if (!secondpass && index != UINT_MAX) {
		snprintf(fp->pathname, sizeof(fp->pathname), "%s/%s%s%s", dir, fnam, suf[index], (dc ? dc->ext : ""));

		if (!dc) {
			rc = maybe_pipe_open(fp);
			goto EndScan;
		}

		if (pipe_open(dc, fp) < 0) {
			rc = -1;
			goto EndScan;
		}
	}

	if (recdepth > 0 && !secondpass) {
		secondpass = 1;
		goto StartScan;
	}

EndScan:
	if (namelist != NULL) {
		for (int n = 0; n < dirents; n++)
			free(namelist[n]);
		free(namelist);
	}

	if (fdir != NULL)
		free(fdir);

	return rc;
}

int
kbdfile_find(const char *fnam, const char *const *dirpath, const char *const *suffixes, struct kbdfile *fp)
{
	int rc, i;

	if (fp->fd != NULL) {
		ERR(fp->ctx, "can't open `%s', because kbdfile already opened: %s", fnam, fp->pathname);
		return -1;
	}

	fp->flags &= ~KBDFILE_PIPE;

	/* Try explicitly given name first */
	strncpy(fp->pathname, fnam, sizeof(fp->pathname) - 1);

	if (!maybe_pipe_open(fp))
		return 0;

	/* Test for full pathname - opening it failed, so need suffix */
	/* (This is just nonsense, for backwards compatibility.) */
	if (*fnam == '/' &&
	    !findfile_by_fullname(fnam, suffixes, fp))
		return 0;

	/* Search a list of directories and directory hierarchies */
	for (i = 0; dirpath[i]; i++) {
		int recdepth = 0;
		char *dir = NULL;

		size_t dl = strlen(dirpath[i]);

		/* trailing stars denote recursion */
		while (dl && dirpath[i][dl - 1] == '*')
			dl--, recdepth++;

		/* delete trailing slashes */
		while (dl && dirpath[i][dl - 1] == '/')
			dl--;

		if (dl)
			dir = strndup(dirpath[i], dl);
		else
			dir = strdup(".");

		if (dir == NULL) {
			char buf[200];
			strerror_r(errno, buf, sizeof(buf));
			ERR(fp->ctx, "strdup: %s", buf);
			return -1;
		}

		rc = findfile_in_dir(fnam, dir, recdepth, suffixes, fp);
		free(dir);

		if (!rc)
			return 0;
	}

	return 1;
}

struct kbdfile *
kbdfile_open(struct kbdfile_ctx *ctx, const char *filename)
{
	struct kbdfile *fp = kbdfile_new(ctx);

	if (!fp)
		return NULL;

	kbdfile_set_pathname(fp, filename);

	if (maybe_pipe_open(fp) < 0) {
		kbdfile_free(fp);
		return NULL;
	}

	return fp;
}

int
kbdfile_is_compressed(struct kbdfile *fp)
{
	return (fp->flags & KBDFILE_PIPE);
}
