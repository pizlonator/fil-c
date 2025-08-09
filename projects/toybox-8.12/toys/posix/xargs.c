/* xargs.c - Run command with arguments taken from stdin.
 *
 * Copyright 2011 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/xargs.html
 *
 * TODO: Rich's whitespace objection, env size isn't fixed anymore.
 * TODO: -I	Insert mode
 * TODO: -L	Max number of lines of input per command
 * TODO: -x	Exit if can't fit everything in one command

USE_XARGS(NEWTOY(xargs, "^E:P#<0(null)=1optr(no-run-if-empty)n#<1(max-args)s#0[!0E]", TOYFLAG_USR|TOYFLAG_BIN))

config XARGS
  bool "xargs"
  default y
  help
    usage: xargs [-0Pprt] [-snE STR] COMMAND...

    Run command line one or more times, appending arguments from stdin.

    If COMMAND exits with 255, don't launch another even if arguments remain.

    -0	Each argument is NULL terminated, no whitespace or quote processing
    -E	Stop at line matching string
    -n	Max number of arguments per command
    -o	Open tty for COMMAND's stdin (default /dev/null)
    -P	Parallel processes (default 1)
    -p	Prompt for y/n from tty before running each command
    -r	Don't run with empty input (otherwise always run command once)
    -s	Size in bytes per command line
    -t	Trace, print command line to stderr
*/

#define FOR_xargs
#include "toys.h"

GLOBALS(
  long s, n, P;
  char *E;

  long entries, bytes, np;
  char delim;
  FILE *tty;
)

// If !entry count TT.bytes and TT.entries, stopping at max.
// Otherwise, fill out entry[].

// Returning NULL means need more data.
// Returning char * means hit data limits, start of data left over
// Returning 1 means hit data limits, but consumed all data
// Returning 2 means hit -E STR

static char *handle_entries(char *data, char **entry)
{
  if (TT.delim) {
    char *save, *ss, *s;

    // Chop up whitespace delimited string into args
    for (s = data; *s; TT.entries++) {
      while (isspace(*s)) s++;
      if (TT.n && TT.entries >= TT.n) return *s ? s : (char *)1;
      if (!*s) break;
      save = ss = s;

      // Specifying -s can cause "argument too long" errors.
      if (!FLAG(s)) TT.bytes += sizeof(void *)+1;
      for (;;) {
        if (++TT.bytes >= TT.s) return save;
        if (!*s || isspace(*s)) break;
        s++;
      }
      if (TT.E && strstart(&ss, TT.E) && ss == s) return (char *)2;
      if (entry) {
        entry[TT.entries] = save;
        if (*s) *s++ = 0;
      }
    }

  // -0 support
  } else {
    long bytes = TT.bytes+sizeof(char *)+strlen(data)+1;

    if (bytes >= TT.s || (TT.n && TT.entries >= TT.n)) return data;
    TT.bytes = bytes;
    if (entry) entry[TT.entries] = data;
    TT.entries++;
  }

  return 0;
}

// Handle SIGUSR1 and SIGUSR2 for -P
static void signal_P(int sig)
{
  if (sig == SIGUSR2 && TT.P>1) TT.P--;
  else TT.P++;
}

static void waitchild(int options)
{
  int ii, status;

  if (1>waitpid(-1, &status, options)) return;
  TT.np--;
  ii = WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status)+128;
  if (ii == 255) {
    error_msg("%s: exited with status 255; aborting", *toys.optargs);
    toys.exitval = 124;
  } else if ((ii|1)==127) toys.exitval = ii;
  else if (ii>127) toys.exitval = 125;
  else if (ii) toys.exitval = 123;
}

void xargs_main(void)
{
  struct double_list *dlist = 0, *dtemp;
  int entries, bytes, done = 0;
  char *data = 0, **out = 0;
  pid_t pid = 0;

  xsignal_flags(SIGUSR1, signal_P, SA_RESTART);
  xsignal_flags(SIGUSR2, signal_P, SA_RESTART);

  // POSIX requires that we never hit the ARG_MAX limit, even if we try to
  // with -s. POSIX also says we have to reserve 2048 bytes "to guarantee
  // that the invoked utility has room to modify its environment variables
  // and command line arguments and still be able to invoke another utility",
  // though obviously that's not really something you can guarantee.
  if (!FLAG(s)) TT.s = sysconf(_SC_ARG_MAX) - environ_bytes() - 4096;

  TT.delim = '\n'*!FLAG(0);

  // If no optargs, call echo.
  if (!toys.optc) {
    free(toys.optargs);
    *(toys.optargs = xzalloc(2*sizeof(char *)))="echo";
    toys.optc = 1;
  }

  // count entries
  for (entries = 0, bytes = -1; entries < toys.optc; entries++)
    bytes += strlen(toys.optargs[entries])+1+sizeof(char *)*!FLAG(s);
  if (bytes >= TT.s) error_exit("command too long");

  // Loop through exec chunks.
  while (data || !done) {
    TT.entries = 0;
    TT.bytes = bytes;
    if (TT.np) waitchild(WNOHANG*!(TT.np==TT.P||(!data && done)));
    if (toys.exitval==124) break;

    // Arbitrary number of execs, can't just leak memory each time...
    llist_traverse(dlist, llist_free_double);
    dlist = 0;
    free(out);
    out = 0;

    // Loop reading input
    for (;;) {
      // Read line
      if (!data) {
        size_t l = 0;

        if (getdelim(&data, &l, TT.delim, stdin)<0) {
          data = 0;
          done++;
          break;
        }
      }
      dlist_add(&dlist, data);
      // Count data used
      if (!(data = handle_entries(data, 0))) continue;
      if (data == (char *)2) done++;
      if ((unsigned long)data <= 2) data = 0;
      else data = xstrdup(data);

      break;
    }

    if (!TT.entries) {
      if (data) error_exit("argument too long");
      if (pid || FLAG(r)) break;
    }

    // Fill out command line to exec
    out = xzalloc((toys.optc+TT.entries+1)*sizeof(char *));
    memcpy(out, toys.optargs, toys.optc*sizeof(char *));
    TT.entries = 0;
    TT.bytes = bytes;
    dlist_terminate(dlist);
    for (dtemp = dlist; dtemp; dtemp = dtemp->next)
      handle_entries(dtemp->data, out+entries);

    if (FLAG(p) || FLAG(t)) {
      int i;

      for (i = 0; out[i]; ++i) fprintf(stderr, "%s ", out[i]);
      if (FLAG(p)) {
        fprintf(stderr, "?");
        if (!TT.tty) TT.tty = xfopen("/dev/tty", "re");
        if (!fyesno(TT.tty, 0)) continue;
      } else fprintf(stderr, "\n");
    }

    if (!(pid = XVFORK())) {
      close(0);
      xopen_stdio(FLAG(o) ? "/dev/tty" : "/dev/null", O_RDONLY|O_CLOEXEC);
      xexec(out);
    }
    TT.np++;
  }
  while (TT.np) waitchild(0);
  if (TT.tty) fclose(TT.tty);
}
