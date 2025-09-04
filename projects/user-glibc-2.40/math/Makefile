# Copyright (C) 1996-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

# Makefile for the math library.

subdir		:= math

include ../Makeconfig

# Installed header files.
headers	:= \
  math.h \
  bits/mathcalls.h \
  fpu_control.h \
  complex.h \
  bits/cmathcalls.h \
  fenv.h \
  bits/fenv.h \
  bits/mathdef.h \
  tgmath.h \
  bits/math-vector.h \
  finclude/math-vector-fortran.h \
  bits/libm-simd-decl-stubs.h \
  bits/iscanonical.h \
  bits/flt-eval-method.h \
  bits/fp-fast.h \
  bits/fp-logb.h \
  bits/long-double.h \
  bits/mathcalls-helper-functions.h \
  bits/floatn.h \
  bits/floatn-common.h \
  bits/mathcalls-narrow.h
  # headers

# FPU support code.
aux := \
  fpu_control \
  setfpucw \
  # aux

# Build the -lm library.

extra-libs	:= libm
extra-libs-others = $(extra-libs)

libm-support = \
  fclrexcpt \
  fedisblxcpt \
  feenablxcpt \
  fegetenv \
  fegetexcept \
  fegetmode \
  fegetround \
  feholdexcpt \
  fesetenv \
  fesetexcept \
  fesetmode\
  fesetround \
  fetestexceptflag \
  feupdateenv \
  fgetexcptflg \
  fraiseexcpt \
  fsetexcptflg \
  ftestexcept \
  s_lib_version \
  s_matherr \
  s_signgam \
  # libm-support

# Wrappers for these functions generated per type using a file named
# <func>_template.c and the appropriate math-type-macros-<TYPE>.h.
gen-libm-calls = \
  cabsF \
  cargF \
  cimagF \
  conjF \
  crealF \
  e_exp2F \
  e_scalbF \
  k_casinhF \
  k_casinhF \
  s_cacosF  \
  s_cacoshF \
  s_canonicalizeF \
  s_casinF \
  s_casinhF \
  s_catanF \
  s_catanhF \
  s_ccosF \
  s_ccoshF \
  s_cexpF \
  s_clog10F \
  s_clogF \
  s_cpowF \
  s_cprojF \
  s_csinF \
  s_csinhF \
  s_csinhF \
  s_csqrtF \
  s_ctanF \
  s_ctanhF \
  s_exp10m1F \
  s_exp2m1F \
  s_fdimF \
  s_fmaxF \
  s_fmaximumF \
  s_fmaximum_magF \
  s_fmaximum_mag_numF \
  s_fmaximum_numF \
  s_fmaxmagF \
  s_fminF \
  s_fminimumF \
  s_fminimum_magF \
  s_fminimum_mag_numF \
  s_fminimum_numF \
  s_fminmagF \
  s_iseqsigF \
  s_log10p1F \
  s_log2p1F \
  s_nanF \
  s_nextdownF \
  s_significandF \
  w_acosF \
  w_acoshF \
  w_asinF \
  w_atan2F \
  w_atanhF \
  w_coshF \
  w_exp10F \
  w_exp2F \
  w_expF \
  w_fmodF \
  w_hypotF \
  w_ilogbF \
  w_j0F \
  w_j1F \
  w_jnF \
  w_lgammaF \
  w_lgammaF_r \
  w_llogbF \
  w_log10F \
  w_log1pF \
  w_log2F \
  w_logF  \
  w_powF \
  w_remainderF \
  w_scalbF \
  w_scalblnF \
  w_sinhF \
  w_sqrtF \
  w_tgammaF \
  # gen-libm-calls

libm-calls = \
  $(calls:s_%=m_%) \
  $(gen-libm-calls) \
  e_acosF \
  e_acoshF \
  e_asinF \
  e_atan2F \
  e_atanhF \
  e_coshF \
  e_exp10F \
  e_expF \
  e_fmodF \
  e_gammaF_r \
  e_hypotF \
  e_ilogbF \
  e_j0F \
  e_j1F \
  e_jnF \
  e_lgammaF_r \
  e_log10F \
  e_log2F \
  e_logF \
  e_powF \
  e_remainderF \
  e_sinhF \
  e_sqrtF \
  gamma_productF \
  k_tanF \
  lgamma_negF \
  lgamma_productF \
  s_asinhF \
  s_atanF \
  s_cbrtF \
  s_ceilF \
  s_cosF \
  s_erfF \
  s_expm1F \
  s_fabsF \
  s_floorF \
  s_fmaF \
  s_fpclassifyF \
  s_fromfpF \
  s_fromfpxF \
  s_getpayloadF \
  s_issignalingF \
  s_llrintF \
  s_llroundF \
  s_log1pF \
  s_logbF \
  s_lrintF \
  s_lroundF \
  s_nearbyintF \
  s_nextafterF \
  s_nexttowardF \
  s_nextupF \
  s_remquoF \
  s_rintF \
  s_roundF \
  s_roundevenF \
  s_scalblnF \
  s_setpayloadF \
  s_setpayloadsigF \
  s_sinF \
  s_sincosF \
  s_tanF \
  s_tanhF \
  s_totalorderF \
  s_totalordermagF \
  s_truncF \
  s_ufromfpF \
  s_ufromfpxF \
  x2y2m1F \
  # libm-calls

libm-compat-calls = \
  k_standardF \
  w_acosF_compat \
  w_acoshF_compat \
  w_asinF_compat \
  w_atan2F_compat \
  w_atanhF_compat \
  w_coshF_compat \
  w_exp10F_compat \
  w_exp2F_compat \
  w_expF_compat	\
  w_fmodF_compat \
  w_hypotF_compat \
  w_j0F_compat \
  w_j1F_compat\
  w_jnF_compat \
  w_lgammaF_compat2 \
  w_lgammaF_r_compat \
  w_lgamma_compatF \
  w_log10F_compat \
  w_log2F_compat \
  w_logF_compat	\
  w_powF_compat \
  w_remainderF_compat \
  w_scalbF_compat \
  w_sinhF_compat \
  w_sqrtF_compat \
  w_tgammaF_compat \
  # libm-compat-calls

libm-narrow-fns = \
  add \
  div \
  fma \
  mul \
  sqrt \
  sub \
  # libm-narrow-fns
libm-narrow-types-basic = \
  s_f32xFf64 \
  s_fF \
  # libm-narrow-types-basic
libm-narrow-types-ldouble-yes = \
  s_dFl \
  s_fFl \
  # libm-narrow-types-ldouble-yes
libm-narrow-types-float128-yes = \
  s_f32Ff128 \
  s_f64Ff128 \
  s_f64xFf128 \
  # libm-narrow-types-float128-yes
libm-narrow-types-float128-alias-yes = \
  s_f64xFf128 \
  # libm-narrow-types-float128-alias-yes
libm-narrow-types = \
  $(libm-narrow-types-basic) \
  $(libm-narrow-types-float128-$(float128-fcts)) \
  $(libm-narrow-types-float128-alias-$(float128-alias-fcts)) \
  $(libm-narrow-types-ldouble-$(long-double-fcts)) \
  # libm-narrow-types

# Type specific routine support.
#
# The following three variables control what is included for each type:
#
# type-floatN-suffix = The suffix of the type
# type-floatN-routines = Type specific support objects
# type-floatN-yes = If the type is supported, evaluates to floatN
#
# Finally, note that types is an intentionally recursive variable.
# We only know the full set of supported types for the target machine
# after the Rules makefile has been parsed.
types-basic = \
  $(type-ldouble-$(long-double-fcts)) \
  double \
  float \
  # types-basic

# Like types, but includes types whose functions alias those for
# another type.
test-types-basic = \
  double \
  float \
  ldouble \
  # test-types-basic

# long double support
type-ldouble-suffix := l
type-ldouble-routines := \
  e_rem_pio2l \
  k_cosl \
  k_sincosl \
  k_sinl \
  s_iscanonicall \
  t_sincosl \
  # type-ldouble-routines
type-ldouble-yes := ldouble

# double support
type-double-suffix :=
type-double-routines := \
  branred \
  e_exp_data \
  e_log2_data \
  e_log_data \
  e_pow_log_data \
  k_rem_pio2 \
  math_err \
  sincostab \
  # type-double-routines

# float support
type-float-suffix := f
type-float-routines := \
  e_exp2f_data \
  e_log2f_data \
  e_logf_data \
  e_powf_log2_data \
  math_errf \
  s_sincosf_data \
  # type-float-routines

# _Float128 support
type-float128-suffix := f128
type-float128-routines := \
  e_rem_pio2f128 \
  k_cosf128 \
  k_sincosf128 \
  k_sinf128 \
  t_sincosf128 \
  # type-float128-routines
type-float128-yes := float128

# _Float64x may be supported, only as an alias type.
type-float64x-yes := float64x

# IBM long double support in additional to IEEE 128 long double support
type-ibm128-suffix := l
type-ibm128-yes := ibm128

types = \
  $(type-float128-$(float128-fcts)) \
  $(types-basic) \
  # types
test-types = \
  $(test-types-basic) \
  $(type-float128-$(float128-alias-fcts)) \
  $(type-float128-$(float128-fcts)) \
  $(type-float64x-$(float64x-alias-fcts)) \
  $(type-ibm128-$(ibm128-fcts)) \
  float32 \
  float32x \
  float64 \
  # test-types

# Pairs of types for which narrowing functions should be tested (this
# variable has more entries than libm-narrow-types because it includes
# pairs for which the functions sometimes or always alias functions
# for other types). This definition embeds the assumption that if
# _Float64x is supported, so is _Float128, and vice versa (they may or
# may not have the same format).
test-type-pairs = \
  $(test-type-pairs-f64xf128-$(float128-alias-fcts)) \
  $(test-type-pairs-f64xf128-$(float128-fcts)) \
  double-ldouble \
  float-double \
  float-ldouble \
  float32-float32x \
  float32-float64 \
  float32x-float64 \
  # test-type-pairs
test-type-pairs-f64xf128-yes = \
  float32-float128 \
  float32-float64x \
  float32x-float128 \
  float32x-float64x \
  float64-float128 \
  float64-float64x \
  float64x-float128 \
  # test-type-pairs-f64xf128-yes

# For each of the basic types (float, double, long double), replace the
# occurrences of 'F' in arg 1 with the appropriate suffix for the type.
type-basic-foreach = $(foreach t, $(types-basic), \
		       $(subst F,$(type-$(t)-suffix),$(1)))

# Apply suffix to each type in arg 1
type-foreach = $(foreach t,$(types),$(subst F,$(type-$(t)-suffix),$(1)))

libm-routines = $(strip $(libm-support)					\
			$(call type-basic-foreach,			\
			       $(libm-compat-calls))			\
			$(call type-foreach, $(libm-calls))		\
			$(foreach t, $(types), $(type-$(t)-routines))) 	\
			$(foreach f,$(libm-narrow-fns),			\
				    $(subst F,$(f),$(libm-narrow-types)))

# These functions are in libc instead of libm because __printf_fp
# calls them, so any program using printf will need them linked in,
# and we don't want to have to link every program with -lm.
# In libm-calls (above), list m_foo in place of s_foo for any
# routine that should be compiled separately for its libc and libm versions.
calls = \
  $(gen-calls) \
  s_copysignF \
  s_finiteF \
  s_frexpF \
  s_isinfF \
  s_isnanF \
  s_modfF \
  s_scalbnF \
  s_signbitF \
  # calls
gen-calls = s_ldexpF
generated += $(foreach s,.c .S,$(call type-foreach, $(calls:s_%=m_%$(s))))
routines = $(call type-foreach, $(calls))
# The $(calls) that are shared between libm and libc are not included in static
# libm so the symbols end up in exactly one place.
libm-shared-only-routines = $(call type-foreach, $(calls:s_%=m_%))

ifeq ($(build-mathvec),yes)
# We need to install libm.so and libm.a as linker scripts
# for transparent use of vector math library.
install-lib-ldscripts := \
  libm.a \
  libm.so \
  # install-lib-ldscripts
install-others = \
  $(inst_libdir)/libm.a \
  $(inst_libdir)/libm.so \
  # install-others
$(inst_libdir)/libm.so: $(common-objpfx)format.lds \
			$(libm) \
			$(libmvec) \
			$(+force)
	(echo '/* GNU ld script'; echo '*/';\
	 cat $<; \
	 echo 'GROUP ( $(slibdir)/libm.so$(libm.so-version) ' \
	      'AS_NEEDED ( $(slibdir)/libmvec.so$(libmvec.so-version) ) )' \
	) > $@.new
	mv -f $@.new $@

$(inst_libdir)/libm-$(version).a: $(objpfx)libm.a \
				  $(+force)
	$(do-install)

$(inst_libdir)/libm.a: $(common-objpfx)format.lds \
		       $(inst_libdir)/libm-$(version).a \
		       $(objpfx)../mathvec/libmvec.a \
		       $(+force)
	(echo '/* GNU ld script'; echo '*/';\
	 cat $<; \
	 echo 'GROUP ( $(libdir)/libm-$(version).a $(libdir)/libmvec.a )' \
	) > $@.new
	mv -f $@.new $@
endif

# Rules for the test suite.
tests = \
  $(tests-static) \
  bug-nextafter \
  bug-nexttoward \
  bug-tgmath1 \
  test-ceil-except-2 \
  test-femode \
  test-femode-traps \
  test-fenv basic-test \
  test-fenv-clear \
  test-fenv-preserve \
  test-fenv-return \
  test-fenv-tls \
  test-fesetexcept \
  test-fesetexcept-traps \
  test-fetestexceptflag \
  test-fexcept \
  test-fexcept-traps \
  test-floor-except-2 \
  test-flt-eval-method \
  test-fp-ilogb-constants \
  test-fp-llogb-constants \
  test-fpucw \
  test-fpucw-ieee \
  test-iseqsig-excess-precision \
  test-iszero-excess-precision \
  test-matherr-3 \
  test-misc \
  test-nan-const \
  test-nan-overflow \
  test-nan-payload \
  test-narrow-macros \
  test-nearbyint-except \
  test-nearbyint-except-2 \
  test-powl \
  test-signgam-uchar \
  test-signgam-uchar-init \
  test-signgam-uint \
  test-signgam-uint-init \
  test-signgam-ullong \
  test-signgam-ullong-init \
  test-snan \
  test-tgmath \
  test-tgmath-int \
  test-tgmath-ret \
  test-tgmath2 \
  test-trunc-except-2 \
  tst-CMPLX \
  tst-CMPLX2 \
  tst-definitions \
  # tests
ifneq ($(config-cflags-signaling-nans),)
tests += \
  test-fe-snans-always-signal \
  # tests
endif
tests-static = \
  test-fpucw-ieee-static \
  test-fpucw-static \
  test-signgam-uchar-init-static \
  test-signgam-uchar-static \
  test-signgam-uint-init-static \
  test-signgam-uint-static \
  test-signgam-ullong-init-static \
  test-signgam-ullong-static \
  # tests-static

# The tested symbols matherr, _LIB_VERSION have been removed in glibc 2.27.
ifeq ($(have-GLIBC_2.26)$(build-shared),yesyes)
tests += \
  test-matherr \
  test-matherr-2 \
  # tests
tests-2.0 += \
  test-matherr-2 \
  # tests-2.0
endif

# These tests use internal (unexported) GMP functions and are linked
# statically to obtain access to these functions.
tests-static += \
  atest-exp \
  atest-exp2 \
  atest-sincos \
  # tests-static

ifneq (,$(CXX))
tests += \
  test-math-cxx11 \
  test-math-iscanonical \
  test-math-iseqsig \
  test-math-isinff \
  test-math-issignaling \
  test-math-iszero \
  # tests
endif

libm-vec-tests = $(addprefix test-,$(libmvec-tests))
libm-test-support = $(foreach t,$(test-types),libm-test-support-$(t))
libm-test-support-static = $(foreach t,$(test-types),libm-test-support-$(t)-static)
test-extras += $(libm-test-support) $(libm-test-support-static)
extra-test-objs += $(addsuffix .o, $(libm-test-support)) \
		   $(addsuffix .o, $(libm-test-support-static))
libm-vec-test-wrappers = $(addsuffix -wrappers, $(libm-vec-tests))
test-extras += $(libm-vec-test-wrappers)
extra-test-objs += $(addsuffix .o, $(libm-vec-test-wrappers))

ulps-file = $(firstword $(wildcard $(sysdirs:%=%/libm-test-ulps)))

$(objpfx)libm-test-ulps.h: $(ulps-file) gen-libm-test.py
	$(make-target-directory)
	$(PYTHON) gen-libm-test.py -u $< -H $@

libm-test-funcs-auto = \
  acos \
  acosh \
  asin \
  asinh \
  atan \
  atan2 \
  atanh \
  cabs \
  cacos \
  cacosh \
  carg \
  casin \
  casinh \
  catan \
  catanh \
  cbrt \
  ccos \
  ccosh \
  cexp \
  clog \
  clog10 \
  cos \
  cosh \
  cpow \
  csin \
  csinh \
  csqrt \
  ctan \
  ctanh \
  erf \
  erfc \
  exp \
  exp2 \
  exp10 \
  exp10m1 \
  exp2m1 \
  expm1 \
  fma \
  hypot \
  j0 \
  j1 \
  jn \
  lgamma \
  log \
  log2 \
  log10 \
  log10p1 \
  log1p \
  log2p1 \
  pow \
  sin \
  sincos \
  sinh \
  sqrt \
  tan \
  tanh \
  tgamma \
  y0 \
  y1 \
  yn \
  # libm-test-funcs-auto
libm-test-funcs-noauto-base = \
  canonicalize \
  ceil \
  cimag \
  conj \
  copysign \
  cproj \
  creal \
  fabs \
  fdim \
  floor \
  fmax \
  fmaximum \
  fmaximum_mag \
  fmaximum_mag_num \
  fmaximum_num \
  fmaxmag \
  fmin \
  fminimum \
  fminimum_mag \
  fminimum_mag_num \
  fminimum_num \
  fminmag \
  fmod \
  fpclassify \
  frexp \
  fromfp \
  fromfpx \
  getpayload \
  ilogb \
  iscanonical \
  iseqsig \
  isfinite \
  isgreater \
  isgreaterequal \
  isinf \
  isless \
  islessequal \
  islessgreater \
  isnan \
  isnormal \
  issignaling \
  issubnormal \
  isunordered \
  iszero \
  llogb \
  llrint \
  llround \
  logb \
  lrint \
  lround \
  modf \
  nearbyint \
  nextafter \
  nextdown \
  nexttoward \
  nextup \
  remainder \
  remquo \
  rint \
  round \
  roundeven \
  scalb \
  scalbln \
  scalbn \
  setpayload \
  setpayloadsig \
  signbit \
  significand \
  totalorder \
  totalordermag \
  trunc \
  ufromfp \
  ufromfpx \
  # libm-test-funcs-noauto-base
libm-test-funcs-noauto = \
  $(libm-test-funcs-noauto-base) \
  compat_totalorder \
  compat_totalordermag \
  # libm-test-funcs-noauto
libm-test-funcs-compat = \
  compat_totalorder \
  compat_totalordermag \
  # libm-test-funcs-compat
libm-test-funcs-narrow = \
  add \
  div \
  fma \
  mul \
  sqrt \
  sub \
  # libm-test-funcs-narrow
libm-test-funcs-all = \
  $(libm-test-funcs-auto) \
  $(libm-test-funcs-noauto) \
  # libm-test-funcs-all
libm-test-c-auto = $(foreach f,$(libm-test-funcs-auto),libm-test-$(f).c)
libm-test-c-noauto = $(foreach f,$(libm-test-funcs-noauto),libm-test-$(f).c)
libm-test-c-narrow = $(foreach f,$(libm-test-funcs-narrow),\
				 libm-test-narrow-$(f).c)
generated += \
  $(libm-test-c-auto) \
  $(libm-test-c-narrow) \
  $(libm-test-c-noauto) \
  libm-test-ulps.h \
  # generated

libm-tests-base-normal = $(foreach t,$(test-types),test-$(t))
libm-tests-base-narrow = $(foreach t,$(test-type-pairs),test-$(t))
libm-tests-base = $(libm-tests-base-normal) $(libm-vec-tests)
libm-tests-normal = $(foreach t,$(libm-tests-base-normal),\
				$(foreach f,$(libm-test-funcs-all),\
					    $(t)-$(f)))
libm-tests-narrow = $(foreach t,$(libm-tests-base-narrow),\
				$(foreach f,$(libm-test-funcs-narrow),\
					    $(t)-$(f)))
libm-tests-vector = $(foreach t,$(libmvec-tests),\
				$(foreach f,$($(t)-funcs),test-$(t)-$(f)))
libm-tests = $(libm-tests-normal) $(libm-tests-narrow) $(libm-tests-vector)
libm-tests-compat = $(foreach t,$(libm-tests-base-normal) \
				$(libm-tests-base-finite),\
				$(foreach f,$(libm-test-funcs-compat),\
					    $(t)-$(f)))
libm-tests-for-type = $(foreach f,$(libm-test-funcs-all),\
				  test-$(1)-$(f) test-i$(1)-$(f)) \
		      $(filter test-$(1)-%,$(libm-tests-vector) \
					   $(libm-tests-narrow))

libm-tests.o = $(addsuffix .o,$(libm-tests))

tests += $(libm-tests)
generated += $(addsuffix .c,$(libm-tests)) \
	     $(foreach t,$(test-types),libm-test-support-$(t).c)

libm-test-c-auto-obj = $(addprefix $(objpfx),$(libm-test-c-auto))
libm-test-c-noauto-obj = $(addprefix $(objpfx),$(libm-test-c-noauto))
libm-test-c-narrow-obj = $(addprefix $(objpfx),$(libm-test-c-narrow))

$(libm-test-c-noauto-obj): $(objpfx)libm-test%.c: libm-test%.inc \
						  gen-libm-test.py
	$(make-target-directory)
	$(PYTHON) gen-libm-test.py -c $< -a /dev/null -C $@

$(libm-test-c-auto-obj): $(objpfx)libm-test%.c: libm-test%.inc \
						gen-libm-test.py \
						auto-libm-test-out%
	$(make-target-directory)
	$(PYTHON) gen-libm-test.py -c $< -a auto-libm-test-out$* -C $@

$(libm-test-c-narrow-obj): $(objpfx)libm-test%.c: libm-test%.inc \
						  gen-libm-test.py \
						  auto-libm-test-out%
	$(make-target-directory)
	$(PYTHON) gen-libm-test.py -c $< -a auto-libm-test-out$* -C $@


libm-test-funcs-auto-static = \
  $(libm-test-funcs-auto) \
  # libm-test-funcs-auto-static
libm-test-funcs-noauto-static = \
  $(libm-test-funcs-noauto-base) \
  # libm-test-funcs-noauto-static
libm-test-funcs-narrow-static = \
  $(libm-test-funcs-narrow) \
  # libm-test-funcs-narrow-static
libm-test-funcs-all-static = $(libm-test-funcs-auto-static) $(libm-test-funcs-noauto-static)

libm-test-c-auto-static = $(foreach f,$(libm-test-funcs-auto-static),libm-test-$(f)-static.c)
libm-test-c-noauto-static = $(foreach f,$(libm-test-funcs-noauto-static),libm-test-$(f)-static.c)
libm-test-c-narrow-static = $(foreach f,$(libm-test-funcs-narrow-static),\
				 libm-test-narrow-$(f)-static.c)
generated += $(libm-test-c-auto-static) $(libm-test-c-noauto-static) $(libm-test-c-narrow-static)

libm-tests-normal-static = $(foreach t,$(libm-tests-base-normal),\
				$(foreach f,$(libm-test-funcs-all-static),\
					    $(t)-$(f)-static))
libm-tests-narrow-static = $(foreach t,$(libm-tests-base-narrow-static),\
				$(foreach f,$(libm-test-funcs-narrow-static),\
					    $(t)-$(f)-static))
libm-tests-vector-static = $(foreach t,$(libmvec-tests-static),\
				$(foreach f,$($(t)-funcs),test-$(t)-$(f)-static))
libm-tests-static = $(libm-tests-normal-static) $(libm-tests-narrow-static) $(libm-tests-vector-static)
libm-tests-for-type-static = $(foreach f,$(libm-test-funcs-all-static),\
					 test-$(1)-$(f)-static test-i$(1)-$(f)-static) \
			     $(filter test-$(1)-%,$(libm-tests-vector-static) \
						  $(libm-tests-narrow-static))

libm-tests.o += $(addsuffix .o,$(libm-tests-static))

ifeq ($(build-math-static-tests),yes)
tests-static += $(libm-tests-static)
generated += $(addsuffix .c,$(libm-tests)) \
	     $(foreach t,$(test-types),libm-test-support-$(t)-static.c)
endif

libm-test-c-auto-obj-static = $(addprefix $(objpfx),$(libm-test-c-auto-static))
libm-test-c-noauto-obj-static = $(addprefix $(objpfx),$(libm-test-c-noauto-static))
libm-test-c-narrow-obj-static = $(addprefix $(objpfx),$(libm-test-c-narrow-static))

# Use the same input test definitions for both dynamic and static tests.
.SECONDEXPANSION:
$(libm-test-c-noauto-obj-static): $(objpfx)libm-test%.c: libm-test$$(subst -static,,%).inc \
							 gen-libm-test.py
	$(make-target-directory)
	$(PYTHON) gen-libm-test.py -c $< -a /dev/null -C $@

.SECONDEXPANSION:
$(libm-test-c-auto-obj-static): $(objpfx)libm-test%.c: libm-test$$(subst -static,,%).inc \
						       gen-libm-test.py \
						       auto-libm-test-out$$(subst -static,,%)
	$(make-target-directory)
	$(PYTHON) gen-libm-test.py -c $< -a auto-libm-test-out`echo $* | sed 's/-static//'` -C $@

.SECONDEXPANSION:
$(libm-test-c-narrow-obj-static): $(objpfx)libm-test%.c: libm-test$$(subst -static,,%).inc \
							 gen-libm-test.py \
							 auto-libm-test-out$$(subst -static,,%)
	$(make-target-directory)
	$(PYTHON) gen-libm-test.py -c $< -a auto-libm-test-out`echo $* | sed 's/-static//'` -C $@

# Tests for totalorder compat symbols reuse the table of tests as
# processed by gen-libm-test.py, so add dependencies on the generated
# .c files.
$(foreach t,$(libm-tests-base),\
	    $(objpfx)$(t)-compat_totalorder.o): $(objpfx)libm-test-totalorder.c

$(foreach t,$(libm-tests-base),\
	    $(objpfx)$(t)-compat_totalordermag.o): $(objpfx)libm-test-totalordermag.c

# _Float128x omitted as not supported by gen-tgmath-tests.py.
tgmath3-narrow-types = \
  d \
  f \
  f16 \
  f32 \
  f128 \
  f32x \
  f64 \
  f64x \
  # tgmath3-narrow-types
tgmath3-narrow-macros = $(foreach t,$(tgmath3-narrow-types), \
				    $(foreach f,$(libm-narrow-fns),$(t)$(f)))
tgmath3-macros = \
  $(tgmath3-narrow-macros) \
  acos \
  acosh \
  asin \
  asinh \
  atan \
  atan2 \
  atanh \
  carg \
  cbrt \
  ceil \
  cimag \
  conj \
  copysign \
  cos \
  cosh \
  cproj \
  creal \
  erf \
  erfc \
  exp \
  exp10m1 \
  exp2 \
  exp10 \
  exp2m1 \
  expm1 \
  fabs \
  fdim \
  floor \
  fma \
  fmax \
  fmaximum \
  fmaximum_mag \
  fmaximum_mag_num \
  fmaximum_num \
  fmaxmag \
  fmin \
  fminimum \
  fminimum_mag \
  fminimum_mag_num \
  fminimum_num \
  fminmag \
  fmod \
  frexp \
  fromfp \
  fromfpx \
  hypot \
  ilogb \
  ldexp \
  lgamma \
  llogb \
  llrint \
  llround \
  log \
  log10 \
  log10p1 \
  log1p \
  log2 \
  log2p1 \
  logb \
  logp1 \
  lrint \
  lround \
  nearbyint \
  nextafter \
  nextdown \
  nexttoward \
  nextup \
  pow \
  remainder \
  remquo \
  rint \
  round \
  roundeven \
  scalb \
  scalbln \
  scalbn \
  sin \
  sinh \
  sqrt \
  tan \
  tanh \
  tgamma \
  trunc \
  ufromfp \
  ufromfpx \
  # tgmath3-macros
tgmath3-macro-tests = $(addprefix test-tgmath3-,$(tgmath3-macros))
tests += $(tgmath3-macro-tests)
generated += $(addsuffix .c,$(tgmath3-macro-tests))

$(tgmath3-macro-tests:%=$(objpfx)%.o): CFLAGS += -fno-builtin

$(foreach m,$(tgmath3-macros),\
	    $(objpfx)test-tgmath3-$(m).c): $(objpfx)test-tgmath3-%.c: \
					   gen-tgmath-tests.py
	$(PYTHON) gen-tgmath-tests.py $* > $@

# Verify that the list of supported macros is in sync between the
# Makefile and gen-tgmath-tests.py.
tests-special += $(objpfx)test-tgmath3-macro-list.out
$(objpfx)test-tgmath3-macro-list.out: gen-tgmath-tests.py
	$(PYTHON) $< check-list $(tgmath3-macros) > $@; \
	$(evaluate-test)

libm-test-fast-math-cflags = -fno-builtin -D__FAST_MATH__ -DTEST_FAST_MATH
libm-test-vec-cflags = $(libm-test-fast-math-cflags) -fno-inline \
		       -ffloat-store -D_OPENMP=201307 -Wno-unknown-pragmas

CFLAGS-test-double-vlen4-wrappers.c += $(double-vlen4-arch-ext-cflags)

CFLAGS-test-double-vlen8-wrappers.c += $(double-vlen8-arch-ext-cflags)

CFLAGS-test-float-vlen8-wrappers.c += $(float-vlen8-arch-ext-cflags)

CFLAGS-test-float-vlen16-wrappers.c += $(float-vlen16-arch-ext-cflags)

# The no-inline tests use -fsignaling-nans, and thereby use the
# versions of classification macros that support sNaNs.  The inline
# function tests use the versions of classification macros that may
# raise spurious exceptions for sNaNs, but also do not test for
# exceptions.  Thus both versions of the classification macros are
# validated.
libm-test-no-inline-cflags = -fno-inline -ffloat-store -fno-builtin \
			     $(config-cflags-signaling-nans)
CFLAGS-test-tgmath.c += -fno-builtin
# The following testcase uses very long lines (>3 million), so it may take a
# while to compile it. See: http://llvm.org/bugs/show_bug.cgi?id=14106 and
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54402
CFLAGS-test-tgmath2.c += -fno-builtin
CFLAGS-test-tgmath-ret.c += -fno-builtin
CFLAGS-test-powl.c += -fno-builtin

CFLAGS-test-snan.c += $(config-cflags-signaling-nans)

CFLAGS-test-signgam-uchar.c += -std=c99
CFLAGS-test-signgam-uchar-init.c += -std=c99
CFLAGS-test-signgam-uchar-static.c += -std=c99
CFLAGS-test-signgam-uchar-init-static.c += -std=c99
CFLAGS-test-signgam-uint.c += -std=c99
CFLAGS-test-signgam-uint-init.c += -std=c99
CFLAGS-test-signgam-uint-static.c += -std=c99
CFLAGS-test-signgam-uint-init-static.c += -std=c99
CFLAGS-test-signgam-ullong.c += -std=c99
CFLAGS-test-signgam-ullong-init.c += -std=c99
CFLAGS-test-signgam-ullong-static.c += -std=c99
CFLAGS-test-signgam-ullong-init-static.c += -std=c99

CFLAGS-test-math-cxx11.cc += -std=c++11

CFLAGS-test-math-isinff.cc += -std=gnu++11
CFLAGS-test-math-iszero.cc += -std=gnu++11
CFLAGS-test-math-issignaling.cc += -std=gnu++11
CFLAGS-test-math-iscanonical.cc += -std=gnu++11

CFLAGS-test-iszero-excess-precision.c += -fexcess-precision=standard
CFLAGS-test-iseqsig-excess-precision.c += -fexcess-precision=standard
CFLAGS-test-flt-eval-method.c += -fexcess-precision=standard

CFLAGS-test-fe-snans-always-signal.c += $(config-cflags-signaling-nans)

CFLAGS-test-nan-const.c += -fno-builtin
CFLAGS-test-nan-payload.c += -fno-builtin

CFLAGS-test-ceil-except-2.c += -fno-builtin
CFLAGS-test-floor-except-2.c += -fno-builtin
CFLAGS-test-trunc-except-2.c += -fno-builtin

include ../Rules

gen-all-calls = $(gen-libm-calls) $(gen-calls)

generated += $(addsuffix .c,$(call type-foreach,$(gen-all-calls))) \
	     gen-libm-templates.stmp

# Create wrappers in the math build directory.
$(objpfx)gen-libm-templates.stmp: Makefile
	$(make-target-directory)
	for gcall in $(gen-all-calls); do                                 \
	  func=$${gcall%F*}$${gcall#*F};                                  \
	  for type in $(foreach t,$(types),$(t)__$(type-$(t)-suffix)); do \
	    suff=$${type#*__};                                            \
	    type=$${type%__*};                                            \
	    file=$(objpfx)$${gcall%F*}$${suff}$${gcall#*F}.c;             \
	    (                                                             \
	      echo "#include <math-type-macros-$${type}.h>";              \
	      echo "#include <$${func}_template.c>";                      \
	    ) > $${file};                                                 \
	  done;                                                           \
	done;                                                             \
	echo > $(@)

# Add dependency to ensure the generator runs prior.
$(foreach t, $(call type-foreach, $(gen-all-calls)), \
             $(objpfx)$(t).c): $(objpfx)gen-libm-templates.stmp

# This must come after the inclusion of sysdeps Makefiles via Rules.

$(foreach t,$(libm-tests-normal),$(objpfx)$(t).c): $(objpfx)test-%.c:
	type_func=$*; \
	type=$${type_func%%-*}; \
	func=$${type_func#*-}; \
	( \
	  echo "#include <test-$$type.h>"; \
	  echo "#include <test-math-exceptions.h>"; \
	  echo "#include <test-math-errno.h>"; \
	  echo "#include <test-math-scalar.h>"; \
	  echo "#include <libm-test-$$func.c>"; \
	) > $@

$(foreach t,$(libm-tests-normal-static),$(objpfx)$(t).c): $(objpfx)test-%.c:
	type_func=$*; \
	type=$${type_func%%-*}; \
	func=$${type_func#*-}; \
	( \
	  echo "#include <test-$$type.h>"; \
	  echo "#include <test-math-exceptions.h>"; \
	  echo "#include <test-math-errno.h>"; \
	  echo "#include <test-math-scalar.h>"; \
	  echo "#include <libm-test-$$func.c>"; \
	) > $@

$(foreach t,$(libm-tests-narrow),$(objpfx)$(t).c): $(objpfx)test-%.c:
	type_pair_func=$*; \
	type_pair=$${type_pair_func%-*}; \
	func=$${type_pair_func##*-}; \
	ret_type=$${type_pair%%-*}; \
	arg_type=$${type_pair#*-}; \
	( \
	  echo "#include <test-$$ret_type.h>"; \
	  echo "#include <test-arg-$$arg_type.h>"; \
	  echo "#include <test-math-exceptions.h>"; \
	  echo "#include <test-math-errno.h>"; \
	  echo "#include <test-math-narrow.h>"; \
	  echo "#include <libm-test-narrow-$$func.c>"; \
	) > $@

$(foreach t,$(libm-tests-vector),$(objpfx)$(t).c): $(objpfx)test-%.c:
	type_func=$*; \
	type=$${type_func%-*}; \
	func=$${type_func##*-}; \
	func_args=$$(grep "ALL_RM_TEST ($$func," libm-test-$$func.inc \
		     | sed 's/.*RUN_TEST_LOOP_//;s/_.*//'); \
	( \
	  echo "#include <test-$$type.h>"; \
	  echo "WRAPPER_DECL_$$func_args (WRAPPER_NAME (FUNC ($$func)))"; \
	  echo "#include <libm-test-$$func.c>"; \
	) > $@

$(foreach t,$(test-types),\
	    $(objpfx)libm-test-support-$(t).c): $(objpfx)libm-test-support-%.c:
	( \
	  echo "#include <test-$*.h>"; \
	  echo "#include <libm-test-support.c>"; \
	) > $@

$(foreach t,$(test-types),\
	    $(objpfx)libm-test-support-$(t)-static.c): $(objpfx)libm-test-support-%.c:
	( \
	  echo "#include <test-$*.h>"; \
	  echo "#include <libm-test-support.c>"; \
	) > $@

$(addprefix $(objpfx), $(libm-tests.o)): $(objpfx)libm-test-ulps.h

define o-iterator-doit
$(foreach f,$(libm-test-funcs-all),\
	    $(objpfx)$(o)-$(f).o): $(objpfx)$(o)%.o: $(objpfx)libm-test%.c
endef
object-suffixes-left := $(libm-tests-base)
include $(o-iterator)

define o-iterator-doit
$(foreach f,$(libm-test-funcs-all-static),\
	    $(objpfx)$(o)-$(f)-static.o): $(objpfx)$(o)%.o: $(objpfx)libm-test%.c
endef
object-suffixes-left := $(libm-tests-base)
include $(o-iterator)

define o-iterator-doit
$(foreach f,$(libm-test-funcs-narrow),\
	    $(objpfx)$(o)-$(f).o): $(objpfx)$(o)%.o: \
				   $(objpfx)libm-test-narrow%.c
endef
object-suffixes-left := $(libm-tests-base-narrow)
include $(o-iterator)

define o-iterator-doit
$(foreach f,$(libm-test-funcs-all),\
	    $(objpfx)$(o)-$(f).o): CFLAGS += $(libm-test-no-inline-cflags)
endef
object-suffixes-left := $(libm-tests-base-normal)
include $(o-iterator)

define o-iterator-doit
$(foreach f,$(libm-test-funcs-all-static),\
	    $(objpfx)$(o)-$(f)-static.o): CFLAGS += $(libm-test-no-inline-cflags)
endef
object-suffixes-left := $(libm-tests-base-normal)
include $(o-iterator)

define o-iterator-doit
$(foreach f,$(libm-test-funcs-narrow),\
	    $(objpfx)$(o)-$(f).o): CFLAGS += $(libm-test-no-inline-cflags)
endef
object-suffixes-left := $(libm-tests-base-narrow)
include $(o-iterator)

define o-iterator-doit
$(foreach f,$($(o)-funcs),\
	    $(objpfx)test-$(o)-$(f).o): CFLAGS += $(libm-test-vec-cflags)
endef
object-suffixes-left := $(libmvec-tests)
include $(o-iterator)

define o-iterator-doit
$(addprefix $(objpfx),\
	    $(call libm-tests-for-type,$(o))): $(objpfx)libm-test-support-$(o).o
endef
object-suffixes-left := $(test-types)
include $(o-iterator)

define o-iterator-doit
$(addprefix $(objpfx),\
	    $(call libm-tests-for-type-static,$(o))): $(objpfx)libm-test-support-$(o)-static.o
endef
object-suffixes-left := $(test-types)
include $(o-iterator)

define o-iterator-doit
$(objpfx)libm-test-support-$(o).o: CFLAGS += $(libm-test-no-inline-cflags)
endef
object-suffixes-left := $(test-types)
include $(o-iterator)

# Run the math programs to automatically generate ULPs files.
.PHONY: regen-ulps

run-regen-ulps = $(test-wrapper-env) \
	    $(run-program-env) \
	    $($*-ENV) $(rtld-prefix) $(objpfx)$${run}

regen-ulps: $(addprefix $(objpfx),$(libm-tests))
	rm -f $(objpfx)ULPs; rm -f $(objpfx)NewUlps; \
	cp $(ulps-file) $(objpfx)libm-test-ulps; \
	for run in $(libm-tests); do \
	  echo "Regenerating ULPs for $${run}"; \
	  $(run-regen-ulps) -u -o $(objpfx); \
	  cat $(objpfx)ULPs >> $(objpfx)libm-test-ulps; \
	  rm $(objpfx)ULPs; \
	done; \
	$(PYTHON) gen-libm-test.py -n $(objpfx)NewUlps \
				   -u $(objpfx)libm-test-ulps; \
	echo "Automatic regeneration of ULPs complete."; \
	echo "Difference between the current baseline and the new baseline is:";\
	diff -urN $(ulps-file) $(objpfx)NewUlps; \
	echo "Copy $(objpfx)NewUlps to $(ulps-file) (relative to source)."

# The generated sysd-rules file defines rules like this for sources
# coming from sysdeps/ directories.  These rules find the generic sources.
define o-iterator-doit
$(objpfx)m_%$o: s_%.c $(before-compile); $$(compile-command.c)
endef
object-suffixes-left := $(all-object-suffixes)
include $(o-iterator)

# Likewise, for those generated files shared with libc.
define o-iterator-doit
$(objpfx)m_%$o: $(objpfx)s_%.c $(before-compile); $$(compile-command.c)
endef
object-suffixes-left := $(all-object-suffixes)
include $(o-iterator)

ifneq ($(long-double-fcts),yes)
# The `double' and `long double' types are the same on this machine.
# We won't compile the `long double' code at all.  Tell the `double' code
# to define aliases for the `FUNCl' names.
math-CPPFLAGS += -DNO_LONG_DOUBLE
# GCC 10 diagnoses aliases with types conflicting with built-in
# functions.
CFLAGS-w_acos.c += -fno-builtin-acosl
CFLAGS-w_acosh.c += -fno-builtin-acoshl
CFLAGS-w_asin.c += -fno-builtin-asinl
CFLAGS-s_asinh.c += -fno-builtin-asinhl
CFLAGS-s_atan.c += -fno-builtin-atanl
CFLAGS-w_atan2.c += -fno-builtin-atan2l
CFLAGS-w_atanh.c += -fno-builtin-atanhl
CFLAGS-s_cabs.c += -fno-builtin-cabsl
CFLAGS-s_cacos.c += -fno-builtin-cacosl
CFLAGS-s_cacosh.c += -fno-builtin-cacoshl
CFLAGS-s_canonicalize.c += -fno-builtin-canonicalizel
CFLAGS-s_carg.c += -fno-builtin-cargl
CFLAGS-s_casin.c += -fno-builtin-casinl
CFLAGS-s_casinh.c += -fno-builtin-casinhl
CFLAGS-s_catan.c += -fno-builtin-catanl
CFLAGS-s_catanh.c += -fno-builtin-catanhl
CFLAGS-s_cbrt.c += -fno-builtin-cbrtl
CFLAGS-s_ccos.c += -fno-builtin-ccosl
CFLAGS-s_ccosh.c += -fno-builtin-ccoshl
CFLAGS-s_ceil.c += -fno-builtin-ceill
CFLAGS-s_cexp.c += -fno-builtin-cexpl
CFLAGS-s_cimag.c += -fno-builtin-cimagl
CFLAGS-s_clog.c += -fno-builtin-clogl
CFLAGS-s_clog10.c += -fno-builtin-clog10l
CFLAGS-s_conj.c += -fno-builtin-conjl
CFLAGS-s_copysign.c += -fno-builtin-copysignl
CFLAGS-s_cos.c += -fno-builtin-cosl
CFLAGS-w_cosh.c += -fno-builtin-coshl
CFLAGS-s_cpow.c += -fno-builtin-cpowl
CFLAGS-s_cproj.c += -fno-builtin-cprojl
CFLAGS-s_creal.c += -fno-builtin-creall
CFLAGS-s_csin.c += -fno-builtin-csinl
CFLAGS-s_csinh.c += -fno-builtin-csinhl
CFLAGS-s_csqrt.c += -fno-builtin-csqrtl
CFLAGS-s_ctan.c += -fno-builtin-ctanl
CFLAGS-s_ctanh.c += -fno-builtin-ctanhl
CFLAGS-s_erf.c += -fno-builtin-erfl
CFLAGS-s_erfc.c += -fno-builtin-erfcl
CFLAGS-e_exp.c += -fno-builtin-expl
CFLAGS-w_exp10.c += -fno-builtin-exp10l
CFLAGS-s_exp10m1.c += -fno-builtin-exp10m1l
CFLAGS-e_exp2.c += -fno-builtin-exp2l
CFLAGS-s_exp2m1.c += -fno-builtin-exp2m1l
CFLAGS-s_expm1.c += -fno-builtin-expm1l
CFLAGS-s_f32xaddf64.c += -fno-builtin-daddl
CFLAGS-s_f32xdivf64.c += -fno-builtin-ddivl
CFLAGS-s_f32xfmaf64.c += -fno-builtin-dfmal
CFLAGS-s_f32xmulf64.c += -fno-builtin-dmull
CFLAGS-s_f32xsqrtf64.c += -fno-builtin-dsqrtl
CFLAGS-s_f32xsubf64.c += -fno-builtin-dsubl
CFLAGS-s_fabs.c += -fno-builtin-fabsl
CFLAGS-s_fadd.c += -fno-builtin-faddl
CFLAGS-s_fdim.c += -fno-builtin-fdiml
CFLAGS-s_fdiv.c += -fno-builtin-fdivl
CFLAGS-s_ffma.c += -fno-builtin-ffmal
CFLAGS-s_finite.c += -fno-builtin-finitel
CFLAGS-s_floor.c += -fno-builtin-floorl
CFLAGS-s_fma.c += -fno-builtin-fmal
CFLAGS-s_fmax.c += -fno-builtin-fmaxl
CFLAGS-s_fmaximum.c += -fno-builtin-fmaximuml
CFLAGS-s_fmaximum_mag.c += -fno-builtin-fmaximum_magl
CFLAGS-s_fmaximum_mag_num.c += -fno-builtin-fmaximum_mag_numl
CFLAGS-s_fmaximum_num.c += -fno-builtin-fmaximum_numl
CFLAGS-s_fmaxmag.c += -fno-builtin-fmaxmagl
CFLAGS-s_fmin.c += -fno-builtin-fminl
CFLAGS-s_fminimum.c += -fno-builtin-fminimuml
CFLAGS-s_fminimum_mag.c += -fno-builtin-fminimum_magl
CFLAGS-s_fminimum_mag_num.c += -fno-builtin-fminimum_mag_numl
CFLAGS-s_fminimum_num.c += -fno-builtin-fminimum_numl
CFLAGS-s_fminmag.c += -fno-builtin-fminmagl
CFLAGS-w_fmod.c += -fno-builtin-fmodl
CFLAGS-s_fmul.c += -fno-builtin-fmull
CFLAGS-s_frexp.c += -fno-builtin-frexpl
CFLAGS-s_fromfp.c += -fno-builtin-fromfpl
CFLAGS-s_fromfpx.c += -fno-builtin-fromfpxl
CFLAGS-s_fsqrt.c += -fno-builtin-fsqrtl
CFLAGS-s_fsub.c += -fno-builtin-fsubl
CFLAGS-s_getpayload.c += -fno-builtin-getpayloadl
CFLAGS-w_hypot.c += -fno-builtin-hypotl
CFLAGS-w_ilogb.c += -fno-builtin-ilogbl
CFLAGS-s_isinf.c += -fno-builtin-isinfl
CFLAGS-s_isnan.c += -fno-builtin-isnanl
CFLAGS-w_j0.c += -fno-builtin-j0l
CFLAGS-w_j1.c += -fno-builtin-j1l
CFLAGS-w_jn.c += -fno-builtin-jnl
CFLAGS-s_ldexp.c += -fno-builtin-ldexpl
CFLAGS-w_lgamma.c += -fno-builtin-lgammal
CFLAGS-w_lgamma_r.c += -fno-builtin-lgammal_r
CFLAGS-w_llogb.c += -fno-builtin-llogbl
CFLAGS-s_llrint.c += -fno-builtin-llrintl
CFLAGS-s_llround.c += -fno-builtin-llroundl
CFLAGS-e_log.c += -fno-builtin-logl
CFLAGS-w_log10.c += -fno-builtin-log10l
CFLAGS-s_log10p1.c += -fno-builtin-log10p1l
CFLAGS-w_log1p.c += -fno-builtin-log1pl -fno-builtin-logp1l
CFLAGS-e_log2.c += -fno-builtin-log2l
CFLAGS-s_log2p1.c += -fno-builtin-log2p1l
CFLAGS-s_logb.c += -fno-builtin-logbl
CFLAGS-s_lrint.c += -fno-builtin-lrintl
CFLAGS-s_lround.c += -fno-builtin-lroundl
CFLAGS-s_modf.c += -fno-builtin-modfl
CFLAGS-s_nan.c += -fno-builtin-nanl
CFLAGS-s_nearbyint.c += -fno-builtin-nearbyintl
CFLAGS-s_nextafter.c += -fno-builtin-nextafterl
CFLAGS-s_nextdown.c += -fno-builtin-nextdownl
CFLAGS-s_nexttoward.c += -fno-builtin-nexttoward -fno-builtin-nexttowardl
CFLAGS-s_nexttowardf.c += -fno-builtin-nexttowardf
CFLAGS-s_nextup.c += -fno-builtin-nextupl
CFLAGS-e_pow.c += -fno-builtin-powl
CFLAGS-w_remainder.c += -fno-builtin-remainderl -fno-builtin-dreml
CFLAGS-s_remquo.c += -fno-builtin-remquol
CFLAGS-s_rint.c += -fno-builtin-rintl
CFLAGS-s_round.c += -fno-builtin-roundl
CFLAGS-s_roundeven.c += -fno-builtin-roundevenl
CFLAGS-w_scalb.c += -fno-builtin-scalbl
CFLAGS-w_scalbln.c += -fno-builtin-scalblnl
CFLAGS-s_scalbn.c += -fno-builtin-scalbnl
CFLAGS-s_setpayload.c += -fno-builtin-setpayloadl
CFLAGS-s_setpayloadsig.c += -fno-builtin-setpayloadsigl
CFLAGS-s_significand.c += -fno-builtin-significandl
CFLAGS-s_sin.c += -fno-builtin-sinl
CFLAGS-s_sincos.c += -fno-builtin-sincosl
CFLAGS-w_sinh.c += -fno-builtin-sinhl
CFLAGS-w_sqrt.c += -fno-builtin-sqrtl
CFLAGS-s_tan.c += -fno-builtin-tanl
CFLAGS-s_tanh.c += -fno-builtin-tanhl
CFLAGS-w_tgamma.c += -fno-builtin-tgammal
CFLAGS-s_totalorder.c += -fno-builtin-totalorderl
CFLAGS-s_totalordermag.c += -fno-builtin-totalordermagl
CFLAGS-s_trunc.c += -fno-builtin-truncl
CFLAGS-s_ufromfp.c += -fno-builtin-ufromfpl
CFLAGS-s_ufromfpx.c += -fno-builtin-ufromfpxl
CFLAGS-s_y0.c += -fno-builtin-y0l
CFLAGS-s_y1.c += -fno-builtin-y1l
CFLAGS-s_yn.c += -fno-builtin-ynl
endif

# Likewise, for _Float32x and _Float64 aliases.
CFLAGS-w_acos.c += -fno-builtin-acosf32x -fno-builtin-acosf64
CFLAGS-w_acosh.c += -fno-builtin-acoshf32x -fno-builtin-acoshf64
CFLAGS-w_asin.c += -fno-builtin-asinf32x -fno-builtin-asinf64
CFLAGS-s_asinh.c += -fno-builtin-asinhf32x -fno-builtin-asinhf64
CFLAGS-s_atan.c += -fno-builtin-atanf32x -fno-builtin-atanf64
CFLAGS-w_atan2.c += -fno-builtin-atan2f32x -fno-builtin-atan2f64
CFLAGS-w_atanh.c += -fno-builtin-atanhf32x -fno-builtin-atanhf64
CFLAGS-s_cabs.c += -fno-builtin-cabsf32x -fno-builtin-cabsf64
CFLAGS-s_cacos.c += -fno-builtin-cacosf32x -fno-builtin-cacosf64
CFLAGS-s_cacosh.c += -fno-builtin-cacoshf32x -fno-builtin-cacoshf64
CFLAGS-s_canonicalize.c += -fno-builtin-canonicalizef32x -fno-builtin-canonicalizef64
CFLAGS-s_carg.c += -fno-builtin-cargf32x -fno-builtin-cargf64
CFLAGS-s_casin.c += -fno-builtin-casinf32x -fno-builtin-casinf64
CFLAGS-s_casinh.c += -fno-builtin-casinhf32x -fno-builtin-casinhf64
CFLAGS-s_catan.c += -fno-builtin-catanf32x -fno-builtin-catanf64
CFLAGS-s_catanh.c += -fno-builtin-catanhf32x -fno-builtin-catanhf64
CFLAGS-s_cbrt.c += -fno-builtin-cbrtf32x -fno-builtin-cbrtf64
CFLAGS-s_ccos.c += -fno-builtin-ccosf32x -fno-builtin-ccosf64
CFLAGS-s_ccosh.c += -fno-builtin-ccoshf32x -fno-builtin-ccoshf64
CFLAGS-s_ceil.c += -fno-builtin-ceilf32x -fno-builtin-ceilf64
CFLAGS-s_cexp.c += -fno-builtin-cexpf32x -fno-builtin-cexpf64
CFLAGS-s_cimag.c += -fno-builtin-cimagf32x -fno-builtin-cimagf64
CFLAGS-s_clog.c += -fno-builtin-clogf32x -fno-builtin-clogf64
CFLAGS-s_clog10.c += -fno-builtin-clog10f32x -fno-builtin-clog10f64
CFLAGS-s_conj.c += -fno-builtin-conjf32x -fno-builtin-conjf64
CFLAGS-s_copysign.c += -fno-builtin-copysignf32x -fno-builtin-copysignf64
CFLAGS-s_cos.c += -fno-builtin-cosf32x -fno-builtin-cosf64
CFLAGS-w_cosh.c += -fno-builtin-coshf32x -fno-builtin-coshf64
CFLAGS-s_cpow.c += -fno-builtin-cpowf32x -fno-builtin-cpowf64
CFLAGS-s_cproj.c += -fno-builtin-cprojf32x -fno-builtin-cprojf64
CFLAGS-s_creal.c += -fno-builtin-crealf32x -fno-builtin-crealf64
CFLAGS-s_csin.c += -fno-builtin-csinf32x -fno-builtin-csinf64
CFLAGS-s_csinh.c += -fno-builtin-csinhf32x -fno-builtin-csinhf64
CFLAGS-s_csqrt.c += -fno-builtin-csqrtf32x -fno-builtin-csqrtf64
CFLAGS-s_ctan.c += -fno-builtin-ctanf32x -fno-builtin-ctanf64
CFLAGS-s_ctanh.c += -fno-builtin-ctanhf32x -fno-builtin-ctanhf64
CFLAGS-s_erf.c += -fno-builtin-erff32x -fno-builtin-erff64
CFLAGS-s_erfc.c += -fno-builtin-erfcf32x -fno-builtin-erfcf64
CFLAGS-e_exp.c += -fno-builtin-expf32x -fno-builtin-expf64
CFLAGS-w_exp10.c += -fno-builtin-exp10f32x -fno-builtin-exp10f64
CFLAGS-s_exp10m1.c += -fno-builtin-exp10m1f32x -fno-builtin-exp10m1f64
CFLAGS-e_exp2.c += -fno-builtin-exp2f32x -fno-builtin-exp2f64
CFLAGS-s_exp2m1.c += -fno-builtin-exp2m1f32x -fno-builtin-exp2m1f64
CFLAGS-s_expm1.c += -fno-builtin-expm1f32x -fno-builtin-expm1f64
CFLAGS-s_fabs.c += -fno-builtin-fabsf32x -fno-builtin-fabsf64
CFLAGS-s_fadd.c += -fno-builtin-f32addf32x -fno-builtin-f32addf64
CFLAGS-s_fdim.c += -fno-builtin-fdimf32x -fno-builtin-fdimf64
CFLAGS-s_fdiv.c += -fno-builtin-f32divf32x -fno-builtin-f32divf64
CFLAGS-s_ffma.c += -fno-builtin-f32fmaf32x -fno-builtin-f32fmaf64
CFLAGS-s_floor.c += -fno-builtin-floorf32x -fno-builtin-floorf64
CFLAGS-s_fma.c += -fno-builtin-fmaf32x -fno-builtin-fmaf64
CFLAGS-s_fmax.c += -fno-builtin-fmaxf32x -fno-builtin-fmaxf64
CFLAGS-s_fmaximum.c += -fno-builtin-fmaximumf32x -fno-builtin-fmaximumf64
CFLAGS-s_fmaximum_mag.c += -fno-builtin-fmaximum_magf32x -fno-builtin-fmaximum_magf64
CFLAGS-s_fmaximum_mag_num.c += -fno-builtin-fmaximum_mag_numf32x -fno-builtin-fmaximum_mag_numf64
CFLAGS-s_fmaximum_num.c += -fno-builtin-fmaximum_numf32x -fno-builtin-fmaximum_numf64
CFLAGS-s_fmaxmag.c += -fno-builtin-fmaxmagf32x -fno-builtin-fmaxmagf64
CFLAGS-s_fmin.c += -fno-builtin-fminf32x -fno-builtin-fminf64
CFLAGS-s_fminimum.c += -fno-builtin-fminimumf32x -fno-builtin-fminimumf64
CFLAGS-s_fminimum_mag.c += -fno-builtin-fminimum_magf32x -fno-builtin-fminimum_magf64
CFLAGS-s_fminimum_mag_num.c += -fno-builtin-fminimum_mag_numf32x -fno-builtin-fminimum_mag_numf64
CFLAGS-s_fminimum_num.c += -fno-builtin-fminimum_numf32x -fno-builtin-fminimum_numf64
CFLAGS-s_fminmag.c += -fno-builtin-fminmagf32x -fno-builtin-fminmagf64
CFLAGS-w_fmod.c += -fno-builtin-fmodf32x -fno-builtin-fmodf64
CFLAGS-s_fmul.c += -fno-builtin-f32mulf32x -fno-builtin-f32mulf64
CFLAGS-s_frexp.c += -fno-builtin-frexpf32x -fno-builtin-frexpf64
CFLAGS-s_fromfp.c += -fno-builtin-fromfpf32x -fno-builtin-fromfpf64
CFLAGS-s_fromfpx.c += -fno-builtin-fromfpxf32x -fno-builtin-fromfpxf64
CFLAGS-s_fsqrt.c += -fno-builtin-f32sqrtf32x -fno-builtin-f32sqrtf64
CFLAGS-s_fsub.c += -fno-builtin-f32subf32x -fno-builtin-f32subf64
CFLAGS-s_getpayload.c += -fno-builtin-getpayloadf32x -fno-builtin-getpayloadf64
CFLAGS-w_hypot.c += -fno-builtin-hypotf32x -fno-builtin-hypotf64
CFLAGS-w_ilogb.c += -fno-builtin-ilogbf32x -fno-builtin-ilogbf64
CFLAGS-w_j0.c += -fno-builtin-j0f32x -fno-builtin-j0f64
CFLAGS-w_j1.c += -fno-builtin-j1f32x -fno-builtin-j1f64
CFLAGS-w_jn.c += -fno-builtin-jnf32x -fno-builtin-jnf64
CFLAGS-s_ldexp.c += -fno-builtin-ldexpf32x -fno-builtin-ldexpf64
CFLAGS-w_lgamma.c += -fno-builtin-lgammaf32x -fno-builtin-lgammaf64
CFLAGS-w_lgamma_r.c += -fno-builtin-lgammaf32x_r -fno-builtin-lgammaf64_r
CFLAGS-w_llogb.c += -fno-builtin-llogbf32x -fno-builtin-llogbf64
CFLAGS-s_llrint.c += -fno-builtin-llrintf32x -fno-builtin-llrintf64
CFLAGS-s_llround.c += -fno-builtin-llroundf32x -fno-builtin-llroundf64
CFLAGS-e_log.c += -fno-builtin-logf32x -fno-builtin-logf64
CFLAGS-w_log10.c += -fno-builtin-log10f32x -fno-builtin-log10f64
CFLAGS-s_log10p1.c += -fno-builtin-log10p1f32x -fno-builtin-log10p1f64
CFLAGS-w_log1p.c += -fno-builtin-log1pf32x -fno-builtin-log1pf64 -fno-builtin-logp1f32x -fno-builtin-logp1f64
CFLAGS-e_log2.c += -fno-builtin-log2f32x -fno-builtin-log2f64
CFLAGS-s_log2p1.c += -fno-builtin-log2p1f32x -fno-builtin-log2p1f64
CFLAGS-s_logb.c += -fno-builtin-logbf32x -fno-builtin-logbf64
CFLAGS-s_lrint.c += -fno-builtin-lrintf32x -fno-builtin-lrintf64
CFLAGS-s_lround.c += -fno-builtin-lroundf32x -fno-builtin-lroundf64
CFLAGS-s_modf.c += -fno-builtin-modff32x -fno-builtin-modff64
CFLAGS-s_nan.c += -fno-builtin-nanf32x -fno-builtin-nanf64
CFLAGS-s_nearbyint.c += -fno-builtin-nearbyintf32x -fno-builtin-nearbyintf64
CFLAGS-s_nextafter.c += -fno-builtin-nextafterf32x -fno-builtin-nextafterf64
CFLAGS-s_nextdown.c += -fno-builtin-nextdownf32x -fno-builtin-nextdownf64
CFLAGS-s_nextup.c += -fno-builtin-nextupf32x -fno-builtin-nextupf64
CFLAGS-e_pow.c += -fno-builtin-powf32x -fno-builtin-powf64
CFLAGS-w_remainder.c += -fno-builtin-remainderf32x -fno-builtin-remainderf64
CFLAGS-s_remquo.c += -fno-builtin-remquof32x -fno-builtin-remquof64
CFLAGS-s_rint.c += -fno-builtin-rintf32x -fno-builtin-rintf64
CFLAGS-s_round.c += -fno-builtin-roundf32x -fno-builtin-roundf64
CFLAGS-s_roundeven.c += -fno-builtin-roundevenf32x -fno-builtin-roundevenf64
CFLAGS-w_scalbln.c += -fno-builtin-scalblnf32x -fno-builtin-scalblnf64
CFLAGS-s_scalbn.c += -fno-builtin-scalbnf32x -fno-builtin-scalbnf64
CFLAGS-s_setpayload.c += -fno-builtin-setpayloadf32x -fno-builtin-setpayloadf64
CFLAGS-s_setpayloadsig.c += -fno-builtin-setpayloadsigf32x -fno-builtin-setpayloadsigf64
CFLAGS-s_sin.c += -fno-builtin-sinf32x -fno-builtin-sinf64
CFLAGS-s_sincos.c += -fno-builtin-sincosf32x -fno-builtin-sincosf64
CFLAGS-w_sinh.c += -fno-builtin-sinhf32x -fno-builtin-sinhf64
CFLAGS-w_sqrt.c += -fno-builtin-sqrtf32x -fno-builtin-sqrtf64
CFLAGS-s_tan.c += -fno-builtin-tanf32x -fno-builtin-tanf64
CFLAGS-s_tanh.c += -fno-builtin-tanhf32x -fno-builtin-tanhf64
CFLAGS-w_tgamma.c += -fno-builtin-tgammaf32x -fno-builtin-tgammaf64
CFLAGS-s_totalorder.c += -fno-builtin-totalorderf32x -fno-builtin-totalorderf64
CFLAGS-s_totalordermag.c += -fno-builtin-totalordermagf32x -fno-builtin-totalordermagf64
CFLAGS-s_trunc.c += -fno-builtin-truncf32x -fno-builtin-truncf64
CFLAGS-s_ufromfp.c += -fno-builtin-ufromfpf32x -fno-builtin-ufromfpf64
CFLAGS-s_ufromfpx.c += -fno-builtin-ufromfpxf32x -fno-builtin-ufromfpxf64
CFLAGS-s_y0.c += -fno-builtin-y0f32x -fno-builtin-y0f64
CFLAGS-s_y1.c += -fno-builtin-y1f32x -fno-builtin-y1f64
CFLAGS-s_yn.c += -fno-builtin-ynf32x -fno-builtin-ynf64

# Likewise, for _Float32 aliases.
CFLAGS-w_acosf.c += -fno-builtin-acosf32
CFLAGS-w_acoshf.c += -fno-builtin-acoshf32
CFLAGS-w_asinf.c += -fno-builtin-asinf32
CFLAGS-s_asinhf.c += -fno-builtin-asinhf32
CFLAGS-s_atanf.c += -fno-builtin-atanf32
CFLAGS-w_atan2f.c += -fno-builtin-atan2f32
CFLAGS-w_atanhf.c += -fno-builtin-atanhf32
CFLAGS-s_cabsf.c += -fno-builtin-cabsf32
CFLAGS-s_cacosf.c += -fno-builtin-cacosf32
CFLAGS-s_cacoshf.c += -fno-builtin-cacoshf32
CFLAGS-s_canonicalizef.c += -fno-builtin-canonicalizef32
CFLAGS-s_cargf.c += -fno-builtin-cargf32
CFLAGS-s_casinf.c += -fno-builtin-casinf32
CFLAGS-s_casinhf.c += -fno-builtin-casinhf32
CFLAGS-s_catanf.c += -fno-builtin-catanf32
CFLAGS-s_catanhf.c += -fno-builtin-catanhf32
CFLAGS-s_cbrtf.c += -fno-builtin-cbrtf32
CFLAGS-s_ccosf.c += -fno-builtin-ccosf32
CFLAGS-s_ccoshf.c += -fno-builtin-ccoshf32
CFLAGS-s_ceilf.c += -fno-builtin-ceilf32
CFLAGS-s_cexpf.c += -fno-builtin-cexpf32
CFLAGS-s_cimagf.c += -fno-builtin-cimagf32
CFLAGS-s_clogf.c += -fno-builtin-clogf32
CFLAGS-s_clog10f.c += -fno-builtin-clog10f32
CFLAGS-s_conjf.c += -fno-builtin-conjf32
CFLAGS-s_copysignf.c += -fno-builtin-copysignf32
CFLAGS-s_cosf.c += -fno-builtin-cosf32
CFLAGS-w_coshf.c += -fno-builtin-coshf32
CFLAGS-s_cpowf.c += -fno-builtin-cpowf32
CFLAGS-s_cprojf.c += -fno-builtin-cprojf32
CFLAGS-s_crealf.c += -fno-builtin-crealf32
CFLAGS-s_csinf.c += -fno-builtin-csinf32
CFLAGS-s_csinhf.c += -fno-builtin-csinhf32
CFLAGS-s_csqrtf.c += -fno-builtin-csqrtf32
CFLAGS-s_ctanf.c += -fno-builtin-ctanf32
CFLAGS-s_ctanhf.c += -fno-builtin-ctanhf32
CFLAGS-s_erff.c += -fno-builtin-erff32
CFLAGS-s_erfcf.c += -fno-builtin-erfcf32
CFLAGS-e_expf.c += -fno-builtin-expf32
CFLAGS-w_exp10f.c += -fno-builtin-exp10f32
CFLAGS-s_exp10m1f.c += -fno-builtin-exp10m1f32
CFLAGS-e_exp2f.c += -fno-builtin-exp2f32
CFLAGS-s_exp2m1f.c += -fno-builtin-exp2m1f32
CFLAGS-s_expm1f.c += -fno-builtin-expm1f32
CFLAGS-s_fabsf.c += -fno-builtin-fabsf32
CFLAGS-s_fdimf.c += -fno-builtin-fdimf32
CFLAGS-s_floorf.c += -fno-builtin-floorf32
CFLAGS-s_fmaf.c += -fno-builtin-fmaf32
CFLAGS-s_fmaxf.c += -fno-builtin-fmaxf32
CFLAGS-s_fmaximumf.c += -fno-builtin-fmaximumf32
CFLAGS-s_fmaximum_magf.c += -fno-builtin-fmaximum_magf32
CFLAGS-s_fmaximum_mag_numf.c += -fno-builtin-fmaximum_mag_numf32
CFLAGS-s_fmaximum_numf.c += -fno-builtin-fmaximum_numf32
CFLAGS-s_fmaxmagf.c += -fno-builtin-fmaxmagf32
CFLAGS-s_fminf.c += -fno-builtin-fminf32
CFLAGS-s_fminimumf.c += -fno-builtin-fminimumf32
CFLAGS-s_fminimum_magf.c += -fno-builtin-fminimum_magf32
CFLAGS-s_fminimum_mag_numf.c += -fno-builtin-fminimum_mag_numf32
CFLAGS-s_fminimum_numf.c += -fno-builtin-fminimum_numf32
CFLAGS-s_fminmagf.c += -fno-builtin-fminmagf32
CFLAGS-w_fmodf.c += -fno-builtin-fmodf32
CFLAGS-s_frexpf.c += -fno-builtin-frexpf32
CFLAGS-s_fromfpf.c += -fno-builtin-fromfpf32
CFLAGS-s_fromfpxf.c += -fno-builtin-fromfpxf32
CFLAGS-s_getpayloadf.c += -fno-builtin-getpayloadf32
CFLAGS-w_hypotf.c += -fno-builtin-hypotf32
CFLAGS-w_ilogbf.c += -fno-builtin-ilogbf32
CFLAGS-w_j0f.c += -fno-builtin-j0f32
CFLAGS-w_j1f.c += -fno-builtin-j1f32
CFLAGS-w_jnf.c += -fno-builtin-jnf32
CFLAGS-s_ldexpf.c += -fno-builtin-ldexpf32
CFLAGS-w_lgammaf.c += -fno-builtin-lgammaf32
CFLAGS-w_lgammaf_r.c += -fno-builtin-lgammaf32_r
CFLAGS-w_llogbf.c += -fno-builtin-llogbf32
CFLAGS-s_llrintf.c += -fno-builtin-llrintf32
CFLAGS-s_llroundf.c += -fno-builtin-llroundf32
CFLAGS-e_logf.c += -fno-builtin-logf32
CFLAGS-w_log10f.c += -fno-builtin-log10f32
CFLAGS-s_log10p1f.c += -fno-builtin-log10p1f32
CFLAGS-w_log1pf.c += -fno-builtin-log1pf32 -fno-builtin-logp1f32
CFLAGS-e_log2f.c += -fno-builtin-log2f32
CFLAGS-s_log2p1f.c += -fno-builtin-log2p1f32
CFLAGS-s_logbf.c += -fno-builtin-logbf32
CFLAGS-s_lrintf.c += -fno-builtin-lrintf32
CFLAGS-s_lroundf.c += -fno-builtin-lroundf32
CFLAGS-s_modff.c += -fno-builtin-modff32
CFLAGS-s_nanf.c += -fno-builtin-nanf32
CFLAGS-s_nearbyintf.c += -fno-builtin-nearbyintf32
CFLAGS-s_nextafterf.c += -fno-builtin-nextafterf32
CFLAGS-s_nextdownf.c += -fno-builtin-nextdownf32
CFLAGS-s_nextupf.c += -fno-builtin-nextupf32
CFLAGS-e_powf.c += -fno-builtin-powf32
CFLAGS-w_remainderf.c += -fno-builtin-remainderf32
CFLAGS-s_remquof.c += -fno-builtin-remquof32
CFLAGS-s_rintf.c += -fno-builtin-rintf32
CFLAGS-s_roundf.c += -fno-builtin-roundf32
CFLAGS-s_roundevenf.c += -fno-builtin-roundevenf32
CFLAGS-w_scalblnf.c += -fno-builtin-scalblnf32
CFLAGS-s_scalbnf.c += -fno-builtin-scalbnf32
CFLAGS-s_setpayloadf.c += -fno-builtin-setpayloadf32
CFLAGS-s_setpayloadsigf.c += -fno-builtin-setpayloadsigf32
CFLAGS-s_sinf.c += -fno-builtin-sinf32
CFLAGS-s_sincosf.c += -fno-builtin-sincosf32
CFLAGS-w_sinhf.c += -fno-builtin-sinhf32
CFLAGS-w_sqrtf.c += -fno-builtin-sqrtf32
CFLAGS-s_tanf.c += -fno-builtin-tanf32
CFLAGS-s_tanhf.c += -fno-builtin-tanhf32
CFLAGS-w_tgammaf.c += -fno-builtin-tgammaf32
CFLAGS-s_totalorderf.c += -fno-builtin-totalorderf32
CFLAGS-s_totalordermagf.c += -fno-builtin-totalordermagf32
CFLAGS-s_truncf.c += -fno-builtin-truncf32
CFLAGS-s_ufromfpf.c += -fno-builtin-ufromfpf32
CFLAGS-s_ufromfpxf.c += -fno-builtin-ufromfpxf32
CFLAGS-s_y0f.c += -fno-builtin-y0f32
CFLAGS-s_y1f.c += -fno-builtin-y1f32
CFLAGS-s_ynf.c += -fno-builtin-ynf32

# These files quiet sNaNs in a way that is optimized away without
# -fsignaling-nans.
CFLAGS-s_modf.c += $(config-cflags-signaling-nans)
CFLAGS-s_modff.c += $(config-cflags-signaling-nans)
CFLAGS-s_modfl.c += $(config-cflags-signaling-nans)
CFLAGS-s_modff128.c += $(config-cflags-signaling-nans)

$(addprefix $(objpfx),\
	    $(filter-out $(tests-static) $(libm-tests-vector),\
			 $(tests) $(tests-internal))): $(libm)
$(addprefix $(objpfx),$(tests-static)): $(objpfx)libm.a
define o-iterator-doit
$(foreach f,$($(o)-funcs),\
	    $(objpfx)test-$(o)-$(f)): $(objpfx)test-$(o)-wrappers.o \
				      $(libm) $(libmvec)
endef
object-suffixes-left := $(libmvec-tests)
include $(o-iterator)

$(objpfx)test-fenv-tls: $(shared-thread-library)
