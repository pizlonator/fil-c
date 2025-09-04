/* x86-64 definitions for libc main startup.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

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

#ifndef SHARED
# define ARCH_SETUP_IREL() apply_irel ()
# define ARCH_APPLY_IREL()
# ifdef __CET__
/* Get CET features enabled in the static executable.  */

static inline unsigned int
get_cet_feature (void)
{
  /* Check if CET is supported and not disabled by tunables.  */
  const struct cpu_features *cpu_features = __get_cpu_features ();
  unsigned int cet_feature = 0;
  if (CPU_FEATURE_USABLE_P (cpu_features, IBT))
    cet_feature |= GNU_PROPERTY_X86_FEATURE_1_IBT;
  if (CPU_FEATURE_USABLE_P (cpu_features, SHSTK))
    cet_feature |= GNU_PROPERTY_X86_FEATURE_1_SHSTK;
  if (!cet_feature)
    return cet_feature;

  struct link_map *main_map = _dl_get_dl_main_map ();

  /* Scan program headers backward to check PT_GNU_PROPERTY early for
     x86 feature bits on static executable.  */
  const ElfW(Phdr) *phdr = GL(dl_phdr);
  const ElfW(Phdr) *ph;
  for (ph = phdr + GL(dl_phnum); ph != phdr; ph--)
    if (ph[-1].p_type == PT_GNU_PROPERTY)
      {
	_dl_process_pt_gnu_property (main_map, -1, &ph[-1]);
	/* Enable IBT and SHSTK only if they are enabled on static
	   executable.  */
	cet_feature &= (main_map->l_x86_feature_1_and
			& (GNU_PROPERTY_X86_FEATURE_1_IBT
			   | GNU_PROPERTY_X86_FEATURE_1_SHSTK));
	/* Set GL(dl_x86_feature_1) to the enabled CET features.  */
	GL(dl_x86_feature_1) = cet_feature;
	break;
      }

  return cet_feature;
}

/* The function using this macro to enable shadow stack must not return
   to avoid shadow stack underflow.  */
#  define ARCH_SETUP_TLS()						\
  {									\
    __libc_setup_tls ();						\
									\
    unsigned int cet_feature = get_cet_feature ();			\
    ENABLE_X86_CET (cet_feature);					\
    _dl_cet_setup_features (cet_feature);				\
  }
# else
#  define ARCH_SETUP_TLS()	__libc_setup_tls ()
# endif
#endif /* !SHARED */
