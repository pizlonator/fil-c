/* Include gmp-mparam.h first, such that definitions of _SHORT_LIMB
   and _LONG_LONG_LIMB in it can take effect into gmp.h.  */
#include <gmp-mparam.h>

#ifndef __GMP_H__

#include <stdlib/gmp.h>

#include <bits/floatn.h>

#ifndef _ISOMAC
/* Now define the internal interfaces.  */
extern mp_size_t __mpn_extract_double (mp_ptr res_ptr, mp_size_t size,
				       int *expt, int *is_neg,
				       double value) attribute_hidden;

extern mp_size_t __mpn_extract_long_double (mp_ptr res_ptr, mp_size_t size,
					    int *expt, int *is_neg,
					    long double value)
     attribute_hidden;

#if __HAVE_DISTINCT_FLOAT128
extern mp_size_t __mpn_extract_float128 (mp_ptr res_ptr, mp_size_t size,
					 int *expt, int *is_neg,
					 _Float128 value)
     attribute_hidden;
#endif

extern float __mpn_construct_float (mp_srcptr frac_ptr, int expt, int sign)
     attribute_hidden;

extern double __mpn_construct_double (mp_srcptr frac_ptr, int expt,
				      int negative) attribute_hidden;

extern long double __mpn_construct_long_double (mp_srcptr frac_ptr, int expt,
						int sign)
     attribute_hidden;

#if __HAVE_DISTINCT_FLOAT128
extern _Float128 __mpn_construct_float128 (mp_srcptr frac_ptr, int expt,
					   int sign) attribute_hidden;
#endif

#endif

#endif
