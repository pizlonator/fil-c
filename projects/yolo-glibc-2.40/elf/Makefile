# Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

# Makefile for elf subdirectory of GNU C Library.

subdir		:= elf

include ../Makeconfig

headers = \
  bits/elfclass.h \
  bits/link.h \
  bits/link_lavcurrent.h \
  elf.h \
  link.h \
  # headers

routines = \
  $(all-dl-routines) \
  dl-addr \
  dl-addr-obj \
  dl-early_allocate \
  dl-find_object \
  dl-iteratephdr \
  dl-libc \
  dl-origin \
  dl-reloc-static-pie \
  dl-support \
  dl-sym \
  dl-sysdep \
  enbl-secure \
  libc-dl-profstub  \
  libc-dl_find_object \
  libc_early_init \
  rtld_static_init \
  # routines

# The core dynamic linking functions are in libc for the static and
# profiled libraries.
dl-routines = \
  dl-call-libc-early-init \
  dl-call_fini \
  dl-catch \
  dl-close \
  dl-debug \
  dl-debug-symbols \
  dl-deps \
  dl-exception \
  dl-execstack \
  dl-fini \
  dl-init \
  dl-load \
  dl-lookup \
  dl-lookup-direct \
  dl-minimal-malloc \
  dl-misc \
  dl-object \
  dl-open \
  dl-origin \
  dl-printf \
  dl-reloc \
  dl-runtime \
  dl-scope \
  dl-setup_hash \
  dl-sort-maps \
  dl-thread_gscope_wait \
  dl-tls \
  dl-tls_init_tp \
  dl-trampoline \
  dl-tunables \
  dl-version \
  dl-write \
  # dl-routines

ifeq (yes,$(use-ldconfig))
dl-routines += dl-cache
endif

ifeq (yesyes,$(build-shared)$(run-built-tests))
tests-special += $(objpfx)list-tunables.out
endif

# Make sure that the compiler does not insert any library calls in tunables
# code paths.
ifeq (yes,$(have-loop-to-function))
CFLAGS-dl-tunables.c += -fno-tree-loop-distribute-patterns
endif

all-dl-routines = $(dl-routines) $(sysdep-dl-routines)
# But they are absent from the shared libc, because that code is in ld.so.
elide-routines.os = \
  $(all-dl-routines) \
  dl-early_allocate \
  dl-exception \
  dl-origin \
  dl-reloc-static-pie \
  dl-support \
  dl-sysdep \
  enbl-secure \
  rtld_static_init \
  thread_gscope_wait \
  # elide-routines.os

# These object files are only included in the dynamically-linked libc.
shared-only-routines = \
  libc-dl-profile \
  libc-dl-profstub \
  libc-dl_find_object \
  # shared-only-routines

# ld.so uses those routines, plus some special stuff for being the program
# interpreter and operating independent of libc.
rtld-routines = \
  $(all-dl-routines) \
  dl-audit \
  dl-compat \
  dl-diagnostics \
  dl-diagnostics-cpu \
  dl-diagnostics-kernel \
  dl-environ \
  dl-hwcaps \
  dl-hwcaps-subdirs \
  dl-hwcaps_split \
  dl-libc_freeres \
  dl-minimal \
  dl-mutex \
  dl-profile \
  dl-sysdep \
  dl-usage \
  rtld \
  # rtld-routines

all-rtld-routines = $(rtld-routines) $(sysdep-rtld-routines)

CFLAGS-dl-runtime.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-dl-lookup.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-dl-iteratephdr.c += $(uses-callbacks)

# Called during static library initialization, so turn stack-protection
# off for non-shared builds.
CFLAGS-dl-minimal-malloc.o = $(no-stack-protector)
CFLAGS-dl-minimal-malloc.op = $(no-stack-protector)

# On targets without __builtin_memset, rtld.c uses a hand-coded loop
# in _dl_start.  Make sure this isn't turned into a call to regular memset.
ifeq (yes,$(have-loop-to-function))
CFLAGS-rtld.c += -fno-tree-loop-distribute-patterns
endif

# Compile rtld itself without stack protection.
# Also compile all routines in the static library that are elided from
# the shared libc because they are in libc.a in the same way.

CFLAGS-.o += $(call elide-stack-protector,.o,$(elide-routines.os))
CFLAGS-.op += $(call elide-stack-protector,.op,$(elide-routines.os))
CFLAGS-.os += $(call elide-stack-protector,.os,$(all-rtld-routines))

# Add the requested compiler flags to the early startup code.
CFLAGS-dl-misc.os += $(rtld-early-cflags)
CFLAGS-dl-printf.os += $(rtld-early-cflags)
CFLAGS-dl-setup_hash.os += $(rtld-early-cflags)
CFLAGS-dl-sysdep.os += $(rtld-early-cflags)
CFLAGS-dl-tunables.os += $(rtld-early-cflags)
CFLAGS-dl-write.os += $(rtld-early-cflags)
CFLAGS-dl-writev.os += $(rtld-early-cflags)
CFLAGS-rtld.os += $(rtld-early-cflags)

ifeq ($(unwind-find-fde),yes)
routines += unwind-dw2-fde-glibc
shared-only-routines += unwind-dw2-fde-glibc
endif

before-compile  += $(objpfx)trusted-dirs.h
generated	+= trusted-dirs.h trusted-dirs.st for-renamed/renamed.so
generated-dirs	+= for-renamed

ifeq ($(build-shared),yes)
ld-map		= $(common-objpfx)ld.map
endif

ifeq (yes,$(build-shared))
extra-objs = \
  $(all-rtld-routines:%=%.os) \
  interp.os \
  sofini.os \
  # extra-objs
generated += \
  dl-allobjs.os \
  ld.so ldd \
  librtld.os \
  # generated
install-others	= $(inst_rtlddir)/$(rtld-installed-name) $(inst_bindir)/ld.so
install-bin-script = ldd
endif

others		= sprof sln
install-bin	= sprof
others-static   = sln
install-rootsbin = sln
sln-modules	:= static-stubs
extra-objs	+= $(sln-modules:=.o)

ifeq (yes,$(use-ldconfig))
ifeq (yes,$(build-shared))
others-static	+= ldconfig
others		+= ldconfig
install-rootsbin += ldconfig

ldconfig-modules := \
  cache \
  chroot_canon \
  readlib \
  static-stubs \
  stringtable \
  xmalloc \
  xstrdup \
  # ldconfig-modules
extra-objs	+= $(ldconfig-modules:=.o)
others-extras   = $(ldconfig-modules)
endif
endif

# To find xmalloc.c and xstrdup.c
vpath %.c ../locale/programs

ifeq ($(build-shared),yes)
extra-objs += sotruss-lib.os sotruss-lib.so
install-others += $(inst_auditdir)/sotruss-lib.so
install-bin-script += sotruss
generated += sotruss
libof-sotruss-lib = extramodules
LDFLAGS-sotruss-lib.so += $(z-now-$(bind-now))
$(objpfx)sotruss-lib.so: $(objpfx)sotruss-lib.os
	$(build-module-asneeded)
$(objpfx)sotruss-lib.so: $(common-objpfx)libc.so $(objpfx)ld.so \
	$(common-objpfx)libc_nonshared.a

$(objpfx)sotruss: sotruss.sh $(common-objpfx)config.make
	sed -e 's%@VERSION@%$(version)%g' \
	    -e 's%@TEXTDOMAINDIR@%$(localedir)%g' \
	    -e 's%@PREFIX@%$(prefix)%g' \
	    -e 's|@PKGVERSION@|$(PKGVERSION)|g' \
	    -e 's|@REPORT_BUGS_TO@|$(REPORT_BUGS_TO)|g' \
	    < $< > $@.new
	chmod 555 $@.new
	mv -f $@.new $@
$(inst_auditdir)/sotruss-lib.so: $(objpfx)sotruss-lib.so $(+force)
	$(do-install-program)
endif

tests-static-normal := \
  tst-array1-static \
  tst-array5-static \
  tst-dl-iter-static \
  tst-dst-static \
  tst-env-setuid-static \
  tst-getauxval-static \
  tst-linkall-static \
  tst-single_threaded-pthread-static \
  tst-single_threaded-static \
  tst-tls-allocation-failure-static \
  tst-tlsalign-extern-static \
  tst-tlsalign-static \
  # tests-static-normal

tests-static-internal := \
  tst-dl-printf-static \
  tst-dl_find_object-static \
  tst-env-setuid-tunables \
  tst-ptrguard1-static \
  tst-stackguard1-static \
  tst-tls1-static \
  tst-tls1-static-non-pie \
  tst-tunables \
  tst-tunables-enable_secure \
  # tests-static-internal

CRT-tst-tls1-static-non-pie := $(csu-objpfx)crt1.o
tst-tls1-static-non-pie-no-pie = yes

tests-container := \
  tst-ldconfig-bad-aux-cache \
  tst-ldconfig-ld_so_conf-update \
  # tests-container

ifeq (no,$(build-hardcoded-path-in-tests))
# This is an ld.so.cache test, and RPATH/RUNPATH in the executable
# interferes with its test objectives.
tests-container += tst-glibc-hwcaps-prepend-cache
endif

tests := \
  tst-array1 \
  tst-array2 \
  tst-array3 \
  tst-array4 \
  tst-array5 \
  tst-auxv \
  tst-decorate-maps \
  tst-dl-hash \
  tst-env-setuid \
  tst-leaks1 \
  tst-stringtable \
  tst-tls9 \
  tst-tunables-enable_secure-env \
  # tests

tests-internal := \
  $(tests-static-internal) \
  tst-tls1 \
  # tests-internal

tests-static := $(tests-static-normal) $(tests-static-internal)

ifeq (yes,$(build-shared))
tests-static += \
  tst-single_threaded-static-dlopen \
  tst-tls9-static \
  # tests-static

static-dlopen-environment = \
  LD_LIBRARY_PATH=$(ld-library-path):$(common-objpfx)dlfcn
tst-tls9-static-ENV = $(static-dlopen-environment)
tst-single_threaded-static-dlopen-ENV = $(static-dlopen-environment)
tst-rootdir-ENV = LD_LIBRARY_PATH=/

tests += \
  argv0test \
  constload1 \
  dblload \
  dblunload \
  filter \
  global \
  initfirst \
  lateglobal \
  loadfail \
  multiload \
  next \
  nodelete \
  nodelete2 \
  nodlopen \
  nodlopen2 \
  noload \
  order \
  order2 \
  origtest \
  preloadtest \
  reldep \
  reldep2 \
  reldep3 \
  reldep4 \
  reldep5 \
  reldep6 \
  reldep7 \
  reldep8 \
  resolvfail \
  restest1 \
  restest2 \
  tst-absolute-sym \
  tst-absolute-zero \
  tst-addr1 \
  tst-align \
  tst-align2 \
  tst-align3 \
  tst-audit-tlsdesc \
  tst-audit-tlsdesc-dlopen \
  tst-audit1 \
  tst-audit2 \
  tst-audit8 \
  tst-audit9 \
  tst-audit11 \
  tst-audit12 \
  tst-audit13 \
  tst-audit17 \
  tst-audit18 \
  tst-audit19b \
  tst-audit20 \
  tst-audit21 \
  tst-audit22 \
  tst-audit23 \
  tst-audit24a \
  tst-audit24b \
  tst-audit24c \
  tst-audit24d \
  tst-audit25a \
  tst-audit25b \
  tst-audit28 \
  tst-auditmany \
  tst-auxobj \
  tst-auxobj-dlopen \
  tst-big-note \
  tst-debug1 \
  tst-deep1 \
  tst-dl-is_dso \
  tst-dlclose-lazy \
  tst-dlmodcount \
  tst-dlmopen-dlerror \
  tst-dlmopen-gethostbyname \
  tst-dlmopen-twice \
  tst-dlmopen1 \
  tst-dlmopen3 \
  tst-dlmopen4 \
  tst-dlopen-self \
  tst-dlopen-tlsmodid \
  tst-dlopen-tlsreinit1 \
  tst-dlopen-tlsreinit2 \
  tst-dlopen-tlsreinit3 \
  tst-dlopen-tlsreinit4 \
  tst-dlopenfail \
  tst-dlopenfail-2 \
  tst-dlopenrpath \
  tst-dlsym-error \
  tst-filterobj \
  tst-filterobj-dlopen \
  tst-glibc-hwcaps \
  tst-glibc-hwcaps-mask \
  tst-glibc-hwcaps-prepend \
  tst-global1 \
  tst-global2 \
  tst-gnu2-tls2 \
  tst-initfinilazyfail \
  tst-initorder \
  tst-initorder2 \
  tst-latepthread \
  tst-main1 \
  tst-next-ver \
  tst-nodelete-dlclose \
  tst-nodelete-opened \
  tst-nodelete2 \
  tst-nodeps1 \
  tst-nodeps2 \
  tst-noload \
  tst-non-directory-path \
  tst-null-argv \
  tst-p_align1 \
  tst-p_align2 \
  tst-p_align3 \
  tst-recursive-tls \
  tst-relsort1 \
  tst-ro-dynamic \
  tst-rtld-run-static \
  tst-single_threaded \
  tst-single_threaded-pthread \
  tst-sonamemove-dlopen \
  tst-sonamemove-link \
  tst-thrlock \
  tst-tls-dlinfo \
  tst-tls-ie \
  tst-tls-ie-dlmopen \
  tst-tls-manydynamic \
  tst-tls4 \
  tst-tls5 \
  tst-tls10 \
  tst-tls11 \
  tst-tls12 \
  tst-tls13 \
  tst-tls14 \
  tst-tls15 \
  tst-tls16 \
  tst-tls17 \
  tst-tls18 \
  tst-tls19 \
  tst-tls20 \
  tst-tls21 \
  tst-tlsalign \
  tst-tlsalign-extern \
  tst-tlsgap \
  tst-unique1 \
  tst-unique2 \
  tst-unwind-ctor \
  tst-unwind-main \
  unload3 \
  unload4 \
  unload5 \
  unload6 \
  unload7 \
  unload8 \
  valgrind-test \
  # tests
tests-cxx = \
  tst-dlopen-nodelete-reloc \
  tst-nodelete \
  tst-unique3 \
  tst-unique4 \
  # tests-cxx

tests += $(if $(CXX),$(tests-cxx))

tests-internal += \
  circleload1 \
  loadtest \
  neededtest \
  neededtest2 \
  neededtest3 \
  neededtest4 \
  tst-audit19a \
  tst-create_format1 \
  tst-dl-hwcaps_split \
  tst-dl_find_object \
  tst-dl_find_object-threads \
  tst-dlmopen2 \
  tst-ptrguard1 \
  tst-stackguard1 \
  tst-tls-surplus \
  tst-tls3 \
  tst-tls6 \
  tst-tls7 \
  tst-tls8 \
  unload \
  unload2 \
  # tests-internal

tests-container += \
  tst-dlopen-self-container \
  tst-dlopen-tlsmodid-container \
  tst-pldd \
  tst-preload-pthread-libc \
  tst-rootdir \
  # tests-container

test-srcs = \
  tst-pathopt \
  tst-sprof-basic \
  # tests-srcs

ifeq (yes,$(have-fpie))
tests-pie += tst-align3
endif
selinux-enabled := $(shell cat /selinux/enforce 2> /dev/null)

ifneq ($(selinux-enabled),1)
tests-execstack-yes = \
  tst-execstack \
  tst-execstack-needed \
  tst-execstack-prog \
  # tests-execstack-yes
endif
ifeq ($(have-depaudit),yes)
tests += \
  tst-audit14 \
  tst-audit14a \
  tst-audit15 \
  tst-audit16 \
  # tests
ifeq ($(run-built-tests),yes)
tests-special += \
  $(objpfx)tst-audit14-cmp.out \
  $(objpfx)tst-audit14a-cmp.out \
  $(objpfx)tst-audit15-cmp.out \
  $(objpfx)tst-audit16-cmp.out \
  # tests-special
endif
endif
ifeq ($(have-dt-relr),yes)
tests += \
  tst-relr \
  tst-relr2 \
  tst-relr3 \
  tst-relr4 \
  # tests
modules-names-dt-relr = \
  tst-relr-mod2 \
  tst-relr-mod3a \
  tst-relr-mod3b \
  tst-relr-mod4a \
  tst-relr-mod4b \
  # modules-names-dt-relr
modules-names += $(modules-names-dt-relr)
# These shared libraries have special build rules.
modules-names-nobuild += $(modules-names-dt-relr)
ifeq ($(have-fpie),yes)
tests += \
  tst-relr-pie \
  # tests
tests-pie += \
  tst-relr-pie \
  # tests-pie
tests-special += \
  $(objpfx)check-tst-relr-pie.out \
  # tests-special
endif
CFLAGS-tst-relr-pie.c += $(pie-ccflag)
LDFLAGS-tst-relr += -Wl,-z,pack-relative-relocs
LDFLAGS-tst-relr-pie += -Wl,-z,pack-relative-relocs
CFLAGS-tst-relr-mod2.c += $(no-stack-protector)
CFLAGS-tst-relr-mod3a.c += $(no-stack-protector)
CFLAGS-tst-relr-mod3b.c += $(no-stack-protector)
CFLAGS-tst-relr-mod4a.c += $(no-stack-protector)
CFLAGS-tst-relr-mod4b.c += $(no-stack-protector)
endif
endif

tests-special += $(objpfx)tst-relro-ldso.out $(objpfx)tst-relro-libc.out
$(objpfx)tst-relro-ldso.out: tst-relro-symbols.py $(..)/scripts/glibcelf.py \
  $(objpfx)ld.so
	$(PYTHON) tst-relro-symbols.py $(objpfx)ld.so \
	  --required=_rtld_global_ro \
	  > $@ 2>&1; $(evaluate-test)
# The optional symbols are present in libc only if the architecture has
# the GLIBC_2.0 symbol set in libc.
$(objpfx)tst-relro-libc.out: tst-relro-symbols.py $(..)/scripts/glibcelf.py \
  $(common-objpfx)libc.so
	$(PYTHON) tst-relro-symbols.py $(common-objpfx)libc.so \
	    --required=_IO_file_jumps \
	    --required=_IO_wfile_jumps \
	    --required=__io_vtables \
	  > $@ 2>&1; $(evaluate-test)

ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-valgrind-smoke.out
endif
$(objpfx)tst-valgrind-smoke.out: tst-valgrind-smoke.sh $(objpfx)ld.so $(objpfx)valgrind-test
	$(SHELL) $< $(objpfx)ld.so  $(rtlddir)/$(rtld-installed-name) '$(test-wrapper-env)' \
		'$(run-program-env)' '$(rpath-link)' $(objpfx)valgrind-test > $@; $(evaluate-test)

ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-rtld-does-not-exist.out
endif
$(objpfx)tst-rtld-does-not-exist.out: tst-rtld-does-not-exist.sh $(objpfx)ld.so
	$(SHELL) $< $(objpfx)ld.so > $@; \
	$(evaluate-test)

tests += $(tests-execstack-$(have-z-execstack))
ifeq ($(run-built-tests),yes)
tests-special += \
  $(objpfx)tst-ldconfig-X.out \
  $(objpfx)tst-ldconfig-p.out \
  $(objpfx)tst-ldconfig-soname.out \
  $(objpfx)tst-rtld-help.out \
  # tests-special
ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
tests-special += \
  $(objpfx)noload-mem.out \
  $(objpfx)tst-leaks1-mem.out \
  # tests-special
endif
endif
endif
tlsmod17a-suffixes = 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
tlsmod18a-suffixes = 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
tlsmod17a-modules = $(addprefix tst-tlsmod17a, $(tlsmod17a-suffixes))
tlsmod18a-modules = $(addprefix tst-tlsmod18a, $(tlsmod17a-suffixes))
one-hundred = $(foreach x,0 1 2 3 4 5 6 7 8 9, \
  0$x 1$x 2$x 3$x 4$x 5$x 6$x 7$x 8$x 9$x)
tst-tls-many-dynamic-modules := \
  $(foreach n,$(one-hundred),tst-tls-manydynamic$(n)mod)
tst-tls-many-dynamic-modules-dep-suffixes = 0 1 2 3 4 5 6 7 8 9 10 11 12 13 \
					    14 15 16 17 18 19
tst-tls-many-dynamic-modules-dep = \
  $(foreach n,$(tst-tls-many-dynamic-modules-dep-suffixes),tst-tls-manydynamic$(n)mod-dep)
tst-tls-many-dynamic-modules-dep-bad-suffixes = 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14
tst-tls-many-dynamic-modules-dep-bad = \
  $(foreach n,$(tst-tls-many-dynamic-modules-dep-bad-suffixes),tst-tls-manydynamic$(n)mod-dep-bad)
extra-test-objs += \
  $(tlsmod17a-modules:=.os) \
  $(tlsmod18a-modules:=.os) \
  tst-tlsalign-vars.o \
  # extra-test-objs
test-extras += \
  tst-tlsalign-vars \
  tst-tlsmod17a \
  tst-tlsmod18a \
  # test-extras
modules-names += \
  circlemod1 \
  circlemod1a \
  circlemod2 \
  circlemod2a \
  circlemod3 \
  circlemod3a \
  constload2 \
  constload3 \
  dblloadmod1 \
  dblloadmod2 \
  dblloadmod3 \
  dep1 \
  dep2 \
  dep3 \
  dep4 \
  failobj \
  filtmod1 \
  filtmod2 \
  firstobj \
  globalmod1 \
  libmarkermod1-1 \
  libmarkermod1-2 \
  libmarkermod1-3 \
  libmarkermod2-1 \
  libmarkermod2-2 \
  libmarkermod3-1 \
  libmarkermod3-2 \
  libmarkermod3-3 \
  libmarkermod4-1 \
  libmarkermod4-2 \
  libmarkermod4-3 \
  libmarkermod4-4 \
  libmarkermod5-1 \
  libmarkermod5-2 \
  libmarkermod5-3 \
  libmarkermod5-4 \
  libmarkermod5-5 \
  libtracemod1-1 \
  libtracemod2-1 \
  libtracemod3-1 \
  libtracemod4-1 \
  libtracemod5-1 \
  ltglobmod1 \
  ltglobmod2 \
  neededobj1 \
  neededobj2 \
  neededobj3 \
  neededobj4 \
  neededobj5 \
  neededobj6 \
  nextmod1 \
  nextmod2 \
  nextmod3 \
  nodel2mod1 \
  nodel2mod2 \
  nodel2mod3 \
  nodelmod1 \
  nodelmod2 \
  nodelmod3 \
  nodelmod4 \
  nodlopenmod \
  nodlopenmod2 \
  order2mod1 \
  order2mod2 \
  order2mod3 \
  order2mod4 \
  pathoptobj \
  reldep4mod1 \
  reldep4mod2 \
  reldep4mod3 \
  reldep4mod4 \
  reldep6mod0 \
  reldep6mod1 \
  reldep6mod2 \
  reldep6mod3 \
  reldep6mod4 \
  reldep7mod1 \
  reldep7mod2 \
  reldep8mod1 \
  reldep8mod2 \
  reldep8mod3 \
  reldep9mod1 \
  reldep9mod2 \
  reldep9mod3 \
  reldepmod1 \
  reldepmod2 \
  reldepmod3 \
  reldepmod4 \
  reldepmod5 \
  reldepmod6 \
  testobj1 \
  testobj1_1 \
  testobj2 \
  testobj3 \
  testobj4 \
  testobj5 \
  testobj6 \
  tst-absolute-sym-lib \
  tst-absolute-zero-lib \
  tst-alignmod \
  tst-alignmod2 \
  tst-alignmod3 \
  tst-array2dep \
  tst-array5dep \
  tst-audit-tlsdesc-mod1 \
  tst-audit-tlsdesc-mod2 \
  tst-audit11mod1 \
  tst-audit11mod2 \
  tst-audit12mod1 \
  tst-audit12mod2 \
  tst-audit12mod3 \
  tst-audit13mod1 \
  tst-audit18mod \
  tst-audit19bmod \
  tst-audit23mod \
  tst-audit24amod1 \
  tst-audit24amod2 \
  tst-audit24bmod1 \
  tst-audit24bmod2 \
  tst-audit24dmod1 \
  tst-audit24dmod2 \
  tst-audit24dmod3 \
  tst-audit24dmod4 \
  tst-audit25mod1 \
  tst-audit25mod2 \
  tst-audit25mod3 \
  tst-audit25mod4 \
  tst-auditlogmod-1 \
  tst-auditlogmod-2 \
  tst-auditlogmod-3 \
  tst-auditmanymod1 \
  tst-auditmanymod2 \
  tst-auditmanymod3 \
  tst-auditmanymod4 \
  tst-auditmanymod5 \
  tst-auditmanymod6 \
  tst-auditmanymod7 \
  tst-auditmanymod8 \
  tst-auditmanymod9 \
  tst-auditmod-tlsdesc  \
  tst-auditmod1 \
  tst-auditmod11 \
  tst-auditmod12 \
  tst-auditmod18 \
  tst-auditmod19a \
  tst-auditmod19b \
  tst-auditmod20 \
  tst-auditmod21a \
  tst-auditmod21b \
  tst-auditmod22 \
  tst-auditmod23 \
  tst-auditmod24a \
  tst-auditmod24b \
  tst-auditmod24c \
  tst-auditmod24d \
  tst-auditmod25 \
  tst-auditmod28 \
  tst-auditmod9a \
  tst-auditmod9b \
  tst-auxvalmod \
  tst-big-note-lib \
  tst-deep1mod1 \
  tst-deep1mod2 \
  tst-deep1mod3 \
  tst-dl_find_object-mod1 \
  tst-dl_find_object-mod2 \
  tst-dl_find_object-mod3 \
  tst-dl_find_object-mod4 \
  tst-dl_find_object-mod5 \
  tst-dl_find_object-mod6 \
  tst-dl_find_object-mod7 \
  tst-dl_find_object-mod8 \
  tst-dl_find_object-mod9 \
  tst-dlclose-lazy-mod1 \
  tst-dlclose-lazy-mod2 \
  tst-dlmopen-dlerror-mod \
  tst-dlmopen-gethostbyname-mod \
  tst-dlmopen-twice-mod1 \
  tst-dlmopen-twice-mod2 \
  tst-dlmopen1mod \
  tst-dlopen-tlsreinitmod1 \
  tst-dlopen-tlsreinitmod2 \
  tst-dlopen-tlsreinitmod3 \
  tst-dlopenfaillinkmod \
  tst-dlopenfailmod1 \
  tst-dlopenfailmod2 \
  tst-dlopenfailmod3 \
  tst-dlopenfailnodelmod \
  tst-dlopenrpathmod \
  tst-filterobj-aux \
  tst-filterobj-filtee \
  tst-filterobj-flt \
  tst-finilazyfailmod \
  tst-globalmod2 \
  tst-gnu2-tls2mod0 \
  tst-gnu2-tls2mod1 \
  tst-gnu2-tls2mod2 \
  tst-initlazyfailmod \
  tst-initorder2a \
  tst-initorder2b \
  tst-initorder2c \
  tst-initorder2d \
  tst-initordera1 \
  tst-initordera2 \
  tst-initordera3 \
  tst-initordera4 \
  tst-initorderb1 \
  tst-initorderb2 \
  tst-latepthreadmod \
  tst-ldconfig-ld-mod \
  tst-ldconfig-soname-lib-with-soname \
  tst-ldconfig-soname-lib-without-soname \
  tst-main1mod \
  tst-nodelete-dlclose-dso \
  tst-nodelete-dlclose-plugin \
  tst-nodelete-opened-lib \
  tst-nodelete2mod \
  tst-nodeps1-mod \
  tst-nodeps2-mod \
  tst-non-directory-mod \
  tst-null-argv-lib \
  tst-p_alignmod-base \
  tst-p_alignmod3 \
  tst-recursive-tlsmallocmod \
  tst-recursive-tlsmod0 \
  tst-recursive-tlsmod1 \
  tst-recursive-tlsmod2 \
  tst-recursive-tlsmod3 \
  tst-recursive-tlsmod4 \
  tst-recursive-tlsmod5 \
  tst-recursive-tlsmod6 \
  tst-recursive-tlsmod7 \
  tst-recursive-tlsmod8 \
  tst-recursive-tlsmod9 \
  tst-recursive-tlsmod10 \
  tst-recursive-tlsmod11 \
  tst-recursive-tlsmod12 \
  tst-recursive-tlsmod13 \
  tst-recursive-tlsmod14 \
  tst-recursive-tlsmod15 \
  tst-relsort1mod1 \
  tst-relsort1mod2 \
  tst-ro-dynamic-mod \
  tst-rootdir-lib \
  tst-single_threaded-mod1 \
  tst-single_threaded-mod2 \
  tst-single_threaded-mod3 \
  tst-single_threaded-mod4 \
  tst-sonamemove-linkmod1 \
  tst-sonamemove-runmod1 \
  tst-sonamemove-runmod2 \
  tst-sprof-mod \
  tst-tls-ie-mod0 \
  tst-tls-ie-mod1 \
  tst-tls-ie-mod2 \
  tst-tls-ie-mod3 \
  tst-tls-ie-mod4 \
  tst-tls-ie-mod5 \
  tst-tls-ie-mod6 \
  tst-tls19mod1 \
  tst-tls19mod2 \
  tst-tls19mod3 \
  tst-tls20mod-bad \
  tst-tls21mod \
  tst-tlsalign-lib \
  tst-tlsgap-mod0 \
  tst-tlsgap-mod1 \
  tst-tlsgap-mod2 \
  tst-tlsmod1 \
  tst-tlsmod10 \
  tst-tlsmod11 \
  tst-tlsmod12 \
  tst-tlsmod13 \
  tst-tlsmod13a \
  tst-tlsmod14a \
  tst-tlsmod14b \
  tst-tlsmod15a \
  tst-tlsmod15b \
  tst-tlsmod16a \
  tst-tlsmod16b \
  tst-tlsmod17b \
  tst-tlsmod2 \
  tst-tlsmod3 \
  tst-tlsmod4 \
  tst-tlsmod5 \
  tst-tlsmod6 \
  tst-tlsmod7 \
  tst-tlsmod8 \
  tst-tlsmod9 \
  tst-unique1mod1 \
  tst-unique1mod2 \
  tst-unique2mod1 \
  tst-unique2mod2 \
  tst-unwind-ctor-lib \
  unload2dep \
  unload2mod \
  unload3mod1 \
  unload3mod2 \
  unload3mod3 \
  unload3mod4 \
  unload4mod1 \
  unload4mod2 \
  unload4mod3 \
  unload4mod4 \
  unload6mod1 \
  unload6mod2 \
  unload6mod3 \
  unload7mod1 \
  unload7mod2 \
  unload8mod1 \
  unload8mod1x \
  unload8mod2 \
  unload8mod3 \
  unloadmod \
  vismod1 \
  vismod2 \
  vismod3 \
  # modules-names

modules-names-cxx = \
  tst-dlopen-nodelete-reloc-mod1 \
  tst-dlopen-nodelete-reloc-mod2 \
  tst-dlopen-nodelete-reloc-mod3 \
  tst-dlopen-nodelete-reloc-mod4 \
  tst-dlopen-nodelete-reloc-mod5 \
  tst-dlopen-nodelete-reloc-mod6 \
  tst-dlopen-nodelete-reloc-mod7 \
  tst-dlopen-nodelete-reloc-mod8 \
  tst-dlopen-nodelete-reloc-mod9 \
  tst-dlopen-nodelete-reloc-mod10 \
  tst-dlopen-nodelete-reloc-mod11 \
  tst-dlopen-nodelete-reloc-mod12 \
  tst-dlopen-nodelete-reloc-mod13 \
  tst-dlopen-nodelete-reloc-mod14 \
  tst-dlopen-nodelete-reloc-mod15 \
  tst-dlopen-nodelete-reloc-mod16 \
  tst-dlopen-nodelete-reloc-mod17 \
  tst-nodelete-rtldmod \
  tst-nodelete-uniquemod \
  tst-nodelete-zmod \
  tst-unique3lib \
  tst-unique3lib2 \
  tst-unique4lib \
  # modules-names-cxx

modules-names += \
  $(if $(CXX),$(modules-names-cxx)) \
  $(modules-execstack-$(have-z-execstack)) \
  $(tlsmod17a-modules) \
  $(tlsmod18a-modules) \
  $(tst-tls-many-dynamic-modules) \
  $(tst-tls-many-dynamic-modules-dep) \
  $(tst-tls-many-dynamic-modules-dep-bad) \
  # modules-names

# Most modules build with _ISOMAC defined, but those filtered out
# depend on internal headers.
modules-names-tests = $(filter-out ifuncmod% tst-tlsmod%,\
				   $(modules-names))

# For +depfiles in Makerules.
extra-test-objs += tst-auditmod17.os

ifneq (no,$(have-mtls-descriptor))
tests += tst-gnu2-tls1
modules-names += tst-gnu2-tls1mod
$(objpfx)tst-gnu2-tls1: $(objpfx)tst-gnu2-tls1mod.so
tst-gnu2-tls1mod.so-no-z-defs = yes
CFLAGS-tst-gnu2-tls1mod.c += -mtls-dialect=$(have-mtls-descriptor)
endif # $(have-mtls-descriptor)

ifeq (yes,$(have-protected-data))
modules-names += tst-protected1moda tst-protected1modb
tests += tst-protected1a tst-protected1b
$(objpfx)tst-protected1a: $(addprefix $(objpfx),tst-protected1moda.so tst-protected1modb.so)
$(objpfx)tst-protected1b: $(addprefix $(objpfx),tst-protected1modb.so tst-protected1moda.so)
tst-protected1modb.so-no-z-defs = yes
# These tests fail with GCC versions prior to 5.1 and with some versions
# of binutils.  See https://sourceware.org/bugzilla/show_bug.cgi?id=17709
# and https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65248 for details.
# Perhaps in future we can make these XFAILs conditional on some detection
# of compiler/linker behavior/version.
test-xfail-tst-protected1a = yes
test-xfail-tst-protected1b = yes
endif
ifeq (yesyes,$(have-fpie)$(build-shared))
modules-names += tst-piemod1
tests += \
  tst-dlopen-pie \
  tst-dlopen-self-pie \
  tst-dlopen-tlsmodid-pie \
  tst-pie1 \
  tst-pie2 \
  # tests
tests-pie += \
  tst-dlopen-self-pie \
  tst-dlopen-tlsmodid-pie \
  tst-pie1 \
  tst-pie2 \
  # tests-pie
ifeq (yes,$(have-protected-data))
tests += vismain
tests-pie += vismain
CFLAGS-vismain.c += $(PIE-ccflag)
endif
endif
modules-execstack-yes = tst-execstack-mod
extra-test-objs += $(addsuffix .os,$(strip $(modules-names)))

# filtmod1.so, tst-big-note-lib.so, tst-ro-dynamic-mod.so have special
# rules.
modules-names-nobuild += \
  filtmod1 \
  tst-audit24bmod1 \
  tst-audit24bmod2 \
  tst-big-note-lib \
  tst-nodeps1-mod \
  tst-nodeps2-mod \
  tst-ro-dynamic-mod \
  # modules-names-nobuild

tests += $(tests-static)

ifeq (yes,$(have-ifunc))
tests-ifuncstatic := \
  ifuncmain1picstatic \
  ifuncmain1static \
  ifuncmain2picstatic \
  ifuncmain2static \
  ifuncmain4picstatic \
  ifuncmain4static \
  ifuncmain5picstatic \
  ifuncmain5static \
  ifuncmain7picstatic \
  ifuncmain7static \
  # tests-ifuncstatic
ifeq (yes,$(have-gcc-ifunc))
tests-ifuncstatic += ifuncmain9static ifuncmain9picstatic
endif
tests-static += $(tests-ifuncstatic)
tests-internal += $(tests-ifuncstatic)
ifeq (yes,$(build-shared))
tests += \
  tst-ifunc-fault-bindnow \
  tst-ifunc-fault-lazy \
  # tests
# Note: sysdeps/x86_64/ifuncmain8.c uses ifuncmain8.
tests-internal += \
  ifuncmain1pic \
  ifuncmain1staticpic \
  ifuncmain1vis \
  ifuncmain1vispic \
  ifuncmain2 \
  ifuncmain2pic \
  ifuncmain3 \
  ifuncmain4 \
  ifuncmain5staticpic \
  ifuncmain7 \
  ifuncmain7pic \
  # tests-internal
ifeq (no,$(with-lld))
tests-internal += \
  ifuncmain1 \
  ifuncmain5 \
  ifuncmain5pic \
  # tests-internal
endif
ifeq (yes,$(have-gcc-ifunc))
tests-internal += \
  ifuncmain9 \
  ifuncmain9pic \
  # tests-internal
endif
ifunc-test-modules = \
  ifuncdep1 \
  ifuncdep1pic \
  ifuncdep2 \
  ifuncdep2pic \
  ifuncdep5 \
  ifuncdep5pic \
  # ifunc-test-modules
extra-test-objs += $(ifunc-test-modules:=.o)
test-internal-extras += $(ifunc-test-modules)
ifeq (yes,$(have-fpie))
ifunc-pie-tests = \
  ifuncmain1pie \
  ifuncmain1staticpie \
  ifuncmain1vispie \
  ifuncmain6pie \
  ifuncmain7pie \
  # ifunc-pie-tests
ifeq (yes,$(have-gcc-ifunc))
ifunc-pie-tests += ifuncmain9pie
endif
ifeq (no,$(with-lld))
ifunc-pie-tests += ifuncmain5pie
endif
ifeq (yes,$(have-textrel_ifunc))
ifunc-pie-tests += tst-ifunc-textrel
endif
tests-internal += $(ifunc-pie-tests)
tests-pie += $(ifunc-pie-tests)
endif
modules-names += \
  ifuncmod1 \
  ifuncmod3 \
  ifuncmod6 \
  # modules-names
ifeq (no,$(with-lld))
modules-names += ifuncmod5
endif
endif
endif

ifeq (yes,$(build-shared))
ifeq ($(run-built-tests),yes)
tests-special += \
  $(objpfx)argv0test.out \
  $(objpfx)tst-non-directory-path.out \
  $(objpfx)tst-pathopt.out \
  $(objpfx)tst-rtld-help.out \
  $(objpfx)tst-rtld-list-diagnostics.out \
  $(objpfx)tst-rtld-load-self.out \
  $(objpfx)tst-rtld-preload.out \
  $(objpfx)tst-sprof-basic.out \
  # tests-special
endif
tests-special += \
  $(objpfx)check-execstack.out \
  $(objpfx)check-initfini.out \
  $(objpfx)check-localplt.out \
  $(objpfx)check-textrel.out \
  $(objpfx)check-wx-segment.out \
  # tests-special
endif

ifeq ($(run-built-tests),yes)
tests-special += \
  $(objpfx)order-cmp.out \
  $(objpfx)order2-cmp.out \
  $(objpfx)tst-array1-cmp.out \
  $(objpfx)tst-array1-static-cmp.out \
  $(objpfx)tst-array2-cmp.out \
  $(objpfx)tst-array3-cmp.out \
  $(objpfx)tst-array4-cmp.out \
  $(objpfx)tst-array5-cmp.out \
  $(objpfx)tst-array5-static-cmp.out \
  $(objpfx)tst-initorder-cmp.out \
  $(objpfx)tst-initorder2-cmp.out \
  $(objpfx)tst-trace1.out \
  $(objpfx)tst-trace2.out \
  $(objpfx)tst-trace3.out \
  $(objpfx)tst-trace4.out \
  $(objpfx)tst-trace5.out \
  $(objpfx)tst-unused-dep-cmp.out \
  $(objpfx)tst-unused-dep.out \
  # tests-special
endif

ifndef avoid-generated
# DSO sorting tests:
# The dso-ordering-test.py script generates testcase source files in $(objpfx),
# creating a $(objpfx)<testcase-name>-dir for each testcase, and creates a
# Makefile fragment to be included.
define include_dsosort_tests
$(objpfx)$(1).generated-makefile: $(1)
	$(PYTHON) $(..)scripts/dso-ordering-test.py \
	--description-file $$< --objpfx $(objpfx) --output-makefile $$@T
	mv $$@T $$@
-include $(objpfx)$(1).generated-makefile
endef
endif

postclean-generated += $(objpfx)/dso-sort-tests-2.generated-makefile \
		       $(objpfx)/dso-sort-tests-2.generated-makefile

# Generate from each testcase description file
$(eval $(call include_dsosort_tests,dso-sort-tests-1.def))
$(eval $(call include_dsosort_tests,dso-sort-tests-2.def))

# BZ15311 is intentionally underlinked.
LDFLAGS-tst-bz15311-b.so += -Wl,-z,lazy
LDFLAGS-tst-bz15311-c.so += -Wl,-z,lazy
LDFLAGS-tst-bz15311-d.so += -Wl,-z,lazy

check-abi: $(objpfx)check-abi-ld.out \
	   $(objpfx)check-abi-version-libc.out
tests-special += \
  $(objpfx)check-abi-ld.out \
  $(objpfx)check-abi-version-libc.out \
# tests-special
update-abi: update-abi-ld
update-all-abi: update-all-abi-ld

tests-special += $(objpfx)tst-glibcelf.out
$(objpfx)tst-glibcelf.out: tst-glibcelf.py elf.h $(..)/scripts/glibcelf.py \
  $(..)/scripts/glibcextract.py
	PYTHONPATH=$(..)scripts $(PYTHON) tst-glibcelf.py \
          --cc="$(CC) $(patsubst -DMODULE_NAME=%,-DMODULE_NAME=testsuite,$(CPPFLAGS))" \
	  < /dev/null > $@ 2>&1; $(evaluate-test)

ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-tls-allocation-failure-static-patched.out
endif

# The test requires shared _and_ PIE because the executable
# unit test driver must be able to link with the shared object
# that is going to eventually go into an installed DSO.
ifeq (yesyes,$(have-fpie)$(build-shared))
tests-internal += tst-_dl_addr_inside_object
tests-pie += tst-_dl_addr_inside_object
$(objpfx)tst-_dl_addr_inside_object: $(objpfx)dl-addr-obj.os
CFLAGS-tst-_dl_addr_inside_object.c += $(PIE-ccflag)
endif

include ../Rules

ifeq (yes,$(build-shared))
# Make sure these things are built in the `make lib' pass so they can be used
# to run programs during the `make others' pass.
lib-noranlib: $(objpfx)$(rtld-installed-name) \
	      $(addprefix $(objpfx),$(extra-objs))
endif

# Command to link into a larger single relocatable object.
reloc-link = $(LINK.o) -nostdlib -nostartfiles -r

$(objpfx)dl-allobjs.os: $(all-rtld-routines:%=$(objpfx)%.os)
	$(reloc-link) -o $@ $^

# Link together the dynamic linker into a single relocatable object.
# First we do a link against libc_pic.a just to get a link map,
# and discard the object produced by that link.  From the link map
# we can glean all the libc modules that need to go into the dynamic
# linker.  Then we do a recursive make that goes into all the subdirs
# those modules come from and builds special rtld-foo.os versions that
# are compiled with special flags, and puts these modules into rtld-libc.a
# for us.  Then we do the real link using rtld-libc.a instead of libc_pic.a.

# These symbols need to be stubbed out during symbol discovery because
# their implementation is provided differently in rtld, and the symbol
# discovery mechanism is not compatible with the libc implementation
# when compiled for libc.
rtld-stubbed-symbols = \
  __GI___pthread_disable_asynccancel \
  __GI___pthread_enable_asynccancel \
  __libc_assert_fail \
  __pthread_disable_asynccancel \
  __pthread_enable_asynccancel \
  calloc \
  free \
  malloc \
  realloc \
  # rtld-stubbed-symbols

ifeq ($(have-ssp),yes)
# rtld is not built with the stack protector, so these references will
# go away in the rebuilds.
rtld-stubbed-symbols += __stack_chk_fail __stack_chk_fail_local
endif

$(objpfx)librtld.map: $(objpfx)dl-allobjs.os $(common-objpfx)libc_pic.a
	@-rm -f $@T
	for symbol in $(rtld-stubbed-symbols); do \
		echo ".globl $$symbol"; \
		echo "$$symbol:"; \
	done | $(CC) -o $@T.o $(ASFLAGS) -c -x assembler -
	$(reloc-link) -o $@.o $@T.o '-Wl,-(' $^ -lgcc '-Wl,-)' -Wl,-Map,$@T
	rm -f %@T.o $@.o
	mv -f $@T $@

# For lld, skip preceding addresses and values before matching the archive and the member.
$(objpfx)librtld.mk: $(objpfx)librtld.map Makefile
	LC_ALL=C \
	sed -n 's@^[0-9a-f ]*$(common-objpfx)\([^(]*\)(\([^)]*\.os\)) *.*$$@\1 \2@p' \
	    $< | \
	while read lib file; do \
	  case $$lib in \
	  libc_pic.a) \
	    LC_ALL=C grep -F -l /$$file \
		  $(common-objpfx)stamp.os $(common-objpfx)*/stamp.os | \
	    LC_ALL=C \
	    sed 's@^$(common-objpfx)\([^/]*\)/stamp\.os$$@rtld-\1'" +=$$file@"\
	    ;; \
	  */*.a) \
	    echo rtld-$${lib%%/*} += $$file ;; \
	  *) echo "Wasn't expecting $$lib($$file)" >&2; exit 1 ;; \
	  esac; \
	done > $@T
	echo rtld-subdirs = `LC_ALL=C sed 's/^rtld-\([^ ]*\).*$$/\1/' $@T \
			     | LC_ALL=C sort -u` >> $@T
	mv -f $@T $@

$(objpfx)rtld-libc.a: $(objpfx)librtld.mk FORCE
	$(MAKE) -f $< -f rtld-Rules

$(objpfx)librtld.os: $(objpfx)dl-allobjs.os $(objpfx)rtld-libc.a
	$(LINK.o) -nostdlib -nostartfiles -r -o $@ '-Wl,-(' $^ -lgcc '-Wl,-)' \
		  -Wl,-Map,$@.map

generated += librtld.map librtld.mk rtld-libc.a librtld.os.map

z-now-yes = -Wl,-z,now

$(objpfx)ld.so: $(objpfx)librtld.os $(ld-map)
# Link into a temporary file so that we don't touch $@ at all
# if the sanity check below fails.
	$(LINK.o) -nostdlib -nostartfiles -shared -o $@.new		\
		  $(LDFLAGS-rtld) -Wl,-z,defs $(z-now-$(bind-now))	\
		  $(dt-relr-ldflag) \
		  $(filter-out $(map-file),$^) $(load-map-file)		\
		  -Wl,-soname=$(rtld-installed-name)
	$(call after-link,$@.new)
	$(READELF) -s $@.new \
	  | $(AWK) '($$7 ~ /^UND(|EF)$$/ && $$1 != "0:" && $$4 != "REGISTER") { print; p=1 } END { exit p != 0 }'
	mv -f $@.new $@

ifeq (yes,$(build-shared))
# interp.c exists just to get the runtime linker path into libc.so.
$(objpfx)interp.os: $(common-objpfx)runtime-linker.h
endif

ifneq (ld.so,$(rtld-installed-name))
# Make sure ld.so.1 exists in the build directory so we can link
# against it.
$(objpfx)$(rtld-installed-name): $(objpfx)ld.so
	$(make-link)
generated += $(rtld-installed-name)
endif

# Build a file mentioning all trustworthy directories to look for shared
# libraries when using LD_LIBRARY_PATH in a setuid program.  The user can
# add directories to the list by defining $(user-defined-trusted-dirs)
# before starting make.
$(objpfx)trusted-dirs.h: $(objpfx)trusted-dirs.st; @:
$(objpfx)trusted-dirs.st: Makefile $(..)Makeconfig
	$(make-target-directory)
	echo "$(subst :, ,$(default-rpath) $(user-defined-trusted-dirs))"    \
	| $(AWK) -f gen-trusted-dirs.awk > ${@:st=T};
	echo '#define DL_DST_LIB "$(notdir $(slibdir))"' >> ${@:st=T}
	$(move-if-change) ${@:st=T} ${@:st=h}
	touch $@
CPPFLAGS-dl-load.c += -I$(objpfx). -I$(csu-objpfx).

ifeq (yes,$(build-shared))
$(inst_rtlddir)/$(rtld-installed-name): $(objpfx)ld.so $(+force)
	$(make-target-directory)
	$(do-install-program)

# Creates the relative /usr/bin/ld.so symbolic link.
$(inst_bindir)/ld.so: $(inst_rtlddir)/$(rtld-installed-name)
	$(make-target-directory)
	$(make-link)

# Special target called by parent to install just the dynamic linker.
.PHONY: ldso_install
ldso_install: $(inst_rtlddir)/$(rtld-installed-name)
endif # $(build-shared)


# Workarounds for ${exec_prefix} expansion in configure variables.
# config.status cannot be used directly for processing ldd.bash.in or
# expanding variables such as sysconfdir because the expansion
# contains the literal string ${exec_prefix}, which is not valid in C
# headers or installed shell scripts.

ldd-rewrite = -e 's%@RTLD@%$(rtlddir)/$(rtld-installed-name)%g' \
	      -e 's%@VERSION@%$(version)%g' \
	      -e 's|@PKGVERSION@|$(PKGVERSION)|g' \
	      -e 's|@REPORT_BUGS_TO@|$(REPORT_BUGS_TO)|g' \
	      -e 's%@TEXTDOMAINDIR@%$(localedir)%g'

ifeq ($(ldd-rewrite-script),no)
define gen-ldd
LC_ALL=C sed $(ldd-rewrite) < $< > $@.new
endef
else
define gen-ldd
LC_ALL=C sed $(ldd-rewrite) < $< \
| LC_ALL=C sed -f $(patsubst $(..)/%,/%,$(..)$(ldd-rewrite-script)) > $@.new
endef
endif

$(objpfx)ldd: ldd.bash.in $(common-objpfx)soversions.mk \
	      $(common-objpfx)config.make
	$(gen-ldd)
	chmod 555 $@.new
	mv -f $@.new $@

$(objpfx)sln: $(sln-modules:%=$(objpfx)%.o)

$(objpfx)ldconfig: $(ldconfig-modules:%=$(objpfx)%.o)

SYSCONF-FLAGS := -D'SYSCONFDIR="$(sysconfdir)"'
CFLAGS-ldconfig.c += $(SYSCONF-FLAGS) -D'LIBDIR="$(libdir)"' \
		    -D'SLIBDIR="$(slibdir)"'
libof-ldconfig = ldconfig
CFLAGS-dl-cache.c += $(SYSCONF-FLAGS)
CFLAGS-cache.c += $(SYSCONF-FLAGS)
CFLAGS-rtld.c += $(SYSCONF-FLAGS)
CFLAGS-dl-usage.c += $(SYSCONF-FLAGS) \
  -D'RTLD="$(rtlddir)/$(rtld-installed-name)"'
CFLAGS-dl-diagnostics.c += $(SYSCONF-FLAGS) \
  -D'PREFIX="$(prefix)"' \
  -D'RTLD="$(rtlddir)/$(rtld-installed-name)"'

cpp-srcs-left := $(all-rtld-routines:=.os)
lib := rtld
include $(patsubst %,$(..)libof-iterator.mk,$(cpp-srcs-left))

test-modules = $(addprefix $(objpfx),$(addsuffix .so,$(strip $(modules-names))))
generated += $(addsuffix .so,$(strip $(modules-names)))

$(objpfx)testobj1_1.so: $(objpfx)testobj1.so
$(objpfx)testobj2.so: $(objpfx)testobj1.so
$(objpfx)testobj6.so: $(objpfx)testobj1.so $(objpfx)testobj2.so
$(objpfx)failobj.so: $(objpfx)testobj6.so
$(objpfx)dep1.so: $(objpfx)dep2.so $(objpfx)dep4.so
$(objpfx)dep2.so: $(objpfx)dep3.so $(objpfx)dep4.so
$(objpfx)dep4.so: $(objpfx)dep3.so
$(objpfx)nodelmod3.so: $(objpfx)nodelmod4.so
$(objpfx)neededobj2.so: $(objpfx)neededobj1.so
$(objpfx)neededobj3.so: $(objpfx)neededobj1.so $(objpfx)neededobj2.so
$(objpfx)neededobj4.so: $(objpfx)neededobj1.so $(objpfx)neededobj2.so \
			$(objpfx)neededobj3.so
$(objpfx)neededobj6.so: $(objpfx)neededobj5.so
$(objpfx)unload2mod.so: $(objpfx)unload2dep.so
$(objpfx)firstobj.so: $(shared-thread-library)
$(objpfx)reldep4mod1.so: $(objpfx)reldep4mod3.so
$(objpfx)reldep4mod2.so: $(objpfx)reldep4mod4.so
$(objpfx)dblloadmod1.so: $(objpfx)dblloadmod3.so
$(objpfx)dblloadmod2.so: $(objpfx)dblloadmod3.so
$(objpfx)reldepmod5.so: $(objpfx)reldepmod2.so
$(objpfx)reldepmod6.so: $(objpfx)reldepmod2.so
$(objpfx)reldep6mod1.so: $(objpfx)reldep6mod0.so
$(objpfx)reldep6mod2.so: $(objpfx)reldep6mod1.so
$(objpfx)reldep6mod3.so: $(objpfx)reldep6mod2.so
$(objpfx)reldep6mod4.so: $(objpfx)reldep6mod1.so
$(objpfx)tst-tlsmod3.so: $(objpfx)tst-tlsmod2.so
$(objpfx)tst-tlsmod8.so: $(objpfx)tst-tlsmod7.so
$(objpfx)tst-tlsmod10.so: $(objpfx)tst-tlsmod9.so
$(objpfx)tst-tlsmod12.so: $(objpfx)tst-tlsmod11.so
$(objpfx)tst-tlsmod13a.so: $(objpfx)tst-tlsmod13.so
# For tst-tls9-static, make sure the modules it dlopens have libc.so in DT_NEEDED
$(objpfx)tst-tlsmod5.so: $(common-objpfx)libc.so
$(objpfx)tst-tlsmod6.so: $(common-objpfx)libc.so
$(objpfx)tst-tls19mod1.so: $(objpfx)tst-tls19mod2.so $(objpfx)tst-tls19mod3.so
$(objpfx)tst-tls19mod3.so: $(objpfx)ld.so
$(objpfx)reldep8mod3.so: $(objpfx)reldep8mod1.so $(objpfx)reldep8mod2.so
$(objpfx)nodel2mod3.so: $(objpfx)nodel2mod1.so $(objpfx)nodel2mod2.so
$(objpfx)reldep9mod2.so: $(objpfx)reldep9mod1.so
$(objpfx)reldep9mod3.so: $(objpfx)reldep9mod1.so $(objpfx)reldep9mod2.so
$(objpfx)unload3mod1.so: $(objpfx)unload3mod3.so
$(objpfx)unload3mod2.so: $(objpfx)unload3mod3.so
$(objpfx)unload3mod3.so: $(objpfx)unload3mod4.so
$(objpfx)unload4mod1.so: $(objpfx)unload4mod2.so $(objpfx)unload4mod3.so
$(objpfx)unload4mod2.so: $(objpfx)unload4mod4.so $(objpfx)unload4mod3.so
$(objpfx)unload7mod2.so: $(objpfx)unload7mod1.so
$(objpfx)unload8mod1.so: $(objpfx)unload8mod2.so
$(objpfx)unload8mod2.so: $(objpfx)unload8mod3.so
$(objpfx)tst-initordera2.so: $(objpfx)tst-initordera1.so
$(objpfx)tst-initorderb2.so: $(objpfx)tst-initorderb1.so $(objpfx)tst-initordera2.so
$(objpfx)tst-initordera3.so: $(objpfx)tst-initorderb2.so $(objpfx)tst-initorderb1.so
$(objpfx)tst-initordera4.so: $(objpfx)tst-initordera3.so
$(objpfx)tst-initorder: $(objpfx)tst-initordera4.so $(objpfx)tst-initordera1.so $(objpfx)tst-initorderb2.so
$(objpfx)tst-null-argv: $(objpfx)tst-null-argv-lib.so
$(objpfx)tst-tlsalign: $(objpfx)tst-tlsalign-lib.so
$(objpfx)tst-nodelete-opened.out: $(objpfx)tst-nodelete-opened-lib.so

$(objpfx)tst-tlsalign-extern: $(objpfx)tst-tlsalign-vars.o
$(objpfx)tst-tlsalign-extern-static: $(objpfx)tst-tlsalign-vars.o

tst-null-argv-ENV = LD_DEBUG=all LD_DEBUG_OUTPUT=$(objpfx)tst-null-argv.debug.out
LDFLAGS-nodel2mod3.so = -Wl,--no-as-needed
LDFLAGS-reldepmod5.so = -Wl,--no-as-needed
LDFLAGS-reldep6mod1.so = -Wl,--no-as-needed
LDFLAGS-reldep6mod4.so = -Wl,--no-as-needed
LDFLAGS-reldep8mod3.so = -Wl,--no-as-needed
LDFLAGS-unload4mod1.so = -Wl,--no-as-needed
LDFLAGS-unload4mod2.so = -Wl,--no-as-needed
LDFLAGS-tst-initorder = -Wl,--no-as-needed
LDFLAGS-tst-initordera2.so = -Wl,--no-as-needed
LDFLAGS-tst-initordera3.so = -Wl,--no-as-needed
LDFLAGS-tst-initordera4.so = -Wl,--no-as-needed
LDFLAGS-tst-initorderb2.so = -Wl,--no-as-needed
LDFLAGS-tst-tlsmod5.so = -nostdlib -Wl,--no-as-needed
LDFLAGS-tst-tlsmod6.so = -nostdlib -Wl,--no-as-needed

# The following tests are underlinked, and rely on late loading.  On toolchains
# that set -z now by default, this leads to failures to load or fix up the
# executables being tested.
LDFLAGS-circlemod2.so = -Wl,-z,lazy
LDFLAGS-tst-tls20mod-bad.so = -Wl,-z,lazy
LDFLAGS-reldep6mod1.so += -Wl,-z,lazy
LDFLAGS-constload2.so = -Wl,-z,lazy
LDFLAGS-constload3.so = -Wl,-z,lazy
LDFLAGS-dblloadmod3.so = -Wl,-z,lazy
LDFLAGS-ifuncmod6.so = -Wl,-z,lazy
LDFLAGS-ltglobmod2.so = -Wl,-z,lazy
LDFLAGS-testobj1.so = -Wl,-z,lazy
LDFLAGS-testobj6.so = -Wl,-z,lazy

testobj1.so-no-z-defs = yes
testobj3.so-no-z-defs = yes
testobj4.so-no-z-defs = yes
testobj5.so-no-z-defs = yes
testobj6.so-no-z-defs = yes
failobj.so-no-z-defs = yes
constload2.so-no-z-defs = yes
constload3.so-no-z-defs = yes
nodelmod1.so-no-z-defs = yes
nodelmod2.so-no-z-defs = yes
nodelmod4.so-no-z-defs = yes
nodel2mod2.so-no-z-defs = yes
reldepmod2.so-no-z-defs = yes
reldepmod3.so-no-z-defs = yes
reldepmod4.so-no-z-defs = yes
reldep4mod4.so-no-z-defs = yes
reldep4mod2.so-no-z-defs = yes
ltglobmod2.so-no-z-defs = yes
dblloadmod3.so-no-z-defs = yes
tst-tlsmod1.so-no-z-defs = yes
tst-tlsmod2.so-no-z-defs = yes
tst-tlsmod3.so-no-z-defs = yes
tst-tlsmod4.so-no-z-defs = yes
tst-tlsmod7.so-no-z-defs = yes
tst-tlsmod8.so-no-z-defs = yes
tst-tlsmod9.so-no-z-defs = yes
tst-tlsmod10.so-no-z-defs = yes
tst-tlsmod12.so-no-z-defs = yes
tst-tlsmod14a.so-no-z-defs = yes
tst-tlsmod14b.so-no-z-defs = yes
tst-tlsmod15a.so-no-z-defs = yes
tst-tlsmod16b.so-no-z-defs = yes
circlemod2.so-no-z-defs = yes
circlemod3.so-no-z-defs = yes
circlemod3a.so-no-z-defs = yes
reldep8mod2.so-no-z-defs = yes
reldep9mod1.so-no-z-defs = yes
unload3mod4.so-no-z-defs = yes
unload4mod1.so-no-z-defs = yes
ifuncmod1.so-no-z-defs = yes
ifuncmod5.so-no-z-defs = yes
ifuncmod6.so-no-z-defs = yes
tst-auditmod9a.so-no-z-defs = yes
tst-auditmod9b.so-no-z-defs = yes
tst-nodelete-uniquemod.so-no-z-defs = yes
tst-nodelete-rtldmod.so-no-z-defs = yes
tst-nodelete-zmod.so-no-z-defs = yes
tst-nodelete2mod.so-no-z-defs = yes

ifeq ($(build-shared),yes)
# Build all the modules even when not actually running test programs.
tests: $(test-modules)
endif

LDFLAGS-loadtest = -rdynamic

$(objpfx)loadtest.out: $(test-modules)

$(objpfx)neededtest.out: $(objpfx)neededobj1.so $(objpfx)neededobj2.so \
			 $(objpfx)neededobj3.so

$(objpfx)neededtest2.out: $(objpfx)neededobj1.so $(objpfx)neededobj2.so \
			  $(objpfx)neededobj3.so

$(objpfx)neededtest3.out: $(objpfx)neededobj1.so $(objpfx)neededobj2.so \
			  $(objpfx)neededobj3.so $(objpfx)neededobj4.so

$(objpfx)neededtest4: $(objpfx)neededobj1.so
$(objpfx)neededtest4.out: $(objpfx)neededobj5.so $(objpfx)neededobj6.so

$(objpfx)restest1: $(objpfx)testobj1.so $(objpfx)testobj1_1.so
LDFLAGS-restest1 = -rdynamic

LDFLAGS-restest2 = -rdynamic

$(objpfx)restest1.out: $(test-modules)

preloadtest-preloads = testobj1 testobj2 testobj3 testobj4 testobj5
$(objpfx)preloadtest: $(objpfx)testobj6.so
LDFLAGS-preloadtest = -rdynamic
$(objpfx)preloadtest.out: $(preloadtest-preloads:%=$(objpfx)%.so)
preloadtest-ENV = \
  LD_PRELOAD=$(subst $(empty) ,:,$(strip $(preloadtest-preloads:=.so)))

LDFLAGS-loadfail = -rdynamic

$(objpfx)loadfail.out: $(objpfx)failobj.so $(objpfx)testobj1.so \
		       $(objpfx)testobj2.so $(objpfx)testobj3.so \
		       $(objpfx)testobj4.so $(objpfx)testobj5.so

LDFLAGS-multiload = -rdynamic
CFLAGS-multiload.c += -DOBJDIR=\"$(elf-objpfx)\"

$(objpfx)multiload.out: $(objpfx)testobj1.so

LDFLAGS-origtest = -rdynamic
$(objpfx)origtest.out: $(objpfx)testobj1.so

$(objpfx)resolvfail.out: $(objpfx)testobj1.so
ifeq ($(have-thread-library),yes)
$(objpfx)resolvfail: $(shared-thread-library)
endif

$(objpfx)constload1.out: $(objpfx)constload2.so $(objpfx)constload3.so

$(objpfx)circleload1.out: $(objpfx)circlemod1.so \
			  $(objpfx)circlemod1a.so

$(objpfx)circlemod1.so: $(objpfx)circlemod2.so
$(objpfx)circlemod2.so: $(objpfx)circlemod3.so
$(objpfx)circlemod1a.so: $(objpfx)circlemod2a.so
$(objpfx)circlemod2a.so: $(objpfx)circlemod3a.so

$(objpfx)order: $(addprefix $(objpfx),dep4.so dep3.so dep2.so dep1.so)

$(objpfx)order-cmp.out: $(objpfx)order.out
	(echo "0123456789" | cmp $< -) > $@; \
	$(evaluate-test)

$(objpfx)vismain: $(addprefix $(objpfx),vismod1.so vismod2.so)
$(objpfx)vismain.out: $(addprefix $(objpfx),vismod3.so)
vismain-ENV = LD_PRELOAD=$(addprefix $(objpfx),vismod3.so)

$(objpfx)noload: $(objpfx)testobj1.so
LDFLAGS-noload = -rdynamic -Wl,--no-as-needed
$(objpfx)noload.out: $(objpfx)testobj5.so

$(objpfx)noload-mem.out: $(objpfx)noload.out
	$(common-objpfx)malloc/mtrace $(objpfx)noload.mtrace > $@; \
	$(evaluate-test)
noload-ENV = MALLOC_TRACE=$(objpfx)noload.mtrace \
	     LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

LDFLAGS-nodelete = -rdynamic
LDFLAGS-nodelmod1.so = -Wl,--enable-new-dtags,-z,nodelete
LDFLAGS-nodelmod4.so = -Wl,--enable-new-dtags,-z,nodelete
$(objpfx)nodelete.out: $(objpfx)nodelmod1.so $(objpfx)nodelmod2.so \
		       $(objpfx)nodelmod3.so

LDFLAGS-nodlopenmod.so = -Wl,--enable-new-dtags,-z,nodlopen
$(objpfx)nodlopen.out: $(objpfx)nodlopenmod.so

$(objpfx)nodlopenmod2.so: $(objpfx)nodlopenmod.so
$(objpfx)nodlopen2.out: $(objpfx)nodlopenmod2.so

$(objpfx)filtmod1.so: $(objpfx)filtmod1.os $(objpfx)filtmod2.so
	$(LINK.o) -shared -o $@ -B$(csu-objpfx) $(LDFLAGS.so) \
		  $(dt-relr-ldflag) \
		  -L$(subst :, -L,$(rpath-link)) \
		  -Wl,-rpath-link=$(rpath-link) \
		  $< -Wl,-F,$(objpfx)filtmod2.so
$(objpfx)filter: $(objpfx)filtmod1.so

# This does not link against libc.
CFLAGS-filtmod1.c += $(no-stack-protector)

$(objpfx)unload.out: $(objpfx)unloadmod.so

$(objpfx)reldep.out: $(objpfx)reldepmod1.so $(objpfx)reldepmod2.so

$(objpfx)reldep2.out: $(objpfx)reldepmod1.so $(objpfx)reldepmod3.so

$(objpfx)reldep3.out: $(objpfx)reldepmod1.so $(objpfx)reldepmod4.so

$(objpfx)reldep4.out: $(objpfx)reldep4mod1.so $(objpfx)reldep4mod2.so

$(objpfx)next: $(objpfx)nextmod1.so $(objpfx)nextmod2.so
LDFLAGS-next = -Wl,--no-as-needed

$(objpfx)tst-next-ver: $(objpfx)nextmod3.so
LDFLAGS-tst-next-ver = -Wl,--no-as-needed

$(objpfx)unload2.out: $(objpfx)unload2mod.so $(objpfx)unload2dep.so

$(objpfx)lateglobal.out: $(objpfx)ltglobmod1.so $(objpfx)ltglobmod2.so

$(objpfx)tst-pathopt.out: tst-pathopt.sh $(objpfx)tst-pathopt \
			  $(objpfx)pathoptobj.so
	$(SHELL) $< $(common-objpfx) '$(test-wrapper-env)' \
		 '$(run-program-env)'; \
	$(evaluate-test)

$(objpfx)tst-rtld-load-self.out: tst-rtld-load-self.sh $(objpfx)ld.so
	$(SHELL) $^ '$(test-wrapper)' '$(test-wrapper-env)' > $@; \
	$(evaluate-test)

tst-rtld-preload-OBJS = $(subst $(empty) ,:,$(strip $(preloadtest-preloads:=.so)))
$(objpfx)tst-rtld-preload.out: tst-rtld-preload.sh $(objpfx)ld.so \
			       $(objpfx)preloadtest \
			       $(preloadtest-preloads:%=$(objpfx)%.so)
	$(SHELL) $< $(objpfx)ld.so $(objpfx)preloadtest \
		    '$(test-wrapper-env)' '$(run_program_env)' \
		    '$(rpath-link)' '$(tst-rtld-preload-OBJS)' > $@; \
	$(evaluate-test)

$(objpfx)initfirst.out: $(objpfx)firstobj.so

$(objpfx)global: $(objpfx)globalmod1.so
$(objpfx)global.out: $(objpfx)reldepmod1.so $(objpfx)reldepmod4.so

$(objpfx)dblload.out: $(objpfx)dblloadmod1.so $(objpfx)dblloadmod2.so

$(objpfx)dblunload.out: $(objpfx)dblloadmod1.so $(objpfx)dblloadmod2.so

$(objpfx)reldep5.out: $(objpfx)reldepmod5.so $(objpfx)reldepmod6.so

$(objpfx)reldep6.out: $(objpfx)reldep6mod3.so $(objpfx)reldep6mod4.so

$(objpfx)reldep7.out: $(objpfx)reldep7mod1.so $(objpfx)reldep7mod2.so

$(objpfx)reldep8.out: $(objpfx)reldep8mod3.so

LDFLAGS-nodel2mod2.so = -Wl,--enable-new-dtags,-z,nodelete
$(objpfx)nodelete2.out: $(objpfx)nodel2mod3.so

$(objpfx)reldep9.out: $(objpfx)reldep9mod3.so

$(objpfx)tst-tls3: $(objpfx)tst-tlsmod1.so

$(objpfx)tst-tls4.out: $(objpfx)tst-tlsmod2.so

$(objpfx)tst-tls5.out: $(objpfx)tst-tlsmod2.so

$(objpfx)tst-tls6.out: $(objpfx)tst-tlsmod2.so

$(objpfx)tst-tls7.out: $(objpfx)tst-tlsmod3.so

$(objpfx)tst-tls8.out: $(objpfx)tst-tlsmod3.so $(objpfx)tst-tlsmod4.so

$(objpfx)tst-tls9.out: $(objpfx)tst-tlsmod5.so $(objpfx)tst-tlsmod6.so

$(objpfx)tst-tls10: $(objpfx)tst-tlsmod8.so $(objpfx)tst-tlsmod7.so

$(objpfx)tst-tls11: $(objpfx)tst-tlsmod10.so $(objpfx)tst-tlsmod9.so

$(objpfx)tst-tls12: $(objpfx)tst-tlsmod12.so $(objpfx)tst-tlsmod11.so

$(objpfx)tst-tls13.out: $(objpfx)tst-tlsmod13a.so

$(objpfx)tst-tls14: $(objpfx)tst-tlsmod14a.so
$(objpfx)tst-tls14.out: $(objpfx)tst-tlsmod14b.so

$(objpfx)tst-tls15.out: $(objpfx)tst-tlsmod15a.so $(objpfx)tst-tlsmod15b.so

$(objpfx)tst-tls-dlinfo.out: $(objpfx)tst-tlsmod2.so



$(objpfx)tst-tls16.out: $(objpfx)tst-tlsmod16a.so $(objpfx)tst-tlsmod16b.so

$(objpfx)tst-tls17.out: $(objpfx)tst-tlsmod17b.so
$(patsubst %,$(objpfx)%.os,$(tlsmod17a-modules)): $(objpfx)tst-tlsmod17a%.os: tst-tlsmod17a.c
	$(compile-command.c) -DN=$*
$(patsubst %,$(objpfx)%.so,$(tlsmod17a-modules)): $(objpfx)tst-tlsmod17a%.so: $(objpfx)ld.so
$(objpfx)tst-tlsmod17b.so: $(patsubst %,$(objpfx)%.so,$(tlsmod17a-modules))

$(objpfx)tst-tls18.out: $(patsubst %,$(objpfx)%.so,$(tlsmod18a-modules))
$(patsubst %,$(objpfx)%.os,$(tlsmod18a-modules)): $(objpfx)tst-tlsmod18a%.os : tst-tlsmod18a.c
	$(compile-command.c) -DN=$*
$(patsubst %,$(objpfx)%.so,$(tlsmod18a-modules)): $(objpfx)tst-tlsmod18a%.so: $(objpfx)ld.so

$(objpfx)tst-tls19.out: $(objpfx)tst-tls19mod1.so

CFLAGS-tst-align.c += $(stack-align-test-flags)
CFLAGS-tst-align2.c += $(stack-align-test-flags)
CFLAGS-tst-alignmod.c += $(stack-align-test-flags)
CFLAGS-tst-alignmod2.c += $(stack-align-test-flags)
$(objpfx)tst-align.out: $(objpfx)tst-alignmod.so
$(objpfx)tst-align2: $(objpfx)tst-alignmod2.so
$(objpfx)tst-align3: $(objpfx)tst-alignmod3.so
ifeq (yes,$(have-fpie))
CFLAGS-tst-align3.c += $(PIE-ccflag)
endif
LDFLAGS-tst-align3 += -Wl,-z,max-page-size=0x200000
LDFLAGS-tst-alignmod3.so += -Wl,-z,max-page-size=0x200000
$(objpfx)tst-alignmod3.so: $(libsupport)

$(objpfx)unload3.out: $(objpfx)unload3mod1.so $(objpfx)unload3mod2.so \
		      $(objpfx)unload3mod3.so $(objpfx)unload3mod4.so

$(objpfx)unload4.out: $(objpfx)unload4mod1.so $(objpfx)unload4mod3.so

$(objpfx)unload5.out: $(objpfx)unload3mod1.so $(objpfx)unload3mod2.so \
		      $(objpfx)unload3mod3.so $(objpfx)unload3mod4.so

$(objpfx)unload6.out: $(objpfx)unload6mod1.so $(objpfx)unload6mod2.so \
		      $(objpfx)unload6mod3.so

$(objpfx)unload7.out: $(objpfx)unload7mod1.so $(objpfx)unload7mod2.so
unload7-ENV = MALLOC_PERTURB_=85

$(objpfx)unload8.out: $(objpfx)unload8mod1.so $(objpfx)unload8mod1x.so

$(objpfx)tst-tls9-static.out: $(objpfx)tst-tlsmod5.so $(objpfx)tst-tlsmod6.so

ifeq ($(have-z-execstack),yes)
$(objpfx)tst-execstack.out: $(objpfx)tst-execstack-mod.so
CPPFLAGS-tst-execstack.c += -DUSE_PTHREADS=0
LDFLAGS-tst-execstack = -Wl,-z,noexecstack
LDFLAGS-tst-execstack-mod.so = -Wl,-z,execstack

$(objpfx)tst-execstack-needed: $(objpfx)tst-execstack-mod.so
LDFLAGS-tst-execstack-needed = -Wl,-z,noexecstack

LDFLAGS-tst-execstack-prog = -Wl,-z,execstack
CFLAGS-tst-execstack-prog.c += -Wno-trampolines
CFLAGS-tst-execstack-mod.c += -Wno-trampolines
endif

LDFLAGS-tst-array2 = -Wl,--no-as-needed
LDFLAGS-tst-array5 = -Wl,--no-as-needed

$(objpfx)tst-array1-cmp.out: tst-array1.exp $(objpfx)tst-array1.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)tst-array1-static-cmp.out: tst-array1.exp \
				    $(objpfx)tst-array1-static.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)tst-array2: $(objpfx)tst-array2dep.so
$(objpfx)tst-array2-cmp.out: tst-array2.exp $(objpfx)tst-array2.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)tst-array3-cmp.out: tst-array1.exp $(objpfx)tst-array3.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)tst-array4.out: $(objpfx)tst-array2dep.so
$(objpfx)tst-array4-cmp.out: tst-array4.exp $(objpfx)tst-array4.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)tst-array5: $(objpfx)tst-array5dep.so
$(objpfx)tst-array5-cmp.out: tst-array5.exp $(objpfx)tst-array5.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)tst-array5-static-cmp.out: tst-array5-static.exp \
				$(objpfx)tst-array5-static.out
	cmp $^ > $@; \
	$(evaluate-test)

CFLAGS-tst-pie1.c += $(pie-ccflag)
CFLAGS-tst-pie2.c += $(pie-ccflag)

$(objpfx)tst-piemod1.so: $(libsupport)
$(objpfx)tst-pie1: $(objpfx)tst-piemod1.so
$(objpfx)tst-dlopen-pie.out: $(objpfx)tst-pie1

ifeq (yes,$(build-shared))
# NB: Please keep cet-built-dso in sysdeps/x86/Makefile in sync with
# all-built-dso here.
all-built-dso := $(common-objpfx)elf/ld.so $(common-objpfx)libc.so \
		 $(filter-out $(common-objpfx)linkobj/libc.so, \
			      $(sort $(wildcard $(addprefix $(common-objpfx), \
							    */lib*.so \
							    iconvdata/*.so))))

$(all-built-dso:=.dyn): %.dyn: %
	@rm -f $@T
	LC_ALL=C $(READELF) -W -d $< > $@T
	test -s $@T
	mv -f $@T $@
common-generated += $(all-built-dso:$(common-objpfx)%=%.dyn)

$(objpfx)check-textrel.out: $(..)scripts/check-textrel.awk \
			    $(all-built-dso:=.dyn)
	LC_ALL=C $(AWK) -f $^ > $@; \
	$(evaluate-test)
generated += check-textrel.out

$(objpfx)execstack-default: $(first-word $(wildcard $(sysdirs:%=%/stackinfo.h)))
	$(make-target-directory)
	{ echo '#include <elf.h>'; \
	  echo '#include <stackinfo.h>'; \
	  echo '#if (DEFAULT_STACK_PERMS & PF_X) == 0'; \
	  echo '@@@execstack-no@@@'; \
	  echo '#else'; \
	  echo '@@@execstack-yes@@@'; \
	  echo '#endif'; } | \
	$(CC) $(CFLAGS) $(CPPFLAGS) -E -x c-header - | \
	sed -n -e 's/^@@@\(.*\)@@@/\1/p' > $@T
	mv -f $@T $@
generated += execstack-default

$(all-built-dso:=.phdr): %.phdr: %
	@rm -f $@T
	LC_ALL=C $(READELF) -W -l $< > $@T
	test -s $@T
	mv -f $@T $@
common-generated += $(all-built-dso:$(common-objpfx)%=%.phdr)

$(objpfx)check-execstack.out: $(..)scripts/check-execstack.awk \
			      $(objpfx)execstack-default \
			      $(all-built-dso:=.phdr)
	LC_ALL=C $(AWK) -v "xfail=$(check-execstack-xfail)" -f $^ > $@; \
	$(evaluate-test)
generated += check-execstack.out

$(objpfx)check-wx-segment.out: $(..)scripts/check-wx-segment.py \
			      $(all-built-dso:=.phdr)
	$(PYTHON) $^ --xfail="$(check-wx-segment-xfail)" > $@; \
	$(evaluate-test)
generated += check-wx-segment.out

$(objpfx)tst-dlmodcount.out: $(test-modules)

$(all-built-dso:=.jmprel): %.jmprel: % Makefile
	@rm -f $@T
	LC_ALL=C $(READELF) -W -S -d -r $< > $@T
	test -s $@T
	mv -f $@T $@
common-generated += $(all-built-dso:$(common-objpfx)%=%.jmprel)

localplt-built-dso := $(addprefix $(common-objpfx),\
				  libc.so \
				  elf/ld.so \
				  math/libm.so \
				  dlfcn/libdl.so \
				  resolv/libresolv.so \
		       )
ifeq ($(build-mathvec),yes)
localplt-built-dso += $(addprefix $(common-objpfx), mathvec/libmvec.so)
endif
ifeq ($(have-thread-library),yes)
localplt-built-dso += $(filter-out %_nonshared.a, $(shared-thread-library))
endif
ifneq ($(pthread-in-libc),yes)
localplt-built-dso += $(addprefix $(common-objpfx), rt/librt.so)
endif

vpath localplt.data $(+sysdep_dirs)

$(objpfx)check-localplt.out: $(..)scripts/check-localplt.awk \
			     $(..)scripts/localplt.awk \
			     $(localplt-built-dso:=.jmprel) \
			     localplt.data
	LC_ALL=C $(AWK) -f $(filter-out $< %localplt.data,$^) | \
	  LC_ALL=C $(AWK) -f $< $(filter %localplt.data,$^) - \
	  > $@; \
	$(evaluate-test)
endif

$(all-built-dso:=.dynsym): %.dynsym: %
	@rm -f $@T
	LC_ALL=C $(READELF) -W --dyn-syms $< > $@T
	test -s $@T
	mv -f $@T $@
common-generated += $(all-built-dso:$(common-objpfx)%=%.dynsym)

$(objpfx)check-initfini.out: $(..)scripts/check-initfini.awk \
			    $(all-built-dso:=.dynsym)
	LC_ALL=C $(AWK) -f $^ > $@; \
	$(evaluate-test)
generated += check-initfini.out

$(objpfx)tst-dlopenrpath: $(objpfx)tst-dlopenrpathmod.so
CFLAGS-tst-dlopenrpath.c += -DPFX=\"$(objpfx)\"
LDFLAGS-tst-dlopenrpathmod.so += -Wl,-rpath,\$$ORIGIN/test-subdir
$(objpfx)tst-dlopenrpath.out: $(objpfx)firstobj.so

$(objpfx)tst-deep1mod2.so: $(objpfx)tst-deep1mod3.so
$(objpfx)tst-deep1: $(objpfx)tst-deep1mod1.so
$(objpfx)tst-deep1.out: $(objpfx)tst-deep1mod2.so
LDFLAGS-tst-deep1 += -rdynamic
tst-deep1mod3.so-no-z-defs = yes

$(objpfx)tst-dlmopen1.out: $(objpfx)tst-dlmopen1mod.so

$(objpfx)tst-dlmopen2.out: $(objpfx)tst-dlmopen1mod.so

$(objpfx)tst-dlmopen3.out: $(objpfx)tst-dlmopen1mod.so

$(objpfx)tst-dlmopen4.out: $(objpfx)tst-dlmopen1mod.so

$(objpfx)tst-audit1.out: $(objpfx)tst-auditmod1.so
tst-audit1-ENV = LD_AUDIT=$(objpfx)tst-auditmod1.so

$(objpfx)tst-audit2.out: $(objpfx)tst-auditmod1.so $(objpfx)tst-auditmod9b.so
# Prevent GCC-5 from translating a malloc/memset pair into calloc
CFLAGS-tst-audit2.c += -fno-builtin
tst-audit2-ENV = LD_AUDIT=$(objpfx)tst-auditmod1.so

$(objpfx)tst-audit9.out: $(objpfx)tst-auditmod9a.so $(objpfx)tst-auditmod9b.so
tst-audit9-ENV = LD_AUDIT=$(objpfx)tst-auditmod9a.so

$(objpfx)tst-audit8: $(libm)
$(objpfx)tst-audit8.out: $(objpfx)tst-auditmod1.so
tst-audit8-ENV = LD_AUDIT=$(objpfx)tst-auditmod1.so

$(objpfx)tst-global1.out: $(objpfx)testobj6.so $(objpfx)testobj2.so

$(objpfx)tst-global2: $(objpfx)tst-globalmod2.so
$(objpfx)tst-global2.out: $(objpfx)reldepmod1.so $(objpfx)reldepmod4.so
LDFLAGS-tst-global2 = -Wl,--enable-new-dtags
LDFLAGS-tst-globalmod2.so = -Wl,--enable-new-dtags

$(objpfx)order2.out: $(objpfx)order2mod1.so $(objpfx)order2mod2.so
$(objpfx)order2-cmp.out: $(objpfx)order2.out
	(echo "12345" | cmp $< -) > $@; \
	$(evaluate-test)
$(objpfx)order2mod1.so: $(objpfx)order2mod4.so
$(objpfx)order2mod4.so: $(objpfx)order2mod3.so
$(objpfx)order2mod2.so: $(objpfx)order2mod3.so
order2mod2.so-no-z-defs = yes
LDFLAGS-order2mod1.so = -Wl,--no-as-needed
LDFLAGS-order2mod2.so = -Wl,--no-as-needed

tst-stackguard1-ARGS = --command "$(host-test-program-cmd) --child"
tst-stackguard1-static-ARGS = --command "$(objpfx)tst-stackguard1-static --child"

tst-ptrguard1-ARGS = --command "$(host-test-program-cmd) --child"
# When built statically, the pointer guard interface uses
# __pointer_chk_guard_local.
CFLAGS-tst-ptrguard1-static.c += -DPTRGUARD_LOCAL
tst-ptrguard1-static-ARGS = --command "$(objpfx)tst-ptrguard1-static --child"

$(objpfx)tst-leaks1-mem.out: $(objpfx)tst-leaks1.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-leaks1.mtrace > $@; \
	$(evaluate-test)

tst-leaks1-ENV = MALLOC_TRACE=$(objpfx)tst-leaks1.mtrace \
		 LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)tst-thrlock: $(shared-thread-library)
$(objpfx)tst-thrlock.out: $(libm)
$(objpfx)tst-noload.out: $(libm)

tst-tst-dlopen-tlsmodid-no-pie = yes
$(objpfx)tst-dlopen-tlsmodid: $(shared-thread-library)
$(objpfx)tst-dlopen-tlsmodid.out: $(objpfx)tst-dlopen-self
CFLAGS-tst-dlopen-tlsmodid-pie.c += $(pie-ccflag)
$(objpfx)tst-dlopen-tlsmodid-pie: $(shared-thread-library)
$(objpfx)tst-dlopen-tlsmodid-pie.out: $(objpfx)tst-dlopen-self-pie
$(objpfx)tst-dlopen-tlsmodid-container: $(shared-thread-library)
LDFLAGS-tst-dlopen-tlsmodid-container += -Wl,-rpath,\$$ORIGIN

tst-tst-dlopen-self-no-pie = yes
CFLAGS-tst-dlopen-self-pie.c += $(pie-ccflag)
LDFLAGS-tst-dlopen-self-container += -Wl,-rpath,\$$ORIGIN

CFLAGS-ifuncmain1pic.c += $(pic-ccflag)
CFLAGS-ifuncmain1picstatic.c += $(pic-ccflag)
CFLAGS-ifuncmain1staticpic.c += $(pic-ccflag)
CFLAGS-ifuncdep1pic.c += $(pic-ccflag)
CFLAGS-ifuncmain1vispic.c += $(pic-ccflag)
CFLAGS-ifuncmain2pic.c += $(pic-ccflag)
CFLAGS-ifuncmain2picstatic.c += $(pic-ccflag)
CFLAGS-ifuncdep2pic.c += $(pic-ccflag)
CFLAGS-ifuncmain4picstatic.c += $(pic-ccflag)
CFLAGS-ifuncmain5pic.c += $(pic-ccflag)
CFLAGS-ifuncmain5picstatic.c += $(pic-ccflag)
CFLAGS-ifuncmain5staticpic.c += $(pic-ccflag)
CFLAGS-ifuncdep5pic.c += $(pic-ccflag)
CFLAGS-ifuncmain7pic.c += $(pic-ccflag)
CFLAGS-ifuncmain7picstatic.c += $(pic-ccflag)
CFLAGS-ifuncmain9pic.c += $(pic-ccflag)
CFLAGS-ifuncmain9picstatic.c += $(pic-ccflag)

LDFLAGS-ifuncmain3 = -Wl,-export-dynamic

CFLAGS-ifuncmain1pie.c += $(pie-ccflag)
CFLAGS-ifuncmain1vispie.c += $(pie-ccflag)
CFLAGS-ifuncmain1staticpie.c += $(pie-ccflag)
CFLAGS-ifuncmain5pie.c += $(pie-ccflag)
CFLAGS-ifuncmain6pie.c += $(pie-ccflag)
CFLAGS-ifuncmain7pie.c += $(pie-ccflag)
CFLAGS-ifuncmain9pie.c += $(pie-ccflag)
CFLAGS-tst-ifunc-textrel.c += $(pic-ccflag)

LDFLAGS-ifuncmain6pie = -Wl,-z,lazy

$(objpfx)ifuncmain1pie: $(objpfx)ifuncmod1.so
$(objpfx)ifuncmain1staticpie: $(objpfx)ifuncdep1pic.o
$(objpfx)ifuncmain1vispie: $(objpfx)ifuncmod1.so
$(objpfx)ifuncmain5pie: $(objpfx)ifuncmod5.so
$(objpfx)ifuncmain6pie: $(objpfx)ifuncmod6.so

$(objpfx)ifuncmain1: $(addprefix $(objpfx),ifuncmod1.so)
$(objpfx)ifuncmain1pic: $(addprefix $(objpfx),ifuncmod1.so)
$(objpfx)ifuncmain1staticpic: $(addprefix $(objpfx),ifuncdep1pic.o)
$(objpfx)ifuncmain1static: $(addprefix $(objpfx),ifuncdep1.o)
$(objpfx)ifuncmain1picstatic: $(addprefix $(objpfx),ifuncdep1pic.o)
$(objpfx)ifuncmain1vis: $(addprefix $(objpfx),ifuncmod1.so)
$(objpfx)ifuncmain1vispic: $(addprefix $(objpfx),ifuncmod1.so)
$(objpfx)ifuncmain2: $(addprefix $(objpfx),ifuncdep2.o)
$(objpfx)ifuncmain2pic: $(addprefix $(objpfx),ifuncdep2pic.o)
$(objpfx)ifuncmain2static: $(addprefix $(objpfx),ifuncdep2.o)
$(objpfx)ifuncmain2picstatic: $(addprefix $(objpfx),ifuncdep2pic.o)

$(objpfx)ifuncmain3.out: $(objpfx)ifuncmod3.so

$(objpfx)ifuncmain5: $(addprefix $(objpfx),ifuncmod5.so)
$(objpfx)ifuncmain5pic: $(addprefix $(objpfx),ifuncmod5.so)
$(objpfx)ifuncmain5static: $(addprefix $(objpfx),ifuncdep5.o)
$(objpfx)ifuncmain5staticpic: $(addprefix $(objpfx),ifuncdep5pic.o)
$(objpfx)ifuncmain5picstatic: $(addprefix $(objpfx),ifuncdep5pic.o)

LDFLAGS-tst-ifunc-fault-lazy = -Wl,-z,lazy
LDFLAGS-tst-ifunc-fault-bindnow = -Wl,-z,now
define tst-ifunc-fault-script
( $(test-wrapper) $(rtld-prefix) --verify $^ \
  && $(test-wrapper-env) LD_TRACE_LOADED_OBJECTS=1 $(rtld-prefix) $^ \
  && $(test-wrapper-env) LD_TRACE_LOADED_OBJECTS=1 LD_DEBUG=unused \
			 $(rtld-prefix) $^ \
) > $@; $(evaluate-test)
endef
$(objpfx)tst-ifunc-fault-lazy.out: $(objpfx)tst-ifunc-fault-lazy $(objpfx)ld.so
	$(tst-ifunc-fault-script)
$(objpfx)tst-ifunc-fault-bindnow.out: $(objpfx)tst-ifunc-fault-bindnow \
   $(objpfx)ld.so
	$(tst-ifunc-fault-script)

$(objpfx)tst-unique1.out: $(objpfx)tst-unique1mod1.so \
			  $(objpfx)tst-unique1mod2.so

$(objpfx)tst-unique2: $(objpfx)tst-unique2mod1.so
$(objpfx)tst-unique2.out: $(objpfx)tst-unique2mod2.so

$(objpfx)tst-unique3: $(objpfx)tst-unique3lib.so
$(objpfx)tst-unique3.out: $(objpfx)tst-unique3lib2.so

$(objpfx)tst-unique4: $(objpfx)tst-unique4lib.so

$(objpfx)tst-nodelete.out: $(objpfx)tst-nodelete-uniquemod.so \
			   $(objpfx)tst-nodelete-rtldmod.so \
			   $(objpfx)tst-nodelete-zmod.so

LDFLAGS-tst-nodelete = -rdynamic
LDFLAGS-tst-nodelete-zmod.so = -Wl,--enable-new-dtags,-z,nodelete

$(objpfx)tst-nodelete2.out: $(objpfx)tst-nodelete2mod.so

LDFLAGS-tst-nodelete2 = -rdynamic

$(objpfx)tst-initorder-cmp.out: tst-initorder.exp $(objpfx)tst-initorder.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)tst-initorder2: $(objpfx)tst-initorder2a.so $(objpfx)tst-initorder2d.so $(objpfx)tst-initorder2c.so
$(objpfx)tst-initorder2a.so: $(objpfx)tst-initorder2b.so
$(objpfx)tst-initorder2b.so: $(objpfx)tst-initorder2c.so
$(objpfx)tst-initorder2c.so: $(objpfx)tst-initorder2d.so
LDFLAGS-tst-initorder2 = -Wl,--no-as-needed
LDFLAGS-tst-initorder2a.so = -Wl,--no-as-needed
LDFLAGS-tst-initorder2b.so = -Wl,--no-as-needed
LDFLAGS-tst-initorder2c.so = -Wl,--no-as-needed
define o-iterator-doit
$(objpfx)tst-initorder2$o.os: tst-initorder2.c; \
$$(compile-command.c) -DNAME=\"$o\"
endef
object-suffixes-left := a b c d
include $(o-iterator)

$(objpfx)tst-initorder2-cmp.out: tst-initorder2.exp $(objpfx)tst-initorder2.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)tst-relsort1mod1.so: $(libm) $(objpfx)tst-relsort1mod2.so
$(objpfx)tst-relsort1mod2.so: $(libm)
$(objpfx)tst-relsort1.out: $(objpfx)tst-relsort1mod1.so \
			   $(objpfx)tst-relsort1mod2.so

$(objpfx)tst-unused-dep.out: $(objpfx)testobj1.so
	$(test-wrapper-env) \
	LD_TRACE_LOADED_OBJECTS=1 \
	LD_DEBUG=unused \
	LD_PRELOAD= \
	$(rtld-prefix) \
	  $< > $@; \
	$(evaluate-test)

$(objpfx)tst-unused-dep-cmp.out: $(objpfx)tst-unused-dep.out
	cmp $< /dev/null > $@; \
	$(evaluate-test)

tst-tunables-enable_secure-env-ARGS = -- $(host-test-program-cmd)

$(objpfx)tst-audit11.out: $(objpfx)tst-auditmod11.so $(objpfx)tst-audit11mod1.so
tst-audit11-ENV = LD_AUDIT=$(objpfx)tst-auditmod11.so
$(objpfx)tst-audit11mod1.so: $(objpfx)tst-audit11mod2.so
LDFLAGS-tst-audit11mod2.so = -Wl,--version-script=tst-audit11mod2.map,-soname,tst-audit11mod2.so

$(objpfx)tst-audit12.out: $(objpfx)tst-auditmod12.so $(objpfx)tst-audit12mod1.so $(objpfx)tst-audit12mod3.so
tst-audit12-ENV = LD_AUDIT=$(objpfx)tst-auditmod12.so
$(objpfx)tst-audit12mod1.so: $(objpfx)tst-audit12mod2.so
LDFLAGS-tst-audit12mod2.so = -Wl,--version-script=tst-audit12mod2.map

$(objpfx)tst-audit13.out: $(objpfx)tst-audit13mod1.so
LDFLAGS-tst-audit13mod1.so = -Wl,-z,lazy
tst-audit13-ENV = LD_AUDIT=$(objpfx)tst-audit13mod1.so

$(objpfx)tst-auditmany.out: $(objpfx)tst-auditmanymod1.so \
  $(objpfx)tst-auditmanymod2.so $(objpfx)tst-auditmanymod3.so \
  $(objpfx)tst-auditmanymod4.so $(objpfx)tst-auditmanymod5.so \
  $(objpfx)tst-auditmanymod6.so $(objpfx)tst-auditmanymod7.so \
  $(objpfx)tst-auditmanymod8.so	$(objpfx)tst-auditmanymod9.so
tst-auditmany-ENV = \
  LD_AUDIT=tst-auditmanymod1.so:tst-auditmanymod2.so:tst-auditmanymod3.so:tst-auditmanymod4.so:tst-auditmanymod5.so:tst-auditmanymod6.so:tst-auditmanymod7.so:tst-auditmanymod8.so:tst-auditmanymod9.so

LDFLAGS-tst-audit14 = -Wl,--audit=tst-auditlogmod-1.so,--disable-new-dtags
$(objpfx)tst-auditlogmod-1.so: $(libsupport)
$(objpfx)tst-audit14.out: $(objpfx)tst-auditlogmod-1.so
LDFLAGS-tst-audit14a = -Wl,--audit=tst-auditlogmod-1.so,--enable-new-dtags
$(objpfx)tst-audit14a.out: $(objpfx)tst-auditlogmod-1.so
LDFLAGS-tst-audit15 = \
  -Wl,--audit=tst-auditlogmod-1.so,--depaudit=tst-auditlogmod-2.so
$(objpfx)tst-auditlogmod-2.so: $(libsupport)
$(objpfx)tst-audit15.out: \
  $(objpfx)tst-auditlogmod-1.so $(objpfx)tst-auditlogmod-2.so
LDFLAGS-tst-audit16 = \
  -Wl,--audit=tst-auditlogmod-1.so:tst-auditlogmod-2.so \
  -Wl,--depaudit=tst-auditlogmod-3.so
$(objpfx)tst-auditlogmod-3.so: $(libsupport)
$(objpfx)tst-audit16.out: \
  $(objpfx)tst-auditlogmod-1.so $(objpfx)tst-auditlogmod-2.so \
  $(objpfx)tst-auditlogmod-3.so
$(objpfx)tst-audit17.out: $(objpfx)tst-auditmod17.so
# The test check if a audit library without libc.so on DT_NEEDED works as
# intended, so it uses an explicit link rule.
$(objpfx)tst-auditmod17.so: $(objpfx)tst-auditmod17.os
	$(CC) -nostdlib -nostartfiles -shared -o $@.new \
	$(filter-out $(map-file),$^)
	$(call after-link,$@.new)
	mv -f $@.new $@
CFLAGS-.os += $(call elide-stack-protector,.os,tst-auditmod17)
tst-audit17-ENV = LD_AUDIT=$(objpfx)tst-auditmod17.so

$(objpfx)tst-audit14-cmp.out: tst-audit14.exp $(objpfx)tst-audit14.out
	cmp $^ > $@; \
	$(evaluate-test)
$(objpfx)tst-audit14a-cmp.out: tst-audit14.exp $(objpfx)tst-audit14a.out
	cmp $^ > $@; \
	$(evaluate-test)
$(objpfx)tst-audit15-cmp.out: tst-audit15.exp $(objpfx)tst-audit15.out
	cmp $^ > $@; \
	$(evaluate-test)
$(objpfx)tst-audit16-cmp.out: tst-audit16.exp $(objpfx)tst-audit16.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)tst-audit18.out: $(objpfx)tst-auditmod18.so \
			  $(objpfx)tst-audit18mod.so
tst-audit18-ARGS = -- $(host-test-program-cmd)

$(objpfx)tst-audit19a.out: $(objpfx)tst-auditmod19a.so
tst-audit19a-ENV = LD_AUDIT=$(objpfx)tst-auditmod19a.so

$(objpfx)tst-audit19b.out: $(objpfx)tst-auditmod19b.so
$(objpfx)tst-audit19b: $(objpfx)tst-audit19bmod.so
tst-audit19b-ARGS = -- $(host-test-program-cmd)

$(objpfx)tst-audit20.out: $(objpfx)tst-auditmod20.so
tst-audit20-ENV = LD_AUDIT=$(objpfx)tst-auditmod20.so

$(objpfx)tst-audit21: $(shared-thread-library)
$(objpfx)tst-audit21.out: $(objpfx)tst-auditmod21a.so
$(objpfx)tst-auditmod21a.so: $(objpfx)tst-auditmod21b.so
tst-audit21-ENV = LD_AUDIT=$(objpfx)tst-auditmod21a.so

$(objpfx)tst-audit22.out: $(objpfx)tst-auditmod22.so
tst-audit22-ARGS = -- $(host-test-program-cmd)

$(objpfx)tst-audit23.out: $(objpfx)tst-auditmod23.so \
			  $(objpfx)tst-audit23mod.so
tst-audit23-ARGS = -- $(host-test-program-cmd)

$(objpfx)tst-audit24a.out: $(objpfx)tst-auditmod24a.so
$(objpfx)tst-audit24a: $(objpfx)tst-audit24amod1.so \
		       $(objpfx)tst-audit24amod2.so
tst-audit24a-ENV = LD_AUDIT=$(objpfx)tst-auditmod24a.so
LDFLAGS-tst-audit24a = -Wl,-z,now

$(objpfx)tst-audit24b.out: $(objpfx)tst-auditmod24b.so
$(objpfx)tst-audit24b: $(objpfx)tst-audit24bmod1.so \
		       $(objpfx)tst-audit24bmod2.so
$(objpfx)tst-audit24bmod1: $(objpfx)tst-audit24bmod2.so
# The test checks if a library without .gnu.version correctly calls the
# audit callbacks.  So it uses an explicit link rule to avoid linking
# against libc.so.
$(objpfx)tst-audit24bmod1.so: $(objpfx)tst-audit24bmod1.os
	$(CC) -nostdlib -nostartfiles -shared -o $@.new $(objpfx)tst-audit24bmod1.os \
	  -Wl,-z,now
	$(call after-link,$@.new)
	mv -f $@.new $@
CFLAGS-.os += $(call elide-stack-protector,.os,tst-audit24bmod1)
$(objpfx)tst-audit24bmod2.so: $(objpfx)tst-audit24bmod2.os
	$(CC) -nostdlib -nostartfiles -shared -o $@.new $(objpfx)tst-audit24bmod2.os
	$(call after-link,$@.new)
	mv -f $@.new $@
CFLAGS-.os += $(call elide-stack-protector,.os,tst-audit24bmod2)
tst-audit24b-ENV = LD_AUDIT=$(objpfx)tst-auditmod24b.so
LDFLAGS-tst-audit24b = -Wl,-z,now

# Same as tst-audit24a, but tests LD_BIND_NOW
$(objpfx)tst-audit24c.out: $(objpfx)tst-auditmod24c.so
$(objpfx)tst-audit24c: $(objpfx)tst-audit24amod1.so \
		       $(objpfx)tst-audit24amod2.so
tst-audit24c-ENV = LD_BIND_NOW=1 LD_AUDIT=$(objpfx)tst-auditmod24c.so
LDFLAGS-tst-audit24c = -Wl,-z,lazy

$(objpfx)tst-audit24d.out: $(objpfx)tst-auditmod24d.so
$(objpfx)tst-audit24d: $(objpfx)tst-audit24dmod1.so \
		       $(objpfx)tst-audit24dmod2.so
$(objpfx)tst-audit24dmod1.so: $(objpfx)tst-audit24dmod3.so
LDFLAGS-tst-audit24dmod1.so = -Wl,-z,now
$(objpfx)tst-audit24dmod2.so: $(objpfx)tst-audit24dmod4.so
LDFLAGS-tst-audit24dmod2.so = -Wl,-z,lazy
tst-audit24d-ENV = LD_AUDIT=$(objpfx)tst-auditmod24d.so
LDFLAGS-tst-audit24d = -Wl,-z,lazy

$(objpfx)tst-audit25a.out: $(objpfx)tst-auditmod25.so
$(objpfx)tst-audit25a: $(objpfx)tst-audit25mod1.so \
		       $(objpfx)tst-audit25mod2.so \
		       $(objpfx)tst-audit25mod3.so \
		       $(objpfx)tst-audit25mod4.so
LDFLAGS-tst-audit25a = -Wl,-z,lazy
$(objpfx)tst-audit25mod1.so: $(objpfx)tst-audit25mod3.so
LDFLAGS-tst-audit25mod1.so = -Wl,-z,now
$(objpfx)tst-audit25mod2.so: $(objpfx)tst-audit25mod4.so
LDFLAGS-tst-audit25mod2.so = -Wl,-z,lazy
tst-audit25a-ARGS = -- $(host-test-program-cmd)

$(objpfx)tst-audit25b.out: $(objpfx)tst-auditmod25.so
$(objpfx)tst-audit25b: $(objpfx)tst-audit25mod1.so \
		       $(objpfx)tst-audit25mod2.so \
		       $(objpfx)tst-audit25mod3.so \
		       $(objpfx)tst-audit25mod4.so
LDFLAGS-tst-audit25b = -Wl,-z,now
tst-audit25b-ARGS = -- $(host-test-program-cmd)

$(objpfx)tst-audit28.out: $(objpfx)tst-auditmod28.so
$(objpfx)tst-auditmod28.so: $(libsupport)
tst-audit28-ENV = LD_AUDIT=$(objpfx)tst-auditmod28.so

# tst-sonamemove links against an older implementation of the library.
LDFLAGS-tst-sonamemove-linkmod1.so = \
  -Wl,--version-script=tst-sonamemove-linkmod1.map \
  -Wl,-soname,tst-sonamemove-runmod1.so
LDFLAGS-tst-sonamemove-runmod1.so = -Wl,--no-as-needed \
  -Wl,--version-script=tst-sonamemove-runmod1.map \
  -Wl,-soname,tst-sonamemove-runmod1.so
LDFLAGS-tst-sonamemove-runmod2.so = \
  -Wl,--version-script=tst-sonamemove-runmod2.map \
  -Wl,-soname,tst-sonamemove-runmod2.so
$(objpfx)tst-sonamemove-runmod1.so: $(objpfx)tst-sonamemove-runmod2.so
# Link against the link module, but depend on the run-time modules
# for execution.
$(objpfx)tst-sonamemove-link: $(objpfx)tst-sonamemove-linkmod1.so
$(objpfx)tst-sonamemove-link.out: \
  $(objpfx)tst-sonamemove-runmod1.so \
  $(objpfx)tst-sonamemove-runmod2.so
$(objpfx)tst-sonamemove-dlopen.out: \
  $(objpfx)tst-sonamemove-runmod1.so \
  $(objpfx)tst-sonamemove-runmod2.so

$(objpfx)tst-dlmopen-dlerror-mod.so: $(libsupport)
$(objpfx)tst-dlmopen-dlerror.out: $(objpfx)tst-dlmopen-dlerror-mod.so

# Override -z defs, so that we can reference an undefined symbol.
# Force lazy binding for the same reason.
LDFLAGS-tst-latepthreadmod.so = \
  -Wl,-z,lazy -Wl,--unresolved-symbols=ignore-all
# Do not optimize sibling calls as the test relies on a JMP_SLOT relocation for
# function this_function_is_not_defined.
CFLAGS-tst-latepthreadmod.c += -fno-optimize-sibling-calls
$(objpfx)tst-latepthreadmod.so: $(shared-thread-library)
$(objpfx)tst-latepthread.out: $(objpfx)tst-latepthreadmod.so

# The test modules are parameterized by preprocessor macros.
$(patsubst %,$(objpfx)%.os,$(tst-tls-many-dynamic-modules)): \
  $(objpfx)tst-tls-manydynamic%mod.os : tst-tls-manydynamicmod.c
	$(compile-command.c) \
	  -DNAME=tls_global_$* -DSETTER=set_value_$* -DGETTER=get_value_$*
$(objpfx)tst-tls-manydynamic: $(shared-thread-library)
$(objpfx)tst-tls-manydynamic.out: \
  $(patsubst %,$(objpfx)%.so,$(tst-tls-many-dynamic-modules))

$(objpfx)tst-ldconfig-X.out : tst-ldconfig-X.sh $(objpfx)ldconfig
	$(SHELL) $< '$(common-objpfx)' '$(test-wrapper-env)' \
		 '$(run-program-env)' > $@; \
	$(evaluate-test)

$(objpfx)tst-ldconfig-p.out : tst-ldconfig-p.sh $(objpfx)ldconfig
	$(SHELL) $< '$(common-objpfx)' '$(sysconfdir)' '$(test-wrapper-env)' \
		 '$(run-program-env)' > $@; \
	$(evaluate-test)

LDFLAGS-tst-ldconfig-soname-lib-with-soname.so = \
  -Wl,-soname,libtst-ldconfig-soname-lib-with-soname.so.1

$(objpfx)tst-ldconfig-soname.out : tst-ldconfig-soname.sh \
  $(objpfx)ldconfig \
  $(objpfx)tst-ldconfig-soname-lib-with-soname.so \
  $(objpfx)tst-ldconfig-soname-lib-without-soname.so
	$(SHELL) $< '$(common-objpfx)' '$(test-wrapper-env)' \
		 '$(run-program-env)' > $@; \
	$(evaluate-test)

# Test static linking of all the libraries we can possibly link
# together.  Note that in some configurations this may be less than the
# complete list of libraries we build but we try to maxmimize this list.
ifeq ($(pthread-in-libc),no)
$(objpfx)tst-linkall-static: \
  $(common-objpfx)resolv/libanl.a
endif
$(objpfx)tst-linkall-static: \
  $(common-objpfx)math/libm.a \
  $(common-objpfx)resolv/libresolv.a \
  $(common-objpfx)login/libutil.a \
  $(common-objpfx)rt/librt.a \
  $(static-thread-library)

LDFLAGS-nextmod3.so = -Wl,--version-script=nextmod3.map

# The application depends on the DSO, and the DSO loads the plugin.
# The plugin also depends on the DSO. This creates the circular
# dependency via dlopen that we're testing to make sure works.
$(objpfx)tst-nodelete-dlclose-plugin.so: $(objpfx)tst-nodelete-dlclose-dso.so
$(objpfx)tst-nodelete-dlclose: $(objpfx)tst-nodelete-dlclose-dso.so
$(objpfx)tst-nodelete-dlclose.out: $(objpfx)tst-nodelete-dlclose-dso.so \
				   $(objpfx)tst-nodelete-dlclose-plugin.so

$(objpfx)tst-debug1.out: $(objpfx)tst-debug1mod1.so

$(objpfx)tst-debug1mod1.so: $(objpfx)testobj1.so
	$(OBJCOPY) --only-keep-debug $< $@

$(objpfx)tst-main1: $(objpfx)tst-main1mod.so
CRT-tst-main1 := $(csu-objpfx)crt1.o
tst-main1-no-pie = yes
LDLIBS-tst-main1 = $(libsupport)
tst-main1mod.so-no-z-defs = yes

LDLIBS-tst-absolute-sym-lib.so = tst-absolute-sym-lib.lds
$(objpfx)tst-absolute-sym-lib.so: $(LDLIBS-tst-absolute-sym-lib.so)
$(objpfx)tst-absolute-sym: $(objpfx)tst-absolute-sym-lib.so

LDLIBS-tst-absolute-zero-lib.so = tst-absolute-zero-lib.lds
$(objpfx)tst-absolute-zero-lib.so: $(LDLIBS-tst-absolute-zero-lib.so)
$(objpfx)tst-absolute-zero: $(objpfx)tst-absolute-zero-lib.so

$(objpfx)tst-big-note: $(objpfx)tst-big-note-lib.so
# Avoid creating an ABI tag note, which may come before the
# artificial, large note in tst-big-note-lib.o and invalidate the
# test.
$(objpfx)tst-big-note-lib.so: $(objpfx)tst-big-note-lib.o
	$(LINK.o) -shared -o $@ $(LDFLAGS.so) $(dt-relr-ldflag) $<

$(objpfx)tst-unwind-ctor: $(objpfx)tst-unwind-ctor-lib.so

CFLAGS-tst-unwind-main.c += -funwind-tables -DUSE_PTHREADS=0

$(objpfx)tst-initfinilazyfail.out: \
  $(objpfx)tst-initlazyfailmod.so $(objpfx)tst-finilazyfailmod.so
# Override -z defs, so that we can reference an undefined symbol.
# Force lazy binding for the same reason.
LDFLAGS-tst-initlazyfailmod.so = \
  -Wl,-z,lazy -Wl,--unresolved-symbols=ignore-all
LDFLAGS-tst-finilazyfailmod.so = \
  -Wl,-z,lazy -Wl,--unresolved-symbols=ignore-all

$(objpfx)tst-dlopenfail.out: \
  $(objpfx)tst-dlopenfailmod1.so $(objpfx)tst-dlopenfailmod2.so
# Order matters here.  tst-dlopenfaillinkmod.so's soname ensures a
# run-time loader failure.  --as-needed breaks this test because
# nothing actually references tst-dlopenfailmod2.so (with its soname
# tst-dlopenfail-missingmod.so).
LDFLAGS-tst-dlopenfailmod1.so = -Wl,--no-as-needed
$(objpfx)tst-dlopenfailmod1.so: \
  $(shared-thread-library) $(objpfx)tst-dlopenfaillinkmod.so
LDFLAGS-tst-dlopenfaillinkmod.so = -Wl,-soname,tst-dlopenfail-missingmod.so
$(objpfx)tst-dlopenfailmod2.so: $(objpfx)tst-dlopenfailnodelmod.so
$(objpfx)tst-dlopenfail-2.out: \
  $(objpfx)tst-dlopenfailmod1.so $(objpfx)tst-dlopenfailmod2.so \
  $(objpfx)tst-dlopenfailmod3.so
# tst-dlopenfailnodelmod.so emulates how libpthread was linked.
$(objpfx)tst-dlopenfailnodelmod.so: $(libsupport)
LDFLAGS-tst-dlopenfailnodelmod.so = \
  -Wl,--enable-new-dtags,-z,nodelete,-z,initfirst
# tst-dlopenfail should export the libsupport symbols, so that
# tst-dlopenfailnodelmod.so uses them for error reporting.
LDFLAGS-tst-dlopenfail = -Wl,-E

$(objpfx)tst-dlopen-nodelete-reloc.out: \
  $(objpfx)tst-dlopen-nodelete-reloc-mod1.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod2.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod3.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod4.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod5.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod6.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod7.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod8.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod9.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod10.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod11.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod12.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod13.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod14.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod15.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod16.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod17.so
tst-dlopen-nodelete-reloc-mod2.so-no-z-defs = yes
LDFLAGS-tst-dlopen-nodelete-reloc-mod2.so = -Wl,-z,nodelete
$(objpfx)tst-dlopen-nodelete-reloc-mod4.so: \
  $(objpfx)tst-dlopen-nodelete-reloc-mod3.so
LDFLAGS-tst-dlopen-nodelete-reloc-mod4.so = -Wl,--no-as-needed
$(objpfx)tst-dlopen-nodelete-reloc-mod5.so: \
  $(objpfx)tst-dlopen-nodelete-reloc-mod4.so
LDFLAGS-tst-dlopen-nodelete-reloc-mod5.so = -Wl,-z,nodelete,--no-as-needed
tst-dlopen-nodelete-reloc-mod5.so-no-z-defs = yes
tst-dlopen-nodelete-reloc-mod7.so-no-z-defs = yes
tst-dlopen-nodelete-reloc-mod11.so-no-z-defs = yes
$(objpfx)tst-dlopen-nodelete-reloc-mod13.so: \
  $(objpfx)tst-dlopen-nodelete-reloc-mod12.so
$(objpfx)tst-dlopen-nodelete-reloc-mod15.so: \
  $(objpfx)tst-dlopen-nodelete-reloc-mod14.so
tst-dlopen-nodelete-reloc-mod16.so-no-z-defs = yes
$(objpfx)tst-dlopen-nodelete-reloc-mod16.so: \
  $(objpfx)tst-dlopen-nodelete-reloc-mod15.so
LDFLAGS-tst-dlopen-nodelete-reloc-mod16.so = -Wl,--no-as-needed
$(objpfx)tst-dlopen-nodelete-reloc-mod17.so: \
  $(objpfx)tst-dlopen-nodelete-reloc-mod15.so \
  $(objpfx)tst-dlopen-nodelete-reloc-mod16.so
LDFLAGS-tst-dlopen-nodelete-reloc-mod17.so = -Wl,--no-as-needed

$(objpfx)tst-ldconfig-ld_so_conf-update.out: $(objpfx)tst-ldconfig-ld-mod.so

LDFLAGS-tst-filterobj-flt.so = -Wl,--filter=$(objpfx)tst-filterobj-filtee.so
$(objpfx)tst-filterobj: $(objpfx)tst-filterobj-flt.so
$(objpfx)tst-filterobj.out: $(objpfx)tst-filterobj-filtee.so
$(objpfx)tst-filterobj-dlopen.out: $(objpfx)tst-filterobj-filtee.so

LDFLAGS-tst-filterobj-aux.so = -Wl,--auxiliary=$(objpfx)tst-filterobj-filtee.so
$(objpfx)tst-auxobj: $(objpfx)tst-filterobj-aux.so
$(objpfx)tst-auxobj.out: $(objpfx)tst-filterobj-filtee.so
$(objpfx)tst-auxobj-dlopen.out: $(objpfx)tst-filterobj-filtee.so

$(objpfx)tst-single_threaded: $(objpfx)tst-single_threaded-mod1.so
$(objpfx)tst-single_threaded.out: \
  $(objpfx)tst-single_threaded-mod2.so $(objpfx)tst-single_threaded-mod3.so
$(objpfx)tst-single_threaded-static-dlopen: \
  $(objpfx)tst-single_threaded-mod1.o
$(objpfx)tst-single_threaded-static-dlopen.out: \
  $(objpfx)tst-single_threaded-mod2.so
$(objpfx)tst-single_threaded-pthread: \
  $(objpfx)tst-single_threaded-mod1.so $(shared-thread-library)
$(objpfx)tst-single_threaded-pthread.out: \
  $(objpfx)tst-single_threaded-mod2.so $(objpfx)tst-single_threaded-mod3.so \
  $(objpfx)tst-single_threaded-mod4.so
$(objpfx)tst-single_threaded-pthread-static: $(static-thread-library)

$(objpfx)tst-tls-ie: $(shared-thread-library)
$(objpfx)tst-tls-ie.out: \
  $(objpfx)tst-tls-ie-mod0.so \
  $(objpfx)tst-tls-ie-mod1.so \
  $(objpfx)tst-tls-ie-mod2.so \
  $(objpfx)tst-tls-ie-mod3.so \
  $(objpfx)tst-tls-ie-mod4.so \
  $(objpfx)tst-tls-ie-mod5.so \
  $(objpfx)tst-tls-ie-mod6.so

$(objpfx)tst-tls-ie-dlmopen: $(shared-thread-library)
$(objpfx)tst-tls-ie-dlmopen.out: \
  $(objpfx)tst-tls-ie-mod0.so \
  $(objpfx)tst-tls-ie-mod1.so \
  $(objpfx)tst-tls-ie-mod2.so \
  $(objpfx)tst-tls-ie-mod3.so \
  $(objpfx)tst-tls-ie-mod4.so \
  $(objpfx)tst-tls-ie-mod5.so \
  $(objpfx)tst-tls-ie-mod6.so

$(objpfx)argv0test.out: tst-rtld-argv0.sh $(objpfx)ld.so \
			$(objpfx)argv0test
	$(SHELL) $< $(objpfx)ld.so $(objpfx)argv0test \
            '$(test-wrapper-env)' '$(run_program_env)' \
            '$(rpath-link)' 'test-argv0' > $@; \
    $(evaluate-test)

# A list containing the name of the most likely searched subdirectory
# of the glibc-hwcaps directory, for each supported architecture (in
# other words, the oldest hardware level recognized by the
# glibc-hwcaps mechanism for this architecture).  Used to obtain test
# coverage for some glibc-hwcaps tests for the widest possible range
# of systems.
glibc-hwcaps-first-subdirs-for-tests = power9 x86-64-v2 z13

# The test modules are parameterized by preprocessor macros.
LDFLAGS-libmarkermod1-1.so += -Wl,-soname,libmarkermod1.so
LDFLAGS-libmarkermod2-1.so += -Wl,-soname,libmarkermod2.so
LDFLAGS-libmarkermod3-1.so += -Wl,-soname,libmarkermod3.so
LDFLAGS-libmarkermod4-1.so += -Wl,-soname,libmarkermod4.so
LDFLAGS-libmarkermod5-1.so += -Wl,-soname,libmarkermod5.so
$(objpfx)libmarkermod%.os : markermodMARKER-VALUE.c
	$(compile-command.c) \
	  -DMARKER=marker$(firstword $(subst -, ,$*)) \
	  -DVALUE=$(lastword $(subst -, ,$*))
$(objpfx)libmarkermod1.so: $(objpfx)libmarkermod1-1.so
	cp $< $@
$(objpfx)libmarkermod2.so: $(objpfx)libmarkermod2-1.so
	cp $< $@
$(objpfx)libmarkermod3.so: $(objpfx)libmarkermod3-1.so
	cp $< $@
$(objpfx)libmarkermod4.so: $(objpfx)libmarkermod4-1.so
	cp $< $@
$(objpfx)libmarkermod5.so: $(objpfx)libmarkermod5-1.so
	cp $< $@

# tst-glibc-hwcaps-prepend checks that --glibc-hwcaps-prepend is
# preferred over auto-detected subdirectories.
$(objpfx)tst-glibc-hwcaps-prepend: $(objpfx)libmarkermod1-1.so
$(objpfx)glibc-hwcaps/prepend-markermod1/libmarkermod1.so: \
  $(objpfx)libmarkermod1-2.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/%/libmarkermod1.so: $(objpfx)libmarkermod1-3.so
	$(make-target-directory)
	cp $< $@
$(objpfx)tst-glibc-hwcaps-prepend.out: \
  $(objpfx)tst-glibc-hwcaps-prepend $(objpfx)libmarkermod1.so \
  $(patsubst %,$(objpfx)glibc-hwcaps/%/libmarkermod1.so,prepend-markermod1 \
    $(glibc-hwcaps-first-subdirs-for-tests))
	$(test-wrapper) $(rtld-prefix) \
	  --glibc-hwcaps-prepend prepend-markermod1 \
	  $< > $@; \
	$(evaluate-test)

# Like tst-glibc-hwcaps-prepend, but uses a container and loads the
# library via ld.so.cache.  Test setup is contained in the test
# itself.
$(objpfx)tst-glibc-hwcaps-prepend-cache.out: \
  $(objpfx)tst-glibc-hwcaps-prepend-cache $(objpfx)libmarkermod1-1.so \
  $(objpfx)libmarkermod1-2.so $(objpfx)libmarkermod1-3.so

# tst-glibc-hwcaps-mask checks that --glibc-hwcaps-mask can be used to
# suppress all auto-detected subdirectories.
$(objpfx)tst-glibc-hwcaps-mask: $(objpfx)libmarkermod1-1.so
$(objpfx)tst-glibc-hwcaps-mask.out: \
  $(objpfx)tst-glibc-hwcaps-mask $(objpfx)libmarkermod1.so \
  $(patsubst %,$(objpfx)glibc-hwcaps/%/libmarkermod1.so,\
    $(glibc-hwcaps-first-subdirs-for-tests))
	$(test-wrapper) $(rtld-prefix) \
	  --glibc-hwcaps-mask does-not-exist \
	  $< > $@; \
	$(evaluate-test)

# Generic dependency for sysdeps implementation of
# tst-glibc-hwcaps-cache.
$(objpfx)tst-glibc-hwcaps-cache.out: $(objpfx)tst-glibc-hwcaps

tst-tunables-ARGS = -- $(host-test-program-cmd)
tst-tunables-enable_secure-ARGS = -- $(host-test-program-cmd)

$(objpfx)list-tunables.out: tst-rtld-list-tunables.sh $(objpfx)ld.so
	$(SHELL) $< $(objpfx)ld.so '$(test-wrapper-env)' \
	    '$(run_program_env)' > $(objpfx)/tst-rtld-list-tunables.out
	cmp tst-rtld-list-tunables.exp \
	    $(objpfx)/tst-rtld-list-tunables.out > $@; \
	$(evaluate-test)

tst-dst-static-ENV = LD_LIBRARY_PATH='$$ORIGIN'

$(objpfx)tst-rtld-help.out: $(objpfx)ld.so
	$(test-wrapper) $(rtld-prefix) --help > $@; \
	status=$$?; \
	echo "info: ld.so exit status: $$status" >> $@; \
	(exit $$status); \
	$(evaluate-test)

# Reuses tst-tls-many-dynamic-modules
$(patsubst %,$(objpfx)%.os,$(tst-tls-many-dynamic-modules-dep)): \
  $(objpfx)tst-tls-manydynamic%mod-dep.os : tst-tls-manydynamicmod.c
	$(compile-command.c) \
	  -DNAME=tls_global_$* -DSETTER=set_value_$* -DGETTER=get_value_$*
$(patsubst %,$(objpfx)%.os,$(tst-tls-many-dynamic-modules-dep-bad)): \
  $(objpfx)tst-tls-manydynamic%mod-dep-bad.os : tst-tls-manydynamicmod.c
	$(compile-command.c) \
	  -DNAME=tls_global_$* -DSETTER=set_value_$* -DGETTER=get_value_$*
tst-tls20mod-bad.so-no-z-defs = yes
# Single dependency.
$(objpfx)tst-tls-manydynamic0mod-dep.so: $(objpfx)tst-tls-manydynamic1mod-dep.so
# Double dependencies.
$(objpfx)tst-tls-manydynamic2mod-dep.so: $(objpfx)tst-tls-manydynamic3mod-dep.so \
					 $(objpfx)tst-tls-manydynamic4mod-dep.so
# Double dependencies with each dependency dependent of another module.
$(objpfx)tst-tls-manydynamic5mod-dep.so: $(objpfx)tst-tls-manydynamic6mod-dep.so \
					 $(objpfx)tst-tls-manydynamic7mod-dep.so
$(objpfx)tst-tls-manydynamic6mod-dep.so: $(objpfx)tst-tls-manydynamic8mod-dep.so
$(objpfx)tst-tls-manydynamic7mod-dep.so: $(objpfx)tst-tls-manydynamic8mod-dep.so
# Long chain with one double dependency in the middle
$(objpfx)tst-tls-manydynamic9mod-dep.so: $(objpfx)tst-tls-manydynamic10mod-dep.so \
					 $(objpfx)tst-tls-manydynamic11mod-dep.so
$(objpfx)tst-tls-manydynamic10mod-dep.so: $(objpfx)tst-tls-manydynamic12mod-dep.so
$(objpfx)tst-tls-manydynamic12mod-dep.so: $(objpfx)tst-tls-manydynamic13mod-dep.so
# Long chain with two double dependencies in the middle
$(objpfx)tst-tls-manydynamic14mod-dep.so: $(objpfx)tst-tls-manydynamic15mod-dep.so
$(objpfx)tst-tls-manydynamic15mod-dep.so: $(objpfx)tst-tls-manydynamic16mod-dep.so \
					  $(objpfx)tst-tls-manydynamic17mod-dep.so
$(objpfx)tst-tls-manydynamic16mod-dep.so: $(objpfx)tst-tls-manydynamic18mod-dep.so \
					  $(objpfx)tst-tls-manydynamic19mod-dep.so
# Same but with an invalid module.
# Single dependency.
$(objpfx)tst-tls-manydynamic0mod-dep-bad.so: $(objpfx)tst-tls20mod-bad.so
LDFLAGS-tst-tls-manydynamic0mod-dep-bad.so = -Wl,--no-as-needed
# Double dependencies.
$(objpfx)tst-tls-manydynamic1mod-dep-bad.so: $(objpfx)tst-tls-manydynamic2mod-dep-bad.so \
					     $(objpfx)tst-tls20mod-bad.so
LDFLAGS-tst-tls-manydynamic1mod-dep-bad.so = -Wl,--no-as-needed
# Double dependencies with each dependency dependent of another module.
$(objpfx)tst-tls-manydynamic3mod-dep-bad.so: $(objpfx)tst-tls-manydynamic4mod-dep-bad.so \
					     $(objpfx)tst-tls-manydynamic5mod-dep-bad.so
LDFLAGS-tst-tls-manydynamic3mod-dep-bad.so = -Wl,--no-as-needed
$(objpfx)tst-tls-manydynamic4mod-dep-bad.so: $(objpfx)tst-tls20mod-bad.so
LDFLAGS-tst-tls-manydynamic4mod-dep-bad.so = -Wl,--no-as-needed
$(objpfx)tst-tls-manydynamic5mod-dep-bad.so: $(objpfx)tst-tls20mod-bad.so
LDFLAGS-tst-tls-manydynamic5mod-dep-bad.so = -Wl,--no-as-needed
# Long chain with one double dependency in the middle
$(objpfx)tst-tls-manydynamic6mod-dep-bad.so: $(objpfx)tst-tls-manydynamic7mod-dep-bad.so \
					     $(objpfx)tst-tls-manydynamic8mod-dep-bad.so
LDFLAGS-tst-tls-manydynamic6mod-dep-bad.so = -Wl,--no-as-needed
$(objpfx)tst-tls-manydynamic7mod-dep-bad.so: $(objpfx)tst-tls-manydynamic9mod-dep-bad.so
LDFLAGS-tst-tls-manydynamic7mod-dep-bad.so = -Wl,--no-as-needed
$(objpfx)tst-tls-manydynamic9mod-dep-bad.so: $(objpfx)tst-tls20mod-bad.so
LDFLAGS-tst-tls-manydynamic9mod-dep-bad.so = -Wl,--no-as-needed
# Long chain with two double dependencies in the middle
$(objpfx)tst-tls-manydynamic10mod-dep-bad.so: $(objpfx)tst-tls-manydynamic11mod-dep-bad.so
LDFLAGS-tst-tls-manydynamic10mod-dep-bad.so = -Wl,--no-as-needed
$(objpfx)tst-tls-manydynamic11mod-dep-bad.so: $(objpfx)tst-tls-manydynamic12mod-dep-bad.so \
					      $(objpfx)tst-tls-manydynamic13mod-dep-bad.so
LDFLAGS-tst-tls-manydynamic11mod-dep-bad.so = -Wl,--no-as-needed
$(objpfx)tst-tls-manydynamic12mod-dep-bad.so: $(objpfx)tst-tls-manydynamic14mod-dep-bad.so \
					      $(objpfx)tst-tls20mod-bad.so
LDFLAGS-tst-tls-manydynamic12mod-dep-bad.so = -Wl,--no-as-needed
$(objpfx)tst-tls20: $(shared-thread-library)
$(objpfx)tst-tls20.out: $(objpfx)tst-tls20mod-bad.so \
			$(tst-tls-many-dynamic-modules:%=$(objpfx)%.so) \
			$(tst-tls-many-dynamic-modules-dep:%=$(objpfx)%.so) \
			$(tst-tls-many-dynamic-modules-dep-bad:%=$(objpfx)%.so) \

# Reuses tst-tls-many-dynamic-modules
$(objpfx)tst-tls21: $(shared-thread-library)
$(objpfx)tst-tls21.out: $(objpfx)tst-tls21mod.so
$(objpfx)tst-tls21mod.so: $(tst-tls-many-dynamic-modules:%=$(objpfx)%.so)

$(objpfx)tst-getauxval-static.out: $(objpfx)tst-auxvalmod.so
tst-getauxval-static-ENV = LD_LIBRARY_PATH=$(objpfx):$(common-objpfx)

$(objpfx)tst-dlmopen-gethostbyname.out: $(objpfx)tst-dlmopen-gethostbyname-mod.so

$(objpfx)tst-ro-dynamic: $(objpfx)tst-ro-dynamic-mod.so
$(objpfx)tst-ro-dynamic-mod.so: $(objpfx)tst-ro-dynamic-mod.os \
  tst-ro-dynamic-mod.map
	$(LINK.o) -nostdlib -nostartfiles -shared -o $@ \
		$(dt-relr-ldflag) \
		-Wl,--script=tst-ro-dynamic-mod.map \
		$(objpfx)tst-ro-dynamic-mod.os

$(objpfx)tst-rtld-list-diagnostics.out: tst-rtld-list-diagnostics.py \
  $(..)manual/dynlink.texi $(objpfx)$(rtld-installed-name)
	$(PYTHON) tst-rtld-list-diagnostics.py \
	  --manual=$(..)manual/dynlink.texi \
	  "$(test-wrapper-env) $(objpfx)$(rtld-installed-name) --list-diagnostics" \
	  > $@; \
	$(evaluate-test)

$(objpfx)tst-rtld-run-static.out: $(objpfx)/ldconfig

$(objpfx)tst-dl_find_object.out: \
  $(objpfx)tst-dl_find_object-mod1.so $(objpfx)tst-dl_find_object-mod2.so
$(objpfx)tst-dl_find_object-static.out: \
  $(objpfx)tst-dl_find_object-mod1.so $(objpfx)tst-dl_find_object-mod2.so
tst-dl_find_object-static-ENV = $(static-dlopen-environment)
CFLAGS-tst-dl_find_object.c += -funwind-tables
CFLAGS-tst-dl_find_object-static.c += -funwind-tables
LDFLAGS-tst-dl_find_object-static += -Wl,--eh-frame-hdr
CFLAGS-tst-dl_find_object-mod1.c += -funwind-tables
CFLAGS-tst-dl_find_object-mod2.c += -funwind-tables
LDFLAGS-tst-dl_find_object-mod2.so += -Wl,--enable-new-dtags,-z,nodelete
$(objpfx)tst-dl_find_object-threads: $(shared-thread-library)
CFLAGS-tst-dl_find_object-threads.c += -funwind-tables
$(objpfx)tst-dl_find_object-threads.out: \
  $(objpfx)tst-dl_find_object-mod1.so \
  $(objpfx)tst-dl_find_object-mod2.so \
  $(objpfx)tst-dl_find_object-mod3.so \
  $(objpfx)tst-dl_find_object-mod4.so \
  $(objpfx)tst-dl_find_object-mod5.so \
  $(objpfx)tst-dl_find_object-mod6.so \
  $(objpfx)tst-dl_find_object-mod7.so \
  $(objpfx)tst-dl_find_object-mod8.so \
  $(objpfx)tst-dl_find_object-mod9.so
CFLAGS-tst-dl_find_object-mod3.c += -funwind-tables
CFLAGS-tst-dl_find_object-mod4.c += -funwind-tables
CFLAGS-tst-dl_find_object-mod5.c += -funwind-tables
CFLAGS-tst-dl_find_object-mod6.c += -funwind-tables
CFLAGS-tst-dl_find_object-mod7.c += -funwind-tables
CFLAGS-tst-dl_find_object-mod8.c += -funwind-tables
CFLAGS-tst-dl_find_object-mod9.c += -funwind-tables

$(objpfx)tst-p_alignmod-base.so: $(libsupport)
LDFLAGS-tst-p_alignmod-base.so += -Wl,-z,max-page-size=0x200000

$(objpfx)tst-p_align1: $(objpfx)tst-p_alignmod1.so

# Make a copy of tst-p_alignmod-base.so and lower p_align of the first
# PT_LOAD segment.
$(objpfx)tst-p_alignmod1.so: $(objpfx)tst-p_alignmod-base.so
	rm -f $@
	cp $(objpfx)tst-p_alignmod-base.so $@
	$(PYTHON) $(..)scripts/tst-elf-edit.py -a half $@

$(objpfx)tst-p_align2: $(objpfx)tst-p_alignmod2.so

# Make a copy of tst-p_alignmod-base.so and update p_align of the first
# PT_LOAD segment.
$(objpfx)tst-p_alignmod2.so: $(objpfx)tst-p_alignmod-base.so
	rm -f $@
	cp $(objpfx)tst-p_alignmod-base.so $@
	$(PYTHON) $(..)scripts/tst-elf-edit.py -a 1 $@

LDFLAGS-tst-p_alignmod3.so += -Wl,-z,max-page-size=0x100,-z,common-page-size=0x100

$(objpfx)tst-p_align3: $(objpfx)tst-p_alignmod3.so
$(objpfx)tst-p_align3.out: tst-p_align3.sh $(objpfx)tst-p_align3
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix)'; \
	$(evaluate-test)

$(objpfx)check-abi-version-libc.out: $(common-objpfx)libc.so
	LC_ALL=C $(READELF) -V -W $< \
		| sed -ne '/.gnu.version_d/, /.gnu.version_r/ p' \
		| grep GLIBC_ABI_DT_RELR > $@; \
	$(evaluate-test)

$(objpfx)check-tst-relr-pie.out: $(objpfx)tst-relr-pie
	LC_ALL=C $(OBJDUMP) -p $< \
		| sed -ne '/required from libc.so/,$$ p' \
		| grep GLIBC_ABI_DT_RELR > $@; \
	$(evaluate-test)

# The test checks if a DT_RELR shared library without DT_NEEDED works as
# intended, so it uses an explicit link rule.
$(objpfx)tst-relr2: $(objpfx)tst-relr-mod2.so
$(objpfx)tst-relr-mod2.so: $(objpfx)tst-relr-mod2.os
	$(LINK.o) -nostdlib -nostartfiles -Wl,-z,pack-relative-relocs \
	$(LDFLAGS-soname-fname) \
	-shared -o $@.new $(filter-out $(map-file),$^)
	$(call after-link,$@.new)
	mv -f $@.new $@

# The test checks if a DT_RELR shared library without DT_VERNEED works as
# intended, so it uses an explicit link rule.
$(objpfx)tst-relr3: $(objpfx)tst-relr-mod3a.so
$(objpfx)tst-relr-mod3b.so: $(objpfx)tst-relr-mod3b.os
	$(LINK.o) -nostdlib -nostartfiles -Wl,-z,pack-relative-relocs \
	$(LDFLAGS-soname-fname) \
	-shared -o $@.new $(filter-out $(map-file),$^)
	$(call after-link,$@.new)
	mv -f $@.new $@

$(objpfx)tst-relr-mod3a.so: $(objpfx)tst-relr-mod3a.os \
  $(objpfx)tst-relr-mod3b.so
	$(LINK.o) -nostdlib -nostartfiles -Wl,-z,pack-relative-relocs \
	$(LDFLAGS-soname-fname) $(LDFLAGS-rpath-ORIGIN) \
	-shared -o $@.new $(filter-out $(map-file),$^)
	$(call after-link,$@.new)
	mv -f $@.new $@

# The test checks if a DT_RELR shared library without libc.so on DT_NEEDED
# works as intended, so it uses an explicit link rule.
$(objpfx)tst-relr4: $(objpfx)tst-relr-mod4a.so
$(objpfx)tst-relr-mod4b.so: $(objpfx)tst-relr-mod4b.os
	$(LINK.o) -nostdlib -nostartfiles -Wl,-z,pack-relative-relocs \
	$(LDFLAGS-soname-fname) \
	-Wl,--version-script=tst-relr-mod4b.map \
	-shared -o $@.new $(filter-out $(map-file),$^)
	$(call after-link,$@.new)
	mv -f $@.new $@

$(objpfx)tst-relr-mod4a.so: $(objpfx)tst-relr-mod4a.os \
  $(objpfx)tst-relr-mod4b.so
	$(LINK.o) -nostdlib -nostartfiles -Wl,-z,pack-relative-relocs \
	$(LDFLAGS-soname-fname) $(LDFLAGS-rpath-ORIGIN) \
	-shared -o $@.new $(filter-out $(map-file),$^)
	$(call after-link,$@.new)
	mv -f $@.new $@

LDFLAGS-libtracemod1-1.so += -Wl,-soname,libtracemod1.so
LDFLAGS-libtracemod2-1.so += -Wl,-soname,libtracemod2.so
LDFLAGS-libtracemod3-1.so += -Wl,-soname,libtracemod3.so
LDFLAGS-libtracemod4-1.so += -Wl,-soname,libtracemod4.so
LDFLAGS-libtracemod5-1.so += -Wl,-soname,libtracemod5.so

$(objpfx)libtracemod1-1.so: $(objpfx)libtracemod2-1.so \
			    $(objpfx)libtracemod3-1.so
$(objpfx)libtracemod2-1.so: $(objpfx)libtracemod4-1.so \
			    $(objpfx)libtracemod5-1.so

define libtracemod-x
$(objpfx)libtracemod$(1)/libtracemod$(1).so: $(objpfx)libtracemod$(1)-1.so
	$$(make-target-directory)
	cp $$< $$@
endef
libtracemod-suffixes = 1 2 3 4 5
$(foreach i,$(libtracemod-suffixes), $(eval $(call libtracemod-x,$(i))))

define tst-trace-skeleton
$(objpfx)tst-trace$(1).out: $(objpfx)libtracemod1/libtracemod1.so \
			    $(objpfx)libtracemod2/libtracemod2.so \
			    $(objpfx)libtracemod3/libtracemod3.so \
			    $(objpfx)libtracemod4/libtracemod4.so \
			    $(objpfx)libtracemod5/libtracemod5.so \
			    $(..)scripts/tst-ld-trace.py \
			    tst-trace$(1).exp
	${ $(PYTHON) $(..)scripts/tst-ld-trace.py \
	    "$(test-wrapper-env) $(elf-objpfx)$(rtld-installed-name) \
	    --library-path $(common-objpfx):$(strip $(2)) \
	    $(objpfx)libtracemod1/libtracemod1.so" tst-trace$(1).exp \
	} > $$@; $$(evaluate-test)
endef

$(eval $(call tst-trace-skeleton,1,))
$(eval $(call tst-trace-skeleton,2,\
	$(objpfx)libtracemod2))
$(eval $(call tst-trace-skeleton,3,\
	$(objpfx)libtracemod2:$(objpfx)libtracemod3))
$(eval $(call tst-trace-skeleton,4,\
	$(objpfx)libtracemod2:$(objpfx)libtracemod3:$(objpfx)libtracemod4))
$(eval $(call tst-trace-skeleton,5,\
	$(objpfx)libtracemod2:$(objpfx)libtracemod3:$(objpfx)libtracemod4:$(objpfx)libtracemod5))

$(objpfx)tst-tls-allocation-failure-static-patched: \
  $(objpfx)tst-tls-allocation-failure-static $(..)scripts/tst-elf-edit.py
	cp $< $@
	$(PYTHON) $(..)scripts/tst-elf-edit.py --maximize-tls-size $@

$(objpfx)tst-tls-allocation-failure-static-patched.out: \
  $(objpfx)tst-tls-allocation-failure-static-patched
	$(test-wrapper) $< > $@ 2>&1; echo "status: $$?" >> $@
	grep -q '^Fatal glibc error: Cannot allocate TLS block$$' $@ \
	  && grep -q '^status: 127$$' $@; \
	  $(evaluate-test)

$(objpfx)tst-audit-tlsdesc: $(objpfx)tst-audit-tlsdesc-mod1.so \
			    $(objpfx)tst-audit-tlsdesc-mod2.so \
			    $(shared-thread-library)
ifneq (no,$(have-mtls-descriptor))
# The test is valid for all TLS types, but we want to exercise GNU2
# TLS if possible.
CFLAGS-tst-audit-tlsdesc-mod1.c += -mtls-dialect=$(have-mtls-descriptor)
CFLAGS-tst-audit-tlsdesc-mod2.c += -mtls-dialect=$(have-mtls-descriptor)
endif
$(objpfx)tst-audit-tlsdesc-dlopen: $(shared-thread-library)
$(objpfx)tst-audit-tlsdesc-dlopen.out: $(objpfx)tst-audit-tlsdesc-mod1.so \
				       $(objpfx)tst-audit-tlsdesc-mod2.so
$(objpfx)tst-audit-tlsdesc-mod1.so: $(objpfx)tst-audit-tlsdesc-mod2.so
$(objpfx)tst-audit-tlsdesc.out: $(objpfx)tst-auditmod-tlsdesc.so
tst-audit-tlsdesc-ENV = LD_AUDIT=$(objpfx)tst-auditmod-tlsdesc.so
$(objpfx)tst-audit-tlsdesc-dlopen.out: $(objpfx)tst-auditmod-tlsdesc.so
tst-audit-tlsdesc-dlopen-ENV = LD_AUDIT=$(objpfx)tst-auditmod-tlsdesc.so

$(objpfx)tst-dlmopen-twice.out: \
  $(objpfx)tst-dlmopen-twice-mod1.so \
  $(objpfx)tst-dlmopen-twice-mod2.so

LDFLAGS-tst-sprof-mod.so = -Wl,-soname,tst-sprof-mod.so
$(objpfx)tst-sprof-basic: $(objpfx)tst-sprof-mod.so
$(objpfx)tst-sprof-basic.out: tst-sprof-basic.sh $(objpfx)tst-sprof-basic
	$(SHELL) $< $(common-objpfx) '$(test-wrapper-env)' \
		 '$(run-program-env)' > $@; \
	$(evaluate-test)
generated += tst-sprof-mod.so.profile

LDFLAGS-tst-dlclose-lazy-mod1.so = -Wl,-z,lazy,--no-as-needed
$(objpfx)tst-dlclose-lazy-mod1.so: $(objpfx)tst-dlclose-lazy-mod2.so
$(objpfx)tst-dlclose-lazy.out: \
  $(objpfx)tst-dlclose-lazy-mod1.so $(objpfx)tst-dlclose-lazy-mod2.so

$(objpfx)tst-decorate-maps: $(shared-thread-library)

tst-decorate-maps-ENV = \
  GLIBC_TUNABLES=glibc.malloc.arena_max=8:glibc.malloc.mmap_threshold=1024:glibc.mem.decorate_maps=1
tst-decorate-maps-ARGS = 8

LDFLAGS-tst-non-directory-mod.so = -Wl,-soname,tst-non-directory-mod.so
$(objpfx)tst-non-directory-path: $(objpfx)tst-non-directory-mod.so
$(objpfx)tst-non-directory-path.out: tst-non-directory-path.sh \
	$(objpfx)tst-non-directory-path
	$(SHELL) tst-non-directory-path.sh $(objpfx)ld.so $(objpfx)tst-non-directory-path \
		    '$(test-wrapper-env)' '$(run_program_env)' \
		    '$(rpath-link)' $(objpfx) > $@; \
	$(evaluate-test)

tst-env-setuid-ARGS = -- $(host-test-program-cmd)

# Reuse a module with a SONAME, to specific as the LD_PROFILE.
$(objpfx)tst-env-setuid: $(objpfx)tst-sonamemove-runmod2.so
$(objpfx)tst-env-setuid-static.out: $(objpfx)tst-sonamemove-runmod1.so

# The object tst-nodeps1-mod.so has no explicit dependencies on libc.so.
# We do not use $(link-test-modules-rpath-link) since the object has no
# DT_NEEDED.
$(objpfx)tst-nodeps1-mod.so: $(objpfx)tst-nodeps1-mod.os
	$(LINK.o) -nostartfiles -nostdlib -shared -o $@ $^
tst-nodeps1.so-no-z-defs = yes
# Link libc.so before the test module with the IFUNC resolver reference.
LDFLAGS-tst-nodeps1 = $(common-objpfx)libc.so $(objpfx)tst-nodeps1-mod.so
$(objpfx)tst-nodeps1: $(objpfx)tst-nodeps1-mod.so
# Reuse the tst-nodeps1 module.  Link libc.so before the test module
# with the IFUNC resolver reference.
$(objpfx)tst-nodeps2-mod.so: $(common-objpfx)libc.so \
  $(objpfx)tst-nodeps1-mod.so $(objpfx)tst-nodeps2-mod.os
	$(LINK.o) -Wl,--no-as-needed -nostartfiles -nostdlib -shared -o $@ $^ \
	$(link-test-modules-rpath-link)
$(objpfx)tst-nodeps2.out: \
  $(objpfx)tst-nodeps1-mod.so $(objpfx)tst-nodeps2-mod.so

$(objpfx)tst-tlsgap: $(shared-thread-library)
$(objpfx)tst-tlsgap.out: \
  $(objpfx)tst-tlsgap-mod0.so \
  $(objpfx)tst-tlsgap-mod1.so \
  $(objpfx)tst-tlsgap-mod2.so

$(objpfx)tst-gnu2-tls2: $(shared-thread-library)
$(objpfx)tst-gnu2-tls2.out: \
  $(objpfx)tst-gnu2-tls2mod0.so \
  $(objpfx)tst-gnu2-tls2mod1.so \
  $(objpfx)tst-gnu2-tls2mod2.so

ifneq (no,$(have-mtls-descriptor))
CFLAGS-tst-tlsgap-mod0.c += -mtls-dialect=$(have-mtls-descriptor)
CFLAGS-tst-tlsgap-mod1.c += -mtls-dialect=$(have-mtls-descriptor)
CFLAGS-tst-tlsgap-mod2.c += -mtls-dialect=$(have-mtls-descriptor)
CFLAGS-tst-gnu2-tls2mod0.c += -mtls-dialect=$(have-mtls-descriptor)
CFLAGS-tst-gnu2-tls2mod1.c += -mtls-dialect=$(have-mtls-descriptor)
CFLAGS-tst-gnu2-tls2mod2.c += -mtls-dialect=$(have-mtls-descriptor)
endif

$(objpfx)tst-recursive-tls: $(objpfx)tst-recursive-tlsmallocmod.so
# More objects than DTV_SURPLUS, to trigger DTV reallocation.
$(objpfx)tst-recursive-tls.out: \
  $(patsubst %,$(objpfx)tst-recursive-tlsmod%.so, \
    0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)
$(objpfx)tst-recursive-tlsmod%.os: tst-recursive-tlsmodN.c
	$(compile-command.c) -DVAR=thread_$* -DFUNC=get_threadvar_$*

# Order matters here.  The test needs the constructor for
# tst-dlopen-tlsreinitmod2.so to be called first.
LDFLAGS-tst-dlopen-tlsreinitmod1.so = -Wl,--no-as-needed
$(objpfx)tst-dlopen-tlsreinitmod1.so: \
  $(objpfx)tst-dlopen-tlsreinitmod3.so $(objpfx)tst-dlopen-tlsreinitmod2.so
LDFLAGS-tst-dlopen-tlsreinit2 = -Wl,--no-as-needed
$(objpfx)tst-dlopen-tlsreinit2: \
  $(objpfx)tst-dlopen-tlsreinitmod3.so $(objpfx)tst-dlopen-tlsreinitmod2.so
LDFLAGS-tst-dlopen-tlsreinit4 = -Wl,--no-as-needed
$(objpfx)tst-dlopen-tlsreinit4: \
  $(objpfx)tst-dlopen-tlsreinitmod3.so $(objpfx)tst-dlopen-tlsreinitmod2.so
# tst-dlopen-tlsreinitmod2.so is underlinked and refers to
# tst-dlopen-tlsreinitmod3.so.  The dependency is provided via
# $(objpfx)tst-dlopen-tlsreinitmod1.so.
tst-dlopen-tlsreinitmod2.so-no-z-defs = yes
$(objpfx)tst-dlopen-tlsreinit1.out: $(objpfx)tst-dlopen-tlsreinitmod1.so \
  $(objpfx)tst-dlopen-tlsreinitmod2.so $(objpfx)tst-dlopen-tlsreinitmod3.so
# Reuse an audit module which provides ample debug logging.
$(objpfx)tst-dlopen-tlsreinit3.out: $(objpfx)tst-auditmod1.so
tst-dlopen-tlsreinit3-ENV = LD_AUDIT=$(objpfx)tst-auditmod1.so
$(objpfx)tst-dlopen-tlsreinit4.out: $(objpfx)tst-auditmod1.so
tst-dlopen-tlsreinit4-ENV = LD_AUDIT=$(objpfx)tst-auditmod1.so
