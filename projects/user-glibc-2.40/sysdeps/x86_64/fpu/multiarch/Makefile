ifeq ($(subdir),math)
CFLAGS-e_asin-fma.c = -mfma -mavx2
CFLAGS-e_atan2-fma.c = -mfma -mavx2
CFLAGS-e_exp-fma.c = -mfma -mavx2
CFLAGS-e_log-fma.c = -mfma -mavx2
CFLAGS-e_log2-fma.c = -mfma -mavx2
CFLAGS-e_pow-fma.c = -mfma -mavx2
CFLAGS-s_atan-fma.c = -mfma -mavx2
CFLAGS-s_expm1-fma.c = -mfma -mavx2
CFLAGS-s_log1p-fma.c = -mfma -mavx2
CFLAGS-s_sin-fma.c = -mfma -mavx2
CFLAGS-s_tan-fma.c = -mfma -mavx2
CFLAGS-s_sincos-fma.c = -mfma -mavx2

CFLAGS-e_exp2f-fma.c = -mfma -mavx2
CFLAGS-e_expf-fma.c = -mfma -mavx2
CFLAGS-e_log2f-fma.c = -mfma -mavx2
CFLAGS-e_logf-fma.c = -mfma -mavx2
CFLAGS-e_powf-fma.c = -mfma -mavx2
CFLAGS-s_sinf-fma.c = -mfma -mavx2
CFLAGS-s_cosf-fma.c = -mfma -mavx2
CFLAGS-s_sincosf-fma.c = -mfma -mavx2

# Check if ISA level is 3 or above.
ifneq (,$(filter $(have-x86-isa-level),$(x86-isa-level-3-or-above)))
libm-sysdep_routines += \
  s_ceil-avx \
  s_ceilf-avx \
  s_floor-avx \
  s_floorf-avx \
  s_nearbyint-avx \
  s_nearbyintf-avx \
  s_rint-avx \
  s_rintf-avx \
  s_roundeven-avx \
  s_roundevenf-avx \
  s_trunc-avx \
  s_truncf-avx \
# libm-sysdep_routines
else
ifeq (no,$(have-x86-apx))
libm-sysdep_routines += \
  e_asin-fma4 \
  e_atan2-fma4 \
  e_exp-fma4 \
  e_log-fma4 \
  e_pow-fma4 \
  s_atan-fma4 \
  s_sin-fma4 \
  s_sincos-fma4 \
  s_tan-fma4 \
# libm-sysdep_routines
endif
libm-sysdep_routines += \
  e_asin-fma \
  e_atan2-avx \
  e_atan2-fma \
  e_exp-avx \
  e_exp-fma \
  e_exp2f-fma \
  e_expf-fma \
  e_log-avx \
  e_log-fma \
  e_log2-fma \
  e_log2f-fma \
  e_logf-fma \
  e_pow-fma \
  e_powf-fma \
  s_atan-avx \
  s_atan-fma \
  s_ceil-sse4_1 \
  s_ceilf-sse4_1 \
  s_cosf-fma \
  s_cosf-sse2 \
  s_expm1-fma \
  s_floor-sse4_1 \
  s_floorf-sse4_1 \
  s_log1p-fma \
  s_nearbyint-sse4_1 \
  s_nearbyintf-sse4_1 \
  s_rint-sse4_1 \
  s_rintf-sse4_1 \
  s_roundeven-sse4_1 \
  s_roundevenf-sse4_1 \
  s_sin-avx \
  s_sin-fma \
  s_sincos-avx \
  s_sincos-fma \
  s_sincosf-fma \
  s_sincosf-sse2 \
  s_sinf-fma \
  s_sinf-sse2 \
  s_tan-avx \
  s_tan-fma \
  s_trunc-sse4_1 \
  s_truncf-sse4_1 \
# libm-sysdep_routines
ifeq ($(have-x86-isa-level),baseline)
libm-sysdep_routines += \
  s_ceil-c \
  s_ceilf-c \
  s_floor-c \
  s_floorf-c \
  s_nearbyint-c \
  s_nearbyintf-c \
  s_rint-c \
  s_rintf-c \
  s_roundeven-c \
  s_roundevenf-c \
  s_trunc-c \
  s_truncf-c \
# libm-sysdep_routines
endif
endif

CFLAGS-e_asin-fma4.c = -mfma4
CFLAGS-e_atan2-fma4.c = -mfma4
CFLAGS-e_exp-fma4.c = -mfma4
CFLAGS-e_log-fma4.c = -mfma4
CFLAGS-e_pow-fma4.c = -mfma4
CFLAGS-s_atan-fma4.c = -mfma4
CFLAGS-s_sin-fma4.c = -mfma4
CFLAGS-s_tan-fma4.c = -mfma4
CFLAGS-s_sincos-fma4.c = -mfma4

CFLAGS-e_atan2-avx.c = -msse2avx -DSSE2AVX
CFLAGS-e_exp-avx.c = -msse2avx -DSSE2AVX
CFLAGS-e_log-avx.c = -msse2avx -DSSE2AVX
CFLAGS-s_atan-avx.c = -msse2avx -DSSE2AVX
CFLAGS-s_sin-avx.c = -msse2avx -DSSE2AVX
CFLAGS-s_tan-avx.c = -msse2avx -DSSE2AVX
CFLAGS-s_sincos-avx.c = -msse2avx -DSSE2AVX
endif

ifeq ($(subdir),mathvec)
libmvec-multiarch-double-func-list = \
  2_core-sse2 \
  2_core_sse4 \
  4_core-sse \
  4_core_avx2 \
  8_core-avx2 \
  8_core_avx512
libmvec-multiarch-float-func-list = \
  f4_core-sse2 \
  f4_core_sse4 \
  f8_core-sse \
  f8_core_avx2 \
  f16_core-avx2 \
  f16_core_avx512

libmvec-sysdep_routines += \
  $(foreach l,$(libmvec-multiarch-double-func-list), \
    $(addprefix svml_d_,$(addsuffix $(l),$(libmvec-funcs)))) \
  $(foreach l,$(libmvec-multiarch-float-func-list), \
    $(addprefix svml_s_,$(addsuffix $(l),$(libmvec-funcs))))
endif
