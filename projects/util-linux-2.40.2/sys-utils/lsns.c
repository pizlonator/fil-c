/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * lsns(8) - list system namespaces
 *
 * Copyright (C) 2015 Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wchar.h>
#include <libsmartcols.h>
#include <libmount.h>

#ifdef HAVE_LINUX_NET_NAMESPACE_H
# include <stdbool.h>
# include <sys/socket.h>
# include <linux/netlink.h>
# include <linux/rtnetlink.h>
# include <linux/net_namespace.h>
#endif

#ifdef HAVE_LINUX_NSFS_H
# include <linux/nsfs.h>
# if defined(NS_GET_NSTYPE) && defined(NS_GET_OWNER_UID)
#  define USE_NS_GET_API	1
# endif
#endif

#include "pathnames.h"
#include "nls.h"
#include "xalloc.h"
#include "c.h"
#include "list.h"
#include "closestream.h"
#include "optutils.h"
#include "procfs.h"
#include "strutils.h"
#include "namespace.h"
#include "idcache.h"
#include "fileutils.h"

#include "debug.h"

static UL_DEBUG_DEFINE_MASK(lsns);
UL_DEBUG_DEFINE_MASKNAMES(lsns) = UL_DEBUG_EMPTY_MASKNAMES;

#define LSNS_DEBUG_INIT		(1 << 1)
#define LSNS_DEBUG_PROC		(1 << 2)
#define LSNS_DEBUG_NS		(1 << 3)
#define LSNS_DEBUG_ALL		0xFFFF

#define LSNS_NETNS_UNUSABLE -2

#define DBG(m, x)       __UL_DBG(lsns, LSNS_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(lsns, LSNS_DEBUG_, m, x)

#define lsns_ioctl(fildes, request, ...) __extension__ ({ \
	int ret = ioctl(fildes, request, ##__VA_ARGS__); \
	if (ret == -1 && (errno == ENOTTY || errno == ENOSYS))	\
		warnx("Unsupported ioctl %s", #request); \
	ret; })

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(lsns)
#include "debugobj.h"

#define EXIT_UNSUPPORTED_IOCTL 2

static struct idcache *uid_cache = NULL;

/* column IDs */
enum {
	COL_NS = 0,
	COL_TYPE,
	COL_PATH,
	COL_NPROCS,
	COL_PID,
	COL_PPID,
	COL_COMMAND,
	COL_UID,
	COL_USER,
	COL_NETNSID,
	COL_NSFS,
	COL_PNS,		/* parent namespace */
	COL_ONS,		/* owner namespace */
};

/* column names */
struct colinfo {
	const char *name; /* header */
	double	   whint; /* width hint (N < 1 is in percent of termwidth) */
	int	   flags; /* SCOLS_FL_* */
	const char *help;
	int        json_type;
};

/* columns descriptions */
static const struct colinfo infos[] = {
	[COL_NS]      = { "NS",     10, SCOLS_FL_RIGHT, N_("namespace identifier (inode number)"), SCOLS_JSON_NUMBER },
	[COL_TYPE]    = { "TYPE",    5, 0, N_("kind of namespace") },
	[COL_PATH]    = { "PATH",    0, 0, N_("path to the namespace")},
	[COL_NPROCS]  = { "NPROCS",  5, SCOLS_FL_RIGHT, N_("number of processes in the namespace"), SCOLS_JSON_NUMBER },
	[COL_PID]     = { "PID",     5, SCOLS_FL_RIGHT, N_("lowest PID in the namespace"), SCOLS_JSON_NUMBER },
	[COL_PPID]    = { "PPID",    5, SCOLS_FL_RIGHT, N_("PPID of the PID"), SCOLS_JSON_NUMBER },
	[COL_COMMAND] = { "COMMAND", 0, SCOLS_FL_TRUNC, N_("command line of the PID")},
	[COL_UID]     = { "UID",     0, SCOLS_FL_RIGHT, N_("UID of the PID"), SCOLS_JSON_NUMBER},
	[COL_USER]    = { "USER",    0, 0, N_("username of the PID")},
	[COL_NETNSID] = { "NETNSID", 0, SCOLS_FL_RIGHT, N_("namespace ID as used by network subsystem")},
	[COL_NSFS]    = { "NSFS",    0, SCOLS_FL_WRAP, N_("nsfs mountpoint (usually used network subsystem)")},
	[COL_PNS]     = { "PNS",   10, SCOLS_FL_RIGHT, N_("parent namespace identifier (inode number)"), SCOLS_JSON_NUMBER },
	[COL_ONS]     = { "ONS",   10, SCOLS_FL_RIGHT, N_("owner namespace identifier (inode number)"), SCOLS_JSON_NUMBER },
};

static int columns[ARRAY_SIZE(infos) * 2];
static size_t ncolumns;

enum {
	LSNS_ID_MNT = 0,
	LSNS_ID_NET,
	LSNS_ID_PID,
	LSNS_ID_UTS,
	LSNS_ID_IPC,
	LSNS_ID_USER,
	LSNS_ID_CGROUP,
	LSNS_ID_TIME
};

static char *ns_names[] = {
	[LSNS_ID_MNT] = "mnt",
	[LSNS_ID_NET] = "net",
	[LSNS_ID_PID] = "pid",
	[LSNS_ID_UTS] = "uts",
	[LSNS_ID_IPC] = "ipc",
	[LSNS_ID_USER] = "user",
	[LSNS_ID_CGROUP] = "cgroup",
	[LSNS_ID_TIME] = "time"
};

enum {
      RELA_PARENT,
      RELA_OWNER,
      MAX_RELA
};

struct lsns_namespace {
	ino_t id;
	int type;			/* LSNS_* */
	int nprocs;
	int netnsid;
	ino_t related_id[MAX_RELA];

	struct lsns_process *proc;

	struct lsns_namespace *related_ns[MAX_RELA];
	struct libscols_line *ns_outline;
	uid_t uid_fallback;	/* refer this member if `proc' is NULL. */

	struct list_head namespaces;	/* lsns->processes member */
	struct list_head processes;	/* head of lsns_process *siblings */
};

struct lsns_process {
	pid_t pid;		/* process PID */
	pid_t ppid;		/* parent's PID */
	pid_t tpid;		/* thread group */
	char state;
	uid_t uid;

	ino_t            ns_ids[ARRAY_SIZE(ns_names)];
	ino_t            ns_pids[ARRAY_SIZE(ns_names)];
	ino_t            ns_oids[ARRAY_SIZE(ns_names)];

	struct list_head ns_siblings[ARRAY_SIZE(ns_names)];

	struct list_head processes;	/* list of processes */

	struct libscols_line *outline;
	struct lsns_process *parent;

	int netnsid;
};


enum {
      LSNS_TREE_NONE,
      LSNS_TREE_PROCESS,
      LSNS_TREE_OWNER,
      LSNS_TREE_PARENT,
};

struct lsns {
	struct list_head processes;
	struct list_head namespaces;

	pid_t	fltr_pid;	/* filter out by PID */
	ino_t	fltr_ns;	/* filter out by namespace */
	int	fltr_types[ARRAY_SIZE(ns_names)];
	int	fltr_ntypes;

	unsigned int raw	: 1,
		     json	: 1,
		     tree	: 2,
		     persist	: 1,
		     no_trunc	: 1,
		     no_headings: 1,
		     no_wrap    : 1;


	struct libmnt_table *tab;
};

struct netnsid_cache {
	ino_t ino;
	int   id;
	struct list_head netnsids;
};

static struct list_head netnsids_cache;

static int netlink_fd = -1;

static void lsns_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(lsns, LSNS_DEBUG_, 0, LSNS_DEBUG);
}

static int ns_name2type(const char *name)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(ns_names); i++) {
		if (strcmp(ns_names[i], name) == 0)
			return i;
	}
	return -1;
}

static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	assert(name);

	for (i = 0; i < ARRAY_SIZE(infos); i++) {
		const char *cn = infos[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static int has_column(int id)
{
	size_t i;

	for (i = 0; i < ncolumns; i++) {
		if (columns[i] == id)
			return 1;
	}
	return 0;
}

static inline int get_column_id(int num)
{
	assert(num >= 0);
	assert((size_t) num < ncolumns);
	assert(columns[num] < (int) ARRAY_SIZE(infos));

	return columns[num];
}

static inline const struct colinfo *get_column_info(unsigned num)
{
	return &infos[ get_column_id(num) ];
}

static int get_ns_ino(int dir, const char *nsname, ino_t *ino, ino_t *pino, ino_t *oino)
{
	struct stat st;
	char path[16];

	snprintf(path, sizeof(path), "ns/%s", nsname);

	if (fstatat(dir, path, &st, 0) != 0)
		return -errno;
	*ino = st.st_ino;

	*pino = 0;
	*oino = 0;

#ifdef USE_NS_GET_API
	int fd, pfd, ofd;
	fd = openat(dir, path, 0);
	if (fd < 0)
		return -errno;
	if (strcmp(nsname, "pid") == 0 || strcmp(nsname, "user") == 0) {
		if ((pfd = lsns_ioctl(fd, NS_GET_PARENT)) < 0) {
			if (errno == EPERM
			    /* On the test platforms, "build (qemu-user, s390x)" and
			     * "build (qemu-user, riscv64)", the ioctl reported ENOSYS.
			     */
			    || errno == ENOSYS)
				goto user;
			close(fd);
			return -errno;
		}
		if (fstat(pfd, &st) < 0) {
			close(pfd);
			close(fd);
			return -errno;
		}
		*pino = st.st_ino;
		close(pfd);
	}
 user:
	if ((ofd = lsns_ioctl(fd, NS_GET_USERNS)) < 0) {
		if (errno == EPERM
		    /* On the test platforms, "build (qemu-user, s390x)" and
		     * "build (qemu-user, riscv64)", the ioctl reported ENOSYS.
		     */
		    || errno == ENOSYS)
			goto out;
		close(fd);
		return -errno;
	}
	if (fstat(ofd, &st) < 0) {
		close(ofd);
		close(fd);
		return -errno;
	}
	*oino = st.st_ino;
	close(ofd);
 out:
	close(fd);
#endif
	return 0;
}

static int parse_proc_stat(FILE *fp, pid_t *pid, char *state, pid_t *ppid)
{
	char *line = NULL, *p;
	size_t len = 0;
	int rc;

	if (getline(&line, &len, fp) < 0) {
		rc = -errno;
		goto error;
	}

	p = strrchr(line, ')');
	if (p == NULL ||
	    sscanf(line, "%d (", pid) != 1 ||
	    sscanf(p, ") %c %d*[^\n]", state, ppid) != 2) {
		rc = -EINVAL;
		goto error;
	}
	rc = 0;

error:
	free(line);
	return rc;
}

#ifdef HAVE_LINUX_NET_NAMESPACE_H
static int netnsid_cache_find(ino_t netino, int *netnsid)
{
	struct list_head *p;

	list_for_each(p, &netnsids_cache) {
		struct netnsid_cache *e = list_entry(p,
						     struct netnsid_cache,
						     netnsids);
		if (e->ino == netino) {
			*netnsid = e->id;
			return 1;
		}
	}

	return 0;
}

static void netnsid_cache_add(ino_t netino, int netnsid)
{
	struct netnsid_cache *e;

	e = xcalloc(1, sizeof(*e));
	e->ino = netino;
	e->id  = netnsid;
	INIT_LIST_HEAD(&e->netnsids);
	list_add(&e->netnsids, &netnsids_cache);
}

static int get_netnsid_via_netlink_send_request(int target_fd)
{
	unsigned char req[NLMSG_SPACE(sizeof(struct rtgenmsg))
			  + RTA_SPACE(sizeof(int32_t))];

	struct nlmsghdr *nlh = (struct nlmsghdr *)req;
	struct rtgenmsg *rt = NLMSG_DATA(req);
	struct rtattr *rta = (struct rtattr *)
		(req + NLMSG_SPACE(sizeof(struct rtgenmsg)));
	int32_t *fd = RTA_DATA(rta);

	nlh->nlmsg_len = sizeof(req);
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_type = RTM_GETNSID;
	rt->rtgen_family = AF_UNSPEC;
	rta->rta_type = NETNSA_FD;
	rta->rta_len = RTA_SPACE(sizeof(int32_t));
	*fd = target_fd;

	if (send(netlink_fd, req, sizeof(req), 0) < 0)
		return -1;
	return 0;
}

static int get_netnsid_via_netlink_recv_response(int *netnsid)
{
	unsigned char res[NLMSG_SPACE(sizeof(struct rtgenmsg))
			  + ((RTA_SPACE(sizeof(int32_t))
			      < RTA_SPACE(sizeof(struct nlmsgerr)))
			     ? RTA_SPACE(sizeof(struct nlmsgerr))
			     : RTA_SPACE(sizeof(int32_t)))];
	int rtalen;
	ssize_t reslen;

	struct nlmsghdr *nlh;
	struct rtattr *rta;

	reslen = recv(netlink_fd, res, sizeof(res), 0);
	if (reslen < 0)
		return -1;

	nlh = (struct nlmsghdr *)res;
	if (!(NLMSG_OK(nlh, (size_t)reslen)
	      && nlh->nlmsg_type == RTM_NEWNSID))
		return -1;

	rtalen = NLMSG_PAYLOAD(nlh, sizeof(struct rtgenmsg));
	rta = (struct rtattr *)(res + NLMSG_SPACE(sizeof(struct rtgenmsg)));
	if (!(RTA_OK(rta, rtalen)
	      && rta->rta_type == NETNSA_NSID))
		return -1;

	*netnsid = *(int *)RTA_DATA(rta);

	return 0;
}

static int get_netnsid_via_netlink(int dir, const char *path)
{
	int netnsid;
	int target_fd;

	if (netlink_fd < 0)
		return LSNS_NETNS_UNUSABLE;

	target_fd = openat(dir, path, O_RDONLY);
	if (target_fd < 0)
		return LSNS_NETNS_UNUSABLE;

	if (get_netnsid_via_netlink_send_request(target_fd) < 0) {
		netnsid = LSNS_NETNS_UNUSABLE;
		goto out;
	}

	if (get_netnsid_via_netlink_recv_response(&netnsid) < 0) {
		netnsid = LSNS_NETNS_UNUSABLE;
		goto out;
	}

 out:
	close(target_fd);
	return netnsid;
}

static int get_netnsid(int dir, ino_t netino)
{
	int netnsid;

	if (!netnsid_cache_find(netino, &netnsid)) {
		netnsid = get_netnsid_via_netlink(dir, "ns/net");
		netnsid_cache_add(netino, netnsid);
	}

	return netnsid;
}
#else
static int get_netnsid(int dir __attribute__((__unused__)),
		       ino_t netino __attribute__((__unused__)))
{
	return LSNS_NETNS_UNUSABLE;
}
#endif /* HAVE_LINUX_NET_NAMESPACE_H */

static int read_process(struct lsns *ls, pid_t pid)
{
	struct lsns_process *p = NULL;
	char buf[BUFSIZ];
	DIR *dir;
	int rc = 0, fd;
	FILE *f = NULL;
	size_t i;
	struct stat st;

	DBG(PROC, ul_debug("reading %d", (int) pid));

	snprintf(buf, sizeof(buf), "/proc/%d", pid);
	dir = opendir(buf);
	if (!dir)
		return -errno;

	p = xcalloc(1, sizeof(*p));
	p->netnsid = LSNS_NETNS_UNUSABLE;

	if (fstat(dirfd(dir), &st) == 0) {
		p->uid = st.st_uid;
		add_uid(uid_cache, st.st_uid);
	}

	fd = openat(dirfd(dir), "stat", O_RDONLY);
	if (fd < 0) {
		rc = -errno;
		goto done;
	}
	if (!(f = fdopen(fd, "r"))) {
		rc = -errno;
		goto done;
	}
	rc = parse_proc_stat(f, &p->pid, &p->state, &p->ppid);
	if (rc < 0)
		goto done;
	rc = 0;

	for (i = 0; i < ARRAY_SIZE(p->ns_ids); i++) {
		INIT_LIST_HEAD(&p->ns_siblings[i]);

		if (!ls->fltr_types[i])
			continue;

		rc = get_ns_ino(dirfd(dir), ns_names[i], &p->ns_ids[i],
				&p->ns_pids[i], &p->ns_oids[i]);
		if (rc && rc != -EACCES && rc != -ENOENT)
			goto done;
		if (i == LSNS_ID_NET)
			p->netnsid = get_netnsid(dirfd(dir), p->ns_ids[i]);
		rc = 0;
	}

	INIT_LIST_HEAD(&p->processes);

	DBG(PROC, ul_debugobj(p, "new pid=%d", p->pid));
	list_add_tail(&p->processes, &ls->processes);
done:
	if (f)
		fclose(f);
	closedir(dir);
	if (rc)
		free(p);
	return rc;
}

static int read_processes(struct lsns *ls)
{
	DIR *dir;
	struct dirent *d;
	int rc = 0;

	DBG(PROC, ul_debug("opening /proc"));

	dir = opendir(_PATH_PROC);
	if (!dir)
		return -errno;

	while ((d = xreaddir(dir))) {
		pid_t pid = 0;

		if (procfs_dirent_get_pid(d, &pid) != 0)
			continue;

		/* TODO: use ul_new_procfs_path(pid, NULL) to read files from /proc/pid/
		 */
		rc = read_process(ls, pid);
		if (rc && rc != -EACCES && rc != -ENOENT)
			break;
		rc = 0;
	}

	DBG(PROC, ul_debug("closing /proc"));
	closedir(dir);
	return rc;
}

static struct lsns_namespace *get_namespace(struct lsns *ls, ino_t ino)
{
	struct list_head *p;

	list_for_each(p, &ls->namespaces) {
		struct lsns_namespace *ns = list_entry(p, struct lsns_namespace, namespaces);

		if (ns->id == ino)
			return ns;
	}
	return NULL;
}

static int namespace_has_process(struct lsns_namespace *ns, pid_t pid)
{
	struct list_head *p;

	list_for_each(p, &ns->processes) {
		struct lsns_process *proc = list_entry(p, struct lsns_process, ns_siblings[ns->type]);

		if (proc->pid == pid)
			return 1;
	}
	return 0;
}

static struct lsns_namespace *add_namespace(struct lsns *ls, int type, ino_t ino,
					    ino_t parent_ino, ino_t owner_ino)
{
	struct lsns_namespace *ns = xcalloc(1, sizeof(*ns));

	if (!ns)
		return NULL;

	DBG(NS, ul_debugobj(ns, "new %s[%ju]", ns_names[type], (uintmax_t)ino));

	INIT_LIST_HEAD(&ns->processes);
	INIT_LIST_HEAD(&ns->namespaces);

	ns->type = type;
	ns->id = ino;
	ns->related_id[RELA_PARENT] = parent_ino;
	ns->related_id[RELA_OWNER] = owner_ino;

	list_add_tail(&ns->namespaces, &ls->namespaces);
	return ns;
}

static int add_process_to_namespace(struct lsns *ls, struct lsns_namespace *ns, struct lsns_process *proc)
{
	struct list_head *p;

	DBG(NS, ul_debugobj(ns, "add process [%p] pid=%d to %s[%ju]",
		proc, proc->pid, ns_names[ns->type], (uintmax_t)ns->id));

	list_for_each(p, &ls->processes) {
		struct lsns_process *xproc = list_entry(p, struct lsns_process, processes);

		if (xproc->pid == proc->ppid)		/* my parent */
			proc->parent = xproc;
		else if (xproc->ppid == proc->pid)	/* my child */
			xproc->parent = proc;
	}

	list_add_tail(&proc->ns_siblings[ns->type], &ns->processes);
	ns->nprocs++;

	if (!ns->proc || ns->proc->pid > proc->pid)
		ns->proc = proc;

	return 0;
}

static int cmp_namespaces(struct list_head *a, struct list_head *b,
			  __attribute__((__unused__)) void *data)
{
	struct lsns_namespace *xa = list_entry(a, struct lsns_namespace, namespaces),
			      *xb = list_entry(b, struct lsns_namespace, namespaces);

	return cmp_numbers(xa->id, xb->id);
}

static int netnsid_xasputs(char **str, int netnsid)
{
	if (netnsid >= 0)
		return xasprintf(str, "%d", netnsid);
#ifdef NETNSA_NSID_NOT_ASSIGNED
	if (netnsid == NETNSA_NSID_NOT_ASSIGNED)
		return xasprintf(str, "%s", "unassigned");
#endif
	return 0;
}

#ifdef USE_NS_GET_API
static int clone_type_to_lsns_type(int clone_type)
{
	switch (clone_type) {
	case CLONE_NEWNS:
		return LSNS_ID_MNT;
	case CLONE_NEWCGROUP:
		return LSNS_ID_CGROUP;
	case CLONE_NEWUTS:
		return LSNS_ID_UTS;
	case CLONE_NEWIPC:
		return LSNS_ID_IPC;
	case CLONE_NEWUSER:
		return LSNS_ID_USER;
	case CLONE_NEWPID:
		return LSNS_ID_PID;
	case CLONE_NEWNET:
		return LSNS_ID_NET;
#ifdef CLONE_NEWTIME
	case CLONE_NEWTIME:
		return LSNS_ID_TIME;
#endif
	default:
		return -1;
	}
}

static struct lsns_namespace *add_namespace_for_nsfd(struct lsns *ls, int fd, ino_t ino)
{
	int fd_owner = -1, fd_parent = -1;
	struct stat st_owner, st_parent;
	ino_t ino_owner = 0, ino_parent = 0;
	struct lsns_namespace *ns;
	int clone_type, lsns_type;

	clone_type = lsns_ioctl(fd, NS_GET_NSTYPE);
	if (clone_type < 0)
		return NULL;
	lsns_type = clone_type_to_lsns_type(clone_type);
	if (lsns_type < 0 || ls->fltr_types[lsns_type] == 0)
		return NULL;

	fd_owner = lsns_ioctl(fd, NS_GET_USERNS);
	if (fd_owner < 0)
		goto parent;
	if (fstat(fd_owner, &st_owner) < 0)
		goto parent;
	ino_owner = st_owner.st_ino;

 parent:
	fd_parent = lsns_ioctl(fd, NS_GET_PARENT);
	if (fd_parent < 0)
		goto add_ns;
	if (fstat(fd_parent, &st_parent) < 0)
		goto add_ns;
	ino_parent = st_parent.st_ino;

 add_ns:
	ns = add_namespace(ls, lsns_type, ino, ino_parent, ino_owner);
	lsns_ioctl(fd, NS_GET_OWNER_UID, &ns->uid_fallback);
	add_uid(uid_cache, ns->uid_fallback);

	if ((lsns_type == LSNS_ID_USER || lsns_type == LSNS_ID_PID)
	    && ino_parent != ino && ino_parent != 0) {
		ns->related_ns[RELA_PARENT] = get_namespace(ls, ino_parent);
		if (!ns->related_ns[RELA_PARENT]) {
			ns->related_ns[RELA_PARENT] = add_namespace_for_nsfd(ls, fd_parent, ino_parent);
			if (ino_parent == ino_owner)
				ns->related_ns[RELA_OWNER] = ns->related_ns[RELA_PARENT];
		}
	}

	if (ns->related_ns[RELA_OWNER] == NULL && ino_owner != 0) {
		ns->related_ns[RELA_OWNER] = get_namespace(ls, ino_owner);
		if (!ns->related_ns[RELA_OWNER])
			ns->related_ns[RELA_OWNER] = add_namespace_for_nsfd(ls, fd_owner, ino_owner);
	}

	if (fd_owner >= 0)
		close(fd_owner);
	if (fd_parent >= 0)
		close(fd_parent);

	return ns;
}

static void interpolate_missing_namespaces(struct lsns *ls, struct lsns_namespace *orphan, int rela)
{
	const int cmd[MAX_RELA] = {
		[RELA_PARENT] = NS_GET_PARENT,
		[RELA_OWNER] = NS_GET_USERNS
	};
	char buf[BUFSIZ];
	int fd_orphan, fd_missing;
	struct stat st;

	if (!orphan->proc)
		return;

	orphan->related_ns[rela] = get_namespace(ls, orphan->related_id[rela]);
	if (orphan->related_ns[rela])
		return;

	snprintf(buf, sizeof(buf), "/proc/%d/ns/%s", orphan->proc->pid, ns_names[orphan->type]);
	fd_orphan = open(buf, O_RDONLY);
	if (fd_orphan < 0)
		return;

	fd_missing = lsns_ioctl(fd_orphan, cmd[rela]);
	close(fd_orphan);
	if (fd_missing < 0)
		return;

	if (fstat(fd_missing, &st) < 0
	    || st.st_ino != orphan->related_id[rela]) {
		close(fd_missing);
		return;
	}

	orphan->related_ns[rela] = add_namespace_for_nsfd(ls, fd_missing, orphan->related_id[rela]);
	close(fd_missing);
}

static void read_related_namespaces(struct lsns *ls)
{
	struct list_head *p;
	struct lsns_namespace *orphan[2] = {NULL, NULL};
	int rela;

	list_for_each(p, &ls->namespaces) {
		struct lsns_namespace *ns = list_entry(p, struct lsns_namespace, namespaces);
		struct list_head *pp;
		list_for_each(pp, &ls->namespaces) {
			struct lsns_namespace *pns = list_entry(pp, struct lsns_namespace, namespaces);
			if (ns->type == LSNS_ID_USER
			    || ns->type == LSNS_ID_PID) {
				if (ns->related_id[RELA_PARENT] == pns->id)
					ns->related_ns[RELA_PARENT] = pns;
				if (ns->related_id[RELA_OWNER] == pns->id)
					ns->related_ns[RELA_OWNER] = pns;
				if (ns->related_ns[RELA_PARENT] && ns->related_ns[RELA_OWNER])
					break;
			} else {
				if (ns->related_id[RELA_OWNER] == pns->id) {
					ns->related_ns[RELA_OWNER] = pns;
					break;
				}
			}
		}

		/* lsns scans /proc/[0-9]+ for finding namespaces.
		 * So if a namespace has no process, lsns cannot
		 * find it. Here we call it a missing namespace.
		 *
		 * If the id for a related namesspce is known but
		 * namespace for the id is not found, there must
		 * be orphan namespaces. A missing namespace is an
		 * owner or a parent of the orphan namespace.
		 */
		for (rela = 0; rela < MAX_RELA; rela++) {
			if (ns->related_id[rela] != 0
			    && ns->related_ns[rela] == NULL) {
				ns->related_ns[rela] = orphan[rela];
				orphan[rela] = ns;
			}
		}
	}

	for (rela = 0; rela < MAX_RELA; rela++) {
		while (orphan[rela]) {
			struct lsns_namespace *current = orphan[rela];
			orphan[rela] = orphan[rela]->related_ns[rela];
			current->related_ns[rela] = NULL;
			interpolate_missing_namespaces(ls, current, rela);
		}
	}
}

static int read_persistent_namespaces(struct lsns *ls)
{
	struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_FORWARD);
	struct libmnt_fs *fs = NULL;

	while (mnt_table_next_fs(ls->tab, itr, &fs) == 0) {
		const char *root;
		char *p, *end = NULL;
		ino_t ino;
		int fd;

		if (!mnt_fs_match_fstype(fs, "nsfs"))
			continue;
		root = mnt_fs_get_root(fs);
		if (!root || !(p = strchr(root, '[')))
			continue;

		errno = 0;
		ino = strtoumax(++p, &end, 10);
		if (!end || *end != ']' || errno != 0)
			continue;
		if (get_namespace(ls, ino))
			continue;

		fd = open(mnt_fs_get_target(fs), O_RDONLY);
		if (fd < 0)
			continue;

		add_namespace_for_nsfd(ls, fd, ino);
		close(fd);
	}

	mnt_free_iter(itr);
	return 0;
}

#endif /* USE_NS_GET_API */

static int read_namespaces(struct lsns *ls)
{
	struct list_head *p;

	DBG(NS, ul_debug("reading namespace"));

	list_for_each(p, &ls->processes) {
		size_t i;
		struct lsns_namespace *ns;
		struct lsns_process *proc = list_entry(p, struct lsns_process, processes);

		for (i = 0; i < ARRAY_SIZE(proc->ns_ids); i++) {
			if (proc->ns_ids[i] == 0)
				continue;
			if (!(ns = get_namespace(ls, proc->ns_ids[i]))) {
				ns = add_namespace(ls, i, proc->ns_ids[i],
						   proc->ns_pids[i], proc->ns_oids[i]);
				if (!ns)
					return -ENOMEM;
			}
			add_process_to_namespace(ls, ns, proc);
		}
	}

#ifdef USE_NS_GET_API
	read_persistent_namespaces(ls);

	if (ls->tree == LSNS_TREE_OWNER || ls->tree == LSNS_TREE_PARENT)
		read_related_namespaces(ls);
#endif
	list_sort(&ls->namespaces, cmp_namespaces, NULL);

	return 0;
}

static int is_nsfs_root(struct libmnt_fs *fs, void *data)
{
	if (!mnt_fs_match_fstype(fs, "nsfs") || !mnt_fs_get_root(fs))
		return 0;

	return (strcmp(mnt_fs_get_root(fs), (char *)data) == 0);
}

static int is_path_included(const char *path_set, const char *elt,
			      const char sep)
{
	size_t elt_len;
	size_t path_set_len;
	char *tmp;


	tmp = strstr(path_set, elt);
	if (!tmp)
		return 0;

	elt_len = strlen(elt);
	path_set_len = strlen(path_set);

	/* path_set includes only elt or
	 * path_set includes elt as the first element.
	 */
	if (tmp == path_set
	    && ((path_set_len == elt_len)
		|| (path_set[elt_len] == sep)))
		return 1;

	/* path_set includes elt at the middle
	 * or as the last element.
	 */
	if ((*(tmp - 1) == sep)
	    && ((*(tmp + elt_len) == sep)
		|| (*(tmp + elt_len) == '\0')))
		return 1;

	return 0;
}

static int nsfs_xasputs(char **str,
			struct lsns_namespace *ns,
			struct libmnt_table *tab,
			char sep)
{
	struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_FORWARD);
	char *expected_root;
	struct libmnt_fs *fs = NULL;

	xasprintf(&expected_root, "%s:[%ju]", ns_names[ns->type], (uintmax_t)ns->id);
	*str = NULL;

	while (mnt_table_find_next_fs(tab, itr, is_nsfs_root,
				      expected_root, &fs) == 0) {

		const char *tgt = mnt_fs_get_target(fs);

		if (!*str)
			xasprintf(str, "%s", tgt);

		else if (!is_path_included(*str, tgt, sep)) {
			char *tmp = NULL;

			xasprintf(&tmp, "%s%c%s", *str, sep, tgt);
			free(*str);
			*str = tmp;
		}
	}
	free(expected_root);
	mnt_free_iter(itr);

	return 1;
}
static void add_scols_line(struct lsns *ls, struct libscols_table *table,
			   struct lsns_namespace *ns, struct lsns_process *proc)
{
	size_t i;
	struct libscols_line *line;

	assert(ns);
	assert(table);

	line = scols_table_new_line(table,
			(ls->tree == LSNS_TREE_PROCESS && proc) && proc->parent ? proc->parent->outline:
			(ls->tree == LSNS_TREE_PARENT)  && ns->related_ns[RELA_PARENT] ? ns->related_ns[RELA_PARENT]->ns_outline:
			(ls->tree == LSNS_TREE_OWNER)   && ns->related_ns[RELA_OWNER]  ? ns->related_ns[RELA_OWNER]->ns_outline:
			NULL);
	if (!line) {
		warn(_("failed to add line to output"));
		return;
	}

	for (i = 0; i < ncolumns; i++) {
		char *str = NULL;

		switch (get_column_id(i)) {
		case COL_NS:
			xasprintf(&str, "%ju", (uintmax_t)ns->id);
			break;
		case COL_PID:
			if (proc)
				xasprintf(&str, "%d", (int) proc->pid);
			break;
		case COL_PPID:
			if (proc)
				xasprintf(&str, "%d", (int) proc->ppid);
			break;
		case COL_TYPE:
			xasprintf(&str, "%s", ns_names[ns->type]);
			break;
		case COL_NPROCS:
			xasprintf(&str, "%d", ns->nprocs);
			break;
		case COL_COMMAND:
			if (!proc)
				break;
			str = pid_get_cmdline(proc->pid);
			if (!str)
				str = pid_get_cmdname(proc->pid);
			break;
		case COL_PATH:
			if (!proc)
				break;
			xasprintf(&str, "/proc/%d/ns/%s", (int) proc->pid, ns_names[ns->type]);
			break;
		case COL_UID:
			xasprintf(&str, "%d", proc? (int) proc->uid: (int) ns->uid_fallback);
			break;
		case COL_USER:
			xasprintf(&str, "%s", get_id(uid_cache, proc? proc->uid: ns->uid_fallback)->name);
			break;
		case COL_NETNSID:
			if (!proc)
				break;
			if (ns->type == LSNS_ID_NET)
				netnsid_xasputs(&str, proc->netnsid);
			break;
		case COL_NSFS:
			nsfs_xasputs(&str, ns, ls->tab, ls->no_wrap ? ',' : '\n');
			break;
		case COL_PNS:
			xasprintf(&str, "%ju", (uintmax_t)ns->related_id[RELA_PARENT]);
			break;
		case COL_ONS:
			xasprintf(&str, "%ju", (uintmax_t)ns->related_id[RELA_OWNER]);
			break;
		default:
			break;
		}

		if (str && scols_line_refer_data(line, i, str) != 0)
			err_oom();
	}

	if (ls->tree == LSNS_TREE_OWNER || ls->tree == LSNS_TREE_PARENT)
		ns->ns_outline = line;
	else if (proc)
		proc->outline = line;
}

static struct libscols_table *init_scols_table(struct lsns *ls)
{
	struct libscols_table *tab;
	size_t i;

	tab = scols_new_table();
	if (!tab) {
		warn(_("failed to initialize output table"));
		return NULL;
	}

	scols_table_enable_raw(tab, ls->raw);
	scols_table_enable_json(tab, ls->json);
	scols_table_enable_noheadings(tab, ls->no_headings);

	if (ls->json)
		scols_table_set_name(tab, "namespaces");

	for (i = 0; i < ncolumns; i++) {
		const struct colinfo *col = get_column_info(i);
		int flags = col->flags;
		struct libscols_column *cl;

		if (ls->no_trunc)
		       flags &= ~SCOLS_FL_TRUNC;
		if (ls->tree == LSNS_TREE_PROCESS && get_column_id(i) == COL_COMMAND)
			flags |= SCOLS_FL_TREE;
		if (ls->no_wrap)
			flags &= ~SCOLS_FL_WRAP;
		if ((ls->tree == LSNS_TREE_OWNER || ls->tree == LSNS_TREE_PARENT)
		    && get_column_id(i) == COL_NS) {
			flags |= SCOLS_FL_TREE;
			flags &= ~SCOLS_FL_RIGHT;
		}

		cl = scols_table_new_column(tab, col->name, col->whint, flags);
		if (cl == NULL) {
			warnx(_("failed to initialize output column"));
			goto err;
		}
		if (ls->json)
			scols_column_set_json_type(cl, col->json_type);

		if (!ls->no_wrap && get_column_id(i) == COL_NSFS) {
			scols_column_set_wrapfunc(cl,
						  scols_wrapnl_chunksize,
						  scols_wrapnl_nextchunk,
						  NULL);
			scols_column_set_safechars(cl, "\n");
		}
	}

	return tab;
err:
	scols_unref_table(tab);
	return NULL;
}

static void show_namespace(struct lsns *ls, struct libscols_table *tab,
			   struct lsns_namespace *ns, struct lsns_process *proc)
{
	/*
	 * create a tree from owner->owned and/or parent->child relation
	 */
	if (ls->tree == LSNS_TREE_OWNER
	    && ns->related_ns[RELA_OWNER]
	    && !ns->related_ns[RELA_OWNER]->ns_outline)
		show_namespace(ls, tab, ns->related_ns[RELA_OWNER], ns->related_ns[RELA_OWNER]->proc);
	else if (ls->tree == LSNS_TREE_PARENT) {
		if (ns->related_ns[RELA_PARENT]) {
			if (!ns->related_ns[RELA_PARENT]->ns_outline)
				show_namespace(ls, tab, ns->related_ns[RELA_PARENT], ns->related_ns[RELA_PARENT]->proc);
		}
		else if (ns->related_ns[RELA_OWNER] && !ns->related_ns[RELA_OWNER]->ns_outline)
			show_namespace(ls, tab, ns->related_ns[RELA_OWNER], ns->related_ns[RELA_OWNER]->proc);
	}

	add_scols_line(ls, tab, ns, proc);
}

static int show_namespaces(struct lsns *ls)
{
	struct libscols_table *tab;
	struct list_head *p;
	int rc = 0;

	tab = init_scols_table(ls);
	if (!tab)
		return -ENOMEM;

	list_for_each(p, &ls->namespaces) {
		struct lsns_namespace *ns = list_entry(p, struct lsns_namespace, namespaces);

		if (ls->fltr_pid != 0 && !namespace_has_process(ns, ls->fltr_pid))
			continue;
		if (ls->persist && ns->nprocs != 0)
			continue;

		if (!ns->ns_outline)
			show_namespace(ls, tab, ns, ns->proc);
	}

	scols_print_table(tab);
	scols_unref_table(tab);
	return rc;
}

static void show_process(struct lsns *ls, struct libscols_table *tab,
			 struct lsns_process *proc, struct lsns_namespace *ns)
{
	/*
	 * create a tree from parent->child relation, but only if the parent is
	 * within the same namespace
	 */
	if (ls->tree == LSNS_TREE_PROCESS
	    && proc->parent
	    && !proc->parent->outline
	    && proc->parent->ns_ids[ns->type] == proc->ns_ids[ns->type])
		show_process(ls, tab, proc->parent, ns);

	add_scols_line(ls, tab, ns, proc);
}


static int show_namespace_processes(struct lsns *ls, struct lsns_namespace *ns)
{
	struct libscols_table *tab;
	struct list_head *p;

	tab = init_scols_table(ls);
	if (!tab)
		return -ENOMEM;

	list_for_each(p, &ns->processes) {
		struct lsns_process *proc = list_entry(p, struct lsns_process, ns_siblings[ns->type]);

		if (!proc->outline)
			show_process(ls, tab, proc, ns);
	}


	scols_print_table(tab);
	scols_unref_table(tab);
	return 0;
}

static void free_lsns_process(struct lsns_process *lsns_p)
{
	free(lsns_p);
}

static void free_netnsid_caches(struct netnsid_cache *cache)
{
	free(cache);
}

static void free_lsns_namespace(struct lsns_namespace *lsns_n)
{
	free(lsns_n);
}

static void free_all(struct lsns *ls)
{
	list_free(&ls->processes, struct lsns_process, processes, free_lsns_process);
	list_free(&netnsids_cache, struct netnsid_cache, netnsids, free_netnsid_caches);
	list_free(&ls->namespaces, struct lsns_namespace, namespaces, free_lsns_namespace);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);

	fprintf(out,
		_(" %s [options] [<namespace>]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("List system namespaces.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -J, --json             use JSON output format\n"), out);
	fputs(_(" -l, --list             use list format output\n"), out);
	fputs(_(" -n, --noheadings       don't print headings\n"), out);
	fputs(_(" -o, --output <list>    define which output columns to use\n"), out);
	fputs(_("     --output-all       output all columns\n"), out);
	fputs(_(" -P, --persistent       namespaces without processes\n"), out);
	fputs(_(" -p, --task <pid>       print process namespaces\n"), out);
	fputs(_(" -r, --raw              use the raw output format\n"), out);
	fputs(_(" -u, --notruncate       don't truncate text in columns\n"), out);
	fputs(_(" -W, --nowrap           don't use multi-line representation\n"), out);
	fputs(_(" -t, --type <name>      namespace type (mnt, net, ipc, user, pid, uts, cgroup, time)\n"), out);
	fputs(_(" -T, --tree <rel>       use tree format (parent, owner, or process)\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(24));

	fputs(USAGE_COLUMNS, out);
	for (i = 0; i < ARRAY_SIZE(infos); i++)
		fprintf(out, " %11s  %s\n", infos[i].name, _(infos[i].help));

	fprintf(out, USAGE_MAN_TAIL("lsns(8)"));

	exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[])
{
	struct lsns ls;
	int c, force_list = 0;
	int r = 0;
	char *outarg = NULL;
	enum {
		OPT_OUTPUT_ALL = CHAR_MAX + 1
	};
	static const struct option long_opts[] = {
		{ "json",       no_argument,       NULL, 'J' },
		{ "task",       required_argument, NULL, 'p' },
		{ "help",	no_argument,       NULL, 'h' },
		{ "output",     required_argument, NULL, 'o' },
		{ "output-all", no_argument,       NULL, OPT_OUTPUT_ALL },
		{ "persistent", no_argument,       NULL, 'P' },
		{ "notruncate", no_argument,       NULL, 'u' },
		{ "version",    no_argument,       NULL, 'V' },
		{ "noheadings", no_argument,       NULL, 'n' },
		{ "nowrap",     no_argument,       NULL, 'W' },
		{ "list",       no_argument,       NULL, 'l' },
		{ "raw",        no_argument,       NULL, 'r' },
		{ "type",       required_argument, NULL, 't' },
		{ "tree",       optional_argument, NULL, 'T' },
		{ NULL, 0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{ 'J','r' },
		{ 'P','p' },
		{ 'l','T' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;
	int is_net = 0;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	lsns_init_debug();
	memset(&ls, 0, sizeof(ls));

	INIT_LIST_HEAD(&ls.processes);
	INIT_LIST_HEAD(&ls.namespaces);
	INIT_LIST_HEAD(&netnsids_cache);

	while ((c = getopt_long(argc, argv,
				"JlPp:o:nruhVt:T::W", long_opts, NULL)) != -1) {

		err_exclusive_options(c, long_opts, excl, excl_st);

		switch(c) {
		case 'J':
			ls.json = 1;
			break;
		case 'l':
			force_list = 1;
			break;
		case 'o':
			outarg = optarg;
			break;
		case OPT_OUTPUT_ALL:
			for (ncolumns = 0; ncolumns < ARRAY_SIZE(infos); ncolumns++)
				columns[ncolumns] = ncolumns;
			break;
		case 'P':
			ls.persist = 1;
			break;
		case 'p':
			ls.fltr_pid = strtos32_or_err(optarg, _("invalid PID argument"));
			break;
		case 'n':
			ls.no_headings = 1;
			break;
		case 'r':
			ls.no_wrap = ls.raw = 1;
			break;
		case 'u':
			ls.no_trunc = 1;
			break;
		case 't':
		{
			int type = ns_name2type(optarg);
			if (type < 0)
				errx(EXIT_FAILURE, _("unknown namespace type: %s"), optarg);
			ls.fltr_types[type] = 1;
			ls.fltr_ntypes++;
			if (type == LSNS_ID_NET)
				is_net = 1;
			break;
		}
		case 'W':
			ls.no_wrap = 1;
			break;
		case 'T':
			ls.tree = LSNS_TREE_OWNER;
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				if (strcmp (optarg, "parent") == 0)
					ls.tree = LSNS_TREE_PARENT;
				else if (strcmp (optarg, "process") == 0)
					ls.tree = LSNS_TREE_PROCESS;
				else if (strcmp (optarg, "owner") != 0)
					errx(EXIT_FAILURE, _("unknown tree type: %s"), optarg);
			}
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (!ls.fltr_ntypes) {
		size_t i;

		for (i = 0; i < ARRAY_SIZE(ns_names); i++)
			ls.fltr_types[i] = 1;
	}

	if (optind < argc) {
		if (ls.fltr_pid)
			errx(EXIT_FAILURE, _("--task is mutually exclusive with <namespace>"));
		ls.fltr_ns = strtou64_or_err(argv[optind], _("invalid namespace argument"));
		if (!ls.tree && !force_list)
			ls.tree = LSNS_TREE_PROCESS;

		if (!ncolumns) {
			columns[ncolumns++] = COL_PID;
			columns[ncolumns++] = COL_PPID;
			columns[ncolumns++] = COL_USER;
			columns[ncolumns++] = COL_COMMAND;
		}
	}

	if (!ncolumns) {
		columns[ncolumns++] = COL_NS;
		columns[ncolumns++] = COL_TYPE;
		columns[ncolumns++] = COL_NPROCS;
		columns[ncolumns++] = COL_PID;
		columns[ncolumns++] = COL_USER;
		if (is_net) {
			columns[ncolumns++] = COL_NETNSID;
			columns[ncolumns++] = COL_NSFS;
		}
		columns[ncolumns++] = COL_COMMAND;

		if (!ls.tree && !force_list)
			ls.tree = LSNS_TREE_PROCESS;
	}

#ifndef USE_NS_GET_API
	if (ls.tree && ls.tree != LSNS_TREE_PROCESS)
		errx(EXIT_FAILURE, _("--tree={parent|owner} is unsupported for your system"));
#endif
	if (outarg && string_add_to_idarray(outarg, columns, ARRAY_SIZE(columns),
				  &ncolumns, column_name_to_id) < 0)
		return EXIT_FAILURE;

	scols_init_debug(0);

	uid_cache = new_idcache();
	if (!uid_cache)
		err(EXIT_FAILURE, _("failed to allocate UID cache"));

#ifdef HAVE_LINUX_NET_NAMESPACE_H
	if (has_column(COL_NETNSID))
		netlink_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
#endif
	ls.tab = mnt_new_table_from_file(_PATH_PROC_MOUNTINFO);
	if (!ls.tab)
		err(MNT_EX_FAIL, _("failed to parse %s"), _PATH_PROC_MOUNTINFO);

	r = read_processes(&ls);
	if (!r)
		r = read_namespaces(&ls);
	if (!r) {
		if (ls.fltr_ns) {
			struct lsns_namespace *ns = get_namespace(&ls, ls.fltr_ns);

			if (!ns)
				errx(EXIT_FAILURE, _("not found namespace: %ju"), (uintmax_t) ls.fltr_ns);
			r = show_namespace_processes(&ls, ns);
		} else
			r = show_namespaces(&ls);
	}

	mnt_free_table(ls.tab);
	if (netlink_fd >= 0)
		close(netlink_fd);
	free_idcache(uid_cache);

	free_all(&ls);

	switch (r) {
		case 0: return EXIT_SUCCESS;
		case -ENOTTY: return EXIT_UNSUPPORTED_IOCTL;
		default: return EXIT_FAILURE;
	}
}
