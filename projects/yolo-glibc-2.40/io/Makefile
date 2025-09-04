# Copyright (C) 1992-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

#
#	Sub-makefile for I/O portion of the library.
#
subdir := io

include ../Makeconfig

headers := \
  bits/fcntl.h \
  bits/fcntl2.h \
  bits/poll.h \
  bits/poll2.h \
  bits/stat.h \
  bits/statfs.h \
  bits/statvfs.h \
  bits/statx-generic.h \
  bits/statx.h \
  bits/types/struct_statx.h \
  bits/types/struct_statx_timestamp.h \
  fcntl.h \
  fts.h \
  ftw.h \
  poll.h \
  sys/fcntl.h \
  sys/poll.h \
  sys/sendfile.h \
  sys/stat.h \
  sys/statfs.h \
  sys/statvfs.h \
  sys/vfs.h \
  utime.h \
  # headers

routines := \
  access \
  chdir \
  chmod \
  chown \
  close \
  close_range \
  closefrom \
  copy_file_range \
  creat \
  creat64 \
  dup \
  dup2 \
  dup3 \
  euidaccess \
  faccessat \
  fchdir \
  fchmod \
  fchmodat \
  fchown \
  fchownat \
  fcntl \
  fcntl64 \
  file_change_detection \
  flock \
  fstat \
  fstat64 \
  fstatat \
  fstatat64 \
  fstatfs \
  fstatfs64 \
  fstatvfs \
  fstatvfs64 \
  fts \
  fts64 \
  fts64-time64 \
  ftw \
  ftw64 \
  ftw64-time64 \
  futimens \
  getcwd \
  getdirname \
  getwd \
  isatty \
  lchmod \
  lchown \
  link \
  linkat \
  lockf \
  lockf64 \
  lseek \
  lseek64 \
  lstat \
  lstat64 \
  mkdir \
  mkdirat \
  mkfifo \
  mkfifoat \
  mknod \
  mknodat \
  open \
  open64 \
  open64_2 \
  open_2 \
  openat \
  openat64 \
  openat64_2 \
  openat_2 \
  pipe \
  pipe2 \
  poll \
  posix_fadvise \
  posix_fadvise64 \
  posix_fallocate \
  posix_fallocate64 \
  ppoll \
  read \
  readlink \
  readlinkat \
  rmdir \
  sendfile \
  sendfile64 \
  stat \
  stat64 \
  statfs \
  statfs64 \
  statvfs \
  statvfs64 \
  statx \
  symlink \
  symlinkat \
  ttyname \
  ttyname_r \
  umask \
  unlink \
  unlinkat \
  utime \
  utimensat \
  write \
  # routines

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  getcwd \
  getwd \
  open \
  open64 \
  openat \
  openat64 \
  poll \
  ppoll \
  read \
  readlink \
  readlinkat \
  ttyname_r \
  # routines_no_fortify

others := \
 pwd \
 # others

test-srcs := \
  ftwtest \
  ftwtest-time64 \
  # test-srcs

tests := \
  bug-ftw1 \
  bug-ftw2 \
  bug-ftw3 \
  bug-ftw4 \
  bug-ftw5 \
  test-lfs \
  test-stat \
  test-stat2 \
  test-utime \
  tst-close_range \
  tst-closefrom \
  tst-copy_file_range \
  tst-faccessat \
  tst-fchmodat \
  tst-fchownat \
  tst-fcntl \
  tst-fcntl-lock \
  tst-fcntl-lock-lfs \
  tst-fstatat \
  tst-fts \
  tst-fts-lfs \
  tst-ftw-bz26353 \
  tst-ftw-bz28126 \
  tst-ftw-lnk \
  tst-futimens \
  tst-futimes \
  tst-futimesat \
  tst-getcwd \
  tst-getcwd-abspath \
  tst-lchmod \
  tst-linkat \
  tst-lockf \
  tst-lutimes \
  tst-mkdirat \
  tst-mkfifoat \
  tst-mknodat \
  tst-open-tmpfile \
  tst-openat \
  tst-posix_fallocate \
  tst-posix_fallocate64 \
  tst-read-zero \
  tst-readlinkat \
  tst-renameat \
  tst-stat \
  tst-stat-lfs \
  tst-statvfs \
  tst-symlinkat \
  tst-ttyname_r \
  tst-unlinkat \
  tst-utime \
  tst-utimensat \
  tst-utimes \
  # tests

tests-time64 := \
  tst-fcntl-time64 \
  tst-fts-time64 \
  tst-futimens-time64 \
  tst-futimes-time64\
  tst-futimesat-time64 \
  tst-lchmod-time64 \
  tst-lutimes-time64 \
  tst-stat-time64 \
  tst-utime-time64 \
  tst-utimensat-time64 \
  tst-utimes-time64 \
  # tests-time64

# Likewise for statx, but we do not need static linking here.
tests-internal += \
  tst-file_change_detection \
  tst-statx \
  # tests-internal

tests-static += \
  tst-statx \
  # tests-static

ifeq ($(run-built-tests),yes)
tests-special += \
  $(objpfx)ftwtest-time64.out \
  $(objpfx)ftwtest.out \
  # tests-special
endif

include ../Rules

CFLAGS-open.c += -fexceptions -fasynchronous-unwind-tables $(config-cflags-wno-ignored-attributes)
CFLAGS-openat.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-open64.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-creat.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-creat64.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-fcntl.c += -fexceptions -fasynchronous-unwind-tables $(config-cflags-wno-ignored-attributes)
CFLAGS-fcntl64.c += -fexceptions -fasynchronous-unwind-tables $(config-cflags-wno-ignored-attributes)
CFLAGS-poll.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-ppoll.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-lockf.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-lockf64.c += -fexceptions -fasynchronous-unwind-tables $(config-cflags-wno-ignored-attributes)
CFLAGS-statfs.c += -fexceptions
CFLAGS-fstatfs.c += -fexceptions
CFLAGS-statvfs.c += -fexceptions $(config-cflags-wno-ignored-attributes)
CFLAGS-fstatvfs.c += -fexceptions $(config-cflags-wno-ignored-attributes)
CFLAGS-fts.c += -Wno-uninitialized $(uses-callbacks) -fexceptions
CFLAGS-fts64.c += -Wno-uninitialized $(uses-callbacks) -fexceptions
CFLAGS-fts64-time64.c += -Wno-uninitialized $(uses-callbacks) -fexceptions
CFLAGS-ftw.c += $(uses-callbacks) -fexceptions
CFLAGS-ftw64.c += $(uses-callbacks) -fexceptions
CFLAGS-ftw64-time64.c += $(uses-callbacks) -fexceptions
CFLAGS-posix_fallocate.c += -fexceptions
CFLAGS-posix_fallocate64.c += -fexceptions
CFLAGS-fallocate.c += -fexceptions
CFLAGS-fallocate64.c += -fexceptions
CFLAGS-read.c += -fexceptions -fasynchronous-unwind-tables $(config-cflags-wno-ignored-attributes)
CFLAGS-write.c += -fexceptions -fasynchronous-unwind-tables $(config-cflags-wno-ignored-attributes)
CFLAGS-close.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-lseek64.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-tst-read-zero.c += $(no-fortify-source),-D_FORTIFY_SOURCE=$(supported-fortify)

CFLAGS-test-stat.c += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CFLAGS-test-lfs.c += -D_LARGEFILE64_SOURCE
CFLAGS-tst-lchmod.c += -D_FILE_OFFSET_BITS=64

test-stat2-ARGS = Makefile . $(objpfx)test-stat2

tst-statvfs-ARGS = $(objpfx)tst-statvfs tst-statvfs.c /tmp

tst-open-tmpfile-ARGS = --test-dir=$(objpfx)

CFLAGS-ftwtest-time64.c += -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64

ifeq ($(run-built-tests),yes)
$(objpfx)ftwtest.out: ftwtest-sh $(objpfx)ftwtest
	$(SHELL) $< $(common-objpfx) '$(test-program-cmd)' > $@; \
	$(evaluate-test)

$(objpfx)ftwtest-time64.out: ftwtest-sh $(objpfx)ftwtest-time64
	$(SHELL) $< $(common-objpfx) '$(test-program-cmd)' > $@; \
	$(evaluate-test)
endif
