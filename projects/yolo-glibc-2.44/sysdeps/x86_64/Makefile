# The i387 `long double' is a distinct type we support.
long-double-fcts = yes

ifeq (yes,$(have-z-mark-plt))
# Always generate DT_X86_64_PLT* tags.
sysdep-LDFLAGS += -Wl,-z,mark-plt
# Never generate DT_X86_64_PLT* tags on ld.so to avoid changing its own
# PLT.
LDFLAGS-rtld += -Wl,-z,nomark-plt
endif

ifeq ($(subdir),csu)
gen-as-const-headers += link-defines.sym
gen-as-const-headers += tlsdesc.sym rtld-offsets.sym
endif

ifeq ($(subdir),gmon)
sysdep_routines += _mcount
# We cannot compile _mcount.S with -pg because that would create
# recursive calls when ENTRY is used.  Just copy the normal static
# object.
sysdep_noprof += _mcount

ifeq (yes,$(have-x86-apx))
CFLAGS-mcount.c += -mno-apxf
endif
endif

ifeq ($(subdir),string)
sysdep_routines += \
  strcasecmp_l-nonascii \
  strcspn-generic \
  strncase_l-nonascii \
  strpbrk-generic \
  strspn-generic \
  varshift \
# sysdep_routines
gen-as-const-headers += locale-defines.sym
tests += \
  tst-rsi-strlen \
# tests
endif

ifeq ($(subdir),elf)
# There is no good reason to use MMX in x86-64 ld.so with GCC.
CFLAGS-.os += $(if $(filter $(@F),$(patsubst %,%.os,$(all-rtld-routines))),\
		   -mno-mmx)

modules-names += ifuncmod8

$(objpfx)ifuncmain8: $(objpfx)ifuncmod8.so

tests += tst-quad1 tst-quad2
modules-names += tst-quadmod1 tst-quadmod2

$(objpfx)tst-quad1: $(objpfx)tst-quadmod1.so
$(objpfx)tst-quad2: $(objpfx)tst-quadmod2.so

quad-pie-test += tst-quad1pie tst-quad2pie
tests += $(quad-pie-test)
tests-pie += $(quad-pie-test)
test-extras += tst-quadmod1pie tst-quadmod2pie
extra-test-objs += tst-quadmod1pie.o tst-quadmod2pie.o

$(objpfx)tst-quad1pie: $(objpfx)tst-quadmod1pie.o
$(objpfx)tst-quad2pie: $(objpfx)tst-quadmod2pie.o

CFLAGS-tst-quad1pie.c = $(PIE-ccflag)
CFLAGS-tst-quad2pie.c = $(PIE-ccflag)

tests += tst-platform-1
modules-names += tst-platformmod-1 x86_64/tst-platformmod-2
extra-test-objs += tst-platformmod-2.o
CFLAGS-tst-platform-1.c = -mno-avx
CFLAGS-tst-platformmod-1.c = -mno-avx
CFLAGS-tst-platformmod-2.c = -mno-avx
LDFLAGS-tst-platformmod-2.so = -Wl,-soname,tst-platformmod-2.so
$(objpfx)tst-platform-1: $(objpfx)tst-platformmod-1.so
$(objpfx)tst-platform-1.out: $(objpfx)x86_64/tst-platformmod-2.so
# Turn off AVX512F and AVX2 so that GLRO(dl_platform) is
# always set to x86_64.
tst-platform-1-ENV = LD_PRELOAD=$(objpfx)\$$PLATFORM/tst-platformmod-2.so \
	GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX512F,-AVX2

tests += tst-audit3 tst-audit4 tst-audit5 tst-audit6 tst-audit7 \
	 tst-audit10 tst-sse tst-avx tst-avx512
test-extras += tst-audit4-aux tst-audit10-aux \
	       tst-avx-aux tst-avx512-aux
extra-test-objs += tst-audit4-aux.o tst-audit10-aux.o \
		   tst-avx-aux.o tst-avx512-aux.o

ifeq ($(have-insert),yes)
tests += tst-split-dynreloc
LDFLAGS-tst-split-dynreloc = -Wl,-T,$(..)sysdeps/x86_64/tst-split-dynreloc.lds
tst-split-dynreloc-ENV = LD_BIND_NOW=1
endif

modules-names += tst-auditmod3a tst-auditmod3b \
		tst-auditmod4a tst-auditmod4b \
		tst-auditmod5a tst-auditmod5b \
		tst-auditmod6a tst-auditmod6b tst-auditmod6c \
		tst-auditmod7a tst-auditmod7b \
		tst-auditmod10a tst-auditmod10b \
		tst-ssemod tst-avxmod tst-avx512mod

$(objpfx)tst-audit3: $(objpfx)tst-auditmod3a.so
$(objpfx)tst-audit3.out: $(objpfx)tst-auditmod3b.so
tst-audit3-ENV = LD_AUDIT=$(objpfx)tst-auditmod3b.so

$(objpfx)tst-audit4: $(objpfx)tst-audit4-aux.o $(objpfx)tst-auditmod4a.so
$(objpfx)tst-audit4.out: $(objpfx)tst-auditmod4b.so
tst-audit4-ENV = LD_AUDIT=$(objpfx)tst-auditmod4b.so

$(objpfx)tst-audit5: $(objpfx)tst-auditmod5a.so
$(objpfx)tst-audit5.out: $(objpfx)tst-auditmod5b.so
tst-audit5-ENV = LD_AUDIT=$(objpfx)tst-auditmod5b.so

$(objpfx)tst-audit6: $(objpfx)tst-auditmod6a.so
$(objpfx)tst-audit6.out: $(objpfx)tst-auditmod6b.so \
			 $(objpfx)tst-auditmod6c.so
tst-audit6-ENV = LD_AUDIT=$(objpfx)tst-auditmod6b.so:$(objpfx)tst-auditmod6c.so

$(objpfx)tst-audit7: $(objpfx)tst-auditmod7a.so
$(objpfx)tst-audit7.out: $(objpfx)tst-auditmod7b.so
tst-audit7-ENV = LD_AUDIT=$(objpfx)tst-auditmod7b.so

$(objpfx)tst-audit10: $(objpfx)tst-audit10-aux.o $(objpfx)tst-auditmod10a.so
$(objpfx)tst-audit10.out: $(objpfx)tst-auditmod10b.so
tst-audit10-ENV = LD_AUDIT=$(objpfx)tst-auditmod10b.so

$(objpfx)tst-sse: $(objpfx)tst-ssemod.so
$(objpfx)tst-avx: $(objpfx)tst-avx-aux.o $(objpfx)tst-avxmod.so
$(objpfx)tst-avx512: $(objpfx)tst-avx512-aux.o $(objpfx)tst-avx512mod.so

AVX-CFLAGS=-mavx -mno-vzeroupper
CFLAGS-tst-audit4-aux.c += $(AVX-CFLAGS)
CFLAGS-tst-auditmod4a.c += $(AVX-CFLAGS)
CFLAGS-tst-auditmod4b.c += $(AVX-CFLAGS)
CFLAGS-tst-auditmod6b.c += $(AVX-CFLAGS)
CFLAGS-tst-auditmod6c.c += $(AVX-CFLAGS)
CFLAGS-tst-auditmod7b.c += $(AVX-CFLAGS)
CFLAGS-tst-avx-aux.c += $(AVX-CFLAGS)
CFLAGS-tst-avxmod.c += $(AVX-CFLAGS)
AVX512-CFLAGS = -mavx512f
CFLAGS-tst-audit10-aux.c += $(AVX512-CFLAGS)
CFLAGS-tst-auditmod10a.c += $(AVX512-CFLAGS)
CFLAGS-tst-avx512-aux.c += $(AVX512-CFLAGS)
CFLAGS-tst-avx512mod.c += $(AVX512-CFLAGS)

$(objpfx)tst-glibc-hwcaps: $(objpfx)libmarkermod2-1.so \
  $(objpfx)libmarkermod3-1.so $(objpfx)libmarkermod4-1.so
$(objpfx)tst-glibc-hwcaps.out: \
  $(objpfx)libmarkermod2.so \
    $(objpfx)glibc-hwcaps/x86-64-v2/libmarkermod2.so \
  $(objpfx)libmarkermod3.so \
    $(objpfx)glibc-hwcaps/x86-64-v2/libmarkermod3.so \
    $(objpfx)glibc-hwcaps/x86-64-v3/libmarkermod3.so \
  $(objpfx)libmarkermod4.so \
    $(objpfx)glibc-hwcaps/x86-64-v2/libmarkermod4.so \
    $(objpfx)glibc-hwcaps/x86-64-v3/libmarkermod4.so \
    $(objpfx)glibc-hwcaps/x86-64-v4/libmarkermod4.so \

$(objpfx)glibc-hwcaps/x86-64-v2/libmarkermod2.so: $(objpfx)libmarkermod2-2.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/x86-64-v2/libmarkermod3.so: $(objpfx)libmarkermod3-2.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/x86-64-v3/libmarkermod3.so: $(objpfx)libmarkermod3-3.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/x86-64-v2/libmarkermod4.so: $(objpfx)libmarkermod4-2.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/x86-64-v3/libmarkermod4.so: $(objpfx)libmarkermod4-3.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/x86-64-v4/libmarkermod4.so: $(objpfx)libmarkermod4-4.so
	$(make-target-directory)
	cp $< $@

ifeq (no,$(build-hardcoded-path-in-tests))
# This is an ld.so.cache test, and RPATH/RUNPATH in the executable
# interferes with its test objectives.
tests-container += tst-glibc-hwcaps-cache
endif

ifeq (yes,$(have-z-mark-plt))
tests += \
  tst-plt-rewrite1 \
  tst-plt-rewrite2 \
# tests
modules-names += \
  tst-plt-rewritemod1 \
  tst-plt-rewritemod2 \
# modules-names

tst-plt-rewrite1-no-pie = yes
LDFLAGS-tst-plt-rewrite1 = -Wl,-z,now
ifeq ($(have-test-cc-cflags-fsemantic-interposition),yes)
CFLAGS-tst-plt-rewritemod1.c += -fsemantic-interposition
endif
LDFLAGS-tst-plt-rewritemod1.so = -Wl,-z,now
tst-plt-rewrite1-ENV = GLIBC_TUNABLES=glibc.cpu.plt_rewrite=1 LD_DEBUG=files:bindings
$(objpfx)tst-plt-rewrite1: $(objpfx)tst-plt-rewritemod1.so
$(objpfx)tst-plt-rewrite1.out: /dev/null $(objpfx)tst-plt-rewrite1
	$(tst-plt-rewrite1-ENV) $(make-test-out) > $@ 2>&1; \
	grep -q -E "changing 'bar' PLT entry in .*/elf/tst-plt-rewritemod1.so' to direct branch" $@; \
	$(evaluate-test)

tst-plt-rewrite2-no-pie = yes
LDFLAGS-tst-plt-rewrite2 = -Wl,-z,now
LDFLAGS-tst-plt-rewritemod2.so = -Wl,-z,now,-z,undefs
tst-plt-rewrite2-ENV = GLIBC_TUNABLES=glibc.cpu.plt_rewrite=2
$(objpfx)tst-plt-rewrite2: $(objpfx)tst-plt-rewritemod2.so
endif

tests-special += $(objpfx)check-dt-x86-64-plt.out

$(objpfx)check-dt-x86-64-plt.out: $(common-objpfx)libc.so
	LC_ALL=C $(READELF) -V -W $< \
		| sed -ne '/.gnu.version_d/, /.gnu.version_r/ p' \
		| grep GLIBC_ABI_DT_X86_64_PLT > $@; \
	$(evaluate-test)
generated += check-dt-x86-64-plt.out

test-internal-extras += tst-gnu2-tls2mod1

tests-special += $(objpfx)check-rtld.out

$(objpfx)rtld.reloc: $(objpfx)rtld.os
	@rm -f $@T
	LC_ALL=C $(READELF) -rW $< > $@T
	test -s $@T
	mv -f $@T $@
common-generated += $(objpfx)rtld.reloc

# Verify that there are no run-time relocations against __ehdr_start nor
# _end.
$(objpfx)check-rtld.out: $(objpfx)rtld.reloc
	LC_ALL=C; \
	if grep -E "R_X86_64_64.*(__ehdr_start|_end)" $^ > $@; \
	  then false; else true; fi; \
	$(evaluate-test)
generated += check-rtld.out
endif # $(subdir) == elf

ifeq ($(subdir),wcsmbs)

sysdep_routines += \
  wcpcpy-generic \
  wcpncpy-generic \
  wcscat-generic \
  wcscpy-generic \
  wcsncat-generic \
  wcsncmp-generic \
  wcsncpy-generic \
  wcsnlen-generic \
# sysdep_routines

tests += \
  tst-rsi-wcslen \
# tests
endif


ifeq ($(subdir),setjmp)
ifneq ($(enable-cet),no)
tests += \
  tst-setjmp-cet \
# tests
tst-setjmp-cet-ENV = GLIBC_TUNABLES=glibc.cpu.x86_ibt=on:glibc.cpu.x86_shstk=on
endif
endif


ifneq ($(enable-cet),no)
# Add -fcf-protection to CFLAGS when CET is enabled.
CFLAGS-.o += -fcf-protection
CFLAGS-.os += -fcf-protection
CFLAGS-.op += -fcf-protection
CFLAGS-.oS += -fcf-protection

# Compile assembly codes with <cet.h> when CET is enabled.
asm-CPPFLAGS += -fcf-protection -include cet.h

ifeq ($(subdir),elf)
sysdep-dl-routines += dl-cet

tests += \
  tst-cet-legacy-1 \
  tst-cet-legacy-10 \
  tst-cet-legacy-10-static \
  tst-cet-legacy-10a \
  tst-cet-legacy-10a-static \
  tst-cet-legacy-1a \
  tst-cet-legacy-2 \
  tst-cet-legacy-2a \
  tst-cet-legacy-3 \
  tst-cet-legacy-4 \
  tst-cet-legacy-5a \
  tst-cet-legacy-6a \
  tst-cet-legacy-7 \
  tst-cet-legacy-8 \
  tst-cet-legacy-9 \
  tst-cet-legacy-9-static \
# tests
tests-static += \
  tst-cet-legacy-10-static \
  tst-cet-legacy-10a-static \
  tst-cet-legacy-9-static \
# tests-static
tst-cet-legacy-1a-ARGS = -- $(host-test-program-cmd)

tests += \
  tst-shstk-legacy-1a \
  tst-shstk-legacy-1a-static \
  tst-shstk-legacy-1b \
  tst-shstk-legacy-1b-static \
  tst-shstk-legacy-1c \
  tst-shstk-legacy-1c-static \
  tst-shstk-legacy-1d \
  tst-shstk-legacy-1d-static \
  tst-shstk-legacy-1e \
  tst-shstk-legacy-1e-static \
  tst-shstk-legacy-1f \
  tst-shstk-legacy-1g \
# tests
modules-names += \
  tst-shstk-legacy-mod-1 \
# modules-names
tests-static += \
  tst-shstk-legacy-1a-static \
  tst-shstk-legacy-1b-static \
  tst-shstk-legacy-1c-static \
  tst-shstk-legacy-1d-static \
  tst-shstk-legacy-1e-static \
# tests-static
extra-objs += \
  tst-shstk-legacy-1-extra.o \
# extra-objs

tests += \
  tst-cet-legacy-4a \
  tst-cet-legacy-4b \
  tst-cet-legacy-4c \
  tst-cet-legacy-5b \
  tst-cet-legacy-6b \
# tests
modules-names += \
  tst-cet-legacy-mod-1 \
  tst-cet-legacy-mod-2 \
  tst-cet-legacy-mod-4 \
  tst-cet-legacy-mod-5a \
  tst-cet-legacy-mod-5b \
  tst-cet-legacy-mod-5c \
  tst-cet-legacy-mod-6a \
  tst-cet-legacy-mod-6b \
  tst-cet-legacy-mod-6c \
# modules-names

CFLAGS-tst-cet-legacy-2.c += -fcf-protection=none -fcf-protection=branch
CFLAGS-tst-cet-legacy-2a.c += -fcf-protection
CFLAGS-tst-cet-legacy-mod-1.c += -fcf-protection=none
CFLAGS-tst-cet-legacy-mod-2.c += -fcf-protection=none
CFLAGS-tst-cet-legacy-3.c += -fcf-protection=none
CFLAGS-tst-cet-legacy-4.c += -fcf-protection=none -fcf-protection=branch
CPPFLAGS-tst-cet-legacy-4a.c += -DCET_IS_PERMISSIVE=1
CFLAGS-tst-cet-legacy-4a.c += -fcf-protection
CFLAGS-tst-cet-legacy-4b.c += -fcf-protection
CFLAGS-tst-cet-legacy-mod-4.c += -fcf-protection=none
CFLAGS-tst-cet-legacy-5a.c += -fcf-protection -mshstk
ifeq ($(enable-cet),permissive)
CPPFLAGS-tst-cet-legacy-5a.c += -DCET_IS_PERMISSIVE=1
endif
CFLAGS-tst-cet-legacy-5b.c += -fcf-protection -mshstk
CPPFLAGS-tst-cet-legacy-5b.c += -DCET_DISABLED_BY_ENV=1
CFLAGS-tst-cet-legacy-mod-5a.c += -fcf-protection=none -fcf-protection=branch
CFLAGS-tst-cet-legacy-mod-5b.c += -fcf-protection
CFLAGS-tst-cet-legacy-mod-5c.c += -fcf-protection
CFLAGS-tst-cet-legacy-6a.c += -fcf-protection -mshstk
ifeq ($(enable-cet),permissive)
CPPFLAGS-tst-cet-legacy-6a.c += -DCET_IS_PERMISSIVE=1
endif
CFLAGS-tst-cet-legacy-6b.c += -fcf-protection -mshstk
CPPFLAGS-tst-cet-legacy-6b.c += -DCET_DISABLED_BY_ENV=1
CFLAGS-tst-cet-legacy-mod-6a.c += -fcf-protection=none -fcf-protection=branch
CFLAGS-tst-cet-legacy-mod-6b.c += -fcf-protection
CFLAGS-tst-cet-legacy-mod-6c.c += -fcf-protection
CFLAGS-tst-cet-legacy-7.c += -fcf-protection=none
CFLAGS-tst-cet-legacy-10.c += -mshstk
CFLAGS-tst-cet-legacy-10-static.c += -mshstk
CFLAGS-tst-cet-legacy-10a.c += -fcf-protection=none -mshstk
CFLAGS-tst-cet-legacy-10a-static.c += -fcf-protection=none -mshstk

tst-cet-legacy-4-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
tst-cet-legacy-6-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
tst-cet-legacy-10-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
tst-cet-legacy-10-static-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
tst-cet-legacy-10a-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
tst-cet-legacy-10a-static-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK

CFLAGS-tst-shstk-legacy-1a.c += -fcf-protection=none
CFLAGS-tst-shstk-legacy-1a-static.c += -fcf-protection=none
CFLAGS-tst-shstk-legacy-1d.c += -fcf-protection=none
CFLAGS-tst-shstk-legacy-1d-static.c += -fcf-protection=none
CFLAGS-tst-shstk-legacy-1f.c += -fcf-protection=none

$(objpfx)tst-cet-legacy-1: $(objpfx)tst-cet-legacy-mod-1.so \
		       $(objpfx)tst-cet-legacy-mod-2.so
$(objpfx)tst-cet-legacy-1a: $(objpfx)tst-cet-legacy-mod-1.so \
		       $(objpfx)tst-cet-legacy-mod-2.so
$(objpfx)tst-cet-legacy-2: $(objpfx)tst-cet-legacy-mod-2.so
$(objpfx)tst-cet-legacy-2.out: $(objpfx)tst-cet-legacy-mod-1.so
$(objpfx)tst-cet-legacy-2a: $(objpfx)tst-cet-legacy-mod-2.so
$(objpfx)tst-cet-legacy-2a.out: $(objpfx)tst-cet-legacy-mod-1.so
$(objpfx)tst-cet-legacy-4.out: $(objpfx)tst-cet-legacy-mod-4.so
$(objpfx)tst-cet-legacy-5a.out: $(objpfx)tst-cet-legacy-mod-5a.so \
				$(objpfx)tst-cet-legacy-mod-5b.so
$(objpfx)tst-cet-legacy-mod-5a.so: $(objpfx)tst-cet-legacy-mod-5c.so
$(objpfx)tst-cet-legacy-mod-5b.so: $(objpfx)tst-cet-legacy-mod-5c.so
$(objpfx)tst-cet-legacy-6a.out: $(objpfx)tst-cet-legacy-mod-6a.so \
				$(objpfx)tst-cet-legacy-mod-6b.so
$(objpfx)tst-cet-legacy-mod-6a.so: $(objpfx)tst-cet-legacy-mod-6c.so
$(objpfx)tst-cet-legacy-mod-6b.so: $(objpfx)tst-cet-legacy-mod-6c.so
LDFLAGS-tst-cet-legacy-mod-6c.so = -Wl,--enable-new-dtags,-z,nodelete
$(objpfx)tst-cet-legacy-4a.out: $(objpfx)tst-cet-legacy-mod-4.so
tst-cet-legacy-4a-ENV = GLIBC_TUNABLES=glibc.cpu.x86_shstk=permissive
$(objpfx)tst-cet-legacy-4b.out: $(objpfx)tst-cet-legacy-mod-4.so
tst-cet-legacy-4b-ENV = GLIBC_TUNABLES=glibc.cpu.x86_shstk=on
$(objpfx)tst-cet-legacy-4c.out: $(objpfx)tst-cet-legacy-mod-4.so
tst-cet-legacy-4c-ENV = GLIBC_TUNABLES=glibc.cpu.x86_shstk=off
$(objpfx)tst-cet-legacy-5b.out: $(objpfx)tst-cet-legacy-mod-5a.so \
				$(objpfx)tst-cet-legacy-mod-5b.so
tst-cet-legacy-5b-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=-IBT,-SHSTK
$(objpfx)tst-cet-legacy-6b.out: $(objpfx)tst-cet-legacy-mod-6a.so \
				$(objpfx)tst-cet-legacy-mod-6b.so
tst-cet-legacy-6b-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=-IBT,-SHSTK
tst-cet-legacy-9-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=-IBT,-SHSTK
tst-cet-legacy-9-static-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=-IBT,-SHSTK

tst-shstk-legacy-1a-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
tst-shstk-legacy-1a-static-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
$(objpfx)tst-shstk-legacy-1a: $(objpfx)tst-shstk-legacy-1-extra.o
$(objpfx)tst-shstk-legacy-1a-static: $(objpfx)tst-shstk-legacy-1-extra.o
tst-shstk-legacy-1b-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
tst-shstk-legacy-1b-static-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
$(objpfx)tst-shstk-legacy-1b: $(objpfx)tst-shstk-legacy-1-extra.o
$(objpfx)tst-shstk-legacy-1b-static: $(objpfx)tst-shstk-legacy-1-extra.o
tst-shstk-legacy-1c-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=-SHSTK
tst-shstk-legacy-1c-static-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=-SHSTK
$(objpfx)tst-shstk-legacy-1c: $(objpfx)tst-shstk-legacy-1-extra.o
$(objpfx)tst-shstk-legacy-1c-static: $(objpfx)tst-shstk-legacy-1-extra.o
tst-shstk-legacy-1d-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
tst-shstk-legacy-1d-static-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
$(objpfx)tst-shstk-legacy-1d: $(objpfx)tst-shstk-legacy-1-extra.o
$(objpfx)tst-shstk-legacy-1d-static: $(objpfx)tst-shstk-legacy-1-extra.o
$(objpfx)tst-shstk-legacy-1e: $(objpfx)tst-shstk-legacy-1-extra.o
$(objpfx)tst-shstk-legacy-1e-static: $(objpfx)tst-shstk-legacy-1-extra.o
$(objpfx)tst-shstk-legacy-1e.out: \
  $(..)/sysdeps/x86_64/tst-shstk-legacy-1e.sh $(objpfx)tst-shstk-legacy-1e
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix)' 2> $@; \
	$(evaluate-test)
$(objpfx)tst-shstk-legacy-1e-static.out: \
  $(..)/sysdeps/x86_64/tst-shstk-legacy-1e-static.sh \
  $(objpfx)tst-shstk-legacy-1e-static
	$(SHELL) $< $(common-objpfx) 2> $@; \
	$(evaluate-test)
tst-shstk-legacy-1f-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK
$(objpfx)tst-shstk-legacy-1f: $(objpfx)tst-shstk-legacy-mod-1.so
$(objpfx)tst-shstk-legacy-mod-1.so: \
  $(objpfx)tst-shstk-legacy-mod-1.os \
  $(objpfx)tst-shstk-legacy-1-extra.os
$(objpfx)tst-shstk-legacy-1g: $(objpfx)tst-shstk-legacy-mod-1.so
$(objpfx)tst-shstk-legacy-1g.out: \
  $(..)/sysdeps/x86_64/tst-shstk-legacy-1g.sh $(objpfx)tst-shstk-legacy-1g
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix)' 2> $@; \
	$(evaluate-test)

ifeq (yes,$(build-shared))
tests-special += $(objpfx)check-cet.out
endif

# FIXME: Can't use all-built-dso in elf/Makefile since this file is
# processed before elf/Makefile.  Duplicate it here.
cet-built-dso := $(common-objpfx)elf/ld.so $(common-objpfx)libc.so \
		 $(filter-out $(common-objpfx)linkobj/libc.so, \
			      $(sort $(wildcard $(addprefix $(common-objpfx), \
							    */lib*.so \
							    iconvdata/*.so))))

$(cet-built-dso:=.note): %.note: %
	@rm -f $@T
	LC_ALL=C $(READELF) -n $< > $@T
	test -s $@T
	mv -f $@T $@
common-generated += $(cet-built-dso:$(common-objpfx)%=%.note)

$(objpfx)check-cet.out: $(..)sysdeps/x86_64/check-cet.awk \
			$(cet-built-dso:=.note)
	LC_ALL=C $(AWK) -f $^ > $@; \
	$(evaluate-test)
generated += check-cet.out
endif # $(subdir) == elf
endif # $(enable) != no

do-tests-clean common-mostlyclean: tst-x86_64-1-clean

.PHONY: tst-x86_64-1-clean
tst-x86_64-1-clean:
	-rm -rf $(objpfx)x86_64

$(objpfx)x86_64/tst-platformmod-2.os: $(objpfx)tst-platformmod-2.os
	$(make-target-directory)
	rm -f $@
	ln $< $@
