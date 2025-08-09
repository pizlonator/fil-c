/* ip.c - Show / manipulate routing, devices, policy routing and tunnels.
 *
 * Copyright 2014 Sameer Prakash Pradhan <sameer.p.pradhan@gmail.com>
 * Copyright 2014 Ranjan Kumar <ranjankumar.bth@gmail.com>
 * Copyright 2014 Rajni Kant <rajnikant12345@gmail.com>
 * Copyright 2014 Bilal Qureshi <bilal.jmi@gmail.com>
 *
 * No Standard.
 *
USE_IP(NEWTOY(ip, NULL, TOYFLAG_SBIN))
USE_IP(OLDTOY(ipaddr, ip, TOYFLAG_SBIN))
USE_IP(OLDTOY(iplink, ip, TOYFLAG_SBIN))
USE_IP(OLDTOY(iproute, ip, TOYFLAG_SBIN))
USE_IP(OLDTOY(iprule, ip, TOYFLAG_SBIN))
USE_IP(OLDTOY(iptunnel, ip, TOYFLAG_SBIN))

config IP
  bool "ip"
  default n
  help
    usage: ip [ OPTIONS ] OBJECT { COMMAND }

    Show / manipulate routing, devices, policy routing and tunnels.

    where OBJECT := {address | link | route | rule | tunnel}
    OPTIONS := { -f[amily] { inet | inet6 | link } | -o[neline] }
*/
#define FOR_ip
#include "toys.h"
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_ether.h>
#include <linux/if_addr.h>
#include <net/if_arp.h>
#include <ifaddrs.h>
#include <fnmatch.h>
#include <linux/ip.h> // Centos 7.2 EOL June 30 2024
#include <linux/if_tunnel.h>

#ifndef IP_DF
#define IP_DF 0x4000  /* don't fragment flag. */
#endif

GLOBALS(
  char stats, singleline, flush, *filter_dev, gbuf[8192];
  int sockfd, connected, from_ok, route_cmd;
  int8_t addressfamily, is_addr;
)

struct arglist {
  char *name;
  int idx;
};

static struct
{
  int ifindex, scope, scopemask, up, to;
  char *label, *addr;
} addrinfo;

struct linkdata  {
  struct linkdata *next, *prev;
  int flags, iface_idx, mtu, txqueuelen, parent,iface_type;
  char qdiscpline[IFNAMSIZ+1], state[IFNAMSIZ+1], type[IFNAMSIZ+1],
       iface[IFNAMSIZ+1], laddr[64], bcast[64];
  struct  rtnl_link_stats rt_stat;
}*linfo;

typedef int (*cmdobj)(char **argv);

#define MESG_LEN 8192

// For "/etc/iproute2/RPDB_tables"
enum {
  RPDB_rtdsfield = 1,
  RPDB_rtprotos = 2,
  RPDB_rtrealms = 3,
  RPDB_rtscopes = 4,
  RPDB_rttables = 5
};

#define RPDB_ENTRIES 256
static int8_t rttable_init;
static int8_t rtprotos_init;
static int8_t rtdsfield_init;
static int8_t rtscope_init;
static int8_t rtrealms_init;

static struct arglist *rt_dsfield[RPDB_ENTRIES];
static struct arglist *rt_protos[RPDB_ENTRIES];
static struct arglist *rt_tables[RPDB_ENTRIES];
static struct arglist *rt_realms[RPDB_ENTRIES];
static struct arglist *rt_scope[RPDB_ENTRIES];

static struct arglist rtmtypes[] = { {"none", RTN_UNSPEC},
  {"unicast", RTN_UNICAST}, {"local", RTN_LOCAL},
  {"broadcast", RTN_BROADCAST}, {"anycast", RTN_ANYCAST},
  {"multicast", RTN_MULTICAST}, {"blackhole", RTN_BLACKHOLE},
  {"unreachable", RTN_UNREACHABLE}, {"prohibit", RTN_PROHIBIT},
  {"throw", RTN_THROW}, {"nat", RTN_NAT},
  {"xresolve", RTN_XRESOLVE}, {NULL, -1}
};

static int filter_nlmesg(int (*fun)(struct nlmsghdr *mhdr, char **), char **);
static int  ipaddr_print(struct linkdata *, int flg);

// extended route attribute metrics.
static const char *mx_names[RTAX_MAX + 1] = {
    [RTAX_MTU] = "mtu",
    [RTAX_WINDOW] = "window",
    [RTAX_RTT] = "rtt",
    [RTAX_RTTVAR] = "rttvar",
    [RTAX_SSTHRESH] = "ssthresh",
    [RTAX_CWND] = "cwnd",
    [RTAX_ADVMSS] = "advmss",
    [RTAX_REORDERING] = "reordering",
    [RTAX_HOPLIMIT] = "hoplimit",
    [RTAX_INITCWND] = "initcwnd",
    [RTAX_FEATURES] = "features",
    [RTAX_RTO_MIN] = "rto_min",
    [RTAX_INITRWND] = "initrwnd",
    [RTAX_QUICKACK] = "quickack",
    [RTAX_CC_ALGO] = "congctl"};

// ===========================================================================
// Common Code for IP Options (like: addr, link, route etc.)
// ===========================================================================
static int substring_to_idx(char *str, struct arglist *list)
{
  struct arglist *alist;
  int len;

  if (!str) return -1;
  len = strlen(str);

  for (alist = list; alist->name; alist++)
    if (!memcmp(str, alist->name, len)) return alist->idx;
  return -1;
}

static int string_to_idx(char *str, struct arglist *list)
{
  struct arglist *alist;

  if (!str) return -1;
  for (alist = list; alist->name; alist++)
    if (!strcmp(str, alist->name)) return alist->idx;
  return -1;
}

static char *idx_to_string(int idx, struct arglist *list)
{
  struct arglist *alist;

  if (idx < 0) return NULL;
  for (alist = list; alist->name; alist++)
    if (idx == alist->idx) return alist->name;
  return NULL;
}

static void send_nlmesg(int type, int flags, int family,
    void *buf, int blen)
{
  struct {
    struct nlmsghdr nlh;
    struct rtgenmsg g;
  } req;

  if (!buf) {
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = sizeof(req);
    req.nlh.nlmsg_type = type;
    req.nlh.nlmsg_flags = flags;
    req.g.rtgen_family = family;
    buf = &req;
    blen = sizeof(req);
  }
  if (send(TT.sockfd , (void*)buf, blen, 0) < 0)
    perror_exit("Unable to send data on socket.");
}

// Parse /etc/iproute2/RPDB_tables and prepare list.
static void parseRPDB(char *fname, struct arglist **list, int32_t size)
{
  FILE *fp = fopen(fname, "r");
  char *line = 0;
  size_t l = 0;
  ssize_t len;

  if (!fp) return;
  while ((len = getline(&line, &l, fp)) > 0) {
    char *ptr = line;
    int32_t idx;

    while (*ptr == ' ' || *ptr == '\t') ptr++;
    if (*ptr == 0 || *ptr == '#' || *ptr == '\n') continue;
    if ((sscanf(ptr, "0x%x %s\n", &idx, toybuf) != 2) &&
        (sscanf(ptr, "0x%x %s #", &idx, toybuf) != 2) &&
        (sscanf(ptr, "%d %s\n", &idx, toybuf) != 2) &&
        (sscanf(ptr, "%d %s #", &idx, toybuf) != 2)) {
      error_msg("corrupt %s", fname);
      break;
    }
    if (idx >= 0 && idx < size) {
      int index = idx & (size-1);
      if (list[index]) free(list[index]->name);
      else list[index] = xzalloc(sizeof(struct arglist));
      list[index]->idx = idx;
      list[index]->name = xstrdup(toybuf);
    }
  }
  free(line);
  fclose(fp);
}

static void free_alist(struct arglist **list)
{
  int i;
  for (i = 0;i<RPDB_ENTRIES;i++) {
    if (list[i]) {
      free(list[i]->name);
      free(list[i]);
    }
  }
}

static void init_arglist(struct arglist **list,int value, char* name)
{
  if (!list[value]) list[value] =  xzalloc(sizeof(struct arglist));
  list[value]->idx = value;
  list[value]->name = xstrdup(name);
}

static struct arglist **getlist(u_int8_t whichDB)
{
  struct arglist **alist;

  switch (whichDB) {
    case RPDB_rtdsfield:
      alist = rt_dsfield;
      if (!rtdsfield_init) {
        rtdsfield_init = 1;
        parseRPDB("/etc/iproute2/rt_dsfield", alist, ARRAY_LEN(rt_dsfield));
      }
      break;
    case RPDB_rtprotos:
      alist = rt_protos;
      if (!rttable_init) {
        rtprotos_init = 1;
        init_arglist(rt_protos,0,"none");
        init_arglist(rt_protos,1,"redirect");
        init_arglist(rt_protos,2,"kernel");
        init_arglist(rt_protos,3,"boot");
        init_arglist(rt_protos,4,"static");
        init_arglist(rt_protos,8,"gated");
        init_arglist(rt_protos,9,"ra");
        init_arglist(rt_protos,10,"mrt");
        init_arglist(rt_protos,11,"zebra");
        init_arglist(rt_protos,12,"bird");
        parseRPDB("/etc/iproute2/rt_protos", alist, ARRAY_LEN(rt_protos));
      }
      break;
    case RPDB_rtrealms:
      alist = rt_realms;
      if (!rtrealms_init) {
        rtrealms_init = 1;
        init_arglist(rt_realms,0,"unspec");
        parseRPDB("/etc/iproute2/rt_realms", alist, ARRAY_LEN(rt_realms));
      }
      break;
    case RPDB_rtscopes:
      alist = rt_scope;
      if (!rtscope_init) {
        rtscope_init = 1;
        init_arglist(rt_scope,0,"global");
        init_arglist(rt_scope,200,"site");
        init_arglist(rt_scope,253,"link");
        init_arglist(rt_scope,254,"host");
        init_arglist(rt_scope,255,"nowhere");
        parseRPDB("/etc/iproute2/rt_scopes", alist, ARRAY_LEN(rt_scope));
      }
      break;
    case RPDB_rttables:
      alist = rt_tables;
      if (!rttable_init) {
        rttable_init = 1;
        init_arglist(rt_tables,RT_TABLE_DEFAULT,"default");
        init_arglist(rt_tables,RT_TABLE_MAIN,"main");
        init_arglist(rt_tables,RT_TABLE_LOCAL,"local");
        parseRPDB("/etc/iproute2/rt_tables", alist, ARRAY_LEN(rt_tables));
      }
      break;
    default:
      error_exit("wrong database");
      break; // Unreachable code.
  }
  return alist;
}

/*
 * Parse RPBD tables (if not parsed already).
 * return RPDB table name as per idx.
 */
static char *namefromRPDB(int idx, u_int8_t whichDB)
{
  struct arglist **alist;

  if (idx < 0 || idx >= RPDB_ENTRIES) {
    snprintf(toybuf, RPDB_ENTRIES, "%u", idx);
    return toybuf;
  }

  alist = getlist(whichDB);

  if (alist[idx] && alist[idx]->name) return alist[idx]->name;

  if (whichDB == RPDB_rtdsfield) snprintf(toybuf, RPDB_ENTRIES, "0x%02x", idx);
  else snprintf(toybuf, RPDB_ENTRIES, "%u", idx);

  return toybuf;
}

static int idxfromRPDB(char *name, u_int8_t whichDB)
{
  struct arglist **alist;
  long i = 0;
  char *ptr = NULL;

  for (alist = getlist(whichDB); i < RPDB_ENTRIES; i++) {
    if (!alist[i] || !alist[i]->name) continue;
    if (!strcmp(alist[i]->name, name)) return i;
  }
  i = strtol(name, &ptr, 0);
  if (errno || (ptr && *ptr) || i < 0 || i > 255)
    return -1;
  return i;
}

static char *rtmtype_idx2str(u_int8_t idx)
{
  char *name = idx_to_string(idx, rtmtypes);

  if (!name) snprintf(toybuf, RPDB_ENTRIES, "%u", idx);
  else snprintf(toybuf, sizeof(toybuf), "%s", name);
  return toybuf;
}

static int rtmtype_str2idx(char *name)
{
  int idx = string_to_idx(name, rtmtypes);

  if (idx < 0) return atolx_range(name, 0, 255);
  return idx;
}

/*
 * Used to get the prefix value in binary form.
 * For IPv4: non-standard parsing used; as 10.10 will be treated as 10.10.0.0
 * unlike inet_aton which is 10.0.0.10
 */
static int get_prefix(uint32_t *addr, uint8_t *af, char *name, int family)
{
  if (family == AF_PACKET) error_exit("'%s' may be inet prefix", name);
  if (!memcmp(name, "default", strlen(name))
      || !memcmp(name, "all", strlen(name))
      || !memcmp(name, "any", strlen(name))) {
    *af = family;
    return 0;
  }
  if (strchr(name, ':')) {
    *af = AF_INET6;
    if (family != AF_UNSPEC && family != AF_INET6) return 1;
    if (inet_pton(AF_INET6, name, (void *)addr) != 1)
      return 1;
  } else { // for IPv4.
    char *ptr = name;
    uint8_t count = 0;

    *af = AF_INET;
    if (family != AF_UNSPEC && family != AF_INET) return 1;
    while (*ptr) {
      int val, len = 0;

      if (*ptr == '.') ptr++;
      sscanf(ptr, "%d%n", &val, &len);
      if (!len || len > 3 || val < 0 || val > 255 || count > 3) return 1;
      ptr += len;
      ((uint8_t*)addr)[count++] = val;
    }
  }
  return 0;
}

/*
 * Used to calculate netmask, which can be in the form of
 * either 255.255.255.0 or 24 or default or any or all strings.
 */
static int get_nmask_prefix(uint32_t *netmask, uint8_t af,
    char *name, uint8_t family)
{
  char *ptr;
  uint32_t naddr[4] = {0,};
  uint64_t plen;
  uint8_t naf = AF_UNSPEC;

  *netmask = (af == AF_INET6) ? 128 : 32; // set default netmask
  plen = strtoul(name, &ptr, 0);

  if (!ptr || ptr == name || *ptr || !plen || plen > *netmask) {
    if (get_prefix(naddr, &naf, name, family)) return -1;
    if (naf == AF_INET) {
      uint32_t mask = htonl(*naddr), host = ~mask;
      if (host & (host + 1)) return -1;
      for (plen = 0; mask; mask <<= 1) ++plen;
      if (plen > 32) return -1;
    }
  }
  *netmask = plen;
  return 0;
}

/*
 * Parse prefix, which will be in form of
 * either default or default/default or default/24 or default/255.255.255.0
 * or 10.20.30.40 or 10.20.30.40/default or 10.20.30.40/24
 * or 10.20.30.40/255.255.255.0
 */
static void parse_prefix(uint32_t *addr, uint32_t *netmask, uint8_t *len,
    char *name, int family)
{
  uint8_t af = AF_UNSPEC;
  char *slash = strchr(name, '/');

  if (slash) *slash = 0;
  if (get_prefix(addr, &af, name, family)) error_exit("Invalid prefix");

  if (slash) { // grab netmask.
    if (get_nmask_prefix(netmask, af, slash+1, family))
      error_exit("Invalid prefix");
    *slash ='/';
  }
  else if (af == AF_INET && *addr) *netmask = 32;
  else if (af == AF_INET6 && (*addr || *(addr+3))) *netmask = 128;

  if (!*addr && !slash && !af) *len = 0;
  else *len = (af == AF_INET6) ? 16 : 4;
}

/*
 * Add a route attribute to a buffer; this is primarily used for extended
 * attributes which get collected in a separate buffer from the normal route
 * attributes and later get added to the main rtm message.
 */
static void add_varlen_rtattr_to_buffer(struct rtattr *rta, int maxlen,
                                        int type, void *data, int alen) {
  struct rtattr *subrta;
  int len = RTA_LENGTH(alen);
  if (RTA_ALIGN(rta->rta_len) + RTA_ALIGN(len) > maxlen) {
      error_exit("RTA exceeds max length %d", maxlen);
  }
  subrta = (struct rtattr*)(((char*)rta) + RTA_ALIGN(rta->rta_len));
  subrta->rta_type = type;
  subrta->rta_len = len;
  if (alen) {
    memcpy(RTA_DATA(subrta), data, alen);
  }
  rta->rta_len = NLMSG_ALIGN(rta->rta_len) + RTA_ALIGN(len);
}

static void add_uint32_rtattr_to_buffer(struct rtattr *rta, int maxlen,
                                        int type, uint32_t attr) {
  add_varlen_rtattr_to_buffer(rta, maxlen, type, (char*)&attr, sizeof(attr));
}

/*
 * Add a route attribute to a RTM message.
 */
static void add_string_to_rtattr(struct nlmsghdr *n, int maxlen,
    int type, void *data, int alen)
{
  int len = RTA_LENGTH(alen);
  struct rtattr *rta;

  if ((int)(NLMSG_ALIGN(n->nlmsg_len) + len) > maxlen) return;
  rta = (struct rtattr*)(((char*)n) + NLMSG_ALIGN(n->nlmsg_len));
  rta->rta_type = type;
  rta->rta_len = len;
  memcpy(RTA_DATA(rta), data, alen);
  n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;
}



// ===========================================================================
// Code for ip link.
// ===========================================================================
#ifndef NLMSG_TAIL
#define NLMSG_TAIL(nmsg) \
  ((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))
#endif

static uint32_t get_ifaceindex(char *name, int ext)
{
  struct if_nameindex *if_ni, *i;
  int index = -1;

  if_ni = if_nameindex();
  if (!if_ni) perror_exit("if_nameindex");

  for (i = if_ni; i->if_index && i->if_name; i++)
    if (!strcmp(name, i->if_name)) {
      index = i->if_index;
      break;
    }
  if_freenameindex(if_ni);
  if (index == -1 && ext) perror_exit("can't find device '%s'", name);
  return index;
}

static void fill_hwaddr(char *arg, int len, unsigned char *address)
{
  int count = 0, val, length;

  while (count < len) {
    val = length = 0;
    if (!arg) error_exit("bad hw-addr '%s'", "");
    if (*arg == ':') arg++, count++;
    sscanf(arg, "%2x%n", &val, &length);
    if (!length || length > 2)
      error_exit("bad hw-addr '%s'", arg);
    arg += length;
    count += length;
    *address++ = val;
  }
}

// Multimach = 1, single match = 0
static char *get_flag_string(struct arglist *aflags, int flags, int ismulti)
{
  struct arglist *p = aflags;
  char *out = NULL, *tmp = NULL;

  for (; p->name; p++) {
    int test = (ismulti ? p->idx & flags : 0) || p->idx == flags;
    if (test) { // flags can be zero
      tmp = out ? xmprintf("%s,%s", out, p->name) : xmprintf("%s", p->name);
      if (out) free(out);
      out = tmp;
    }
  }
  return out;
}

static void vlan_parse_opt(char **argv, struct nlmsghdr *n, unsigned int size)
{
  struct arglist vlan_optlist[] = {{"id", 0}, {"protocol", 1},
    {"reorder_hdr", 2}, {"gvrp", 3}, {NULL,-1}};
  struct arglist vlan_protolist[] = {{"802.1q", 0}, {"802.1ad", 1}, {NULL,-1}};
  struct arglist on_off[] = { {"on", 0}, {"off", 1}, {NULL,-1}};
  int idx;
  struct ifla_vlan_flags flags;

  memset(&flags, 0, sizeof(flags));
  for (; *argv; argv++) {
    int param, proto;

    if ((idx = substring_to_idx(*argv++, vlan_optlist)) == -1) help_exit(0);
    switch (idx) {
      case 0: // ARG_id
        if (!*argv) help_exit(0);
        param = atolx(*argv);
        add_string_to_rtattr(n, size, IFLA_VLAN_ID, &param, sizeof(param));
        break;
      case 1: // ARG_protocol
        if (!*argv) error_exit("Invalid vlan id.");
        if ((idx = substring_to_idx(*argv, vlan_protolist)) == -1) help_exit(0);
        if (!idx) proto = ETH_P_8021Q; // PROTO_8021Q - 0
        else if (idx == 1) proto = 0x88A8; // ETH Protocol - 8021AD
        // IFLA VLAN PROTOCOL - 5
        add_string_to_rtattr(n, size, 5, &proto, sizeof(proto));
        break;
      case 2: // ARG_reorder_hdr
      case 3: // ARG_gvrp
        if ((param = substring_to_idx(*argv, on_off)) == -1) help_exit(0);

        flags.mask |= (idx -1); // VLAN FLAG REORDER Header
        flags.flags &= ~(idx -1); // VLAN FLAG REORDER Header
        if (!param) flags.flags |= (idx -1); // VLAN FLAG REORDER Header
        break;
    }
  }
  if (flags.mask)
    add_string_to_rtattr(n, size, IFLA_VLAN_FLAGS, &flags, sizeof(flags));
}

static int linkupdate(char **argv)
{
  struct {
    struct nlmsghdr mhdr;
    struct ifinfomsg info;
    char buf[1024];
  } request;
  char *name, *dev, *type, *link, *addr;
  struct rtattr *attr = NULL;
  int len = 0, add = (*argv[-1] == 'a') ? 1 : 0;

  name = dev = type = link = addr = NULL;
  for (; *argv; argv++) {
    struct arglist objectlist[] = { {"type", 0}, {"name", 1}, {"link", 2},
      {"address", 3}, {NULL,-1}};
    uint8_t idx = substring_to_idx(*argv, objectlist);

    if (!idx) {
      type = *++argv;
      break;
    }
    else if (idx == 1) dev = name = *++argv;
    else if (idx == 2) link = *++argv;
    else if (idx == 3) addr = *++argv;
    else if (!dev) name = dev = *argv;
  }

  if (!name && !add)
    error_exit("Not enough information: \"dev\" argument is required.\n");
  else if (!type  && add)
    error_exit("Not enough information: \"type\" argument is required.\n");

  memset(&request, 0, sizeof(request));
  request.mhdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
  request.mhdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
  if (add) {
    request.mhdr.nlmsg_flags |= NLM_F_CREATE|NLM_F_EXCL;
    request.mhdr.nlmsg_type = RTM_NEWLINK;
  } else {
    request.mhdr.nlmsg_type = RTM_DELLINK;
    request.info.ifi_index = get_ifaceindex(name, 1);
  }
  request.info.ifi_family = AF_UNSPEC;
  attr = NLMSG_TAIL(&request.mhdr);
  if (type) {
    add_string_to_rtattr(&request.mhdr, sizeof(request),
        IFLA_LINKINFO, NULL, 0);
    add_string_to_rtattr(&request.mhdr, sizeof(request),
        IFLA_INFO_KIND, type, strlen(type));
    if (!strcmp(type, "vlan")) {
      struct rtattr *data = NLMSG_TAIL(&request.mhdr);
      add_string_to_rtattr(&request.mhdr, sizeof(request),
          IFLA_INFO_DATA, NULL, 0);
      vlan_parse_opt(++argv, &request.mhdr, sizeof(request));
      data->rta_len = (void *)NLMSG_TAIL(&request.mhdr) - (void *)data;
    }
    attr->rta_len = (void *)NLMSG_TAIL(&request.mhdr) - (void *)attr;
  }

  if (link) {
    uint32_t idx = get_ifaceindex(link, 1);
    add_string_to_rtattr(&request.mhdr, sizeof(request),
        IFLA_LINK, &idx, sizeof(uint32_t));
  }
  if (addr) {
    char abuf[IF_NAMESIZE] = {0,};

    fill_hwaddr(addr, IF_NAMESIZE, (unsigned char *)abuf);
    add_string_to_rtattr(&request.mhdr, sizeof(request),
        IFLA_ADDRESS, abuf, strlen(abuf));
  }
  if (!name) {
    snprintf(toybuf, IFNAMSIZ, "%s%d", type, 0);
    for (len = 1; ; len++) {
      if (!get_ifaceindex(toybuf, 0)) break;
      snprintf(toybuf, IFNAMSIZ, "%s%d", type, len);
    }
    name = toybuf;
  }
  len = strlen(name) + 1;
  if (len < 2 || len > IFNAMSIZ) error_exit("Invalid device name.");
  add_string_to_rtattr(&request.mhdr, sizeof(request), IFLA_IFNAME, name, len);

  send_nlmesg(0, 0, 0, (void *)&request, request.mhdr.nlmsg_len);
  return (filter_nlmesg(NULL,NULL));
}

static int link_set(char **argv)
{
  struct arglist cmd_objectlist[] = {{"up", 0}, {"down", 1}, {"arp", 2},
    {"multicast", 3}, {"dynamic", 4}, {"name", 5}, {"txqueuelen", 6},
    {"mtu", 7},{"address", 8}, {"broadcast", 9}, {"dev", 10}, {NULL,-1}};
  int case_flags[] = {IFF_NOARP,IFF_MULTICAST,IFF_DYNAMIC};
  struct ifreq req;
  int idx, flags = 0, masks = 0xffff, fd;
  char *dev = NULL, *newdev = NULL, *newaddr = NULL, *newbrd = NULL;
  int mtu = -1, txqlen = -1;

  int i = 0;
  while (*argv) {
    switch(idx = substring_to_idx(*argv, cmd_objectlist)) {
      case 0:
        flags |= IFF_UP; break;
      case 1:
        masks &= ~IFF_UP; break;
      case 2:
      case 3:
      case 4:
        ++argv;
        if (!*argv) help_exit(0);
        else if (!strcmp(*argv, "on")) {
          if (idx == 2) {
            masks &= ~case_flags[idx-2];
            flags &= ~case_flags[idx-2];
          } else flags |= case_flags[idx-2];
        } else if (!strcmp(*argv,"off")) {
          if (idx == 2) {
            masks |= case_flags[idx-2];
            flags |= case_flags[idx-2];
          } else masks &= ~case_flags[idx-2];
        } else help_exit(0);
        break;
      case 5:
        ++argv;
        if (!*argv) error_exit("Incomplete Command line");
        newdev = *argv;
        break;
      case 6:
        ++argv;
        if (!*argv) error_exit("Incomplete Command line");
        txqlen = atolx(*argv);
        break;
      case 7:
        ++argv;
        if (!*argv) error_exit("Incomplete Command line");
        mtu = atolx(*argv);
        break;
      case 8:
        ++argv;
        if (!*argv) error_exit("Incomplete Command line");
        newaddr = *argv;
        break;
      case 9:
        ++argv;
        if (!*argv) error_exit("Incomplete Command line");
        newbrd = *argv;
        break;
      case 10:
        ++argv;
        if (!*argv) error_exit("Incomplete Command line");

      default:
        if (dev)
          error_exit("Either \"dev\" is duplicate or %s is garbage",
              *argv);
        dev = *argv;
        break;
    }
    ++argv;
  }

  memset(&req, 0, sizeof(req));
  if (!dev) error_exit("\"dev\" missing");
  xstrncpy(req.ifr_name, dev, IF_NAMESIZE);
  fd = xsocket(AF_INET, SOCK_DGRAM, 0);

  if (newdev) {
    xstrncpy(req.ifr_ifru.ifru_newname, newdev, IF_NAMESIZE);
    xioctl(fd, SIOCSIFNAME, &req);
    xstrncpy(req.ifr_name, newdev, IF_NAMESIZE);
  }
  xioctl(fd, SIOCGIFINDEX, &req);

  if (txqlen != -1) {
    req.ifr_ifru.ifru_ivalue = txqlen;
    xioctl(fd, SIOCSIFTXQLEN, &req);
  }

  if (mtu != -1) {
    req.ifr_ifru.ifru_mtu = mtu;
    xioctl(fd, SIOCSIFMTU, &req);
  }

  if (newaddr) {
    xioctl(fd, SIOCGIFHWADDR, &req);
    fill_hwaddr(newaddr, IF_NAMESIZE,
        (unsigned char *)(req.ifr_hwaddr.sa_data));
    xioctl(fd, SIOCSIFHWADDR, &req);
  }

  if (newbrd) {
    xioctl(fd, SIOCGIFHWADDR, &req);
    fill_hwaddr(newbrd, IF_NAMESIZE,
        (unsigned char *)(req.ifr_hwaddr.sa_data));
    xioctl(fd, SIOCSIFHWBROADCAST, &req);
  }

  xioctl(fd, SIOCGIFFLAGS, &req);
  req.ifr_ifru.ifru_flags |= flags;
  req.ifr_ifru.ifru_flags &= masks;
  xioctl(fd, SIOCSIFFLAGS, &req);
  xclose(fd);
  return 0;
}

static void print_stats(struct  rtnl_link_stats *rtstat)
{
  char *line_feed = (!TT.singleline ? "\n    " : " ");

  if (TT.stats > 0) {
    xprintf("    RX: bytes  packets  errors  "
        "dropped  overrun  mcast%s%-10u %-8u %-7u %-8u %-8u %-8u\n",
        line_feed, rtstat->rx_bytes, rtstat->rx_packets, rtstat->rx_errors,
        rtstat->rx_dropped, rtstat->rx_over_errors, rtstat->multicast);
    if (TT.stats > 1) {
      xprintf("    RX: errors  length  crc  "
          "frame  fifo  missed%s%-10u %-8u %-7u %-8u %-8u %-8u\n",
          line_feed, rtstat->rx_errors, rtstat->rx_length_errors,
          rtstat->rx_crc_errors, rtstat->rx_frame_errors,
          rtstat->rx_fifo_errors, rtstat->rx_missed_errors);
    }
    xprintf("    TX: bytes  packets  errors  "
        "dropped  carrier  collsns%s%-10u %-8u %-7u %-8u %-8u %-8u\n",
        line_feed, rtstat->tx_bytes, rtstat->tx_packets, rtstat->tx_errors,
        rtstat->tx_dropped, rtstat->tx_carrier_errors, rtstat->collisions);
    if (TT.stats > 1) {
      xprintf("    TX: errors  aborted  fifo  window  "
          "heartbeat%s%-10u %-8u %-7u %-8u %-8u\n",
          line_feed, rtstat->tx_errors, rtstat->tx_aborted_errors,
          rtstat->tx_fifo_errors, rtstat->tx_window_errors,
          rtstat->tx_heartbeat_errors);
    }
  }
}

static int print_link_output(struct linkdata *link)
{
  char *line_feed = " ", *flags,*peer = "brd";
  struct arglist iface_flags[] = {{"",0},{"UP", IFF_UP},
    {"BROADCAST", IFF_BROADCAST}, {"DEBUG", IFF_DEBUG},
    {"LOOPBACK", IFF_LOOPBACK}, {"POINTOPOINT", IFF_POINTOPOINT},
    {"NOTRAILERS", IFF_NOTRAILERS}, {"RUNNING", IFF_RUNNING},
    {"NOARP", IFF_NOARP}, {"PROMISC",IFF_PROMISC},
    {"ALLMULTI", IFF_ALLMULTI}, {"MASTER", IFF_MASTER}, {"SLAVE", IFF_SLAVE},
    {"MULTICAST", IFF_MULTICAST}, {"PORTSEL", IFF_PORTSEL},
    {"AUTOMEDIA", IFF_AUTOMEDIA}, {"DYNAMIC", IFF_DYNAMIC},
    {"LOWER_UP", IFF_LOWER_UP}, {"DORMANT", IFF_DORMANT},
    {"ECHO", IFF_ECHO}, {NULL,-1}};

  if (link->parent != -1) {
    int fd = 0;
    struct ifreq req;

    memset(&req, 0, sizeof(req));
    if_indextoname( link->parent,req.ifr_ifrn.ifrn_name);
    fd = xsocket(AF_INET, SOCK_DGRAM, 0);
    if (ioctl(fd, SIOCGIFTXQLEN, &req)) perror("");
    else link->txqueuelen = req.ifr_ifru.ifru_ivalue;
    xclose(fd);
  }

  if (TT.is_addr && addrinfo.label && fnmatch(addrinfo.label, link->iface, 0))
    return 0;


  if (!(flags = get_flag_string(iface_flags, link->flags, 1)))
    error_exit("Invalid data.");
  if (!TT.singleline) line_feed="\n    ";
  if (link->parent != -1) {
    char iface[IF_NAMESIZE];

    if (!if_indextoname(link->parent, iface)) perror_exit(NULL);
    sprintf(toybuf,"%s@%s", link->iface, iface);
  }
  if (link->flags & IFF_POINTOPOINT) peer = "peer";
  if (TT.is_addr && TT.singleline && TT.addressfamily)
    xprintf("%d: %s", link->iface_idx,
        ((link->parent == -1) ? link->iface : toybuf));
  else xprintf("%d: %s: <%s> mtu %d qdisc %s state %s qlen %d",
      link->iface_idx, ((link->parent == -1) ? link->iface : toybuf), flags,
      link->mtu, link->qdiscpline, link->state, link->txqueuelen);

  if (!TT.addressfamily || TT.addressfamily == AF_PACKET)
    xprintf("%slink/%s %s %s %s",
        line_feed, link->type, link->laddr, peer ,link->bcast);

  xputc('\n');

  //user can specify stats flag two times
  //one for stats and other for erros e.g. -s and -s -s
  print_stats(&link->rt_stat);
  free(flags);

  return 0;
}

static void fill_address(void *p, char *ip)
{
  unsigned char *ptr = (unsigned char*)p;
  snprintf(ip, 64, " %02x:%02x:%02x:%02x:%02x:%02x",
      ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
}

static int get_link_info(struct nlmsghdr* h,struct linkdata* link,char **argv)
{
  struct ifinfomsg *iface = NLMSG_DATA(h);
  struct rtattr *attr = IFLA_RTA(iface);
  int len = h->nlmsg_len - NLMSG_LENGTH(sizeof(*iface));
  struct arglist hwtypes[]={{"generic",0},{"ether",ARPHRD_ETHER},
    {"loopback", ARPHRD_LOOPBACK},{"sit",ARPHRD_SIT},
#ifdef ARPHRD_INFINIBAND
    {"infiniband",ARPHRD_INFINIBAND},
#endif
#ifdef ARPHRD_IEEE802_TR
    {"ieee802",ARPHRD_IEEE802}, {"tr",ARPHRD_IEEE802_TR},
#else
    {"tr",ARPHRD_IEEE802},
#endif
#ifdef ARPHRD_IEEE80211
    {"ieee802.11",ARPHRD_IEEE80211},
#endif
#ifdef ARPHRD_IEEE1394
    {"ieee1394",ARPHRD_IEEE1394},
#endif
    {"irda",ARPHRD_IRDA},{"slip",ARPHRD_SLIP},{"cslip",ARPHRD_CSLIP},
    {"slip6",ARPHRD_SLIP6}, {"cslip6",ARPHRD_CSLIP6}, {"ppp",ARPHRD_PPP},
    {"ipip",ARPHRD_TUNNEL}, {"tunnel6",ARPHRD_TUNNEL6},
    {"gre",ARPHRD_IPGRE},
#ifdef ARPHRD_VOID
    {"void",ARPHRD_VOID},
#endif
    {NULL,-1}};
  char *lname = get_flag_string(hwtypes, iface->ifi_type, 0);

  link->next = link->prev = 0;
  link->iface_type = iface->ifi_type;
  if (!lname) error_exit("Invalid link.");
  xstrncpy(link->type, lname, IFNAMSIZ);
  free(lname);
  link->iface_idx = iface->ifi_index;
  link->flags = iface->ifi_flags;
  if (*argv && !strcasecmp("up",*argv) && !(link->flags & IFF_UP)) return 1;
  link->parent =  -1;
  for (; RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
    switch(attr->rta_type) {
      case IFLA_IFNAME:
        snprintf(link->iface, IFNAMSIZ, "%s",(char *) RTA_DATA(attr));
        break;
      case IFLA_ADDRESS:
        if ( iface->ifi_type== ARPHRD_TUNNEL ||
            iface->ifi_type == ARPHRD_SIT ||
            iface->ifi_type == ARPHRD_IPGRE)
          inet_ntop(AF_INET, RTA_DATA(attr), link->laddr, 64);
        else fill_address(RTA_DATA(attr), link->laddr);
        break;
      case IFLA_BROADCAST:
        if (iface->ifi_type== ARPHRD_TUNNEL ||
            iface->ifi_type == ARPHRD_SIT ||
            iface->ifi_type == ARPHRD_IPGRE)
          inet_ntop(AF_INET, RTA_DATA(attr), link->bcast, 64);
        else  fill_address(RTA_DATA(attr), link->bcast);
        break;
      case IFLA_MTU:
        link->mtu = *((int*)(RTA_DATA(attr)));
        break;
      case IFLA_QDISC:
        snprintf(link->qdiscpline, IFNAMSIZ, "%s", (char *) RTA_DATA(attr));
        break;
      case IFLA_STATS  :
        link->rt_stat = *((struct rtnl_link_stats*) RTA_DATA(attr));
        break;
      case IFLA_LINK:
        link->parent = *((int*)(RTA_DATA(attr)));
        break;
      case IFLA_TXQLEN:
        link->txqueuelen = *((int*)(RTA_DATA(attr)));
        break;
      case IFLA_OPERSTATE:
        {
          struct arglist flags[]={{"UNKNOWN", 0}, {"NOTPRESENT", 1},
            {"DOWN", 2}, {"LOWERLAYERDOWN", 3}, {"TESTING", 4},
            {"DORMANT", 5}, {"UP", 6}, {NULL, -1}};
          if (!(lname = get_flag_string(flags, *((int*)(RTA_DATA(attr))), 0)))
            error_exit("Invalid state.");
          xstrncpy(link->state, lname,IFNAMSIZ);
          free(lname);
        }
        break;
      default: break;
    }
  }
  return 0;
}

static int display_link_info(struct nlmsghdr *mhdr, char **argv)
{
  struct linkdata link;

  if (!get_link_info(mhdr, &link, argv)) {
    if (TT.is_addr) {
      struct linkdata *lnk = xzalloc(sizeof(struct linkdata));
      memcpy(lnk, &link, sizeof(struct linkdata));
      dlist_add_nomalloc((struct double_list **)&linfo,
          (struct double_list *)lnk);
    }
    else print_link_output(&link);
  }
  return 0;
}

static int link_show(char **argv)
{
  struct {
    struct nlmsghdr mhdr;
    struct ifinfomsg info;
  } request;
  uint32_t index = 0;

  if (*argv && strcasecmp("up",*argv)) index = get_ifaceindex(*argv, 1);
  memset(&request, 0, sizeof(request));
  request.mhdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
  request.mhdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
  if (!index) request.mhdr.nlmsg_flags |= NLM_F_ROOT|NLM_F_MATCH;
  else request.info.ifi_change =  0xffffffff; // used in single operation
  request.mhdr.nlmsg_type = RTM_GETLINK;
  request.info.ifi_index = index;
  request.info.ifi_family = AF_UNSPEC;
  send_nlmesg(0, 0, 0, (void*)&request, sizeof(request));
  return (filter_nlmesg(display_link_info, argv));
}

static int iplink(char **argv)
{
  int idx;
  cmdobj ipcmd, cmdobjlist[] = {linkupdate, link_set, link_show};
  struct arglist cmd_objectlist[] = {{"add", 0}, {"delete", 0},
    {"set", 1}, {"show", 2}, {"list", 2}, {"lst", 2}, {NULL,-1}};

  if (!*argv) idx = 2;
  else if ((idx = substring_to_idx(*argv++, cmd_objectlist)) == -1)
    help_exit(0);
  ipcmd = cmdobjlist[idx];
  return ipcmd(argv);
}

// ===========================================================================
// Code for ip addr.
// ===========================================================================

static int print_addrinfo(struct nlmsghdr *h, int flag_l)
{
  struct rtattr *rta, *rta_tb[IFA_MAX+1] = {0,};
  char *family = toybuf, *scope = toybuf+256, *label = toybuf+512,
       *brd = toybuf+768, *peer = toybuf+1024, *any = toybuf+1280,
       lbuf[INET6_ADDRSTRLEN] = {0,}, lbuf_ifa[INET6_ADDRSTRLEN] = {0,};
  struct ifaddrmsg *ifa = NLMSG_DATA(h);
  int len;

  if ((len = h->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa))) < 0) {
    error_msg("wrong nlmsg len %d", len);
    return 0;
  }

  for (rta = IFA_RTA(ifa); RTA_OK(rta, len); rta=RTA_NEXT(rta, len))
    if (rta->rta_type <= IFA_MAX) rta_tb[rta->rta_type] = rta;

  if (!rta_tb[IFA_LOCAL]) rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
  if (!rta_tb[IFA_ADDRESS]) rta_tb[IFA_ADDRESS] = rta_tb[IFA_LOCAL];
  if ((addrinfo.scope ^ ifa->ifa_scope)&addrinfo.scopemask) return 0;
  if (addrinfo.ifindex && addrinfo.ifindex != ifa->ifa_index) return 0;

  if (flag_l && addrinfo.label && ifa->ifa_family == AF_INET6) return 0;
  if ((rta_tb[IFA_LABEL])) {
    xstrncpy(label, RTA_DATA(rta_tb[IFA_LABEL]), 256);
    label[255] = '\0';
    if (addrinfo.label && fnmatch(addrinfo.label, label, 0))
      return 0;
  }

  if (TT.flush) {
    if (ifa->ifa_index == addrinfo.ifindex) {
      h->nlmsg_type = RTM_DELADDR;
      h->nlmsg_flags = NLM_F_REQUEST;
      send_nlmesg(RTM_DELADDR, 0, 0, h, h->nlmsg_len);
      return 0;
    }
  }

  if (h->nlmsg_type == RTM_DELADDR) printf("Deleted ");

  if (TT.singleline) {
    if (!if_indextoname(ifa->ifa_index, lbuf)) perror_exit(NULL);
    printf("%u: %s",ifa->ifa_index, lbuf);
  }

  sprintf(scope, " scope %s ", namefromRPDB(ifa->ifa_scope, RPDB_rtscopes));

  if (ifa->ifa_family == AF_INET) strcpy(family, "    inet ");
  else if (ifa->ifa_family == AF_INET6) strcpy(family, "    inet6 ");
  else sprintf(family, "    family %d", ifa->ifa_family);

  if (rta_tb[IFA_LOCAL]) {
    if (!inet_ntop(ifa->ifa_family, RTA_DATA(rta_tb[IFA_LOCAL]),
          lbuf, sizeof(lbuf))) perror_exit("inet");

    sprintf(family+strlen(family), lbuf, strlen(lbuf));
    if (!rta_tb[IFA_ADDRESS] || !memcmp(RTA_DATA(rta_tb[IFA_ADDRESS]),
          RTA_DATA(rta_tb[IFA_LOCAL]), 4))
      sprintf(family+strlen(family), "/%d ", ifa->ifa_prefixlen);
    else {
      if (!inet_ntop(ifa->ifa_family, RTA_DATA(rta_tb[IFA_ADDRESS]),
            lbuf_ifa, sizeof(lbuf_ifa))) perror_exit("inet");
      sprintf(peer, " peer %s/%d ", lbuf_ifa, ifa->ifa_prefixlen);
    }
  }

  if (addrinfo.to && strcmp(addrinfo.addr, lbuf))
    return 0;

  if (rta_tb[IFA_BROADCAST]) {
    if (!inet_ntop(ifa->ifa_family, RTA_DATA(rta_tb[IFA_BROADCAST]),
          lbuf, sizeof(lbuf))) perror_exit("inet");
    sprintf(brd, " brd %s", lbuf);
  }else brd = "";

  if (rta_tb[IFA_ANYCAST]) {
    if (!inet_ntop(ifa->ifa_family, RTA_DATA(rta_tb[IFA_ANYCAST]),
          lbuf, sizeof(lbuf))) perror_exit("inet");
    sprintf(any, " any %s", lbuf);
  }

  if (ifa->ifa_family == AF_INET)
    printf("%s%s%s%s%s %c", family, brd, peer, scope, label,
        (TT.singleline? '\0' : '\n'));
  else printf("%s%s %c", family, scope, (TT.singleline? '\0' : '\n'));
  if (TT.singleline && (ifa->ifa_family == AF_INET)) xputc('\n');

  if (rta_tb[IFA_CACHEINFO]) {
    struct ifa_cacheinfo *ci = RTA_DATA(rta_tb[IFA_CACHEINFO]);

    printf("%c      valid_lft ", (TT.singleline? '\\' : '\0'));
    if (ci->ifa_valid ==  0xFFFFFFFFU) printf("forever");
    else printf("%usec", ci->ifa_valid);
    printf(" preferred_lft ");
    if (ci->ifa_prefered ==  0xFFFFFFFFU) printf("forever");
    else printf("%dsec", ci->ifa_prefered);
    xputc('\n');
  }
  return 0;
}

static int ipaddrupdate(char **argv)
{
  int length, cmd = !memcmp("add", argv[-1], strlen(argv[-1]))
    ? RTM_NEWADDR: RTM_DELADDR;
  int idx = 0,length_brd = 0, length_peer = 0,length_any = 0,length_local = 0,
      scoped = 0;
  char *dev = NULL,*label = NULL, reply[8192];

  struct nlmsghdr *addr_ptr = NULL;
  struct nlmsgerr *err = NULL;
  struct arglist cmd_objectlist[] = {{"dev",0}, {"peer", 1},
    {"remote", 2}, {"broadcast", 3}, {"brd", 4}, {"label", 5},
    {"anycast", 6},{"scope", 7}, {"local", 8}, {NULL, -1}};
  struct {
    struct nlmsghdr nlm;
    struct ifaddrmsg ifadd;
    char buf[256];
  } req;
  typedef struct {
    int family, bytelen, bitlen;
    __u32  data[8];
  } option_data;
  option_data local;

  memset(&req, 0, sizeof(req));
  req.nlm.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
  req.nlm.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
  req.nlm.nlmsg_type = cmd;
  req.ifadd.ifa_family = TT.addressfamily;

  while (*argv) {
    idx = substring_to_idx(*argv, cmd_objectlist);
    if (idx >= 0)
      if (!*++argv)
        error_exit("Incomplete Command line");
    switch(idx) {
      case 0:
        dev = *argv;
        break;
      case 1:
      case 2:
        {
          uint32_t addr[4] = {0,}, netmask = 0;
          uint8_t len = 0;
          parse_prefix(addr, &netmask, &len, *argv,
              req.ifadd.ifa_family);
          if (len)
            req.ifadd.ifa_family = ((len == 4) ? AF_INET : AF_INET6);
          length_peer = len;
          add_string_to_rtattr(&req.nlm, sizeof(req),
              IFA_ADDRESS, addr, len);
          req.ifadd.ifa_prefixlen = netmask;
        }
        break;
      case 3:
      case 4:
        if (*argv[0] == '+') {
          length_brd = -1;
        } else if (*argv[0] == '-') {
          length_brd = -2;
        } else {
          uint32_t addr[4] = {0,};
          uint8_t af = AF_UNSPEC;

          if (get_prefix(addr, &af, *argv, req.ifadd.ifa_family))
            error_exit("Invalid prefix");

          length_brd = ((af == AF_INET6) ? 16 : 4);
          if (req.ifadd.ifa_family == AF_UNSPEC)
            req.ifadd.ifa_family = af;
          add_string_to_rtattr(&req.nlm, sizeof(req),
              IFA_BROADCAST, &addr, length_brd);
        }
        break;
      case 5:
        label = *argv;
        add_string_to_rtattr(&req.nlm, sizeof(req),
            IFA_LABEL, label, strlen(label) + 1);
        break;
      case 6:
        {
          uint32_t addr[4] = {0,};
          uint8_t af = AF_UNSPEC;

          if (get_prefix(addr, &af, *argv, req.ifadd.ifa_family))
            error_exit("Invalid prefix");
          length_any = ((af == AF_INET6) ? 16 : 4);
          if (req.ifadd.ifa_family == AF_UNSPEC)
            req.ifadd.ifa_family = af;
          add_string_to_rtattr(&req.nlm, sizeof(req),
              IFA_ANYCAST, &addr, length_any);
        }
        break;
      case 7:
        {
          int scope = idxfromRPDB(*argv, RPDB_rtscopes);
          if (scope < 0) error_exit("wrong scope '%s'", *argv);
          req.ifadd.ifa_scope = scope;
          scoped = 1;
        }
        break;
      default:
        {
          //local is by default
          uint32_t addr[8] = {0,}, netmask = 0;
          uint8_t len = 0;

          parse_prefix(addr, &netmask, &len, *argv,
              req.ifadd.ifa_family);
          if (len)
            req.ifadd.ifa_family = ((len == 4) ? AF_INET : AF_INET6);
          length_local = len;
          local.bitlen = netmask;
          local.bytelen = len;
          memcpy(local.data, addr, sizeof(local.data));
          local.family = req.ifadd.ifa_family;
          add_string_to_rtattr(&req.nlm, sizeof(req),
              IFA_LOCAL, &local.data, local.bytelen);
        }
        break;
    }
    argv++;
  }
  if (!dev) error_exit("need \"dev \" argument");
  if (label && strncmp(dev, label, strlen(dev)) != 0)
    error_exit("\"dev\" (%s) must match \"label\" (%s)", dev, label);

  if (length_peer == 0 && length_local && cmd != RTM_DELADDR){
    add_string_to_rtattr(&req.nlm, sizeof(req),
        IFA_ADDRESS, &local.data, local.bytelen);
  }

  if (length_brd < 0 && cmd != RTM_DELADDR){
    int i;

    if (req.ifadd.ifa_family != AF_INET)
      error_exit("broadcast can be set only for IPv4 addresses");

    if (local.bitlen <= 30) {
      for (i = 31; i >= local.bitlen; i--) {
        if (length_brd == -1)
          local.data[0] |= htonl(1<<(31-i));
        else
          local.data[0] &= ~htonl(1<<(31-i));
      }
      add_string_to_rtattr(&req.nlm, sizeof(req),
          IFA_BROADCAST, &local.data, local.bytelen);
      length_brd = local.bytelen;
    }
  }
  if (req.ifadd.ifa_prefixlen == 0)
    req.ifadd.ifa_prefixlen = local.bitlen;
  if (!scoped && (cmd != RTM_DELADDR) && (local.family == AF_INET)
      && (local.bytelen >= 1 && *(uint8_t*)&local.data == 127))
    req.ifadd.ifa_scope = RT_SCOPE_HOST;
  req.ifadd.ifa_index = get_ifaceindex(dev, 1);

  send_nlmesg(RTM_NEWADDR, 0, AF_UNSPEC, (void *)&req, req.nlm.nlmsg_len);
  length = recv(TT.sockfd, reply, sizeof(reply), 0);
  addr_ptr = (struct nlmsghdr *) reply;
  for (; NLMSG_OK(addr_ptr, length); addr_ptr = NLMSG_NEXT(addr_ptr, length)) {
    if (addr_ptr->nlmsg_type == NLMSG_DONE)
      return 1;
    if (addr_ptr->nlmsg_type == NLMSG_ERROR)
      err = (struct nlmsgerr*) NLMSG_DATA(addr_ptr);
    if (err && err->error) {
      errno = -err->error;
      perror_exit("RTNETLINK answers:");
    }
  }
  return 0;
}

static int ipaddr_listflush(char **argv)
{
  int idx; uint32_t netmask = 0, found = 0;
  char *tmp = NULL, *name = NULL;
  struct double_list *dlist;
  struct arglist cmd_objectlist[] = {{"to", 0}, {"scope", 1}, {"up", 2},
    {"label", 3}, {"dev", 4}, {NULL, -1}};

  TT.flush = *argv[-1] == 'f' ? 1 : 0;
  memset(&addrinfo, 0, sizeof(addrinfo));

  if (TT.flush) {
    if (!*argv)
      error_exit("Incomplete command for \"flush\"");
    if (TT.addressfamily == AF_PACKET)
      error_exit("Can't flush link Addresses");
  }
  addrinfo.scope = -1;
  while (*argv) {
    switch (idx = substring_to_idx(*argv, cmd_objectlist)) {
      case 0:
        {// ADDR_TO
          if (!*++argv) error_exit("Incomplete Command line");
          else if (!strcmp(*argv, "0")) return 0;
          uint32_t addr[4] = {0,};
          uint8_t len = 0;

          addrinfo.to = 1;
          parse_prefix(addr, &netmask, &len, *argv, TT.addressfamily);
          if (len)
            TT.addressfamily = ((len == 4) ? AF_INET : AF_INET6);
          addrinfo.addr  = strtok(*argv, "/");
        }
        break;
      case 1: // ADDR_SCOPE
        {
          int scope = 0;
          if (!*++argv) error_exit("Incomplete Command line");
          name = *argv;

          addrinfo.scopemask = -1;
          if (isdigit(**argv)) {
            int idx = atolx(*argv);

            name = xstrdup(namefromRPDB(idx, RPDB_rtscopes));
          }
          if ((scope = idxfromRPDB(name, RPDB_rtscopes)) < 0) {
            if (strcmp(name, "all"))
              error_exit("wrong scope '%s'", name);
            scope = RT_SCOPE_NOWHERE;
            addrinfo.scopemask = 0;
          }

          if (isdigit(**argv))
            free(name);
          addrinfo.scope = scope;
        }
        break;
      case 2: // ADDR_UP
        addrinfo.up = 1;
        break;
      case 3: // ADDR_LABEL
        if (!*++argv) error_exit("Incomplete Command line");
        addrinfo.label = *argv;
        break;
      case 4: // ADDR_DEV
        if (!*++argv) error_exit("Incomplete Command line");

      default:
        if (TT.filter_dev)
          error_exit("Either \"dev\" is duplicate or %s is garbage",
              *argv);
        TT.filter_dev = *argv;
        break;
    }
    argv++;
  }

  link_show(&tmp);
  while ( linfo && (dlist = dlist_pop(&linfo))){
    struct linkdata *tmp  = (struct linkdata*) dlist;
    char *temp = &tmp->iface[0];

    if (TT.filter_dev && strcmp(TT.filter_dev, temp))
      continue;
    found = 1;
    if (TT.flush && addrinfo.label) ipaddr_print( tmp, 0);
    if (addrinfo.up && !(tmp->flags & IFF_UP)){
      ipaddr_print(tmp, 0);
      continue;
    }
    if (addrinfo.label){
      if ( fnmatch(addrinfo.label, temp, 0)) {
        ipaddr_print(tmp, 1);
        continue;
      }
    }
    if (!TT.addressfamily && ! TT.flush ) print_link_output(tmp);

    ipaddr_print(tmp, 0);
    free(tmp);
  }
  if (TT.filter_dev && !found)
    error_exit("Device \"%s\" doesn't exist. \n", TT.filter_dev);
  return 0;
}

static int ipaddr_print( struct linkdata *link, int flag_l)
{
  struct nlmsghdr *addr_ptr;
  int ip_match = 0;

  addrinfo.ifindex = link->iface_idx;
  send_nlmesg(RTM_GETADDR, NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST,
      AF_UNSPEC, NULL, 0);
  if (TT.addressfamily == AF_PACKET) print_link_output(link);

  if (addrinfo.label){
    char *col = strchr(addrinfo.label, ':');
    if (!col && (fnmatch(addrinfo.label, &link->iface[0], 0)))
      return 0;
  }

  while (1){
    int len = recv(TT.sockfd, TT.gbuf, sizeof(TT.gbuf), 0);
    addr_ptr = (struct nlmsghdr *)TT.gbuf;
    struct ifaddrmsg *addressInfo = NLMSG_DATA(addr_ptr);
    char lbuf[INET6_ADDRSTRLEN];
    struct rtattr *rta, *rta_tb[IFA_MAX+1] = {0,};

    int len1 = addr_ptr->nlmsg_len - NLMSG_LENGTH(sizeof(*addressInfo));
    if (len1 > 0) {
      for (; NLMSG_OK(addr_ptr, len); addr_ptr = NLMSG_NEXT(addr_ptr, len)) {
        addressInfo = NLMSG_DATA(addr_ptr);
        if (TT.addressfamily && TT.addressfamily != addressInfo->ifa_family)
          continue;
        if (addrinfo.ifindex && addrinfo.ifindex != addressInfo->ifa_index)
          continue;

        if (addrinfo.to) {
          memset(rta_tb, 0, sizeof(rta_tb));
          int rt_len = IFA_PAYLOAD(addr_ptr);
          for (rta = IFA_RTA(addressInfo); RTA_OK(rta, rt_len); rta=RTA_NEXT(rta, rt_len)) {
            if (rta->rta_type <= IFA_MAX) rta_tb[rta->rta_type] = rta;
          }
          if (!rta_tb[IFA_LOCAL]) rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
          if (rta_tb[IFA_LOCAL]) {
            if (!inet_ntop(TT.addressfamily, RTA_DATA(rta_tb[IFA_LOCAL]),
                  lbuf, sizeof(lbuf))) perror_exit("inet");
            if (strcmp(addrinfo.addr, lbuf))
              continue;
            ip_match=1;
          }
          if (!ip_match)
            continue;
        }

        if (!TT.flush){
          if (addrinfo.scope != -1 && TT.addressfamily && TT.addressfamily ==
              addressInfo->ifa_family &&
              (addrinfo.ifindex == addressInfo->ifa_index)) {
            if ((addrinfo.scope ^ addressInfo->ifa_scope) & addrinfo.scopemask)
              continue;
            else if (addrinfo.up && (link->flags & IFF_UP))
              print_link_output(link);
            else if (!addrinfo.up) print_link_output(link);
          }
          if (TT.addressfamily &&
              (addrinfo.ifindex == addressInfo->ifa_index) &&
              (addrinfo.scope == -1)){
            if (addrinfo.up && (link->flags & IFF_UP))
              print_link_output(link);
            else if (!addrinfo.up) print_link_output(link);
          }
        }

        for (; NLMSG_OK(addr_ptr, len); addr_ptr = NLMSG_NEXT(addr_ptr, len)) {
          if (addr_ptr->nlmsg_type == RTM_NEWADDR)
            print_addrinfo(addr_ptr, flag_l);
          if ((addr_ptr->nlmsg_type == NLMSG_DONE) ||
              (addr_ptr->nlmsg_type == NLMSG_ERROR) ||
              (TT.flush && addrinfo.to))
            goto ret_stop;
        }
        if ((addr_ptr->nlmsg_type == NLMSG_DONE) ||
            (addr_ptr->nlmsg_type == NLMSG_ERROR))
          break;
      }
    }
    else
      return 0;
  }

ret_stop:
  return 0;
}

static int ipaddr(char **argv)
{
  int    idx;
  cmdobj ipcmd, cmdobjlist[] = {ipaddrupdate, ipaddr_listflush};
  struct arglist cmd_objectlist[] = { {"add", 0}, {"delete", 0},
    {"list", 1},{"show", 1},{"lst", 1}, {"flush", 1}, {NULL,-1}};

  TT.is_addr++;
  if (!*argv) idx = 1;
  else if ((idx = substring_to_idx(*argv++, cmd_objectlist)) == -1)
    help_exit(0);

  ipcmd = cmdobjlist[idx];
  return ipcmd(argv);
}

// ===========================================================================
// code for ip route
// ===========================================================================
struct I_data {
  unsigned char family;
  uint32_t addr[8] , netmask ;
  uint8_t len ;
};

struct {
  int tb,idev,odev,proto;
  struct I_data rvia, rdst, mdst, rsrc, msrc;
} gfilter;

static void show_iproute_help(void)
{
  error_exit("\n\n" \
       "iproute { list | flush } SELECTOR\n" \
       "iproute get ADDRESS [from ADDRESS iif STRING]\n" \
       "	[oif STRING]\n" \
       "iproute { add | del | change | append | replace | test } ROUTE\n" \
       "	SELECTOR := [root PREFIX] [match PREFIX] [proto RTPROTO]\n" \
       "	ROUTE := [TYPE] PREFIX [proto RTPROTO] [metric METRIC]");
}

static void print_rta_metrics(char* out, const struct rtattr *mxattr)
{
  int32_t tvar = RTA_PAYLOAD(mxattr);
  struct rtattr *rta, *mxrta[RTAX_MAX+1] = {0,};
  unsigned int mxlock = 0;
  int i;

  for (rta = RTA_DATA(mxattr); RTA_OK(rta, tvar); rta=RTA_NEXT(rta, tvar))
    if (rta->rta_type <= RTA_MAX) mxrta[rta->rta_type] = rta;

  if (mxrta[RTAX_LOCK])
    mxlock = *(u_int32_t *)RTA_DATA(mxrta[RTAX_LOCK]);

  for (i = 2; i <= RTAX_MAX; i++) {
    uint32_t val = 0;

    if (mxrta[i] == NULL && !(mxlock & (1 << i)))
      continue;

    if (mxrta[i] != NULL && i != RTAX_CC_ALGO)
      val = *(u_int32_t *)RTA_DATA(mxrta[i]);

    if (i == RTAX_HOPLIMIT && (int)val == -1)
      continue;

    if (i < sizeof(mx_names)/sizeof(char *) && mx_names[i])
      sprintf(out, "%s%s ", out, mx_names[i]);
    else
      sprintf(out, "%smetric %d ", out, i);

    if (mxlock & (1<<i))
      sprintf(out, "%slock ", out);

    switch (i) {
      case RTAX_RTT:
      case RTAX_RTTVAR:
      case RTAX_RTO_MIN:
        if (i == RTAX_RTT)
          val /= 8;
        else if (i == RTAX_RTTVAR)
          val /= 4;

        if (val >= 1000)
          sprintf(out, "%s%gs ", out, val / 1e3);
        else
          sprintf(out, "%s%ums ", out, val);
        break;

      case RTAX_CC_ALGO:
        sprintf(out, "%scongestion %s ", out, (const char*)RTA_DATA(mxrta[i]));
        break;

      default:
        sprintf(out, "%s%u ", out, val);
        break;
    }
  }
}

static int display_route_info(struct nlmsghdr *mhdr, char **argv)
{
  char *inetval = NULL, out[1024] = {0};
  struct rtmsg *msg = NLMSG_DATA(mhdr);
  struct rtattr *rta, *attr[RTA_MAX+1] = {0,};
  int32_t tvar, msglen = mhdr->nlmsg_len - NLMSG_LENGTH(sizeof(struct rtmsg));
  int hlen = ((msg->rtm_family == AF_INET) ? 32
      : ((msg->rtm_family == AF_INET6) ? 128 : -1));

  if (mhdr->nlmsg_type != RTM_NEWROUTE) return 0;
  if (msglen < 0) return 1;

  if (msg->rtm_family == AF_INET6) {
    if (gfilter.tb) {
      if (gfilter.tb < 0) {
        if (!(msg->rtm_flags & RTM_F_CLONED)) return 0;
      } else {
        if (msg->rtm_flags & RTM_F_CLONED) return 0;
        if (gfilter.tb == RT_TABLE_LOCAL && msg->rtm_type != RTN_LOCAL)
          return 0;
        else if (gfilter.tb == RT_TABLE_MAIN && msg->rtm_type == RTN_LOCAL)
          return 0;
      }
    }
  }
  else if (gfilter.tb > 0 && gfilter.tb != msg->rtm_table) return 0;

  if (gfilter.proto && (msg->rtm_protocol != gfilter.proto)) return 0;


  if (gfilter.rdst.family && (msg->rtm_family != gfilter.rdst.family ||
        gfilter.rdst.netmask > msg->rtm_dst_len)) return 0;
  if (gfilter.mdst.family && (msg->rtm_family != gfilter.mdst.family
        || (gfilter.mdst.netmask < msg->rtm_dst_len))) return 0;
  if (gfilter.rsrc.family && (msg->rtm_family != gfilter.rsrc.family
        || gfilter.rsrc.netmask > msg->rtm_src_len)) return 0;
  if (gfilter.msrc.family && (msg->rtm_family != gfilter.msrc.family
        || (gfilter.msrc.netmask < msg->rtm_src_len))) return 0;
  tvar = msglen;

  for (rta = RTM_RTA(msg); RTA_OK(rta, tvar); rta=RTA_NEXT(rta, tvar))
    if (rta->rta_type <= RTA_MAX) attr[rta->rta_type] = rta;

  if (msg->rtm_type != RTN_UNICAST)
    sprintf(out,"%s%s ", out,rtmtype_idx2str(msg->rtm_type));
  if (attr[RTA_DST]) {
    inetval = (char *)inet_ntop(msg->rtm_family, RTA_DATA(attr[RTA_DST]),
        toybuf, sizeof(toybuf));
    if (gfilter.rdst.family &&
        memcmp(RTA_DATA(attr[RTA_DST]), &gfilter.rdst.addr, gfilter.rdst.len))
      return 0;
    if (gfilter.mdst.family &&
        memcmp(RTA_DATA(attr[RTA_DST]), &gfilter.mdst.addr, gfilter.mdst.len))
      return 0;
    sprintf(out,"%s%s",out,inetval);
  }
  if (msg->rtm_dst_len) sprintf(out,"%s/%d ", out,msg->rtm_dst_len);
  else sprintf(out,"%s%s",out,"default ");

  if (attr[RTA_SRC]) {
    inetval = (char *)inet_ntop(msg->rtm_family, RTA_DATA(attr[RTA_SRC]),
        toybuf, sizeof(toybuf));
    if (gfilter.rsrc.family &&
        memcmp(RTA_DATA(attr[RTA_SRC]), &gfilter.rsrc.addr, gfilter.rsrc.len))
      return 0;
    if (gfilter.msrc.family &&
        memcmp(RTA_DATA(attr[RTA_SRC]), &gfilter.msrc.addr, gfilter.msrc.len))
      return 0;
    sprintf(out, "%s from %s", out, inetval);
  }
  if (msg->rtm_src_len) sprintf(out, "%s/%d ", out, msg->rtm_src_len);

  if (attr[RTA_GATEWAY]) {
    inetval = (char *)inet_ntop(msg->rtm_family, RTA_DATA(attr[RTA_GATEWAY]),
        toybuf, sizeof(toybuf));
    sprintf(out, "%s via %s ", out, inetval);
  }
  if (gfilter.rvia.family) {
    char tmp[256];

    if (!attr[RTA_GATEWAY]) return 0;
    if (strcmp((char *)inet_ntop(msg->rtm_family, gfilter.rvia.addr,
            tmp, sizeof(tmp)), inetval)) return 0;
  }

  if (gfilter.odev != 0) if (!attr[RTA_OIF]) return 0;
  if (attr[RTA_OIF]) {
    if (gfilter.odev !=0 && gfilter.odev != *(int*)RTA_DATA(attr[RTA_OIF]))
      return 0;
    sprintf(out, "%s dev %s ", out,
        if_indextoname(*(int*)RTA_DATA(attr[RTA_OIF]), toybuf));
  }

  if (attr[RTA_PREFSRC] && hlen) {
    inetval = (char *)inet_ntop(msg->rtm_family, RTA_DATA(attr[RTA_PREFSRC]),
        toybuf, sizeof(toybuf));
    sprintf(out, "%s src %s ", out, inetval);
  }
  if (attr[RTA_PRIORITY])
    sprintf(out, "%s metric %d ", out, *(uint32_t*)RTA_DATA(attr[RTA_PRIORITY]));
  if (msg->rtm_family == AF_INET6) {
    struct rta_cacheinfo *ci = NULL;
    if (attr[RTA_CACHEINFO]) ci = RTA_DATA(attr[RTA_CACHEINFO]);
    if ((msg->rtm_flags & RTM_F_CLONED) || (ci && ci->rta_expires)) {
      if (msg->rtm_flags & RTM_F_CLONED) sprintf(out, "%s%s    cache ",
          out, (!TT.singleline ? "\n" : " "));
      if (ci && ci->rta_expires) {
        int hz = 0;
        FILE *fp = xfopen("/proc/net/psched","r");

        if (fp) {
          unsigned int nom, denom;

          if (fscanf(fp, "%*08x%*08x%08x%08x", &nom, &denom) == 2)
            if (nom == 1000000)
              hz = denom;
          fclose(fp);
        }
        if (!hz) hz = sysconf(_SC_CLK_TCK);
        sprintf(out, "%s expires %dsec", out, ci->rta_expires /hz);
      }
      if (ci && ci->rta_error) sprintf(out, "%s error %d", out, ci->rta_error);
    }
    else if (ci && ci->rta_error)
      sprintf(out, "%s error %d", out, ci->rta_error);
  }
  if (attr[RTA_IIF] && !gfilter.idev)
    sprintf(out, "%s iif %s", out,
        if_indextoname(*(int*)RTA_DATA(attr[RTA_IIF]), toybuf));

  if (attr[RTA_METRICS])
    print_rta_metrics(out, attr[RTA_METRICS]);

  if (TT.flush || (TT.connected && !TT.from_ok))
    memcpy(toybuf, (void*)mhdr,mhdr->nlmsg_len);

  if (TT.flush) {
    int sockfd = 0;
    struct nlmsghdr* mhdr = (struct nlmsghdr*)toybuf;
    struct rtmsg *msg = NLMSG_DATA(mhdr);
    int tvar, msglen = mhdr->nlmsg_len - NLMSG_LENGTH(sizeof(struct rtmsg));
    struct rtattr *rta, *attr[RTA_MAX+1] = {0,};

    tvar = msglen;
    for (rta = RTM_RTA(msg); RTA_OK(rta, tvar); rta=RTA_NEXT(rta, tvar))
      if (rta->rta_type <= RTA_MAX) attr[rta->rta_type] = rta;

    if (msg->rtm_family == AF_INET6
        && !msg->rtm_dst_len
        && msg->rtm_type == RTN_UNREACHABLE
        && attr[RTA_PRIORITY]
        && *(int*)RTA_DATA(attr[RTA_PRIORITY]) == -1)
      return 0;

    mhdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    mhdr->nlmsg_type  = RTM_DELROUTE;
    mhdr->nlmsg_pid = 0;
    sockfd = xsocket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (send(sockfd , (void*)mhdr, mhdr->nlmsg_len, 0) < 0)
      perror_exit("Unable to send data on socket.");

    while (1) {
      struct nlmsghdr *mhdr;
      int msglen = recv(sockfd, toybuf, sizeof(toybuf), 0);

      if ((msglen < 0) && (errno == EINTR || errno == EAGAIN)) continue;
      else if (msglen < 0) {
        error_msg("netlink receive error %s", strerror(errno));
        xclose(sockfd);
        return 1;
      } else if (!msglen) {
        error_msg("EOF on netlink");
        xclose(sockfd);
        return 1;
      }

      for (mhdr = (struct nlmsghdr*)toybuf; NLMSG_OK(mhdr, msglen);
          mhdr = NLMSG_NEXT(mhdr, msglen)) {
        switch (mhdr->nlmsg_type) {
          case NLMSG_DONE:
            xclose(sockfd);
            return 0;
          case NLMSG_ERROR:
            {
              struct nlmsgerr *merr = (struct nlmsgerr*)NLMSG_DATA(mhdr);

              if (merr->error == 0)  { xclose(sockfd); return 0; }
              if (mhdr->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr)))
                error_msg("ERROR truncated");
              else {
                errno = -merr->error;
                perror_msg("RTNETLINK answers");
              }
              xclose(sockfd);
              return 1;
            }
          default:
            break;
        }
      } // End of for loop.
    } // End of while loop.

    xclose(sockfd);
  } else printf("%s\n",out);
  return 0;
}

static int route_get(char **argv)
{
  int idx, flag;
  struct arglist cmd_objectlist[] = {{"from", 0}, {"iif", 1}, {"oif", 2},
    {"dev", 3}, {"notify", 4}, {"connected", 5}, {"to", 6}, {NULL, -1}};
  char *idev = NULL, *odev = NULL;
  struct {
    struct nlmsghdr mhdr;
    struct rtmsg msg;
    char buf[1024];
  } request;

  memset(&request, 0, sizeof(request));
  request.mhdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  request.mhdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
  request.mhdr.nlmsg_type = RTM_GETROUTE;
  request.msg.rtm_family = AF_UNSPEC;

  for (; *argv; argv++) {
    switch(idx = substring_to_idx(*argv, cmd_objectlist)) {
      case 0: TT.from_ok = 1; // dst address
      case 6: argv++; //fallthrough
      default:
              {
                uint32_t addr[8] = {0,}, netmask = 0;
                uint8_t len = 0;

                if (!*argv) error_exit("'%s': Missing Prefix", argv[-1]);
                parse_prefix(addr, &netmask, &len, *argv, request.msg.rtm_family);
                if (len) request.msg.rtm_family = ((len == 4) ? AF_INET : AF_INET6);
                netmask = (request.msg.rtm_family == AF_INET6) ? 128 : 32;
                if (!idx) request.msg.rtm_src_len = netmask;
                else request.msg.rtm_dst_len = netmask;
                add_string_to_rtattr(&request.mhdr, sizeof(request),
                    (!idx ? RTA_SRC : RTA_DST), addr, len);
                break;
              }
      case 1:
      case 2:
      case 3:
              if (!*++argv) show_iproute_help();
              if (idx == 1) idev = *argv, flag = RTA_IIF;
              else odev = *argv, flag = RTA_OIF;
              idx = get_ifaceindex(*argv, 1);
              add_string_to_rtattr(&request.mhdr, sizeof(request),
                  flag, (char*)&idx, sizeof(idx));
              break;
      case 4:
              request.msg.rtm_flags |= RTM_F_NOTIFY;
              break;
      case 5:
              TT.connected = 1;
              break;
    }
  }
  if (!request.msg.rtm_dst_len)
    error_exit("need at least destination address");

  send_nlmesg(0, 0, 0, &request, sizeof(request));
  filter_nlmesg(display_route_info, NULL);

  if (TT.connected && !TT.from_ok) {
    struct nlmsghdr *mhdr = (struct nlmsghdr*)toybuf;
    struct rtmsg *msg = NLMSG_DATA(mhdr);
    int tvar, msglen = mhdr->nlmsg_len - NLMSG_LENGTH(sizeof(struct rtmsg));
    struct rtattr *rta, *attr[RTA_MAX+1] = {0,};

    if (mhdr->nlmsg_type != RTM_NEWROUTE) error_exit("not a route?");
    if (msglen < 0) error_exit("wrong len %d", msglen);

    tvar = msglen;
    for (rta = RTM_RTA(msg); RTA_OK(rta, tvar); rta=RTA_NEXT(rta, tvar))
      if (rta->rta_type <= RTA_MAX) attr[rta->rta_type] = rta;

    if (attr[RTA_PREFSRC]) {
      attr[RTA_PREFSRC]->rta_type = RTA_SRC;
      msg->rtm_src_len = 8*RTA_PAYLOAD(attr[RTA_PREFSRC]);
    } else if (!attr[RTA_SRC]) error_exit("can't connect the route");

    if (!odev && attr[RTA_OIF]) attr[RTA_OIF]->rta_type = 0;
    if (attr[RTA_GATEWAY]) attr[RTA_GATEWAY]->rta_type = 0;
    if (!idev && attr[RTA_IIF]) attr[RTA_IIF]->rta_type = 0;
    mhdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    mhdr->nlmsg_type  = RTM_GETROUTE;
    mhdr->nlmsg_pid = 0;
    xclose(TT.sockfd);
    TT.sockfd = xsocket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    send_nlmesg(0, 0, 0, mhdr, mhdr->nlmsg_len);
    filter_nlmesg(display_route_info, NULL);
  }
  return 0;
}

static int route_show_flush(char **argv)
{
  struct arglist cmd_objectlist[] = {{"protocol", 0}, {"dev", 1}, {"oif", 2},
    {"iif", 3}, {"via", 4}, {"table", 5}, {"cache", 6}, {"from", 7},
    {"to", 8}, {"all", 9}, {"root", 10}, {"match", 11}, {"exact", 12},
    {"main", 13}, {NULL,-1}};
  int family = TT.addressfamily, idx;
  struct {
    struct nlmsghdr mhdr;
    struct rtmsg msg;
  } request;

  if (*argv[-1] == 'f') TT.flush = 1;
  if (TT.flush && !*argv) show_iproute_help();

  gfilter.tb = RT_TABLE_MAIN;
  for (; *argv; argv++) {
    switch (idx = substring_to_idx(*argv, cmd_objectlist)) {
      case 0:
        if (!*++argv) show_iproute_help();
        if ((idx = idxfromRPDB(*argv,RPDB_rtprotos)) < 0)
          error_exit("Invalid argument protocol.");
        gfilter.proto = idx;
        break;
      case 1:
      case 2:
      case 3:
        {
          if (!*++argv) show_iproute_help();
          int dev = get_ifaceindex(*argv, 1);

          if (idx == 3) gfilter.idev = dev;
          else gfilter.odev = dev;
        }
        break;
      case 4:
        if (!*++argv) show_iproute_help();
        parse_prefix(gfilter.rvia.addr, &gfilter.rvia.netmask,
            &gfilter.rvia.len, *argv, gfilter.rvia.family);
        if (gfilter.rvia.len)
          gfilter.rvia.family = ((gfilter.rvia.len == 4) ?
              AF_INET : AF_INET6);
        break;
      case 5:
        if (!*++argv) show_iproute_help();
        idx = substring_to_idx(*argv, cmd_objectlist);
        if (idx == 6) gfilter.tb = -1;
        else if (idx == 9) gfilter.tb = 0;
        else if (idx != 13) {
          if ((gfilter.tb = idxfromRPDB(*argv, RPDB_rttables)) < 0)
            error_exit("table %s is invalid.", *argv);
        }
        break;
      case 6:
        gfilter.tb = -1;
        break;
      case 7:
        if (!*++argv) show_iproute_help();
        idx = substring_to_idx(*argv, cmd_objectlist);
        if (idx < 0)  if (!*++argv) show_iproute_help();
        if (idx == 10)
           if (!*++argv) show_iproute_help();
          parse_prefix(gfilter.rsrc.addr, &gfilter.rsrc.netmask,
              &gfilter.rsrc.len, *argv, gfilter.rsrc.family);
        if (gfilter.rsrc.len)
          gfilter.rsrc.family = ((gfilter.rsrc.len == 4) ?
              AF_INET : AF_INET6);
        else {
          if ((idx == 12 ||idx == 11) && !*++argv) show_iproute_help();
          parse_prefix(gfilter.msrc.addr, &gfilter.msrc.netmask,
              &gfilter.msrc.len, *argv, gfilter.msrc.family);
          if (gfilter.msrc.len)
            gfilter.msrc.family = ((gfilter.msrc.len == 4) ?
                AF_INET : AF_INET6);
          if (idx != 11) gfilter.rsrc = gfilter.msrc;
        }
        break;
      case 8:
        idx = substring_to_idx(*argv, cmd_objectlist);
        if (idx != -1 && !*++argv) show_iproute_help();
      default: // fallthrough
        if (idx == 10) {
          if (!*++argv) show_iproute_help();
          parse_prefix(gfilter.rdst.addr, &gfilter.rdst.netmask,
              &gfilter.rdst.len, *argv, gfilter.rdst.family);
        if (gfilter.rdst.len)
          gfilter.rdst.family = ((gfilter.rdst.len == 4) ?
              AF_INET : AF_INET6);
        }
        else {
          if ((idx == 12 ||idx == 11) && !*++argv) show_iproute_help();
          parse_prefix(gfilter.mdst.addr, &gfilter.mdst.netmask,
              &gfilter.mdst.len, *argv, gfilter.mdst.family);
          if (gfilter.mdst.len)
            gfilter.mdst.family = ((gfilter.mdst.len == 4) ?
                AF_INET : AF_INET6);
          if (idx != 11) gfilter.rdst = gfilter.mdst;
        }
        break;
    }
  }
  if (family == AF_UNSPEC && gfilter.tb) family = AF_INET;

  if (TT.flush) {
    if (gfilter.tb < 0) { // flush table cache
      if (family != AF_INET6) {
        FILE *fp = xfopen("/proc/sys/net/ipv4/route/flush", "w");

        if (fwrite("-1",1,2,fp) < 2) error_exit("can't flush routing cache");
        fclose(fp);
      }
      if (family == AF_INET) return 0;
    }
  }

  memset(&request, 0, sizeof (request));
  request.mhdr.nlmsg_len = NLMSG_LENGTH(sizeof (struct rtmsg));
  request.mhdr.nlmsg_flags = NLM_F_REQUEST;
  request.mhdr.nlmsg_flags |= NLM_F_ROOT | NLM_F_MATCH;
  request.mhdr.nlmsg_type = RTM_GETROUTE;
  request.msg.rtm_family = family;
  if (gfilter.tb < 0) request.msg.rtm_flags = RTM_F_CLONED;
  send_nlmesg(0, 0, 0, (void*)&request, sizeof (request));
  return (filter_nlmesg(display_route_info, NULL));
}

static int route_update(char **argv, unsigned int route_flags)
{
  char mxbuf[256], *d = NULL;
  struct rtattr *mxrta = (void*)mxbuf;
  unsigned mxlock = 0, ok = 0;
  int idx;
  uint32_t addr[8] = {0,}, netmask = 0;
  uint8_t len = 0;

  struct arglist cmd_objectlist[] = {{"src", 0}, {"via", 1}, {"mtu", 2},
    {"lock", 3}, {"protocol", 4}, {"table", 5}, {"dev", 6}, {"oif", 7},
    {"to", 8}, {"metric", 9}, {NULL,-1}
  };
  enum {
    gtwy_ok = 1,
    dst_ok = 2,
    proto_ok = 4,
    type_ok = 8
  };
  struct {
    struct nlmsghdr hdr;
    struct rtmsg msg;
    char buf[1024];
  } req;

  memset(&req, 0, sizeof(req));
  req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  req.hdr.nlmsg_flags = NLM_F_ACK| NLM_F_REQUEST | route_flags;
  req.hdr.nlmsg_type = TT.route_cmd;
  req.msg.rtm_family = AF_UNSPEC;
  req.msg.rtm_table = RT_TABLE_MAIN;
  req.msg.rtm_scope = RT_SCOPE_NOWHERE;

  if (TT.route_cmd != RTM_DELROUTE) {
    req.msg.rtm_protocol = RTPROT_BOOT;
    req.msg.rtm_scope = RT_SCOPE_UNIVERSE;
    req.msg.rtm_type = RTN_UNICAST;
  }

  mxrta->rta_type = RTA_METRICS;
  mxrta->rta_len = RTA_LENGTH(0);

  for (; *argv; argv++) {
    idx = substring_to_idx(*argv, cmd_objectlist);
    if (!idx) {
      if (!*++argv) show_iproute_help();
      parse_prefix(addr, &netmask, &len, *argv, req.msg.rtm_family);
      if (len) req.msg.rtm_family = ((len == 4) ? AF_INET : AF_INET6);
      add_string_to_rtattr(&req.hdr, sizeof(req), RTA_PREFSRC, addr, len);
    } else if (idx == 1) {
      ok |= gtwy_ok;
      if (!*++argv) show_iproute_help();
      parse_prefix(addr, &netmask, &len, *argv, req.msg.rtm_family);
      if (len) req.msg.rtm_family = ((len == 4) ? AF_INET : AF_INET6);
      add_string_to_rtattr(&req.hdr, sizeof(req),RTA_GATEWAY, addr, len);
    } else if (idx == 2) {
      if (!*++argv) show_iproute_help();
      if (substring_to_idx(*argv, cmd_objectlist ) == 3) {
        mxlock |= (1 << RTAX_MTU);
        if (!*++argv) show_iproute_help();
      }
      idx = atolx(*argv);
      add_uint32_rtattr_to_buffer(mxrta, sizeof(mxbuf), RTAX_MTU, idx);
    } else if (idx == 4) {
      if (!*++argv) show_iproute_help();
      if ((idx = idxfromRPDB(*argv,RPDB_rtprotos)) < 0)
      error_exit("Invalid argument protocol %s.",*argv);
      req.msg.rtm_protocol = idx;
      ok |= proto_ok;
    } else if (idx == 5) {
      if (!*++argv) show_iproute_help();
      req.msg.rtm_table = idxfromRPDB(*argv, RPDB_rttables);
    } else if (idx == 6 || idx == 7) {
      if (!*++argv) show_iproute_help();
      d = *argv;
    } else if (idx == 9) {
      unsigned long metric;
      unsigned int res;
      char* ptr;
      if (!*++argv) show_iproute_help();
      metric = strtoul(*argv, &ptr, 0);
      if (!(!*ptr && metric <= 0xFFFFFFFFUL))
        error_exit("Invalid argument metric %s.",*argv);
      else
        res = metric;
      add_string_to_rtattr(&req.hdr, sizeof(req),
          RTA_PRIORITY, (char*)&res, sizeof(res));
    } else {
      if (idx == 8)
        if (!*++argv) show_iproute_help();
      idx = substring_to_idx(*argv,rtmtypes);
      if (idx != -1) {
        if (!*++argv) show_iproute_help();
        req.msg.rtm_type = idx;
        ok |= type_ok;
      }
      if (ok & dst_ok) error_exit("Duplicate argument 'to'");
      parse_prefix(addr, &netmask, &len, *argv, req.msg.rtm_family);
      if (len) req.msg.rtm_family = ((len == 4) ? AF_INET : AF_INET6);
      req.msg.rtm_dst_len = netmask;
      ok |= dst_ok;
      if (len) add_string_to_rtattr(&req.hdr, sizeof(req),RTA_DST, addr, len);
    }
  }

  if (d) {
    idx = get_ifaceindex(d,1);
    add_string_to_rtattr(&req.hdr, sizeof(req),
        RTA_OIF, (char*)&idx, sizeof(idx));
  }
  if (mxrta->rta_len > RTA_LENGTH(0)) {
    if (mxlock)
      add_uint32_rtattr_to_buffer(mxrta, sizeof(mxbuf), RTAX_LOCK, mxlock);
    add_string_to_rtattr(&req.hdr, sizeof(req),
        RTA_METRICS, RTA_DATA(mxrta), RTA_PAYLOAD(mxrta));
  }

  if (req.msg.rtm_type == RTN_LOCAL || req.msg.rtm_type == RTN_NAT)
    req.msg.rtm_scope = RT_SCOPE_HOST;
  else if (req.msg.rtm_type == RTN_BROADCAST||req.msg.rtm_type == RTN_MULTICAST
      || req.msg.rtm_type == RTN_ANYCAST)
    req.msg.rtm_scope = RT_SCOPE_LINK;
  else if (req.msg.rtm_type == RTN_UNICAST || req.msg.rtm_type == RTN_UNSPEC) {
    if (TT.route_cmd == RTM_DELROUTE)
      req.msg.rtm_scope = RT_SCOPE_NOWHERE;
    else if (!(ok & gtwy_ok))
      req.msg.rtm_scope = RT_SCOPE_LINK;
  }
  if (req.msg.rtm_family == AF_UNSPEC) req.msg.rtm_family = AF_INET;
  send_nlmesg(0, 0, 0, &req, sizeof(req));
  filter_nlmesg(NULL, NULL);
  return 0;
}

static int iproute(char **argv)
{
  int idx = 1;
  struct arglist cmd_objectlist1[] = {{"add", 0}, {"append", 1},{"change", 2},
    {"chg", 3},{"delete",4}, {"get", 5}, {"list", 6}, {"show", 7},
    {"prepend", 8},{"replace", 9},{"test", 10}, {"flush", 11},{NULL,-1}};

  TT.route_cmd = RTM_NEWROUTE;
  switch (idx = substring_to_idx(*argv , cmd_objectlist1)) {
    case 0: // add
      return route_update(++argv , NLM_F_CREATE|NLM_F_EXCL);
    case 1: // append
      return route_update(++argv , NLM_F_CREATE|NLM_F_APPEND);
    case 2: // change
    case 3: // chg
      return route_update(++argv , NLM_F_REPLACE);
    case 4: // delete
      TT.route_cmd = RTM_DELROUTE;
      return route_update(++argv , RTM_DELROUTE);
    case 5:
      return route_get(++argv);
    case 6:
    case 7:
      return route_show_flush(++argv);
    case 8: // prepend
      return route_update(++argv , NLM_F_CREATE);
    case 9: // replace
      return route_update(++argv ,  NLM_F_CREATE|NLM_F_REPLACE);
    case 10: // test
      return route_update(++argv , NLM_F_EXCL);
    case 11: // flush
      return route_show_flush(++argv);
    default:
      if (!*argv) return route_show_flush(argv);
      else show_iproute_help();
  }
  return 0; // non reachable code.
}


// ===========================================================================
// code for ip rule.
// ===========================================================================
static void show_iprule_help(void)
{
  error_exit("usage: ip rule [ list | add | del ] SELECTOR ACTION\n"
    "SELECTOR := [ from PREFIX ] [ to PREFIX ] [pref NUMBER] [ tos TOS ]\n"
    "            [ fwmark FWMARK] [ dev/iif STRING ] [type TYPE]\n"
    "ACTION := [ table TABLE_ID ] [ realms [SRCREALM/]DSTREALM ]");
}

static int ruleupdate(char **argv)
{
  int8_t idx, tflag = 0, opt = (*argv[-1] == 'a') ? RTM_NEWRULE : RTM_DELRULE;
  struct arglist options[] = {{"from", 0}, {"to", 1}, {"preference", 2},
    {"order", 2}, {"priority", 2}, {"tos", 3}, {"dsfield", 3}, {"fwmark", 4},
    {"realms", 5}, {"table", 6}, {"lookup", 6}, {"dev", 7}, {"iif", 7},
    {"nat", 8}, {"map-to", 8}, {"type", 9}, {"help", 10}, {NULL, -1}};
  struct {
    struct nlmsghdr mhdr;
    struct rtmsg    msg;
    char buf[1024];
  } request;

  memset(&request, 0, sizeof(request));
  request.mhdr.nlmsg_type = opt;
  request.mhdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  request.mhdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK |
    ((opt == RTM_DELRULE) ? 0 : NLM_F_CREATE | NLM_F_EXCL);
  request.msg.rtm_family = TT.addressfamily;
  request.msg.rtm_protocol = RTPROT_BOOT;
  request.msg.rtm_scope = RT_SCOPE_UNIVERSE;
  request.msg.rtm_table = 0;
  request.msg.rtm_type = ((opt == RTM_DELRULE) ? RTN_UNSPEC : RTN_UNICAST);

  for (; *argv; argv++) {
    switch ((idx = substring_to_idx(*argv, options))) {
      case 0:
      case 1:
        { // e.g. from IP/Netmask and to IP/Netmask.
          uint32_t addr[4] = {0,}, netmask = 0;
          uint8_t len = 0, *tmp;

          if (!*++argv) error_exit("'%s': Missing Prefix", argv[-1]);
          parse_prefix(addr, &netmask, &len, *argv, request.msg.rtm_family);

          tmp = idx ? &request.msg.rtm_dst_len : &request.msg.rtm_src_len;
          if (!netmask) *tmp = 0;
          else *tmp = netmask;

          add_string_to_rtattr(&request.mhdr, sizeof(request),
              (idx ? RTA_DST : RTA_SRC), addr, len);
        }
        break;
      case 2:
      case 4:
        { // e.g. Preference p# and fwmark MARK
          uint32_t pref;
          char *ptr;

          if (!*++argv)
            error_exit("Missing %s", (idx == 2) ? "Preference" : "fwmark");
          pref = strtoul(*argv, &ptr, 0);
          if (!ptr || (ptr == *argv) || *ptr  || pref > 0xFFFFFFFFUL)
            error_exit("Invalid %s",  (idx == 2) ? "Preference" : "fwmark");
          add_string_to_rtattr(&request.mhdr, sizeof(request),
              ((idx == 2) ? RTA_PRIORITY : RTA_PROTOINFO),
              (void *)&pref, sizeof(uint32_t));
        }
        break;
      case 3:
        {
          uint32_t tos;
          if (!*++argv) error_exit("Missing TOS key");
          if ((tos = idxfromRPDB(*argv, RPDB_rtdsfield)) < 0)
            error_exit("Invalid TOS");
          request.msg.rtm_tos = tos;
        }
        break;
      case 5:
        { // e.g. realms FROM_realm/TO_realm
          uint32_t realms = 0;
          int ret;
          char *ptr;

          if (!*++argv) error_exit("Missing REALMSID");
          if ((ptr = strchr(*argv, '/'))) {
            *ptr = 0;
            if ((ret = idxfromRPDB(*argv, RPDB_rtrealms)) < 0)
              error_exit("Invalid realms");
            realms = ret;
            realms <<= 16;
            *ptr++ = '/';
          } else ptr = *argv;
          if ((ret = idxfromRPDB(ptr, RPDB_rtrealms)) < 0)
            error_exit("Invalid realms");
          realms |= ret;
          add_string_to_rtattr(&request.mhdr, sizeof(request),
              RTA_FLOW, (void *)&realms, sizeof(uint32_t));
        }
        break;
      case 6:
        { // e.g. table tid/tableName
          int tid;
          if (!*++argv) error_exit("Missing TableID");
          if ((tid = idxfromRPDB(*argv, RPDB_rttables)) < 0)
            error_exit("Invalid TID");
          request.msg.rtm_table = tid;
          tflag = 1;
        }
        break;
      case 7:
        {
          if (!*++argv) error_exit("Missing dev/iif NAME");
          add_string_to_rtattr(&request.mhdr, sizeof(request),
              RTA_IIF, *argv, strlen(*argv)+1);
        }
        break;
      case 8:
        {
          uint32_t addr[4] = {0,};
          uint8_t af = AF_UNSPEC;

          if (!*++argv) error_exit("Missing nat/map-to ADDRESS");
          if (get_prefix(addr, &af /* Un-used variable */, *argv, AF_INET))
            error_exit("Invalid mapping Address");

          add_string_to_rtattr(&request.mhdr, sizeof(request),
              RTA_GATEWAY, addr, sizeof(uint32_t));
          request.msg.rtm_type = RTN_NAT;
        }
        break;
      case 9:
        {
          if (!*++argv) error_exit("TYPE Missing");
          request.msg.rtm_type = rtmtype_str2idx(*argv);
        }
        break;
      case 10:
        show_iprule_help();
        break; // Unreachable code.
      default:
        error_exit("Invalid argument '%s'", *argv);
        break; // Unreachable code.
    }
  }

  if (!request.msg.rtm_family) request.msg.rtm_family = AF_INET;
  if (!tflag && opt == RTM_NEWRULE) request.msg.rtm_table = RT_TABLE_MAIN;

  send_nlmesg(0, 0, 0, &request, sizeof(request));
  return (filter_nlmesg(NULL, NULL));
}

static int show_rules(struct nlmsghdr *mhdr,
    char **argv __attribute__ ((__unused__)))
{
  struct rtmsg *msg = NLMSG_DATA(mhdr);
  struct rtattr *rta, *attr[RTA_MAX+1] = {0,};
  int32_t tvar, msglen = mhdr->nlmsg_len - NLMSG_LENGTH(sizeof(struct rtmsg));
  int hlen = ((msg->rtm_family == AF_INET) ? 32
      : ((msg->rtm_family == AF_INET6) ? 128 : -1));

  if (mhdr->nlmsg_type != RTM_NEWRULE) return 0;
  if (msglen < 0) return 1;

  tvar = msglen;
  for (rta = RTM_RTA(msg); RTA_OK(rta, tvar); rta=RTA_NEXT(rta, tvar))
    if (rta->rta_type <= RTA_MAX) attr[rta->rta_type] = rta;

  if (tvar) error_msg("deficit %d, rtalen = %d!", tvar, rta->rta_len);

  printf("%u:\tfrom ", attr[RTA_PRIORITY] ?
      *(unsigned *)RTA_DATA(attr[RTA_PRIORITY]) : 0);

  if (attr[RTA_SRC]) {
    printf("%s", (msg->rtm_family == AF_INET || msg->rtm_family == AF_INET6)
        ? inet_ntop(msg->rtm_family, RTA_DATA(attr[RTA_SRC]),
          toybuf, sizeof(toybuf))
        : "???");
    (msg->rtm_src_len != hlen) ? printf("/%u", msg->rtm_src_len) : 0;
  } else msg->rtm_src_len ? printf("0/%d", msg->rtm_src_len) : printf("all");

  xputc(' ');
  if (attr[RTA_DST]) {
    printf("to %s", (msg->rtm_family == AF_INET || msg->rtm_family == AF_INET6)
        ? inet_ntop(msg->rtm_family, RTA_DATA(attr[RTA_DST]),
          toybuf, sizeof(toybuf))  : "???");
    (msg->rtm_dst_len != hlen) ? printf("/%u", msg->rtm_dst_len) : xputc(' ');
  } else if (msg->rtm_dst_len)
    printf("to 0/%d ", msg->rtm_dst_len);

  if (msg->rtm_tos)
    printf("tos %s ", namefromRPDB(msg->rtm_tos, RPDB_rtdsfield));

  if (attr[RTA_PROTOINFO])
    printf("fwmark %#x ", *(uint32_t*)RTA_DATA(attr[RTA_PROTOINFO]));

  if (attr[RTA_IIF]) printf("iif %s ", (char*)RTA_DATA(attr[RTA_IIF]));

  if (msg->rtm_table)
    printf("lookup %s ", namefromRPDB(msg->rtm_table, RPDB_rttables));

  if (attr[RTA_FLOW]) {
    u_int32_t from, to = *(u_int32_t *)RTA_DATA(attr[RTA_FLOW]);
    char *format = "realms %s/";

    to = (from = (to >> 16)) & 0xFFFF;
    format = (from ? format: "%s");
    printf(format, namefromRPDB((from ? from : to), RPDB_rtrealms));
  }

  if (msg->rtm_type == RTN_NAT) {
    if (!attr[RTA_GATEWAY]) printf("masquerade");
    else printf("map-to %s ", inet_ntop(msg->rtm_family,
          RTA_DATA(attr[RTA_GATEWAY]), toybuf, sizeof(toybuf)));
  } else if (msg->rtm_type != RTN_UNICAST)
    printf("%s", rtmtype_idx2str(msg->rtm_type));

  xputc('\n');
  return 0;
}

static int rulelist(char **argv)
{
  if (*argv) {
    error_msg("'ip rule show' does not take any arguments.");
    return 1;
  }
  send_nlmesg(RTM_GETRULE, NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST,
      ((TT.addressfamily != AF_UNSPEC) ? TT.addressfamily : AF_INET), NULL, 0);
  return filter_nlmesg(show_rules, argv);
}

static int iprule(char **argv)
{
  int idx;
  struct arglist options[] = {{"add", 0}, {"delete", 0}, {"list", 1},
    {"show", 1}, {NULL, -1}};
  cmdobj ipcmd, cmdobjlist[] = {ruleupdate, rulelist};

  if (!*argv) idx = 1;
  else if ((idx = substring_to_idx(*argv++, options)) == -1)
    show_iprule_help();
  ipcmd = cmdobjlist[idx];
  return ipcmd(argv);
}
//============================================================================
// code for ip tunnel.
//============================================================================
static void show_iptunnel_help(void)
{
  error_exit("usage: iptunnel { add | change | del | show } [NAME]\n"
    "           [mode { ipip | gre | sit }] [remote ADDR] [local ADDR]\n"
    "           [[i|o]seq] [[i|o]key KEY] [[i|o]csum] [ttl TTL]\n"
    "           [tos TOS] [[no]pmtudisc] [dev PHYS_DEV]");
}

static int tnl_ioctl(char *dev, int rtype, struct ip_tunnel_parm *ptnl)
{
  struct ifreq req;
  int fd, ret = 0;

  if ((rtype == SIOCCHGTUNNEL || rtype == SIOCDELTUNNEL) && *ptnl->name)
    xstrncpy(req.ifr_name, ptnl->name, IF_NAMESIZE);
  else xstrncpy(req.ifr_name, dev, IF_NAMESIZE);

  if (rtype != SIOCGIFHWADDR) req.ifr_ifru.ifru_data = (void*)ptnl;
  fd = xsocket(AF_INET, SOCK_DGRAM, 0);

  if (rtype == SIOCGETTUNNEL) ret = ioctl(fd, rtype, &req);
  else if (rtype == SIOCGIFHWADDR)
    ret = (ioctl(fd, rtype, &req) < 0) ? -1 : req.ifr_addr.sa_family;
  else xioctl(fd, rtype, &req);

  close(fd);
  return ret;
}

static int display_tunnel(struct ip_tunnel_parm *ptnl)
{
  char rmt_addr[64], lcl_addr[64], ikey_str[64], okey_str[64];

  printf("%s: %s/ip", ptnl->name, ptnl->iph.protocol == IPPROTO_IPIP ? "ip" :
      (ptnl->iph.protocol == IPPROTO_GRE ? "gre" :
       (ptnl->iph.protocol == IPPROTO_IPV6 ? "ipv6" : "unknown")));
  printf("  remote %s  local %s ", ptnl->iph.daddr ?
      inet_ntop(AF_INET, &ptnl->iph.daddr, rmt_addr, sizeof(rmt_addr)) : "any",
      ptnl->iph.saddr ? inet_ntop(AF_INET, &ptnl->iph.saddr, lcl_addr,
        sizeof(lcl_addr)) : "any");
  if (ptnl->link) {
    struct ifreq req;
    int fd;

    req.ifr_ifindex = ptnl->link;
    fd = xsocket(AF_INET, SOCK_DGRAM, 0);
    if (ioctl(fd, SIOCGIFNAME, &req) < 0) perror_msg("SIOCGIFNAME");
    else printf(" dev %s ", req.ifr_name);
    close(fd);
  }
  if (ptnl->iph.ttl) printf(" ttl %d ", ptnl->iph.ttl);
  else printf(" ttl inherit ");

  if (ptnl->iph.tos) {
    printf(" tos");
    if (ptnl->iph.tos & 1) printf(" inherit");
    if (ptnl->iph.tos & ~1) printf("%c%s ", ptnl->iph.tos & 1 ? '/' : ' ',
        namefromRPDB((ptnl->iph.tos & ~1), RPDB_rtdsfield));
  }
  if (!(ptnl->iph.frag_off & htons(IP_DF))) printf(" nopmtudisc");
  inet_ntop(AF_INET, &ptnl->i_key, ikey_str, sizeof(ikey_str));
  if ((ptnl->i_flags & GRE_KEY) && (ptnl->o_flags & GRE_KEY)
      && ptnl->o_key == ptnl->i_key) printf(" key %s", ikey_str);
  else if ((ptnl->i_flags | ptnl->o_flags) & GRE_KEY) {
    inet_ntop(AF_INET, &ptnl->o_key, okey_str, sizeof(okey_str));
    if (ptnl->i_flags & GRE_KEY) printf(" ikey %s ", ikey_str);
    if (ptnl->o_flags & GRE_KEY) printf(" okey %s ", okey_str);
  }
  if (ptnl->i_flags & GRE_SEQ) printf("\n  Drop packets out of sequence.\n");
  if (ptnl->i_flags & GRE_CSUM)
    printf("\n  Checksum in received packet is required.");
  if (ptnl->o_flags & GRE_SEQ) printf("\n  Sequence packets on output.");
  if (ptnl->o_flags & GRE_CSUM) printf("\n  Checksum output packets.");
  xputc('\n');
  return 0;
}

static int read_tunnel(struct ip_tunnel_parm *ptnl)
{
  int count = 0;
  char iface[IF_NAMESIZE];
  struct ip_tunnel_parm iptnl;
  FILE *fp = xfopen("/proc/net/dev", "r");

  while (fgets(toybuf, sizeof(toybuf), fp)) {
    char *ptr;
    int ret;

    if (count++ < 2) continue; // 1st two lines are header.

    ptr = strchr(toybuf, ':');
    if (!ptr || (*ptr++ = 0, sscanf(toybuf, "%s", iface) != 1))
      error_exit("invalid format of '/proc/net/dev'");
    if (*ptnl->name && strcmp(ptnl->name, iface)) continue;
    if ((ret = tnl_ioctl(iface, SIOCGIFHWADDR, &iptnl)) < 0) {
      error_msg("failed to get type of '%s'", iface);
      continue;
    }
    if (ret != ARPHRD_TUNNEL && ret !=  ARPHRD_SIT &&
        ret != ARPHRD_IPGRE) continue;

    memset(&iptnl, 0, sizeof(iptnl));
    if (tnl_ioctl(iface, SIOCGETTUNNEL, &iptnl) < 0) continue;
    if ((ptnl->link && iptnl.link != ptnl->link) || (*ptnl->name &&
          strcmp(iptnl.name, ptnl->name)) || (ptnl->iph.daddr &&
          iptnl.iph.daddr != ptnl->iph.daddr) || (ptnl->iph.saddr &&
            iptnl.iph.saddr != ptnl->iph.saddr) || (ptnl->i_key &&
              iptnl.i_key != ptnl->i_key)) continue;
    display_tunnel(&iptnl);
  }
  fclose(fp);
  return 0;
}

static void parse_iptunnel_args(struct ip_tunnel_parm *ptnl, char **argv,
    int ipt_opt_idx)
{
  int idx;
  uint8_t af = AF_INET;
  uint32_t addr = 0;
  struct arglist opts[] = { {"mode", 0}, {"key", 1}, {"ikey", 2},
    {"okey", 3}, {"seq", 4}, {"iseq", 5}, {"oseq", 6}, {"csum", 7},
    {"icsum", 8}, {"ocsum", 9}, {"nopmtudisc", 10}, {"pmtudisc", 11},
    {"remote", 12}, {"local", 13},{"dev", 14}, {"ttl", 15}, {"tos", 16},
    {"dsfield", 17}, {"name", 18}, {NULL, -1}
  };

  ptnl->iph.version = 4; // The value indicates the version of IP (4 or 6)
  ptnl->iph.ihl = 5; // Minimum Internet Header Length
  // frag_off is measured in units of 8 octets (64 bits)
  ptnl->iph.frag_off = htons(IP_DF);
  if (*argv && ipt_opt_idx <= 2 && string_to_idx(*argv, opts) == -1) {
    xstrncpy(ptnl->name, *argv, IF_NAMESIZE);
    if (ipt_opt_idx == 1) {
      struct ip_tunnel_parm iptnl_old;

      memset(&iptnl_old, 0, sizeof(iptnl_old));
      tnl_ioctl(ptnl->name, SIOCGETTUNNEL, &iptnl_old);
      *ptnl = iptnl_old;
    }
    argv++;
  }
  for (; *argv; argv++, addr = 0) {
    switch (idx = string_to_idx(*argv, opts)) {
      case 0:
        if (!*++argv) error_exit("mode is missing");
        if ((!strcmp("ipip", *argv) || !strcmp("ip/ip", *argv)))
          ptnl->iph.protocol = IPPROTO_IPIP;
        else if ((!strcmp("gre", *argv) || !strcmp("gre/ip", *argv)))
          ptnl->iph.protocol = IPPROTO_GRE;
        else if ((!strcmp("sit", *argv) || !strcmp("ipv6/ip", *argv)))
          ptnl->iph.protocol = IPPROTO_IPV6;
        else show_iptunnel_help();
        break;
      case 1:
      case 2:
      case 3:
        {
          struct addrinfo *info, hint;
          int ret;

          if (!*++argv) error_exit("key value is missing");
          memset(&hint, 0, sizeof(hint));
          hint.ai_family = AF_INET;
          ret = getaddrinfo(*argv, NULL, &hint, &info);
          if (ret || !info) error_exit("invalid argument to key");
          freeaddrinfo(info);

          if (strchr(*argv, '.')) {
            if (get_prefix(&addr, &af, *argv, AF_INET))
              error_exit("invalid key '%s'", *argv);
          } else {
            unsigned key_val;

            sscanf(*argv, "%u", &key_val);
            addr = htonl(key_val);
          }
          if (idx == 1) {
            ptnl->i_flags |= GRE_KEY;
            ptnl->o_flags |= GRE_KEY;
            ptnl->i_key = ptnl->o_key = addr;
          } else if (idx == 2) {
            ptnl->i_flags |= GRE_KEY;
            ptnl->i_key = addr;
          } else {
            ptnl->o_flags |= GRE_KEY;
            ptnl->o_key = addr;
          }
        }
        break;
      case 4:
        ptnl->i_flags |= GRE_SEQ;
        ptnl->o_flags |= GRE_SEQ;
        break;
      case 5:
        ptnl->i_flags |= GRE_SEQ;
        break;
      case 6:
        ptnl->o_flags |= GRE_SEQ;
        break;
      case 7:
        ptnl->i_flags |= GRE_CSUM;
        ptnl->o_flags |= GRE_CSUM;
        break;
      case 8:
        ptnl->i_flags |= GRE_CSUM;
        break;
      case 9:
        ptnl->o_flags |= GRE_CSUM;
        break;
      case 10:
        ptnl->iph.frag_off = 0;
        break;
      case 11:
        ptnl->iph.frag_off = htons(IP_DF);
        break;
      case 12:
      case 13:
        if (!*++argv) error_exit("remote/local address is missing");
        if (get_prefix(&addr, &af, *argv, AF_INET))
          error_exit("invalid remote/local address '%s'", *argv);
        (idx == 12) ? (ptnl->iph.daddr = addr) : (ptnl->iph.saddr = addr);
        break;
      case 14:
        if (!*++argv) error_exit("device name is missing");
        else {
          struct ifreq req;
          int fd;

          xstrncpy(req.ifr_name, *argv, IFNAMSIZ);
          fd = xsocket(AF_INET, SOCK_DGRAM, 0);
          xioctl(fd, SIOCGIFINDEX, &req);
          close(fd);
          ptnl->link = req.ifr_ifindex;
        }
        break;
      case 15:
        if (!*++argv) error_exit("ttl value is missing");
        if (strcmp(*argv, "inherit"))
          ptnl->iph.ttl = atolx_range(*argv, 0, 255);
        break;
      case 16:
      case 17:
        if (!*++argv) error_exit("tos value is missing");
        if (strcmp(*argv, "inherit")) {
          char *ptr;
          unsigned long tval = strtoul(*argv, &ptr, 16);

          if (tval > 255) error_exit("invalid tos value '%s'", *argv);
          if (*ptr) {
            int ret;

            if ((ret = idxfromRPDB(*argv, RPDB_rtdsfield)) < 0)
              error_exit("invalid tos value");
            ptnl->iph.tos = ret;
          } else ptnl->iph.tos = tval;
        } else ptnl->iph.tos = 1;
        break;
      case 18:
        if (*ptnl->name) error_exit("invalid tunnel");
        else {
          if (!*++argv) error_exit("name is missing");
          xstrncpy(ptnl->name, *argv, IF_NAMESIZE);
        }
        break;
      default:
        if (*ptnl->name) error_exit("invalid tunnel");
        xstrncpy(ptnl->name, *argv, IF_NAMESIZE);
        break;
    }
  }
  if (ptnl->iph.protocol == IPPROTO_IPIP ||
      ptnl->iph.protocol == IPPROTO_IPV6) {
    if ((ptnl->i_flags & GRE_KEY) || (ptnl->o_flags & GRE_KEY))
      error_exit("[i|o]key is allowed with gre only");
    if ((ptnl->i_flags & GRE_SEQ) || (ptnl->o_flags & GRE_SEQ))
      error_exit("[i|o]seq is allowed with gre only");
    if ((ptnl->i_flags & GRE_CSUM) || (ptnl->o_flags & GRE_CSUM))
      error_exit("[i|o]csum is allowed with gre only");
  }
  if (!ptnl->i_key && IN_MULTICAST(ntohl(ptnl->iph.daddr))) {
    ptnl->i_key = ptnl->iph.daddr;
    ptnl->i_flags |= GRE_KEY;
  }
  if (!ptnl->o_key && IN_MULTICAST(ntohl(ptnl->iph.daddr))) {
    ptnl->o_key = ptnl->iph.daddr;
    ptnl->o_flags |= GRE_KEY;
  }
  if (IN_MULTICAST(ntohl(ptnl->iph.daddr)) && !ptnl->iph.saddr)
    error_exit("broadcast tunnel requires a source address");
}

static int tunnellist(char **argv)
{
  struct ip_tunnel_parm iptnl;
  int ret = 0;

  memset(&iptnl, 0, sizeof(iptnl));
  parse_iptunnel_args(&iptnl, argv, 3);

  if (iptnl.iph.protocol == IPPROTO_IPIP)
    ret = tnl_ioctl(*iptnl.name ? iptnl.name : "tunl0", SIOCGETTUNNEL, &iptnl);
  else if (iptnl.iph.protocol == IPPROTO_GRE)
    ret = tnl_ioctl(*iptnl.name ? iptnl.name : "gre0", SIOCGETTUNNEL, &iptnl);
  else if (iptnl.iph.protocol == IPPROTO_IPV6)
    ret = tnl_ioctl(*iptnl.name ? iptnl.name : "sit0", SIOCGETTUNNEL, &iptnl);
  else return read_tunnel(&iptnl);

  if (ret < 0) {
    perror_msg("SIOCGETTUNNEL");
    return ret;
  } else return display_tunnel(&iptnl);
}

// Performing add, change, & delete tunnel action, according to passed req_type
static int tunnelupdate(char **argv)
{
  struct ip_tunnel_parm iptnl;
  int idx = 2, rtype = SIOCDELTUNNEL;

  if (*argv[-1] == 'a') {
    idx = 0;
    rtype = SIOCADDTUNNEL;
  } else if (*argv[-1] == 'c') {
    idx = 1;
    rtype = SIOCCHGTUNNEL;
  }

  memset(&iptnl, 0, sizeof(iptnl));
  parse_iptunnel_args(&iptnl, argv, idx);
  if (idx != 2 && iptnl.iph.ttl && !(iptnl.iph.frag_off))
    error_exit("ttl > 0 and nopmtudisc are incompatible");
  if (iptnl.iph.protocol == IPPROTO_IPIP)
    return (tnl_ioctl("tunl0", rtype, &iptnl) < 0) ? 1 : 0;
  else if (iptnl.iph.protocol == IPPROTO_GRE)
    return (tnl_ioctl("gre0", rtype, &iptnl) < 0) ? 1 : 0;
  else if (iptnl.iph.protocol == IPPROTO_IPV6)
    return (tnl_ioctl("sit0", rtype, &iptnl) < 0) ? 1 : 0;
  else {
    if (idx != 2) error_exit("invalid tunnel mode");
    return (tnl_ioctl(iptnl.name, rtype, &iptnl) < 0) ? 1 : 0;
  }
}

static int iptunnel(char **argv)
{
  int idx;
  struct arglist opts[] = {{"add", 0}, {"change", 0}, {"del", 0},
    {"delete", 0}, {"show", 1}, {"list", 1}, {"lst", 1}, {NULL, -1}
  };
  cmdobj ipcmd, cmdobjlist[] = {tunnelupdate, tunnellist};

  if (!*argv) idx = 1;
  else if ((idx = substring_to_idx(*argv++, opts)) == -1)
    show_iptunnel_help();
  ipcmd = cmdobjlist[idx];
  return ipcmd(argv);
}

// ===========================================================================
// Common code, which is used for all ip options.
// ===========================================================================

// Parse netlink messages and call input callback handler for action
static int filter_nlmesg(int (*fun)(struct nlmsghdr *mhdr, char **argv),
    char **argv)
{
  while (1) {
    struct nlmsghdr *mhdr;
    int msglen = recv(TT.sockfd, TT.gbuf, MESG_LEN, 0);

    if ((msglen < 0) && (errno == EINTR || errno == EAGAIN)) continue;
    else if (msglen < 0) {
      error_msg("netlink receive error %s", strerror(errno));
      return 1;
    } else if (!msglen) {
      error_msg("EOF on netlink");
      return 1;
    }

    for (mhdr = (struct nlmsghdr*)TT.gbuf; NLMSG_OK(mhdr, msglen);
        mhdr = NLMSG_NEXT(mhdr, msglen)) {
      int err;
      if (mhdr->nlmsg_pid != getpid())
        continue;
      switch (mhdr->nlmsg_type) {
        case NLMSG_DONE:
          return 0;
        case NLMSG_ERROR:
          {
            struct nlmsgerr *merr = (struct nlmsgerr*)NLMSG_DATA(mhdr);

            if (merr->error == 0) return 0;
            if (mhdr->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr)))
              error_msg("ERROR truncated");
            else {
              errno = -merr->error;
              perror_msg("RTNETLINK answers");
            }
            return 1;
          }
        default:
          if (fun && (err = fun(mhdr, argv))) return err;
          break;
      }
    } // End of for loop.
  } // End of while loop.
  return 0;
}

void ip_main(void)
{
  char **optargv = toys.argv;
  int idx, isip = !(toys.which->name[2]); //1 -> if only ip
  cmdobj ipcmd, cmdobjlist[] = {ipaddr, iplink, iproute, iprule, iptunnel};

  for (++optargv; *optargv; ++optargv) {
    char *ptr = *optargv;
    struct arglist ip_options[] = {{"oneline", 0}, {"family",  1},
      {"4", 1}, {"6", 1}, {"0", 1}, {"stats", 2}, {NULL, -1}};

    if (*ptr != '-') break;
    else if ((*(ptr+1) == '-') && (*(ptr+2))) ptr +=2;
    //escape "--" and stop ip arg parsing.
    else if ((*(ptr+1) == '-') && (!*(ptr+2))) {
      *ptr +=1;
      break;
    } else ptr +=1;
    switch (substring_to_idx(ptr, ip_options)) {
      case 0: TT.singleline = 1;
              break;
      case 1: {
                if (isdigit(*ptr)) {
                  long num = atolx(ptr);
                  if (num == 4) TT.addressfamily  = AF_INET;
                  else if (num == 6) TT.addressfamily  = AF_INET6;
                  else TT.addressfamily = AF_PACKET;
                } else {
                  struct arglist ip_aflist[] = {{"inet", AF_INET},
                    {"inet6", AF_INET6}, {"link", AF_PACKET}, {NULL, -1}};

                  if (!*++optargv) help_exit(0);
                  if ((TT.addressfamily = string_to_idx(*optargv, ip_aflist)) == -1)
                    error_exit("wrong family '%s'", *optargv);
                }
              }
              break;
      case 2:
              TT.stats++;
              break;
      default: help_exit(0);
               break; // unreachable code.
    }
  }

  TT.sockfd = xsocket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

  if (isip) {// only for ip
    if (*optargv) {
      struct arglist ip_objectlist[] = { {"address", 0}, {"link", 1},
        {"route", 2}, {"rule", 3}, {"tunnel", 4}, {"tunl", 4}, {NULL, -1}};

      if ((idx = substring_to_idx(*optargv, ip_objectlist)) == -1) help_exit(0);
      ipcmd = cmdobjlist[idx];
      toys.exitval = ipcmd(++optargv);
    } else help_exit(0);
  } else {
    struct arglist ip_objectlist[] = { {"ipaddr", 0}, {"iplink", 1},
      {"iproute", 2}, {"iprule", 3}, {"iptunnel", 4}, {NULL, -1}};
    if ((idx = string_to_idx(toys.which->name, ip_objectlist)) == -1)
      help_exit(0);
    ipcmd = cmdobjlist[idx];
    toys.exitval = ipcmd(optargv);
  }
  xclose(TT.sockfd);
  if (rtdsfield_init) free_alist(rt_dsfield);
  if (rtrealms_init) free_alist(rt_realms);
  if (rtscope_init) free_alist(rt_scope);
  if (rttable_init) free_alist(rt_tables);
  if (rtprotos_init) free_alist(rt_protos);
}
