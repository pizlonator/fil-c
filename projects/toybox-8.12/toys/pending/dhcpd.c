/* dhcpd.c - DHCP server for dynamic network configuration.
 *
 * Copyright 2013 Madhur Verma <mad.flexi@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gamil.com>
 * Copyright 2015 Yeongdeok Suh <skyducks111@gmail.com>
 *
 * No Standard
USE_DHCPD(NEWTOY(dhcpd, ">1P#<0>65535fi:S46[!46]", TOYFLAG_SBIN|TOYFLAG_ROOTONLY))

config DHCPD
  bool "dhcpd"
  default n
  help
   usage: dhcpd [-46fS] [-i IFACE] [-P N] [CONFFILE]

    -f    Run in foreground
    -i Interface to use
    -S    Log to syslog too
    -P N  Use port N (default ipv4 67, ipv6 547)
    -4, -6    Run as a DHCPv4 or DHCPv6 server

config DEBUG_DHCP
  bool "debugging messeges ON/OFF"
  default n
  depends on DHCPD
*/

/*
 * TODO
 * - Working as an relay agent
 * - Rapid commit option support
 * - Additional packet options (commented on the middle of sources)
 * - Create common modules
 */

#define FOR_dhcpd

#include "toys.h"
#include <linux/sockios.h>
#include <linux/if_ether.h>

// Todo: headers not in posix
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>

#if CFG_DEBUG_DHCP==1
# define dbg(fmt, arg...)   printf(fmt, ##arg)
#else
# define dbg(fmt, arg...)
#endif

#define LOG_SILENT          0x0
#define LOG_CONSOLE         0x1
#define LOG_SYSTEM          0x2

#define DHCP_MAGIC          0x63825363

#define DHCPDISCOVER        1
#define DHCPOFFER           2
#define DHCPREQUEST         3
#define DHCPDECLINE         4
#define DHCPACK             5
#define DHCPNAK             6
#define DHCPRELEASE         7
#define DHCPINFORM          8

#define DHCP6SOLICIT        1
#define DHCP6ADVERTISE      2   // server -> client
#define DHCP6REQUEST        3
#define DHCP6CONFIRM        4
#define DHCP6RENEW          5
#define DHCP6REBIND         6
#define DHCP6REPLY          7   // server -> client
#define DHCP6RELEASE        8
#define DHCP6DECLINE        9
#define DHCP6RECONFIGURE    10  // server -> client
#define DHCP6INFOREQUEST    11
#define DHCP6RELAYFLOW      12  // relay -> relay/server
#define DHCP6RELAYREPLY     13  // server/relay -> relay

#define DHCP_NUM8           (1<<8)
#define DHCP_NUM16          (1<<9)
#define DHCP_NUM32          DHCP_NUM16 | DHCP_NUM8
#define DHCP_STRING         (1<<10)
#define DHCP_STRLST         (1<<11)
#define DHCP_IP             (1<<12)
#define DHCP_IPLIST         (1<<13)
#define DHCP_IPPLST         (1<<14)
#define DHCP_STCRTS         (1<<15)

// DHCP option codes (partial list). See RFC 2132 and
#define DHCP_OPT_PADDING                          0x00
#define DHCP_OPT_HOST_NAME          DHCP_STRING | 0x0c // either client informs server or server gives name to client
#define DHCP_OPT_REQUESTED_IP       DHCP_IP     | 0x32 // sent by client if specific IP is wanted
#define DHCP_OPT_LEASE_TIME         DHCP_NUM32  | 0x33
#define DHCP_OPT_OPTION_OVERLOAD                  0x34
#define DHCP_OPT_MESSAGE_TYPE       DHCP_NUM8   | 0x35
#define DHCP_OPT_SERVER_ID          DHCP_IP     | 0x36 // by default server's IP
#define DHCP_OPT_PARAM_REQ          DHCP_STRING | 0x37 // list of options client wants
#define DHCP_OPT_END                              0xff

// DHCPv6 option codes (partial). See RFC 3315
#define DHCP6_OPT_CLIENTID      1
#define DHCP6_OPT_SERVERID      2
#define DHCP6_OPT_IA_NA         3
#define DHCP6_OPT_IA_ADDR       5
#define DHCP6_OPT_ORO           6
#define DHCP6_OPT_PREFERENCE    7
#define DHCP6_OPT_ELAPSED_TIME  8
#define DHCP6_OPT_RELAY_MSG     9
#define DHCP6_OPT_STATUS_CODE   13
#define DHCP6_OPT_IA_PD         25
#define DHCP6_OPT_IA_PREFIX     26

#define DHCP6_STATUS_SUCCESS        0
#define DHCP6_STATUS_NOADDRSAVAIL   2

#define DHCP6_DUID_LLT    1
#define DHCP6_DUID_EN     2
#define DHCP6_DUID_LL     3
#define DHCP6_DUID_UUID   4

GLOBALS(
  char *i;
  long p;
)

struct config_keyword {
  char *keyword;
  int (*handler)(const char *str, void *var);
  void *var;
  char *def;
};

typedef struct dhcp_msg_s {
  char op, htype, hlen, hops;
  unsigned xid;
  unsigned short secs, flags;
  unsigned ciaddr, yiaddr, nsiaddr, ngiaddr;
  char chaddr[16], sname[64], file[128];
  unsigned cookie;
  char options[308];
} dhcp_msg_t;

typedef struct dhcp6_msg_s {
  char msgtype, transaction_id[3], options[524];
} dhcp6_msg_t;

typedef struct __attribute__((packed)) dhcp_raw_s {
  struct iphdr iph;
  struct udphdr udph;
  dhcp_msg_t dhcp;
} dhcp_raw_t;

typedef struct __attribute__((packed)) dhcp6_raw_s {
  struct ip6_hdr iph;
  struct udphdr udph;
  dhcp6_msg_t dhcp6;
} dhcp6_raw_t;

typedef struct static_lease_s {
  struct static_lease_s *next;
  unsigned nip;
  int mac[6];
} static_lease;

typedef struct static_lease6_s {
  struct static_lease6_s *next;
  unsigned short duid_len, ia_type;
  unsigned iaid;
  char nip6[16], duid[20];
} static_lease6;

typedef struct {
  unsigned expires, lease_nip;
  char lease_mac[6], hostname[20], pad[2];
} dyn_lease;

typedef struct {
  unsigned short duid_len, ia_type;
  unsigned expires, iaid;
  char lease_nip6[16], duid[20];
} dyn_lease6;

typedef struct option_val_s {
  char *key;
  unsigned short code;
  void *val;
  size_t len;
} option_val_t;

struct optval_duid_llt {
  unsigned short type, hwtype;
  unsigned time;
  char lladdr[];
};

struct optval_ia_na {
  unsigned iaid, t1, t2;
  char optval[];
};

struct optval_ia_addr {
  char ipv6_addr[16];
  unsigned pref_lifetime, valid_lifetime;
};
struct optval_status_code {
  unsigned short status_code;
  char status_msg[];
};

typedef struct __attribute__((__may_alias__)) server_config_s {
  char *interface;                // interface to use
  int ifindex;
  uint8_t server_nip6[16];
  uint32_t server_nip;
  uint32_t port;
  uint8_t server_mac[6];          // our MAC address (used only for ARP probing)
  void *options[256];             // list of DHCP options loaded from the config file
  /* start,end are in host order: we need to compare start <= ip <= end*/
  uint32_t start_ip;              // start address of leases, in host order
  uint32_t end_ip;                // end of leases, in host order
  uint8_t start_ip6[16];          // start address of leases, in IPv6 mode
  uint8_t end_ip6[16];            // end of leases, in IPv6 mode
  uint32_t max_lease_sec;         // maximum lease time (host order)
  uint32_t min_lease_sec;         // minimum lease time a client can request
  uint32_t max_leases;            // maximum number of leases (including reserved addresses)
  uint32_t auto_time;             // how long should dhcpd wait before writing a config file.
                                  // if this is zero, it will only write one on SIGUSR1
  uint32_t decline_time;          // how long an address is reserved if a client returns a
                                  // decline message
  uint32_t conflict_time;         // how long an arp conflict offender is leased for
  uint32_t offer_time;            // how long an offered address is reserved
  uint32_t siaddr_nip;            // "next server" bootp option
  char *lease_file;
  char *lease6_file;
  char *pidfile;
  char *notify_file;              // what to run whenever leases are written
  char *sname;                    // bootp server name
  char *boot_file;                // bootp boot file option
  uint32_t pref_lifetime;
  uint32_t valid_lifetime;
  uint32_t t1,t2;
  struct static_lease *static_leases; // List of ip/mac pairs to assign static leases
} server_config_t;

typedef struct __attribute__((__may_alias__)) server_state_s {
  uint8_t client_nip6[16];
  uint32_t client_port;
  uint8_t rqcode;
  int listensock;
  union {
    dhcp_msg_t rcvd_pkt;
    dhcp6_msg_t rcvd_pkt6;
  } rcvd;
  uint8_t* rqopt;
  union {
    dhcp_msg_t send_pkt;
    dhcp6_msg_t send_pkt6;
  } send;
  union {
    static_lease *sleases;
    static_lease6 *sleases6;
  } leases;
  struct arg_list *dleases;
} server_state_t;

static option_val_t options_list[] = {
    {"lease"          , DHCP_NUM32  | 0x33, NULL, 0},
    {"subnet"         , DHCP_IP     | 0x01, NULL, 0},
    {"broadcast"      , DHCP_IP     | 0x1c, NULL, 0},
    {"router"         , DHCP_IP     | 0x03, NULL, 0},
    {"ipttl"          , DHCP_NUM8   | 0x17, NULL, 0},
    {"mtu"            , DHCP_NUM16  | 0x1a, NULL, 0},
    {"hostname"       , DHCP_STRING | 0x0c, NULL, 0},
    {"domain"         , DHCP_STRING | 0x0f, NULL, 0},
    {"search"         , DHCP_STRLST | 0x77, NULL, 0},
    {"nisdomain"      , DHCP_STRING | 0x28, NULL, 0},
    {"timezone"       , DHCP_NUM32  | 0x02, NULL, 0},
    {"tftp"           , DHCP_STRING | 0x42, NULL, 0},
    {"bootfile"       , DHCP_STRING | 0x43, NULL, 0},
    {"bootsize"       , DHCP_NUM16  | 0x0d, NULL, 0},
    {"rootpath"       , DHCP_STRING | 0x11, NULL, 0},
    {"wpad"           , DHCP_STRING | 0xfc, NULL, 0},
    {"serverid"       , DHCP_IP     | 0x36, NULL, 0},
    {"message"        , DHCP_STRING | 0x38, NULL, 0},
    {"vlanid"         , DHCP_NUM32  | 0x84, NULL, 0},
    {"vlanpriority"   , DHCP_NUM32  | 0x85, NULL, 0},
    {"dns"            , DHCP_IPLIST | 0x06, NULL, 0},
    {"wins"           , DHCP_IPLIST | 0x2c, NULL, 0},
    {"nissrv"         , DHCP_IPLIST | 0x29, NULL, 0},
    {"ntpsrv"         , DHCP_IPLIST | 0x2a, NULL, 0},
    {"lprsrv"         , DHCP_IPLIST | 0x09, NULL, 0},
    {"swapsrv"        , DHCP_IP     | 0x10, NULL, 0},
    {"routes"         , DHCP_STCRTS | 0x21, NULL, 0},
    {"staticroutes"   , DHCP_STCRTS | 0x79, NULL, 0},
    {"msstaticroutes" , DHCP_STCRTS | 0xf9, NULL, 0},
};

struct fd_pair { int rd; int wr; };
static server_config_t gconfig;
static server_state_t gstate;
static uint8_t infomode;
static struct fd_pair sigfd;
static int constone = 1;
static sa_family_t addr_version = AF_INET;

// calculate options size.
static int dhcp_opt_size(uint8_t *optionptr)
{
  int i = 0;
  for(;optionptr[i] != 0xff; i++)
    if(optionptr[i] != 0x00) i += optionptr[i + 1] + 2 -1;
  return i;
}

// calculates checksum for dhcp messeges.
static uint16_t dhcp_checksum(void *addr, int count)
{
  int32_t sum = 0;
  uint16_t tmp = 0, *source = (uint16_t *)addr;

  while (count > 1)  {
    sum += *source++;
    count -= 2;
  }
  if (count > 0) {
    *(uint8_t*)&tmp = *(uint8_t*)source;
    sum += tmp;
  }
  while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
  return ~sum;
}

// gets information of INTERFACE and updates IFINDEX, MAC and IP
static int get_interface(const char *interface, int *ifindex, void *oip,
    uint8_t *mac)
{
  struct ifreq req;
  struct sockaddr_in *ip;
  struct sockaddr_in6 ip6;
  int fd = xsocket(addr_version, SOCK_RAW, IPPROTO_RAW);
  char ipv6_addr[40] = {0,};

  req.ifr_addr.sa_family = addr_version;
  xstrncpy(req.ifr_name, (char *)interface, IFNAMSIZ);

  xioctl(fd, SIOCGIFFLAGS, &req);

  if (!(req.ifr_flags & IFF_UP)) return -1;

  if (addr_version == AF_INET6) {

    FILE *fd6 = fopen("/proc/net/if_inet6", "r");
    uint8_t *oip6 = (uint8_t*)oip;
    int i;

    while(fgets(toybuf, sizeof(toybuf), fd6)) {
      if (!strstr(toybuf, interface))
        continue;

      if (sscanf(toybuf, "%32s \n", ipv6_addr) == 1)
        break;
    }
    fclose(fd6);

    if (oip6) {
      char *ptr = ipv6_addr+sizeof(ipv6_addr)-1;

      // convert giant hex string into colon-spearated ipv6 address by
      // inserting ':' every 4 characters.
      for (i = 32; i; i--)
        if ((*(ptr--) = ipv6_addr[i])) if (!(i&3)) *(ptr--) = ':';

      dbg("ipv6 %s\n", ipv6_addr);
      if(inet_pton(AF_INET6, ipv6_addr, &ip6.sin6_addr) <= 0)
        error_msg("inet : the ipv6 address is not proper");
      else
        memcpy(oip6, ip6.sin6_addr.s6_addr32, sizeof(uint32_t)*4);
    }
  } else {
    uint32_t *oip4 = (uint32_t*)oip;
    if (oip4) {
      xioctl(fd, SIOCGIFADDR, &req);
      ip = (struct sockaddr_in*) &req.ifr_addr;
      dbg("IP %s\n", inet_ntoa(ip->sin_addr));
      *oip4 = ntohl(ip->sin_addr.s_addr);
    }
  }

  if (ifindex) {
    xioctl(fd, SIOCGIFINDEX, &req);
    dbg("Adapter index %d\n", req.ifr_ifindex);
    *ifindex = req.ifr_ifindex;
  }
  if (mac) {
    xioctl(fd, SIOCGIFHWADDR, &req);
    memcpy(mac, req.ifr_hwaddr.sa_data, 6);
    dbg("MAC %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  close(fd);
  return 0;
}

/*
 *logs messeges to syslog or console
 *opening the log is still left with applet.
 *FIXME: move to more relevent lib. probably libc.c
 */
static void infomsg(uint8_t infomode, char *s, ...)
{
  int used;
  char *msg;
  va_list p, t;

  if (infomode == LOG_SILENT) return;
  va_start(p, s);
  va_copy(t, p);
  used = vsnprintf(NULL, 0, s, t);
  used++;
  va_end(t);

  msg = xmalloc(used);
  vsnprintf(msg, used, s, p);
  va_end(p);

  if (infomode & LOG_SYSTEM) syslog(LOG_INFO, "%s", msg);
  if (infomode & LOG_CONSOLE) printf("%s\n", msg);
  free(msg);
}

/*
 * Writes self PID in file PATH
 * FIXME: libc implementation only writes in /var/run
 * this is more generic as some implemenation may provide
 * arguments to write in specific file. as dhcpd does.
 */
static void write_pid(char *path)
{
  int pidfile = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
  if (pidfile > 0) {
    char pidbuf[12];

    sprintf(pidbuf, "%u", (unsigned)getpid());
    write(pidfile, pidbuf, strlen(pidbuf));
    close(pidfile);
  }
}

// Generic signal handler real handling is done in main funcrion.
static void signal_handler(int sig)
{
  unsigned char ch = sig;
  if (write(sigfd.wr, &ch, 1) != 1) dbg("can't send signal\n");
}

// signal setup for SIGUSR1 SIGTERM
static int setup_signal()
{
  if (pipe((int *)&sigfd) < 0) {
    dbg("signal pipe failed\n");
    return -1;
  }
  fcntl(sigfd.wr , F_SETFD, FD_CLOEXEC);
  fcntl(sigfd.rd , F_SETFD, FD_CLOEXEC);
  int flags = fcntl(sigfd.wr, F_GETFL);
  fcntl(sigfd.wr, F_SETFL, flags | O_NONBLOCK);
  signal(SIGUSR1, signal_handler);
  signal(SIGTERM, signal_handler);
  return 0;
}

// String STR to UINT32 conversion strored in VAR
static int strtou32(const char *str, void *var)
{
  char *endptr = NULL;
  int base = 10;
  errno=0;
  *((uint32_t*)(var)) = 0;
  if (str[0]=='0' && (str[1]=='x' || str[1]=='X')) {
    base = 16;
    str+=2;
  }

  long ret_val = strtol(str, &endptr, base);
  if (errno) infomsg(infomode, "config : Invalid num %s",str);
  else if (endptr && (*endptr!='\0'||endptr == str))
      infomsg(infomode, "config : Not a valid num %s",str);
  else *((uint32_t*)(var)) = (uint32_t)ret_val;
  return 0;
}

// copy string STR in variable VAR
static int strinvar(const char *str, void *var)
{
  char **dest = var;
  if (*dest) free(*dest);
  *dest = strdup(str);
  return 0;
}

// IP String STR to binary data.
static int striptovar(const char *str, void *var)
{
  *((uint32_t*)(var)) = 0;
  if(!str) {
    error_msg("config : NULL address string \n");
    return -1;
  }
  if((inet_pton(AF_INET6, str, var)<=0) && (inet_pton(AF_INET, str, var)<=0)) {
    error_msg("config : wrong address %s \n", str);
    return -1;
  }
  return 0;
}

// String to dhcp option conversion
static int strtoopt(const char *str, void *var)
{
  char *option, *valstr, *grp, *tp;
  uint32_t optcode = 0, inf = infomode, convtmp, mask, nip, router;
  uint16_t flag = 0;
  int count, size = ARRAY_LEN(options_list);

  if (!*str) return 0;
  if (!(option = strtok((char*)str, " \t="))) return -1;

  infomode = LOG_SILENT;
  strtou32(option, (uint32_t*)&optcode);
  infomode = inf;

  if (optcode > 0 && optcode < 256) { // raw option
    for (count = 0; count < size; count++) {
      if ((options_list[count].code & 0X00FF) == optcode) {
        flag = (options_list[count].code & 0XFF00);
        break;
      }
    }
  } else { //string option
    for (count = 0; count < size; count++) {
      if (!strncmp(options_list[count].key, option, strlen(options_list[count].key))) {
        flag = (options_list[count].code & 0XFF00);
        optcode = (options_list[count].code & 0X00FF);
        break;
      }
    }
  }
  if (count == size) {
    infomsg(inf, "config : Obsolete OR Unknown Option : %s", option);
    return -1;
  }

  if (!flag || !optcode) return -1;

  if (!(valstr = strtok(NULL, " \t"))) {
    dbg("config : option %s has no value defined.\n", option);
    return -1;
  }
  dbg(" value : %-20s : ", valstr);
  switch (flag) {
  case DHCP_NUM32:
    options_list[count].len = sizeof(uint32_t);
    options_list[count].val = xmalloc(sizeof(uint32_t));
    strtou32(valstr, &convtmp);
    memcpy(options_list[count].val, &convtmp, sizeof(uint32_t));
    break;
  case DHCP_NUM16:
    options_list[count].len = sizeof(uint16_t);
    options_list[count].val = xmalloc(sizeof(uint16_t));
    strtou32(valstr, &convtmp);
    memcpy(options_list[count].val, &convtmp, sizeof(uint16_t));
    break;
  case DHCP_NUM8:
    options_list[count].len = sizeof(uint8_t);
    options_list[count].val = xmalloc(sizeof(uint8_t));
    strtou32(valstr, &convtmp);
    memcpy(options_list[count].val, &convtmp, sizeof(uint8_t));
    break;
  case DHCP_IP:
    options_list[count].len = sizeof(uint32_t);
    options_list[count].val = xmalloc(sizeof(uint32_t));
    striptovar(valstr, options_list[count].val);
    break;
  case DHCP_STRING:
    options_list[count].len = strlen(valstr);
    options_list[count].val = strdup(valstr);
    break;
  case DHCP_IPLIST:
    while(valstr){
      options_list[count].val = xrealloc(options_list[count].val, options_list[count].len + sizeof(uint32_t));
      striptovar(valstr, ((uint8_t*)options_list[count].val)+options_list[count].len);
      options_list[count].len += sizeof(uint32_t);
      valstr = strtok(NULL," \t");
    }
    break;
  case DHCP_IPPLST:
    break;
  case DHCP_STCRTS:
    /* Option binary format:
     * mask [one byte, 0..32]
     * ip [0..4 bytes depending on mask]
     * router [4 bytes]
     * may be repeated
     * staticroutes 10.0.0.0/8 10.127.0.1, 10.11.12.0/24 10.11.12.1
     */
    grp = strtok(valstr, ",");;
    while(grp){
      while(*grp == ' ' || *grp == '\t') grp++;
      tp = strchr(grp, '/');
      if (!tp) error_exit("wrong formatted static route option");
      *tp = '\0';
      mask = strtol(++tp, &tp, 10);
      if (striptovar(grp, (uint8_t*)&nip)<0) error_exit("wrong formatted static route option");
      while(*tp == ' ' || *tp == '\t' || *tp == '-') tp++;
      if (striptovar(tp, (uint8_t*)&router)<0) error_exit("wrong formatted static route option");
      options_list[count].val = xrealloc(options_list[count].val, options_list[count].len + 1 + mask/8 + 4);
      memcpy(((uint8_t*)options_list[count].val)+options_list[count].len, &mask, 1);
      options_list[count].len += 1;
      memcpy(((uint8_t*)options_list[count].val)+options_list[count].len, &nip, mask/8);
      options_list[count].len += mask/8;
      memcpy(((uint8_t*)options_list[count].val)+options_list[count].len, &router, 4);
      options_list[count].len += 4;
      tp = NULL;
      grp = strtok(NULL, ",");
    }
    break;
  }
  return 0;
}

// Reads Static leases from STR and updates inner structures.
static int get_staticlease(const char *str, void *var)
{
  struct static_lease_s *sltmp;
  char *tkmac, *tkip;
  int count;

  if (!*str) return 0;

  if (!(tkmac = strtok((char*)str, " \t"))) {
    infomsg(infomode, "config : static lease : mac not found");
    return 0;
  }
  if (!(tkip = strtok(NULL, " \t"))) {
    infomsg(infomode, "config : static lease : no ip bind to mac %s", tkmac);
    return 0;
  }
  sltmp = xzalloc(sizeof(struct static_lease_s));
  for (count = 0; count < 6; count++, tkmac++) {
    errno = 0;
    sltmp->mac[count] = strtol(tkmac, &tkmac, 16);
    if (sltmp->mac[count]>255 || sltmp->mac[count]<0 || (*tkmac && *tkmac!=':') || errno) {
      infomsg(infomode, "config : static lease : mac address wrong format");
      free(sltmp);
      return 0;
    }
  }
  striptovar(tkip, &sltmp->nip);
  sltmp->next = gstate.leases.sleases;
  gstate.leases.sleases = sltmp;

  return 0;
}

static struct config_keyword keywords[] = {
// keyword          handler           variable address                default
  {"start"        , striptovar      , (void*)&gconfig.start_ip     , "192.168.0.20"},
  {"end"          , striptovar      , (void*)&gconfig.end_ip       , "192.168.0.254"},
  {"interface"    , strinvar        , (void*)&gconfig.interface    , "eth0"},
  {"port"         , strtou32        , (void*)&gconfig.port         , "67"},
  {"min_lease"    , strtou32        , (void*)&gconfig.min_lease_sec, "60"},
  {"max_leases"   , strtou32        , (void*)&gconfig.max_leases   , "235"},
  {"auto_time"    , strtou32        , (void*)&gconfig.auto_time    , "7200"},
  {"decline_time" , strtou32        , (void*)&gconfig.decline_time , "3600"},
  {"conflict_time", strtou32        , (void*)&gconfig.conflict_time, "3600"},
  {"offer_time"   , strtou32        , (void*)&gconfig.offer_time   , "60"},
  {"lease_file"   , strinvar        , (void*)&gconfig.lease_file   , "/var/lib/misc/dhcpd.leases"}, //LEASES_FILE
  {"lease6_file"  , strinvar        , (void*)&gconfig.lease6_file  , "/var/lib/misc/dhcpd6.leases"}, //LEASES_FILE
  {"pidfile"      , strinvar        , (void*)&gconfig.pidfile      , "/var/run/dhcpd.pid"}, //DPID_FILE
  {"siaddr"       , striptovar      , (void*)&gconfig.siaddr_nip   , "0.0.0.0"},
  {"option"       , strtoopt        , (void*)&gconfig.options      , ""},
  {"opt"          , strtoopt        , (void*)&gconfig.options      , ""},
  {"notify_file"  , strinvar        , (void*)&gconfig.notify_file  , ""},
  {"sname"        , strinvar        , (void*)&gconfig.sname        , ""},
  {"boot_file"    , strinvar        , (void*)&gconfig.boot_file    , ""},
  {"static_lease" , get_staticlease , (void*)&gconfig.static_leases, ""},
  {"start6"       , striptovar      , (void*)&gconfig.start_ip6    , "2001:620:40b:555::100"},
  {"end6"         , striptovar      , (void*)&gconfig.end_ip6      , "2001:620:40b:555::200"},
  {"preferred_lifetime" , strtou32  , (void*)&gconfig.pref_lifetime, "3600"},
  {"valid_lifetime"     , strtou32  , (void*)&gconfig.valid_lifetime, "7200"},
  {"t1"           , strtou32        , (void*)&gconfig.t1           , "3600"},
  {"t2"           , strtou32        , (void*)&gconfig.t2           , "5400"},
};

// Parses the server config file and updates the global server config accordingly.
static int parse_server_config(char *config_file, struct config_keyword *confkey)
{
  FILE *fs = NULL;
  char *confline_temp = NULL,*confline = NULL, *tk = NULL, *tokens[2] = {NULL, NULL};
  int len, linelen, tcount, count, size = ARRAY_LEN(keywords);

  for (count = 0; count < size; count++)
    if (confkey[count].handler)
      confkey[count].handler(confkey[count].def, confkey[count].var);

  if (!(fs = fopen(config_file, "r"))) perror_msg("%s", config_file);
  for (len = 0, linelen = 0; fs;) {
    len = getline(&confline_temp, (size_t*) &linelen, fs);
    confline = confline_temp;
    if (len <= 0) break;
    for (; *confline == ' '; confline++, len--);
    if ((confline[0] == '#') || (confline[0] == '\n')) goto free_conf_continue;
    tk = strchr(confline, '#');
    if (tk) {
      for (; *(tk-1)==' ' || *(tk-1)=='\t'; tk--);
      *tk = '\0';
    }
    tk = strchr(confline, '\n');
    if (tk) {
      for (; *(tk-1)==' ' || *(tk-1)=='\t'; tk--);
      *tk = '\0';
    }
    for (tcount=0, tk=strtok(confline, " \t"); tk && (tcount < 2);
        tcount++, tk=strtok(NULL,(tcount==1)?"":" \t")) {
      while ((*tk == '\t') || (*tk == ' ')) tk++;
      tokens[tcount] = xstrdup(tk);
    }
    if (tcount<=1) goto free_tk0_continue;
    for (count = 0; count < size; count++) {
      if (!strcmp(confkey[count].keyword,tokens[0])) {
        dbg("got config : %15s : ", confkey[count].keyword);
        if (confkey[count].handler(tokens[1], confkey[count].var) == 0)
          dbg("%s \n", tokens[1]);
        break;
      }
    }
    if (tokens[1]) { free(tokens[1]); tokens[1] = NULL; }
free_tk0_continue:
    if (tokens[0]) { free(tokens[0]); tokens[0] = NULL; }
free_conf_continue:
    free(confline_temp);
    confline_temp = NULL;
  }
  if (fs) fclose(fs);
  return 0;
}

// opens UDP socket for listen ipv6 packets
static int open_listensock6(void)
{
  struct sockaddr_in6 addr6;
  struct ipv6_mreq mreq;

  if (gstate.listensock > 0) close(gstate.listensock);

  dbg("Opening listen socket on *:%d %s\n", gconfig.port, gconfig.interface);

  gstate.listensock = xsocket(PF_INET6, SOCK_DGRAM, 0);
  setsockopt(gstate.listensock, SOL_SOCKET, SO_REUSEADDR, &constone, sizeof(constone));
  setsockopt(gstate.listensock, IPPROTO_IPV6, IPV6_CHECKSUM, &constone, sizeof(constone));

  if (setsockopt(gstate.listensock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &constone,
        sizeof(constone)) == -1) {
    error_msg("failed to receive ipv6 packets.\n");
    close(gstate.listensock);
    return -1;
  }

  setsockopt(gstate.listensock, SOL_SOCKET, SO_BINDTODEVICE, gconfig.interface, strlen(gconfig.interface)+1);

  memset(&addr6, 0, sizeof(addr6));
  addr6.sin6_family = AF_INET6;
  addr6.sin6_port = htons(gconfig.port); //SERVER_PORT
  addr6.sin6_scope_id = if_nametoindex(gconfig.interface);
  //Listening for multicast packet
  inet_pton(AF_INET6, "ff02::1:2", &addr6.sin6_addr);

  if (bind(gstate.listensock, (struct sockaddr *) &addr6, sizeof(addr6)) == -1) {
    close(gstate.listensock);
    perror_exit("bind failed");
  }

  memset(&mreq, 0, sizeof(mreq));
  mreq.ipv6mr_interface = if_nametoindex(gconfig.interface);
  memcpy(&mreq.ipv6mr_multiaddr, &addr6.sin6_addr, sizeof(addr6.sin6_addr));

  if(setsockopt(gstate.listensock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) == -1) {
    error_msg("failed to join a multicast group.\n");
    close(gstate.listensock);
    return -1;
  }

  dbg("OPEN : success\n");
  return 0;
}

// opens UDP socket for listen
static int open_listensock(void)
{
  struct sockaddr_in addr;
  struct ifreq ifr;

  if (gstate.listensock > 0) close(gstate.listensock);

  dbg("Opening listen socket on *:%d %s\n", gconfig.port, gconfig.interface);
  gstate.listensock = xsocket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  setsockopt(gstate.listensock, SOL_SOCKET, SO_REUSEADDR, &constone, sizeof(constone));
  if (setsockopt(gstate.listensock, SOL_SOCKET, SO_BROADCAST, &constone, sizeof(constone)) == -1) {
    error_msg("failed to receive brodcast packets.\n");
    close(gstate.listensock);
    return -1;
  }
  memset(&ifr, 0, sizeof(ifr));
  xstrncpy(ifr.ifr_name, gconfig.interface, IFNAMSIZ);
  setsockopt(gstate.listensock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr));

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(gconfig.port); //SERVER_PORT
  addr.sin_addr.s_addr = INADDR_ANY ;

  if (bind(gstate.listensock, (struct sockaddr *) &addr, sizeof(addr))) {
    close(gstate.listensock);
    perror_exit("bind failed");
  }
  dbg("OPEN : success\n");
  return 0;
}

static int send_packet6(uint8_t relay, uint8_t *client_lla, uint16_t optlen)
{
  struct sockaddr_ll dest_sll;
  dhcp6_raw_t packet;
  unsigned padding;
  int fd, result = -1;

  memset(&packet, 0, sizeof(dhcp6_raw_t));
  memcpy(&packet.dhcp6, &gstate.send.send_pkt6, sizeof(dhcp6_msg_t));
  padding = sizeof(packet.dhcp6.options) - optlen;

  if ((fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IPV6))) < 0) {
    dbg("SEND : ipv6 socket failed\n");
    return -1;
  }
  memset(&dest_sll, 0, sizeof(dest_sll));
  dest_sll.sll_family = AF_PACKET;
  dest_sll.sll_protocol = htons(ETH_P_IPV6);
  dest_sll.sll_ifindex = gconfig.ifindex;
  dest_sll.sll_halen = ETH_ALEN;
  memcpy(dest_sll.sll_addr, client_lla, sizeof(uint8_t)*6);

  if (bind(fd, (struct sockaddr *) &dest_sll, sizeof(dest_sll)) < 0) {
    dbg("SEND : bind failed\n");
    close(fd);
    return -1;
  }
  memcpy(&packet.iph.ip6_src, &gconfig.server_nip6, sizeof(uint32_t)*4);
  memcpy(&packet.iph.ip6_dst, &gstate.client_nip6, sizeof(uint32_t)*4);

  packet.udph.source = htons(gconfig.port); //SERVER_PORT
  packet.udph.dest = gstate.client_port; //CLIENT_PORT
  packet.udph.len = htons(sizeof(dhcp6_raw_t) - sizeof(struct ip6_hdr) - padding);
  packet.iph.ip6_ctlun.ip6_un1.ip6_un1_plen = htons(ntohs(packet.udph.len) + 0x11);
  packet.udph.check = dhcp_checksum(&packet, sizeof(dhcp6_raw_t) - padding);
  packet.iph.ip6_ctlun.ip6_un1.ip6_un1_flow = htonl(0x60000000);
  packet.iph.ip6_ctlun.ip6_un1.ip6_un1_plen = packet.udph.len;
  packet.iph.ip6_ctlun.ip6_un1.ip6_un1_nxt = IPPROTO_UDP;
  packet.iph.ip6_ctlun.ip6_un1.ip6_un1_hlim = 0x64;

  result = sendto(fd, &packet, sizeof(dhcp6_raw_t)-padding,
      0, (struct sockaddr *) &dest_sll, sizeof(dest_sll));

  dbg("sendto %d\n", result);
  close(fd);
  if (result < 0) dbg("PACKET send error\n");
  return result;
}

// Sends data through raw socket.
static int send_packet(uint8_t broadcast)
{
  struct sockaddr_ll dest_sll;
  dhcp_raw_t packet;
  unsigned padding;
  int fd, result = -1;
  char bmacaddr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  memset(&packet, 0, sizeof(dhcp_raw_t));
  memcpy(&packet.dhcp, &gstate.send.send_pkt, sizeof(dhcp_msg_t));

  if ((fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
    dbg("SEND : socket failed\n");
    return -1;
  }
  memset(&dest_sll, 0, sizeof(dest_sll));
  dest_sll.sll_family = AF_PACKET;
  dest_sll.sll_protocol = htons(ETH_P_IP);
  dest_sll.sll_ifindex = gconfig.ifindex;
  dest_sll.sll_halen = 6;
  memcpy(dest_sll.sll_addr, (broadcast)?bmacaddr:gstate.rcvd.rcvd_pkt.chaddr , 6);

  if (bind(fd, (struct sockaddr *) &dest_sll, sizeof(dest_sll)) < 0) {
    dbg("SEND : bind failed\n");
    close(fd);
    return -1;
  }
  padding = 308 - 1 - dhcp_opt_size(gstate.send.send_pkt.options);
  packet.iph.protocol = IPPROTO_UDP;
  packet.iph.saddr = gconfig.server_nip;
  packet.iph.daddr = (broadcast || (gstate.rcvd.rcvd_pkt.ciaddr == 0))?
    INADDR_BROADCAST : gstate.rcvd.rcvd_pkt.ciaddr;
  packet.udph.source = htons(gconfig.port);//SERVER_PORT
  packet.udph.dest = gstate.client_port; //CLIENT_PORT
  packet.udph.len = htons(sizeof(dhcp_raw_t) - sizeof(struct iphdr) - padding);
  packet.iph.tot_len = packet.udph.len;
  packet.udph.check = dhcp_checksum(&packet, sizeof(dhcp_raw_t) - padding);
  packet.iph.tot_len = htons(sizeof(dhcp_raw_t) - padding);
  packet.iph.ihl = sizeof(packet.iph) >> 2;
  packet.iph.version = IPVERSION;
  packet.iph.ttl = IPDEFTTL;
  packet.iph.check = dhcp_checksum(&packet.iph, sizeof(packet.iph));

  result = sendto(fd, &packet, sizeof(dhcp_raw_t) - padding, 0,
      (struct sockaddr *) &dest_sll, sizeof(dest_sll));

  dbg("sendto %d\n", result);
  close(fd);
  if (result < 0) dbg("PACKET send error\n");
  return result;
}

static int read_packet6(void)
{
  int ret;
  struct sockaddr_in6 c_addr;
  socklen_t c_addr_size = sizeof(c_addr);

  memset(&gstate.rcvd.rcvd_pkt6, 0, sizeof(dhcp6_msg_t));
  ret = recvfrom(gstate.listensock, &gstate.rcvd.rcvd_pkt6, sizeof(dhcp6_msg_t),
      0, (struct sockaddr*) &c_addr, &c_addr_size);
  memcpy(gstate.client_nip6, &c_addr.sin6_addr, sizeof(uint32_t)*4);
  memcpy(&gstate.client_port, &c_addr.sin6_port, sizeof(uint32_t));
  if (ret < 0) {
    dbg("Packet read error, ignoring. \n");
    return ret; // returns -1
  }
  if (gstate.rcvd.rcvd_pkt6.msgtype < 1) {
    dbg("Bad message type, igroning. \n");
    return -2;
  }

  dbg("Received an ipv6 packet. Size : %d \n", ret);
  return ret;
}

// Reads from UDP socket
static int read_packet(void)
{
  int ret;
  struct sockaddr_in c_addr;
  socklen_t c_addr_size = sizeof(c_addr);

  memset(&gstate.rcvd.rcvd_pkt, 0, sizeof(dhcp_msg_t));
  ret = recvfrom(gstate.listensock, &gstate.rcvd.rcvd_pkt, sizeof(dhcp_msg_t),
      0, (struct sockaddr*) &c_addr, &c_addr_size);
  memcpy(&gstate.client_port, &c_addr.sin_port, sizeof(uint32_t));
  /*ret = read(gstate.listensock, &gstate.rcvd.rcvd_pkt, sizeof(dhcp_msg_t));*/
  if (ret < 0) {
    dbg("Packet read error, ignoring. \n");
    return ret; // returns -1
  }
  if (gstate.rcvd.rcvd_pkt.cookie != htonl(DHCP_MAGIC)) {
    dbg("Packet with bad magic, ignoring. \n");
    return -2;
  }
  if (gstate.rcvd.rcvd_pkt.op != 1) { //BOOTPREQUEST
    dbg("Not a BOOT REQUEST ignoring. \n");
    return -2;
  }
  if (gstate.rcvd.rcvd_pkt.hlen != 6) {
    dbg("hlen != 6 ignoring. \n");
    return -2;
  }
  dbg("Received a packet. Size : %d \n", ret);
  return ret;
}

// Preapres a dhcp packet with defaults and configs
static uint8_t* prepare_send_pkt(void)
{
  memset((void*)&gstate.send.send_pkt, 0, sizeof(gstate.send.send_pkt));
  gstate.send.send_pkt.op = 2; //BOOTPREPLY
  gstate.send.send_pkt.htype = 1;
  gstate.send.send_pkt.hlen = 6;
  gstate.send.send_pkt.xid = gstate.rcvd.rcvd_pkt.xid;
  gstate.send.send_pkt.cookie = htonl(DHCP_MAGIC);
  gstate.send.send_pkt.nsiaddr = gconfig.server_nip;
  memcpy(gstate.send.send_pkt.chaddr, gstate.rcvd.rcvd_pkt.chaddr, 16);
  gstate.send.send_pkt.options[0] = DHCP_OPT_END;
  return gstate.send.send_pkt.options;
}

static uint8_t* prepare_send_pkt6(uint16_t opt)
{
  memset((void*)&gstate.send.send_pkt6, 0, sizeof(gstate.send.send_pkt6));
  gstate.send.send_pkt6.msgtype = opt;
  memcpy(gstate.send.send_pkt6.transaction_id, gstate.rcvd.rcvd_pkt6.transaction_id, 3);
  return gstate.send.send_pkt6.options;
}

// Sets a option value in dhcp packet's option field
static uint8_t* set_optval(uint8_t *optptr, uint16_t opt, void *var, size_t len)
{
  while (*optptr != DHCP_OPT_END) optptr++;
  *optptr++ = (uint8_t)(opt & 0x00FF);
  *optptr++ = (uint8_t) len;
  memcpy(optptr, var, len);
  optptr += len;
  *optptr = DHCP_OPT_END;
  return optptr;
}

static uint8_t* set_optval6(uint8_t *optptr, uint16_t opt, void *var, size_t len)
{
  *((uint16_t*)optptr) = htons(opt);
  *(uint16_t*)(optptr+2) = htons(len);
  memcpy(optptr+4, var, len);
  optptr += len+4;
  return optptr;
}

// Gets a option value from dhcp packet's option field
static uint8_t* get_optval(uint8_t *optptr, uint16_t opt, void *var)
{
  size_t len;
  uint8_t overloaded = 0;

  while (1) {
    while (*optptr == DHCP_OPT_PADDING) optptr++;
    if ((*optptr & 0x00FF) == DHCP_OPT_END) break;
    if ((*optptr & 0x00FF) == DHCP_OPT_OPTION_OVERLOAD) {
      overloaded = optptr[2];
      optptr += optptr[1] + 2;
    }
    len = optptr[1];
    if (*optptr == (opt & 0x00FF))
      switch (opt & 0xFF00) {
        case DHCP_NUM32: // FALLTHROUGH
        case DHCP_IP:
          memcpy(var, optptr+2, sizeof(uint32_t));
          optptr += len + 2;
          return optptr;
          break;
        case DHCP_NUM16:
          memcpy(var, optptr+2, sizeof(uint16_t));
          optptr += len + 2;
          return optptr;
          break;
        case DHCP_NUM8:
          memcpy(var, optptr+2, sizeof(uint8_t));
          optptr += len + 2;
          return optptr;
          break;
        case DHCP_STRING:
          var = xstrndup((char*) optptr, len);
          optptr += len + 2;
          return optptr;
          break;
      }
    optptr += len + 2;
  }
  if ((overloaded == 1) | (overloaded == 3)) get_optval((uint8_t*)&gstate.rcvd.rcvd_pkt.file, opt, var);
  if ((overloaded == 2) | (overloaded == 3)) get_optval((uint8_t*)&gstate.rcvd.rcvd_pkt.sname, opt, var);
  return optptr;
}

static uint8_t* get_optval6(uint8_t *optptr, uint16_t opt, uint16_t *datalen, void **var)
{
  uint16_t optcode;
  uint16_t len;

  memcpy(&optcode, optptr, sizeof(uint16_t));
  memcpy(&len, optptr+2, sizeof(uint16_t));
  if(!optcode) {
    dbg("Option %d is not exist.\n", opt);
    return optptr;
  }
  optcode = ntohs(optcode);
  len = ntohs(len);

  if (opt == optcode) {
    *var = xmalloc(len);
    memcpy(*var, optptr+4, len);
    optptr = optptr + len + 4;
    memcpy(datalen, &len, sizeof(uint16_t));
  }
  else {
    optptr = get_optval6(optptr+len+4, opt, datalen, var);
  }

  return optptr;
}

// Retrives Requested Parameter list from dhcp req packet.
static uint8_t get_reqparam(uint8_t **list)
{
  uint8_t len, *optptr;
  if(*list) free(*list);
  for (optptr = gstate.rcvd.rcvd_pkt.options;
      *optptr && *optptr!=((DHCP_OPT_PARAM_REQ) & 0x00FF); optptr+=optptr[1]+2);
  len = *++optptr;
  *list = xzalloc(len+1);
  memcpy(*list, ++optptr, len);
  return len;
}

// Sets values of req param in dhcp offer packet.
static uint8_t* set_reqparam(uint8_t *optptr, uint8_t *list)
{
  uint8_t reqcode;
  int count, size = ARRAY_LEN(options_list);

  while (*list) {
    reqcode = *list++;
    for (count = 0; count < size; count++) {
      if ((options_list[count].code & 0X00FF)==reqcode) {
        if (!(options_list[count].len) || !(options_list[count].val)) break;
        for (; *optptr && *optptr!=DHCP_OPT_END; optptr+=optptr[1]+2);
        *optptr++ = (uint8_t) (options_list[count].code & 0x00FF);
        *optptr++ = (uint8_t) options_list[count].len;
        memcpy(optptr, options_list[count].val, options_list[count].len);
        optptr += options_list[count].len;
        *optptr = DHCP_OPT_END;
        break;
      }
    }
  }
  return optptr;
}

static void run_notify(char **argv)
{
  struct stat sts;
  volatile int error = 0;
  pid_t pid;

  if (stat(argv[0], &sts) == -1 && errno == ENOENT) {
    infomsg(infomode, "notify file: %s : not exist.", argv[0]);
    return;
  }
  fflush(NULL);

  pid = vfork();
  if (pid < 0) {
    dbg("Fork failed.\n");
    return;
  }
  if (!pid) {
    execvp(argv[0], argv);
    error = errno;
    _exit(111);
  }
  if (error) {
    waitpid(pid, NULL, 0);
    errno = error;
  }
  dbg("script complete.\n");
}

static void write_leasefile(void)
{
  int fd;
  uint32_t curr, tmp_time;
  int64_t timestamp;
  struct arg_list *listdls = gstate.dleases;
  dyn_lease *dls;

  if ((fd = open(gconfig.lease_file, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0) {
    perror_msg("can't open %s ", gconfig.lease_file);
  } else {
    curr = timestamp = time(NULL);
    timestamp = SWAP_BE64(timestamp);
    writeall(fd, &timestamp, sizeof(timestamp));

    while (listdls) {
      dls = (dyn_lease*)listdls->arg;
      tmp_time = dls->expires;
      dls->expires -= curr;
      if ((int32_t) dls->expires < 0) goto skip;
      dls->expires = htonl(dls->expires);
      writeall(fd, dls, sizeof(dyn_lease));
skip:
      dls->expires = tmp_time;
      listdls = listdls->next;
    }
    close(fd);
    if (gconfig.notify_file) {
      char *argv[3];
      argv[0] = gconfig.notify_file;
      argv[1] = gconfig.lease_file;
      argv[2] = NULL;
      run_notify(argv);
    }
  }
}

static void write_lease6file(void)
{
  int fd;
  uint32_t curr, tmp_time;
  int64_t timestamp;
  struct arg_list *listdls = gstate.dleases;
  dyn_lease6 *dls6;

  if ((fd = open(gconfig.lease6_file, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0) {
    perror_msg("can't open %s ", gconfig.lease6_file);
  } else {
    curr = timestamp = time(NULL);
    timestamp = SWAP_BE64(timestamp);
    writeall(fd, &timestamp, sizeof(timestamp));

    while (listdls) {
      dls6 = (dyn_lease6*)listdls->arg;
      tmp_time = dls6->expires;
      dls6->expires -= curr;
      if ((int32_t) dls6->expires < 0) goto skip;
      dls6->expires = htonl(dls6->expires);
      writeall(fd, dls6, sizeof(dyn_lease6));
skip:
      dls6->expires = tmp_time;
      listdls = listdls->next;
    }
    close(fd);
    if (gconfig.notify_file) {
      char *argv[3];
      argv[0] = gconfig.notify_file;
      argv[1] = gconfig.lease6_file;
      argv[2] = NULL;
      run_notify(argv);
    }
  }
}

// Update max lease time from options.
static void set_maxlease(void)
{
  int count, size = ARRAY_LEN(options_list);
  for (count = 0; count < size; count++)
    if (options_list[count].val && options_list[count].code == (DHCP_OPT_LEASE_TIME)) {
      gconfig.max_lease_sec = *((uint32_t*)options_list[count].val);
      break;
    }
  if (!gconfig.max_lease_sec) gconfig.max_lease_sec = (60*60*24*10);// DEFAULT_LEASE_TIME;
}

// Returns lease time for client.
static uint32_t get_lease(uint32_t req_exp)
{
  uint32_t now = time(NULL);
  req_exp = req_exp - now;
  if(addr_version == AF_INET6) {
    if ((req_exp <= 0) || req_exp > gconfig.pref_lifetime ||
        req_exp > gconfig.valid_lifetime) {
      if ((gconfig.pref_lifetime > gconfig.valid_lifetime)) {
        error_msg("The valid lifetime must be greater than the preferred lifetime, \
            setting to valid lifetime %u", gconfig.valid_lifetime);
        return gconfig.valid_lifetime;
      }
      return gconfig.pref_lifetime;
    }
  } else {
    if ((req_exp <= 0) || (req_exp > gconfig.max_lease_sec))
      return gconfig.max_lease_sec;

    if (req_exp < gconfig.min_lease_sec)
      return gconfig.min_lease_sec;
  }

  return req_exp;
}

static int verifyip6_in_lease(uint8_t *nip6, uint8_t *duid, uint16_t ia_type, uint32_t iaid)
{
  static_lease6 *sls6;
  struct arg_list *listdls;

  for (listdls = gstate.dleases; listdls; listdls = listdls->next) {
    if (!memcmp(((dyn_lease6*) listdls->arg)->lease_nip6, nip6, sizeof(uint32_t)*4))
      return -1;

    if (!memcmp(((dyn_lease6*) listdls->arg)->duid, duid, ((dyn_lease6*) listdls->arg)->duid_len)
        && ((dyn_lease6*) listdls->arg)->ia_type == ia_type)
      return -1;
  }
  for (sls6 = gstate.leases.sleases6; sls6; sls6 = sls6->next)
    if (memcmp(sls6->nip6, nip6, sizeof(uint32_t)*4)==0) return -2;

  if (memcmp(nip6, gconfig.start_ip6, sizeof(uint32_t)*4) < 0 ||
      memcmp(nip6, gconfig.end_ip6, sizeof(uint32_t)*4) > 0)
    return -3;

  return 0;
}

// Verify ip NIP in current leases ( assigned or not)
static int verifyip_in_lease(uint32_t nip, uint8_t mac[6])
{
  static_lease *sls;
  struct arg_list *listdls;

  for (listdls = gstate.dleases; listdls; listdls = listdls->next) {
    if (((dyn_lease*) listdls->arg)->lease_nip == nip) {
      if (((int32_t)(((dyn_lease*) listdls->arg)->expires) - time(NULL)) < 0)
        return 0;
      return -1;
    }
    if (!memcmp(((dyn_lease*) listdls->arg)->lease_mac, mac, 6)) return -1;
  }
  for (sls = gstate.leases.sleases; sls; sls = sls->next)
    if (sls->nip == nip) return -2;

  if ((ntohl(nip) < gconfig.start_ip) || (ntohl(nip) > gconfig.end_ip))
    return -3;

  return 0;
}

// add ip assigned_nip to dynamic lease.
static int addip_to_lease(uint32_t assigned_nip, uint8_t mac[6], uint32_t *req_exp, char *hostname, uint8_t update)
{
  dyn_lease *dls;
  struct arg_list *listdls = gstate.dleases;
  uint32_t now = time(NULL);

  while (listdls) {
    if (!memcmp(((dyn_lease*) listdls->arg)->lease_mac, mac, 6)) {
      if (update) *req_exp = get_lease(*req_exp + ((dyn_lease*) listdls->arg)->expires);
      ((dyn_lease*) listdls->arg)->expires = *req_exp + now;
      return 0;
    }
    listdls = listdls->next;
  }

  dls = xzalloc(sizeof(dyn_lease));
  memcpy(dls->lease_mac, mac, 6);
  dls->lease_nip = assigned_nip;
  if (hostname) memcpy(dls->hostname, hostname, 20);

  if (update) *req_exp = get_lease(*req_exp + now);
  dls->expires = *req_exp + now;

  listdls = xzalloc(sizeof(struct arg_list));
  listdls->next = gstate.dleases;
  listdls->arg = (char*)dls;
  gstate.dleases = listdls;

  return 0;
}

static int addip6_to_lease(uint8_t *assigned_nip, uint8_t *duid, uint16_t duid_len, uint16_t ia_type, uint32_t iaid, uint32_t *lifetime, uint8_t update)
{
  dyn_lease6 *dls6;
  struct arg_list *listdls = gstate.dleases;
  uint32_t now = time(NULL);

  while (listdls) {
    if (!memcmp(((dyn_lease6*) listdls->arg)->duid, duid, ((dyn_lease6*) listdls->arg)->duid_len)) {
      if (update) *lifetime = get_lease(*lifetime + ((dyn_lease6*) listdls->arg)->expires);
      ((dyn_lease6*) listdls->arg)->expires = *lifetime + now;
      return 0;
    }
    listdls = listdls->next;
  }

  dls6 = xzalloc(sizeof(dyn_lease6));
  dls6->duid_len = duid_len;
  memcpy(dls6->duid, duid, duid_len);
  dls6->ia_type = ia_type;
  dls6->iaid = iaid;
  memcpy(dls6->lease_nip6, assigned_nip, sizeof(uint32_t)*4);

  if (update) *lifetime = get_lease(*lifetime + now);
  dls6->expires = *lifetime + now;

  listdls = xzalloc(sizeof(struct arg_list));
  listdls->next = gstate.dleases;
  listdls->arg = (char*)dls6;
  gstate.dleases = listdls;

  return 0;
}

// delete ip assigned_nip from dynamic lease.
static int delip_from_lease(uint32_t assigned_nip, uint8_t mac[6], uint32_t del_time)
{
  struct arg_list *listdls = gstate.dleases;

  while (listdls) {
    if (!memcmp(((dyn_lease*) listdls->arg)->lease_mac, mac, 6)) {
      ((dyn_lease*) listdls->arg)->expires = del_time + time(NULL);
      return 0;
    }
    listdls = listdls->next;
  }
  return -1;
}

// returns a IP from static, dynamic leases or free ip pool, 0 otherwise.
static uint32_t getip_from_pool(uint32_t req_nip, uint8_t mac[6], uint32_t *req_exp, char *hostname)
{
  uint32_t nip = 0;
  static_lease *sls = gstate.leases.sleases;
  struct arg_list *listdls = gstate.dleases, *tmp = NULL;

  if (req_nip && (!verifyip_in_lease(req_nip, mac))) nip = req_nip;

  if (!nip) {
    while (listdls) {
      if (!memcmp(((dyn_lease*)listdls->arg)->lease_mac, mac, 6)) {
        nip = ((dyn_lease*)listdls->arg)->lease_nip;
        if (tmp) tmp->next = listdls->next;
        else gstate.dleases = listdls->next;
        free(listdls->arg);
        free(listdls);
        if (verifyip_in_lease(nip, mac) < 0) nip = 0;
        break;
      }
      tmp = listdls;
      listdls = listdls->next;
    }
  }
  if (!nip) {
    while (sls) {
      if (memcmp(sls->mac, mac, 6) == 0) {
        nip = sls->nip;
        break;
      }
      sls = sls->next;
    }
  }
  if (!nip) {
    for (nip = htonl(gconfig.start_ip); ntohl(nip) <= gconfig.end_ip; ) {
      if (!verifyip_in_lease(nip, mac)) break;
      nip = ntohl(nip);
      nip = htonl(++nip);
    }
    if (ntohl(nip) > gconfig.end_ip) {
      nip = 0;
      infomsg(infomode, "can't find free IP in IP Pool.");
    }
  }
  if (nip) addip_to_lease(nip, mac, req_exp, hostname, 1);
  return nip;
}

static uint8_t *getip6_from_pool(uint8_t *duid, uint16_t duid_len, uint16_t ia_type, uint32_t iaid, uint32_t *lifetime)
{
  static uint8_t nip6[16] = {0, };
  static_lease6 *sls6 = gstate.leases.sleases6;
  struct arg_list *listdls6 = gstate.dleases, *tmp = NULL;

  while(listdls6) {
    if (!memcmp(((dyn_lease6*)listdls6->arg)->duid, duid, duid_len)) {
      memcpy(nip6, ((dyn_lease6*)listdls6->arg)->lease_nip6, sizeof(nip6));
      if(tmp) tmp->next = listdls6->next;
      else gstate.dleases = listdls6->next;
      free(listdls6->arg);
      free(listdls6);

      if(verifyip6_in_lease(nip6, duid, ia_type, iaid) < 0)
        memset(nip6, 0, sizeof(nip6));
      break;
    }
    tmp = listdls6;
    listdls6 = listdls6->next;
  }

  if(!memcmp(nip6, (uint8_t[16]){0}, sizeof(uint32_t)*4)) {
    while(sls6) {
      if(!memcmp(sls6->duid, duid, 6)) {
        memcpy(nip6, sls6->nip6, sizeof(nip6));
        break;
      }
      sls6 = sls6->next;
    }
  }

  if(!memcmp(nip6, (uint8_t[16]){0}, sizeof(uint32_t)*4)) {
    memcpy(nip6, gconfig.start_ip6, sizeof(nip6));
    while(memcmp(nip6, gconfig.end_ip6, sizeof(nip6)) < 0) {
      if(!verifyip6_in_lease(nip6, duid, ia_type, iaid)) break;
      int i=sizeof(nip6);
      while(i--) {
        ++nip6[i];
        if (!nip6[i]) {
          if(i==(sizeof(nip6)-1)) ++nip6[i];
          ++nip6[i-1];
        } else
          break;
      }
    }

    if (memcmp(nip6, gconfig.end_ip6, sizeof(nip6)) > 0) {
      memset(nip6, 0, sizeof(nip6));
      infomsg(infomode, "can't find free IP in IPv6 Pool.");
    }
  }

  if(memcmp(nip6, (uint8_t[16]){0}, sizeof(uint32_t)*4)) {
    addip6_to_lease(nip6, duid, duid_len, ia_type, iaid, lifetime, 1);
    infomsg(infomode, "Assigned IPv6 %02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X",
        nip6[0], nip6[1], nip6[2], nip6[3], nip6[4], nip6[5], nip6[6], nip6[7], nip6[8],
        nip6[9], nip6[10], nip6[11], nip6[12], nip6[13], nip6[14], nip6[15]);
  }
  return nip6;
}

static void read_leasefile(void)
{
  uint32_t passed, ip;
  int32_t tmp_time;
  int64_t timestamp;
  dyn_lease *dls;
  int fd = open(gconfig.lease_file, O_RDONLY);

  dls = xzalloc(sizeof(dyn_lease));

  if (read(fd, &timestamp, sizeof(timestamp)) != sizeof(timestamp))
    goto lease_error_exit;

  timestamp = SWAP_BE64(timestamp);
  passed = time(NULL) - timestamp;
  if ((uint64_t)passed > 12 * 60 * 60) goto lease_error_exit;

  while (read(fd, dls, sizeof(dyn_lease)) == sizeof(dyn_lease)) {
    ip = ntohl(dls->lease_nip);
    if (ip >= gconfig.start_ip && ip <= gconfig.end_ip) {
      tmp_time = ntohl(dls->expires) - passed;
      if (tmp_time < 0) continue;
      addip_to_lease(dls->lease_nip, dls->lease_mac,
          (uint32_t*)&tmp_time, dls->hostname, 0);
    }
  }
lease_error_exit:
  free(dls);
  close(fd);
}

static void read_lease6file(void)
{
  uint32_t passed;
  uint32_t tmp_time;
  int64_t timestamp;
  dyn_lease6 *dls6;
  int fd = open(gconfig.lease6_file, O_RDONLY);

  dls6 = xzalloc(sizeof(dyn_lease6));

  if (read(fd, &timestamp, sizeof(timestamp)) != sizeof(timestamp))
    goto lease6_error_exit;

  timestamp = SWAP_BE64(timestamp);
  passed = time(NULL) - timestamp;
  if ((uint64_t)passed > 12 * 60 * 60) goto lease6_error_exit;

  while (read(fd, dls6, sizeof(dyn_lease6)) == sizeof(dyn_lease6)) {
    if (memcmp(dls6->lease_nip6, gconfig.start_ip6, sizeof(uint32_t)*4) > 0 &&
        memcmp(dls6->lease_nip6, gconfig.end_ip6, sizeof(uint32_t)*4) < 0) {
      tmp_time = ntohl(dls6->expires) - passed;
      if (tmp_time < 0U) continue;
      addip6_to_lease(dls6->lease_nip6, dls6->duid, dls6->duid_len, dls6->ia_type, dls6->iaid,
          (uint32_t*)&tmp_time, 0);
    }
  }

lease6_error_exit:
  free(dls6);
  close(fd);
}

void dhcpd_main(void)
{
  struct timeval tv;
  int retval, i;
  char *optptr, msgtype = 0, *hstname = 0, transactionid[3] = {0};
  unsigned short optlen = 0;
  unsigned waited = 0, serverid = 0, requested_nip = 0, reqested_lease = 0,
           ip_pool_size = 0;
  fd_set rfds;

  infomode = LOG_CONSOLE;
  if (!FLAG(f)) {
    daemon(0, 0);
    infomode = LOG_SILENT;
  }
  if (FLAG(S)) {
    openlog("UDHCPD :", LOG_PID, LOG_DAEMON);
    infomode |= LOG_SYSTEM;
  }
  setlinebuf(stdout);
  //DHCPD_CONF_FILE
  parse_server_config((toys.optc==1) ? *toys.optargs: "/etc/dhcpd.conf",
    keywords);
  infomsg(infomode, "toybox dhcpd started");

  if (FLAG(6)) {
    addr_version = AF_INET6;
    gconfig.t1 = ntohl(gconfig.t1);
    gconfig.t2 = ntohl(gconfig.t2);
    gconfig.pref_lifetime = ntohl(gconfig.pref_lifetime);
    gconfig.valid_lifetime = ntohl(gconfig.valid_lifetime);
    gconfig.port = 547;
    for(i=0; i<4; i++)
      ip_pool_size += (gconfig.end_ip6[i]-gconfig.start_ip6[i])<<((3-i)*8);
  } else {
    gconfig.start_ip = ntohl(gconfig.start_ip);
    gconfig.end_ip = ntohl(gconfig.end_ip);
    ip_pool_size = gconfig.end_ip - gconfig.start_ip + 1;
  }

  if (gconfig.max_leases > ip_pool_size) {
    error_msg("max_leases=%u is too big, setting to %u",
        (unsigned) gconfig.max_leases, ip_pool_size);
    gconfig.max_leases = ip_pool_size;
  }
  write_pid(gconfig.pidfile);
  set_maxlease();
  if (TT.i) gconfig.interface = TT.i;
  if (TT.p) gconfig.port = TT.p;
  (addr_version==AF_INET6) ? read_lease6file() : read_leasefile();

  if (get_interface(gconfig.interface, &gconfig.ifindex,
        (addr_version==AF_INET6)? (void*)gconfig.server_nip6 :
        (void*)&gconfig.server_nip, gconfig.server_mac) < 0)
    perror_exit("Failed to get interface %s", gconfig.interface);
  setup_signal();
  if (addr_version==AF_INET6) open_listensock6();
  else {
    gconfig.server_nip = htonl(gconfig.server_nip);
    open_listensock();
  }

  fcntl(gstate.listensock, F_SETFD, FD_CLOEXEC);

  for (;;) {
    uint32_t timestmp = time(NULL);
    FD_ZERO(&rfds);
    FD_SET(gstate.listensock, &rfds);
    FD_SET(sigfd.rd, &rfds);
    tv.tv_sec = gconfig.auto_time - waited;
    tv.tv_usec = 0;
    retval = 0;
    serverid = 0;
    msgtype = 0;

    int maxfd = (sigfd.rd > gstate.listensock)? sigfd.rd : gstate.listensock;
    dbg("select waiting ....\n");
    retval = select(maxfd + 1, &rfds, NULL, NULL, (gconfig.auto_time?&tv:NULL));
    if (retval < 0) {
      if (errno == EINTR) {
        waited += (unsigned) time(NULL) - timestmp;
        continue;
      }
      dbg("Error in select wait again...\n");
      continue;
    }
    if (!retval) { // Timed out
      dbg("select wait Timed Out...\n");
      waited = 0;
      (addr_version == AF_INET6)? write_lease6file() : write_leasefile();
      if (get_interface(gconfig.interface, &gconfig.ifindex,
            (addr_version==AF_INET6)? (void*)gconfig.server_nip6 :
            (void*)&gconfig.server_nip, gconfig.server_mac)<0)
        perror_exit("Failed to get interface %s", gconfig.interface);
      if(addr_version != AF_INET6) {
        gconfig.server_nip = htonl(gconfig.server_nip);
      }
      continue;
    }
    if (FD_ISSET(sigfd.rd, &rfds)) { // Some Activity on RDFDs : is signal
      unsigned char sig;
      if (read(sigfd.rd, &sig, 1) != 1) {
        dbg("signal read failed.\n");
        continue;
      }
      switch (sig) {
        case SIGUSR1:
          infomsg(infomode, "Received SIGUSR1");
          (addr_version==AF_INET6)? write_lease6file() : write_leasefile();
          continue;
        case SIGTERM:
          infomsg(infomode, "received sigterm");
          (addr_version==AF_INET6)? write_lease6file() : write_leasefile();
          unlink(gconfig.pidfile);
          exit(0);
          break;
        default: break;
      }
    }
    if (FD_ISSET(gstate.listensock, &rfds)) { // Some Activity on RDFDs : is socket
      dbg("select listen sock read\n");
      if(addr_version==AF_INET6) {
        void *client_duid, *server_duid, *client_ia_na, *server_ia_na,
             *client_ia_pd;
        uint8_t client_lla[6] = {0,};
        uint16_t client_duid_len = 0, server_duid_len = 0, server_ia_na_len = 0,
                 client_ia_na_len = 0, client_ia_pd_len = 0;

        if(read_packet6() < 0) {
          open_listensock6();
          continue;
        }
        waited += time(NULL) - timestmp;

        memcpy(&gstate.rqcode, &gstate.rcvd.rcvd_pkt6.msgtype, sizeof(uint8_t));
        memcpy(&transactionid, &gstate.rcvd.rcvd_pkt6.transaction_id,
            sizeof(transactionid));

        if (!gstate.rqcode || gstate.rqcode < DHCP6SOLICIT ||
            gstate.rqcode > DHCP6RELAYREPLY) {
          dbg("no or bad message type option, ignoring packet.\n");
          continue;
        }
        if (memcmp(gstate.rcvd.rcvd_pkt6.transaction_id, transactionid, 3)) {
          dbg("no or bad transaction id, ignoring packet.\n");
          continue;
        }

        waited += time(NULL) - timestmp;
        switch (gstate.rqcode) {
          case DHCP6SOLICIT:
            dbg("Message Type: DHCP6SOLICIT\n");
            optptr = prepare_send_pkt6(DHCP6ADVERTISE);
            optlen = 0;

            //TODO policy check
            //TODO Receive: ORO check (e.g. DNS)

            //Receive: Client Identifier (DUID)
            get_optval6((uint8_t*)&gstate.rcvd.rcvd_pkt6.options,
                DHCP6_OPT_CLIENTID, &client_duid_len, &client_duid);

            //Receive: Identity Association for Non-temporary Address
            if(get_optval6((uint8_t*)&gstate.rcvd.rcvd_pkt6.options,
                  DHCP6_OPT_IA_NA, &client_ia_na_len, &client_ia_na)) {
              uint16_t ia_addr_len = sizeof(struct optval_ia_addr);
              void *ia_addr, *status_code;
              char *status_code_msg;
              uint16_t status_code_len = 0;
              server_ia_na_len = sizeof(struct optval_ia_na);

              //IA Address
              ia_addr = xzalloc(ia_addr_len);
              struct optval_ia_addr *ia_addr_p = (struct optval_ia_addr*)ia_addr;
              (*ia_addr_p).pref_lifetime = gconfig.pref_lifetime;
              (*ia_addr_p).valid_lifetime = gconfig.valid_lifetime;
              memcpy(&(*ia_addr_p).ipv6_addr,
                  getip6_from_pool(client_duid, client_duid_len,
                    DHCP6_OPT_IA_NA, (*(struct optval_ia_na*) client_ia_na).iaid,
                    &(*ia_addr_p).pref_lifetime), sizeof(uint32_t)*4);
              server_ia_na_len += (ia_addr_len+4);

              //Status Code
              if(memcmp((*ia_addr_p).ipv6_addr, (uint8_t[16]){0}, sizeof(uint32_t)*4)) {
                status_code_msg = xstrdup("Assigned an address.");
                status_code_len = strlen(status_code_msg)+1;
                status_code = xzalloc(status_code_len);
                struct optval_status_code *status_code_p =
                  (struct optval_status_code*)status_code;
                (*status_code_p).status_code = htons(DHCP6_STATUS_SUCCESS);
                memcpy((*status_code_p).status_msg, status_code_msg,
                    status_code_len);
                server_ia_na_len += (status_code_len+4);
                free(status_code_msg);
              } else {
                status_code_msg = xstrdup("There's no available address.");
                status_code_len = strlen(status_code_msg)+1;
                status_code = xzalloc(status_code_len);
                struct optval_status_code *status_code_p =
                  (struct optval_status_code*)status_code;
                (*status_code_p).status_code = htons(DHCP6_STATUS_NOADDRSAVAIL);
                memcpy((*status_code_p).status_msg, status_code_msg,
                    status_code_len);
                server_ia_na_len += (status_code_len+4);
                server_ia_na_len -= (ia_addr_len+4);
                ia_addr_len = 0;
                free(ia_addr);
                free(status_code_msg);
                //TODO send failed status code
                break;
              }

              //combine options
              server_ia_na = xzalloc(server_ia_na_len);
              struct optval_ia_na *ia_na_p = (struct optval_ia_na*)server_ia_na;
              (*ia_na_p).iaid = (*(struct optval_ia_na*)client_ia_na).iaid;
              (*ia_na_p).t1 = gconfig.t1;
              (*ia_na_p).t2 = gconfig.t2;

              uint8_t* ia_na_optptr = (*ia_na_p).optval;
              if(ia_addr_len) {
                set_optval6(ia_na_optptr, DHCP6_OPT_IA_ADDR, ia_addr, ia_addr_len);
                ia_na_optptr += (ia_addr_len + 4);
                free(ia_addr);
              }
              if(status_code_len) {
                set_optval6(ia_na_optptr, DHCP6_OPT_STATUS_CODE, status_code,
                    status_code_len);
                ia_na_optptr += (status_code_len);
                free(status_code);
              }

              //Response: Identity Association for Non-temporary Address
              optptr = set_optval6(optptr, DHCP6_OPT_IA_NA, server_ia_na,
                  server_ia_na_len);
              optlen += (server_ia_na_len + 4);
              free(client_ia_na);free(server_ia_na);
            }
            //Receive: Identity Association for Prefix Delegation
            else if(get_optval6((uint8_t*)&gstate.rcvd.rcvd_pkt6.options,
                  DHCP6_OPT_IA_PD, &client_ia_pd_len, &client_ia_pd)) {

              //TODO
              //Response: Identity Association for Prefix Delegation
            }

            //DUID type: link-layer address plus time
            if(ntohs((*(struct optval_duid_llt*)client_duid).type) ==
                DHCP6_DUID_LLT) {
              server_duid_len = 8+sizeof(gconfig.server_mac);
              server_duid = xzalloc(server_duid_len);
              struct optval_duid_llt *server_duid_p =
                (struct optval_duid_llt*)server_duid;
              (*server_duid_p).type = htons(1);
              (*server_duid_p).hwtype = htons(1);
              (*server_duid_p).time = htonl((uint32_t)
                  (time(NULL) - 946684800) & 0xffffffff);
              memcpy((*server_duid_p).lladdr, gconfig.server_mac,
                  sizeof(gconfig.server_mac));
              memcpy(&client_lla, (*(struct optval_duid_llt*)client_duid).lladdr,
                  sizeof(client_lla));

              //Response: Server Identifier (DUID)
              optptr = set_optval6(optptr, DHCP6_OPT_SERVERID, server_duid,
                  server_duid_len);
              optlen += (server_duid_len + 4);
              //Response: Client Identifier
              optptr = set_optval6(optptr, DHCP6_OPT_CLIENTID, client_duid,
                  client_duid_len);
              optlen += (client_duid_len + 4);
              free(client_duid);free(server_duid);
            }

            send_packet6(0, client_lla, optlen);
            write_lease6file();
            break;
          case DHCP6REQUEST:
            dbg("Message Type: DHCP6REQUEST\n");
            optptr = prepare_send_pkt6(DHCP6REPLY);
            optlen = 0;

            //Receive: Client Identifier (DUID)
            get_optval6((uint8_t*)&gstate.rcvd.rcvd_pkt6.options,
                DHCP6_OPT_CLIENTID, &client_duid_len, &client_duid);
            optptr = set_optval6(optptr, DHCP6_OPT_CLIENTID, client_duid,
                client_duid_len);
            optlen += (client_duid_len + 4);
            memcpy(client_lla, (*(struct optval_duid_llt*)client_duid).lladdr,
                sizeof(client_lla));

            //Receive: Identity Association for Non-temporary Address
            if(get_optval6((uint8_t*)&gstate.rcvd.rcvd_pkt6.options,
                  DHCP6_OPT_IA_NA, &client_ia_na_len, &client_ia_na)) {
              uint16_t ia_addr_len = 0, status_code_len = 0;
              void *ia_addr, *status_code;
              uint16_t server_ia_na_len = sizeof(struct optval_ia_na);
              char *status_code_msg;

              //Check IA Address
              get_optval6((uint8_t*)(*(struct optval_ia_na*)client_ia_na).optval,
                  DHCP6_OPT_IA_ADDR, &ia_addr_len, &ia_addr);
              struct optval_ia_addr *ia_addr_p = (struct optval_ia_addr*)ia_addr;
              if(verifyip6_in_lease((*ia_addr_p).ipv6_addr, client_duid,
                    DHCP6_OPT_IA_NA, (*(struct optval_ia_na*)client_ia_na).iaid)
                  == -1) {
                server_ia_na_len += (ia_addr_len + 4);
                //Add Status Code
                status_code_msg = xstrdup("Assigned an address.");
                status_code_len = strlen(status_code_msg) + 1;
                status_code = xzalloc(status_code_len);
                struct optval_status_code *status_code_p =
                  (struct optval_status_code*)status_code;
                (*status_code_p).status_code = htons(DHCP6_STATUS_SUCCESS);
                memcpy((*status_code_p).status_msg, status_code_msg,
                    status_code_len);
                server_ia_na_len += (status_code_len+4);
              } else {
                //TODO send failed status code
                break;
              }

              //combine options
              server_ia_na = xzalloc(server_ia_na_len);
              struct optval_ia_na *ia_na_p = (struct optval_ia_na*)server_ia_na;
              (*ia_na_p).iaid = (*(struct optval_ia_na*)client_ia_na).iaid;
              (*ia_na_p).t1 = gconfig.t1;
              (*ia_na_p).t2 = gconfig.t2;

              uint8_t* ia_na_optptr = (*ia_na_p).optval;
              ia_na_optptr = set_optval6(ia_na_optptr, DHCP6_OPT_IA_ADDR,
                  ia_addr, ia_addr_len);
              free(ia_addr);

              if(status_code_len) {
                ia_na_optptr = set_optval6(ia_na_optptr, DHCP6_OPT_STATUS_CODE,
                    status_code, status_code_len);
                free(status_code);
              }

              //Response: Identity Association for Non-temporary Address
              //(Status Code added)
              optptr = set_optval6(optptr, DHCP6_OPT_IA_NA,
                  server_ia_na, server_ia_na_len);
              optlen += (server_ia_na_len + 4);
              free(client_ia_na);free(server_ia_na);
            }

            //Receive: Server Identifier (DUID)
            get_optval6((uint8_t*)&gstate.rcvd.rcvd_pkt6.options,
                DHCP6_OPT_SERVERID, &server_duid_len, &server_duid);
            optptr = set_optval6(optptr, DHCP6_OPT_SERVERID,
                server_duid, server_duid_len);
            optlen += (server_duid_len + 4);

            free(client_duid); free(server_duid);

            send_packet6(0, client_lla, optlen);
            write_lease6file();
            break;
          case DHCP6DECLINE:  //TODO
          case DHCP6RENEW:    //TODO
          case DHCP6REBIND:   //TODO
          case DHCP6RELEASE:
            dbg("Message Type: DHCP6RELEASE\n");
            optptr = prepare_send_pkt6(DHCP6REPLY);
            break;
          default:
            dbg("Message Type : %u\n", gstate.rqcode);
            break;
        }

      } else {
        if(read_packet() < 0) {
          open_listensock();
          continue;
        }
        waited += time(NULL) - timestmp;

        get_optval((uint8_t*)&gstate.rcvd.rcvd_pkt.options,
            DHCP_OPT_MESSAGE_TYPE, &gstate.rqcode);
        if (gstate.rqcode == 0 || gstate.rqcode < DHCPDISCOVER
            || gstate.rqcode > DHCPINFORM) {
          dbg("no or bad message type option, ignoring packet.\n");
          continue;
        }
        get_optval((uint8_t*) &gstate.rcvd.rcvd_pkt.options,
            DHCP_OPT_SERVER_ID, &serverid);
        if (serverid && (serverid != gconfig.server_nip)) {
          dbg("server ID doesn't match, ignoring packet.\n");
          continue;
        }

        waited += time(NULL) - timestmp;
        switch (gstate.rqcode) {
          case DHCPDISCOVER:
            msgtype = DHCPOFFER;
            dbg("Message Type : DHCPDISCOVER\n");
            get_optval((uint8_t*) &gstate.rcvd.rcvd_pkt.options,
                DHCP_OPT_REQUESTED_IP, &requested_nip);
            get_optval((uint8_t*) &gstate.rcvd.rcvd_pkt.options,
                DHCP_OPT_HOST_NAME, &hstname);
            reqested_lease = gconfig.offer_time;
            get_reqparam(&gstate.rqopt);
            optptr = prepare_send_pkt();
            gstate.send.send_pkt.yiaddr = getip_from_pool(requested_nip,
                gstate.rcvd.rcvd_pkt.chaddr, &reqested_lease, hstname);
            if(!gstate.send.send_pkt.yiaddr){
              msgtype = DHCPNAK;
              optptr = set_optval(optptr, DHCP_OPT_MESSAGE_TYPE, &msgtype, 1);
              send_packet(1);
              break;
            }
            get_optval((uint8_t*) &gstate.rcvd.rcvd_pkt.options,
                DHCP_OPT_LEASE_TIME, &reqested_lease);
            reqested_lease = htonl(get_lease(reqested_lease + time(NULL)));
            optptr = set_optval(optptr, DHCP_OPT_MESSAGE_TYPE, &msgtype, 1);
            optptr = set_optval(optptr, DHCP_OPT_SERVER_ID, &gconfig.server_nip, 4);
            optptr = set_optval(optptr, DHCP_OPT_LEASE_TIME, &reqested_lease, 4);
            optptr = set_reqparam(optptr, gstate.rqopt);
            send_packet(1);
            break;
          case DHCPREQUEST:
            msgtype = DHCPACK;
            dbg("Message Type : DHCPREQUEST\n");
            optptr = prepare_send_pkt();
            get_optval((uint8_t*) &gstate.rcvd.rcvd_pkt.options,
                DHCP_OPT_REQUESTED_IP, &requested_nip);
            get_optval((uint8_t*) &gstate.rcvd.rcvd_pkt.options,
                DHCP_OPT_LEASE_TIME, &reqested_lease);
            get_optval((uint8_t*) &gstate.rcvd.rcvd_pkt.options,
                DHCP_OPT_HOST_NAME, &hstname);
            gstate.send.send_pkt.yiaddr = getip_from_pool(requested_nip,
                gstate.rcvd.rcvd_pkt.chaddr, &reqested_lease, hstname);
            if (!serverid) reqested_lease = gconfig.max_lease_sec;
            if (!gstate.send.send_pkt.yiaddr) {
              msgtype = DHCPNAK;
              optptr = set_optval(optptr, DHCP_OPT_MESSAGE_TYPE, &msgtype, 1);
              send_packet(1);
              break;
            }
            optptr = set_optval(optptr, DHCP_OPT_MESSAGE_TYPE, &msgtype, 1);
            optptr = set_optval(optptr, DHCP_OPT_SERVER_ID, &gconfig.server_nip, 4);
            reqested_lease = htonl(reqested_lease);
            optptr = set_optval(optptr, DHCP_OPT_LEASE_TIME, &reqested_lease, 4);
            send_packet(1);
            write_leasefile();
            break;
          case DHCPDECLINE:// FALL THROUGH
          case DHCPRELEASE:
            dbg("Message Type : DHCPDECLINE or DHCPRELEASE \n");
            get_optval((uint8_t*) &gstate.rcvd.rcvd_pkt.options,
                DHCP_OPT_SERVER_ID, &serverid);
            if (serverid != gconfig.server_nip) break;
            get_optval((uint8_t*) &gstate.rcvd.rcvd_pkt.options,
                DHCP_OPT_REQUESTED_IP, &requested_nip);
            delip_from_lease(requested_nip, gstate.rcvd.rcvd_pkt.chaddr,
                (gstate.rqcode==DHCPRELEASE)?0:gconfig.decline_time);
            break;
          default:
            dbg("Message Type : %u\n", gstate.rqcode);
            break;
        }
      }
    }
  }
}
