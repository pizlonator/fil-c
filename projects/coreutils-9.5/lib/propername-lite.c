/* Localization of proper names.
   Copyright 2023-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include "propername.h"

#include "c-strcase.h"
#include "gettext.h"
#include "localcharset.h"

/* Return the localization of the name spelled NAME_ASCII in ASCII,
   and NAME_UTF8 in UTF-8.  */

char const *
proper_name_lite (char const *name_ascii, char const *name_utf8)
{
  char const *translation = gettext (name_ascii);
  return (translation != name_ascii ? translation
          : c_strcasecmp (locale_charset (), "UTF-8") == 0 ? name_utf8
          : name_ascii);
}
