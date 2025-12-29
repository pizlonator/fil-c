#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

tar -xf sqlite-autoconf-3460100.tar.gz
cd sqlite-autoconf-3460100
./configure --prefix=/usr     \
            --disable-static  \
            --enable-fts{4,5} \
            CPPFLAGS="-D SQLITE_ENABLE_COLUMN_METADATA=1 \
                      -D SQLITE_ENABLE_UNLOCK_NOTIFY=1   \
                      -D SQLITE_ENABLE_DBSTAT_VTAB=1     \
                      -D SQLITE_SECURE_DELETE=1"
make
make install
cd ..
rm -rf sqlite-autoconf-3460100
hash -r

tar -xf pizlonated-gnutls.tar.gz
cd pizlonated-gnutls
./configure --prefix=/usr \
            --docdir=/usr/share/doc/gnutls-3.8.7.1 \
            --with-default-trust-store-pkcs11="pkcs11:" \
            --disable-hardware-acceleration
make
make install
cd ..
rm -rf pizlonated-gnutls
hash -r

tar -xf gsettings-desktop-schemas-46.1.tar.xz
cd gsettings-desktop-schemas-46.1
sed -i -r 's:"(/system):"/org/gnome\1:g' schemas/*.in
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf gsettings-desktop-schemas-46.1
hash -r

tar -xf pizlonated-glib-networking.tar.gz
cd pizlonated-glib-networking
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized -D libproxy=disabled
ninja
ninja install
cd ../..
rm -rf pizlonated-glib-networking
hash -r

tar -xf pizlonated-libsoup.tar.gz
cd pizlonated-libsoup
mkdir -v build
cd build
# FIXME: Install vala and enable vapi!
meson setup --prefix=/usr --buildtype=debugoptimized -D vapi=disabled -D gssapi=disabled -Dsysprof=disabled --wrap-mode=nofallback ..
ninja
ninja install
cd ../..
rm -rf pizlonated-libsoup
hash -r

tar -xf libsecret-0.21.4.tar.xz
cd libsecret-0.21.4
# Cannot use the name "build" because there's already a directory by that name in the libsecret
# distro.
mkdir -v bld
cd bld
meson setup .. --prefix=/usr --buildtype=debugoptimized -D gtk_doc=false -D crypto=gnutls -D vapi=false -D manpage=false
ninja
ninja install
cd ../..
rm -rf libsecret-0.21.4
hash -r

tar -xf libseccomp-2.5.5.tar.gz
cd libseccomp-2.5.5
./configure --prefix=/usr --disable-static
make
make install
cd ..
rm -rf libseccomp-2.5.5
hash -r

tar -xf bubblewrap-0.9.0.tar.xz
cd bubblewrap-0.9.0
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized
ninja
ninja install
cd ../..
rm -rf bubblewrap-0.9.0
hash -r

tar -xf unifdef-2.12.tar.gz
cd unifdef-2.12
make
make prefix=/usr install
cd ..
rm -rf unifdef-2.12
hash -r


