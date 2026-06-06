# Running Fil-C OpenSSH as Your System sshd

This document explains how to run the Fil-C build of OpenSSH at
`/opt/fil/sbin/sshd` instead of what comes on your system. On most Linux
systems, simply running `./setup.sh` will give you the option of making the
Fil-C OpenSSH service run by default instead of the memory-unsafe one that
your distribution shipped with. This document explains what that script did
as well as how to replicate its steps manually.

Fil-C is open source software provided AS IS, without warranty of any kind.
Nothing in this document is a guarantee of security or fitness for any
purpose. You should test the Fil-C OpenSSH server in your own environment
before relying on it, especially before pointing systemd at it on a machine
you care about. The procedure described below for switching systemd over to
a different sshd binary can lock you out of a remote machine if something
goes wrong, so read the section on testing and rollback carefully before you
start, and do not attempt it without console access or a second working way
in.

## Why use the Fil-C OpenSSH server

The Fil-C build of OpenSSH eliminates a broad class of memory safety bugs,
including things like the regreSSHion vulnerability (CVE-2024-6387). All
C files in OpenSSH and its dependencies (including OpenSSL, zlib, glibc,
pam, and other libraries) have been compiled with Fil-C. Fil-C catches all
memory safety issues at [runtime as Fil-C panics](https://fil-c.org/how).
The Fil-C port retains all of [OpenSSH's seccomp based sandbox
logic](https://fil-c.org/seccomp) and all of [OpenSSL's constant time crypto
hardening](https://fil-c.org/constant_time_crypto). Fil-C's performance
and memory usage overhead doesn't seem to impact OpenSSH under normal use
(even when doing things like large file transfers or X11 forwarding). The
included OpenSSH build supports common Linux sshd features like:

- pam
- seccomp
- systemd socket activation (but without a dependency on systemd)
- gssapi for authentication and kex

`/opt/fil/sbin/sshd` is based on OpenSSH 10.3p1 with small changes for Fil-C
compatibility plus Debian patches to support things like gssapi and systemd.

Using `/opt/fil/sbin/sshd` as a replacement has been tested on Ubuntu,
Rocky, Fedora, Debian, and Omarchy.

## What `setup.sh` already did for you

When you ran `./setup.sh`, the installer did a fair amount of work to make
`/opt/fil/sbin/sshd` run as a system service. It is worth understanding what
was actually changed on your system. If the setup script reported errors,
this document will try to help you fix those errors yourself.

If `/etc/ssh` did not exist, the installer created it. If the standard
configuration files (`ssh_config`, `sshd_config`, and `moduli`) were
missing, the installer copied them from
`/opt/fil/share/examples/ssh/`. Existing config files were left alone, so
if you already had a system OpenSSH installation the installer did not
touch your local customizations.

If any of the standard host keys (`ssh_host_rsa_key`,
`ssh_host_ecdsa_key`, and `ssh_host_ed25519_key`, along with their `.pub`
counterparts) were missing, the installer ran `/opt/fil/bin/ssh-keygen -A`
to generate them. Existing host keys were preserved. The installer also
checked the permissions on the host private keys. If any key was mode
`0640` and owned by the `ssh_keys` group, which is the old Red Hat
convention, the installer changed it to mode `0600`, which is the
convention that the Fil-C `/opt/fil/sbin/sshd` expects. Keys with
stranger permission patterns were left alone and reported as warnings,
since the installer cannot know whether those permissions are intentional.
Note that even on Red Hat systems that use `0640` permissions by default,
changing them to `0600` does not break the stock sshd.

The `sshd` privilege separation user and group were created if they were
missing. The user is created with `/opt/fil/var/lib/sshd` as its home
directory and `/bin/false` as its shell, which is the standard non-login
configuration. The installer also creates `/opt/fil/var/lib/sshd` itself
if it does not exist, since sshd's privilege separation can fail at
runtime if that directory is missing. If the user or group already
existed, the installer did not touch it.

Unless you used the `-u --full-setup` options, `./setup.sh` would have
prompted you before making any changes to your system's SSH configuration.
If you did not see any prompt, this means that the script determined that
your existing setup was already sufficient. If you used `-u` without
`--full-setup`, then the installer would not have done anything to your
SSH configuration.

The installer attempted to apply persistent SELinux labels for the
Fil-C binaries and runtime libraries under `/opt/fil`. The logic is
conservative. If SELinux is not active in the kernel, the installer
does nothing. If SELinux is active but the persistent labeling tools
(`semanage` and `restorecon`) are not installed, the installer warns
you and asks you to install the SELinux user-space tools and re-run
the SSH setup phase with `./setup.sh --ssh-setup` rather than trying
to limp along with `chcon` alone (which does not survive a future
`restorecon` or a full SELinux relabel).

When SELinux is active and the tools are present, the installer
registers persistent file context entries with `semanage fcontext -a`
and then applies them with `restorecon`. There are four label rules,
each one validated against a system reference path before the
installer commits to anything. The reference type must match what the
installer recognizes for that role; if it does not, the installer
refuses to label that target and prints what type it expected. The
four rules are: `/opt/fil/sbin/sshd` should match the type of
`/usr/sbin/sshd` (expected `sshd_exec_t`); `/opt/fil/sbin/unix_chkpwd`
should match `/usr/sbin/unix_chkpwd` (expected `chkpwd_exec_t`), which
is needed for PAM password checking from sshd's sandboxed domain;
`/opt/fil/lib/ld-yolo-x86_64.so` should match the system dynamic
loader (expected `ld_so_t`), with the installer probing the common
loader paths (`/lib64/ld-linux-x86-64.so.2`, the Debian/Ubuntu
multiarch path, and so on) to find one; and the shared libraries
under `/opt/fil/lib` should match the type that the system uses for
libc (expected `lib_t`), again with several candidate paths probed.

The shared-library rule is registered as a regex
(`/opt/fil/lib/.+\.so(\..+)?`), and the loader rule is registered as
a literal path, so SELinux's specificity matching picks the loader
rule for the loader and the library rule for everything else. If any
one rule fails (for example, because the system reference has an
unrecognized type), the installer skips that rule and continues with
the others, but it will not declare the SELinux setup successful
overall, and it will refuse to proceed with the systemd setup that
comes after, since starting sshd with partial labels can produce
hard-to-debug failures. After fixing the underlying problem you can
re-run the labeling step with `./setup.sh --ssh-setup`, which is
idempotent: it will report which labels are already in place and only
apply the ones that need changing.

Finally, if systemd is running on your machine and the earlier SELinux
and SSH configuration steps completed cleanly, the installer wired
`/opt/fil/sbin/sshd` into systemd as your system sshd. Unless you used
`-u --full-setup`, you should have received a prompt about systemd
configuration changes. If you used `-u` without `--full-setup`, then no
systemd changes were made.

The exact behavior depends on what was already on the machine. If there
was a stock `sshd.service` or `ssh.service` from your distribution, the
installer wrote a drop-in at `/etc/systemd/system/<unit>.d/fil-c.conf`
that redirects each `ExecStartPre`, `ExecStart`, and `ExecReload` line
mentioning `/usr/sbin/sshd` to `/opt/fil/sbin/sshd`, leaving every other
directive (`EnvironmentFile`, `KillMode`, hardening flags, sandboxing
directives, and so on) untouched. The installer then ran `systemctl
daemon-reload`, restarted the service, and verified that the running
unit was using `/opt/fil/sbin/sshd`. If neither `sshd.service` nor
`ssh.service` existed at all, the installer wrote a minimal
`/etc/systemd/system/sshd.service` that runs `/opt/fil/sbin/sshd -D`,
enabled it to start on boot, and started it. In both cases the installer
prompted you before doing any of this unless you used `--unattended
--full-setup`, and it skipped the step entirely if SELinux labeling or
SSH configuration had not completed cleanly. You can re-attempt the
systemd setup at any time by running `./setup.sh --ssh-setup`, which is
idempotent: it detects whether the systemd unit is already pointing at
`/opt/fil/sbin/sshd` and, if so, just restarts the service if it is
running rather than rewriting anything.

## Switching systemd over to `/opt/fil/sbin/sshd` by hand

The rest of this section describes the systemd setup procedure manually,
for readers who want to understand what `./setup.sh` did, who want to do
the setup by hand instead of letting the installer do it, or who need to
recover from a situation where the installer's automatic attempt did not
work. If `./setup.sh` already wired sshd into systemd successfully and
you can log in through `/opt/fil/sbin/sshd`, you do not need to do any
of this.

Before you change anything, understand the lockout risk. If you point
systemd at a misconfigured sshd, the service may fail to start, or it
may start but refuse logins. On a remote machine without console or
out-of-band access, that situation can leave you unable to get back in.
Before starting this procedure, make sure you have a way to recover if
sshd stops accepting logins. That can be a serial or KVM console, a
hypervisor console, a separate management interface, physical access,
or at minimum a second already-authenticated SSH session that you keep
open throughout the switchover. If none of these is available, do not
do this on a production machine. Try it on a disposable VM first.

To run the Fil-C build as your actual system sshd, you need to point
your existing systemd unit at the new binary, restart the service, and
confirm that you can still log in before you log out. The procedure
varies slightly by distribution because their stock sshd unit files
differ, and that variation is exactly what makes a one-size-fits-all
override file dangerous. Do not paste a generic override from somewhere
on the internet. Read your own distribution's unit first and write the
override from that.

On most distributions the unit is called `sshd.service` (Red Hat, Rocky,
Fedora, and CentOS) or `ssh.service` (Debian and Ubuntu). Find out which
one your system uses by running `systemctl status ssh` or `systemctl
status sshd`. Then read the current unit with `systemctl cat`:

```bash
systemctl cat sshd.service   # or ssh.service
```

Look at the `[Service]` section. Some of its `ExecStartPre=`, `ExecStart=`,
and `ExecReload=` lines will mention `/usr/sbin/sshd`. The override needs
to redirect each of those lines at `/opt/fil/sbin/sshd`, and leave every
other directive (`EnvironmentFile=`, `Type=`, `KillMode=`, `Restart=`,
hardening flags, and so on) untouched so that the distribution's intent
is preserved. The way you redirect an `ExecStart`-family line in a
drop-in is to write the line twice: first the bare key with an empty
value to clear the inherited setting, and then the new value with
`/usr/sbin/sshd` replaced by `/opt/fil/sbin/sshd`. Lines whose value does
not mention `/usr/sbin/sshd` (such as
`ExecReload=/bin/kill -HUP $MAINPID`) are left alone.

A worked example for an Ubuntu `ssh.service` whose relevant lines look
like this:

```ini
ExecStartPre=/usr/sbin/sshd -t
ExecStart=/usr/sbin/sshd -D $SSHD_OPTS
ExecReload=/usr/sbin/sshd -t
ExecReload=/bin/kill -HUP $MAINPID
```

The override should look like this:

```ini
[Service]
ExecStartPre=
ExecStartPre=/opt/fil/sbin/sshd -t
ExecStart=
ExecStart=/opt/fil/sbin/sshd -D $SSHD_OPTS
ExecReload=
ExecReload=/opt/fil/sbin/sshd -t
```

The `ExecReload=/bin/kill ...` line is not in the override because the
original does not mention `/usr/sbin/sshd`. Both the original and any
hardening directives, environment files, and socket bindings stay in
effect through the layered distribution unit.

A worked example for a Rocky `sshd.service` whose relevant lines look
like this:

```ini
ExecStart=/usr/sbin/sshd -D $OPTIONS
ExecReload=/bin/kill -HUP $MAINPID
```

The override is correspondingly shorter:

```ini
[Service]
ExecStart=
ExecStart=/opt/fil/sbin/sshd -D $OPTIONS
```

Note that the Rocky example uses `$OPTIONS` rather than `$SSHD_OPTS`,
because that is what the Rocky unit's `EnvironmentFile=` populates. Do
not change the variable name in the override. The right value to use is
whatever the original unit used.

You can install an override either by running `systemctl edit
sshd.service` (or `ssh.service`) and editing the file the editor opens,
or by writing the drop-in directly to
`/etc/systemd/system/sshd.service.d/fil-c.conf` (substituting
`ssh.service.d` on Debian/Ubuntu). The `systemctl edit` approach is
slightly safer because it asks systemd to reload itself for you on exit.
Different versions of systemd populate the file differently. Some open
it empty, others open it with the current unit content as commented-out
lines for reference. Either way, the content above is what should end up
in it.

The empty `ExecStart=` line is important. It clears the value inherited
from the distribution unit before the new value is appended. Without it
you would end up with both binaries listed and systemd would refuse to
start the service.

Before restarting the service, validate the configuration with
`/opt/fil/sbin/sshd -t`. This catches typos in `sshd_config` and missing
host keys without taking the service down. It does not catch every
runtime failure. PAM stack mismatches, SELinux denials, AppArmor
profiles, systemd sandboxing directives inherited from the distribution
unit, seccomp restrictions, capability mismatches, and port conflicts
will only show up when sshd actually tries to start. A clean `-t` is a
necessary check, not a sufficient one.

Keep one root SSH session open while you do the switchover. Open a
second SSH session from somewhere else to confirm that logins still
work before you close the first session. Under normal conditions
restarting `sshd.service` does not interrupt existing SSH sessions,
because the distribution units use `KillMode=process` (or its
equivalent), which kills only the main listener and leaves
already-connected per-session children alone. That said, this is a
configuration choice in the unit, not a guarantee of the protocol, so
you should still keep a second session open during the switchover. If
something goes wrong, you can usually recover from the still-open
session by running `systemctl revert sshd.service` (or `ssh.service`)
and restarting the service. Note that revert only removes the override
you added. It does not undo the host keys, the SELinux labels, the
`sshd` user, or any other changes that `setup.sh` made earlier. If you
need to fully unwind those, you will have to do it manually.

Reload systemd and restart the service:

```bash
sudo systemctl daemon-reload
sudo systemctl restart sshd   # or 'ssh' on Debian/Ubuntu
sudo systemctl status sshd
```

If the status output shows `active (running)` and the `ExecStart=` line
points at `/opt/fil/sbin/sshd`, the systemd unit is now running the
Fil-C build of OpenSSH. Open a second terminal and test a login before
doing anything else.

If the service fails to start on a system that uses SELinux, the most
likely cause is that one or more of the Fil-C files under `/opt/fil` is
missing the SELinux label it needs. The installer tries to handle this
automatically by registering persistent file context entries for the
sshd binary, the `unix_chkpwd` PAM helper, the Fil-C dynamic loader,
and the Fil-C shared libraries, but if it printed warnings about
unrecognized reference types, or could not find `semanage` and
`restorecon`, you will need to apply the labels by hand. The four
rules the installer would have registered are listed below. Replace
the type names with whatever your distribution actually uses if they
differ from these defaults; you can read the system reference types
off the output of `ls -Z` on each reference path.

```bash
sudo semanage fcontext -a -t sshd_exec_t '/opt/fil/sbin/sshd'
sudo semanage fcontext -a -t chkpwd_exec_t '/opt/fil/sbin/unix_chkpwd'
sudo semanage fcontext -a -t ld_so_t '/opt/fil/lib/ld-yolo-x86_64\.so'
sudo semanage fcontext -a -t lib_t '/opt/fil/lib/.+\.so(\..+)?'
sudo restorecon -v /opt/fil/sbin/sshd /opt/fil/sbin/unix_chkpwd
sudo restorecon -R -v /opt/fil/lib
```

The order of the loader and library rules matters less than it looks,
because SELinux matches the most specific entry rather than the
first-registered one, but registering the loader rule (which uses a
literal path) before the library rule (which uses a regex) is a
defensive convention.

If `semanage` is not installed, you can install it with `dnf install
policycoreutils-python-utils` on Rocky and Fedora, or the equivalent
package on your distribution. Using `chcon` instead of `semanage` is
also possible but is not persistent: a future `restorecon` or full
SELinux relabel will revert the labels because the policy database
will not know about `/opt/fil`.

After the labeling change, restart the service again and check
`journalctl -u sshd` if it still misbehaves. The journal will usually
tell you which file or socket SELinux denied access to, which makes it
easier to track down any remaining policy mismatch. On distributions
that use AppArmor instead of SELinux (Debian and Ubuntu by default)
there is an analogous risk that the distribution's AppArmor profile for
sshd does not cover `/opt/fil/sbin/sshd`. If you see denials in
`dmesg` or the journal that mention `apparmor=`, you will need to
either disable the relevant profile or adjust it to cover the new path.

Finally, if you want to roll back at any point, `systemctl revert
sshd.service` deletes the override you added and returns the unit to
the distribution sshd, and a subsequent `systemctl restart sshd` brings
it back online. As noted above, revert only undoes the override itself.
The other changes that `setup.sh` made, including host keys, SELinux
labels, and the `sshd` user account, are not removed by revert and you
will need to clean those up by hand if you want a fully clean
uninstall.
