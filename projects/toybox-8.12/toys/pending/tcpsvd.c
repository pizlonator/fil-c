/* tcpsvd.c - TCP(UDP)/IP service daemon
 *
 * Copyright 2013 Ashwini Kumar <ak.ashwini@gmail.com>
 * Copyright 2013 Sandeep Sharma <sandeep.jack2756@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard.

USE_TCPSVD(NEWTOY(tcpsvd, "^<3c#=30<1b#=20<0C:u:l:hEv", TOYFLAG_USR|TOYFLAG_BIN))
USE_TCPSVD(OLDTOY(udpsvd, tcpsvd, TOYFLAG_USR|TOYFLAG_BIN))

config TCPSVD
  bool "tcpsvd"
  default n
  depends on TOYBOX_FORK
  help
    usage: tcpsvd [-hEv] [-c N] [-C N[:MSG]] [-b N] [-u User] [-l Name] IP Port Prog
    usage: udpsvd [-hEv] [-c N] [-u User] [-l Name] IP Port Prog

    Create TCP/UDP socket, bind to IP:PORT and listen for incoming connection.
    Run PROG for each connection.

    IP            IP to listen on, 0 = all
    PORT          Port to listen on
    PROG ARGS     Program to run
    -l NAME       Local hostname (else looks up local hostname in DNS)
    -u USER[:GRP] Change to user/group after bind
    -c N          Handle up to N (> 0) connections simultaneously
    -b N          (TCP Only) Allow a backlog of approximately N TCP SYNs
    -C N[:MSG]    (TCP Only) Allow only up to N (> 0) connections from the same IP
                  New connections from this IP address are closed
                  immediately. MSG is written to the peer before close
    -h            Look up peer's hostname
    -E            Don't set up environment variables
    -v            Verbose
*/

#define FOR_tcpsvd
#include "toys.h"

GLOBALS(
  char *l, *u, *C;
  long b, c;

  int maxc;
  int count_all;
  int udp;
)

struct list_pid {
  struct list_pid *next;
  char *ip;
  int pid;
};

struct list {
  struct list* next;
  char *d;
  int count;
};

struct hashed {
  struct list *head;
};

#define HASH_NR 256
struct hashed h[HASH_NR];
struct list_pid *pids = NULL;

// convert IP address to string.
static char *sock_to_address(struct sockaddr *sock, int flags)
{
  char hbuf[NI_MAXHOST] = {0,};
  char sbuf[NI_MAXSERV] = {0,};
  int status = 0;
  socklen_t len = sizeof(struct sockaddr_in6);

  if (!(status = getnameinfo(sock, len, hbuf, sizeof(hbuf), sbuf,
          sizeof(sbuf), flags))) {
    if (flags & NI_NUMERICSERV) return xmprintf("%s:%s",hbuf, sbuf);
    return xmprintf("%s",hbuf);
  }
  error_exit("getnameinfo: %s", gai_strerror(status));
}

// Insert pid, ip and fd in the list.
static void insert(struct list_pid **l, int pid, char *addr)
{
  struct list_pid *newnode = xmalloc(sizeof(struct list_pid));
  newnode->pid = pid;
  newnode->ip = addr;
  newnode->next = NULL;
  if (!*l) *l = newnode;
  else {
    newnode->next = (*l);
   *l = newnode;
  }
}

// Hashing of IP address.
static int haship(char *addr)
{
  uint32_t ip[8] = {0,};
  int count = 0, i = 0;

  if (!addr) error_exit("NULL ip");
  while (i < strlen(addr)) {
    while (addr[i] && (addr[i] != ':') && (addr[i] != '.')) {
      ip[count] = ip[count]*10 + (addr[i]-'0');
      i++;
    }
    if (i >= strlen(addr)) break;
    count++;
    i++;
  }
  return (ip[0]^ip[1]^ip[2]^ip[3]^ip[4]^ip[5]^ip[6]^ip[7])%HASH_NR;
}

// Remove a node from the list.
static char *delete(struct list_pid **pids, int pid)
{
  struct list_pid *prev, *free_node, *head = *pids;
  char *ip = NULL;

  if (!head) return NULL;
  prev = free_node = NULL;
  while (head) {
    if (head->pid == pid) {
      ip = head->ip;
      free_node = head;
      if (!prev) *pids = head->next;
      else prev->next = head->next;
      free(free_node);
      return ip;
    }
    prev = head;
    head = head->next;
  }
  return NULL;
}

// decrement the ref count fora connection, if count reches ZERO then remove the node
static void remove_connection(char *ip)
{
  struct list *head, *prev = NULL, *free_node = NULL;
  int hash = haship(ip);

  head = h[hash].head;
  while (head) {
    if (!strcmp(ip, head->d)) {
      head->count--;
      free_node = head;
      if (!head->count) {
        if (!prev) h[hash].head = head->next;
        else prev->next = head->next;
        free(free_node);
      }
      break;
    }
    prev = head;
    head = head->next;
  }
  free(ip);
}

// Handler function.
static void handle_exit(int sig)
{
  int status;
  pid_t pid_n = wait(&status);
  char *ip = (pid_n<1) ? 0 : delete(&pids, pid_n);

  if (!ip) return;
  remove_connection(ip);
  TT.count_all--;
  if (FLAG(v)) {
    if (WIFEXITED(status))
      xprintf("%s: end %d exit %d\n",toys.which->name, pid_n, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
      xprintf("%s: end %d signaled %d\n",toys.which->name, pid_n, WTERMSIG(status));
    if (TT.c > 1) xprintf("%s: status %d/%ld\n",toys.which->name, TT.count_all, TT.c);
  }
}

// Grab uid and gid
static void get_uidgid(uid_t *uid, gid_t *gid, char *ug)
{
  struct passwd *pass = NULL;
  struct group *grp = NULL;
  char *user = NULL, *group = NULL;
  unsigned int n;

  user = ug;
  group = strchr(ug,':');
  if (group) {
    *group = '\0';
    group++;
  }
  if (!(pass = getpwnam(user))) {
    n = atolx_range(user, 0, INT_MAX);
    if (!(pass = getpwuid(n))) perror_exit("Invalid user '%s'", user);
  }
  *uid = pass->pw_uid;
  *gid = pass->pw_gid;

  if (group) {
    if (!(grp = getgrnam(group))) {
      n = atolx_range(group, 0, INT_MAX);
      if (!(grp = getgrgid(n))) perror_exit("Invalid group '%s'",group);
    }
  }
  if (grp) *gid = grp->gr_gid;
}

// Bind socket.
static int create_bind_sock(char *host, struct sockaddr *haddr)
{
  struct addrinfo hints, *res = NULL, *rp;
  int sockfd, ret, set = 1;
  char *ptr;
  unsigned long port;

  errno = 0;
  port = strtoul(toys.optargs[1], &ptr, 10);
  if (errno || port > 65535)
    error_exit("Invalid port, Range is [0-65535]");
  if (*ptr) ptr = toys.optargs[1];
  else {
    sprintf(toybuf, "%lu", port);
    ptr = toybuf;
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = (TT.udp ? SOCK_DGRAM : SOCK_STREAM);
  if ((ret = getaddrinfo(host, ptr, &hints, &res)))
    perror_exit("%s", gai_strerror(ret));

  for (rp = res; rp; rp = rp->ai_next)
    if ( (rp->ai_family == AF_INET) || (rp->ai_family == AF_INET6)) break;

  if (!rp) error_exit("Invalid IP %s", host);

  sockfd = xsocket(rp->ai_family, TT.udp ? SOCK_DGRAM : SOCK_STREAM, 0);
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set));
  if (TT.udp) setsockopt(sockfd, IPPROTO_IP, IP_PKTINFO, &set, sizeof(set));
  xbind(sockfd, rp->ai_addr, rp->ai_addrlen);
  if(haddr) memcpy(haddr, rp->ai_addr, rp->ai_addrlen);
  freeaddrinfo(res);
  return sockfd;
}

static void handle_signal(int sig)
{
  if (FLAG(v)) xprintf("got signal %d, exit\n", sig);
  raise(sig);
  _exit(sig + 128); //should not reach here
}

void tcpsvd_main(void)
{
  uid_t uid = 0;
  gid_t gid = 0;
  pid_t pid;
  char haddr[sizeof(struct sockaddr_in6)];
  struct list *head, *newnode;
  int hash, fd, newfd, j;
  char *ptr = NULL, *addr, *server, buf[sizeof(struct sockaddr_in6)];
  socklen_t len = sizeof(buf);

  TT.udp = *toys.which->name == 'u';
  if (TT.udp) toys.optflags &= ~FLAG_C;
  memset(buf, 0, len);
  if (FLAG(C)) {
    if ((ptr = strchr(TT.C, ':'))) *ptr++ = 0;
    TT.maxc = atolx_range(TT.C, 1, INT_MAX);
  }

  fd = create_bind_sock(toys.optargs[0], (struct sockaddr*)&haddr);
  if (FLAG(u)) {
    get_uidgid(&uid, &gid, TT.u);
    setuid(uid);
    setgid(gid);
  }

  if (!TT.udp && (listen(fd, TT.b) < 0)) perror_exit("Listen failed");
  server = sock_to_address((struct sockaddr*)&haddr, NI_NUMERICHOST|NI_NUMERICSERV);
  if (FLAG(v)) {
    if (FLAG(u))
      xprintf("%s: listening on %s, starting, uid %u, gid %u\n",
        toys.which->name, server, uid, gid);
    else
      xprintf("%s: listening on %s, starting\n", toys.which->name, server);
  }
  for (j = 0; j < HASH_NR; j++) h[j].head = 0;
  sigatexit(handle_signal);
  signal(SIGCHLD, handle_exit);

  while (1) {
    if (TT.count_all < TT.c) {
      if (TT.udp) {
        if (recvfrom(fd, 0, 0, MSG_PEEK, (void *)buf, &len) < 0)
          perror_exit("recvfrom");
        newfd = fd;
      } else {
        newfd = accept(fd, (struct sockaddr *)buf, &len);
        if (newfd < 0) perror_exit("Error on accept");
      }
    } else {
      sigset_t ss;
      sigemptyset(&ss);
      sigsuspend(&ss);
      continue;
    }
    TT.count_all++;
    addr = sock_to_address((struct sockaddr*)buf, NI_NUMERICHOST);

    hash = haship(addr);
    if (FLAG(C)) {
      for (head = h[hash].head; head; head = head->next)
        if (!strcmp(head->d, addr)) break;

      if (head && head->count >= TT.maxc) {
        if (ptr) write(newfd, ptr, strlen(ptr)); // TODO: this can block
        close(newfd);
        TT.count_all--;
        continue;
      }
    }

    newnode = (struct list*)xzalloc(sizeof(struct list));
    newnode->d = addr;
    for (head = h[hash].head; head; head = head->next) {
      if (!strcmp(addr, head->d)) {
        head->count++;
        free(newnode);
        break;
      }
    }

    if (!head) {
      newnode->next = h[hash].head;
      h[hash].head = newnode;
      h[hash].head->count++;
    }

    if (!(pid = xfork())) {
      char *serv = NULL, *clie = NULL;
      char *client = sock_to_address((void *)buf, NI_NUMERICHOST | NI_NUMERICSERV);
      if (FLAG(h)) { //lookup name
        serv = TT.l ? xstrdup(TT.l) : sock_to_address((void *)&haddr, 0);
        clie = sock_to_address((void *)buf, 0);
      }

      if (!FLAG(E)) {
        setenv("PROTO", TT.udp ? "UDP" :"TCP", 1);
        setenv("PROTOLOCALADDR", server, 1);
        setenv("PROTOREMOTEADDR", client, 1);
        if (FLAG(h)) {
          setenv("PROTOLOCALHOST", serv, 1);
          setenv("PROTOREMOTEHOST", clie, 1);
        }
        if (!TT.udp) {
          char max_c[32];
          sprintf(max_c, "%d", TT.maxc);
          setenv("TCPCONCURRENCY", max_c, 1); //Not valid for udp
        }
      }
      if (FLAG(v)) {
        xprintf("%s: start %d %s-%s",toys.which->name, getpid(), server, client);
        if (FLAG(h)) xprintf(" (%s-%s)", serv, clie);
        xputc('\n');
        if (TT.c > 1)
          xprintf("%s: status %d/%ld\n",toys.which->name, TT.count_all, TT.c);
      }
      free(client);
      if (FLAG(h)) {
        free(serv);
        free(clie);
      }
      if (TT.udp) xconnect(newfd, (struct sockaddr *)buf, sizeof(buf));

      close(0);
      close(1);
      dup2(newfd, 0);
      dup2(newfd, 1);
      xexec(toys.optargs+2); //skip IP PORT
    } else {
      insert(&pids, pid, addr);
      xclose(newfd); //close and reopen for next client.
      if (TT.udp) fd = create_bind_sock(toys.optargs[0],
          (struct sockaddr*)&haddr);
    }
  } //while(1)
}
