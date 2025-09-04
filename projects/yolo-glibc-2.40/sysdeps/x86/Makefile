ifeq ($(subdir),csu)
gen-as-const-headers += cpu-features-offsets.sym features-offsets.sym
endif

ifeq ($(subdir),elf)
sysdep_routines += get-cpuid-feature-leaf
sysdep-dl-routines += dl-get-cpu-features
sysdep_headers += \
  bits/platform/features.h \
  bits/platform/x86.h \
  sys/platform/x86.h \
# sysdep_headers

CFLAGS-dl-get-cpu-features.os += $(rtld-early-cflags)
CFLAGS-get-cpuid-feature-leaf.o += $(no-stack-protector)

tests += \
  tst-cpu-features-cpuinfo \
  tst-cpu-features-cpuinfo-static \
  tst-cpu-features-supports \
  tst-cpu-features-supports-static \
  tst-get-cpu-features \
  tst-get-cpu-features-static \
  tst-hwcap-tunables \
# tests
tests-static += \
  tst-cpu-features-cpuinfo-static \
  tst-cpu-features-supports-static \
  tst-get-cpu-features-static \
# tests-static
ifeq (yes,$(have-ifunc))
ifeq (yes,$(have-gcc-ifunc))
tests += \
  tst-ifunc-isa-1 \
  tst-ifunc-isa-1-static \
# tests
tests-static += \
  tst-ifunc-isa-1-static \
# tests-static
test-xfail-tst-ifunc-isa-1 = $(with-lld)
test-xfail-tst-ifunc-isa-1-static = $(with-lld)
tests += \
  tst-ifunc-isa-2 \
  tst-ifunc-isa-2-static \
# tests
tests-static += \
  tst-ifunc-isa-2-static \
# tests-static
test-xfail-tst-ifunc-isa-2 = $(with-lld)
test-xfail-tst-ifunc-isa-2-static = $(with-lld)
endif
endif
ifeq (yes,$(enable-x86-isa-level))
tests += \
  tst-isa-level-1 \
# tests
modules-names += \
  tst-isa-level-mod-1-baseline \
  tst-isa-level-mod-1-v2 \
  tst-isa-level-mod-1-v3 \
  tst-isa-level-mod-1-v4 \
# modules-names

# X86 ISA level baseline
CFLAGS-tst-isa-level-mod-1-baseline.c += -DINCLUDE_X86_ISA_LEVEL \
					 -DISA_LEVEL=0x1 \
					 -march=x86-64
# X86 ISA level v2
CFLAGS-tst-isa-level-mod-1-v2.c += -DINCLUDE_X86_ISA_LEVEL \
				   -DISA_LEVEL=0x3 \
				   -march=x86-64
# X86 ISA level v3
CFLAGS-tst-isa-level-mod-1-v3.c += -DINCLUDE_X86_ISA_LEVEL \
				   -DISA_LEVEL=0x7 \
				   -march=x86-64
# X86 ISA level v4
CFLAGS-tst-isa-level-mod-1-v4.c += -DINCLUDE_X86_ISA_LEVEL \
				   -DISA_LEVEL=0xf \
				   -march=x86-64

$(objpfx)tst-isa-level-1.out: $(objpfx)tst-isa-level-mod-1-baseline.so \
			      $(objpfx)tst-isa-level-mod-1-v2.so \
			      $(objpfx)tst-isa-level-mod-1-v3.so \
			      $(objpfx)tst-isa-level-mod-1-v4.so
endif
tst-ifunc-isa-2-ENV = GLIBC_TUNABLES=glibc.cpu.hwcaps=-SSE4_2,-AVX,-AVX2,-AVX512F
tst-ifunc-isa-2-static-ENV = $(tst-ifunc-isa-2-ENV)
tst-hwcap-tunables-ARGS = -- $(host-test-program-cmd)

CFLAGS-tst-gnu2-tls2.c += -msse
CFLAGS-tst-gnu2-tls2mod0.c += -msse2 -mtune=haswell
CFLAGS-tst-gnu2-tls2mod1.c += -msse2 -mtune=haswell
CFLAGS-tst-gnu2-tls2mod2.c += -msse2 -mtune=haswell
endif

ifeq ($(subdir),math)
tests += \
 tst-ldbl-nonnormal-printf \
# tests
endif # $(subdir) == math

ifeq ($(subdir),setjmp)
gen-as-const-headers += jmp_buf-ssp.sym
sysdep_routines += __longjmp_cancel
endif

ifeq ($(subdir),string)
sysdep_routines += cacheinfo

tests += \
  tst-memchr-rtm \
  tst-memcmp-rtm \
  tst-memmove-rtm \
  tst-memrchr-rtm \
  tst-memset-rtm \
  tst-strcasecmp-rtm \
  tst-strchr-rtm \
  tst-strcmp-rtm \
  tst-strcpy-rtm \
  tst-strlen-rtm \
  tst-strncasecmp-rtm \
  tst-strncmp-rtm \
  tst-strrchr-rtm \
  tst-wcscmp-rtm \
  tst-wcsncmp-rtm \
# tests

CFLAGS-tst-memchr-rtm.c += -mrtm
CFLAGS-tst-memcmp-rtm.c += -mrtm
CFLAGS-tst-memmove-rtm.c += -mrtm
CFLAGS-tst-memrchr-rtm.c += -mrtm
CFLAGS-tst-memset-rtm.c += -mrtm
CFLAGS-tst-strcasecmp-rtm.c += -mrtm
CFLAGS-tst-strchr-rtm.c += -mrtm
CFLAGS-tst-strcmp-rtm.c += -mrtm
CFLAGS-tst-strcpy-rtm.c += -mrtm
CFLAGS-tst-strlen-rtm.c += -mrtm
CFLAGS-tst-strncasecmp-rtm.c += -mrtm -Wno-error
CFLAGS-tst-strncmp-rtm.c += -mrtm -Wno-error
CFLAGS-tst-strrchr-rtm.c += -mrtm
CFLAGS-tst-wcscmp-rtm.c += -mrtm
CFLAGS-tst-wcsncmp-rtm.c += -mrtm -Wno-error
endif

ifeq ($(subdir),posix)
tests += \
  tst-sysconf-cache-linesize \
  tst-sysconf-cache-linesize-static \
# tests
tests-static += \
  tst-sysconf-cache-linesize-static \
# tests-static
endif
