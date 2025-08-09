#!/bin/bash

# tar up completed system images to send to website, with READMEs

rm -f root/toybox-* root/*.tgz
for i in root/*/fs/bin/toybox
do
  cp $i root/toybox-$(echo $i | sed 's@root/\([^/]*\)/.*@\1@') || exit 1
done

for i in root/*/run-qemu.sh
do
  i=${i%/run-qemu.sh} j=${i#root/}
  [ ! -e "$i" ] && continue
  # Add README, don't include "fs" dir (you can extract it from cpio.gz)
  cp mkroot/README.root $i/docs/README &&
  tar cvzfC $i.tgz root --exclude=fs $j || exit 1
done

# Generate top level README
KVERS=$(toybox sed -n '3s@# Linux/[^ ]* \(.*\) Kernel Configuration@\1@p' root/*/docs/linux-fullconfig)
cat > root/README << EOF
Bootable system images created by:

  mkroot/mkroot.sh LINUX=~/linux CROSS=allnonstop

Each system image is built from two packages: toybox and linux.
The run-qemu.sh script in each tarball should boot the system
to a shell prompt under qemu, exit from that shell to shut down the
virtual system and stop the emulator.

See https://landley.net/toybox/faq.html#mkroot for details.

Built from mkroot $(git describe --tags), and Linux $KVERS with patches in linux-patches/
EOF

if [ $# -eq 2 ]
then
  scp root/toybox-* "$1/$2/" &&
  scp root/*.tgz root/README "$1/mkroot/$2/"
fi
