/* dumpleases.c - Dump the leases granted by udhcpd.
 *
 * Copyright 2013 Sandeep Sharma <sandeep.jack2756@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *

USE_DUMPLEASES(NEWTOY(dumpleases, ">0arf:[!ar]", TOYFLAG_USR|TOYFLAG_BIN))

config DUMPLEASES
  bool "dumpleases"
  default n
  help
    usage: dumpleases [-r|-a] [-f LEASEFILE]

    Display DHCP leases granted by udhcpd
    -f FILE,  Lease file
    -r        Show remaining time
    -a        Show expiration time
*/

#define FOR_dumpleases
#include "toys.h"

GLOBALS(
    char *file;
)

//lease structure
struct lease {
  uint32_t expires;
  uint32_t lease_nip;
  uint8_t lease_mac[6];
  char hostname[20];
  uint8_t pad[2]; //Padding
};

void dumpleases_main(void)
{
  struct in_addr addr;
  struct lease lease_struct;
  int64_t written_time , current_time, exp;
  int i, fd;

  if(!(toys.optflags & FLAG_f)) TT.file = "/var/lib/misc/dhcpd.leases"; //DEF_LEASE_FILE
  fd = xopenro(TT.file);
  xprintf("Mac Address       IP Address      Host Name           Expires %s\n", (toys.optflags & FLAG_a) ? "at" : "in");
  xread(fd, &written_time, sizeof(written_time));
  current_time = time(NULL);
  written_time = SWAP_BE64(written_time);
  if(current_time < written_time) written_time = current_time;

  while(sizeof(lease_struct) ==
      (readall(fd, &lease_struct, sizeof(lease_struct)))) {
    for (i = 0; i < 6; i++) printf(":%02x"+ !i, lease_struct.lease_mac[i]);

    addr.s_addr = lease_struct.lease_nip;
    lease_struct.hostname[19] = '\0';
    xprintf(" %-16s%-20s", inet_ntoa(addr), lease_struct.hostname );
    exp = ntohl(lease_struct.expires) + written_time;
    if (exp <= current_time) {
      xputs("expired");
      continue;
    }
    if (!(toys.optflags & FLAG_a)) {
      unsigned dt, hr, m;
      unsigned expires = exp - current_time;
      dt = expires / (24*60*60); expires %= (24*60*60);
      hr = expires / (60*60); expires %= (60*60);
      m = expires / 60; expires %= 60;
      if (dt) xprintf("%u days ", dt);
      xprintf("%02u:%02u:%02u\n", hr, m, (unsigned)expires);
    } else {
      fputs(ctime((const time_t*)&exp), stdout);
    }
  }
  xclose(fd);
}
