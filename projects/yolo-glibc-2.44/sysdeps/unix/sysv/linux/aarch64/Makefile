ifeq ($(subdir),elf)
tests += \
  tst-tlsdesc-pac \
  # tests
modules-names += \
  tst-tlsdesc-pac-mod \
  # modules-names

LDFLAGS-tst-tlsdesc-pac = -rdynamic

$(objpfx)tst-tlsdesc-pac.out: $(objpfx)tst-tlsdesc-pac-mod.so

ifeq (yes,$(enable-static-pie))
tests += \
  tst-cpu-tunable-static-pie \
  # tests
tests-static += \
  tst-cpu-tunable-static-pie \
  # tests-static
tst-cpu-tunable-static-pie-TUNABLES = glibc.cpu.name=generic
endif
endif

ifeq ($(subdir),misc)
sysdep_headers += sys/elf.h
tests += \
  tst-aarch64-pkey \
  # tests

ifeq (yes,$(have-test-bti))

tests += \
  tst-bti-abort-imm \
  tst-bti-abort-static \
  tst-bti-abort-transitive \
  tst-bti-abort-unprot \
  tst-bti-abort-unprot-preload \
  tst-bti-dep-prot \
  tst-bti-dep-prot-preload \
  tst-bti-dlopen-imm \
  tst-bti-dlopen-prot \
  tst-bti-dlopen-transitive \
  tst-bti-ld-debug-both \
  tst-bti-ld-debug-dlopen \
  tst-bti-ld-debug-exe \
  tst-bti-ld-debug-shared \
  tst-bti-permissive-audit \
  tst-bti-permissive-dlopen \
  tst-bti-permissive-imm \
  tst-bti-permissive-preload \
  tst-bti-permissive-transitive \
  tst-bti-prot-audit \
  tst-bti-unprot-audit \
  # tests

modules-names += \
  tst-bti-mod \
  tst-bti-mod-prot \
  tst-bti-mod-prot-audit \
  tst-bti-mod-prot-preload \
  tst-bti-mod-unprot \
  tst-bti-mod-unprot-audit \
  tst-bti-mod-unprot-preload \
  # modules-names

$(objpfx)tst-bti-abort-imm: $(objpfx)tst-bti-mod-unprot.so
$(objpfx)tst-bti-abort-transitive: $(objpfx)tst-bti-mod.so
$(objpfx)tst-bti-abort-unprot: $(objpfx)tst-bti-mod-prot.so
$(objpfx)tst-bti-dep-prot: $(objpfx)tst-bti-mod-prot.so
$(objpfx)tst-bti-mod.so: $(objpfx)tst-bti-mod-unprot.so
$(objpfx)tst-bti-permissive-imm: $(objpfx)tst-bti-mod-unprot.so
$(objpfx)tst-bti-permissive-transitive: $(objpfx)tst-bti-mod.so
$(objpfx)tst-bti-ld-debug-shared: $(objpfx)tst-bti-mod.so
$(objpfx)tst-bti-ld-debug-both: $(objpfx)tst-bti-mod-unprot.so

$(objpfx)tst-bti-dlopen-imm.out: $(objpfx)tst-bti-mod-unprot.so
$(objpfx)tst-bti-dlopen-prot.out: $(objpfx)tst-bti-mod-prot.so
$(objpfx)tst-bti-dlopen-transitive.out: $(objpfx)tst-bti-mod.so
$(objpfx)tst-bti-ld-debug-dlopen.out: $(objpfx)tst-bti-mod-unprot.so
$(objpfx)tst-bti-permissive-dlopen.out: $(objpfx)tst-bti-mod-unprot.so

$(objpfx)tst-bti-abort-unprot-preload.out: $(objpfx)tst-bti-mod-unprot-preload.so
tst-bti-abort-unprot-preload-ENV = LD_PRELOAD=$(objpfx)tst-bti-mod-unprot-preload.so
tst-bti-abort-unprot-preload-TUNABLES = glibc.cpu.aarch64_bti=1

$(objpfx)tst-bti-dep-prot-preload.out: $(objpfx)tst-bti-mod-prot-preload.so
# The 'fun' function will be provided by the preload library
LDFLAGS-tst-bti-dep-prot-preload = -Wl,--unresolved-symbols=ignore-all
tst-bti-dep-prot-preload-ENV = LD_PRELOAD=$(objpfx)tst-bti-mod-prot-preload.so
tst-bti-dep-prot-preload-TUNABLES += glibc.cpu.aarch64_bti=0


$(objpfx)tst-bti-permissive-preload.out: $(objpfx)tst-bti-mod-unprot-preload.so
tst-bti-permissive-preload-ENV = LD_PRELOAD=$(objpfx)tst-bti-mod-unprot-preload.so
tst-bti-permissive-preload-TUNABLES += glibc.cpu.aarch64_bti=0

CFLAGS-tst-bti-abort-unprot.o += -mbranch-protection=none
CFLAGS-tst-bti-ld-debug-exe.o += -mbranch-protection=none
CFLAGS-tst-bti-ld-debug-both.o += -mbranch-protection=none
CFLAGS-tst-bti-mod-unprot.os += -mbranch-protection=none
CFLAGS-tst-bti-mod-unprot-preload.os += -mbranch-protection=none
CFLAGS-tst-bti-mod-unprot-audit.os += -mbranch-protection=none

tst-bti-abort-imm-TUNABLES = glibc.cpu.aarch64_bti=1
tst-bti-abort-transitive-TUNABLES = glibc.cpu.aarch64_bti=1
tst-bti-abort-unprot-TUNABLES = glibc.cpu.aarch64_bti=1
tst-bti-dep-prot-TUNABLES += glibc.cpu.aarch64_bti=1
tst-bti-dlopen-imm-TUNABLES += glibc.cpu.aarch64_bti=1
tst-bti-dlopen-prot-TUNABLES += glibc.cpu.aarch64_bti=1
tst-bti-dlopen-transitive-TUNABLES += glibc.cpu.aarch64_bti=1

tst-bti-permissive-imm-TUNABLES += glibc.cpu.aarch64_bti=0
tst-bti-permissive-transitive-TUNABLES += glibc.cpu.aarch64_bti=0
tst-bti-permissive-dlopen-TUNABLES-only = glibc.cpu.aarch64_bti=0

$(objpfx)tst-bti-unprot-audit.out: $(objpfx)tst-bti-mod-unprot-audit.so
tst-bti-unprot-audit-ARGS = -- $(host-test-program-cmd)

$(objpfx)tst-bti-prot-audit.out: $(objpfx)tst-bti-mod-prot-audit.so
$(objpfx)tst-bti-prot-audit: $(objpfx)tst-bti-mod-prot.so
tst-bti-prot-audit-ARGS = -- $(host-test-program-cmd)

tst-bti-permissive-audit-TUNABLES += glibc.cpu.aarch64_bti=0
$(objpfx)tst-bti-permissive-audit.out: $(objpfx)tst-bti-mod-unprot-audit.so
$(objpfx)tst-bti-permissive-audit: $(objpfx)tst-bti-mod.so
tst-bti-permissive-audit-ARGS = -- $(host-test-program-cmd)

define run-bti-abort-test
  $(test-wrapper-env) $(run-program-env) \
  $(tst-bti-abort-$*-ENV) \
  GLIBC_TUNABLES=$(subst $(empty) ,:,$(strip $(tst-bti-abort-$*-TUNABLES))) \
  $(host-test-program-cmd)
endef

$(objpfx)tst-bti-abort-%.out: $(..)sysdeps/unix/sysv/linux/aarch64/tst-bti-abort.sh \
	$(objpfx)tst-bti-abort-%
	$(SHELL) $< $(common-objpfx) $(test-name) '$(run-bti-abort-test)'; \
	$(evaluate-test)

tests-static += \
  tst-bti-abort-static \
  # tests-static

tst-bti-abort-static-TUNABLES = glibc.cpu.aarch64_bti=1
CFLAGS-tst-bti-abort-static.o += -mbranch-protection=none

$(objpfx)tst-bti-ld-debug-%.out: $(..)elf/tst-dl-debug-protect.sh $(objpfx)tst-bti-ld-debug-%
	$(SHELL) $< $(objpfx) '$(test-wrapper-env)' '$(rtld-prefix)' \
	  '$(run-program-env) GLIBC_TUNABLES=glibc.cpu.aarch64_bti=0' \
	  'security: not compatible with AArch64 BTI: $(objpfx)' \
	  $(objpfx)tst-bti-ld-debug-$* > $@; $(evaluate-test)

endif # ifeq (yes,$(have-test-bti))

ifeq (yes,$(have-test-gcs))

gcs-tests-dynamic = \
  tst-gcs-audit-disabled \
  tst-gcs-audit-enforced \
  tst-gcs-audit-optional \
  tst-gcs-audit-override \
  tst-gcs-disabled \
  tst-gcs-dlopen-disabled \
  tst-gcs-dlopen-enforced \
  tst-gcs-dlopen-optional-off \
  tst-gcs-dlopen-optional-on \
  tst-gcs-dlopen-override \
  tst-gcs-enforced \
  tst-gcs-enforced-abort \
  tst-gcs-execv \
  tst-gcs-fork \
  tst-gcs-ld-debug-both \
  tst-gcs-ld-debug-dlopen \
  tst-gcs-ld-debug-exe \
  tst-gcs-ld-debug-shared \
  tst-gcs-lock \
  tst-gcs-lock-ptrace \
  tst-gcs-noreturn \
  tst-gcs-optional-off \
  tst-gcs-optional-on \
  tst-gcs-override \
  tst-gcs-preload-disabled \
  tst-gcs-preload-enforced-abort \
  tst-gcs-preload-optional \
  tst-gcs-preload-override \
  tst-gcs-shared-disabled \
  tst-gcs-shared-enforced-abort \
  tst-gcs-shared-optional \
  tst-gcs-shared-override \
  tst-gcs-unlock \
  # gcs-tests-dynamic

gcs-tests-static = \
  tst-gcs-disabled-static \
  tst-gcs-enforced-static \
  tst-gcs-enforced-static-abort \
  tst-gcs-lock-static \
  tst-gcs-optional-static-off \
  tst-gcs-optional-static-on \
  tst-gcs-override-static \
  tst-gcs-unlock-static \
  # gcs-tests-static

tests += \
  $(gcs-tests-dynamic) \
  $(gcs-tests-static) \
  # tests

tests-static += \
  $(gcs-tests-static) \
  # tests-static

define run-gcs-abort-test
  $(test-wrapper-env) $(run-program-env) \
  $(tst-gcs-$*-abort-ENV) \
  GLIBC_TUNABLES=$(subst $(empty) ,:,$(strip $(tst-gcs-$*-abort-TUNABLES))) \
  $(host-test-program-cmd)
endef

$(objpfx)tst-gcs-%-abort.out: $(..)sysdeps/unix/sysv/linux/aarch64/tst-gcs-abort.sh \
	$(objpfx)tst-gcs-%-abort
	$(SHELL) $< $(common-objpfx) $(test-name) '$(run-gcs-abort-test)'; \
	$(evaluate-test)

LDFLAGS-tst-gcs-disabled += -Wl,-z,gcs=always
LDFLAGS-tst-gcs-enforced += -Wl,-z,gcs=always
LDFLAGS-tst-gcs-enforced-abort += -Wl,-z,gcs=never
LDFLAGS-tst-gcs-optional-on += -Wl,-z,gcs=always
LDFLAGS-tst-gcs-optional-off += -Wl,-z,gcs=never
LDFLAGS-tst-gcs-override += -Wl,-z,gcs=never

LDFLAGS-tst-gcs-disabled-static += -Wl,-z,gcs=always
LDFLAGS-tst-gcs-enforced-static += -Wl,-z,gcs=always
LDFLAGS-tst-gcs-enforced-static-abort += -Wl,-z,gcs=never
LDFLAGS-tst-gcs-optional-static-on += -Wl,-z,gcs=always
LDFLAGS-tst-gcs-optional-static-off += -Wl,-z,gcs=never
LDFLAGS-tst-gcs-override-static += -Wl,-z,gcs=never

tst-gcs-disabled-TUNABLES += glibc.cpu.aarch64_gcs=0
tst-gcs-enforced-TUNABLES += glibc.cpu.aarch64_gcs=1
tst-gcs-enforced-abort-TUNABLES += glibc.cpu.aarch64_gcs=1
tst-gcs-optional-on-TUNABLES += glibc.cpu.aarch64_gcs=2
tst-gcs-optional-off-TUNABLES += glibc.cpu.aarch64_gcs=2
tst-gcs-override-TUNABLES += glibc.cpu.aarch64_gcs=3

tst-gcs-disabled-static-TUNABLES += glibc.cpu.aarch64_gcs=0
tst-gcs-enforced-static-TUNABLES += glibc.cpu.aarch64_gcs=1
tst-gcs-enforced-static-abort-TUNABLES += glibc.cpu.aarch64_gcs=1
tst-gcs-optional-static-on-TUNABLES += glibc.cpu.aarch64_gcs=2
tst-gcs-optional-static-off-TUNABLES += glibc.cpu.aarch64_gcs=2
tst-gcs-override-static-TUNABLES += glibc.cpu.aarch64_gcs=3

LDFLAGS-tst-gcs-execv += -Wl,-z,gcs=always
tst-gcs-execv-TUNABLES += glibc.cpu.aarch64_gcs=1
tst-gcs-execv-ARGS = -- $(host-test-program-cmd)
LDFLAGS-tst-gcs-fork += -Wl,-z,gcs=always
tst-gcs-fork-TUNABLES += glibc.cpu.aarch64_gcs=2

LDFLAGS-tst-gcs-lock += -Wl,-z,gcs=always
tst-gcs-lock-TUNABLES += glibc.cpu.aarch64_gcs=1
LDFLAGS-tst-gcs-lock-ptrace += -Wl,-z,gcs=always
tst-gcs-lock-ptrace-TUNABLES += glibc.cpu.aarch64_gcs=1
tst-gcs-lock-ptrace-ARGS = -- $(host-test-program-cmd)
LDFLAGS-tst-gcs-lock-static += -Wl,-z,gcs=always
tst-gcs-lock-static-TUNABLES += glibc.cpu.aarch64_gcs=1
LDFLAGS-tst-gcs-unlock += -Wl,-z,gcs=always
tst-gcs-unlock-TUNABLES += glibc.cpu.aarch64_gcs=2
LDFLAGS-tst-gcs-unlock-static += -Wl,-z,gcs=always
tst-gcs-unlock-static-TUNABLES += glibc.cpu.aarch64_gcs=2

# force one of the dependencies to be unmarked
LDFLAGS-tst-gcs-mod2.so += -Wl,-z,gcs=never

LDFLAGS-tst-gcs-shared-disabled = -Wl,-z,gcs=always
LDFLAGS-tst-gcs-shared-enforced-abort = -Wl,-z,gcs=always
LDFLAGS-tst-gcs-shared-optional = -Wl,-z,gcs=always
LDFLAGS-tst-gcs-shared-override = -Wl,-z,gcs=always

LDFLAGS-tst-gcs-ld-debug-shared = -Wl,-z,gcs=always
LDFLAGS-tst-gcs-ld-debug-dlopen = -Wl,-z,gcs=always

modules-names += \
  tst-gcs-audit1 \
  tst-gcs-mod1 \
  tst-gcs-mod2 \
  tst-gcs-mod3 \
  tst-gcs-mod4 \
  # modules-names

$(objpfx)tst-gcs-shared-disabled: $(objpfx)tst-gcs-mod1.so $(objpfx)tst-gcs-mod3.so
$(objpfx)tst-gcs-shared-enforced-abort: $(objpfx)tst-gcs-mod1.so $(objpfx)tst-gcs-mod3.so
$(objpfx)tst-gcs-shared-optional: $(objpfx)tst-gcs-mod1.so $(objpfx)tst-gcs-mod3.so
$(objpfx)tst-gcs-shared-override: $(objpfx)tst-gcs-mod1.so $(objpfx)tst-gcs-mod3.so
$(objpfx)tst-gcs-mod1.so: $(objpfx)tst-gcs-mod2.so
$(objpfx)tst-gcs-ld-debug-both: $(objpfx)tst-gcs-mod2.so
$(objpfx)tst-gcs-ld-debug-shared: $(objpfx)tst-gcs-mod1.so $(objpfx)tst-gcs-mod3.so

tst-gcs-shared-disabled-TUNABLES += glibc.cpu.aarch64_gcs=0
tst-gcs-shared-enforced-abort-TUNABLES = glibc.cpu.aarch64_gcs=1
tst-gcs-shared-optional-TUNABLES += glibc.cpu.aarch64_gcs=2
tst-gcs-shared-override-TUNABLES += glibc.cpu.aarch64_gcs=3

LDFLAGS-tst-gcs-dlopen-disabled = -Wl,-z,gcs=always
LDFLAGS-tst-gcs-dlopen-enforced = -Wl,-z,gcs=always
LDFLAGS-tst-gcs-dlopen-optional-on = -Wl,-z,gcs=always
LDFLAGS-tst-gcs-dlopen-optional-off = -Wl,-z,gcs=never
LDFLAGS-tst-gcs-dlopen-override = -Wl,-z,gcs=always
LDFLAGS-tst-gcs-ld-debug-exe = -Wl,-z,gcs=never
LDFLAGS-tst-gcs-ld-debug-both = -Wl,-z,gcs=never

tst-gcs-dlopen-disabled-TUNABLES += glibc.cpu.aarch64_gcs=0
tst-gcs-dlopen-enforced-TUNABLES += glibc.cpu.aarch64_gcs=1
tst-gcs-dlopen-optional-on-TUNABLES += glibc.cpu.aarch64_gcs=2
tst-gcs-dlopen-optional-off-TUNABLES += glibc.cpu.aarch64_gcs=2
tst-gcs-dlopen-override-TUNABLES += glibc.cpu.aarch64_gcs=3

$(objpfx)tst-gcs-dlopen-disabled.out: $(objpfx)tst-gcs-mod2.so
$(objpfx)tst-gcs-dlopen-enforced.out: $(objpfx)tst-gcs-mod2.so
$(objpfx)tst-gcs-dlopen-optional-on.out: $(objpfx)tst-gcs-mod2.so
$(objpfx)tst-gcs-dlopen-optional-off.out: $(objpfx)tst-gcs-mod2.so
$(objpfx)tst-gcs-dlopen-override.out: $(objpfx)tst-gcs-mod2.so
$(objpfx)tst-gcs-ld-debug-dlopen.out: $(objpfx)tst-gcs-mod2.so

LDFLAGS-tst-gcs-noreturn = -Wl,-z,gcs=always

tst-gcs-noreturn-TUNABLES += glibc.cpu.aarch64_gcs=0

$(objpfx)tst-gcs-ld-debug-%.out: $(..)elf/tst-dl-debug-protect.sh $(objpfx)tst-gcs-ld-debug-%
	$(SHELL) $< $(objpfx) '$(test-wrapper-env)' '$(rtld-prefix)' \
	  '$(run-program-env) GLIBC_TUNABLES=glibc.cpu.aarch64_gcs=2' \
	  'security: not compatible with AArch64 GCS: $(objpfx)' \
	  $(objpfx)tst-gcs-ld-debug-$* > $@; $(evaluate-test)

LDFLAGS-tst-gcs-preload-disabled += -Wl,-z,gcs=always
LDFLAGS-tst-gcs-preload-enforced-abort = -Wl,-z,gcs=always
LDFLAGS-tst-gcs-preload-optional = -Wl,-z,gcs=always
LDFLAGS-tst-gcs-preload-override = -Wl,-z,gcs=always

$(objpfx)tst-gcs-preload-disable: $(objpfx)tst-gcs-mod1.so
$(objpfx)tst-gcs-preload-enforced-abort: $(objpfx)tst-gcs-mod1.so
$(objpfx)tst-gcs-preload-optional: $(objpfx)tst-gcs-mod1.so
$(objpfx)tst-gcs-preload-override: $(objpfx)tst-gcs-mod1.so

tst-gcs-preload-disabled-ENV = LD_PRELOAD=$(objpfx)tst-gcs-mod1.so
tst-gcs-preload-disabled-TUNABLES += glibc.cpu.aarch64_gcs=0

tst-gcs-preload-enforced-abort-ENV = LD_PRELOAD=$(objpfx)tst-gcs-mod1.so
tst-gcs-preload-enforced-abort-TUNABLES += glibc.cpu.aarch64_gcs=1

tst-gcs-preload-optional-ENV = LD_PRELOAD=$(objpfx)tst-gcs-mod1.so
tst-gcs-preload-optional-TUNABLES += glibc.cpu.aarch64_gcs=2

tst-gcs-preload-override-ENV = LD_PRELOAD=$(objpfx)tst-gcs-mod1.so
tst-gcs-preload-override-TUNABLES += glibc.cpu.aarch64_gcs=3

LDFLAGS-tst-gcs-audit1.so += -Wl,-z,gcs=never

LDFLAGS-tst-gcs-audit-disabled += -Wl,-z,gcs=always
LDFLAGS-tst-gcs-audit-enforced += -Wl,-z,gcs=always
LDFLAGS-tst-gcs-audit-optional += -Wl,-z,gcs=always

$(objpfx)tst-gcs-audit-disabled.out: $(objpfx)tst-gcs-audit1.so
$(objpfx)tst-gcs-audit-disabled: $(objpfx)tst-gcs-mod4.so
$(objpfx)tst-gcs-audit-enforced.out: $(objpfx)tst-gcs-audit1.so
$(objpfx)tst-gcs-audit-enforced: $(objpfx)tst-gcs-mod4.so
$(objpfx)tst-gcs-audit-optional.out: $(objpfx)tst-gcs-audit1.so
$(objpfx)tst-gcs-audit-optional: $(objpfx)tst-gcs-mod4.so
$(objpfx)tst-gcs-audit-override.out: $(objpfx)tst-gcs-audit1.so
$(objpfx)tst-gcs-audit-override: $(objpfx)tst-gcs-mod4.so

tst-gcs-audit-disabled-ARGS = -- $(host-test-program-cmd)
tst-gcs-audit-enforced-ARGS = -- $(host-test-program-cmd)
tst-gcs-audit-optional-ARGS = -- $(host-test-program-cmd)
tst-gcs-audit-override-ARGS = -- $(host-test-program-cmd)

endif # ifeq ($(have-test-gcs),yes)

endif # ifeq ($(subdir),misc)

ifeq ($(subdir),stdlib)
gen-as-const-headers += ucontext_i.sym
endif

abi-variants := lp64
abi-variants += lp64_be

ifeq (,$(filter $(default-abi),$(abi-variants)))
Unknown ABI, must be one of $(abi-variants)
endif

abi-lp64-condition := !defined __AARCH64EB__
abi-lp64_be-condition := defined __AARCH64EB__
