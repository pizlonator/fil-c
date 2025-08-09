/* tunctl.c - Control tap/tun network devices.
 *
 * Copyright 2016 Rob Landley <rob@landley.net>
 *
 * See http://kernel.org/doc/Documentation/networking/tuntap.txt
 *
 * This is useful for things like "kvm -netdev tap" and containers.
 * See https://landley.net/lxc/02-networking.html for example usage.
 *
 * TODO: bridge mode
 *  -b	bridge daemon (forwards packets between NAME and NAME2 interfaces)


USE_TUNCTL(NEWTOY(tunctl, "<1>1t|d|u:T[!td]", TOYFLAG_USR|TOYFLAG_BIN))

config TUNCTL
  bool "tunctl"
  default y
  help
    usage: tunctl [-dtT] [-u USER] NAME

    Create and delete tun/tap virtual ethernet devices.

    -T	Use tap (ethernet frames) instead of tun (ip packets)
    -d	Delete tun/tap device
    -t	Create tun/tap device
    -u	Set owner (user who can read/write device without root access)
*/

#define FOR_tunctl
#include "toys.h"
#include <linux/if_tun.h>

GLOBALS(
  char *u;
)

void tunctl_main(void)
{
  struct ifreq *ifr = (void *)toybuf;
  uid_t u = TT.u ?  xgetuid(TT.u) : 0;
  int fd = xopen("/dev/net/tun", O_RDWR);

  // Associate filehandle with device
  ifr->ifr_flags = (FLAG(T) ? IFF_TUN : IFF_TAP)|IFF_NO_PI;
  strncpy(ifr->ifr_name, *toys.optargs, sizeof(ifr->ifr_name));
  xioctl(fd, TUNSETIFF, toybuf);

  if (FLAG(t)) {
    xioctl(fd, TUNSETPERSIST, (void *)1);
    xioctl(fd, TUNSETOWNER, (void *)(long)u);
  } else xioctl(fd, TUNSETPERSIST, (void *)0);
}
