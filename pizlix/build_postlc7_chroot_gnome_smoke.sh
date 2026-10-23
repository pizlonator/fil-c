#!/bin/bash

set -e
set -x

# Sprint 5: non-invasive runtime smoke test for the GNOME session wiring.
# Runs INSIDE the chroot as root.  Starts the system bus and elogind long
# enough to prove they come up and talk to each other, then shuts both
# down again.  No DRM, no session, nothing persistent (all state lives in
# the tmpfs /run).
#
# Sprint 10: this smoke covers ONLY the postlc7-level GNOME desktop:
# dbus, elogind, the mutter/gnome-session binaries, and the session glue
# FILES the setup script installs (session file, autostart desktops,
# launcher wrappers).  The shell-level checks (gnome-shell itself, the
# Shell/St/Gvc typelibs, the mutter/gnome-desktop introspection data,
# hicolor) live in build_postlc8_chroot_smoke.sh, because gnome-shell and
# the introspection rebuilds are POSTLC8 artifacts and do not exist yet
# when postlc7 runs on a fresh ./build.sh - checking for them here
# aborted the whole chain (the shell-port sprints grew this file in place
# while testing against a chroot that already had everything).
#
# Sprint 11: teardown moved into an EXIT trap, registered BEFORE any
# process is started.  Whatever way this script exits - the happy path,
# a failed check, a kill - the system bus and elogind it started are
# taken back down, so a failed smoke leaves no daemon behind in the
# chroot and no /sys/fs/cgroup mount for build_unmount.sh to trip over.
# Every cleanup step tolerates nothing having been started yet, since
# the trap also fires on the earliest failures.

# Pids of the processes this script started, recorded as they come up.
# Empty until then; the trap and the pidof fallbacks below tolerate that.
DBUS_PID=
DBUS_STARTED=
ELOGIND_PID=

# Our processes live in this chroot, but /proc is the host's, so a bare
# pidof match could name a host daemon.  A pid is ours only when its
# root is this chroot's own root.  The trailing "/." matters: a bare
# /proc/$p/root stat reports a per-process anonymous device, while
# /proc/$p/root/. resolves through the magic symlink onto the root
# directory itself and gives its real device:inode (the host's daemons
# sit on the host root filesystem, ours on the chroot's).
ours() {
    local p self root
    self=$(stat -c %d:%i /. 2>/dev/null) || return 0
    for p in "$@"; do
        root=$(stat -c %d:%i /proc/$p/root/. 2>/dev/null) || continue
        if [ "$root" = "$self" ]; then
            echo $p
        fi
    done
}

cleanup() {
    # Our elogind first (normal kill, then -9 after a grace sleep): killing
    # the system bus below would make elogind drop its bus connection and
    # self-exit mid-race, so take the daemon down explicitly while the bus
    # it watches is still up.
    local pid=""
    pid=$(ours ${ELOGIND_PID:-} $(pidof elogind 2>/dev/null) 2>/dev/null || true)
    if [ -n "$pid" ]; then
        kill $pid 2>/dev/null || true
        sleep 1
        kill -9 $pid 2>/dev/null || true
    fi
    # Our dbus-daemon: the pid we recorded when it came up, else the pid
    # file it wrote (this is the chroot's /run, so the file can only name
    # a daemon this script started), else pidof restricted to processes
    # rooted in this chroot.
    if [ -n "$DBUS_PID" ]; then
        pid=$DBUS_PID
    elif [ -f /run/dbus/pid ]; then
        pid=$(cat /run/dbus/pid 2>/dev/null || true)
    elif [ -n "$DBUS_STARTED" ]; then
        pid=$(ours $(pidof dbus-daemon 2>/dev/null) 2>/dev/null || true)
    fi
    if [ -n "$pid" ]; then
        kill $pid 2>/dev/null || true
    fi
    # elogind mounted cgroup2 at /sys/fs/cgroup while it ran.  Take that
    # nested mount down too, so the build's unmount step can unmount the
    # chroot's /sys (otherwise /sys stays busy and the postlc7 snapshot tar
    # walks live sysfs).  If elogind left /sys/fs/cgroup as a plain dir,
    # leave it: extraction recreates the empty dir.
    umount /sys/fs/cgroup 2>/dev/null || true
}
trap cleanup EXIT

mkdir -p /run/dbus
rm -f /run/dbus/pid /run/dbus/system_bus_socket

# dbus-daemon self-daemonizes (<fork/> in the stock system.conf) and
# writes /run/dbus/pid.
dbus-daemon --system
DBUS_STARTED=1
DBUS_PID=$(cat /run/dbus/pid 2>/dev/null || true)

sleep 1

# Prove the system bus answers.
dbus-send --system --dest=org.freedesktop.DBus --print-reply / \
    org.freedesktop.DBus.ListNames | head -5

# elogind sanity: the binary runs and identifies itself.
/usr/libexec/elogind --version | head -1

# Start elogind (daemonizes) and prove it claims its bus name.
/usr/libexec/elogind --daemon
ELOGIND_PID=$(pidof elogind 2>/dev/null || true)
sleep 1
dbus-send --system --dest=org.freedesktop.DBus --print-reply / \
    org.freedesktop.DBus.ListNames | grep -i login

# Other session pieces.
mutter --version
gnome-session --version

# The DisplayServer-phase launcher is a glue FILE installed by the setup
# script (it execs gnome-shell, which is a postlc8 artifact - the shell
# itself and its typelibs are checked by build_postlc8_chroot_smoke.sh,
# which runs after the postlc8 stage builds them).
test -x /usr/bin/pizlix-shell

# The session must require the shell by desktop-file id, and the autostart
# entry that carries the wrapper must have the same id (a desktop file's id
# IS its filename); the superseded mutter autostart entry must be gone.
grep -v '^#' /usr/share/gnome-session/sessions/pizlix-gnome.session \
    | grep '^RequiredComponents=org.gnome.Shell;$'
grep '^Exec=/usr/bin/pizlix-shell$' /etc/xdg/autostart/org.gnome.Shell.desktop
grep '^X-GNOME-Autostart-Phase=DisplayServer$' /etc/xdg/autostart/org.gnome.Shell.desktop
test ! -e /etc/xdg/autostart/pizlix-mutter.desktop

ls -l /usr/bin/pizlix-gnome /usr/bin/pizlix-shell /usr/bin/pizlix-mutter \
      /usr/share/gnome-session/sessions/pizlix-gnome.session \
      /etc/xdg/autostart/org.gnome.Shell.desktop \
      /etc/xdg/autostart/pizlix-terminal.desktop

# No inline teardown here: the EXIT trap registered above does it, on the
# happy path just like on any failure path.

echo GNOME-SMOKE-OK
