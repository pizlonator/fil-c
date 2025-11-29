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

#include "gdkglcontext.h"
#include "gdkdrawcontextprivate.h"
#include "gdkglversionprivate.h"
#include "gdkdmabufprivate.h"

G_BEGIN_DECLS

typedef enum {
  GDK_GL_FEATURE_DEBUG                      = 1 << 0,
  GDK_GL_FEATURE_UNPACK_SUBIMAGE            = 1 << 1,
  GDK_GL_FEATURE_VERTEX_HALF_FLOAT          = 1 << 2,
  GDK_GL_FEATURE_SYNC                       = 1 << 3,
  GDK_GL_FEATURE_BASE_INSTANCE              = 1 << 4,
  GDK_GL_FEATURE_BUFFER_STORAGE             = 1 << 5,
} GdkGLFeatures;

typedef enum {
  GDK_GL_NONE = 0,
  GDK_GL_EGL,
  GDK_GL_GLX,
  GDK_GL_WGL,
  GDK_GL_CGL
} GdkGLBackend;

typedef enum {
  /* The format is supported for glTexImage2D() */
  GDK_GL_FORMAT_USABLE = 1 << 0,
  /* The format can be rendered to.
   * GL/GLES spec term: "color-renderable" */
  GDK_GL_FORMAT_RENDERABLE = 1 << 1,
  /* GL_LINEAR/GL_MIPMAP_LINEAR can be used for textures in this format.
   * GLES spec term: "texture-filterable" */
  GDK_GL_FORMAT_FILTERABLE = 1 << 2
} GdkGLMemoryFlags;

/* The maximum amount of buffers we track update regions for.
 * Note that this is equal to the max buffer age value we
 * can provide a damage region for */
#define GDK_GL_MAX_TRACKED_BUFFERS 4

#define GDK_GL_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GL_CONTEXT, GdkGLContextClass))
#define GDK_IS_GL_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GL_CONTEXT))
#define GDK_GL_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GL_CONTEXT, GdkGLContextClass))

typedef struct _GdkGLContextClass       GdkGLContextClass;

struct _GdkGLContext
{
  GdkDrawContext parent_instance;

  /* We store the old drawn areas to support buffer-age optimizations */
  cairo_region_t *old_updated_area[GDK_GL_MAX_TRACKED_BUFFERS];
};

struct _GdkGLContextClass
{
  GdkDrawContextClass parent_class;

  GdkGLBackend        backend_type;

  GdkGLAPI              (* realize)                             (GdkGLContext          *context,
                                                                 GError               **error);

  gboolean              (* make_current)                        (GdkGLContext          *context,
                                                                 gboolean               surfaceless);
  gboolean              (* clear_current)                       (GdkGLContext          *context);
  gboolean              (* is_current)                          (GdkGLContext          *context);
  cairo_region_t *      (* get_damage)                          (GdkGLContext          *context);

  gboolean              (* is_shared)                           (GdkGLContext          *self,
                                                                 GdkGLContext          *other);

  guint                 (* get_default_framebuffer)             (GdkGLContext          *self);
};

typedef struct {
  guint program;
  guint position_location;
  guint uv_location;
  guint map_location;
  guint flip_location;
} GdkGLContextProgram;

typedef struct {
  guint tmp_framebuffer;
  guint tmp_vertex_buffer;

  GdkGLContextProgram texture_2d_quad_program;
  GdkGLContextProgram texture_rect_quad_program;

  GdkGLContextProgram *current_program;

  guint is_legacy : 1;
  guint use_es : 1;
} GdkGLContextPaintData;

gboolean                gdk_gl_backend_can_be_used              (GdkGLBackend     backend_type,
                                                                 GError         **error);
void                    gdk_gl_backend_use                      (GdkGLBackend     backend_type);

void                    gdk_gl_context_clear_current_if_surface (GdkSurface      *surface);

GdkGLContext *          gdk_gl_context_new                      (GdkDisplay      *display,
                                                                 GdkSurface      *surface);

gboolean                gdk_gl_context_is_api_allowed           (GdkGLContext           *self,
                                                                 GdkGLAPI                api,
                                                                 GError                **error);
void                    gdk_gl_context_set_version              (GdkGLContext           *context,
                                                                 const GdkGLVersion     *version);
void                    gdk_gl_context_set_is_legacy            (GdkGLContext           *context,
                                                                 gboolean                is_legacy);
gboolean                gdk_gl_context_check_gl_version         (GdkGLContext           *context,
                                                                 const GdkGLVersion     *gl_version,
                                                                 const GdkGLVersion     *gles_version);

static inline gboolean
gdk_gl_context_check_version (GdkGLContext *context,
                              const char   *gl_version,
                              const char   *gles_version)
{
  return gdk_gl_context_check_gl_version (context,
                                          gl_version ? &GDK_GL_VERSION_STRING (gl_version) : NULL,
                                          gles_version ? &GDK_GL_VERSION_STRING (gles_version) : NULL);
}

void                    gdk_gl_context_get_matching_version     (GdkGLContext           *context,
                                                                 GdkGLAPI                api,
                                                                 gboolean                legacy,
                                                                 GdkGLVersion           *out_version);

void                    gdk_gl_context_push_debug_group         (GdkGLContext    *context,
                                                                 const char      *message);
void                    gdk_gl_context_push_debug_group_printf  (GdkGLContext    *context,
                                                                 const char      *format,
                                                                 ...)  G_GNUC_PRINTF (2, 3);
void                    gdk_gl_context_pop_debug_group          (GdkGLContext    *context);
void                    gdk_gl_context_label_object             (GdkGLContext    *context,
                                                                 guint            identifier,
                                                                 guint            name,
                                                                 const char      *label);
void                    gdk_gl_context_label_object_printf      (GdkGLContext    *context,
                                                                 guint            identifier,
                                                                 guint            name,
                                                                 const char      *format,
                                                                ...)  G_GNUC_PRINTF (4, 5);

const char *            gdk_gl_context_get_glsl_version_string  (GdkGLContext    *self);

GdkGLMemoryFlags        gdk_gl_context_get_format_flags         (GdkGLContext    *self,
                                                                 GdkMemoryFormat  format) G_GNUC_PURE;
gboolean                gdk_gl_context_has_feature              (GdkGLContext    *self,
                                                                 GdkGLFeatures    feature) G_GNUC_PURE;

gboolean                gdk_gl_context_use_es_bgra              (GdkGLContext    *context);

gboolean                gdk_gl_context_has_vertex_arrays        (GdkGLContext    *self) G_GNUC_PURE;

double                  gdk_gl_context_get_scale                (GdkGLContext    *self);

guint                   gdk_gl_context_import_dmabuf            (GdkGLContext    *self,
                                                                 int              width,
                                                                 int              height,
                                                                 const GdkDmabuf *dmabuf,
                                                                 gboolean        *external);

gboolean                gdk_gl_context_export_dmabuf            (GdkGLContext    *self,
                                                                 unsigned int     texture_id,
                                                                 GdkDmabuf       *dmabuf);

G_END_DECLS
