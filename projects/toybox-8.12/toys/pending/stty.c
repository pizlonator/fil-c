/* stty.c - Get/set terminal configuration.
 *
 * Copyright 2017 The Android Open Source Project.
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/stty.html

USE_STTY(NEWTOY(stty, "?aF:g[!ag]", TOYFLAG_BIN))

config STTY
  bool "stty"
  default n
  help
    usage: stty [-ag] [-F device] SETTING...

    Get/set terminal configuration.

    -F	Open device instead of stdin
    -a	Show all current settings (default differences from "sane")
    -g	Show all current settings usable as input to stty

    Special characters (syntax ^c or undef): intr quit erase kill eof eol eol2
    swtch start stop susp rprnt werase lnext discard

    Control/input/output/local settings as shown by -a, '-' prefix to disable

    Combo settings: cooked/raw, evenp/oddp/parity, nl, ek, sane

    N	set input and output speed (ispeed N or ospeed N for just one)
    cols N	set number of columns
    rows N	set number of rows
    line N	set line discipline
    min N	set minimum chars per read
    time N	set read timeout
    speed	show speed only
    size	show size only
*/

#define FOR_stty
#include "toys.h"

#include <linux/tty.h>

GLOBALS(
  char *F;

  int fd, col;
  unsigned output_cols;
)

static const int bauds[] = {
  0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600,
  19200, 38400, 57600, 115200, 230400, 460800, 500000, 576000, 921600,
  1000000, 1152000, 1500000, 2000000, 2500000, 3000000, 3500000, 4000000
};

static int baud(speed_t speed)
{
  if (speed&CBAUDEX) speed=(speed&~CBAUDEX)+15;
  return bauds[speed];
}

static speed_t speed(int baud)
{
  int i;

  for (i=0;i<ARRAY_LEN(bauds);i++) if (bauds[i] == baud) break;
  if (i == ARRAY_LEN(bauds)) error_exit("unknown speed: %d", baud);
  return i+4081*(i>=16);
}

struct flag {
  char *name;
  int value;
  int mask;
};

static const struct flag chars[] = {
  { "intr", VINTR }, { "quit", VQUIT }, { "erase", VERASE }, { "kill", VKILL },
  { "eof", VEOF }, { "eol", VEOL }, { "eol2", VEOL2 }, { "swtch", VSWTC },
  { "start", VSTART }, { "stop", VSTOP }, { "susp", VSUSP },
  { "rprnt", VREPRINT }, { "werase", VWERASE }, { "lnext", VLNEXT },
  { "discard", VDISCARD }, { "min", VMIN }, { "time", VTIME },
};

static const struct flag cflags[] = {
  { "parenb", PARENB }, { "parodd", PARODD }, { "cmspar", CMSPAR },
  { "cs5", CS5, CSIZE }, { "cs6", CS6, CSIZE }, { "cs7", CS7, CSIZE },
  { "cs8", CS8, CSIZE }, { "hupcl", HUPCL }, { "cstopb", CSTOPB },
  { "cread", CREAD }, { "clocal", CLOCAL }, { "crtscts", CRTSCTS },
};

static const struct flag iflags[] = {
  { "ignbrk", IGNBRK }, { "brkint", BRKINT }, { "ignpar", IGNPAR },
  { "parmrk", PARMRK }, { "inpck", INPCK }, { "istrip", ISTRIP },
  { "inlcr", INLCR }, { "igncr", IGNCR }, { "icrnl", ICRNL }, { "ixon", IXON },
  { "ixoff", IXOFF }, { "iuclc", IUCLC }, { "ixany", IXANY },
  { "imaxbel", IMAXBEL }, { "iutf8", IUTF8 },
};

static const struct flag oflags[] = {
  { "opost", OPOST }, { "olcuc", OLCUC }, { "ocrnl", OCRNL },
  { "onlcr", ONLCR }, { "onocr", ONOCR }, { "onlret", ONLRET },
  { "ofill", OFILL }, { "ofdel", OFDEL }, { "nl0", NL0, NLDLY },
  { "nl1", NL1, NLDLY }, { "cr0", CR0, CRDLY }, { "cr1", CR1, CRDLY },
  { "cr2", CR2, CRDLY }, { "cr3", CR3, CRDLY }, { "tab0", TAB0, TABDLY },
  { "tab1", TAB1, TABDLY }, { "tab2", TAB2, TABDLY }, { "tab3", TAB3, TABDLY },
  { "bs0", BS0, BSDLY }, { "bs1", BS1, BSDLY }, { "vt0", VT0, VTDLY },
  { "vt1", VT1, VTDLY }, { "ff0", FF0, FFDLY }, { "ff1", FF1, FFDLY },
};

static const struct flag lflags[] = {
  { "isig", ISIG }, { "icanon", ICANON }, { "iexten", IEXTEN },
  { "echo", ECHO }, { "echoe", ECHOE }, { "echok", ECHOK },
  { "echonl", ECHONL }, { "noflsh", NOFLSH }, { "xcase", XCASE },
  { "tostop", TOSTOP }, { "echoprt", ECHOPRT }, { "echoctl", ECHOCTL },
  { "echoke", ECHOKE }, { "flusho", FLUSHO }, { "extproc", EXTPROC },
};

static const struct synonym {
  char *from;
  char *to;
} synonyms[] = {
  { "cbreak", "-icanon" }, { "-cbreak", "icanon" },
  { "-cooked", "raw" }, { "-raw", "cooked" },
  { "crterase", "echoe" }, { "-crterase", "-echoe" },
  { "crtkill", "echoke" }, { "-crtkill", "-echoke" },
  { "ctlecho", "echoctl" }, { "-ctlecho", "-echoctl" },
  { "-tandem", "-ixoff" }, { "tandem", "ixoff" },
  { "hup", "hupcl" }, { "-hup", "-hupcl" },
  { "prterase", "echoprt" }, { "-prterase", "-echoprt" },
  { "tabs", "tab0" }, { "-tabs", "tab3" },
};

static void out(const char *fmt, ...)
{
  va_list va;
  int len;
  char *prefix = " ";

  va_start(va, fmt);
  len = vsnprintf(toybuf, sizeof(toybuf), fmt, va);
  va_end(va);

  if (TT.output_cols == 0) {
    TT.output_cols = 80;
    terminal_size(&TT.output_cols, NULL);
  }

  if (TT.col == 0 || *fmt == '\n') prefix = "";
  else if (TT.col + 1 + len >= TT.output_cols) {
    prefix = "\n";
    TT.col = 0;
  }
  xprintf("%s%s", prefix, toybuf);

  if (toybuf[len-1] == '\n') TT.col = 0;
  else TT.col += strlen(prefix) + len;
}

static void show_flags(tcflag_t actual, tcflag_t sane,
                       const struct flag *flags, int len)
{
  int i, j, value, mask;

  // Implement -a by ensuring that sane != actual so we'll show everything.
  if (FLAG(a)) sane = ~actual;

  for (i = j = 0; i<len; i++) {
    value = flags[i].value;
    if ((mask = flags[i].mask)) {
      if ((actual&mask)==value && (sane&mask)!=value) {
        out("%s", flags[i].name);
        j++;
      }
    } else {
      if ((actual&value) != (sane&value)) {
        out("%s%s", actual&value?"":"-", flags[i].name);
        j++;
      }
    }
  }
  if (j) out("\n");
}

static void show_size(int verbose)
{
  struct winsize ws;

  if (ioctl(TT.fd, TIOCGWINSZ, &ws)) perror_exit("TIOCGWINSZ %s", TT.F);
  out(verbose ? "rows %d; columns %d;" : "%d %d\n", ws.ws_row, ws.ws_col);
}

static void show_speed(struct termios *t, int verbose)
{
  int ispeed = baud(cfgetispeed(t)), ospeed = baud(cfgetospeed(t));
  char *fmt = verbose ? "ispeed %d baud; ospeed %d baud;" : "%d %d\n";

  if (ispeed == ospeed) fmt += (verbose ? 17 : 3);
  out(fmt, ispeed, ospeed);
}

static int get_arg(int *i, long long high)
{
  (*i)++;
  if (!toys.optargs[*i]) error_exit("missing arg");
  return atolx_range(toys.optargs[*i], 0, high);
}

static int set_flag(tcflag_t *f, const struct flag *flags, int len,
                    char *name, int on)
{
  int i;

  for (i=0;i<len;i++) {
    if (!strcmp(flags[i].name, name)) {
      if (on) {
        *f &= ~flags[i].mask;
        *f |= flags[i].value;
      } else {
        if (flags[i].mask) error_exit("%s isn't a boolean", name);
        *f &= ~flags[i].value;
      }
      return 1;
    }
  }
  return 0;
}

static void set_option(struct termios *new, char *option)
{
  int on = (*option != '-');

  if (!on) option++;
  if (!set_flag(&new->c_cflag, cflags, ARRAY_LEN(cflags), option, on) &&
      !set_flag(&new->c_iflag, iflags, ARRAY_LEN(iflags), option, on) &&
      !set_flag(&new->c_oflag, oflags, ARRAY_LEN(oflags), option, on) &&
      !set_flag(&new->c_lflag, lflags, ARRAY_LEN(lflags), option, on))
    error_exit("unknown option: %s", option);
}

static void set_options(struct termios* new, ...)
{
  va_list va;
  char *option;

  va_start(va, new);
  while ((option = va_arg(va, char *))) set_option(new, option);
  va_end(va);
}

static void set_size(int is_rows, unsigned short value)
{
  struct winsize ws;

  if (ioctl(TT.fd, TIOCGWINSZ, &ws)) perror_exit("TIOCGWINSZ %s", TT.F);
  if (is_rows) ws.ws_row = value;
  else ws.ws_col = value;
  if (ioctl(TT.fd, TIOCSWINSZ, &ws)) perror_exit("TIOCSWINSZ %s", TT.F);
}

static int set_special_character(struct termios *new, int *i, char *char_name)
{
  int j;

  // The -2 is to ignore VMIN and VTIME, which are just unsigned integers.
  for (j=0;j<ARRAY_LEN(chars)-2;j++) {
    if (!strcmp(chars[j].name, char_name)) {
      char *arg = toys.optargs[++(*i)];
      cc_t ch;

      if (!arg) error_exit("missing arg");
      if (!strcmp(arg, "^-") || !strcmp(arg, "undef")) ch = _POSIX_VDISABLE;
      else if (!strcmp(arg, "^?")) ch = 0x7f;
      else if (arg[0] == '^' && arg[2] == 0) ch = (toupper(arg[1])-'@');
      else if (!arg[1]) ch = arg[0];
      else error_exit("invalid arg: %s", arg);
      new->c_cc[chars[j].value] = ch;
      return 1;
    }
  }
  return 0;
}

static void make_sane(struct termios *t)
{
  // POSIX has no opinion about what "sane" means. From "man stty".
  // "cs8" is missing from the man page, but needed to get identical results.
  set_options(t, "cread", "-ignbrk", "brkint", "-inlcr", "-igncr", "icrnl",
    "icanon", "iexten", "echo", "echoe", "echok", "-echonl", "-noflsh",
    "-ixoff", "-iutf8", "-iuclc", "-ixany", "imaxbel", "-xcase", "-olcuc",
    "-ocrnl", "opost", "-ofill", "onlcr", "-onocr", "-onlret", "nl0", "cr0",
    "tab0", "bs0", "vt0", "ff0", "isig", "-tostop", "-ofdel", "-echoprt",
    "echoctl", "echoke", "-extproc", "-flusho", "cs8", NULL);
  memset(t->c_cc, 0, NCCS);
  t->c_cc[VINTR] = 0x3;
  t->c_cc[VQUIT] = 0x1c;
  t->c_cc[VERASE] = 0x7f;
  t->c_cc[VKILL] = 0x15;
  t->c_cc[VEOF] = 0x4;
  t->c_cc[VTIME] = 0;
  t->c_cc[VMIN] = 1;
  t->c_cc[VSWTC] = 0;
  t->c_cc[VSTART] = 0x11;
  t->c_cc[VSTOP] = 0x13;
  t->c_cc[VSUSP] = 0x1a;
  t->c_cc[VEOL] = 0;
  t->c_cc[VREPRINT] = 0x12;
  t->c_cc[VDISCARD] = 0xf;
  t->c_cc[VWERASE] = 0x17;
  t->c_cc[VLNEXT] = 0x16;
  t->c_cc[VEOL2] = 0;
}

static void xtcgetattr(struct termios *t)
{
  if (tcgetattr(TT.fd, t)) perror_exit("tcgetattr %s", TT.F);
}

void stty_main(void)
{
  struct termios old, sane;
  int i, j, n;

  if (toys.optflags&(FLAG_a|FLAG_g) && *toys.optargs)
    error_exit("no settings with -a/-g");

  if (!TT.F) TT.F = "standard input";
  else TT.fd = xopen(TT.F, (O_RDWR*!!*toys.optargs)|O_NOCTTY|O_NONBLOCK);

  xtcgetattr(&old);

  if (*toys.optargs) {
    struct termios new = old, tmp;

    for (i=0; toys.optargs[i]; i++) {
      char *arg = toys.optargs[i];

      if (!strcmp(arg, "size")) show_size(0);
      else if (!strcmp(arg, "speed")) show_speed(&old, 0);
      else if (!strcmp(arg, "line")) new.c_line = get_arg(&i, NR_LDISCS);
      else if (!strcmp(arg, "min")) new.c_cc[VMIN] = get_arg(&i, 255);
      else if (!strcmp(arg, "time")) new.c_cc[VTIME] = get_arg(&i, 255);
      else if (sscanf(arg, "%x:%x:%x:%x:%n", &tmp.c_iflag, &tmp.c_oflag,
                        &tmp.c_cflag, &tmp.c_lflag, &n) == 4)
      {
        int value;

        new.c_iflag = tmp.c_iflag;
        new.c_oflag = tmp.c_oflag;
        new.c_cflag = tmp.c_cflag;
        new.c_lflag = tmp.c_lflag;
        arg += n;
        for (j=0;j<NCCS;j++) {
          if (sscanf(arg, "%x%n", &value, &n) != 1) error_exit("bad -g string");
          new.c_cc[j] = value;
          arg += n+1;
        }
      } else if (atoi(arg) > 0) {
        int new_speed = speed(atolx_range(arg, 0, 4000000));

        cfsetispeed(&new, new_speed);
        cfsetospeed(&new, new_speed);
      } else if (!strcmp(arg, "ispeed"))
        cfsetispeed(&new, speed(get_arg(&i, 4000000)));
      else if (!strcmp(arg, "ospeed"))
        cfsetospeed(&new, speed(get_arg(&i, 4000000)));
      else if (!strcmp(arg, "rows")) set_size(1, get_arg(&i, USHRT_MAX));
      else if (!strcmp(arg, "cols") || !strcmp(arg, "columns"))
        set_size(0, get_arg(&i, USHRT_MAX));
      else if (set_special_character(&new, &i, arg));
        // Already done as a side effect.
      else if (!strcmp(arg, "cooked"))
        set_options(&new, "brkint", "ignpar", "istrip", "icrnl", "ixon",
          "opost", "isig", "icanon", NULL);
      else if (!strcmp(arg, "evenp") || !strcmp(arg, "parity"))
        set_options(&new, "parenb", "cs7", "-parodd", NULL);
      else if (!strcmp(arg, "oddp"))
        set_options(&new, "parenb", "cs7", "parodd", NULL);
      else if (!strcmp(arg, "-parity") || !strcmp(arg, "-evenp") ||
                 !strcmp(arg, "-oddp")) {
        set_options(&new, "-parenb", "cs8", NULL);
      } else if (!strcmp(arg, "raw")) {
        // POSIX and "man stty" differ wildly. This is "man stty".
        set_options(&new, "-ignbrk", "-brkint", "-ignpar", "-parmrk", "-inpck",
          "-istrip", "-inlcr", "-igncr", "-icrnl", "-ixon", "-ixoff", "-iuclc",
          "-ixany", "-imaxbel", "-opost", "-isig", "-icanon", "-xcase", NULL);
        new.c_cc[VMIN] = 1;
        new.c_cc[VTIME] = 0;
      } else if (!strcmp(arg, "nl"))
        set_options(&new, "-icrnl", "-ocrnl", NULL);
      else if (!strcmp(arg, "-nl"))
        set_options(&new, "icrnl", "ocrnl", "-inlcr", "-igncr", NULL);
      else if (!strcmp(arg, "ek")) {
        new.c_cc[VERASE] = 0x7f;
        new.c_cc[VKILL] = 0x15;
      } else if (!strcmp(arg, "sane")) make_sane(&new);
      else {
        // Translate historical cruft into canonical forms.
        for (j=0; j<ARRAY_LEN(synonyms); j++) {
          if (!strcmp(synonyms[j].from, arg)) {
            arg = synonyms[j].to;
            break;
          }
        }
        set_option(&new, arg);
      }
    }

    tcsetattr(TT.fd, TCSADRAIN, &new);
    xtcgetattr(&old);
    if (memcmp(&old, &new, sizeof(old)))
      error_exit("unable to perform all requested operations on %s", TT.F);

    return;
  }

  if (FLAG(g)) {
    xprintf("%x:%x:%x:%x:", old.c_iflag, old.c_oflag, old.c_cflag, old.c_lflag);
    for (i=0;i<NCCS;i++) xprintf("%x%c", old.c_cc[i], i==NCCS-1?'\n':':');
    return;
  }

  // Without arguments, "stty" only shows the speed, the line discipline,
  // special characters and any flags that differ from the "sane" settings.
  make_sane(&sane);
  show_speed(&old, 1);
  if (FLAG(a)) show_size(1);
  out("line = %d;\n", old.c_line);

  for (i=j=0; i<ARRAY_LEN(chars); i++) {
    char vis[16] = {};
    cc_t ch = old.c_cc[chars[i].value];

    if (ch == sane.c_cc[chars[i].value] && !FLAG(a))
      continue;

    if (chars[i].value == VMIN || chars[i].value == VTIME)
      snprintf(vis, sizeof(vis), "%u", ch);
    else if (ch == _POSIX_VDISABLE) strcat(vis, "<undef>");
    else {
      if (ch > 0x7f) {
        strcat(vis, "M-");
        ch -= 128;
      }
      if (ch < ' ') sprintf(vis+strlen(vis), "^%c", (ch+'@'));
      else if (ch == 0x7f) strcat(vis, "^?");
      else sprintf(vis+strlen(vis), "%c", ch);
    }
    out("%s = %s;", chars[i].name, vis);
    j++;
  }
  if (j) out("\n");

  show_flags(old.c_cflag, sane.c_cflag, cflags, ARRAY_LEN(cflags));
  show_flags(old.c_iflag, sane.c_iflag, iflags, ARRAY_LEN(iflags));
  show_flags(old.c_oflag, sane.c_oflag, oflags, ARRAY_LEN(oflags));
  show_flags(old.c_lflag, sane.c_lflag, lflags, ARRAY_LEN(lflags));

  if (TT.fd) close(TT.fd);
}
