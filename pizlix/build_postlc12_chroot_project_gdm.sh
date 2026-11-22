#!/bin/bash

set -e
set -x

# gdm 46.2 -- the GNOME Display Manager.  THE login screen: runlevel 5's
# /etc/rc.d/init.d/xdm (blfs-bootscripts pattern, wired by
# build_postlc12_chroot_setup_scripts_for_gdm.sh) execs /usr/sbin/gdm, which
# runs FOREGROUND (daemon/main.c has no daemonize flag and no fork -- only
# --fatal-warnings/--timed-exit/--version) under inittab respawn, spawns the
# greeter session on the initial VT (tty1), and brokers the PAM conversation
# (gdm-password) for the user session it starts from
# /usr/share/wayland-sessions/pizlix-gnome.desktop.
#
# Meson flags (checked against meson_options.txt):
#   -Dlogind-provider=elogind      libelogind.pc 255.27 (the systemd branch
#                                  would need libsystemd + systemd-multi-seat-x).
#   -Dplymouth=disabled            no plymouth in Pizlix.
#   -Dlibaudit=disabled            audit was disabled in the Linux-PAM build.
#   -Dselinux=disabled             no selinux.
#   -Dxdmcp=disabled               no remote login (and no libXdmcp).
#   -Dipv6=false -Dprofiling=false -Dtcp-wrappers=false -Dsolaris=false
#                                  stock opt-outs.
#   -Dsystemd-journal=false        no journald (pizlix is sysvinit; the
#                                  default 'true' would add sd_journal calls).
#   -Dsystemdsystemunitdir=no -Dsystemduserunitdir=no
#                                  the string 'no' is explicitly handled
#                                  (data/meson.build:164-181 skips both unit
#                                  installs); pizlix boots from inittab.
#   -Ddefault-pam-config=lfs       installs data/pam-lfs/*.pam as
#                                  /etc/pam.d/{gdm-autologin,gdm-launch-
#                                  environment,gdm-fingerprint,gdm-smartcard,
#                                  gdm-password} (the lfs entry is the one set
#                                  in upstream's pam_data_files_map that matches
#                                  this layout).
#   -Dpam-mod-dir=/usr/lib/security  where the pizlonated Linux-PAM 1.7.2
#                                  installed its modules; pam_gdm.so (built by
#                                  this package) lands there too.
#   -Ddbus-sys=/usr/share/dbus-1/system.d   where NM/wpa_supplicant/NM's
#                                  conf fragments already live; gdm.conf (the
#                                  org.gnome.DisplayManager dbus policy, which
#                                  lets the gdm user talk to the daemon) goes
#                                  there and the system dbus-daemon reads it at
#                                  its next start.
#   -Dgdm-xsession=false           no X11 session wrapper (wayland only).
#   -Dinitial-vt=1                 the default; the inittab frees tty1 for the
#                                  greeter in runlevel 5 (gettys tty2-6).
#
# Every hard dependency is in the chroot: udev 256, gudev-1.0 238,
# libpam 1.7.2, glib 2.80.4, gtk3 3.24.52 (wayland-only backend -- the
# greeter chooser is the only other gtk3 piece and it is not built without
# X11 sessions), dconf 0.40.0 (the `dconf`
# CLI compiles the greeter dconf db at build time), json-glib 1.10.6 (only
# required when the PAM headers advertise PAM_BINARY_PROMPT, which the
# pizlonated Linux-PAM does not), accountsservice 23.13.9 (+ the
# AccountsService-1.0 typelib for the greeter's user list), xcb/x11/xau,
# libcanberra-gtk3 0.30 (built earlier in this stage), and libelogind.
# GDM installs its own libgdm (Gdm-1.0 gir/typelib -- the shell's screen
# lock + greeter login dialog load gi://Gdm) and pam_gdm.so.
#
# Two X11-only pieces are skipped (the pizlonated gtk3 has NO X11 backend --
# targets=broadway,wayland, so gdk/gdkx.h does not exist): the docs/ manual
# (itstool) and utils/gdm-screenshot (a debug tool that screenshots the
# greeter through GDK's X11 backend; nothing references it).  libcanberra-
# gtk3 stays installed so the top-level dependency('libcanberra-gtk3')
# still resolves.  GDM's remaining X11 usage (x11/xau in daemon/meson.build
# for the X session path, xcb) compiles fine -- those are the plain client
# libraries, present in the chroot.
#
# Fil-C fixes (see the projeny patch): #1 the greeter session's
# RequiredComponents drop the ~14 org.gnome.SettingsDaemon.* entries
# (gnome-settings-daemon is not built; the greeter shell comes from the
# existing /etc/xdg/autostart/org.gnome.Shell.desktop -> /usr/bin/pizlix-shell
# with GNOME_SHELL_SESSION_MODE=gdm); #2 the data/pam-lfs/*.pam files are
# rewritten for the pizlonated PAM stack (no system-account/system-password/
# system-session includes, no pam_gnome_keyring.so, pam_systemd.so ->
# pam_elogind.so -- the greeter session MUST register with elogind to get its
# runtime dir and its seat).  libgdm's version script (libgdm.map) already
# has `global: *;` so the pizlonated_ forwarders are exported unmodified.

cd /sources
rm -rf pizlonated-gdm
tar -xf pizlonated-gdm.tar.gz
cd pizlonated-gdm
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized \
      -Dlogind-provider=elogind  \
      -Dplymouth=disabled        \
      -Dlibaudit=disabled        \
      -Dselinux=disabled         \
      -Dxdmcp=disabled           \
      -Dipv6=false               \
      -Dprofiling=false          \
      -Dtcp-wrappers=false       \
      -Dsolaris=false            \
      -Dsystemd-journal=false    \
      -Dsystemdsystemunitdir=no  \
      -Dsystemduserunitdir=no    \
      -Ddefault-pam-config=lfs   \
      -Dpam-mod-dir=/usr/lib/security \
      -Ddbus-sys=/usr/share/dbus-1/system.d \
      -Dgdm-xsession=false       \
      -Dinitial-vt=1
ninja
ninja install
cd ../..

# --- gates ---------------------------------------------------------------------

echo "=== gate: the daemon + session launchers installed ==="
test -e /usr/sbin/gdm
test -e /usr/libexec/gdm-session-worker
test -e /usr/libexec/gdm-wayland-session
/usr/sbin/gdm --version

echo "=== gate: libgdm + the gir/typelib pair (the shell's screen lock loads gi://Gdm) ==="
test -e /usr/lib/libgdm.so
test -e /usr/share/gir-1.0/Gdm-1.0.gir
test -e /usr/lib/girepository-1.0/Gdm-1.0.typelib
pkg-config --modversion gdm
gjs -c "const Gdm = imports.gi.Gdm; print('Gdm loaded, Client ctor:', typeof Gdm.Client, Gdm.Client.\$gtype);"

echo "=== gate: the greeter session file + its RequiredComponents (patch #1) ==="
test -e /usr/share/gnome-session/sessions/gnome-login.session
grep -q "RequiredComponents=org.gnome.Shell;" /usr/share/gnome-session/sessions/gnome-login.session
if grep -q "SettingsDaemon" /usr/share/gnome-session/sessions/gnome-login.session; then
    echo "FATAL: the greeter session still requires gnome-settings-daemon components:" >&2
    grep SettingsDaemon /usr/share/gnome-session/sessions/gnome-login.session >&2
    exit 1
fi
cat /usr/share/gnome-session/sessions/gnome-login.session

echo "=== gate: the PAM configs installed (patch #2) ==="
for f in gdm-autologin gdm-launch-environment gdm-fingerprint gdm-smartcard gdm-password; do
    test -e /etc/pam.d/$f
done
grep -q "pam_elogind.so" /etc/pam.d/gdm-password
grep -q "pam_elogind.so" /etc/pam.d/gdm-launch-environment
if grep -vE "^[[:space:]]*#" /etc/pam.d/gdm-* | grep -qE "system-account|system-password|system-session|gnome_keyring|pam_systemd"; then
    echo "FATAL: the gdm PAM files still reference the stack Pizlix does not ship:" >&2
    grep -vnE "^[[:space:]]*#" /etc/pam.d/gdm-* | grep -E "system-account|system-password|system-session|gnome_keyring|pam_systemd" >&2
    exit 1
fi
cat /etc/pam.d/gdm-password

echo "=== gate: pam_gdm.so built into the pizlonated PAM module dir ==="
test -e /usr/lib/security/pam_gdm.so

echo "=== gate: the gdm dbus policy + the greeter dconf db + the gschema ==="
test -e /usr/share/dbus-1/system.d/gdm.conf
grep -q "org.gnome.DisplayManager" /usr/share/dbus-1/system.d/gdm.conf
test -e /usr/share/dconf/profile/gdm
test -e /usr/share/gdm/greeter-dconf-defaults
test -e /usr/share/glib-2.0/schemas/org.gnome.login-screen.gschema.xml
test "$(DCONF_PROFILE=gdm dconf read /org/gnome/desktop/session/session-name)" = "'gnome-login'"

echo "=== gate: the udev rule (virtio-gpu/qxl/bochs wayland detection for QEMU) ==="
ls /usr/lib/udev/rules.d/61-gdm.rules
grep -q "virtio" /usr/lib/udev/rules.d/61-gdm.rules

echo "=== gate: the pizlonated forwarders are exported from libgdm ==="
nm -D --defined-only /usr/lib/libgdm.so | grep -c pizlonated_gdm_

echo "=== gate: ldd clean ==="
for f in /usr/sbin/gdm /usr/libexec/gdm-session-worker /usr/libexec/gdm-wayland-session /usr/lib/libgdm.so; do
    if ldd $f 2>&1 | grep -q 'not found'; then
        echo "FATAL: $f has unresolved shared libraries:" >&2
        ldd $f 2>&1 | grep 'not found' >&2
        exit 1
    fi
done
echo "ldd: clean"

rm -rf pizlonated-gdm
hash -r

echo "postlc12 gdm: ALL GATES GREEN"
