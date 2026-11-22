#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

rm -vf projects/*/pizlonated-*.tar.gz

# Sprint D of the WiFi/GDM port (postlc12, "the login manager"): the four
# new projeny packages, all URL-style (no tarball checked into git).
#
# gdm 46.2 is the point of the stage: the GNOME Display Manager, so Pizlix
# boots into the GNOME login window on tty1 (runlevel 5, inittab respawn --
# gdm 46.2 runs in the foreground) and starts user sessions through
# /usr/share/wayland-sessions/pizlix-gnome.desktop.  Its two mandatory
# patches live in the projeny file: the greeter session's RequiredComponents
# reduced to org.gnome.Shell (no gnome-settings-daemon in Pizlix) and the
# data/pam-lfs/*.pam files rewritten for the pizlonated PAM stack (elogind
# instead of systemd, no gnome-keyring, the nonexistent system-* includes
# inlined).
#
# libcanberra 0.30 exists only because gdm's meson.build declares
# libcanberra-gtk3 >= 0.4 UNCONDITIONALLY (meson.build:66 -- only
# utils/gdm-screenshot consumes it); the option of patching gdm instead was
# rejected.  Its own configure hard-requires vorbisfile, hence:
# libvorbis 1.3.7, hence: libogg 1.3.5.  libcanberra's projeny patch also
# guards its X11-only helpers on GDK_WINDOWING_X11 (the pizlonated gtk3 is
# built targets=broadway,wayland -- no gdkx.h in the chroot).

filc/projeny package projects/libogg.projeny projects/libogg/pizlonated-libogg.tar.gz
filc/projeny package projects/libvorbis.projeny projects/libvorbis/pizlonated-libvorbis.tar.gz
filc/projeny package projects/libcanberra.projeny projects/libcanberra/pizlonated-libcanberra.tar.gz
filc/projeny package projects/gdm.projeny projects/gdm/pizlonated-gdm.tar.gz

ls -la projects/libogg/pizlonated-libogg.tar.gz \
       projects/libvorbis/pizlonated-libvorbis.tar.gz \
       projects/libcanberra/pizlonated-libcanberra.tar.gz \
       projects/gdm/pizlonated-gdm.tar.gz
