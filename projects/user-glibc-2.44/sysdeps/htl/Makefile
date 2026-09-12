ifeq ($(subdir),rt)
sysdep_routines += timer_routines
endif

ifeq ($(subdir),posix)
CFLAGS-confstr.c += -DLIBPTHREAD_VERSION='"HTL $(version)"'
endif

ifeq ($(subdir),elf)
sysdep-dl-routines += pt-static-tls
endif
