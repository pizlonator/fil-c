ifeq ($(subdir),csu)
ifeq (yes,$(build-shared))
sysdep_routines += divdi3
shared-only-routines += divdi3
CPPFLAGS-divdi3.c = -Din_divdi3_c
endif
endif

ifeq ($(subdir),conform)
# For bugs 23081, 23082, 23083, 23084, 23085, 23086.
# (type conformance)
conformtest-xfail-conds += i386-gnu
endif
