# ==========================================================================
# Build system
# ==========================================================================

# Allow i486 insns (basically, bswap insn)
# Do not try to tune for 486+ (might add padding)
CFLAGS += $(call cc-option,-march=i486 -mtune=i386,)

ifeq ($(CONFIG_STACK_OPTIMIZATION_386),y)
# -mpreferred-stack-boundary=2 is essential in preventing gcc 4.2.x
# from aligning stack to 16 bytes. (Which is gcc's way of supporting SSE).
CFLAGS += $(call cc-option,-mpreferred-stack-boundary=2,)
endif

# "Control how GCC aligns variables.
# Supported values for type are compat uses increased alignment value
# compatible uses GCC 4.8 and earlier, abi uses alignment value as specified by the psABI,
# and cacheline uses increased alignment value to match the cache line size.
# compat is the default."
# "abi" seems to be somewhat successful in preventing oversealous data alignment.
CFLAGS += $(call cc-option,-malign-data=abi,)
