# Makefile fragment for PowerPC with no FPU.

ifeq ($(subdir),soft-fp)
sysdep_routines += $(gcc-single-routines) $(gcc-double-routines) \
		   sqrtsf2 sqrtdf2 \
		   sim-full atomic-feholdexcept atomic-feclearexcept \
		   atomic-feupdateenv flt-rounds
endif

ifeq ($(subdir),math)
libm-support += fenv_const
CPPFLAGS += -I../soft-fp/
# The follow CFLAGS are a work around for GCC Bugzilla Bug 29253
# "expand_abs wrong default code for floating point"
# As this is not a regression, a fix is not likely to go into
# gcc-4.1.1 and may be too late for gcc-4.2.  So we need these flags
# until the fix in a gcc release and glibc drops support for earlier
# versions of gcc.
#
# This bug can cause wrong signs of zero results from fabsl, and
# spurious "invalid" exceptions from comparisons with NaN in the
# default code sequence generated.  The fpclassify expansion used by
# __builtin_fpclassify, which is used in the absence of
# -fsignaling-nans, uses __builtin_fabsl, and is not affected by
# -fno-builtin-fabsl, so requiring -fsignaling-nans for some affected
# source files.
CFLAGS-e_atan2l.c += -fno-builtin-fabsl
CFLAGS-e_hypotl.c += -fno-builtin-fabsl
CFLAGS-e_powl.c += -fno-builtin-fabsl
CFLAGS-s_cacosl.c += $(config-cflags-signaling-nans)
CFLAGS-s_cacoshl.c += $(config-cflags-signaling-nans)
CFLAGS-s_casinhl.c += $(config-cflags-signaling-nans)
CFLAGS-s_catanl.c += $(config-cflags-signaling-nans)
CFLAGS-s_catanhl.c += $(config-cflags-signaling-nans)
CFLAGS-s_ccoshl.c += -fno-builtin-fabsl $(config-cflags-signaling-nans)
CFLAGS-s_cexpl.c += $(config-cflags-signaling-nans)
CFLAGS-s_csinhl.c += -fno-builtin-fabsl $(config-cflags-signaling-nans)
CFLAGS-s_clogl.c += -fno-builtin-fabsl $(config-cflags-signaling-nans)
CFLAGS-s_clog10l.c += -fno-builtin-fabsl $(config-cflags-signaling-nans)
CFLAGS-s_cospil.c += -fno-builtin-fabsl
CFLAGS-s_csinl.c += -fno-builtin-fabsl $(config-cflags-signaling-nans)
CFLAGS-s_csqrtl.c += -fno-builtin-fabsl $(config-cflags-signaling-nans)
CFLAGS-w_acosl_compat.c += -fno-builtin-fabsl
CFLAGS-s_acospil.c += -fno-builtin-fabsl
CFLAGS-w_asinl_compat.c += -fno-builtin-fabsl
CFLAGS-s_asinpil.c += -fno-builtin-fabsl
CFLAGS-s_atan2pil.c += -fno-builtin-fabsl
CFLAGS-w_atanhl_compat.c += -fno-builtin-fabsl
CFLAGS-s_atanpil.c += -fno-builtin-fabsl
CFLAGS-w_j0l_compat.c += -fno-builtin-fabsl
CFLAGS-w_j1l_compat.c += -fno-builtin-fabsl
CFLAGS-e_lgammal_r.c += -fno-builtin-fabsl
CFLAGS-e_log10l.c += -fno-builtin-fabsl
CFLAGS-e_log2l.c += -fno-builtin-fabsl
CFLAGS-e_remainderl.c += -fno-builtin-fabsl
CFLAGS-s_fmaxmagl.c += -fno-builtin-fabsl
CFLAGS-s_fminmagl.c += -fno-builtin-fabsl
CFLAGS-s_fmaximum_magl.c += -fno-builtin-fabsl
CFLAGS-s_fmaximum_mag_numl.c += -fno-builtin-fabsl
CFLAGS-s_fminimum_magl.c += -fno-builtin-fabsl
CFLAGS-s_fminimum_mag_numl.c += -fno-builtin-fabsl
CFLAGS-s_log10p1l.c += -fno-builtin-fabsl
CFLAGS-s_log2p1l.c += -fno-builtin-fabsl
CFLAGS-s_sinpil.c += -fno-builtin-fabsl
CFLAGS-s_tanpil.c += -fno-builtin-fabsl
endif
