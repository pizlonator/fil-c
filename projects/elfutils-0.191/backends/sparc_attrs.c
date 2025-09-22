/* Object attribute tags for SPARC.
   Copyright (C) 2015 Oracle, Inc.
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

#define BACKEND sparc_
#include "libebl_CPU.h"

bool
sparc_check_object_attribute (Ebl *ebl __attribute__ ((unused)),
			      const char *vendor, int tag, uint64_t value,
			      const char **tag_name, const char **value_name)
{
  static const char *hwcaps[32] =
    {
      "mul32", "div32", "fsmuld", "v8plus", "popc", "vis", "vis2",
      "asi_blk_init", "fmaf", "vis3", "hpc", "random", "trans",
      "fjfmau", "ima", "asi_cache_sparing", "aes", "des", "kasumi",
      "camellia", "md5", "sha1", "sha256", "sha512", "mpmul", "mont",
      "pause", "cbcond", "crc32c", "resv30", "resv31"
    };

  
  static const char *hwcaps2[32] =
    {
      "fjathplus", "vis3b", "adp", "sparc5", "mwait", "xmpmul", "xmont",
      "nsec", "resv8", "resv9" , "resv10", "resv11", "fjathhpc", "fjdes",
      "fjaes", "resv15", "resv16", "resv17", "resv18", "resv19", "resv20",
      "resv21", "resv22", "resv23", "resv24", "resv25", "resv26", "resv27",
      "resv28", "resv29", "resv30", "resv31",
    };

  /* NAME should be big enough to hold any possible comma-separated
     list (no repetitions allowed) of attribute names from one of the
     arrays above.  */
  static char name[32*17+32+1];
  name[0] = '\0';

  if (!strcmp (vendor, "gnu"))
    switch (tag)
      {
      case 4:
      case 8:
        {
          const char **caps;
          int cap;
          
          if (tag == 4)
            {
              *tag_name = "GNU_Sparc_HWCAPS";
              caps = hwcaps;
            }
          else
            {
              *tag_name = "GNU_Sparc_HWCAPS2";
              caps = hwcaps2;
            }
          
          char *s = name;
          for (cap = 0; cap < 32; cap++)
            if (value & (1U << cap))
              {
                if (*s != '\0')
                  s = strcat (s, ",");
                s = strcat (s, caps[cap]);
              }
          
          *value_name = s;
          return true;
        }
      }

  return false;
}

