/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _OPENGL_VERSIONS_H_
#define _OPENGL_VERSIONS_H_

/* list of known OpenGL versions */
/* *INDENT-OFF* */
static const struct { int major, minor; } opengl_versions[] = {
   {4, 5},
   {4, 4},
   {4, 3},
   {4, 2},
   {4, 1},
   {4, 0},

   {3, 3},
   {3, 2},
   {3, 1},
   {3, 0},

   {2, 1},
   {2, 0},

   {1, 5},
   {1, 4},
   {1, 3},
   {1, 2},
   {1, 1},
   {1, 0},

   {0, 0} /* end of list */
};

/* *INDENT-ON* */

#endif /* _OPENGL_VERSIONS_H_ */
