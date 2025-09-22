/*
 * lsfd(1) - list file descriptors
 *
 * Copyright (C) 2021 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
 *            Karel Zak <kzak@redhat.com>
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

#include <stdio.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <search.h>
#include <poll.h>
#include <sys/select.h>

#include <sys/uio.h>
#include <linux/sched.h>
#include <sys/syscall.h>

#ifdef HAVE_LINUX_KCMP_H
#  include <linux/kcmp.h>
static int kcmp(pid_t pid1, pid_t pid2, int type,
		unsigned long idx1, unsigned long idx2)
{
	return syscall(SYS_kcmp, pid1, pid2, type, idx1, idx2);
}
#else
#  ifndef KCMP_FS
#    define KCMP_FS 0
#  endif
#  ifndef KCMP_VM
#    define KCMP_VM 0
#  endif
#  ifndef KCMP_FILES
#    define KCMP_FILES 0
#  endif
static int kcmp(pid_t pid1 __attribute__((__unused__)),
		pid_t pid2 __attribute__((__unused__)),
		int type __attribute__((__unused__)),
		unsigned long idx1 __attribute__((__unused__)),
		unsigned long idx2 __attribute__((__unused__)))
{
	/* lsfd uses kcmp only for optimization. If the platform doesn't provide
	 * kcmp, just returning an error is acceptable. */
	errno = ENOSYS;
	return -1;
}
#endif

/* See proc(5).
 * Defined in linux/include/linux/sched.h private header file. */
#define PF_KTHREAD		0x00200000	/* I am a kernel thread */

#include "c.h"
#include "list.h"
#include "closestream.h"
#include "column-list-table.h"
#include "strutils.h"
#include "procfs.h"
#include "fileutils.h"
#include "idcache.h"
#include "pathnames.h"

#include "lsfd.h"

UL_DEBUG_DEFINE_MASK(lsfd);
UL_DEBUG_DEFINE_MASKNAMES(lsfd) = UL_DEBUG_EMPTY_MASKNAMES;

static void lsfd_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(lsfd, LSFD_DEBUG_, 0, LSFD_DEBUG);
}

/*
 * /proc/$pid/mountinfo entries
 */
struct nodev {
	struct list_head nodevs;
	unsigned long minor;
	char *filesystem;
};

struct nodev_table {
#define NODEV_TABLE_SIZE 97
	struct list_head tables[NODEV_TABLE_SIZE];
};
static struct nodev_table nodev_table;

struct name_manager {
	struct idcache *cache;
	unsigned long next_id;
};

/*
 * /proc/devices entries
 */
struct devdrv {
	struct list_head devdrvs;
	unsigned long major;
	char *name;
};

static struct list_head chrdrvs;
static struct list_head blkdrvs;

/*
 * IPC table
 */

#define IPC_TABLE_SIZE 997
struct ipc_table {
	struct list_head tables[IPC_TABLE_SIZE];
};

static struct ipc_table ipc_table;

/*
 * Column related stuffs
 */

/* column names */
struct colinfo {
	const char *name;
	double whint;
	int flags;
	int json_type;
	const char *help;
};

/* columns descriptions */
static const struct colinfo infos[] = {
	[COL_AINODECLASS]      = { "AINODECLASS",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("class of anonymous inode") },
	[COL_ASSOC]            = { "ASSOC",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("association between file and process") },
	[COL_BLKDRV]           = { "BLKDRV",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("block device driver name resolved by /proc/devices") },
	[COL_BPF_MAP_ID]       = { "BPF-MAP.ID",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("bpf map id associated with the fd") },
	[COL_BPF_MAP_TYPE]     = { "BPF-MAP.TYPE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("bpf map type (decoded)") },
	[COL_BPF_MAP_TYPE_RAW]= { "BPF-MAP.TYPE.RAW",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("bpf map type (raw)") },
	[COL_BPF_NAME]         = { "BPF.NAME",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("bpf object name") },
	[COL_BPF_PROG_ID]      = { "BPF-PROG.ID",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("bpf program id associated with the fd") },
	[COL_BPF_PROG_TYPE]    = { "BPF-PROG.TYPE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("bpf program type (decoded)") },
	[COL_BPF_PROG_TYPE_RAW]= { "BPF-PROG.TYPE.RAW",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("bpf program type (raw)") },
	[COL_CHRDRV]           = { "CHRDRV",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("character device driver name resolved by /proc/devices") },
	[COL_COMMAND]          = { "COMMAND",
				   0.3, SCOLS_FL_TRUNC, SCOLS_JSON_STRING,
				   N_("command of the process opening the file") },
	[COL_DELETED]          = { "DELETED",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_BOOLEAN,
				   N_("reachability from the file system") },
	[COL_DEV]              = { "DEV",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("ID of device containing file") },
	[COL_DEVTYPE]          = { "DEVTYPE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("device type (blk, char, or nodev)") },
	[COL_ENDPOINTS]        = { "ENDPOINTS",
				   0,   SCOLS_FL_WRAP,  SCOLS_JSON_ARRAY_STRING,
				   N_("IPC endpoints information communicated with the fd") },
	[COL_EVENTFD_ID]       = {"EVENTFD.ID",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("eventfd ID") },
	[COL_EVENTPOLL_TFDS]   = {"EVENTPOLL.TFDS",
				   0,   SCOLS_FL_WRAP,  SCOLS_JSON_ARRAY_NUMBER,
				   N_("file descriptors targeted by the eventpoll file") },
	[COL_FD]               = { "FD",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("file descriptor for the file") },
	[COL_FLAGS]            = { "FLAGS",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("flags specified when opening the file") },
	[COL_FUID]             = { "FUID",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("user ID number of the file's owner") },
	[COL_INET_LADDR]       = { "INET.LADDR",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("local IP address") },
	[COL_INET_RADDR]       = { "INET.RADDR",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("remote IP address") },
	[COL_INET6_LADDR]      = { "INET6.LADDR",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("local IPv6 address") },
	[COL_INET6_RADDR]      = { "INET6.RADDR",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("remote IPv6 address") },
	[COL_INODE]            = { "INODE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("inode number") },
	[COL_INOTIFY_INODES]   = { "INOTIFY.INODES",
				   0,   SCOLS_FL_WRAP,  SCOLS_JSON_ARRAY_STRING,
				   N_("list of monitoring inodes (cooked)") },
	[COL_INOTIFY_INODES_RAW]={ "INOTIFY.INODES.RAW",
				   0,   SCOLS_FL_WRAP,  SCOLS_JSON_ARRAY_STRING,
				   N_("list of monitoring inodes (raw, don't decode devices)") },
	[COL_KNAME]            = { "KNAME",
				   0.4, SCOLS_FL_TRUNC, SCOLS_JSON_STRING,
				   N_("name of the file (raw)") },
	[COL_KTHREAD]          = { "KTHREAD",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_BOOLEAN,
				   N_("opened by a kernel thread") },
	[COL_MAJMIN]           = { "MAJ:MIN",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("device ID for special, or ID of device containing file") },
	[COL_MAPLEN]           = { "MAPLEN",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("length of file mapping (in page)") },
	[COL_MISCDEV]          = { "MISCDEV",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("misc character device name resolved by /proc/misc") },
	[COL_MNT_ID]           = { "MNTID",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("mount id") },
	[COL_MODE]             = { "MODE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("access mode (rwx)") },
	[COL_NAME]             = { "NAME",
				   0.4, SCOLS_FL_TRUNC, SCOLS_JSON_STRING,
				   N_("name of the file (cooked)") },
	[COL_NETLINK_GROUPS]   = { "NETLINK.GROUPS",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("netlink multicast groups") },
	[COL_NETLINK_LPORT]    = { "NETLINK.LPORT",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("netlink local port id") },
	[COL_NETLINK_PROTOCOL] = { "NETLINK.PROTOCOL",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("netlink protocol") },
	[COL_NLINK]            = { "NLINK",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("link count") },
	[COL_NS_NAME]          = { "NS.NAME",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("name of the namespace (NS.TYPE:[INODE])") },
	[COL_NS_TYPE]          = { "NS.TYPE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("type of the namespace") },
	[COL_OWNER]            = { "OWNER",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("owner of the file") },
	[COL_PACKET_IFACE]     = { "PACKET.IFACE",
				   0,   SCOLS_FL_RIGHT,SCOLS_JSON_STRING,
				   N_("net interface associated with the packet socket") },
	[COL_PACKET_PROTOCOL]  = { "PACKET.PROTOCOL",
				   0,   SCOLS_FL_RIGHT,SCOLS_JSON_STRING,
				   N_("L3 protocol associated with the packet socket") },
	[COL_PARTITION]        = { "PARTITION",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("block device name resolved by /proc/partition") },
	[COL_PID]              = { "PID",
				   5,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("PID of the process opening the file") },
	[COL_PIDFD_COMM]       = { "PIDFD.COMM",
				   0.2, SCOLS_FL_TRUNC, SCOLS_JSON_STRING,
				   N_("command of the process targeted by the pidfd") },
	[COL_PIDFD_NSPID]      = { "PIDFD.NSPID",
				   0.2, SCOLS_FL_TRUNC, SCOLS_JSON_STRING,
				   N_("NSpid field in fdinfo of the pidfd") },
	[COL_PIDFD_PID]        = { "PIDFD.PID",
				   5,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("PID of the process targeted by the pidfd") },
	[COL_PING_ID]          = { "PING.ID",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("ICMP echo request ID") },
	[COL_POS]              = { "POS",
				   5,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("file position") },
	[COL_PTMX_TTY_INDEX]   = { "PTMX.TTY-INDEX",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("tty index of the counterpart") },
	[COL_RAW_PROTOCOL]     = { "RAW.PROTOCOL",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("protocol number of the raw socket") },
	[COL_RDEV]             = { "RDEV",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("device ID (if special file)") },
	[COL_SIGNALFD_MASK]    = { "SIGNALFD.MASK",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("masked signals") },
	[COL_SIZE]             = { "SIZE",
				   4,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("file size"), },
	[COL_SOCK_LISTENING]   = { "SOCK.LISTENING",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_BOOLEAN,
				   N_("listening socket") },
	[COL_SOCK_NETNS]       = { "SOCK.NETNS",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("inode identifying network namespace where the socket belongs to") },
	[COL_SOCK_PROTONAME]   = { "SOCK.PROTONAME",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("protocol name") },
	[COL_SOCK_SHUTDOWN]    = { "SOCK.SHUTDOWN",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("shutdown state of socket ([-r?][-w?])") },
	[COL_SOCK_STATE]       = { "SOCK.STATE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("state of socket") },
	[COL_SOCK_TYPE]        = { "SOCK.TYPE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("type of socket") },
	[COL_SOURCE]           = { "SOURCE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("file system, partition, or device containing file") },
	[COL_STTYPE]           = { "STTYPE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("file type (raw)") },
	[COL_TCP_LADDR]        = { "TCP.LADDR",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("local TCP address (INET address:TCP port)") },
	[COL_TCP_RADDR]        = { "TCP.RADDR",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("remote TCP address (INET address:TCP port)") },
	[COL_TCP_LPORT]        = { "TCP.LPORT",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("local TCP port") },
	[COL_TCP_RPORT]        = { "TCP.RPORT",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("remote TCP port") },
	[COL_TID]              = { "TID",
				   5,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("thread ID of the process opening the file") },
	[COL_TIMERFD_CLOCKID]  = { "TIMERFD.CLOCKID",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("clockid") },
	[COL_TIMERFD_INTERVAL] = { "TIMERFD.INTERVAL",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_FLOAT,
				   N_("interval") },
	[COL_TIMERFD_REMAINING]= { "TIMERFD.REMAINING",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_FLOAT,
				   N_("remaining time") },
	[COL_TUN_IFACE]        = { "TUN.IFACE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("network interface behind the tun device") },
	[COL_TYPE]             = { "TYPE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("file type (cooked)") },
	[COL_UDP_LADDR]        = { "UDP.LADDR",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("local UDP address (INET address:UDP port)") },
	[COL_UDP_RADDR]        = { "UDP.RADDR",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("remote UDP address (INET address:UDP port)") },
	[COL_UDP_LPORT]        = { "UDP.LPORT",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("local UDP port") },
	[COL_UDP_RPORT]        = { "UDP.RPORT",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("remote UDP port") },
	[COL_UDPLITE_LADDR]    = { "UDPLITE.LADDR",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("local UDPLite address (INET address:UDPLite port)") },
	[COL_UDPLITE_RADDR]    = { "UDPLITE.RADDR",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("remote UDPLite address (INET address:UDPLite port)") },
	[COL_UDPLITE_LPORT]    = { "UDPLITE.LPORT",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("local UDPLite port") },
	[COL_UDPLITE_RPORT]    = { "UDPLITE.RPORT",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("remote UDPLite port") },
	[COL_UID]              = { "UID",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER,
				   N_("user ID number of the process") },
	[COL_UNIX_PATH]        = { "UNIX.PATH",
				   0.4, SCOLS_FL_TRUNC, SCOLS_JSON_STRING,
				   N_("filesystem pathname for UNIX domain socket") },
	[COL_USER]             = { "USER",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("user of the process") },
	[COL_XMODE]            = { "XMODE",
				   0,   SCOLS_FL_RIGHT, SCOLS_JSON_STRING,
				   N_("extended version of MODE (rwxD[Ll]m)") },
};

static const int default_columns[] = {
	COL_COMMAND,
	COL_PID,
	COL_USER,
	COL_ASSOC,
	COL_XMODE,
	COL_TYPE,
	COL_SOURCE,
	COL_MNT_ID,
	COL_INODE,
	COL_NAME,
};

static const int default_threads_columns[] = {
	COL_COMMAND,
	COL_PID,
	COL_TID,
	COL_USER,
	COL_ASSOC,
	COL_XMODE,
	COL_TYPE,
	COL_SOURCE,
	COL_MNT_ID,
	COL_INODE,
	COL_NAME,
};

static int columns[ARRAY_SIZE(infos) * 2] = {-1};
static size_t ncolumns;

static ino_t *mnt_namespaces;
static size_t nspaces;

struct counter_spec {
	struct list_head specs;
	const char *name;
	const char *expr;
};

static const struct counter_spec default_counter_specs[] = {
	{
		.name = N_("processes"),
		.expr = "ASSOC == 'cwd'",
	},
	{
		.name = N_("root owned processes"),
		.expr = "(ASSOC == 'cwd') && (UID == 0)",
	},
	{
		.name = N_("kernel threads"),
		.expr = "(ASSOC == 'cwd') && KTHREAD",
	},
	{
		.name = N_("open files"),
		.expr = "FD >= 0",
	},
	{
		.name = N_("RO open files"),
		.expr = "(FD >= 0) and (MODE == 'r--')",
	},
	{
		.name = N_("WO open files"),
		.expr = "(FD >= 0) and (MODE == '-w-')",
	},
	{
		.name = N_("shared mappings"),
		.expr = "ASSOC == 'shm'",
	},
	{
		.name = N_("RO shared mappings"),
		.expr = "(ASSOC == 'shm') and (MODE == 'r--')",
	},
	{
		.name = N_("WO shared mappings"),
		.expr = "(ASSOC == 'shm') and (MODE == '-w-')",
	},
	{
		.name = N_("regular files"),
		.expr = "(FD >= 0) && (STTYPE == 'REG')",
	},
	{
		.name = N_("directories"),
		.expr = "(FD >= 0) && (STTYPE == 'DIR')",
	},
	{
		.name = N_("sockets"),
		.expr = "(FD >= 0) && (STTYPE == 'SOCK')",
	},
	{
		.name = N_("fifos/pipes"),
		.expr = "(FD >= 0) && (STTYPE == 'FIFO')",
	},
	{
		.name = N_("character devices"),
		.expr = "(FD >= 0) && (STTYPE == 'CHR')",
	},
	{
		.name = N_("block devices"),
		.expr = "(FD >= 0) && (STTYPE == 'BLK')",
	},
	{
		.name = N_("unknown types"),
		.expr = "(FD >= 0) && (STTYPE == 'UNKN')",
	}
};

/* "userdata" used by callback for libsmartcols filter */
struct filler_data {
	struct proc *proc;
	struct file *file;
};

struct lsfd_control {
	struct libscols_table *tb;		/* output */
	struct list_head procs;			/* list of all processes */

	unsigned int	noheadings : 1,
			raw : 1,
			json : 1,
			notrunc : 1,
			threads : 1,
			show_main : 1,		/* print main table */
			show_summary : 1,	/* print summary/counters */
			sockets_only : 1,	/* display only SOCKETS */
			show_xmode : 1;		/* XMODE column is enabled. */

	struct libscols_filter *filter;		/* filter */
	struct libscols_filter **ct_filters;	/* counters (NULL terminated array) */
};

static void *proc_tree;			/* for tsearch/tfind */

static int proc_tree_compare(const void *a, const void *b)
{
	return ((struct proc *)a)->pid - ((struct proc *)b)->pid;
}

struct proc *get_proc(pid_t pid)
{
	struct proc key = { .pid = pid };
	struct proc **node = tfind(&key, &proc_tree, proc_tree_compare);
	if (node)
		return *node;
	return NULL;
}

static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(infos); i++) {
		const char *cn = infos[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static int get_column_id(int num)
{
	assert(num >= 0);
	assert((size_t) num < ncolumns);
	assert(columns[num] < (int) ARRAY_SIZE(infos));

	return columns[num];
}

static const struct colinfo *get_column_info(int num)
{
	return &infos[ get_column_id(num) ];
}

static struct libscols_column *add_column(struct libscols_table *tb,
					  const struct colinfo *col, int extra)
{
	struct libscols_column *cl;
	int flags = col->flags;

	cl = scols_table_new_column(tb, col->name, col->whint, flags | extra);
	if (cl) {
		scols_column_set_json_type(cl, col->json_type);
		if (col->flags & SCOLS_FL_WRAP) {
			scols_column_set_wrapfunc(cl,
						  scols_wrapnl_chunksize,
						  scols_wrapnl_nextchunk,
						  NULL);
			scols_column_set_safechars(cl, "\n");
		}
	}

	return cl;
}

static struct libscols_column *add_column_by_id(struct lsfd_control *ctl,
						int colid, int extra)
{
	struct libscols_column *cl;

	if (ncolumns >= ARRAY_SIZE(columns))
		errx(EXIT_FAILURE, _("too many columns are added via filter expression"));

	assert(colid < LSFD_N_COLS);

	cl = add_column(ctl->tb, infos + colid, extra);
	if (!cl)
		err(EXIT_FAILURE, _("failed to allocate output column"));
	columns[ncolumns++] = colid;

	if (colid == COL_TID)
		ctl->threads = 1;

	return cl;
}

static int has_mnt_ns(ino_t id)
{
	size_t i;

	for (i = 0; i < nspaces; i++) {
		if (mnt_namespaces[i] == id)
			return 1;
	}
	return 0;
}

static void add_mnt_ns(ino_t id)
{
	size_t nmax = 0;

	if (nspaces)
		nmax = (nspaces + 16) / 16 * 16;
	if (nmax <= nspaces + 1) {
		nmax += 16;
		mnt_namespaces = xreallocarray(mnt_namespaces,
					       nmax, sizeof(ino_t));
	}
	mnt_namespaces[nspaces++] = id;
}

static const struct file_class *stat2class(struct stat *sb)
{
	dev_t dev;

	assert(sb);

	switch (sb->st_mode & S_IFMT) {
	case S_IFCHR:
		return &cdev_class;
	case S_IFBLK:
		return &bdev_class;
	case S_IFSOCK:
		return &sock_class;
	case S_IFIFO:
		return &fifo_class;
	case S_IFLNK:
	case S_IFDIR:
		return &file_class;
	case S_IFREG:
		dev = sb->st_dev;
		if (major(dev) != 0)
			return &file_class;

		if (is_nsfs_dev(dev))
			return &nsfs_file_class;

		if (is_mqueue_dev(dev))
			return &mqueue_file_class;

		if (is_pidfs_dev(dev))
			return &pidfs_file_class;

		return &file_class;
	default:
		break;
	}

	return &unkn_class;
}

static struct file *new_file(struct proc *proc, const struct file_class *class)
{
	struct file *file;

	assert(class);
	file = xcalloc(1, class->size);
	file->class = class;

	file->proc = proc;

	INIT_LIST_HEAD(&file->files);
	list_add_tail(&file->files, &proc->files);

	return file;
}

static struct file *copy_file(struct file *old)
{
	struct file *file = xcalloc(1, old->class->size);

	INIT_LIST_HEAD(&file->files);
	file->proc = old->proc;
	list_add_tail(&file->files, &old->proc->files);

	file->class = old->class;
	file->association = old->association;
	file->name = xstrdup(old->name);
	file->stat = old->stat;

	return file;
}

static void file_set_path(struct file *file, struct stat *sb, const char *name, int association)
{
	file->association = association;
	file->name = xstrdup(name);
	file->stat = *sb;
}

static void file_init_content(struct file *file)
{
	if (file->class && file->class->initialize_content)
		file->class->initialize_content(file);
}

static void free_file(struct file *file)
{
	const struct file_class *class = file->class;

	while (class) {
		if (class->free_content)
			class->free_content(file);
		class = class->super;
	}
	free(file);
}


static struct proc *new_proc(pid_t pid, struct proc *leader)
{
	struct proc *proc = xcalloc(1, sizeof(*proc));

	proc->pid  = pid;
	proc->leader = leader? leader: proc;
	proc->command = NULL;

	INIT_LIST_HEAD(&proc->files);
	INIT_LIST_HEAD(&proc->procs);
	INIT_LIST_HEAD(&proc->eventpolls);

	proc->kthread = 0;
	return proc;
}

static void free_proc(struct proc *proc)
{
	list_free(&proc->files, struct file, files, free_file);

	free(proc->command);
	free(proc);
}

static void read_fdinfo(struct file *file, FILE *fdinfo)
{
	char buf[1024];

	while (fgets(buf, sizeof(buf), fdinfo)) {
		const struct file_class *class;
		char *val = strchr(buf, ':');

		if (!val)
			continue;
		*val++ = '\0';	/* terminate key */

		val = (char *) skip_space(val);
		rtrim_whitespace((unsigned char *) val);

		class = file->class;
		while (class) {
			if (class->handle_fdinfo
			    && class->handle_fdinfo(file, buf, val))
				break;
			class = class->super;
		}
	}
}

static struct file *collect_file_symlink(struct path_cxt *pc,
					 struct proc *proc,
					 const char *name,
					 int assoc,
					 bool sockets_only)
{
	char sym[PATH_MAX] = { '\0' };
	struct stat sb;
	struct file *f, *prev;

	if (ul_path_readlink(pc, sym, sizeof(sym), name) < 0)
		return NULL;

	/* The /proc/#/{fd,ns} often contains the same file (e.g. /dev/tty)
	 * more than once. Let's try to reuse the previous file if the real
	 * path is the same to save stat() call.
	 */
	prev = list_last_entry(&proc->files, struct file, files);
	if (prev && prev->name && strcmp(prev->name, sym) == 0) {
		f = copy_file(prev);
		f->association = assoc;
	} else {
		const struct file_class *class;

		if (ul_path_stat(pc, &sb, 0, name) < 0)
			return NULL;

		class = stat2class(&sb);
		if (sockets_only
		    /* A nsfs file is not a socket but the nsfs file can
		     * be used as a entry point to collect information from
		     * other network namespaces. Besed on the information,
		     * various columns of sockets can be filled.
		     */
		    && (class != &sock_class) && (class != &nsfs_file_class))
			return NULL;
		f = new_file(proc, class);
		file_set_path(f, &sb, sym, assoc);
	}

	file_init_content(f);

	if (is_association(f, NS_MNT))
		proc->ns_mnt = f->stat.st_ino;
	else if (is_association(f, NS_NET))
		load_sock_xinfo(pc, name, f->stat.st_ino);

	else if (assoc >= 0) {
		/* file-descriptor based association */
		FILE *fdinfo;

		if (ul_path_stat(pc, &sb, AT_SYMLINK_NOFOLLOW, name) == 0)
			f->mode = sb.st_mode;

		if (is_nsfs_dev(f->stat.st_dev))
			load_sock_xinfo(pc, name, f->stat.st_ino);

		fdinfo = ul_path_fopenf(pc, "r", "fdinfo/%d", assoc);
		if (fdinfo) {
			read_fdinfo(f, fdinfo);
			fclose(fdinfo);
		}
	}

	return f;
}

/* read symlinks from /proc/#/fd
 */
static void collect_fd_files(struct path_cxt *pc, struct proc *proc,
			     bool sockets_only)
{
	DIR *sub = NULL;
	struct dirent *d = NULL;
	char path[sizeof("fd/") + sizeof(stringify_value(UINT64_MAX))];

	while (ul_path_next_dirent(pc, &sub, "fd", &d) == 0) {
		uint64_t num;

		if (ul_strtou64(d->d_name, &num, 10) != 0)	/* only numbers */
			continue;

		snprintf(path, sizeof(path), "fd/%ju", (uintmax_t) num);
		collect_file_symlink(pc, proc, path, num, sockets_only);
	}
}

static void parse_maps_line(struct path_cxt *pc, char *buf, struct proc *proc)
{
	uint64_t start, end, offset, ino;
	unsigned long major, minor;
	enum association assoc = ASSOC_MEM;
	struct stat sb;
	struct file *f, *prev;
	char *path, modestr[5];
	dev_t devno;

	/* read rest of the map */
	if (sscanf(buf, "%"SCNx64		/* start */
			"-%"SCNx64		/* end */
			" %4[^ ]"		/* mode */
			" %"SCNx64		/* offset */
			" %lx:%lx"		/* maj:min */
			" %"SCNu64,		/* inode */

			&start, &end, modestr, &offset,
			&major, &minor, &ino) != 7)
		return;

	/* Skip private anonymous mappings. */
	if (major == 0 && minor == 0 && ino == 0)
		return;

	devno = makedev(major, minor);

	if (modestr[3] == 's')
		assoc = ASSOC_SHM;

	/* The map usually contains the same file more than once, try to reuse
	 * the previous file (if devno and ino are the same) to save stat() call.
	 */
	prev = list_last_entry(&proc->files, struct file, files);

	if (prev && prev->stat.st_dev == devno && prev->stat.st_ino == ino) {
		f = copy_file(prev);
		f->association = -assoc;
	} else if ((path = strchr(buf, '/'))) {
		rtrim_whitespace((unsigned char *) path);
		if (stat(path, &sb) < 0)
			/* If a file is mapped but deleted from the file system,
			 * "stat by the file name" may not work. In that case,
			 */
			goto try_map_files;
		f = new_file(proc, stat2class(&sb));
		file_set_path(f, &sb, path, -assoc);
	} else {
		/* As used in tcpdump, AF_PACKET socket can be mmap'ed. */
		char map_file[sizeof("map_files/0000000000000000-ffffffffffffffff")];
		char sym[PATH_MAX] = { '\0' };

	try_map_files:
		snprintf(map_file, sizeof(map_file), "map_files/%"PRIx64"-%"PRIx64, start, end);
		if (ul_path_stat(pc, &sb, 0, map_file) < 0)
			return;
		if (ul_path_readlink(pc, sym, sizeof(sym), map_file) < 0)
			return;
		f = new_file(proc, stat2class(&sb));
		file_set_path(f, &sb, sym, -assoc);
	}

	if (modestr[0] == 'r')
		f->mode |= S_IRUSR;
	if (modestr[1] == 'w')
		f->mode |= S_IWUSR;
	if (modestr[2] == 'x')
		f->mode |= S_IXUSR;

	f->map_start = start;
	f->map_end = end;
	f->pos = offset;

	file_init_content(f);
}

static void collect_mem_files(struct path_cxt *pc, struct proc *proc)
{
	FILE *fp;
	char buf[BUFSIZ];

	fp = ul_path_fopen(pc, "r", "maps");
	if (!fp)
		return;

	while (fgets(buf, sizeof(buf), fp))
		parse_maps_line(pc, buf, proc);

	fclose(fp);
}

static void collect_outofbox_files(struct path_cxt *pc,
				   struct proc *proc,
				   enum association assocs[],
				   const char *names[],
				   size_t count,
				   bool sockets_only)
{
	size_t i;

	for (i = 0; i < count; i++)
		collect_file_symlink(pc, proc, names[assocs[i]], assocs[i] * -1,
				     sockets_only);
}

static void collect_execve_file(struct path_cxt *pc, struct proc *proc,
				bool sockets_only)
{
	enum association assocs[] = { ASSOC_EXE };
	const char *names[] = {
		[ASSOC_EXE]  = "exe",
	};
	collect_outofbox_files(pc, proc, assocs, names, ARRAY_SIZE(assocs),
			       sockets_only);
}

static void collect_fs_files(struct path_cxt *pc, struct proc *proc,
			     bool sockets_only)
{
	enum association assocs[] = { ASSOC_CWD, ASSOC_ROOT };
	const char *names[] = {
		[ASSOC_CWD]  = "cwd",
		[ASSOC_ROOT] = "root",
	};
	collect_outofbox_files(pc, proc, assocs, names, ARRAY_SIZE(assocs),
			       sockets_only);
}

static void collect_namespace_files_tophalf(struct path_cxt *pc, struct proc *proc)
{
	enum association assocs[] = {
		ASSOC_NS_CGROUP,
		ASSOC_NS_IPC,
		ASSOC_NS_MNT,
	};
	const char *names[] = {
		[ASSOC_NS_CGROUP] = "ns/cgroup",
		[ASSOC_NS_IPC]    = "ns/ipc",
		[ASSOC_NS_MNT]    = "ns/mnt",
	};
	collect_outofbox_files(pc, proc, assocs, names, ARRAY_SIZE(assocs),
			       /* Namespace information is alwasys needed. */
			       false);
}

static void collect_namespace_files_bottomhalf(struct path_cxt *pc, struct proc *proc)
{
	enum association assocs[] = {
		ASSOC_NS_NET,
		ASSOC_NS_PID,
		ASSOC_NS_PID4C,
		ASSOC_NS_TIME,
		ASSOC_NS_TIME4C,
		ASSOC_NS_USER,
		ASSOC_NS_UTS,
	};
	const char *names[] = {
		[ASSOC_NS_NET]    = "ns/net",
		[ASSOC_NS_PID]    = "ns/pid",
		[ASSOC_NS_PID4C]  = "ns/pid_for_children",
		[ASSOC_NS_TIME]   = "ns/time",
		[ASSOC_NS_TIME4C] = "ns/time_for_children",
		[ASSOC_NS_USER]   = "ns/user",
		[ASSOC_NS_UTS]    = "ns/uts",
	};
	collect_outofbox_files(pc, proc, assocs, names, ARRAY_SIZE(assocs),
			       /* Namespace information is alwasys needed. */
			       false);
}

static struct nodev *new_nodev(unsigned long minor, const char *filesystem)
{
	struct nodev *nodev = xcalloc(1, sizeof(*nodev));

	INIT_LIST_HEAD(&nodev->nodevs);
	nodev->minor = minor;
	nodev->filesystem = xstrdup(filesystem);

	return nodev;
}

static void free_nodev(struct nodev *nodev)
{
	free(nodev->filesystem);
	free(nodev);
}

void add_nodev(unsigned long minor, const char *filesystem)
{
	struct nodev *nodev = new_nodev(minor, filesystem);
	unsigned long slot = nodev->minor % NODEV_TABLE_SIZE;

	list_add_tail(&nodev->nodevs, &nodev_table.tables[slot]);
}

static void initialize_nodevs(void)
{
	int i;

	for (i = 0; i < NODEV_TABLE_SIZE; i++)
		INIT_LIST_HEAD(&nodev_table.tables[i]);
}

static void finalize_nodevs(void)
{
	int i;

	for (i = 0; i < NODEV_TABLE_SIZE; i++)
		list_free(&nodev_table.tables[i], struct nodev, nodevs, free_nodev);

	free(mnt_namespaces);
}

const char *get_nodev_filesystem(unsigned long minor)
{
	struct list_head *n;
	int slot = minor % NODEV_TABLE_SIZE;

	list_for_each (n, &nodev_table.tables[slot]) {
		struct nodev *nodev = list_entry(n, struct nodev, nodevs);
		if (nodev->minor == minor)
			return nodev->filesystem;
	}
	return NULL;
}

static void read_mountinfo(FILE *mnt)
{
	/* This can be very long. A line in mountinfo can have more than 3
	 * paths. */
	char line[PATH_MAX * 3 + 256];

	while (fgets(line, sizeof(line), mnt)) {
		unsigned long major, minor;
		char filesystem[256];

		/* 23 61 0:22 / /sys rw,nosuid,nodev,noexec,relatime shared:2 - sysfs sysfs rw,seclabel */
		if(sscanf(line, "%*d %*d %lu:%lu %*s %*s %*s %*[^-] - %255s %*[^\n]",
			  &major, &minor, filesystem) != 3)
			/* 1600 1458 0:55 / / rw,nodev,relatime - overlay overlay rw,context="s... */
			if (sscanf(line, "%*d %*d %lu:%lu %*s %*s %*s - %255s %*[^\n]",
				   &major, &minor, filesystem) != 3)
				continue;

		if (major != 0)
			continue;
		if (get_nodev_filesystem(minor))
			continue;

		add_nodev(minor, filesystem);
	}
}

static void initialize_ipc_table(void)
{
	for (int i = 0; i < IPC_TABLE_SIZE; i++)
		INIT_LIST_HEAD(ipc_table.tables + i);
}

static void free_ipc(struct ipc *ipc)
{
	if (ipc->class->free)
		ipc->class->free(ipc);
	free(ipc);
}

static void finalize_ipc_table(void)
{
	for (int i = 0; i < IPC_TABLE_SIZE; i++)
		list_free(&ipc_table.tables[i], struct ipc, ipcs, free_ipc);
}

struct ipc *new_ipc(const struct ipc_class *class)
{
	struct ipc *ipc = xcalloc(1, class->size);
	ipc->class = class;
	INIT_LIST_HEAD(&ipc->endpoints);
	INIT_LIST_HEAD(&ipc->ipcs);
	return ipc;
}

struct ipc *get_ipc(struct file *file)
{
	int slot;
	struct list_head *e;
	const struct ipc_class *ipc_class;

	if (!file->class->get_ipc_class)
		return NULL;

	ipc_class = file->class->get_ipc_class(file);
	if (!ipc_class)
		return NULL;

	slot = ipc_class->get_hash(file) % IPC_TABLE_SIZE;
	list_for_each (e, &ipc_table.tables[slot]) {
		struct ipc *ipc = list_entry(e, struct ipc, ipcs);
		if (ipc->class != ipc_class)
			continue;
		if (ipc_class->is_suitable_ipc(ipc, file))
			return ipc;
	}
	return NULL;
}

void add_ipc(struct ipc *ipc, unsigned int hash)
{
	int slot = hash % IPC_TABLE_SIZE;
	list_add(&ipc->ipcs, &ipc_table.tables[slot]);
}

void init_endpoint(struct ipc_endpoint *endpoint)
{
	INIT_LIST_HEAD(&endpoint->endpoints);
}

void add_endpoint(struct ipc_endpoint *endpoint, struct ipc *ipc)
{
	endpoint->ipc = ipc;
	list_add(&endpoint->endpoints, &ipc->endpoints);
}


static void fill_column(struct proc *proc,
			struct file *file,
			struct libscols_line *ln,
			int column_id,
			size_t column_index)
{
	const struct file_class *class = file->class;

	while (class) {
		if (class->fill_column
		    && class->fill_column(proc, file, ln,
					  column_id, column_index))
			break;
		class = class->super;
	}
}

static int filter_filler_cb(
		struct libscols_filter *fltr __attribute__((__unused__)),
		struct libscols_line *ln,
		size_t colnum,
		void *userdata)
{
	struct filler_data *fid = (struct filler_data *) userdata;

	fill_column(fid->proc, fid->file, ln, get_column_id(colnum), colnum);
	return 0;
}

static void convert_file(struct proc *proc,
		     struct file *file,
		     struct libscols_line *ln)

{
	size_t i;

	for (i = 0; i < ncolumns; i++) {
		if (scols_line_is_filled(ln, i))
			continue;
		fill_column(proc, file, ln, get_column_id(i), i);
	}
}

static void convert(struct list_head *procs, struct lsfd_control *ctl)
{
	struct list_head *p;

	list_for_each (p, procs) {
		struct proc *proc = list_entry(p, struct proc, procs);
		struct list_head *f;

		list_for_each (f, &proc->files) {
			struct file *file = list_entry(f, struct file, files);
			struct libscols_line *ln = scols_table_new_line(ctl->tb, NULL);
			struct libscols_filter **ct_fltr = NULL;

			if (!ln)
				err(EXIT_FAILURE, _("failed to allocate output line"));
			if (ctl->filter) {
				int status = 0;
				struct filler_data fid = {
					.proc = proc,
					.file = file
				};

				scols_filter_set_filler_cb(ctl->filter,
						filter_filler_cb, (void *) &fid);
				if (scols_line_apply_filter(ln, ctl->filter, &status))
					err(EXIT_FAILURE, _("failed to apply filter"));
				if (status == 0) {
					scols_table_remove_line(ctl->tb, ln);
					continue;
				}
			}

			convert_file(proc, file, ln);

			if (!ctl->ct_filters)
				continue;

			for (ct_fltr = ctl->ct_filters; *ct_fltr; ct_fltr++)
				scols_line_apply_filter(ln, *ct_fltr, NULL);
		}
	}
}

static void delete(struct list_head *procs, struct lsfd_control *ctl)
{
	struct list_head *p;

	list_for_each (p, procs) {
		struct proc *proc = list_entry(p, struct proc, procs);
		tdelete(proc, &proc_tree, proc_tree_compare);
	}
	list_free(procs, struct proc, procs, free_proc);

	scols_unref_table(ctl->tb);
	scols_unref_filter(ctl->filter);

	if (ctl->ct_filters) {
		struct libscols_filter **ct_fltr;
		for (ct_fltr = ctl->ct_filters; *ct_fltr; ct_fltr++)
			scols_unref_filter(*ct_fltr);
		free(ctl->ct_filters);
	}
}

static void emit(struct lsfd_control *ctl)
{
	scols_print_table(ctl->tb);
}


static void initialize_class(const struct file_class *class)
{
	if (class->initialize_class)
		class->initialize_class();
}

static void initialize_classes(void)
{
	initialize_class(&file_class);
	initialize_class(&cdev_class);
	initialize_class(&bdev_class);
	initialize_class(&sock_class);
	initialize_class(&unkn_class);
}

static void finalize_class(const struct file_class *class)
{
	if (class->finalize_class)
		class->finalize_class();
}

static void finalize_classes(void)
{
	finalize_class(&file_class);
	finalize_class(&cdev_class);
	finalize_class(&bdev_class);
	finalize_class(&sock_class);
	finalize_class(&unkn_class);
}

static struct devdrv *new_devdrv(unsigned long major, const char *name)
{
	struct devdrv *devdrv = xcalloc(1, sizeof(*devdrv));

	INIT_LIST_HEAD(&devdrv->devdrvs);

	devdrv->major = major;
	devdrv->name = xstrdup(name);

	return devdrv;
}

static void free_devdrv(struct devdrv *devdrv)
{
	free(devdrv->name);
	free(devdrv);
}

#define READ_DEVICES_LINE_LEN 256
static struct devdrv *read_devdrv(const char *line)
{
	unsigned long major;
	char name[READ_DEVICES_LINE_LEN];

	if (sscanf(line, "%lu %s", &major, name) != 2)
		return NULL;

	return new_devdrv(major, name);
}

static void read_devices(struct list_head *chrdrvs_list,
			 struct list_head *blkdrvs_list, FILE *devices_fp)
{
	char line[READ_DEVICES_LINE_LEN];

	/* Skip to the line "Character devices:". */
	while (fgets(line, sizeof(line), devices_fp)) {
		if (line[0] == 'C')
			break;
		continue;
	}

	while (fgets(line, sizeof(line), devices_fp)) {
		/* Find the blank line before "Block devices:" line. */
		if (line[0] == '\n')
			break;

		/* Read the character device drivers */
		struct devdrv *devdrv = read_devdrv(line);
		if (devdrv)
			list_add_tail(&devdrv->devdrvs, chrdrvs_list);
	}

	/* Skip to the line "Block devices:". */
	while (fgets(line, sizeof(line), devices_fp)) {
		if (line[0] == 'B')
			break;
		continue;
	}

	/* Read the block device drivers */
	while (fgets(line, sizeof(line), devices_fp)) {
		struct devdrv *devdrv = read_devdrv(line);
		if (devdrv)
			list_add_tail(&devdrv->devdrvs, blkdrvs_list);
	}
}

static void initialize_devdrvs(void)
{
	FILE *devices_fp;

	INIT_LIST_HEAD(&chrdrvs);
	INIT_LIST_HEAD(&blkdrvs);

	devices_fp = fopen("/proc/devices", "r");
	if (devices_fp) {
		read_devices(&chrdrvs, &blkdrvs, devices_fp);
		fclose(devices_fp);
	}
}

static void finalize_devdrvs(void)
{
	list_free(&blkdrvs, struct devdrv,  devdrvs, free_devdrv);
	list_free(&chrdrvs, struct devdrv,  devdrvs, free_devdrv);
}

static const char *get_devdrv(struct list_head *devdrvs_list, unsigned long major)
{
	struct list_head *c;
	list_for_each(c, devdrvs_list) {
		struct devdrv *devdrv = list_entry(c, struct devdrv, devdrvs);
		if (devdrv->major == major)
			return devdrv->name;
	}
	return NULL;
}

const char *get_chrdrv(unsigned long major)
{
	return get_devdrv(&chrdrvs, major);
}

const char *get_blkdrv(unsigned long major)
{
	return get_devdrv(&blkdrvs, major);
}

struct name_manager *new_name_manager(void)
{
	struct name_manager *nm = xcalloc(1, sizeof(struct name_manager));

	nm->cache = new_idcache();
	if (!nm->cache)
		err(EXIT_FAILURE, _("failed to allocate an idcache"));

	nm->next_id = 1;	/* 0 is never issued as id. */
	return nm;
}

void free_name_manager(struct name_manager *nm)
{
	free_idcache(nm->cache);
	free(nm);
}

const char *get_name(struct name_manager *nm, unsigned long id)
{
	struct identry *e;

	e = get_id(nm->cache, id);

	return e? e->name: NULL;
}

unsigned long add_name(struct name_manager *nm, const char *name)
{
	struct identry *e = NULL, *tmp;

	for (tmp = nm->cache->ent; tmp; tmp = tmp->next) {
		if (strcmp(tmp->name, name) == 0) {
			e = tmp;
			break;
		}
	}

	if (e)
		return e->id;

	e = xmalloc(sizeof(struct identry));
	e->name = xstrdup(name);
	e->id = nm->next_id++;
	e->next = nm->cache->ent;
	nm->cache->ent = e;

	return e->id;
}

static void walk_threads(struct lsfd_control *ctl, struct path_cxt *pc,
			 pid_t pid, struct proc *proc,
			 void (*cb)(struct lsfd_control *, struct path_cxt *,
				    pid_t, struct proc *))
{
	DIR *sub = NULL;
	pid_t tid = 0;

	while (procfs_process_next_tid(pc, &sub, &tid) == 0) {
		if (tid == pid)
			continue;
		(*cb)(ctl, pc, tid, proc);
	}
}

static int pollfdcmp(const void *a, const void *b)
{
	const struct pollfd *apfd = a, *bpfd = b;

	return apfd->fd - bpfd->fd;
}

static void mark_poll_fds_as_multiplexed(char *buf,
					 pid_t pid, struct proc *proc)
{
	long fds;
	long nfds;

	struct iovec  local;
	struct iovec  remote;
	ssize_t n;

	struct list_head *f;

	if (sscanf(buf, "%lx %lx", &fds, &nfds) != 2)
		return;

	if (nfds == 0)
		return;

	local.iov_len = sizeof(struct pollfd) * nfds;
	local.iov_base = xmalloc(local.iov_len);
	remote.iov_len = local.iov_len;
	remote.iov_base = (void *)fds;

	n = process_vm_readv(pid, &local, 1, &remote, 1, 0);
	if (n < 0 || ((size_t)n) != local.iov_len)
		goto out;

	qsort(local.iov_base, nfds, sizeof(struct pollfd), pollfdcmp);

	list_for_each (f, &proc->files) {
		struct file *file = list_entry(f, struct file, files);
		if (is_opened_file(file) && !file->multiplexed) {
			int fd = file->association;
			if (bsearch(&(struct pollfd){.fd = fd,}, local.iov_base,
				    nfds, sizeof(struct pollfd), pollfdcmp))
				file->multiplexed = 1;
		}
	}

 out:
	free(local.iov_base);
}

static void mark_select_fds_as_multiplexed(char *buf,
					   pid_t pid, struct proc *proc)
{
	long nfds;
	long fds[3];

	struct iovec  local[3];
	fd_set local_set[3];
	struct iovec  remote[3];
	ssize_t n;
	ssize_t expected_n = 0;

	struct list_head *f;

	if (sscanf(buf, "%lx %lx %lx %lx", &nfds, fds + 0, fds + 1, fds + 2) != 4)
		return;

	if (nfds == 0)
		return;

	for (int i = 0; i < 3; i++) {
		/* If the remote address for the fd_set is 0x0, no set is tehre. */
		remote[i].iov_len = local[i].iov_len = fds[i]? sizeof(local_set[i]): 0;
		expected_n += (ssize_t)local[i].iov_len;
		local[i].iov_base = local_set + i;
		remote[i].iov_base = (void *)(fds[i]);
	}

	n = process_vm_readv(pid, local, 3, remote, 3, 0);
	if (n < 0 || n != expected_n)
			return;

	list_for_each (f, &proc->files) {
		struct file *file = list_entry(f, struct file, files);
		if (is_opened_file(file) && !file->multiplexed) {
			int fd = file->association;
			if (nfds <= fd)
				continue;
			if ((fds[0] && FD_ISSET(fd, (fd_set *)local[0].iov_base))
			    || (fds[1] && FD_ISSET(fd, (fd_set *)local[1].iov_base))
			    || (fds[2] && FD_ISSET(fd, (fd_set *)local[2].iov_base)))
				file->multiplexed = 1;
		}
	}
}

static void parse_proc_syscall(struct lsfd_control *ctl __attribute__((__unused__)),
			       struct path_cxt *pc, pid_t pid, struct proc *proc)
{
	char buf[BUFSIZ];
	char *ptr = NULL;
	long scn;

	if (procfs_process_get_syscall(pc, buf, sizeof(buf)) <= 0)
		return;

	errno  = 0;
	scn = strtol(buf, &ptr, 10);
	if (errno)
		return;
	if (scn < 0)
		return;

	switch (scn) {
#ifdef SYS_poll
	case SYS_poll:
		mark_poll_fds_as_multiplexed(ptr, pid, proc);
		break;
#endif
#ifdef SYS_ppoll
	case SYS_ppoll:
		mark_poll_fds_as_multiplexed(ptr, pid, proc);
		break;
#endif
#ifdef SYS_ppoll_time64
	case SYS_ppoll_time64:
		mark_poll_fds_as_multiplexed(ptr, pid, proc);
		break;
#endif

#ifdef SYS_select
	case SYS_select:
		mark_select_fds_as_multiplexed(ptr, pid, proc);
		break;
#endif
#ifdef SYS_pselect6
	case SYS_pselect6:
		mark_select_fds_as_multiplexed(ptr, pid, proc);
		break;
#endif
#ifdef SYS_pselect6_time64
	case SYS_pselect6_time64:
		mark_select_fds_as_multiplexed(ptr, pid, proc);
		break;
#endif
	}
}

static void read_process(struct lsfd_control *ctl, struct path_cxt *pc,
			 pid_t pid, struct proc *leader)
{
	char buf[BUFSIZ];
	struct proc *proc;

	if (procfs_process_init_path(pc, pid) != 0)
		return;

	proc = new_proc(pid, leader);
	proc->command = procfs_process_get_cmdname(pc, buf, sizeof(buf)) > 0 ?
			xstrdup(buf) : xstrdup(_("(unknown)"));
	procfs_process_get_uid(pc, &proc->uid);

	if (procfs_process_get_stat(pc, buf, sizeof(buf)) > 0) {
		char *p;
		unsigned int flags;
		char *pat = NULL;

		/* See proc(5) about the column in the line. */
		xstrappend(&pat, "%*d (");
		for (p = proc->command; *p != '\0'; p++) {
			if (*p == '%')
				xstrappend(&pat, "%%");
			else
				xstrputc(&pat, *p);
		}
		xstrappend(&pat, ") %*c %*d %*d %*d %*d %*d %u %*[^\n]");
		if (sscanf(buf, pat, &flags) == 1)
			proc->kthread = !!(flags & PF_KTHREAD);
		free(pat);
	}
	if (proc->kthread && !ctl->threads) {
		free_proc(proc);
		goto out;
	}

	collect_execve_file(pc, proc, ctl->sockets_only);

	if (proc->pid == proc->leader->pid
	    || kcmp(proc->leader->pid, proc->pid, KCMP_FS, 0, 0) != 0)
		collect_fs_files(pc, proc, ctl->sockets_only);

	/* Reading /proc/$pid/mountinfo is expensive.
	 * mnt_namespaces is a table for avoiding reading mountinfo files
	 * for an identical mnt namespace.
	 *
	 * After reading a mountinfo file for a mnt namespace, we store $mnt_id
	 * identifying the mnt namespace to mnt_namespaces.
	 *
	 * Before reading a mountinfo, we look up the mnt_namespaces with $mnt_id
	 * as a key. If we find the key, we can skip the reading.
	 *
	 * To utilize mnt_namespaces, we need $mnt_id.
	 * So we read /proc/$pid/ns/mnt here. However, we should not read
	 * /proc/$pid/ns/net here. When reading /proc/$pid/ns/net, we need
	 * the information about backing device of "nsfs" file system.
	 * The information is available in a mountinfo file.
	 */

	/* 1/3. read /proc/$pid/ns/mnt */
	if (proc->ns_mnt == 0)
		collect_namespace_files_tophalf(pc, proc);

	/* 2/3. read /proc/$pid/mountinfo */
	if (proc->ns_mnt == 0 || !has_mnt_ns(proc->ns_mnt)) {
		FILE *mnt = ul_path_fopen(pc, "r", "mountinfo");
		if (mnt) {
			read_mountinfo(mnt);
			if (proc->ns_mnt)
				add_mnt_ns(proc->ns_mnt);
			fclose(mnt);
		}
	}

	/* 3/3. read /proc/$pid/ns/{the other than mnt} */
	collect_namespace_files_bottomhalf(pc, proc);

	/* If kcmp is not available,
	 * there is no way to know whether threads share resources.
	 * In such cases, we must pay the costs: call collect_mem_files()
	 * and collect_fd_files().
	 */
	if ((!ctl->sockets_only)
	    && (proc->pid == proc->leader->pid
		|| kcmp(proc->leader->pid, proc->pid, KCMP_VM, 0, 0) != 0))
		collect_mem_files(pc, proc);

	if (proc->pid == proc->leader->pid
	    || kcmp(proc->leader->pid, proc->pid, KCMP_FILES, 0, 0) != 0)
		collect_fd_files(pc, proc, ctl->sockets_only);

	list_add_tail(&proc->procs, &ctl->procs);
	if (tsearch(proc, &proc_tree, proc_tree_compare) == NULL)
		errx(EXIT_FAILURE, _("failed to allocate memory"));

	if (ctl->show_xmode)
		parse_proc_syscall(ctl, pc, pid, proc);

	/* The tasks collecting overwrites @pc by /proc/<task-pid>/. Keep it as
	 * the last path based operation in read_process()
	 */
	if (ctl->threads && leader == NULL)
		walk_threads(ctl, pc, pid, proc, read_process);
	else if (ctl->show_xmode)
		walk_threads(ctl, pc, pid, proc, parse_proc_syscall);

 out:
	/* Let's be careful with number of open files */
	ul_path_close_dirfd(pc);
}

static void parse_pids(const char *str, pid_t **pids, int *count)
{
	long v;
	char *next = NULL;

	if (*str == '\0')
		return;

	errno = 0;
	v = strtol(str, &next, 10);
	if (errno)
		err(EXIT_FAILURE, _("unexpected value for pid specification: %s"), str);
	if (next == str)
		errx(EXIT_FAILURE, _("garbage at the end of pid specification: %s"), str);
	if (v < 0)
		errx(EXIT_FAILURE, _("out of range value for pid specification: %ld"), v);

	(*count)++;
	*pids = xreallocarray(*pids, *count, sizeof(**pids));
	(*pids)[*count - 1] = (pid_t)v;

	while (next && *next != '\0'
	       && (isspace((unsigned char)*next) || *next == ','))
		next++;
	if (*next != '\0')
		parse_pids(next, pids, count);
}

static int pidcmp(const void *a, const void *b)
{
	pid_t pa = *(pid_t *)a;
	pid_t pb = *(pid_t *)b;

	if (pa < pb)
		return -1;
	else if (pa == pb)
		return 0;
	else
		return 1;
}

static void sort_pids(pid_t pids[], const int count)
{
	qsort(pids, count, sizeof(pid_t), pidcmp);
}

static bool member_pids(const pid_t pid, const pid_t pids[], const int count)
{
	return bsearch(&pid, pids, count, sizeof(pid_t), pidcmp)? true: false;
}

static void collect_processes(struct lsfd_control *ctl, const pid_t pids[], int n_pids)
{
	DIR *dir;
	struct dirent *d;
	struct path_cxt *pc = NULL;

	pc = ul_new_path(NULL);
	if (!pc)
		err(EXIT_FAILURE, _("failed to alloc procfs handler"));

	dir = opendir(_PATH_PROC);
	if (!dir)
		err(EXIT_FAILURE, _("failed to open /proc"));

	while ((d = readdir(dir))) {
		pid_t pid;

		if (procfs_dirent_get_pid(d, &pid) != 0)
			continue;
		if (n_pids == 0 || member_pids(pid, pids, n_pids))
			read_process(ctl, pc, pid, 0);
	}

	closedir(dir);
	ul_unref_path(pc);
}

static void __attribute__((__noreturn__)) list_colunms(const char *table_name,
						       FILE *out,
						       int raw,
						       int json)
{
	struct libscols_table *col_tb = xcolumn_list_table_new(table_name, out, raw, json);

	for (size_t i = 0; i < ARRAY_SIZE(infos); i++)
		xcolumn_list_table_append_line(col_tb, infos[i].name,
					       infos[i].json_type, "<boolean>",
					       _(infos[i].help));

	scols_print_table(col_tb);
	scols_unref_table(col_tb);

	exit(EXIT_SUCCESS);
}

static void print_columns(FILE *out, const char *prefix, const int cols[], size_t n_cols)
{
	fprintf(out, "%15s: ", prefix);
	for (size_t i = 0; i < n_cols; i++) {
		if (i)
			fputc(',', out);
		fputs(infos[cols[i]].name, out);
	}
	fputc('\n', out);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -l, --threads                list in threads level\n"), out);
	fputs(_(" -J, --json                   use JSON output format\n"), out);
	fputs(_(" -n, --noheadings             don't print headings\n"), out);
	fputs(_(" -o, --output <list>          output columns (see --list-columns)\n"), out);
	fputs(_(" -r, --raw                    use raw output format\n"), out);
	fputs(_(" -u, --notruncate             don't truncate text in columns\n"), out);
	fputs(_(" -p, --pid  <pid(s)>          collect information only specified processes\n"), out);
	fputs(_(" -i[4|6], --inet[=4|=6]       list only IPv4 and/or IPv6 sockets\n"), out);
	fputs(_(" -Q, --filter <expr>          apply display filter\n"), out);
	fputs(_("     --debug-filter           dump the internal data structure of filter and exit\n"), out);
	fputs(_(" -C, --counter <name>:<expr>  define custom counter for --summary output\n"), out);
	fputs(_("     --dump-counters          dump counter definitions\n"), out);
	fputs(_("     --summary[=<when>]       print summary information (only, append, or never)\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(_(" -H, --list-columns           list the available columns\n"), out);
	fprintf(out, USAGE_HELP_OPTIONS(30));

	fputs(USAGE_DEFAULT_COLUMNS, out);
	print_columns(out, _("Default"), default_columns, ARRAY_SIZE(default_columns));
	print_columns(out, _("With --threads"), default_threads_columns, ARRAY_SIZE(default_threads_columns));

	fprintf(out, USAGE_MAN_TAIL("lsfd(1)"));

	exit(EXIT_SUCCESS);
}

static void append_filter_expr(char **a, const char *b, bool and)
{
	if (*a == NULL) {
		*a = xstrdup(b);
		return;
	}

	char *tmp = *a;
	*a = NULL;

	xstrappend(a, "(");
	xstrappend(a, tmp);
	xstrappend(a, ")");
	if (and)
		xstrappend(a, "and(");
	else
		xstrappend(a, "or(");
	xstrappend(a, b);
	xstrappend(a, ")");

	free(tmp);
}

static struct libscols_filter *new_filter(const char *expr, bool debug, struct lsfd_control *ctl)
{
	struct libscols_filter *f;
	struct libscols_iter *itr;
	int nerrs = 0;
	const char *name = NULL;

	f = scols_new_filter(NULL);
	if (!f)
		err(EXIT_FAILURE, _("failed to allocate filter"));
	if (expr && scols_filter_parse_string(f, expr) != 0)
		errx(EXIT_FAILURE, _("failed to parse \"%s\": %s"), expr,
				scols_filter_get_errmsg(f));

	itr = scols_new_iter(SCOLS_ITER_FORWARD);
	if (!itr)
		err(EXIT_FAILURE, _("failed to allocate iterator"));

	while (scols_filter_next_holder(f, itr, &name, 0) == 0) {
		struct libscols_column *col = scols_table_get_column_by_name(ctl->tb, name);

		if (!col) {
			int id = column_name_to_id(name, strlen(name));
			if (id >= 0)
				col = add_column_by_id(ctl, id, SCOLS_FL_HIDDEN);
			if (!col) {
				nerrs++;	/* report all unknown columns */
				continue;
			}
		}
		scols_filter_assign_column(f, itr, name, col);
	}

	scols_free_iter(itr);

	if (debug)
		scols_dump_filter(f, stdout);
	if (nerrs)
		exit(EXIT_FAILURE);
	if (debug)
		exit(EXIT_SUCCESS);

	return f;
}

static struct counter_spec *new_counter_spec(const char *spec_str)
{
	char *sep;
	struct counter_spec *spec;

	if (spec_str[0] == '\0')
		errx(EXIT_FAILURE,
		     _("too short counter specification: -C/--counter %s"),
		     spec_str);
	if (spec_str[0] == ':')
		errx(EXIT_FAILURE,
		     _("no name for counter: -C/--counter %s"),
		     spec_str);

	sep = strchr(spec_str, ':');
	if (sep == NULL)
		errx(EXIT_FAILURE,
		     _("no name for counter: -C/--counter %s"),
		     spec_str);
	if (sep[1] == '\0')
		errx(EXIT_FAILURE,
		     _("empty counter expression given: -C/--counter %s"),
		     spec_str);

	/* Split the spec_str in to name and expr. */
	*sep = '\0';

	if (strchr(spec_str, '{'))
		errx(EXIT_FAILURE,
		     _("don't use `{' in the name of a counter: %s"),
		     spec_str);

	spec = xmalloc(sizeof(struct counter_spec));
	INIT_LIST_HEAD(&spec->specs);
	spec->name = spec_str;
	spec->expr = sep + 1;

	return spec;
}

static void free_counter_spec(struct counter_spec *counter_spec)
{
	free(counter_spec);
}

static struct libscols_filter *new_counter(const struct counter_spec *spec, struct lsfd_control *ctl)
{
	struct libscols_filter *f;
	struct libscols_counter *ct;

	f = new_filter(spec->expr, false, ctl);

	ct = scols_filter_new_counter(f);
	if (!ct)
		err(EXIT_FAILURE, _("failed to allocate counter"));

	scols_counter_set_name(ct, spec->name);
	scols_counter_set_func(ct, SCOLS_COUNTER_COUNT);

	return f;
}

static struct libscols_filter **new_counters(struct list_head *specs, struct lsfd_control *ctl)
{
	struct libscols_filter **ct_filters;
	size_t len = list_count_entries(specs);
	size_t i = 0;
	struct list_head *s;

	ct_filters = xcalloc(len + 1, sizeof(struct libscols_filter *));
	list_for_each(s, specs) {
		struct counter_spec *spec = list_entry(s, struct counter_spec, specs);
		ct_filters[i++] = new_counter(spec, ctl);
	}
	assert(ct_filters[len] == NULL);

	return ct_filters;
}

static struct libscols_filter **new_default_counters(struct lsfd_control *ctl)
{
	struct libscols_filter **ct_filters;
	size_t len = ARRAY_SIZE(default_counter_specs);
	size_t i;

	ct_filters = xcalloc(len + 1, sizeof(struct libscols_filter *));
	for (i = 0; i < len; i++) {
		const struct counter_spec *spec = default_counter_specs + i;
		ct_filters[i] = new_counter(spec, ctl);
	}
	assert(ct_filters[len] == NULL);

	return ct_filters;
}

static void dump_default_counter_specs(void)
{
	size_t len = ARRAY_SIZE(default_counter_specs);
	size_t i;

	puts("default counter specs:");
	for (i = 0; i < len; i++) {
		const struct counter_spec *spec = default_counter_specs + i;
		printf("\t%s:%s\n", spec->name, spec->expr);
	}
}

static void dump_counter_specs(struct list_head *specs)
{
	struct list_head *s;

	puts("custom counter specs:");
	list_for_each(s, specs) {
		struct counter_spec *spec = list_entry(s, struct counter_spec, specs);
		printf("\t%s:%s\n", spec->name, spec->expr);
	}
}

static struct libscols_table *new_summary_table(struct lsfd_control *ctl)
{
	struct libscols_table *tb = scols_new_table();

	struct libscols_column *name_cl, *value_cl;

	if (!tb)
		err(EXIT_FAILURE, _("failed to allocate summary table"));

	scols_table_enable_noheadings(tb, ctl->noheadings);
	scols_table_enable_raw(tb, ctl->raw);
	scols_table_enable_json(tb, ctl->json);

	if(ctl->json)
		scols_table_set_name(tb, "lsfd-summary");


	value_cl = scols_table_new_column(tb, _("VALUE"), 0, SCOLS_FL_RIGHT);
	if (!value_cl)
		err(EXIT_FAILURE, _("failed to allocate summary column"));
	if (ctl->json)
		scols_column_set_json_type(value_cl, SCOLS_JSON_NUMBER);

	name_cl = scols_table_new_column(tb, _("COUNTER"), 0, 0);
	if (!name_cl)
		err(EXIT_FAILURE, _("failed to allocate summary column"));
	if (ctl->json)
		scols_column_set_json_type(name_cl, SCOLS_JSON_STRING);

	return tb;
}

static void emit_summary(struct lsfd_control *ctl)
{
	struct libscols_iter *itr;
	struct libscols_filter **ct_fltr;
	struct libscols_table *tb = new_summary_table(ctl);

	itr = scols_new_iter(SCOLS_ITER_FORWARD);

	for (ct_fltr = ctl->ct_filters; *ct_fltr; ct_fltr++) {
		struct libscols_counter *ct = NULL;

		scols_reset_iter(itr, SCOLS_ITER_FORWARD);
		while (scols_filter_next_counter(*ct_fltr, itr, &ct) == 0) {
			char *str = NULL;
			struct libscols_line *ln;

			ln = scols_table_new_line(tb, NULL);
			if (!ln)
				err(EXIT_FAILURE, _("failed to allocate summary line"));

			xasprintf(&str, "%llu", scols_counter_get_result(ct));
			if (scols_line_refer_data(ln, 0, str))
				err(EXIT_FAILURE, _("failed to add summary data"));
			if (scols_line_set_data(ln, 1, scols_counter_get_name(ct)))
				err(EXIT_FAILURE, _("failed to add summary data"));
		}
	}

	scols_free_iter(itr);
	scols_print_table(tb);

	scols_unref_table(tb);
}

static void attach_xinfos(struct list_head *procs)
{
	struct list_head *p;

	list_for_each (p, procs) {
		struct proc *proc = list_entry(p, struct proc, procs);
		struct list_head *f;

		list_for_each (f, &proc->files) {
			struct file *file = list_entry(f, struct file, files);
			if (file->class->attach_xinfo)
				file->class->attach_xinfo(file);
		}
	}
}

static void set_multiplexed_flags(struct list_head *procs)
{
	struct list_head *p;
	list_for_each (p, procs) {
		struct proc *proc = list_entry(p, struct proc, procs);
		struct list_head *f;
		list_for_each (f, &proc->files) {
			struct file *file = list_entry(f, struct file, files);
			if (is_opened_file(file) && !file->multiplexed) {
				int fd = file->association;
				if (is_multiplexed_by_eventpoll(fd, &proc->eventpolls))
					file->multiplexed = 1;
			}
		}
	}
}

/* Filter expressions for implementing -i option.
 *
 * To list up the protocol names, use the following command line
 *
 *   cd linux/net;
 *   find . -type f -exec grep -A 1 --color=auto -nH --null -e 'struct proto .*{' \{\} +
 *
 */
#define INET_SUBEXP_BEGIN "(SOCK.PROTONAME =~ \"^("
#define INET4_REG         "TCP|UDP|RAW|PING|UDP-Lite|SCTP|DCCP|L2TP/IP|SMC"
#define INET6_REG         "TCPv6|UDPv6|RAWv6|PINGv6|UDPLITEv6|SCTPv6|DCCPv6|L2TP/IPv6|SMC6"
#define INET_SUBEXP_END   ")$\")"

static const char *inet4_subexpr = INET_SUBEXP_BEGIN
	INET4_REG
	INET_SUBEXP_END;
static const char *inet6_subexpr = INET_SUBEXP_BEGIN
	INET6_REG
	INET_SUBEXP_END;
static const char *inet46_subexpr = INET_SUBEXP_BEGIN
	INET4_REG "|" INET6_REG
	INET_SUBEXP_END;

int main(int argc, char *argv[])
{
	int c, collist = 0;
	size_t i;
	char *outarg = NULL;
	char  *filter_expr = NULL;
	bool debug_filter = false;
	bool dump_counters = false;
	pid_t *pids = NULL;
	int n_pids = 0;
	struct list_head counter_specs;

	struct lsfd_control ctl = {
		.show_main = 1
	};

	INIT_LIST_HEAD(&counter_specs);

	enum {
		OPT_DEBUG_FILTER = CHAR_MAX + 1,
		OPT_SUMMARY,
		OPT_DUMP_COUNTERS,
	};
	static const struct option longopts[] = {
		{ "noheadings", no_argument, NULL, 'n' },
		{ "output",     required_argument, NULL, 'o' },
		{ "version",    no_argument, NULL, 'V' },
		{ "help",	no_argument, NULL, 'h' },
		{ "json",       no_argument, NULL, 'J' },
		{ "raw",        no_argument, NULL, 'r' },
		{ "threads",    no_argument, NULL, 'l' },
		{ "notruncate", no_argument, NULL, 'u' },
		{ "pid",        required_argument, NULL, 'p' },
		{ "inet",       optional_argument, NULL, 'i' },
		{ "filter",     required_argument, NULL, 'Q' },
		{ "debug-filter",no_argument, NULL, OPT_DEBUG_FILTER },
		{ "summary",    optional_argument, NULL,  OPT_SUMMARY },
		{ "counter",    required_argument, NULL, 'C' },
		{ "dump-counters",no_argument, NULL, OPT_DUMP_COUNTERS },
		{ "list-columns",no_argument, NULL, 'H' },
		{ NULL, 0, NULL, 0 },
	};

	lsfd_init_debug();

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "no:JrVhluQ:p:i::C:sH", longopts, NULL)) != -1) {
		switch (c) {
		case 'n':
			ctl.noheadings = 1;
			break;
		case 'o':
			outarg = optarg;
			break;
		case 'J':
			ctl.json = 1;
			break;
		case 'r':
			ctl.raw = 1;
			break;
		case 'l':
			ctl.threads = 1;
			break;
		case 'u':
			ctl.notrunc = 1;
			break;
		case 'p':
			parse_pids(optarg, &pids, &n_pids);
			break;
		case 'i': {
			const char *subexpr = NULL;

			ctl.sockets_only = 1;
			if (optarg == NULL)
				subexpr = inet46_subexpr;
			else if (strcmp(optarg, "4") == 0)
				subexpr = inet4_subexpr;
			else if (strcmp(optarg, "6") == 0)
				subexpr = inet6_subexpr;
			else
				errx(EXIT_FAILURE,
				     _("unknown -i/--inet argument: %s"),
				     optarg);

			append_filter_expr(&filter_expr, subexpr, true);
			break;
		}
		case 'Q':
			append_filter_expr(&filter_expr, optarg, true);
			break;
		case 'C': {
			struct counter_spec *c = new_counter_spec(optarg);
			list_add_tail(&c->specs, &counter_specs);
			break;
		}
		case OPT_DEBUG_FILTER:
			debug_filter = true;
			break;
		case OPT_SUMMARY:
			if (optarg) {
				if (strcmp(optarg, "never") == 0)
					ctl.show_summary = 0, ctl.show_main = 1;
				else if (strcmp(optarg, "only") == 0)
					ctl.show_summary = 1, ctl.show_main = 0;
				else if (strcmp(optarg, "append") == 0)
					ctl.show_summary = 1, ctl.show_main = 1;
				else
					errx(EXIT_FAILURE, _("unsupported --summary argument"));
			} else
				ctl.show_summary = 1, ctl.show_main = 0;
			break;
		case OPT_DUMP_COUNTERS:
			dump_counters = true;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		case 'H':
			collist = 1;
			break;
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (collist)
		list_colunms("lsfd-columns", stdout, ctl.raw, ctl.json); /* print and exit */

	if (argv[optind])
		errtryhelp(EXIT_FAILURE);

#define INITIALIZE_COLUMNS(COLUMN_SPEC)				\
	for (i = 0; i < ARRAY_SIZE(COLUMN_SPEC); i++)	\
		columns[ncolumns++] = COLUMN_SPEC[i]
	if (!ncolumns) {
		if (ctl.threads)
			INITIALIZE_COLUMNS(default_threads_columns);
		else
			INITIALIZE_COLUMNS(default_columns);
	}

	if (outarg && string_add_to_idarray(outarg, columns, ARRAY_SIZE(columns),
					    &ncolumns, column_name_to_id) < 0)
		return EXIT_FAILURE;

	scols_init_debug(0);

	INIT_LIST_HEAD(&ctl.procs);

	/* inilialize scols table */
	ctl.tb = scols_new_table();
	if (!ctl.tb)
		err(EXIT_FAILURE, _("failed to allocate output table"));

	scols_table_enable_noheadings(ctl.tb, ctl.noheadings);
	scols_table_enable_raw(ctl.tb, ctl.raw);
	scols_table_enable_json(ctl.tb, ctl.json);
	if (ctl.json)
		scols_table_set_name(ctl.tb, "lsfd");

	/* create output columns */
	for (i = 0; i < ncolumns; i++) {
		const struct colinfo *col = get_column_info(i);
		struct libscols_column *cl = add_column(ctl.tb, col, 0);

		if (!cl)
			err(EXIT_FAILURE, _("failed to allocate output column"));

		if (ctl.notrunc) {
			int flags = scols_column_get_flags(cl);
			flags &= ~SCOLS_FL_TRUNC;
			scols_column_set_flags(cl, flags);
		}
	}

	/* make filter */
	if (filter_expr) {
		ctl.filter = new_filter(filter_expr, debug_filter, &ctl);
		free(filter_expr);
	}

	if (dump_counters) {
		if (list_empty(&counter_specs))
			dump_default_counter_specs();
		else
			dump_counter_specs(&counter_specs);
		return 0;
	}

	/* make counters */
	if (ctl.show_summary) {
		if (list_empty(&counter_specs))
			ctl.ct_filters = new_default_counters(&ctl);
		else {
			ctl.ct_filters = new_counters(&counter_specs, &ctl);
			list_free(&counter_specs, struct counter_spec, specs,
				  free_counter_spec);
		}
	}

	if (n_pids > 0)
		sort_pids(pids, n_pids);

	if (scols_table_get_column_by_name(ctl.tb, "XMODE"))
		ctl.show_xmode = 1;

	/* collect data
	 *
	 * The call initialize_ipc_table() must come before
	 * initialize_classes.
	 */
	initialize_nodevs();
	initialize_ipc_table();
	initialize_classes();
	initialize_devdrvs();

	collect_processes(&ctl, pids, n_pids);
	free(pids);

	attach_xinfos(&ctl.procs);
	if (ctl.show_xmode)
		set_multiplexed_flags(&ctl.procs);


	convert(&ctl.procs, &ctl);

	/* print */
	if (ctl.show_main)
		emit(&ctl);

	if (ctl.show_summary && ctl.ct_filters)
		emit_summary(&ctl);

	/* cleanup */
	delete(&ctl.procs, &ctl);

	finalize_devdrvs();
	finalize_classes();
	finalize_ipc_table();
	finalize_nodevs();

	return 0;
}
