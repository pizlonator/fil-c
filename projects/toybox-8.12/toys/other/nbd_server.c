/* nbd-server.c - network block device server
 *
 * Copyright 2022 Rob Landley <rob@landley.net>
 *
 * Not in SUSv4.
 *
 * See https://github.com/NetworkBlockDevice/nbd/blob/master/doc/proto.md

// Work around dash in name trying to put - in function name.
USE_NBD_SERVER(NEWTOY(nbd_server, "<1>1r", 0))
USE_NBD_SERVER(OLDTOY(nbd-server, nbd_server, TOYFLAG_USR|TOYFLAG_BIN))

config NBD_SERVER
  bool "nbd-server"
  default y
  help
    usage: nbd-server [-r] FILE

    Serve a Network Block Device from FILE on stdin/out (ala inetd).

    -r	Read only export
*/

// TODO: -r, block size, exit signal?

#define FOR_nbd_server
#include "toys.h"

static int copy_loop(int from, int to, unsigned len)
{
  int try, rc = 0;

  errno = 0;
  while (len) {
    xreadall(from, toybuf, try = len>4096 ? 4096 : len);
    if (!rc && try != writeall(to, toybuf, try)) rc = errno;
    len -= try;
  }

  return rc;
}

void nbd_server_main(void)
{
  unsigned long long *ll = (void *)toybuf, offset, handle;
  unsigned short *ss = (void *)toybuf;
  unsigned *uu = (void *)toybuf, type, length;
  int fd = xopen(*toys.optargs, O_RDWR*!FLAG(r));

  type = 1;
  setsockopt(0, IPPROTO_TCP, TCP_NODELAY, &type, sizeof(int));

  // Send original recipe negotiation, with device length and flags
  memcpy(toybuf, "NBDMAGIC\x00\x00\x42\x02\x81\x86\x12\x53", 16);
  ll[2] = SWAP_BE64(fdlength(fd));
  uu[6] = SWAP_BE32(5+2*FLAG(r)); // has flags, can flush, maybe read only
  xwrite(1, toybuf, 152);

  // Simple loop, handles one request at a time with "simple" reply.
  for (;;) {
    // Fetch request into toybuf
    xreadall(0, toybuf, 28);
    if (SWAP_BE32(*uu) != 0x25609513) break;
    type = SWAP_BE16(ss[3]);
    handle = SWAP_BE64(ll[1]);
    offset = SWAP_BE64(ll[2]);
    length = SWAP_BE32(uu[6]);

    // type 0 = read, 1 = write, 2 = disconnect, 3 = flush
    if (type==2 || type>3) break;  // disconnect
    if (type==3) { // flush
      if (fdatasync(fd)) uu[1] = SWAP_BE32(errno);
    } else {
      xlseek(fd, offset, SEEK_SET);
      if (type==1) { // write
        uu[1] = copy_loop(0, fd, length);
        ll[1] = SWAP_BE64(handle);
      } else uu[1] = 0; // read never reports errors because send header first
    }

    // Simple reply in toybuf (handle stays put)
    *uu = SWAP_BE32(0x67446698);
    xwrite(1, toybuf, 16);

    // Append read payload
    if (!type) if (copy_loop(fd, 1, length)) break;
  }
}
