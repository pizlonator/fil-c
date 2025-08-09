/* getopt.c - Parse command-line options
 *
 * Copyright 2019 The Android Open Source Project
 *
 * See https://man7.org/linux/man-pages/man1/getopt.1.html

USE_GETOPT(NEWTOY(getopt, "^a(alternative)n:(name)o:(options)l*(long)(longoptions)Tu", TOYFLAG_USR|TOYFLAG_BIN))

config GETOPT
  bool "getopt"
  default y
  help
    usage: getopt [-aTu] [-lo OPTIONS] [-n NAME] [OPTIONS] ARG...

    Outputs command line with recognized OPTIONS character arguments moved to
    front, then "--", then non-option arguments. Returns 1 if unknown options.
    OPTIONS followed by : take an argument, or :: for optional arguments (which
    must be attached, ala -xblah or --long=blah).

    -a	Allow long options starting with a single -
    -l	Long OPTIONS (repeated or comma separated)
    -n	Command NAME for error messages
    -o	Short OPTIONS (instead of using first argument)
    -T	Test whether this is a modern getopt
    -u	Unquoted output (default if no other options set)

    Example:
      $ getopt -l long:,arg:: abc command --long -b there --arg
      --long '-b' --arg '' -- 'command' 'there'
*/

#define FOR_getopt
#include "toys.h"
#include <getopt.h> // Everything else uses lib/args.c

GLOBALS(
  struct arg_list *l;
  char *o, *n;
)

static void out(char *s)
{
  if (FLAG(u)) xprintf(" %s", s);
  else {
    xputsn(" '");
    for (; *s; s++) {
      if (*s == '\'') xputsn("'\\''");
      else putchar(*s);
    }
    putchar('\'');
  }
}

static char *parse_long_opt(void *data, char *str, int len)
{
  struct option **lopt_ptr = data, *lopt = *lopt_ptr;

  // Trailing : or :: means this option takes a required or optional argument.
  // no_argument = 0, required_argument = 1, optional_argument = 2.
  for (lopt->has_arg = 0; len>0 && str[len-1] == ':'; lopt->has_arg++) len--;
  if (!len || lopt->has_arg>2) return str;

  lopt->name = xstrndup(str, len);
  (*lopt_ptr)++;

  return 0;
}

void getopt_main(void)
{
  int argc = toys.optc+1, i = 0, j = 0, ch;
  char **argv = xzalloc(sizeof(char *)*(argc+1));
  struct option *lopts = xzalloc(sizeof(struct option)*argc), *lopt = lopts;

  if (FLAG(T)) {
    toys.exitval = 4;
    return;
  }

  comma_args(TT.l, &lopt, "bad -l", parse_long_opt);
  argv[j++] = TT.n ? : "getopt";

  if (!FLAG(o)) {
    TT.o = toys.optargs[i++];
    argc--;
  }
  if (!TT.o) error_exit("no OPTSTR");
  if (!toys.optflags) toys.optflags = FLAG_u;

  while (i<toys.optc) argv[j++] = toys.optargs[i++];

  // BSD getopt doesn't honor argv[0] (for -n), so handle errors ourselves.
  opterr = 0;
  optind = 1;
  while ((ch = (FLAG(a) ? getopt_long_only : getopt_long)(argc, argv, TT.o,
          lopts, &i)) != -1) {
    if (ch == '?') {
      fprintf(stderr, "%s: invalid option '%c'\n", *argv, optopt);
      toys.exitval = 1;
    } else if (!ch) {
      xprintf(" --%s", lopts[i].name);
      if (lopts[i].has_arg) out(optarg ? : "");
    } else {
      xprintf(" -%c", ch);
      if (optarg) out(optarg);
    }
  }

  xputsn(" --");
  for (; optind<argc; optind++) out(argv[optind]);
  putchar('\n');
}
