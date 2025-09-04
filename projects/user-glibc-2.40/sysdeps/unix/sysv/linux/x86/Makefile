# We don't need any header files.
abi-includes :=

abi-variants := 32 64 x32

abi-32-condition := !defined __x86_64__
abi-64-condition := defined __x86_64__ && defined __LP64__
abi-x32-condition := defined __x86_64__ && defined __ILP32__

ifeq ($(subdir),misc)
sysdep_headers += sys/elf.h sys/perm.h sys/reg.h sys/vm86.h sys/debugreg.h sys/io.h
endif

ifeq ($(subdir),nptl)
CFLAGS-elision-lock.c += -mrtm
CFLAGS-elision-unlock.c += -mrtm
CFLAGS-elision-timed.c += -mrtm
CFLAGS-elision-trylock.c += -mrtm
endif

ifeq ($(subdir),setjmp)
tests += tst-saved_mask-1
endif
