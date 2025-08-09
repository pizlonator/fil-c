#include "toys.h"

int xsocket(int domain, int type, int protocol)
{
  int fd = socket(domain, type, protocol);

  if (fd < 0) perror_exit("socket %x %x", type, protocol);
  fcntl(fd, F_SETFD, FD_CLOEXEC);

  return fd;
}

void xsetsockopt(int fd, int level, int opt, void *val, socklen_t len)
{
  if (-1 == setsockopt(fd, level, opt, val, len)) perror_exit("setsockopt");
}

// if !host bind to all local interfaces
struct addrinfo *xgetaddrinfo(char *host, char *port, int family, int socktype,
  int protocol, int flags)
{
  struct addrinfo info, *ai;
  int rc;

  memset(&info, 0, sizeof(struct addrinfo));
  info.ai_family = family;
  info.ai_socktype = socktype;
  info.ai_protocol = protocol;
  info.ai_flags = flags;
  if (!host) info.ai_flags |= AI_PASSIVE;

  rc = getaddrinfo(host, port, &info, &ai);
  if (rc || !ai)
    error_exit("%s%s%s: %s", host ? host : "*", port ? ":" : "",
      port ? port : "", rc ? gai_strerror(rc) : "not found");

  return ai;
}

static int xconnbind(struct addrinfo *ai_arg, int dobind)
{
  struct addrinfo *ai;
  int fd = -1, one = 1;

  // Try all the returned addresses. Report errors if last entry can't connect.
  for (ai = ai_arg; ai; ai = ai->ai_next) {
    fd = (ai->ai_next ? socket : xsocket)(ai->ai_family, ai->ai_socktype,
      ai->ai_protocol);
    xsetsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (!(dobind ? bind : connect)(fd, ai->ai_addr, ai->ai_addrlen)) break;
    else if (!ai->ai_next) perror_exit_raw(dobind ? "bind" : "connect");
    close(fd);
  }
  freeaddrinfo(ai_arg);

  return fd;
}

int xconnectany(struct addrinfo *ai)
{
  return xconnbind(ai, 0);
}


int xbindany(struct addrinfo *ai)
{
  return xconnbind(ai, 1);
}

void xbind(int fd, const struct sockaddr *sa, socklen_t len)
{
  if (bind(fd, sa, len)) perror_exit("bind");
}

void xconnect(int fd, const struct sockaddr *sa, socklen_t len)
{
  if (connect(fd, sa, len)) perror_exit("connect");
}

int xpoll(struct pollfd *fds, int nfds, int timeout)
{
  int i;
  long long now, then = timeout>0 ? millitime() : 0;

  for (;;) {
    if (0<=(i = poll(fds, nfds, timeout)) || toys.signal) return i;
    if (errno != EINTR && errno != ENOMEM) perror_exit("xpoll");
    else {
      now = millitime();
      timeout -= now-then;
      then = now;
    }
  }
}

// Loop forwarding data from in1 to out1 and in2 to out2, handling
// half-connection shutdown. timeouts return if no data for X ms.
// Returns 0: both closed, 1 shutdown_timeout, 2 timeout
int pollinate(int in1, int in2, int out1, int out2,
              void (*callback)(int fd, void *buf, size_t len),
              int timeout, int shutdown_timeout)
{
  struct pollfd pollfds[2];
  int i, pollcount = 2;

  memset(pollfds, 0, 2*sizeof(struct pollfd));
  pollfds[0].events = pollfds[1].events = POLLIN;
  pollfds[0].fd = in1;
  pollfds[1].fd = in2;

  // Poll loop copying data from each fd to the other one.
  for (;;) {
    if (!xpoll(pollfds, pollcount, timeout)) return pollcount;

    for (i=0; i<pollcount; i++) {
      if (pollfds[i].revents & POLLIN) {
        int len = read(pollfds[i].fd, libbuf, sizeof(libbuf));
        if (len<1) pollfds[i].revents = POLLHUP;
        else {
          callback(i ? out2 : out1, libbuf, len);
          continue;
        }
      }
      if (pollfds[i].revents & POLLHUP) {
        // Close half-connection.  This is needed for things like
        // "echo GET / | netcat landley.net 80"
        // Note that in1 closing triggers timeout, in2 returns now.
        if (i) {
          shutdown(pollfds[0].fd, SHUT_WR);
          pollcount--;
          timeout = shutdown_timeout;
        } else return 0;
      }
    }
  }
}

// Return converted ipv4/ipv6 numeric address in libbuf
char *ntop(struct sockaddr *sa)
{
  void *addr;

  if (sa->sa_family == AF_INET) addr = &((struct sockaddr_in *)sa)->sin_addr;
  else addr = &((struct sockaddr_in6 *)sa)->sin6_addr;

  inet_ntop(sa->sa_family, addr, libbuf, sizeof(libbuf));

  return libbuf;
}

void xsendto(int sockfd, void *buf, size_t len, struct sockaddr *dest)
{
  int rc = sendto(sockfd, buf, len, 0, dest,
    dest->sa_family == AF_INET ? sizeof(struct sockaddr_in) :
      sizeof(struct sockaddr_in6));

  if (rc != len) perror_exit("sendto");
}

// xrecvfrom with timeout in milliseconds
int xrecvwait(int fd, char *buf, int len, union socksaddr *sa, int timeout)
{
  socklen_t sl = sizeof(*sa);

  if (timeout >= 0) {
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = POLLIN;
    if (!xpoll(&pfd, 1, timeout)) return 0;
  }

  len = recvfrom(fd, buf, len, 0, (void *)sa, &sl);
  if (len<0) perror_exit("recvfrom");

  return len;
}

// Convert space/low ascii to %XX escapes, plus any chars in "and" string.
// Returns newly allocated copy of string (even if no changes)
char *escape_url(char *str, char *and)
{
  int i, j , count;
  char *ret QUIET, *ss QUIET;

  for (j = count = 0;;) {
    for (i = 0;;) {
      if (str[i] && (str[i]<=' ' || (and && strchr(and, str[i])))) {
        if (j) ss += sprintf(ss, "%%%02x", str[i]);
        else count++;
      } else if (j) *ss++ = str[i];
      if (!str[i++]) break;
    }
    if (j++) break;
    ret = ss = xmalloc(i+count*2);
  }

  return ret;
}

// Convert %XX escapes to character (in place)
char *unescape_url(char *str, int do_cut)
{
  char *to, *cut = do_cut ? strchr(str, '?') : 0;
  int i;

  for (to = str;;) {
    if (*str!='%' || !isxdigit(str[1]) || !isxdigit(str[2])) {
      if (str==cut) {
        *to = 0;
        cut++;

        break;
      } else if (!(*to++ = *str++)) break;
    } else {
      sscanf(++str, "%2x", &i);
      *to++ = i;
      str += 2;
    }
  }

  return cut;
}
