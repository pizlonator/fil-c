ifeq ($(subdir),elf)
ifeq (yes,$(build-shared))
# This is needed to support g++ v2 and v3.
sysdep_routines += \
  framestate \
  # sysdep_routines
shared-only-routines += \
  framestate \
  # shared-only-routines
endif
endif

ifeq ($(subdir),rt)
librt-routines += \
  rt-sysdep \
  # librt-routines

librt-shared-only-routines += \
  rt-sysdep \
  # librt-shared-only-routines
endif

ifeq ($(subdir),stdlib)
gen-as-const-headers += \
  ucontext_i.sym \
  # gen-as-const-headers
sysdep_routines += \
  __makecontext_ret \
  # sysdep_routines
endif

ifeq ($(subdir),misc)
tests += \
  tst-ptrace-singleblock \
  # tests
sysdep_headers += \
  sys/elf.h \
  # sysdep_headers
endif
