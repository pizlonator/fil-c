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

ifeq ($(subdir),mach)
# Avoid SSE&MMX to avoid __sigreturn2 thrashing it
CFLAGS-RPC_mach_port_mod_refs.c = -mno-sse -mno-mmx
CFLAGS-RPC_gsync_wake.c = -mno-sse -mno-mmx
endif
