#!/bin/bash

set -e
set -x

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

if test ! -d $LFS/sources/etc
then
    mkdir -v $LFS/sources/etc
    chmod -v a+wt $LFS/sources/etc
fi
if test ! -d $LFS/sources/etc/sysconfig
then
    mkdir -v $LFS/sources/etc/sysconfig
    chmod -v a+wt $LFS/sources/etc/sysconfig
fi
if test ! -d $LFS/sources/keymaps
then
    mkdir -v $LFS/sources/keymaps
    chmod -v a+wt $LFS/sources/keymaps
fi

cp -v \
    acl-2.3.2.tar.xz \
    attr-2.5.2.tar.gz \
    autoconf-2.72.tar \
    automake-1.17.tar.xz \
    bash-5.2.32.tar.gz \
    bc-6.7.6.tar.xz \
    binutils-2.43.1.tar.xz \
    bison-3.8.2.tar.xz \
    bzip2-1.0.8-install_docs-1.patch \
    bzip2-1.0.8.tar.gz \
    check-0.15.2.tar.gz \
    coreutils-9.5-i18n-2.patch \
    coreutils-9.5.tar.xz \
    curl-8.9.1.tar.xz \
    dejagnu-1.6.3.tar.gz \
    dhcpcd-10.0.8.tar.xz \
    diffutils-3.10.tar.xz \
    e2fsprogs-1.47.1.tar.gz \
    elfutils-0.191.tar.bz2 \
    expat-2.6.2.tar.xz \
    expect-5.45.4-gcc14-1.patch \
    expect5.45.4.tar.gz \
    file-5.45.tar.gz \
    findutils-4.10.0.tar.xz \
    flex-2.6.4.tar.gz \
    flit_core-3.9.0.tar.gz \
    gawk-5.3.0.tar.xz \
    gcc-14.2.0.tar.xz \
    gdbm-1.24.tar.gz \
    gettext-0.22.5.tar.xz \
    glibc-2.40-fhs-1.patch \
    glibc-2.40.tar.xz \
    gmp-6.3.0.tar.xz \
    gperf-3.1.tar.gz \
    grep-3.11.tar.xz \
    groff-1.23.0.tar.gz \
    grub-2.12.tar.xz \
    gzip-1.13.tar.xz \
    iana-etc-20240806.tar.gz \
    inetutils-2.5.tar.xz \
    intltool-0.51.0.tar.gz \
    iproute2-6.10.0.tar.xz \
    jinja2-3.1.4.tar.gz \
    kmod-33.tar.xz \
    less-661.tar.gz \
    libcap-2.70.tar.xz \
    libffi-3.4.6.tar.gz \
    libpipeline-1.5.7.tar.gz \
    libtool-2.4.7.tar.xz \
    libxcrypt-4.4.36.tar.xz \
    lz4-1.10.0.tar.gz \
    m4-1.4.19.tar.xz \
    make-4.4.1.tar.gz \
    man-db-2.12.1.tar.xz \
    man-pages-6.9.1.tar.xz \
    MarkupSafe-2.1.5.tar.gz \
    mg-3.7.tar.gz \
    mpc-1.3.1.tar.gz \
    mpfr-4.2.1.tar.xz \
    ncurses-6.5.tar.gz \
    ninja-1.12.1.tar.gz \
    openssl-3.3.1.tar.gz \
    patch-2.7.6.tar.xz \
    perl-5.40.0.tar.xz \
    pkgconf-2.3.0.tar.xz \
    procps-ng-4.0.4.tar.xz \
    psmisc-23.7.tar.xz \
    python-3.12.5-docs-html.tar.bz2 \
    Python-3.12.5.tar.xz \
    readline-8.2.13.tar.gz \
    sed-4.9.tar.xz \
    setuptools-72.2.0.tar.gz \
    shadow-4.16.0.tar.xz \
    sysklogd-2.6.1.tar.gz \
    systemd-256.4.tar.gz \
    systemd-man-pages-256.4.tar.xz \
    sysvinit-3.10-consolidated-1.patch \
    sysvinit-3.10.tar.xz \
    tar-1.35.tar.xz \
    tcl8.6.14-html.tar.gz \
    tcl8.6.14-src.tar.gz \
    texinfo-7.1.tar.xz \
    tzdata2024a.tar.gz \
    udev-lfs-20230818.tar.xz \
    util-linux-2.40.2.tar.xz \
    vim-9.1.0660.tar.gz \
    wheel-0.44.0.tar.gz \
    XML-Parser-2.47.tar.gz \
    xz-5.6.2.tar.xz \
    zlib-1.3.1.tar.gz \
    zstd-1.5.6.tar.gz \
    build_prelc_sub2_chroot_part1.sh \
    build_prelc_sub2_chroot_part2.sh \
    build_lc_sub2_yolo_chroot.sh \
    build_lc_sub3_user_chroot.sh \
    build_postlc_sub2_chroot_part1.sh \
    build_postlc_sub2_chroot_part2.sh \
    build_postlc_chroot_project_systemd.sh \
    build_postlc_chroot_project_perl.sh \
    build_postlc_chroot_project_meson.sh \
    linux-config-6.10.5 \
    preserve-yolo.txt \
    $LFS/sources

chown root:root $LFS/sources/*

cp -v \
    etc/inittab \
    etc/profile \
    etc/inputrc \
    etc/shells \
    etc/fstab \
    etc/hosts \
    etc/emacs \
    etc/bashrc \
    etc/topdefaultrc \
    etc/sudoers \
    etc/limits \
    etc/sysctl.conf \
    $LFS/sources/etc

cp -v \
    etc/sysconfig/clock \
    etc/sysconfig/console \
    etc/sysconfig/ifconfig.eth0 \
    $LFS/sources/etc/sysconfig

cp -v \
    keymaps/fixed-keys.map \
    $LFS/sources/keymaps

chown root:root $LFS/sources/etc/*
chown root:root $LFS/sources/etc/sysconfig/*
chown root:root $LFS/sources/keymaps/*


