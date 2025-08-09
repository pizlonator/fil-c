#!/bin/sh

BUILD='cc -funsigned-char -I scripts/prereq -I . -Os -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fno-strict-aliasing -DTOYBOX_VERSION=""'
LINK=''
FILES="
main.c toys/lsb/gzip.c toys/other/readlink.c toys/other/which.c 
toys/pending/tr.c toys/posix/basename.c toys/posix/cat.c toys/posix/chmod.c 
toys/posix/cmp.c toys/posix/dirname.c toys/posix/echo.c toys/posix/fold.c 
toys/posix/grep.c toys/posix/head.c toys/posix/ln.c toys/posix/ls.c 
toys/posix/mkdir.c toys/posix/od.c toys/posix/rm.c toys/posix/sed.c 
toys/posix/sort.c toys/posix/tail.c toys/posix/tee.c toys/posix/uname.c 
toys/posix/wc.c toys/posix/xargs.c
"

$BUILD lib/*.c $FILES $LINK -o toybox-prereq
