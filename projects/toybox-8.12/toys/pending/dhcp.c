/* dhcp.c - DHCP client for dynamic network configuration.
 *
 * Copyright 2012 Madhur Verma <mad.flexi@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * See RFC 2132, and 3442 (option 121)

USE_DHCP(NEWTOY(dhcp, "V:H:F:x*r:O*A#<0=20T#<0=3t#<0=3s:p:i:SBRCaovqnbf", TOYFLAG_SBIN|TOYFLAG_ROOTONLY))

config DHCP
  bool "dhcp"
  default n
  help
   usage: dhcp [-fbnqvoCRB] [-i IFACE] [-r IP] [-s PROG] [-p PIDFILE]
               [-H HOSTNAME] [-V VENDOR] [-x OPT:VAL] [-O OPT]

        Configure network dynamically using DHCP.

      -i Interface to use (default eth0)
      -p Create pidfile
      -s Run PROG at DHCP events (default /usr/share/dhcp/default.script)
      -B Request broadcast replies
      -t Send up to N discover packets
      -T Pause between packets (default 3 seconds)
      -A Wait N seconds after failure (default 20)
      -f Run in foreground
      -b Background if lease is not obtained
      -n Exit if lease is not obtained
      -q Exit after obtaining lease
      -R Release IP on exit
      -S Log to syslog too
      -a Use arping to validate offered address
      -O Request option OPT from server (cumulative)
      -o Don't request any options (unless -O is given)
      -r Request this IP address
      -x OPT:VAL  Include option OPT in sent packets (cumulative)
      -F Ask server to update DNS mapping for NAME
      -H Send NAME as client hostname (default none)
      -V VENDOR Vendor identifier (default 'toybox VERSION')
      -C Don't send MAC as client identifier
      -v Verbose

      Signals:
      USR1  Renew current lease
      USR2  Release current lease

*/

#define FOR_dhcp
#include "toys.h"

// TODO: headers not in posix:
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>

#include <linux/filter.h> //FIXME: linux specific. fix for other OS ports
#include <linux/if_ether.h>

GLOBALS(
    char *iface;
    char *pidfile;
    char *script;
    long retries;
    long timeout;
    long tryagain;
    struct arg_list *req_opt;
    char *req_ip;
    struct arg_list *pkt_opt;
    char *fdn_name;
    char *hostname;
    char *vendor_cls;
)

#define STATE_INIT            0
#define STATE_REQUESTING      1
#define STATE_BOUND           2
#define STATE_RENEWING        3
#define STATE_REBINDING       4
#define STATE_RENEW_REQUESTED 5
#define STATE_RELEASED        6

#define BOOTP_BROADCAST   0x8000
#define DHCP_MAGIC        0x63825363

#define DHCP_REQUEST          1
#define DHCP_REPLY            2
#define DHCP_HTYPE_ETHERNET   1

#define DHCPC_SERVER_PORT     67
#define DHCPC_CLIENT_PORT     68

#define DHCPDISCOVER      1
#define DHCPOFFER         2
#define DHCPREQUEST       3
#define DHCPACK           5
#define DHCPNAK           6
#define DHCPRELEASE       7

#define DHCP_OPTION_PADDING     0x00
#define DHCP_OPTION_SUBNET_MASK 0x01
#define DHCP_OPTION_ROUTER      0x03
#define DHCP_OPTION_DNS_SERVER  0x06
#define DHCP_OPTION_HOST_NAME   0x0c
#define DHCP_OPTION_BROADCAST   0x1c
#define DHCP_OPTION_REQ_IPADDR  0x32
#define DHCP_OPTION_LEASE_TIME  0x33
#define DHCP_OPTION_OVERLOAD    0x34
#define DHCP_OPTION_MSG_TYPE    0x35
#define DHCP_OPTION_SERVER_ID   0x36
#define DHCP_OPTION_REQ_LIST    0x37
#define DHCP_OPTION_MAX_SIZE    0x39
#define DHCP_OPTION_CLIENTID    0x3D
#define DHCP_OPTION_VENDOR      0x3C
#define DHCP_OPTION_FQDN        0x51
#define DHCP_OPTION_END         0xFF

#define DHCP_NUM8           (1<<8)
#define DHCP_NUM16          (1<<9)
#define DHCP_NUM32          DHCP_NUM16 | DHCP_NUM8
#define DHCP_STRING         (1<<10)
#define DHCP_STRLST         (1<<11)
#define DHCP_IP             (1<<12)
#define DHCP_IPLIST         (1<<13)
#define DHCP_IPPLST         (1<<14)
#define DHCP_STCRTS         (1<<15)

#define LOG_SILENT          0x0
#define LOG_CONSOLE         0x1
#define LOG_SYSTEM          0x2

#define MODE_OFF        0
#define MODE_RAW        1
#define MODE_APP        2

static void (*dbg)(char *format, ...);
static void dummy(char *format, ...){
  return;
}

typedef struct dhcpc_result_s {
  struct in_addr serverid;
  struct in_addr ipaddr;
  struct in_addr netmask;
  struct in_addr dnsaddr;
  struct in_addr default_router;
  uint32_t lease_time;
} dhcpc_result_t;

typedef struct __attribute__((packed)) dhcp_msg_s {
  uint8_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;
  uint32_t xid;
  uint16_t secs;
  uint16_t flags;
  uint32_t ciaddr;
  uint32_t yiaddr;
  uint32_t nsiaddr;
  uint32_t ngiaddr;
  uint8_t chaddr[16];
  uint8_t sname[64];
  uint8_t file[128];
  uint32_t cookie;
  uint8_t options[308];
} dhcp_msg_t;

typedef struct __attribute__((packed)) dhcp_raw_s {
  struct iphdr iph;
  struct udphdr udph;
  dhcp_msg_t dhcp;
} dhcp_raw_t;

typedef struct dhcpc_state_s {
  uint8_t macaddr[6];
   char *iface;
  int ifindex;
  int sockfd;
  int status;
  int mode;
  uint32_t mask;
  struct in_addr ipaddr;
  struct in_addr serverid;
  dhcp_msg_t pdhcp;
} dhcpc_state_t;

typedef struct option_val_s {
  char *key;
  uint16_t code;
  void *val;
  size_t len;
} option_val_t;

struct fd_pair { int rd; int wr; };
static uint32_t xid;
static dhcpc_state_t *state;
static struct fd_pair sigfd;
uint8_t bmacaddr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
 int set = 1;
uint8_t infomode = LOG_CONSOLE;
uint8_t raw_opt[29];
int raw_optcount = 0;
struct arg_list *x_opt;
in_addr_t server = 0;

static option_val_t *msgopt_list = NULL;
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

static  struct sock_filter filter_instr[] = {
    BPF_STMT(BPF_LD|BPF_B|BPF_ABS, 9),
    BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, IPPROTO_UDP, 0, 6),
    BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 6),
    BPF_JUMP(BPF_JMP|BPF_JSET|BPF_K, 0x1fff, 4, 0),
    BPF_STMT(BPF_LDX|BPF_B|BPF_MSH, 0), BPF_STMT(BPF_LD|BPF_H|BPF_IND, 2),
    BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 68, 0, 1),
    BPF_STMT(BPF_RET|BPF_K, 0xffffffff), BPF_STMT(BPF_RET|BPF_K, 0),
};

static  struct sock_fprog filter_prog = {
    .len = ARRAY_LEN(filter_instr),
    .filter = (struct sock_filter *) filter_instr,
};

// calculate options size.
static int dhcp_opt_size(uint8_t *optionptr)
{
  int i = 0;
  for(;optionptr[i] != 0xff; i++) if(optionptr[i] != 0x00) i += optionptr[i + 1] + 2 -1;
  return i;
}

// calculates checksum for dhcp messages.
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
static int get_interface( char *interface, int *ifindex, uint32_t *oip, uint8_t *mac)
{
  struct ifreq req;
  struct sockaddr_in *ip;
  int fd = xsocket(AF_INET, SOCK_RAW, IPPROTO_RAW);

  req.ifr_addr.sa_family = AF_INET;
  xstrncpy(req.ifr_name, interface, IFNAMSIZ);
  req.ifr_name[IFNAMSIZ-1] = '\0';

  xioctl(fd, SIOCGIFFLAGS, &req);
  if (!(req.ifr_flags & IFF_UP)) return -1;

  if (oip) {
    xioctl(fd, SIOCGIFADDR, &req);
    ip = (struct sockaddr_in*) &req.ifr_addr;
    dbg("IP %s\n", inet_ntoa(ip->sin_addr));
    *oip = ntohl(ip->sin_addr.s_addr);
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
static void infomsg(uint8_t infomode,  char *s, ...)
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

// String STR to UINT32 conversion strored in VAR
static long strtou32( char *str)
{
  char *endptr = NULL;
  int base = 10;
  errno=0;
  if (str[0]=='0' && (str[1]=='x' || str[1]=='X')) {
    base = 16;
    str+=2;
  }
  long ret_val = strtol(str, &endptr, base);
  if (errno) return -1;
  else if (endptr && (*endptr!='\0'||endptr == str)) return -1;
  return ret_val;
}

// IP String STR to binary data.
static int striptovar( char *str, void *var)
{
  in_addr_t addr;
  if(!str) error_exit("NULL address string.");
  addr = inet_addr(str);
  if(addr == -1) error_exit("Wrong address %s.",str );
  *((uint32_t*)(var)) = (uint32_t)addr;
  return 0;
}

// String to dhcp option conversion
static int strtoopt( char *str, uint8_t optonly)
{
  char *option, *valstr, *grp, *tp;
  long optcode = 0, convtmp;
  uint16_t flag = 0;
  uint32_t mask, nip, router;
  int count, size = ARRAY_LEN(options_list);

  if (!*str) return 0;
  option = strtok((char*)str, ":");
  if (!option) return -1;

  dbg("-x option : %s ", option);
  optcode = strtou32(option);

  if (optcode > 0 && optcode < 256) {         // raw option
    for (count = 0; count < size; count++) {
      if ((options_list[count].code & 0X00FF) == optcode) {
        flag = (options_list[count].code & 0XFF00);
        break;
      }
    }
    if (count == size) error_exit("Obsolete OR Unknown Option : %s", option);
  } else {    // string option
    for (count = 0; count < size; count++) {
      if (!strcmp(options_list[count].key, option)) {
        flag = (options_list[count].code & 0XFF00);
        optcode = (options_list[count].code & 0X00FF);
        break;
      }
    }
    if (count == size) error_exit("Obsolete OR Unknown Option : %s", option);
  }
  if (!flag || !optcode) return -1;
  if (optonly) return optcode;

  valstr = strtok(NULL, "\n");
  if (!valstr) error_exit("option %s has no value defined.\n", option);
  dbg(" value : %-20s \n ", valstr);
  switch (flag) {
  case DHCP_NUM32:
    options_list[count].len = sizeof(uint32_t);
    options_list[count].val = xmalloc(sizeof(uint32_t));
    convtmp = strtou32(valstr);
    if (convtmp < 0) error_exit("Invalid/wrong formatted number %s", valstr);
    convtmp = htonl(convtmp);
    memcpy(options_list[count].val, &convtmp, sizeof(uint32_t));
    break;
  case DHCP_NUM16:
    options_list[count].len = sizeof(uint16_t);
    options_list[count].val = xmalloc(sizeof(uint16_t));
    convtmp = strtou32(valstr);
    if (convtmp < 0) error_exit("Invalid/malformed number %s", valstr);
    convtmp = htons(convtmp);
    memcpy(options_list[count].val, &convtmp, sizeof(uint16_t));
    break;
  case DHCP_NUM8:
    options_list[count].len = sizeof(uint8_t);
    options_list[count].val = xmalloc(sizeof(uint8_t));
    convtmp = strtou32(valstr);
    if (convtmp < 0) error_exit("Invalid/malformed number %s", valstr);
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
  case DHCP_STRLST:
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
      if (!tp) error_exit("malformed static route option");
      *tp = '\0';
      mask = strtol(++tp, &tp, 10);
      if (striptovar(grp, (uint8_t*)&nip) < 0) error_exit("malformed static route option");
      while(*tp == ' ' || *tp == '\t' || *tp == '-') tp++;
      if (striptovar(tp, (uint8_t*)&router) < 0) error_exit("malformed static route option");
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

// Creates environment pointers from RES to use in script
static int fill_envp(dhcpc_result_t *res)
{
  struct in_addr temp;
  int size = ARRAY_LEN(options_list), count, ret = -1;

  ret = setenv("interface", state->iface, 1);
  if (!res) return ret;
  if (res->ipaddr.s_addr) {
      temp.s_addr = htonl(res->ipaddr.s_addr);
      ret = setenv("ip", inet_ntoa(temp), 1);
      if (ret) return ret;
  }
  if (msgopt_list) {
    for (count = 0; count < size; count++) {
        if ((msgopt_list[count].len == 0) || (msgopt_list[count].val == NULL)) continue;
        ret = setenv(msgopt_list[count].key, (char*)msgopt_list[count].val, 1);
        if (ret) return ret;
      }
  }
  return ret;
}

// Executes Script NAME.
static void run_script(dhcpc_result_t *res,  char *name)
{
  volatile int error = 0;
  pid_t pid;
  char *argv[3];
  struct stat sts;
  char *script = (toys.optflags & FLAG_s) ? TT.script
    : "/usr/share/dhcp/default.script";

  if (stat(script, &sts) == -1 && errno == ENOENT) return;
  if (fill_envp(res)) {
    dbg("Failed to create environment variables.");
    return;
  }
  dbg("Executing %s %s\n", script, name);
  argv[0] = (char*) script;
  argv[1] = (char*) name;
  argv[2] = NULL;
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
    waitpid(pid, NULL,0);
    errno = error;
    perror_msg("script exec failed");
  }
  dbg("script complete.\n");
}

// returns a randome ID
static uint32_t getxid(void)
{
  uint32_t randnum;
  int fd = xopenro("/dev/urandom");

// TODO xreadfile
  xreadall(fd, &randnum, sizeof(randnum));
  xclose(fd);
  return randnum;
}

// opens socket in raw mode.
static int mode_raw(void)
{
  state->mode = MODE_OFF;
  struct sockaddr_ll sock;

  if (state->sockfd > 0) close(state->sockfd);
  dbg("Opening raw socket on ifindex %d\n", state->ifindex);

  state->sockfd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
  if (state->sockfd < 0) {
    dbg("MODE RAW : socket fail ERROR : %d\n", state->sockfd);
    return -1;
  }
  dbg("Got raw socket fd %d\n", state->sockfd);
  memset(&sock, 0, sizeof(sock));
  sock.sll_family = AF_PACKET;
  sock.sll_protocol = htons(ETH_P_IP);
  sock.sll_ifindex = state->ifindex;

  if (bind(state->sockfd, (struct sockaddr *) &sock, sizeof(sock))) {
    dbg("MODE RAW : bind fail.\n");
    close(state->sockfd);
    return -1;
  }
  state->mode = MODE_RAW;
  if (setsockopt(state->sockfd, SOL_SOCKET, SO_ATTACH_FILTER, &filter_prog, sizeof(filter_prog)) < 0)
    dbg("MODE RAW : filter attach fail.\n");

  dbg("MODE RAW : success\n");
  return 0;
}

// opens UDP socket
static int mode_app(void)
{
  struct sockaddr_in addr;
  struct ifreq ifr;

  state->mode = MODE_OFF;
  if (state->sockfd > 0) close(state->sockfd);

  dbg("Opening listen socket on *:%d %s\n", DHCPC_CLIENT_PORT, state->iface);
  state->sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (state->sockfd < 0) {
    dbg("MODE APP : socket fail ERROR: %d\n", state->sockfd);
    return -1;
  }
  setsockopt(state->sockfd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set));
  if (setsockopt(state->sockfd, SOL_SOCKET, SO_BROADCAST, &set, sizeof(set)) == -1) {
    dbg("MODE APP : brodcast failed.\n");
    close(state->sockfd);
    return -1;
  }
  xstrncpy(ifr.ifr_name, state->iface, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ -1] = '\0';
  setsockopt(state->sockfd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr));

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(DHCPC_CLIENT_PORT);
  addr.sin_addr.s_addr = INADDR_ANY ;

  if (bind(state->sockfd, (struct sockaddr *) &addr, sizeof(addr))) {
    close(state->sockfd);
    dbg("MODE APP : bind failed.\n");
    return -1;
  }
  state->mode = MODE_APP;
  dbg("MODE APP : success\n");
  return 0;
}

static int read_raw(void)
{
  dhcp_raw_t packet;
  int bytes = 0;

  memset(&packet, 0, sizeof(packet));
  if ((bytes = read(state->sockfd, &packet, sizeof(packet))) < 0) {
    dbg("\tPacket read error, ignoring\n");
    return bytes;
  }
  if (bytes < (int) (sizeof(packet.iph) + sizeof(packet.udph))) {
    dbg("\tPacket is too short, ignoring\n");
    return -2;
  }
  if (bytes < ntohs(packet.iph.tot_len)) {
    dbg("\tOversized packet, ignoring\n");
    return -2;
  }
  // ignore any extra garbage bytes
  bytes = ntohs(packet.iph.tot_len);
  // make sure its the right packet for us, and that it passes sanity checks
  if (packet.iph.protocol != IPPROTO_UDP || packet.iph.version != IPVERSION
   || packet.iph.ihl != (sizeof(packet.iph) >> 2)
   || packet.udph.dest != htons(DHCPC_CLIENT_PORT)
   || ntohs(packet.udph.len) != (uint16_t)(bytes - sizeof(packet.iph))) {
    dbg("\tUnrelated/bogus packet, ignoring\n");
    return -2;
  }
  // Verify IP checksum.
  if (dhcp_checksum(&packet.iph, sizeof(packet.iph)) != 0) {
    dbg("\tBad IP header checksum, ignoring\n");
    return -2;
  }
  // Verify UDP checksum. From RFC 768, the UDP checksum is done over the IPv4
  // pseudo header, the UDP header and the UDP data. The IPv4 pseudo header
  // includes saddr, daddr, protocol, and UDP length. The IP header has to be
  // modified for this.
  memset(&packet.iph, 0, ((size_t) &((struct iphdr *)0)->protocol));
  packet.iph.check = 0;
  packet.iph.tot_len = packet.udph.len;
  if (packet.udph.check != 0 && dhcp_checksum(&packet, bytes) != 0) {
    dbg("\tPacket with bad UDP checksum received, ignoring\n");
    return -2;
  }
  memcpy(&state->pdhcp, &packet.dhcp, bytes - (sizeof(packet.iph) + sizeof(packet.udph)));
  if (state->pdhcp.cookie != htonl(DHCP_MAGIC)) {
    dbg("\tPacket with bad magic, ignoring\n");
    return -2;
  }
  return bytes - sizeof(packet.iph) - sizeof(packet.udph);
}

static int read_app(void)
{
  int ret;

  memset(&state->pdhcp, 0, sizeof(dhcp_msg_t));
  if ((ret = read(state->sockfd, &state->pdhcp, sizeof(dhcp_msg_t))) < 0) {
    dbg("Packet read error, ignoring\n");
    return ret; /* returns -1 */
  }
  if (state->pdhcp.cookie != htonl(DHCP_MAGIC)) {
    dbg("Packet with bad magic, ignoring\n");
    return -2;
  }
  return ret;
}

// Sends data through raw socket.
static int send_raw(void)
{
  struct sockaddr_ll dest_sll;
  dhcp_raw_t packet;
  unsigned padding;
  int fd, result = -1;

  memset(&packet, 0, sizeof(dhcp_raw_t));
  memcpy(&packet.dhcp, &state->pdhcp, sizeof(dhcp_msg_t));

  if ((fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
    dbg("SEND RAW: socket failed\n");
    return result;
  }
  memset(&dest_sll, 0, sizeof(dest_sll));
  dest_sll.sll_family = AF_PACKET;
  dest_sll.sll_protocol = htons(ETH_P_IP);
  dest_sll.sll_ifindex = state->ifindex;
  dest_sll.sll_halen = 6;
  memcpy(dest_sll.sll_addr, bmacaddr , 6);

  if (bind(fd, (struct sockaddr *) &dest_sll, sizeof(dest_sll)) < 0) {
    dbg("SEND RAW: bind failed\n");
    close(fd);
    return result;
  }
  padding = 308 - 1 - dhcp_opt_size(state->pdhcp.options);
  packet.iph.protocol = IPPROTO_UDP;
  packet.iph.saddr = INADDR_ANY;
  packet.iph.daddr = INADDR_BROADCAST;
  packet.udph.source = htons(DHCPC_CLIENT_PORT);
  packet.udph.dest = htons(DHCPC_SERVER_PORT);
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

  close(fd);
  if (result < 0) dbg("SEND RAW: PACKET send error\n");
  return result;
}

// Sends data through UDP socket.
static int send_app(void)
{
  struct sockaddr_in cli;
  int fd, ret = -1;

  if ((fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    dbg("SEND APP: sock failed.\n");
    return ret;
  }
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set));

  memset(&cli, 0, sizeof(cli));
  cli.sin_family = AF_INET;
  cli.sin_port = htons(DHCPC_CLIENT_PORT);
  cli.sin_addr.s_addr = state->pdhcp.ciaddr;
  if (bind(fd, (struct sockaddr *)&cli, sizeof(cli)) == -1) {
    dbg("SEND APP: bind failed.\n");
    goto error_fd;
  }
  memset(&cli, 0, sizeof(cli));
  cli.sin_family = AF_INET;
  cli.sin_port = htons(DHCPC_SERVER_PORT);
  cli.sin_addr.s_addr = state->serverid.s_addr;
  if (connect(fd, (struct sockaddr *)&cli, sizeof(cli)) == -1) {
    dbg("SEND APP: connect failed.\n");
    goto error_fd;
  }
  int padding = 308 - 1 - dhcp_opt_size(state->pdhcp.options);
  if((ret = write(fd, &state->pdhcp, sizeof(dhcp_msg_t) - padding)) < 0) {
    dbg("SEND APP: write failed error %d\n", ret);
    goto error_fd;
  }
  dbg("SEND APP: write success wrote %d\n", ret);
error_fd:
  close(fd);
  return ret;
}

// Generic signal handler real handling is done in main funcrion.
static void signal_handler(int sig)
{
  unsigned char ch = sig;
  if (write(sigfd.wr, &ch, 1) != 1) dbg("can't send signal\n");
}

// signal setup for SIGUSR1 SIGUSR2 SIGTERM
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
  signal(SIGUSR2, signal_handler);
  signal(SIGTERM, signal_handler);

  return 0;
}

// adds client id to dhcp packet
static uint8_t *dhcpc_addclientid(uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_CLIENTID;
  *optptr++ = 7;
  *optptr++ = 1;
  memcpy(optptr, &state->macaddr, 6);
  return optptr + 6;
}

// adds messege type to dhcp packet
static uint8_t *dhcpc_addmsgtype(uint8_t *optptr, uint8_t type)
{
  *optptr++ = DHCP_OPTION_MSG_TYPE;
  *optptr++ = 1;
  *optptr++ = type;
  return optptr;
}

// adds max size to dhcp packet
static uint8_t *dhcpc_addmaxsize(uint8_t *optptr, uint16_t size)
{
  *optptr++ = DHCP_OPTION_MAX_SIZE;
  *optptr++ = 2;
  memcpy(optptr, &size, 2);
  return optptr + 2;
}

static uint8_t *dhcpc_addstropt(uint8_t *optptr, uint8_t opcode, char* str, int len)
{
  *optptr++ = opcode;
  *optptr++ = len;
  memcpy(optptr, str, len);
  return optptr + len;
}

// adds server id to dhcp packet.
static uint8_t *dhcpc_addserverid(struct in_addr *serverid, uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_SERVER_ID;
  *optptr++ = 4;
  memcpy(optptr, &serverid->s_addr, 4);
  return optptr + 4;
}

// adds requested ip address to dhcp packet.
static uint8_t *dhcpc_addreqipaddr(struct in_addr *ipaddr, uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_REQ_IPADDR;
  *optptr++ = 4;
  memcpy(optptr, &ipaddr->s_addr, 4);
  return optptr + 4;
}

// adds hostname to dhcp packet.
static uint8_t *dhcpc_addfdnname(uint8_t *optptr, char *hname)
{
  int size = strlen(hname);

  *optptr++ = DHCP_OPTION_FQDN;
  *optptr++ = size + 3;
  *optptr++ = 0x1;  //flags
  optptr += 2;      // two blank bytes
  strcpy((char*)optptr, hname); // name

  return optptr + size;
}

// adds request options using -o,-O flag to dhcp packet
static uint8_t *dhcpc_addreqoptions(uint8_t *optptr)
{
  uint8_t *len;

  *optptr++ = DHCP_OPTION_REQ_LIST;
  len = optptr;
  *len = 0;
  optptr++;

  if (!(toys.optflags & FLAG_o)) {
    *len = 4;
    *optptr++ = DHCP_OPTION_SUBNET_MASK;
    *optptr++ = DHCP_OPTION_ROUTER;
    *optptr++ = DHCP_OPTION_DNS_SERVER;
    *optptr++ = DHCP_OPTION_BROADCAST;
  }
  if (toys.optflags & FLAG_O) {
    memcpy(optptr++, raw_opt, raw_optcount);
    *len += raw_optcount;
  }
  return optptr;
}

static uint8_t *dhcpc_addend(uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_END;
  return optptr;
}

// Sets values of -x options in dhcp discover and request packet.
static uint8_t* set_xopt(uint8_t *optptr)
{
  int count;
  int size = ARRAY_LEN(options_list);
  for (count = 0; count < size; count++) {
    if ((options_list[count].len == 0) || (options_list[count].val == NULL)) continue;
    *optptr++ = (uint8_t) (options_list[count].code & 0x00FF);
    *optptr++ = (uint8_t) options_list[count].len;
    memcpy(optptr, options_list[count].val, options_list[count].len);
    optptr += options_list[count].len;
  }
  return optptr;
}

static uint32_t get_option_serverid (uint8_t *opt, dhcpc_result_t *presult)
{
  uint32_t var = 0;
  while (*opt != DHCP_OPTION_SERVER_ID) {
    if (*opt == DHCP_OPTION_END) return var;
    opt += opt[1] + 2;
  }
  memcpy(&var, opt+2, sizeof(uint32_t));
  state->serverid.s_addr = var;
  presult->serverid.s_addr = state->serverid.s_addr;
  presult->serverid.s_addr = ntohl(presult->serverid.s_addr);
  return var;
}

static uint8_t get_option_msgtype(uint8_t *opt)
{
  uint32_t var = 0;
  while (*opt != DHCP_OPTION_MSG_TYPE) {
    if (*opt == DHCP_OPTION_END) return var;
    opt += opt[1] + 2;
  }
  memcpy(&var, opt+2, sizeof(uint8_t));
  return var;
}

static uint8_t get_option_lease(uint8_t *opt, dhcpc_result_t *presult)
{
  uint32_t var = 0;
  while (*opt != DHCP_OPTION_LEASE_TIME) {
    if (*opt == DHCP_OPTION_END) return var;
    opt += opt[1] + 2;
  }
  memcpy(&var, opt+2, sizeof(uint32_t));
  var = htonl(var);
  presult->lease_time = var;
  return var;
}


// sends dhcp msg of MSGTYPE
static int dhcpc_sendmsg(int msgtype)
{
  uint8_t *pend;
  struct in_addr rqsd;
  char *vendor;

  // Create the common message header settings
  memset(&state->pdhcp, 0, sizeof(dhcp_msg_t));
  state->pdhcp.op = DHCP_REQUEST;
  state->pdhcp.htype = DHCP_HTYPE_ETHERNET;
  state->pdhcp.hlen = 6;
  state->pdhcp.xid = xid;
  memcpy(state->pdhcp.chaddr, state->macaddr, 6);
  memset(&state->pdhcp.chaddr[6], 0, 10);
  state->pdhcp.cookie = htonl(DHCP_MAGIC);;

  // Add the common header options
  pend = state->pdhcp.options;
  pend = dhcpc_addmsgtype(pend, msgtype);

  if (!(toys.optflags & FLAG_C)) pend = dhcpc_addclientid(pend);
  // Handle the message specific settings
  switch (msgtype) {
  case DHCPDISCOVER: // Broadcast DISCOVER message to all servers
    state->pdhcp.flags = htons(BOOTP_BROADCAST); //  Broadcast bit.
    if (toys.optflags & FLAG_r) {
      inet_aton(TT.req_ip, &rqsd);
      pend = dhcpc_addreqipaddr(&rqsd, pend);
    }
    pend = dhcpc_addmaxsize(pend, htons(sizeof(dhcp_raw_t)));
    vendor = (toys.optflags & FLAG_V) ? TT.vendor_cls : "toybox\0";
    pend = dhcpc_addstropt(pend, DHCP_OPTION_VENDOR, vendor, strlen(vendor));
    if (toys.optflags & FLAG_H) pend = dhcpc_addstropt(pend, DHCP_OPTION_HOST_NAME, TT.hostname, strlen(TT.hostname));
    if (toys.optflags & FLAG_F) pend = dhcpc_addfdnname(pend, TT.fdn_name);
    if (!(toys.optflags & FLAG_o) || (toys.optflags & FLAG_O))
      pend = dhcpc_addreqoptions(pend);
    if (toys.optflags & FLAG_x) pend = set_xopt(pend);
    break;
  case DHCPREQUEST: // Send REQUEST message to the server that sent the *first* OFFER
    state->pdhcp.flags = htons(BOOTP_BROADCAST); //  Broadcast bit.
    if (state->status == STATE_RENEWING) memcpy(&state->pdhcp.ciaddr, &state->ipaddr.s_addr, 4);
    pend = dhcpc_addmaxsize(pend, htons(sizeof(dhcp_raw_t)));
    rqsd.s_addr = htonl(server);
    pend = dhcpc_addserverid(&rqsd, pend);
    pend = dhcpc_addreqipaddr(&state->ipaddr, pend);
    vendor = (toys.optflags & FLAG_V) ? TT.vendor_cls : "toybox\0";
    pend = dhcpc_addstropt(pend, DHCP_OPTION_VENDOR, vendor, strlen(vendor));
    if (toys.optflags & FLAG_H) pend = dhcpc_addstropt(pend, DHCP_OPTION_HOST_NAME, TT.hostname, strlen(TT.hostname));
    if (toys.optflags & FLAG_F) pend = dhcpc_addfdnname(pend, TT.fdn_name);
    if (!(toys.optflags & FLAG_o) || (toys.optflags & FLAG_O))
      pend = dhcpc_addreqoptions(pend);
    if (toys.optflags & FLAG_x) pend = set_xopt(pend);
    break;
  case DHCPRELEASE: // Send RELEASE message to the server.
    memcpy(&state->pdhcp.ciaddr, &state->ipaddr.s_addr, 4);
    rqsd.s_addr = htonl(server);
    pend = dhcpc_addserverid(&rqsd, pend);
    break;
  default:
    return -1;
  }
  pend = dhcpc_addend(pend);

  if (state->mode == MODE_APP) return send_app();
  return send_raw();
}

/*
 * parses options from received dhcp packet at OPTPTR and
 * stores result in PRESULT or MSGOPT_LIST
 */
static uint8_t dhcpc_parseoptions(dhcpc_result_t *presult, uint8_t *optptr)
{
  uint8_t type = 0, *options, overloaded = 0;;
  uint16_t flag = 0;
  char *dest, *pfx;
  int count, optlen, size = ARRAY_LEN(options_list);

  if (toys.optflags & FLAG_x) {
    if(msgopt_list){
      for (count = 0; count < size; count++){
        if(msgopt_list[count].val) free(msgopt_list[count].val);
        msgopt_list[count].val = NULL;
        msgopt_list[count].len = 0;
      }
    } else {
     msgopt_list = xmalloc(sizeof(options_list));
     memcpy(msgopt_list, options_list, sizeof(options_list));
     for (count = 0; count < size; count++) {
         msgopt_list[count].len = 0;
         msgopt_list[count].val = NULL;
     }
    }
  } else {
    msgopt_list = options_list;
    for (count = 0; count < size; count++) {
      msgopt_list[count].len = 0;
      if(msgopt_list[count].val) free(msgopt_list[count].val);
      msgopt_list[count].val = NULL;
    }
  }

  while (*optptr != DHCP_OPTION_END) {
    if (*optptr == DHCP_OPTION_PADDING) {
      optptr++;
      continue;
    }
    if (*optptr == DHCP_OPTION_OVERLOAD) {
      overloaded = optptr[2];
      optptr += optptr[1] + 2;
      continue;
    }
    for (count = 0, flag = 0; count < size; count++) {
      if ((msgopt_list[count].code & 0X00FF) == *optptr) {
        flag = (msgopt_list[count].code & 0XFF00);
        break;
      }
    }
    msgopt_list[count].val = 0;
    msgopt_list[count].len = 0;
    switch (flag) {
    case DHCP_NUM32:
      msgopt_list[count].val = xmprintf("%llu", peek_be(optptr+2, 4));
      break;
    case DHCP_NUM16:
      msgopt_list[count].val = xmprintf("%llu", peek_be(optptr+2, 2));
      break;
    case DHCP_NUM8:
      msgopt_list[count].val = xmprintf("%llu", peek_be(optptr+2, 1));
      break;
    case DHCP_IP:
      msgopt_list[count].val = xstrdup(inet_ntoa((struct in_addr){peek(optptr+2, 4)}));
      break;
    case DHCP_STRING:
      msgopt_list[count].val = xmprintf("%.*s", optptr[1], optptr+2);
      break;
    case DHCP_IPLIST:
      options = &optptr[2];
      optlen = optptr[1];
      dest = toybuf;
      while (optlen) {
        dest += sprintf(dest, "%s ", inet_ntoa((struct in_addr){peek(options, 4)}));
        options += 4;
        optlen -= 4;
      }
      *(dest - 1) = '\0';
      msgopt_list[count].val = strdup(toybuf);
      break;
    case DHCP_STRLST: //FIXME: do smthing.
    case DHCP_IPPLST:
      break;
    case DHCP_STCRTS:
      pfx = "";
      dest = toybuf;
      options = &optptr[2];
      optlen = optptr[1];

      while (optlen >= 1 + 4) {
        uint32_t nip = 0;
        int bytes;
        uint8_t *p_tmp;
        unsigned mask = *options;

        if (mask > 32) break;
        optlen--;
        p_tmp = (void*) &nip;
        bytes = (mask + 7) / 8;
        while (--bytes >= 0) {
          *p_tmp++ = *options++;
          optlen--;
        }
        if (optlen < 4) break;
        dest += sprintf(dest, "%s%u.%u.%u.%u", pfx, ((uint8_t*) &nip)[0],
            ((uint8_t*) &nip)[1], ((uint8_t*) &nip)[2], ((uint8_t*) &nip)[3]);
        pfx = " ";
        dest += sprintf(dest, "/%u ", mask);
        dest += sprintf(dest, "%u.%u.%u.%u", options[0], options[1], options[2], options[3]);
        options += 4;
        optlen -= 4;
      }
      msgopt_list[count].val = strdup(toybuf);
      break;
    default: break;
    }
    if (msgopt_list[count].val)
      msgopt_list[count].len = strlen(msgopt_list[count].val);

    optptr += optptr[1] + 2;
  }
  if ((overloaded == 1) || (overloaded == 3)) dhcpc_parseoptions(presult, optptr);
  if ((overloaded == 2) || (overloaded == 3)) dhcpc_parseoptions(presult, optptr);
  return type;
}

// parses recvd messege to check that it was for us.
static uint8_t dhcpc_parsemsg(dhcpc_result_t *presult)
{
  if (state->pdhcp.op == DHCP_REPLY
      && !memcmp(state->pdhcp.chaddr, state->macaddr, 6)
      && !memcmp(&state->pdhcp.xid, &xid, sizeof(xid))) {
    memcpy(&presult->ipaddr.s_addr, &state->pdhcp.yiaddr, 4);
    presult->ipaddr.s_addr = ntohl(presult->ipaddr.s_addr);
    return get_option_msgtype(state->pdhcp.options);
  }
  return 0;
}

// Sends a IP renew request.
static void renew(void)
{
  infomsg(infomode, "Performing a DHCP renew");
  switch (state->status) {
  case STATE_INIT:
    break;
  case STATE_BOUND:
    mode_raw();
  case STATE_RENEWING:    // FALLTHROUGH
  case STATE_REBINDING:   // FALLTHROUGH
    state->status = STATE_RENEW_REQUESTED;
    break;
  case STATE_RENEW_REQUESTED:
    run_script(NULL, "deconfig");
  case STATE_REQUESTING:           // FALLTHROUGH
  case STATE_RELEASED:             // FALLTHROUGH
    mode_raw();
    state->status = STATE_INIT;
    break;
  default: break;
  }
}

// Sends a IP release request.
static void release(void)
{
  char buffer[sizeof("255.255.255.255\0")];
  struct in_addr temp_addr;

  mode_app();
  // send release packet
  if (state->status == STATE_BOUND || state->status == STATE_RENEWING || state->status == STATE_REBINDING) {
    temp_addr.s_addr = htonl(server);
    xstrncpy(buffer, inet_ntoa(temp_addr), sizeof(buffer));
    temp_addr.s_addr = state->ipaddr.s_addr;
    infomsg( infomode, "Unicasting a release of %s to %s", inet_ntoa(temp_addr), buffer);
    dhcpc_sendmsg(DHCPRELEASE);
    run_script(NULL, "deconfig");
  }
  infomsg(infomode, "Entering released state");
  close(state->sockfd);
  state->sockfd = -1;
  state->mode = MODE_OFF;
  state->status = STATE_RELEASED;
}

static void free_option_stores(void)
{
  int count, size = ARRAY_LEN(options_list);
  for (count = 0; count < size; count++)
    if (options_list[count].val) free(options_list[count].val);
  if (toys.optflags & FLAG_x) {
    for (count = 0; count < size; count++)
        if (msgopt_list[count].val) free(msgopt_list[count].val);
    free(msgopt_list);
  }
}

void dhcp_main(void)
{
  struct timeval tv;
  int retval, bufflen = 0;
  dhcpc_result_t result;
  uint8_t packets = 0, retries = 0;
  uint32_t timeout = 0, waited = 0;
  fd_set rfds;

  xid = 0;
  setlinebuf(stdout);
  dbg = dummy;
  if (toys.optflags & FLAG_v) dbg = xprintf;
  if (toys.optflags & FLAG_p) write_pid(TT.pidfile);
  retries = TT.retries;
  if (toys.optflags & FLAG_S) {
      openlog("UDHCPC :", LOG_PID, LOG_DAEMON);
      infomode |= LOG_SYSTEM;
  }
  infomsg(infomode, "dhcp started");
  if (toys.optflags & FLAG_O) {
    while (TT.req_opt) {
      raw_opt[raw_optcount] = (uint8_t) strtoopt(TT.req_opt->arg, 1);
      raw_optcount++;
      TT.req_opt = TT.req_opt->next;
    }
  }
  if (toys.optflags & FLAG_x) {
    while (TT.pkt_opt) {
      (void) strtoopt(TT.pkt_opt->arg, 0);
      TT.pkt_opt = TT.pkt_opt->next;
    }
  }
  memset(&result, 0, sizeof(dhcpc_result_t));
  state = (dhcpc_state_t*) xmalloc(sizeof(dhcpc_state_t));
  memset(state, 0, sizeof(dhcpc_state_t));
  state->iface = (toys.optflags & FLAG_i) ? TT.iface : "eth0";

  if (get_interface(state->iface, &state->ifindex, NULL, state->macaddr))
    perror_exit("Failed to get interface %s", state->iface);

  run_script(NULL, "deconfig");
  setup_signal();
  state->status = STATE_INIT;
  mode_raw();
  fcntl(state->sockfd, F_SETFD, FD_CLOEXEC);

  for (;;) {
    FD_ZERO(&rfds);
    if (state->sockfd >= 0) FD_SET(state->sockfd, &rfds);
    FD_SET(sigfd.rd, &rfds);
    tv.tv_sec = timeout - waited;
    tv.tv_usec = 0;
    retval = 0;

    int maxfd = (sigfd.rd > state->sockfd)? sigfd.rd : state->sockfd;
    dbg("select wait ....\n");
    uint32_t timestmp = time(NULL);
    if((retval = select(maxfd + 1, &rfds, NULL, NULL, &tv)) < 0) {
      if (errno == EINTR) {
        waited += (unsigned) time(NULL) - timestmp;
        continue;
      }
      perror_exit("Error in select");
    }
    if (!retval) { // Timed out
      if (get_interface(state->iface, &state->ifindex, NULL, state->macaddr))
        error_exit("Interface lost %s\n", state->iface);

      switch (state->status) {
      case STATE_INIT:
        if (packets < retries) {
          if (!packets) xid = getxid();
          run_script(NULL, "deconfig");
          infomsg(infomode, "Sending discover...");
          dhcpc_sendmsg(DHCPDISCOVER);
          server = 0;
          timeout = TT.timeout;
          waited = 0;
          packets++;
          continue;
        }
lease_fail:
        run_script(NULL,"leasefail");
        if (toys.optflags & FLAG_n) {
          infomsg(infomode, "Lease failed. Exiting");
          goto ret_with_sockfd;
        }
        if (toys.optflags & FLAG_b) {
          infomsg(infomode, "Lease failed. Going Daemon mode");
          daemon(0, 0);
          if (toys.optflags & FLAG_p) write_pid(TT.pidfile);
          toys.optflags &= ~FLAG_b;
          toys.optflags |= FLAG_f;
        }
        timeout = TT.tryagain;
        waited = 0;
        packets = 0;
        continue;
      case STATE_REQUESTING:
        if (packets < retries) {
          memcpy(&state->ipaddr.s_addr,&state->pdhcp.yiaddr, 4);
          dhcpc_sendmsg(DHCPREQUEST);
          infomsg(infomode, "Sending select for %d.%d.%d.%d...",
              (result.ipaddr.s_addr >> 24) & 0xff, (result.ipaddr.s_addr >> 16) & 0xff, (result.ipaddr.s_addr >> 8) & 0xff, (result.ipaddr.s_addr) & 0xff);
          timeout = TT.timeout;
          waited = 0;
          packets++;
          continue;
        }
        mode_raw();
        state->status = STATE_INIT;
        goto lease_fail;
      case STATE_BOUND:
        state->status = STATE_RENEWING;
        dbg("Entering renew state\n");
        // FALLTHROUGH
      case STATE_RENEW_REQUESTED:   // FALLTHROUGH
      case STATE_RENEWING:
renew_requested:
        if (timeout > 60) {
          dhcpc_sendmsg(DHCPREQUEST);
          timeout >>= 1;
          waited = 0;
          continue;
        }
        dbg("Entering rebinding state\n");
        state->status = STATE_REBINDING;
        // FALLTHROUGH
      case STATE_REBINDING:
        mode_raw();
        if (timeout > 0) {
          dhcpc_sendmsg(DHCPREQUEST);
          timeout >>= 1;
          waited = 0;
          continue;
        }
        infomsg(infomode, "Lease lost, entering INIT state");
        run_script(NULL, "deconfig");
        state->status = STATE_INIT;
        timeout = 0;
        waited = 0;
        packets = 0;
        continue;
      default: break;
      }
      timeout = INT_MAX;
      waited = 0;
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
        renew();
        packets = 0;
        waited = 0;
        if (state->status == STATE_RENEW_REQUESTED) goto renew_requested;
        if (state->status == STATE_INIT) timeout = 0;
        continue;
      case SIGUSR2:
        infomsg(infomode, "Received SIGUSR2");
        release();
        timeout = INT_MAX;
        waited = 0;
        packets = 0;
        continue;
      case SIGTERM:
        infomsg(infomode, "Received SIGTERM");
        if (toys.optflags & FLAG_R) release();
        goto ret_with_sockfd;
      default: break;
      }
    }
    if (FD_ISSET(state->sockfd, &rfds)) { // Some Activity on RDFDs : is socket
      dbg("main sock read\n");
      uint8_t msgType;
      if (state->mode == MODE_RAW) bufflen = read_raw();
      if (state->mode == MODE_APP) bufflen = read_app();
      if (bufflen < 0) {
        if (state->mode == MODE_RAW) mode_raw();
        if (state->mode == MODE_APP) mode_app();
        continue;
      }
      waited += time(NULL) - timestmp;
      memset(&result, 0, sizeof(dhcpc_result_t));
      msgType = dhcpc_parsemsg(&result);
      if (msgType != DHCPNAK && result.ipaddr.s_addr == 0 ) continue;       // no ip for me ignore
      if (!msgType || !get_option_serverid(state->pdhcp.options, &result)) continue; //no server id ignore
      if (msgType == DHCPOFFER && server == 0) server = result.serverid.s_addr; // select the server
      if (result.serverid.s_addr != server) continue; // not from the server we requested ignore
      dhcpc_parseoptions(&result, state->pdhcp.options);
      get_option_lease(state->pdhcp.options, &result);

      switch (state->status) {
      case STATE_INIT:
        if (msgType == DHCPOFFER) {
          state->status = STATE_REQUESTING;
          mode_raw();
          timeout = 0;
          waited = 0;
          packets = 0;
        }
        continue;
      case STATE_REQUESTING:         // FALLTHROUGH
      case STATE_RENEWING:           // FALLTHROUGH
      case STATE_RENEW_REQUESTED:    // FALLTHROUGH
      case STATE_REBINDING:
        if (msgType == DHCPACK) {
          timeout = result.lease_time / 2;
          run_script(&result, state->status == STATE_REQUESTING ? "bound" : "renew");
          state->status = STATE_BOUND;
          infomsg(infomode, "Lease of %d.%d.%d.%d obtained, lease time %d from server %d.%d.%d.%d",
              (result.ipaddr.s_addr >> 24) & 0xff, (result.ipaddr.s_addr >> 16) & 0xff, (result.ipaddr.s_addr >> 8) & 0xff, (result.ipaddr.s_addr) & 0xff,
              result.lease_time,
              (result.serverid.s_addr >> 24) & 0xff, (result.serverid.s_addr >> 16) & 0xff, (result.serverid.s_addr >> 8) & 0xff, (result.serverid.s_addr) & 0xff);
          if (toys.optflags & FLAG_q) {
            if (toys.optflags & FLAG_R) release();
            goto ret_with_sockfd;
          }
          toys.optflags &= ~FLAG_n;
          if (!(toys.optflags & FLAG_f)) {
            daemon(0, 0);
            toys.optflags |= FLAG_f;
            if (toys.optflags & FLAG_p) write_pid(TT.pidfile);
          }
          waited = 0;
          continue;
        } else if (msgType == DHCPNAK) {
          dbg("NACK received.\n");
          run_script(&result, "nak");
          if (state->status != STATE_REQUESTING) run_script(NULL, "deconfig");
          mode_raw();
          sleep(3);
          state->status = STATE_INIT;
          state->ipaddr.s_addr = 0;
          server = 0;
          timeout = 0;
          packets = 0;
          waited = 0;
        }
        continue;
      default: break;
      }
    }
  }
ret_with_sockfd:
  if (CFG_TOYBOX_FREE) {
    free_option_stores();
    if (state->sockfd > 0) close(state->sockfd);
    free(state);
  }
}
