/* Object attribute tags for ARM.
   Copyright (C) 2009 Red Hat, Inc.
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

#include <string.h>
#include <dwarf.h>

#define BACKEND arm_
#include "libebl_CPU.h"

#define KNOWN_VALUES(...) do				\
  {							\
    static const char *table[] = { __VA_ARGS__ };	\
    if (value < sizeof table / sizeof table[0])		\
      *value_name = table[value];			\
  } while (0)

bool
arm_check_object_attribute (Ebl *ebl __attribute__ ((unused)),
			    const char *vendor, int tag, uint64_t value,
			    const char **tag_name, const char **value_name)
{
  if (!strcmp (vendor, "aeabi"))
    switch (tag)
      {
      case 4:
	*tag_name = "CPU_raw_name";
	return true;
      case 5:
	*tag_name = "CPU_name";
	return true;
      case 6:
	*tag_name = "CPU_arch";
	KNOWN_VALUES ("Pre-v4",
		      "v4",
		      "v4T",
		      "v5T",
		      "v5TE",
		      "v5TEJ",
		      "v6",
		      "v6KZ",
		      "v6T2",
		      "v6K",
		      "v7",
		      "v6-M",
		      "v6S-M");
	return true;
      case 7:
	*tag_name = "CPU_arch_profile";
	switch (value)
	  {
	  case 'A':
	    *value_name = "Application";
	    break;
	  case 'R':
	    *value_name = "Realtime";
	    break;
	  case 'M':
	    *value_name = "Microcontroller";
	    break;
	  }
	return true;
      case 8:
	*tag_name = "ARM_ISA_use";
	KNOWN_VALUES ("No", "Yes");
	return true;
      case 9:
	*tag_name = "THUMB_ISA_use";
	KNOWN_VALUES ("No", "Thumb-1", "Thumb-2");
	return true;
      case 10:
	*tag_name = "VFP_arch";
	KNOWN_VALUES ("No", "VFPv1", "VFPv2", "VFPv3", "VFPv3-D16");
	return true;
      case 11:
	*tag_name = "WMMX_arch";
	KNOWN_VALUES ("No", "WMMXv1", "WMMXv2");
	return true;
      case 12:
	*tag_name = "Advanced_SIMD_arch";
	KNOWN_VALUES ("No", "NEONv1");
	return true;
      case 13:
	*tag_name = "PCS_config";
	KNOWN_VALUES ("None",
		      "Bare platform",
		      "Linux application",
		      "Linux DSO",
		      "PalmOS 2004",
		      "PalmOS (reserved)",
		      "SymbianOS 2004",
		      "SymbianOS (reserved)");
	return true;
      case 14:
	*tag_name = "ABI_PCS_R9_use";
	KNOWN_VALUES ("V6", "SB", "TLS", "Unused");
	return true;
      case 15:
	*tag_name = "ABI_PCS_RW_data";
	KNOWN_VALUES ("Absolute", "PC-relative", "SB-relative", "None");
	return true;
      case 16:
	*tag_name = "ABI_PCS_RO_data";
	KNOWN_VALUES ("Absolute", "PC-relative", "None");
	return true;
      case 17:
	*tag_name = "ABI_PCS_GOT_use";
	KNOWN_VALUES ("None", "direct", "GOT-indirect");
	return true;
      case 18:
	*tag_name = "ABI_PCS_wchar_t";
	return true;
      case 19:
	*tag_name = "ABI_FP_rounding";
	KNOWN_VALUES ("Unused", "Needed");
	return true;
      case 20:
	*tag_name = "ABI_FP_denormal";
	KNOWN_VALUES ("Unused", "Needed", "Sign only");
	return true;
      case 21:
	*tag_name = "ABI_FP_exceptions";
	KNOWN_VALUES ("Unused", "Needed");
	return true;
      case 22:
	*tag_name = "ABI_FP_user_exceptions";
	KNOWN_VALUES ("Unused", "Needed");
	return true;
      case 23:
	*tag_name = "ABI_FP_number_model";
	KNOWN_VALUES ("Unused", "Finite", "RTABI", "IEEE 754");
	return true;
      case 24:
	*tag_name = "ABI_align8_needed";
	KNOWN_VALUES ("No", "Yes", "4-byte");
	return true;
      case 25:
	*tag_name = "ABI_align8_preserved";
	KNOWN_VALUES ("No", "Yes, except leaf SP", "Yes");
	return true;
      case 26:
	*tag_name = "ABI_enum_size";
	KNOWN_VALUES ("Unused", "small", "int", "forced to int");
	return true;
      case 27:
	*tag_name = "ABI_HardFP_use";
	KNOWN_VALUES ("as VFP_arch", "SP only", "DP only", "SP and DP");
	return true;
      case 28:
	*tag_name = "ABI_VFP_args";
	KNOWN_VALUES ("AAPCS", "VFP registers", "custom");
	return true;
      case 29:
	*tag_name = "ABI_WMMX_args";
	KNOWN_VALUES ("AAPCS", "WMMX registers", "custom");
	return true;
      case 30:
	*tag_name = "ABI_optimization_goals";
	KNOWN_VALUES ("None",
		      "Prefer Speed",
		      "Aggressive Speed",
		      "Prefer Size",
		      "Aggressive Size",
		      "Prefer Debug",
		      "Aggressive Debug");
	return true;
      case 31:
	*tag_name = "ABI_FP_optimization_goals";
	KNOWN_VALUES ("None",
		      "Prefer Speed",
		      "Aggressive Speed",
		      "Prefer Size",
		      "Aggressive Size",
		      "Prefer Accuracy",
		      "Aggressive Accuracy");
	return true;
      case 34:
	*tag_name = "CPU_unaligned_access";
	KNOWN_VALUES ("None", "v6");
	return true;
      case 36:
	*tag_name = "VFP_HP_extension";
	KNOWN_VALUES ("Not Allowed", "Allowed");
	return true;
      case 38:
	*tag_name = "ABI_FP_16bit_format";
	KNOWN_VALUES ("None", "IEEE 754", "Alternative Format");
	return true;
      case 64:
	*tag_name = "nodefaults";
	return true;
      case 65:
	*tag_name = "also_compatible_with";
	return true;
      case 66:
	*tag_name = "T2EE_use";
	KNOWN_VALUES ("Not Allowed", "Allowed");
	return true;
      case 67:
	*tag_name = "conformance";
	return true;
      case 68:
	*tag_name = "Virtualization_use";
	KNOWN_VALUES ("Not Allowed", "Allowed");
	return true;
      case 70:
	*tag_name = "MPextension_use";
	KNOWN_VALUES ("Not Allowed", "Allowed");
	return true;
      }

  return false;
}
