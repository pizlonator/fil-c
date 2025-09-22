/* Return function name in line.
   This file is part of elfutils.
   Written by John Mellor-Crummey <johnmc@rice.edu>, 2021.

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
#include "libdwP.h"


const char *
dwarf_linefunctionname (Dwarf *dbg, Dwarf_Line *line)
{
  if (dbg == NULL || line == NULL)
    return NULL;
  if (line->context == 0)
    return NULL;

  Elf_Data *str_data = dbg->sectiondata[IDX_debug_str];
  if (str_data == NULL || line->function_name >= str_data->d_size
      || memchr (str_data->d_buf + line->function_name, '\0',
		 str_data->d_size - line->function_name) == NULL)
    return NULL;

  return (char *) str_data->d_buf + line->function_name;
}
