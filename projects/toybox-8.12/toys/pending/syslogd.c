/* syslogd.c - a system logging utility.
 *
 * Copyright 2013 Madhur Verma <mad.flexi@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard

USE_SYSLOGD(NEWTOY(syslogd,">0l#<1>8=8R:b#<0>99=1s#<0=200m#<0>71582787=20O:p:f:a:nSKLD", TOYFLAG_SBIN|TOYFLAG_STAYROOT))

config SYSLOGD
  bool "syslogd"
  default n
  help
    usage: syslogd  [-a socket] [-O logfile] [-f config file] [-m interval]
                    [-p socket] [-s SIZE] [-b N] [-R HOST] [-l N] [-nSLKD]

    System logging utility

    -a      Extra unix socket for listen
    -O FILE Default log file <DEFAULT: /var/log/messages>
    -f FILE Config file <DEFAULT: /etc/syslog.conf>
    -p      Alternative unix domain socket <DEFAULT : /dev/log>
    -n      Avoid auto-backgrounding
    -S      Smaller output
    -m MARK interval <DEFAULT: 20 minutes> (RANGE: 0 to 71582787)
    -R HOST Log to IP or hostname on PORT (default PORT=514/UDP)"
    -L      Log locally and via network (default is network only if -R)"
    -s SIZE Max size (KB) before rotation (default:200KB, 0=off)
    -b N    rotated logs to keep (default:1, max=99, 0=purge)
    -K      Log to kernel printk buffer (use dmesg to read it)
    -l N    Log only messages more urgent than prio(default:8 max:8 min:1)
    -D      Drop duplicates
*/

#define FOR_syslogd
#include "toys.h"

// UNIX Sockets for listening
struct unsocks {
  struct unsocks *next;
  char *path;
  struct sockaddr_un sdu;
  int sd;
};

// Log file entry to log into.
struct logfile {
  struct logfile *next;
  char *filename;
  uint32_t facility[8];
  uint8_t level[LOG_NFACILITIES];
  int logfd;
  struct sockaddr_in saddr;
};

GLOBALS(
  char *socket;
  char *config_file;
  char *unix_socket;
  char *logfile;
  long interval;
  long rot_size;
  long rot_count;
  char *remote_log;
  long log_prio;

  struct unsocks *lsocks;  // list of listen sockets
  struct logfile *lfiles;  // list of write logfiles
  int sigfd[2];
)

// Lookup numerical code from name
// Also used in logger
int logger_lookup(int where, char *key)
{
  CODE *w = ((CODE *[]){facilitynames, prioritynames})[where];

  for (; w->c_name; w++)
    if (!strcasecmp(key, w->c_name)) return w->c_val;

  return -1;
}

//search the given name and return its value
static char *dec(int val, CODE *clist, char *buf)
{
  for (; clist->c_name; clist++)
    if (val == clist->c_val) return clist->c_name;
  sprintf(buf, "%u", val);

  return buf;
}

/*
 * recurses the logfile list and resolves config
 * for evry file and updates facilty and log level bits.
 */
static int resolve_config(struct logfile *file, char *config)
{
  char *tk;

  for (tk = strtok(config, "; \0"); tk; tk = strtok(NULL, "; \0")) {
    char *fac = tk, *lvl;
    int i = 0;
    unsigned facval = 0;
    uint8_t set, levval, bits = 0;

    tk = strchr(fac, '.');
    if (!tk) return -1;
    *tk = '\0';
    lvl = tk + 1;

    for (;;) {
      char *nfac = strchr(fac, ',');

      if (nfac) *nfac = '\0';
      if (*fac == '*') {
        facval = 0xFFFFFFFF;
        if (fac[1]) return -1;
      } else {
        if ((i = logger_lookup(0, fac)) == -1) return -1;
        facval |= (1 << LOG_FAC(i));
      }
      if (nfac) fac = nfac + 1;
      else break;
    }

    levval = 0;
    for (tk = "!=*"; *tk; tk++, bits <<= 1) {
      if (*lvl == *tk) {
        bits++;
        lvl++;
      }
    }
    if (bits & 2) levval = 0xff;
    if (*lvl) {
      if ((i = logger_lookup(1, lvl)) == -1) return -1;
      levval |= (bits & 4) ? LOG_MASK(i) : LOG_UPTO(i);
      if (bits & 8) levval = ~levval;
    }

    for (i = 0, set = levval; set; set >>= 1, i++)
      if (set & 0x1) file->facility[i] |= ~facval;
    for (i = 0; i < LOG_NFACILITIES; facval >>= 1, i++)
      if (facval & 0x1) file->level[i] |= ~levval;
  }

  return 0;
}

// Parse config file and update the log file list.
static int parse_config_file(void)
{
  struct logfile *file;
  FILE *fp;
  char *confline, *tk[2];
  int len, lineno = 0;
  size_t linelen;
  /*
   * if -K then open only /dev/kmsg
   * all other log files are neglected
   * thus no need to open config either.
   */
  if (toys.optflags & FLAG_K) {
    file = xzalloc(sizeof(struct logfile));
    file->filename = xstrdup("/dev/kmsg");
    TT.lfiles = file;
    return 0;
  }
  /*
   * if -R then add remote host to log list
   * if -L is not provided all other log
   * files are neglected thus no need to
   * open config either so just return.
   */
  if (toys.optflags & FLAG_R) {
    file = xzalloc(sizeof(struct logfile));
    file->filename = xmprintf("@%s",TT.remote_log);
    TT.lfiles = file;
    if (!(toys.optflags & FLAG_L)) return 0;
  }
  /*
   * Read config file and add logfiles to the list
   * with their configuration.
   */
  if (!(fp = fopen(TT.config_file, "r")) && (toys.optflags & FLAG_f))
    perror_exit("can't open '%s'", TT.config_file);

  for (linelen = 0; fp;) {
    confline = NULL;
    len = getline(&confline, &linelen, fp);
    if (len <= 0) break;
    lineno++;
    for (; *confline == ' '; confline++, len--) ;
    if ((confline[0] == '#') || (confline[0] == '\n')) continue;
    tk[0] = confline;
    for (; len && !(*tk[0]==' ' || *tk[0]=='\t'); tk[0]++, len--);
    for (tk[1] = tk[0]; len && (*tk[1]==' ' || *tk[1]=='\t'); tk[1]++, len--);
    if (!len || (len == 1 && *tk[1] == '\n')) {
      error_msg("error in '%s' at line %d", TT.config_file, lineno);
      return -1;
    }
    else if (*(tk[1] + len - 1) == '\n') *(tk[1] + len - 1) = '\0';
    *tk[0] = '\0';
    if (*tk[1] != '*') {
      file = TT.lfiles;
      while (file && strcmp(file->filename, tk[1])) file = file->next;
      if (!file) {
        file = xzalloc(sizeof(struct logfile));
        file->filename = xstrdup(tk[1]);
        file->next = TT.lfiles;
        TT.lfiles = file;
      }
      if (resolve_config(file, confline) == -1) {
        error_msg("error in '%s' at line %d", TT.config_file, lineno);
        return -1;
      }
    }
    free(confline);
  }
  /*
   * Can't open config file or support is not enabled
   * adding default logfile to the head of list.
   */
  if (!fp){
    file = xzalloc(sizeof(struct logfile));
    file->filename = xstrdup((toys.optflags & FLAG_O) ?
                     TT.logfile : "/var/log/messages"); //DEFLOGFILE
    file->next = TT.lfiles;
    TT.lfiles = file;
  } else fclose(fp);
  return 0;
}

// open every log file in list.
static void open_logfiles(void)
{
  struct logfile *tfd;

  for (tfd = TT.lfiles; tfd; tfd = tfd->next) {
    char *p, *tmpfile;
    long port = 514;

    if (*tfd->filename == '@') { // network
      struct addrinfo *info, rp;

      tmpfile = xstrdup(tfd->filename + 1);
      if ((p = strchr(tmpfile, ':'))) {
        char *endptr;

        *p = '\0';
        port = strtol(++p, &endptr, 10);
        if (*endptr || endptr == p || port < 0 || port > 65535)
          error_exit("bad port in %s", tfd->filename);
      }
      memset(&rp, 0, sizeof(rp));
      rp.ai_family = AF_INET;
      rp.ai_socktype = SOCK_DGRAM;
      rp.ai_protocol = IPPROTO_UDP;

      if (getaddrinfo(tmpfile, NULL, &rp, &info) || !info)
        perror_exit("BAD ADDRESS: can't find : %s ", tmpfile);
      ((struct sockaddr_in*)info->ai_addr)->sin_port = htons(port);
      memcpy(&tfd->saddr, info->ai_addr, info->ai_addrlen);
      freeaddrinfo(info);

      tfd->logfd = xsocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      free(tmpfile);
    } else tfd->logfd = open(tfd->filename, O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (tfd->logfd < 0) {
      tfd->filename = "/dev/console";
      tfd->logfd = open(tfd->filename, O_APPEND);
    }
  }
}

//write to file with rotation
static int write_rotate(struct logfile *tf, int len)
{
  int size, isreg;
  struct stat statf;
  isreg = (!fstat(tf->logfd, &statf) && S_ISREG(statf.st_mode));
  size = statf.st_size;

  if ((toys.optflags & FLAG_s) || (toys.optflags & FLAG_b)) {
    if (TT.rot_size && isreg && (size + len) > (TT.rot_size*1024)) {
      if (TT.rot_count) { /* always 0..99 */
        int i = strlen(tf->filename) + 3 + 1;
        char old_file[i];
        char new_file[i];
        i = TT.rot_count - 1;
        while (1) {
          sprintf(new_file, "%s.%d", tf->filename, i);
          if (!i) break;
          sprintf(old_file, "%s.%d", tf->filename, --i);
          rename(old_file, new_file);
        }
        rename(tf->filename, new_file);
        unlink(tf->filename);
        close(tf->logfd);
        tf->logfd = open(tf->filename, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (tf->logfd < 0) {
          perror_msg("can't open %s", tf->filename);
          return -1;
        }
      }
      ftruncate(tf->logfd, 0);
    }
  }
  return write(tf->logfd, toybuf, len);
}

//Parse message and write to file.
static void logmsg(char *msg, int len)
{
  time_t now;
  char *p, *ts, *lvlstr, *facstr;
  struct utsname uts;
  int pri = 0;
  struct logfile *tf = TT.lfiles;

  char *omsg = msg;
  int olen = len, fac, lvl;

  if (*msg == '<') { // Extract the priority no.
    pri = (int) strtoul(msg + 1, &p, 10);
    if (*p == '>') msg = p + 1;
  }
  /* Jan 18 00:11:22 msg...
   * 01234567890123456
   */
  if (len < 16 || msg[3] != ' ' || msg[6] != ' ' || msg[9] != ':'
      || msg[12] != ':' || msg[15] != ' ') {
    time(&now);
    ts = ctime(&now) + 4; /* skip day of week */
  } else {
    now = 0;
    ts = msg;
    msg += 16;
  }
  ts[15] = '\0';
  fac = LOG_FAC(pri);
  lvl = LOG_PRI(pri);

  if (toys.optflags & FLAG_K) len = sprintf(toybuf, "<%d> %s", pri, msg);
  else {
    char facbuf[12], pribuf[12];

    facstr = dec(pri & LOG_FACMASK, facilitynames, facbuf);
    lvlstr = dec(LOG_PRI(pri), prioritynames, pribuf);

    p = "local";
    if (!uname(&uts)) p = uts.nodename;
    if (toys.optflags & FLAG_S) len = sprintf(toybuf, "%s %s", ts, msg);
    else len = sprintf(toybuf, "%s %s %s.%s %s", ts, p, facstr, lvlstr, msg);
  }
  if (lvl >= TT.log_prio) return;

  for (; tf; tf = tf->next) {
    if (tf->logfd > 0) {
      if (!((tf->facility[lvl] & (1 << fac)) || (tf->level[fac] & (1<<lvl)))) {
        int wlen, isNetwork = *tf->filename == '@';
        if (isNetwork)
          wlen = sendto(tf->logfd, omsg, olen, 0, (struct sockaddr*)&tf->saddr, sizeof(tf->saddr));
        else wlen = write_rotate(tf, len);
        if (wlen < 0) perror_msg("write failed file : %s ", tf->filename + isNetwork);
      }
    }
  }
}

/*
 * closes all read and write fds
 * and frees all nodes and lists
 */
static void cleanup(void)
{
  while (TT.lsocks) {
    struct unsocks *fnode = TT.lsocks;

    if (fnode->sd >= 0) {
      close(fnode->sd);
      unlink(fnode->path);
    }
    TT.lsocks = fnode->next;
    free(fnode);
  }

  while (TT.lfiles) {
    struct logfile *fnode = TT.lfiles;

    free(fnode->filename);
    if (fnode->logfd >= 0) close(fnode->logfd);
    TT.lfiles = fnode->next;
    free(fnode);
  }
}

static void signal_handler(int sig)
{
  unsigned char ch = sig;
  if (write(TT.sigfd[1], &ch, 1) != 1) error_msg("can't send signal");
}

void syslogd_main(void)
{
  struct unsocks *tsd;
  int nfds, retval, last_len=0;
  struct timeval tv;
  fd_set rfds;        // fds for reading
  char *temp, *buffer = (toybuf +2048), *last_buf = (toybuf + 3072); //these two buffs are of 1K each

  if ((toys.optflags & FLAG_p) && (strlen(TT.unix_socket) > 108))
    error_exit("Socket path should not be more than 108");

  TT.config_file = (toys.optflags & FLAG_f) ?
                   TT.config_file : "/etc/syslog.conf"; //DEFCONFFILE
init_jumpin:
  tsd = xzalloc(sizeof(struct unsocks));

  tsd->path = (toys.optflags & FLAG_p) ? TT.unix_socket : "/dev/log"; // DEFLOGSOCK
  TT.lsocks = tsd;

  if (toys.optflags & FLAG_a) {
    for (temp = strtok(TT.socket, ":"); temp; temp = strtok(NULL, ":")) {
      if (strlen(temp) > 107) temp[108] = '\0';
      tsd = xzalloc(sizeof(struct unsocks));
      tsd->path = temp;
      tsd->next = TT.lsocks;
      TT.lsocks = tsd;
    }
  }
  /*
   * initializes unsock_t structure
   * and opens socket for reading
   * and adds to global lsock list.
  */
  nfds = 0;
  for (tsd = TT.lsocks; tsd; tsd = tsd->next) {
    tsd->sdu.sun_family = AF_UNIX;
    strcpy(tsd->sdu.sun_path, tsd->path);
    tsd->sd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (tsd->sd < 0) {
      perror_msg("OPEN SOCKS : failed");
      continue;
    }
    unlink(tsd->sdu.sun_path);
    if (bind(tsd->sd, (struct sockaddr *) &tsd->sdu, sizeof(tsd->sdu))) {
      perror_msg("BIND SOCKS : failed sock : %s", tsd->sdu.sun_path);
      close(tsd->sd);
      continue;
    }
    chmod(tsd->path, 0777);
    nfds++;
  }
  if (!nfds) {
    error_msg("Can't open single socket for listening.");
    goto clean_and_exit;
  }

  // Setup signals
  xpipe(TT.sigfd);

  fcntl(TT.sigfd[1] , F_SETFD, FD_CLOEXEC);
  fcntl(TT.sigfd[0] , F_SETFD, FD_CLOEXEC);
  int flags = fcntl(TT.sigfd[1], F_GETFL);
  fcntl(TT.sigfd[1], F_SETFL, flags | O_NONBLOCK);
  signal(SIGHUP, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGQUIT, signal_handler);

  if (parse_config_file() == -1) goto clean_and_exit;
  open_logfiles();
  if (!(toys.optflags & FLAG_n)) {
    daemon(0, 0);
    //don't daemonize again if SIGHUP received.
    toys.optflags |= FLAG_n;
  }
  xpidfile("syslogd");

  logmsg("<46>Toybox: syslogd started", 27); //27 : the length of message
  for (;;) {
    // Add opened socks to rfds for select()
    FD_ZERO(&rfds);
    for (tsd = TT.lsocks; tsd; tsd = tsd->next) FD_SET(tsd->sd, &rfds);
    FD_SET(TT.sigfd[0], &rfds);
    tv.tv_usec = 0;
    tv.tv_sec = TT.interval*60;

    retval = select(TT.sigfd[0] + 1, &rfds, NULL, NULL, (TT.interval)?&tv:NULL);
    if (retval < 0) {
      if (errno != EINTR) perror_msg("Error in select ");
    }
    else if (!retval) logmsg("<46>-- MARK --", 14);
    else if (FD_ISSET(TT.sigfd[0], &rfds)) { /* May be a signal */
      unsigned char sig;

      if (read(TT.sigfd[0], &sig, 1) != 1) {
        error_msg("signal read failed.\n");
        continue;
      }
      switch(sig) {
        case SIGTERM:    /* FALLTHROUGH */
        case SIGINT:     /* FALLTHROUGH */
        case SIGQUIT:
          logmsg("<46>syslogd exiting", 19);
          if (CFG_TOYBOX_FREE ) cleanup();
          signal(sig, SIG_DFL);
          sigset_t ss;
          sigemptyset(&ss);
          sigaddset(&ss, sig);
          sigprocmask(SIG_UNBLOCK, &ss, NULL);
          raise(sig);
          _exit(1);  /* Should not reach it */
          break;
        case SIGHUP:
          logmsg("<46>syslogd exiting", 19);
          cleanup(); //cleanup is done, as we restart syslog.
          goto init_jumpin;
        default: break;
      }
    } else { /* Some activity on listen sockets. */
      for (tsd = TT.lsocks; tsd; tsd = tsd->next) {
        int sd = tsd->sd;
        if (FD_ISSET(sd, &rfds)) {
          // Buffer is of 1 KiB, hence reading only 1022 bytes, reserving 1
          // for '\n' and 1 for '\0'
          int len = read(sd, buffer, 1022);

          // The syslog function's documentation says that a trailing '\n' is
          // optional. We trim any that are present, and then append one.
          while (len > 0 &&
                 (buffer[len - 1] == '\n' || buffer[len - 1] == '\0')) {
            --len;
          }

          if (len > 0) {
            buffer[len++] = '\n';
            buffer[len] = '\0';
            if((toys.optflags & FLAG_D) && (len == last_len))
              if (!memcmp(last_buf, buffer, len)) break;

            memcpy(last_buf, buffer, len);
            last_len = len;
            logmsg(buffer, len);
          }
          break;
        }
      }
    }
  }
clean_and_exit:
  logmsg("<46>syslogd exiting", 19);
  if (CFG_TOYBOX_FREE ) cleanup();
}
