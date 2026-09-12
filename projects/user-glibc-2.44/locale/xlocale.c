/* C locale object.
   Copyright (C) 2001-2026 Free Software Foundation, Inc.
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

#include <locale.h>
#include "localeinfo.h"
#include <ctype.h>

#define DEFINE_CATEGORY(category, category_name, items, a) \
extern struct __locale_data _nl_C_##category;
#include "categories.def"
#undef	DEFINE_CATEGORY

const struct __locale_struct _nl_C_locobj attribute_hidden =
  {
    .__locales =
    {
#define DEFINE_CATEGORY(category, category_name, items, a) \
      [category] = &_nl_C_##category,
#include "categories.def"
#undef	DEFINE_CATEGORY
    },
    .__names =
    {
      [LC_ALL] = _nl_C_name,
#define DEFINE_CATEGORY(category, category_name, items, a) \
      [category] = _nl_C_name,
#include "categories.def"
#undef	DEFINE_CATEGORY
    },
    .__ctype_b = (const unsigned short int *) _nl_C_LC_CTYPE_class + 128,
    .__ctype_tolower = (const int *) _nl_C_LC_CTYPE_tolower + 128,
    .__ctype_toupper = (const int *) _nl_C_LC_CTYPE_toupper + 128
  };
