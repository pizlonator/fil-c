ifeq ($(subdir),misc)
tests += tst-writev
endif

# strtol is aliased to stroll
CFLAGS-strtol.c += -fno-builtin-strtoll $(config-cflags-wno-ignored-attributes)
# strtoul is aliased to strtoull
CFLAGS-strtoul.c += -fno-builtin-strtoull $(config-cflags-wno-ignored-attributes)
