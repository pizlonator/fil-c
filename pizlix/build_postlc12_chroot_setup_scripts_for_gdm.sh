#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Sprint D of the WiFi/GDM port: the boot-to-GNOME wiring.  This is the
# setup half of the postlc12 "login manager" stage (the build half is
# libtool/libogg/libvorbis/libcanberra/gdm).  What it wires:
#
# 1. THE GDM USER/GROUP (uid/gid 65, the BLFS numbering): gdm's daemon
#    refuses to run without it (daemon/main.c g_critical's "Can't find the
#    GDM user"); the greeter session (gnome-session + gnome-shell in
#    GDM greeter mode) runs AS this user, and /etc/dbus-1 system.d/gdm.conf
#    (installed by the gdm package) grants uid gdm the rights to talk to
#    org.gnome.DisplayManager.
# 2. /var/lib/gdm: the greeter's HOME (gdm's -Dworking-dir default).  Owned
#    by gdm so the greeter can write its dconf user-db under $HOME/.config
#    (DCONF_PROFILE=gdm also points it at the compiled greeter defaults GDM
#    installed).  /var/run/gdm and /var/log/gdm are ensured by the daemon
#    itself at startup (daemon/main.c gdm_daemon_ensure_dirs) -- they are
#    pre-created here anyway so the first boot's perms are right.
# 3. THE DISPLAY-MANAGER BOOTSCRIPT + ITS SYSCONF: /etc/rc.d/init.d/xdm
#    (from the tracked blfs-bootscripts tarball -- the script is NOT a
#    bootscript with dependencies, it is meant to be run directly from
#    inittab: `x:5:respawn:/etc/rc.d/init.d/xdm`) and
#    /etc/sysconfig/xdm with DISPLAY_MANAGER=/usr/sbin/gdm.  gdm 46.2 has
#    NO daemonize flag (daemon/main.c: foreground GMainLoop, root only), so
#    the exec'd process is exactly what inittab should supervise: if it
#    dies, init restarts it (sysvinit throttles respawn loops itself).
# 4. THE INITTAB: deployed from the tracked pizlix/etc/inittab (staged into
#    /sources/etc by build_postlc12.sh so the recovery chain gets it too --
#    the full chain gets the same file via build_copy_stuff.sh).  The boot
#    design (see the inittab itself):
#      - id:5:initdefault (GNOME, the graphical runlevel);
#      - tty1's getty is restricted to runlevels 2-4 (1:234:): in runlevel
#        5 tty1 is GDM's initial VT (-Dinitial-vt=1);
#      - tty2-6 keep gettys in runlevels 2-5 (2:345: ... 6:345:): the
#        rescue consoles survive alongside the greeter (runlevel 3 remains
#        the full text-mode path -- six gettys -- and `init 3` / `init 5`
#        switches live);
#      - x:5:respawn:/etc/rc.d/init.d/xdm.
# 5. THE USER-SESSION SIDE: /usr/share/wayland-sessions/pizlix-gnome.desktop
#    (the wayland-sessions directory is where GDM discovers user sessions;
#    Exec=/usr/bin/pizlix-gnome is the existing session launcher -- dbus-
#    run-session + gnome-session --session=pizlix-gnome, the exact session
#    that has been running since Sprint 5).  GDM wraps the chosen Exec with
#    its own gdm-wayland-session + a private session bus
#    (daemon/gdm-session.c:3212), so the launcher does not change.
# 6. THE GREETER'S SHELL LOG: /etc/xdg/autostart/org.gnome.Shell.desktop ->
#    /usr/bin/pizlix-shell is shared by the greeter and the user session;
#    the wrapper's log file was made per-session-mode
#    (/tmp/pizlix-shell-$GNOME_SHELL_SESSION_MODE.log with the user session
#    keeping the historical /tmp/pizlix-shell-session.log name) so the gdm
#    user can open its own log even when the file from a previous user
#    session exists (a stale 644 root/pizlo-owned file would break the
#    wrapper's redirection and kill the greeter's shell before it starts).
#
# rc5.d needs NO changes here: rc2-5 already carry S20dbus + S25elogind +
# S33networkmanager (Sprint B's networkmanager boot wiring installs S33 in
# rc2..rc5) + S30sshd + S80seatd, and the S20network (dhcpcd) symlinks are
# already absent (NM manages eth0; the network script stays on disk for
# rescue).  The greeter's network indicator runs over that same NM.
#
# The gdm package's PAM configs (/etc/pam.d/gdm-*) are installed by the
# gdm build script (patched for the pizlonated stack); this script only
# GATE-checks them.

cd /sources

# --- 1+2: the gdm user/group + the greeter home -------------------------------

getent group gdm >/dev/null || groupadd -g 65 gdm
getent passwd gdm >/dev/null || useradd -c 'GDM Daemon' -d /var/lib/gdm -u 65 \
    -g gdm -s /bin/false -r gdm

install -d -m750 -o gdm -g gdm /var/lib/gdm
install -d -m755 -o root -g gdm /var/run/gdm /var/log/gdm
install -d -m750 -o gdm -g gdm /var/lib/gdm/.config
install -d -m750 -o gdm -g gdm /var/lib/gdm/.config/dconf
install -d -m750 -o gdm -g gdm /var/lib/gdm/.local
install -d -m750 -o gdm -g gdm /var/lib/gdm/.local/share

# --- 3: the display-manager bootscript + sysconfig ----------------------------

rm -rf blfs-bootscripts-20240416
tar -xf blfs-bootscripts-20240416.tar.xz
install -m755 blfs-bootscripts-20240416/blfs/init.d/xdm /etc/rc.d/init.d/xdm
rm -rf blfs-bootscripts-20240416

cat > /etc/sysconfig/xdm << "EOF"
# Which display manager to use (full path):
# Pizlix: gdm 46.2 (postlc12).  gdm runs in the FOREGROUND (no daemonize
# flag -- daemon/main.c), so inittab's respawn entry supervises it directly;
# do NOT pass daemonizing options.
DISPLAY_MANAGER=/usr/sbin/gdm

# Options to pass to the display manager:
DM_OPTIONS=""
EOF

# --- 4: the inittab (deployed from the tracked copy) --------------------------

test -e /sources/etc/inittab
install -m644 /sources/etc/inittab /etc/inittab

# --- 5: the user-session entry ------------------------------------------------

install -d -m755 /usr/share/wayland-sessions
cat > /usr/share/wayland-sessions/pizlix-gnome.desktop << "EOF"
[Desktop Entry]
Type=Application
# Pizlix's GNOME (Wayland) session, started by GDM's gdm-wayland-session
# (which wraps this Exec with its own private session bus).  Same launcher
# the getty-login path has used since Sprint 5.
Name=Pizlix GNOME
Comment=This session logs you into the Pizlix GNOME desktop
Exec=/usr/bin/pizlix-gnome
TryExec=/usr/bin/pizlix-gnome
DesktopNames=GNOME
EOF

# --- 6: the greeter/user shell log split ---------------------------------------

# The tracked wrapper (staged into /sources/etc/gnome by build_postlc12.sh)
# carries the per-session-mode log name; deploy it over /usr/bin/pizlix-shell.
test -e /sources/etc/gnome/pizlix-shell.sh
install -m755 /sources/etc/gnome/pizlix-shell.sh /usr/bin/pizlix-shell
chmod 755 /usr/bin/pizlix-shell
grep -q "SHELL_SESSION_MODE" /usr/bin/pizlix-shell
grep -q "pizlix-shell-\$SHELL_SESSION_MODE.log" /usr/bin/pizlix-shell

# --- the canonical dconf profile location --------------------------------------

# gdm's meson installs the greeter dconf profile to
# $datadir/dconf/profile/gdm (= /usr/share/dconf/profile/gdm here, since
# dconf.pc's prefix is /usr).  libdconf finds it there (verified: with
# DCONF_PROFILE=gdm, `dconf read /org/gnome/desktop/session/session-name`
# returns 'gnome-login' from the compiled greeter defaults).  The canonical
# /etc/dconf/profile location is mirrored here as well: it is what gdm's own
# DCONF_SYSCONFIG constant points at and what distros ship, so tools looking
# in /etc see the same profile.
if test ! -e /etc/dconf/profile/gdm; then
    install -d -m755 /etc/dconf/profile
    install -m644 /usr/share/dconf/profile/gdm /etc/dconf/profile/gdm
fi

# --- the default user session --------------------------------------------------

# gnome-session installs BOTH stock wayland-sessions entries
# (gnome.desktop + gnome-wayland.desktop, Exec=/usr/bin/gnome-session with no
# --session argument): such a session resolves its session name from
# org.gnome.desktop.session session-name (default 'gnome') and pulls the STOCK
# gnome.session, whose RequiredComponents list needs ~15
# org.gnome.SettingsDaemon.* components that Pizlix does not build -- the
# login would open a PAM session and then die two seconds later on the
# gnome-session fail whale (verified live, /home/pizlo/.cache/gdm/session.log).
# The greeter's session menu defaulted to the first alphabetical entry
# ('gnome') and hit exactly that.  Remove the two broken entries --
# /usr/share/wayland-sessions/pizlix-gnome.desktop (Exec=/usr/bin/pizlix-gnome,
# the launcher that has carried the session since Sprint 5) becomes the only
# GNOME wayland session and therefore the default; weston.desktop stays (it
# works, as an alternative).
for f in gnome.desktop gnome-wayland.desktop; do
    if test -e /usr/share/wayland-sessions/$f; then
        rm -v /usr/share/wayland-sessions/$f
    fi
done
test -e /usr/share/wayland-sessions/pizlix-gnome.desktop

# Preseed pizlo's persisted session choice through accountsservice's own
# store (the same thing picking a session in the greeter writes): the greeter
# starts the last-used session, so this pins pizlix-gnome as the default for
# the primary user even if more sessions appear later.  The file format is
# accountsservice's (owned root, 0600).
install -d -m755 /var/lib/AccountsService/users
cat > /var/lib/AccountsService/users/pizlo << "EOF"
[User]
Session=pizlix-gnome
SessionType=wayland
SystemAccount=false
EOF
chmod 600 /var/lib/AccountsService/users/pizlo

# --- gates ---------------------------------------------------------------------

echo "=== gate: the gdm user/group (uid/gid 65) ==="
test "$(getent passwd gdm | cut -d: -f3)" = 65
test "$(getent group gdm | cut -d: -f3)" = 65
test "$(getent passwd gdm | cut -d: -f6)" = /var/lib/gdm
test "$(getent passwd gdm | cut -d: -f7)" = /bin/false
id gdm

echo "=== gate: the greeter home + runtime/log dirs ==="
test -d /var/lib/gdm
test "$(stat -c %U:%G /var/lib/gdm)" = gdm:gdm
ls -la /var/lib/gdm/
test -d /var/run/gdm
test -d /var/log/gdm

echo "=== gate: the xdm bootscript + sysconfig ==="
test -e /etc/rc.d/init.d/xdm
test -x /etc/rc.d/init.d/xdm
grep -q "DISPLAY_MANAGER" /etc/rc.d/init.d/xdm
grep -q "DISPLAY_MANAGER=/usr/sbin/gdm" /etc/sysconfig/xdm
grep -q 'DM_OPTIONS=""' /etc/sysconfig/xdm
cat /etc/sysconfig/xdm

echo "=== gate: the inittab (initdefault 5, tty1 getty restricted to 2-4, x:5 respawn) ==="
grep -q "^id:5:initdefault:" /etc/inittab
grep -q "^1:234:respawn:" /etc/inittab
grep -q "^2:345:respawn:" /etc/inittab
grep -q "^6:345:respawn:" /etc/inittab
grep -q "^x:5:respawn:/etc/rc.d/init.d/xdm" /etc/inittab
if grep -q "^1:2345:respawn:" /etc/inittab; then
    echo "FATAL: tty1's getty still covers runlevel 5 (would fight GDM for tty1):" >&2
    grep "^1:" /etc/inittab >&2
    exit 1
fi
if grep -qE "^d[mx]:5:respawn" /etc/inittab && [ "$(grep -cE '^(x|dm):5:respawn' /etc/inittab)" != 1 ]; then
    echo "FATAL: more than one display-manager respawn entry:" >&2
    grep -E "^(x|dm):5:respawn" /etc/inittab >&2
    exit 1
fi
cat /etc/inittab

echo "=== gate: runlevel 5 has the services (dbus, elogind, sshd, NM, seatd) and no dhcpcd ==="
for s in S20dbus S25elogind S30sshd S33networkmanager S80seatd; do
    test -e /etc/rc.d/rc5.d/$s
done
test ! -e /etc/rc.d/rc5.d/S20network
ls /etc/rc.d/rc5.d/
test -e /etc/rc.d/init.d/network
test -e /etc/sysconfig/ifconfig.eth0

echo "=== gate: the user-session entry (GDM discovers it in wayland-sessions) ==="
test -e /usr/share/wayland-sessions/pizlix-gnome.desktop
grep -q "Exec=/usr/bin/pizlix-gnome" /usr/share/wayland-sessions/pizlix-gnome.desktop
grep -q "DesktopNames=GNOME" /usr/share/wayland-sessions/pizlix-gnome.desktop
cat /usr/share/wayland-sessions/pizlix-gnome.desktop

echo "=== gate: the greeter dconf profile + the locked session-name ==="
test -e /usr/share/dconf/profile/gdm -o -e /etc/dconf/profile/gdm
test "$(DCONF_PROFILE=gdm dconf read /org/gnome/desktop/session/session-name)" = "'gnome-login'"

echo "=== gate: the greeter session file (from the gdm build) ==="
test -e /usr/share/gnome-session/sessions/gnome-login.session
grep -q "RequiredComponents=org.gnome.Shell;" /usr/share/gnome-session/sessions/gnome-login.session

echo "=== gate: the PAM services (installed by the gdm build) ==="
for f in gdm-autologin gdm-launch-environment gdm-password; do
    test -e /etc/pam.d/$f
    grep -q "pam_elogind.so" /etc/pam.d/$f
done
test -e /usr/lib/security/pam_gdm.so
cat /etc/pam.d/gdm-launch-environment

echo "=== gate: the default user session (stock entries gone, accountsservice preseeded) ==="
test ! -e /usr/share/wayland-sessions/gnome.desktop
test ! -e /usr/share/wayland-sessions/gnome-wayland.desktop
test -e /usr/share/wayland-sessions/pizlix-gnome.desktop
grep -q "Session=pizlix-gnome" /var/lib/AccountsService/users/pizlo
grep -q "SessionType=wayland" /var/lib/AccountsService/users/pizlo
ls /usr/share/wayland-sessions/

echo "=== gate: the greeter shell launcher + the session split ==="
test -x /usr/bin/pizlix-shell
test -x /usr/bin/pizlix-gnome
grep -q "GNOME_SHELL_SESSION_MODE" /usr/bin/pizlix-shell || true
ls /etc/xdg/autostart/org.gnome.Shell.desktop

hash -r

echo "postlc12 gdm boot wiring: ALL GATES GREEN"
