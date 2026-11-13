#!/bin/sh
#
# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

. libpas/common.sh

set -e
set -x

cd projects
rm -rf php/extracted-source
../filc/projeny extract php.projeny php/extracted-source
cd php/extracted-source
# Canonicalize the prefix: PHP bakes it into php-config, phpize, and the
# default extension_dir and php.ini search path, and this build tree is
# deleted after install.
PZ=$(cd $PWD/../../.. && echo $PWD/pizfix)
# --disable-fiber-asm: fibers switch stacks with ucontext instead of Boost's
#   fcontext assembly.
# --without-pcre-jit, --disable-opcache-jit: both JITs emit machine code,
#   which Fil-C cannot run.
# --disable-huge-code-pages: OPcache, which PHP 8.5 always builds in, finds
#   PHP's machine code with dl_iterate_phdr() to copy it into huge pages;
#   Fil-C's libc lacks dl_iterate_phdr(), so PHP would not link.
# ac_cv_have_decl_UFFDIO_WRITEPROTECT_MODE_WP=no: phpdbg watchpoints would
#   use userfaultfd, which Fil-C does not support; phpdbg dies at startup.
# ac_cv_have_decl_SYS_waitid=no: pcntl_waitid() fills its $rusage argument
#   through syscall(SYS_waitid, ...), and Fil-C's syscall() does not support
#   waitid, so PHP panicked; without it pcntl_waitid() leaves $rusage alone.
# php_cv_have_fpu_inline_asm_x86=no: the x87 fnstcw/fldcw probe is a link-only
#   test, and Fil-C compiles inline asm into a runtime panic instead of
#   rejecting it, so the probe would falsely succeed and make zend_float.h
#   emit x87 asm on non-x86 hosts (e.g. aarch64), panicking every request
#   startup. Fil-C cannot execute inline asm on any target.
# --define-prefix: most pizfix .pc files record a prefix through an
#   extracted-source directory that their build script has since deleted.
# build_sqlite.sh installs a static libsqlite3 without a pkg-config file.
PATH=$PZ/bin:$PATH PKG_CONFIG="$PZ/bin/pkg-config --define-prefix" PKG_CONFIG_PATH=$PZ/lib/pkgconfig \
    SQLITE_CFLAGS="-I$PZ/include" SQLITE_LIBS="-L$PZ/lib -lsqlite3 -lm" \
    CC="$PWD/../../../build/bin/clang -g -O2" CXX="$PWD/../../../build/bin/clang++ -g -O2" \
    ./configure --prefix=$PZ \
    --disable-fiber-asm --without-pcre-jit --disable-opcache-jit \
    --disable-huge-code-pages \
    --with-external-pcre \
    --enable-fpm \
    --with-openssl --with-zlib --with-bz2=$PZ --with-curl \
    --with-sqlite3 --with-pdo-sqlite \
    --enable-intl --with-libedit \
    --enable-mbstring --disable-mbregex \
    --enable-gd --with-jpeg --with-webp \
    --enable-bcmath --enable-calendar --enable-exif \
    --enable-pcntl --enable-sockets --enable-shmop \
    ac_cv_have_decl_UFFDIO_WRITEPROTECT_MODE_WP=no \
    ac_cv_have_decl_SYS_waitid=no \
    php_cv_have_fpu_inline_asm_x86=no
make -j $NCPU
make -j $NCPU install

# Do a cheesy test of what the Fil-C patch touches: WeakMap keys, fibers and
# their stack limit, Dom\HTMLDocument (lexbor name IDs), overflow arithmetic,
# opcache, and the system allocator (Zend's counts real usage in 2 MiB chunks).
$PZ/bin/php -n -d zend.assertions=1 -d opcache.enable_cli=1 -r '
    $m = new WeakMap; $o = new stdClass; $m[$o] = 1;
    foreach ($m as $k => $v) assert($k === $o && $v === 1);
    unset($o, $k); assert(count($m) === 0);
    function f($n) { return array_map("f", [$n + 1]); }
    $fiber = new Fiber(function () {
        try { f(0); } catch (Error $e) { Fiber::suspend($e->getMessage()); }
    });
    assert(str_starts_with($fiber->start(), "Maximum call stack size"));
    $d = Dom\HTMLDocument::createFromString("<custom-el data-x=y>hi</custom-el>", LIBXML_NOERROR);
    assert($d->querySelector("custom-el")->getAttribute("data-x") === "y");
    $i = PHP_INT_MAX; $i++; assert(is_float($i));
    assert(opcache_get_status(false)["opcache_enabled"]);
    assert(memory_get_usage(true) % (2 << 20) !== 0);
    echo "php ok\n";'
cd ..
rm -rf extracted-source
