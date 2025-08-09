/* nbd-client.c - network block device client
 *
 * Copyright 2010 Rob Landley <rob@landley.net>
 *
 * Not in SUSv4.

// This little dance is because a NEWTOY with - in the name tries to do
// things like prototype "nbd-client_main" which isn't a valid symbol. So
// we hide the underscore name and OLDTOY the name we want.
USE_NBD_CLIENT(NEWTOY(nbd_client, "<3>3b#<1>4294967295=4096ns", 0))
USE_NBD_CLIENT(OLDTOY(nbd-client, nbd_client, TOYFLAG_USR|TOYFLAG_BIN))

config NBD_CLIENT
  bool "nbd-client"
  default y
  help
    usage: nbd-client [-ns] [-b BLKSZ] HOST PORT DEVICE

    -b	Block size (default 4096)
    -n	Do not daemonize
    -s	nbd swap support (lock server into memory)
*/

#define FOR_nbd_client
#include "toys.h"
#include <linux/nbd.h>

GLOBALS(
  long b;

  int nbd;
)

static void sig_cleanup(int catch)
{
  // Flush on the way out
  ioctl(TT.nbd, NBD_CLEAR_QUE);
  ioctl(TT.nbd, NBD_CLEAR_SOCK);
  _exit(catch ? 128+catch : 0);
}

void nbd_client_main(void)
{
  int sock = -1, flags, temp;
  unsigned long timeout = 0;
  char *host=toys.optargs[0], *port=toys.optargs[1], *device=toys.optargs[2];
  unsigned long long devsize;

  // Daemonize in a nommu-friendly way, but retain stderr
  if (toys.stacktop && !FLAG(n)) {
    dup2(2, 222);
    xvdaemon();
  }
  dup2(222, 2);
  close(222);

  TT.nbd = xopen(device, O_RDWR);
  xsignal(SIGINT, sig_cleanup);
  xsignal(SIGTERM, sig_cleanup);

  for (;;) {
    // Find and connect to server
    sock = xconnectany(xgetaddrinfo(host, port, AF_UNSPEC, SOCK_STREAM, 0, 0));
    temp = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &temp, sizeof(int));

    // Read login data
    xreadall(sock, toybuf, 152);
    if (smemcmp(toybuf, "NBDMAGIC\x00\x00\x42\x02\x81\x86\x12\x53", 16))
      error_exit("bad login %s:%s", host, port);
    devsize = SWAP_BE64(*(unsigned long long *)(toybuf+16));
    flags = SWAP_BE32(*(int *)(toybuf+24));

    // Use 4k block size
    ioctl(TT.nbd, NBD_SET_BLKSIZE, TT.b);
    ioctl(TT.nbd, NBD_SET_SIZE_BLOCKS, devsize/TT.b); // rounds down
    ioctl(TT.nbd, NBD_CLEAR_SOCK);

    // Locally respect read only exports
    flags = (flags>>1)&1;
    xioctl(TT.nbd, BLKROSET, &flags);

    if (timeout && ioctl(TT.nbd, NBD_SET_TIMEOUT, timeout)<0) break;
    if (ioctl(TT.nbd, NBD_SET_SOCK, sock) < 0) break;

    if (FLAG(s)) mlockall(MCL_CURRENT|MCL_FUTURE);

    // Open the device to force reread of the partition table.
    if (!CFG_TOYBOX_FORK || !xfork()) {
      char *s = strrchr(device, '/');
      int i;

      // Give device up to 10 seconds to come up
      sprintf(toybuf, "/sys/block/%.32s/pid", s ? s+1 : device);
      for (i=0; i<100; i++) {
        if (access(toybuf, F_OK)) break;
        msleep(100);
      }
      close(open(device, O_RDONLY));
      if (CFG_TOYBOX_FORK) _exit(0);
    }

    // Process NBD requests until further notice.

    if (ioctl(TT.nbd, NBD_DO_IT)>=0 || errno==EBADR) break;
    close(sock);
    ioctl(TT.nbd, NBD_CLEAR_QUE);
  }

  // Flush queue and exit.
  if (CFG_TOYBOX_FREE) close(TT.nbd);
}
