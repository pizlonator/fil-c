# Building dhcpcd

This attempts to document various ways of building dhcpcd for your
platform.
`./configure` is a POSIX shell script that works in a similar way
to GNU configure.
This works fine provided you don't force any exotic options down
which may or may not be silently discarded.

Some build time warnings are expected - the only platforms with zero
warnings are DragonFlyBSD and NetBSD.
It is expected that the platforms be improvded to support dhcpcd
better.
There maybe some loss of functionality, but for the most part,
dhcpcd can work around these deficiencies.

## Size is an issue
To compile small dhcpcd, maybe to be used for installation media where
size is a concern, you can use the `--small` configure option to enable
a reduced feature set within dhcpcd.
Currently this just removes non important options out of
`dhcpcd-definitions.conf`, the logfile option,
DHCPv6 Prefix Delegation and IPv6 address announcement *(to prefer an
address on another interface)*.
Other features maybe dropped as and when required.
dhcpcd can also be made smaller by removing the IPv4 or IPv6 stack:
  *  `--disable-inet`
  *  `--disable-inet6`

Or by removing the following features:
  *  `--disable-auth`
  *  `--disable-arp`
  *  `--disable-arping`
  *  `--disable-ipv4ll`
  *  `--disable-dhcp6`
  *  `--disable-privsep`

You can also move the embedded extended configuration from the dhcpcd binary
to an external file (LIBEXECDIR/dhcpcd-definitions.conf)
  *  `--disable-embedded`
If dhcpcd cannot load this file at runtime, dhcpcd will work but will not be
able to decode any DHCP/DHCPv6 options that are not defined by the user
in /etc/dhcpcd.conf. This does not really change the total on disk size.

## Cross compiling
If you're cross compiling you may need set the platform if OS is different
from the host.  
`--target=sparc-sun-netbsd5.0`

If you're building for an MMU-less system where fork() does not work, you
should `./configure --disable-fork`.
This also puts the `--no-background` flag on and stops the `--background` flag
from working.

## Default directories
You can change the default dirs with these knobs.
For example, to satisfy FHS compliance you would do this:
`./configure --libexecdir=/lib/dhcpcd dbdir=/var/lib/dhcpcd`

## Compile Issues
We now default to using `-std=c99`. For 64-bit linux, this always works, but
for 32-bit linux it requires either gnu99 or a patch to `asm/types.h`.
Most distros patch linux headers so this should work fine.
linux-2.6.24 finally ships with a working 32-bit header.
If your linux headers are older, or your distro hasn't patched them you can
set `CSTD=gnu99` to work around this.

ArchLinux presently sanitises all kernel headers to the latest version
regardless of the version for your CPU. As such, Arch presently ships a
3.12 kernel with 3.17 headers which claim that it supports temporary address
management and no automatic prefix route generation, both of which are
obviously false. You will have to patch support either in the kernel or
out of the headers (or dhcpcd itself) to have correct operation.

Linux netlink headers cause a sign conversion error.
I [submitted a patch](https://lkml.org/lkml/2019/12/17/680),
but as yet it's not upstreamed.

GLIBC ships an icmp6.h header which will result in signedness warnings.
Their [bug #22489](https://sourceware.org/bugzilla/show_bug.cgi?id=22489)
will solve this once it's actually applied.

## OS specific issues
Some BSD systems do not allow the manipulation of automatically added subnet
routes. You can find discussion here:
    http://mail-index.netbsd.org/tech-net/2008/12/03/msg000896.html
BSD systems where this has been fixed or is known to work are:
    NetBSD-5.0
    FreeBSD-10.0

Some BSD systems protect against IPv6 NS/NA messages by ensuring that the
source address matches a prefix on the recieved by a RA message.
This is an error as the correct check is for on-link prefixes as the
kernel may not be handling RA itself.
BSD systems where this has been fixed or is known to work are:
    NetBSD-7.0
    OpenBSD-5.0
    patch submitted against FreeBSD-10.0

Some BSD systems do not announce IPv6 address flag changes, such as
`IN6_IFF_TENTATIVE`, `IN6_IFF_DUPLICATED`, etc. On these systems,
dhcpcd will poll a freshly added address until either `IN6_IFF_TENTATIVE` is
cleared or `IN6_IFF_DUPLICATED` is set and take action accordingly.
BSD systems where this has been fixed or is known to work are:
    NetBSD-7.0

OpenBSD will always add it's own link-local address if no link-local address
exists, because it doesn't check if the address we are adding is a link-local
address or not.

Some BSD systems do not announce cached neighbour route changes based
on reachability to userland. For such systems, IPv6 routers will always
be assumed to be reachable until they either stop being a router or expire.
BSD systems where this has been fixed or is known to work are:
    NetBSD-7.99.3

Linux prior to 3.17 won't allow userland to manage IPv6 temporary addresses.
Either upgrade or don't allow dhcpcd to manage the RA,
so don't set either `ipv6ra_own` or `slaac private` in `dhcpcd.conf` if you
want to have working IPv6 temporary addresses.
SLAAC private addresses are just as private, just stable.

Linux SECCOMP is very dependant on libc vs kernel.
When libc is changed and uses a syscall that dhcpcd is unaware of,
SECCOMP may break dhcpcd.
When this happens you can configure dhcpcd with --disable-seccomp
so dhcpcd can use a POSIX resource limited sandbox with privilege separation
still. If you do this, please report the issue so that we can adjust the
SECCOMP filter so that dhcpcd can use SECCOMP once more.
Or convince the libc/kernel people to adpot something more maintainable
like FreeBSD's capsicum or OpenBSD's pledge.

## Init systems
We try and detect how dhcpcd should interact with system services at runtime.
If we cannot auto-detect how do to this, or it is wrong then
you can change this by passing shell commands to `--serviceexists`,
`--servicecmd` and optionally `--servicestatus` to `./configure` or overriding
the service variables in a hook.


## /dev management
Some systems have `/dev` management systems and some of these like to rename
interfaces. As this system would listen in the same way as dhcpcd to new
interface arrivals, dhcpcd needs to listen to the `/dev` management sytem
instead of the kernel. However, if the `/dev` management system breaks, stops
working, or changes to a new one, dhcpcd should still try and continue to work.
To facilitate this, dhcpcd allows a plugin to load to instruct dhcpcd when it
can use an interface. As of the time of writing only udev support is included.
You can disable this with `--without-dev`, or `without-udev`.
NOTE: in Gentoo at least, `sys-fs/udev` as provided by systemd leaks memory
`sys-fs/eudev`, the fork of udev does not and as such is recommended.

## crypto
dhcpcd ships with some cryptographic routines taken from various upstreams.
These are routinely monitored and try to be as up to date as possible.
You can optionally configure dhcpcd with `--with-openssl` to use libcrypto
to use these instead.
This is not enabled by default, even if libcrypto is found because libcrypto
generally lives in /usr and dhcpcd in /sbin which could be a separate
filesystem.

## Importing into another source control system
To import the full sources, use the import target.
To import only the needed sources and documentation, use the import-src
target.
Both targets support DESTDIR to set the installation directory,
if unset it defaults to `/tmp/dhcpcd-$VERSION`
Example: `make DESTDIR=/usr/src/contrib/dhcpcd import-src`


## Hooks
Not all the hooks in dhcpcd-hooks are installed by default.
By default we install `01-test`, `20-resolv.conf`and `30-hostname`.
The other hooks, `10-wpa_supplicant`, `15-timezone` and `29-lookup-hostname`
are installed to `$(datadir)/dhcpcd/hooks` by default and need to be
copied to `$(libexecdir)/dhcpcd-hooks` for use.
The configure program attempts to find hooks for systems you have installed.
To add more simply
`./configure -with-hook=ntp.conf`

If using resolvconf, the `20-resolv.conf` hook now requires a version with the
`-C` and `-c` options to deprecate and activate interfaces to support wireless
roaming (Linux) or carrier just drops (NetBSD).
If your resolvconf does not support this then you will see a warning
about an illegal option when the carrier changes, but things should still work.
In this instance the DNS information cannot be Deprecated and may not
be optimal for multi-homed hosts.
