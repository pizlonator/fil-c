/*
 * Check: a unit test framework for C
 * Copyright (C) 2001, 2002 Arien Malec
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

/*
 * AC_FUNC_MALLOC in configure defines malloc to rpl_malloc if
 * malloc (0) is NULL to provide GNU compatibility
 */

#include "libcompat.h"

/* malloc has been defined to rpl_malloc, so first undo that */
#undef malloc

/* this gives us the real malloc to use below */
void *malloc(size_t n);

/* force malloc(0) to return a valid pointer */
void *rpl_malloc(size_t n)
{
    if(n == 0)
        n = 1;
    return malloc(n);
}
