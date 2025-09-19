/* Memory management for dlerror messages.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#ifndef _DLERROR_H
#define _DLERROR_H

#include <dlfcn.h>
#include <ldsodefs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* Thread-local variable for storing dlfcn failures for subsequent
   reporting via dlerror.  */
extern __thread char *__libc_dlerror_result_string
  attribute_tls_model_ie;

#endif /* _DLERROR_H */
