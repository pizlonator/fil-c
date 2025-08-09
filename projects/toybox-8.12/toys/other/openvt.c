/* openvt.c - Run a program on a new VT
 *
 * Copyright 2008 David Anders <danders@amltd.com>
 * Copyright 2014 Vivek Kumar Bhagat <vivek.bhagat89@gmail.com>
 *
 * No Standard

USE_OPENVT(NEWTOY(openvt, "^<1c#<1>63sw", TOYFLAG_BIN|TOYFLAG_NEEDROOT))
USE_CHVT(NEWTOY(chvt, "<1>1", TOYFLAG_USR|TOYFLAG_BIN))
USE_DEALLOCVT(NEWTOY(deallocvt, ">1", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_NEEDROOT))

config OPENVT
  bool "openvt"
  default y
  help
    usage: openvt [-c NUM] [-sw] COMMAND...

    Run COMMAND on a new virtual terminal.

    -c NUM  Use VT NUM
    -s    Switch to the new VT
    -w    Wait for command to exit (with -s, deallocates VT on exit)

config CHVT
  bool "chvt"
  default y
  help
    usage: chvt NUM

    Change to virtual terminal number NUM. (This only works in text mode.)

    Virtual terminals are the Linux VGA text mode (or framebuffer) displays,
    switched between via alt-F1, alt-F2, etc. Use ctrl-alt-F1 to switch
    from X11 to a virtual terminal, and alt-F6 (or F7, or F8) to get back.

config DEALLOCVT
  bool "deallocvt"
  default y
  help
    usage: deallocvt [NUM]

    Deallocate unused virtual terminals, either a specific /dev/ttyNUM, or all.
*/

#define FOR_openvt
#include "toys.h"
#include <linux/vt.h>
#include <linux/kd.h>

GLOBALS(
  long c;
)

static int open_console(void)
{
  char arg = 0, *console_name[] = {"/dev/tty", "/dev/tty0", "/dev/console"};
  int i, fd;

  for (i = 0; i < ARRAY_LEN(console_name); i++) {
    if (0>(fd = open(console_name[i], O_RDWR))) continue;
    if (!ioctl(fd, KDGKBTYPE, &arg)) return fd;
    close(fd);
  }
  for (fd = 0; fd < 3; fd++) if (!ioctl(fd, KDGKBTYPE, &arg)) return fd;
  error_exit("can't open console");
}

static int activate(int fd, int cc)
{
  return ioctl(fd, VT_ACTIVATE, cc) || ioctl(fd, VT_WAITACTIVE, cc);
}

void openvt_main(void)
{
  struct vt_stat vstate;
  int fd, cc = (int)TT.c;
  pid_t pid;

  // find current console
  if (-1 == (ioctl(fd = open_console(), VT_GETSTATE, &vstate)) ||
      (!cc && 0>=(cc = xioctl(fd, VT_OPENQRY, &fd))))
    perror_exit("can't find open VT");

  sprintf(toybuf, "/dev/tty%d", cc);
  if (!(pid = XVFORK())) {
    close(0);  //new vt becomes stdin
    dup2(dup2(xopen_stdio(toybuf, O_RDWR), 1), 2);
    if (FLAG(s)) activate(0, cc);
    setsid();
    ioctl(0, TIOCSCTTY, 0);
    if (fd>2) close(fd);
    xexec(toys.optargs);
  }
  if (FLAG(w)) {
    while (-1 == waitpid(pid, NULL, 0) && errno == EINTR) errno = 0;
    if (FLAG(s)) {
      activate(fd, vstate.v_active);
      dprintf(2, "%d\n", ioctl(fd, VT_DISALLOCATE, cc));
    }
  }
}

void chvt_main(void)
{
  if (activate(open_console(), atoi(*toys.optargs)))
    perror_exit_raw(*toys.optargs);
}

void deallocvt_main(void)
{
  int fd = open_console(), vt_num = 0; // 0 = all

  if (*toys.optargs) vt_num = atolx_range(*toys.optargs, 1, 63);
  if (-1 == ioctl(fd, VT_DISALLOCATE, vt_num)) perror_exit("%d", vt_num);
}
