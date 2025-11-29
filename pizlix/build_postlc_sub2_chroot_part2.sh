#!/bin/bash

set -e
set -x

ulimit -c unlimited

rm /bin/sh
ln -sv bash /bin/sh
cd ..
rm -rf pizlonated-bash

tar -xf libtool-2.4.7.tar.xz
cd libtool-2.4.7
./configure --prefix=/usr
make
make install
rm -fv /usr/lib/libltdl.a
cd ..
rm -rf libtool-2.4.7
hash -r

tar -xf gdbm-1.24.tar.gz
cd gdbm-1.24
./configure --prefix=/usr \
    --disable-static \
    --enable-libgdbm-compat
make
make install
cd ..
rm -rf gdbm-1.24
hash -r

tar -xf gperf-3.1.tar.gz
cd gperf-3.1
CXX="g++ -Wno-register" ./configure --prefix=/usr --docdir=/usr/share/doc/gperf-3.1
make
make install
cd ..
rm -rf gperf-3.1
hash -r

tar -xf expat-2.6.2.tar.xz
cd expat-2.6.2
./configure --prefix=/usr \
    --disable-static \
    --docdir=/usr/share/doc/expat-2.6.2
make
make install
install -v -m644 doc/*.{html,css} /usr/share/doc/expat-2.6.2
cd ..
rm -rf expat-2.6.2
hash -r

tar -xf inetutils-2.5.tar.xz
cd inetutils-2.5
sed -i 's/def HAVE_TERMCAP_TGETENT/ 1/' telnet/telnet.c
./configure --prefix=/usr \
    --bindir=/usr/bin \
    --localstatedir=/var \
    --disable-logger \
    --disable-whois \
    --disable-rcp \
    --disable-rexec \
    --disable-rlogin \
    --disable-rsh \
    --disable-servers
make
make install
mv -v /usr/{,s}bin/ifconfig
cd ..
rm -rf inetutils-2.5
hash -r

tar -xf less-661.tar.gz
cd less-661
./configure --prefix=/usr --sysconfdir=/etc
make
make install
cd ..
rm -rf less-661
hash -r

./build_postlc_chroot_project_perl.sh
hash -r

tar -xf pizlonated-xml-parser.tar.gz
cd pizlonated-xml-parser
perl Makefile.PL
make
make install
cd ..
rm -rf pizlonated-xml-parser
hash -r

tar -xf intltool-0.51.0.tar.gz
cd intltool-0.51.0
sed -i 's:\\\${:\\\$\\{:' intltool-update.in
./configure --prefix=/usr
make
make install
install -v -Dm644 doc/I18N-HOWTO /usr/share/doc/intltool-0.51.0/I18N-HOWTO
cd ..
rm -rf intltool-0.51.0
hash -r

tar -xf autoconf-2.72.tar
cd autoconf-2.72
./configure --prefix=/usr
make
make install
cd ..
rm -rf autoconf-2.72
hash -r

tar -xf automake-1.17.tar.xz
cd automake-1.17
./configure --prefix=/usr --docdir=/usr/share/doc/automake-1.17
make
make install
cd ..
rm -rf automake-1.17
hash -r

tar -xf pizlonated-openssl.tar.gz
cd pizlonated-openssl
./config --prefix=/usr \
    --openssldir=/etc/ssl \
    --libdir=lib \
    shared \
    zlib-dynamic
make
sed -i '/INSTALL_LIBS/s/libcrypto.a libssl.a//' Makefile
make MANSUFFIX=ssl install
mv -v /usr/share/doc/openssl /usr/share/doc/openssl-3.3.1
cp -vfr doc/* /usr/share/doc/openssl-3.3.1
cd ..
rm -rf pizlonated-openssl
hash -r

tar -xf pizlonated-kmod.tar.gz
cd pizlonated-kmod
./configure --prefix=/usr \
    --sysconfdir=/etc \
    --with-openssl \
    --with-xz \
    --with-zstd \
    --with-zlib \
    --disable-manpages
make
make install
for target in depmod insmod modinfo modprobe rmmod; do
    ln -sfv ../bin/kmod /usr/sbin/$target
    rm -fv /usr/bin/$target
done
cd ..
rm -rf pizlonated-kmod
hash -r

tar -xf pizlonated-elfutils.tar.gz
cd pizlonated-elfutils
mkdir build
cd build
../configure --prefix=/usr \
    --disable-debuginfod \
    --enable-libdebuginfod=dummy \
    --disable-symbol-versioning
make
make -C libelf install
install -vm644 config/libelf.pc /usr/lib/pkgconfig
rm /usr/lib/libelf.a
cd ../..
rm -rf pizlonated-elfutils
hash -r

tar -xf pizlonated-libffi.tar.gz
cd pizlonated-libffi
./configure --prefix=/usr \
    --disable-static \
    --with-gcc-arch=native \
    --disable-exec-static-tramp
make
make install
cd ..
rm -rf pizlonated-libffi
hash -r

tar -xf pizlonated-cpython.tar.gz
cd pizlonated-cpython
./configure --prefix=/usr \
    --enable-shared \
    --with-system-expat \
    --without-mimalloc --without-pymalloc --without-freelists \
    --disable-test-modulesmake
make install
cat > /etc/pip.conf << EOF
[global]
root-user-action = ignore
disable-pip-version-check = true
EOF
cd ..
rm -rf pizlonated-cpython
hash -r

tar -xf flit_core-3.9.0.tar.gz
cd flit_core-3.9.0
pip3 wheel -w dist --no-cache-dir --no-build-isolation --no-deps $PWD
pip3 install --no-index --no-user --find-links dist flit_core
cd ..
rm -rf flit_core-3.9.0
hash -r

tar -xf wheel-0.44.0.tar.gz
cd wheel-0.44.0
pip3 wheel -w dist --no-cache-dir --no-build-isolation --no-deps $PWD
pip3 install --no-index --find-links=dist wheel
cd ..
rm -rf wheel-0.44.0
hash -r

tar -xf setuptools-72.2.0.tar.gz
cd setuptools-72.2.0
pip3 wheel -w dist --no-cache-dir --no-build-isolation --no-deps $PWD
pip3 install --no-index --find-links dist setuptools
cd ..
rm -rf setuptools-72.2.0
hash -r

tar -xf ninja-1.12.1.tar.gz
cd ninja-1.12.1
python3 configure.py --bootstrap
install -vm755 ninja /usr/bin/
install -vDm644 misc/bash-completion /usr/share/bash-completion/completions/ninja
install -vDm644 misc/zsh-completion /usr/share/zsh/site-functions/_ninja
cd ..
rm -rf ninja-1.12.1
hash -r

./build_postlc_chroot_project_meson.sh
hash -r

tar -xf coreutils-9.5.tar.xz
cd coreutils-9.5
patch -Np1 -i ../coreutils-9.5-i18n-2.patch
autoreconf -fiv
FORCE_UNSAFE_CONFIGURE=1 ./configure \
    --prefix=/usr \
    --enable-no-install-program=kill,uptime
make
rm /usr/bin/env
make install
mv -v /usr/bin/chroot /usr/sbin
mv -v /usr/share/man/man1/chroot.1 /usr/share/man/man8/chroot.8
sed -i 's/"1"/"8"/' /usr/share/man/man8/chroot.8
cd ..
rm -rf coreutils-9.5
hash -r

tar -xf pizlonated-check.tar.gz
cd pizlonated-check
./configure --prefix=/usr --disable-static
make
make docdir=/usr/share/doc/check-0.15.2 install
cd ..
rm -rf pizlonated-check
hash -r

tar -xf pizlonated-diffutils.tar.gz
cd pizlonated-diffutils
./configure --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-diffutils
hash -r

tar -xf gawk-5.3.0.tar.xz
cd gawk-5.3.0
sed -i 's/extras//' Makefile.in
./configure --prefix=/usr
make
rm -f /usr/bin/gawk-5.3.0
make install
ln -sv gawk.1 /usr/share/man/man1/awk.1
cd ..
rm -rf gawk-5.3.0
hash -r

tar -xf findutils-4.10.0.tar.xz
cd findutils-4.10.0
./configure --prefix=/usr --localstatedir=/var/lib/locate
make
make install
cd ..
rm -rf findutils-4.10.0
hash -r

tar -xf groff-1.23.0.tar.gz
cd groff-1.23.0
PAGE=letter ./configure --prefix=/usr
make
make install
cd ..
rm -rf groff-1.23.0
hash -r

# skipping grub

tar -xf gzip-1.13.tar.xz
cd gzip-1.13
./configure --prefix=/usr
make
make install
cd ..
rm -rf gzip-1.13
hash -r

tar -xf iproute2-6.10.0.tar.xz
cd iproute2-6.10.0
sed -i /ARPD/d Makefile
rm -fv man/man8/arpd.8
make NETNS_RUN_DIR=/run/netns
make SBINDIR=/usr/sbin install
cd ..
rm -rf iproute2-6.10.0
hash -r

tar -xf pizlonated-kbd.tar.gz
cd pizlonated-kbd
./configure --prefix=/usr --disable-vlock
make
make install
cd ..
rm -rf pizlonated-kbd
hash -r

tar -xf pizlonated-libpipeline.tar.gz
cd pizlonated-libpipeline
./configure --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-libpipeline
hash -r

tar -xf pizlonated-make.tar.gz
cd pizlonated-make
./configure --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-make
hash -r

tar -xf patch-2.7.6.tar.xz
cd patch-2.7.6
./configure --prefix=/usr
make
make install
cd ..
rm -rf patch-2.7.6
hash -r

tar -xf pizlonated-tar.tar.gz
cd pizlonated-tar
FORCE_UNSAFE_CONFIGURE=1 \
    ./configure --prefix=/usr
make
make install
make -C doc install-html docdir=/usr/share/doc/tar-1.35
cd ..
rm -rf pizlonated-tar
hash -r

tar -xf pizlonated-texinfo.tar.gz
cd pizlonated-texinfo
./configure --prefix=/usr
make
make install
make TEXMF=/usr/share/texmf install-tex
cd ..
rm -rf pizlonated-texinfo
hash -r

tar -xf pizlonated-vim.tar.gz
cd pizlonated-vim
echo '#define SYS_VIMRC_FILE "/etc/vimrc"' >> src/feature.h
./configure --prefix=/usr
make
make install
ln -sv vim /usr/bin/vi
for L in /usr/share/man/{,*/}man1/vim.1; do
    ln -sv vim.1 $(dirname $L)/vi.1
done
ln -sv ../vim/vim91/doc /usr/share/doc/vim-9.1.0660
cat > /etc/vimrc << "EOF"
" Begin /etc/vimrc
" Ensure defaults are set before customizing settings, not after
source $VIMRUNTIME/defaults.vim
let skip_defaults_vim=1
set nocompatible
set backspace=2
set mouse=
syntax on
if (&term == "xterm") || (&term == "putty")
set background=dark
endif
" End /etc/vimrc
EOF
cd ..
rm -rf pizlonated-vim
hash -r

tar -xf mg-3.7.tar.gz 
cd mg-3.7
./configure --prefix=/usr
make
make install
cd ..
rm -rf mg-3.7
hash -r

tar -xf MarkupSafe-2.1.5.tar.gz
cd MarkupSafe-2.1.5
pip3 wheel -w dist --no-cache-dir --no-build-isolation --no-deps $PWD
pip3 install --no-index --no-user --find-links dist Markupsafe
cd ..
rm -rf MarkupSafe-2.1.5
hash -r

tar -xf jinja2-3.1.4.tar.gz
cd jinja2-3.1.4
pip3 wheel -w dist --no-cache-dir --no-build-isolation --no-deps $PWD
pip3 install --no-index --no-user --find-links dist Jinja2
cd ..
rm -rf jinja2-3.1.4
hash -r

# Build util-linux so that udev sees libmount.
tar -xf pizlonated-util-linux.tar.gz
cd pizlonated-util-linux
./configure --bindir=/usr/bin \
    --libdir=/usr/lib \
    --runstatedir=/run \
    --sbindir=/usr/sbin \
    --disable-chfn-chsh \
    --disable-login \
    --disable-nologin \
    --disable-su \
    --disable-setpriv \
    --disable-runuser \
    --disable-pylibmount \
    --disable-liblastlog2 \
    --disable-static \
    --without-python \
    --without-systemd \
    --without-systemdsystemunitdir \
    ADJTIME_PATH=/var/lib/hwclock/adjtime \
    --docdir=/usr/share/doc/util-linux-2.40.2
make
make install
cd ..
rm -rf pizlonated-util-linux
hash -r

./build_postlc_chroot_project_systemd.sh
hash -r

tar -xf pizlonated-man-db.tar.gz
cd pizlonated-man-db
./configure --prefix=/usr \
    --docdir=/usr/share/doc/man-db-2.12.1 \
    --sysconfdir=/etc \
    --disable-setuid \
    --enable-cache-owner=bin \
    --with-browser=/usr/bin/lynx \
    --with-vgrind=/usr/bin/vgrind \
    --with-grap=/usr/bin/grap \
    --with-systemdtmpfilesdir= \
    --with-systemdsystemunitdir=
make
make install
cd ..
rm -rf pizlonated-man-db
hash -r

tar -xf pizlonated-procps.tar.gz
cd pizlonated-procps
./configure --prefix=/usr \
    --docdir=/usr/share/doc/procps-ng-4.0.4 \
    --disable-static \
    --disable-kill
make
make install
cd ..
rm -rf pizlonated-procps
hash -r

# Rebuild util-linux so that it can use libudev.
tar -xf pizlonated-util-linux.tar.gz
cd pizlonated-util-linux
./configure --bindir=/usr/bin \
    --libdir=/usr/lib \
    --runstatedir=/run \
    --sbindir=/usr/sbin \
    --disable-chfn-chsh \
    --disable-login \
    --disable-nologin \
    --disable-su \
    --disable-setpriv \
    --disable-runuser \
    --disable-pylibmount \
    --disable-liblastlog2 \
    --disable-static \
    --without-python \
    --without-systemd \
    --without-systemdsystemunitdir \
    ADJTIME_PATH=/var/lib/hwclock/adjtime \
    --docdir=/usr/share/doc/util-linux-2.40.2
make
make install
cd ..
rm -rf pizlonated-util-linux
hash -r

tar -xf pizlonated-e2fsprogs.tar.gz
cd pizlonated-e2fsprogs
mkdir -v build
cd build
../configure --prefix=/usr \
    --sysconfdir=/etc \
    --enable-elf-shlibs \
    --disable-libblkid \
    --disable-libuuid \
    --disable-uuidd \
    --disable-fsck
make
make install
rm -fv /usr/lib/{libcom_err,libe2p,libext2fs,libss}.a
gunzip -v /usr/share/info/libext2fs.info.gz
install-info --dir-file=/usr/share/info/dir /usr/share/info/libext2fs.info
cd ../..
rm -rf pizlonated-e2fsprogs
hash -r

tar -xf sysklogd-2.6.1.tar.gz
cd sysklogd-2.6.1
./configure --prefix=/usr \
    --sysconfdir=/etc \
    --runstatedir=/run \
    --without-logger
make
make install
cat > /etc/syslog.conf << "EOF"
# Begin /etc/syslog.conf
auth,authpriv.* -/var/log/auth.log
*.*;auth,authpriv.none -/var/log/sys.log
daemon.* -/var/log/daemon.log
kern.* -/var/log/kern.log
mail.* -/var/log/mail.log
user.* -/var/log/user.log
*.emerg *
# Do not open any internet ports.
secure_mode 2
# End /etc/syslog.conf
EOF
cd ..
rm -rf sysklogd-2.6.1
hash -r

tar -xf sysvinit-3.10.tar.xz
cd sysvinit-3.10
patch -Np1 -i ../sysvinit-3.10-consolidated-1.patch
make
make install
cd ..
rm -rf sysvinit-3.10
hash -r

find /usr/lib /usr/libexec -name \*.la -delete
find /usr -depth -name $(uname -m)-lfs-linux-gnu\* | xargs rm -rf
cd /
tar -cpf preserve-yolo.tar --files-from sources/preserve-yolo.txt
rm -rf yolo
tar -xpf preserve-yolo.tar
rm -v preserve-yolo.tar
cd sources
hash -r

tar -xf pizlonated-lfs-bootscripts.tar.gz
cd pizlonated-lfs-bootscripts
make install
cd ..
rm -rf pizlonated-lfs-bootscripts

bash /usr/lib/udev/init-net-rules.sh

cp -v etc/sysconfig/ifconfig.eth0 /etc/sysconfig/
echo pizlix > /etc/hostname
cp -v etc/hosts /etc/
cp -v etc/inittab /etc/
cp -v etc/sysconfig/clock /etc/sysconfig/
cp -v etc/sysconfig/console /etc/sysconfig/
cp -v etc/profile /etc/
mkdir -v /etc/profile.d
cp -v etc/inputrc /etc/
cp -v etc/shells /etc/
cp -v etc/fstab /etc/
cp -v etc/topdefaultrc /etc/
cp -v etc/limits /etc/
cp -v etc/sysctl.conf /etc/

mkdir -v /usr/share/keymaps/custom
cp -v keymaps/fixed-keys.map /usr/share/keymaps/custom/

useradd -m pizlo
echo pizlo:pizlo | chpasswd
cp -v etc/emacs /home/pizlo/.emacs
cp -v etc/emacs /root/.emacs
cp -v etc/bashrc /home/pizlo/.bashrc
cp -v etc/bashrc /root/.bashrc
chown -v pizlo:pizlo /home/pizlo/.emacs
chown -v pizlo:pizlo /home/pizlo/.bashrc

# The Linux kernel's userspace components - which are part of the build - are pizlonated.
tar -xf pizlonated-linux.tar.gz
cd pizlonated-linux
make CC=/yolo/bin/gcc HOSTCC="/usr/bin/gcc -Wno-unknown-warning-option" mrproper
cp -v ../linux-config-6.10.5 .config
make CC=/yolo/bin/gcc HOSTCC="/usr/bin/gcc -Wno-unknown-warning-option"
make CC=/yolo/bin/gcc HOSTCC="/usr/bin/gcc -Wno-unknown-warning-option" modules_install
cp -v arch/x86/boot/bzImage /boot/vmlinuz-6.10.5-lfs-12.2
cp -v System.map /boot/System.map-6.10.5
cp -v .config /boot/config-6.10.5
cp -r Documentation -T /usr/share/doc/linux-6.10.5
install -v -m755 -d /etc/modprobe.d
cat > /etc/modprobe.d/usb.conf << "EOF"
# Begin /etc/modprobe.d/usb.conf
install ohci_hcd /sbin/modprobe ehci_hcd ; /sbin/modprobe -i ohci_hcd ; true
install uhci_hcd /sbin/modprobe ehci_hcd ; /sbin/modprobe -i uhci_hcd ; true
# End /etc/modprobe.d/usb.conf
EOF
cd ..
rm -rf pizlonated-linux
