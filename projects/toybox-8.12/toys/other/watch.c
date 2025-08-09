/* watch.c - Show bounded output of a periodically executed command.
 *
 * Copyright 2013 Sandeep Sharma <sandeep.jack2756@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No standard. See http://man7.org/linux/man-pages/man1/watch.1.html
 *
 * TODO: trailing combining characters

USE_WATCH(NEWTOY(watch, "^<1n%<100=2000tebx", TOYFLAG_USR|TOYFLAG_BIN))

config WATCH
  bool "watch"
  default y
  help
    usage: watch [-tebx] [-n SEC] COMMAND...

    Run COMMAND every -n seconds, showing output that fits terminal, q to quit.

    -n	Number of seconds between repeats (default 2.0)
    -t	Don't print header
    -e	Exit on error
    -b	Beep on command error
    -x	Exec command directly (without "sh -c")
*/

#define FOR_watch
#include "toys.h"

GLOBALS(
  int n;

  pid_t pid, oldpid;
)

// When a child process exits, stop tracking them. Handle errors for -be
static void watch_child(int sig)
{
  int status;
  pid_t pid = wait(&status);

  status = WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status)+127;
  if (status) {
    if (FLAG(b)) putchar('\a');
    if (FLAG(e)) {
      printf("Exit status %d\r\n", status);
      tty_reset();
      _exit(status);
    }
  }

  if (pid == TT.oldpid) TT.oldpid = 0;
  else if (pid == TT.pid) TT.pid = 0;
}

// Return early for low-ascii characters with special behavior,
// discard remaining low ascii, escape other unprintable chars normally
static int watch_escape(FILE *out, int cols, int wc)
{
  if (wc==27 || (wc>=7 && wc<=13)) return -1;
  if (wc<32) return 0;

  return crunch_escape(out, cols, wc);
}

void watch_main(void)
{
  char *cmdv[] = {"/bin/sh", "-c", 0, 0}, *cmd, *ss;
  long long now, then = millitime();
  unsigned width, height, i, cmdlen, len, xx QUIET, yy QUIET, active QUIET;
  struct pollfd pfd[2];
  pid_t pid = 0;
  int fds[2], cc;

  // Assemble header line in cmd, cmdlen, and cmdv
  for (i = TT.n%1000, len = i ? 3 : 1; i && !(i%10); i /= 10) len--;
  len = sprintf(toybuf, "Every %u.%0*us:", TT.n/1000, len, i)+1;
  cmdlen = len;
  for (i = 0; toys.optargs[i]; i++) len += strlen(toys.optargs[i])+1;
  ss = stpcpy(cmd = xmalloc(len), toybuf);
  cmdv[2] = cmd+cmdlen;
  for (i = 0; toys.optargs[i]; i++) ss += sprintf(ss, " %s",toys.optargs[i]);
  cmdlen = ss-cmd;

  // Need to poll on process output and stdin
  memset(pfd, 0, sizeof(pfd));
  pfd[0].events = pfd[1].events = POLLIN;

  xsignal_flags(SIGCHLD, watch_child, SA_RESTART|SA_NOCLDSTOP);

  for (;;) {
    fflush(0);

    // Time for a new period?
    if ((now = millitime())>=then) {
      // Incrementing then instead of adding offset to now avoids drift,
      // loop in case we got suspend/resumed and need to skip periods
      while ((then += TT.n)<=now);
      start_redraw(&width, &height);

      // redraw the header
      if (!FLAG(t)) {
        time_t t = time(0);
        int pad, ctimelen;

        // Get and measure time string, trimming gratuitous \n
        ctimelen = strlen(ss = ctime(&t));
        if (ss[ctimelen-1]=='\n') ss[--ctimelen] = 0;

        // print cmdline, then * or ' ' (showing truncation), then ctime
        pad = width-++ctimelen;
        if (pad>0) draw_trim(cmd, -pad, pad);
        printf("%c", pad<cmdlen ? '*' : ' ');
        if (width) xputs(ss+(width>ctimelen ? 0 : width-1));
        if (height>=3) xprintf("\r\n");
        xx = 0;
        yy = 2;
      }

      // If child didn't exit, send TERM signal to current and KILL to previous
      if (TT.oldpid>0) kill(TT.oldpid, SIGKILL);
      if (TT.pid>0) kill(TT.pid, SIGTERM);
      TT.oldpid = pid;
      if (fds[0]>0) close(fds[0]);
      if (fds[1]>0) close(fds[1]);

      // Spawn child process
      fds[0] = fds[1] = -1;
      TT.pid = xpopen_both(FLAG(x) ? toys.optargs : cmdv, fds);
      pfd[1].fd = fds[1];
      active = 1;
    }

    // Fetch data from child process or keyboard, with timeout
    len = 0;
    xpoll(pfd, 1+(active && yy<height), then-now);
    if (pfd[0].revents&POLLIN) {
      memset(toybuf, 0, 16);
      cc = scan_key_getsize(toybuf, 0, &width, &height);
      // TODO: ctrl-Z suspend
      // TODO if (cc == -3) redraw();
      if (cc == 3 || tolower(cc) == 'q') xexit();
    }
    if (pfd[0].revents&POLLHUP) xexit();
    if (active) {
      if (pfd[1].revents&POLLIN) len = read(fds[1], toybuf, sizeof(toybuf)-1);
      if (pfd[1].revents&POLLHUP) active = 0;
    }

    // Measure output, trim to available display area. Escape low ascii so
    // we don't have to try to parse ansi escapes. TODO: parse ansi escapes.
    if (len<1) continue;
    ss = toybuf;
    toybuf[len] = 0;
    while (yy<height) {
      if (xx==width) {
        xx = 0;
        if (++yy>=height) break;
      }
      xx += crunch_str(&ss, width-xx, stdout, 0, watch_escape);
      if (xx==width) {
        xx = 0;
        if (++yy>=height) break;
        continue;
      }

      if (ss-toybuf==len || *ss>27) break;
      cc = *ss++;
      if (cc==27) continue; // TODO

      // Handle BEL BS HT LF VT FF CR
      if (cc>=10 && cc<=12) {
        if (++yy>=height) break;
        if (cc=='\n') putchar('\r'), xx = 0;
      }
      putchar(cc);
      if (cc=='\b' && xx) xx--;
      else if (cc=='\t') {
        xx = (xx|7)+1;
        if (xx>width-1) xx = width-1;
      }
    }
  }

  if (CFG_TOYBOX_FREE) free(cmd);
}
