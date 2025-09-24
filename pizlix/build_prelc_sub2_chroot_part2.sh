#!/yolo/bin/bash

set -e
set -x

ulimit -c unlimited

touch /var/log/{btmp,lastlog,faillog,wtmp}
chgrp -v utmp /var/log/lastlog
chmod -v 664 /var/log/lastlog
chmod -v 600 /var/log/btmp

cd /sources

tar -xf gettext-0.22.5.tar.xz
cd gettext-0.22.5
./configure --disable-shared
make
cp -v gettext-tools/src/{msgfmt,msgmerge,xgettext} /yolo/bin
cd ..
rm -rf gettext-0.22.5

tar -xf bison-3.8.2.tar.xz
cd bison-3.8.2
./configure --prefix=/yolo \
            --docdir=/yolo/share/doc/bison-3.8.2
make
make install
cd ..
rm -rf bison-3.8.2

tar -xf perl-5.40.0.tar.xz
cd perl-5.40.0
sh Configure -des \
    -D prefix=/yolo \
    -D vendorprefix=/yolo \
    -D useshrplib \
    -D privlib=/yolo/lib/perl5/5.40/core_perl \
    -D archlib=/yolo/lib/perl5/5.40/core_perl \
    -D sitelib=/yolo/lib/perl5/5.40/site_perl \
    -D sitearch=/yolo/lib/perl5/5.40/site_perl \
    -D vendorlib=/yolo/lib/perl5/5.40/vendor_perl \
    -D vendorarch=/yolo/lib/perl5/5.40/vendor_perl \
    -D installprefix=/yolo \
    -D bin=/yolo/bin \
    -D siteprefix=/yolo \
    -D man1dir=/yolo/share/man/man1 \
    -D man3dir=/yolo/share/man/man3 \
    -D scriptdir=/yolo/bin \
    -D sitehtml1dir=/yolo/share/man/man1 \
    -D sitehtml3dir=/yolo/share/man/man3 \
    -D sitescript=/yolo/bin \
    -D sitebin=/yolo/bin \
    -D siteman1dir=/yolo/share/man/man1 \
    -D siteman3dir=/yolo/share/man/man3
make
make install
cd ..
rm -rf perl-5.40.0

tar -xf Python-3.12.5.tar.xz
cd Python-3.12.5
./configure --prefix=/yolo \
    --enable-shared \
    --without-ensurepip
make
make install
cd ..
rm -rf Python-3.12.5

tar -xf texinfo-7.1.tar.xz
cd texinfo-7.1
./configure --prefix=/yolo
make
make install
cd ..
rm -rf texinfo-7.1

tar -xf util-linux-2.40.2.tar.xz
cd util-linux-2.40.2
mkdir -pv /var/lib/hwclock
./configure --libdir=/yolo/lib \
    --prefix=/yolo \
    --runstatedir=/run \
    --disable-chfn-chsh \
    --disable-login \
    --disable-nologin \
    --disable-su \
    --disable-setpriv \
    --disable-runuser \
    --disable-pylibmount \
    --disable-static \
    --disable-liblastlog2 \
    --without-python \
    ADJTIME_PATH=/var/lib/hwclock/adjtime \
    --docdir=/yolo/share/doc/util-linux-2.40.2
make
make install
cd ..
rm -rf util-linux-2.40.2

rm -rf /yolo/share/{info,man,doc}/*
find /yolo/{lib,libexec} -name \*.la -delete
rm -rf /tools

tar -xf iana-etc-20240806.tar.gz
cd iana-etc-20240806
cp services protocols /etc
cd ..
rm -rf iana-etc-20240806

# Option #3 (even better?)
#
# At this point, build yolo glibc, install it in /yolo, and move everything from /usr to /yolo. Maybe
# literally rename /usr to /yolo.
#
# Then slam down the Fil-C compiler!
#
# If this is to work, then theoretically, the iana and man things above should be able to run after
# this next step. Or rather, we build yolo glibc first before man/iana, then we move /usr to /yolo,
# then we do man/iana, and then we slam down the Fil-C compiler and build user glibc.
#
# Even better: extract the parts of yololand that needs to actually survive (so for libc/libstdc++
# and the gcc runtime etc) into /yolo, and keep the rest (for now) in /yolo-tools. That way, we can
# kill /yolo-tools after a time.
#
# The problem with this approach: the /usr directory contains much more than just binaries and so's.
# Everything we've built so far assumes that it's located in /usr. So, we cannot just move it from
# /usr. And we can't really keep it in /usr either.

