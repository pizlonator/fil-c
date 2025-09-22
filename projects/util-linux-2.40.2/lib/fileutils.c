/*
 * This code is in the public domain; do with it what you wish.
 *
 * Copyright (C) 2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright (C) 2012-2024 Karel Zak <kzak@redhat.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>

#include "c.h"
#include "all-io.h"
#include "fileutils.h"
#include "pathnames.h"

int mkstemp_cloexec(char *template)
{
#ifdef HAVE_MKOSTEMP
	return mkostemp(template, O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC);
#else
	int fd, old_flags, errno_save;

	fd = mkstemp(template);
	if (fd < 0)
		return fd;

	old_flags = fcntl(fd, F_GETFD, 0);
	if (old_flags < 0)
		goto unwind;
	if (fcntl(fd, F_SETFD, old_flags | O_CLOEXEC) < 0)
		goto unwind;

	return fd;

unwind:
	errno_save = errno;
	unlink(template);
	close(fd);
	errno = errno_save;

	return -1;
#endif
}

/* Create open temporary file in safe way.  Please notice that the
 * file permissions are -rw------- by default. */
int xmkstemp(char **tmpname, const char *dir, const char *prefix)
{
	char *localtmp;
	const char *tmpenv;
	mode_t old_mode;
	int fd, rc;

	/* Some use cases must be capable of being moved atomically
	 * with rename(2), which is the reason why dir is here.  */
	tmpenv = dir ? dir : getenv("TMPDIR");
	if (!tmpenv)
		tmpenv = _PATH_TMP;

	rc = asprintf(&localtmp, "%s/%s.XXXXXX", tmpenv, prefix);
	if (rc < 0)
		return -1;

	old_mode = umask(077);
	fd = mkstemp_cloexec(localtmp);
	umask(old_mode);
	if (fd == -1) {
		free(localtmp);
		localtmp = NULL;
	}
	*tmpname = localtmp;
	return fd;
}

#ifdef F_DUPFD_CLOEXEC
int dup_fd_cloexec(int oldfd, int lowfd)
#else
int dup_fd_cloexec(int oldfd, int lowfd  __attribute__((__unused__)))
#endif
{
	int fd, flags, errno_save;

#ifdef F_DUPFD_CLOEXEC
	fd = fcntl(oldfd, F_DUPFD_CLOEXEC, lowfd);
	if (fd >= 0)
		return fd;
#endif

	fd = dup(oldfd);
	if (fd < 0)
		return fd;

	flags = fcntl(fd, F_GETFD);
	if (flags < 0)
		goto unwind;
	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0)
		goto unwind;

	return fd;

unwind:
	errno_save = errno;
	close(fd);
	errno = errno_save;

	return -1;
}

/*
 * portable getdtablesize()
 */
unsigned int get_fd_tabsize(void)
{
	int m;

#if defined(HAVE_GETDTABLESIZE)
	m = getdtablesize();
#elif defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE)
	struct rlimit rl;

	getrlimit(RLIMIT_NOFILE, &rl);
	m = rl.rlim_cur;
#elif defined(HAVE_SYSCONF) && defined(_SC_OPEN_MAX)
	m = sysconf(_SC_OPEN_MAX);
#else
	m = OPEN_MAX;
#endif
	return m;
}

void ul_close_all_fds(unsigned int first, unsigned int last)
{
	struct dirent *d;
	DIR *dir;

	dir = opendir(_PATH_PROC_FDDIR);
	if (dir) {
		while ((d = xreaddir(dir))) {
			char *end;
			unsigned int fd;
			int dfd;

			errno = 0;
			fd = strtoul(d->d_name, &end, 10);

			if (errno || end == d->d_name || !end || *end)
				continue;
			dfd = dirfd(dir);
			if (dfd < 0)
				continue;
			if ((unsigned int)dfd == fd)
				continue;
			if (fd < first || last < fd)
				continue;
			close(fd);
		}
		closedir(dir);
	} else {
		unsigned fd, tbsz = get_fd_tabsize();

		for (fd = 0; fd < tbsz; fd++) {
			if (first <= fd && fd <= last)
				close(fd);
		}
	}
}

#ifdef TEST_PROGRAM_FILEUTILS
int main(int argc, char *argv[])
{
	if (argc < 2)
		errx(EXIT_FAILURE, "Usage %s --{mkstemp,close-fds,copy-file}", argv[0]);

	if (strcmp(argv[1], "--mkstemp") == 0) {
		FILE *f;
		char *tmpname = NULL;

		f = xfmkstemp(&tmpname, NULL, "test");
		unlink(tmpname);
		free(tmpname);
		fclose(f);

	} else if (strcmp(argv[1], "--close-fds") == 0) {
		ignore_result( dup(STDIN_FILENO) );
		ignore_result( dup(STDIN_FILENO) );
		ignore_result( dup(STDIN_FILENO) );

# ifdef HAVE_CLOSE_RANGE
		if (close_range(STDERR_FILENO + 1, ~0U, 0) < 0)
# endif
			ul_close_all_fds(STDERR_FILENO + 1, ~0U);

	} else if (strcmp(argv[1], "--copy-file") == 0) {
		int ret = ul_copy_file(STDIN_FILENO, STDOUT_FILENO);
		if (ret == UL_COPY_READ_ERROR)
			err(EXIT_FAILURE, "read");
		else if (ret == UL_COPY_WRITE_ERROR)
			err(EXIT_FAILURE, "write");
	}
	return EXIT_SUCCESS;
}
#endif


int ul_mkdir_p(const char *path, mode_t mode)
{
	char *p, *dir;
	int rc = 0;

	if (!path || !*path)
		return -EINVAL;

	dir = p = strdup(path);
	if (!dir)
		return -ENOMEM;

	if (*p == '/')
		p++;

	while (p && *p) {
		char *e = strchr(p, '/');
		if (e)
			*e = '\0';
		if (*p) {
			rc = mkdir(dir, mode);
			if (rc && errno != EEXIST)
				break;
			rc = 0;
		}
		if (!e)
			break;
		*e = '/';
		p = e + 1;
	}

	free(dir);
	return rc;
}

/* returns basename and keeps dirname in the @path, if @path is "/" (root)
 * then returns empty string */
char *stripoff_last_component(char *path)
{
	char *p = path ? strrchr(path, '/') : NULL;

	if (!p)
		return NULL;
	*p = '\0';
	return p + 1;
}

static int copy_file_simple(int from, int to)
{
	ssize_t nr;
	char buf[BUFSIZ];

	while ((nr = read_all(from, buf, sizeof(buf))) > 0)
		if (write_all(to, buf, nr) == -1)
			return UL_COPY_WRITE_ERROR;
	if (nr < 0)
		return UL_COPY_READ_ERROR;
#ifdef HAVE_EXPLICIT_BZERO
	explicit_bzero(buf, sizeof(buf));
#endif
	return 0;
}

/* Copies the contents of a file. Returns -1 on read error, -2 on write error. */
int ul_copy_file(int from, int to)
{
#ifdef HAVE_SENDFILE
	struct stat st;
	ssize_t nw;

	if (fstat(from, &st) == -1)
		return UL_COPY_READ_ERROR;
	if (!S_ISREG(st.st_mode))
		return copy_file_simple(from, to);
	if (sendfile_all(to, from, NULL, st.st_size) < 0)
		return copy_file_simple(from, to);
	/* ensure we either get an EOF or an error */
	while ((nw = sendfile_all(to, from, NULL, 16*1024*1024)) != 0)
		if (nw < 0)
			return copy_file_simple(from, to);
	return 0;
#else
	return copy_file_simple(from, to);
#endif
}

int ul_reopen(int fd, int flags)
{
	ssize_t ssz;
	char buf[PATH_MAX];
	char fdpath[ sizeof(_PATH_PROC_FDDIR) + sizeof(stringify_value(INT_MAX)) ];

	snprintf(fdpath, sizeof(fdpath), _PATH_PROC_FDDIR "/%d", fd);

	ssz = readlink(fdpath, buf, sizeof(buf) - 1);
	if (ssz < 0)
		return -errno;

	assert(ssz > 0);

	buf[ssz] = '\0';

	return open(buf, flags);
}


/* This is a libc-independent version of basename(), which is necessary to
 * maintain functionality across different libc implementations. It was
 * inspired by the behavior and implementation of glibc.
 */
char *ul_basename(char *path)
{
	char *p;

	if (!path || !*path)
		return (char *) ".";	/* ugly, static string */

	p = strrchr(path, '/');
	if (!p)
		return path;		/* no '/', return original */

	if (*(p + 1) != '\0')
		return p + 1;		/* begin of the name */

	while (p > path && *(p - 1) == '/')
		--p;			/* remove tailing '/' */

	if (p > path) {
		*p-- = '\0';
		while (p > path && *(p - 1) != '/')
			--p;		/* move to the beginning of the name */
	} else while (*(p + 1) != '\0')
		++p;

	return p;
}
