# Copyright (C) 1991-2024 Free Software Foundation, Inc.
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
#	Sub-makefile for misc portion of the library.
#

subdir	:= misc

include ../Makeconfig

headers	:= \
  sys/uio.h \
  bits/uio-ext.h \
  bits/uio_lim.h \
  sys/ioctl.h \
  bits/ioctls.h \
  bits/ioctl-types.h \
  sys/ptrace.h \
  sys/file.h \
  sys/dir.h \
  sys/cdefs.h \
  ar.h \
  a.out.h \
  libgen.h \
  stab.h \
  bits/stab.def \
  sgtty.h \
  ttyent.h \
  paths.h \
  sys/reboot.h \
  sys/mman.h \
  sys/param.h \
  bits/param.h \
  fstab.h \
  mntent.h \
  search.h \
  err.h \
  error.h \
  sys/queue.h \
  sysexits.h \
  syscall.h \
  sys/syscall.h \
  sys/swap.h \
  sys/select.h \
  sys/sysinfo.h \
  regexp.h \
  bits/select.h \
  bits/mman.h \
  sys/xattr.h \
  syslog.h \
  sys/syslog.h \
  bits/syslog-decl.h \
  bits/syslog.h \
  bits/syslog-ldbl.h \
  bits/syslog-path.h \
  bits/error.h \
  bits/select-decl.h \
  bits/select2.h \
  bits/hwcap.h \
  sys/auxv.h \
  sys/sysmacros.h \
  bits/sysmacros.h \
  bits/types/struct_iovec.h \
  bits/err-ldbl.h \
  bits/error-ldbl.h \
  sys/single_threaded.h \
  # headers

routines := \
  acct \
  allocate_once \
  atomic_wide_counter \
  brk \
  chflags \
  chroot \
  daemon \
  dirname \
  efgcvt \
  efgcvt_r \
  err \
  error \
  fchflags \
  fd_to_filename \
  fdatasync \
  fgetxattr \
  flistxattr \
  fremovexattr \
  fsetxattr \
  fstab \
  fsync \
  ftruncate \
  ftruncate64 \
  futimes \
  futimesat \
  getauxval \
  getclktck \
  getdomain \
  getdtsz \
  gethostid \
  gethostname \
  getloadavg \
  getpagesize \
  getpass \
  getsysstats \
  getttyent \
  getusershell \
  getxattr \
  gtty \
  hsearch \
  hsearch_r \
  ifunc-impl-list \
  insremque \
  ioctl \
  lgetxattr \
  listxattr \
  llistxattr \
  lremovexattr \
  lsearch \
  lsetxattr \
  lutimes \
  madvise \
  makedev \
  mincore \
  mkdtemp \
  mkostemp \
  mkostemp64 \
  mkostemps \
  mkostemps64 \
  mkstemp \
  mkstemp64 \
  mkstemps \
  mkstemps64 \
  mktemp \
  mlock \
  mlockall \
  mmap \
  mmap64 \
  mntent \
  mntent_r \
  mprotect \
  msync \
  munlock \
  munlockall \
  munmap \
  preadv \
  preadv2 \
  preadv64 \
  preadv64v2 \
  pselect \
  ptrace \
  pwritev \
  pwritev2 \
  pwritev64 \
  pwritev64v2 \
  qefgcvt \
  qefgcvt_r \
  readv \
  reboot \
  regexp \
  remap_file_pages \
  removexattr \
  revoke \
  sbrk \
  select \
  setdomain \
  setegid \
  seteuid \
  sethostid \
  sethostname \
  setregid \
  setreuid \
  setxattr \
  single_threaded \
  sstk \
  stty \
  swapoff \
  swapon \
  sync \
  syncfs \
  syscall \
  syslog \
  truncate \
  truncate64 \
  tsearch \
  ttyslot \
  ualarm \
  unwind-link \
  usleep \
  ustat \
  utimes \
  vhangup \
  writev \
  # routines

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  getdomain \
  gethostname \
  syslog \
  # routines_no_fortify

ifeq ($(run-built-tests),yes)
ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
generated += \
  tst-allocate_once-mem.out \
  tst-allocate_once.mtrace \
  tst-error1-mem.out \
  tst-error1.mtrace \
  # generated
endif
endif
endif

aux := init-misc
install-lib := libg.a

gpl2lgpl := \
  error.c \
  error.h \
  # gpl2lgpl

tests := \
  bug-hsearch1 \
  bug18240 \
  tst-dbl-efgcvt \
  tst-dirname \
  tst-empty \
  tst-error1 \
  tst-fdset \
  tst-hsearch \
  tst-insremque \
  tst-ioctl \
  tst-ldbl-efgcvt \
  tst-ldbl-error \
  tst-ldbl-errorfptr \
  tst-ldbl-warn \
  tst-makedev \
  tst-mntent \
  tst-mntent-autofs \
  tst-mntent-blank-corrupt \
  tst-mntent-blank-passno \
  tst-mntent-escape \
  tst-mntent2 \
  tst-mremap1 \
  tst-mremap2 \
  tst-preadvwritev \
  tst-preadvwritev2 \
  tst-preadvwritev64 \
  tst-preadvwritev64v2 \
  tst-pselect \
  tst-select \
  tst-syscalls \
  tst-tsearch \
  tst-warn-wide \
  # tests

tests-time64 := \
  tst-ioctl-time64 \
  tst-pselect-time64 \
  tst-select-time64 \
  # tests-time64

# Tests which need libdl.
ifeq (yes,$(build-shared))
tests += tst-gethostid
endif

tests-internal := \
  tst-allocate_once \
  tst-atomic \
  tst-atomic-long \
  # tests-internal
tests-static := tst-empty

# Test for the internal, non-exported __fd_to_filename function.
tests-internal += tst-fd_to_filename
tests-static += tst-fd_to_filename

ifeq ($(run-built-tests),yes)
ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
tests-special += \
  $(objpfx)tst-allocate_once-mem.out \
  $(objpfx)tst-error1-mem.out \
  # tests-special
endif
endif
endif

tests-container := \
  tst-syslog \
  tst-syslog-long-progname \
  # tests-container

CFLAGS-select.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-tsearch.c += $(uses-callbacks)
CFLAGS-lsearch.c += $(uses-callbacks)
CFLAGS-pselect.c += -fexceptions
CFLAGS-readv.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-writev.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-preadv.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-preadv64.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-pwritev.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-pwritev64.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-preadv2.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-preadv64v2.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-pwritev2.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-pwritev64v2.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-usleep.c += -fexceptions
CFLAGS-syslog.c += -fexceptions
CFLAGS-error.c += -fexceptions
CFLAGS-getpass.c += -fexceptions
CFLAGS-mkstemp.c += -fexceptions
CFLAGS-mkstemp64.c += -fexceptions
CFLAGS-getsysstats.c += -fexceptions
CFLAGS-getusershell.c += -fexceptions
CFLAGS-err.c += -fexceptions
CFLAGS-tst-tsearch.c += $(stack-align-test-flags)
CFLAGS-msync.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-fdatasync.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-fsync.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-makedev.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-mmap64.c += $(config-cflags-wno-ignored-attributes)

# Called during static library initialization, so turn stack-protection
# off for non-shared builds.
CFLAGS-sbrk.o = $(no-stack-protector)
CFLAGS-sbrk.op = $(no-stack-protector)
CFLAGS-brk.o = $(no-stack-protector)
CFLAGS-brk.op = $(no-stack-protector)
CFLAGS-mmap.o = $(no-stack-protector)
CFLAGS-mmap.op = $(no-stack-protector)
CFLAGS-mmap64.o = $(no-stack-protector)
CFLAGS-mmap64.op = $(no-stack-protector)

include ../Rules

$(objpfx)libg.a: $(dep-dummy-lib); $(make-dummy-lib)

$(objpfx)tst-tsearch: $(libm)

tst-error1-ENV = MALLOC_TRACE=$(objpfx)tst-error1.mtrace \
		 LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
tst-error1-ARGS = $(objpfx)tst-error1.out
$(objpfx)tst-error1-mem.out: $(objpfx)tst-error1.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-error1.mtrace > $@; \
	$(evaluate-test)

tst-allocate_once-ENV = MALLOC_TRACE=$(objpfx)tst-allocate_once.mtrace \
			LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
$(objpfx)tst-allocate_once-mem.out: $(objpfx)tst-allocate_once.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-allocate_once.mtrace > $@; \
	$(evaluate-test)

tst-syslog-long-progname-ENV = GLIBC_TUNABLES=glibc.malloc.check=3 \
			       LD_PRELOAD=libc_malloc_debug.so.0

$(objpfx)tst-select: $(librt)
$(objpfx)tst-select-time64: $(librt)
$(objpfx)tst-pselect: $(librt)
$(objpfx)tst-pselect-time64: $(librt)
