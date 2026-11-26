#!/bin/bash

set -e
set -x

# This step takes ~90 mins (estimate: CLI tools + weston + GTK/GL + crypto libs).
# This stage merges the old postlc2..postlc5 stages: CLI/network/crypto tools,
# then Wayland/Weston, then the GTK/GL foundation, then the crypto/text
# libraries that depend on nettle/p11-kit/glib built earlier in the stage.

ulimit -c unlimited

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

export FILCSRC=..
test -d $FILCSRC/projects

test -e $LFS/sources/lfsbuildstate
lfsbuildstate=`cat $LFS/sources/lfsbuildstate`
test "x$lfsbuildstate" = "xpostlc"

SRCDIR=$PWD

echo "postlc2-part" > $LFS/sources/lfsbuildstate

# Seed the CA trust state (host side) before the chroot phase: GnuTLS is
# built ONCE in this stage, with the pre-seeded /etc/ssl/ca-bundle.crt as its
# --with-default-trust-store-file (there is no later trust-store rebuild
# stage), and curl's --with-ca-path=/etc/ssl/certs sees the split certs.
# The script installs etc/ssl/ca-bundle.crt into $LFS/etc/ssl, splits it
# into /etc/ssl/certs, and populates /etc/pki/anchors; it needs no chroot
# mounts (its openssl rehash only touches the certs directory), so it runs
# before build_mount.sh.  The snapshot written at the end of this stage
# carries the pre-seed forward.
./build_preseed_pizlix.sh

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc2_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

# Staging: everything this stage's chroot script consumes (the recovery
# chain never re-runs build_copy_stuff.sh, so each stage stages its own
# inputs).  The lines below are the union of the merged stages' staging
# (old postlc2 + postlc3 + postlc4 + postlc5), in build order; identical
# lines deduped.  The pizlonated-*.tar.gz wildcard carries the projeny
# products sub1 just built (including pizlonated-lcms2 from
# projects/lcms2.projeny).

cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources

# CLI/network/crypto tools (the old postlc2 stage).
cp -v which-2.21.tar.gz $LFS/sources
cp -v libpsl-0.23.3.tar.gz $LFS/sources
cp -v make-ca-1.16.1.tar.gz $LFS/sources
cp -v wget-1.25.0.tar.gz $LFS/sources
cp -v blfs-bootscripts-20240416.tar.xz $LFS/sources
cp -v build_postlc2_sub2_chroot.sh $LFS/sources
cp -v build_postlc2_chroot_project_dhcpcd.sh $LFS/sources
cp -v build_postlc2_chroot_project_libidn2.sh $LFS/sources

# Wayland/Weston + text-rendering foundation (the old postlc3 stage).
cp -v wayland-protocols-1.45.tar.xz $LFS/sources
cp -v xkeyboard-config-2.42.tar.xz $LFS/sources
cp -v pixman-0.43.4.tar.gz $LFS/sources
cp -v packaging-24.1.tar.gz $LFS/sources
cp -v daemon-0.6.4.tar.gz $LFS/sources
cp -v build_postlc2_chroot_project_glib.sh $LFS/sources
cp -v build_postlc2_chroot_subproject_gobject_introspection.sh $LFS/sources
cp -v build_postlc2_chroot_project_cairo.sh $LFS/sources
cp -v build_postlc2_chroot_project_weston.sh $LFS/sources
cp -v build_postlc2_chroot_project_seatd.sh $LFS/sources
cp -v build_postlc2_chroot_project_libdrm.sh $LFS/sources
cp -v build_postlc2_chroot_setup_scripts_for_weston.sh $LFS/sources
cp -v etc/weston.ini $LFS/sources/etc
cp -v etc/project2030.jpg $LFS/sources/etc
cp -v etc/profile $LFS/sources/etc
cp -v etc/profile-xdg-runtime-dir.sh $LFS/sources/etc
cp -v etc/seatd $LFS/sources/etc
cp -v build_postlc2_chroot_project_freetype.sh $LFS/sources
cp -v build_postlc2_chroot_project_graphite.sh $LFS/sources
cp -v build_postlc2_chroot_project_fontconfig.sh $LFS/sources
cp -v build_postlc2_chroot_project_harfbuzz.sh $LFS/sources
cp -v build_postlc2_chroot_project_lcms2.sh $LFS/sources

# GTK/GL foundation (the old postlc4 stage).
cp -v fribidi-1.0.15.tar.xz $LFS/sources
cp -v shared-mime-info-2.4.tar.gz $LFS/sources
cp -v pycairo-1.26.1.tar.gz $LFS/sources
cp -v build_postlc2_chroot_project_graphene.sh $LFS/sources
cp -v build_postlc2_chroot_project_pygobject.sh $LFS/sources
cp -v iso-codes_4.16.0.orig.tar.xz $LFS/sources
cp -v Mako-1.3.5.tar.gz $LFS/sources
cp -v build_postlc2_chroot_project_mesa.sh $LFS/sources
cp -v libepoxy-1.5.10.tar.xz $LFS/sources
cp -v build_postlc2_chroot_project_pango.sh $LFS/sources
cp -v build_postlc2_chroot_project_gdk-pixbuf.sh $LFS/sources
cp -v build_postlc2_chroot_project_gtk4.sh $LFS/sources
cp -v build_postlc2_chroot_project_at-spi2-core.sh $LFS/sources
cp -v build_postlc2_chroot_project_gtk3.sh $LFS/sources

cp -rv filc-llvm-config $LFS/sources/filc-llvm-config

# Crypto/text libraries (the old postlc5 stage).
cp -v sqlite-autoconf-3460100.tar.gz $LFS/sources
cp -v gsettings-desktop-schemas-46.1.tar.xz $LFS/sources
cp -v libsecret-0.21.4.tar.xz $LFS/sources
cp -v libseccomp-2.5.5.tar.gz $LFS/sources
cp -v bubblewrap-0.9.0.tar.xz $LFS/sources
cp -v unifdef-2.12.tar.gz $LFS/sources
cp -v aspell6-en-2020.12.07-0.tar.bz2 $LFS/sources

./build_chroot_late.sh /sources/build_postlc2_sub2_chroot.sh

mkdir -pv $LFS/usr/share/fonts
cp -rv dejavu $LFS/usr/share/fonts/

echo "postlc2" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc2.tar.gz --exclude='var/coredumps/*' .

echo Post-libc part 2 OK
