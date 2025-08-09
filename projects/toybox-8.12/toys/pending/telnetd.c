/* telnetd.c - Telnet Server
 *
 * Copyright 2013 Sandeep Sharma <sandeep.jack2756@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
USE_TELNETD(NEWTOY(telnetd, "w#<0b:p#<0>65535=23f:l:FSKi[!wi]", TOYFLAG_USR|TOYFLAG_BIN))

config TELNETD
  bool "telnetd"
  default n
  help
    Handle incoming telnet connections

    -l LOGIN  Exec LOGIN on connect
    -f ISSUE_FILE Display ISSUE_FILE instead of /etc/issue
    -K Close connection as soon as login exits
    -p PORT   Port to listen on
    -b ADDR[:PORT]  Address to bind to
    -F Run in foreground
    -i Inetd mode
    -w SEC    Inetd 'wait' mode, linger time SEC
    -S Log to syslog (implied by -i or without -F and -w)
*/

#define FOR_telnetd
#include "toys.h"
#include <arpa/telnet.h>

GLOBALS(
    char *login_path;
    char *issue_path;
    int port;
    char *host_addr;
    long w_sec;

    int gmax_fd;
    pid_t fork_pid;
)

#define BUFSIZE 4*1024
struct term_session {
  int new_fd, pty_fd;
  pid_t child_pid;
  int buff1_avail, buff2_avail;
  int buff1_written, buff2_written;
  int rem;  //unprocessed data from socket
  char buff1[BUFSIZE], buff2[BUFSIZE];
  struct term_session *next;
};

struct term_session *session_list = NULL;

static void get_sockaddr(char *host, void *buf)
{
  in_port_t port_num = htons(TT.port);
  struct addrinfo hints, *result;
  int status, af = AF_UNSPEC;
  char *s;

  // [ipv6]:port or exactly one :
  if (*host == '[') {
    host++;
    s = strchr(host, ']');
    if (s) *s++ = 0;
    else error_exit("bad address '%s'", host-1);
    af = AF_INET6;
  } else {
    s = strrchr(host, ':');
    if (s && strchr(host, ':') == s) {
      *s = 0;
      af = AF_INET;
    } else if (s && strchr(host, ':') != s) {
      af = AF_INET6;
      s = 0;
    }
  }

  if (s++) {
    char *ss;
    unsigned long p = strtoul(s, &ss, 0);
    if (!*s || *ss || p > 65535) error_exit("bad port '%s'", s);
    port_num = htons(p);
  }

  memset(&hints, 0 , sizeof(struct addrinfo));
  hints.ai_family = af;
  hints.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(host, NULL, &hints, &result);
  if (status) error_exit("bad address '%s' : %s", host, gai_strerror(status));

  memcpy(buf, result->ai_addr, result->ai_addrlen);
  freeaddrinfo(result);

  if (af == AF_INET) ((struct sockaddr_in*)buf)->sin_port = port_num;
  else ((struct sockaddr_in6*)buf)->sin6_port = port_num;
}

static int listen_socket(void)
{
  int s, af = AF_INET, yes = 1;
  char buf[sizeof(struct sockaddr_storage)];

  memset(buf, 0, sizeof(buf));
  if (FLAG(b)) {
    get_sockaddr(TT.host_addr, buf);
    af = ((struct sockaddr *)buf)->sa_family;
  } else {
    ((struct sockaddr_in*)buf)->sin_port = htons(TT.port);
    ((struct sockaddr_in*)buf)->sin_family = af;
  }
  s = xsocket(af, SOCK_STREAM, 0);
  xsetsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  xbind(s, (struct sockaddr *)buf, ((af == AF_INET)?
          (sizeof(struct sockaddr_in)):(sizeof(struct sockaddr_in6))));

  if (listen(s, 1) < 0) perror_exit("listen");
  return s;
}

static void write_issue(char *tty)
{
  int size;
  char ch = 0;
  struct utsname u;
  int fd = open(TT.issue_path, O_RDONLY);

  if (fd < 0) return ;
  uname(&u);
  while ((size = readall(fd, &ch, 1)) > 0) {
    if (ch == '\\' || ch == '%') {
      if (readall(fd, &ch, 1) <= 0) perror_exit("readall!");
      if (ch == 's') fputs(u.sysname, stdout);
      if (ch == 'n'|| ch == 'h') fputs(u.nodename, stdout);
      if (ch == 'r') fputs(u.release, stdout);
      if (ch == 'm') fputs(u.machine, stdout);
      if (ch == 'l') fputs(tty, stdout);
    }
    else if (ch == '\n') {
      fputs("\n\r\0", stdout);
    } else fputc(ch, stdout);
  }
  fflush(NULL);
  close(fd);
}

static int new_session(int sockfd)
{
  char *argv_login[] = {NULL, "-h", NULL, NULL};
  char tty_name[30]; //tty name length.
  int fd, i = 1;
  char intial_iacs[] = {IAC, DO, TELOPT_ECHO, IAC, DO, TELOPT_NAWS,
    IAC, WILL, TELOPT_ECHO, IAC, WILL, TELOPT_SGA };
  struct sockaddr_storage sa;
  socklen_t sl = sizeof(sa);

  setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &i, sizeof(i));

  writeall(FLAG(i)?1:sockfd, intial_iacs, sizeof(intial_iacs));
  if ((TT.fork_pid = forkpty(&fd, tty_name, NULL, NULL)) > 0) return fd;
  if (TT.fork_pid < 0) perror_exit("fork");

  if (getpeername(sockfd, (void *)&sa, &sl)) perror_exit("getpeername");
  if (getnameinfo((void *)&sa, sl, toybuf, sizeof(toybuf), NULL, 0, NI_NUMERICHOST))
    perror_exit("getnameinfo");

  write_issue(tty_name);
  argv_login[0] = TT.login_path;
  argv_login[2] = toybuf;
  execvp(argv_login[0], argv_login);
  exit(EXIT_FAILURE);
}

static int handle_iacs(struct term_session *tm, int c, int fd)
{
  char *curr ,*start,*end;
  int i = 0;

  curr = start = tm->buff2+tm->buff2_avail;
  end = tm->buff2 + c -1;
  tm->rem = 0;
  while (curr <= end) {
    if (*curr != IAC){

      if (*curr != '\r') {
        toybuf[i++] = *curr++;
        continue;
      } else {
        toybuf[i++] = *curr++;
        curr++;
        if (curr < end && (*curr == '\n' || *curr == '\0'))
          curr++;
        continue;
      }
    }

    if ((curr + 1) > end) {
      tm->rem = 1;
      break;
    }
    if (*(curr+1) == IAC) { //IAC as data --> IAC IAC
      toybuf[i++] = *(curr+1);
      curr += 2; //IAC IAC --> 2 bytes
      continue;
    }
    if (*(curr + 1) == NOP || *(curr + 1) == SE) {
      curr += 2;
      continue;
    }

    if (*(curr + 1) == SB ) {
      if (*(curr+2) == TELOPT_NAWS) {
        struct winsize ws;
        if ((curr+8) >= end) {  //ensure we have data to process.
          tm->rem = end - curr;
          break;
        }
        ws.ws_col = (curr[3] << 8) | curr[4];
        ws.ws_row = (curr[5] << 8) | curr[6];
        ioctl(fd, TIOCSWINSZ, (char *)&ws);
        curr += 9;
        continue;
      } else { //eat non-supported sub neg. options.
        curr++, tm->rem++;
        while (*curr != IAC && curr <= end) {
          curr++;
          tm->rem++;
        }
        if (*curr == IAC) {
          tm->rem = 0;
          continue;
        } else break;
      }
    }
    curr += 3; //skip non-supported 3 bytes.
  }
  memcpy(start, toybuf, i);
  memcpy(start + i, end - tm->rem, tm->rem); //put remaining if we break;
  return i;
}

static int dup_iacs(char *start, int fd, int len)
{
  char arr[] = {IAC, IAC};
  char *needle = NULL;
  int ret = 0, c, count = 0;

  while (len) {
    if (*start == IAC) {
      count = writeall(fd, arr, sizeof(arr));
      if (count != 2) break; //short write
      start++;
      ret++;
      len--;
      continue;
    }
    needle = memchr(start, IAC, len);
    if (needle) c = needle - start;
    else c = len;
    count = writeall(fd, start, c);
    if (count < 0) break;
    len -= count;
    ret += count;
    start += count;
  }
  return ret;
}

void telnetd_main(void)
{
  fd_set rd, wr;
  struct term_session *tm = NULL;
  struct timeval tv, *tv_ptr = NULL;
  int pty_fd, new_fd, c = 0, w, master_fd = 0;

  if (!FLAG(l)) TT.login_path = "/bin/login";
  if (!FLAG(f)) TT.issue_path = "/etc/issue.net";
  if (FLAG(w)) toys.optflags |= FLAG_F;
  if (!FLAG(i)) {
    master_fd = listen_socket();
    fcntl(master_fd, F_SETFD, FD_CLOEXEC);
    if (master_fd > TT.gmax_fd) TT.gmax_fd = master_fd;
    if (!FLAG(F)) daemon(0, 0);
  } else {
    pty_fd = new_session(master_fd); //master_fd = 0
    if (pty_fd > TT.gmax_fd) TT.gmax_fd = pty_fd;
    tm = xzalloc(sizeof(struct term_session));
    tm->child_pid = TT.fork_pid;
    tm->new_fd = 0;
    tm->pty_fd = pty_fd;
    if (session_list) {
      tm->next = session_list;
      session_list = tm;
    } else session_list = tm;
  }

  if (FLAG(w) && !session_list) {
    tv.tv_sec = TT.w_sec;
    tv.tv_usec = 0;
    tv_ptr = &tv;
  }
  signal(SIGCHLD, generic_signal);

  for (;;) {
    FD_ZERO(&rd);
    FD_ZERO(&wr);
    if (!FLAG(i)) FD_SET(master_fd, &rd);

    tm = session_list;
    while (tm) {

      if (tm->pty_fd > 0 && tm->buff1_avail < BUFSIZE) FD_SET(tm->pty_fd, &rd);
      if (tm->new_fd >= 0 && tm->buff2_avail < BUFSIZE) FD_SET(tm->new_fd, &rd);
      if (tm->pty_fd > 0 && (tm->buff2_avail - tm->buff2_written) > 0)
        FD_SET(tm->pty_fd, &wr);
      if (tm->new_fd >= 0 && (tm->buff1_avail - tm->buff1_written) > 0)
        FD_SET(tm->new_fd, &wr);
      tm = tm->next;
    }


    int r = select(TT.gmax_fd + 1, &rd, &wr, NULL, tv_ptr);
    if (!r) error_exit("select timed out");
    if (r < -1) continue;

    if (!FLAG(i) && FD_ISSET(master_fd, &rd)) { //accept new connection
      new_fd = accept(master_fd, NULL, NULL);
      if (new_fd < 0) continue;
      tv_ptr = NULL;
      fcntl(new_fd, F_SETFD, FD_CLOEXEC);
      if (new_fd > TT.gmax_fd) TT.gmax_fd = new_fd;
      pty_fd = new_session(new_fd);
      if (pty_fd > TT.gmax_fd) TT.gmax_fd = pty_fd;

      tm = xzalloc(sizeof(struct term_session));
      tm->child_pid = TT.fork_pid;
      tm->new_fd = new_fd;
      tm->pty_fd = pty_fd;
      if (session_list) {
        tm->next = session_list;
        session_list = tm;
      } else session_list = tm;
    }

    tm = session_list;
    for (;tm;tm=tm->next) {
      if (FD_ISSET(tm->pty_fd, &rd)) {
        if ((c = read(tm->pty_fd, tm->buff1 + tm->buff1_avail,
                BUFSIZE-tm->buff1_avail)) <= 0) break;
        tm->buff1_avail += c;
        if ((w = dup_iacs(tm->buff1 + tm->buff1_written, tm->new_fd + FLAG(i),
                tm->buff1_avail - tm->buff1_written)) < 0) break;
        tm->buff1_written += w;
      }
      if (FD_ISSET(tm->new_fd, &rd)) {
        if ((c = read(tm->new_fd, tm->buff2+tm->buff2_avail,
                BUFSIZE-tm->buff2_avail)) <= 0) {
          // The other side went away without a proper shutdown. Happens if
          // you exit telnet via ^]^D, leaving the socket in TIME_WAIT.
          xclose(tm->new_fd);
          tm->new_fd = -1;
          xclose(tm->pty_fd);
          tm->pty_fd = -1;
          break;
        }
        c = handle_iacs(tm, c, tm->pty_fd);
        tm->buff2_avail += c;
        if ((w = write(tm->pty_fd, tm->buff2+ tm->buff2_written,
                tm->buff2_avail - tm->buff2_written)) < 0) break;
        tm->buff2_written += w;
      }
      if (FD_ISSET(tm->pty_fd, &wr)) {
        if ((w = write(tm->pty_fd,  tm->buff2 + tm->buff2_written,
                tm->buff2_avail - tm->buff2_written)) < 0) break;
        tm->buff2_written += w;
      }
      if (FD_ISSET(tm->new_fd, &wr)) {
        if ((w = dup_iacs(tm->buff1 + tm->buff1_written, tm->new_fd + FLAG(i),
                tm->buff1_avail - tm->buff1_written)) < 0) break;
        tm->buff1_written += w;
      }
      if (tm->buff1_written == tm->buff1_avail)
        tm->buff1_written = tm->buff1_avail = 0;
      if (tm->buff2_written == tm->buff2_avail)
        tm->buff2_written = tm->buff2_avail = 0;
      fflush(NULL);
    }

    // Loop to handle (unknown number of) SIGCHLD notifications
    while (toys.signal) {
      int status;
      struct term_session *prev = NULL;
      pid_t pid;

      // funny little dance to avoid race conditions.
      toys.signal = 0;
      pid = waitpid(-1, &status, WNOHANG);
      if (pid <= 0) break;
      toys.signal++;

      for (tm = session_list; tm; tm = tm->next) {
        if (tm->child_pid == pid) break;
        prev = tm;
      }
      if (!tm) error_exit("unexpected reparenting of %d", pid);

      if (FLAG(i)) exit(EXIT_SUCCESS);

      if (!prev) session_list = session_list->next;
      else prev->next = tm->next;
      xclose(tm->pty_fd);
      xclose(tm->new_fd);
      free(tm);
    }
  }
}
