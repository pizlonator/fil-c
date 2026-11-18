#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

rm -vf projects/*/pizlonated-*.tar.gz

# Sprint W1 of the Epiphany-on-Pizlix port: the postlc10 stage packaging.
#
# 1. pizlonated-webkitgtk.tar.gz: WebKitGTK 2.44.3, packaged from the
#    checked-in projects/webkitgtk-2.44.3 tree with package-source.sh (the
#    same pattern as the checked-in gtk4/mesa trees).  The tree is clean,
#    so package-source.sh takes the fast git-archive path; when Fil-C build
#    fixes land as dirty diffs in the tree, the same command handles them
#    (git archive + git diff | patch -p1 + tar).  The stage driver copies
#    the tarball into $LFS/sources and
#    build_postlc10_chroot_project_webkitgtk.sh builds it.
#
# 2. The three W3 Epiphany dependency packages, packaged with `filc/projeny
#    package` (each sets its checkout up first if needed).  They are not
#    built in this stage -- only shipped so the W3 epiphany stage has them
#    in /sources.
./package-source.sh projects/webkitgtk-2.44.3 pizlonated-webkitgtk

filc/projeny package projects/epiphany.projeny projects/epiphany/pizlonated-epiphany.tar.gz
filc/projeny package projects/libportal.projeny projects/libportal/pizlonated-libportal.tar.gz
filc/projeny package projects/desktop-file-utils.projeny projects/desktop-file-utils/pizlonated-desktop-file-utils.tar.gz

ls -la projects/webkitgtk-2.44.3/pizlonated-webkitgtk.tar.gz \
       projects/epiphany/pizlonated-epiphany.tar.gz \
       projects/libportal/pizlonated-libportal.tar.gz \
       projects/desktop-file-utils/pizlonated-desktop-file-utils.tar.gz
