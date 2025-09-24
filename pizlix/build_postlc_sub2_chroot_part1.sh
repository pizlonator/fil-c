#!/yolo/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

tar -xf man-pages-6.9.1.tar.xz
cd man-pages-6.9.1
rm -v man3/crypt*
make prefix=/usr install
cd ..
rm -rf man-pages-6.9.1

tar -xf zlib-1.3.1.tar.gz
cd zlib-1.3.1
./configure --prefix=/usr
make
make install
rm -fv /usr/lib/libz.a
cd ..
rm -rf zlib-1.3.1
hash -r

tar -xf bzip2-1.0.8.tar.gz
cd bzip2-1.0.8
patch -Np1 -i ../bzip2-1.0.8-install_docs-1.patch
sed -i 's@\(ln -s -f \)$(PREFIX)/bin/@\1@' Makefile
sed -i "s@(PREFIX)/man@(PREFIX)/share/man@g" Makefile
make -f Makefile-libbz2_so
make clean
make
make PREFIX=/usr install
cp -av libbz2.so.* /usr/lib
ln -sv libbz2.so.1.0.8 /usr/lib/libbz2.so
cp -v bzip2-shared /usr/bin/bzip2
for i in /usr/bin/{bzcat,bunzip2}; do
    ln -sfv bzip2 $i
done
rm -fv /usr/lib/libbz2.a
cd ..
rm -rf bzip2-1.0.8
hash -r

tar -xf pizlonated-xz.tar.gz
cd pizlonated-xz
./configure --prefix=/usr \
    --disable-static \
    --docdir=/usr/share/doc/xz-5.6.2
make
make install
cd ..
rm -rf pizlonated-xz
hash -r

tar -xf lz4-1.10.0.tar.gz
cd lz4-1.10.0
make BUILD_STATIC=no PREFIX=/usr
make BUILD_STATIC=no PREFIX=/usr install
cd ..
rm -rf lz4-1.10.0
hash -r

tar -xf zstd-1.5.6.tar.gz
cd zstd-1.5.6
ZSTD_NO_ASM=1 make prefix=/usr
ZSTD_NO_ASM=1 make prefix=/usr install
rm -v /usr/lib/libzstd.a
cd ..
rm -rf zstd-1.5.6
hash -r

tar -xf file-5.45.tar.gz
cd file-5.45
./configure --prefix=/usr
make
make install
cd ..
rm -rf file-5.45
hash -r

# Normal LFS build does not build ncurses here. It builds it later (see comment).
tar -xf ncurses-6.5.tar.gz
cd ncurses-6.5
./configure --prefix=/usr \
    --mandir=/usr/share/man \
    --with-shared \
    --without-debug \
    --without-normal \
    --without-ada \
    --with-cxx-shared \
    --enable-pc-files \
    --with-pkg-config-libdir=/usr/lib/pkgconfig
make
make DESTDIR=$PWD/dest install
install -vm755 dest/usr/lib/libncursesw.so.6.5 /usr/lib
rm -v dest/usr/lib/libncursesw.so.6.5
sed -e 's/^#if.*XOPEN.*$/#if 1/' \
    -i dest/usr/include/curses.h
cp -av dest/* /
for lib in ncurses form panel menu ; do
    ln -sfv lib${lib}w.so /usr/lib/lib${lib}.so
    ln -sfv ${lib}w.pc /usr/lib/pkgconfig/${lib}.pc
done
ln -sfv libncursesw.so /usr/lib/libcurses.so
cp -v -R doc -T /usr/share/doc/ncurses-6.5
cd ..
rm -rf ncurses-6.5
hash -r

tar -xf readline-8.2.13.tar.gz
cd readline-8.2.13
sed -i '/MV.*old/d' Makefile.in
sed -i '/{OLDSUFF}/c:' support/shlib-install
sed -i 's/-Wl,-rpath,[^ ]*//' support/shobj-conf
./configure --prefix=/usr \
    --disable-static \
    --with-curses \
    --docdir=/usr/share/doc/readline-8.2.13
make SHLIB_LIBS="-lncursesw"
make SHLIB_LIBS="-lncursesw" install
install -v -m644 doc/*.{ps,pdf,html,dvi} /usr/share/doc/readline-8.2.13
cd ..
rm -rf readline-8.2.13
hash -r

tar -xf pizlonated-m4.tar.gz
cd pizlonated-m4
./configure --prefix=/usr
make
rm /usr/bin/m4 # Delete the symlink we created earlier.
make install
cd ..
rm -rf pizlonated-m4
hash -r

tar -xf bc-6.7.6.tar.xz
cd bc-6.7.6
CC=gcc ./configure --prefix=/usr -G -O3 -r
make
make install
cd ..
rm -rf bc-6.7.6
hash -r

tar -xf flex-2.6.4.tar.gz
cd flex-2.6.4
./configure --prefix=/usr \
            --docdir=/usr/share/doc/flex-2.6.4 \
            --disable-static
make
make install
ln -sv flex /usr/bin/lex
ln -sv flex.1 /usr/share/man/man1/lex.1
cd ..
rm -rf flex-2.6.4
hash -r

tar -xf tcl8.6.14-src.tar.gz
cd tcl8.6.14
SRCDIR=$(pwd)
cd unix
./configure --prefix=/usr \
    --mandir=/usr/share/man \
    --disable-rpath
make
sed -e "s|$SRCDIR/unix|/usr/lib|" \
    -e "s|$SRCDIR|/usr/include|" \
    -i tclConfig.sh
sed -e "s|$SRCDIR/unix/pkgs/tdbc1.1.7|/usr/lib/tdbc1.1.7|" \
    -e "s|$SRCDIR/pkgs/tdbc1.1.7/generic|/usr/include|" \
    -e "s|$SRCDIR/pkgs/tdbc1.1.7/library|/usr/lib/tcl8.6|" \
    -e "s|$SRCDIR/pkgs/tdbc1.1.7|/usr/include|" \
    -i pkgs/tdbc1.1.7/tdbcConfig.sh
sed -e "s|$SRCDIR/unix/pkgs/itcl4.2.4|/usr/lib/itcl4.2.4|" \
    -e "s|$SRCDIR/pkgs/itcl4.2.4/generic|/usr/include|" \
    -e "s|$SRCDIR/pkgs/itcl4.2.4|/usr/include|" \
    -i pkgs/itcl4.2.4/itclConfig.sh
make install
chmod -v u+w /usr/lib/libtcl8.6.so
make install-private-headers
ln -sfv tclsh8.6 /usr/bin/tclsh
mv /usr/share/man/man3/{Thread,Tcl_Thread}.3
cd ../..
rm -rf tcl8.6.14
hash -r

tar -xf expect5.45.4.tar.gz
cd expect5.45.4
python3 -c 'from pty import spawn; spawn(["echo", "ok"])'
patch -Np1 -i ../expect-5.45.4-gcc14-1.patch
./configure --prefix=/usr \
    --with-tcl=/usr/lib \
    --enable-shared \
    --disable-rpath \
    --mandir=/usr/share/man \
    --with-tclinclude=/usr/include
make
make install
ln -svf expect5.45.4/libexpect5.45.4.so /usr/lib
cd ..
rm -rf expect5.45.4
hash -r

tar -xf dejagnu-1.6.3.tar.gz
cd dejagnu-1.6.3
mkdir -v build
cd build
../configure --prefix=/usr
makeinfo --html --no-split -o doc/dejagnu.html ../doc/dejagnu.texi
makeinfo --plaintext -o doc/dejagnu.txt ../doc/dejagnu.texi
make install
install -v -dm755 /usr/share/doc/dejagnu-1.6.3
install -v -m644 doc/dejagnu.{html,txt} /usr/share/doc/dejagnu-1.6.3
cd ../..
rm -rf dejagnu-1.6.3
hash -r

tar -xf pizlonated-pkgconf.tar.gz
cd pizlonated-pkgconf
./configure --prefix=/usr \
    --disable-static \
    --docdir=/usr/share/doc/pkgconf-2.3.0
make
make install
ln -sv pkgconf /usr/bin/pkg-config
ln -sv pkgconf.1 /usr/share/man/man1/pkg-config.1
cd ..
rm -rf pizlonated-pkgconf
hash -r

tar -xf pizlonated-binutils.tar.gz
cd pizlonated-binutils
mkdir -v build
cd build
../configure --prefix=/usr \
    --sysconfdir=/etc \
    --enable-gold \
    --enable-ld=default \
    --enable-plugins \
    --enable-shared \
    --disable-werror \
    --enable-64-bit-bfd \
    --enable-new-dtags \
    --with-system-zlib \
    --enable-default-hash-style=gnu \
    --disable-gprofng
make tooldir=/usr
make tooldir=/usr install
rm -fv /usr/lib/lib{bfd,ctf,ctf-nobfd,gprofng,opcodes,sframe}.a
cd ../..
rm -rf pizlonated-binutils
hash -r

tar -xf pizlonated-gmp.tar.gz
cd pizlonated-gmp
./configure --prefix=/usr \
    --enable-cxx \
    --disable-static \
    --docdir=/usr/share/doc/gmp-6.3.0 \
    --disable-assembly
make
make html
make install
make install-html
cd ..
rm -rf pizlonated-gmp
hash -r

tar -xf mpfr-4.2.1.tar.xz
cd mpfr-4.2.1
./configure --prefix=/usr \
    --disable-static \
    --enable-thread-safe \
    --docdir=/usr/share/doc/mpfr-4.2.1
make
make html
make install
make install-html
cd ..
rm -rf mpfr-4.2.1
hash -r

tar -xf mpc-1.3.1.tar.gz
cd mpc-1.3.1
./configure --prefix=/usr \
    --disable-static \
    --docdir=/usr/share/doc/mpc-1.3.1
make
make html
make install
make install-html
cd ..
rm -rf mpc-1.3.1
hash -r

tar -xf pizlonated-attr.tar.gz
cd pizlonated-attr
./configure --prefix=/usr \
    --disable-static \
    --sysconfdir=/etc \
    --docdir=/usr/share/doc/attr-2.5.2
make
make install
cd ..
rm -rf pizlonated-attr
hash -r

tar -xf acl-2.3.2.tar.xz
cd acl-2.3.2
sed -i s/-Wl,--version-script,/-Wc,--version-script=/g Makefile.in
./configure --prefix=/usr \
    --disable-static \
    --docdir=/usr/share/doc/acl-2.3.2
make
make install
cd ..
rm -rf acl-2.3.2
hash -r

tar -xf libcap-2.70.tar.xz
cd libcap-2.70
sed -i '/install -m.*STA/d' libcap/Makefile
make prefix=/usr lib=lib
make prefix=/usr lib=lib install
cd ..
rm -rf libcap-2.70
hash -r

tar -xf pizlonated-libxcrypt.tar.gz
cd pizlonated-libxcrypt
./configure --prefix=/usr \
    --enable-hashes=strong,glibc \
    --enable-obsolete-api=no \
    --disable-static \
    --disable-failure-tokens
make
make install
cd ..
rm -rf pizlonated-libxcrypt
hash -r

tar -xf pizlonated-shadow.tar.gz
cd pizlonated-shadow
sed -i 's/groups$(EXEEXT) //' src/Makefile.in
find man -name Makefile.in -exec sed -i 's/groups\.1 / /' {} \;
find man -name Makefile.in -exec sed -i 's/getspnam\.3 / /' {} \;
find man -name Makefile.in -exec sed -i 's/passwd\.5 / /' {} \;
sed -e 's:#ENCRYPT_METHOD DES:ENCRYPT_METHOD YESCRYPT:' \
    -e 's:/var/spool/mail:/var/mail:' \
    -e '/PATH=/{s@/sbin:@@;s@/bin:@@}' \
    -i etc/login.defs
touch /usr/bin/passwd
./configure --sysconfdir=/etc \
    --disable-static \
    --with-{b,yes}crypt \
    --without-libbsd \
    --with-group-name-max-length=32
make
make exec_prefix=/usr install
make -C man install-man
pwconv
grpconv
mkdir -p /etc/default
useradd -D --gid 999
echo root:root | chpasswd
cd ..
rm -rf pizlonated-shadow
hash -r

# NOTE: This is where we originally built ncurses

tar -xf pizlonated-sed.tar.gz
cd pizlonated-sed
./configure --prefix=/usr
make
make html
make install
install -d -m755 /usr/share/doc/sed-4.9
install -m644 doc/sed.html /usr/share/doc/sed-4.9
cd ..
rm -rf pizlonated-sed
hash -r

tar -xf psmisc-23.7.tar.xz
cd psmisc-23.7
./configure --prefix=/usr
make
make install
cd ..
rm -rf psmisc-23.7
hash -r

tar -xf pizlonated-gettext.tar.gz
cd pizlonated-gettext
./configure --prefix=/usr \
    --disable-static \
    --docdir=/usr/share/doc/gettext-0.22.5
make
make install
chmod -v 0755 /usr/lib/preloadable_libintl.so
cd ..
rm -rf pizlonated-gettext
hash -r

tar -xf pizlonated-bison.tar.gz
cd pizlonated-bison
./configure --prefix=/usr --docdir=/usr/share/doc/bison-3.8.2
make
make install
cd ..
rm -rf pizlonated-bison
hash -r

tar -xf pizlonated-grep.tar.gz
cd pizlonated-grep
sed -i "s/echo/#echo/" src/egrep.sh
./configure --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-grep
hash -r

tar -xf pizlonated-bash.tar.gz
cd pizlonated-bash
./configure --prefix=/usr \
    --without-bash-malloc \
    --with-installed-readline \
    bash_cv_strtold_broken=no \
    --docdir=/usr/share/doc/bash-5.2.32
make
rm /bin/bash
make install

exec /bin/bash /sources/build_postlc_sub2_chroot_part2.sh
