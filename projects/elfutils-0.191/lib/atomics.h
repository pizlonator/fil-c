/* Conditional wrapper header for C11-style atomics.
   Copyright (C) 2019-2019 Red Hat, Inc.
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

#include "config.h"

#if HAVE_STDATOMIC_H
/* If possible, use the compiler's preferred atomics.  */
# include <stdatomic.h>
#else
/* Otherwise, try to use the builtins provided by this compiler.  */
# include "stdatomic-fbsd.h"
#endif /* HAVE_STDATOMIC_H */
