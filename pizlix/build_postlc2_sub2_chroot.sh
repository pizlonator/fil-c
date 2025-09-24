#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

./build_postlc2_chroot_project_dhcpcd.sh
hash -r

tar -xf patchelf-0.18.0.tar.gz
cd patchelf-0.18.0
./configure --prefix=/usr \
            --docdir=/usr/share/doc/patchelf-0.18.0
make
make install
cd ..
rm -rf patchelf-0.18.0
hash -r

tar -xf libunistring-1.2.tar.xz
cd libunistring-1.2
./configure --prefix=/usr    \
            --disable-static \
            --docdir=/usr/share/doc/libunistring-1.2
make
make install
cd ..
rm -rf libunistring-1.2
hash -r

./build_postlc2_chroot_project_libidn2.sh
hash -r

tar -xf libpsl-0.21.5.tar.gz
cd libpsl-0.21.5
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=release
ninja
ninja install
cd ../..
rm -rf libpsl-0.21.5
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

tar -xf nghttp2-1.62.1.tar.xz
cd nghttp2-1.62.1
./configure --prefix=/usr     \
            --disable-static  \
            --enable-lib-only \
            --docdir=/usr/share/doc/nghttp2-1.62.1
make
make install
cd ..
rm -rf nghttp2-1.62.1
hash -r

tar -xf pizlonated-curl.tar.gz
cd pizlonated-curl
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
cp -v -R docs -T /usr/share/doc/curl-8.9.1
cd ..
rm -rf pizlonated-curl
hash -r

tar -xf openssh-9.8p1.tar.gz
cd openssh-9.8p1
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
install -v -m755 -d /usr/share/doc/openssh-9.8p1
install -v -m644    INSTALL LICENCE OVERVIEW README* \
                    /usr/share/doc/openssh-9.8p1
cd ..
rm -rf openssh-9.8p1
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
            --docdir=/usr/share/doc/sudo-1.9.15p5 \
            --with-passprompt="[sudo] password for %p: "
make
make install
cd ..
rm -rf pizlonated-sudo
hash -r
cp -rv etc/sudoers /etc

tar -xf pcre2-10.44.tar.bz2
cd pcre2-10.44
./configure --prefix=/usr                       \
            --docdir=/usr/share/doc/pcre2-10.44 \
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
rm -rf pcre2-10.44
hash -r

tar -xf wget-1.24.5.tar.gz
cd wget-1.24.5
./configure --prefix=/usr      \
            --sysconfdir=/etc  \
            --with-ssl=openssl
make
make install
cd ..
rm -rf wget-1.24.5
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
cd pizlonated-icu/icu4c/source
./configure --prefix=/usr
make
make install
cd ../../..
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
            --docdir=/usr/share/doc/libxml2-2.13.3
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

tar -xf brotli-1.1.0.tar.gz
cd brotli-1.1.0
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
rm -rf brotli-1.1.0
hash -r

