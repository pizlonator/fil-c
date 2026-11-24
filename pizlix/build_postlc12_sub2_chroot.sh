#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

# The "login manager" stage.  Build order:
#
# 1. libtool (the libltdl Fil-C fix: the weak preloaded-symbols reference).
#    Must run FIRST -- libcanberra's configure probes libltdl with an
#    AC_CHECK_LIB link test, and the part2-era libltdl.so carried a hard
#    undefined reference to pizlonated_lt_libltdl_LTX_preloaded_symbols that
#    poisoned every executable link against it.  Nothing else in the base
#    system links libltdl, which is why the breakage hid until now.
# 2. libogg -> 3. libvorbis -> 4. libcanberra (the audio trio: gdm 46.2
#    declares libcanberra-gtk3 >= 0.4 UNCONDITIONALLY (meson.build:66), and
#    libcanberra's configure hard-requires vorbisfile, which needs libogg;
#    the chroot had neither).  libcanberra's projeny patch also makes the
#    gtk3 backend build against the pizlonated gtk3, which has NO X11
#    backend (targets=broadway,wayland): the X11-only helpers are guarded on
#    GDK_WINDOWING_X11.
# 5. gdm 46.2 (meson; -Dlogind-provider=elogind, no systemd, no plymouth,
#    the lfs PAM set patched for the pizlonated stack, the greeter session's
#    RequiredComponents reduced to org.gnome.Shell).
# 6. The boot wiring (setup): the gdm user/group + /var/lib/gdm, the blfs
#    xdm bootscript + /etc/sysconfig/xdm (DISPLAY_MANAGER=/usr/sbin/gdm),
#    the tracked inittab (id:5:initdefault, tty1's getty restricted to
#    runlevels 2-4, x:5:respawn:/etc/rc.d/init.d/xdm), and
#    /usr/share/wayland-sessions/pizlix-gnome.desktop (GDM's user-session
#    entry -> /usr/bin/pizlix-gnome).

./build_postlc12_chroot_project_libtool.sh
hash -r

./build_postlc12_chroot_project_libogg.sh
hash -r

./build_postlc12_chroot_project_libvorbis.sh
hash -r

./build_postlc12_chroot_project_libcanberra.sh
hash -r

./build_postlc12_chroot_project_gdm.sh
hash -r

./build_postlc12_chroot_setup_scripts_for_gdm.sh
hash -r
