/* tty.c - cursor control
 *
 * Copyright 2015 Rob Landley <rob@landley.net>
 *
 * Common ANSI (See https://man7.org/linux/man-pages/man4/console_codes.4.html)
 * \e[#m   - color change           \e[y;xH - jump to x/y pos (1;1 is top left)
 * \e[K    - delete to EOL          \e[25l  - disable cursor (h to enable)
 * \e[1L   - Insert 1 (blank) line  \e[1M   - Delete 1 line (scrolling rest up)
 * \e[2J   - clear screen
 *
 * colors: 0=black 1=red 2=green 3=brown 4=blue 5=purple 6=cyan 7=grey
 *         +30 foreground, +40 background.
 *         \e[1m = bright, \e[2m = dark, \e[0m = reset to defaults
 *         \e[1;32;2;42mhello\e[0m - dark green text on light green background
 */

#include "toys.h"

// Check stdout, stderr, stdin (in that order) and if none open /dev/tty
int tty_fd(void)
{
  int i, j;

  for (i = 0; i<3; i++) if (isatty(j = (i+1)%3)) return j;

  return xnotstdio(open("/dev/tty", O_RDWR));
}

// Query size of terminal (without ANSI probe fallback).
// set x=80 y=25 before calling to provide defaults. Returns 0 if couldn't
// determine size.

int terminal_size(unsigned *xx, unsigned *yy)
{
  struct winsize ws;
  unsigned i, x = 0, y = 0;
  char *s;

  // Check stdin, stdout, stderr
  for (i = 0; i<3; i++) {
    memset(&ws, 0, sizeof(ws));
    if (isatty(i) && !ioctl(i, TIOCGWINSZ, &ws)) {
      if (ws.ws_col) x = ws.ws_col;
      if (ws.ws_row) y = ws.ws_row;

      break;
    }
  }
  s = getenv("COLUMNS");
  if (s) sscanf(s, "%u", &x);
  s = getenv("LINES");
  if (s) sscanf(s, "%u", &y);

  // Never return 0 for either value, leave it at default instead.
  if (xx && x) *xx = x;
  if (yy && y) *yy = y;

  return x || y;
}

// Query terminal size, sending ANSI probe if necesary. (Probe queries xterm
// size through serial connection, when local TTY doesn't know but remote does.)
// Returns 0 if ANSI probe sent, 1 if size determined from tty or environment

int terminal_probesize(unsigned *xx, unsigned *yy)
{
  if (terminal_size(xx, yy) && (!xx || *xx) && (!yy || *yy)) return 1;

  // Send probe: bookmark cursor position, jump to bottom right,
  // query position, return cursor to bookmarked position.
  xprintf("\e[s\e[999C\e[999B\e[6n\e[u");

  return 0;
}

// This table skips both B0 and BOTHER
static const int speeds[] = {50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800,
  2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 500000, 576000,
  921600, 1000000, 1152000, 1500000, 2000000, 2500000, 3000000,3500000,4000000};

// Show bits per second for cfspeed value. Assumes we have a valid speed
unsigned cfspeed2bps(unsigned speed)
{
  if (!(speed&15)) return 0;
  if (speed>15) speed = (speed&15)+15;

  return speeds[--speed];
}

// Convert bits per second to cfspeed value. Returns 0 for unknown bps
unsigned bps2cfspeed(unsigned baud)
{
  int i = 0;

  while (i<ARRAY_LEN(speeds))
    if (speeds[i++]==baud) return i+(i>15)*(4096-16+1);

  return 0;
}

void xsetspeed(struct termios *tio, int bps)
{
  int i = bps2cfspeed(bps);

  if (!i) error_exit("unknown speed: %d", bps);
  cfsetspeed(tio, i);
}

// Reset terminal to known state, saving copy of old state if old != NULL.
int set_terminal(int fd, int raw, int speed, struct termios *old)
{
  struct termios tio;
  int i = tcgetattr(fd, &tio);

  // Fetch local copy of old terminfo, and copy struct contents to *old if set
  if (i) return i;
  if (old) *old = tio;

  cfmakeraw(&tio);
  if (speed) xsetspeed(&tio, speed);
  if (!raw) {
    // Put the "cooked" bits back.

    // Convert CR to NL on input, UTF8 aware backspace, Any key unblocks input.
    tio.c_iflag |= ICRNL|IUTF8|IXANY;

    // Output appends CR to NL and does magic undocumented postprocessing.
    tio.c_oflag |= OPOST|ONLCR;

    // 8 bit chars, enable receiver.
    tio.c_cflag |= CS8|CREAD;

    // Generate signals, input entire line at once, echo output erase,
    // line kill, escape control characters with ^, erase line char at a time
    // "extended" behavior: ctrl-V quotes next char, ctrl-R reprints unread line
    // ctrl-W erases word
    tio.c_lflag |= ISIG|ICANON|ECHO|ECHOE|ECHOK|ECHOCTL|ECHOKE|IEXTEN;
  }

  return tcsetattr(fd, TCSAFLUSH, &tio);
}

void xset_terminal(int fd, int raw, int speed, struct termios *old)
{
  if (-1 == set_terminal(fd, raw, speed, old)) perror_exit("tcsetattr");
}

struct scan_key_list {
  int key;
  char *seq;
} static const scan_key_list[] = {
  {KEY_UP, "\e[A"}, {KEY_DOWN, "\e[B"},
  {KEY_RIGHT, "\e[C"}, {KEY_LEFT, "\e[D"},

  {KEY_UP|KEY_SHIFT, "\e[1;2A"}, {KEY_DOWN|KEY_SHIFT, "\e[1;2B"},
  {KEY_RIGHT|KEY_SHIFT, "\e[1;2C"}, {KEY_LEFT|KEY_SHIFT, "\e[1;2D"},

  {KEY_UP|KEY_ALT, "\e[1;3A"}, {KEY_DOWN|KEY_ALT, "\e[1;3B"},
  {KEY_RIGHT|KEY_ALT, "\e[1;3C"}, {KEY_LEFT|KEY_ALT, "\e[1;3D"},

  {KEY_UP|KEY_CTRL, "\e[1;5A"}, {KEY_DOWN|KEY_CTRL, "\e[1;5B"},
  {KEY_RIGHT|KEY_CTRL, "\e[1;5C"}, {KEY_LEFT|KEY_CTRL, "\e[1;5D"},

  // VT102/VT220 escapes.
  {KEY_HOME, "\e[1~"},
  {KEY_HOME|KEY_CTRL, "\e[1;5~"},
  {KEY_INSERT, "\e[2~"},
  {KEY_DELETE, "\e[3~"},
  {KEY_END, "\e[4~"},
  {KEY_END|KEY_CTRL, "\e[4;5~"},
  {KEY_PGUP, "\e[5~"},
  {KEY_PGDN, "\e[6~"},
  // "Normal" "PC" escapes (xterm).
  {KEY_HOME, "\eOH"},
  {KEY_END, "\eOF"},
  // "Application" "PC" escapes (gnome-terminal).
  {KEY_HOME, "\e[H"},
  {KEY_END, "\e[F"},
  {KEY_HOME|KEY_CTRL, "\e[1;5H"},
  {KEY_END|KEY_CTRL, "\e[1;5F"},

  {KEY_FN+1, "\eOP"}, {KEY_FN+2, "\eOQ"}, {KEY_FN+3, "\eOR"},
  {KEY_FN+4, "\eOS"}, {KEY_FN+5, "\e[15~"}, {KEY_FN+6, "\e[17~"},
  {KEY_FN+7, "\e[18~"}, {KEY_FN+8, "\e[19~"}, {KEY_FN+9, "\e[20~"},
};

// Scan stdin for a keypress, parsing known escape sequences, including
// responses to screen size queries.
// Blocks for timeout_ms milliseconds, 0=return immediately, -1=wait forever.
// Returns 0-255=literal, -1=EOF, -2=TIMEOUT, -3=RESIZE, 256+= a KEY_ constant.
// Scratch space is necessary because last char of !seq could start new seq.
// Zero out first byte of scratch before first call to scan_key.
int scan_key_getsize(char *scratch, int timeout_ms, unsigned *xx, unsigned *yy)
{
  struct pollfd pfd;
  int maybe, i, j;
  char *test;

  for (;;) {
    pfd.fd = 0;
    pfd.events = POLLIN;
    pfd.revents = 0;

    maybe = 0;
    if (*scratch) {
      int pos[6];
      unsigned x, y;

      // Check for return from terminal size probe
      memset(pos, 0, 6*sizeof(int));
      scratch[(1+*scratch)&15] = 0;
      sscanf(scratch+1, "\e%n[%n%3u%n;%n%3u%nR%n", pos, pos+1, &y,
             pos+2, pos+3, &x, pos+4, pos+5);
      if (pos[5]) {
        // Recognized X/Y position, consume and return
        *scratch = 0;
        if (xx) *xx = x;
        if (yy) *yy = y;
        return -3;
      } else for (i=0; i<6; i++) if (pos[i]==*scratch) maybe = 1;

      // Check sequences
      for (i = 0; i<ARRAY_LEN(scan_key_list); i++) {
        test = scan_key_list[i].seq;
        for (j = 0; j<*scratch; j++) if (scratch[j+1] != test[j]) break;
        if (j == *scratch) {
          maybe = 1;
          if (!test[j]) {
            // We recognized current sequence: consume and return
            *scratch = 0;
            return 256+scan_key_list[i].key;
          }
        }
      }

      // If current data can't be a known sequence, return next raw char
      if (!maybe) break;
    }

    // Need more data to decide

    // 30ms is about the gap between characters at 300 baud
    if (maybe || timeout_ms != -1)
      if (!xpoll(&pfd, 1, maybe ? 30 : timeout_ms)) break;

    // Read 1 byte so we don't overshoot sequence match. (We can deviate
    // and fail to match, but match consumes entire buffer.)
    if (toys.signal>0 || 1 != read(0, scratch+1+*scratch, 1))
      return (toys.signal>0) ? -3 : -1;
    ++*scratch;
  }

  // Was not a sequence
  if (!*scratch) return -2;
  i = scratch[1];
  if (--*scratch) memmove(scratch+1, scratch+2, *scratch);

  return i;
}

// Wrapper that ignores results from ANSI probe to update screensize.
// Otherwise acts like scan_key_getsize().
int scan_key(char *scratch, int timeout_ms)
{
  return scan_key_getsize(scratch, timeout_ms, NULL, NULL);
}

void tty_reset(void)
{
  set_terminal(0, 0, 0, 0);
  xputsn("\e[?25h\e[0m\e[999H\e[K");
}

// If you call set_terminal(), use sigatexit(tty_sigreset);
void tty_sigreset(int i)
{
  tty_reset();
  _exit(i ? 128+i : 0);
}

void start_redraw(unsigned *width, unsigned *height)
{
  // If never signaled, do raw mode setup.
  if (!toys.signal) {
    *width = 80;
    *height = 25;
    set_terminal(0, 1, 0, 0);
    sigatexit(tty_sigreset);
    xsignal(SIGWINCH, generic_signal);
  }
  if (toys.signal != -1) {
    toys.signal = -1;
    terminal_probesize(width, height);
  }
  xputsn("\e[H\e[J");
}
