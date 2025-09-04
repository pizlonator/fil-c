ifeq ($(subdir),mathvec)
libmvec-double-func-list = \
  2_core \
  4_core \
  4_core_avx \
  8_core
libmvec-float-func-list = \
  f4_core \
  f8_core \
  f8_core_avx \
  f16_core
libmvec-support += \
  svml_d_exp_data \
  svml_d_log_data \
  svml_d_pow_data \
  svml_d_trig_data \
  svml_s_expf_data \
  svml_s_logf_data \
  svml_s_powf_data \
  svml_s_trig_data \
  $(foreach l,$(libmvec-double-func-list), \
    $(addprefix svml_d_,$(addsuffix $(l),$(libmvec-funcs)))) \
  $(foreach l,$(libmvec-float-func-list), \
    $(addprefix svml_s_,$(addsuffix $(l),$(libmvec-funcs))))
endif

# Do not run libmvec tests if multiarch not enabled.
ifneq ($(multi-arch),no)
# Variables for libmvec tests.
ifeq ($(subdir)$(build-mathvec),mathyes)
libmvec-tests += double-vlen2 double-vlen4 double-vlen4-avx2 \
		 float-vlen4 float-vlen8 float-vlen8-avx2 \
		 double-vlen8 float-vlen16
tests += \
  $(libmvec-abi-func-tests) \
  $(libmvec-abi-func-avx-tests) \
  $(libmvec-abi-func-avx2-tests) \
  $(libmvec-abi-func-avx512f-tests)

double-vlen2-funcs = $(libmvec-funcs)
double-vlen4-funcs = $(libmvec-funcs)
double-vlen4-avx2-funcs = $(libmvec-funcs)
double-vlen8-funcs = $(libmvec-funcs)
float-vlen4-funcs = $(libmvec-funcs)
float-vlen8-funcs = $(libmvec-funcs)
float-vlen8-avx2-funcs = $(libmvec-funcs)
float-vlen16-funcs = $(libmvec-funcs)

double-vlen4-arch-ext-cflags = -mavx
double-vlen4-arch-ext2-cflags = -mavx2
double-vlen8-arch-ext-cflags = -mavx512f

float-vlen8-arch-ext-cflags = -mavx
float-vlen8-arch-ext2-cflags = -mavx2
float-vlen16-arch-ext-cflags = -mavx512f

libmvec-abi-test-cflags = \
  $(libm-test-fast-math-cflags) \
  -fno-inline -fopenmp -Wno-unknown-pragmas

CFLAGS-test-double-vlen4-avx2-wrappers.c = $(double-vlen4-arch-ext2-cflags)

CFLAGS-test-float-vlen8-avx2-wrappers.c = $(float-vlen8-arch-ext2-cflags)
endif

ifeq ($(subdir)$(config-cflags-mprefer-vector-width),mathyes)
# When compiled with -O3 -march=skylake, GCC 8 and 9 optimize some loops
# in branred.c with 256-bit vector instructions, which leads to store
# forward stall:
#
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90579
#
# Limit vector width to 128 bits to work around this issue.  It improves
# performance of sin and cos by more than 40% on Skylake.
CFLAGS-branred.c = -mprefer-vector-width=128
endif

ifeq ($(subdir)$(build-mathvec),benchtestsyes)
double-vlen4-arch-ext-cflags = -mavx
double-vlen4-arch-ext2-cflags = -mavx2
double-vlen8-arch-ext-cflags = -mavx512f

float-vlen8-arch-ext-cflags = -mavx
float-vlen8-arch-ext2-cflags = -mavx2
float-vlen16-arch-ext-cflags = -mavx512f

bench-libmvec := $(bench-libmvec-double) $(bench-libmvec-float)

ifeq (${STATIC-BENCHTESTS},yes)
libmvec-benchtests = $(common-objpfx)mathvec/libmvec.a $(common-objpfx)math/libm.a
else
libmvec-benchtests = $(libmvec) $(libm)
endif

$(addprefix $(objpfx)bench-,$(bench-libmvec-double)): $(libmvec-benchtests)
$(addprefix $(objpfx)bench-,$(bench-libmvec-float)): $(libmvec-benchtests)
bench-libmvec-deps = $(..)benchtests/bench-libmvec-skeleton.c $(..)sysdeps/x86_64/fpu/bench-libmvec-arch.h bench-timing.h Makefile

$(objpfx)bench-float-%.c: $(bench-libmvec-deps)
	{ if [ -n "$($*-INCLUDE)" ]; then \
	  cat $($*-INCLUDE); \
	fi; \
	$(PYTHON) $(..)sysdeps/x86_64/fpu/scripts/bench_libmvec.py $(basename $(@F)); } > $@-tmp
	mv -f $@-tmp $@

$(objpfx)bench-double-%.c: $(bench-libmvec-deps)
	{ if [ -n "$($*-INCLUDE)" ]; then \
	  cat $($*-INCLUDE); \
	fi; \
	$(PYTHON) $(..)sysdeps/x86_64/fpu/scripts/bench_libmvec.py $(basename $(@F)); } > $@-tmp
	mv -f $@-tmp $@
endif
endif
