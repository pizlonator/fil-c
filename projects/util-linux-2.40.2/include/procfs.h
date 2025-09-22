/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Copyright (C) 2021 Karel Zak <kzak@redhat.com>
 */
#ifndef UTIL_LINUX_PROCFS_H
#define UTIL_LINUX_PROCFS_H


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <inttypes.h>
#include <dirent.h>

#include "path.h"

struct procfs_process {
	pid_t pid;
};

extern void ul_procfs_init_debug(void);
extern struct path_cxt *ul_new_procfs_path(pid_t pid, const char *prefix);
extern int procfs_process_init_path(struct path_cxt *pc, pid_t pid);

extern int procfs_process_get_uid(struct path_cxt *pc, uid_t *uid);
extern ssize_t procfs_process_get_cmdline(struct path_cxt *pc, char *buf, size_t bufsz);
extern ssize_t procfs_process_get_cmdname(struct path_cxt *pc, char *buf, size_t bufsz);
extern ssize_t procfs_process_get_stat(struct path_cxt *pc, char *buf, size_t bufsz);
extern ssize_t procfs_process_get_syscall(struct path_cxt *pc, char *buf, size_t bufsz);

extern int procfs_process_get_stat_nth(struct path_cxt *pc, int n, uintmax_t *re);

static inline ssize_t procfs_process_get_exe(struct path_cxt *pc, char *buf, size_t bufsz)
{
	return ul_path_readlink(pc, buf, bufsz, "exe");
}

static inline ssize_t procfs_process_get_root(struct path_cxt *pc, char *buf, size_t bufsz)
{
	return ul_path_readlink(pc, buf, bufsz, "root");
}

static inline ssize_t procfs_process_get_cwd(struct path_cxt *pc, char *buf, size_t bufsz)
{
	return ul_path_readlink(pc, buf, bufsz, "cwd");
}

extern int procfs_process_next_tid(struct path_cxt *pc, DIR **sub, pid_t *tid);
extern int procfs_process_next_fd(struct path_cxt *pc, DIR **sub, int *fd);

extern int procfs_dirent_is_process(struct dirent *d);
extern int procfs_dirent_get_pid(struct dirent *d, pid_t *pid);
extern int procfs_dirent_get_uid(DIR *procfs, struct dirent *d, uid_t *uid);
extern int procfs_dirent_match_uid(DIR *procfs, struct dirent *d, uid_t uid);
extern int procfs_dirent_get_name(DIR *procfs, struct dirent *d, char *buf, size_t bufsz);
extern int procfs_dirent_match_name(DIR *procfs, struct dirent *d, const char *name);

extern int fd_is_procfs(int fd);
extern char *pid_get_cmdname(pid_t pid);
extern char *pid_get_cmdline(pid_t pid);

#endif /* UTIL_LINUX_PROCFS_H */
