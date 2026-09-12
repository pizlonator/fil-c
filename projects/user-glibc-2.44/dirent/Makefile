# Copyright (C) 1991-2026 Free Software Foundation, Inc.
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
#	Sub-makefile for dirent portion of the library.
#
subdir		:= dirent

include ../Makeconfig

headers := \
  bits/dirent.h \
  bits/dirent_ext.h \
  dirent.h \
  # headers
routines := \
  alphasort \
  alphasort64 \
  closedir \
  dirfd \
  fdopendir \
  getdents \
  getdents64 \
  opendir \
  readdir \
  readdir64 \
  readdir64_r \
  readdir_r \
  rewinddir \
  scandir \
  scandir-cancel \
  scandir-tail \
  scandir64 \
  scandir64-tail \
  scandirat \
  scandirat64 \
  seekdir \
  telldir \
  versionsort \
  versionsort64 \
  # routines

tests := \
  bug-readdir1 \
  list \
  opendir-tst1 \
  tst-closedir-leaks \
  tst-fdopendir \
  tst-fdopendir2 \
  tst-readdir-long \
  tst-readdir-zero-inode \
  tst-rewinddir \
  tst-scandir \
  tst-scandir64 \
  tst-seekdir \
  # tests

ifeq ($(run-built-tests),yes)
ifneq ($(PERL),no)
generated += \
  $(objpfx)tst-closedir-leaks-mem.out \
  # generated

tests-special += \
  $(objpfx)tst-closedir-leaks-mem.out \
  # tests-special
endif # $(PERL) ! no
endif # $(run-built-tests) == yes

CFLAGS-scandir.c += $(uses-callbacks)
CFLAGS-scandir64.c += $(uses-callbacks)
CFLAGS-scandir-tail.c += $(uses-callbacks)
CFLAGS-scandir64-tail.c += $(uses-callbacks)

include ../Rules

opendir-tst1-ARGS = --test-dir=${common-objpfx}dirent

tst-closedir-leaks-ENV += MALLOC_TRACE=$(objpfx)tst-closedir-leaks.mtrace \
		   LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)tst-closedir-leaks-mem.out: $(objpfx)tst-closedir-leaks.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-closedir-leaks.mtrace > $@; \
	$(evaluate-test)
