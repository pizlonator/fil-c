#!/bin/bash

set -e
set -x

rm -rf pizlonated-systemd
tar -xf pizlonated-systemd.tar.gz
cd pizlonated-systemd
sed -i -e 's/GROUP="render"/GROUP="video"/' \
    -e 's/GROUP="sgx", //' rules.d/50-udev-default.rules.in
sed '/systemd-sysctl/s/^/#/' -i rules.d/99-systemd.rules.in
sed '/NETWORK_DIRS/s/systemd/udev/' -i src/basic/path-lookup.h
mkdir -pv build
cd build
meson setup .. \
    --prefix=/usr \
    --buildtype=release \
    -D mode=release \
    -D dev-kvm-mode=0660 \
    -D link-udev-shared=false \
    -D logind=false \
    -D vconsole=false
export udev_helpers=$(grep "'name' :" ../src/udev/meson.build | \
    awk '{print $3}' | tr -d ",'" | grep -v 'udevadm')
ninja udevadm systemd-hwdb \
    $(ninja -n | grep -Eo '(src/(lib)?udev|rules.d|hwdb.d)/[^ ]*') \
    $(realpath libudev.so --relative-to .) \
    $udev_helpers
install -vm755 -d {/usr/lib,/etc}/udev/{hwdb.d,rules.d,network}
install -vm755 -d /usr/{lib,share}/pkgconfig
install -vm755 udevadm /usr/bin/
install -vm755 systemd-hwdb /usr/bin/udev-hwdb
ln -svfn ../bin/udevadm /usr/sbin/udevd
cp -av libudev.so{,*[0-9]} /usr/lib/
install -vm644 ../src/libudev/libudev.h /usr/include/
install -vm644 src/libudev/*.pc /usr/lib/pkgconfig/
install -vm644 src/udev/*.pc /usr/share/pkgconfig/
install -vm644 ../src/udev/udev.conf /etc/udev/
install -vm644 rules.d/* ../rules.d/README /usr/lib/udev/rules.d/
install -vm644 $(find ../rules.d/*.rules \
    -not -name '*power-switch*') /usr/lib/udev/rules.d/
install -vm644 hwdb.d/* ../hwdb.d/{*.hwdb,README} /usr/lib/udev/hwdb.d/
install -vm755 $udev_helpers /usr/lib/udev
install -vm644 ../network/99-default.link /usr/lib/udev/network
tar -xvf ../../udev-lfs-20230818.tar.xz
make -f udev-lfs-20230818/Makefile.lfs install
tar -xf ../../systemd-man-pages-256.4.tar.xz \
    --no-same-owner --strip-components=1 \
    -C /usr/share/man --wildcards '*/udev*' '*/libudev*' \
        '*/systemd.link.5' \
        '*/systemd-'{hwdb,udevd.service}.8
sed 's|systemd/network|udev/network|' \
    /usr/share/man/man5/systemd.link.5 \
    > /usr/share/man/man5/udev.link.5
sed 's/systemd\(\\\?-\)/udev\1/' /usr/share/man/man8/systemd-hwdb.8 \
    > /usr/share/man/man8/udev-hwdb.8
sed 's|lib.*udevd|sbin/udevd|' \
    /usr/share/man/man8/systemd-udevd.service.8 \
    > /usr/share/man/man8/udevd.8
rm /usr/share/man/man*/systemd*
unset udev_helpers
cd ../..
rm -rf pizlonated-systemd
