ifeq ($(subdir),math)
# lrintf and llrintf are aliased to llrint, so suppress compiler builtins to
# avoid mismatched signatures.
CFLAGS-s_llrint.c += -fno-builtin-lrintf -fno-builtin-llrintf \
		     -fno-builtin-lrintf32 -fno-builtin-llrintf32
# Same as before but for lroundf and llroundf
CFLAGS-s_llround.c += -fno-builtin-lroundf -fno-builtin-llroundf \
		      -fno-builtin-lroundf32 -fno-builtin-llroundf32
endif
