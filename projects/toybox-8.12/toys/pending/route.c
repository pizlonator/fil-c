/* route.c - Display/edit network routing table.
 *
 * Copyright 2012 Ranjan Kumar <ranjankumar.bth@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 * Copyright 2020 Eric Molitor <eric@molitor.org>
 *
 * No Standard
 *
 * route add -net target 10.0.0.0 netmask 255.0.0.0 dev eth0
 * route del delete
 * delete net route, must match netmask, informative error message
 *
 * mod dyn reinstate metric netmask gw mss window irtt dev

USE_ROUTE(NEWTOY(route, "?neA:", TOYFLAG_SBIN))
config ROUTE
  bool "route"
  default n
  help
    usage: route [-ne] [-A [inet|inet6]] [add|del TARGET [OPTIONS]]

    Display, add or delete network routes in the "Forwarding Information Base",
    which send packets out a network interface to an address.

    -n	Show numerical addresses (no DNS lookups)
    -e	display netstat fields

    Assigning an address to an interface automatically creates an appropriate
    network route ("ifconfig eth0 10.0.2.15/8" does "route add 10.0.0.0/8 eth0"
    for you), although some devices (such as loopback) won't show it in the
    table. For machines more than one hop away, you need to specify a gateway
    (ala "route add default gw 10.0.2.2").

    The address "default" is a wildcard address (0.0.0.0/0) matching all
    packets without a more specific route.

    Available OPTIONS include:
    reject   - blocking route (force match failure)
    dev NAME - force matching packets out this interface (ala "eth0")
    netmask  - old way of saying things like ADDR/24
    gw ADDR  - forward packets to gateway ADDR
*/

#define FOR_route
#include "toys.h"
#define _LINUX_SYSINFO_H     // workaround for musl bug
#include <linux/rtnetlink.h>

GLOBALS(
  char *A;
)

struct _arglist {
  char *arg;
  int action;
};

static struct _arglist arglist1[] = {
  { "add", 1 }, { "del", 2 },
  { "delete", 2 }, { NULL, 0 }
};

static struct _arglist arglist2[] = {
  { "-net", 1 }, { "-host", 2 },
  { NULL, 0 }
};

void xsend(int sockfd, void *buf, size_t len)
{
  if (send(sockfd, buf, len, 0) != len) perror_exit("xsend");
}

int xrecv(int sockfd, void *buf, size_t len)
{
  int msg_len = recv(sockfd, buf, len, 0);
  if (msg_len < 0) perror_exit("xrecv");

  return msg_len;
}

void addAttr(struct nlmsghdr *nl, int maxlen, void *attr, int type, int len)
{
  struct rtattr *rt;
  int rtlen = RTA_LENGTH(len);
  if (NLMSG_ALIGN(nl->nlmsg_len) + rtlen > maxlen) perror_exit("addAttr");
  rt = (struct rtattr*)((char *)nl + NLMSG_ALIGN(nl->nlmsg_len));
  rt->rta_type = type;
  rt->rta_len = rtlen;
  memcpy(RTA_DATA(rt), attr, len);
  nl->nlmsg_len = NLMSG_ALIGN(nl->nlmsg_len) + rtlen;
}

static void get_hostname(sa_family_t f, void *a, char *dst, size_t len) {
  size_t a_len = (AF_INET6 == f) ? sizeof(struct in6_addr) : sizeof(struct in_addr);

  struct hostent *host = gethostbyaddr(a, a_len, f);
  if (host) xstrncpy(dst, host->h_name, len);
}

static void display_routes(sa_family_t f)
{
  int fd, msg_hdr_len, route_protocol;
  struct {
    struct nlmsghdr nl;
    struct rtmsg rt;
  } req;
  struct nlmsghdr buf[8192 / sizeof(struct nlmsghdr)];
  struct nlmsghdr *msg_hdr_ptr;
  struct rtmsg *route_entry;
  struct rtattr *rteattr;

  fd = xsocket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

  memset(&req, 0, sizeof(req));
  req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  req.nl.nlmsg_type = RTM_GETROUTE;
  req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  req.nl.nlmsg_pid = getpid();
  req.nl.nlmsg_seq = 1;
  req.rt.rtm_family = f;
  req.rt.rtm_table = RT_TABLE_MAIN;
  xsend(fd, &req, sizeof(req));

  if (f == AF_INET) {
    xprintf("Kernel IP routing table\n"
            "Destination     Gateway         Genmask         Flags %s Iface\n",
            FLAG(e) ? "  MSS Window  irtt" : "Metric Ref    Use");
  } else {
    xprintf("Kernel IPv6 routing table\n"
            "%-31s%-26s Flag Metric Ref Use If\n", "Destination", "Next Hop");
  }

  msg_hdr_len = xrecv(fd, buf, sizeof(buf));
  msg_hdr_ptr = buf;
  while (msg_hdr_ptr->nlmsg_type != NLMSG_DONE) {
    while (NLMSG_OK(msg_hdr_ptr, msg_hdr_len)) {
      route_entry = NLMSG_DATA(msg_hdr_ptr);
      route_protocol = route_entry->rtm_protocol;

      // Annoyingly NLM_F_MATCH is not yet implemented so even if we pass in
      // RT_TABLE_MAIN with RTM_GETROUTE it still returns everything so we
      // have to filter here.
      if (route_entry->rtm_table == RT_TABLE_MAIN) {
        int route_attribute_len;
        char dest[INET6_ADDRSTRLEN], gate[INET6_ADDRSTRLEN], netmask[32],
             flags[10] = "U", if_name[IF_NAMESIZE] = "-";
        unsigned priority = 0, mss = 0, win = 0, irtt = 0, ref = 0, use = 0,
                 route_netmask, metric_len;
        struct in_addr netmask_addr;
        struct rtattr *metric;
        struct rta_cacheinfo *cache_info;

        if (f == AF_INET) {
          strcpy(dest, FLAG(n) ? "0.0.0.0" : "default");
          strcpy(gate, FLAG(n) ? "*" : "0.0.0.0");
          strcpy(netmask, "0.0.0.0");
        } else {
          strcpy(dest, "::");
          strcpy(gate, "::");
        }

        route_netmask = route_entry->rtm_dst_len;
        if (route_netmask == 0) netmask_addr.s_addr = ~((in_addr_t) -1);
        else netmask_addr.s_addr = htonl(~((1 << (32 - route_netmask)) - 1));
        inet_ntop(AF_INET, &netmask_addr, netmask, sizeof(netmask));

        rteattr = RTM_RTA(route_entry);
        route_attribute_len = RTM_PAYLOAD(msg_hdr_ptr);
        while (RTA_OK(rteattr, route_attribute_len)) {
          switch (rteattr->rta_type) {
            case RTA_DST:
              if (FLAG(n)) inet_ntop(f, RTA_DATA(rteattr), dest, sizeof(dest));
              else get_hostname(f, RTA_DATA(rteattr), dest, sizeof(dest));
              break;

            case RTA_GATEWAY:
              if (FLAG(n)) inet_ntop(f, RTA_DATA(rteattr), gate, sizeof(dest));
              else get_hostname(f, RTA_DATA(rteattr), gate, sizeof(dest));
              strcat(flags, "G");
              break;

            case RTA_PRIORITY:
              priority = *(unsigned *)RTA_DATA(rteattr);
              break;

            case RTA_OIF:
              if_indextoname(*(int *)RTA_DATA(rteattr), if_name);
              break;

            case RTA_METRICS:
              metric_len = RTA_PAYLOAD(rteattr);
              for (metric = RTA_DATA(rteattr); RTA_OK(metric, metric_len);
                   metric = RTA_NEXT(metric, metric_len))
                if (metric->rta_type == RTAX_ADVMSS)
                  mss = *(unsigned *)RTA_DATA(metric);
                else if (metric->rta_type == RTAX_WINDOW)
                  win = *(unsigned *)RTA_DATA(metric);
                else if (metric->rta_type == RTAX_RTT)
                  irtt = (*(unsigned *)RTA_DATA(metric))/8;
              break;

            case RTA_CACHEINFO:
              cache_info = RTA_DATA(rteattr);
              ref = cache_info->rta_clntref;
              use = cache_info->rta_used;
              break;
          }

          rteattr = RTA_NEXT(rteattr, route_attribute_len);
        }

        if (route_entry->rtm_type == RTN_UNREACHABLE) flags[0] = '!';
        if (route_netmask == 32) strcat(flags, "H");
        if (route_protocol == RTPROT_REDIRECT) strcat(flags, "D");

        if (f == AF_INET) {
          xprintf("%-15.15s %-15.15s %-16s%-6s", dest, gate, netmask, flags);
          if (FLAG(e)) xprintf("%5d %-5d %6d %s\n", mss, win, irtt, if_name);
          else xprintf("%-6d %-2d %7d %s\n", priority, ref, use, if_name);
        } else {
          char *dest_with_mask = xmprintf("%s/%u", dest, route_netmask);
          xprintf("%-30s %-26s %-4s %-6d %-4d %2d %-8s\n",
                  dest_with_mask, gate, flags, priority, ref, use, if_name);
          free(dest_with_mask);
        }
      }
      msg_hdr_ptr = NLMSG_NEXT(msg_hdr_ptr, msg_hdr_len);
    }

    msg_hdr_len = xrecv(fd, buf, sizeof(buf));
    msg_hdr_ptr = buf;
  }

  xclose(fd);
}

// find parameter (add/del/net/host) in list, return appropriate action or 0.
static int get_action(char ***argv, struct _arglist *list)
{
  struct _arglist *alist;

  if (!**argv) return 0;
  for (alist = list; alist->arg; alist++) { //find the given parameter in list
    if (!strcmp(**argv, alist->arg)) {
      *argv += 1;
      return alist->action;
    }
  }
  return 0;
}

// add/del a route.
static void setroute(sa_family_t f, char **argv)
{
  char *tgtip;
  int sockfd, arg2_action;
  int action = get_action(&argv, arglist1); //verify the arg for add/del.
  struct nlmsghdr buf[8192 / sizeof(struct nlmsghdr)];
  struct nlmsghdr *nlMsg;
  struct rtmsg *rtMsg;

  if (!action || !*argv) help_exit("setroute");
  arg2_action = get_action(&argv, arglist2); //verify the arg for -net or -host
  if (!*argv) help_exit("setroute");
  tgtip = *argv++;
  sockfd = xsocket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  memset(buf, 0, sizeof(buf));
  nlMsg = (struct nlmsghdr *) buf;
  rtMsg = (struct rtmsg *) NLMSG_DATA(nlMsg);

  nlMsg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));

  //TODO(emolitor): Improve action and arg2_action handling
  if (action == 1) { // Add
    nlMsg->nlmsg_type = RTM_NEWROUTE;
    nlMsg->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
  } else { // Delete
    nlMsg->nlmsg_type = RTM_DELROUTE;
    nlMsg->nlmsg_flags = NLM_F_REQUEST;
  }

  nlMsg->nlmsg_pid = getpid();
  nlMsg->nlmsg_seq = 1;
  rtMsg->rtm_family = f;
  rtMsg->rtm_table = RT_TABLE_UNSPEC;
  rtMsg->rtm_type = RTN_UNICAST;
  rtMsg->rtm_protocol = RTPROT_UNSPEC;
  rtMsg->rtm_flags = RTM_F_NOTIFY;
  rtMsg->rtm_dst_len = rtMsg->rtm_src_len = (f == AF_INET) ? 32 : 128;

  if (arg2_action == 2) rtMsg->rtm_scope = RT_SCOPE_HOST;

  size_t addr_len = sizeof(struct in_addr);
  if (f == AF_INET6) addr_len = sizeof(struct in6_addr);
  unsigned char addr[sizeof(struct in6_addr)] = {0,};

  for (; *argv; argv++) {
    if (!strcmp(*argv, "mod")) continue;
    else if (!strcmp(*argv, "dyn")) continue;
    else if (!strcmp(*argv, "reinstate")) continue;
    else if (!strcmp(*argv, "reject")) rtMsg->rtm_type = RTN_UNREACHABLE;
    else {
      if (!argv[1]) show_help(stdout, 1);

      if (!strcmp(*argv, "metric")) {
        unsigned int priority = atolx_range(argv[1], 0, UINT_MAX);
        addAttr(nlMsg, sizeof(toybuf), &priority, RTA_PRIORITY, sizeof(unsigned int));
      } else if (!strcmp(*argv, "netmask")) {
        uint32_t netmask;
        char *ptr;
        uint32_t naddr[4] = {0,};
        uint64_t plen;

        netmask = (f == AF_INET6) ? 128 : 32; // set default netmask
        plen = strtoul(argv[1], &ptr, 0);

        if (!ptr || ptr == argv[1] || *ptr || !plen || plen > netmask) {
          if (!inet_pton(f, argv[1], &naddr)) error_exit("invalid netmask");
          if (f == AF_INET) {
            uint32_t mask = htonl(*naddr), host = ~mask;
            if (host & (host + 1)) error_exit("invalid netmask");
            for (plen = 0; mask; mask <<= 1) ++plen;
            if (plen > 32) error_exit("invalid netmask");
          }
        }
        netmask = plen;
        rtMsg->rtm_dst_len = netmask;
      } else if (!strcmp(*argv, "gw")) {
        if (!inet_pton(f, argv[1], &addr)) error_exit("invalid gw");
        addAttr(nlMsg, sizeof(toybuf), &addr, RTA_GATEWAY, addr_len);
      } else if (!strcmp(*argv, "mss")) {
        // TODO(emolitor): Add RTA_METRICS support
        //set the TCP Maximum Segment Size for connections over this route.
        //rt->rt_mtu = atolx_range(argv[1], 64, 65536);
        //rt->rt_flags |= RTF_MSS;
      } else if (!strcmp(*argv, "window")) {
        // TODO(emolitor): Add RTA_METRICS support
        //set the TCP window size for connections over this route to W bytes.
        //rt->rt_window = atolx_range(argv[1], 128, INT_MAX); //win low
        //rt->rt_flags |= RTF_WINDOW;
      } else if (!strcmp(*argv, "irtt")) {
        // TODO(emolitor): Add RTA_METRICS support
        //rt->rt_irtt = atolx_range(argv[1], 0, INT_MAX);
        //rt->rt_flags |= RTF_IRTT;
      } else if (!strcmp(*argv, "dev")) {
        unsigned int if_idx = if_nametoindex(argv[1]);
        if (!if_idx) perror_exit("dev");
        addAttr(nlMsg, sizeof(toybuf), &if_idx, RTA_OIF, sizeof(unsigned int));
      } else help_exit("no '%s'", *argv);
      argv++;
    }
  }

  if (strcmp(tgtip, "default") != 0) {
    char *prefix = strtok(0, "/");

    if (prefix) rtMsg->rtm_dst_len = strtoul(prefix, &prefix, 0);
    if (!inet_pton(f, strtok(tgtip, "/"), &addr)) error_exit("invalid target");
    addAttr(nlMsg, sizeof(toybuf), &addr, RTA_DST, addr_len);
  } else rtMsg->rtm_dst_len = 0;

  xsend(sockfd, nlMsg, nlMsg->nlmsg_len);
  xclose(sockfd);
}

void route_main(void)
{
  if (!*toys.optargs) {
    if (!TT.A || !strcmp(TT.A, "inet")) display_routes(AF_INET);
    else if (!strcmp(TT.A, "inet6")) display_routes(AF_INET6);
    else show_help(stdout, 1);
  } else {
    if (!TT.A) {
      if (toys.optc>1 && strchr(toys.optargs[1], ':')) {
          xprintf("WARNING: Implicit IPV6 address using -Ainet6\n");
          TT.A = "inet6";
      } else TT.A = "inet";
    }

    if (!strcmp(TT.A, "inet")) setroute(AF_INET, toys.optargs);
    else setroute(AF_INET6, toys.optargs);
  }
}
