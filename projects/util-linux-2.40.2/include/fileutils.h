/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 **/
#ifndef UTIL_LINUX_FILEUTILS
#define UTIL_LINUX_FILEUTILS

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "c.h"

extern int mkstemp_cloexec(char *template);

extern int xmkstemp(char **tmpname, const char *dir, const char *prefix);

static inline FILE *xfmkstemp(char **tmpname, const char *dir, const char *prefix)
{
	int fd;
	FILE *ret;

	fd = xmkstemp(tmpname, dir, prefix);
	if (fd == -1)
		return NULL;

	if (!(ret = fdopen(fd, "w+" UL_CLOEXECSTR))) {
		close(fd);
		return NULL;
	}
	return ret;
}

#ifdef HAVE_OPENAT
static inline FILE *fopen_at(int dir, const char *filename,
                             int flags, const char *mode)
{
	int fd = openat(dir, filename, flags);
	FILE *ret;

	if (fd < 0)
		return NULL;

	ret = fdopen(fd, mode);
	if (!ret)
		close(fd);
	return ret;
}
#endif

static inline int is_same_inode(const int fd, const struct stat *st)
{
	struct stat f;

	if (fstat(fd, &f) < 0)
		return 0;
	else if (f.st_dev != st->st_dev || f.st_ino != st->st_ino)
		return 0;
	return 1;
}

extern int dup_fd_cloexec(int oldfd, int lowfd);
extern unsigned int get_fd_tabsize(void);

extern int ul_mkdir_p(const char *path, mode_t mode);
extern char *stripoff_last_component(char *path);

/* This is readdir()-like function, but skips "." and ".." directory entries */
static inline struct dirent *xreaddir(DIR *dp)
{
	struct dirent *d;

	while ((d = readdir(dp))) {
		if (!strcmp(d->d_name, ".") ||
		    !strcmp(d->d_name, ".."))
			continue;
		break;
	}
	return d;
}


#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>

# if !defined(HAVE_CLOSE_RANGE) && defined(SYS_close_range)
#  include <sys/types.h>
static inline int close_range(unsigned int first, unsigned int last, int flags)
{
	return syscall(SYS_close_range, first, last, flags);
}
#  define HAVE_CLOSE_RANGE 1
# endif	/* SYS_close_range */

# if !defined(HAVE_STATX) && defined(HAVE_STRUCT_STATX) && defined(SYS_statx)
static inline int statx(int fd, const char *restrict path, int flags,
		    unsigned int mask, struct statx *stx)
{
	return syscall(SYS_statx, fd, path, flags, mask, stx);
}
#  define HAVE_STATX 1
# endif /* SYS_statx */

#endif	/* HAVE_SYS_SYSCALL_H */


extern void ul_close_all_fds(unsigned int first, unsigned int last);

#define UL_COPY_READ_ERROR (-1)
#define UL_COPY_WRITE_ERROR (-2)
int ul_copy_file(int from, int to);


extern int ul_reopen(int fd, int flags);
extern char *ul_basename(char *path);

#endif /* UTIL_LINUX_FILEUTILS */
