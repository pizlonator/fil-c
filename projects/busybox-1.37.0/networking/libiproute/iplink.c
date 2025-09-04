/* vi: set sw=4 ts=4: */
/*
 * Authors: Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 * 			Patrick McHardy <kaber@trash.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include <net/if.h>
/*#include <net/if_packet.h> - not needed? */
#include <netpacket/packet.h>
#include <netinet/if_ether.h>

#include <linux/if_vlan.h>
#if ENABLE_FEATURE_IP_LINK_CAN
# include <linux/can/netlink.h>
#endif
#include "ip_common.h"  /* #include "libbb.h" is inside */
#include "rt_names.h"
#include "utils.h"

#undef  ETH_P_8021AD
#define ETH_P_8021AD            0x88A8
#undef  VLAN_FLAG_REORDER_HDR
#define VLAN_FLAG_REORDER_HDR   0x1
#undef  VLAN_FLAG_GVRP
#define VLAN_FLAG_GVRP          0x2
#undef  VLAN_FLAG_LOOSE_BINDING
#define VLAN_FLAG_LOOSE_BINDING 0x4
#undef  VLAN_FLAG_MVRP
#define VLAN_FLAG_MVRP          0x8
#undef  IFLA_VLAN_PROTOCOL
#define IFLA_VLAN_PROTOCOL      5

#ifndef NLMSG_TAIL
#define NLMSG_TAIL(nmsg) \
	((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))
#endif

#ifndef IFLA_LINKINFO
# define IFLA_LINKINFO 18
# define IFLA_INFO_KIND 1
# define IFLA_INFO_DATA 2
#endif

#ifndef IFLA_VLAN_MAX
# define IFLA_VLAN_ID 1
# define IFLA_VLAN_FLAGS 2
struct ifla_vlan_flags {
	uint32_t	flags;
	uint32_t	mask;
};
#endif

/* taken from linux/sockios.h */
#define SIOCSIFNAME  0x8923  /* set interface name */

#if 0
# define dbg(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif


#define str_on_off "on\0""off\0"

enum {
	PARM_on = 0,
	PARM_off
};

/* Exits on error */
static int get_ctl_fd(void)
{
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;
	fd = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;
	return xsocket(PF_INET6, SOCK_DGRAM, 0);
}

/* Exits on error */
static void do_chflags(char *dev, uint32_t flags, uint32_t mask)
{
	struct ifreq ifr;
	int fd;

	strncpy_IFNAMSIZ(ifr.ifr_name, dev);
	fd = get_ctl_fd();
	xioctl(fd, SIOCGIFFLAGS, &ifr);
	if ((ifr.ifr_flags ^ flags) & mask) {
		ifr.ifr_flags &= ~mask;
		ifr.ifr_flags |= mask & flags;
		xioctl(fd, SIOCSIFFLAGS, &ifr);
	}
	close(fd);
}

/* Exits on error */
static void do_changename(char *dev, char *newdev)
{
	struct ifreq ifr;
	int fd;

	strncpy_IFNAMSIZ(ifr.ifr_name, dev);
	strncpy_IFNAMSIZ(ifr.ifr_newname, newdev);
	fd = get_ctl_fd();
	xioctl(fd, SIOCSIFNAME, &ifr);
	close(fd);
}

/* Exits on error */
static void set_qlen(char *dev, int qlen)
{
	struct ifreq ifr;
	int s;

	s = get_ctl_fd();
	memset(&ifr, 0, sizeof(ifr));
	strncpy_IFNAMSIZ(ifr.ifr_name, dev);
	ifr.ifr_qlen = qlen;
	xioctl(s, SIOCSIFTXQLEN, &ifr);
	close(s);
}

/* Exits on error */
static void set_mtu(char *dev, int mtu)
{
	struct ifreq ifr;
	int s;

	s = get_ctl_fd();
	memset(&ifr, 0, sizeof(ifr));
	strncpy_IFNAMSIZ(ifr.ifr_name, dev);
	ifr.ifr_mtu = mtu;
	xioctl(s, SIOCSIFMTU, &ifr);
	close(s);
}

/* Exits on error */
static void set_master(char *dev, int master)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr  n;
		struct ifinfomsg i;
		char             buf[1024];
	} req;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_NEWLINK;
	req.i.ifi_family = preferred_family;

	xrtnl_open(&rth);
	req.i.ifi_index = xll_name_to_index(dev);
	//printf("master %i for %i\n", master, req.i.ifi_index);
	addattr_l(&req.n, sizeof(req), IFLA_MASTER, &master, 4);
	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		xfunc_die();
}

/* Exits on error */
static void set_netns(char *dev, int netns)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr  n;
		struct ifinfomsg i;
		char             buf[1024];
	} req;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_NEWLINK;
	req.i.ifi_family = preferred_family;

	xrtnl_open(&rth);
	req.i.ifi_index = xll_name_to_index(dev);
	//printf("netns %i for %i\n", netns, req.i.ifi_index);
	addattr_l(&req.n, sizeof(req), IFLA_NET_NS_PID, &netns, 4);
	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		xfunc_die();
}

/* Exits on error */
static int get_address(char *dev, int *htype)
{
	struct ifreq ifr;
	struct sockaddr_ll me;
	int s;

	s = xsocket(PF_PACKET, SOCK_DGRAM, 0);

	/*memset(&ifr, 0, sizeof(ifr)); - SIOCGIFINDEX does not need to clear all */
	strncpy_IFNAMSIZ(ifr.ifr_name, dev);
	xioctl(s, SIOCGIFINDEX, &ifr);

	memset(&me, 0, sizeof(me));
	me.sll_family = AF_PACKET;
	me.sll_ifindex = ifr.ifr_ifindex;
	me.sll_protocol = htons(ETH_P_LOOP);
	xbind(s, (struct sockaddr*)&me, sizeof(me));
	bb_getsockname(s, (struct sockaddr*)&me, sizeof(me));
	//never happens:
	//if (getsockname(s, (struct sockaddr*)&me, &alen) == -1)
	//	bb_perror_msg_and_die("getsockname");
	close(s);
	*htype = me.sll_hatype;
	return me.sll_halen;
}

/* Exits on error */
static void parse_address(char *dev, int hatype, int halen, char *lla, struct ifreq *ifr)
{
	int alen;

	memset(ifr, 0, sizeof(*ifr));
	strncpy_IFNAMSIZ(ifr->ifr_name, dev);
	ifr->ifr_hwaddr.sa_family = hatype;

	alen = hatype == 1/*ARPHRD_ETHER*/ ? 14/*ETH_HLEN*/ : 19/*INFINIBAND_HLEN*/;
	alen = ll_addr_a2n((unsigned char *)(ifr->ifr_hwaddr.sa_data), alen, lla);
	if (alen < 0)
		exit_FAILURE();
	if (alen != halen) {
		bb_error_msg_and_die("wrong address (%s) length: expected %d bytes", lla, halen);
	}
}

/* Exits on error */
static void set_address(struct ifreq *ifr, int brd)
{
	int s;

	s = get_ctl_fd();
	if (brd)
		xioctl(s, SIOCSIFHWBROADCAST, ifr);
	else
		xioctl(s, SIOCSIFHWADDR, ifr);
	close(s);
}


static void die_must_be_on_off(const char *msg) NORETURN;
static void die_must_be_on_off(const char *msg)
{
	bb_error_msg_and_die("argument of \"%s\" must be \"on\" or \"off\"", msg);
}

#if ENABLE_FEATURE_IP_LINK_CAN
static uint32_t get_float_1000(char *arg, const char *errmsg)
{
	uint32_t ret;
	double d;
	char *ptr;

	errno = 0;
//TODO: needs setlocale(LC_NUMERIC, "C")?
	d = strtod(arg, &ptr);
	if (errno || ptr == arg || *ptr
	 || d > (0xFFFFFFFFU / 1000) || d < 0
	) {
		invarg_1_to_2(arg, errmsg); /* does not return */
	}
	ret = d * 1000;

	return ret;
}

static void do_set_can(char *dev, char **argv)
{
	struct can_bittiming bt = {}, dbt = {};
	struct can_ctrlmode cm = {};
	char *keyword;
	static const char keywords[] ALIGN1 =
		"bitrate\0""sample-point\0""tq\0"
		"prop-seg\0""phase-seg1\0""phase-seg2\0""sjw\0"
		"dbitrate\0""dsample-point\0""dtq\0"
		"dprop-seg\0""dphase-seg1\0""dphase-seg2\0""dsjw\0"
		"loopback\0""listen-only\0""triple-sampling\0"
		"one-shot\0""berr-reporting\0"
		"fd\0""fd-non-iso\0""presume-ack\0"
		"cc-len8-dlc\0""restart\0""restart-ms\0"
		"termination\0";
	enum { ARG_bitrate = 0, ARG_sample_point, ARG_tq,
		ARG_prop_seg, ARG_phase_seg1, ARG_phase_seg2, ARG_sjw,
		ARG_dbitrate, ARG_dsample_point, ARG_dtq,
		ARG_dprop_seg, ARG_dphase_seg1, ARG_dphase_seg2, ARG_dsjw,
		ARG_loopback, ARG_listen_only, ARG_triple_sampling,
		ARG_one_shot, ARG_berr_reporting,
		ARG_fd, ARG_fd_non_iso, ARG_presume_ack,
		ARG_cc_len8_dlc, ARG_restart, ARG_restart_ms,
		ARG_termination };
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr  n;
		struct ifinfomsg i;
		char buf[1024];
	} req;
	size_t dev_len;
	struct rtattr *linkinfo, *data;
	smalluint key, param;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_NEWLINK;
	req.i.ifi_family = preferred_family;
	xrtnl_open(&rth);
	req.i.ifi_index = xll_name_to_index(dev);
	dev_len = strlen(dev);
	if (dev_len < 2 || dev_len > IFNAMSIZ)
		invarg_1_to_2(dev, "dev");

	addattr_l(&req.n, sizeof(req), IFLA_IFNAME, dev, dev_len);
	linkinfo = NLMSG_TAIL(&req.n);
	addattr_l(&req.n, sizeof(req), IFLA_LINKINFO, NULL, 0);
	addattr_l(&req.n, sizeof(req), IFLA_INFO_KIND, (void *)"can",
		  strlen("can"));
	data = NLMSG_TAIL(&req.n);
	addattr_l(&req.n, sizeof(req), IFLA_INFO_DATA, NULL, 0);
	while (*argv) {
		key = index_in_substrings(keywords, *argv);
		keyword = *argv;
		//printf("%s: key: %d, *argv: %s\n", __func__, key, *argv);
		switch (key) {
		case ARG_bitrate:
		case ARG_tq:
		case ARG_prop_seg:
		case ARG_phase_seg1:
		case ARG_phase_seg2:
		case ARG_sjw: {
			uint32_t *val;
			NEXT_ARG();
			if (key == ARG_bitrate)
				val = &bt.bitrate;
			else if (key == ARG_tq)
				val = &bt.tq;
			else if (key == ARG_prop_seg)
				val = &bt.prop_seg;
			else if (key == ARG_phase_seg1)
				val = &bt.phase_seg1;
			else if (key == ARG_phase_seg2)
				val = &bt.phase_seg2;
			else
				val = &bt.sjw;

			*val = get_u32(*argv, keyword);
			break;
		}
		case ARG_sample_point: {
			NEXT_ARG();
			bt.sample_point = get_float_1000(*argv, keyword);
			break;
		}
		case ARG_dbitrate:
		case ARG_dtq:
		case ARG_dprop_seg:
		case ARG_dphase_seg1:
		case ARG_dphase_seg2:
		case ARG_dsjw: {
			uint32_t *val;
			NEXT_ARG();
			if (key == ARG_dbitrate)
				val = &dbt.bitrate;
			else if (key == ARG_dtq)
				val = &dbt.tq;
			else if (key == ARG_dprop_seg)
				val = &dbt.prop_seg;
			else if (key == ARG_dphase_seg1)
				val = &dbt.phase_seg1;
			else if (key == ARG_dphase_seg2)
				val = &dbt.phase_seg2;
			else
				val = &dbt.sjw;

			*val = get_u32(*argv, keyword);
			break;
		}
		case ARG_dsample_point: {
			NEXT_ARG();
			dbt.sample_point = get_float_1000(*argv, keyword);
			break;
		}
		case ARG_loopback:
		case ARG_listen_only:
		case ARG_triple_sampling:
		case ARG_one_shot:
		case ARG_berr_reporting:
		case ARG_fd:
		case ARG_fd_non_iso:
		case ARG_presume_ack:
		case ARG_cc_len8_dlc: {
			uint32_t flag = 0;

			NEXT_ARG();
			param = index_in_strings(str_on_off, *argv);
			if (param < 0)
				die_must_be_on_off(keyword);

			if (key == ARG_loopback)
				flag = CAN_CTRLMODE_LOOPBACK;
			else if (key == ARG_listen_only)
				flag = CAN_CTRLMODE_LISTENONLY;
			else if (key == ARG_triple_sampling)
				flag = CAN_CTRLMODE_3_SAMPLES;
			else if (key == ARG_one_shot)
				flag = CAN_CTRLMODE_ONE_SHOT;
			else if (key == ARG_berr_reporting)
				flag = CAN_CTRLMODE_BERR_REPORTING;
			else if (key == ARG_fd)
				flag = CAN_CTRLMODE_FD;
			else if (key == ARG_fd_non_iso)
				flag = CAN_CTRLMODE_FD_NON_ISO;
			else if (key == ARG_presume_ack)
				flag = CAN_CTRLMODE_PRESUME_ACK;
			else
#if defined(CAN_CTRLMODE_CC_LEN8_DLC)
				flag = CAN_CTRLMODE_CC_LEN8_DLC;
#else
				die_must_be_on_off(keyword);
#endif
			cm.mask |= flag;
			if (param == PARM_on)
				cm.flags |= flag;

			break;
		}
		case ARG_restart: {
			uint32_t val = 1;
			/*NEXT_ARG(); - WRONG? */
			addattr_l(&req.n, sizeof(req), IFLA_CAN_RESTART, &val, sizeof(val));
			break;
		}
		case ARG_restart_ms: {
			uint32_t val;
			NEXT_ARG();
			val = get_u32(*argv, keyword);
			addattr_l(&req.n, sizeof(req), IFLA_CAN_RESTART_MS, &val, sizeof(val));
			break;
		}
		case ARG_termination: {
			uint16_t val;
			NEXT_ARG();
			val = get_u16(*argv, keyword);
			addattr_l(&req.n, sizeof(req), IFLA_CAN_TERMINATION, &val, sizeof(val));
			break;
		}
		default:
			break;
		}
		argv++;
	}

	if (bt.bitrate || bt.tq)
		addattr_l(&req.n, sizeof(req), IFLA_CAN_BITTIMING, &bt, sizeof(bt));

	if (cm.mask)
		addattr_l(&req.n, sizeof(req), IFLA_CAN_CTRLMODE, &cm, sizeof(cm));

	data->rta_len = (void *)NLMSG_TAIL(&req.n) - (void *)data;
	linkinfo->rta_len = (void *)NLMSG_TAIL(&req.n) - (void *)linkinfo;

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		xfunc_die();
}

static void set_type(char *type, char *dev, char **argv)
{
/* When we have more than just "type can ARGS" supported, maybe:
	static const char keywords[] ALIGN1 = ""
		IF_FEATURE_IP_LINK_CAN("can\0")
		;
	typedef void FAST_FUNC(*ip_type_set_func_ptr_t)(char*, char**);
	static const ip_type_set_func_ptr_t funcs[] ALIGN_PTR = {
		IF_FEATURE_IP_LINK_CAN(do_set_can,)
	};
	ip_type_set_func_ptr_t func;
	int key;

	key = index_in_substrings(keywords, type);
	if (key < 0)
		invarg_1_to_2(type, "type");
	func = funcs[key];
	func(dev, argv);
*/
	if (strcmp(type, "can") != 0)
		invarg_1_to_2(type, "type");
	do_set_can(dev, argv);
}
#endif

/* Return value becomes exitcode. It's okay to not return at all */
static int do_set(char **argv)
{
	char *dev = NULL;
#if ENABLE_FEATURE_IP_LINK_CAN
	char *type = NULL;
#endif
	uint32_t mask = 0;
	uint32_t flags = 0;
	int qlen = -1;
	int mtu = -1;
	int master = -1;
	int netns = -1;
	char *newaddr = NULL;
	char *newbrd = NULL;
	struct ifreq ifr0, ifr1;
	char *newname = NULL;
	int htype, halen;
	/* If you add stuff here, update iplink_full_usage */
	static const char keywords[] ALIGN1 =
		"up\0""down\0""name\0""mtu\0""qlen\0""multicast\0"
		"arp\0""promisc\0""address\0""netns\0"
		"master\0""nomaster\0"
#if ENABLE_FEATURE_IP_LINK_CAN
		"type\0"
#endif
		"dev\0" /* must be last */;
	enum { ARG_up = 0, ARG_down, ARG_name, ARG_mtu, ARG_qlen, ARG_multicast,
		ARG_arp, ARG_promisc, ARG_addr, ARG_netns,
		ARG_master, ARG_nomaster,
#if ENABLE_FEATURE_IP_LINK_CAN
		ARG_type,
#endif
		ARG_dev };
	smalluint key;

	while (*argv) {
		/* substring search ensures that e.g. "addr" and "address"
		 * are both accepted */
		key = index_in_substrings(keywords, *argv);
		//printf("%s: key: %d, *argv: %s\n", __func__, key, *argv);
		if (key == ARG_up) {
			mask |= IFF_UP;
			flags |= IFF_UP;
		} else if (key == ARG_down) {
			mask |= IFF_UP;
			flags &= ~IFF_UP;
		} else if (key == ARG_name) {
			NEXT_ARG();
			newname = *argv;
		} else if (key == ARG_mtu) {
			NEXT_ARG();
			if (mtu != -1)
				duparg("mtu", *argv);
			mtu = get_unsigned(*argv, "mtu");
		} else if (key == ARG_qlen) {
//TODO: txqueuelen, txqlen are synonyms to qlen
			NEXT_ARG();
			if (qlen != -1)
				duparg("qlen", *argv);
			qlen = get_unsigned(*argv, "qlen");
		} else if (key == ARG_addr) {
			NEXT_ARG();
			newaddr = *argv;
		} else if (key == ARG_master) {
			NEXT_ARG();
			master = xll_name_to_index(*argv);
		} else if (key == ARG_nomaster) {
			master = 0;
		} else if (key == ARG_netns) {
			NEXT_ARG();
			netns = get_unsigned(*argv, "netns");
#if ENABLE_FEATURE_IP_LINK_CAN
		} else if (key == ARG_type) {
			NEXT_ARG();
			type = *argv;
			argv++;
			break;
#endif
		} else if (key >= ARG_dev) {
			/* ^^^^^^ ">=" here results in "dev IFACE" treated as default */
			if (key == ARG_dev) {
				NEXT_ARG();
			}
			if (dev)
				duparg2("dev", *argv);

			dev = *argv;
		} else {
			/* "on|off" options */
			int param;
			NEXT_ARG();
			param = index_in_strings(str_on_off, *argv);
			if (key == ARG_multicast) {
				if (param < 0)
					die_must_be_on_off("multicast");
				mask |= IFF_MULTICAST;
				if (param == PARM_on)
					flags |= IFF_MULTICAST;
				else
					flags &= ~IFF_MULTICAST;
			} else if (key == ARG_arp) {
				if (param < 0)
					die_must_be_on_off("arp");
				mask |= IFF_NOARP;
				if (param == PARM_on)
					flags &= ~IFF_NOARP;
				else
					flags |= IFF_NOARP;
			} else if (key == ARG_promisc) {
				if (param < 0)
					die_must_be_on_off("promisc");
				mask |= IFF_PROMISC;
				if (param == PARM_on)
					flags |= IFF_PROMISC;
				else
					flags &= ~IFF_PROMISC;
			}
		}

/* Other keywords recognized by iproute2-3.12.0: */
#if 0
		} else if (matches(*argv, "broadcast") == 0 ||
				strcmp(*argv, "brd") == 0) {
			NEXT_ARG();
			len = ll_addr_a2n(abuf, sizeof(abuf), *argv);
			if (len < 0)
				return -1;
			addattr_l(&req->n, sizeof(*req), IFLA_BROADCAST, abuf, len);
                } else if (strcmp(*argv, "netns") == 0) {
                        NEXT_ARG();
                        if (netns != -1)
                                duparg("netns", *argv);
			if ((netns = get_netns_fd(*argv)) >= 0)
				addattr_l(&req->n, sizeof(*req), IFLA_NET_NS_FD, &netns, 4);
			else if (get_integer(&netns, *argv, 0) == 0)
				addattr_l(&req->n, sizeof(*req), IFLA_NET_NS_PID, &netns, 4);
			else
                                invarg_1_to_2(*argv, "netns");
		} else if (strcmp(*argv, "allmulticast") == 0) {
			NEXT_ARG();
			req->i.ifi_change |= IFF_ALLMULTI;
			if (strcmp(*argv, "on") == 0) {
				req->i.ifi_flags |= IFF_ALLMULTI;
			} else if (strcmp(*argv, "off") == 0) {
				req->i.ifi_flags &= ~IFF_ALLMULTI;
			} else
				return on_off("allmulticast", *argv);
		} else if (strcmp(*argv, "trailers") == 0) {
			NEXT_ARG();
			req->i.ifi_change |= IFF_NOTRAILERS;
			if (strcmp(*argv, "off") == 0) {
				req->i.ifi_flags |= IFF_NOTRAILERS;
			} else if (strcmp(*argv, "on") == 0) {
				req->i.ifi_flags &= ~IFF_NOTRAILERS;
			} else
				return on_off("trailers", *argv);
		} else if (strcmp(*argv, "vf") == 0) {
			struct rtattr *vflist;
			NEXT_ARG();
			if (get_integer(&vf,  *argv, 0)) {
				invarg_1_to_2(*argv, "vf");
			}
			vflist = addattr_nest(&req->n, sizeof(*req),
					      IFLA_VFINFO_LIST);
			len = iplink_parse_vf(vf, &argc, &argv, req);
			if (len < 0)
				return -1;
			addattr_nest_end(&req->n, vflist);
		} else if (matches(*argv, "master") == 0) {
			int ifindex;
			NEXT_ARG();
			ifindex = ll_name_to_index(*argv);
			if (!ifindex)
				invarg_1_to_2(*argv, "master");
			addattr_l(&req->n, sizeof(*req), IFLA_MASTER,
				  &ifindex, 4);
		} else if (matches(*argv, "nomaster") == 0) {
			int ifindex = 0;
			addattr_l(&req->n, sizeof(*req), IFLA_MASTER,
				  &ifindex, 4);
		} else if (matches(*argv, "dynamic") == 0) {
			NEXT_ARG();
			req->i.ifi_change |= IFF_DYNAMIC;
			if (strcmp(*argv, "on") == 0) {
				req->i.ifi_flags |= IFF_DYNAMIC;
			} else if (strcmp(*argv, "off") == 0) {
				req->i.ifi_flags &= ~IFF_DYNAMIC;
			} else
				return on_off("dynamic", *argv);
		} else if (matches(*argv, "alias") == 0) {
			NEXT_ARG();
			addattr_l(&req->n, sizeof(*req), IFLA_IFALIAS,
				  *argv, strlen(*argv));
			argc--; argv++;
			break;
		} else if (strcmp(*argv, "group") == 0) {
			NEXT_ARG();
			if (*group != -1)
				duparg("group", *argv);
			if (rtnl_group_a2n(group, *argv))
				invarg_1_to_2(*argv, "group");
		} else if (strcmp(*argv, "mode") == 0) {
			int mode;
			NEXT_ARG();
			mode = get_link_mode(*argv);
			if (mode < 0)
				invarg_1_to_2(*argv, "mode");
			addattr8(&req->n, sizeof(*req), IFLA_LINKMODE, mode);
		} else if (strcmp(*argv, "state") == 0) {
			int state;
			NEXT_ARG();
			state = get_operstate(*argv);
			if (state < 0)
				invarg_1_to_2(*argv, "state");
			addattr8(&req->n, sizeof(*req), IFLA_OPERSTATE, state);
		} else if (matches(*argv, "numtxqueues") == 0) {
			NEXT_ARG();
			if (numtxqueues != -1)
				duparg("numtxqueues", *argv);
			if (get_integer(&numtxqueues, *argv, 0))
				invarg_1_to_2(*argv, "numtxqueues");
			addattr_l(&req->n, sizeof(*req), IFLA_NUM_TX_QUEUES,
				  &numtxqueues, 4);
		} else if (matches(*argv, "numrxqueues") == 0) {
			NEXT_ARG();
			if (numrxqueues != -1)
				duparg("numrxqueues", *argv);
			if (get_integer(&numrxqueues, *argv, 0))
				invarg_1_to_2(*argv, "numrxqueues");
			addattr_l(&req->n, sizeof(*req), IFLA_NUM_RX_QUEUES,
				  &numrxqueues, 4);
		}
#endif

		argv++;
	}

	if (!dev) {
		bb_error_msg_and_die(bb_msg_requires_arg, "\"dev\"");
	}

	if (newaddr || newbrd) {
		halen = get_address(dev, &htype);
		if (newaddr) {
			parse_address(dev, htype, halen, newaddr, &ifr0);
			set_address(&ifr0, 0);
		}
		if (newbrd) {
			parse_address(dev, htype, halen, newbrd, &ifr1);
			set_address(&ifr1, 1);
		}
	}

	if (newname && strcmp(dev, newname)) {
		do_changename(dev, newname);
		dev = newname;
	}
	if (qlen != -1) {
		set_qlen(dev, qlen);
	}
	if (mtu != -1) {
		set_mtu(dev, mtu);
	}
	if (master != -1) {
		set_master(dev, master);
	}
	if (netns != -1) {
		set_netns(dev, netns);
	}
	if (mask)
		do_chflags(dev, flags, mask);
#if ENABLE_FEATURE_IP_LINK_CAN
	if (type)
		set_type(type, dev, argv);
#endif
	return 0;
}

static int ipaddr_list_link(char **argv)
{
	preferred_family = AF_PACKET;
	return ipaddr_list_or_flush(argv, 0);
}

static void vlan_parse_opt(char **argv, struct nlmsghdr *n, unsigned int size)
{
	static const char keywords[] ALIGN1 =
		"id\0"
		"protocol\0"
		"reorder_hdr\0"
		"gvrp\0"
		"mvrp\0"
		"loose_binding\0"
	;
	static const char protocols[] ALIGN1 =
		"802.1q\0"
		"802.1ad\0"
	;
	enum {
		ARG_id = 0,
		ARG_protocol,
		ARG_reorder_hdr,
		ARG_gvrp,
		ARG_mvrp,
		ARG_loose_binding,
	};
	enum {
		PROTO_8021Q = 0,
		PROTO_8021AD,
	};
	int arg;
	uint16_t id, proto;
	struct ifla_vlan_flags flags = {};

	while (*argv) {
		arg = index_in_substrings(keywords, *argv);
		if (arg < 0)
			invarg_1_to_2(*argv, "type vlan");

		NEXT_ARG();
		if (arg == ARG_id) {
			id = get_u16(*argv, "id");
			addattr_l(n, size, IFLA_VLAN_ID, &id, sizeof(id));
		} else if (arg == ARG_protocol) {
			arg = index_in_substrings(protocols, str_tolower(*argv));
			if (arg == PROTO_8021Q)
				proto = htons(ETH_P_8021Q);
			else if (arg == PROTO_8021AD)
				proto = htons(ETH_P_8021AD);
			else
				bb_error_msg_and_die("unknown VLAN encapsulation protocol '%s'",
								     *argv);
			addattr_l(n, size, IFLA_VLAN_PROTOCOL, &proto, sizeof(proto));
		} else {
			int param = index_in_strings(str_on_off, *argv);
			if (param < 0)
				die_must_be_on_off(nth_string(keywords, arg));

			if (arg == ARG_reorder_hdr) {
				flags.mask |= VLAN_FLAG_REORDER_HDR;
				flags.flags &= ~VLAN_FLAG_REORDER_HDR;
				if (param == PARM_on)
					flags.flags |= VLAN_FLAG_REORDER_HDR;
			} else if (arg == ARG_gvrp) {
				flags.mask |= VLAN_FLAG_GVRP;
				flags.flags &= ~VLAN_FLAG_GVRP;
				if (param == PARM_on)
					flags.flags |= VLAN_FLAG_GVRP;
			} else if (arg == ARG_mvrp) {
				flags.mask |= VLAN_FLAG_MVRP;
				flags.flags &= ~VLAN_FLAG_MVRP;
				if (param == PARM_on)
					flags.flags |= VLAN_FLAG_MVRP;
			} else { /*if (arg == ARG_loose_binding) */
				flags.mask |= VLAN_FLAG_LOOSE_BINDING;
				flags.flags &= ~VLAN_FLAG_LOOSE_BINDING;
				if (param == PARM_on)
					flags.flags |= VLAN_FLAG_LOOSE_BINDING;
			}
		}
		argv++;
	}

	if (flags.mask)
		addattr_l(n, size, IFLA_VLAN_FLAGS, &flags, sizeof(flags));
}

static void vrf_parse_opt(char **argv, struct nlmsghdr *n, unsigned int size)
{
/* IFLA_VRF_TABLE is an enum, not a define -
 * can't test "defined(IFLA_VRF_TABLE)".
 */
#if !defined(IFLA_VRF_MAX)
# define IFLA_VRF_TABLE 1
#endif
	uint32_t table;

	if (strcmp(*argv, "table") != 0)
		invarg_1_to_2(*argv, "type vrf");

	NEXT_ARG();
	table = get_u32(*argv, "table");
	addattr_l(n, size, IFLA_VRF_TABLE, &table, sizeof(table));
}

/* Return value becomes exitcode. It's okay to not return at all */
static int do_add_or_delete(char **argv, const unsigned rtm)
{
	static const char keywords[] ALIGN1 =
		"link\0""name\0""type\0""dev\0""address\0";
	enum {
		ARG_link,
		ARG_name,
		ARG_type,
		ARG_dev,
		ARG_address,
	};
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr  n;
		struct ifinfomsg i;
		char             buf[1024];
	} req;
	smalluint arg;
	char *name_str = NULL;
	char *link_str = NULL;
	char *type_str = NULL;
	char *dev_str = NULL;
	char *address_str = NULL;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = rtm;
	req.i.ifi_family = preferred_family;
	if (rtm == RTM_NEWLINK)
		req.n.nlmsg_flags |= NLM_F_CREATE|NLM_F_EXCL;

	/* NB: update iplink_full_usage if you extend this code */

	while (*argv) {
		arg = index_in_substrings(keywords, *argv);
		if (arg == ARG_type) {
			NEXT_ARG();
			type_str = *argv++;
			dbg("type_str:'%s'", type_str);
			break;
		}
		if (arg == ARG_link) {
			NEXT_ARG();
			link_str = *argv;
			dbg("link_str:'%s'", link_str);
		} else if (arg == ARG_name) {
			NEXT_ARG();
			name_str = *argv;
			dbg("name_str:'%s'", name_str);
		} else if (arg == ARG_address) {
			NEXT_ARG();
			address_str = *argv;
			dbg("address_str:'%s'", address_str);
		} else {
			if (arg == ARG_dev) {
				if (dev_str)
					duparg(*argv, "dev");
				NEXT_ARG();
			}
			dev_str = *argv;
			dbg("dev_str:'%s'", dev_str);
		}
		argv++;
	}
	xrtnl_open(&rth);
	ll_init_map(&rth);
	if (type_str) {
		struct rtattr *linkinfo = NLMSG_TAIL(&req.n);

		addattr_l(&req.n, sizeof(req), IFLA_LINKINFO, NULL, 0);
		addattr_l(&req.n, sizeof(req), IFLA_INFO_KIND, type_str,
				strlen(type_str));

		if (*argv) {
			struct rtattr *data = NLMSG_TAIL(&req.n);
			addattr_l(&req.n, sizeof(req), IFLA_INFO_DATA, NULL, 0);

			if (strcmp(type_str, "vlan") == 0)
				vlan_parse_opt(argv, &req.n, sizeof(req));
			else if (strcmp(type_str, "vrf") == 0)
				vrf_parse_opt(argv, &req.n, sizeof(req));

			data->rta_len = (void *)NLMSG_TAIL(&req.n) - (void *)data;
		}

		linkinfo->rta_len = (void *)NLMSG_TAIL(&req.n) - (void *)linkinfo;
	}
	/* Allow "ip link add dev" and "ip link add name" */
	if (!name_str)
		name_str = dev_str;
	else if (!dev_str)
		dev_str = name_str;
	/* else if (!strcmp(name_str, dev_str))
		name_str = dev_str; */

	if (rtm != RTM_NEWLINK) {
		if (!dev_str)
			return 1; /* Need a device to delete */
		req.i.ifi_index = xll_name_to_index(dev_str);
	} else {
		if (link_str) {
			int idx = xll_name_to_index(link_str);
			addattr_l(&req.n, sizeof(req), IFLA_LINK, &idx, 4);
		}
		if (address_str) {
			unsigned char abuf[32];
			int len = ll_addr_a2n(abuf, sizeof(abuf), address_str);
			dbg("address len:%d", len);
			if (len < 0)
				return -1;
			addattr_l(&req.n, sizeof(req), IFLA_ADDRESS, abuf, len);
		}
	}
	if (name_str) {
		const size_t name_len = strlen(name_str) + 1;
		if (name_len < 2 || name_len > IFNAMSIZ)
			invarg_1_to_2(name_str, "name");
		addattr_l(&req.n, sizeof(req), IFLA_IFNAME, name_str, name_len);
	}
	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		return 2;
	return 0;
}

/* Return value becomes exitcode. It's okay to not return at all */
int FAST_FUNC do_iplink(char **argv)
{
	static const char keywords[] ALIGN1 =
		"add\0""delete\0""set\0""show\0""lst\0""list\0";

	xfunc_error_retval = 2; //TODO: move up to "ip"? Is it the common rule for all "ip" tools?
	if (*argv) {
		int key = index_in_substrings(keywords, *argv);
		if (key < 0) /* invalid argument */
			invarg_1_to_2(*argv, applet_name);
		argv++;
		if (key <= 1) /* add/delete */
			return do_add_or_delete(argv, key ? RTM_DELLINK : RTM_NEWLINK);
		if (key == 2) /* set */
			return do_set(argv);
	}
	/* show, lst, list */
	return ipaddr_list_link(argv);
}
