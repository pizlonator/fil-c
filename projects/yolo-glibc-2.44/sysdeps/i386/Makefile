# The mpn functions need a #define for asm syntax flavor.
# Every i386 port in use uses gas syntax (I think).
asm-CPPFLAGS += -DGAS_SYNTAX

# The i386 `long double' is a distinct type we support.
long-double-fcts = yes

ifeq ($(subdir),math)
# The CORE-MATH implementation assumes FLT_EVAL_METHOD == 0 to provide
# correctly rounded results.
CFLAGS-e_cosh.c += -fexcess-precision=standard
CFLAGS-e_lgamma_r.c += -fexcess-precision=standard
CFLAGS-e_gamma_r.c += -fexcess-precision=standard
CFLAGS-e_sinh.c += -fexcess-precision=standard
CFLAGS-s_erf.c += -fexcess-precision=standard
CFLAGS-s_erfc.c += -fexcess-precision=standard
CFLAGS-s_erf_common.c += -fexcess-precision=standard
CFLAGS-s_fma.c += -fexcess-precision=standard
CFLAGS-s_fmaf.c += -fexcess-precision=standard
CFLAGS-s_tanh.c += -fexcess-precision=standard
endif

ifeq ($(subdir),gmon)
sysdep_routines += i386-mcount
ifeq ($(build-pie-default)$(have-test-cc-cflags-mfentry),noyes)
# Compile with -mfentry to avoid GCC 16 error:
# cc1: error: ‘-pg’ without ‘-mfentry’ may be unreliable with shrink wrapping [-Werror]
CFLAGS-tst-gmon.c += -mfentry
CFLAGS-tst-mcleanup.c += -mfentry
CFLAGS-tst-mcount-overflow.c += -mfentry
endif
endif

ifeq ($(subdir),csu)
gen-as-const-headers += link-defines.sym
gen-as-const-headers += tlsdesc.sym
else
stack-align-test-flags += -malign-double
endif

# Make sure no code in ld.so uses mm/xmm/ymm/zmm registers on i386 since
# the first 3 mm/xmm/ymm/zmm registers are used to pass vector parameters
# which must be preserved.
# With SSE disabled, ensure -fpmath is not set to use sse either.
rtld-CFLAGS += -mno-sse -mno-mmx -mfpmath=387
ifeq ($(subdir),elf)
CFLAGS-rtld.c += -Wno-uninitialized -Wno-unused
CFLAGS-dl-load.c += -Wno-unused
CFLAGS-dl-reloc.c += -Wno-unused
sysdep-dl-routines += \
  dl-tls-get-addr \
# sysdep-dl-routines

tests += tst-audit3
modules-names += tst-auditmod3a tst-auditmod3b

$(objpfx)tst-audit3: $(objpfx)tst-auditmod3a.so
$(objpfx)tst-audit3.out: $(objpfx)tst-auditmod3b.so
tst-audit3-ENV = LD_AUDIT=$(objpfx)tst-auditmod3b.so
CFLAGS-.os += $(if $(filter $(@F),$(patsubst %,%.os,$(all-rtld-routines))),\
		   $(rtld-CFLAGS))

tests-special += $(objpfx)tst-ld-sse-use.out
$(objpfx)tst-ld-sse-use.out: ../sysdeps/i386/tst-ld-sse-use.sh $(objpfx)ld.so
	@echo "Checking ld.so for SSE register use.  This will take a few seconds..."
	$(BASH) $< $(objpfx) '$(NM)' '$(OBJDUMP)' '$(READELF)' > $@; \
	$(evaluate-test)

tests-special += $(objpfx)check-gnu-tls.out

$(objpfx)check-gnu-tls.out: $(common-objpfx)libc.so
	LC_ALL=C $(READELF) -V -W $< \
		| sed -ne '/.gnu.version_d/, /.gnu.version_r/ p' \
		| grep GLIBC_ABI_GNU_TLS > $@; \
	$(evaluate-test)
generated += check-gnu-tls.out
else
CFLAGS-.os += $(if $(filter rtld-%.os,$(@F)), $(rtld-CFLAGS))
endif

ifeq ($(build-pie-default)$(build-profile)$(have-cc-cflags-mfentry),noyesyes)
# Compile .op files with -mfentry to avoid GCC 16 error:
# cc1: error: ‘-pg’ without ‘-mfentry’ may be unreliable with shrink wrapping [-Werror]
CFLAGS-.op += -mfentry
endif
