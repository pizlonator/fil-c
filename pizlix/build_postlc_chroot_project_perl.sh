#!/bin/bash

set -e
set -x

rm -rf pizlonated-perl
tar -xf pizlonated-perl.tar.gz
cd pizlonated-perl
mkdir compiler-bin
cat > compiler-bin/cc << "EOF"
#!/bin/bash
set -e
set -x
unset LD_PRELOAD
unset LD_LIBRARY_PATH
/usr/bin/cc "$@"
EOF
chmod 755 compiler-bin/cc
export PATH=$PWD/compiler-bin:$PATH
export BUILD_ZLIB=False
export BUILD_BZIP2=0
sh Configure -des \
    -D optimize="-O3 -g -fno-strict-aliasing -D_GNU_SOURCE" \
    -D prefix=/usr \
    -D vendorprefix=/usr \
    -D privlib=/usr/lib/perl5/5.40/core_perl \
    -D archlib=/usr/lib/perl5/5.40/core_perl \
    -D sitelib=/usr/lib/perl5/5.40/site_perl \
    -D sitearch=/usr/lib/perl5/5.40/site_perl \
    -D vendorlib=/usr/lib/perl5/5.40/vendor_perl \
    -D vendorarch=/usr/lib/perl5/5.40/vendor_perl \
    -D man1dir=/usr/share/man/man1 \
    -D man3dir=/usr/share/man/man3 \
    -D pager="/usr/bin/less -isR" \
    -D useshrplib \
    -D usethreads
make
make install
cd ..
rm -rf pizlonated-perl

