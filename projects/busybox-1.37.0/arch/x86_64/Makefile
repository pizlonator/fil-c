# ==========================================================================
# Build system
# ==========================================================================

# "Control how GCC aligns variables.
# Supported values for type are compat uses increased alignment value
# compatible uses GCC 4.8 and earlier, abi uses alignment value as specified by the psABI,
# and cacheline uses increased alignment value to match the cache line size.
# compat is the default."
# "abi" seems to be somewhat successful in preventing oversealous data alignment.
CFLAGS += $(call cc-option,-malign-data=abi,)
