ifeq ($(subdir),misc)
sysdep_routines += ioperm iopl
endif

ifeq ($(subdir),stdlib)
sysdep_routines += __start_context allocate-shadow-stack
endif

ifeq ($(subdir),csu)
gen-as-const-headers += ucontext_i.sym
endif

ifeq ($(subdir),misc)
gen-as-const-headers += sigaltstack-offsets.sym
endif

ifeq ($(subdir),elf)
ifeq (yes,$(enable-x86-isa-level))
tests += \
  tst-glibc-hwcaps-2 \
# tests
ifeq (no,$(build-hardcoded-path-in-tests))
# This is an ld.so.cache test, and RPATH/RUNPATH in the executable
# interferes with its test objectives.
tests-container += \
  tst-glibc-hwcaps-2-cache \
# tests-container
endif
modules-names += \
  libx86-64-isa-level-1 \
  libx86-64-isa-level-2 \
  libx86-64-isa-level-3 \
  libx86-64-isa-level-4 \
# modules-names

$(objpfx)tst-glibc-hwcaps-2: $(objpfx)libx86-64-isa-level.so

$(objpfx)tst-glibc-hwcaps-2.out: \
  $(objpfx)glibc-hwcaps/x86-64-v2/libx86-64-isa-level.so \
  $(objpfx)glibc-hwcaps/x86-64-v4/libx86-64-isa-level.so \
  $(objpfx)glibc-hwcaps/x86-64-v3/libx86-64-isa-level.so
$(objpfx)glibc-hwcaps/x86-64-v2/libx86-64-isa-level.so: \
  $(objpfx)libx86-64-isa-level-2.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/x86-64-v3/libx86-64-isa-level.so: \
  $(objpfx)libx86-64-isa-level-3.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/x86-64-v4/libx86-64-isa-level.so: \
  $(objpfx)libx86-64-isa-level-4.so
	$(make-target-directory)
	cp $< $@

CFLAGS-libx86-64-isa-level-1.os += -march=x86-64
CFLAGS-libx86-64-isa-level-2.os += -march=x86-64
CFLAGS-libx86-64-isa-level-3.os += -march=x86-64
CFLAGS-libx86-64-isa-level-4.os += -march=x86-64

# The test modules are parameterized by preprocessor macros.
LDFLAGS-libx86-64-isa-level-1.so += -Wl,-soname,libx86-64-isa-level.so
LDFLAGS-libx86-64-isa-level-4.so += -Wl,-soname,libx86-64-isa-level.so
$(objpfx)libx86-64-isa-level%.os: $(..)/sysdeps/unix/sysv/linux/x86_64/x86-64-isa-level-VALUE.c
	$(compile-command.c) -DVALUE=$(lastword $(subst -, ,$*)) \
	  -DISA_LEVEL="(1 << ($(lastword $(subst -, ,$*)) - 1))"
$(objpfx)libx86-64-isa-level.so: $(objpfx)libx86-64-isa-level-1.so
	cp $< $@
endif

ifeq (yesgnu2,$(have-mamx-tile)$(have-test-mtls-descriptor))
tests += \
  tst-gnu2-tls2-amx \
# tests

modules-names += \
  tst-gnu2-tls2-amx-mod0 \
  tst-gnu2-tls2-amx-mod1 \
  tst-gnu2-tls2-amx-mod2 \
# modules-names

$(objpfx)tst-gnu2-tls2-amx: $(shared-thread-library)
$(objpfx)tst-gnu2-tls2-amx.out: \
  $(objpfx)tst-gnu2-tls2-amx-mod0.so \
  $(objpfx)tst-gnu2-tls2-amx-mod1.so \
  $(objpfx)tst-gnu2-tls2-amx-mod2.so
$(objpfx)tst-gnu2-tls2-amx-mod0.so: $(libsupport)
$(objpfx)tst-gnu2-tls2-amx-mod1.so: $(libsupport)
$(objpfx)tst-gnu2-tls2-amx-mod2.so: $(libsupport)

CFLAGS-tst-gnu2-tls2-amx.c += -mamx-tile -DTEST_AMX
CFLAGS-tst-gnu2-tls2-amx-mod0.c += -mamx-tile -DTEST_AMX -mtls-dialect=gnu2
CFLAGS-tst-gnu2-tls2-amx-mod1.c += -mamx-tile -DTEST_AMX -mtls-dialect=gnu2
CFLAGS-tst-gnu2-tls2-amx-mod2.c += -mamx-tile -DTEST_AMX -mtls-dialect=gnu2
endif

endif # $(subdir) == elf

ifneq ($(enable-cet),no)
ifeq ($(subdir),elf)
tests += tst-cet-property-1 tst-cet-property-2

CFLAGS-tst-cet-property-1.o += -fcf-protection
ASFLAGS-tst-cet-property-dep-2.o += -fcf-protection

$(objpfx)tst-cet-property-2: $(objpfx)tst-cet-property-dep-2.o
$(objpfx)tst-cet-property-2.out: $(objpfx)tst-cet-property-2 \
				 $(objpfx)tst-cet-property-1.out
	env $(run-program-env) $(test-via-rtld-prefix) \
	  $(objpfx)tst-cet-property-2 \
	  < $(objpfx)tst-cet-property-1.out > $@; \
	  $(evaluate-test)
endif

ifeq ($(subdir),posix)
tests += tst-cet-vfork-1
CFLAGS-tst-cet-vfork-1.c += -mshstk
endif

ifeq ($(subdir),stdlib)
tests += tst-cet-setcontext-1
CFLAGS-tst-cet-setcontext-1.c += -mshstk
endif
endif
