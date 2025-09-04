ifeq ($(subdir),string)
sysdep_routines += \
  memcpy \
  memcpy-generic \
  memcpy_noalignment \
  # sysdep_routines

CFLAGS-memcpy_noalignment.c += -mno-strict-align
endif
