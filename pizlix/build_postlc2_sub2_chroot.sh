#!/bin/bash

set -e
set -x

ulimit -c unlimited

# The merged postlc2 chroot script (the old postlc2..postlc5 stages' sub2
# scripts, concatenated in build order; identical content deduped).  The
# cross-stage rebuilds the old layout needed are gone: GnuTLS is built ONCE
# with the pre-seeded trust bundle (below), and lcms2 comes from its projeny
# package instead of the plain tarball.

cd /sources

# --- CLI/network/crypto tools (the old postlc2 stage) ------------------------

./build_postlc2_chroot_project_dhcpcd.sh
hash -r

tar -xf pizlonated-patchelf.tar.gz
cd pizlonated-patchelf
./configure --prefix=/usr \
            --docdir=/usr/share/doc/patchelf-0.19.3
make
make install
cd ..
rm -rf pizlonated-patchelf
hash -r

tar -xf pizlonated-libunistring.tar.gz
cd pizlonated-libunistring
./configure --prefix=/usr    \
            --disable-static \
            --docdir=/usr/share/doc/libunistring-1.5
make
make install
cd ..
rm -rf pizlonated-libunistring
hash -r

./build_postlc2_chroot_project_libidn2.sh
hash -r

tar -xf libpsl-0.23.3.tar.gz
cd libpsl-0.23.3
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=release
ninja
ninja install
cd ../..
rm -rf libpsl-0.23.3
hash -r

tar -xf pizlonated-libtasn1.tar.gz
cd pizlonated-libtasn1
./configure --prefix=/usr --disable-static
make
make install
make -C doc/reference install-data-local
cd ..
rm -rf pizlonated-libtasn1
hash -r

tar -xf pizlonated-p11-kit.tar.gz
cd pizlonated-p11-kit
sed '20,$ d' -i trust/trust-extract-compat
cat >> trust/trust-extract-compat << "EOF"
# Copy existing anchor modifications to /etc/ssl/local
/usr/libexec/make-ca/copy-trust-modifications

# Update trust stores
/usr/sbin/make-ca -r
EOF
mkdir -v p11-build
cd p11-build
meson setup ..            \
      --prefix=/usr       \
      --buildtype=release \
      -D trust_paths=/etc/pki/anchors
ninja
ninja install
ln -sfv /usr/libexec/p11-kit/trust-extract-compat \
        /usr/bin/update-ca-certificates
cd ../..
rm -rf pizlonated-p11-kit
hash -r

tar -xf make-ca-1.16.1.tar.gz
cd make-ca-1.16.1
make install
install -vdm755 /etc/ssl/local
cd ..
rm -rf make-ca-1.16.1
hash -r

tar -xf pizlonated-nghttp2.tar.gz
cd pizlonated-nghttp2
./configure --prefix=/usr     \
            --disable-static  \
            --enable-lib-only \
            --docdir=/usr/share/doc/nghttp2-1.71.0
make
make install
cd ..
rm -rf pizlonated-nghttp2
hash -r

tar -xf pizlonated-curl.tar.gz
cd pizlonated-curl
# --with-ca-path points at the /etc/ssl/certs directory the driver's
# build_preseed_pizlix.sh run split out of the CA bundle before this chroot
# phase, so curl sees the real anchors from its first build.
./configure --prefix=/usr                           \
            --disable-static                        \
            --with-openssl                          \
            --enable-threaded-resolver              \
            --with-ca-path=/etc/ssl/certs
make
make install
rm -rf docs/examples/.deps
find docs \( -name Makefile\* -o  \
             -name \*.1       -o  \
             -name \*.3       -o  \
             -name CMakeLists.txt \) -delete
cp -v -R docs -T /usr/share/doc/curl-8.23.0
cd ..
rm -rf pizlonated-curl
hash -r

tar -xf pizlonated-openssh.tar.gz
cd pizlonated-openssh
install -v -g sys -m700 -d /var/lib/sshd &&
groupadd -g 50 sshd
useradd  -c 'sshd PrivSep' \
         -d /var/lib/sshd  \
         -g sshd           \
         -s /bin/false     \
         -u 50 sshd
./configure --prefix=/usr                            \
            --sysconfdir=/etc/ssh                    \
            --with-privsep-path=/var/lib/sshd        \
            --with-default-path=/usr/bin             \
            --with-superuser-path=/usr/sbin:/usr/bin \
            --with-pid-dir=/run
make
make install
install -v -m755    contrib/ssh-copy-id /usr/bin
install -v -m644    contrib/ssh-copy-id.1 \
                    /usr/share/man/man1
install -v -m755 -d /usr/share/doc/openssh-10.6p2
install -v -m644    INSTALL LICENCE OVERVIEW README* \
                    /usr/share/doc/openssh-10.6p2
cd ..
rm -rf pizlonated-openssh
hash -r

tar -xf pizlonated-emacs.tar.gz
cd pizlonated-emacs
./configure --without-all --without-x --with-dumping=none --with-pdumper=no --with-unexec=no --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-emacs
hash -r
cp -rv emacs-lisp/* /usr/share/emacs/site-lisp/

tar -xf pizlonated-sudo.tar.gz
cd pizlonated-sudo
./configure --prefix=/usr              \
            --libexecdir=/usr/lib      \
            --with-secure-path         \
            --with-env-editor          \
            --docdir=/usr/share/doc/sudo-1.9.17p2 \
            --with-passprompt="[sudo] password for %p: "
make
make install
cd ..
rm -rf pizlonated-sudo
hash -r
cp -rv etc/sudoers /etc

tar -xf pizlonated-pcre2.tar.gz
cd pizlonated-pcre2
./configure --prefix=/usr                       \
            --docdir=/usr/share/doc/pcre2-10.49 \
            --enable-unicode                    \
            --disable-jit                       \
            --enable-pcre2-16                   \
            --enable-pcre2-32                   \
            --enable-pcre2grep-libz             \
            --enable-pcre2grep-libbz2           \
            --enable-pcre2test-libreadline      \
            --disable-static
make
make install
cd ..
rm -rf pizlonated-pcre2
hash -r

tar -xf wget-1.25.0.tar.gz
cd wget-1.25.0
# OpenSSL 4.0 removed SSLv3_client_method(); defining OPENSSL_NO_SSL3_METHOD
# makes wget's #ifndef OPENSSL_NO_SSL3_METHOD guard skip the SSLv3 case, whose
# NULL meth wget already handles gracefully (same treatment the /opt/fil wget
# build gets in optfil/build_opt.sh).
CPPFLAGS='-DOPENSSL_NO_SSL3_METHOD' ./configure --prefix=/usr      \
            --sysconfdir=/etc  \
            --with-ssl=openssl
make
make install
cd ..
rm -rf wget-1.25.0
hash -r

tar -xf pizlonated-git.tar.gz
cd pizlonated-git
./configure --prefix=/usr \
            --with-gitconfig=/etc/gitconfig \
            --with-python=python3 \
            --with-libpcre2
make
make perllibdir=/usr/lib/perl5/5.40/site_perl install
cd ..
rm -rf pizlonated-git
hash -r

tar -xf pizlonated-libuv.tar.gz
cd pizlonated-libuv
./configure --prefix=/usr --disable-static
make
make install
cd ..
rm -rf pizlonated-libuv
hash -r

tar -xf pizlonated-icu.tar.gz
cd pizlonated-icu/source
./configure --prefix=/usr
make
make install
cd ../..
rm -rf pizlonated-icu
hash -r

tar -xf pizlonated-libxml2.tar.gz
cd pizlonated-libxml2
./configure --prefix=/usr           \
            --sysconfdir=/etc       \
            --disable-static        \
            --with-history          \
            --with-icu              \
            PYTHON=/usr/bin/python3 \
            --docdir=/usr/share/doc/libxml2-2.15.4
make
make install
rm -vf /usr/lib/libxml2.la
sed '/libs=/s/xml2.*/xml2"/' -i /usr/bin/xml2-config
cd ..
rm -rf pizlonated-libxml2
hash -r

tar -xf pizlonated-libarchive.tar.gz
cd pizlonated-libarchive
./configure --prefix=/usr --without-expat --without-nettle
make
make install
cd ..
rm -rf pizlonated-libarchive
hash -r

tar -xf blfs-bootscripts-20240416.tar.xz
cd blfs-bootscripts-20240416
make install-service-dhcpcd
make install-sshd
cd ..
rm -rf blfs-bootscripts-20240416

tar -xf which-2.21.tar.gz
cd which-2.21
./configure --prefix=/usr
make
make install
cd ..
rm -rf which-2.21
hash -r

tar -xf pizlonated-cmake.tar.gz
cd pizlonated-cmake
./bootstrap --prefix=/usr        \
            --system-libs        \
            --bootstrap-system-libuv \
            --mandir=/share/man  \
            --no-system-jsoncpp  \
            --no-system-cppdap   \
            --no-system-librhash \
            --docdir=/share/doc/cmake-3.30.2 \
            CFLAGS="-O2" \
            CXXFLAGS="-O2"
make
make install
cd ..
rm -rf pizlonated-cmake
hash -r

tar -xf pizlonated-brotli.tar.gz
cd pizlonated-brotli
mkdir build
cd build
cmake -D CMAKE_INSTALL_PREFIX=/usr \
      -D CMAKE_BUILD_TYPE=Release  \
      ..
make
make install
cd ..
hash -r
sed "/c\/.*\.[ch]'/d;\
     /include_dirs=\[/\
     i libraries=['brotlicommon','brotlidec','brotlienc']," \
    -i setup.py
pip3 wheel -w dist --no-build-isolation --no-deps --no-cache-dir $PWD
pip3 install --no-index --find-links=dist --no-cache-dir --no-user Brotli
cd ..
rm -rf pizlonated-brotli
hash -r

tar -xf pizlonated-yaml.tar.gz
cd pizlonated-yaml
./configure --enable-shared --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-yaml
hash -r

tar -xf pizlonated-ruby.tar.gz
cd pizlonated-ruby
./configure --enable-shared --disable-yjit --disable-rjit --without-jit --without-gcc --without-valgrind --with-thread=pthread --without-jemalloc --with-coroutine=pthread --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-ruby
hash -r

# --- Wayland/Weston + text-rendering foundation (the old postlc3 stage) ------

# Some things in this build require a libgcc.a, which looks like an idiotic byproduct of CMake being
# retarded. Give them what they want instead of trying to understand CMake.
cd /usr/lib
rm -f libgcc.a
ar cr libgcc.a

cd /sources

tar -xf pizlonated-wayland.tar.gz
cd pizlonated-wayland
mkdir build
cd build
meson setup ..            \
      --prefix=/usr       \
      --buildtype=debugoptimized \
      -D documentation=false
ninja
ninja install
cd ../..
rm -rf pizlonated-wayland
hash -r

tar -xf wayland-protocols-1.45.tar.xz
cd wayland-protocols-1.45
mkdir build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized
ninja
ninja install
cd ../..
rm -rf wayland-protocols-1.45
hash -r

tar -xf pizlonated-libevdev.tar.gz
cd pizlonated-libevdev
mkdir build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized -D documentation=disabled ..
ninja
ninja install
cd ../..
rm -rf pizlonated-libevdev
hash -r

tar -xf pizlonated-libinput.tar.gz
cd pizlonated-libinput
mkdir build
cd build
meson setup ..            \
      --prefix=/usr       \
      --buildtype=debugoptimized \
      -D debug-gui=false  \
      -D libwacom=false \
      -D mtdev=false \
      -D documentation=false
ninja
ninja install
cd ../..
rm -rf pizlonated-libinput
hash -r

tar -xf xkeyboard-config-2.42.tar.xz
cd xkeyboard-config-2.42
mkdir build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf xkeyboard-config-2.42
hash -r

tar -xf pizlonated-xkbcommon.tar.gz
cd pizlonated-xkbcommon
mkdir -v build
cd build
meson setup ..             \
      --prefix=/usr        \
      --buildtype=debugoptimized \
      -D enable-x11=false  \
      -D enable-docs=false
ninja
ninja install
cd ../..
rm -rf pizlonated-xkbcommon
hash -r

tar -xf pizlonated-libpng.tar.gz
cd pizlonated-libpng
./configure --prefix=/usr --disable-static
make
make install
cd ..
rm -rf pizlonated-libpng
hash -r

if test "$(uname -m)" = x86_64
then
    tar -xf pizlonated-simde.tar.gz
    set -- -DSIMDE_NEON=1 -DSIMDE_INCLUDE_DIR="$PWD/pizlonated-simde" -DREQUIRE_SIMD=1
else
    set -- -DCMAKE_ASM_NASM_COMPILER=""
fi
tar -xf pizlonated-libjpeg-turbo.tar.gz
cd pizlonated-libjpeg-turbo
mkdir -v build
cd build
cmake -G"Unix Makefiles" .. -DCMAKE_INSTALL_PREFIX=/usr "$@" \
      -D CMAKE_BUILD_TYPE=RELEASE         \
      -D ENABLE_STATIC=FALSE              \
      -D CMAKE_INSTALL_DEFAULT_LIBDIR=lib \
      -D CMAKE_SKIP_INSTALL_RPATH=ON      \
      -D CMAKE_INSTALL_DOCDIR=/usr/share/doc/libjpeg-turbo-3.1.4
make
make install
cd ../..
rm -rf pizlonated-libjpeg-turbo
rm -rf pizlonated-simde
hash -r

tar -xf pizlonated-tiff.tar.gz
cd pizlonated-tiff
./configure --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-tiff
hash -r

tar -xf pizlonated-libwebp.tar.gz
cd pizlonated-libwebp
./configure \
    --prefix=/usr \
    --enable-libwebpmux     \
    --enable-libwebpdemux   \
    --enable-libwebpdecoder \
    --enable-libwebpextras  \
    --enable-swap-16bit-csp    
make
make install
cd ..
rm -rf pizlonated-libwebp
hash -r

# Rebuild tiff because of circular dep with libwebp
tar -xf pizlonated-tiff.tar.gz
cd pizlonated-tiff
./configure --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-tiff
hash -r

tar -xf pizlonated-openjpeg.tar.gz
cd pizlonated-openjpeg
mkdir -v build
cd build
cmake -G"Unix Makefiles" .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
make
make install
cd ../..
rm -rf pizlonated-openjpeg
hash -r

tar -xf pixman-0.43.4.tar.gz
cd pixman-0.43.4
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf pixman-0.43.4
hash -r

tar -xf packaging-24.1.tar.gz
cd packaging-24.1
pip3 wheel -w dist --no-build-isolation --no-deps --no-cache-dir $PWD
pip3 install --no-index --find-links=dist --no-cache-dir --no-user packaging
cd ..
rm -rf packaging-24.1
hash -r

./build_postlc2_chroot_project_glib.sh
./build_postlc2_chroot_project_freetype.sh
./build_postlc2_chroot_project_graphite.sh
./build_postlc2_chroot_project_fontconfig.sh
./build_postlc2_chroot_project_cairo.sh
./build_postlc2_chroot_project_harfbuzz.sh
./build_postlc2_chroot_project_freetype.sh
./build_postlc2_chroot_project_graphite.sh
./build_postlc2_chroot_project_libdrm.sh
./build_postlc2_chroot_project_seatd.sh
hash -r

# lcms2 from its projeny package -- built ONCE here (the plain
# lcms2-2.16.tar.gz build of the old layout is gone; nothing re-packages or
# rebuilds lcms2 later in the chain).
./build_postlc2_chroot_project_lcms2.sh
hash -r

./build_postlc2_chroot_project_weston.sh
hash -r

tar -xf daemon-0.6.4.tar.gz
cd daemon-0.6.4
./configure
make -j 1
make -j 1 PREFIX=/usr install
cd ..
rm -rf daemon-0.6.4
hash -r

./build_postlc2_chroot_setup_scripts_for_weston.sh

# --- GTK/GL foundation (the old postlc4 stage) --------------------------------

tar -xf fribidi-1.0.15.tar.xz
cd fribidi-1.0.15
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized
ninja
ninja install
cd ../..
rm -rf fribidi-1.0.15
hash -r

tar -xf shared-mime-info-2.4.tar.gz
cd shared-mime-info-2.4
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized -D update-mimedb=true ..
ninja
ninja install
cd ../..
rm -rf shared-mime-info-2.4
hash -r

tar -xf pycairo-1.26.1.tar.gz
cd pycairo-1.26.1
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf pycairo-1.26.1
hash -r

./build_postlc2_chroot_project_pygobject.sh
./build_postlc2_chroot_project_graphene.sh
hash -r

tar -xf iso-codes_4.16.0.orig.tar.xz
cd iso-codes-4.16.0
./configure --prefix=/usr
make
make install
cd ..
rm -rf iso-codes-4.16.0
hash -r

tar -xf Mako-1.3.5.tar.gz
cd Mako-1.3.5
pip3 wheel -w dist --no-build-isolation --no-deps --no-cache-dir $PWD
pip3 install --no-index --find-links=dist --no-cache-dir --no-user Mako
cd ..
rm -rf Mako-1.3.5
hash -r

# Mesa from the fixed projeny package, then libepoxy and gtk4 build against
# the fixed headers it installs (the GL/EGL stack is built ONCE in this
# stage -- the old layout's cross-stage GL rebuilds are gone).
./build_postlc2_chroot_project_mesa.sh
hash -r

tar -xf libepoxy-1.5.10.tar.xz
cd libepoxy-1.5.10
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized .. -Dx11=false
ninja
ninja install
cd ../..
rm -rf libepoxy-1.5.10
hash -r

./build_postlc2_chroot_project_at-spi2-core.sh
hash -r

./build_postlc2_chroot_project_pango.sh
./build_postlc2_chroot_project_gdk-pixbuf.sh
hash -r

tar -xf pizlonated-nettle.tar.gz
cd pizlonated-nettle
./configure --prefix=/usr --disable-static --disable-assembler
make
make install
cd ..
rm -rf pizlonated-nettle
hash -r

tar -xf pizlonated-libgudev.tar.gz
cd pizlonated-libgudev
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf pizlonated-libgudev
hash -r

tar -xf pizlonated-gstreamer.tar.gz
cd pizlonated-gstreamer
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized -D gst_debug=false
ninja
ninja install
cd ../..
rm -rf pizlonated-gstreamer
hash -r

tar -xf pizlonated-gst-plugins-base.tar.gz
cd pizlonated-gst-plugins-base
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized --wrap-mode=nodownload
ninja
ninja install
cd ../..
rm -rf pizlonated-gst-plugins-base
hash -r

tar -xf pizlonated-gst-plugins-bad.tar.gz
cd pizlonated-gst-plugins-bad
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized -D gpl=enabled
ninja
ninja install
cd ../..
rm -rf pizlonated-gst-plugins-bad
hash -r

./build_postlc2_chroot_project_gtk4.sh
hash -r

./build_postlc2_chroot_project_gtk3.sh
hash -r

# --- Crypto/text libraries (the old postlc5 stage) ----------------------------
# These depend on nettle/p11-kit/glib built earlier in this stage.

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
# GnuTLS is built ONCE, with BOTH trust inputs wired: the pkcs11 store
# (p11-kit-trust, which serves zero objects on this system) and the PEM
# bundle the driver pre-seeded into /etc/ssl before the chroot phase
# (build_preseed_pizlix.sh; add_system_trust() in lib/system/certs.c loads
# both).  There is no later trust-store rebuild stage.
./configure --prefix=/usr \
            --docdir=/usr/share/doc/gnutls-3.8.7.1 \
            --with-default-trust-store-pkcs11="pkcs11:" \
            --with-default-trust-store-file=/etc/ssl/ca-bundle.crt \
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

tar -xf pizlonated-libgpg-error.tar.gz
cd pizlonated-libgpg-error
./configure --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-libgpg-error
hash -r

tar -xf pizlonated-libgcrypt.tar.gz
cd pizlonated-libgcrypt
./configure --prefix=/usr --disable-asm
make
make install
cd ..
rm -rf pizlonated-libgcrypt
hash -r

tar -xf pizlonated-aspell.tar.gz
cd pizlonated-aspell
./configure --prefix=/usr
make
make install
ln -svfn aspell-0.60 /usr/lib/aspell
cd ..
rm -rf pizlonated-aspell
hash -r

tar -xf aspell6-en-2020.12.07-0.tar.bz2
cd aspell6-en-2020.12.07-0
./configure
make
make install
cd ..
rm -rf aspell6-en-2020.12.07-0
hash -r

tar -xf pizlonated-enchant.tar.gz
cd pizlonated-enchant
./configure --prefix=/usr   \
            --disable-static
make
make install
cd ..
rm -rf pizlonated-enchant
hash -r 
