ifeq ($(subdir),elf)
sysdep-rtld-routines += aeabi_read_tp libc-do-syscall
# The test uses INTERNAL_SYSCALL_CALL.  In thumb mode, this uses
# an undefined reference to __libc_do_syscall.
CFLAGS-tst-nolink-libc.c += -marm

# These static tests fail on arm due to limitations of arm32 kABI:
# https://sourceware.org/bugzilla/show_bug.cgi?id=34096
test-xfail-tst-thp-1-no-s-code-static = yes
test-xfail-tst-thp-1-static = yes
endif

ifeq ($(subdir),misc)
sysdep_routines += ioperm
sysdep_headers += sys/elf.h
endif

ifeq ($(subdir),signal)
sysdep_routines += sigrestorer
endif

ifeq ($(subdir),stdlib)
gen-as-const-headers += ucontext_i.sym
endif

# Add a syscall function to each library that needs one.

ifeq ($(subdir),rt)
librt-sysdep_routines += libc-do-syscall
librt-shared-only-routines += libc-do-syscall
endif

ifeq ($(subdir),resolv)
libanl-sysdep_routines += libc-do-syscall
libanl-shared-only-routines += libc-do-syscall
endif

ifeq ($(subdir),csu)
sysdep_routines += libc-do-syscall
endif

ifeq ($(subdir),nscd)
nscd-modules += libc-do-syscall
endif

ifeq ($(subdir),nss)
libnss_db-sysdep_routines += libc-do-syscall
libnss_db-shared-only-routines += libc-do-syscall
LDFLAGS-tst-rfc3484 += $(common-objpfx)csu/libc-do-syscall.o
LDFLAGS-tst-rfc3484-2 += $(common-objpfx)csu/libc-do-syscall.o
LDFLAGS-tst-rfc3484-3 += $(common-objpfx)csu/libc-do-syscall.o
endif

abi-variants := soft hard

ifeq (,$(filter $(default-abi),$(abi-variants)))
Unknown ABI, must be one of $(abi-variants)
endif

abi-includes :=
abi-soft-condition := !defined __ARM_PCS_VFP
abi-hard-condition := defined __ARM_PCS_VFP
