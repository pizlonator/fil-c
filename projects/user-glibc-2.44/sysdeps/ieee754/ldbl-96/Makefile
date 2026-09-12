# Makefile for sysdeps/ieee754/ldbl-96.
# Copyright (C) 2016-2026 Free Software Foundation, Inc.
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

ifeq ($(subdir),math)
tests += test-canonical-ldbl-96 test-totalorderl-ldbl-96 test-sinl-pseudo
ifeq ($(have-ssp),yes)
CFLAGS-test-sinl-pseudo.c += -fstack-protector-all
endif

CFLAGS-w_acosl.c += -fno-builtin-acosf64x
CFLAGS-w_acoshl.c += -fno-builtin-acoshf64x
CFLAGS-w_asinl.c += -fno-builtin-asinf64x
CFLAGS-s_asinhl.c += -fno-builtin-asinhf64x
CFLAGS-s_atanl.c += -fno-builtin-atanf64x
CFLAGS-w_atan2l.c += -fno-builtin-atan2f64x
CFLAGS-w_atanhl.c += -fno-builtin-atanhf64x
CFLAGS-s_cabsl.c += -fno-builtin-cabsf64x
CFLAGS-s_cacosl.c += -fno-builtin-cacosf64x
CFLAGS-s_cacoshl.c += -fno-builtin-cacoshf64x
CFLAGS-s_canonicalizel.c += -fno-builtin-canonicalizef64x
CFLAGS-s_cargl.c += -fno-builtin-cargf64x
CFLAGS-s_casinl.c += -fno-builtin-casinf64x
CFLAGS-s_casinhl.c += -fno-builtin-casinhf64x
CFLAGS-s_catanl.c += -fno-builtin-catanf64x
CFLAGS-s_catanhl.c += -fno-builtin-catanhf64x
CFLAGS-s_cbrtl.c += -fno-builtin-cbrtf64x
CFLAGS-s_ccosl.c += -fno-builtin-ccosf64x
CFLAGS-s_ccoshl.c += -fno-builtin-ccoshf64x
CFLAGS-s_ceill.c += -fno-builtin-ceilf64x
CFLAGS-s_cexpl.c += -fno-builtin-cexpf64x
CFLAGS-s_cimagl.c += -fno-builtin-cimagf64x
CFLAGS-s_clogl.c += -fno-builtin-clogf64x
CFLAGS-s_clog10l.c += -fno-builtin-clog10f64x
CFLAGS-s_conjl.c += -fno-builtin-conjf64x
CFLAGS-s_copysignl.c += -fno-builtin-copysignf64x
CFLAGS-s_cosl.c += -fno-builtin-cosf64x
CFLAGS-w_coshl.c += -fno-builtin-coshf64x
CFLAGS-s_cpowl.c += -fno-builtin-cpowf64x
CFLAGS-s_cprojl.c += -fno-builtin-cprojf64x
CFLAGS-s_creall.c += -fno-builtin-crealf64x
CFLAGS-s_csinl.c += -fno-builtin-csinf64x
CFLAGS-s_csinhl.c += -fno-builtin-csinhf64x
CFLAGS-s_csqrtl.c += -fno-builtin-csqrtf64x
CFLAGS-s_ctanl.c += -fno-builtin-ctanf64x
CFLAGS-s_ctanhl.c += -fno-builtin-ctanhf64x
CFLAGS-s_daddl.c += -fno-builtin-f64addf64x
CFLAGS-s_ddivl.c += -fno-builtin-f64divf64x
CFLAGS-s_dfmal.c += -fno-builtin-f64fmaf64x
CFLAGS-s_dmull.c += -fno-builtin-f64mulf64x
CFLAGS-s_dsqrtl.c += -fno-builtin-f64sqrtf64x
CFLAGS-s_dsubl.c += -fno-builtin-f64subf64x
CFLAGS-s_erfl.c += -fno-builtin-erff64x
CFLAGS-s_erfcl.c += -fno-builtin-erfcf64x
CFLAGS-e_expl.c += -fno-builtin-expf64x
CFLAGS-w_exp10l.c += -fno-builtin-exp10f64x
CFLAGS-e_exp2l.c += -fno-builtin-exp2f64x
CFLAGS-s_expm1l.c += -fno-builtin-expm1f64x
CFLAGS-s_fabsl.c += -fno-builtin-fabsf64x
CFLAGS-s_faddl.c += -fno-builtin-f32addf64x
CFLAGS-s_fdiml.c += -fno-builtin-fdimf64x
CFLAGS-s_fdivl.c += -fno-builtin-f32divf64x
CFLAGS-s_ffmal.c += -fno-builtin-f32fmaf64x
CFLAGS-s_floorl.c += -fno-builtin-floorf64x
CFLAGS-s_fmal.c += -fno-builtin-fmaf64x
CFLAGS-s_fmaxl.c += -fno-builtin-fmaxf64x
CFLAGS-s_fmaximuml.c += -fno-builtin-fmaximumf64x
CFLAGS-s_fmaximum_magl.c += -fno-builtin-fmaximum_magf64x
CFLAGS-s_fmaximum_mag_numl.c += -fno-builtin-fmaximum_mag_numf64x
CFLAGS-s_fmaximum_numl.c += -fno-builtin-fmaximum_numf64x
CFLAGS-s_fmaxmagl.c += -fno-builtin-fmaxmagf64x
CFLAGS-s_fminl.c += -fno-builtin-fminf64x
CFLAGS-s_fminimuml.c += -fno-builtin-fminimumf64x
CFLAGS-s_fminimum_magl.c += -fno-builtin-fminimum_magf64x
CFLAGS-s_fminimum_mag_numl.c += -fno-builtin-fminimum_mag_numf64x
CFLAGS-s_fminimum_numl.c += -fno-builtin-fminimum_numf64x
CFLAGS-s_fminmagl.c += -fno-builtin-fminmagf64x
CFLAGS-w_fmodl.c += -fno-builtin-fmodf64x
CFLAGS-s_fmull.c += -fno-builtin-f32mulf64x
CFLAGS-s_frexpl.c += -fno-builtin-frexpf64x
CFLAGS-s_compat_fromfpl.c += -fno-builtin-fromfpf64x
CFLAGS-s_compat_fromfpxl.c += -fno-builtin-fromfpxf64x
CFLAGS-s_fromfpl.c += -fno-builtin-fromfpf64x
CFLAGS-s_fromfpxl.c += -fno-builtin-fromfpxf64x
CFLAGS-s_fsqrtl.c += -fno-builtin-f32sqrtf64x
CFLAGS-s_fsubl.c += -fno-builtin-f32subf64x
CFLAGS-s_getpayloadl.c += -fno-builtin-getpayloadf64x
CFLAGS-w_hypotl.c += -fno-builtin-hypotf64x
CFLAGS-w_ilogbl.c += -fno-builtin-ilogbf64x
CFLAGS-w_j0l.c += -fno-builtin-j0f64x
CFLAGS-w_j1l.c += -fno-builtin-j1f64x
CFLAGS-w_jnl.c += -fno-builtin-jnf64x
CFLAGS-s_ldexpl.c += -fno-builtin-ldexpf64x
CFLAGS-w_lgammal.c += -fno-builtin-lgammaf64x
CFLAGS-w_lgammal_r.c += -fno-builtin-lgammaf64x_r
CFLAGS-w_llogbl.c += -fno-builtin-llogbf64x
CFLAGS-s_llrintl.c += -fno-builtin-llrintf64x
CFLAGS-s_llroundl.c += -fno-builtin-llroundf64x
CFLAGS-e_logl.c += -fno-builtin-logf64x
CFLAGS-w_log10l.c += -fno-builtin-log10f64x
CFLAGS-w_log1pl.c += -fno-builtin-log1pf64x -fno-builtin-logp1f64x
CFLAGS-e_log2l.c += -fno-builtin-log2f64x
CFLAGS-s_logbl.c += -fno-builtin-logbf64x
CFLAGS-s_lrintl.c += -fno-builtin-lrintf64x
CFLAGS-s_lroundl.c += -fno-builtin-lroundf64x
CFLAGS-s_modfl.c += -fno-builtin-modff64x
CFLAGS-s_nanl.c += -fno-builtin-nanf64x
CFLAGS-s_nearbyintl.c += -fno-builtin-nearbyintf64x
CFLAGS-s_nextafterl.c += -fno-builtin-nextafterf64x
CFLAGS-s_nextdownl.c += -fno-builtin-nextdownf64x
CFLAGS-s_nextupl.c += -fno-builtin-nextupf64x
CFLAGS-e_powl.c += -fno-builtin-powf64x
CFLAGS-w_remainderl.c += -fno-builtin-remainderf64x
CFLAGS-s_remquol.c += -fno-builtin-remquof64x
CFLAGS-s_rintl.c += -fno-builtin-rintf64x
CFLAGS-s_roundl.c += -fno-builtin-roundf64x
CFLAGS-s_roundevenl.c += -fno-builtin-roundevenf64x
CFLAGS-w_scalblnl.c += -fno-builtin-scalblnf64x
CFLAGS-s_scalbnl.c += -fno-builtin-scalbnf64x
CFLAGS-s_setpayloadl.c += -fno-builtin-setpayloadf64x
CFLAGS-s_setpayloadsigl.c += -fno-builtin-setpayloadsigf64x
CFLAGS-s_sinl.c += -fno-builtin-sinf64x
CFLAGS-s_sincosl.c += -fno-builtin-sincosf64x
CFLAGS-w_sinhl.c += -fno-builtin-sinhf64x
CFLAGS-w_sqrtl.c += -fno-builtin-sqrtf64x
CFLAGS-s_tanl.c += -fno-builtin-tanf64x
CFLAGS-s_tanhl.c += -fno-builtin-tanhf64x
CFLAGS-w_tgammal.c += -fno-builtin-tgammaf64x
CFLAGS-s_totalorderl.c += -fno-builtin-totalorderf64x
CFLAGS-s_totalordermagl.c += -fno-builtin-totalordermagf64x
CFLAGS-s_truncl.c += -fno-builtin-truncf64x
CFLAGS-s_compat_ufromfpl.c += -fno-builtin-ufromfpf64x
CFLAGS-s_compat_ufromfpxl.c += -fno-builtin-ufromfpxf64x
CFLAGS-s_ufromfpl.c += -fno-builtin-ufromfpf64x
CFLAGS-s_ufromfpxl.c += -fno-builtin-ufromfpxf64x
CFLAGS-s_y0l.c += -fno-builtin-y0f64x
CFLAGS-s_y1l.c += -fno-builtin-y1f64x
CFLAGS-s_ynl.c += -fno-builtin-ynf64x

endif # $(subdir) == math

ifeq ($(subdir),stdio-common)
fmt-xscanf-real-convs += ldouble
endif
