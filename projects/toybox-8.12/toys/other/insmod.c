/* insmod.c - Load a module into the Linux kernel.
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>

USE_INSMOD(NEWTOY(insmod, "<1", TOYFLAG_SBIN|TOYFLAG_NEEDROOT))

config INSMOD
  bool "insmod"
  default y
  help
    usage: insmod MODULE [OPTION...]

    Load the module named MODULE passing options if given.
*/

#include "toys.h"

void insmod_main(void)
{
  int fd = xopenro(*toys.optargs);
  int i, rc;

  i = 1;
  while (toys.optargs[i] &&
    strlen(toybuf) + strlen(toys.optargs[i]) + 2 < sizeof(toybuf))
  {
    strcat(toybuf, toys.optargs[i++]);
    strcat(toybuf, " ");
  }

  // finit_module doesn't work on stdin, so we fall back to init_module...
  rc = syscall(SYS_finit_module, fd, toybuf, 0);
  if (rc && (fd == 0 || errno == ENOSYS)) {
    off_t len = 0;
    char *path = !strcmp(*toys.optargs, "-") ? "/dev/stdin" : *toys.optargs;
    char *buf = readfileat(AT_FDCWD, path, NULL, &len);

    if (!buf) perror_exit("couldn't read %s", path);
    rc = syscall(SYS_init_module, buf, len, toybuf);
    if (CFG_TOYBOX_FREE) free(buf);
  }

  if (rc) perror_exit("failed to load %s", toys.optargs[0]);

  if (CFG_TOYBOX_FREE) close(fd);
}
