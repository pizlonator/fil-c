#!/bin/bash

set -e
set -x

# This step takes ~105 mins (estimate: LibreOffice + gdm).
# This stage merges the old postlc6's LibreOffice part with the old postlc12
# stage: LibreOffice, the audio trio (libogg/libvorbis/libcanberra) and gdm,
# plus the boot-to-GNOME wiring.  (The old postlc12 libtool rebuild is gone:
# the postlc stage's part2 has built the fixed libltdl from the projeny
# package since the 2026-11-22 conversion.)

ulimit -c unlimited

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

export FILCSRC=..
test -d $FILCSRC/projects

test -e $LFS/sources/lfsbuildstate
lfsbuildstate=`cat $LFS/sources/lfsbuildstate`
test "x$lfsbuildstate" = "xpostlc4"

SRCDIR=$PWD

echo "postlc5-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc5_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

# Staging: everything this stage's chroot script consumes (the recovery
# chain never re-runs build_copy_stuff.sh, so each stage stages its own
# inputs).  The lines below are the old postlc6 driver's LibreOffice
# staging plus the old postlc12 driver's staging.

# LibreOffice: the four source tarballs, the pre-downloaded external
# dependency tarballs (the chroot has no network access, so `make fetch`
# cannot download the ~136 external tarballs inside the chroot), the Fil-C
# patch and the autogen flags.
cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources
cp -v libreoffice-24.8.0.3.tar.xz $LFS/sources
cp -v libreoffice-dictionaries-24.8.0.3.tar.xz $LFS/sources
cp -v libreoffice-help-24.8.0.3.tar.xz $LFS/sources
cp -v libreoffice-translations-24.8.0.3.tar.xz $LFS/sources
mkdir -p $LFS/sources/libreoffice-external-tarballs
cp -v libreoffice-external-tarballs/* $LFS/sources/libreoffice-external-tarballs/
cp -v libreoffice-filc.patch $LFS/sources
cp -v libreoffice-filc.autogen.flags $LFS/sources

# The login-manager half: the chroot sub2 script, the project scripts (the
# LibreOffice build script plus libogg/libvorbis/libcanberra/gdm) and the
# setup script.
cp -v build_postlc5_sub2_chroot.sh $LFS/sources
cp -v build_postlc5_chroot_project_*.sh $LFS/sources
cp -v build_postlc5_chroot_setup_scripts_for_gdm.sh $LFS/sources

# The blfs-bootscripts tarball (the xdm bootscript is installed out of it by
# the setup script; postlc3's NetworkManager wiring stages this same
# tarball, but this stage is self-contained).
cp -v blfs-bootscripts-20240416.tar.xz $LFS/sources

# The tracked GDM inittab (etc/inittab-gdm) + the greeter/user shell wrapper,
# deployed by the setup script out of /sources/etc (the recovery chain does
# not re-run build_copy_stuff.sh, so the stage stages its own copies; the
# full chain gets the text-mode inittab through build_copy_stuff.sh as well,
# and this stage replaces it with the GDM one).
mkdir -pv $LFS/sources/etc/gnome
cp -v etc/inittab-gdm $LFS/sources/etc/inittab
cp -v etc/gnome/pizlix-shell.sh $LFS/sources/etc/gnome

./build_chroot_late.sh /sources/build_postlc5_sub2_chroot.sh

echo "postlc5" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc5.tar.gz --exclude='var/coredumps/*' --exclude='hostbuild/*' .

echo Post-libc part 5 OK
