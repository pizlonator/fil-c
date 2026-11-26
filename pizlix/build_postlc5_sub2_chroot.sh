#!/bin/bash

set -e
set -x

ulimit -c unlimited

# The "login manager" stage.  Build order:
#
# 1. LibreOffice (moved here from the old postlc6 stage; it needs the X11
#    libraries, abseil, boost, libxslt, zip/unzip built in postlc3, plus the
#    four source tarballs + the external tarballs + the Fil-C patch staged by
#    this stage's driver).
# 2. libogg -> 3. libvorbis -> 4. libcanberra (the audio trio: gdm 46.2
#    declares libcanberra-gtk3 >= 0.4 UNCONDITIONALLY (meson.build:66), and
#    libcanberra's configure hard-requires vorbisfile, which needs libogg;
#    the chroot had neither).  libcanberra's projeny patch also makes the
#    gtk3 backend build against the pizlonated gtk3, which has NO X11
#    backend (targets=broadway,wayland): the X11-only helpers are guarded on
#    GDK_WINDOWING_X11.
#    (libtool is NOT rebuilt here: the postlc stage's part2 has built the
#    fixed libltdl from the projeny package since the 2026-11-22 conversion
#    -- the weak preloaded-symbols reference is already correct in every
#    snapshot this chain can start from, so the old same-version rebuild
#    would be dead weight.)
# 5. gdm 46.2 (meson; -Dlogind-provider=elogind, no systemd, no plymouth,
#    the lfs PAM set patched for the pizlonated stack, the greeter session's
#    RequiredComponents reduced to org.gnome.Shell).
# 6. The boot wiring (setup): the gdm user/group + /var/lib/gdm, the blfs
#    xdm bootscript + /etc/sysconfig/xdm (DISPLAY_MANAGER=/usr/sbin/gdm),
#    the tracked inittab (id:5:initdefault, tty1's getty restricted to
#    runlevels 2-4, x:5:respawn:/etc/rc.d/init.d/xdm), and
#    /usr/share/wayland-sessions/pizlix-gnome.desktop (GDM's user-session
#    entry -> /usr/bin/pizlix-gnome).

cd /sources

./build_postlc5_chroot_project_libreoffice.sh
hash -r

./build_postlc5_chroot_project_libogg.sh
hash -r

./build_postlc5_chroot_project_libvorbis.sh
hash -r

./build_postlc5_chroot_project_libcanberra.sh
hash -r

./build_postlc5_chroot_project_gdm.sh
hash -r

./build_postlc5_chroot_setup_scripts_for_gdm.sh
hash -r
