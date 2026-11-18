#!/bin/bash
# TASK 4 verification runner (runs INSIDE the chroot via build_chroot_late.sh):
# simulate the from-scratch condition for the moved postlc7 audio/policy
# stack, rebuild it in the new postlc7 order, then gnome-control-center.
#
# Preconditions (host side): all 9 packages' artifacts (duktape, libsndfile,
# pulseaudio, polkit, gcr-4, ibus, accountsservice, upower,
# gnome-control-center) were removed from /usr, and the stale /sources
# extraction trees were removed.  The pizlonated tarballs stay in /sources
# (that is the from-scratch input: the stage driver copies them).

set -e
set -x

cd /sources

echo "=== GATE 1: from-scratch condition (libpulse and friends must be gone) ==="
if ls /usr/lib/pkgconfig/libpulse*.pc >/dev/null 2>&1; then
  echo "FATAL: libpulse .pc files still present"; exit 1
fi
if pkg-config --exists libpulse; then
  echo "FATAL: pkg-config still finds libpulse"; exit 1
fi
for pc in sndfile duktape polkit-gobject-1 gcr-4 ibus-1.0 accountsservice upower-glib; do
  if pkg-config --exists $pc; then
    echo "FATAL: $pc still present (removal incomplete)"; exit 1
  fi
done
echo "GATE 1 PASSED: none of the 8 moved packages are visible to pkg-config"

echo "=== GATE 2: rebuild the moved stack in the new postlc7 order ==="
./build_postlc7_chroot_project_duktape.sh
echo "MARK: duktape OK"
./build_postlc7_chroot_project_libsndfile.sh
echo "MARK: libsndfile OK"
./build_postlc7_chroot_project_pulseaudio.sh
echo "MARK: pulseaudio OK (libpulse + libpulse-mainloop-glib installed)"
./build_postlc7_chroot_project_polkit.sh
echo "MARK: polkit OK"
./build_postlc7_chroot_project_gcr.sh
echo "MARK: gcr-4 OK"
./build_postlc7_chroot_project_ibus.sh
echo "MARK: ibus OK"
./build_postlc7_chroot_project_accountsservice.sh
echo "MARK: accountsservice OK"
./build_postlc7_chroot_project_upower.sh
echo "MARK: upower OK"

echo "=== GATE 3: gnome-control-center now finds gvc's libpulse and builds ==="
./build_postlc7_chroot_project_gnome_control_center.sh
echo "MARK: gnome-control-center OK"

echo "=== GATE 4: artifacts landed ==="
pkg-config --modversion libpulse libpulse-mainloop-glib libpulse-simple \
           sndfile duktape polkit-gobject-1 polkit-agent-1 gcr-4 gck-2 \
           ibus-1.0 accountsservice upower-glib
test -x /usr/bin/gnome-control-center
test -f /usr/share/applications/org.gnome.Settings.desktop
test -f /usr/share/dbus-1/services/org.gnome.Settings.service
test -f /usr/share/gnome-control-center/keybindings/50-mutter-system.xml
test -f /usr/lib/sysusers.d/polkit.conf || true
ls /usr/share/applications/gnome-*-panel.desktop | wc -l
echo "GATE 4 PASSED"

echo "ALL VERIFICATION GATES PASSED"
