/* Lazy binding during dlclose.  Directly loaded module.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

/* This function is called from exported_function below.  It is only
   defined in this module.  The weak attribute mimics how G++
   implements vague linkage for C++.  */
void __attribute__ ((weak))
lazily_bound_exported_function (void)
{
}

/* Called from tst-dlclose-lazy-mod2.so.  */
void
exported_function (int call_it)
{
  if (call_it)
    /* Previous to the fix this would crash when called during dlclose
       since symbols from the DSO were no longer available for binding
       (bug 30425) after the DSO started being closed by dlclose.  */
    lazily_bound_exported_function ();
}
