/* netstat.c - Display Linux networking subsystem.
 *
 * Copyright 2012 Ranjan Kumar <ranjankumar.bth@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * Not in SUSv4.
 *
USE_NETSTAT(NEWTOY(netstat, "pWrxwutneal", TOYFLAG_BIN))
config NETSTAT
  bool "netstat"
  default y
  help
    usage: netstat [-pWrxwutneal]

    Display networking information. Default is netstat -tuwx

    -r	Routing table
    -a	All sockets (not just connected)
    -l	Listening server sockets
    -t	TCP sockets
    -u	UDP sockets
    -w	Raw sockets
    -x	Unix sockets
    -e	Extended info
    -n	Don't resolve names
    -W	Wide display
    -p	Show PID/program name of sockets
*/

#define FOR_netstat
#include "toys.h"
#include <net/route.h>

GLOBALS(
  struct num_cache *inodes;
  int wpad;
)

struct num_cache {
  struct num_cache *next;
  long long num;
  char data[];
};

// Find num in cache
static struct num_cache *get_num_cache(struct num_cache *cache, long long num)
{
  while (cache) {
    if (num==cache->num) return cache;
    cache = cache->next;
  }

  return 0;
}

// Uniquely add num+data to cache. Updates *cache, returns pointer to existing
// entry if it was already there.
static struct num_cache *add_num_cache(struct num_cache **cache, long long num,
  void *data, int len)
{
  struct num_cache *old = get_num_cache(*cache, num);

  if (old) return old;

  old = xzalloc(sizeof(struct num_cache)+len);
  old->next = *cache;
  old->num = num;
  memcpy(old->data, data, len);

  *cache = old;

  return 0;
}

static void addr2str(int af, void *addr, unsigned port, char *buf, int len,
  char *proto)
{
  char pres[INET6_ADDRSTRLEN];
  struct servent *se = 0;
  int pos, count;

  if (!inet_ntop(af, addr, pres, sizeof(pres))) perror_exit("inet_ntop");

  if (FLAG(n) || !port) {
    strcpy(buf, pres);
  } else {
    struct addrinfo hints, *result, *rp;
    char cut[4];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = af;

    if (!getaddrinfo(pres, NULL, &hints, &result)) {
      socklen_t sock_len = (af == AF_INET) ? sizeof(struct sockaddr_in)
        : sizeof(struct sockaddr_in6);

      // We assume that a failing getnameinfo dosn't stomp "buf" here.
      for (rp = result; rp; rp = rp->ai_next)
        if (!getnameinfo(rp->ai_addr, sock_len, buf, 256, 0, 0, 0)) break;
      freeaddrinfo(result);
      buf[len] = 0;
    }

    // getservbyport() doesn't understand proto "tcp6", so truncate
    memcpy(cut, proto, 3);
    cut[3] = 0;
    se = getservbyport(htons(port), cut);
  }

  if (!strcmp(buf, "::")) strcpy(buf, "[::]");

  // Append :service or :* if port == 0.
  if (se) {
    count = snprintf(0, 0, ":%s", se->s_name);
    // NI_MAXSERV == 32, which is greater than our minimum field width.
    // (Although the longest service name on Debian in 2021 is only 16 bytes.)
    if (count>=len) {
      count = len-1;
      se->s_name[count] = 0;
    }
  } else count = port ? snprintf(0, 0, ":%u", port) : 2;
  // We always show the port, even if that means clobbering the end of the host.
  pos = strlen(buf);
  if (len-pos<count) pos = len-count;
  if (se) sprintf(buf+pos, ":%s", se->s_name);
  else sprintf(buf+pos, port ? ":%u" : ":*", port);
}

// Display info for tcp/udp/raw
static void show_ip(char *fname)
{
  char *ss_state = "UNKNOWN", buf[12], *s, *label = strrchr(fname, '/')+1;
  char *state_label[] = {"", "ESTABLISHED", "SYN_SENT", "SYN_RECV", "FIN_WAIT1",
                         "FIN_WAIT2", "TIME_WAIT", "CLOSE", "CLOSE_WAIT",
                         "LAST_ACK", "LISTEN", "CLOSING", "UNKNOWN"};
  FILE *fp = xfopen(fname, "r");

  // Skip header.
  (void)fgets(toybuf, sizeof(toybuf), fp);

  while (fgets(toybuf, sizeof(toybuf), fp)) {
    char lip[256], rip[256];
    union {
      struct {unsigned u; unsigned char b[4];} i4;
      struct {struct {unsigned a, b, c, d;} u; unsigned char b[16];} i6;
    } laddr, raddr;
    unsigned lport, rport, state, txq, rxq, num, uid, af = AF_INET6;
    unsigned long inode;

    // Try ipv6, then try ipv4
    if (16 != sscanf(toybuf,
      " %d: %8x%8x%8x%8x:%x %8x%8x%8x%8x:%x %x %x:%x %*X:%*X %*X %d %*d %ld",
      &num, &laddr.i6.u.a, &laddr.i6.u.b, &laddr.i6.u.c,
      &laddr.i6.u.d, &lport, &raddr.i6.u.a, &raddr.i6.u.b,
      &raddr.i6.u.c, &raddr.i6.u.d, &rport, &state, &txq, &rxq,
      &uid, &inode))
    {
      af = AF_INET;
      if (10 != sscanf(toybuf,
        " %d: %x:%x %x:%x %x %x:%x %*X:%*X %*X %d %*d %ld",
        &num, &laddr.i4.u, &lport, &raddr.i4.u, &rport, &state, &txq,
        &rxq, &uid, &inode)) continue;
    }

    // Should we display this? (listening or all or TCP/UDP/RAW)
    if (!(FLAG(l) && (!rport && (state&0xA))) && !FLAG(a) && !(rport&0x70))
      continue;

    addr2str(af, &laddr, lport, lip, TT.wpad, label);
    addr2str(af, &raddr, rport, rip, TT.wpad, label);

    // Display data
    s = label;
    if (strstart(&s, "tcp")) {
      int sz = ARRAY_LEN(state_label);
      if (!state || state >= sz) state = sz-1;
      ss_state = state_label[state];
    } else if (strstart(&s, "udp")) {
      if (state == 1) ss_state = state_label[state];
      else if (state == 7) ss_state = "";
    } else if (strstart(&s, "raw")) sprintf(ss_state = buf, "%u", state);

    printf("%-6s%6d%7d %*.*s %*.*s %-11s", label, rxq, txq, -TT.wpad, TT.wpad,
      lip, -TT.wpad, TT.wpad, rip, ss_state);
    if (FLAG(e)) {
      if (FLAG(n)) sprintf(s = toybuf, "%d", uid);
      else s = getusername(uid);
      printf(" %-10s %-11ld", s, inode);
    }
    if (FLAG(p)) {
      struct num_cache *nc = get_num_cache(TT.inodes, inode);

      printf(" %s", nc ? nc->data : "-");
    }
    xputc('\n');
  }
  fclose(fp);
}

static void show_unix_sockets(void)
{
  char *types[] = {"","STREAM","DGRAM","RAW","RDM","SEQPACKET","DCCP","PACKET"},
       *states[] = {"","LISTENING","CONNECTING","CONNECTED","DISCONNECTING"},
       *filename = 0;
  unsigned long refcount, flags, type, state, inode;
  FILE *fp = xfopen("/proc/net/unix", "r");

  // Skip header.
  (void)fgets(toybuf, sizeof(toybuf), fp);

  while (fscanf(fp, "%*p: %lX %*X %lX %lX %lX %lu%m[^\n]", &refcount, &flags,
                &type, &state, &inode, &filename) >= 5) {
    // Linux exports only SO_ACCEPTCON since 2.3.15pre3 in 1999, but let's
    // filter in case they add more someday.
    flags &= 1<<16;

    // Only show unconnected listening sockets with -a or -l.
    if (state==1 && flags && !(FLAG(a) || FLAG(l))) continue;

    if (type==10) type = 7; // move SOCK_PACKET into line
    if (type>=ARRAY_LEN(types)) type = 0;
    if (state>=ARRAY_LEN(states) || (state==1 && !flags)) state = 0;

    if (state!=1 && FLAG(l)) continue;

    sprintf(toybuf, "[ %s]", flags ? "ACC " : "");
    printf("unix  %-6ld %-11s %-10s %-13s %-8lu ",
      refcount, toybuf, types[type], states[state], inode);
    if (FLAG(p)) {
      struct num_cache *nc = get_num_cache(TT.inodes, inode);

      printf("%-19.19s ", nc ? nc->data : "-");
    }

    if (filename) {
      printf("%s\n", filename+!FLAG(p));
      free(filename);
      filename = 0;
    } else xputc('\n');
  }
  fclose(fp);
}

static int scan_pids(struct dirtree *node)
{
  char *s = toybuf+256;
  struct dirent *entry;
  DIR *dp;
  int pid, dirfd;

  if (!node->parent) return DIRTREE_RECURSE;
  if (!(pid = atol(node->name))) return 0;

  sprintf(toybuf, "/proc/%d/cmdline", pid);
  if (!(readfile(toybuf, toybuf, 256))) return 0;

  sprintf(s, "%d/fd", pid);
  if (-1==(dirfd = openat(dirtree_parentfd(node), s, O_RDONLY))) return 0;
  if (!(dp = fdopendir(dirfd))) close(dirfd);
  else while ((entry = readdir(dp))) {
    s = toybuf+256;
    if (!readlinkat0(dirfd, entry->d_name, s, sizeof(toybuf)-256)) continue;
    // Can the "[0000]:" happen in a modern kernel?
    if (strstart(&s, "socket:[") || strstart(&s, "[0000]:")) {
      long long ll = atoll(s);

      sprintf(s, "%d/%s", pid, getbasename(toybuf));
      add_num_cache(&TT.inodes, ll, s, strlen(s)+1);
    }
  }
  closedir(dp);

  return 0;
}

// extract inet4 route info from /proc/net/route file and display it.
static void display_routes(void)
{
  static const char flagchars[] = "GHRDMDAC";
  static const unsigned flagarray[] = {
    RTF_GATEWAY, RTF_HOST, RTF_REINSTATE, RTF_DYNAMIC, RTF_MODIFIED
  };
  unsigned dest, gate, mask;
  int flags, ref, use, metric, mss, win, irtt;
  char *out = toybuf, *flag_val;
  char iface[64]={0};
  FILE *fp = xfopen("/proc/net/route", "r");

  // Skip header.
  (void)fgets(toybuf, sizeof(toybuf), fp);

  printf("Kernel IP routing table\n"
          "Destination\tGateway \tGenmask \tFlags %s Iface\n",
          !FLAG(e) ? "  MSS Window  irtt" : "Metric Ref    Use");

  while (fscanf(fp, "%63s%x%x%X%d%d%d%x%d%d%d", iface, &dest, &gate, &flags,
                &ref, &use, &metric, &mask, &mss, &win, &irtt) == 11) {
    char *destip = 0, *gateip = 0, *maskip = 0;

    // skip down interfaces.
    if (!(flags & RTF_UP)) continue;

// TODO /proc/net/ipv6_route

    if (dest) {
      if (inet_ntop(AF_INET, &dest, out, 16)) destip = out;
    } else destip = FLAG(n) ? "0.0.0.0" : "default";
    out += 16;

    if (gate) {
      if (inet_ntop(AF_INET, &gate, out, 16)) gateip = out;
    } else gateip = FLAG(n) ? "0.0.0.0" : "*";
    out += 16;

// TODO /24
    //For Mask
    if (inet_ntop(AF_INET, &mask, out, 16)) maskip = out;
    else maskip = "?";
    out += 16;

    //Get flag Values
    flag_val = out;
    *out++ = 'U';
    for (dest = 0; dest < ARRAY_LEN(flagarray); dest++)
      if (flags&flagarray[dest]) *out++ = flagchars[dest];
    *out = 0;
    if (flags & RTF_REJECT) *flag_val = '!';

    printf("%-15.15s %-15.15s %-16s%-6s", destip, gateip, maskip, flag_val);
    if (!FLAG(e)) printf("%5d %-5d %6d %s\n", mss, win, irtt, iface);
    else printf("%-6d %-2d %7d %s\n", metric, ref, use, iface);
  }
  fclose(fp);
}

void netstat_main(void)
{
  int tuwx = FLAG_t|FLAG_u|FLAG_w|FLAG_x;
  char *type = "w/o servers";

  TT.wpad = FLAG(W) ? 51 : 23;
  if (!(toys.optflags&(FLAG_r|tuwx))) toys.optflags |= tuwx;
  if (FLAG(r)) display_routes();
  if (!(toys.optflags&tuwx)) return;

  if (FLAG(a)) type = "servers and established";
  else if (FLAG(l)) type = "only servers";

  if (FLAG(p)) dirtree_read("/proc", scan_pids);

  if (toys.optflags&(FLAG_t|FLAG_u|FLAG_w)) {
    printf("Active Internet connections (%s)\n", type);
    printf("Proto Recv-Q Send-Q %*s %*s State      ", -TT.wpad, "Local Address",
      -TT.wpad, "Foreign Address");
    if (FLAG(e)) printf(" User       Inode      ");
    if (FLAG(p)) printf(" PID/Program Name");
    xputc('\n');

    if (FLAG(t)) {
      show_ip("/proc/net/tcp");
      show_ip("/proc/net/tcp6");
    }
    if (FLAG(u)) {
      show_ip("/proc/net/udp");
      show_ip("/proc/net/udp6");
    }
    if (FLAG(w)) {
      show_ip("/proc/net/raw");
      show_ip("/proc/net/raw6");
    }
  }

  if (FLAG(x)) {
    printf("Active UNIX domain sockets (%s)\n", type);
    printf("Proto RefCnt Flags       Type       State         I-Node%sPath\n",
           FLAG(p) ? "   PID/Program Name     " : "   ");
    show_unix_sockets();
  }

  if (FLAG(p) && CFG_TOYBOX_FREE) llist_traverse(TT.inodes, free);
  toys.exitval = 0;
}
