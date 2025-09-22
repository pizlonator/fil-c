/* Abstract map data type, with out-of-memory checking.
   Copyright (C) 2009-2021 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2018.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#ifndef _GL_XMAP_H
#define _GL_XMAP_H

#include "gl_map.h"
#include "xalloc.h"

#ifndef _GL_INLINE_HEADER_BEGIN
 #error "Please include config.h first."
#endif
_GL_INLINE_HEADER_BEGIN
#ifndef GL_XMAP_INLINE
# define GL_XMAP_INLINE _GL_INLINE
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* These functions are thin wrappers around the corresponding functions with
   _nx_ infix from gl_map.h.  Upon out-of-memory, they invoke xalloc_die (),
   instead of returning an error indicator.  */
#if 0 /* These are defined inline below.  */
extern gl_map_t gl_map_create_empty (gl_map_implementation_t implementation,
                                     gl_mapkey_equals_fn equals_fn,
                                     gl_mapkey_hashcode_fn hashcode_fn,
                                     gl_mapkey_dispose_fn kdispose_fn,
                                     gl_mapvalue_dispose_fn vdispose_fn)
  /*_GL_ATTRIBUTE_DEALLOC (gl_map_free, 1)*/
  _GL_ATTRIBUTE_RETURNS_NONNULL;
extern bool gl_map_put (gl_map_t map, const void *key, const void *value);
extern bool gl_map_getput (gl_map_t map, const void *key, const void *value,
                           const void **oldvaluep);
#endif

GL_XMAP_INLINE
/*_GL_ATTRIBUTE_DEALLOC (gl_map_free, 1)*/
_GL_ATTRIBUTE_RETURNS_NONNULL
gl_map_t
gl_map_create_empty (gl_map_implementation_t implementation,
                     gl_mapkey_equals_fn equals_fn,
                     gl_mapkey_hashcode_fn hashcode_fn,
                     gl_mapkey_dispose_fn kdispose_fn,
                     gl_mapvalue_dispose_fn vdispose_fn)
{
  gl_map_t result =
    gl_map_nx_create_empty (implementation, equals_fn, hashcode_fn,
                            kdispose_fn, vdispose_fn);
  if (result == NULL)
    xalloc_die ();
  return result;
}

GL_XMAP_INLINE bool
gl_map_put (gl_map_t map, const void *key, const void *value)
{
  int result = gl_map_nx_put (map, key, value);
  if (result < 0)
    xalloc_die ();
  return result;
}

GL_XMAP_INLINE bool
gl_map_getput (gl_map_t map, const void *key, const void *value,
               const void **oldvaluep)
{
  int result = gl_map_nx_getput (map, key, value, oldvaluep);
  if (result < 0)
    xalloc_die ();
  return result;
}

#ifdef __cplusplus
}
#endif

_GL_INLINE_HEADER_END

#endif /* _GL_XMAP_H */
