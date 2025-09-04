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
#	Sub-makefile for POSIX portion of the library.
#
subdir	:= posix

include ../Makeconfig

headers := \
  bits/confname.h \
  bits/cpu-set.h \
  bits/environments.h \
  bits/getopt_core.h \
  bits/getopt_ext.h \
  bits/getopt_posix.h \
  bits/local_lim.h \
  bits/mman_ext.h \
  bits/posix1_lim.h \
  bits/posix2_lim.h \
  bits/posix_opt.h \
  bits/pthreadtypes-arch.h \
  bits/pthreadtypes.h \
  bits/sched.h \
  bits/spawn_ext.h \
  bits/thread-shared-types.h \
  bits/types.h \
  bits/types/idtype_t.h \
  bits/types/struct_sched_param.h \
  bits/typesizes.h \
  bits/unistd-decl.h \
  bits/unistd.h \
  bits/unistd_ext.h \
  bits/utsname.h \
  bits/waitflags.h \
  bits/waitstatus.h \
  cpio.h \
  fnmatch.h \
  getopt.h \
  glob.h \
  re_comp.h \
  regex.h \
  sched.h \
  spawn.h \
  sys/times.h \
  sys/types.h \
  sys/unistd.h \
  sys/utsname.h \
  sys/wait.h \
  tar.h \
  unistd.h \
  wait.h \
  wordexp.h \
  # headers

routines := \
  _Fork \
  _exit \
  alarm \
  bsd-getpgrp \
  confstr \
  execl \
  execle \
  execlp \
  execv \
  execve \
  execveat \
  execvp \
  execvpe \
  fexecve \
  fnmatch \
  fork \
  fpathconf \
  gai_strerror \
  get_child_max \
  getegid \
  geteuid \
  getgid \
  getgroups \
  getopt \
  getopt1 \
  getpgid \
  getpgrp \
  getpid \
  getppid \
  getresgid \
  getresuid \
  getsid \
  getuid \
  glob \
  glob-lstat-compat \
  glob64 \
  glob64-lstat-compat \
  glob64-time64 \
  glob_pattern_p \
  globfree \
  globfree64 \
  globfree64-time64 \
  group_member \
  nanosleep \
  pathconf \
  pause \
  posix_madvise \
  pread \
  pread64 \
  pwrite \
  pwrite64 \
  regex \
  register-atfork \
  sched_cpualloc \
  sched_cpucount \
  sched_cpufree \
  sched_getaffinity \
  sched_getp \
  sched_gets \
  sched_primax \
  sched_primin \
  sched_rr_gi \
  sched_setaffinity \
  sched_setp \
  sched_sets \
  sched_yield \
  setgid \
  setgroups \
  setpgid \
  setpgrp \
  setresgid \
  setresuid \
  setsid \
  setuid \
  shm-directory \
  sleep \
  spawn \
  spawn_faction_addchdir \
  spawn_faction_addclose \
  spawn_faction_addclosefrom \
  spawn_faction_adddup2 \
  spawn_faction_addfchdir \
  spawn_faction_addopen \
  spawn_faction_addtcsetpgrp_np \
  spawn_faction_destroy \
  spawn_faction_init \
  spawn_valid_fd \
  spawnattr_destroy \
  spawnattr_getdefault \
  spawnattr_getflags \
  spawnattr_getpgroup \
  spawnattr_getschedparam \
  spawnattr_getschedpolicy \
  spawnattr_getsigmask \
  spawnattr_init \
  spawnattr_setdefault \
  spawnattr_setflags \
  spawnattr_setpgroup \
  spawnattr_setschedparam \
  spawnattr_setschedpolicy \
  spawnattr_setsigmask \
  spawni \
  spawnp \
  streams-compat \
  sysconf \
  times \
  uname \
  vfork \
  wait \
  wait3 \
  wait4 \
  waitid \
  waitpid \
  wordexp \
  # routines

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  confstr \
  getgroups \
  pread \
  pread64 \
  # routines_no_fortify

aux := \
  environ \
  init-posix \
  # aux

tests := \
  bug-ga1 \
  bug-getopt1 \
  bug-getopt2 \
  bug-getopt3 \
  bug-getopt4 \
  bug-getopt5 \
  bug-glob2 \
  bug-glob3 \
  bug-regex1 \
  bug-regex2 \
  bug-regex3 \
  bug-regex4 \
  bug-regex6 \
  bug-regex7 \
  bug-regex8 \
  bug-regex9 \
  bug-regex10 \
  bug-regex11 \
  bug-regex12 \
  bug-regex13 \
  bug-regex14 \
  bug-regex15 \
  bug-regex16 \
  bug-regex17 \
  bug-regex18 \
  bug-regex19 \
  bug-regex21 \
  bug-regex22 \
  bug-regex23 \
  bug-regex24 \
  bug-regex25 \
  bug-regex26 \
  bug-regex27 \
  bug-regex28 \
  bug-regex29 \
  bug-regex30 \
  bug-regex31 \
  bug-regex32 \
  bug-regex34 \
  bug-regex35 \
  bug-regex36 \
  bug-regex37 \
  bug-regex38 \
  regexbug1 \
  runptests \
  runtests \
  test-errno \
  test-ssize-max \
  test-vfork \
  testfnm \
  transbug \
  tst-boost \
  tst-chmod \
  tst-cpucount \
  tst-cpuset \
  tst-dir \
  tst-execl1 \
  tst-execl2 \
  tst-execle1 \
  tst-execle2 \
  tst-execlp1 \
  tst-execlp2 \
  tst-execv1 \
  tst-execv2 \
  tst-execve1 \
  tst-execve2 \
  tst-execveat \
  tst-execvp1 \
  tst-execvp2 \
  tst-execvp3 \
  tst-execvp4 \
  tst-execvpe1 \
  tst-execvpe2 \
  tst-execvpe3 \
  tst-execvpe4 \
  tst-execvpe5 \
  tst-execvpe6 \
  tst-fexecve \
  tst-fnmatch \
  tst-fnmatch2 \
  tst-fnmatch3 \
  tst-fnmatch4 \
  tst-fnmatch5 \
  tst-fnmatch6 \
  tst-fnmatch7 \
  tst-fork \
  tst-gai_strerror \
  tst-getopt_long1 \
  tst-glob-tilde \
  tst-glob_symlinks \
  tst-gnuglob \
  tst-gnuglob64 \
  tst-mmap \
  tst-mmap-offset \
  tst-nanosleep \
  tst-nice \
  tst-pathconf \
  tst-pcre \
  tst-posix_fadvise \
  tst-posix_fadvise64 \
  tst-posix_spawn-fd \
  tst-posix_spawn-setsid \
  tst-preadwrite \
  tst-preadwrite64 \
  tst-regcomp-truncated \
  tst-regex \
  tst-regex2 \
  tst-regexloc \
  tst-rxspencer \
  tst-rxspencer-no-utf8 \
  tst-sched_getaffinity \
  tst-spawn-chdir \
  tst-spawn4 \
  tst-spawn5 \
  tst-spawn6 \
  tst-spawn7 \
  tst-sysconf \
  tst-sysconf-empty-chroot \
  tst-truncate \
  tst-truncate64 \
  tst-vfork1 \
  tst-vfork2 \
  tst-wait3 \
  tst-wait4 \
  tst-waitid \
  tst-wordexp-nocmd \
  tstgetopt \
  # tests

# Test for the glob symbol version that was replaced in glibc 2.27.
ifeq ($(have-GLIBC_2.26)$(build-shared),yesyes)
tests += \
  tst-glob_lstat_compat
  # tests
endif

# Test for the posix_spawn, posix_spawnp symbol versions replaced in
# glibc 2.15.
ifeq ($(have-GLIBC_2.14)$(build-shared),yesyes)
tests += \
  tst-spawn4-compat \
  # tests
endif

tests-internal := \
  bug-regex5 \
  bug-regex20 \
  bug-regex33 \
  # tests-internal

tests-container := \
  bug-ga2 \
  tst-vfork3 \
  # tests-container

tests-time64 := \
  tst-gnuglob64-time64 \
  tst-wait3-time64 \
  tst-wait4-time64 \
  # tests-time64

xtests	:= \
  tst-sched_rr_get_interval \
  # xtests

xtests-time64 := \
  tst-sched_rr_get_interval-time64 \
  # xtests-time64

ifeq (yes,$(build-shared))
test-srcs := \
  globtest
  # tests-src
tests += \
  tst-exec \
  tst-spawn \
  tst-spawn2 \
  tst-spawn3 \
  wordexp-test \
  # tests
endif

ifeq (yesyes,$(build-shared)$(have-thread-library))
tests += \
  tst-_Fork \
  tst-getopt-cancel \
  # tests
endif

tests-static = \
  tst-exec-static \
  tst-spawn-static \
  # tests-static

tests += \
  $(tests-static)
  # tests

others := \
  getconf \
  # others

install-bin := \
  getconf \
  # getconf

install-others-programs	:= \
  $(inst_libexecdir)/getconf \
  # install-others-programs

before-compile += \
  $(objpfx)posix-conf-vars-def.h \
  # before-compile

# So they get cleaned up.
generated += \
  $(addprefix wordexp-test-result, 1 2 3 4 5 6 7 8 9 10) \
  annexc \
  annexc.out \
  getconf.speclist \
  ptestcases.h \
  testcases.h \
  tst-getconf.out \
  wordexp-tst.out \
  # generated

ifeq ($(run-built-tests),yes)
ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
generated += \
  bug-ga2-mem.out \
  bug-ga2.mtrace \
  bug-glob2-mem.out \
  bug-glob2.mtrace \
  bug-regex14-mem.out \
  bug-regex14.mtrace \
  bug-regex2-mem.out \
  bug-regex2.mtrace \
  bug-regex21-mem.out \
  bug-regex21.mtrace \
  bug-regex31-mem.out \
  bug-regex31.mtrace \
  bug-regex36-mem.out \
  bug-regex36.mtrace \
  tst-boost-mem.out \
  tst-boost.mtrace \
  tst-fnmatch-mem.out \
  tst-fnmatch.mtrace \
  tst-pcre-mem.out \
  tst-pcre.mtrace \
  tst-rxspencer-no-utf8-mem.out \
  tst-rxspencer-no-utf8.mtrace \
  tst-vfork3-mem.out \
  tst-vfork3.mtrace \
  # generated
endif
endif
endif

ifeq ($(run-built-tests),yes)
ifeq (yes,$(build-shared))
tests-special += \
  $(objpfx)globtest.out \
  $(objpfx)wordexp-tst.out \
  # tests-special
endif
endif

# Run a test on the header files we use.
# XXX Please note that for now we ignore the result of this test.
tests-special += $(objpfx)annexc.out
ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-getconf.out
ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
tests-special += \
  $(objpfx)bug-ga2-mem.out \
  $(objpfx)bug-glob2-mem.out \
  $(objpfx)bug-regex14-mem.out \
  $(objpfx)bug-regex2-mem.out \
  $(objpfx)bug-regex21-mem.out \
  $(objpfx)bug-regex31-mem.out \
  $(objpfx)bug-regex36-mem.out \
  $(objpfx)tst-boost-mem.out \
  $(objpfx)tst-fnmatch-mem.out \
  $(objpfx)tst-glob-tilde-mem.out \
  $(objpfx)tst-pcre-mem.out \
  $(objpfx)tst-rxspencer-no-utf8-mem.out \
  $(objpfx)tst-vfork3-mem.out \
  # tests-special
endif
endif
endif

include ../Rules

ifeq ($(run-built-tests),yes)
# globtest and wordexp-test currently only works with shared libraries
ifeq (yes,$(build-shared))
$(objpfx)globtest.out: globtest.sh $(objpfx)globtest
	$(SHELL) $< $(common-objpfx) '$(test-via-rtld-prefix)' \
		'$(test-program-prefix)' '$(test-wrapper-env)'; \
	$(evaluate-test)
$(objpfx)wordexp-tst.out: wordexp-tst.sh $(objpfx)wordexp-test
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix-before-env)' \
		 '$(run-program-env)' '$(test-program-prefix-after-env)'; \
	$(evaluate-test)
endif

LOCALES := \
  C.UTF-8 \
  cs_CZ.ISO-8859-2 \
  cs_CZ.UTF-8 \
  da_DK.ISO-8859-1 \
  de_DE.ISO-8859-1 \
  de_DE.UTF-8 \
  en_US.UTF-8 \
  es_US.ISO-8859-1 \
  es_US.UTF-8 \
  ja_JP.EUC-JP \
  tr_TR.UTF-8 \
  # LOCALES
include ../gen-locales.mk

$(objpfx)bug-regex1.out: $(gen-locales)
$(objpfx)bug-regex4.out: $(gen-locales)
$(objpfx)bug-regex5.out: $(gen-locales)
$(objpfx)bug-regex6.out: $(gen-locales)
$(objpfx)bug-regex17.out: $(gen-locales)
$(objpfx)bug-regex18.out: $(gen-locales)
$(objpfx)bug-regex19.out: $(gen-locales)
$(objpfx)bug-regex20.out: $(gen-locales)
$(objpfx)bug-regex22.out: $(gen-locales)
$(objpfx)bug-regex23.out: $(gen-locales)
$(objpfx)bug-regex25.out: $(gen-locales)
$(objpfx)bug-regex26.out: $(gen-locales)
$(objpfx)bug-regex30.out: $(gen-locales)
$(objpfx)bug-regex32.out: $(gen-locales)
$(objpfx)bug-regex33.out: $(gen-locales)
$(objpfx)bug-regex34.out: $(gen-locales)
$(objpfx)bug-regex35.out: $(gen-locales)
$(objpfx)tst-fnmatch.out: $(gen-locales)
$(objpfx)tst-fnmatch4.out: $(gen-locales)
$(objpfx)tst-fnmatch5.out: $(gen-locales)
$(objpfx)tst-fnmatch6.out: $(gen-locales)
$(objpfx)tst-regex.out: $(gen-locales)
$(objpfx)tst-regex2.out: $(gen-locales)
$(objpfx)tst-regexloc.out: $(gen-locales)
$(objpfx)tst-rxspencer.out: $(gen-locales)
$(objpfx)tst-rxspencer-no-utf8.out: $(gen-locales)
$(objpfx)tst-regcomp-truncated.out: $(gen-locales)
endif

# If we will use the generic uname implementation, we must figure out what
# it will say by examining the system, and write the results in config-name.h.
uname.c: $(objpfx)config-name.h
$(objpfx)config-name.h: $(..)scripts/config-uname.sh $(common-objpfx)config.make
	$(make-target-directory)
	$< '$(config-os)' '$(config-release)' \
	   '$(config-machine)-$(config-vendor)' > $@.new
	mv -f $@.new $@

CFLAGS-pause.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-pread.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-pread64.c += -fexceptions -fasynchronous-unwind-tables $(config-cflags-wno-ignored-attributes)
CFLAGS-pwrite.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-pwrite64.c += -fexceptions -fasynchronous-unwind-tables $(config-cflags-wno-ignored-attributes)
CFLAGS-sleep.c += -fexceptions
CFLAGS-wait.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-waitid.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-waitpid.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-wait4.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-getopt.c += -fexceptions
CFLAGS-wordexp.c += -fexceptions
CFLAGS-wordexp.os = -fomit-frame-pointer
CFLAGS-sysconf.c += -fexceptions -DGETCONF_DIR='"$(libexecdir)/getconf"'
CFLAGS-pathconf.c += -fexceptions
CFLAGS-fpathconf.c += -fexceptions
CFLAGS-spawn.c += -fexceptions
CFLAGS-spawn.os = -fomit-frame-pointer
CFLAGS-spawnp.c += -fexceptions
CFLAGS-spawnp.os = -fomit-frame-pointer
CFLAGS-spawni.c += -fexceptions
CFLAGS-spawni.os = -fomit-frame-pointer
CFLAGS-glob.c += $(uses-callbacks) -fexceptions
CFLAGS-glob64.c += $(uses-callbacks) -fexceptions
CFLAGS-getconf.c += -DGETCONF_DIR='"$(libexecdir)/getconf"'
CFLAGS-execve.os = -fomit-frame-pointer
CFLAGS-fexecve.os = -fomit-frame-pointer
CFLAGS-execv.os = -fomit-frame-pointer
CFLAGS-execle.os = -fomit-frame-pointer
CFLAGS-execl.os = -fomit-frame-pointer
CFLAGS-execvp.os = -fomit-frame-pointer
CFLAGS-execlp.os = -fomit-frame-pointer
CFLAGS-nanosleep.c += -fexceptions -fasynchronous-unwind-tables
CFLAGS-fork.c = $(libio-mtsafe) $(config-cflags-wno-ignored-attributes)

tstgetopt-ARGS = -a -b -cfoobar --required foobar --optional=bazbug \
		--none random --col --color --colour

tst-exec-ARGS = -- $(host-test-program-cmd)
tst-exec-static-ARGS = $(tst-exec-ARGS)
tst-execvpe5-ARGS = -- $(host-test-program-cmd)
tst-spawn-ARGS = -- $(host-test-program-cmd)
tst-spawn-static-ARGS = $(tst-spawn-ARGS)
tst-spawn5-ARGS = -- $(host-test-program-cmd)
tst-spawn6-ARGS = -- $(host-test-program-cmd)
tst-spawn7-ARGS = -- $(host-test-program-cmd)
tst-posix_spawn-setsid-ARGS = -- $(host-test-program-cmd)
tst-dir-ARGS = `pwd` `cd $(common-objdir)/$(subdir); pwd` `cd $(common-objdir); pwd` $(objpfx)tst-dir
tst-chmod-ARGS = $(objdir)
tst-vfork3-ARGS = --test-dir=$(objpfx)

tst-rxspencer-ARGS = --utf8 rxspencer/tests
tst-rxspencer-no-utf8-ARGS = rxspencer/tests
tst-pcre-ARGS = PCRE.tests
tst-boost-ARGS = BOOST.tests
bug-glob1-ARGS = "$(objpfx)"
tst-execvp3-ARGS = --test-dir=$(objpfx)
CFLAGS-tst-spawn3.c += -DOBJPFX=\"$(objpfx)\"

# Test voluntarily overflows struct dirent
CFLAGS-bug-glob2.c += $(no-fortify-source)

$(objpfx)testcases.h: TESTS TESTS2C.sed
	LC_ALL=C sed -f TESTS2C.sed < $< > $@T
	mv $@T $@
$(objpfx)runtests.o: $(objpfx)testcases.h

$(objpfx)ptestcases.h: PTESTS PTESTS2C.sed
	LC_ALL=C sed -f PTESTS2C.sed < $< > $@T
	mv $@T $@
$(objpfx)runptests.o: $(objpfx)ptestcases.h

$(objpfx)tst-getopt-cancel: $(shared-thread-library)
$(objpfx)tst-_Fork: $(shared-thread-library)

test-xfail-annexc = yes
$(objpfx)annexc.out: $(objpfx)annexc
	$(dir $<)$(notdir $<) '$(CC)' \
	  '$(patsubst %,-I../%,$(sorted-subdirs)) -I../include $(+sysdep-includes) $(sysincludes) -I..' > $@; \
	$(evaluate-test)

annexc-CFLAGS = -O
$(objpfx)annexc: annexc.c
	$(native-compile)

tst-fnmatch-ENV += MALLOC_TRACE=$(objpfx)tst-fnmatch.mtrace \
		   LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)tst-fnmatch-mem.out: $(objpfx)tst-fnmatch.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-fnmatch.mtrace > $@; \
	$(evaluate-test)

bug-regex2-ENV = MALLOC_TRACE=$(objpfx)bug-regex2.mtrace \
		 LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)bug-regex2-mem.out: $(objpfx)bug-regex2.out
	$(common-objpfx)malloc/mtrace $(objpfx)bug-regex2.mtrace > $@; \
	$(evaluate-test)

bug-regex14-ENV = MALLOC_TRACE=$(objpfx)bug-regex14.mtrace \
		  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)bug-regex14-mem.out: $(objpfx)bug-regex14.out
	$(common-objpfx)malloc/mtrace $(objpfx)bug-regex14.mtrace > $@; \
	$(evaluate-test)

bug-regex21-ENV = MALLOC_TRACE=$(objpfx)bug-regex21.mtrace \
		  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)bug-regex21-mem.out: $(objpfx)bug-regex21.out
	$(common-objpfx)malloc/mtrace $(objpfx)bug-regex21.mtrace > $@; \
	$(evaluate-test)

bug-regex31-ENV = MALLOC_TRACE=$(objpfx)bug-regex31.mtrace \
		  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)bug-regex31-mem.out: $(objpfx)bug-regex31.out
	$(common-objpfx)malloc/mtrace $(objpfx)bug-regex31.mtrace > $@; \
	$(evaluate-test)

bug-regex36-ENV = MALLOC_TRACE=$(objpfx)bug-regex36.mtrace \
		  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)bug-regex36-mem.out: $(objpfx)bug-regex36.out
	$(common-objpfx)malloc/mtrace $(objpfx)bug-regex36.mtrace > $@; \
	$(evaluate-test)

tst-vfork3-ENV = MALLOC_TRACE=$(objpfx)tst-vfork3.mtrace \
		 LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)tst-vfork3-mem.out: $(objpfx)tst-vfork3.out
	{ test -r $(objpfx)tst-vfork3.mtrace \
	|| ( echo "tst-vfork3.mtrace does not exist"; exit 77; ) \
	&& $(common-objpfx)malloc/mtrace $(objpfx)tst-vfork3.mtrace; } > $@; \
	$(evaluate-test)

# tst-rxspencer.mtrace is not generated, only
# tst-rxspencer-no-utf8.mtrace, since otherwise the file has almost
# 100M and takes very long time to process.
tst-rxspencer-no-utf8-ENV += \
  MALLOC_TRACE=$(objpfx)tst-rxspencer-no-utf8.mtrace \
  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
$(objpfx)tst-rxspencer-no-utf8-mem.out: $(objpfx)tst-rxspencer-no-utf8.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-rxspencer-no-utf8.mtrace \
				      > $@; \
	$(evaluate-test)

tst-pcre-ENV = MALLOC_TRACE=$(objpfx)tst-pcre.mtrace \
	       LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
$(objpfx)tst-pcre-mem.out: $(objpfx)tst-pcre.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-pcre.mtrace > $@; \
	$(evaluate-test)

tst-boost-ENV = MALLOC_TRACE=$(objpfx)tst-boost.mtrace \
		LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
$(objpfx)tst-boost-mem.out: $(objpfx)tst-boost.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-boost.mtrace > $@; \
	$(evaluate-test)

$(objpfx)tst-getconf.out: tst-getconf.sh $(objpfx)getconf
	$(SHELL) $< $(common-objpfx) '$(built-program-cmd)'; \
	$(evaluate-test)

$(objpfx)bug-ga2-mem.out: $(objpfx)bug-ga2.out
	{ test -r $(objpfx)bug-ga2.mtrace \
	|| ( echo "bug-ga2.mtrace does not exist"; exit 77; ) \
	&& $(common-objpfx)malloc/mtrace $(objpfx)bug-ga2.mtrace; } > $@; \
	$(evaluate-test)

bug-ga2-ENV = MALLOC_TRACE=$(objpfx)bug-ga2.mtrace \
	      LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

bug-glob2-ENV = MALLOC_TRACE=$(objpfx)bug-glob2.mtrace \
		LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)bug-glob2-mem.out: $(objpfx)bug-glob2.out
	$(common-objpfx)malloc/mtrace $(objpfx)bug-glob2.mtrace > $@; \
	$(evaluate-test)

tst-glob-tilde-ENV = MALLOC_TRACE=$(objpfx)tst-glob-tilde.mtrace \
		     LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

$(objpfx)tst-glob-tilde-mem.out: $(objpfx)tst-glob-tilde.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-glob-tilde.mtrace > $@; \
	$(evaluate-test)

$(inst_libexecdir)/getconf: $(inst_bindir)/getconf \
			    $(objpfx)getconf.speclist FORCE
	$(addprefix $(..)./scripts/mkinstalldirs ,\
		    $(filter-out $(wildcard $@),$@))
	while read spec; do \
	  ln -f $< $@/$$spec.new || $(INSTALL_PROGRAM) $< $@/$$spec.new; \
	  mv -f $@/$$spec.new $@/$$spec; \
	done < $(objpfx)getconf.speclist

$(objpfx)getconf.speclist: getconf-speclist.c posix-envs.def
	$(compile.c) -E -o - \
	    | sed -n -e '/@@@PRESENT_/s/@@@PRESENT_//p' > $@.new
	mv -f $@.new $@

# This file is only actually needed at install time.  But forcing it to
# be built both makes it available for eyeball inspection and avoids the
# surprise of things that look like compilation being done by 'make install'.
others: $(objpfx)getconf.speclist

$(objpfx)posix-conf-vars-def.h: $(..)scripts/gen-posix-conf-vars.awk \
				posix-conf-vars.list Makefile
	$(make-target-directory)
	$(AWK) -f $(filter-out Makefile, $^) > $@.tmp
	mv -f $@.tmp $@
