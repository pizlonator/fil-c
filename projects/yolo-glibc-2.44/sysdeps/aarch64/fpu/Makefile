libmvec-supported-funcs = acos \
                          acosh \
                          acospi \
                          asin \
                          asinh \
                          asinpi \
                          atan \
                          atanh \
                          atanpi \
                          atan2 \
                          atan2pi \
                          cbrt \
                          cos \
                          cosh \
                          cospi \
                          erf \
                          erfc \
                          exp \
                          exp10 \
                          exp2 \
                          expm1 \
                          exp2m1 \
                          exp10m1 \
                          hypot \
                          log \
                          log10 \
                          log10p1 \
                          log1p \
                          log2 \
                          log2p1 \
                          pow \
                          powr \
                          rsqrt \
                          sin \
                          sinh \
                          sinpi \
                          tan \
                          tanh \
                          tanpi

float-advsimd-funcs = $(libmvec-supported-funcs)
double-advsimd-funcs = $(libmvec-supported-funcs)
float-sve-funcs = $(libmvec-supported-funcs)
double-sve-funcs = $(libmvec-supported-funcs)
mathvec-types = float double
mathvec-subdirs = advsimd sve

ifeq ($(subdir),mathvec)
generated-dirs += $(mathvec-subdirs)
generated += $(addsuffix /stamp,$(mathvec-subdirs))
before-compile += $(addprefix $(objpfx),$(addsuffix /stamp,$(mathvec-subdirs)))

$(addprefix $(objpfx),$(addsuffix /stamp,$(mathvec-subdirs))):
	$(make-target-directory)
	touch $@

sysd-rules-patterns += $(foreach dir,$(mathvec-subdirs),$(dir)/%:$(dir)/%)

define libmvec-routines-for
$(addprefix $(1)/,$(addsuffix f,$(float-$(1)-funcs))) \
$(addprefix $(1)/,$(double-$(1)-funcs))
endef

libmvec-vector-routines = $(foreach dir,$(mathvec-subdirs),\
                           $(call libmvec-routines-for,$(dir)))
libmvec-data-routines = v_log_data \
                        v_exp_data \
                        v_log2_data \
                        v_log10_data \
                        erf_data \
                        erff_data \
                        v_exp_tail_data \
                        erfc_data \
                        erfcf_data \
                        v_pow_exp_data \
                        v_pow_log_data \
                        v_powf_data
libmvec-support = $(libmvec-vector-routines) $(libmvec-data-routines)
endif

# Enable SVE for building libmvec.  Since CFLAGS may contain a -mcpu or -march,
# add a generic -mcpu and -march with SVE enabled.  Also use a tune for a modern
# SVE core.

sve-cflags = -mcpu=generic+sve -march=armv8-a+sve -mtune=neoverse-v1

bench-script-advsimd = bench_libmvec_advsimd.py
bench-script-sve = bench_libmvec_sve.py

define bench-libmvec-for
$(addprefix $(1)-$(2)-,$($(1)-$(2)-funcs))
endef

ifeq ($(build-mathvec),yes)
bench-libmvec = $(foreach type,$(mathvec-types),\
                 $(foreach dir,$(mathvec-subdirs),\
                 $(call bench-libmvec-for,$(type),$(dir))))
endif

define bench-libmvec-rule
$(objpfx)bench-$(1)-$(2)-%.c:
	$$(PYTHON) $$(..)sysdeps/aarch64/fpu/scripts/$$(bench-script-$(2)) $$(basename $$(@F)) > $$@
endef

$(foreach type,$(mathvec-types),\
  $(foreach dir,$(mathvec-subdirs),\
    $(eval $(call bench-libmvec-rule,$(type),$(dir)))))

ifeq (${STATIC-BENCHTESTS},yes)
libmvec-benchtests = $(common-objpfx)mathvec/libmvec.a $(common-objpfx)math/libm.a
else
libmvec-benchtests = $(libmvec) $(libm)
endif

$(addprefix $(objpfx)bench-,$(bench-libmvec)): $(libmvec-benchtests)

ifeq ($(build-mathvec),yes)
libmvec-tests += $(foreach type,$(mathvec-types),\
                  $(foreach dir,$(mathvec-subdirs),$(type)-$(dir)))
endif

define sve-bench-cflags-template
CFLAGS-bench-$(1)-sve-$(2).c += $(sve-cflags)
endef

$(foreach type,$(mathvec-types),\
  $(foreach f,$($(type)-sve-funcs),\
    $(eval $(call sve-bench-cflags-template,$(type),$(f)))))

# Specialize CFLAGS used for building SVE sources.
define sve-object-cflags-template
$(objpfx)sve/%$(1): CFLAGS += $(sve-cflags)
endef

$(foreach o,$(all-object-suffixes),$(eval $(call sve-object-cflags-template,$(o))))

CFLAGS-test-float-sve-wrappers.c = $(sve-cflags)
CFLAGS-test-double-sve-wrappers.c = $(sve-cflags)
