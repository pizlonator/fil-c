ifeq ($(subdir),misc)
tests += tst-writev
endif

ifeq ($(subdir),stdio-common)
fmt-xscanf-int-convs += int long
fmt-xscanf-uint-convs += uint ulong
endif

# strtol is aliased to stroll
CFLAGS-strtol.c += -fno-builtin-strtoll
# strtoul is aliased to strtoull
CFLAGS-strtoul.c += -fno-builtin-strtoull
