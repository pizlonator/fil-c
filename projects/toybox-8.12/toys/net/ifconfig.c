/* ifconfig.c - Configure network interface.
 *
 * Copyright 2012 Ranjan Kumar <ranjankumar.bth@gmail.com>
 * Copyright 2012 Kyungwan Han <asura321@gamil.com>
 * Reviewed by Kyungsu Kim <kaspyx@gmail.com>
 *
 * Not in SUSv4.
 *
 * Obsolete fields included for historical purposes:
 * irq|io_addr|mem_start ADDR - micromanage obsolete hardware
 * outfill|keepalive INTEGER - SLIP analog dialup line quality monitoring
 * metric INTEGER - added to Linux 0.9.10 with comment "never used", still true

USE_IFCONFIG(NEWTOY(ifconfig, "^?aS", TOYFLAG_SBIN))

config IFCONFIG
  bool "ifconfig"
  default y
  help
    usage: ifconfig [-aS] [INTERFACE [ACTION...]]

    Display or configure network interface.

    With no arguments, display active interfaces. First argument is interface
    to operate on, one argument by itself displays that interface.

    -a	All interfaces displayed, not just active ones
    -S	Short view, one line per interface

    Standard ACTIONs to perform on an INTERFACE:

    ADDR[/MASK]        - set IPv4 address (1.2.3.4/5) and activate interface
    add|del ADDR[/LEN] - add/remove IPv6 address (1111::8888/128)
    up|down            - activate or deactivate interface

    Advanced ACTIONs (default values usually suffice):

    default          - remove IPv4 address
    netmask ADDR     - set IPv4 netmask via 255.255.255.0 instead of /24
    txqueuelen LEN   - number of buffered packets before output blocks
    mtu LEN          - size of outgoing packets (Maximum Transmission Unit)
    broadcast ADDR   - Set broadcast address
    pointopoint ADDR - PPP and PPPOE use this instead of "route add default gw"
    hw TYPE ADDR     - set hardware (mac) address (type = ether|infiniband)
    rename NEWNAME   - rename interface

    Flags you can set on an interface (or -remove by prefixing with -):

    arp       - don't use Address Resolution Protocol to map LAN routes
    promisc   - don't discard packets that aren't to this LAN hardware address
    multicast - force interface into multicast mode if the driver doesn't
    allmulti  - promisc for multicast packets
*/

#define FOR_ifconfig
#include "toys.h"

#include <net/if_arp.h>
#include <net/ethernet.h>

GLOBALS(
  int sockfd;
)

// Convert hostname to binary address for AF_INET or AF_INET6
// return /prefix (or range max if none)
static int get_addrinfo(char *host, sa_family_t af, void *addr)
{
  struct addrinfo hints, *result, *rp = 0;
  int status, len;
  char *from, *slash;

  memset(&hints, 0 , sizeof(struct addrinfo));
  hints.ai_family = af;
  hints.ai_socktype = SOCK_STREAM;

  slash = strchr(host, '/');
  if (slash) *slash = 0;

  status = getaddrinfo(host, NULL, &hints, &result);
  if (!status)
    for (rp = result; rp; rp = rp->ai_next)
      if (rp->ai_family == af) break;
  if (!rp) error_exit("bad address '%s' : %s", host, gai_strerror(status));

  // ai_addr isn't struct in_addr or in6_addr, it's struct sockaddr. Of course.
  // You'd think ipv4 and ipv6 would have some basic compatibility, but no.
  from = ((char *)rp->ai_addr) + 4;
  if (af == AF_INET6) {
    len = 16;
    from += 4;  // skip "flowinfo" field ipv6 puts before address
  } else len = 4;
  memcpy(addr, from, len);
  freeaddrinfo(result);

  len = -1;
  if (slash) len = atolx_range(slash+1, 0, (af == AF_INET) ? 32 : 128);

  return len;
}

static void display_ifconfig(char *name, int always, unsigned long long val[])
{
  struct ifreq ifre;
  struct sockaddr_in *si = (void *)&ifre.ifr_addr;
  struct {
    int type;
    char *title;
  } types[] = {
    {ARPHRD_LOOPBACK, "Local Loopback"}, {ARPHRD_ETHER, "Ethernet"},
    {ARPHRD_PPP, "Point-to-Point Protocol"}, {ARPHRD_INFINIBAND, "InfiniBand"},
    {ARPHRD_SIT, "IPv6-in-IPv4"}, {-1, "UNSPEC"}
  };
  int i;
  char *pp;
  FILE *fp;
  short flags;

  xstrncpy(ifre.ifr_name, name, IFNAMSIZ);
  if (ioctl(TT.sockfd, SIOCGIFFLAGS, &ifre)<0) perror_exit_raw(name);
  flags = ifre.ifr_flags;
  if (!always && !(flags & IFF_UP)) return;

  if (FLAG(S)) {
    unsigned uu = 0;
    int len;

    ioctl(TT.sockfd, SIOCGIFADDR, &ifre);
    len = printf("%*s %s", -9, name, inet_ntoa(si->sin_addr));
    if (!ioctl(TT.sockfd, SIOCGIFNETMASK, &ifre))
      uu = htonl(*(unsigned *)&(si->sin_addr));
    for (i = 0; uu; i++) uu <<= 1;
    len += printf("/%d", i);
    printf("%*c", 29-len, ' ');
  }

  // Query hardware type and hardware address.
  // Not xioctl because you don't have permission for this on Android.
  ioctl(TT.sockfd, SIOCGIFHWADDR, &ifre);

  if (FLAG(S))
    for (i=0; i<6; i++) printf(":%02x"+!i, ifre.ifr_hwaddr.sa_data[i]);
  else {
    for (i=0; i < ARRAY_LEN(types)-1; i++)
      if (ifre.ifr_hwaddr.sa_family == types[i].type) break;
    xprintf("%-9s Link encap:%s  ", name, types[i].title);
    if(ifre.ifr_hwaddr.sa_family == ARPHRD_ETHER) {
      xprintf("HWaddr ");
      for (i=0; i<6; i++) xprintf(":%02x"+!i, ifre.ifr_hwaddr.sa_data[i]);
    }
    sprintf(toybuf, "/sys/class/net/%.15s/device/driver", name);
    if (readlink0(toybuf, toybuf, sizeof(toybuf))>0)
      if ((pp = strrchr(toybuf, '/'))) xprintf("  Driver %s", pp+1);
    xputc('\n');
  }

  // If an address is assigned record that.

  ifre.ifr_addr.sa_family = AF_INET;
  memset(&ifre.ifr_addr, 0, sizeof(ifre.ifr_addr));
  ioctl(TT.sockfd, SIOCGIFADDR, &ifre);
  pp = (char *)&ifre.ifr_addr;
  for (i = 0; i<sizeof(ifre.ifr_addr); i++) if (pp[i]) break;

  if (!FLAG(S) && i != sizeof(ifre.ifr_addr)) {
    struct sockaddr_in *si = (struct sockaddr_in *)&ifre.ifr_addr;
    struct {
      char *name;
      int flag, ioctl;
    } addr[] = {
      {"addr", 0, 0},
      {"P-t-P", IFF_POINTOPOINT, SIOCGIFDSTADDR},
      {"Bcast", IFF_BROADCAST, SIOCGIFBRDADDR},
      {"Mask", 0, SIOCGIFNETMASK}
    };

    // TODO: can this be ipv6? Why are we checking here when ipv6 source later?
    xprintf("%10c%s", ' ', (si->sin_family == AF_INET) ? "inet" :
        (si->sin_family == AF_INET6) ? "inet6" : "unspec");

    for (i=0; i<ARRAY_LEN(addr); i++) {
      if (!addr[i].flag || (flags & addr[i].flag)) {
        if (addr[i].ioctl && ioctl(TT.sockfd, addr[i].ioctl, &ifre))
          si->sin_family = 0;
        xprintf(" %s:%s ", addr[i].name,
          (si->sin_family == 0xFFFF || !si->sin_family)
            ? "[NOT SET]" : inet_ntoa(si->sin_addr));
      }
    }

    xputc('\n');
  }

  fp = fopen(pp = "/proc/net/if_inet6", "r");
  if (fp) {
    char iface_name[IFNAMSIZ];
    int plen, iscope;

    while (fgets(toybuf, sizeof(toybuf), fp)) {
      int nitems;
      char ipv6_addr[40];

      nitems = sscanf(toybuf, "%32s %*08x %02x %02x %*02x %15s\n",
                      ipv6_addr, &plen, &iscope, iface_name);
      if (nitems<0 && feof(fp)) break;
      if (nitems != 4) perror_exit("bad %s", pp);

      if (!strcmp(name, iface_name)) {
        struct sockaddr_in6 s6;
        char *ptr = ipv6_addr+sizeof(ipv6_addr)-1;

        // convert giant hex string into colon-spearated ipv6 address by
        // inserting ':' every 4 characters.
        for (i = 32; i; i--)
          if ((*(ptr--) = ipv6_addr[i])) if (!(i&3)) *(ptr--) = ':';

        // Convert to binary and back to get abbreviated :: version
        if (inet_pton(AF_INET6, ipv6_addr, (void *)&s6.sin6_addr) > 0) {
          if (inet_ntop(AF_INET6, &s6.sin6_addr, toybuf, sizeof(toybuf))) {
            char *scopes[] = {"Global","Host","Link","Site","Compat"},
                 *scope = "Unknown";

            for (i=0; i<ARRAY_LEN(scopes); i++)
              if (iscope == (!!i)<<(i+3)) scope = scopes[i];
            if (FLAG(S)) xprintf(" %s/%d@%c", toybuf, plen,*scope);
            else xprintf("%10cinet6 addr: %s/%d Scope: %s\n",
                         ' ', toybuf, plen, scope);
          }
        }
      }
    }
    fclose(fp);
  }

  if (FLAG(S)) {
    xputc('\n');
    return;
  }

  xprintf("%10c", ' ');

  if (flags) {
    unsigned short mask = 1;
    char **s, *str[] = {
      "UP", "BROADCAST", "DEBUG", "LOOPBACK", "POINTOPOINT", "NOTRAILERS",
      "RUNNING", "NOARP", "PROMISC", "ALLMULTI", "MASTER", "SLAVE", "MULTICAST",
      "PORTSEL", "AUTOMEDIA", "DYNAMIC", "LOWER_UP", "DORMANT", "ECHO", NULL
    };

    for (s = str; *s; s++) {
      if (flags & mask) xprintf("%s ", *s);
      mask <<= 1;
    }
  } else xprintf("[NO FLAGS] ");

  if (ioctl(TT.sockfd, SIOCGIFMTU, &ifre) < 0) ifre.ifr_mtu = 0;
  xprintf(" MTU:%d", ifre.ifr_mtu);
  if (ioctl(TT.sockfd, SIOCGIFMETRIC, &ifre) < 0) ifre.ifr_metric = 0;
  if (!ifre.ifr_metric) ifre.ifr_metric = 1;
  xprintf("  Metric:%d", ifre.ifr_metric);

  // non-virtual interface

  if (val) {
    char *label[] = {"RX bytes", "RX packets", "errors", "dropped", "overruns",
      "frame", 0, 0, "TX bytes", "TX packets", "errors", "dropped", "overruns",
      "collisions", "carrier", 0, "txqueuelen"};
    signed char order[] = {-1, 1, 2, 3, 4, 5, -1, 9, 10, 11, 12, 14, -1,
      13, 16, -1, 0, 8};
    int i;

    // Query txqueuelen
    if (ioctl(TT.sockfd, SIOCGIFTXQLEN, &ifre) >= 0) val[16] = ifre.ifr_qlen;
    else val[16] = -1;

    for (i = 0; i<sizeof(order); i++) {
      int j = order[i];

      if (j < 0) xprintf("\n%10c", ' ');
      else xprintf("%s:%llu ", label[j], val[j]);
    }
  }
  xputc('\n');

  if(!ioctl(TT.sockfd, SIOCGIFMAP, &ifre) && (ifre.ifr_map.irq ||
      ifre.ifr_map.mem_start || ifre.ifr_map.dma || ifre.ifr_map.base_addr))
  {
    xprintf("%10c", ' ');
    if(ifre.ifr_map.irq) xprintf("Interrupt:%d ", ifre.ifr_map.irq);
    if(ifre.ifr_map.base_addr >= 0x100) // IO_MAP_INDEX
      xprintf("Base address:0x%x ", ifre.ifr_map.base_addr);
    if(ifre.ifr_map.mem_start)
      xprintf("Memory:%lx-%lx ", ifre.ifr_map.mem_start, ifre.ifr_map.mem_end);
    if(ifre.ifr_map.dma) xprintf("DMA chan:%x ", ifre.ifr_map.dma);
    xputc('\n');
  }
  xputc('\n');
}

static void show_iface(char *iface_name)
{
  char *name;
  struct string_list *ifaces = 0, *sl;
  int i, j;
  FILE *fp;

  fp = xfopen("/proc/net/dev", "r");

  for (i=0; fgets(toybuf, sizeof(toybuf), fp); i++) {
    char *buf = toybuf;
    unsigned long long val[17];

    if (i<2) continue;

    while (isspace(*buf)) buf++;
    name = strsep(&buf, ":");
    if(!buf) error_exit("bad name %s", name);

    errno = 0;
    for (j=0; j<16 && !errno; j++) val[j] = strtoll(buf, &buf, 0);
    if (errno) perror_exit("bad %s at %s", name, buf);

    if (iface_name) {
      if (!strcmp(iface_name, name)) {
        display_ifconfig(iface_name, 1, val);

        return;
      }
    } else {
      sl = xmalloc(sizeof(*sl)+strlen(name)+1);
      strcpy(sl->str, name);
      sl->next = ifaces;
      ifaces = sl;

      display_ifconfig(sl->str, FLAG(a), val);
    }
  }
  fclose(fp);

  if (iface_name) display_ifconfig(iface_name, 1, 0);
  else {
    struct ifconf ifcon;
    struct ifreq *ifre;
    int num;

    // Loop until buffer's big enough
    ifcon.ifc_buf = NULL;
    for (num = 30;;num += 10) {
      ifcon.ifc_len = sizeof(struct ifreq)*num;
      ifcon.ifc_buf = xrealloc(ifcon.ifc_buf, ifcon.ifc_len);
      xioctl(TT.sockfd, SIOCGIFCONF, &ifcon);
      if (ifcon.ifc_len != sizeof(struct ifreq)*num) break;
    }

    ifre = ifcon.ifc_req;
    for(num = 0; num < ifcon.ifc_len && ifre; num += sizeof(struct ifreq), ifre++)
    {
      // Skip duplicates
      for(sl = ifaces; sl; sl = sl->next)
        if(!strcmp(sl->str, ifre->ifr_name)) break;

      if(!sl) display_ifconfig(ifre->ifr_name, FLAG(a), 0);
    }

    free(ifcon.ifc_buf);
  }

  llist_traverse(ifaces, free);
}

// Encode offset and size of field into an int, and make result negative
#define IFREQ_OFFSZ(x) -(int)((offsetof(struct ifreq, x)<<16) + sizeof(ifre.x))

void ifconfig_main(void)
{
  char **argv = toys.optargs;
  struct ifreq ifre;
  int i;

  TT.sockfd = xsocket(AF_INET, SOCK_DGRAM, 0);
  if(toys.optc < 2) {
    show_iface(*argv);
    return;
  }

  // Open interface
  memset(&ifre, 0, sizeof(struct ifreq));
  xstrncpy(ifre.ifr_name, *argv, IFNAMSIZ);

  // Perform operations on interface
  while(*++argv) {
    // Table of known operations
    struct argh {
      char *name;
      int on, off; // set, clear
    } try[] = {
      {0, IFF_UP|IFF_RUNNING, SIOCSIFADDR},
      {"up", IFF_UP|IFF_RUNNING, 0},
      {"down", 0, IFF_UP},
      {"arp", 0, IFF_NOARP},
      {"promisc", IFF_PROMISC, 0},
      {"allmulti", IFF_ALLMULTI, 0},
      {"multicast", IFF_MULTICAST, 0},
      {"pointopoint", IFF_POINTOPOINT, SIOCSIFDSTADDR},
      {"broadcast", IFF_BROADCAST, SIOCSIFBRDADDR},
      {"netmask", 0, SIOCSIFNETMASK},
      {"dstaddr", 0, SIOCSIFDSTADDR},
      {"mtu", IFREQ_OFFSZ(ifr_mtu), SIOCSIFMTU},
      {"rename", IFREQ_OFFSZ(ifr_newname), SIOCSIFNAME},
      {"keepalive", IFREQ_OFFSZ(ifr_data), SIOCDEVPRIVATE}, // SIOCSKEEPALIVE
      {"outfill", IFREQ_OFFSZ(ifr_data), SIOCDEVPRIVATE+2}, // SIOCSOUTFILL
      {"metric", IFREQ_OFFSZ(ifr_metric), SIOCSIFMETRIC},
      {"txqueuelen", IFREQ_OFFSZ(ifr_qlen), SIOCSIFTXQLEN},
      {"mem_start", IFREQ_OFFSZ(ifr_map.mem_start), SIOCSIFMAP},
      {"io_addr", IFREQ_OFFSZ(ifr_map.base_addr), SIOCSIFMAP},
      {"irq", IFREQ_OFFSZ(ifr_map.irq), SIOCSIFMAP},
      {"inet", 0, 0},
      {"inet6", 0, 0}
    };
    char *s = *argv;
    int rev = (*s == '-');

    s += rev;

    // "set hardware address" is oddball enough to special case
    if (!strcmp(*argv, "hw")) {
      char *hw_addr, *ptr, *p;
      struct sockaddr *sock = &ifre.ifr_hwaddr;
      int count = 6;

      ptr = p = (char *)sock->sa_data;
      memset(sock, 0, sizeof(struct sockaddr));
      if (argv[1]) {
        if (!strcmp("ether", *++argv)) sock->sa_family = ARPHRD_ETHER;
        else if (!strcmp("infiniband", *argv)) {
          sock->sa_family = ARPHRD_INFINIBAND;
          count = 20;
          p = ptr = toybuf;
        }
      }
      if (!sock->sa_family || !argv[1]) help_exit("bad hw '%s'", *argv);
      hw_addr = *++argv;

      // Parse and verify address.
      while (*hw_addr && (p-ptr) < count) {
        int val, len = 0;

        if (*hw_addr == ':') hw_addr++;
        sscanf(hw_addr, "%2x%n", &val, &len);
        if (!len || len > 2) break; // 1 nibble can be set e.g. C2:79:38:95:D:A
        hw_addr += len;
        *p++ = val;
      }

      if ((p-ptr) != count || *hw_addr)
        error_exit("bad hw-addr '%s'", *argv);

      // the linux kernel's "struct sockaddr" (include/linux/socket.h in the
      // kernel source) only has 14 bytes of sa_data, and an infiniband address
      // is 20. So if we go through the ioctl, the kernel will truncate
      // infiniband addresses, meaning we have to go through sysfs instead.
      if (sock->sa_family == ARPHRD_INFINIBAND && !strchr(ifre.ifr_name, '/')) {
        int fd;

        sprintf(toybuf, "/sys/class/net/%s/address", ifre.ifr_name);
        fd = xopen(toybuf, O_RDWR);
        xwrite(fd, *argv, strlen(*argv));
        close(fd);
      } else xioctl(TT.sockfd, SIOCSIFHWADDR, &ifre);
      continue;

    // Add/remove ipv6 address to interface

    } else if (!strcmp(*argv, "add") || !strcmp(*argv, "del")) {
      struct ifreq_inet6 {
        struct in6_addr addr;
        unsigned prefix;
        int index;
      } ifre6;
      int plen, fd6 = xsocket(AF_INET6, SOCK_DGRAM, 0);

      if (!argv[1]) help_exit("%s", *argv);

      plen = get_addrinfo(argv[1], AF_INET6, &ifre6.addr);
      if (plen < 0) plen = 128;
      xioctl(fd6, SIOCGIFINDEX, &ifre);
      ifre6.index = ifre.ifr_ifindex;
      ifre6.prefix = plen;
      xioctl(fd6, **(argv++)=='a' ? SIOCSIFADDR : SIOCDIFADDR, &ifre6);

      close(fd6);
      continue;
    // Iterate through table to find/perform operation
    } else for (i = 0; i<ARRAY_LEN(try); i++) {
      struct argh *t = try+i;
      int on = t->on, off = t->off;

      // First entry in list assigns address to interface (no command name)
      if (!t->name) {
        if (isdigit(**argv) || !strcmp(*argv, "default")) argv--;
        else continue;
      } else if (strcmp(t->name, s)) continue;

      // Is this an SIOCSI entry?
      if ((off|0xff) == 0x89ff) {
        if (!rev) {
          if (!*++argv) error_exit("%s needs argument", t->name);

          // Assign value to ifre field and call ioctl? (via IFREQ_OFFSZ.)
          if (on < 0) {
            void *dest = ((on = -on)>>16)+(char *)&ifre;

            // If we're about to set mem_start/io_addr/irq, get other 2 first
            if (off == SIOCSIFMAP) xioctl(TT.sockfd, SIOCGIFMAP, &ifre);
            if (off == SIOCSIFNAME) xstrncpy(dest, *argv, on&0xffff);
            else poke(dest, strtoul(*argv, 0, 0), on&15);
            xioctl(TT.sockfd, off, &ifre);
            break;
          } else {
            struct sockaddr_in *si = (struct sockaddr_in *)&ifre.ifr_addr;
            int mask = -1;

            si->sin_family = AF_INET;

            if (!strcmp(*argv, "default")) si->sin_addr.s_addr = INADDR_ANY;
            else mask = get_addrinfo(*argv, AF_INET, &si->sin_addr);
            xioctl(TT.sockfd, off, &ifre);

            // Handle /netmask
            if (mask >= 0) {
              // sin_addr probably isn't unaligned, but just in case...
              mask = htonl((~0)<<(32-mask));
              memcpy(&si->sin_addr, &mask, 4);
              xioctl(TT.sockfd, SIOCSIFNETMASK, &ifre);
            }
          }
        }
        off = 0;
      }

      // Set flags
      if (on || off) {
        xioctl(TT.sockfd, SIOCGIFFLAGS, &ifre);
        ifre.ifr_flags &= ~(rev ? on : off);
        ifre.ifr_flags |= (rev ? off : on);
        xioctl(TT.sockfd, SIOCSIFFLAGS, &ifre);
      }

      break;
    }
    if (i == ARRAY_LEN(try)) help_exit("bad argument '%s'", *argv);
  }
  close(TT.sockfd);
}
