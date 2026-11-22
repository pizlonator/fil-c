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

# GNOME-on-Wayland stage (postlc7): session, compositor, and support libraries.
# xkbcommon is not a projeny project; it is packaged from its checked-in
# source directory with package-source.sh (same name as in the postlc3
# stage).  postlc7 rebuilds it with X11 support (xkbcommon-x11) for mutter.
./package-source.sh projects/libxkbcommon-xkbcommon-1.11.0 pizlonated-xkbcommon

# Sprint 7a: the postlc6 snapshot's installed mesa/libepoxy/gtk4 predate the
# folded Fil-C fixes (EGL native handles as real pointers, dri.sym
# pizlonated* exports, xgetbv, sparse_array, glsl_types name_id, nir
# _parent, queryobj), and postlc7 is the first stage to rebuild anything
# that sits on top of them (mutter).  So package the fixed GL/EGL stack
# here: mesa from its projeny patch, gtk4 from its checked-in source dir.
# (libepoxy needs no repackaging; its stock tarball is already in /sources
# from the postlc6 stage.)
filc/projeny package projects/mesa.projeny projects/mesa/pizlonated-mesa.tar.gz
./package-source.sh projects/gtk-4.14.5 pizlonated-gtk4
filc/projeny package projects/dbus.projeny projects/dbus/pizlonated-dbus.tar.gz
filc/projeny package projects/Linux-PAM.projeny projects/Linux-PAM/pizlonated-pam.tar.gz
filc/projeny package projects/json-glib.projeny projects/json-glib/pizlonated-json-glib.tar.gz
filc/projeny package projects/lcms2.projeny projects/lcms2/pizlonated-lcms2.tar.gz
filc/projeny package projects/libusb.projeny projects/libusb/pizlonated-libusb.tar.gz
filc/projeny package projects/gusb.projeny projects/gusb/pizlonated-gusb.tar.gz
filc/projeny package projects/colord.projeny projects/colord/pizlonated-colord.tar.gz
filc/projeny package projects/libei.projeny projects/libei/pizlonated-libei.tar.gz
filc/projeny package projects/libxmlb.projeny projects/libxmlb/pizlonated-libxmlb.tar.gz
filc/projeny package projects/appstream.projeny projects/appstream/pizlonated-appstream.tar.gz
filc/projeny package projects/elogind.projeny projects/elogind/pizlonated-elogind.tar.gz


# polkit 126's JS rule authority (system duktape; 0.116 bundled its own).
filc/projeny package projects/duktape.projeny projects/duktape/pizlonated-duktape.tar.gz

# libsndfile 1.2.2: a hard dep of pulseaudio even for the client-only
# build (libpulsecommon links pulsecore/sound-file.c; pacat/pactl link
# sndfile).  FLAC/Ogg/Vorbis/mpeg/alsa/sqlite are all disabled.
filc/projeny package projects/libsndfile.projeny projects/libsndfile/pizlonated-libsndfile.tar.gz

# pulseaudio 17.0, CLIENT-ONLY (libpulse + libpulse-mainloop-glib): the
# gvc subproject bundled by gnome-control-center AND gnome-shell links
# against both.
filc/projeny package projects/pulseaudio.projeny projects/pulseaudio/pizlonated-pulseaudio.tar.gz

# Session/policy services (g-c-c's power/sharing/keyboard panels and
# gnome-shell's quick settings / user menu / certificate chooser):
filc/projeny package projects/polkit.projeny projects/polkit/pizlonated-polkit.tar.gz
filc/projeny package projects/gcr.projeny projects/gcr/pizlonated-gcr.tar.gz
filc/projeny package projects/ibus.projeny projects/ibus/pizlonated-ibus.tar.gz
filc/projeny package projects/upower.projeny projects/upower/pizlonated-upower.tar.gz
filc/projeny package projects/accountsservice.projeny projects/accountsservice/pizlonated-accountsservice.tar.gz

filc/projeny package projects/libXfixes.projeny projects/libXfixes/pizlonated-libXfixes.tar.gz
filc/projeny package projects/libXcomposite.projeny projects/libXcomposite/pizlonated-libXcomposite.tar.gz
filc/projeny package projects/libXcursor.projeny projects/libXcursor/pizlonated-libXcursor.tar.gz
filc/projeny package projects/libXdamage.projeny projects/libXdamage/pizlonated-libXdamage.tar.gz
filc/projeny package projects/libXtst.projeny projects/libXtst/pizlonated-libXtst.tar.gz
filc/projeny package projects/libXi.projeny projects/libXi/pizlonated-libXi.tar.gz
filc/projeny package projects/libxkbfile.projeny projects/libxkbfile/pizlonated-libxkbfile.tar.gz
filc/projeny package projects/libxcvt.projeny projects/libxcvt/pizlonated-libxcvt.tar.gz
filc/projeny package projects/libsass.projeny projects/libsass/pizlonated-libsass.tar.gz
filc/projeny package projects/sassc.projeny projects/sassc/pizlonated-sassc.tar.gz
filc/projeny package projects/gnome-desktop.projeny projects/gnome-desktop/pizlonated-gnome-desktop.tar.gz
filc/projeny package projects/libadwaita.projeny projects/libadwaita/pizlonated-libadwaita.tar.gz
filc/projeny package projects/gtksourceview.projeny projects/gtksourceview/pizlonated-gtksourceview.tar.gz
filc/projeny package projects/libeditorconfig.projeny projects/libeditorconfig/pizlonated-libeditorconfig.tar.gz
filc/projeny package projects/mutter.projeny projects/mutter/pizlonated-mutter.tar.gz
filc/projeny package projects/gnome-session.projeny projects/gnome-session/pizlonated-gnome-session.tar.gz
filc/projeny package projects/adwaita-icon-theme.projeny projects/adwaita-icon-theme/pizlonated-adwaita-icon-theme.tar.gz
filc/projeny package projects/dconf.projeny projects/dconf/pizlonated-dconf.tar.gz
filc/projeny package projects/gnome-text-editor.projeny projects/gnome-text-editor/pizlonated-gnome-text-editor.tar.gz
filc/projeny package projects/gnome-font-viewer.projeny projects/gnome-font-viewer/pizlonated-gnome-font-viewer.tar.gz
filc/projeny package projects/vte.projeny projects/vte/pizlonated-vte.tar.gz
filc/projeny package projects/gnome-terminal.projeny projects/gnome-terminal/pizlonated-gnome-terminal.tar.gz
filc/projeny package projects/gnome-control-center.projeny projects/gnome-control-center/pizlonated-gnome-control-center.tar.gz
