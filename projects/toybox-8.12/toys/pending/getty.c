/* getty.c - A getty program to get controlling terminal.
 *
 * Copyright 2012 Sandeep Sharma <sandeep.jack2756@gamil.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard.

USE_GETTY(NEWTOY(getty, "<2t#<0H:I:l:f:iwnmLh", TOYFLAG_SBIN))

config GETTY
  bool "getty"
  default n
  help
    usage: getty [OPTIONS] BAUD_RATE[,BAUD_RATE]... TTY [TERMTYPE]

    Wait for a modem to dial into serial port, adjust baud rate, call login.

    -h    Enable hardware RTS/CTS flow control
    -L    Set CLOCAL (ignore Carrier Detect state)
    -m    Get baud rate from modem's CONNECT status message
    -n    Don't prompt for login name
    -w    Wait for CR or LF before sending /etc/issue
    -i    Don't display /etc/issue
    -f ISSUE_FILE  Display ISSUE_FILE instead of /etc/issue
    -l LOGIN  Invoke LOGIN instead of /bin/login
    -t SEC    Terminate after SEC if no login name is read
    -I INITSTR  Send INITSTR before anything else
    -H HOST    Log HOST into the utmp file as the hostname
*/

#define FOR_getty
#include "toys.h"

GLOBALS(
  char *f, *l, *I, *H;
  long t;

  char *tty_name, buff[128];
  int speeds[20], sc;
  struct termios termios;
  struct utsname uts;
)

#define CTL(x)        ((x) ^ 0100)
#define HOSTNAME_SIZE 32

static void parse_speeds(char *sp)
{
  char *ptr;

  TT.sc = 0;
  while ((ptr = strsep(&sp, ","))) {
    TT.speeds[TT.sc] = atolx_range(ptr, 0, INT_MAX);
    if (TT.speeds[TT.sc] < 0) perror_exit("bad speed %s", ptr);
    if (++TT.sc > 10) perror_exit("too many speeds, max is 10");
  }
}

// Get controlling terminal and redirect stdio
static void open_tty(void)
{
  if (strcmp(TT.tty_name, "-")) {
    if (*(TT.tty_name) != '/') TT.tty_name = xmprintf("/dev/%s", TT.tty_name);
    // Sends SIGHUP to all foreground process if Session leader don't die,Ignore
    void* handler = signal(SIGHUP, SIG_IGN);
    ioctl(0, TIOCNOTTY, 0); // Giveup if there is any controlling terminal
    signal(SIGHUP, handler);
    if ((setsid() < 0) && (getpid() != getsid(0))) perror_exit("setsid");
    xclose(0);
    xopen_stdio(TT.tty_name, O_RDWR|O_NDELAY|O_CLOEXEC);
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) & ~O_NONBLOCK); // Block read
    dup2(0, 1);
    dup2(0, 2);
    if (ioctl(0, TIOCSCTTY, 1) < 0) perror_msg("ioctl(TIOCSCTTY)");
    if (!isatty(0)) perror_exit("/dev/%s: not a tty", TT.tty_name);
    chown(TT.tty_name, 0, 0); // change ownership, Hope login will change this
    chmod(TT.tty_name, 0620);
  } else { // We already have opened TTY
    if (setsid() < 0) perror_msg("setsid failed");
    if ((fcntl(0, F_GETFL) & (O_RDWR|O_RDONLY|O_WRONLY)) != O_RDWR)
      perror_exit("no read/write permission");
  }
}

static void termios_init(void)
{
  if (tcgetattr(0, &TT.termios) < 0) perror_exit("tcgetattr");
  // Flush input and output queues, important for modems!
  tcflush(0, TCIOFLUSH);
  TT.termios.c_cflag &= (0|CSTOPB|PARENB|PARODD);
#ifdef CRTSCTS
  if (FLAG(h)) TT.termios.c_cflag |= CRTSCTS;
#endif
  if (FLAG(L)) TT.termios.c_cflag |= CLOCAL;
  TT.termios.c_cc[VTIME] = 0;
  TT.termios.c_cc[VMIN] = 1;
  TT.termios.c_oflag = OPOST|ONLCR;
  TT.termios.c_cflag |= CS8|CREAD|HUPCL|CBAUDEX;
  // login will disable echo for passwd.
  TT.termios.c_lflag |= ISIG|ICANON|ECHO|ECHOE|ECHOK|ECHOKE;
  TT.termios.c_cc[VINTR] = CTL('C');
  TT.termios.c_cc[VQUIT] = CTL('\\');
  TT.termios.c_cc[VEOF] = CTL('D');
  TT.termios.c_cc[VEOL] = '\n';
  TT.termios.c_cc[VKILL] = CTL('U');
  TT.termios.c_cc[VERASE] = 127; // CERASE
  TT.termios.c_iflag = ICRNL|IXON|IXOFF;
  // Set non-zero baud rate. Zero baud rate left it unchanged.
  if (TT.speeds[0] != 0) xsetspeed(&TT.termios, TT.speeds[0]);
  if (tcsetattr(0, TCSANOW, &TT.termios) < 0) perror_exit("tcsetattr");
}

// Get the baud rate from modems CONNECT mesage, Its of form <junk><BAUD><Junk>
static void sense_baud(void)
{
  int vmin, speed;
  ssize_t size;
  char *ptr;

  vmin = TT.termios.c_cc[VMIN]; // Store old
  TT.termios.c_cc[VMIN] = 0; // No block even queue is empty.
  if (tcsetattr(0, TCSANOW, &TT.termios) < 0) perror_exit("tcsetattr");
  size = readall(0, TT.buff, sizeof(TT.buff)-1);
  if (size > 0) {
    for (ptr = TT.buff; ptr < TT.buff+size; ptr++) {
      if (isdigit(*ptr)) {
        speed = atolx_range(ptr, 0, INT_MAX);
        if (speed > 0) xsetspeed(&TT.termios, speed);
        break;
      }
    }
  }
  TT.termios.c_cc[VMIN] = vmin; //restore old value
  if (tcsetattr(0, TCSANOW, &TT.termios) < 0) perror_exit("tcsetattr");
}

// Print /etc/issue, interpreting escape sequences.
void print_issue(void)
{
  FILE *fp = fopen(TT.f, "r");
  int ch;

  if (!fp) return;
  while ((ch = fgetc(fp)) != -1) {
    if (ch == '\\' || ch == '%') {
      ch = fgetc(fp);
      if (ch == 'h' || ch == 'n') xputsn(TT.uts.nodename);
      else if (ch == 'm') xputsn(TT.uts.machine);
      else if (ch == 'r') xputsn(TT.uts.release);
      else if (ch == 's') xputsn(TT.uts.sysname);
      else if (ch == 'l') xputsn(TT.tty_name);
      else printf("<bad escape>");
    } else xputc(ch);
  }
}

// Read login name and print prompt and Issue file.
static int read_login_name(void)
{
  tcflush(0, TCIFLUSH); // Flush pending speed switches
  while (1) {
    int i = 0;

    if (!FLAG(i)) print_issue();

    printf("%s login: ", TT.uts.nodename);
    fflush(stdout);

    TT.buff[0] = getchar();
    if (!TT.buff[0] && TT.sc > 1) return 0; // Switch speed
    if (TT.buff[0] == '\n') continue;
    if (TT.buff[0] != '\n')
      if (!fgets(&TT.buff[1], HOSTNAME_SIZE-1, stdin)) _exit(1);
    while (i < HOSTNAME_SIZE-1 && isgraph(TT.buff[i])) i++;
    TT.buff[i] = 0;
    break;
  }
  return 1;
}

static void utmp_entry(void)
{
  struct utmpx entry = {.ut_pid = getpid()}, *ep;
  int fd;

  // We're responsible for ensuring that the utmp file exists.
  if (access(_PATH_UTMP, F_OK) && (fd = open(_PATH_UTMP, O_CREAT, 0664)) != -1)
    close(fd);

  // Find any existing entry.
  setutxent();
  while ((ep = getutxent()))
    if (ep->ut_pid == entry.ut_pid && ep->ut_type >= INIT_PROCESS) break;
  if (ep) entry = *ep;
  else entry.ut_type = LOGIN_PROCESS;

  // Modify.
  entry.ut_tv.tv_sec = time(0);
  xstrncpy(entry.ut_user, "LOGIN", sizeof(entry.ut_user));
  xstrncpy(entry.ut_line, ttyname(0) + strlen("/dev/"), sizeof(entry.ut_line));
  if (FLAG(H)) xstrncpy(entry.ut_host, TT.H, sizeof(entry.ut_host));

  // Write.
  pututxline(&entry);
  endutxent();
}

void getty_main(void)
{
  char ch, *cmd[3] = {TT.l ? : "/bin/login", 0, 0}; // space to add username

  if (!FLAG(f)) TT.f = "/etc/issue";
  uname(&TT.uts);

  // parse arguments and set $TERM
  if (isdigit(**toys.optargs)) {
    parse_speeds(*toys.optargs);
    if (*++toys.optargs) TT.tty_name = xmprintf("%s", *toys.optargs);
  } else {
    TT.tty_name = xmprintf("%s", *toys.optargs);
    if (*++toys.optargs) parse_speeds(*toys.optargs);
  }
  if (*++toys.optargs) setenv("TERM", *toys.optargs, 1);

  open_tty();
  termios_init();
  tcsetpgrp(0, getpid());
  utmp_entry();
  if (FLAG(I)) xputsn(TT.I);
  if (FLAG(m)) sense_baud();
  if (FLAG(t)) alarm(TT.t);
  if (FLAG(w)) while (readall(0, &ch, 1) != 1)  if (ch=='\n' || ch=='\r') break;
  if (!FLAG(n)) {
    int index = 1; // 0th we already set.

    for (;;) {
      if (read_login_name()) break;
      index %= TT.sc;
      xsetspeed(&TT.termios, TT.speeds[index]);
      //Necessary after cfsetspeed
      if (tcsetattr(0, TCSANOW, &TT.termios) < 0) perror_exit("tcsetattr");
    }
    cmd[1] = TT.buff; //put the username in the login command line
  }
  xexec(cmd);
}
