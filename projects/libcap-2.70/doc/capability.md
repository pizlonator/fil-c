# Notes concerning wider use of capabilities

## Overview

**NOTE** These notes were added to the libcap package in
libcap-1.03. They pre-date file capability support, but fully
anticipate it. They are some thoughts on how to restructure a system
to better leverage capability support. I've updated them to render as
an `.md` formatted file.

As of Linux 2.2.0, the power of the superuser has been partitioned
into a set of discrete capabilities (in other places, these
capabilities are know as privileges).

The contents of the libcap package are a library and a number of
simple programs that are intended to show how an application/daemon
can be protected (with wrappers) or rewritten to take advantage of
this fine grained approach to constraining the danger to your system
from programs running as 'root'.

## Notes on securing your system

### Adopting a role approach to system security

Changing all of the system binaries and directories to be owned by
some user that cannot log on. You might like to create a user with
the name 'system' who's account is locked with a '*' password. This
user can be made the owner of all of the system directories on your
system and critical system binaries too.

Why is this a good idea? In a simple case, the `CAP_FOWNER` capability
is required for the superuser to delete files owned by a non-root user
in a _sticky-bit_ protected non-root owned directory. Thus, the sticky
bit can help you protect the `/lib/` directory from a compromized
daemon where the directory and the files it contains are owned by the
system user. It can be protected to ensure that the daemon is not
running with the `CAP_FOWNER` capability...

### Limiting the damage

If your daemon only needs to be setuid-root in order to bind to a low
numbered port. You should restrict it to only having access to the
`CAP_NET_BIND_SERVICE` capability. Coupled with not having any files
on the system owned by root, it becomes significantly harder for such
a daemon to damage your system.

Note, you should think of this kind of trick as making things harder
for a potential attacker to exploit a hole in a daemon of this
type. Being able to bind to any privileged port is still a formidable
privilege and can lead to difficult but _interesting_
man-in-the-middle attacks -- hijack the telnet port for example and
masquerade as the login program... Collecting passwords for another
day.

### The /proc/ filesystem

This Linux-specific directory tree holds most of the state of the
system in a form that can sometimes be manipulated by file
read/writes.  Take care to ensure that the filesystem is not mounted
with uid=0, since root (with no capabilities) would still be able to
read sensitive files in the `/proc/` tree - `kcore` for example.

[Patch is available for 2.2.1 - I just wrote it!]
