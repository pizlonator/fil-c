/* Linux/x86-64 CET initializers function.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sys/prctl.h>
#include <asm/prctl.h>
#include <features-offsets.h>

static __always_inline int
dl_cet_disable_cet (unsigned int cet_feature)
{
  if (cet_feature != GNU_PROPERTY_X86_FEATURE_1_SHSTK)
    return -1;
  long long int kernel_feature = ARCH_SHSTK_SHSTK;
  return (int) INTERNAL_SYSCALL_CALL (arch_prctl, ARCH_SHSTK_DISABLE,
				      kernel_feature);
}

static __always_inline int
dl_cet_lock_cet (unsigned int cet_feature)
{
  if (cet_feature != GNU_PROPERTY_X86_FEATURE_1_SHSTK)
    return -1;
  /* Lock all SHSTK features.  */
  long long int kernel_feature = -1;
  return (int) INTERNAL_SYSCALL_CALL (arch_prctl, ARCH_SHSTK_LOCK,
				      kernel_feature);
}

static __always_inline unsigned int
dl_cet_get_cet_status (void)
{
  unsigned long long kernel_feature;
  unsigned int status = 0;
  if (INTERNAL_SYSCALL_CALL (arch_prctl, ARCH_SHSTK_STATUS,
			     &kernel_feature) == 0)
    {
      if ((kernel_feature & ARCH_SHSTK_SHSTK) != 0)
	status = GNU_PROPERTY_X86_FEATURE_1_SHSTK;
    }
  return status;
}

static __always_inline bool
dl_cet_ibt_enabled (void)
{
  unsigned int feature_1 = THREAD_GETMEM (THREAD_SELF,
					  header.feature_1);
  return (feature_1 & GNU_PROPERTY_X86_FEATURE_1_IBT) != 0;
}

/* Enable shadow stack with a macro to avoid shadow stack underflow.  */
#define ENABLE_X86_CET(cet_feature)				\
  if ((cet_feature & GNU_PROPERTY_X86_FEATURE_1_SHSTK))		\
    {								\
      long long int kernel_feature = ARCH_SHSTK_SHSTK;		\
      INTERNAL_SYSCALL_CALL (arch_prctl, ARCH_SHSTK_ENABLE,	\
			     kernel_feature);			\
    }

#define X86_STRINGIFY_1(x)	#x
#define X86_STRINGIFY(x)	X86_STRINGIFY_1 (x)

/* Enable shadow stack before calling _dl_init if it is enabled in
   GL(dl_x86_feature_1).  Call _dl_setup_x86_features to setup shadow
   stack.  */
#define RTLD_START_ENABLE_X86_FEATURES \
"\
	# Check if shadow stack is enabled in GL(dl_x86_feature_1).\n\
	movl _rtld_local+" X86_STRINGIFY (RTLD_GLOBAL_DL_X86_FEATURE_1_OFFSET) "(%rip), %edx\n\
	testl $" X86_STRINGIFY (X86_FEATURE_1_SHSTK) ", %edx\n\
	jz 1f\n\
	# Enable shadow stack if enabled in GL(dl_x86_feature_1).\n\
	movl $" X86_STRINGIFY (ARCH_SHSTK_SHSTK) ", %esi\n\
	movl $" X86_STRINGIFY (ARCH_SHSTK_ENABLE) ", %edi\n\
	movl $" X86_STRINGIFY (__NR_arch_prctl) ", %eax\n\
	syscall\n\
1:\n\
	# Pass GL(dl_x86_feature_1) to _dl_cet_setup_features.\n\
	movl %edx, %edi\n\
	# Align stack for the _dl_cet_setup_features call.\n\
	and $-16, %" RSP_LP "\n\
	call _dl_cet_setup_features\n\
	# Restore %rax and %rsp from %r12 and %r13.\n\
	mov %" R12_LP ", %" RAX_LP "\n\
	mov %" R13_LP ", %" RSP_LP "\n\
"
