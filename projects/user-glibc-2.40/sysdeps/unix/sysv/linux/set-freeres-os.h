/* System specific resource deallocation.  Linux version.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

extern void __check_pf_freemem (void) attribute_hidden;
extern char * __ttyname_freemem_ptr attribute_hidden;

#define call_freeres_os_funcs			  \
  call_function_static_weak (__check_pf_freemem); \
  call_free_static_weak (__ttyname_freemem_ptr)
