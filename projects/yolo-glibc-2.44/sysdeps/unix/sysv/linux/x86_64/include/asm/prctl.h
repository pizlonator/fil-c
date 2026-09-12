/* FIXME: CET arch_prctl bits should come from the kernel header files.
   This file should be removed if <asm/prctl.h> from the required kernel
   header files contains CET arch_prctl bits.  */

#include_next <asm/prctl.h>

#ifndef ARCH_SHSTK_ENABLE
/* Enable SHSTK features in unsigned long int features.  */
# define ARCH_SHSTK_ENABLE		0x5001
/* Disable SHSTK features in unsigned long int features.  */
# define ARCH_SHSTK_DISABLE		0x5002
/* Lock SHSTK features in unsigned long int features.  */
# define ARCH_SHSTK_LOCK		0x5003
/* Unlock SHSTK features in unsigned long int features.  */
# define ARCH_SHSTK_UNLOCK		0x5004
/* Return SHSTK features in unsigned long int features.  */
# define ARCH_SHSTK_STATUS		0x5005

/* ARCH_SHSTK_ features bits */
# define ARCH_SHSTK_SHSTK		0x1
# define ARCH_SHSTK_WRSS		0x2
#endif

#ifndef ARCH_GET_XCOMP_PERM
# define ARCH_GET_XCOMP_PERM		0x1022
# define ARCH_REQ_XCOMP_PERM		0x1023
#endif
