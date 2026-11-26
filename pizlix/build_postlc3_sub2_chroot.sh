#!/bin/bash

set -e
set -x

ulimit -c unlimited

# The merged postlc3 chroot script (the old postlc6-minus-LibreOffice +
# postlc7 + postlc8 + postlc11 stages' sub2 scripts, concatenated in build
# order).  LibreOffice moved to postlc5, and the old layout's cross-stage
# rebuilds are gone: the GL/EGL stack is built once in postlc2 from the
# fixed projeny package and fixed headers (no mesa/libepoxy/gtk4 re-runs
# here), lcms2 comes from postlc2's projeny build, pygobject comes from
# postlc2 (with the import-gi fix), and the mutter/gnome-desktop gir
# rebuilds are same-stage second passes.

cd /sources

# --- X11 libraries + LibreOffice dependencies (the old postlc6 stage) --------

# X11 libraries
for pkg in util-macros-1.20.1 xorgproto-2024.1 xtrans-1.5.0 xcb-proto-1.17.0 \
           libXau-1.0.11 libXdmcp-1.1.4 libxcb-1.17.0 libX11-1.8.10 \
           libICE-1.1.1 libSM-1.2.4 libXt-1.3.0 \
           libXext-1.3.6 libXrender-0.9.11 libXrandr-1.5.4 libXinerama-1.1.5; do
    rm -rf $pkg
    tar -xf $pkg.tar.xz
    cd $pkg
    ./configure --prefix=/usr --sysconfdir=/etc
    make -j$(nproc)
    make install
    cd ..
    rm -rf $pkg
done
hash -r

# libepoxy with X11 enabled (the second, X11-capable pass over the plain
# tarball; postlc2 built it with -Dx11=false before X11 existed).  Built
# ONCE here -- the old layout re-ran it after the GL fix folds, but the
# stack it builds against is already fixed in postlc2.
./build_postlc3_chroot_project_libepoxy.sh
hash -r

# cairo with X11 (the second pass; postlc2 built cairo without X11).
./build_postlc3_chroot_project_cairo_xlib.sh
hash -r

./build_postlc3_chroot_project_glstub.sh
hash -r

./build_postlc3_chroot_project_abseil.sh
hash -r

./build_postlc3_chroot_project_boost.sh
hash -r

./build_postlc3_chroot_project_libxslt.sh
hash -r

./build_postlc3_chroot_project_zip.sh
hash -r

./build_postlc3_chroot_project_unzip.sh
hash -r

# --- GNOME desktop (the old postlc7 stage) ------------------------------------

./build_postlc3_chroot_project_dbus.sh
hash -r

# PAM first: everything else in this stage (shadow, elogind) needs it.
./build_postlc3_chroot_project_pam.sh
hash -r

# shadow rebuilt with PAM + working /etc/pam.d configs.
./build_postlc3_chroot_project_shadow.sh
hash -r

# elogind (logind API + pam_elogind.so + libelogind for libei).
./build_postlc3_chroot_project_elogind.sh
hash -r

# Small support libraries.
./build_postlc3_chroot_project_dconf.sh
hash -r

./build_postlc3_chroot_project_json-glib.sh
hash -r

./build_postlc3_chroot_project_libxcvt.sh
hash -r

./build_postlc3_chroot_project_libusb.sh
hash -r

./build_postlc3_chroot_project_gusb.sh
hash -r

./build_postlc3_chroot_project_colord.sh
hash -r

./build_postlc3_chroot_project_libei.sh
hash -r

# X11 extension libraries needed by mutter/GNOME.
./build_postlc3_chroot_project_x11extras.sh
hash -r

# App metadata.
./build_postlc3_chroot_project_libxmlb.sh
hash -r

./build_postlc3_chroot_project_appstream.sh
hash -r

# Build tools for libadwaita (installed under /yolo, not /usr).
./build_postlc3_chroot_project_sassc.sh
hash -r

# The GNOME Wayland session stack.
# Order: xkbcommon X11 rebuild first (mutter needs xkbcommon-x11), then
# gnome-desktop (mutter needs gnome-desktop-4), then mutter itself.
# Mesa/libepoxy/gtk4 are NOT rebuilt here: postlc2 built the GL/EGL stack
# once from the fixed projeny package (EGL native handles as real pointers,
# dri.sym pizlonated* exports, xgetbv, sparse_array real-pointer links,
# glsl_types name_id decoding, nir_src provenance-carrying parent pointers,
# queryobj buffer pointers), and everything in this stage compiles against
# that fixed install.

# The audio / policy / input-method stack gnome-control-center needs at
# meson setup (ordered here BEFORE g-c-c -- in the old layout a later stage
# built them after g-c-c, which broke from-scratch builds).  g-c-c's
# vendored gvc subproject (executed at meson.build line 118, before any
# other dependency check) hard-requires libpulse + libpulse-mainloop-glib
# (pkg-config AND cmake), and the top-level meson.build then hard-requires
# accountsservice, upower-glib, gcr-4, polkit-gobject-1 and (with the ibus
# option on, which it is) ibus-1.0.  gnome-shell (the shell block below)
# needs the same set (gvc again, polkit-agent, Gcr-4, IBus-1.0,
# AccountsService-1.0, UPowerGlib-1.0 typelibs), so building it all here
# keeps both consumers fed.  Internal order: libsndfile is a hard dep of
# pulseaudio even for the client-only build (meson.build's
# dependency('sndfile') is unconditional); duktape is a hard dep of polkit
# 126 (the JS rule authority); polkit must precede accountsservice (its
# meson.build requires polkit-gobject-1) and upower (polkit=auto picks up
# the daemon policy actions); gcr-4 needs gtk4 (postlc2's fixed build);
# elogind earlier in this stage provides libelogind (polkit's
# session_tracking, accountsservice's sd-login) and libudev.pc (upower's
# os_backend); dbus at the top of this stage provides dbus-1
# (polkit/accountsservice hard-require it, pulseaudio/ibus auto-need it).
./build_postlc3_chroot_project_duktape.sh
hash -r

./build_postlc3_chroot_project_libsndfile.sh
hash -r

./build_postlc3_chroot_project_pulseaudio.sh
hash -r

./build_postlc3_chroot_project_polkit.sh
hash -r

./build_postlc3_chroot_project_gcr.sh
hash -r

./build_postlc3_chroot_project_ibus.sh
hash -r

./build_postlc3_chroot_project_accountsservice.sh
hash -r

./build_postlc3_chroot_project_upower.sh
hash -r

# xkbcommon rebuilt with -D enable-x11=true (installs over the postlc2
# build which had X11 disabled).
./build_postlc3_chroot_project_xkbcommon.sh
hash -r

# gnome-desktop 44.5 (libgnome-desktop-3 for gnome-session,
# libgnome-desktop-4 for mutter).  A second pass with introspection enabled
# runs in the shell block below.
./build_postlc3_chroot_project_gnome_desktop.sh
hash -r

# mutter 46.9, the compositor.  A second pass with introspection enabled
# runs in the shell block below.
./build_postlc3_chroot_project_mutter.sh
hash -r

# The session applications.
# Order: gtksourceview first (standalone gtk4 library), then gnome-session
# (only needs pre-existing libs), then libadwaita (needs sassc from /yolo),
# then libeditorconfig (cmake), then the two apps that need all of the
# above (gnome-text-editor, gnome-font-viewer), and finally the icon theme
# (pure data, must come after gtk4 so gtk4-update-icon-cache exists).

# gtksourceview 5.12.1 (syntax highlighting engine used by
# gnome-text-editor; pcre2 comes from the postlc2 stage).
./build_postlc3_chroot_project_gtksourceview.sh
hash -r

# gnome-session 46.0 (elogind fallback for libsystemd).
./build_postlc3_chroot_project_gnome_session.sh
hash -r

# libadwaita 1.5.8 (the sassc stylesheet tool lives in /yolo/bin; the
# project script extends PATH to find it).
./build_postlc3_chroot_project_libadwaita.sh
hash -r

# libeditorconfig 0.12.11 (cmake; uses pcre2 from the postlc2 stage).
./build_postlc3_chroot_project_libeditorconfig.sh
hash -r

# The applications.
./build_postlc3_chroot_project_gnome_text_editor.sh
hash -r

./build_postlc3_chroot_project_gnome_font_viewer.sh
hash -r

./build_postlc3_chroot_project_libnl.sh
hash -r
./build_postlc3_chroot_project_libndp.sh
hash -r
./build_postlc3_chroot_project_wpa_supplicant.sh
hash -r
./build_postlc3_chroot_project_networkmanager.sh
hash -r
./build_postlc3_chroot_project_dnsmasq.sh
hash -r
./build_postlc3_chroot_project_libnma.sh
hash -r

./build_postlc3_chroot_project_vte.sh
hash -r

./build_postlc3_chroot_project_gnome_terminal.sh
hash -r

./build_postlc3_chroot_project_gnome_control_center.sh
hash -r

# Icon theme last (data only; runs gtk4-update-icon-cache).
./build_postlc3_chroot_project_adwaita_icon_theme.sh
hash -r

# Session wiring, LAST of the desktop block (all packages above must be in
# place first): the dbus/elogind bootscript links, the GNOME session file,
# the autostart entries, and the /usr/bin/pizlix-* launchers, installed from
# the glue files build_postlc3.sh copied into /sources/etc.  This used to be
# a second build_chroot_late call in the stage driver (with a smoke test
# after it); the stage has exactly ONE build_chroot_late call, so the setup
# script runs here, and the stage snapshot carries the glue.  (The shell
# block below and the WiFi tail after it ran after this wiring in the old
# layout too.)
./build_postlc3_chroot_setup_scripts_for_gnome.sh
hash -r

# --- GNOME Shell (the old postlc8 stage) ---------------------------------------
# The two gir rebuilds (mutter, gnome-desktop) are drop-in same-version
# rebuilds over the installs above, now with GObject introspection enabled,
# because gnome-shell's JS needs their typelibs.  pygobject is NOT rebuilt
# here: postlc2's build carries the Fil-C import-gi fix (PyGObject_MetaType
# is still NULL while the _gi module executes; the pizlonated CPython
# asserts zhasvalidcap() in Py_SET_TYPE), which everything below that runs
# g-ir-scanner or a python gi module at build time (libgweather's
# gen_locations_variant.py) relies on.

# mutter 46.9 rebuilt WITH introspection: Meta/Clutter/Cally/Cogl/Mtk-14
# typelibs.  gnome-shell needs all of them.  Drop-in same-version rebuild
# over the mutter install earlier in this stage (uses this stage's
# pizlonated-mutter tarball).
./build_postlc3_chroot_project_mutter_gir.sh
hash -r

# gnome-desktop 44.5 rebuilt WITH introspection: GnomeDesktop-4.0 /
# GnomeBG-4.0 typelibs for the shell's clock and animated backgrounds.
# Drop-in same-version rebuild over the gnome-desktop install above (uses
# the same pizlonated-gnome-desktop tarball, whose projeny patch carries
# the gnome-rr introspection Fil-C fixes).
./build_postlc3_chroot_project_gnome_desktop_gir.sh
hash -r

# at-spi2-core 2.60.5 full build (the postlc2 install was atk_only):
# libatspi + Atspi-2.0 gir/typelib + the ATK bridge.  Drop-in same-version
# rebuild over the atk_only install.
./build_postlc3_chroot_project_at_spi.sh
hash -r


# Location: geoclue first, then geocode-glib, then libgweather (its meson
# setup runs python3 'import gi', proving postlc2's pygobject import-gi
# fix).
./build_postlc3_chroot_project_geoclue.sh
hash -r

./build_postlc3_chroot_project_geocode_glib.sh
hash -r

./build_postlc3_chroot_project_libgweather.sh
hash -r

./build_postlc3_chroot_project_gnome_menus.sh
hash -r

# gnome-autoar: gnome-shell's extensions-tool subproject uses it.
./build_postlc3_chroot_project_gnome_autoar.sh
hash -r

# hicolor-icon-theme: the default fallback icon theme (data only), which
# completes the icon-theme lookup chain for the session.
./build_postlc3_chroot_project_hicolor_icon_theme.sh
hash -r

# The Shell runtime, last: mozjs (the engine, ~10-15 min), then gjs (the
# bindings, which build against mozjs), then gnome-shell (which needs the
# mutter/gnome-desktop/at-spi typelibs from above plus gjs).
./build_postlc3_chroot_project_mozjs.sh
hash -r

./build_postlc3_chroot_project_gjs.sh
hash -r

# Deferred NM + libnma gates: libnma and NetworkManager must build before
# gnome-control-center links libnm + libnma-gtk4, but gjs is only built
# here -- so the networkmanager/libnma scripts' "gjs can load the typelib"
# gates are deferred to this point, now that gjs exists.
gjs -c "const NM = imports.gi.NM; print('NM', NM.MAJOR_VERSION, NM.MINOR_VERSION);"
gjs -c "const NMA4 = imports.gi.NMA4; print('NMA4 loaded, WifiDialog ctor:', typeof NMA4.WifiDialog, NMA4.WifiDialog.\$gtype);"

./build_postlc3_chroot_project_gnome_shell.sh
hash -r

# --- WiFi tail: kernel HWSIM + firmware + NM boot wiring (the old
# postlc11 stage) ---------------------------------------------------------------

# The kernel rebuild with CONFIG_MAC80211_HWSIM=m.  This is a drop-in of
# part2's kernel block (the only kernel change the WiFi stage needs;
# iwlwifi/cfg80211/mac80211 are already =y).  It must run FIRST: the
# firmware script's manifest-driven selection reads
# /lib/modules/6.10.5/modules.builtin.modinfo, which this rebuild refreshes.
./build_postlc3_chroot_project_kernel.sh
hash -r

# The WiFi firmware subset (the manifest-driven iwlwifi ucode/pnvm
# selection) + the wens-signed wireless-regdb, all installed FLAT into
# /lib/firmware.
./build_postlc3_chroot_project_firmware.sh
hash -r

# The boot wiring -- the blfs networkmanager bootscript (S33networkmanager
# in rc2-5 after S20dbus + S25elogind, K28 on shutdown/halt), the
# S20network (dhcpcd) symlinks dropped from the managed runlevels (NM
# manages eth0 too; the script stays on disk for rescue), and
# /etc/NetworkManager/NetworkManager.conf.  The hostname is unaffected:
# localnet (rcS.d S08) sets it from /etc/hostname.  (The NetworkManager
# PACKAGE installs earlier in this stage; this boot wiring runs at the end
# of the stage because wiring the bootscripts is a setup step, not a build
# dependency.)
./build_postlc3_chroot_setup_scripts_for_networkmanager.sh
hash -r
