libmvec-supported-funcs = acos \
                          acosh \
                          asin \
                          asinh \
                          atan \
                          atanh \
                          atan2 \
                          cbrt \
                          cos \
                          cosh \
                          erf \
                          erfc \
                          exp \
                          exp10 \
                          exp2 \
                          expm1 \
                          hypot \
                          log \
                          log10 \
                          log1p \
                          log2 \
                          pow \
                          sin \
                          sinh \
                          tan \
                          tanh

float-advsimd-funcs = $(libmvec-supported-funcs)
double-advsimd-funcs = $(libmvec-supported-funcs)
float-sve-funcs = $(libmvec-supported-funcs)
double-sve-funcs = $(libmvec-supported-funcs)

ifeq ($(subdir),mathvec)
libmvec-support = $(addsuffix f_advsimd,$(float-advsimd-funcs)) \
                  $(addsuffix _advsimd,$(double-advsimd-funcs)) \
                  $(addsuffix f_sve,$(float-sve-funcs)) \
                  $(addsuffix _sve,$(double-sve-funcs)) \
                  v_log_data \
                  v_exp_data \
                  v_log2_data \
                  v_log10_data \
                  erf_data \
                  erff_data \
                  sv_erf_data \
                  sv_erff_data \
                  v_exp_tail_data \
                  erfc_data \
                  erfcf_data \
                  v_pow_exp_data \
                  v_pow_log_data \
                  v_powf_data
endif

sve-cflags = -march=armv8-a+sve


ifeq ($(build-mathvec),yes)
bench-libmvec = $(addprefix float-advsimd-,$(float-advsimd-funcs)) \
                $(addprefix double-advsimd-,$(double-advsimd-funcs)) \
                $(addprefix float-sve-,$(float-sve-funcs)) \
                $(addprefix double-sve-,$(double-sve-funcs))
endif

$(objpfx)bench-float-advsimd-%.c:
	$(PYTHON) $(..)sysdeps/aarch64/fpu/scripts/bench_libmvec_advsimd.py $(basename $(@F)) > $@
$(objpfx)bench-double-advsimd-%.c:
	$(PYTHON) $(..)sysdeps/aarch64/fpu/scripts/bench_libmvec_advsimd.py $(basename $(@F)) > $@
$(objpfx)bench-float-sve-%.c:
	$(PYTHON) $(..)sysdeps/aarch64/fpu/scripts/bench_libmvec_sve.py $(basename $(@F)) > $@
$(objpfx)bench-double-sve-%.c:
	$(PYTHON) $(..)sysdeps/aarch64/fpu/scripts/bench_libmvec_sve.py $(basename $(@F)) > $@

ifeq (${STATIC-BENCHTESTS},yes)
libmvec-benchtests = $(common-objpfx)mathvec/libmvec.a $(common-objpfx)math/libm.a
else
libmvec-benchtests = $(libmvec) $(libm)
endif

$(addprefix $(objpfx)bench-,$(bench-libmvec)): $(libmvec-benchtests)

ifeq ($(build-mathvec),yes)
libmvec-tests += float-advsimd double-advsimd float-sve double-sve
endif

define sve-float-cflags-template
CFLAGS-$(1)f_sve.c += $(sve-cflags)
CFLAGS-bench-float-sve-$(1).c += $(sve-cflags)
endef

define sve-double-cflags-template
CFLAGS-$(1)_sve.c += $(sve-cflags)
CFLAGS-bench-double-sve-$(1).c += $(sve-cflags)
endef

$(foreach f,$(float-sve-funcs), $(eval $(call sve-float-cflags-template,$(f))))
$(foreach f,$(double-sve-funcs), $(eval $(call sve-double-cflags-template,$(f))))

CFLAGS-test-float-sve-wrappers.c = $(sve-cflags)
CFLAGS-test-double-sve-wrappers.c = $(sve-cflags)
