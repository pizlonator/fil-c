# The default ABI is x32.
default-abi := x32

ifeq ($(subdir),misc)
sysdep_routines += arch_prctl

tests += \
  tst-prctl-x32 \
# tests

CFLAGS-tst-prctl-x32.c += $(no-stack-protector)
endif

ifeq ($(subdir),conform)
# For bugs 16437 and 21279.
conformtest-xfail-conds += x86_64-x32-linux
endif
