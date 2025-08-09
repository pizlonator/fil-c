/* hostname.c - Get/Set the hostname
 *
 * Copyright 2012 Andre Renaud <andre@bluewatersys.com>
 *
 * http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/hostname.html

USE_HOSTNAME(NEWTOY(hostname, ">1bdsfF:[!bdsf]", TOYFLAG_BIN))
USE_DNSDOMAINNAME(NEWTOY(dnsdomainname, ">0", TOYFLAG_BIN))

config HOSTNAME
  bool "hostname"
  default y
  help
    usage: hostname [-bdsf] [-F FILENAME] [newname]

    Get/set the current hostname.

    -b	Set hostname to 'localhost' if otherwise unset
    -d	Show DNS domain name (no host)
    -f	Show fully-qualified name (host+domain, FQDN)
    -F	Set hostname to contents of FILENAME
    -s	Show short host name (no domain)

config DNSDOMAINNAME
  bool "dnsdomainname"
  default y
  help
    usage: dnsdomainname

    Show domain this system belongs to (same as hostname -d).
*/

#define FOR_hostname
#define FORCE_FLAGS
#include "toys.h"

GLOBALS(
  char *F;
)

void hostname_main(void)
{
  char *hostname = toybuf, *dot;
  struct hostent *h;

  gethostname(toybuf, sizeof(toybuf)-1);
  if (TT.F && (hostname = xreadfile(TT.F, 0, 0))) {
    if (!*chomp(hostname)) {
      if (CFG_TOYBOX_FREE) free(hostname);
      if (!FLAG(b)) error_exit("empty '%s'", TT.F);
      hostname = 0;
    }
  } else hostname  = (FLAG(b) && !*toybuf) ? "localhost" : *toys.optargs;

  // Setting?
  if (hostname) {
    if (sethostname(hostname, strlen(hostname)))
      perror_exit("set '%s'", hostname);
    return;
  }

  // We only do the DNS lookup for -d and -f.
  if (FLAG(d) || FLAG(f)) {
    if (!(h = gethostbyname(toybuf)))
      error_exit("gethostbyname: %s", hstrerror(h_errno));
    snprintf(toybuf, sizeof(toybuf), "%s", h->h_name);
  }
  dot = toybuf+strcspn(toybuf, ".");
  if (FLAG(s)) *dot = 0;
  xputs(FLAG(d) ? dot+1 : toybuf);
}

void dnsdomainname_main(void)
{
  toys.optflags = FLAG_d;
  hostname_main();
}
