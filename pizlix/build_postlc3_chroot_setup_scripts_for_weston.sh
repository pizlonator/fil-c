#!/bin/bash

set -e
set -x

# This isn't really necessary, postlc should have done this.
cp -v etc/profile /etc/profile
mkdir -pv /etc/profile.d

cp -v etc/profile-xdg-runtime-dir.sh /etc/profile.d/xdg-runtime-dir.sh

groupadd seatd
usermod -a -G seatd pizlo

cp etc/seatd /etc/init.d
ln -s ../init.d/seatd /etc/rc.d/rc0.d/K20seatd
ln -s ../init.d/seatd /etc/rc.d/rc1.d/K20seatd
ln -s ../init.d/seatd /etc/rc.d/rc2.d/S80seatd
ln -s ../init.d/seatd /etc/rc.d/rc3.d/S80seatd
ln -s ../init.d/seatd /etc/rc.d/rc4.d/S80seatd
ln -s ../init.d/seatd /etc/rc.d/rc5.d/S80seatd
ln -s ../init.d/seatd /etc/rc.d/rc6.d/K20seatd

mkdir -v /etc/weston
cp -v etc/weston.ini /etc/weston
