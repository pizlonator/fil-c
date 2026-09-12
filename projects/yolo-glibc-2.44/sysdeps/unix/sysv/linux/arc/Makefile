ifeq ($(subdir),stdlib)
gen-as-const-headers += ucontext_i.sym
endif

ifeq ($(subdir),signal)
sysdep_routines += sigrestorer
endif

ifeq ($(subdir),misc)
# MIPS/Tile-style cacheflush routine
sysdep_headers += sys/cachectl.h
sysdep_routines += cacheflush
endif
