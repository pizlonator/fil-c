#!/bin/bash

set -e
set -x

ulimit -c unlimited

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

export FILCSRC=..
test -d $FILCSRC/projects

test -e $LFS/sources/lfsbuildstate
lfsbuildstate=`cat $LFS/sources/lfsbuildstate`
test "x$lfsbuildstate" = "xpostlc11"

SRCDIR=$PWD

echo "postlc12-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc12_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

# Sprint D of the WiFi/GDM port: postlc12 is "the login manager" stage --
# libtool (the libltdl fix), libogg, libvorbis, libcanberra (the audio trio
# gdm's unconditional libcanberra-gtk3 dependency pulls in) and gdm 46.2,
# plus the boot-to-GNOME wiring (the gdm user, /var/lib/gdm, the blfs xdm
# bootscript + sysconfig, the runlevel-5 inittab, and the
# wayland-sessions/pizlix-gnome.desktop user-session entry).
cp -v build_postlc12_sub2_chroot.sh $LFS/sources
cp -v build_postlc12_chroot_project_*.sh $LFS/sources
cp -v build_postlc12_chroot_setup_scripts_for_gdm.sh $LFS/sources

# The libtool rebuild needs its Fil-C patch next to the tarball in /sources.
cp -v libtool-2.4.7-filc-ltdl-1.patch $LFS/sources

# The blfs-bootscripts tarball (the xdm bootscript is installed out of it by
# the setup script; postlc11's networkmanager wiring already stages this
# same tarball, but this stage is self-contained).
cp -v blfs-bootscripts-20240416.tar.xz $LFS/sources

# The tracked inittab + the greeter/user shell wrapper, deployed by the
# setup script out of /sources/etc (the recovery chain does not re-run
# build_copy_stuff.sh, so the stage stages its own copies; the full chain
# gets the same files through build_copy_stuff.sh as well).
mkdir -pv $LFS/sources/etc/gnome
cp -v etc/inittab $LFS/sources/etc
cp -v etc/gnome/pizlix-shell.sh $LFS/sources/etc/gnome

./build_chroot_late.sh /sources/build_postlc12_sub2_chroot.sh

echo "postlc12" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc12.tar.gz --exclude='var/coredumps/*' --exclude='hostbuild/*' .

echo Post-libc part 12 OK
