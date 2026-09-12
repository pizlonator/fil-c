# Copyright (C) 1996-2026 Free Software Foundation, Inc.
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
#	Sub-makefile for login portion of the library.
#

subdir	:= login

include ../Makeconfig

headers := \
  bits/utmp.h \
  lastlog.h \
  pty.h \
  utmp.h \
  # headers

routines := \
  forkpty \
  getlogin \
  getlogin_r \
  getlogin_r_chk \
  getpt \
  getutent \
  getutent_r \
  getutid \
  getutid_r \
  getutline \
  getutline_r \
  grantpt \
  login \
  login_tty \
  logout \
  logwtmp \
  openpty \
  ptsname \
  ptsname_r_chk \
  setlogin \
  unlockpt \
  updwtmp \
  utmp_file \
  utmpname \
  # routines

CFLAGS-grantpt.c += -DLIBEXECDIR='"$(libexecdir)"'

others = utmpdump

ifeq (yes,$(build-pt-chown))
others += \
  pt_chown \
  # others
others-pie = \
  pt_chown \
  # others-pie
install-others-programs = $(inst_libexecdir)/pt_chown
endif

subdir-dirs = programs
vpath %.c programs

tests := \
  tst-getlogin \
  tst-grantpt \
  tst-ptsname \
  tst-pututxline-cache \
  tst-pututxline-lockfail \
  tst-updwtmpx \
  tst-utmp \
  tst-utmp-size \
  tst-utmp-size-64 \
  tst-utmp-unsigned \
  tst-utmp-unsigned-64 \
  tst-utmpx \
  # tests

CFLAGS-tst-utmp-size-64.c += -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64
CFLAGS-tst-utmp-unsigned-64.c += -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64

# Empty compatibility library for old binaries.
extra-libs      := libutil
extra-libs-others := $(extra-libs)
ifeq ($(have-GLIBC_2.33),yes)
libutil-routines := libutil-compat
libutil-shared-only-routines := libutil-compat

# Pretend that libutil.so is a linker script, so that the symbolic
# link is not installed.
install-lib-ldscripts = libutil.so
$(inst_libdir)/libutil.so:
else  # not $(have-GLIBC_2.33)
libutil-inhibit-o = $(filter-out .o,$(object-suffixes))
endif # $(have-GLIBC_2.33)

include ../Rules

CFLAGS-getpt.c += -fexceptions

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  getlogin_r \
  ptsname_r \
  # routines_no_fortify

ifeq (yesyes,$(have-fpie)$(build-shared))
pt_chown-cflags += $(pie-ccflag)
endif
ifeq (yes,$(have-libcap))
libcap = -lcap
endif
CFLAGS-pt_chown.c += $(pt_chown-cflags)
LDLIBS-pt_chown = $(libcap)
ifeq (yesyes,$(have-fpie)$(build-shared))
LDFLAGS-pt_chown = -Wl,-z,now
endif

# pt_chown needs to be setuid root.
$(inst_libexecdir)/pt_chown: $(objpfx)pt_chown $(+force)
	$(make-target-directory)
	-$(INSTALL_PROGRAM) -m 4755 -o root $< $@

$(objpfx)tst-pututxline-lockfail: $(shared-thread-library)
$(objpfx)tst-pututxline-cache: $(shared-thread-library)
