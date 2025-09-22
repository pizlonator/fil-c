/* Return context in line.
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

#include "libdwP.h"


Dwarf_Line*
dwarf_linecontext (Dwarf_Lines* lines, Dwarf_Line *line)
{
  if (lines == NULL || line == NULL)
    return NULL;
  if (line->context == 0 || line->context >= lines->nlines)
    return NULL;

  return lines->info + (line->context - 1);
}
