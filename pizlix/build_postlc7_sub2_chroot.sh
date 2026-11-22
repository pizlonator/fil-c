#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

./build_postlc7_chroot_project_dbus.sh
hash -r

# PAM first: everything else in this stage (shadow, elogind) needs it.
./build_postlc7_chroot_project_pam.sh
hash -r

# shadow rebuilt with PAM + working /etc/pam.d configs.
./build_postlc7_chroot_project_shadow.sh
hash -r

# elogind (logind API + pam_elogind.so + libelogind for libei).
./build_postlc7_chroot_project_elogind.sh
hash -r

# Small support libraries.
./build_postlc7_chroot_project_dconf.sh
hash -r

./build_postlc7_chroot_project_json-glib.sh
hash -r

./build_postlc7_chroot_project_lcms2.sh
hash -r

./build_postlc7_chroot_project_libxcvt.sh
hash -r

./build_postlc7_chroot_project_libusb.sh
hash -r

./build_postlc7_chroot_project_gusb.sh
hash -r

./build_postlc7_chroot_project_colord.sh
hash -r

./build_postlc7_chroot_project_libei.sh
hash -r

# X11 extension libraries needed by mutter/GNOME.
./build_postlc7_chroot_project_x11extras.sh
hash -r

# App metadata.
./build_postlc7_chroot_project_libxmlb.sh
hash -r

./build_postlc7_chroot_project_appstream.sh
hash -r

# Build tools for libadwaita (installed under /yolo, not /usr).
./build_postlc7_chroot_project_sassc.sh
hash -r

# Everything below is Sprint 3: the GNOME Wayland session stack.
# Order: xkbcommon X11 rebuild first (mutter needs xkbcommon-x11), then
# gnome-desktop (mutter needs gnome-desktop-4), then mutter itself.

# Sprint 7a: rebuild the GL/EGL stack from the folded Fil-C fixes before
# anything that compiles against it.  The postlc6 snapshot's mesa was built
# before the fil-c-compat fixes were folded into projects/mesa.projeny (EGL
# native handles as real pointers, dri.sym pizlonated* exports, xgetbv,
# sparse_array real-pointer links, glsl_types name_id decoding, nir_src
# provenance-carrying parent pointers, queryobj buffer pointers), and its
# libepoxy/gtk4 were compiled against the unfixed EGL headers.  Order
# matters: mesa installs the fixed headers, then libepoxy and gtk4 build
# against them, and mutter below builds against all of the above.
./build_postlc4_chroot_project_mesa.sh
hash -r

./build_postlc6_chroot_project_libepoxy.sh
hash -r

./build_postlc4_chroot_project_gtk4.sh
hash -r

# The audio / policy / input-method stack gnome-control-center needs at
# meson setup (moved here from postlc8, which built them AFTER g-c-c --
# the cross-stage ordering bug a from-scratch build exposed).  g-c-c's
# vendored gvc subproject (executed at meson.build line 118, before any
# other dependency check) hard-requires libpulse + libpulse-mainloop-glib
# (pkg-config AND cmake), and the top-level meson.build then hard-requires
# accountsservice, upower-glib, gcr-4, polkit-gobject-1 and (with the ibus
# option on, which it is) ibus-1.0.  gnome-shell (postlc8) needs the same
# set (gvc again, polkit-agent, Gcr-4, IBus-1.0, AccountsService-1.0,
# UPowerGlib-1.0 typelibs), so building it all here keeps both consumers
# fed.  Internal order: libsndfile is a hard dep of pulseaudio even for
# the client-only build (meson.build's dependency('sndfile') is
# unconditional); duktape is a hard dep of polkit 126 (the JS rule
# authority); polkit must precede accountsservice (its meson.build
# requires polkit-gobject-1) and upower (polkit=auto picks up the daemon
# policy actions); the GL/EGL rebuild above must precede gcr-4 (its ui
# bits link gtk4); elogind earlier in this stage provides libelogind
# (polkit's session_tracking, accountsservice's sd-login) and libudev.pc
# (upower's os_backend); dbus at the top of this stage provides dbus-1
# (polkit/accountsservice hard-require it, pulseaudio/ibus auto-need it).
./build_postlc7_chroot_project_duktape.sh
hash -r

./build_postlc7_chroot_project_libsndfile.sh
hash -r

./build_postlc7_chroot_project_pulseaudio.sh
hash -r

./build_postlc7_chroot_project_polkit.sh
hash -r

./build_postlc7_chroot_project_gcr.sh
hash -r

./build_postlc7_chroot_project_ibus.sh
hash -r

./build_postlc7_chroot_project_accountsservice.sh
hash -r

./build_postlc7_chroot_project_upower.sh
hash -r

# xkbcommon rebuilt with -D enable-x11=true (installs over the postlc3
# build which had X11 disabled).
./build_postlc7_chroot_project_xkbcommon.sh
hash -r

# gnome-desktop 44.5 (libgnome-desktop-3 for gnome-session,
# libgnome-desktop-4 for mutter).
./build_postlc7_chroot_project_gnome_desktop.sh
hash -r

# mutter 46.9, the compositor.
./build_postlc7_chroot_project_mutter.sh
hash -r

# Everything below is Sprint 4: the session applications.
# Order: gtksourceview first (standalone gtk4 library), then gnome-session
# (only needs pre-existing libs), then libadwaita (needs sassc from /yolo),
# then libeditorconfig (cmake), then the two apps that need all of the
# above (gnome-text-editor, gnome-font-viewer), and finally the icon theme
# (pure data, must come after gtk4 so gtk4-update-icon-cache exists).

# gtksourceview 5.12.1 (syntax highlighting engine used by
# gnome-text-editor; pcre2 comes from the postlc2 stage).
./build_postlc7_chroot_project_gtksourceview.sh
hash -r

# gnome-session 46.0 (elogind fallback for libsystemd).
./build_postlc7_chroot_project_gnome_session.sh
hash -r

# libadwaita 1.5.8 (the sassc stylesheet tool lives in /yolo/bin; the
# project script extends PATH to find it).
./build_postlc7_chroot_project_libadwaita.sh
hash -r

# libeditorconfig 0.12.11 (cmake; uses pcre2 from the postlc2 stage).
./build_postlc7_chroot_project_libeditorconfig.sh
hash -r

# The applications.
./build_postlc7_chroot_project_gnome_text_editor.sh
hash -r

./build_postlc7_chroot_project_gnome_font_viewer.sh
hash -r

./build_postlc7_chroot_project_vte.sh
hash -r

./build_postlc7_chroot_project_gnome_terminal.sh
hash -r

./build_postlc7_chroot_project_gnome_control_center.sh
hash -r

# Icon theme last (data only; runs gtk4-update-icon-cache).
./build_postlc7_chroot_project_adwaita_icon_theme.sh
hash -r

# Session wiring, LAST (all packages above must be in place first): the
# dbus/elogind bootscript links, the GNOME session file, the autostart
# entries, and the /usr/bin/pizlix-* launchers, installed from the glue
# files build_postlc7.sh copied into /sources/etc.  This used to be a
# second build_chroot_late call in build_postlc7.sh (with a smoke test
# after it); the stage now has exactly ONE build_chroot_late call, so the
# setup script runs here, at the end of the chroot phase, and the postlc7
# snapshot carries the glue.
./build_postlc7_chroot_setup_scripts_for_gnome.sh
hash -r
