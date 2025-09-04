ifeq ($(subdir),misc)
sysdep_routines += ioperm
sysdep_headers += sys/io.h
endif

ifeq ($(subdir),debug)
gen-as-const-headers += signal-defines.sym
endif

ifeq ($(subdir),setjmp)
gen-as-const-headers += signal-defines.sym
endif

ifeq ($(subdir),stdlib)
gen-as-const-headers += ucontext_i.sym
endif
