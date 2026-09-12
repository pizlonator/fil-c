# The`long double' type is a distinct type we support if
# -mlong-double-128 option is used (or when it becomes a default
# when -mlong-double-64 is not used).
long-double-fcts = yes
ifeq (,$(filter -mlong-double-128,$(sysdep-CFLAGS)))
sysdep-CFLAGS += -mlong-double-128
endif

ifeq ($(subdir),math)
libm-routines += s_nexttowardfd
routines += nldbl-compat

extra-libs += libnldbl
libnldbl-calls = \
  acos \
  acosh \
  acospi \
  asin \
  asinh \
  asinpi \
  asprintf \
  asprintf_chk \
  atan \
  atan2 \
  atan2pi \
  atanh \
  atanpi \
  cabs \
  cacos \
  cacosh \
  canonicalize \
  carg \
  casin \
  casinh \
  catan \
  catanh \
  cbrt \
  ccos \
  ccosh \
  ceil \
  cexp \
  cimag \
  clog \
  clog10 \
  compoundn \
  conj \
  copysign \
  cos \
  cosh \
  cospi \
  cpow \
  cproj \
  creal \
  csin \
  csinh \
  csqrt \
  ctan \
  ctanh \
  dadd \
  ddiv \
  dfma \
  dmul \
  dprintf \
  dprintf_chk \
  dsqrt \
  dsub \
  erf \
  erfc \
  exp \
  exp10m1 \
  exp2 \
  exp10 \
  exp2m1 \
  expm1 \
  fabs \
  fadd \
  fdim \
  fdiv \
  ffma \
  finite \
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
  fmul \
  fprintf \
  fprintf_chk \
  frexp \
  fromfp \
  fromfpx \
  fscanf \
  fsqrt \
  fsub \
  fwprintf \
  fwprintf_chk \
  fwscanf \
  gamma \
  getpayload \
  hypot \
  ilogb \
  iovfscanf \
  isinf \
  isnan \
  isoc23_fscanf \
  isoc23_fwscanf \
  isoc23_scanf \
  isoc23_sscanf \
  isoc23_swscanf \
  isoc23_vfscanf \
  isoc23_vfwscanf \
  isoc23_vscanf \
  isoc23_vsscanf \
  isoc23_vswscanf \
  isoc23_vwscanf \
  isoc23_wscanf \
  isoc99_fscanf \
  isoc99_fwscanf \
  isoc99_scanf \
  isoc99_sscanf \
  isoc99_swscanf \
  isoc99_vfscanf \
  isoc99_vfwscanf \
  isoc99_vscanf \
  isoc99_vsscanf \
  isoc99_vswscanf \
  isoc99_vwscanf \
  isoc99_wscanf \
  j0 \
  j1 \
  jn \
  ldexp \
  lgamma \
  lgamma_r \
  llogb \
  llrint \
  llround \
  log \
  log2 \
  log10 \
  log10p1 \
  log1p \
  log2p1 \
  logb \
  lrint \
  lround \
  modf \
  nan \
  nearbyint \
  nextafter \
  nextdown \
  nexttoward \
  nexttowardf \
  nextup \
  obstack_printf \
  obstack_printf_chk \
  obstack_vprintf \
  obstack_vprintf_chk \
  pow \
  pown \
  powr \
  printf \
  printf_chk \
  printf_fp \
  printf_size \
  qecvt \
  qecvt_r \
  qfcvt \
  qfcvt_r \
  qgcvt \
  remainder \
  remquo \
  rint \
  rootn \
  round \
  roundeven \
  rsqrt \
  scalb \
  scalbln \
  scalbn \
  scanf \
  setpayload \
  setpayloadsig \
  signbit \
  significand \
  sin \
  sincos \
  sinh \
  sinpi \
  snprintf \
  snprintf_chk \
  sprintf \
  sprintf_chk \
  sqrt \
  sscanf \
  strfmon \
  strfmon_l \
  strfroml \
  strtold \
  strtold_l \
  strtoldint \
  swprintf \
  swprintf_chk \
  swscanf \
  syslog \
  syslog_chk \
  tan \
  tanh \
  tanpi \
  tgamma \
  totalorder \
  totalordermag \
  trunc \
  ufromfp \
  ufromfpx \
  vasprintf \
  vasprintf_chk \
  vdprintf \
  vdprintf_chk \
  vfprintf \
  vfprintf_chk \
  vfscanf \
  vfwprintf \
  vfwprintf_chk \
  vfwscanf \
  vprintf \
  vprintf_chk \
  vscanf \
  vsnprintf \
  vsnprintf_chk \
  vsprintf \
  vsprintf_chk \
  vsscanf \
  vswprintf \
  vswprintf_chk \
  vswscanf \
  vsyslog \
  vsyslog_chk \
  vwprintf \
  vwprintf_chk \
  vwscanf \
  wcstold \
  wcstold_l \
  wcstoldint \
  wprintf \
  wprintf_chk \
  wscanf \
  y0 \
  y1 \
  yn \
# libnldbl-calls
libnldbl-routines = $(libnldbl-calls:%=nldbl-%)
libnldbl-inhibit-o = $(object-suffixes)
libnldbl-static-only-routines = $(libnldbl-routines)
extra-objs += $(addsuffix .oS, $(libnldbl-routines))

CFLAGS-nldbl-acos.c = -fno-builtin-acosl
CFLAGS-nldbl-acosh.c = -fno-builtin-acoshl
CFLAGS-nldbl-acospi.c = -fno-builtin-acospil
CFLAGS-nldbl-asin.c = -fno-builtin-asinl
CFLAGS-nldbl-asinh.c = -fno-builtin-asinhl
CFLAGS-nldbl-asinpi.c = -fno-builtin-asinpil
CFLAGS-nldbl-atan.c = -fno-builtin-atanl
CFLAGS-nldbl-atan2.c = -fno-builtin-atan2l
CFLAGS-nldbl-atan2pi.c = -fno-builtin-atan2pil
CFLAGS-nldbl-atanh.c = -fno-builtin-atanhl
CFLAGS-nldbl-atanpi.c = -fno-builtin-atanpil
CFLAGS-nldbl-cabs.c = -fno-builtin-cabsl
CFLAGS-nldbl-cacos.c = -fno-builtin-cacosl
CFLAGS-nldbl-cacosh.c = -fno-builtin-cacoshl
CFLAGS-nldbl-canonicalize.c = -fno-builtin-canonicalizel
CFLAGS-nldbl-carg.c = -fno-builtin-cargl
CFLAGS-nldbl-casin.c = -fno-builtin-casinl
CFLAGS-nldbl-casinh.c = -fno-builtin-casinhl
CFLAGS-nldbl-catan.c = -fno-builtin-catanl
CFLAGS-nldbl-catanh.c = -fno-builtin-catanhl
CFLAGS-nldbl-cbrt.c = -fno-builtin-cbrtl
CFLAGS-nldbl-ccos.c = -fno-builtin-ccosl
CFLAGS-nldbl-ccosh.c = -fno-builtin-ccoshl
CFLAGS-nldbl-ceil.c = -fno-builtin-ceill
CFLAGS-nldbl-cexp.c = -fno-builtin-cexpl
CFLAGS-nldbl-cimag.c = -fno-builtin-cimagl
CFLAGS-nldbl-clog.c = -fno-builtin-clogl
CFLAGS-nldbl-clog10.c = -fno-builtin-clog10l
CFLAGS-nldbl-compoundn.c = -fno-builtin-compoundnl
CFLAGS-nldbl-conj.c = -fno-builtin-conjl
CFLAGS-nldbl-copysign.c = -fno-builtin-copysignl
CFLAGS-nldbl-cos.c = -fno-builtin-cosl
CFLAGS-nldbl-cosh.c = -fno-builtin-coshl
CFLAGS-nldbl-cospi.c = -fno-builtin-cospil
CFLAGS-nldbl-cpow.c = -fno-builtin-cpowl
CFLAGS-nldbl-cproj.c = -fno-builtin-cprojl
CFLAGS-nldbl-creal.c = -fno-builtin-creall
CFLAGS-nldbl-csin.c = -fno-builtin-csinl
CFLAGS-nldbl-csinh.c = -fno-builtin-csinhl
CFLAGS-nldbl-csqrt.c = -fno-builtin-csqrtl
CFLAGS-nldbl-ctan.c = -fno-builtin-ctanl
CFLAGS-nldbl-ctanh.c = -fno-builtin-ctanhl
CFLAGS-nldbl-dadd.c = -fno-builtin-daddl
CFLAGS-nldbl-ddiv.c = -fno-builtin-ddivl
CFLAGS-nldbl-dfma.c = -fno-builtin-dfmal
CFLAGS-nldbl-dmul.c = -fno-builtin-dmull
CFLAGS-nldbl-dsqrt.c = -fno-builtin-dsqrtl
CFLAGS-nldbl-dsub.c = -fno-builtin-dsubl
CFLAGS-nldbl-erf.c = -fno-builtin-erfl
CFLAGS-nldbl-erfc.c = -fno-builtin-erfcl
CFLAGS-nldbl-exp.c = -fno-builtin-expl
CFLAGS-nldbl-exp10.c = -fno-builtin-exp10l
CFLAGS-nldbl-exp10m1.c = -fno-builtin-exp10m1l
CFLAGS-nldbl-exp2.c = -fno-builtin-exp2l
CFLAGS-nldbl-exp2m1.c = -fno-builtin-exp2m1l
CFLAGS-nldbl-expm1.c = -fno-builtin-expm1l
CFLAGS-nldbl-fabs.c = -fno-builtin-fabsl
CFLAGS-nldbl-fadd.c = -fno-builtin-faddl
CFLAGS-nldbl-fdim.c = -fno-builtin-fdiml
CFLAGS-nldbl-fdiv.c = -fno-builtin-fdivl
CFLAGS-nldbl-ffma.c = -fno-builtin-ffmal
CFLAGS-nldbl-finite.c = -fno-builtin-finitel
CFLAGS-nldbl-floor.c = -fno-builtin-floorl
CFLAGS-nldbl-fma.c = -fno-builtin-fmal
CFLAGS-nldbl-fmax.c = -fno-builtin-fmaxl
CFLAGS-nldbl-fmaximum.c += -fno-builtin-fmaximuml
CFLAGS-nldbl-fmaximum_mag.c += -fno-builtin-fmaximum_magl
CFLAGS-nldbl-fmaximum_mag_num.c += -fno-builtin-fmaximum_mag_numl
CFLAGS-nldbl-fmaximum_num.c += -fno-builtin-fmaximum_numl
CFLAGS-nldbl-fmaxmag.c = -fno-builtin-fmaxmagl
CFLAGS-nldbl-fmin.c = -fno-builtin-fminl
CFLAGS-nldbl-fminimum.c += -fno-builtin-fminimuml
CFLAGS-nldbl-fminimum_mag.c += -fno-builtin-fminimum_magl
CFLAGS-nldbl-fminimum_mag_num.c += -fno-builtin-fminimum_mag_numl
CFLAGS-nldbl-fminimum_num.c += -fno-builtin-fminimum_numl
CFLAGS-nldbl-fminmag.c = -fno-builtin-fminmagl
CFLAGS-nldbl-fmod.c = -fno-builtin-fmodl
CFLAGS-nldbl-fmul.c = -fno-builtin-fmull
CFLAGS-nldbl-frexp.c = -fno-builtin-frexpl
CFLAGS-nldbl-fromfp.c = -fno-builtin-fromfpl
CFLAGS-nldbl-fromfpx.c = -fno-builtin-fromfpxl
CFLAGS-nldbl-fsqrt.c = -fno-builtin-fsqrtl
CFLAGS-nldbl-fsub.c = -fno-builtin-fsubl
CFLAGS-nldbl-gamma.c = -fno-builtin-gammal
CFLAGS-nldbl-getpayload.c = -fno-builtin-getpayloadl
CFLAGS-nldbl-hypot.c = -fno-builtin-hypotl
CFLAGS-nldbl-ilogb.c = -fno-builtin-ilogbl
CFLAGS-nldbl-isinf.c = -fno-builtin-isinfl
CFLAGS-nldbl-isnan.c = -fno-builtin-isnanl
CFLAGS-nldbl-j0.c = -fno-builtin-j0l
CFLAGS-nldbl-j1.c = -fno-builtin-j1l
CFLAGS-nldbl-jn.c = -fno-builtin-jnl
CFLAGS-nldbl-ldexp.c = -fno-builtin-ldexpl
CFLAGS-nldbl-lgamma.c = -fno-builtin-lgammal
CFLAGS-nldbl-lgamma_r.c = -fno-builtin-lgammal_r
CFLAGS-nldbl-llogb.c = -fno-builtin-llogbl
CFLAGS-nldbl-llrint.c = -fno-builtin-llrintl
CFLAGS-nldbl-llround.c = -fno-builtin-llroundl
CFLAGS-nldbl-log.c = -fno-builtin-logl
CFLAGS-nldbl-log10.c = -fno-builtin-log10l
CFLAGS-nldbl-log10p1.c = -fno-builtin-log10p1l
CFLAGS-nldbl-log1p.c = -fno-builtin-log1pl -fno-builtin-logp1l
CFLAGS-nldbl-log2.c = -fno-builtin-log2l
CFLAGS-nldbl-log2p1.c = -fno-builtin-log2p1l
CFLAGS-nldbl-logb.c = -fno-builtin-logbl
CFLAGS-nldbl-lrint.c = -fno-builtin-lrintl
CFLAGS-nldbl-lround.c = -fno-builtin-lroundl
CFLAGS-nldbl-modf.c = -fno-builtin-modfl
CFLAGS-nldbl-nan.c = -fno-builtin-nanl
CFLAGS-nldbl-nearbyint.c = -fno-builtin-nearbyintl
CFLAGS-nldbl-nextafter.c = -fno-builtin-nextafterl
CFLAGS-nldbl-nextdown.c = -fno-builtin-nextdownl
CFLAGS-nldbl-nexttoward.c = -fno-builtin-nexttoward -fno-builtin-nexttowardl
CFLAGS-nldbl-nexttowardf.c = -fno-builtin-nexttowardf
CFLAGS-nldbl-nextup.c = -fno-builtin-nextupl
CFLAGS-nldbl-pow.c = -fno-builtin-powl
CFLAGS-nldbl-pown.c = -fno-builtin-pownl
CFLAGS-nldbl-powr.c = -fno-builtin-powrl
CFLAGS-nldbl-remainder.c = -fno-builtin-remainderl -fno-builtin-dreml
CFLAGS-nldbl-remquo.c = -fno-builtin-remquol
CFLAGS-nldbl-rint.c = -fno-builtin-rintl
CFLAGS-nldbl-rootn.c = -fno-builtin-rootnl
CFLAGS-nldbl-round.c = -fno-builtin-roundl
CFLAGS-nldbl-roundeven.c = -fno-builtin-roundevenl
CFLAGS-nldbl-rsqrt.c = -fno-builtin-rsqrtl
CFLAGS-nldbl-scalb.c = -fno-builtin-scalbl
CFLAGS-nldbl-scalbln.c = -fno-builtin-scalblnl
CFLAGS-nldbl-scalbn.c = -fno-builtin-scalbnl
CFLAGS-nldbl-setpayload.c = -fno-builtin-setpayloadl
CFLAGS-nldbl-setpayloadsig.c = -fno-builtin-setpayloadsigl
CFLAGS-nldbl-significand.c = -fno-builtin-significandl
CFLAGS-nldbl-sin.c = -fno-builtin-sinl
CFLAGS-nldbl-sincos.c = -fno-builtin-sincosl
CFLAGS-nldbl-sinh.c = -fno-builtin-sinhl
CFLAGS-nldbl-sinpi.c = -fno-builtin-sinpil
CFLAGS-nldbl-sqrt.c = -fno-builtin-sqrtl
CFLAGS-nldbl-tan.c = -fno-builtin-tanl
CFLAGS-nldbl-tanh.c = -fno-builtin-tanhl
CFLAGS-nldbl-tanpi.c = -fno-builtin-tanpil
CFLAGS-nldbl-tgamma.c = -fno-builtin-tgammal
CFLAGS-nldbl-totalorder.c = -fno-builtin-totalorderl
CFLAGS-nldbl-totalordermag.c = -fno-builtin-totalordermagl
CFLAGS-nldbl-trunc.c = -fno-builtin-truncl
CFLAGS-nldbl-ufromfp.c = -fno-builtin-ufromfpl
CFLAGS-nldbl-ufromfpx.c = -fno-builtin-ufromfpxl
CFLAGS-nldbl-y0.c = -fno-builtin-y0l
CFLAGS-nldbl-y1.c = -fno-builtin-y1l
CFLAGS-nldbl-yn.c = -fno-builtin-ynl

tests += \
  test-narrow-macros-ldbl-64 \
  test-nldbl-redirect \
  test-redirection-ldbl-64 \
# tests
CFLAGS-test-narrow-macros-ldbl-64.c += -mlong-double-64
CFLAGS-test-nldbl-redirect.c += -mlong-double-64
CFLAGS-test-redirection-ldbl-64.c += -mlong-double-64

$(objpfx)test-nldbl-redirect: $(objpfx)libnldbl_nonshared.a

endif

# Tests for argp.h functions (reusing the relevant tests from argp/)
ifeq ($(subdir),argp)
tests-internal += tst-nldbl-argp

$(objpfx)tst-nldbl-argp.c: tst-ldbl-argp.c
	cp $< $@

CFLAGS-tst-nldbl-argp.c += -mlong-double-64
endif

# Tests for err.h and error.h functions (reusing the relevant tests from
# misc/).
ifeq ($(subdir), misc)
tests-internal += tst-nldbl-warn
tests-internal += tst-nldbl-error
tests-internal += tst-nldbl-errorfptr

$(objpfx)tst-nldbl-warn.c: tst-ldbl-warn.c
	cp $< $@

$(objpfx)tst-nldbl-error.c: tst-ldbl-error.c
	cp $< $@

$(objpfx)tst-nldbl-errorfptr.c: tst-ldbl-errorfptr.c
	cp $< $@
CFLAGS-tst-nldbl-warn.c += -mlong-double-64
CFLAGS-tst-nldbl-error.c += -mlong-double-64
CFLAGS-tst-nldbl-errorfptr.c += -mlong-double-64
endif

ifeq ($(subdir), stdio-common)
# We already have the long double conversion defined with our common rules
# for the IEEE binary128 format, so define these special rules to build
# and run extra executables for the IEEE binary64 format, reusing the same
# sources.
define tst-scanf-format-64-tests
test-srcs += \
  tst-scanf-format-$(1)-ldouble-64 \
  # test-srcs

$$(objpfx)tst-scanf-format-$(1)-ldouble-64.o: \
	tst-scanf-format-$(1)-ldouble.c $$(before-compile)
	$$(compile-command.c)
$$(objpfx)tst-scanf-format-$(1)-ldouble-64: $$(libm)
endef
$(foreach p,$(xscanf-funcs), \
  $(eval $(call tst-scanf-format-64-tests,$(p))))

# Use the IEEE binary64 format for long double scanf format tests.
$(foreach suf,$(all-object-suffixes), \
	  $(foreach p,$(xscanf-funcs), \
		    $(objpfx)tst-scanf-format-$(p)-ldouble-64$(suf))): \
  sysdep-CFLAGS += -mlong-double-64

ifeq ($(run-built-tests),yes)
define tst-scanf-format-64-rule
tests-special += \
  $$(objpfx)tst-scanf-format-$(1)-ldouble-64-$(2).out \
  # tests-special

$$(objpfx)tst-scanf-format-$(1)-ldouble-64-$(2).out: \
	../sysdeps/ieee754/ldbl-opt/tst-scanf-format-ldouble-$(2).input \
	$$(objpfx)tst-scanf-format-$(1)-ldouble-64
	$$(make-tst-scanf-format-out) > $$@ < $$<; \
	$$(evaluate-test)

ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
tests-special += \
  $$(objpfx)tst-scanf-format-$(1)-ldouble-64-$(2)-mem.out \
  # tests-special

generated += \
  $$(objpfx)tst-scanf-format-$(1)-ldouble-64-$(2)-mem.out \
  $$(objpfx)tst-scanf-format-$(1)-ldouble-64-$(2).mtrace \
  # generated
endif
endif
endef
$(foreach f,$(fmts-xscanf-real), \
  $(foreach p,$(xscanf-funcs), \
    $(eval $(call tst-scanf-format-64-rule,$(p),$(f)))))
endif # $(run-built-tests) == yes

tests += \
  tst-nldbl-scanf-binary-c11 \
  tst-nldbl-scanf-binary-c23 \
  tst-nldbl-scanf-binary-gnu11 \
  tst-nldbl-scanf-binary-gnu89 \
# tests

# Some versions of GCC supported for building glibc do not support -std=c23
# (added in GCC 14), or the older name -std=c2x (added in GCC 9), so
# the test for that version uses -std=c11 and then _ISOC23_SOURCE is defined in
# the test as needed.
CFLAGS-tst-nldbl-scanf-binary-c11.c += -mlong-double-64 -std=c11 \
				       -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-nldbl-scanf-binary-c23.c += -mlong-double-64 -std=c11 \
				       -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-nldbl-scanf-binary-gnu11.c += -mlong-double-64 -std=gnu11 \
					 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-nldbl-scanf-binary-gnu89.c += -mlong-double-64 -std=gnu89 \
					 -DOBJPFX=\"$(objpfx)\"

endif # $(subdir) == stdio-common

ifeq ($(subdir), wcsmbs)
tests += \
  tst-nldbl-wscanf-binary-c11 \
  tst-nldbl-wscanf-binary-c23 \
  tst-nldbl-wscanf-binary-gnu11 \
  tst-nldbl-wscanf-binary-gnu89 \
# tests

# Some versions of GCC supported for building glibc do not support -std=c23
# (added in GCC 14), or the older name -std=c2x (added in GCC 9), so
# the test for that version uses -std=c11 and then _ISOC23_SOURCE is defined in
# the test as needed.
CFLAGS-tst-nldbl-wscanf-binary-c11.c += -mlong-double-64 -std=c11 \
					-DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-nldbl-wscanf-binary-c23.c += -mlong-double-64 -std=c11 \
					-DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-nldbl-wscanf-binary-gnu11.c += -mlong-double-64 -std=gnu11 \
					  -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-nldbl-wscanf-binary-gnu89.c += -mlong-double-64 -std=gnu89 \
					  -DOBJPFX=\"$(objpfx)\"

endif

routines_no_fortify += \
  nldbl-asprintf \
  nldbl-dprintf \
  nldbl-fprintf \
  nldbl-fwprintf \
  nldbl-obstack_printf \
  nldbl-obstack_vprintf \
  nldbl-printf \
  nldbl-snprintf \
  nldbl-sprintf \
  nldbl-swprintf \
  nldbl-syslog \
  nldbl-vasprintf \
  nldbl-vdprintf \
  nldbl-vfprintf \
  nldbl-vfwprintf \
  nldbl-vprintf \
  nldbl-vsnprintf \
  nldbl-vsprintf \
  nldbl-vswprintf \
  nldbl-vsyslog \
  nldbl-vwprintf \
  nldbl-wprintf \
  # routines_no_fortify

# We can't rely on Makeconfig here for whatever reason
CFLAGS-.oS += $(call elide-fortify-source,.oS,$(routines_no_fortify))
CFLAGS-.oS += $(call elide-fortify-source,_chk.oS,$(routines_no_fortify))
