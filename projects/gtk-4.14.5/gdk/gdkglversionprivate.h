/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontextprivate.h: GL context abstraction
 *
 * Copyright © 2014  Emmanuele Bassi
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gdkenums.h>

G_BEGIN_DECLS

/* Version requirements for EGL contexts.
 *
 * If you add support for EGL to your backend, please require this.
 */
#define GDK_EGL_MIN_VERSION_MAJOR (1)
#define GDK_EGL_MIN_VERSION_MINOR (4)

/* Minimum OpenGL versions supported by GTK.
 * Backends should make sure to never create a context of a previous version.
 *
 * The macros refer to OpenGL; OpenGL with OPENGL_COMPATIBILITY_PROFILE_BIT as
 * OPENGL_PROFILE_MASK; and OpenGL ES respectively
 */
#define GDK_GL_MIN_GL_VERSION GDK_GL_VERSION_INIT (3, 2)
#define GDK_GL_MIN_GL_LEGACY_VERSION GDK_GL_VERSION_INIT (3, 0)
#define GDK_GL_MIN_GLES_VERSION GDK_GL_VERSION_INIT (2, 0)

typedef struct _GdkGLVersion GdkGLVersion;

struct _GdkGLVersion
{
  int major;
  int minor;
};

#define GDK_GL_VERSION_INIT(maj,min) { maj, min }

static const GdkGLVersion supported_gl_versions[] = {
  GDK_GL_VERSION_INIT (4, 6),
  GDK_GL_VERSION_INIT (4, 5),
  GDK_GL_VERSION_INIT (4, 4),
  GDK_GL_VERSION_INIT (4, 3),
  GDK_GL_VERSION_INIT (4, 2),
  GDK_GL_VERSION_INIT (4, 1),
  GDK_GL_VERSION_INIT (4, 0),
  GDK_GL_VERSION_INIT (3, 3),
  GDK_GL_VERSION_INIT (3, 2),
  GDK_GL_VERSION_INIT (3, 1),
  GDK_GL_VERSION_INIT (3, 0),

  GDK_GL_VERSION_INIT (0, 0)
};

static const GdkGLVersion supported_gles_versions[] = {
  GDK_GL_VERSION_INIT (3, 2),
  GDK_GL_VERSION_INIT (3, 1),
  GDK_GL_VERSION_INIT (3, 0),
  GDK_GL_VERSION_INIT (2, 0),

  GDK_GL_VERSION_INIT (0, 0)
};

#undef GDK_GL_VERSION_INIT
#define GDK_GL_VERSION_INIT(maj,min) (GdkGLVersion) { maj, min }
#define GDK_GL_VERSION_STRING(str) GDK_GL_VERSION_INIT(str[0] - '0', str[2] - '0')

static inline const GdkGLVersion *
gdk_gl_versions_get_for_api (GdkGLAPI api)
{
  switch (api)
    {
      case GDK_GL_API_GL:
        return supported_gl_versions;
      case GDK_GL_API_GLES:
        return supported_gles_versions;
      default:
        g_assert_not_reached ();
        return NULL;
    }
}

static inline int
gdk_gl_version_get_major (const GdkGLVersion *self)
{
  return self->major;
}

static inline int
gdk_gl_version_get_minor (const GdkGLVersion *self)
{
  return self->minor;
}

static inline int
gdk_gl_version_compare (const GdkGLVersion *a,
                        const GdkGLVersion *b)
{
  if (a->major > b->major)
    return 1;
  if (a->major < b->major)
    return -1;

  if (a->minor > b->minor)
    return 1;
  if (a->minor < b->minor)
    return -1;

  return 0;
}

static inline gboolean
gdk_gl_version_greater_equal (const GdkGLVersion *a,
                              const GdkGLVersion *b)
{
  return gdk_gl_version_compare (a, b) >= 0;
}

/* in gdkglcontext.c */
void            gdk_gl_version_init_epoxy                       (GdkGLVersion           *version);

G_END_DECLS

