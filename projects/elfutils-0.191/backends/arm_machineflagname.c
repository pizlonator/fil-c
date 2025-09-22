/* Arm-specific ELF flag names.
   Copyright (C) 2022 Red Hat, Inc.
   This file is part of elfutils.

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

#define BACKEND arm_
#include "libebl_CPU.h"

const char *
arm_machine_flag_name (Elf64_Word orig, Elf64_Word *flagref)
{
  unsigned version = EF_ARM_EABI_VERSION (*flagref) >> 24;
  if (version != 0)
    {
      static const char vername[5][14] =
        {
	  "Version1 EABI",
	  "Version2 EABI",
	  "Version3 EABI",
	  "Version4 EABI",
	  "Version5 EABI",
        };
      *flagref &= ~((Elf64_Word) EF_ARM_EABIMASK);
      return version <= 5 ? vername[version - 1] : NULL;
    }
  switch (EF_ARM_EABI_VERSION (orig))
    {
    case EF_ARM_EABI_VER2:
      if ((*flagref & EF_ARM_DYNSYMSUSESEGIDX) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_DYNSYMSUSESEGIDX);
	  return "dynamic symbols use segment index";
	}
      if ((*flagref & EF_ARM_MAPSYMSFIRST) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_MAPSYMSFIRST);
	  return "mapping symbols precede others";
	}
      FALLTHROUGH;
    case EF_ARM_EABI_VER1:
      if ((*flagref & EF_ARM_SYMSARESORTED) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_SYMSARESORTED);
	  return "sorted symbol tables";
	}
      break;
    case EF_ARM_EABI_VER3:
      break;
    case EF_ARM_EABI_VER5:
      if ((*flagref & EF_ARM_SOFT_FLOAT) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_SOFT_FLOAT);
	  return "soft-float ABI";
	}
      if ((*flagref & EF_ARM_VFP_FLOAT) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_VFP_FLOAT);
	  return "hard-float ABI";
	}
      FALLTHROUGH;
    case EF_ARM_EABI_VER4:
      if ((*flagref & EF_ARM_BE8) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_BE8);
	  return "BE8";
	}
      if ((*flagref & EF_ARM_LE8) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_LE8);
	  return "LE8";
	}
      break;
    case EF_ARM_EABI_UNKNOWN:
      if ((*flagref & EF_ARM_INTERWORK) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_INTERWORK);
	  return "interworking enabled";
	}
      if ((*flagref & EF_ARM_APCS_26) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_APCS_26);
	  return "uses APCS/26";
	}
      if ((*flagref & EF_ARM_APCS_FLOAT) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_APCS_FLOAT);
	  return "uses APCS/float";
	}
      if ((*flagref & EF_ARM_PIC) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_PIC);
	  return "position independent";
	}
      if ((*flagref & EF_ARM_ALIGN8) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_ALIGN8);
	  return "8 bit structure alignment";
	}
      if ((*flagref & EF_ARM_NEW_ABI) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_NEW_ABI);
	  return "uses new ABI";
	}
      if ((*flagref & EF_ARM_OLD_ABI) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_OLD_ABI);
	  return "uses old ABI";
	}
      if ((*flagref & EF_ARM_SOFT_FLOAT) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_SOFT_FLOAT);
	  return "software FP";
	}
      if ((*flagref & EF_ARM_VFP_FLOAT) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_VFP_FLOAT);
	  return "VFP";
	}
      if ((*flagref & EF_ARM_MAVERICK_FLOAT) != 0)
	{
	  *flagref &= ~((Elf64_Word) EF_ARM_MAVERICK_FLOAT);
	  return "Maverick FP";
	}
      break;
    default:
      break;
    }
  return NULL;
}
