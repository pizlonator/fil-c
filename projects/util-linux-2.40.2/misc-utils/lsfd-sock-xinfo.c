/*
 * lsfd-sock-xinfo.c - read various information from files under /proc/net/ and NETLINK_SOCK_DIAG
 *
 * Copyright (C) 2022 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
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
#include <arpa/inet.h>		/* inet_ntop */
#include <netinet/in.h>		/* in6_addr */
#include <fcntl.h>		/* open(2) */
#include <ifaddrs.h>		/* getifaddrs */
#include <inttypes.h>		/* SCNu16 */
#include <net/if.h>		/* if_nametoindex */
#include <linux/if_ether.h>	/* ETH_P_* */
#include <linux/net.h>		/* SS_* */
#include <linux/netlink.h>	/* NETLINK_*, NLMSG_* */
#include <linux/rtnetlink.h>	/* RTA_*, struct rtattr,  */
#include <linux/sock_diag.h>	/* SOCK_DIAG_BY_FAMILY */
#include <linux/un.h>		/* UNIX_PATH_MAX */
#include <linux/unix_diag.h>	/* UNIX_DIAG_*, UDIAG_SHOW_*,
				   struct unix_diag_req */
#include <sched.h>		/* for setns(2) */
#include <search.h>		/* tfind, tsearch */
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>		/* SOCK_* */

#include "sysfs.h"
#include "bitops.h"

#include "lsfd.h"
#include "lsfd-sock.h"

static void load_xinfo_from_proc_icmp(ino_t netns_inode, enum sysfs_byteorder byteorder);
static void load_xinfo_from_proc_icmp6(ino_t netns_inode, enum sysfs_byteorder byteorder);
static void load_xinfo_from_proc_unix(ino_t netns_inode);
static void load_xinfo_from_proc_raw(ino_t netns_inode, enum sysfs_byteorder byteorder);
static void load_xinfo_from_proc_tcp(ino_t netns_inode, enum sysfs_byteorder byteorder);
static void load_xinfo_from_proc_udp(ino_t netns_inode, enum sysfs_byteorder byteorder);
static void load_xinfo_from_proc_udplite(ino_t netns_inode, enum sysfs_byteorder byteorder);
static void load_xinfo_from_proc_tcp6(ino_t netns_inode, enum sysfs_byteorder byteorder);
static void load_xinfo_from_proc_udp6(ino_t netns_inode, enum sysfs_byteorder byteorder);
static void load_xinfo_from_proc_udplite6(ino_t netns_inode, enum sysfs_byteorder byteorder);
static void load_xinfo_from_proc_raw6(ino_t netns_inode, enum sysfs_byteorder byteorder);
static void load_xinfo_from_proc_netlink(ino_t netns_inode);
static void load_xinfo_from_proc_packet(ino_t netns_inode);

static void load_xinfo_from_diag_unix(int diag, ino_t netns_inode);

static int self_netns_fd = -1;
static struct stat self_netns_sb;

static void *xinfo_tree;	/* for tsearch/tfind */
static void *netns_tree;

struct iface {
	unsigned int index;
	char name[IF_NAMESIZE];
};

static const char *get_iface_name(ino_t netns, unsigned int iface_index);

struct netns {
	ino_t inode;
	struct iface *ifaces;
};

static int netns_compare(const void *a, const void *b)
{
	const struct netns *netns_a = a;
	const struct netns *netns_b = b;

	return netns_a->inode - netns_b->inode;
}

static void netns_free(void *netns)
{
	struct netns *nsobj = netns;

	free(nsobj->ifaces);
	free(netns);
}

/*
 * iface index -> iface name mappings
 */
static void load_ifaces_from_getifaddrs(struct netns *nsobj)
{
	struct ifaddrs *ifa_list;
	struct ifaddrs *ifa;
	size_t i, count = 0;

	if (getifaddrs(&ifa_list) < 0)
		return;

	for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next)
		count++;

	nsobj->ifaces = xcalloc(count + 1, sizeof(*nsobj->ifaces));

	for (ifa = ifa_list, i = 0; ifa != NULL; ifa = ifa->ifa_next, i++) {
		unsigned int if_index = if_nametoindex(ifa->ifa_name);

		nsobj->ifaces[i].index = if_index;
		strncpy(nsobj->ifaces[i].name, ifa->ifa_name, IF_NAMESIZE - 1);
		/* The slot for the last byte is already filled by calloc. */
	}
	/* nsobj->ifaces[count] is the sentinel value. */

	freeifaddrs(ifa_list);

	return;
}

static const char *get_iface_name(ino_t netns, unsigned int iface_index)
{
	struct netns key = { .inode = netns };
	struct netns **nsobj = tfind(&key, &netns_tree, netns_compare);
	if (!nsobj)
		return NULL;

	for (size_t i = 0; (*nsobj)->ifaces[i].index; i++) {
		if ((*nsobj)->ifaces[i].index == iface_index)
			return (*nsobj)->ifaces[i].name;
	}

	return NULL;
}

static bool is_sock_xinfo_loaded(ino_t netns)
{
	struct netns key = { .inode = netns };
	return tfind(&key, &netns_tree, netns_compare)? true: false;
}

static struct netns *mark_sock_xinfo_loaded(ino_t ino)
{
	struct netns *netns = xcalloc(1, sizeof(*netns));
	ino_t **tmp;

	netns->inode = ino;
	tmp = tsearch(netns, &netns_tree, netns_compare);
	if (tmp == NULL)
		errx(EXIT_FAILURE, _("failed to allocate memory"));
	return *(struct netns **)tmp;
}

static void load_sock_xinfo_no_nsswitch(struct netns *nsobj)
{
	ino_t netns = nsobj? nsobj->inode: 0;
	int diagsd;
	enum sysfs_byteorder byteorder = sysfs_get_byteorder(NULL);

	load_xinfo_from_proc_unix(netns);
	load_xinfo_from_proc_tcp(netns, byteorder);
	load_xinfo_from_proc_udp(netns, byteorder);
	load_xinfo_from_proc_udplite(netns, byteorder);
	load_xinfo_from_proc_raw(netns, byteorder);
	load_xinfo_from_proc_tcp6(netns, byteorder);
	load_xinfo_from_proc_udp6(netns, byteorder);
	load_xinfo_from_proc_udplite6(netns, byteorder);
	load_xinfo_from_proc_raw6(netns, byteorder);
	load_xinfo_from_proc_icmp(netns, byteorder);
	load_xinfo_from_proc_icmp6(netns, byteorder);
	load_xinfo_from_proc_netlink(netns);
	load_xinfo_from_proc_packet(netns);

	diagsd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_SOCK_DIAG);
	DBG(ENDPOINTS, ul_debug("made a diagnose socket [fd=%d; %s]", diagsd,
				(diagsd >= 0)? "successful": strerror(errno)));
	if (diagsd >= 0) {
		load_xinfo_from_diag_unix(diagsd, netns);
		close(diagsd);
		DBG(ENDPOINTS, ul_debug("close the diagnose socket"));
	}

	if (nsobj)
		load_ifaces_from_getifaddrs(nsobj);
}

static void load_sock_xinfo_with_fd(int fd, struct netns *nsobj)
{
	if (setns(fd, CLONE_NEWNET) == 0) {
		load_sock_xinfo_no_nsswitch(nsobj);
		setns(self_netns_fd, CLONE_NEWNET);
	}
}

void load_sock_xinfo(struct path_cxt *pc, const char *name, ino_t netns)
{
	if (self_netns_fd == -1)
		return;

	if (!is_sock_xinfo_loaded(netns)) {
		int fd;
		struct netns *nsobj = mark_sock_xinfo_loaded(netns);
		fd = ul_path_open(pc, O_RDONLY, name);
		if (fd < 0)
			return;

		load_sock_xinfo_with_fd(fd, nsobj);
		close(fd);
	}
}

void initialize_sock_xinfos(void)
{
	struct path_cxt *pc;
	DIR *dir;
	struct dirent *d;

	self_netns_fd = open("/proc/self/ns/net", O_RDONLY);

	if (self_netns_fd < 0)
		load_sock_xinfo_no_nsswitch(NULL);
	else {
		if (fstat(self_netns_fd, &self_netns_sb) == 0) {
			unsigned long m;
			struct netns *nsobj = mark_sock_xinfo_loaded(self_netns_sb.st_ino);
			load_sock_xinfo_no_nsswitch(nsobj);

			m = minor(self_netns_sb.st_dev);
			add_nodev(m, "nsfs");
		}
	}

	/* Load /proc/net/{unix,...} of the network namespace
	 * specified with netns files under /var/run/netns/.
	 *
	 * `ip netns' command pins a network namespace on
	 * /var/run/netns.
	 */
	pc = ul_new_path("/var/run/netns");
	if (!pc)
		err(EXIT_FAILURE, _("failed to alloc path context for /var/run/netns"));
	dir = ul_path_opendir(pc, NULL);
	if (dir == NULL) {
		ul_unref_path(pc);
		return;
	}
	while ((d = readdir(dir))) {
		struct stat sb;
		int fd;
		struct netns *nsobj;
		if (ul_path_stat(pc, &sb, 0, d->d_name) < 0)
			continue;
		if (is_sock_xinfo_loaded(sb.st_ino))
			continue;
		nsobj = mark_sock_xinfo_loaded(sb.st_ino);
		fd = ul_path_open(pc, O_RDONLY, d->d_name);
		if (fd < 0)
			continue;
		load_sock_xinfo_with_fd(fd, nsobj);
		close(fd);
	}
	closedir(dir);
	ul_unref_path(pc);
}

static void free_sock_xinfo(void *node)
{
	struct sock_xinfo *xinfo = node;
	if (xinfo->class->free)
		xinfo->class->free(xinfo);
	free(node);
}

void finalize_sock_xinfos(void)
{
	if (self_netns_fd != -1)
		close(self_netns_fd);
	tdestroy(netns_tree, netns_free);
	tdestroy(xinfo_tree, free_sock_xinfo);
}

static int xinfo_compare(const void *a, const void *b)
{
	return ((struct sock_xinfo *)a)->inode - ((struct sock_xinfo *)b)->inode;
}

static void add_sock_info(struct sock_xinfo *xinfo)
{
	struct sock_xinfo **tmp = tsearch(xinfo, &xinfo_tree, xinfo_compare);

	if (tmp == NULL)
		errx(EXIT_FAILURE, _("failed to allocate memory"));
}

struct sock_xinfo *get_sock_xinfo(ino_t inode)
{
	struct sock_xinfo key = { .inode = inode };
	struct sock_xinfo **xinfo = tfind(&key, &xinfo_tree, xinfo_compare);

	if (xinfo)
		return *xinfo;
	return NULL;
}

bool is_nsfs_dev(dev_t dev)
{
	return dev == self_netns_sb.st_dev;
}

static const char *sock_decode_type(uint16_t type)
{
	switch (type) {
	case SOCK_STREAM:
		return "stream";
	case SOCK_DGRAM:
		return "dgram";
	case SOCK_RAW:
		return "raw";
	case SOCK_RDM:
		return "rdm";
	case SOCK_SEQPACKET:
		return "seqpacket";
	case SOCK_DCCP:
		return "dccp";
	case SOCK_PACKET:
		return "packet";
	default:
		return "unknown";
	}
}

static void send_diag_request(int diagsd, void *req, size_t req_size,
			      bool (*cb)(ino_t, size_t, void *),
			      ino_t netns)
{
	int r;
	struct sockaddr_nl nladdr = {
		.nl_family = AF_NETLINK,
	};

	struct nlmsghdr nlh = {
		.nlmsg_len = sizeof(nlh) + req_size,
		.nlmsg_type = SOCK_DIAG_BY_FAMILY,
		.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
	};

	struct iovec iovecs[] = {
		{ &nlh, sizeof(nlh) },
		{ req, req_size },
	};

	const struct msghdr mhd = {
		.msg_namelen = sizeof(nladdr),
		.msg_name = &nladdr,
		.msg_iovlen = ARRAY_SIZE(iovecs),
		.msg_iov = iovecs,
	};

	__attribute__((aligned(sizeof(void *)))) uint8_t buf[8192];

	r = sendmsg(diagsd, &mhd, 0);
	DBG(ENDPOINTS, ul_debug("sendmsg [rc=%d; %s]",
				r, (r >= 0)? "successful": strerror(errno)));
	if (r < 0)
		return;

	for (;;) {
		const struct nlmsghdr *h;
		r = recvfrom(diagsd, buf, sizeof(buf), 0, NULL, NULL);
		DBG(ENDPOINTS, ul_debug("recvfrom [rc=%d; %s]",
					r, (r >= 0)? "successful": strerror(errno)));
		if (r < 0)
			return;

		h = (void *) buf;
		DBG(ENDPOINTS, ul_debug("   OK: %d", NLMSG_OK(h, (size_t)r)));
		if (!NLMSG_OK(h, (size_t)r))
			return;

		for (; NLMSG_OK(h, (size_t)r); h = NLMSG_NEXT(h, r)) {
			if (h->nlmsg_type == NLMSG_DONE) {
				DBG(ENDPOINTS, ul_debug("      DONE"));
				return;
			}
			if (h->nlmsg_type == NLMSG_ERROR)  {
				struct nlmsgerr *e = (struct nlmsgerr *)NLMSG_DATA(h);
				DBG(ENDPOINTS, ul_debug("      ERROR: %s",
							strerror(- e->error)));
				return;
			}

			if (h->nlmsg_type == SOCK_DIAG_BY_FAMILY) {
				DBG(ENDPOINTS, ul_debug("      FAMILY"));
				if (!cb(netns, h->nlmsg_len, NLMSG_DATA(h)))
					return;
			}
			DBG(ENDPOINTS, ul_debug("   NEXT"));
		}
		DBG(ENDPOINTS, ul_debug("   OK: 0"));
	}
}

/*
 * Protocol specific code
 */

/*
 * UNIX
 */
struct unix_ipc {
	struct ipc ipc;
	ino_t inode;
	ino_t ipeer;
};

struct unix_xinfo {
	struct sock_xinfo sock;
	int acceptcon;	/* flags */
	uint16_t type;
	uint8_t  st;
#define is_shutdown_mask_set(mask) ((mask) & (1 << 2))
#define set_shutdown_mask(mask)    ((mask) |= (1 << 2))
	uint8_t  shutdown_mask:3;
	struct unix_ipc *unix_ipc;
	char path[
		  UNIX_PATH_MAX
		  + 1		/* for @ */
		  + 1		/* \0? */
		  ];
};

static const char *unix_decode_state(uint8_t st)
{
	switch (st) {
	case SS_FREE:
		return "free";
	case SS_UNCONNECTED:
		return "unconnected";
	case SS_CONNECTING:
		return "connecting";
	case SS_CONNECTED:
		return "connected";
	case SS_DISCONNECTING:
		return "disconnecting";
	default:
		return "unknown";
	}
}

static char *unix_get_name(struct sock_xinfo *sock_xinfo,
			   struct sock *sock)
{
	struct unix_xinfo *ux = (struct unix_xinfo *)sock_xinfo;
	const char *state = unix_decode_state(ux->st);
	char *str = NULL;

	if (sock->protoname && (strcmp(sock->protoname, "UNIX-STREAM") == 0))
		xasprintf(&str, "state=%s%s%s",
			  (ux->acceptcon)? "listen": state,
			  *(ux->path)? " path=": "",
			  *(ux->path)? ux->path: "");
	else
		xasprintf(&str, "state=%s%s%s type=%s",
			  (ux->acceptcon)? "listen": state,
			  *(ux->path)? " path=": "",
			  *(ux->path)? ux->path: "",
			  sock_decode_type(ux->type));
	return str;
}

static char *unix_get_type(struct sock_xinfo *sock_xinfo,
			   struct sock *sock __attribute__((__unused__)))
{
	const char *str;
	struct unix_xinfo *ux = (struct unix_xinfo *)sock_xinfo;

	str = sock_decode_type(ux->type);
	return xstrdup(str);
}

static char *unix_get_state(struct sock_xinfo *sock_xinfo,
			    struct sock *sock __attribute__((__unused__)))
{
	const char *str;
	struct unix_xinfo *ux = (struct unix_xinfo *)sock_xinfo;

	if (ux->acceptcon)
		return xstrdup("listen");

	str = unix_decode_state(ux->st);
	return xstrdup(str);
}

static bool unix_get_listening(struct sock_xinfo *sock_xinfo,
			       struct sock *sock __attribute__((__unused__)))
{
	struct unix_xinfo *ux = (struct unix_xinfo *)sock_xinfo;

	return ux->acceptcon;
}

static unsigned int unix_get_hash(struct file *file)
{
	return (unsigned int)(file->stat.st_ino % UINT_MAX);
}

static bool unix_is_suitable_ipc(struct ipc *ipc, struct file *file)
{
	return ((struct unix_ipc *)ipc)->inode == file->stat.st_ino;
}

/* For looking up an ipc struct for a sock inode, we need a sock strcuct
 * for the inode. See the signature o get_ipc().
 *
 * However, there is a case that we have no sock strcuct for the inode;
 * in the context we know only the sock inode.
 * For the case, unix_make_dumy_sock() provides the way to make a
 * dummy sock struct for the inode.
 */
static void unix_make_dumy_sock(struct sock *original, ino_t ino, struct sock *dummy)
{
	*dummy = *original;
	dummy->file.stat.st_ino = ino;
}

static struct ipc_class unix_ipc_class = {
	.size = sizeof(struct unix_ipc),
	.get_hash = unix_get_hash,
	.is_suitable_ipc = unix_is_suitable_ipc,
	.free = NULL,
};

static struct ipc_class *unix_get_ipc_class(struct sock_xinfo *sock_xinfo __attribute__((__unused__)),
					    struct sock *sock __attribute__((__unused__)))
{
	return &unix_ipc_class;
}

static bool unix_shutdown_chars(struct unix_xinfo *ux, char rw[2])
{
	uint8_t mask = ux->shutdown_mask;

	if (is_shutdown_mask_set(mask)) {
		rw[0] = ((mask & (1 << 0))? '-': 'r');
		rw[1] = ((mask & (1 << 1))? '-': 'w');
		return true;
	}

	return false;
}

static inline char *unix_xstrendpoint(struct sock *sock)
{
	char *str = NULL;
	char shutdown_chars[3] = { 0 };

	if (!unix_shutdown_chars(((struct unix_xinfo *)sock->xinfo), shutdown_chars)) {
		shutdown_chars[0] = '?';
		shutdown_chars[1] = '?';
	}
	xasprintf(&str, "%d,%s,%d%c%c",
		  sock->file.proc->pid, sock->file.proc->command, sock->file.association,
		  shutdown_chars[0], shutdown_chars[1]);

	return str;
}

static struct ipc *unix_get_peer_ipc(struct unix_xinfo *ux,
				     struct sock *sock)
{
	struct unix_ipc *unix_ipc;
	struct sock dummy_peer_sock;

	unix_ipc = ux->unix_ipc;
	if (!unix_ipc)
		return NULL;

	unix_make_dumy_sock(sock, unix_ipc->ipeer, &dummy_peer_sock);
	return get_ipc(&dummy_peer_sock.file);
}

static bool unix_fill_column(struct proc *proc __attribute__((__unused__)),
			     struct sock_xinfo *sock_xinfo,
			     struct sock *sock,
			     struct libscols_line *ln __attribute__((__unused__)),
			     int column_id,
			     size_t column_index __attribute__((__unused__)),
			     char **str)
{
	struct unix_xinfo *ux = (struct unix_xinfo *)sock_xinfo;
	struct ipc *peer_ipc;
	struct list_head *e;
	char shutdown_chars[3] = { 0 };

	switch (column_id) {
	case COL_UNIX_PATH:
		if (*ux->path) {
			*str = xstrdup(ux->path);
			return true;
		}
		break;
	case COL_ENDPOINTS:
		peer_ipc = unix_get_peer_ipc(ux, sock);
		if (!peer_ipc)
			break;

		list_for_each_backwardly(e, &peer_ipc->endpoints) {
			struct sock *peer_sock = list_entry(e, struct sock, endpoint.endpoints);
			char *estr;

			if (*str)
				xstrputc(str, '\n');
			estr = unix_xstrendpoint(peer_sock);
			xstrappend(str, estr);
			free(estr);
		}
		if (*str)
			return true;
		break;
	case COL_SOCK_SHUTDOWN:
		if (unix_shutdown_chars(ux, shutdown_chars)) {
			*str = xstrdup(shutdown_chars);
			return true;
		}
		break;
	}

	return false;
}

static const struct sock_xinfo_class unix_xinfo_class = {
	.get_name = unix_get_name,
	.get_type = unix_get_type,
	.get_state = unix_get_state,
	.get_listening = unix_get_listening,
	.fill_column = unix_fill_column,
	.get_ipc_class = unix_get_ipc_class,
	.free = NULL,
};

/* UNIX_LINE_LEN need at least 54 + 21 + UNIX_PATH_MAX + 1.
 *
 * An actual number must be used in this definition
 * since UNIX_LINE_LEN is specified as an argument for
 * stringify_value().
 */
#define UNIX_LINE_LEN 256
static void load_xinfo_from_proc_unix(ino_t netns_inode)
{
	char line[UNIX_LINE_LEN];
	FILE *unix_fp;

	unix_fp = fopen("/proc/net/unix", "r");
	DBG(ENDPOINTS, ul_debug("open /proc/net/unix [fp=%p; %s]", unix_fp,
				unix_fp? "successful": strerror(errno)));
	if (!unix_fp)
		return;

	if (fgets(line, sizeof(line), unix_fp) == NULL)
		goto out;
	if (!(line[0] == 'N' && line[1] == 'u' && line[2] == 'm'))
		/* Unexpected line */
		goto out;

	while (fgets(line, sizeof(line), unix_fp)) {
		uint64_t flags;
		uint32_t type;
		unsigned int st;
		unsigned long inode;
		struct unix_xinfo *ux;
		char path[UNIX_LINE_LEN + 1] = { 0 };
		int r;

		DBG(ENDPOINTS, ul_debug("   line: %s", line));

		r = sscanf(line, "%*x: %*x %*x %" SCNx64 " %x %x %lu %"
			   stringify_value(UNIX_LINE_LEN) "[^\n]",
			   &flags, &type, &st, &inode, path);
		DBG(ENDPOINTS, ul_debug("   scanf: %d", r));
		if (r < 4)
			continue;

		DBG(ENDPOINTS, ul_debug("   inode: %lu", inode));
		if (inode == 0)
			continue;

		ux = xcalloc(1, sizeof(*ux));
		ux->sock.class = &unix_xinfo_class;
		ux->sock.inode = (ino_t)inode;
		ux->sock.netns_inode = netns_inode;

		ux->acceptcon = !!flags;
		ux->type = type;
		ux->st = st;
		xstrncpy(ux->path, path, sizeof(ux->path));

		DBG(ENDPOINTS, ul_debug("   path: %s", ux->path));
		add_sock_info(&ux->sock);
	}

 out:
	DBG(ENDPOINTS, ul_debug("close /proc/net/unix"));
	fclose(unix_fp);
}

/* The path name extracted from /proc/net/unix is unreliable; the line oriented interface cannot
 * represent a file name including newlines. With unix_refill_name(), we patch the path
 * member of unix_xinfos with information received via netlink diag interface. */
static void unix_refill_name(struct sock_xinfo *xinfo, const char *name, size_t len)
{
	struct unix_xinfo *ux = (struct unix_xinfo *)xinfo;
	size_t min_len;

	if (len == 0)
		return;

	min_len = min(sizeof(ux->path) - 1, len);
	memcpy(ux->path, name, min_len);
	if (ux->path[0] == '\0') {
		ux->path[0] = '@';
	}
	ux->path[min_len] = '\0';
}

static bool handle_diag_unix(ino_t netns __attribute__((__unused__)),
			     size_t nlmsg_len, void *nlmsg_data)
{
	const struct unix_diag_msg *diag = nlmsg_data;
	size_t rta_len;
	ino_t inode;
	struct sock_xinfo *xinfo;
	struct unix_xinfo *unix_xinfo;

	if (diag->udiag_family != AF_UNIX)
		return false;
	DBG(ENDPOINTS, ul_debug("         UNIX"));
	DBG(ENDPOINTS, ul_debug("         LEN: %zu (>= %zu)", nlmsg_len,
				(size_t)(NLMSG_LENGTH(sizeof(*diag)))));

	if (nlmsg_len < NLMSG_LENGTH(sizeof(*diag)))
		return false;

	inode = (ino_t)diag->udiag_ino;
	xinfo = get_sock_xinfo(inode);

	DBG(ENDPOINTS, ul_debug("         inode: %llu", (unsigned long long)inode));
	DBG(ENDPOINTS, ul_debug("         xinfo: %p", xinfo));

	if (xinfo == NULL)
		/* The socket is found in the diag response
		   but not in the proc fs. */
		return true;

	DBG(ENDPOINTS, ul_debug("         xinfo->class == &unix_xinfo_class: %d",
				xinfo->class == &unix_xinfo_class));
	if (xinfo->class != &unix_xinfo_class)
		return true;
	unix_xinfo = (struct unix_xinfo *)xinfo;

	rta_len = nlmsg_len - NLMSG_LENGTH(sizeof(*diag));
	DBG(ENDPOINTS, ul_debug("         rta_len: %zu", rta_len));
	for (struct rtattr *attr = (struct rtattr *)(diag + 1);
	     RTA_OK(attr, rta_len);
	     attr = RTA_NEXT(attr, rta_len)) {
		size_t len = RTA_PAYLOAD(attr);

		DBG(ENDPOINTS, ul_debug("            len = %2zu, type: %d",
					rta_len, attr->rta_type));
		switch (attr->rta_type) {
		case UNIX_DIAG_NAME:
			unix_refill_name(xinfo, RTA_DATA(attr), len);
			break;

		case UNIX_DIAG_SHUTDOWN:
			if (len < 1)
				break;

			unix_xinfo->shutdown_mask = *(uint8_t *)RTA_DATA(attr);
			set_shutdown_mask(unix_xinfo->shutdown_mask);
			break;

		case UNIX_DIAG_PEER:
			if (len < 4)
				break;

			unix_xinfo->unix_ipc = (struct unix_ipc *)new_ipc(&unix_ipc_class);
			unix_xinfo->unix_ipc->inode = inode;
			unix_xinfo->unix_ipc->ipeer = (ino_t)(*(uint32_t *)RTA_DATA(attr));
			add_ipc(&unix_xinfo->unix_ipc->ipc, inode % UINT_MAX);
			break;
		}
	}
	return true;
}

static void load_xinfo_from_diag_unix(int diagsd, ino_t netns)
{
	struct unix_diag_req udr = {
		.sdiag_family = AF_UNIX,
		.udiag_states = -1, /* set the all bits. */
		.udiag_show = UDIAG_SHOW_NAME | UDIAG_SHOW_PEER | UNIX_DIAG_SHUTDOWN,
	};

	send_diag_request(diagsd, &udr, sizeof(udr), handle_diag_unix, netns);
}

/*
 * AF_INET
 */
struct inet_xinfo {
	struct sock_xinfo sock;
	struct in_addr local_addr;
	struct in_addr remote_addr;
};

static uint32_t kernel32_to_cpu(enum sysfs_byteorder byteorder, uint32_t v)
{
	if (byteorder == SYSFS_BYTEORDER_LITTLE)
		return le32_to_cpu(v);
	else
		return be32_to_cpu(v);
}

/*
 * AF_INET6
 */
struct inet6_xinfo {
	struct sock_xinfo sock;
	struct in6_addr local_addr;
	struct in6_addr remote_addr;
};

/*
 * L4 abstract-layer for protocols stacked on IP and IP6.
 */
enum l4_state {
	/*
	 * Taken from linux/include/net/tcp_states.h.
	 * (GPL-2.0-or-later)
	 *
	 * UDP and RAW sockets also uses the contents in Linux.
	 */
	TCP_ESTABLISHED = 1,
	TCP_SYN_SENT,
	TCP_SYN_RECV,
	TCP_FIN_WAIT1,
	TCP_FIN_WAIT2,
	TCP_TIME_WAIT,
	TCP_CLOSE,
	TCP_CLOSE_WAIT,
	TCP_LAST_ACK,
	TCP_LISTEN,
	TCP_CLOSING,
	TCP_NEW_SYN_RECV,

	TCP_MAX_STATES	/* Leave at the end! */
};

static const char *l4_decode_state(enum l4_state st)
{
	const char * const table [] = {
		[TCP_ESTABLISHED] = "established",
		[TCP_SYN_SENT] = "syn-sent",
		[TCP_SYN_RECV] = "syn-recv",
		[TCP_FIN_WAIT1] = "fin-wait1",
		[TCP_FIN_WAIT2] = "fin-wait2",
		[TCP_TIME_WAIT] = "time-wait",
		[TCP_CLOSE] = "close",
		[TCP_CLOSE_WAIT] = "close-wait",
		[TCP_LAST_ACK] = "last-ack",
		[TCP_LISTEN] = "listen",
		[TCP_CLOSING] = "closing",
		[TCP_NEW_SYN_RECV] = "new-syn-recv",
	};

	if (st < TCP_MAX_STATES)
		return table[st];
	return "unknown";
}

struct l4_xinfo {
	union {
		struct inet_xinfo inet;
		struct inet6_xinfo inet6;
	};
	enum l4_state st;
};

enum l4_side { L4_LOCAL, L4_REMOTE };
enum l3_decorator { L3_DECO_START, L3_DECO_END };

struct l4_xinfo_class {
	struct sock_xinfo_class sock;
	struct sock_xinfo *(*scan_line)(const struct sock_xinfo_class *,
					char *,
					ino_t,
					enum sysfs_byteorder);
	void * (*get_addr)(struct l4_xinfo *, enum l4_side);
	bool (*is_any_addr)(void *);
	int family;
	const char *l3_decorator[2];
};

#define l3_fill_column_handler(L3, SOCK_XINFO, COLUMN_ID, STR)	__extension__ \
	({								\
		struct l4_xinfo_class *class = (struct l4_xinfo_class *)SOCK_XINFO->class; \
		struct l4_xinfo *l4 = (struct l4_xinfo *)SOCK_XINFO;	\
		void *n = NULL;						\
		char s[BUFSIZ];						\
		bool r = false;						\
									\
		switch (COLUMN_ID) {					\
		case COL_##L3##_LADDR:					\
			n = class->get_addr(l4, L4_LOCAL);		\
			break;						\
		case COL_##L3##_RADDR:					\
			n = class->get_addr(l4, L4_REMOTE);		\
			break;						\
		default:						\
			break;						\
		}							\
									\
		if (n && inet_ntop(class->family, n, s, sizeof(s))) {	\
			*STR = xstrdup(s);				\
			r = true;					\
		}							\
		r;							\
	})

/*
 * TCP
 */
struct tcp_xinfo {
	struct l4_xinfo l4;
	uint16_t local_port;
	uint16_t remote_port;
};

static char *tcp_get_name(struct sock_xinfo *sock_xinfo,
			  struct sock *sock  __attribute__((__unused__)))
{
	char *str = NULL;
	struct tcp_xinfo *tcp = ((struct tcp_xinfo *)sock_xinfo);
	struct l4_xinfo *l4 = &tcp->l4;
	const char *st_str = l4_decode_state(l4->st);
	struct l4_xinfo_class *class = (struct l4_xinfo_class *)sock_xinfo->class;
	void *laddr = class->get_addr(l4, L4_LOCAL);
	void *raddr = class->get_addr(l4, L4_REMOTE);
	char local_s[BUFSIZ];
	char remote_s[BUFSIZ];
	const char *start = class->l3_decorator[L3_DECO_START];
	const char *end = class->l3_decorator[L3_DECO_END];

	if (!inet_ntop(class->family, laddr, local_s, sizeof(local_s)))
		xasprintf(&str, "state=%s", st_str);
	else if (l4->st == TCP_LISTEN
		 || !inet_ntop(class->family, raddr, remote_s, sizeof(remote_s)))
		xasprintf(&str, "state=%s laddr=%s%s%s:%"PRIu16,
			  st_str,
			  start, local_s, end, tcp->local_port);
	else
		xasprintf(&str, "state=%s laddr=%s%s%s:%"PRIu16" raddr=%s%s%s:%"PRIu16,
			  st_str,
			  start, local_s, end, tcp->local_port,
			  start, remote_s, end, tcp->remote_port);
	return str;
}

static char *tcp_get_type(struct sock_xinfo *sock_xinfo __attribute__((__unused__)),
			   struct sock *sock __attribute__((__unused__)))
{
	return xstrdup("stream");
}

static char *tcp_get_state(struct sock_xinfo *sock_xinfo,
			   struct sock *sock __attribute__((__unused__)))
{
	return xstrdup(l4_decode_state(((struct l4_xinfo *)sock_xinfo)->st));
}

static bool tcp_get_listening(struct sock_xinfo *sock_xinfo,
			      struct sock *sock __attribute__((__unused__)))
{
	return ((struct l4_xinfo *)sock_xinfo)->st == TCP_LISTEN;
}

#define l4_fill_column_handler(L4, SOCK_XINFO, COLUMN_ID, STR)	__extension__ \
	({								\
		struct l4_xinfo_class *class = (struct l4_xinfo_class *)SOCK_XINFO->class; \
		struct tcp_xinfo *tcp = (struct tcp_xinfo *)SOCK_XINFO; \
		struct l4_xinfo *l4 = &tcp->l4;				\
		void *n = NULL;						\
		bool has_laddr = false;					\
		unsigned short p;					\
		bool has_lport = false;					\
		char s[BUFSIZ];						\
		bool r = true;						\
									\
		switch (COLUMN_ID) {					\
		case COL_##L4##_LADDR:					\
			n = class->get_addr(l4, L4_LOCAL);		\
			has_laddr = true;				\
			p = tcp->local_port;				\
			/* FALL THROUGH */				\
		case COL_##L4##_RADDR:					\
			if (!has_laddr) {				\
				n = class->get_addr(l4, L4_REMOTE);	\
				p = tcp->remote_port;			\
			}						\
			if (n && inet_ntop(class->family, n, s, sizeof(s))) \
				xasprintf(STR, "%s%s%s:%"PRIu16, \
					  class->l3_decorator[L3_DECO_START], \
					  s,				\
					  class->l3_decorator[L3_DECO_END], \
					  p);				\
			break;						\
		case COL_##L4##_LPORT:					\
			p = tcp->local_port;				\
			has_lport = true;				\
			/* FALL THROUGH */				\
		case COL_##L4##_RPORT:					\
			if (!has_lport)					\
				p = tcp->remote_port;			\
			xasprintf(STR, "%"PRIu16, p);			\
			break;						\
		default:						\
			r = false;					\
			break;						\
		}							\
		r;							\
	})

static struct sock_xinfo *tcp_xinfo_scan_line(const struct sock_xinfo_class *class,
					      char * line,
					      ino_t netns_inode,
					      enum sysfs_byteorder byteorder)
{
	unsigned long local_addr;
	unsigned long local_port;
	unsigned long remote_addr;
	unsigned long remote_port;
	unsigned long st;
	unsigned long long inode;
	struct tcp_xinfo *tcp;
	struct inet_xinfo *inet;
	struct sock_xinfo *sock;

	if (sscanf(line, "%*d: %lx:%lx %lx:%lx %lx %*x:%*x %*x:%*x %*x %*u %*u %lld",
		   &local_addr, &local_port, &remote_addr, &remote_port,
		   &st, &inode) != 6)
		return NULL;

	if (inode == 0)
		return NULL;

	tcp = xcalloc(1, sizeof(*tcp));
	inet = &tcp->l4.inet;
	sock = &inet->sock;
	sock->class = class;
	sock->inode = (ino_t)inode;
	sock->netns_inode = netns_inode;
	inet->local_addr.s_addr = kernel32_to_cpu(byteorder, local_addr);
	tcp->local_port = local_port;
	inet->remote_addr.s_addr = kernel32_to_cpu(byteorder, remote_addr);
	tcp->remote_port = remote_port;
	tcp->l4.st = st;

	return sock;
}

static void *tcp_xinfo_get_addr(struct l4_xinfo *l4, enum l4_side side)
{
	return (side == L4_LOCAL)
		? &l4->inet.local_addr
		: &l4->inet.remote_addr;
}

static bool tcp_xinfo_is_any_addr(void *addr)
{
	return ((struct in_addr *)addr)->s_addr == INADDR_ANY;
}

static bool tcp_fill_column(struct proc *proc __attribute__((__unused__)),
			    struct sock_xinfo *sock_xinfo,
			    struct sock *sock __attribute__((__unused__)),
			    struct libscols_line *ln __attribute__((__unused__)),
			    int column_id,
			    size_t column_index __attribute__((__unused__)),
			    char **str)
{
	return l3_fill_column_handler(INET, sock_xinfo, column_id, str)
		|| l4_fill_column_handler(TCP, sock_xinfo, column_id, str);
}

static const struct l4_xinfo_class tcp_xinfo_class = {
	.sock = {
		.get_name = tcp_get_name,
		.get_type = tcp_get_type,
		.get_state = tcp_get_state,
		.get_listening = tcp_get_listening,
		.fill_column = tcp_fill_column,
		.free = NULL,
	},
	.scan_line = tcp_xinfo_scan_line,
	.get_addr = tcp_xinfo_get_addr,
	.is_any_addr = tcp_xinfo_is_any_addr,
	.family = AF_INET,
	.l3_decorator = {"", ""},
};

static bool L4_verify_initial_line(const char *line)
{
	/* At least we expect two white spaces. */
	if (strncmp(line, "  ", 2) != 0)
		return false;
	line += 2;

	/* Skip white spaces. */
	line = skip_space(line);

	return strncmp(line, "sl", 2) == 0;
}

#define TCP_LINE_LEN 256
static void load_xinfo_from_proc_inet_L4(ino_t netns_inode, const char *proc_file,
					 const struct l4_xinfo_class *class,
					 enum sysfs_byteorder byteorder)
{
	char line[TCP_LINE_LEN];
	FILE *tcp_fp;

	tcp_fp = fopen(proc_file, "r");
	if (!tcp_fp)
		return;

	if (fgets(line, sizeof(line), tcp_fp) == NULL)
		goto out;
	if (!L4_verify_initial_line(line))
		/* Unexpected line */
		goto out;

	while (fgets(line, sizeof(line), tcp_fp)) {
		struct sock_xinfo *sock = class->scan_line(&class->sock, line, netns_inode, byteorder);
		if (sock)
			add_sock_info(sock);
	}

 out:
	fclose(tcp_fp);
}

static void load_xinfo_from_proc_tcp(ino_t netns_inode, enum sysfs_byteorder byteorder)
{
	load_xinfo_from_proc_inet_L4(netns_inode,
				     "/proc/net/tcp",
				     &tcp_xinfo_class,
				     byteorder);
}

/*
 * UDP
 */
static char *udp_get_name(struct sock_xinfo *sock_xinfo,
			  struct sock *sock  __attribute__((__unused__)))
{
	char *str = NULL;
	struct tcp_xinfo *tcp = ((struct tcp_xinfo *)sock_xinfo);
	struct l4_xinfo *l4 = &tcp->l4;
	unsigned int st = l4->st;
	const char *st_str = l4_decode_state(st);
	struct l4_xinfo_class *class = (struct l4_xinfo_class *)sock_xinfo->class;
	void *laddr = class->get_addr(l4, L4_LOCAL);
	void *raddr = class->get_addr(l4, L4_REMOTE);
	char local_s[BUFSIZ];
	char remote_s[BUFSIZ];
	const char *start = class->l3_decorator[L3_DECO_START];
	const char *end = class->l3_decorator[L3_DECO_END];

	if (!inet_ntop(class->family, laddr, local_s, sizeof(local_s)))
		xasprintf(&str, "state=%s", st_str);
	else if ((class->is_any_addr(raddr) && tcp->remote_port == 0)
		 || !inet_ntop(class->family, raddr, remote_s, sizeof(remote_s)))
		xasprintf(&str, "state=%s laddr=%s%s%s:%"PRIu16,
			  st_str,
			  start, local_s, end, tcp->local_port);
	else
		xasprintf(&str, "state=%s laddr=%s%s%s:%"PRIu16" raddr=%s%s%s:%"PRIu16,
			  st_str,
			  start, local_s, end, tcp->local_port,
			  start, remote_s, end, tcp->remote_port);
	return str;
}

static char *udp_get_type(struct sock_xinfo *sock_xinfo __attribute__((__unused__)),
			  struct sock *sock __attribute__((__unused__)))
{
	return xstrdup("dgram");
}

static bool udp_fill_column(struct proc *proc __attribute__((__unused__)),
			    struct sock_xinfo *sock_xinfo,
			    struct sock *sock __attribute__((__unused__)),
			    struct libscols_line *ln __attribute__((__unused__)),
			    int column_id,
			    size_t column_index __attribute__((__unused__)),
			    char **str)
{
	return l3_fill_column_handler(INET, sock_xinfo, column_id, str)
		|| l4_fill_column_handler(UDP, sock_xinfo, column_id, str);
}

static const struct l4_xinfo_class udp_xinfo_class = {
	.sock = {
		.get_name = udp_get_name,
		.get_type = udp_get_type,
		.get_state = tcp_get_state,
		.get_listening = NULL,
		.fill_column = udp_fill_column,
		.free = NULL,
	},
	.scan_line = tcp_xinfo_scan_line,
	.get_addr = tcp_xinfo_get_addr,
	.is_any_addr = tcp_xinfo_is_any_addr,
	.family = AF_INET,
	.l3_decorator = {"", ""},
};

static void load_xinfo_from_proc_udp(ino_t netns_inode, enum sysfs_byteorder byteorder)
{
	load_xinfo_from_proc_inet_L4(netns_inode,
				     "/proc/net/udp",
				     &udp_xinfo_class,
				     byteorder);
}

/*
 * UDP-Lite
 */
static bool udplite_fill_column(struct proc *proc __attribute__((__unused__)),
				struct sock_xinfo *sock_xinfo,
				struct sock *sock __attribute__((__unused__)),
				struct libscols_line *ln __attribute__((__unused__)),
				int column_id,
				size_t column_index __attribute__((__unused__)),
				char **str)
{
	return l3_fill_column_handler(INET, sock_xinfo, column_id, str)
		|| l4_fill_column_handler(UDPLITE, sock_xinfo, column_id, str);
}

static const struct l4_xinfo_class udplite_xinfo_class = {
	.sock = {
		.get_name = udp_get_name,
		.get_type = udp_get_type,
		.get_state = tcp_get_state,
		.get_listening = NULL,
		.fill_column = udplite_fill_column,
		.free = NULL,
	},
	.scan_line = tcp_xinfo_scan_line,
	.get_addr = tcp_xinfo_get_addr,
	.is_any_addr = tcp_xinfo_is_any_addr,
	.family = AF_INET,
	.l3_decorator = {"", ""},
};

static void load_xinfo_from_proc_udplite(ino_t netns_inode, enum sysfs_byteorder byteorder)
{
	load_xinfo_from_proc_inet_L4(netns_inode,
				     "/proc/net/udplite",
				     &udplite_xinfo_class,
				     byteorder);
}

/*
 * RAW
 */
struct raw_xinfo {
	struct l4_xinfo l4;
	uint16_t protocol;
};

static char *raw_get_name_common(struct sock_xinfo *sock_xinfo,
				 struct sock *sock  __attribute__((__unused__)),
				 const char *port_label)
{
	char *str = NULL;
	struct l4_xinfo_class *class = (struct l4_xinfo_class *)sock_xinfo->class;
	struct raw_xinfo *raw = ((struct raw_xinfo *)sock_xinfo);
	struct l4_xinfo *l4 = &raw->l4;
	const char *st_str = l4_decode_state(l4->st);
	void *laddr = class->get_addr(l4, L4_LOCAL);
	void *raddr = class->get_addr(l4, L4_REMOTE);
	char local_s[BUFSIZ];
	char remote_s[BUFSIZ];

	if (!inet_ntop(class->family, laddr, local_s, sizeof(local_s)))
		xasprintf(&str, "state=%s", st_str);
	else if (class->is_any_addr(raddr)
		 || !inet_ntop(class->family, raddr, remote_s, sizeof(remote_s)))
		xasprintf(&str, "state=%s %s=%"PRIu16" laddr=%s",
			  st_str,
			  port_label,
			  raw->protocol, local_s);
	else
		xasprintf(&str, "state=%s %s=%"PRIu16" laddr=%s raddr=%s",
			  st_str,
			  port_label,
			  raw->protocol, local_s, remote_s);
	return str;
}

static char *raw_get_name(struct sock_xinfo *sock_xinfo,
			  struct sock *sock  __attribute__((__unused__)))
{
	return raw_get_name_common(sock_xinfo, sock, "protocol");
}

static char *raw_get_type(struct sock_xinfo *sock_xinfo __attribute__((__unused__)),
			  struct sock *sock __attribute__((__unused__)))
{
	return xstrdup("raw");
}

static bool raw_fill_column(struct proc *proc __attribute__((__unused__)),
			    struct sock_xinfo *sock_xinfo,
			    struct sock *sock __attribute__((__unused__)),
			    struct libscols_line *ln __attribute__((__unused__)),
			    int column_id,
			    size_t column_index __attribute__((__unused__)),
			    char **str)
{
	if (l3_fill_column_handler(INET, sock_xinfo, column_id, str))
		return true;

	if (column_id == COL_RAW_PROTOCOL) {
		xasprintf(str, "%"PRIu16,
			  ((struct raw_xinfo *)sock_xinfo)->protocol);
		return true;
	}

	return false;
}

static struct sock_xinfo *raw_xinfo_scan_line(const struct sock_xinfo_class *class,
					      char * line,
					      ino_t netns_inode,
					      enum sysfs_byteorder byteorder)
{
	unsigned long local_addr;
	unsigned long protocol;
	unsigned long remote_addr;
	unsigned long st;
	unsigned long long inode;
	struct raw_xinfo *raw;
	struct inet_xinfo *inet;
	struct sock_xinfo *sock;

	if (sscanf(line, "%*d: %lx:%lx %lx:%*x %lx %*x:%*x %*x:%*x %*x %*u %*u %lld",
		   &local_addr, &protocol, &remote_addr,
		   &st, &inode) != 5)
		return NULL;

	if (inode == 0)
		return NULL;

	raw = xcalloc(1, sizeof(*raw));
	inet = &raw->l4.inet;
	sock = &inet->sock;
	sock->class = class;
	sock->inode = (ino_t)inode;
	sock->netns_inode = netns_inode;
	inet->local_addr.s_addr = kernel32_to_cpu(byteorder, local_addr);
	inet->remote_addr.s_addr = kernel32_to_cpu(byteorder, remote_addr);
	raw->protocol = protocol;
	raw->l4.st = st;

	return sock;
}

static const struct l4_xinfo_class raw_xinfo_class = {
	.sock = {
		.get_name = raw_get_name,
		.get_type = raw_get_type,
		.get_state = tcp_get_state,
		.get_listening = NULL,
		.fill_column = raw_fill_column,
		.free = NULL,
	},
	.scan_line = raw_xinfo_scan_line,
	.get_addr = tcp_xinfo_get_addr,
	.is_any_addr = tcp_xinfo_is_any_addr,
	.family = AF_INET,
	.l3_decorator = {"", ""},
};

static void load_xinfo_from_proc_raw(ino_t netns_inode, enum sysfs_byteorder byteorder)
{
	load_xinfo_from_proc_inet_L4(netns_inode,
				     "/proc/net/raw",
				     &raw_xinfo_class,
				     byteorder);
}

/*
 * PING
 */
static char *ping_get_name(struct sock_xinfo *sock_xinfo,
			  struct sock *sock  __attribute__((__unused__)))
{
	return raw_get_name_common(sock_xinfo, sock, "id");
}

static char *ping_get_type(struct sock_xinfo *sock_xinfo __attribute__((__unused__)),
			   struct sock *sock __attribute__((__unused__)))
{
	return xstrdup("dgram");
}

static bool ping_fill_column(struct proc *proc __attribute__((__unused__)),
			     struct sock_xinfo *sock_xinfo,
			     struct sock *sock __attribute__((__unused__)),
			     struct libscols_line *ln __attribute__((__unused__)),
			     int column_id,
			     size_t column_index __attribute__((__unused__)),
			     char **str)
{
	if (l3_fill_column_handler(INET, sock_xinfo, column_id, str))
		return true;

	if (column_id == COL_PING_ID) {
		xasprintf(str, "%"PRIu16,
			  ((struct raw_xinfo *)sock_xinfo)->protocol);
		return true;
	}

	return false;
}

static const struct l4_xinfo_class ping_xinfo_class = {
	.sock = {
		.get_name = ping_get_name,
		.get_type = ping_get_type,
		.get_state = tcp_get_state,
		.get_listening = NULL,
		.fill_column = ping_fill_column,
		.free = NULL,
	},
	.scan_line = raw_xinfo_scan_line,
	.get_addr = tcp_xinfo_get_addr,
	.is_any_addr = tcp_xinfo_is_any_addr,
	.family = AF_INET,
	.l3_decorator = {"", ""},
};

static void load_xinfo_from_proc_icmp(ino_t netns_inode, enum sysfs_byteorder byteorder)
{
	load_xinfo_from_proc_inet_L4(netns_inode,
				     "/proc/net/icmp",
				     &ping_xinfo_class,
				     byteorder);
}

/*
 * TCP6
 */
static struct sock_xinfo *tcp6_xinfo_scan_line(const struct sock_xinfo_class *class,
					       char * line,
					       ino_t netns_inode,
					       enum sysfs_byteorder byteorder)
{
	uint32_t local_addr[4];
	unsigned int local_port;
	uint32_t remote_addr[4];
	unsigned int remote_port;
	unsigned int st;
	unsigned long inode;
	struct tcp_xinfo *tcp;
	struct inet6_xinfo *inet6;
	struct sock_xinfo *sock;

	if (sscanf(line,
		   "%*d: "
		   "%08x%08x%08x%08x:%04x "
		   "%08x%08x%08x%08x:%04x "
		   "%x %*x:%*x %*x:%*x %*x %*u %*d %lu ",
		   local_addr+0, local_addr+1, local_addr+2, local_addr+3, &local_port,
		   remote_addr+0, remote_addr+1, remote_addr+2, remote_addr+3, &remote_port,
		   &st, &inode) != 12)
		return NULL;

	if (inode == 0)
		return NULL;

	tcp = xmalloc(sizeof(*tcp));
	inet6 = &tcp->l4.inet6;
	sock = &inet6->sock;
	sock->class = class;
	sock->inode = (ino_t)inode;
	sock->netns_inode = netns_inode;
	tcp->local_port = local_port;
	for (int i = 0; i < 4; i++) {
		inet6->local_addr.s6_addr32[i] = kernel32_to_cpu(byteorder, local_addr[i]);
		inet6->remote_addr.s6_addr32[i] = kernel32_to_cpu(byteorder, remote_addr[i]);
	}
	tcp->remote_port = remote_port;
	tcp->l4.st = st;

	return sock;
}

static bool tcp6_fill_column(struct proc *proc  __attribute__((__unused__)),
			     struct sock_xinfo *sock_xinfo,
			     struct sock *sock  __attribute__((__unused__)),
			     struct libscols_line *ln  __attribute__((__unused__)),
			     int column_id,
			     size_t column_index  __attribute__((__unused__)),
			     char **str)
{
	return l3_fill_column_handler(INET6, sock_xinfo, column_id, str)
		|| l4_fill_column_handler(TCP, sock_xinfo, column_id, str);
}

static void *tcp6_xinfo_get_addr(struct l4_xinfo * l4, enum l4_side side)
{
	return (side == L4_LOCAL)
		? &l4->inet6.local_addr
		: &l4->inet6.remote_addr;
}

static bool tcp6_xinfo_is_any_addr(void *addr)
{
	return IN6_ARE_ADDR_EQUAL(addr, &(struct in6_addr)IN6ADDR_ANY_INIT);
}

static const struct l4_xinfo_class tcp6_xinfo_class = {
	.sock = {
		.get_name = tcp_get_name,
		.get_type = tcp_get_type,
		.get_state = tcp_get_state,
		.get_listening = tcp_get_listening,
		.fill_column = tcp6_fill_column,
		.free = NULL,
	},
	.scan_line = tcp6_xinfo_scan_line,
	.get_addr = tcp6_xinfo_get_addr,
	.is_any_addr = tcp6_xinfo_is_any_addr,
	.family = AF_INET6,
	.l3_decorator = {"[", "]"},
};

static void load_xinfo_from_proc_tcp6(ino_t netns_inode, enum sysfs_byteorder byteorder)
{
	load_xinfo_from_proc_inet_L4(netns_inode,
				     "/proc/net/tcp6",
				     &tcp6_xinfo_class,
				     byteorder);
}

/*
 * UDP6
 */
static bool udp6_fill_column(struct proc *proc  __attribute__((__unused__)),
			     struct sock_xinfo *sock_xinfo,
			     struct sock *sock  __attribute__((__unused__)),
			     struct libscols_line *ln  __attribute__((__unused__)),
			     int column_id,
			     size_t column_index  __attribute__((__unused__)),
			     char **str)
{
	return l3_fill_column_handler(INET6, sock_xinfo, column_id, str)
		|| l4_fill_column_handler(UDP, sock_xinfo, column_id, str);
}

static const struct l4_xinfo_class udp6_xinfo_class = {
	.sock = {
		.get_name = udp_get_name,
		.get_type = udp_get_type,
		.get_state = tcp_get_state,
		.get_listening = NULL,
		.fill_column = udp6_fill_column,
		.free = NULL,
	},
	.scan_line = tcp6_xinfo_scan_line,
	.get_addr = tcp6_xinfo_get_addr,
	.is_any_addr = tcp6_xinfo_is_any_addr,
	.family = AF_INET6,
	.l3_decorator = {"[", "]"},
};

static void load_xinfo_from_proc_udp6(ino_t netns_inode, enum sysfs_byteorder byteorder)
{
	load_xinfo_from_proc_inet_L4(netns_inode,
				     "/proc/net/udp6",
				     &udp6_xinfo_class,
				     byteorder);
}

/*
 * UDPLITEv6
 */
static bool udplite6_fill_column(struct proc *proc __attribute__((__unused__)),
				 struct sock_xinfo *sock_xinfo,
				 struct sock *sock __attribute__((__unused__)),
				 struct libscols_line *ln __attribute__((__unused__)),
				 int column_id,
				 size_t column_index __attribute__((__unused__)),
				 char **str)
{
	return l3_fill_column_handler(INET6, sock_xinfo, column_id, str)
		|| l4_fill_column_handler(UDPLITE, sock_xinfo, column_id, str);
}

static const struct l4_xinfo_class udplite6_xinfo_class = {
	.sock = {
		.get_name = udp_get_name,
		.get_type = udp_get_type,
		.get_state = tcp_get_state,
		.get_listening = NULL,
		.fill_column = udplite6_fill_column,
		.free = NULL,
	},
	.scan_line = tcp6_xinfo_scan_line,
	.get_addr = tcp6_xinfo_get_addr,
	.is_any_addr = tcp6_xinfo_is_any_addr,
	.family = AF_INET6,
	.l3_decorator = {"[", "]"},
};

static void load_xinfo_from_proc_udplite6(ino_t netns_inode, enum sysfs_byteorder byteorder)
{
	load_xinfo_from_proc_inet_L4(netns_inode,
				     "/proc/net/udplite6",
				     &udplite6_xinfo_class,
				     byteorder);
}

/*
 * RAW6
 */
static struct sock_xinfo *raw6_xinfo_scan_line(const struct sock_xinfo_class *class,
					       char * line,
					       ino_t netns_inode,
					       enum sysfs_byteorder byteorder)
{
	uint32_t local_addr[4];
	unsigned int protocol;
	uint32_t remote_addr[4];
	unsigned int st;
	unsigned long inode;
	struct raw_xinfo *raw;
	struct inet6_xinfo *inet6;
	struct sock_xinfo *sock;

	if (sscanf(line,
		   "%*d: "
		   "%08x%08x%08x%08x:%04x "
		   "%08x%08x%08x%08x:0000 "
		   "%x %*x:%*x %*x:%*x %*x %*u %*d %lu ",
		   local_addr+0, local_addr+1, local_addr+2, local_addr+3, &protocol,
		   remote_addr+0, remote_addr+1, remote_addr+2, remote_addr+3,
		   &st, &inode) != 11)
		return NULL;

	if (inode == 0)
		return NULL;

	raw = xmalloc(sizeof(*raw));
	inet6 = &raw->l4.inet6;
	sock = &inet6->sock;
	sock->class = class;
	sock->inode = (ino_t)inode;
	sock->netns_inode = netns_inode;
	for (int i = 0; i < 4; i++) {
		inet6->local_addr.s6_addr32[i] = kernel32_to_cpu(byteorder, local_addr[i]);
		inet6->remote_addr.s6_addr32[i] = kernel32_to_cpu(byteorder, remote_addr[i]);
	}
	raw->protocol = protocol;
	raw->l4.st = st;

	return sock;
}

static bool raw6_fill_column(struct proc *proc  __attribute__((__unused__)),
			     struct sock_xinfo *sock_xinfo,
			     struct sock *sock  __attribute__((__unused__)),
			     struct libscols_line *ln  __attribute__((__unused__)),
			     int column_id,
			     size_t column_index  __attribute__((__unused__)),
			     char **str)
{
	struct raw_xinfo *raw;

	if (l3_fill_column_handler(INET6, sock_xinfo, column_id, str))
		return true;

	raw = (struct raw_xinfo *)sock_xinfo;
	if (column_id == COL_RAW_PROTOCOL) {
		xasprintf(str, "%"PRIu16, raw->protocol);
		return true;
	}

	return false;
}

static const struct l4_xinfo_class raw6_xinfo_class = {
	.sock = {
		.get_name = raw_get_name,
		.get_type = raw_get_type,
		.get_state = tcp_get_state,
		.get_listening = NULL,
		.fill_column = raw6_fill_column,
		.free = NULL,
	},
	.scan_line = raw6_xinfo_scan_line,
	.get_addr = tcp6_xinfo_get_addr,
	.is_any_addr = tcp6_xinfo_is_any_addr,
	.family = AF_INET6,
	.l3_decorator = {"[", "]"},
};

static void load_xinfo_from_proc_raw6(ino_t netns_inode, enum sysfs_byteorder byteorder)
{
	load_xinfo_from_proc_inet_L4(netns_inode,
				     "/proc/net/raw6",
				     &raw6_xinfo_class,
				     byteorder);
}

/*
 * PINGv6
 */
static bool ping6_fill_column(struct proc *proc __attribute__((__unused__)),
			     struct sock_xinfo *sock_xinfo,
			     struct sock *sock __attribute__((__unused__)),
			     struct libscols_line *ln __attribute__((__unused__)),
			     int column_id,
			     size_t column_index __attribute__((__unused__)),
			     char **str)
{
	if (l3_fill_column_handler(INET6, sock_xinfo, column_id, str))
		return true;

	if (column_id == COL_PING_ID) {
		xasprintf(str, "%"PRIu16,
			  ((struct raw_xinfo *)sock_xinfo)->protocol);
		return true;
	}

	return false;
}

static const struct l4_xinfo_class ping6_xinfo_class = {
	.sock = {
		.get_name = ping_get_name,
		.get_type = ping_get_type,
		.get_state = tcp_get_state,
		.get_listening = NULL,
		.fill_column = ping6_fill_column,
		.free = NULL,
	},
	.scan_line = raw6_xinfo_scan_line,
	.get_addr = tcp6_xinfo_get_addr,
	.is_any_addr = tcp6_xinfo_is_any_addr,
	.family = AF_INET6,
	.l3_decorator = {"[", "]"},
};

static void load_xinfo_from_proc_icmp6(ino_t netns_inode, enum sysfs_byteorder byteorder)
{
	load_xinfo_from_proc_inet_L4(netns_inode,
				     "/proc/net/icmp6",
				     &ping6_xinfo_class,
				     byteorder);
}

/*
 * NETLINK
 */
struct netlink_xinfo {
	struct sock_xinfo sock;
	uint16_t protocol;
	uint32_t lportid;	/* netlink_diag may provide rportid.  */
	uint32_t groups;
};

static const char *netlink_decode_protocol(uint16_t protocol)
{
	switch (protocol) {
	case NETLINK_ROUTE:
		return "route";
	case NETLINK_UNUSED:
		return "unused";
	case NETLINK_USERSOCK:
		return "usersock";
	case NETLINK_FIREWALL:
		return "firewall";
	case NETLINK_SOCK_DIAG:
		return "sock_diag";
	case NETLINK_NFLOG:
		return "nflog";
	case NETLINK_XFRM:
		return "xfrm";
	case NETLINK_SELINUX:
		return "selinux";
	case NETLINK_ISCSI:
		return "iscsi";
	case NETLINK_AUDIT:
		return "audit";
	case NETLINK_FIB_LOOKUP:
		return "fib_lookup";
	case NETLINK_CONNECTOR:
		return "connector";
	case NETLINK_NETFILTER:
		return "netfilter";
	case NETLINK_IP6_FW:
		return "ip6_fw";
	case NETLINK_DNRTMSG:
		return "dnrtmsg";
	case NETLINK_KOBJECT_UEVENT:
		return "kobject_uevent";
	case NETLINK_GENERIC:
		return "generic";
	case NETLINK_SCSITRANSPORT:
		return "scsitransport";
	case NETLINK_ECRYPTFS:
		return "ecryptfs";
	case NETLINK_RDMA:
		return "rdma";
	case NETLINK_CRYPTO:
		return "crypto";
#ifdef NETLINK_SMC
	case NETLINK_SMC:
		return "smc";
#endif
	default:
		return "unknown";
	}
}

static char *netlink_get_name(struct sock_xinfo *sock_xinfo,
			      struct sock *sock __attribute__((__unused__)))
{
	struct netlink_xinfo *nl = (struct netlink_xinfo *)sock_xinfo;
	char *str = NULL;
	const char *protocol = netlink_decode_protocol(nl->protocol);

	if (nl->groups)
		xasprintf(&str, "protocol=%s lport=%"PRIu16 " groups=%"PRIu32,
			  protocol,
			  nl->lportid, nl->groups);
	else
		xasprintf(&str, "protocol=%s lport=%"PRIu16,
			  protocol,
			  nl->lportid);
	return str;
}

static char *netlink_get_type(struct sock_xinfo *sock_xinfo __attribute__((__unused__)),
			      struct sock *sock __attribute__((__unused__)))
{
	return xstrdup("raw");
}

static bool netlink_fill_column(struct proc *proc __attribute__((__unused__)),
				struct sock_xinfo *sock_xinfo,
				struct sock *sock __attribute__((__unused__)),
				struct libscols_line *ln __attribute__((__unused__)),
				int column_id,
				size_t column_index __attribute__((__unused__)),
				char **str)
{
	struct netlink_xinfo *nl = (struct netlink_xinfo *)sock_xinfo;

	switch (column_id) {
	case COL_NETLINK_GROUPS:
		xasprintf(str, "%"PRIu32, nl->groups);
		return true;
	case COL_NETLINK_LPORT:
		xasprintf(str, "%"PRIu32, nl->lportid);
		return true;
	case COL_NETLINK_PROTOCOL:
		*str = xstrdup(netlink_decode_protocol(nl->protocol));
		return true;
	}

	return false;
}

static const struct sock_xinfo_class netlink_xinfo_class = {
	.get_name = netlink_get_name,
	.get_type = netlink_get_type,
	.get_state = NULL,
	.get_listening = NULL,
	.fill_column = netlink_fill_column,
	.free = NULL,
};

static void load_xinfo_from_proc_netlink(ino_t netns_inode)
{
	char line[BUFSIZ];
	FILE *netlink_fp;

	netlink_fp = fopen("/proc/net/netlink", "r");
	if (!netlink_fp)
		return;

	if (fgets(line, sizeof(line), netlink_fp) == NULL)
		goto out;
	if (!(line[0] == 's' && line[1] == 'k'))
		/* Unexpected line */
		goto out;

	while (fgets(line, sizeof(line), netlink_fp)) {
		uint16_t protocol;
		uint32_t lportid;
		uint32_t groups;
		unsigned long inode;
		struct netlink_xinfo *nl;

		if (sscanf(line, "%*x %" SCNu16 " %" SCNu32 " %" SCNx32 " %*d %*d %*d %*d %*u %lu",
			   &protocol, &lportid, &groups, &inode) < 4)
			continue;

		if (inode == 0)
			continue;

		nl = xcalloc(1, sizeof(*nl));
		nl->sock.class = &netlink_xinfo_class;
		nl->sock.inode = (ino_t)inode;
		nl->sock.netns_inode = netns_inode;

		nl->protocol = protocol;
		nl->lportid = lportid;
		nl->groups = groups;

		add_sock_info(&nl->sock);
	}

 out:
	fclose(netlink_fp);
}

/*
 * PACKET
 */
struct packet_xinfo {
	struct sock_xinfo sock;
	uint16_t type;
	uint16_t protocol;
	unsigned int iface;
};

static const char *packet_decode_protocol(uint16_t proto)
{
	switch (proto) {
	case 0:
		return NULL;
	case ETH_P_802_3:
		return "802_3";
	case ETH_P_AX25:
		return "ax25";
	case ETH_P_ALL:
		return "all";
	case ETH_P_802_2:
		return "802_2";
	case ETH_P_SNAP:
		return "snap";
	case ETH_P_DDCMP:
		return "ddcmp";
	case ETH_P_WAN_PPP:
		return "wan_ppp";
	case ETH_P_PPP_MP:
		return "ppp_mp";
	case ETH_P_LOCALTALK:
		return "localtalk";
	case ETH_P_CAN:
		return "can";
	case ETH_P_CANFD:
		return "canfd";
#ifdef ETH_P_CANXL
	case ETH_P_CANXL:
		return "canxl";
#endif
	case ETH_P_PPPTALK:
		return "ppptalk";
	case ETH_P_TR_802_2:
		return "tr_802_2";
	case ETH_P_MOBITEX:
		return "mobitex";
	case ETH_P_CONTROL:
		return "control";
	case ETH_P_IRDA:
		return "irda";
	case ETH_P_ECONET:
		return "econet";
	case ETH_P_HDLC:
		return "hdlc";
	case ETH_P_ARCNET:
		return "arcnet";
	case ETH_P_DSA:
		return "dsa";
	case ETH_P_TRAILER:
		return "trailer";
	case ETH_P_PHONET:
		return "phonet";
	case ETH_P_IEEE802154:
		return "ieee802154";
	case ETH_P_CAIF:
		return "caif";
#ifdef ETH_P_XDSA
	case ETH_P_XDSA:
		return "xdsa";
#endif
#ifdef ETH_P_MAP
	case ETH_P_MAP:
		return "map";
#endif
#ifdef ETH_P_MCTP
	case ETH_P_MCTP:
		return "mctp";
#endif
	case ETH_P_LOOP:
		return "loop";
	case ETH_P_PUP:
		return "pup";
	case ETH_P_PUPAT:
		return "pupat";
#ifdef ETH_P_TSN
	case ETH_P_TSN:
		return "tsn";
#endif
#ifdef ETH_P_ERSPAN2
	case ETH_P_ERSPAN2:
		return "erspan2";
#endif
	case ETH_P_IP:
		return "ip";
	case ETH_P_X25:
		return "x25";
	case ETH_P_ARP:
		return "arp";
	case ETH_P_BPQ:
		return "bpq";
	case ETH_P_IEEEPUP:
		return "ieeepup";
	case ETH_P_IEEEPUPAT:
		return "ieeepupat";
	case ETH_P_BATMAN:
		return "batman";
	case ETH_P_DEC:
		return "dec";
	case ETH_P_DNA_DL:
		return "dna_dl";
	case ETH_P_DNA_RC:
		return "dna_rc";
	case ETH_P_DNA_RT:
		return "dna_rt";
	case ETH_P_LAT:
		return "lat";
	case ETH_P_DIAG:
		return "diag";
	case ETH_P_CUST:
		return "cust";
	case ETH_P_SCA:
		return "sca";
	case ETH_P_TEB:
		return "teb";
	case ETH_P_RARP:
		return "rarp";
	case ETH_P_ATALK:
		return "atalk";
	case ETH_P_AARP:
		return "aarp";
	case ETH_P_8021Q:
		return "8021q";
#ifdef ETH_P_ERSPAN
	case ETH_P_ERSPAN:
		return "erspan";
#endif
	case ETH_P_IPX:
		return "ipx";
	case ETH_P_IPV6:
		return "ipv6";
	case ETH_P_PAUSE:
		return "pause";
	case ETH_P_SLOW:
		return "slow";
	case ETH_P_WCCP:
		return "wccp";
	case ETH_P_MPLS_UC:
		return "mpls_uc";
	case ETH_P_MPLS_MC:
		return "mpls_mc";
	case ETH_P_ATMMPOA:
		return "atmmpoa";
#ifdef ETH_P_PPP_DISC
	case ETH_P_PPP_DISC:
		return "ppp_disc";
#endif
#ifdef ETH_P_PPP_SES
	case ETH_P_PPP_SES:
		return "ppp_ses";
#endif
	case ETH_P_LINK_CTL:
		return "link_ctl";
	case ETH_P_ATMFATE:
		return "atmfate";
	case ETH_P_PAE:
		return "pae";
#ifdef ETH_P_PROFINET
	case ETH_P_PROFINET:
		return "profinet";
#endif
#ifdef ETH_P_REALTEK
	case ETH_P_REALTEK:
		return "realtek";
#endif
	case ETH_P_AOE:
		return "aoe";
#ifdef ETH_P_ETHERCAT
	case ETH_P_ETHERCAT:
		return "ethercat";
#endif
	case ETH_P_8021AD:
		return "8021ad";
	case ETH_P_802_EX1:
		return "802_ex1";
#ifdef ETH_P_PREAUTH
	case ETH_P_PREAUTH:
		return "preauth";
#endif
	case ETH_P_TIPC:
		return "tipc";
#ifdef ETH_P_LLDP
	case ETH_P_LLDP:
		return "lldp";
#endif
#ifdef ETH_P_MRP
	case ETH_P_MRP:
		return "mrp";
#endif
#ifdef ETH_P_MACSEC
	case ETH_P_MACSEC:
		return "macsec";
#endif
	case ETH_P_8021AH:
		return "8021ah";
#ifdef ETH_P_MVRP
	case ETH_P_MVRP:
		return "mvrp";
#endif
	case ETH_P_1588:
		return "1588";
#ifdef ETH_P_NCSI
	case ETH_P_NCSI:
		return "ncsi";
#endif
#ifdef ETH_P_PRP
	case ETH_P_PRP:
		return "prp";
#endif
#ifdef ETH_P_CFM
	case ETH_P_CFM:
		return "cfm";
#endif
	case ETH_P_FCOE:
		return "fcoe";
#ifdef ETH_P_IBOE
	case ETH_P_IBOE:
		return "iboe";
#endif
	case ETH_P_TDLS:
		return "tdls";
	case ETH_P_FIP:
		return "fip";
#ifdef ETH_P_80221
	case ETH_P_80221:
		return "80221";
#endif
#ifdef ETH_P_HSR
	case ETH_P_HSR:
		return "hsr";
#endif
#ifdef ETH_P_NSH
	case ETH_P_NSH:
		return "nsh";
#endif
#ifdef ETH_P_LOOPBACK
	case ETH_P_LOOPBACK:
		return "loopback";
#endif
	case ETH_P_QINQ1:
		return "qinq1";
	case ETH_P_QINQ2:
		return "qinq2";
	case ETH_P_QINQ3:
		return "qinq3";
	case ETH_P_EDSA:
		return "edsa";
#ifdef ETH_P_DSA_8021Q
	case ETH_P_DSA_8021Q:
		return "dsa_8021q";
#endif
#ifdef ETH_P_DSA_A5PSW
	case ETH_P_DSA_A5PSW:
		return "dsa_a5psw";
#endif
#ifdef ETH_P_IFE
	case ETH_P_IFE:
		return "ife";
#endif
	case ETH_P_AF_IUCV:
		return "af_iucv";
#ifdef ETH_P_802_3_MIN
	case ETH_P_802_3_MIN:
		return "802_3_min";
#endif
	default:
		return "unknown";
	}
}

static char *packet_get_name(struct sock_xinfo *sock_xinfo,
			     struct sock *sock __attribute__((__unused__)))
{
	struct packet_xinfo *pkt = (struct packet_xinfo *)sock_xinfo;
	char *str = NULL;
	const char *type = sock_decode_type(pkt->type);
	const char *proto = packet_decode_protocol(pkt->protocol);
	const char *iface = get_iface_name(sock_xinfo->netns_inode,
					   pkt->iface);

	if (iface && proto)
		xasprintf(&str, "type=%s protocol=%s iface=%s",
			  type, proto, iface);
	else if (proto)
		xasprintf(&str, "type=%s protocol=%s",
			  type, proto);
	else if (iface)
		xasprintf(&str, "type=%s iface=%s",
			  type, iface);
	else
		xasprintf(&str, "type=%s", type);

	return str;
}

static char *packet_get_type(struct sock_xinfo *sock_xinfo __attribute__((__unused__)),
			     struct sock *sock __attribute__((__unused__)))
{
	const char *str;
	struct packet_xinfo *pkt = (struct packet_xinfo *)sock_xinfo;

	str = sock_decode_type(pkt->type);
	return xstrdup(str);
}

static bool packet_fill_column(struct proc *proc __attribute__((__unused__)),
			       struct sock_xinfo *sock_xinfo,
			       struct sock *sock __attribute__((__unused__)),
			       struct libscols_line *ln __attribute__((__unused__)),
			       int column_id,
			       size_t column_index __attribute__((__unused__)),
			       char **str)
{
	struct packet_xinfo *pkt = (struct packet_xinfo *)sock_xinfo;

	switch (column_id) {
	case COL_PACKET_IFACE: {
		const char *iface;
		iface = get_iface_name(sock_xinfo->netns_inode,
				       pkt->iface);
		if (iface) {
			*str = xstrdup(iface);
			return true;
		}
		break;
	}
	case COL_PACKET_PROTOCOL: {
		const char *proto;
		proto = packet_decode_protocol(pkt->protocol);
		if (proto) {
			*str = xstrdup(proto);
			return true;
		}
		break;
	}
	default:
		break;
	}
	return false;
}

static const struct sock_xinfo_class packet_xinfo_class = {
	.get_name = packet_get_name,
	.get_type = packet_get_type,
	.get_state = NULL,
	.get_listening = NULL,
	.fill_column = packet_fill_column,
	.free = NULL,
};

static void load_xinfo_from_proc_packet(ino_t netns_inode)
{
	char line[BUFSIZ];
	FILE *packet_fp;

	packet_fp = fopen("/proc/net/packet", "r");
	if (!packet_fp)
		return;

	if (fgets(line, sizeof(line), packet_fp) == NULL)
		goto out;
	if (!(line[0] == 's' && line[1] == 'k'))
		/* Unexpected line */
		goto out;

	while (fgets(line, sizeof(line), packet_fp)) {
		uint16_t type;
		uint16_t protocol;
		unsigned int iface;
		unsigned long inode;
		struct packet_xinfo *pkt;

		if (sscanf(line, "%*x %*d %" SCNu16 " %" SCNu16 " %u %*d %*d %*d %lu",
			   &type, &protocol, &iface, &inode) < 4)
			continue;

		pkt = xcalloc(1, sizeof(*pkt));
		pkt->sock.class = &packet_xinfo_class;
		pkt->sock.inode = (ino_t)inode;
		pkt->sock.netns_inode = netns_inode;

		pkt->type = type;
		pkt->protocol = protocol;
		pkt->iface = iface;

		add_sock_info(&pkt->sock);
	}

 out:
	fclose(packet_fp);
}
