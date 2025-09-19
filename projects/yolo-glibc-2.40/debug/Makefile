# Copyright (C) 1998-2024 Free Software Foundation, Inc.
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
#	Sub-makefile for debug portion of the library.
#
subdir	:= debug

include ../Makeconfig

headers	:= execinfo.h

# Note that ptsname_r_chk and getlogin_r are not here, but in
# login/Makefile instead.  If that subdir is omitted from the
# build, its _FORTIFY_SOURCE support will be too.
routines = \
  $(static-only-routines) \
  ____longjmp_chk \
  asprintf_chk \
  backtrace \
  backtracesyms \
  backtracesymsfd \
  chk_fail \
  confstr_chk \
  dprintf_chk \
  explicit_bzero_chk \
  fdelt_chk \
  fgets_chk \
  fgets_u_chk \
  fgetws_chk \
  fgetws_u_chk \
  fortify_fail \
  fprintf_chk \
  fread_chk \
  fread_u_chk \
  fwprintf_chk \
  getcwd_chk \
  getdomainname_chk \
  getgroups_chk \
  gethostname_chk \
  gets_chk \
  getwd_chk \
  longjmp_chk \
  mbsnrtowcs_chk \
  mbsrtowcs_chk \
  mbstowcs_chk \
  memcpy_chk \
  memmove_chk \
  mempcpy_chk \
  memset_chk \
  noophooks \
  obprintf_chk \
  poll_chk \
  ppoll_chk \
  pread64_chk \
  pread_chk \
  printf_chk \
  read_chk \
  readlink_chk \
  readlinkat_chk \
  readonly-area \
  realpath_chk \
  recv_chk \
  recvfrom_chk \
  snprintf_chk \
  sprintf_chk \
  stack_chk_fail \
  stpcpy_chk \
  stpncpy_chk \
  strcat_chk \
  strcpy_chk \
  strlcat_chk \
  strlcpy_chk \
  strncat_chk \
  strncpy_chk \
  swprintf_chk \
  ttyname_r_chk \
  vasprintf_chk \
  vdprintf_chk \
  vfprintf_chk \
  vfwprintf_chk \
  vobprintf_chk \
  vprintf_chk \
  vsnprintf_chk \
  vsprintf_chk \
  vswprintf_chk \
  vwprintf_chk \
  wcpcpy_chk \
  wcpncpy_chk \
  wcrtomb_chk \
  wcscat_chk \
  wcscpy_chk \
  wcslcat_chk \
  wcslcpy_chk \
  wcsncat_chk \
  wcsncpy_chk \
  wcsnrtombs_chk \
  wcsrtombs_chk \
  wcstombs_chk \
  wctomb_chk \
  wmemcpy_chk \
  wmemmove_chk \
  wmempcpy_chk \
  wmemset_chk \
  wprintf_chk \
  # routines
static-only-routines := stack_chk_fail_local

# Don't add stack_chk_fail_local.o to libc.a since __stack_chk_fail_local
# is an alias of __stack_chk_fail in stack_chk_fail.o.
elide-routines.o := stack_chk_fail_local

# Building the stack-protector failure routines with stack protection
# is not required since we have already failed a stack check and are
# exiting the process.  However, the local aliases which jump to the
# real routines should still be compiled with stack protection
# (stack_chk_fail_local.c), so that the statically linked parts of the
# library have the expected flags.

CFLAGS-stack_chk_fail.c += $(no-stack-protector)

CFLAGS-backtrace.c += -fno-omit-frame-pointer -funwind-tables
CFLAGS-sprintf_chk.c += $(libio-mtsafe)
CFLAGS-snprintf_chk.c += $(libio-mtsafe)
CFLAGS-vsprintf_chk.c += $(libio-mtsafe)
CFLAGS-vsnprintf_chk.c += $(libio-mtsafe)
CFLAGS-asprintf_chk.c += $(libio-mtsafe)
CFLAGS-vasprintf_chk.c += $(libio-mtsafe)
CFLAGS-obprintf_chk.c += $(libio-mtsafe)
CFLAGS-dprintf_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-vdprintf_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-printf_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-fprintf_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-vprintf_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-vfprintf_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-gets_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-fgets_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-fgets_u_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-fread_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-fread_u_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-swprintf_chk.c += $(libio-mtsafe)
CFLAGS-vswprintf_chk.c += $(libio-mtsafe)
CFLAGS-wprintf_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-fwprintf_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-vwprintf_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-vfwprintf_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-fgetws_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-fgetws_u_chk.c += $(libio-mtsafe) -fexceptions
CFLAGS-read_chk.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-pread_chk.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-pread64_chk.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-recv_chk.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-recvfrom_chk.c += -fexceptions -fasynchronous-unwind-tables

# Need to make sure the settings here override what configure might have
# set up for us, so keep the CFLAGS/CPPFLAGS split logical as the order is:
# <user CFLAGS> <test CFLAGS> <user CPPFLAGS> <test CPPFLAGS>
CFLAGS-tst-longjmp_chk.c += -fexceptions -fasynchronous-unwind-tables
CPPFLAGS-tst-longjmp_chk.c += $(no-fortify-source) -D_FORTIFY_SOURCE=1
CFLAGS-tst-longjmp_chk2.c += -fexceptions -fasynchronous-unwind-tables
CPPFLAGS-tst-longjmp_chk2.c += $(no-fortify-source) -D_FORTIFY_SOURCE=1
CFLAGS-tst-longjmp_chk3.c += -fexceptions -fasynchronous-unwind-tables
CPPFLAGS-tst-longjmp_chk3.c += $(no-fortify-source) -D_FORTIFY_SOURCE=1
CPPFLAGS-tst-realpath-chk.c += $(no-fortify-source) -D_FORTIFY_SOURCE=2
CPPFLAGS-tst-chk-cancel.c += $(no-fortify-source) -D_FORTIFY_SOURCE=2
CFLAGS-tst-sprintf-fortify-rdonly.c += $(no-fortify-source) -D_FORTIFY_SOURCE=2
CFLAGS-tst-fortify-syslog.c += $(no-fortify-source) -D_FORTIFY_SOURCE=2
CFLAGS-tst-fortify-wide.c += $(no-fortify-source) -D_FORTIFY_SOURCE=2

# _FORTIFY_SOURCE tests.
# Auto-generate tests for _FORTIFY_SOURCE for different levels, compilers and
# preprocessor conditions based on tst-fortify.c.
#
# To add a new test condition, define a cflags-$(cond) make variable to set
# CFLAGS for the file.

tests-all-chk = tst-fortify
tests-c-def-chk =
tests-cc-def-chk =
tests-c-time64-chk =
tests-cc-time64-chk =

CFLAGS-tst-fortify.c += -Wno-format -Wno-deprecated-declarations -Wno-error

# No additional flags for the default tests.
define cflags-default
endef

define cflags-lfs
CFLAGS-tst-fortify-$(1)-lfs-$(2)-$(3).$(1) += -D_FILE_OFFSET_BITS=64
endef

define cflags-nongnu
CFLAGS-tst-fortify-$(1)-nongnu-$(2)-$(3).$(1) += -D_LARGEFILE64_SOURCE=1
endef

src-chk-nongnu = \#undef _GNU_SOURCE

# We know these tests have problems with format strings, this is what
# we are testing.  Disable that warning.  They are also testing
# deprecated functions (notably gets) so disable that warning as well.
# And they also generate warnings from warning attributes, which
# cannot be disabled via pragmas, so require -Wno-error to be used.
define gen-chk-test
tests-$(1)-$(4)-chk += tst-fortify-$(1)-$(2)-$(3)-$(4)
CFLAGS-tst-fortify-$(1)-$(2)-$(3)-$(4).$(1) += $(no-fortify-source) -D_FORTIFY_SOURCE=$(3) -Wno-format \
					  -Wno-deprecated-declarations \
					  -Wno-error
$(eval $(call cflags-$(2),$(1),$(3),$(4)))
$(objpfx)tst-fortify-$(1)-$(2)-$(3)-$(4).$(1): tst-fortify.c Makefile
	( echo "/* Autogenerated from Makefile.  */"; \
	  echo "$(src-chk-$(2))"; \
	  echo "#include \"tst-fortify.c\"" ) > $$@.tmp
	mv $$@.tmp $$@
endef

chk-extensions = c cc
chk-types = default lfs nongnu
# 64 bit time_t implies LFS.
chk-types-time64 = default nongnu
chk-levels = 1 2 3

$(foreach e,$(chk-extensions), \
  $(foreach t,$(chk-types), \
    $(foreach l,$(chk-levels), \
      $(eval $(call gen-chk-test,$(e),$(t),$(l),def)))))

tests-all-chk += $(tests-c-def-chk) $(tests-cc-def-chk)

$(foreach e,$(chk-extensions), \
  $(foreach t,$(chk-types-time64), \
    $(foreach l,$(chk-levels), \
      $(eval $(call gen-chk-test,$(e),$(t),$(l),time64)))))

tests-all-time64-chk += $(tests-c-time64-chk) $(tests-cc-time64-chk)

define link-cc
LDLIBS-$(1) = -lstdc++
endef
$(foreach t,$(tests-cc-def-chk), $(eval $(call link-cc,$(t))))
$(foreach t,$(tests-cc-time64-chk), $(eval $(call link-cc,$(t))))

# backtrace_symbols only works if we link with -rdynamic.  backtrace
# requires unwind tables on most architectures.
CFLAGS-tst-backtrace2.c += -funwind-tables
CFLAGS-tst-backtrace3.c += -funwind-tables
CFLAGS-tst-backtrace4.c += -funwind-tables
CFLAGS-tst-backtrace5.c += -funwind-tables
CFLAGS-tst-backtrace6.c += -funwind-tables
LDFLAGS-tst-backtrace2 = -rdynamic
LDFLAGS-tst-backtrace3 = -rdynamic
LDFLAGS-tst-backtrace4 = -rdynamic
LDFLAGS-tst-backtrace5 = -rdynamic
LDFLAGS-tst-backtrace6 = -rdynamic

CFLAGS-tst-ssp-1.c += -fstack-protector-all

# Disable compiler optimizations around vsprintf (the function under test).
CFLAGS-tst-sprintf-fortify-unchecked.c = \
  -fno-builtin-vsprintf -fno-builtin-__vsprintf_chk

tests = \
  $(tests-all-chk) \
  backtrace-tst \
  test-stpcpy_chk \
  test-strcpy_chk \
  tst-backtrace2 \
  tst-backtrace3 \
  tst-backtrace4 \
  tst-backtrace5 \
  tst-backtrace6 \
  tst-fortify-wide \
  tst-longjmp_chk \
  tst-longjmp_chk2 \
  tst-realpath-chk \
  tst-sprintf-fortify-rdonly \
  tst-sprintf-fortify-unchecked \
  # tests

tests-time64 += \
  $(tests-all-time64-chk) \
  # tests-time64

tests-container += \
  tst-fortify-syslog \
  # tests-container

ifeq ($(have-ssp),yes)
tests += tst-ssp-1
endif

ifeq ($(have-thread-library), yes)
tests += tst-chk-cancel
endif

ifeq (,$(CXX))
tests-unsupported = $(tests-cc-def-chk) $(tests-cc-time64-chk)
endif

extra-libs = libpcprofile
extra-libs-others = $(extra-libs)

libpcprofile-routines = pcprofile
libpcprofile-inhibit-o = $(filter-out .os,$(object-suffixes))

others = pcprofiledump
install-bin = pcprofiledump
install-bin-script = xtrace

generated += xtrace

include ../Rules

ifeq ($(run-built-tests),yes)
LOCALES := de_DE.UTF-8
include ../gen-locales.mk

define chk-gen-locales
$(objpfx)$(1).out: $(gen-locales)
endef
$(foreach t, $(tests-all-chk), $(eval $(call chk-gen-locales,$(t))))
$(foreach t, $(tests-all-time64-chk), $(eval $(call chk-gen-locales,$(t))))
endif

sLIBdir := $(shell echo $(slibdir) | sed 's,lib\(\|64\)$$,\\\\$$LIB,')

$(objpfx)pcprofiledump: $(objpfx)pcprofiledump.o

$(objpfx)xtrace: xtrace.sh
	rm -f $@.new
	sed -e 's|@VERSION@|$(version)|' -e 's|@SLIBDIR@|$(sLIBdir)|' \
	    -e 's|@BINDIR@|$(bindir)|' -e 's|@PKGVERSION@|$(PKGVERSION)|' \
	    -e 's|@REPORT_BUGS_TO@|$(REPORT_BUGS_TO)|' $^ > $@.new \
	&& rm -f $@ && mv $@.new $@ && chmod +x $@

$(objpfx)tst-chk-cancel: $(shared-thread-library)
