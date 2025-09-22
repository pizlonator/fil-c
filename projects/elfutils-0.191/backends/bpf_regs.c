/* Register names and numbers for BPF DWARF.
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

#include <stdio.h>
#include <string.h>

#include "bpf.h"

#define BACKEND bpf_
#include "libebl_CPU.h"

ssize_t
bpf_register_info (Ebl *ebl __attribute__ ((unused)),
		   int regno, char *name, size_t namelen,
		   const char **prefix, const char **setname,
		   int *bits, int *type)
{
  ssize_t len;

  if (name == NULL)
    return MAX_BPF_REG;
  if (regno < 0 || regno >= MAX_BPF_REG)
    return -1;

  *prefix = "";
  *setname = "integer";
  *bits = 64;
  *type = DW_ATE_signed;

  len = snprintf(name, namelen, "r%d", regno);
  return ((size_t)len < namelen ? len : -1);
}
