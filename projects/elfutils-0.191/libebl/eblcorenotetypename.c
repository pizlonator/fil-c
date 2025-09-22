/* Return note type name.
   Copyright (C) 2002, 2007, 2008, 2012, 2013 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <inttypes.h>
#include <stdio.h>
#include <libeblP.h>

const char *
ebl_core_note_type_name (Ebl *ebl, uint32_t type, char *buf, size_t len)
{
  const char *res = ebl->core_note_type_name (type, buf, len);

  if (res == NULL)
    {
      static const char *knowntypes[] =
	{
#define KNOWNSTYPE(name) [NT_##name] = #name
	  KNOWNSTYPE (PRSTATUS),
	  KNOWNSTYPE (FPREGSET),
	  KNOWNSTYPE (PRPSINFO),
	  KNOWNSTYPE (TASKSTRUCT),
	  KNOWNSTYPE (PLATFORM),
	  KNOWNSTYPE (AUXV),
	  KNOWNSTYPE (GWINDOWS),
	  KNOWNSTYPE (ASRS),
	  KNOWNSTYPE (PSTATUS),
	  KNOWNSTYPE (PSINFO),
	  KNOWNSTYPE (PRCRED),
	  KNOWNSTYPE (UTSNAME),
	  KNOWNSTYPE (LWPSTATUS),
	  KNOWNSTYPE (LWPSINFO),
	  KNOWNSTYPE (PRFPXREG)
#undef KNOWNSTYPE
	};

      /* Handle standard names.  */
      if (type < sizeof (knowntypes) / sizeof (knowntypes[0])
	  && knowntypes[type] != NULL)
	res = knowntypes[type];
      else
	switch (type)
	  {
#define KNOWNSTYPE(name) case NT_##name: res = #name; break
	    KNOWNSTYPE (PRXFPREG);
	    KNOWNSTYPE (PPC_VMX);
	    KNOWNSTYPE (PPC_SPE);
	    KNOWNSTYPE (PPC_VSX);
	    KNOWNSTYPE (PPC_TM_SPR);
	    KNOWNSTYPE (386_TLS);
	    KNOWNSTYPE (386_IOPERM);
	    KNOWNSTYPE (X86_XSTATE);
	    KNOWNSTYPE (S390_HIGH_GPRS);
	    KNOWNSTYPE (S390_TIMER);
	    KNOWNSTYPE (S390_TODCMP);
	    KNOWNSTYPE (S390_TODPREG);
	    KNOWNSTYPE (S390_CTRS);
	    KNOWNSTYPE (S390_PREFIX);
	    KNOWNSTYPE (S390_LAST_BREAK);
	    KNOWNSTYPE (S390_SYSTEM_CALL);
	    KNOWNSTYPE (ARM_VFP);
	    KNOWNSTYPE (ARM_TLS);
	    KNOWNSTYPE (ARM_HW_BREAK);
	    KNOWNSTYPE (ARM_HW_WATCH);
	    KNOWNSTYPE (ARM_SYSTEM_CALL);
	    KNOWNSTYPE (SIGINFO);
	    KNOWNSTYPE (FILE);
#undef KNOWNSTYPE

	  default:
	    snprintf (buf, len, "%s: %" PRIu32, _("<unknown>"), type);

	    res = buf;
	  }
    }

  return res;
}
