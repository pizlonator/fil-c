/* dhcp6.c - DHCP6 client for dynamic network configuration.
 *
 * Copyright 2015 Rajni Kant <rajnikant12345@gmail.com>
 *
 * Not in SUSv4.
USE_DHCP6(NEWTOY(dhcp6, "r:A#<0T#<0t#<0s:p:i:SRvqnbf", TOYFLAG_SBIN|TOYFLAG_ROOTONLY))

config DHCP6
  bool "dhcp6"
  default n
  help
      usage: dhcp6 [-fbnqvR] [-i IFACE] [-r IP] [-s PROG] [-p PIDFILE]

      Configure network dynamically using DHCP.

      -i Interface to use (default eth0)
      -p Create pidfile
      -s Run PROG at DHCP events
      -t Send up to N Solicit packets
      -T Pause between packets (default 3 seconds)
      -A Wait N seconds after failure (default 20)
      -f Run in foreground
      -b Background if lease is not obtained
      -n Exit if lease is not obtained
      -q Exit after obtaining lease
      -R Release IP on exit
      -S Log to syslog too
      -r Request this IP address
      -v Verbose

      Signals:
      USR1  Renew current lease
      USR2  Release current lease
*/
#define FOR_dhcp6
#include "toys.h"
#include <linux/sockios.h>
#include <linux/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <linux/if_packet.h>
#include <syslog.h>

GLOBALS(
  char *interface_name, *pidfile, *script;
  long retry, timeout, errortimeout;
  char *req_ip;
  int length, state, request_length, sock, sock1, status, retval, retries;
  struct timeval tv;
  uint8_t transction_id[3];
  struct sockaddr_in6 input_socket6;
)

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

#define DHCPC_SERVER_PORT     547
#define DHCPC_CLIENT_PORT     546

#define LOG_SILENT          0x0
#define LOG_CONSOLE         0x1
#define LOG_SYSTEM          0x2

typedef struct __attribute__((packed)) dhcp6_msg_s {
  uint8_t msgtype, transaction_id[3], options[524];
} dhcp6_msg_t;

typedef struct __attribute__((packed)) optval_duid_llt {
  uint16_t type;
  uint16_t hwtype;
  uint32_t time;
  uint8_t lladdr[6];
} DUID;

typedef struct __attribute__((packed)) optval_ia_na {
  uint32_t iaid, t1, t2;
} IA_NA;

typedef struct __attribute__((packed)) dhcp6_raw_s {
  struct ip6_hdr iph;
  struct udphdr udph;
  dhcp6_msg_t dhcp6;
} dhcp6_raw_t;

typedef struct __attribute__((packed)) dhcp_data_client {
  uint16_t  status_code;
  uint32_t iaid , t1,t2, pf_lf, va_lf;
  uint8_t ipaddr[17] ;
} DHCP_DATA;

static DHCP_DATA dhcp_data;
static dhcp6_raw_t *mymsg;
static dhcp6_msg_t mesg;
static DUID *duid;

static void (*dbg)(char *format, ...);
static void dummy(char *format, ...)
{
  return;
}

static void logit(char *format, ...)
{
  int used;
  char *msg;
  va_list p, t;
  uint8_t infomode = LOG_SILENT;

  if (toys.optflags & FLAG_S) infomode |= LOG_SYSTEM;
  if(toys.optflags & FLAG_v) infomode |= LOG_CONSOLE;
  va_start(p, format);
  va_copy(t, p);
  used = vsnprintf(NULL, 0, format, t);
  used++;
  va_end(t);

  msg = xmalloc(used);
  vsnprintf(msg, used, format, p);
  va_end(p);

  if (infomode & LOG_SYSTEM) syslog(LOG_INFO, "%s", msg);
  if (infomode & LOG_CONSOLE) printf("%s", msg);
  free(msg);
  return;
}

static void get_mac(uint8_t *mac, char *interface)
{
  int fd;
  struct ifreq req;

  if (!mac) return;
  fd = xsocket(AF_INET6, SOCK_RAW, IPPROTO_RAW);
  req.ifr_addr.sa_family = AF_INET6;
  xstrncpy(req.ifr_name, interface, IFNAMSIZ);
  xioctl(fd, SIOCGIFHWADDR, &req);
  memcpy(mac, req.ifr_hwaddr.sa_data, 6);
  xclose(fd);
}

static void fill_option(uint16_t option_id, uint16_t option_len, uint8_t **dhmesg)
{
  uint8_t *tmp = *dhmesg;

  *((uint16_t*)tmp) = htons(option_id);
  *(uint16_t*)(tmp+2) = htons(option_len);
  *dhmesg += 4;
  TT.length += 4;
}

static void fill_clientID()
{
  uint8_t *tmp = &mesg.options[TT.length];

  if(!duid) {
    uint8_t mac[7] = {0,};
    duid = (DUID*)malloc(sizeof(DUID));
    duid->type = htons(1);
    duid->hwtype = htons(1);
    duid->time = htonl((uint32_t)(time(NULL) - 946684800) & 0xffffffff);
    fill_option(DHCP6_OPT_CLIENTID,14,&tmp);
    get_mac(mac, TT.interface_name);
    memcpy(duid->lladdr,mac, 6);
    memcpy(tmp,(uint8_t*)duid,sizeof(DUID));
  }
  else {
    fill_option(DHCP6_OPT_CLIENTID,14,&tmp);
    memcpy(tmp,(uint8_t*)duid,sizeof(DUID));
  }
  TT.length += sizeof(DUID);
}

// TODO: make it generic for multiple options.
static void fill_optionRequest()
{
  uint8_t *tmp = &mesg.options[TT.length];

  fill_option(DHCP6_OPT_ORO,4,&tmp);
  *(uint16_t*)(tmp+4) = htons(23);
  *(uint16_t*)(tmp+6) = htons(24);
  TT.length += 4;
}

static void fill_elapsedTime()
{
  uint8_t *tmp = &mesg.options[TT.length];

  fill_option(DHCP6_OPT_ELAPSED_TIME, 2, &tmp);
  *(uint16_t*)(tmp+6) = htons(0);
  TT.length += 2;
}

static void fill_iaid()
{
  IA_NA iana;
  uint8_t *tmp = &mesg.options[TT.length];

  fill_option(DHCP6_OPT_IA_NA, 12, &tmp);
  iana.iaid = rand();
  iana.t1 = 0xffffffff;
  iana.t2 = 0xffffffff;
  memcpy(tmp, (uint8_t*)&iana, sizeof(IA_NA));
  TT.length += sizeof(IA_NA);
}

//static void mode_raw(int *sock_t)
static void mode_raw()
{
  int constone = 1;
  struct sockaddr_ll sockll;

  if (TT.sock > 0) xclose(TT.sock);
  TT.sock = xsocket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IPV6));

  memset(&sockll, 0, sizeof(sockll));
  sockll.sll_family = AF_PACKET;
  sockll.sll_protocol = htons(ETH_P_IPV6);
  sockll.sll_ifindex = if_nametoindex(TT.interface_name);
  xbind(TT.sock, (struct sockaddr *) &sockll, sizeof(sockll));
  if (setsockopt(TT.sock, SOL_PACKET, PACKET_HOST,&constone, sizeof(int)) < 0) {
    if (errno != ENOPROTOOPT) error_exit("MODE RAW : Bind fail.\n");
  }
}

static void generate_transection_id()
{
  int i, r = rand() % 0xffffff;

  for (i=0; i<3; i++) {
    TT.transction_id[i] = r%0xff;
    r = r/10;
  }
}

static void set_timeout(int seconds)
{
  TT.tv.tv_sec = seconds;
  TT.tv.tv_usec = 100000;
}

static void  send_msg(int type)
{
  struct sockaddr_in6 addr6;
  int sendlength = 0;

  memset(&addr6, 0, sizeof(addr6));
  addr6.sin6_family = AF_INET6;
  addr6.sin6_port = htons(DHCPC_SERVER_PORT); //SERVER_PORT
  inet_pton(AF_INET6, "ff02::1:2", &addr6.sin6_addr);
  mesg.msgtype = type;
  generate_transection_id();
  memcpy(mesg.transaction_id, TT.transction_id, 3);

  if (type  == DHCP6SOLICIT) {
    TT.length = 0;
    fill_clientID();
    fill_optionRequest();
    fill_elapsedTime();
    fill_iaid();
    sendlength = sizeof(dhcp6_msg_t) - 524 + TT.length;
  } else if (type == DHCP6REQUEST || type == DHCP6RELEASE || type == DHCP6RENEW)
    sendlength = TT.request_length;
  dbg("Sending message type: %d\n", type);
  sendlength = sendto(TT.sock1, &mesg, sendlength , 0,(struct sockaddr *)&addr6,
          sizeof(struct sockaddr_in6 ));
  if (sendlength <= 0) dbg("Error in sending message type: %d\n", type);
}

uint8_t *get_msg_ptr(uint8_t *data, int data_length, int msgtype)
{
  uint16_t type =  *((uint16_t*)data), length = *((uint16_t*)(data+2));

  type = ntohs(type);
  if (type == msgtype) return data;
  length = ntohs(length);
  while (type != msgtype) {
    data_length -= (4 + length);
    if (data_length <= 0) break;
    data = data + 4 + length;
    type = ntohs(*((uint16_t*)data));
    length = ntohs(*((uint16_t*)(data+2)));
    if (type == msgtype) return data;
  }
  return NULL;
}

static uint8_t *check_server_id(uint8_t *data, int data_length)
{
  return get_msg_ptr(data,  data_length, DHCP6_OPT_SERVERID);
}

static int check_client_id(uint8_t *data, int data_length)
{
  if ((data = get_msg_ptr(data,  data_length, DHCP6_OPT_CLIENTID))) {
    DUID one = *((DUID*)(data+4));
    DUID two = *((DUID*)&mesg.options[4]);

    if (!memcmp(&one, &two, sizeof(DUID))) return 1;
  }
  return 0;
}

static int validate_ids()
{
  if (!check_server_id(mymsg->dhcp6.options,
    TT.status - ((char*)&mymsg->dhcp6.options[0] - (char*)mymsg) )) {
    dbg("Invalid server id: %d\n");
    return 0;
  }
  if (!check_client_id(mymsg->dhcp6.options,
    TT.status - ((char*)&mymsg->dhcp6.options[0] - (char*)mymsg) )) {
    dbg("Invalid client id: %d\n");
    return 0;
  }
  return 1;
}

static void parse_ia_na(uint8_t *data, int data_length)
{
  uint8_t *t = get_msg_ptr(data, data_length, DHCP6_OPT_IA_NA);
  uint16_t iana_len, content_len = 0;

  memset(&dhcp_data,0,sizeof(dhcp_data));
  if (!t) return;

  iana_len = ntohs(*((uint16_t*)(t+2)));
  dhcp_data.iaid = ntohl(*((uint32_t*)(t+4)));
  dhcp_data.t1 = ntohl(*((uint32_t*)(t+8)));
  dhcp_data.t2 = ntohl(*((uint32_t*)(t+12)));
  t += 16;
  iana_len -= 12;

  while(iana_len > 0) {
    uint16_t sub_type = ntohs(*((uint16_t*)(t)));

    switch (sub_type) {
      case DHCP6_OPT_IA_ADDR:
        content_len = ntohs(*((uint16_t*)(t+2)));
        memcpy(dhcp_data.ipaddr,t+4,16);
        if (TT.state == DHCP6SOLICIT) {
          if (TT.req_ip) {
            struct addrinfo *res = NULL;

            if(!getaddrinfo(TT.req_ip, NULL, NULL,&res)) {
              dbg("Requesting IP: %s\n", TT.req_ip);
              memcpy (&TT.input_socket6, res->ai_addr, res->ai_addrlen);
              memcpy(t+4, TT.input_socket6.sin6_addr.s6_addr, 16);
            } else xprintf("Invalid IP: %s\n",TT.req_ip);
            freeaddrinfo(res);
          }
        }
        dhcp_data.pf_lf = ntohl(*((uint32_t*)(t+20)));
        dhcp_data.va_lf = ntohl(*((uint32_t*)(t+24)));
        iana_len -= (content_len + 4);
        t += (content_len + 4);
        break;
      case DHCP6_OPT_STATUS_CODE:
        content_len = ntohs(*((uint16_t*)(t+2)));
        dhcp_data.status_code = ntohs(*((uint16_t*)(t+4)));
        iana_len -= (content_len + 4);
        t += (content_len + 4);
        break;
      default:
        content_len = ntohs(*((uint16_t*)(t+2)));
        iana_len -= (content_len + 4);
        t += (content_len + 4);
        break;
    }
  }
}

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

// Creates environment pointers from RES to use in script
static int fill_envp(DHCP_DATA *res)
{
  int ret = setenv("interface", TT.interface_name, 1);

  if (ret) return ret;
  inet_ntop(AF_INET6, res->ipaddr, toybuf, INET6_ADDRSTRLEN);
  ret = setenv("ip",(const char*)toybuf , 1);
  return ret;
}

// Executes Script NAME.
static void run_script(DHCP_DATA *res,  char *name)
{
  volatile int error = 0;
  struct stat sts;
  pid_t pid;
  char *argv[3];
  char *script = (toys.optflags & FLAG_s) ? TT.script
    : "/usr/share/dhcp/default.script";

  if (stat(script, &sts) == -1 && errno == ENOENT) return;
  if (!res || fill_envp(res)) {
    dbg("Failed to create environment variables.\n");
    return;
  }
  dbg("Executing %s %s\n", script, name);
  argv[0] = (char*)script;
  argv[1] = (char*)name;
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
    waitpid(pid, NULL, 0);
    errno = error;
    perror_msg("script exec failed");
  }
  dbg("script complete.\n");
}

static void lease_fail()
{
  dbg("Lease failed.\n");
  run_script(NULL, "leasefail");
  if (toys.optflags & FLAG_n) {
    xclose(TT.sock);
    xclose(TT.sock1);
    error_exit("Lease Failed, Exiting.");
  }
  if (toys.optflags & FLAG_b) {
    dbg("Lease failed. Going to daemon mode.\n");
    if (daemon(0,0)) perror_exit("daemonize");
    if (toys.optflags & FLAG_p) write_pid(TT.pidfile);
    toys.optflags &= ~FLAG_b;
    toys.optflags |= FLAG_f;
  }
}

// Generic signal handler real handling is done in main funcrion.
static void signal_handler(int sig)
{
    dbg("Caught signal: %d\n", sig);
    switch (sig) {
    case SIGUSR1:
      dbg("SIGUSR1.\n");
      if (TT.state == DHCP6RELEASE || TT.state == DHCP6REQUEST ) {
        TT.state = DHCP6SOLICIT;
        set_timeout(0);
        return;
      }
      dbg("SIGUSR1 sending renew.\n");
      send_msg(DHCP6RENEW);
      TT.state = DHCP6RENEW;
      TT.retries = 0;
      set_timeout(0);
      break;
    case SIGUSR2:
      dbg("SIGUSR2.\n");
      if (TT.state == DHCP6RELEASE) return;
      if (TT.state != DHCP6CONFIRM ) return;
      dbg("SIGUSR2 sending release.\n");
      send_msg(DHCP6RELEASE);
      TT.state = DHCP6RELEASE;
      TT.retries = 0;
      set_timeout(0);
      break;
    case SIGTERM:
    case SIGINT:
      dbg((sig == SIGTERM)?"SIGTERM.\n":"SIGINT.\n");
      if ((toys.optflags & FLAG_R) && TT.state == DHCP6CONFIRM)
        send_msg(DHCP6RELEASE);
      if(sig == SIGINT) exit(0);
      break;
    default: break;
  }
}

// signal setup for SIGUSR1 SIGUSR2 SIGTERM
static int setup_signal()
{
  signal(SIGUSR1, signal_handler);
  signal(SIGUSR2, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  return 0;
}

void dhcp6_main(void)
{
  struct sockaddr_in6  sinaddr6;
  int constone = 1;
  fd_set rfds;

  srand(time(NULL));
  setlinebuf(stdout);
  dbg = dummy;
  TT.state = DHCP6SOLICIT;

  if (toys.optflags & FLAG_v) dbg = logit;
  if (!TT.interface_name) TT.interface_name = "eth0";
  if (toys.optflags & FLAG_p) write_pid(TT.pidfile);
  if (!TT.retry) TT.retry = 3;
  if (!TT.timeout) TT.timeout = 3;
  if (!TT.errortimeout) TT.errortimeout = 20;
  if (toys.optflags & FLAG_S) {
    openlog("DHCP6 :", LOG_PID, LOG_DAEMON);
    dbg = logit;
  }

  dbg("Interface: %s\n", TT.interface_name);
  dbg("pid file: %s\n", TT.pidfile);
  dbg("Retry count: %d\n", TT.retry);
  dbg("Timeout : %d\n", TT.timeout);
  dbg("Error timeout: %d\n", TT.errortimeout);



  setup_signal();
  TT.sock1 = xsocket(PF_INET6, SOCK_DGRAM, 0);
  memset(&sinaddr6, 0, sizeof(sinaddr6));
  sinaddr6.sin6_family = AF_INET6;
  sinaddr6.sin6_port = htons(DHCPC_CLIENT_PORT);
  sinaddr6.sin6_scope_id = if_nametoindex(TT.interface_name);
  sinaddr6.sin6_addr = in6addr_any ;

  xsetsockopt(TT.sock1, SOL_SOCKET, SO_REUSEADDR, &constone, sizeof(constone));

  xbind(TT.sock1, (struct sockaddr *)&sinaddr6, sizeof(sinaddr6));

  mode_raw();
  set_timeout(0);
  for (;;) {
    int maxfd = TT.sock;

    if (TT.sock >= 0) FD_SET(TT.sock, &rfds);
    TT.retval = 0;
    if ((TT.retval = select(maxfd + 1, &rfds, NULL, NULL, &TT.tv)) < 0) {
      if(errno == EINTR) continue;
      perror_exit("Error in select");
    }
    if (!TT.retval) {
      if (TT.state == DHCP6SOLICIT || TT.state == DHCP6CONFIRM) {
        dbg("State is solicit, sending solicit packet\n");
        run_script(NULL, "deconfig");
        send_msg(DHCP6SOLICIT);
        TT.state = DHCP6SOLICIT;
        TT.retries++;
        if(TT.retries > TT.retry) set_timeout(TT.errortimeout);
        else if (TT.retries == TT.retry) {
          dbg("State is solicit, retry count is max.\n");
          lease_fail();
          set_timeout(TT.errortimeout);
        } else set_timeout(TT.timeout);
        continue;
      } else if (TT.state == DHCP6REQUEST || TT.state == DHCP6RENEW ||
              TT.state == DHCP6RELEASE) {
        dbg("State is %d , sending packet\n", TT.state);
        send_msg(TT.state);
        TT.retries++;
        if (TT.retries > TT.retry) set_timeout(TT.errortimeout);
        else if (TT.retries == TT.retry) {
          lease_fail();
          set_timeout(TT.errortimeout);
        } else set_timeout(TT.timeout);
        continue;
      }
    } else if (FD_ISSET(TT.sock, &rfds)) {
      if ((TT.status = read(TT.sock, toybuf, sizeof(toybuf))) <= 0) continue;
      mymsg = (dhcp6_raw_t*)toybuf;
      if (ntohs(mymsg->udph.dest) == 546 &&
              !memcmp(mymsg->dhcp6.transaction_id, TT.transction_id, 3)) {
        if (TT.state == DHCP6SOLICIT) {
          if (mymsg->dhcp6.msgtype == DHCP6ADVERTISE ) {
            if (!validate_ids()) {
              dbg("Invalid id received, solicit.\n");
              TT.state = DHCP6SOLICIT;
              continue;
            }
            dbg("Got reply to request or solicit.\n");
            TT.retries = 0;
            set_timeout(0);
            TT.request_length = TT.status - ((char*)&mymsg->dhcp6 - (char*)mymsg);
            memcpy((uint8_t*)&mesg, &mymsg->dhcp6, TT.request_length);
            parse_ia_na(mesg.options, TT.request_length);
            dbg("Status code:%d\n", dhcp_data.status_code);
            inet_ntop(AF_INET6, dhcp_data.ipaddr, toybuf, INET6_ADDRSTRLEN);
            dbg("Advertiesed IP: %s\n", toybuf);
            TT.state = DHCP6REQUEST;
          } else {
            dbg("Invalid solicit.\n");
            continue;
          }
        } else if (TT.state == DHCP6REQUEST || TT.state == DHCP6RENEW ) {
          if (mymsg->dhcp6.msgtype == DHCP6REPLY) {
            if (!validate_ids()) {
              dbg("Invalid id received, %d.\n", TT.state);
              TT.state = DHCP6REQUEST;
              continue;
            }
            dbg("Got reply to request or renew.\n");
            TT.request_length = TT.status - ((char*)&mymsg->dhcp6 - (char*)mymsg);
            memcpy((uint8_t*)&mesg, &mymsg->dhcp6, TT.request_length);
            parse_ia_na(mymsg->dhcp6.options, TT.request_length);
            dbg("Status code:%d\n", dhcp_data.status_code);
            inet_ntop(AF_INET6, dhcp_data.ipaddr, toybuf, INET6_ADDRSTRLEN);
            dbg("Got IP: %s\n", toybuf);
            TT.retries = 0;
            run_script(&dhcp_data, (TT.state == DHCP6REQUEST) ?
              "request" : "renew");
            if (toys.optflags & FLAG_q) {
              if (toys.optflags & FLAG_R) send_msg(DHCP6RELEASE);
              break;
            }
            TT.state = DHCP6CONFIRM;
            set_timeout((dhcp_data.va_lf)?dhcp_data.va_lf:INT_MAX);
            dbg("Setting timeout to intmax.");
            if (TT.state == DHCP6REQUEST || !(toys.optflags & FLAG_f)) {
              dbg("Making it a daemon\n");
              if (daemon(0,0)) perror_exit("daemonize");
              toys.optflags |= FLAG_f;
              if (toys.optflags & FLAG_p) write_pid(TT.pidfile);
            }
            dbg("Making it a foreground.\n");
            continue;
          } else {
            dbg("Invalid reply.\n");
            continue;
          }
        } else if (TT.state == DHCP6RELEASE) {
          dbg("Got reply to release.\n");
          run_script(NULL, "release");
          set_timeout(INT_MAX);
        }
      }
    }
  }
  xclose(TT.sock1);
  xclose(TT.sock);
}
