/* x86-64 CET initializers function.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.

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

#include <unistd.h>
#include <errno.h>
#include <libintl.h>
#include <ldsodefs.h>
#include <dl-cet.h>
#include <sys/single_threaded.h>

/* GNU_PROPERTY_X86_FEATURE_1_IBT and GNU_PROPERTY_X86_FEATURE_1_SHSTK
   are defined in <elf.h>, which are only available for C sources.
   X86_FEATURE_1_IBT and X86_FEATURE_1_SHSTK are defined in <sysdep.h>
   which are available for both C and asm sources.  They must match.   */
#if GNU_PROPERTY_X86_FEATURE_1_IBT != X86_FEATURE_1_IBT
# error GNU_PROPERTY_X86_FEATURE_1_IBT != X86_FEATURE_1_IBT
#endif
#if GNU_PROPERTY_X86_FEATURE_1_SHSTK != X86_FEATURE_1_SHSTK
# error GNU_PROPERTY_X86_FEATURE_1_SHSTK != X86_FEATURE_1_SHSTK
#endif

struct dl_cet_info
{
  const char *program;

  /* Check how IBT and SHSTK should be enabled.  */
  enum dl_x86_cet_control enable_ibt_type;
  enum dl_x86_cet_control enable_shstk_type;

  /* If IBT and SHSTK were previously enabled.  */
  unsigned int feature_1_enabled;

  /* If IBT and SHSTK should be enabled.  */
  unsigned int enable_feature_1;

  /* If there are any legacy shared object.  */
  unsigned int feature_1_legacy;

  /* Which shared object is the first legacy shared object.  */
  unsigned int feature_1_legacy_ibt;
  unsigned int feature_1_legacy_shstk;
};

/* Check if the object M and its dependencies are legacy object.  */

static void
dl_check_legacy_object (struct link_map *m,
			struct dl_cet_info *info)
{
  unsigned int i;
  struct link_map *l = NULL;

  i = m->l_searchlist.r_nlist;
  while (i-- > 0)
    {
      /* Check each shared object to see if IBT and SHSTK are enabled.  */
      l = m->l_initfini[i];

      if (l->l_init_called)
        continue;

#ifdef SHARED
      /* Skip check for ld.so since it has the features enabled.  The
         features will be disabled later if they are not enabled in
	 executable.  */
      if (l == &GL(dl_rtld_map)
          || l->l_real == &GL(dl_rtld_map)
          || (info->program != NULL && l == m))
         continue;
#endif

      /* IBT and SHSTK set only if enabled in executable and all DSOs.
	 NB: cet_always_on is handled outside of the loop.  */
      info->enable_feature_1 &= ((l->l_x86_feature_1_and
				  & (GNU_PROPERTY_X86_FEATURE_1_IBT
				     | GNU_PROPERTY_X86_FEATURE_1_SHSTK))
				 | ~(GNU_PROPERTY_X86_FEATURE_1_IBT
				     | GNU_PROPERTY_X86_FEATURE_1_SHSTK));
      if ((info->feature_1_legacy
	   & GNU_PROPERTY_X86_FEATURE_1_IBT) == 0
	  && ((info->enable_feature_1
	       & GNU_PROPERTY_X86_FEATURE_1_IBT)
	      != (info->feature_1_enabled
		  & GNU_PROPERTY_X86_FEATURE_1_IBT)))
	{
	  info->feature_1_legacy_ibt = i;
	  info->feature_1_legacy |= GNU_PROPERTY_X86_FEATURE_1_IBT;
	}

      if ((info->feature_1_legacy
	   & GNU_PROPERTY_X86_FEATURE_1_SHSTK) == 0
	  && ((info->enable_feature_1
	       & GNU_PROPERTY_X86_FEATURE_1_SHSTK)
	      != (info->feature_1_enabled
		  & GNU_PROPERTY_X86_FEATURE_1_SHSTK)))
        {
	  info->feature_1_legacy_shstk = i;
	  info->feature_1_legacy |= GNU_PROPERTY_X86_FEATURE_1_SHSTK;
        }
    }

  /* Handle cet_always_on.  */
  if ((info->feature_1_enabled
       & GNU_PROPERTY_X86_FEATURE_1_IBT) != 0
      && info->enable_ibt_type == cet_always_on)
    {
      info->feature_1_legacy &= ~GNU_PROPERTY_X86_FEATURE_1_IBT;
      info->enable_feature_1 |= GNU_PROPERTY_X86_FEATURE_1_IBT;
    }

  if ((info->feature_1_enabled
       & GNU_PROPERTY_X86_FEATURE_1_SHSTK) != 0
      && info->enable_shstk_type == cet_always_on)
    {
      info->feature_1_legacy &= ~GNU_PROPERTY_X86_FEATURE_1_SHSTK;
      info->enable_feature_1 |= GNU_PROPERTY_X86_FEATURE_1_SHSTK;
    }
}

#ifdef SHARED
/* Enable IBT and SHSTK only if they are enabled in executable.  Set
   feature bits properly at the start of the program.  */

static void
dl_cet_check_startup (struct link_map *m, struct dl_cet_info *info)
{
  /* NB: IBT and SHSTK may be disabled by environment variable:

     GLIBC_TUNABLES=glibc.cpu.hwcaps=-IBT,-SHSTK.
   */
  if (CPU_FEATURE_USABLE (IBT))
    {
      if (info->enable_ibt_type == cet_always_on)
	info->enable_feature_1 |= GNU_PROPERTY_X86_FEATURE_1_IBT;
      else
	info->enable_feature_1 &= ((m->l_x86_feature_1_and
				    & GNU_PROPERTY_X86_FEATURE_1_IBT)
				   | ~GNU_PROPERTY_X86_FEATURE_1_IBT);
    }
  else
    info->enable_feature_1 &= ~GNU_PROPERTY_X86_FEATURE_1_IBT;

  if (CPU_FEATURE_USABLE (SHSTK))
    {
      if (info->enable_shstk_type == cet_always_on)
	info->enable_feature_1 |= GNU_PROPERTY_X86_FEATURE_1_SHSTK;
      else
	info->enable_feature_1 &= ((m->l_x86_feature_1_and
				    & GNU_PROPERTY_X86_FEATURE_1_SHSTK)
				   | ~GNU_PROPERTY_X86_FEATURE_1_SHSTK);
    }
  else
    info->enable_feature_1 &= ~GNU_PROPERTY_X86_FEATURE_1_SHSTK;

  if (info->enable_feature_1 != 0)
    dl_check_legacy_object (m, info);

  unsigned int disable_feature_1
    = info->enable_feature_1 ^ info->feature_1_enabled;
  if (disable_feature_1 != 0)
    {
      /* Clear the disabled bits.  Sync dl_x86_feature_1 and
         info->feature_1_enabled with info->enable_feature_1.  */
      info->feature_1_enabled = info->enable_feature_1;
      GL(dl_x86_feature_1) = info->enable_feature_1;
    }
}
#endif

/* Check feature bits when dlopening the shared object M.  */

static void
dl_cet_check_dlopen (struct link_map *m, struct dl_cet_info *info)
{
  /* Check if there are any legacy objects loaded.  */
  if (info->enable_feature_1 != 0)
    {
      dl_check_legacy_object (m, info);

      /* Skip if there are no legacy shared objects loaded.  */
      if (info->feature_1_legacy == 0)
	return;
    }

  unsigned int disable_feature_1 = 0;
  unsigned int legacy_obj = 0;
  const char *msg = NULL;

  if ((info->feature_1_enabled
       & GNU_PROPERTY_X86_FEATURE_1_IBT) != 0
      && (info->feature_1_legacy
	  & GNU_PROPERTY_X86_FEATURE_1_IBT) != 0)
    {
      /* Don't disable IBT if not single threaded since IBT may be still
	 enabled in other threads.  */
      if (info->enable_ibt_type != cet_permissive
	  || !SINGLE_THREAD_P)
	{
	  legacy_obj = info->feature_1_legacy_ibt;
	  msg = N_("rebuild shared object with IBT support enabled");
	}
      else
        disable_feature_1 |= GNU_PROPERTY_X86_FEATURE_1_IBT;
    }

  /* Check the next feature only if there is no error.  */
  if (msg == NULL
      && (info->feature_1_enabled
	  & GNU_PROPERTY_X86_FEATURE_1_SHSTK) != 0
      && (info->feature_1_legacy
	  & GNU_PROPERTY_X86_FEATURE_1_SHSTK) != 0)
    {
      /* Don't disable SHSTK if not single threaded since SHSTK may be
         still enabled in other threads.  */
      if (info->enable_shstk_type != cet_permissive
	  || !SINGLE_THREAD_P)
	{
	  legacy_obj = info->feature_1_legacy_shstk;
	  msg = N_("rebuild shared object with SHSTK support enabled");
	}
      else
        disable_feature_1 |= GNU_PROPERTY_X86_FEATURE_1_SHSTK;
    }

  /* If there is an error, long jump back to the caller.  */
  if (msg != NULL)
    _dl_signal_error (0, m->l_initfini[legacy_obj]->l_name, "dlopen",
		      msg);

  if (disable_feature_1 != 0)
    {
      int res = dl_cet_disable_cet (disable_feature_1);
      if (res)
        {
	  if ((disable_feature_1
	       & GNU_PROPERTY_X86_FEATURE_1_IBT) != 0)
	    msg = N_("can't disable IBT");
	  else
	    msg = N_("can't disable SHSTK");
	  /* Long jump back to the caller on error.  */
	  _dl_signal_error (-res, m->l_initfini[legacy_obj]->l_name,
			    "dlopen", msg);
       }

      /* Clear the disabled bits in dl_x86_feature_1.  */
      GL(dl_x86_feature_1) &= ~disable_feature_1;

      THREAD_SETMEM (THREAD_SELF, header.feature_1,
		     GL(dl_x86_feature_1));
    }
}

static void
dl_cet_check (struct link_map *m, const char *program)
{
  struct dl_cet_info info;

  /* CET is enabled only if RTLD_START_ENABLE_X86_FEATURES is defined.  */
#if defined SHARED && defined RTLD_START_ENABLE_X86_FEATURES
  /* Set dl_x86_feature_1 to features enabled in the executable.  */
  if (program != NULL)
    GL(dl_x86_feature_1) = (m->l_x86_feature_1_and
			    & (X86_FEATURE_1_IBT
			       | X86_FEATURE_1_SHSTK));
#endif

  /* Check how IBT and SHSTK should be enabled. */
  info.enable_ibt_type = GL(dl_x86_feature_control).ibt;
  info.enable_shstk_type = GL(dl_x86_feature_control).shstk;

  info.feature_1_enabled = GL(dl_x86_feature_1);

  /* No legacy object check if IBT and SHSTK are always on.  */
  if (info.enable_ibt_type == cet_always_on
      && info.enable_shstk_type == cet_always_on)
    return;

  /* Check if IBT and SHSTK were enabled.  */
  if (info.feature_1_enabled == 0)
    return;

  info.program = program;

  /* Check which features should be enabled.  */
  info.enable_feature_1 = 0;
  if (info.enable_ibt_type != cet_always_off)
    info.enable_feature_1 |= (info.feature_1_enabled
			      & GNU_PROPERTY_X86_FEATURE_1_IBT);
  if (info.enable_shstk_type != cet_always_off)
    info.enable_feature_1 |= (info.feature_1_enabled
			      & GNU_PROPERTY_X86_FEATURE_1_SHSTK);

  /* Start with no legacy objects.  */
  info.feature_1_legacy = 0;
  info.feature_1_legacy_ibt = 0;
  info.feature_1_legacy_shstk = 0;

#ifdef SHARED
  if (program)
    dl_cet_check_startup (m, &info);
  else
#endif
    dl_cet_check_dlopen (m, &info);
}

void
_dl_cet_open_check (struct link_map *l)
{
  dl_cet_check (l, NULL);
}

/* Set GL(dl_x86_feature_1) to the enabled features and clear the
   active bits of the disabled features.  */

attribute_hidden void
_dl_cet_setup_features (unsigned int cet_feature)
{
  /* NB: cet_feature == GL(dl_x86_feature_1) which is set to features
     enabled from executable, not necessarily supported by kernel.  */
  if (cet_feature != 0)
    {
      cet_feature = dl_cet_get_cet_status ();
      if (cet_feature != 0)
	{
	  THREAD_SETMEM (THREAD_SELF, header.feature_1, cet_feature);

	  /* Lock CET if IBT or SHSTK is enabled in executable.  Don't
	     lock CET if IBT or SHSTK is enabled permissively.  */
	  if (GL(dl_x86_feature_control).ibt != cet_permissive
	      && (GL(dl_x86_feature_control).shstk != cet_permissive))
	    dl_cet_lock_cet (cet_feature);
	}
      /* Sync GL(dl_x86_feature_1) with kernel.  */
      GL(dl_x86_feature_1) = cet_feature;
    }
}

#ifdef SHARED

# ifndef LINKAGE
#  define LINKAGE
# endif

LINKAGE
void
_dl_cet_check (struct link_map *main_map, const char *program)
{
  dl_cet_check (main_map, program);
}
#endif /* SHARED */
