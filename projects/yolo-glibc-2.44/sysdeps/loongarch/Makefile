ifeq ($(subdir),misc)
sysdep_headers += \
  sys/asm.h \
  # sysdep_headers

tests += \
  tst-hwcap-tunables \
  # tests

tst-hwcap-tunables-ARGS = -- $(host-test-program-cmd)
endif

ifeq ($(subdir),elf)
sysdep-dl-routines += \
  dl-tlsdesc \
  tlsdesc \
  # sysdep-dl-routines

gen-as-const-headers += \
  dl-link.sym \
  # gen-as-const-headers
endif

ifeq ($(subdir),csu)
gen-as-const-headers += \
  tlsdesc.sym \
  # gen-as-const-headers
endif

ifeq ($(subdir),elf)
sysdep-dl-routines += \
  dl-get-cpu-features \
  # sysdep-dl-routines

# Disable the compiler from using LSX for TLS descriptor tests, or storing into
# 16B TLS variable may clobber FP/vector registers and prevent us from checking
# their contents.
ifeq (yes,$(loongarch-vec-com))
CFLAGS-tst-gnu2-tls2mod0.c += -mno-lsx
CFLAGS-tst-gnu2-tls2mod1.c += -mno-lsx
CFLAGS-tst-gnu2-tls2mod2.c += -mno-lsx
endif
endif

# LoongArch's assembler also needs to know about PIC as it changes the
# definition of some assembler macros.
ASFLAGS-.os += \
  $(pic-ccflag) \
  # ASFLAGS-.os

# All the objects in lib*_nonshared.a need to be compiled with medium code
# model or large applications may fail to link.
ifeq (yes,$(have-cmodel-medium))
CFLAGS-.oS += \
  -mcmodel=medium \
  # CFLAGS-.oS
endif
