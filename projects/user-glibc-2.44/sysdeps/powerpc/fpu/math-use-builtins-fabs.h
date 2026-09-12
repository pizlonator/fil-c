/* Generic implementations already assume builtin support for
   float, double, and _Float128 .  */

/* We can't enable it for long double due GCC Bugzilla Bug 29253
   "expand_abs wrong default code for floating point" where it can
   cause spurious "invalid" exceptions from comparisons with NaN in
   the code sequence generated.  */
#define USE_FABSL_BUILTIN 0
