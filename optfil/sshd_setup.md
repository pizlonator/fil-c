# Running Fil-C OpenSSH as Your System sshd

This document explains why you might want to replace your system's stock
OpenSSH server with the Fil-C build at `/opt/fil/sbin/sshd`, what the
`setup.sh` installer has already done for you to make that possible, and what
you still need to do to wire it up under systemd.

## Why use the Fil-C OpenSSH server

The Fil-C build of OpenSSH is a strict upgrade in terms of security, and it
costs you nothing on the cryptographic side.

It is better because every line of C code in `sshd`, every line of C code in
OpenSSL underneath it, and every line of C code in the rest of the runtime
that supports it has been compiled with Fil-C and is therefore memory safe.
Out-of-bounds reads and writes, use-after-free bugs, type confusion, and
pointer races are all caught and turned into a clean abort rather than a
silent compromise. As a concrete example, the regreSSHion vulnerability
(CVE-2024-6387) was a use-after-free / signal-handler race in OpenSSH that
allowed a remote unauthenticated attacker to execute arbitrary code as root.
Under Fil-C the bug is still there in the source, but it cannot be
weaponized: the unsafe access aborts before it can corrupt memory. The same
is true of the entire class of bugs that has historically dominated OpenSSH
and OpenSSL CVEs.

It is not worse in any way that matters for cryptography. The Fil-C build
uses the same OpenSSH and OpenSSL source code as upstream, which means the
same protocols, the same key exchange algorithms, the same ciphers, the same
MACs, and the same defaults. Constant-time crypto routines stay
constant-time: Fil-C's instrumentation does not introduce data-dependent
branches or memory accesses into the cryptographic primitives, so the same
side-channel hardening that upstream OpenSSL relies on still holds. You get
the same wire protocol, interoperating with any other OpenSSH client or
server, with the memory safety bugs sandboxed away.

There is a performance cost, since Fil-C code runs slower than ordinary C
code, but for an interactive login server that cost is invisible in normal
use.

## What `setup.sh` already did for you

When you ran `./setup.sh`, the installer did a fair amount of work to make
`/opt/fil/sbin/sshd` ready to run as a system service. It is worth
understanding what was actually changed on your system so you know what is
already taken care of and what is not.

If `/etc/ssh` did not exist, the installer created it. If the standard
configuration files (`ssh_config`, `sshd_config`, and `moduli`) were
missing, the installer copied them from
`/opt/fil/share/examples/ssh/`. Existing config files were left alone, so if
you already had a system OpenSSH installation the installer did not touch
your local customizations.

If any of the standard host keys (`ssh_host_rsa_key`, `ssh_host_ecdsa_key`,
and `ssh_host_ed25519_key`, along with their `.pub` counterparts) were
missing, the installer ran `/opt/fil/bin/ssh-keygen -A` to generate them.
Existing host keys were preserved. The installer also checked the
permissions on the host private keys. If any key was mode `0640` and owned
by the `ssh_keys` group, which is the old Red Hat convention, the installer
changed it to mode `0600`, which is the convention that the Fil-C
`/opt/fil/sbin/sshd` expects. Keys with stranger permission patterns were
left alone and reported as warnings, since the installer cannot know whether
those permissions are intentional.

The `sshd` privilege separation user and group were created if they were
missing. The user is created with `/opt/fil/var/lib/sshd` as its home
directory and `/bin/false` as its shell, which is the standard non-login
configuration. If the user already existed, the installer did not touch it.

Finally, the installer attempted to apply the right SELinux label to
`/opt/fil/sbin/sshd`. The logic is conservative: if SELinux is not enabled
on your system, or the SELinux command line tools are not installed, the
installer does nothing. If SELinux is enabled, the installer reads the
label from `/usr/sbin/sshd` and, if that label looks like the standard
`sshd_exec_t` type, copies the same label onto `/opt/fil/sbin/sshd` using
`chcon --reference=/usr/sbin/sshd /opt/fil/sbin/sshd`. If the label on
`/usr/sbin/sshd` is something the installer does not recognize, it refuses
to guess and prints detailed instructions for what to try by hand. This is
exactly the case where you may need to do the SELinux labeling yourself
before the binary will work under systemd.

## Switching systemd over to `/opt/fil/sbin/sshd`

To run the Fil-C build as your actual system sshd, you need to point your
existing systemd unit at the new binary, restart the service, and convince
yourself that you can still log in before you log out.

The cleanest way to do this without losing your distribution's unit file is
to drop a systemd override. On most distributions the unit is called
`sshd.service` (Red Hat, Rocky, Fedora, and CentOS) or `ssh.service`
(Debian and Ubuntu). Find out which one by running `systemctl status ssh`
or `systemctl status sshd`. Then run `systemctl edit sshd.service` (or
`ssh.service`), which will open an empty override file in your editor. Put
the following into the override and save it.

```ini
[Service]
ExecStartPre=
ExecStart=
ExecReload=
ExecStartPre=/opt/fil/sbin/sshd -t
ExecStart=/opt/fil/sbin/sshd -D $SSHD_OPTS
ExecReload=/opt/fil/sbin/sshd -t
ExecReload=/bin/kill -HUP $MAINPID
```

The empty assignments are important: they clear the values inherited from
the distribution unit before the new values are appended. Without them you
would end up with both binaries listed and systemd would refuse to start the
service.

Before restarting the service, validate the configuration with
`/opt/fil/sbin/sshd -t`. This catches typos in `sshd_config` and missing
host keys without taking the service down.

It is a very good idea to keep one root SSH session open while you do the
switchover, and to open a second SSH session from somewhere else to confirm
that logins still work, before you close the first session. If something
goes wrong, you can recover by running `systemctl revert sshd.service` (or
`ssh.service`) from the still-open session and restarting the service.

Reload systemd and restart the service:

```bash
sudo systemctl daemon-reload
sudo systemctl restart sshd   # or 'ssh' on Debian/Ubuntu
sudo systemctl status sshd
```

If the status output shows `active (running)` and the `ExecStart=` line
points at `/opt/fil/sbin/sshd`, you are now running the memory-safe OpenSSH
server. Open a second terminal and test a login before doing anything else.

If the service fails to start on a system that uses SELinux, the most likely
cause is that the new binary is missing the SELinux label that sshd is
expected to have. The installer tries to handle this automatically, as
described above, but if it printed a warning about an unrecognized label on
`/usr/sbin/sshd`, you need to fix this by hand before the service will run.
The simplest fix is to copy whatever label the stock sshd has:

```bash
sudo chcon --reference=/usr/sbin/sshd /opt/fil/sbin/sshd
```

That sets the label for now, but a future `restorecon` run or full SELinux
relabel will undo it, because the persistent file context database does not
know about `/opt/fil/sbin/sshd`. To make the label stick across relabels,
register it with `semanage`. Replace `sshd_exec_t` below with whatever type
your distribution actually uses, which you can read off the output of
`ls -Z /usr/sbin/sshd`.

```bash
sudo semanage fcontext -a -t sshd_exec_t '/opt/fil/sbin/sshd'
sudo restorecon -v /opt/fil/sbin/sshd
```

If `semanage` is not installed, you can install it with `dnf install
policycoreutils-python-utils` on Rocky and Fedora, or the equivalent
package on your distribution.

After the labeling change, restart the service again and check `journalctl
-u sshd` if it still misbehaves. The journal will usually tell you exactly
which file or socket SELinux denied access to, which makes it easy to track
down any remaining policy mismatch.

Finally, if you want to roll back at any point, `systemctl revert
sshd.service` deletes the override and returns you to the distribution
sshd, and a subsequent `systemctl restart sshd` brings it back online.
