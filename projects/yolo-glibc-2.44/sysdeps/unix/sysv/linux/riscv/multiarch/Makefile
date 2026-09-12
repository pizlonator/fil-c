ifeq ($(subdir),string)
sysdep_routines += \
  memccpy \
  memccpy-generic \
  memccpy-vector \
  memchr \
  memchr-generic \
  memchr-vector \
  memcmp \
  memcmp-generic \
  memcmp-vector \
  memcpy \
  memcpy-generic \
  memcpy-vector \
  memcpy_noalignment \
  memmove \
  memmove-generic \
  memmove-vector \
  memset \
  memset-generic \
  memset-vector \
  stpncpy \
  stpncpy-generic \
  stpncpy-vector \
  strcat \
  strcat-generic \
  strcat-vector \
  strchr \
  strchr-generic \
  strchr-vector \
  strcmp \
  strcmp-generic \
  strcmp-vector \
  strcpy \
  strcpy-generic \
  strcpy-vector \
  strlen \
  strlen-generic \
  strlen-vector \
  strncmp \
  strncmp-generic \
  strncmp-vector \
  strncpy \
  strncpy-generic \
  strncpy-vector \
  strrchr \
  strrchr-generic \
  strrchr-vector \
  # sysdep_routines

CFLAGS-memcpy_noalignment.c += -mno-strict-align
# Called during static initialization
CFLAGS-memset-generic.c += $(no-stack-protector)
CFLAGS-memcpy-generic.c += $(no-stack-protector)
endif
