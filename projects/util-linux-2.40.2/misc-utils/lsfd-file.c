/*
 * lsfd(1) - list file descriptors
 *
 * Copyright (C) 2021 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
 *
 * Very generally based on lsof(8) by Victor A. Abell <abe@purdue.edu>
 * It supports multiple OSes. lsfd specializes to Linux.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>

#ifdef HAVE_LINUX_NSFS_H
# include <linux/nsfs.h>
# if defined(NS_GET_NSTYPE)
#  define USE_NS_GET_API	1
#  include <sys/ioctl.h>
# endif
#endif
#include <linux/sched.h>
#include <sys/shm.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>		/* mq_open */

#include "buffer.h"
#include "idcache.h"
#include "strutils.h"

#include "procfs.h"

#include "lsfd.h"
#include "lsfd-pidfd.h"
#include "pidfd-utils.h"

static struct idcache *username_cache;

static size_t pagesize;

static const char *assocstr[N_ASSOCS] = {
	[ASSOC_CWD]       = "cwd",
	[ASSOC_EXE]       = "exe",
	/* "root" appears as user names, too.
	 * So we use "rtd" here instead of "root". */
	[ASSOC_ROOT]      = "rtd",
	[ASSOC_NS_CGROUP] = "cgroup",
	[ASSOC_NS_IPC]    = "ipc",
	[ASSOC_NS_MNT]    = "mnt",
	[ASSOC_NS_NET]    = "net",
	[ASSOC_NS_PID]    = "pid",
	[ASSOC_NS_PID4C]  = "pid4c",
	[ASSOC_NS_TIME]   = "time",
	[ASSOC_NS_TIME4C] = "time4c",
	[ASSOC_NS_USER]   = "user",
	[ASSOC_NS_UTS]    = "uts",
	[ASSOC_MEM]       = "mem",
	[ASSOC_SHM]       = "shm",
};

static const char *strftype(mode_t ftype)
{
	switch (ftype) {
	case S_IFBLK:
		return "BLK";
	case S_IFCHR:
		return "CHR";
	case S_IFDIR:
		return "DIR";
	case S_IFIFO:
		return "FIFO";
	case S_IFLNK:
		return "LINK";
	case S_IFREG:
		return "REG";
	case S_IFSOCK:
		return "SOCK";
	default:
		return "UNKN";
	}
}

extern void lsfd_decode_file_flags(struct ul_buffer *buf, int flags);
static void file_fill_flags_buf(struct ul_buffer *buf, int flags)
{
	lsfd_decode_file_flags(buf, flags);
}

#define does_file_has_fdinfo_alike(file)	\
	((file)->association >= 0		\
	 || (file)->association == -ASSOC_SHM	\
	 || (file)->association == -ASSOC_MEM)

static uint64_t get_map_length(struct file *file)
{
	uint64_t res = 0;

	if (is_association(file, SHM) || is_association(file, MEM))
		res = (file->map_end - file->map_start) / pagesize;

	return res;
}

void decode_source(char *buf, size_t bufsize,
		  unsigned int dev_major, unsigned int dev_minor,
		  enum decode_source_level level)
{
	if (bufsize == 0)
		return;

	buf[0] = '\0';

	if (level & DECODE_SOURCE_FILESYS_BIT) {
		if (dev_major == 0) {
			const char *filesystem = get_nodev_filesystem(dev_minor);
			if (filesystem) {
				xstrncpy(buf, filesystem, bufsize);
				return;
			}
		}
	}

	if (level & DECODE_SOURCE_PARTITION_BIT) {
		dev_t dev = makedev(dev_major, dev_minor);
		const char *partition = get_partition(dev);
		if (partition) {
			xstrncpy(buf, partition, bufsize);
			return;
		}
	}

	if (level & DECODE_SOURCE_MAJMIN_BIT)
		snprintf(buf, bufsize, "%u:%u",
			 dev_major,
			 dev_minor);
}

static char *strnrstr(const char *haystack, const char *needle, size_t needle_len)
{
	char *last = strstr(haystack, needle);
	if (last == NULL)
		return NULL;

	do {
		char *current = strstr(last + needle_len, needle);
		if (current == NULL)
			return last;
		last = current;
	} while (1);
}

static bool file_fill_column(struct proc *proc,
			     struct file *file,
			     struct libscols_line *ln,
			     int column_id,
			     size_t column_index)
{
	char *str = NULL;
	mode_t ftype;
	char buf[BUFSIZ];

	switch(column_id) {
	case COL_COMMAND:
		if (proc->command
		    && scols_line_set_data(ln, column_index, proc->command))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_NAME:
		if (file->name && file->stat.st_nlink == 0) {
			char *d = strnrstr(file->name, "(deleted)",
					   sizeof("(deleted)") - 1);
			if (d) {
				int r;
				*d = '\0';
				r = scols_line_set_data(ln, column_index, file->name);
				*d = '(';
				if (r)
					err(EXIT_FAILURE, _("failed to add output data"));
				return true;
			}
		}
		/* FALL THROUGH */
	case COL_KNAME:
		if (file->name
		    && scols_line_set_data(ln, column_index, file->name))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_STTYPE:
	case COL_TYPE:
		ftype = file->stat.st_mode & S_IFMT;
		if (scols_line_set_data(ln, column_index, strftype(ftype)))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_USER:
		add_uid(username_cache, (int)proc->uid);
		if (scols_line_set_data(ln, column_index,
					get_id(username_cache,
					       (int)proc->uid)->name))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_OWNER:
		add_uid(username_cache, (int)file->stat.st_uid);
		if (scols_line_set_data(ln, column_index,
					get_id(username_cache,
					       (int)file->stat.st_uid)->name))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_DEVTYPE:
		if (scols_line_set_data(ln, column_index,
					"nodev"))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_FD:
		if (!is_opened_file(file))
			return false;
		/* FALL THROUGH */
	case COL_ASSOC:
		if (is_opened_file(file))
			xasprintf(&str, "%d", file->association);
		else {
			int assoc = file->association * -1;
			if (assoc >= N_ASSOCS)
				return false; /* INTERNAL ERROR */
			str = xstrdup(assocstr[assoc]);
		}
		break;
	case COL_INODE:
		xasprintf(&str, "%llu", (unsigned long long)file->stat.st_ino);
		break;
	case COL_SOURCE:
		decode_source(buf, sizeof(buf), major(file->stat.st_dev), minor(file->stat.st_dev),
			      DECODE_SOURCE_FILESYS);
		str = xstrdup(buf);
		break;
	case COL_PARTITION:
		decode_source(buf, sizeof(buf), major(file->stat.st_dev), minor(file->stat.st_dev),
			      DECODE_SOURCE_PARTITION);
		str = xstrdup(buf);
		break;
	case COL_DEV:
	case COL_MAJMIN:
		decode_source(buf, sizeof(buf), major(file->stat.st_dev), minor(file->stat.st_dev),
			      DECODE_SOURCE_MAJMIN);
		str = xstrdup(buf);
		break;
	case COL_RDEV:
		xasprintf(&str, "%u:%u",
			  major(file->stat.st_rdev),
			  minor(file->stat.st_rdev));
		break;
	case COL_PID:
		xasprintf(&str, "%d", (int)proc->leader->pid);
		break;
	case COL_TID:
		xasprintf(&str, "%d", (int)proc->pid);
		break;
	case COL_UID:
		xasprintf(&str, "%d", (int)proc->uid);
		break;
	case COL_FUID:
		xasprintf(&str, "%d", (int)file->stat.st_uid);
		break;
	case COL_SIZE:
		xasprintf(&str, "%jd", (intmax_t)file->stat.st_size);
		break;
	case COL_NLINK:
		xasprintf(&str, "%ju", (uintmax_t)file->stat.st_nlink);
		break;
	case COL_DELETED:
		xasprintf(&str, "%d", file->stat.st_nlink == 0);
		break;
	case COL_KTHREAD:
		xasprintf(&str, "%u", proc->kthread);
		break;
	case COL_MNT_ID:
		xasprintf(&str, "%d", is_opened_file(file)? file->mnt_id: 0);
		break;
	case COL_MODE:
		if (does_file_has_fdinfo_alike(file))
			xasprintf(&str, "%c%c%c",
				  file->mode & S_IRUSR? 'r': '-',
				  file->mode & S_IWUSR? 'w': '-',
				  (is_mapped_file(file)
				   && file->mode & S_IXUSR)? 'x': '-');
		else
			xasprintf(&str, "---");
		break;
	case COL_XMODE: {
		char r, w, x;
		char D = file->stat.st_nlink == 0? 'D': '-';
		char L = file->locked.write? 'L'
			:file->locked.read?  'l'
			:                    '-';
		char m = file->multiplexed? 'm': '-';

		if (does_file_has_fdinfo_alike(file)) {
			r = file->mode & S_IRUSR? 'r': '-';
			w = file->mode & S_IWUSR? 'w': '-';
			x = (is_mapped_file(file)
			     && file->mode & S_IXUSR)? 'x': '-';
		} else
			r = w = x = '-';
		xasprintf(&str, "%c%c%c%c%c%c", r, w, x, D, L, m);
		break;
	}
	case COL_POS:
		xasprintf(&str, "%" PRIu64,
			  (does_file_has_fdinfo_alike(file))? file->pos: 0);
		break;
	case COL_FLAGS: {
		struct ul_buffer buf = UL_INIT_BUFFER;

		if (!is_opened_file(file))
			return true;

		if (file->sys_flags == 0)
			return true;

		file_fill_flags_buf(&buf, file->sys_flags);
		if (ul_buffer_is_empty(&buf))
			return true;
		str = ul_buffer_get_data(&buf, NULL, NULL);
		break;
	}
	case COL_MAPLEN:
		if (!is_mapped_file(file))
			return true;
		xasprintf(&str, "%ju", (uintmax_t)get_map_length(file));
		break;
	default:
		return false;
	}

	if (!str)
		err(EXIT_FAILURE, _("failed to add output data"));
	if (scols_line_refer_data(ln, column_index, str))
		err(EXIT_FAILURE, _("failed to add output data"));
	return true;
}

enum lock_mode {
	LOCK_NONE,
	READ_LOCK,
	WRITE_LOCK,
};

static unsigned int parse_lock_line(const char *line)
{
	char mode[6] = {0};

	/* Exapmles of lines:
	   ----------------------------------------------------
	   1: FLOCK  ADVISORY  READ 2283292 fd:03:26219728 0 EOF
	   1: FLOCK  ADVISORY  WRITE 2283321 fd:03:26219728 0 EOF
	   1: POSIX  ADVISORY  READ 2283190 fd:03:26219728 0 0
	   1: POSIX  ADVISORY  WRITE 2283225 fd:03:26219728 0 0
	   1: OFDLCK ADVISORY  READ -1 fd:03:26219728 0 0
	   1: OFDLCK ADVISORY  WRITE -1 fd:03:26219728 0 0
	   1: LEASE  ACTIVE    WRITE 2328907 fd:03:26219472 0 EOF
	   1: LEASE  ACTIVE    READ 2326777 fd:03:26219472 0 EOF
	   ---------------------------------------------------- */

	if (sscanf(line, "%*d: %*s %*s %5s %*s", mode) != 1)
		return LOCK_NONE;

	if (strcmp(mode, "READ") == 0)
		return READ_LOCK;

	if (strcmp(mode, "WRITE") == 0)
		return WRITE_LOCK;

	return LOCK_NONE;
}

static int file_handle_fdinfo(struct file *file, const char *key, const char* value)
{
	int rc;

	if (strcmp(key, "pos") == 0) {
		rc = ul_strtou64(value, &file->pos, 10);

	} else if (strcmp(key, "flags") == 0) {
		rc = ul_strtou32(value, &file->sys_flags, 8);

	} else if (strcmp(key, "mnt_id") == 0) {
		rc = ul_strtou32(value, &file->mnt_id, 10);

	} else if (strcmp(key, "lock") == 0) {
		switch (parse_lock_line(value)) {
		case READ_LOCK:
			file->locked.read = 1;
			break;
		case WRITE_LOCK:
			file->locked.write = 1;
			break;
		}
		rc = 1;
	} else
		return 0;	/* ignore -- unknown item */

	if (rc < 0)
		return 0;	/* ignore -- parse failed */

	return 1;		/* success */
}

static void file_free_content(struct file *file)
{
	free(file->name);
}

static unsigned long get_minor_for_sysvipc(void)
{
	int id;
	void *start;

	pid_t self = getpid();
	struct path_cxt *pc = NULL;
	char map_file[sizeof("map_files/0000000000000000-ffffffffffffffff")];

	struct stat sb;
	unsigned long m = 0;

	id = shmget(IPC_PRIVATE, pagesize, IPC_CREAT | 0600);
	if (id == -1)
		return 0;

	start = shmat(id, NULL, SHM_RDONLY);
	if (start == (void *) -1) {
		shmctl(id, IPC_RMID, NULL);
		return 0;
	}

	pc = ul_new_path(NULL);
	if (!pc)
		goto out;

	if (procfs_process_init_path(pc, self) != 0)
		goto out;

	snprintf(map_file, sizeof(map_file),
		 "map_files/%lx-%lx", (long)start, (long)start + pagesize);
	if (ul_path_stat(pc, &sb, 0, map_file) < 0)
		goto out;

	m = minor(sb.st_dev);
 out:
	if (pc)
		ul_unref_path(pc);
	shmdt(start);
	shmctl(id, IPC_RMID, NULL);
	return m;
}

static unsigned long get_minor_for_mqueue(void)
{
	mqd_t mq;
	char mq_name[BUFSIZ];
	struct mq_attr attr = {
		.mq_maxmsg = 1,
		.mq_msgsize = 1,
	};

	pid_t self = getpid();
	struct stat sb;

	snprintf(mq_name, sizeof(mq_name), "/.lsfd-mqueue-nodev-test:%d", self);
	mq = mq_open(mq_name, O_CREAT|O_EXCL | O_RDONLY, S_IRUSR | S_IWUSR, &attr);
	if (mq < 0)
		return 0;

	if (fstat((int)mq, &sb) < 0) {
		mq_close(mq);
		mq_unlink(mq_name);
		return 0;
	}

	mq_close(mq);
	mq_unlink(mq_name);
	return minor(sb.st_dev);
}

static unsigned long get_minor_for_pidfs(void)
{
	int fd = pidfd_open(getpid(), 0);
	struct stat sb;
	unsigned long ret = 0;

	if (fd < 0)
		return 0;

	if (fstat(fd, &sb) == 0 && (sb.st_mode & S_IFMT) == S_IFREG)
		ret = minor(sb.st_dev);

	close(fd);
	return ret;
}

static void file_class_initialize(void)
{
	unsigned long m;

	if (!pagesize)
		pagesize = getpagesize();

	username_cache = new_idcache();
	if (!username_cache)
		err(EXIT_FAILURE, _("failed to allocate UID cache"));

	m = get_minor_for_sysvipc();
	if (m)
		add_nodev(m, "tmpfs");

	m = get_minor_for_mqueue();
	if (m)
		add_nodev(m, "mqueue");

	m = get_minor_for_pidfs();
	if (m)
		add_nodev(m, "pidfs");
}

static void file_class_finalize(void)
{
	free_idcache(username_cache);
}

const struct file_class file_class = {
	.super = NULL,
	.size = sizeof(struct file),
	.initialize_class = file_class_initialize,
	.finalize_class = file_class_finalize,
	.fill_column = file_fill_column,
	.handle_fdinfo = file_handle_fdinfo,
	.free_content = file_free_content,
};

/*
 * Regular files on NSFS
 */

struct nsfs_file {
	struct file file;
	int clone_type;
};

static const char *get_ns_type_name(int clone_type)
{
	switch (clone_type) {
#ifdef USE_NS_GET_API
	case CLONE_NEWNS:
		return "mnt";
	case CLONE_NEWCGROUP:
		return "cgroup";
	case CLONE_NEWUTS:
		return "uts";
	case CLONE_NEWIPC:
		return "ipc";
	case CLONE_NEWUSER:
		return "user";
	case CLONE_NEWPID:
		return "pid";
	case CLONE_NEWNET:
		return "net";
#ifdef CLONE_NEWTIME
	case CLONE_NEWTIME:
		return "time";
#endif	/* CLONE_NEWTIME */
#endif	/* USE_NS_GET_API */
	default:
		return "unknown";
	}
}

static void init_nsfs_file_content(struct file *file)
{
	struct nsfs_file *nsfs_file = (struct nsfs_file *)file;
	nsfs_file->clone_type = -1;

#ifdef USE_NS_GET_API
	char *proc_fname = NULL;
	int ns_fd;
	int ns_type;

	if (is_association(file, NS_CGROUP))
		nsfs_file->clone_type = CLONE_NEWCGROUP;
	else if (is_association(file, NS_IPC))
		nsfs_file->clone_type = CLONE_NEWIPC;
	else if (is_association(file, NS_MNT))
		nsfs_file->clone_type = CLONE_NEWNS;
	else if (is_association(file, NS_NET))
		nsfs_file->clone_type = CLONE_NEWNET;
	else if (is_association(file, NS_PID)
		 || is_association(file, NS_PID4C))
		nsfs_file->clone_type = CLONE_NEWPID;
#ifdef CLONE_NEWTIME
	else if (is_association(file, NS_TIME)
		 || is_association(file, NS_TIME4C))
		nsfs_file->clone_type = CLONE_NEWTIME;
#endif
	else if (is_association(file, NS_USER))
		nsfs_file->clone_type = CLONE_NEWUSER;
	else if (is_association(file, NS_UTS))
		nsfs_file->clone_type = CLONE_NEWUTS;

	if (nsfs_file->clone_type != -1)
		return;

	if (!is_opened_file(file))
		return;

	if (!file->name)
		return;

	xasprintf(&proc_fname, "/proc/%d/fd/%d",
		  file->proc->pid, file->association);
	ns_fd = open(proc_fname, O_RDONLY);
	free(proc_fname);
	if (ns_fd < 0)
		return;

	ns_type = ioctl(ns_fd, NS_GET_NSTYPE);
	close(ns_fd);
	if (ns_type < 0)
		return;

	nsfs_file->clone_type = ns_type;
#endif	/* USE_NS_GET_API */
}


static bool nsfs_file_fill_column(struct proc *proc __attribute__((__unused__)),
				  struct file *file,
				  struct libscols_line *ln,
				  int column_id,
				  size_t column_index)
{
	struct nsfs_file *nsfs_file = (struct nsfs_file *)file;
	char *name = NULL;

	if (nsfs_file->clone_type == -1)
		return false;

	switch (column_id) {
	case COL_NS_NAME:
		xasprintf(&name, "%s:[%llu]",
			  get_ns_type_name(nsfs_file->clone_type),
			  (unsigned long long)file->stat.st_ino);
		break;
	case COL_NS_TYPE:
		if (scols_line_set_data(ln, column_index,
					get_ns_type_name(nsfs_file->clone_type)))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	default:
		return false;
	}

	if (name && scols_line_refer_data(ln, column_index, name))
		err(EXIT_FAILURE, _("failed to add output data"));

	return true;
}

const struct file_class nsfs_file_class = {
	.super = &file_class,
	.size = sizeof(struct nsfs_file),
	.initialize_class = NULL,
	.finalize_class = NULL,
	.initialize_content = init_nsfs_file_content,
	.free_content = NULL,
	.fill_column = nsfs_file_fill_column,
	.handle_fdinfo = NULL,
};

/*
 * POSIX Mqueue
 */
struct mqueue_file {
	struct file file;
	struct ipc_endpoint endpoint;
};

struct mqueue_file_ipc {
	struct ipc ipc;
	ino_t ino;
};

bool is_mqueue_dev(dev_t dev)
{
	const char *fs = get_nodev_filesystem(minor(dev));

	if (fs && (strcmp (fs, "mqueue") == 0))
		return true;

	return false;
}

static inline char *mqueue_file_xstrendpoint(struct file *file)
{
	char *str = NULL;
	xasprintf(&str, "%d,%s,%d%c%c",
		  file->proc->pid, file->proc->command, file->association,
		  (file->mode & S_IRUSR)? 'r': '-',
		  (file->mode & S_IWUSR)? 'w': '-');
	return str;
}

static bool mqueue_file_fill_column(struct proc *proc __attribute__((__unused__)),
				    struct file *file __attribute__((__unused__)),
				    struct libscols_line *ln,
				    int column_id,
				    size_t column_index)
{
	switch (column_id) {
	case COL_TYPE:
		if (scols_line_set_data(ln, column_index, "mqueue"))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_ENDPOINTS: {
		char *str = NULL;
		struct mqueue_file *this = (struct mqueue_file *)file;
		struct list_head *e;
		foreach_endpoint(e, this->endpoint) {
			char *estr;
			struct mqueue_file *other = list_entry(e, struct mqueue_file,
							       endpoint.endpoints);
			if (this == other)
				continue;
			if (str)
				xstrputc(&str, '\n');
			estr = mqueue_file_xstrendpoint(&other->file);
			xstrappend(&str, estr);
			free(estr);
		}
		if (!str)
			return false;
		if (scols_line_refer_data(ln, column_index, str))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	}
	default:
		return false;
	}
}

static unsigned int mqueue_file_get_hash(struct file *file)
{
	return (unsigned int)(file->stat.st_ino % UINT_MAX);
}

static bool mqueue_file_is_suitable_ipc(struct ipc *ipc, struct file *file)
{
	return ((struct mqueue_file_ipc *)ipc)->ino == file->stat.st_ino;
}

static const struct ipc_class *mqueue_file_get_ipc_class(struct file *file __attribute__((__unused__)))
{
	static const struct ipc_class mqueue_file_ipc_class = {
		.size = sizeof(struct mqueue_file_ipc),
		.get_hash = mqueue_file_get_hash,
		.is_suitable_ipc = mqueue_file_is_suitable_ipc,
	};
	return &mqueue_file_ipc_class;
}

static void init_mqueue_file_content(struct file *file)
{
	struct mqueue_file *mqueue_file = (struct mqueue_file *)file;
	struct ipc *ipc;
	unsigned int hash;

	init_endpoint(&mqueue_file->endpoint);
	ipc = get_ipc(file);
	if (ipc)
		goto link;

	ipc = new_ipc(mqueue_file_get_ipc_class(file));
	((struct mqueue_file_ipc *)ipc)->ino = file->stat.st_ino;

	hash = mqueue_file_get_hash(file);
	add_ipc(ipc, hash);
 link:
	add_endpoint(&mqueue_file->endpoint, ipc);
}

const struct file_class mqueue_file_class = {
	.super = &file_class,
	.size = sizeof(struct mqueue_file),
	.initialize_content = init_mqueue_file_content,
	.fill_column = mqueue_file_fill_column,
	.get_ipc_class = mqueue_file_get_ipc_class,
};

struct pidfs_file {
	struct file file;
	struct pidfd_data data;
};

static void init_pidfs_file_content(struct file *file)
{
	struct pidfs_file *pidfs_file = (struct pidfs_file *)file;

	memset(&pidfs_file->data, 0, sizeof(pidfs_file->data));
}

static int pidfs_file_handle_fdinfo(struct file *file, const char *key, const char *value)
{
	struct pidfs_file *pidfs_file = (struct pidfs_file *)file;

	return pidfd_handle_fdinfo(&pidfs_file->data, key, value);
}

static void pidfs_file_free_content(struct file *file)
{
	struct pidfs_file *pidfs_file = (struct pidfs_file *)file;

	pidfd_free(&pidfs_file->data);
}

static bool pidfs_file_fill_column(struct proc *proc __attribute__((__unused__)),
				   struct file *file,
				   struct libscols_line *ln,
				   int column_id,
				   size_t column_index)
{
	struct pidfs_file *pidfs_file = (struct pidfs_file *)file;
	char *buf = NULL;

	switch(column_id) {
	case COL_TYPE:
		if (scols_line_set_data(ln, column_index, "pidfd"))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_NAME:
		buf = pidfd_get_name(&pidfs_file->data);
		break;
	default:
		if (!pidfd_fill_column(&pidfs_file->data, column_id, &buf))
			return false;
	}

	if (buf &&
	    scols_line_refer_data(ln, column_index, buf))
		err(EXIT_FAILURE, _("failed to add output data"));

	return true;
}

const struct file_class pidfs_file_class = {
	.super = &file_class,
	.size = sizeof(struct pidfs_file),
	.initialize_content = init_pidfs_file_content,
	.handle_fdinfo = pidfs_file_handle_fdinfo,
	.fill_column = pidfs_file_fill_column,
	.free_content = pidfs_file_free_content,
};

bool is_pidfs_dev(dev_t dev)
{
	const char *fs = get_nodev_filesystem(minor(dev));

	if (fs && (strcmp (fs, "pidfs") == 0))
		return true;

	return false;
}
