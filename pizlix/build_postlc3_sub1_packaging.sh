#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

# Packaging for the merged postlc3 stage (the old postlc6-minus-LibreOffice
# + postlc7 + postlc8 + postlc11 sub1 lists, unioned in build order;
# identical lines deduped).  NOT packaged here (postlc2 packages them once,
# and their /sources copies carry forward): mesa, gtk4, xkbcommon, cairo,
# lcms2, pygobject -- the old layout re-packaged several of
# these mid-chain because older snapshots predated folded fixes; the current
# projeny/checked-in packages all carry the fixes, so each is built ONCE in
# postlc2 (cairo with X11 disabled there, then re-built here with X11
# enabled from the same tarball; xkbcommon likewise).

rm -vf projects/*/pizlonated-*.tar.gz

# Download large tarballs that are too big to commit to git.
# Each tarball is verified with SHA256 (using sha256sum from coreutils).
download_tarball() {
    local url="$1"
    local filename="$2"
    local expected_sha256="$3"
    local filepath="pizlix/$filename"

    if [ -f "$filepath" ]; then
        local actual_sha256=$(sha256sum "$filepath" | cut -d' ' -f1)
        if [ "$actual_sha256" = "$expected_sha256" ]; then
            echo "Already have $filename (sha256 verified)"
            return 0
        fi
        echo "sha256 mismatch for $filename, re-downloading"
        rm -f "$filepath"
    fi

    echo "Downloading $filename from $url"
    # Retry + resume: transient CDN flakiness killed a 4-hour one-shot build
    # here (curl error 92, "HTTP/2 stream not closed cleanly", from
    # archives.boost.org mid-transfer).  Plain --retry does not cover such
    # errors, so --retry-all-errors retries on any error, and --continue-at -
    # resumes the partial file from where the failed attempt left off
    # instead of restarting a 120MB download from byte 0.
    curl -L --retry 5 --retry-all-errors --continue-at - -o "$filepath" "$url"

    local actual_sha256=$(sha256sum "$filepath" | cut -d' ' -f1)
    if [ "$actual_sha256" != "$expected_sha256" ]; then
        echo "ERROR: sha256 mismatch for $filename after download"
        echo "Expected: $expected_sha256"
        echo "Actual:   $actual_sha256"
        return 1
    fi
    echo "Downloaded $filename (sha256 verified)"
}

# X11 libraries + LibreOffice dependencies (the old postlc6 stage; the
# LibreOffice tarball downloads moved to postlc5's sub1, and the cairo
# repackage is gone -- postlc2 packages cairo once and this stage's cairo
# X11 rebuild re-uses that tarball).
./package-source.sh projects/abseil-cpp-20260107.1 pizlonated-abseil
./package-source.sh projects/libxslt-1.1.42 pizlonated-libxslt
./package-source.sh projects/zip-3.0 pizlonated-zip
./package-source.sh projects/unzip-6.0 pizlonated-unzip

# Boost (built in this stage's chroot; the download used to live with the
# LibreOffice downloads in the old postlc6 sub1).
download_tarball "https://archives.boost.org/release/1.86.0/source/boost_1_86_0.tar.bz2" \
    "boost_1_86_0.tar.bz2" \
    "1bed88e40401b2cb7a1f76d4bab499e352fa4d0c5f31c0dbae64e24d34d7513b"

# GNOME desktop (the old postlc7 stage): session, compositor, and support
# libraries.  xkbcommon's X11 rebuild for mutter happens in this stage's
# chroot, re-using the tarball postlc2 packaged.

# (The old layout re-packaged mesa/gtk4 here because older snapshots carried
# an unfixed GL/EGL stack; postlc2 now builds mesa from the fixed projeny
# package and gtk4 against its headers, once.)

filc/projeny package projects/dbus.projeny projects/dbus/pizlonated-dbus.tar.gz
filc/projeny package projects/Linux-PAM.projeny projects/Linux-PAM/pizlonated-pam.tar.gz
filc/projeny package projects/json-glib.projeny projects/json-glib/pizlonated-json-glib.tar.gz
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

# The WiFi USERSPACE stack + libnma, packaged here so the stack is installed
# before either consumer runs: g-c-c (later in this stage) links libnm +
# libnma-gtk4 and gnome-shell (also this stage) needs libnm + the NMA4
# typelib.  Internal build order is libnl -> libndp -> wpa_supplicant
# -> NetworkManager -> libnma (see sub2 for the rationale); packaging
# order is irrelevant (all five are packaged on the host).
filc/projeny package projects/libnl.projeny projects/libnl/pizlonated-libnl.tar.gz
filc/projeny package projects/libndp.projeny projects/libndp/pizlonated-libndp.tar.gz
filc/projeny package projects/wpa_supplicant.projeny projects/wpa_supplicant/pizlonated-wpa_supplicant.tar.gz
filc/projeny package projects/networkmanager.projeny projects/networkmanager/pizlonated-networkmanager.tar.gz
filc/projeny package projects/dnsmasq.projeny projects/dnsmasq/pizlonated-dnsmasq.tar.gz
filc/projeny package projects/libnma.projeny projects/libnma/pizlonated-libnma.tar.gz

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

# GNOME Shell (the old postlc8 stage).  Everything the shell chroot scripts
# extract is packaged here.  All of the new packages are projeny projects
# (packaged with the projeny tool).  The one exception is at-spi2-core,
# which is not a projeny project: it is packaged from its checked-in source
# directory with package-source.sh (the same fixed tree postlc2 packaged
# for its atk_only build; the rm above deleted that tarball, so repackage
# it here for the full gir build).

# The Shell runtime: mozjs (the JS engine) and gjs (the JS bindings).
filc/projeny package projects/mozjs.projeny projects/mozjs/pizlonated-mozjs.tar.gz
filc/projeny package projects/gjs.projeny projects/gjs/pizlonated-gjs.tar.gz

filc/projeny package projects/geoclue.projeny projects/geoclue/pizlonated-geoclue.tar.gz

# Location stack (libgweather's meson setup runs python3 'import gi',
# which works because postlc2's pygobject build carries the Fil-C
# import-gi fix).
filc/projeny package projects/geocode-glib.projeny projects/geocode-glib/pizlonated-geocode-glib.tar.gz
filc/projeny package projects/libgweather.projeny projects/libgweather/pizlonated-libgweather.tar.gz

# Menus and archive helpers.
filc/projeny package projects/gnome-menus.projeny projects/gnome-menus/pizlonated-gnome-menus.tar.gz
filc/projeny package projects/gnome-autoar.projeny projects/gnome-autoar-0.4.4/pizlonated-gnome-autoar.tar.gz

# The Shell itself.
filc/projeny package projects/gnome-shell.projeny projects/gnome-shell/pizlonated-gnome-shell.tar.gz

# hicolor-icon-theme (the default fallback icon theme;
# fixes the icon-theme lookup chain, which Adwaita alone does not provide).
filc/projeny package projects/hicolor-icon-theme.projeny projects/hicolor-icon-theme/pizlonated-hicolor-icon-theme.tar.gz

# at-spi2-core is not a projeny project (checked-in source dir; the same
# pattern as postlc2's packaging).  This stage rebuilds it WITH
# introspection (libatspi Atspi-2.0 gir/typelib + the ATK bridge) on top of
# postlc2's atk_only install.  The rm above deleted the postlc2 tarball, so
# repackage it here.
./package-source.sh projects/at-spi2-core-2.60.5 pizlonated-at-spi2-core

# NOT packaged here (drop-in same-version gir rebuilds re-use tarballs
# already in /sources from postlc2):
# - pizlonated-pygobject     (postlc2's build carries the import-gi fix)
# The mutter gir rebuild re-uses the pizlonated-mutter tarball staged above.
# The gnome_desktop_gir pass re-uses the pizlonated-gnome-desktop tarball
# packaged in this same stage; it carries the gnome-rr introspection fixes
# (the gnome_rr_dpms_mode_get_type static-enum GType fix and the gnome-rr
# gir meson wiring), so the GnomeRR-4.0 introspection dumper works.

# WiFi tail (the old postlc11 stage): the firmware tarball + the
# wireless-regdb projeny package.
#
# linux-firmware-20241017.tar.xz (379MB compressed) is the LFS-12.2-era
# linux-firmware snapshot.  Only a manifest-driven subset (~195MB) is
# installed onto the target by build_postlc3_chroot_project_firmware.sh
# (the iwlwifi ucode/pnvm families the built-in drivers actually declare),
# but the FULL tarball must be present in /sources for that selection to
# pick from.  The ucode files sit FLAT in the tarball root, which matches
# how the iwlwifi driver requests firmware (flat /lib/firmware, no
# subdirectory).  wireless-regdb is the projeny package
# projects/wireless-regdb.projeny (URL-mode), packaged below so the driver
# can stage pizlonated-wireless-regdb.tar.gz into /sources for the chroot
# firmware script.
download_tarball "https://cdn.kernel.org/pub/linux/kernel/firmware/linux-firmware-20241017.tar.xz" \
    "linux-firmware-20241017.tar.xz" \
    "a26c38ef5a83272f2b98ce8bf8ca1865a852a3deea49ce5a8dd804b914351273"

# The wireless-regdb projeny package (URL-mode; the git.kernel.org snapshot
# URL and its blake3 live in the .projeny file).  projeny auto-setups the
# checkout on a fresh clone (download + blake3 verify + unpack), so the
# pizlonated tarball builds even where pizlix/ has no cached copy (and it
# sends a User-Agent, since git.kernel.org 403s UA-less clients).
filc/projeny package projects/wireless-regdb.projeny projects/wireless-regdb/pizlonated-wireless-regdb.tar.gz

ls -la pizlix/boost_1_86_0.tar.bz2 \
       pizlix/linux-firmware-20241017.tar.xz \
       projects/wireless-regdb/pizlonated-wireless-regdb.tar.gz
