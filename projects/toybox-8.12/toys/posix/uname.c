/* uname.c - return system name
 *
 * Copyright 2008 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/uname.html

USE_UNAME(NEWTOY(uname, "paomvrns", TOYFLAG_BIN))
USE_ARCH(NEWTOY(arch, 0, TOYFLAG_USR|TOYFLAG_BIN))

config ARCH
  bool "arch"
  default y
  help
    usage: arch

    Print machine (hardware) name, same as uname -m.

config UNAME
  bool "uname"
  default y
  help
    usage: uname [-asnrvmo]

    Print system information.

    -s	System name
    -n	Network (domain) name
    -r	Kernel Release number
    -v	Kernel Version
    -m	Machine (hardware) name
    -a	All of the above (in order)

    -o	Userspace type
*/

#define FOR_uname
#define FORCE_FLAGS
#include "toys.h"

void uname_main(void)
{
  int i, needspace = 0;
  char *c;

  uname((void *)toybuf);
  if (!toys.optflags) toys.optflags = FLAG_s;
  for (i=0; i<6; i++) if (toys.optflags & ((1<<i)|FLAG_a)) {
    if (i==5) c = " Toybox"+!needspace;
    else {
      c = toybuf+sizeof(((struct utsname *)0)->sysname)*i;
      if (needspace++) *(--c)=' '; // Can't decrement first entry
    }
    xputsn(c);
  }
  if (FLAG(p)) xputsn(" unknown"+!needspace);
  xputc('\n');
}

void arch_main(void)
{
  toys.optflags = FLAG_m;
  uname_main();
}
