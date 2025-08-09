/* microcom.c - Simple serial console.
 *
 * Copyright 2017 The Android Open Source Project.

USE_MICROCOM(NEWTOY(microcom, "<1>1s#X", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_NOBUF))

config MICROCOM
  bool "microcom"
  default y
  help
    usage: microcom [-s SPEED] [-X] DEVICE

    Simple serial console. Hit CTRL-] for menu.

    -s	Set baud rate to SPEED
    -X	Ignore ^] menu escape
*/

#define FOR_microcom
#include "toys.h"

GLOBALS(
  long s;

  int fd, stok;
  struct termios old_stdin, old_fd;
)

// TODO: tty_sigreset outputs ansi escape sequences, how to disable?
static void restore_states(int i)
{
  if (TT.stok) tcsetattr(0, TCSANOW, &TT.old_stdin);
  tcsetattr(TT.fd, FLAG(s)*TCSAFLUSH, &TT.old_fd);
}

// TODO bookmark, jump to bottom line (if !end), clear and return afterwards
static void handle_esc(void)
{
  char input;

  xputsn("\r\n[b]reak, [p]aste file, [q]uit: ");
  if (read(0, &input, 1)<1 || input == CTRL('D') || input == 'q') {
    xputs("exit\r");
    xexit();
  }
  if (input == 'b') tcsendbreak(TT.fd, 0);
  else if (input == 'p') {
    long long written = 0, size;
    char* filename;
    int len = 0, fd;

    // TODO: share code with hexedit's prompt() and vi's ex mode.
    // TODO: tab completion!
    memset(toybuf, 0, sizeof(toybuf));
    while (1) {
      xprintf("\r\e[2K\e[1mFilename: \e[0m%s", toybuf);
      if (read(0, &input, 1) <= 0 || input == CTRL('[')) {
        len = 0;
        break;
      }
      if (input == '\r') break;
      if (input == 0x7f && len > 0) toybuf[--len] = 0;
      else if (input == CTRL('U')) while (len > 0) toybuf[--len] = 0;
      else if (input >= ' ' && input <= 0x7f && len < sizeof(toybuf))
        toybuf[len++] = input;
    }
    xputsn("\r\e[2K");
    toybuf[len] = 0;
    if (!len) return;
    if ((fd = xopen(toybuf, O_RDONLY | WARN_ONLY)) < 0) {
      // xopen() warning message ends with a LF without CR, so manually print a
      // CR here to move the cursor back to the front.
      fputc('\r', stderr);
      return;
    }
    // TODO xsendfile with progress indicator
    filename = xstrdup(toybuf);
    size = fdlength(fd);
    // The alternative would be to just feed this fd into the usual loop,
    // so we're reading back these characters if they're being echoed, but
    // for my specific use case of pasting into `base64 -d -i > foo`, this
    // is a much more convenient UI.
    while ((len = read(fd, toybuf, sizeof(toybuf))) > 0) {
      written += len;
      xprintf("\r\e[2KPasting '%s' %lld/%lld (%lld%%)...", filename, written,
        size, written*100/size);
      xwrite(TT.fd, toybuf, len);
    }
    free(filename);
    close(fd);
  } else xprintf("Ignoring unknown command.");

  xprintf("\r\n");
}

void microcom_main(void)
{
  struct termios tio;
  struct pollfd fds[2];
  int i;

  // Open with O_NDELAY, but switch back to blocking for reads.
  TT.fd = xopen(*toys.optargs, O_RDWR | O_NOCTTY | O_NDELAY);
  if (-1==(i = fcntl(TT.fd, F_GETFL, 0)) || fcntl(TT.fd, F_SETFL, i&~O_NDELAY)
      || tcgetattr(TT.fd, &TT.old_fd))
    perror_exit_raw(*toys.optargs);

  // Set both input and output to raw mode.
  memcpy(&tio, &TT.old_fd, sizeof(struct termios));
  cfmakeraw(&tio);
  if (FLAG(s)) xsetspeed(&tio, TT.s);
  if (tcsetattr(TT.fd, FLAG(s)*TCSAFLUSH, &tio)) perror_exit("set speed");
  if (!set_terminal(0, 1, 0, &TT.old_stdin)) TT.stok++;
  // ...and arrange to restore things, however we may exit.
  sigatexit(restore_states);

  fds[0].fd = TT.fd;
  fds[1].fd = 0;
  fds[0].events = fds[1].events = POLLIN;

  if (!FLAG(X)) xputs("Escape character is '^]'.\r");
  while (poll(fds, 2, -1) > 0) {

    // Read from connection, write to stdout.
    if (fds[0].revents) {
      if (0 < (i = read(TT.fd, toybuf, sizeof(toybuf)))) xwrite(0, toybuf, i);
      else break;
    }

    // Read from stdin, write to connection.
    if (fds[1].revents) {
      if (read(0, toybuf, 1) != 1) break;
      if (!FLAG(X) && *toybuf == CTRL(']')) handle_esc();
      else xwrite(TT.fd, toybuf, 1);
    }
  }
}
