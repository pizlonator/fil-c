ifeq ($(subdir),math)
libm-sysdep_routines += \
  s_fma-generic \
  s_fma-vpfv4 \
  s_fmaf-generic \
  s_fmaf-vpfv4 \
# libm-sysdep_routinesA

CFLAGS-s_fma-vpfv4.c = -mfpu=vfpv4
CFLAGS-s_fmaf-vpfv4.c = -mfpu=vfpv4
endif
