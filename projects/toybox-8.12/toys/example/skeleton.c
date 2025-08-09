/* skeleton.c - Example program to act as template for new commands.
 *              (Although really, half the time copying hello.c is easier.)
 *
 * Copyright 2014 Rob Landley <rob@landley.net>
 *
 * See https://pubs.opengroup.org/onlinepubs/9799919799/utilities/
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/cmdbehav.html
 * See https://www.ietf.org/rfc/rfc3.txt
 * See https://man7.org/linux/man-pages/man1/intro.1.html
 * No standard.

// Accept many different kinds of command line argument (see top of lib/args.c)
// Demonstrate two commands in the same file (see www/documentation.html)

USE_SKELETON(NEWTOY(skeleton, "(walrus)(blubber):;(also):h(hlong):; g(glong): f(longf):;e@d*c#b:a", TOYFLAG_USR|TOYFLAG_BIN))
USE_SKELETON_ALIAS(NEWTOY(skeleton_alias, "b#dq", TOYFLAG_USR|TOYFLAG_BIN))

config SKELETON
  bool "skeleton"
  default n
  help
    usage: skeleton [-a] [-b STRING] [-c NUMBER] [-d LIST] [-e COUNT] [...]

    Template for new commands. You don't need this.

    When creating a new command, copy this file and delete the parts you
    don't need. Be sure to replace all instances of "skeleton" (upper and lower
    case) with your new command name.

    For simple commands, "hello.c" is probably a better starting point.

config SKELETON_ALIAS
  bool "skeleton_alias"
  default n
  help
    usage: skeleton_alias [-dq] [-b NUMBER]

    Example of a second command with different arguments in the same source
    file as the first. This allows shared infrastructure outside of lib/.
*/

#define FOR_skeleton
#include "toys.h"

// The union lets lib/args.c store arguments for either command.
// It's customary to put a space between argument variables and other globals.
GLOBALS(
  union {
    struct {
      char *b;
      long c;
      struct arg_list *d;
      long e;
      char *f, *g, *h, *also, *blubber;
    } s;
    struct {
      long b;
    } a;
  };

  int more_globals;
)

// Parse many different kinds of command line argument:
void skeleton_main(void)
{
  char **optargs;

  printf("Ran %s\n", toys.which->name);

  // Command line options parsing is done for you by lib/args.c called
  // from main.c using the optstring in the NEWTOY macros. Display results.
  if (toys.optflags) printf("flags=%llx\n", toys.optflags);
  if (FLAG(a)) printf("Saw a\n");
  if (FLAG(b)) printf("b=%s\n", TT.s.b);
  if (FLAG(c)) printf("c=%ld\n", TT.s.c);
  while (TT.s.d) {
    printf("d=%s\n", TT.s.d->arg);
    TT.s.d = TT.s.d->next;
  }
  if (TT.s.e) printf("e was seen %ld times\n", TT.s.e);
  if (TT.s.f) printf("f=%s\n", TT.s.f);
  if (TT.s.g) printf("g=%s\n", TT.s.g);
  if (TT.s.h) printf("h=%s\n", TT.s.h);
  for (optargs = toys.optargs; *optargs; optargs++)
    printf("optarg=%s\n", *optargs);
  if (FLAG(walrus)) printf("Saw --walrus\n");
  if (TT.s.blubber) printf("--blubber=%s\n", TT.s.blubber);

  printf("Other globals should start zeroed: %d\n", TT.more_globals);
}

// Switch gears from skeleton to skeleton_alias (swap FLAG macros).
#define FOR_skeleton_alias
#include "generated/flags.h"

void skeleton_alias_main(void)
{
  printf("Ran %s\n", toys.which->name);
  printf("flags=%llx\n", toys.optflags);

  // Note, this FLAG_b is a different bit position than the other FLAG_b,
  // and fills out a different variable of a different type.
  if (FLAG(b)) printf("b=%ld", TT.a.b);
}
