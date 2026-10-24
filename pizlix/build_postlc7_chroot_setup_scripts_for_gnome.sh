#!/bin/bash

set -e
set -x

# Sprint 5: session wiring for the GNOME (Wayland) session.  Mirrors
# build_postlc3_chroot_setup_scripts_for_weston.sh (files live in
# /sources/etc and get copied into place, then rcN.d symlinks are made).

# Bootscripts.  dbus first (S20, before sshd's S30), then elogind (S25):
# elogind claims org.freedesktop.login1 on the system bus, and
# pam_elogind (used by login) needs elogind reachable before gettys run.
cp -v /sources/etc/init.d/dbus /etc/init.d/dbus
chmod -v 755 /etc/init.d/dbus

cp -v /sources/etc/init.d/elogind /etc/init.d/elogind
chmod -v 755 /etc/init.d/elogind

for n in 2 3 4 5; do
    ln -sfv ../init.d/dbus /etc/rc.d/rc${n}.d/S20dbus
    ln -sfv ../init.d/elogind /etc/rc.d/rc${n}.d/S25elogind
done

# K links mirror the seatd pattern (K20seatd/K25seatd... see
# build_postlc3_chroot_setup_scripts_for_weston.sh).
for n in 0 1 6; do
    ln -sfv ../init.d/dbus /etc/rc.d/rc${n}.d/K20dbus
    ln -sfv ../init.d/elogind /etc/rc.d/rc${n}.d/K25elogind
done

# The session definition.  gnome-session looks the session up in
# /usr/share/gnome-session/sessions/ and resolves RequiredComponents
# entries by desktop-file id.
mkdir -pv /usr/share/gnome-session/sessions
cp -v /sources/etc/gnome-session/pizlix-gnome.session \
      /usr/share/gnome-session/sessions/pizlix-gnome.session

# Autostart entries: gnome-shell (DisplayServer phase) and a terminal.
mkdir -pv /etc/xdg/autostart
cp -v /sources/etc/xdg/autostart/org.gnome.Shell.desktop /etc/xdg/autostart/
cp -v /sources/etc/xdg/autostart/pizlix-terminal.desktop /etc/xdg/autostart/

# Sprint 4 wired standalone mutter into the session; Sprint 5 supersedes it
# with gnome-shell (the same mutter-14 compositor with the shell UI on
# top).  The session no longer references pizlix-mutter, and its autostart
# entry must not linger: a DisplayServer-phase entry in /etc/xdg/autostart
# gets autostarted by every session and would fight the shell for the seat.
rm -fv /etc/xdg/autostart/pizlix-mutter.desktop

# The launchers: /usr/bin/pizlix-gnome (what the user runs) and
# /usr/bin/pizlix-shell (the DisplayServer-phase wrapper).  pizlix-mutter
# stays installed (it is still built, and useful for mutter-only testing),
# but nothing in the session starts it anymore.
cp -v /sources/etc/gnome/pizlix-gnome.sh /usr/bin/pizlix-gnome
chmod -v 755 /usr/bin/pizlix-gnome
cp -v /sources/etc/gnome/pizlix-shell.sh /usr/bin/pizlix-shell
chmod -v 755 /usr/bin/pizlix-shell
cp -v /sources/etc/gnome/pizlix-mutter.sh /usr/bin/pizlix-mutter
chmod -v 755 /usr/bin/pizlix-mutter

# dbus and elogind need a machine id (BLFS dbus page: dbus-uuidgen
# --ensure plus the /etc/machine-id symlink).  It was missing so far.
dbus-uuidgen --ensure
ln -sfv /var/lib/dbus/machine-id /etc/machine-id

# Make shadow's login use PAM (USE_PAM was not set yet) so that
# pam_elogind registers every console login with elogind - the
# prerequisite for mutter to TakeControl of seat0.
grep -q '^USE_PAM' /etc/login.defs || echo 'USE_PAM yes' >> /etc/login.defs

# elogind requires /run/user to be 0755; the LFS mountvirtfs bootscript
# creates it 1777 and elogind then refuses every session
# ("Directory /run/user ... too permissive ... refusing").
sed -i 's|mkdir -m 1777 /run/user|mkdir -m 755 /run/user|' /etc/rc.d/init.d/mountvirtfs
grep -n 'run/user' /etc/rc.d/init.d/mountvirtfs

grep '^USE_PAM' /etc/login.defs

# Fontconfig caches (postlc8 finding: with no /var/cache/fontconfig caches
# every GUI launch rebuilt them from scratch, burning ~1 min of CPU per
# app).  Build them once as root, world-readable, so every user (and every
# app's fontconfig instance) starts from a warm cache.
fc-cache -f
ls -lv /var/cache/fontconfig | head

# gdk-pixbuf loader cache: GTK apps query it on every startup; regenerate
# it if it is missing or older than the loader directory so no app has to
# fall back to a cold scan.
if [ ! -s /usr/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache ] \
   || [ "$(find /usr/lib/gdk-pixbuf-2.0/2.10.0/loaders -newer /usr/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache | head -1)" ]; then
    gdk-pixbuf-query-loaders --update-cache
fi
ls -lv /usr/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache

# Session default: animations off.  The shell runs on QEMU bochs-drm +
# llvmpipe (GBM_ALWAYS_SOFTWARE) with a fil-C pizlonated mozjs that is
# interpreter-only (JIT cannot pass fil-C's guarantees) - a single overview
# animation is dozens of full-screen software repaints, each dominated by
# ~3-orders-of-magnitude-slower JS, which pegs gnome-shell >100% CPU for
# tens of seconds and makes input lag by the same amount.  Ship with
# animations disabled (a GSettings *schema override*, i.e. the compiled-in
# default: no dbus session is needed at build time, it applies to every
# user, and a user can still turn animations back on with gsettings).
cat > /usr/share/glib-2.0/schemas/81_pizlix-software-gpu.gschema.override <<'EOF'
[org.gnome.desktop.interface]
enable-animations=false
EOF
glib-compile-schemas /usr/share/glib-2.0/schemas/
gsettings get org.gnome.desktop.interface enable-animations
