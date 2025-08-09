/* brctl.c - ethernet bridge control
 *
 * Copyright 2013 Ashwini Kumar <ak.ashwini1981@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard

USE_BRCTL(NEWTOY(brctl, "<1", TOYFLAG_USR|TOYFLAG_SBIN))

config BRCTL
  bool "brctl"
  default n
  help
    usage: brctl COMMAND [BRIDGE [INTERFACE]]

    Manage ethernet bridges

    Commands:
    show                  Show a list of bridges
    addbr BRIDGE          Create BRIDGE
    delbr BRIDGE          Delete BRIDGE
    addif BRIDGE IFACE    Add IFACE to BRIDGE
    delif BRIDGE IFACE    Delete IFACE from BRIDGE
    setageing BRIDGE TIME Set ageing time
    setfd BRIDGE TIME     Set bridge forward delay
    sethello BRIDGE TIME  Set hello time
    setmaxage BRIDGE TIME Set max message age
    setpathcost BRIDGE PORT COST   Set path cost
    setportprio BRIDGE PORT PRIO   Set port priority
    setbridgeprio BRIDGE PRIO      Set bridge priority
    stp BRIDGE [1/yes/on|0/no/off] STP on/off
*/

#define FOR_brctl
#include "toys.h"
#include <linux/if_bridge.h>

GLOBALS(
    int sockfd;
)
#define MAX_BRIDGES 1024 //same is no of ports supported

static void get_ports(char *bridge, int *indices)
{
  struct ifreq ifr;
  int ifindices[MAX_BRIDGES];
  unsigned long args[4] = { BRCTL_GET_PORT_LIST,
    (unsigned long) ifindices, MAX_BRIDGES, 0 };

  memset(ifindices, 0, MAX_BRIDGES);
  args[1] = (unsigned long)ifindices;
  xstrncpy(ifr.ifr_name, bridge, IFNAMSIZ);
  ifr.ifr_data = (char *)args;
  xioctl(TT.sockfd, SIOCDEVPRIVATE, &ifr);
  if (indices) memcpy(indices, ifindices, sizeof(ifindices));
}

void get_br_info(char *bridge, struct __bridge_info *info)
{
  struct ifreq ifr;
  unsigned long args[4] = { BRCTL_GET_BRIDGE_INFO,
    (unsigned long) info, 0, 0 };

  memset(info, 0, sizeof(*info));
  xstrncpy(ifr.ifr_name, bridge, IFNAMSIZ);
  ifr.ifr_data = (char *)args;

  if (ioctl(TT.sockfd, SIOCDEVPRIVATE, &ifr) < 0) {
    perror_msg("%s: can't get info %s\n", bridge, strerror(errno));
    return;
  }
}

void br_show(char **argv)
{
  struct __bridge_info info;
  int num, cnt, i, j, ifindices[MAX_BRIDGES], pindices[MAX_BRIDGES];
  unsigned long args[4] = { BRCTL_GET_BRIDGES,
    (unsigned long)ifindices, MAX_BRIDGES,0 };
  char br[IF_NAMESIZE], ifn[IF_NAMESIZE];

  num = ioctl(TT.sockfd, SIOCGIFBR, args); //ret is num of bridges found
  if (num < 0) error_exit("get bridges fail");
  printf("bridge name\tbridge id\t\tSTP enabled\tinterfaces\n");

  for (i = 0; i < num; i++) {
    unsigned char *id;

    if (!if_indextoname(ifindices[i], br)) perror_exit("interface not found");
    get_br_info(br, &info);
    id = (unsigned char*)&(info.bridge_id);
    printf("%s\t\t",br);
    printf("%.2x%.2x.%.2x%.2x%.2x%.2x%.2x%.2x", id[0], id[1],
        id[2], id[3], id[4], id[5], id[6], id[7]);
    printf("\t%s\t\t",(info.stp_enabled)?"yes" : "no");

    memset(pindices, 0, sizeof(pindices));
    get_ports(br, pindices);
    for (j = 0, cnt = 0; j < MAX_BRIDGES; j++) {
      if (!pindices[j]) continue;
      if (!if_indextoname(pindices[j], ifn)) {
        error_msg("no name for index :%d", pindices[j]);
        continue;
      }
      if (cnt) printf("\n\t\t\t\t\t\t\t");
      printf("%s", ifn);
      cnt++;
    }
    xputc('\n');
  }
}

void br_addbr(char **argv)
{
  char br[IFNAMSIZ];
  unsigned long args[4] = {BRCTL_ADD_BRIDGE, (unsigned long) br, 0, 0};

#ifdef SIOCBRADDBR
  xioctl(TT.sockfd, SIOCBRADDBR, argv[0]);
#else
  xstrncpy(br, argv[0], IFNAMSIZ);
  xioctl(TT.sockfd, SIOCSIFBR, args);
#endif
}

void br_delbr(char **argv)
{
  char br[IFNAMSIZ];
  unsigned long args[4] = {BRCTL_DEL_BRIDGE, (unsigned long) br, 0, 0};

#ifdef SIOCBRDELBR
  xioctl(TT.sockfd, SIOCBRDELBR, argv[0]);
#else
  xstrncpy(br, argv[0], IFNAMSIZ);
  xioctl(TT.sockfd, SIOCSIFBR, args);
#endif
}

void br_addif(char **argv)
{
  int index;
  struct ifreq ifr;
  unsigned long args[4] = {BRCTL_ADD_IF, 0, 0, 0};

  if (!(index = if_nametoindex(argv[1]))) perror_exit("interface %s", argv[1]);
  xstrncpy(ifr.ifr_name, argv[0], IFNAMSIZ);
#ifdef SIOCBRADDIF
  ifr.ifr_ifindex = index;
  xioctl(TT.sockfd, SIOCBRADDIF, &ifr);
#else
  args[1] = index;
  ifr.ifr_data = (char *)args;
  xioctl(TT.sockfd, SIOCDEVPRIVATE, &ifr);
#endif
}

void br_delif(char **argv)
{
  int index;
  struct ifreq ifr;
  unsigned long args[4] = {BRCTL_DEL_IF, 0, 0, 0};

  if (!(index = if_nametoindex(argv[1]))) perror_exit("interface %s",argv[1]);
  xstrncpy(ifr.ifr_name, argv[0], IFNAMSIZ);
#ifdef SIOCBRDELIF
  ifr.ifr_ifindex = index;
  xioctl(TT.sockfd, SIOCBRDELIF, &ifr);
#else
  args[1] = index;
  ifr.ifr_data = (char *)args;
  xioctl(TT.sockfd, SIOCDEVPRIVATE, &ifr);
#endif
}

static void strtotimeval(struct timeval *tv, char *time)
{
  double secs;

  if (sscanf(time, "%lf", &secs) != 1) error_exit("time format not proper");
  tv->tv_sec = secs;
  tv->tv_usec = 1000000 * (secs - tv->tv_sec);
}

static unsigned long tv_to_jify(struct timeval *tv)
{
  unsigned long long jify;

  jify = 1000000ULL * tv->tv_sec + tv->tv_usec;
  return (jify/10000);
}

void set_time(char *br, unsigned long cmd, unsigned long val)
{
  struct ifreq ifr;
  unsigned long args[4] = {cmd, val, 0, 0};

  xstrncpy(ifr.ifr_name, br, IFNAMSIZ);
  ifr.ifr_data = (char *)args;
  xioctl(TT.sockfd, SIOCDEVPRIVATE, &ifr);
}

void br_set_ageing_time(char **argv)
{
  struct timeval tv;

  strtotimeval(&tv, argv[1]);
  set_time(argv[0], BRCTL_SET_AGEING_TIME, tv_to_jify(&tv));
}

void br_set_fwd_delay(char **argv)
{
  struct timeval tv;

  strtotimeval(&tv, argv[1]);
  set_time(argv[0], BRCTL_SET_BRIDGE_FORWARD_DELAY, tv_to_jify(&tv));
}

void br_set_hello_time(char **argv)
{
  struct timeval tv;

  strtotimeval(&tv, argv[1]);
  set_time(argv[0], BRCTL_SET_BRIDGE_HELLO_TIME, tv_to_jify(&tv));
}

void br_set_max_age(char **argv)
{
  struct timeval tv;

  strtotimeval(&tv, argv[1]);
  set_time(argv[0], BRCTL_SET_BRIDGE_MAX_AGE, tv_to_jify(&tv));
}

void br_set_bridge_prio(char **argv)
{
  int prio;

  if (sscanf(argv[1], "%i", &prio) != 1) error_exit("prio not proper");
  set_time(argv[0], BRCTL_SET_BRIDGE_PRIORITY, prio);
}

void br_set_stp(char **argv)
{
  int i;
  struct stp {
    char *n;
    int set;
  } ss[] = {{"1", 1}, {"yes", 1},{"on", 1},
    {"0", 0}, {"no", 0},{"off", 0}};

  for (i = 0; i < ARRAY_LEN(ss); i++) {
    if (!strcmp(ss[i].n, argv[1])) break;
  }
  if (i >= ARRAY_LEN(ss)) error_exit("invalid stp state");
  set_time(argv[0], BRCTL_SET_BRIDGE_STP_STATE, ss[i].set);
}

void set_cost_prio(char *br, char *port, unsigned long cmd, unsigned long val)
{
  struct ifreq ifr;
  int i, index, pindices[MAX_BRIDGES];
  unsigned long args[4] = {cmd, 0, val, 0};

  if (!(index = if_nametoindex(port))) error_exit("invalid port");

  memset(pindices, 0, sizeof(pindices));
  get_ports(br, pindices);
  for (i = 0; i < MAX_BRIDGES; i++) {
    if (index == pindices[i]) break;
  }
  if (i >= MAX_BRIDGES) error_exit("%s not in bridge", port);
  args[1] = i;
  xstrncpy(ifr.ifr_name, br, IFNAMSIZ);
  ifr.ifr_data = (char *)args;
  xioctl(TT.sockfd, SIOCDEVPRIVATE, &ifr);
}

void br_set_path_cost(char **argv)
{
  int cost;

  cost = atolx_range(argv[2], 0, INT_MAX);
  set_cost_prio(argv[0], argv[1], BRCTL_SET_PATH_COST, cost);
}

void br_set_port_prio(char **argv)
{
  int prio;

  prio = atolx_range(argv[2], 0, INT_MAX);
  set_cost_prio(argv[0], argv[1], BRCTL_SET_PORT_PRIORITY, prio);

}

void brctl_main(void)
{
  int i;
  struct cmds {
    char *cmd;
    int nargs;
    void (*f)(char **argv);
  } cc[] = {{"show", 0, br_show},
    {"addbr", 1, br_addbr}, {"delbr", 1, br_delbr},
    {"addif", 2, br_addif}, {"delif", 2, br_delif},
    {"setageing", 2, br_set_ageing_time},
    {"setfd", 2, br_set_fwd_delay},
    {"sethello", 2, br_set_hello_time},
    {"setmaxage", 2, br_set_max_age},
    {"setpathcost", 3, br_set_path_cost},
    {"setportprio", 3, br_set_port_prio},
    {"setbridgeprio", 2, br_set_bridge_prio},
    {"stp", 2, br_set_stp},
  };

  TT.sockfd = xsocket(AF_INET, SOCK_STREAM, 0);
  while (*toys.optargs) {
    for (i = 0; i < ARRAY_LEN(cc); i++) {
      struct cmds *t = cc + i;

      if (strcmp(t->cmd, *toys.optargs)) continue;

      toys.optargs++, toys.optc--;
      if (toys.optc < t->nargs) help_exit("check args");
      t->f(toys.optargs);
      toys.optargs += t->nargs;
      toys.optc -= t->nargs;
      break;
    }

    if (i == ARRAY_LEN(cc)) help_exit("invalid option '%s'", *toys.optargs);
  }
  xclose(TT.sockfd);
}
