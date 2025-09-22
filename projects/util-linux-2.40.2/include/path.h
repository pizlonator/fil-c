/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_PATH_H
#define UTIL_LINUX_PATH_H

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "c.h"

struct path_cxt {
	int	dir_fd;
	char	*dir_path;

	int	refcount;

	char *prefix;
	char path_buffer[PATH_MAX];

	void	*dialect;
	void	(*free_dialect)(struct path_cxt *);
	int	(*redirect_on_enoent)(struct path_cxt *, const char *, int *);
};

struct path_cxt *ul_new_path(const char *dir, ...)
			__attribute__ ((__format__ (__printf__, 1, 2)));
void ul_unref_path(struct path_cxt *pc);
void ul_ref_path(struct path_cxt *pc);

void ul_path_init_debug(void);

int ul_path_set_prefix(struct path_cxt *pc, const char *prefix);
const char *ul_path_get_prefix(struct path_cxt *pc);

int ul_path_set_dir(struct path_cxt *pc, const char *dir);
const char *ul_path_get_dir(struct path_cxt *pc);

int ul_path_set_dialect(struct path_cxt *pc, void *data, void free_data(struct path_cxt *));
void *ul_path_get_dialect(struct path_cxt *pc);

int ul_path_set_enoent_redirect(struct path_cxt *pc, int (*func)(struct path_cxt *, const char *, int *));
int ul_path_get_dirfd(struct path_cxt *pc);
void ul_path_close_dirfd(struct path_cxt *pc);
int ul_path_isopen_dirfd(struct path_cxt *pc);
int ul_path_is_accessible(struct path_cxt *pc);

char *ul_path_get_abspath(struct path_cxt *pc, char *buf, size_t bufsz, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 4, 5)));

int ul_path_stat(struct path_cxt *pc, struct stat *sb, int flags, const char *path);
int ul_path_access(struct path_cxt *pc, int mode, const char *path);
int ul_path_accessf(struct path_cxt *pc, int mode, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));

int ul_path_open(struct path_cxt *pc, int flags, const char *path);
int ul_path_openf(struct path_cxt *pc, int flags, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));
int ul_path_vopenf(struct path_cxt *pc, int flags, const char *path, va_list ap)
				__attribute__ ((__format__ (__printf__, 3, 0)));

FILE *ul_path_fopen(struct path_cxt *pc, const char *mode, const char *path);
FILE *ul_path_fopenf(struct path_cxt *pc, const char *mode, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));
FILE *ul_path_vfopenf(struct path_cxt *pc, const char *mode, const char *path, va_list ap)
				__attribute__ ((__format__ (__printf__, 3, 0)));

DIR *ul_path_opendir(struct path_cxt *pc, const char *path);
DIR *ul_path_vopendirf(struct path_cxt *pc, const char *path, va_list ap)
				__attribute__ ((__format__ (__printf__, 2, 0)));
DIR *ul_path_opendirf(struct path_cxt *pc, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 2, 3)));

ssize_t ul_path_readlink(struct path_cxt *pc, char *buf, size_t bufsiz, const char *path);
ssize_t ul_path_readlinkf(struct path_cxt *pc, char *buf, size_t bufsiz, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 4, 5)));

int ul_path_read(struct path_cxt *pc, char *buf, size_t len, const char *path);
int ul_path_vreadf(struct path_cxt *pc, char *buf, size_t len, const char *path, va_list ap)
				__attribute__ ((__format__ (__printf__, 4, 0)));
int ul_path_readf(struct path_cxt *pc, char *buf, size_t len, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 4, 5)));

int ul_path_read_string(struct path_cxt *pc, char **str, const char *path);
int ul_path_readf_string(struct path_cxt *pc, char **str, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));

int ul_path_read_buffer(struct path_cxt *pc, char *buf, size_t bufsz, const char *path);
int ul_path_readf_buffer(struct path_cxt *pc, char *buf, size_t bufsz, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 4, 5)));

int ul_path_scanf(struct path_cxt *pc, const char *path, const char *fmt, ...)
				__attribute__ ((__format__ (__scanf__, 3, 4)));
int ul_path_scanff(struct path_cxt *pc, const char *path, va_list ap, const char *fmt, ...)
				__attribute__ ((__format__ (__printf__, 2, 0)))
				__attribute__ ((__format__ (__scanf__, 4, 5)));

int ul_path_read_majmin(struct path_cxt *pc, dev_t *res, const char *path);
int ul_path_readf_majmin(struct path_cxt *pc, dev_t *res, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));

int ul_path_read_u32(struct path_cxt *pc, uint32_t *res, const char *path);
int ul_path_readf_u32(struct path_cxt *pc, uint32_t *res, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));

int ul_path_read_s32(struct path_cxt *pc, int32_t *res, const char *path);
int ul_path_readf_s32(struct path_cxt *pc, int32_t *res, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));

int ul_path_read_u64(struct path_cxt *pc, uint64_t *res, const char *path);
int ul_path_readf_u64(struct path_cxt *pc, uint64_t *res, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));

int ul_path_read_s64(struct path_cxt *pc, int64_t *res, const char *path);
int ul_path_readf_s64(struct path_cxt *pc, int64_t *res, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));

int ul_path_write_string(struct path_cxt *pc, const char *str, const char *path);
int ul_path_writef_string(struct path_cxt *pc, const char *str, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));

int ul_path_write_s64(struct path_cxt *pc, int64_t num, const char *path);
int ul_path_write_u64(struct path_cxt *pc, uint64_t num, const char *path);
int ul_path_writef_u64(struct path_cxt *pc, uint64_t num, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 3, 4)));

int ul_path_count_dirents(struct path_cxt *pc, const char *path);
int ul_path_countf_dirents(struct path_cxt *pc, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 2, 3)));

int ul_path_next_dirent(struct path_cxt *pc, DIR **sub, const char *dirname, struct dirent **d);


#ifdef HAVE_CPU_SET_T
# include "cpuset.h"
int ul_path_readf_cpuset(struct path_cxt *pc, cpu_set_t **set, int maxcpus, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 4, 5)));

int ul_path_readf_cpulist(struct path_cxt *pc, cpu_set_t **set, int maxcpus, const char *path, ...)
				__attribute__ ((__format__ (__printf__, 4, 5)));
#endif /* HAVE_CPU_SET_T */
#endif /* UTIL_LINUX_PATH_H */
