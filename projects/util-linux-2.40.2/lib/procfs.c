/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Copyright (C) 2021 Karel Zak <kzak@redhat.com>
 */
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#ifdef HAVE_SYS_VFS_H
# include <sys/vfs.h>
# include "statfs_magic.h"
#endif

#include "c.h"
#include "pathnames.h"
#include "procfs.h"
#include "fileutils.h"
#include "all-io.h"
#include "debug.h"
#include "strutils.h"

static void procfs_process_deinit_path(struct path_cxt *pc);

/*
 * Debug stuff (based on include/debug.h)
 */
static UL_DEBUG_DEFINE_MASK(ulprocfs);
UL_DEBUG_DEFINE_MASKNAMES(ulprocfs) = UL_DEBUG_EMPTY_MASKNAMES;

#define ULPROCFS_DEBUG_INIT	(1 << 1)
#define ULPROCFS_DEBUG_CXT	(1 << 2)

#define DBG(m, x)       __UL_DBG(ulprocfs, ULPROCFS_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(ulprocfs, ULPROCFS_DEBUG_, m, x)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(ulprocfs)
#include "debugobj.h"

void ul_procfs_init_debug(void)
{
	if (ulprocfs_debug_mask)
		return;
	__UL_INIT_DEBUG_FROM_ENV(ulprocfs, ULPROCFS_DEBUG_, 0, ULPROCFS_DEBUG);
}

struct path_cxt *ul_new_procfs_path(pid_t pid, const char *prefix)
{
	struct path_cxt *pc = ul_new_path(NULL);

	if (!pc)
		return NULL;
	if (prefix)
		ul_path_set_prefix(pc, prefix);

	if (procfs_process_init_path(pc, pid) != 0) {
		ul_unref_path(pc);
		return NULL;
	}

	DBG(CXT, ul_debugobj(pc, "alloc"));
	return pc;
}

/*
 * procfs_blkdev_* is procfs extension to ul_path_* API to read info about process.
 *
 * The function is possible to call in loop and without sysfs_procfs_deinit_path().
 * The procfs_process_deinit_path() is automatically called by ul_unref_path().
 *
 */
int procfs_process_init_path(struct path_cxt *pc, pid_t pid)
{
	struct procfs_process *prc;
	int rc;
	char buf[sizeof(_PATH_PROC) + sizeof(stringify_value(UINT32_MAX)) + 2];

	/* define path to pid stuff */
	snprintf(buf, sizeof(buf), _PATH_PROC "/%zu", (size_t) pid);
	rc = ul_path_set_dir(pc, buf);
	if (rc)
		return rc;

	/* make sure path exists */
	rc = ul_path_get_dirfd(pc);
	if (rc < 0)
		return rc;

	/* initialize procfs specific stuff */
	prc = ul_path_get_dialect(pc);
	if (!prc) {
		DBG(CXT, ul_debugobj(pc, "alloc new procfs handler"));
		prc = calloc(1, sizeof(struct procfs_process));
		if (!prc)
			return -ENOMEM;

		ul_path_set_dialect(pc, prc, procfs_process_deinit_path);
	}

	DBG(CXT, ul_debugobj(pc, "init procfs stuff"));

	prc->pid = pid;
	return 0;
}

static void procfs_process_deinit_path(struct path_cxt *pc)
{
	struct procfs_process *prc;

	if (!pc)
		return;

	DBG(CXT, ul_debugobj(pc, "deinit"));

	prc = ul_path_get_dialect(pc);
	if (!prc)
		return;

	free(prc);
	ul_path_set_dialect(pc, NULL, NULL);
}

static ssize_t read_procfs_file(int fd, char *buf, size_t bufsz)
{
	ssize_t sz = 0;
	size_t i;

	if (fd < 0)
		return -EINVAL;

	sz = read_all(fd, buf, bufsz);
	if (sz <= 0)
		return sz;

	for (i = 0; i < (size_t) sz; i++) {
		if (buf[i] == '\0')
			buf[i] = ' ';
	}
	buf[sz - 1] = '\0';
	return sz;
}

static ssize_t procfs_process_get_data_for(struct path_cxt *pc, char *buf, size_t bufsz,
					    const char *fname)
{
	int fd = ul_path_open(pc, O_RDONLY|O_CLOEXEC, fname);

	if (fd >= 0) {
		ssize_t sz = read_procfs_file(fd, buf, bufsz);
		close(fd);
		return sz;
	}
	return -errno;
}

ssize_t procfs_process_get_cmdline(struct path_cxt *pc, char *buf, size_t bufsz)
{
	return procfs_process_get_data_for(pc, buf, bufsz, "cmdline");
}

ssize_t procfs_process_get_cmdname(struct path_cxt *pc, char *buf, size_t bufsz)
{
	return procfs_process_get_data_for(pc, buf, bufsz, "comm");
}

ssize_t procfs_process_get_stat(struct path_cxt *pc, char *buf, size_t bufsz)
{
	return procfs_process_get_data_for(pc, buf, bufsz, "stat");
}

ssize_t procfs_process_get_syscall(struct path_cxt *pc, char *buf, size_t bufsz)
{
	return procfs_process_get_data_for(pc, buf, bufsz, "syscall");
}

int procfs_process_get_stat_nth(struct path_cxt *pc, int n, uintmax_t *re)
{
	ssize_t rc;
	char *key = NULL, *tok, *p;
	char buf[BUFSIZ];
	int i;

	if (n == 2 || n == 3)		/* process name and status (strings) */
		return -EINVAL;

	rc = procfs_process_get_data_for(pc, buf, sizeof(buf), "stat");
	if (rc < 0)
		return rc;

	for (i = 0, tok = strtok_r(buf, " ", &key); tok;
	     tok = strtok_r(NULL, " ", &key)) {

		i++;
		if (i == n)
			return ul_strtou64(tok, re, 10);

		/* skip rest of the process name */
		if (i == 2 && (p = strrchr(key, ')')))
			key = p + 2;
	}

	return -EINVAL;
}

int procfs_process_get_uid(struct path_cxt *pc, uid_t *uid)
{
	struct stat sb;
	int rc;

	if ((rc = ul_path_stat(pc, &sb, 0, NULL)) == 0)
		*uid = sb.st_uid;
	return rc;
}

/*
 * returns the next task TID, the @sub is automatically initialized
 * when called first time and closed after last call or you can
 * call closedir()* when you need to break the loop.
 *
 * Returns: <0 on error, 0 on success, >1 done
 *
 * Example:
 *
 * pid_t tid;
 * DIR *sub = NULL;
 * path_cxt *pc = ul_new_procfs_path(123, NULL);
 *
 * while (procfs_process_next_tid(pc, &sub, &tid) == 0)
 *	printf("task: %d", (int) tid);
 *
 */
int procfs_process_next_tid(struct path_cxt *pc, DIR **sub, pid_t *tid)
{
	struct dirent *d;

	if (!pc || !sub || !tid)
		return -EINVAL;

	if (!*sub) {
		*sub = ul_path_opendir(pc, "task");
		if (!*sub)
			return -errno;
	}

	while ((d = xreaddir(*sub))) {
		if (procfs_dirent_get_pid(d, tid) == 0)
			return 0;
	}

	closedir(*sub);
	*sub = NULL;
	return 1;
}

int procfs_process_next_fd(struct path_cxt *pc, DIR **sub, int *fd)
{
	struct dirent *d;

	if (!pc || !sub || !fd)
		return -EINVAL;

	if (!*sub) {
		*sub = ul_path_opendir(pc, "fd");
		if (!*sub)
			return -errno;
	}

	while ((d = xreaddir(*sub))) {
		uint64_t num;
#ifdef _DIRENT_HAVE_D_TYPE
		if (d->d_type != DT_LNK && d->d_type != DT_UNKNOWN)
			continue;
#endif
		if (ul_strtou64(d->d_name, &num, 10) < 0)
			continue;
		*fd = num;
		return 0;
	}

	closedir(*sub);
	*sub = NULL;
	return 1;
}

/*
 * Simple 'dirent' based stuff for use-cases where procfs_process_* API is overkill
 */

/* stupid, but good enough as a basic filter */
int procfs_dirent_is_process(struct dirent *d)
{
#ifdef _DIRENT_HAVE_D_TYPE
	if (d->d_type != DT_DIR && d->d_type != DT_UNKNOWN)
		return 0;
#endif
	if (!isdigit((unsigned char) *d->d_name))
		return 0;

	return 1;
}

int procfs_dirent_get_pid(struct dirent *d, pid_t *pid)
{
	uint64_t num;

	if (!procfs_dirent_is_process(d))
		return -EINVAL;

	if (ul_strtou64(d->d_name, &num, 10) < 0)
		return -EINVAL;

	*pid = (pid_t) num;
	return 0;
}

int procfs_dirent_get_uid(DIR *procfs, struct dirent *d, uid_t *uid)
{
	struct stat st;

	if (!procfs_dirent_is_process(d))
		return -EINVAL;

	if (fstatat(dirfd(procfs), d->d_name, &st, 0))
		return -EINVAL;

	*uid = st.st_uid;
	return 0;
}

int procfs_dirent_match_uid(DIR *procfs, struct dirent *d, uid_t uid)
{
	uid_t x;

	if (procfs_dirent_get_uid(procfs, d, &x) == 0)
		return x == uid;

	return 0;
}

/* "name" of process; may be truncated, see prctl(2) and PR_SET_NAME.
 * The minimal of the @buf has to be 32 bytes. */
int procfs_dirent_get_name(DIR *procfs, struct dirent *d, char *buf, size_t bufsz)
{
	FILE *f;
	size_t sz;
	char tmp[1024], *p, *end = NULL;

	if (bufsz < 32)
		return -EINVAL;
	if (!procfs_dirent_is_process(d))
		return -EINVAL;

	snprintf(tmp, sizeof(tmp), "%s/stat", d->d_name);
	f = fopen_at(dirfd(procfs), tmp, O_CLOEXEC|O_RDONLY, "r");
	if (!f)
		return -errno;

	p = fgets(tmp, sizeof(tmp), f);
	fclose(f);
	if (!p)
		return -errno;

	/* skip PID */
	while (*p && *p != '(')
		p++;

	/* skip extra '(' */
	while (*p && *p == '(')
		p++;

	end = p;
	while (*end && *end != ')')
		end++;

	sz = end - p;
	if (sz >= bufsz)
		sz = bufsz - 1;

	memcpy(buf, p, sz);
	buf[sz] = '\0';

	return 0;
}

int procfs_dirent_match_name(DIR *procfs, struct dirent *d, const char *name)
{
	char buf[33];

	if (procfs_dirent_get_name(procfs, d, buf, sizeof(buf)) == 0)
		return strcmp(name, buf) == 0;

	return 0;
}

#ifdef HAVE_SYS_VFS_H
/* checks if fd is file in a procfs;
 * returns 1 if true, 0 if false or couldn't determine */
int fd_is_procfs(int fd)
{
	struct statfs st;
	int ret;

	do {
		errno = 0;
		ret = fstatfs(fd, &st);

		if (ret < 0) {
			if (errno != EINTR && errno != EAGAIN)
				return 0;
			xusleep(250000);
		}
	} while (ret != 0);

	return st.f_type == STATFS_PROC_MAGIC;
	return 0;
}
#else
int fd_is_procfs(int fd __attribute__((__unused__)))
{
	return 0;
}
#endif

static char *strdup_procfs_file(pid_t pid, const char *name)
{
	char buf[BUFSIZ];
	char *re = NULL;
	int fd;

	snprintf(buf, sizeof(buf), _PATH_PROC "/%d/%s", (int) pid, name);
	fd = open(buf, O_CLOEXEC|O_RDONLY);
	if (fd < 0)
		return NULL;

	if (read_procfs_file(fd, buf, sizeof(buf)) > 0)
		re = strdup(buf);
	close(fd);
	return re;
}

char *pid_get_cmdname(pid_t pid)
{
	return strdup_procfs_file(pid, "comm");
}

char *pid_get_cmdline(pid_t pid)
{
	return strdup_procfs_file(pid, "cmdline");
}

#ifdef TEST_PROGRAM_PROCFS

static int test_tasks(int argc, char *argv[], const char *prefix)
{
	DIR *sub = NULL;
	struct path_cxt *pc;
	pid_t tid = 0, pid;

	if (argc != 2)
		return EXIT_FAILURE;

	pid = strtol(argv[1], (char **) NULL, 10);
	printf("PID=%d, TIDs:", pid);

	pc = ul_new_procfs_path(pid, prefix);
	if (!pc)
		err(EXIT_FAILURE, "alloc procfs handler failed");

	while (procfs_process_next_tid(pc, &sub, &tid) == 0)
		printf(" %d", tid);

	printf("\n");
        ul_unref_path(pc);
	return EXIT_SUCCESS;
}

static int test_fds(int argc, char *argv[], const char *prefix)
{
	DIR *sub = NULL;
	struct path_cxt *pc;
	pid_t pid;
	int fd = -1;

	if (argc != 2)
		return EXIT_FAILURE;

	pid = strtol(argv[1], (char **) NULL, 10);
	printf("PID=%d, FDs:", pid);

	pc = ul_new_procfs_path(pid, prefix);
	if (!pc)
		err(EXIT_FAILURE, "alloc procfs handler failed");

	while (procfs_process_next_fd(pc, &sub, &fd) == 0)
		printf(" %d", fd);

	fputc('\n', stdout);
        ul_unref_path(pc);
	return EXIT_SUCCESS;
}

static int test_processes(int argc, char *argv[])
{
	DIR *dir;
	struct dirent *d;
	char *name = NULL;
	uid_t uid = (uid_t) -1;
	char buf[128];

	if (argc >= 3 && strcmp(argv[1], "--name") == 0)
		name = argv[2];
	if (argc >= 3 && strcmp(argv[1], "--uid") == 0)
		uid = (uid_t) atol(argv[2]);

	dir = opendir(_PATH_PROC);
	if (!dir)
		err(EXIT_FAILURE, "cannot open proc");

	while ((d = xreaddir(dir))) {
		pid_t pid = 0;

		if (procfs_dirent_get_pid(d, &pid) != 0)
			continue;
		if (name && !procfs_dirent_match_name(dir, d, name))
			continue;
		if (uid != (uid_t) -1 && !procfs_dirent_match_uid(dir, d, uid))
			continue;
		procfs_dirent_get_name(dir, d, buf, sizeof(buf));
		printf(" %d [%s]", pid, buf);
	}

	fputc('\n', stdout);
	closedir(dir);
	return EXIT_SUCCESS;
}

static int test_one_process(int argc, char *argv[], const char *prefix)
{
	pid_t pid;
	struct path_cxt *pc;
	char buf[BUFSIZ];
	uid_t uid = (uid_t) -1;

	if (argc != 2)
		return EXIT_FAILURE;
	pid = strtol(argv[1], (char **) NULL, 10);

	pc = ul_new_procfs_path(pid, prefix);
	if (!pc)
		err(EXIT_FAILURE, "cannot alloc procfs handler");

	printf("%d\n", (int) pid);

	procfs_process_get_uid(pc, &uid);
	printf("   UID: %zu\n", (size_t) uid);

	procfs_process_get_cmdline(pc, buf, sizeof(buf));
	printf("   CMDLINE: '%s'\n", buf);

	procfs_process_get_cmdname(pc, buf, sizeof(buf));
	printf("   COMM: '%s'\n", buf);

	ul_unref_path(pc);
	return EXIT_SUCCESS;
}

static int test_isprocfs(int argc, char *argv[])
{
	const char *name = argc > 1 ? argv[1] : "/proc";
	int fd = open(name, O_RDONLY);
	int is = 0;

	if (fd >= 0) {
		is = fd_is_procfs(fd);
		close(fd);
	} else
		err(EXIT_FAILURE, "cannot open %s", name);

	printf("%s: %s procfs\n", name, is ? "is" : "is NOT");
	return is ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int test_process_stat_nth(int argc, char *argv[], const char *prefix)
{
	pid_t pid;
	struct path_cxt *pc;
	uintmax_t num = 0;
	int n, ret;

	if (argc != 3)
		return EXIT_FAILURE;
	pid = strtol(argv[1], (char **) NULL, 10);
	n = strtol(argv[2], (char **) NULL, 10);

	pc = ul_new_procfs_path(pid, prefix);
	if (!pc)
		err(EXIT_FAILURE, "cannot alloc procfs handler");

	ret = procfs_process_get_stat_nth(pc, n, &num);
	if (ret)
		errx(EXIT_FAILURE, "read %dth number failed: %s", n, strerror(-ret));

	printf("%d: %dth %ju\n", (int) pid, n, num);
	ul_unref_path(pc);
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	const char *prefix = NULL;

	if (argc > 2 && strcmp(argv[1], "--prefix") == 0) {
		prefix = argv[2];
		argc -= 2;
		argv += 2;
	}

	if (argc < 2) {
		fprintf(stderr, "usage: %1$s [--prefix <prefix>] --tasks <pid>\n"
				"       %1$s [--prefix <prefix>] --fds <pid>\n"
				"       %1$s --is-procfs [<dir>]\n"
				"       %1$s --processes [--name <name>] [--uid <uid>]\n"
				"       %1$s [--prefix <prefix>] --one <pid>\n"
				"       %1$s [--prefix <prefix>] --stat-nth <pid> <n>\n",
				program_invocation_short_name);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "--tasks") == 0)
		return test_tasks(argc - 1, argv + 1, prefix);
	if (strcmp(argv[1], "--fds") == 0)
		return test_fds(argc - 1, argv + 1, prefix);
	if (strcmp(argv[1], "--processes") == 0)
		return test_processes(argc - 1, argv + 1);
	if (strcmp(argv[1], "--is-procfs") == 0)
		return test_isprocfs(argc - 1, argv + 1);
	if (strcmp(argv[1], "--one") == 0)
		return test_one_process(argc - 1, argv + 1, prefix);
	if (strcmp(argv[1], "--stat-nth") == 0)
		return test_process_stat_nth(argc - 1, argv + 1, prefix);

	return EXIT_FAILURE;
}
#endif /* TEST_PROGRAM_PROCUTILS */
