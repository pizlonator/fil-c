ifeq ($(subdir),math)
CFLAGS-e_sqrt.c += $(config-cflags-nofma)

# The symbols alias to lround
CFLAGS-s_llround.c += -fno-builtin-lround
endif

ifeq ($(subdir),stdio-common)
fmt-xscanf-real-convs += double
endif
