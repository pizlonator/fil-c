# Copyright (C) 1995-2024 Free Software Foundation, Inc.
# Copyright The GNU Toolchain Authors.
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
#	Sub-makefile for gmon portion of the library.
#
subdir	:= gmon

include ../Makeconfig

headers := \
  sys/gmon.h \
  sys/gmon_out.h \
  sys/profil.h \
  # headers
routines := \
  gmon \
  mcount \
  prof-freq \
  profil \
  sprofil \
  # routines

tests = \
  tst-gmon \
  tst-mcleanup \
  tst-mcount-overflow \
  tst-sprofil \
  # tests
ifeq ($(build-profile),yes)
tests += tst-profile-static
tests-static += tst-profile-static

LDFLAGS-tst-profile-static = -profile
endif

tests += tst-gmon-static
tests-static += tst-gmon-static

ifeq (yesyes,$(have-fpie)$(build-shared))
tests += tst-gmon-pie
tests-pie += tst-gmon-pie
ifeq (yes,$(enable-static-pie))
tests += tst-gmon-static-pie
tests-static += tst-gmon-static-pie
endif
endif

# The mcount code won't work without a frame pointer.
CFLAGS-mcount.c := -fno-omit-frame-pointer

CFLAGS-tst-gmon.c := -fno-omit-frame-pointer -pg
tst-gmon-no-pie = yes
CRT-tst-gmon := $(csu-objpfx)g$(start-installed-name)
tst-gmon-ENV := GMON_OUT_PREFIX=$(objpfx)tst-gmon.data
ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-gmon-gprof.out
endif

CFLAGS-tst-mcount-overflow.c := -fno-omit-frame-pointer -pg
tst-mcount-overflow-no-pie = yes
CRT-tst-mcount-overflow := $(csu-objpfx)g$(start-installed-name)
# Intentionally use invalid config where maxarcs<minarcs to check warning is printed
tst-mcount-overflow-ENV := GMON_OUT_PREFIX=$(objpfx)tst-mcount-overflow.data \
                           GLIBC_TUNABLES=glibc.gmon.minarcs=51:glibc.gmon.maxarcs=50
# Send stderr into output file because we make sure expected messages are printed
tst-mcount-overflow-ARGS := 2>&1 1>/dev/null | cat
ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-mcount-overflow-check.out
endif

CFLAGS-tst-mcleanup.c := -fno-omit-frame-pointer -pg
tst-mcleanup-no-pie = yes
CRT-tst-mcleanup := $(csu-objpfx)g$(start-installed-name)
tst-mcleanup-ENV := GMON_OUT_PREFIX=$(objpfx)tst-mcleanup.data
ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-mcleanup.out
endif

CFLAGS-tst-gmon-static.c := $(PIE-ccflag) -fno-omit-frame-pointer -pg
CRT-tst-gmon-static := $(csu-objpfx)g$(static-start-installed-name)
tst-gmon-static-no-pie = yes
tst-gmon-static-ENV := GMON_OUT_PREFIX=$(objpfx)tst-gmon-static.data
ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-gmon-static-gprof.out
endif

CFLAGS-tst-gmon-pie.c := $(PIE-ccflag) -fno-omit-frame-pointer -pg
CRT-tst-gmon-pie := $(csu-objpfx)g$(start-installed-name)
tst-gmon-pie-ENV := GMON_OUT_PREFIX=$(objpfx)tst-gmon-pie.data
ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-gmon-pie-gprof.out
endif

ifeq (yes,$(enable-static-pie))
CFLAGS-tst-gmon-static-pie.c := $(PIE-ccflag) -fno-omit-frame-pointer -pg
CRT-tst-gmon-static-pie := $(csu-objpfx)gr$(static-start-installed-name)
tst-gmon-static-pie-ENV := GMON_OUT_PREFIX=$(objpfx)tst-gmon-static-pie.data
ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-gmon-static-pie-gprof.out
endif
endif


include ../Rules

# We cannot compile mcount.c with -pg because that would
# create recursive calls.  Just copy the normal static object.
# On systems where `profil' is not a system call, the same
# problem exists for the internal functions in profil.c.

noprof := mcount $(sysdep_noprof)
ifeq (,$(filter profil,$(unix-syscalls)))
noprof += profil sprofil
endif

$(noprof:%=$(objpfx)%.op): %.op: %.o
	rm -f $@
	ln $< $@

# GMON_OUTPUT_PREFIX only sets the output prefix.  The actual file
# name contains the PID as well.
$(objpfx)tst-gmon.out: clean-tst-gmon-data
clean-tst-gmon-data:
	rm -f $(objpfx)tst-gmon.data.*

$(objpfx)tst-mcount-overflow.o: clean-tst-mcount-overflow-data
clean-tst-mcount-overflow-data:
	rm -f $(objpfx)tst-mcount-overflow.data.*

$(objpfx)tst-mcount-overflow-check.out: tst-mcount-overflow-check.sh $(objpfx)tst-mcount-overflow.out
	$(SHELL) $< $(objpfx)tst-mcount-overflow > $@; \
	$(evaluate-test)

$(objpfx)tst-mcleanup.out: clean-tst-mcleanup-data
clean-tst-mcleanup-data:
	rm -f $(objpfx)tst-mcleanup.data.*

$(objpfx)tst-gmon-gprof.out: tst-gmon-gprof.sh $(objpfx)tst-gmon.out
	$(SHELL) $< $(GPROF) $(objpfx)tst-gmon $(objpfx)tst-gmon.data.* > $@; \
	$(evaluate-test)

$(objpfx)tst-gmon-static.out: clean-tst-gmon-static-data
clean-tst-gmon-static-data:
	rm -f $(objpfx)tst-gmon-static.data.*

$(objpfx)tst-gmon-static-gprof.out: tst-gmon-static-gprof.sh \
				    $(objpfx)tst-gmon-static.out
	$(SHELL) $< $(GPROF) $(objpfx)tst-gmon-static \
		$(objpfx)tst-gmon-static.data.* > $@; \
	$(evaluate-test)

$(objpfx)tst-gmon-pie.out: clean-tst-gmon-pie-data
clean-tst-gmon-pie-data:
	rm -f $(objpfx)tst-gmon-pie.data.*

$(objpfx)tst-gmon-pie-gprof.out: tst-gmon-gprof.sh $(objpfx)tst-gmon-pie.out
	$(SHELL) $< $(GPROF) $(objpfx)tst-gmon-pie $(objpfx)tst-gmon-pie.data.* > $@; \
	$(evaluate-test)

$(objpfx)tst-gmon-static-pie.out: clean-tst-gmon-static-pie-data
clean-tst-gmon-static-pie-data:
	rm -f $(objpfx)tst-gmon-static-pie.data.*

$(objpfx)tst-gmon-static-pie-gprof.out: tst-gmon-static-gprof.sh \
					$(objpfx)tst-gmon-static-pie.out
	$(SHELL) $< $(GPROF) $(objpfx)tst-gmon-static-pie \
		$(objpfx)tst-gmon-static-pie.data.* > $@; \
	$(evaluate-test)
