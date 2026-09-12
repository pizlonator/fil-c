abi-variants := 32 64
abi-32-condition := __WORDSIZE == 32
abi-64-condition := __WORDSIZE == 64

ifeq ($(subdir),rt)
librt-routines += rt-sysdep
librt-shared-only-routines += rt-sysdep
endif

ifeq ($(subdir),sysvipc)
sysdep_routines += getshmlba
endif

ifeq ($(subdir),signal)
sysdep_routines += sigreturn_stub
endif

ifeq ($(subdir),elf)
# The sparc64 uses an 8KB base page and 8MB PMD transparent huge page
THP-PAGE-SIZE = 0x800000
endif
