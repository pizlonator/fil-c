abi-variants := 32 64-v1 64-v2
abi-32-condition := __WORDSIZE == 32
abi-64-v1-condition := __WORDSIZE == 64 && _CALL_ELF != 2
abi-64-v2-condition := __WORDSIZE == 64 && _CALL_ELF == 2

ifeq ($(subdir),rt)
librt-routines += rt-sysdep
librt-shared-only-routines += rt-sysdep
endif

ifeq ($(subdir),stdlib)
gen-as-const-headers += ucontext_i.sym
endif

ifeq ($(subdir),elf)
dl-routines += \
  dl-hwcap-info \
  # dl-routines

# Otherwise tst-tls-dlinfo fails due to tst-tlsmod2.so using static tls.
ifeq ($(have-tls-get-addr-optimize),yes)
LDFLAGS-tst-tlsmod2.so += -Wl,--no-tls-get-addr-optimize
endif
endif

ifeq ($(subdir),misc)
sysdep_headers += bits/ppc.h
sysdep_routines += get_timebase_freq
tests-static += test-gettimebasefreq-static
tests += \
  $(tests-static) \
  test-gettimebasefreq \
  test-powerpc-linux-sysconf \
  tst-hwcap-tunables \
  # tests

tst-hwcap-tunables-ARGS = -- $(host-test-program-cmd)
endif

ifeq ($(subdir),csu)
# to relocate stinfo->main
CPPFLAGS-libc-start.o += -I../elf
CPPFLAGS-libc-start.op += -I../elf
endif
