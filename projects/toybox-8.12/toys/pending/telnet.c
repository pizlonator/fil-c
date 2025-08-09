/* telnet.c - Telnet client.
 *
 * Copyright 2012 Madhur Verma <mad.flexi@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 * Modified by Ashwini Kumar <ak.ashwini1981@gmail.com>
 *
 * Not in SUSv4.

USE_TELNET(NEWTOY(telnet, "<1>2", TOYFLAG_BIN))

config TELNET
  bool "telnet"
  default n
  help
    usage: telnet HOST [PORT]

    Connect to telnet server.
*/

#define FOR_telnet
#include "toys.h"
#include <arpa/telnet.h>

GLOBALS(
  int sock;
  char buf[2048]; // Half sizeof(toybuf) allows a buffer full of IACs.
  struct termios old_term;
  struct termios raw_term;
  uint8_t mode;
  int echo, sga;
  int state, request;
)

#define NORMAL 0
#define SAW_IAC 1
#define SAW_WWDD 2
#define SAW_SB 3
#define SAW_SB_TTYPE 4
#define WANT_IAC 5
#define WANT_SE 6
#define SAW_CR 10

#define CM_TRY      0
#define CM_ON       1
#define CM_OFF      2

static void raw(int raw)
{
  tcsetattr(0, TCSADRAIN, raw ? &TT.raw_term : &TT.old_term);
}

static void slc(int line)
{
  TT.mode = line ? CM_OFF : CM_ON;
  xprintf("Entering %s mode\r\nEscape character is '^%c'.\r\n",
      line ? "line" : "character", line ? 'C' : ']');
  raw(!line);
}

static void set_mode(void)
{
  if (TT.echo) {
    if (TT.mode == CM_TRY) slc(0);
  } else if (TT.mode != CM_OFF) slc(1);
}

static void handle_esc(void)
{
  char input;

  if (toys.signal) raw(1);

  // This matches busybox telnet, not BSD telnet.
  xputsn("\r\n"
      "Console escape. Commands are:\r\n"
      "\r\n"
      " l  go to line mode\r\n"
      " c  go to character mode\r\n"
      " z  suspend telnet\r\n"
      " e  exit telnet\r\n"
      "\r\n"
      "telnet> ");
  // In particular, the boxes only read a single character, not a line.
  if (read(0, &input, 1) <= 0 || input == 4 || input == 'e') {
    xprintf("Connection closed.\r\n");
    xexit();
  }

  if (input == 'l') {
    if (!toys.signal) {
      TT.mode = CM_TRY;
      TT.echo = TT.sga = 0;
      set_mode();
      dprintf(TT.sock,"%c%c%c%c%c%c",IAC,DONT,TELOPT_ECHO,IAC,DONT,TELOPT_SGA);
      goto ret;
    }
  } else if (input == 'c') {
    if (toys.signal) {
      TT.mode = CM_TRY;
      TT.echo = TT.sga = 1;
      set_mode();
      dprintf(TT.sock,"%c%c%c%c%c%c",IAC,DO,TELOPT_ECHO,IAC,DO,TELOPT_SGA);
      goto ret;
    }
  } else if (input == 'z') {
    raw(0);
    kill(0, SIGTSTP);
    raw(1);
  }

  xprintf("telnet %s %s\r\n", toys.optargs[0], toys.optargs[1] ?: "23");
  if (toys.signal) raw(0);

ret:
  toys.signal = 0;
}

// Handle WILL WONT DO DONT requests from the server.
static void handle_wwdd(char opt)
{
  if (opt == TELOPT_ECHO) {
    if (TT.request == DO) dprintf(TT.sock, "%c%c%c", IAC, WONT, TELOPT_ECHO);
    if (TT.request == DONT) return;
    if (TT.echo) {
        if (TT.request == WILL) return;
    } else if (TT.request == WONT) return;
    if (TT.mode != CM_OFF) TT.echo ^= 1;
    dprintf(TT.sock, "%c%c%c", IAC, TT.echo ? DO : DONT, TELOPT_ECHO);
    set_mode();
  } else if (opt == TELOPT_SGA) { // Suppress Go Ahead
    if (TT.sga) {
      if (TT.request == WILL) return;
    } else if (TT.request == WONT) return;
    TT.sga ^= 1;
    dprintf(TT.sock, "%c%c%c", IAC, TT.sga ? DO : DONT, TELOPT_SGA);
  } else if (opt == TELOPT_TTYPE) { // Terminal TYPE
    dprintf(TT.sock, "%c%c%c", IAC, WILL, TELOPT_TTYPE);
  } else if (opt == TELOPT_NAWS) { // Negotiate About Window Size
    unsigned cols = 80, rows = 24;

    terminal_size(&cols, &rows);
    dprintf(TT.sock, "%c%c%c%c%c%c%c%c%c%c%c%c", IAC, WILL, TELOPT_NAWS,
            IAC, SB, TELOPT_NAWS, cols>>8, cols, rows>>8, rows,
            IAC, SE);
  } else {
    // Say "no" to anything we don't understand.
    dprintf(TT.sock, "%c%c%c", IAC, (TT.request == WILL) ? DONT : WONT, opt);
  }
}

static void handle_server_output(int n)
{
  char *p = TT.buf, *end = TT.buf + n, ch;
  int i = 0;

  // Possibilities:
  //
  // 1. Regular character
  // 2. IAC [WILL|WONT|DO|DONT] option
  // 3. IAC SB option arg... IAC SE
  //
  // The only subnegotiation we support is IAC SB TTYPE SEND IAC SE, so we just
  // hard-code that into our state machine rather than having a more general
  // "collect the subnegotation into a buffer and handle it after we've seen
  // the IAC SE at the end". It's 2021, so we're unlikely to need more.

  while (p < end) {
    ch = *p++;
    if (TT.state == SAW_IAC) {
      if (ch >= WILL && ch <= DONT) {
        TT.state = SAW_WWDD;
        TT.request = ch;
      } else if (ch == SB) {
        TT.state = SAW_SB;
      } else {
        TT.state = NORMAL;
      }
    } else if (TT.state == SAW_WWDD) {
      handle_wwdd(ch);
      TT.state = NORMAL;
    } else if (TT.state == SAW_SB) {
      if (ch == TELOPT_TTYPE) TT.state = SAW_SB_TTYPE;
      else TT.state = WANT_IAC;
    } else if (TT.state == SAW_SB_TTYPE) {
      if (ch == TELQUAL_SEND) {
        dprintf(TT.sock, "%c%c%c%c%s%c%c", IAC, SB, TELOPT_TTYPE, TELQUAL_IS,
                getenv("TERM") ?: "NVT", IAC, SE);
      }
      TT.state = WANT_IAC;
    } else if (TT.state == WANT_IAC) {
      if (ch == IAC) TT.state = WANT_SE;
    } else if (TT.state == WANT_SE) {
      if (ch == SE) TT.state = NORMAL;
    } else if (ch == IAC) {
      TT.state = SAW_IAC;
    } else {
      if (TT.state == SAW_CR && ch == '\0') {
        // CR NUL -> CR
      } else toybuf[i++] = ch;
      if (ch == '\r') TT.state = SAW_CR;
      TT.state = NORMAL;
    }
  }
  if (i) xwrite(0, toybuf, i);
}

static void handle_user_input(int n)
{
  char *p = TT.buf, ch;
  int i = 0;

  while (n--) {
    ch = *p++;
    if (ch == 0x1d) {
      handle_esc();
      return;
    }
    toybuf[i++] = ch;
    if (ch == IAC) toybuf[i++] = IAC; // IAC -> IAC IAC
    else if (ch == '\r') toybuf[i++] = '\n'; // CR -> CR LF
    else if (ch == '\n') { // LF -> CR LF
      toybuf[i-1] = '\r';
      toybuf[i++] = '\n';
    }
  }
  if (i) xwrite(TT.sock, toybuf, i);
}

static void reset_terminal(void)
{
  raw(0);
}

void telnet_main(void)
{
  struct pollfd pfds[2];
  int n = 1;

  tcgetattr(0, &TT.old_term);
  TT.raw_term = TT.old_term;
  cfmakeraw(&TT.raw_term);

  TT.sock = xconnectany(xgetaddrinfo(*toys.optargs, toys.optargs[1] ?: "23", 0,
      SOCK_STREAM, IPPROTO_TCP, 0));
  xsetsockopt(TT.sock, SOL_SOCKET, SO_KEEPALIVE, &n, sizeof(n));

  xprintf("Connected to %s.\r\n", *toys.optargs);

  sigatexit(reset_terminal);
  signal(SIGINT, generic_signal);

  pfds[0].fd = 0;
  pfds[0].events = POLLIN;
  pfds[1].fd = TT.sock;
  pfds[1].events = POLLIN;
  for (;;) {
    if (poll(pfds, 2, -1) < 0) {
      if (toys.signal) handle_esc();
      else perror_exit("poll");
    }
    if (pfds[0].revents) {
      if ((n = read(0, TT.buf, sizeof(TT.buf))) <= 0) xexit();
      handle_user_input(n);
    }
    if (pfds[1].revents) {
      if ((n = read(TT.sock, TT.buf, sizeof(TT.buf))) <= 0)
        error_exit("Connection closed by foreign host\r");
      handle_server_output(n);
    }
  }
}
