# The default ABI is 32.
default-abi := 32

ifeq ($(subdir),misc)
sysdep_routines += ioperm iopl vm86

tests += tst-bz21269
endif

ifeq ($(subdir),io)
sysdep_routines += libc-do-syscall libc-do-syscall-int80
endif

ifeq ($(subdir),stdlib)
gen-as-const-headers += ucontext_i.sym
endif

ifeq ($(subdir),csu)
ifeq (yes,$(build-shared))
sysdep_routines += divdi3
shared-only-routines += divdi3
CPPFLAGS-divdi3.c = -Din_divdi3_c
endif
endif

ifeq ($(subdir),rt)
# pull in __syscall_error routine
librt-routines += sysdep
librt-shared-only-routines += sysdep
endif
