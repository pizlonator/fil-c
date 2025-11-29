/*
 * Copyright © 2020 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkconfig.h"

#include <OpenGL/CGLIOSurface.h>
#include <QuartzCore/QuartzCore.h>

#include "gdkmacosbuffer-private.h"
#include "gdkmacosglcontext-private.h"
#include "gdkmacossurface-private.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

G_DEFINE_TYPE (GdkMacosGLContext, gdk_macos_gl_context, GDK_TYPE_GL_CONTEXT)

#define CHECK(error,cgl_error) _CHECK_CGL(error, G_STRLOC, cgl_error)
static inline gboolean
_CHECK_CGL (GError     **error,
            const char  *location,
            CGLError     cgl_error)
{
  if (cgl_error != kCGLNoError)
    {
      g_log ("Core OpenGL",
             G_LOG_LEVEL_CRITICAL,
             "%s: %s",
             location, CGLErrorString (cgl_error));
      g_set_error (error,
                   GDK_GL_ERROR,
                   GDK_GL_ERROR_NOT_AVAILABLE,
                   "%s",
                   CGLErrorString (cgl_error));
      return FALSE;
    }

  return TRUE;
}

/* Apple's OpenGL implementation does not contain the extension to
 * perform log handler callbacks when errors occur. Therefore, to aid in
 * tracking down issues we have a CHECK_GL() macro that can wrap GL
 * calls and check for an error afterwards.
 *
 * To make this easier, we use a statement expression, as this will
 * always be using something GCC-compatible on macOS.
 */
#define CHECK_GL(error,func) _CHECK_GL(error, G_STRLOC, ({ func; glGetError(); }))
static inline gboolean
_CHECK_GL (GError     **error,
           const char  *location,
           GLenum       gl_error)
{
  const char *msg;

  switch (gl_error)
    {
    case GL_INVALID_ENUM:
      msg = "invalid enum";
      break;
    case GL_INVALID_VALUE:
      msg = "invalid value";
      break;
    case GL_INVALID_OPERATION:
      msg = "invalid operation";
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      msg = "invalid framebuffer operation";
      break;
    case GL_OUT_OF_MEMORY:
      msg = "out of memory";
      break;
    default:
      msg = "unknown error";
      break;
    }

  if (gl_error != GL_NO_ERROR)
    {
      g_log ("OpenGL",
             G_LOG_LEVEL_CRITICAL,
             "%s: %s", location, msg);
      if (error != NULL)
        g_set_error (error,
                     GDK_GL_ERROR,
                     GDK_GL_ERROR_NOT_AVAILABLE,
                     "%s", msg);
      return FALSE;
    }

  return TRUE;
}

static gboolean
check_framebuffer_status (GLenum target)
{
  switch (glCheckFramebufferStatus (target))
    {
    case GL_FRAMEBUFFER_COMPLETE:
      return TRUE;

    case GL_FRAMEBUFFER_UNDEFINED:
      g_critical ("Framebuffer is undefined");
      return FALSE;

    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      g_critical ("Framebuffer has incomplete attachment");
      return FALSE;

    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      g_critical ("Framebuffer has missing attachment");
      return FALSE;

    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
      g_critical ("Framebuffer has incomplete draw buffer");
      return FALSE;

    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
      g_critical ("Framebuffer has incomplete read buffer");
      return FALSE;

    case GL_FRAMEBUFFER_UNSUPPORTED:
      g_critical ("Framebuffer is unsupported");
      return FALSE;

    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
      g_critical ("Framebuffer has incomplete multisample");
      return FALSE;

    default:
      g_critical ("Framebuffer has unknown error");
      return FALSE;
    }
}

static const char *
get_renderer_name (GLint id)
{
  static char renderer_name[32];

  switch (id & kCGLRendererIDMatchingMask)
  {
  case kCGLRendererGenericID: return "Generic";
  case kCGLRendererGenericFloatID: return "Generic Float";
  case kCGLRendererAppleSWID: return "Apple Software Renderer";
  case kCGLRendererATIRage128ID: return "ATI Rage 128";
  case kCGLRendererATIRadeonID: return "ATI Radeon";
  case kCGLRendererATIRageProID: return "ATI Rage Pro";
  case kCGLRendererATIRadeon8500ID: return "ATI Radeon 8500";
  case kCGLRendererATIRadeon9700ID: return "ATI Radeon 9700";
  case kCGLRendererATIRadeonX1000ID: return "ATI Radeon X1000";
  case kCGLRendererATIRadeonX2000ID: return "ATI Radeon X2000";
  case kCGLRendererATIRadeonX3000ID: return "ATI Radeon X3000";
  case kCGLRendererATIRadeonX4000ID: return "ATI Radeon X4000";
  case kCGLRendererGeForce2MXID: return "GeForce 2 MX";
  case kCGLRendererGeForce3ID: return "GeForce 3";
  case kCGLRendererGeForceFXID: return "GeForce FX";
  case kCGLRendererGeForce8xxxID: return "GeForce 8xxx";
  case kCGLRendererGeForceID: return "GeForce";
  case kCGLRendererVTBladeXP2ID: return "VT Blade XP 2";
  case kCGLRendererIntel900ID: return "Intel 900";
  case kCGLRendererIntelX3100ID: return "Intel X3100";
  case kCGLRendererIntelHDID: return "Intel HD";
  case kCGLRendererIntelHD4000ID: return "Intel HD 4000";
  case kCGLRendererIntelHD5000ID: return "Intel HD 5000";
  case kCGLRendererMesa3DFXID: return "Mesa 3DFX";

  default:
    snprintf (renderer_name, sizeof renderer_name, "0x%08x", id & kCGLRendererIDMatchingMask);
    renderer_name[sizeof renderer_name-1] = 0;
    return renderer_name;
  }
}

static GLuint
create_texture (CGLContextObj cgl_context,
                GLuint        target,
                IOSurfaceRef  io_surface,
                guint         width,
                guint         height)
{
  GLuint texture = 0;

  if (!CHECK_GL (NULL, glActiveTexture (GL_TEXTURE0)) ||
      !CHECK_GL (NULL, glGenTextures (1, &texture)) ||
      !CHECK_GL (NULL, glBindTexture (target, texture)) ||
      !CHECK (NULL, CGLTexImageIOSurface2D (cgl_context,
                                            target,
                                            GL_RGBA,
                                            width,
                                            height,
                                            GL_BGRA,
                                            GL_UNSIGNED_INT_8_8_8_8_REV,
                                            io_surface,
                                            0)) ||
      !CHECK_GL (NULL, glTexParameteri (target, GL_TEXTURE_BASE_LEVEL, 0)) ||
      !CHECK_GL (NULL, glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_NEAREST)) ||
      !CHECK_GL (NULL, glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_NEAREST)) ||
      !CHECK_GL (NULL, glTexParameteri (target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)) ||
      !CHECK_GL (NULL, glTexParameteri (target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE)) ||
      !CHECK_GL (NULL, glTexParameteri (target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)) ||
      !CHECK_GL (NULL, glBindTexture (target, 0)))
    {
      glDeleteTextures (1, &texture);
      return 0;
    }

  return texture;
}

static void
gdk_macos_gl_context_allocate (GdkMacosGLContext *self)
{
  GdkSurface *surface;
  GLint opaque;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));
  g_assert (self->cgl_context != NULL);
  g_assert (self->target != 0);
  g_assert (self->texture != 0 || self->fbo == 0);
  g_assert (self->fbo != 0 || self->texture == 0);

  if (!(surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self))))
    return;

  /* Alter to an opaque surface if necessary */
  opaque = _gdk_macos_surface_is_opaque (GDK_MACOS_SURFACE (surface));
  if (opaque != self->last_opaque)
    {
      self->last_opaque = !!opaque;
      if (!CHECK (NULL, CGLSetParameter (self->cgl_context, kCGLCPSurfaceOpacity, &opaque)))
        return;
    }

  if (self->texture == 0)
    {
      GdkMacosBuffer *buffer;
      IOSurfaceRef io_surface;
      guint width;
      guint height;
      GLuint texture = 0;
      GLuint fbo = 0;

      buffer = _gdk_macos_surface_get_buffer (GDK_MACOS_SURFACE (surface));
      io_surface = _gdk_macos_buffer_get_native (buffer);
      width = _gdk_macos_buffer_get_width (buffer);
      height = _gdk_macos_buffer_get_height (buffer);

      /* We might need to re-enforce our CGL context here to keep
       * video playing correctly. Something, somewhere, might have
       * changed the context without touching GdkGLContext.
       *
       * Without this, video_player often breaks in gtk-demo when using
       * the GStreamer backend.
       */
      CGLSetCurrentContext (self->cgl_context);

      if (!(texture = create_texture (self->cgl_context, self->target, io_surface, width, height)) ||
          !CHECK_GL (NULL, glGenFramebuffers (1, &fbo)) ||
          !CHECK_GL (NULL, glBindFramebuffer (GL_FRAMEBUFFER, fbo)) ||
          !CHECK_GL (NULL, glBindTexture (self->target, texture)) ||
          !CHECK_GL (NULL, glFramebufferTexture2D (GL_FRAMEBUFFER,
                                                   GL_COLOR_ATTACHMENT0,
                                                   self->target,
                                                   texture,
                                                   0)) ||
          !check_framebuffer_status (GL_FRAMEBUFFER))
        {
          glDeleteFramebuffers (1, &fbo);
          glDeleteTextures (1, &texture);
          return;
        }

      glBindTexture (self->target, 0);
      glBindFramebuffer (GL_FRAMEBUFFER, 0);

      self->texture = texture;
      self->fbo = fbo;
    }
}

static void
gdk_macos_gl_context_release (GdkMacosGLContext *self)
{
  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));
  g_assert (self->texture != 0 || self->fbo == 0);
  g_assert (self->fbo != 0 || self->texture == 0);

  glBindFramebuffer (GL_FRAMEBUFFER, 0);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (self->target, 0);

  if (self->fbo != 0)
    {
      glDeleteFramebuffers (1, &self->fbo);
      self->fbo = 0;
    }

  if (self->texture != 0)
    {
      glDeleteTextures (1, &self->texture);
      self->texture = 0;
    }
}

static CGLPixelFormatObj
create_pixel_format (GdkGLVersion  *version,
                     gboolean      *out_legacy,
                     GError       **error)
{
  CGLPixelFormatAttribute attrs[] = {
    kCGLPFAOpenGLProfile, 0,
    kCGLPFAAllowOfflineRenderers, /* allow sharing across GPUs */
    kCGLPFADepthSize, 0,
    kCGLPFAStencilSize, 0,
    kCGLPFAColorSize, 24,
    kCGLPFAAlphaSize, 8,
    0
  };
  CGLPixelFormatObj format = NULL;
  GLint n_format = 1;

  *out_legacy = FALSE;

  if (gdk_gl_version_get_major (version) >= 4)
    {
      attrs[1] = (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core;
      if (CHECK (error, CGLChoosePixelFormat (attrs, &format, &n_format)))
        return g_steal_pointer (&format);
    }

  if (gdk_gl_version_greater_equal (version, &GDK_GL_MIN_GL_VERSION))
    {
      attrs[1] = (CGLPixelFormatAttribute)kCGLOGLPVersion_GL3_Core;
      if (CHECK (error, CGLChoosePixelFormat (attrs, &format, &n_format)))
        return g_steal_pointer (&format);
    }

  attrs[1] = (CGLPixelFormatAttribute) kCGLOGLPVersion_Legacy;
  if (!CHECK (error, CGLChoosePixelFormat (attrs, &format, &n_format)))
    return NULL;

  *out_legacy = TRUE;

  return g_steal_pointer (&format);
}

static GdkGLAPI
gdk_macos_gl_context_real_realize (GdkGLContext  *context,
                                   GError       **error)
{
  GdkMacosGLContext *self = (GdkMacosGLContext *)context;
  GdkSurface *surface;
  GdkDisplay *display;
  CGLPixelFormatObj pixelFormat;
  CGLContextObj shared_gl_context = nil;
  CGLContextObj cgl_context;
  CGLContextObj existing;
  GdkGLContext *shared;
  GLint sync_to_framerate = 1;
  GLint validate = 0;
  GLint renderer_id = 0;
  GLint swapRect[4];
  GdkGLVersion min_version, version;
  gboolean legacy;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  if (self->cgl_context != nil)
    return GDK_GL_API_GL;

  if (!gdk_gl_context_is_api_allowed (context, GDK_GL_API_GL, error))
    return 0;

  existing = CGLGetCurrentContext ();

  gdk_gl_context_get_matching_version (context,
                                       GDK_GL_API_GL,
                                       FALSE,
                                       &min_version);

  display = gdk_gl_context_get_display (context);
  shared = gdk_display_get_gl_context (display);

  if (shared != NULL)
    {
      if (!(shared_gl_context = GDK_MACOS_GL_CONTEXT (shared)->cgl_context))
        {
          g_set_error_literal (error,
                               GDK_GL_ERROR,
                               GDK_GL_ERROR_NOT_AVAILABLE,
                               "Cannot access shared CGLContextObj");
          return 0;
        }
    }

  GDK_DISPLAY_DEBUG (display, OPENGL,
                     "Creating CGLContextObj (version %d.%d)",
                     gdk_gl_version_get_major (&min_version), gdk_gl_version_get_minor (&min_version));

  if (!(pixelFormat = create_pixel_format (&min_version, &legacy, error)))
    return 0;

  if (!CHECK (error, CGLCreateContext (pixelFormat, shared_gl_context, &cgl_context)))
    {
      CGLReleasePixelFormat (pixelFormat);
      return 0;
    }

  CGLSetCurrentContext (cgl_context);
  CGLReleasePixelFormat (pixelFormat);

  gdk_gl_version_init_epoxy (&version);
  if (!gdk_gl_version_greater_equal (&version, &min_version))
    {
      CGLReleaseContext (cgl_context);
      return 0;
    }

  if (validate)
    CHECK (NULL, CGLEnable (cgl_context, kCGLCEStateValidation));

  if (!CHECK (error, CGLSetParameter (cgl_context, kCGLCPSwapInterval, &sync_to_framerate)) ||
      !CHECK (error, CGLGetParameter (cgl_context, kCGLCPCurrentRendererID, &renderer_id)))
   {
      CGLReleaseContext (cgl_context);
      return 0;
   }

  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));

  if (surface != NULL)
    {
      /* Setup initial swap rectangle. We might not actually need this
       * anymore though as we are rendering to an IOSurface and we have
       * a scissor clip when rendering to it.
       */
      swapRect[0] = 0;
      swapRect[1] = 0;
      swapRect[2] = surface->width;
      swapRect[3] = surface->height;
      CGLSetParameter (cgl_context, kCGLCPSwapRectangle, swapRect);
      CGLEnable (cgl_context, kCGLCESwapRectangle);
    }

  GDK_DISPLAY_DEBUG (display, OPENGL,
                     "Created CGLContextObj@%p using %s",
                     cgl_context,
                     get_renderer_name (renderer_id));

  gdk_gl_context_set_version (context, &version);
  gdk_gl_context_set_is_legacy (context, legacy);

  self->cgl_context = g_steal_pointer (&cgl_context);

  if (existing != NULL)
    CGLSetCurrentContext (existing);

  return GDK_GL_API_GL;
}

static void
gdk_macos_gl_context_begin_frame (GdkDrawContext *context,
                                  GdkMemoryDepth  depth,
                                  cairo_region_t *region)
{
  GdkMacosGLContext *self = (GdkMacosGLContext *)context;
  GdkMacosBuffer *buffer;
  GdkSurface *surface;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  surface = gdk_draw_context_get_surface (context);
  buffer = _gdk_macos_surface_get_buffer (GDK_MACOS_SURFACE (surface));

  _gdk_macos_buffer_set_flipped (buffer, TRUE);
  _gdk_macos_buffer_set_damage (buffer, region);

  /* Create our render target and bind it */
  gdk_gl_context_make_current (GDK_GL_CONTEXT (self));
  gdk_macos_gl_context_allocate (self);

  GDK_DRAW_CONTEXT_CLASS (gdk_macos_gl_context_parent_class)->begin_frame (context, depth, region);

  gdk_gl_context_make_current (GDK_GL_CONTEXT (self));
  CHECK_GL (NULL, glBindFramebuffer (GL_FRAMEBUFFER, self->fbo));
}

static void
gdk_macos_gl_context_end_frame (GdkDrawContext *context,
                                cairo_region_t *painted)
{
  GdkMacosGLContext *self = GDK_MACOS_GL_CONTEXT (context);
  GdkSurface *surface;
  cairo_rectangle_int_t flush_rect;
  GLint swapRect[4];

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));
  g_assert (self->cgl_context != nil);

  GDK_DRAW_CONTEXT_CLASS (gdk_macos_gl_context_parent_class)->end_frame (context, painted);

  surface = gdk_draw_context_get_surface (context);
  gdk_gl_context_make_current (GDK_GL_CONTEXT (self));

  /* Coordinates are in display coordinates, where as flush_rect is
  * in GDK coordinates. Must flip Y to match display coordinates where
  * 0,0 is the bottom-left corner.
  */
  cairo_region_get_extents (painted, &flush_rect);
  swapRect[0] = flush_rect.x;                   /* left */
  swapRect[1] = surface->height - flush_rect.y; /* bottom */
  swapRect[2] = flush_rect.width;               /* width */
  swapRect[3] = flush_rect.height;              /* height */
  CGLSetParameter (self->cgl_context, kCGLCPSwapRectangle, swapRect);

  gdk_macos_gl_context_release (self);

  glFlush ();

  /* Begin a Core Animation transaction so that all changes we
   * make within the window are seen atomically.
   */
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  _gdk_macos_surface_swap_buffers (GDK_MACOS_SURFACE (surface), painted);
  [CATransaction commit];
}

static void
gdk_macos_gl_context_empty_frame (GdkDrawContext *draw_context)
{
}

static void
gdk_macos_gl_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkMacosGLContext *self = (GdkMacosGLContext *)draw_context;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  if (self->cgl_context != NULL)
    CGLUpdateContext (self->cgl_context);
}

static gboolean
gdk_macos_gl_context_clear_current (GdkGLContext *context)
{
  GdkMacosGLContext *self = GDK_MACOS_GL_CONTEXT (context);

  g_return_val_if_fail (GDK_IS_MACOS_GL_CONTEXT (self), FALSE);

  if (self->cgl_context == CGLGetCurrentContext ())
    {
      glFlush ();
      CGLSetCurrentContext (NULL);
    }

  return TRUE;
}

static gboolean
gdk_macos_gl_context_is_current (GdkGLContext *context)
{
  GdkMacosGLContext *self = GDK_MACOS_GL_CONTEXT (context);

  return self->cgl_context == CGLGetCurrentContext ();
}

static gboolean
gdk_macos_gl_context_make_current (GdkGLContext *context,
                                   gboolean      surfaceless)
{
  GdkMacosGLContext *self = GDK_MACOS_GL_CONTEXT (context);
  CGLContextObj current;

  g_return_val_if_fail (GDK_IS_MACOS_GL_CONTEXT (self), FALSE);

  current = CGLGetCurrentContext ();

  if (self->cgl_context != current)
    {
      /* The OpenGL mac programming guide suggests that glFlush() is called
       * before switching current contexts to ensure that the drawing commands
       * are submitted.
       *
       * TODO: investigate if we need this because we may switch contexts
       *       during composition and only need it when returning to a
       *       previous context that uses the other context.
       */
      if (current != NULL)
        glFlush ();

      CGLSetCurrentContext (self->cgl_context);
    }

  return TRUE;
}

static cairo_region_t *
gdk_macos_gl_context_get_damage (GdkGLContext *context)
{
  GdkMacosGLContext *self = (GdkMacosGLContext *)context;
  const cairo_region_t *damage;
  GdkMacosBuffer *buffer;
  GdkSurface *surface;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  if ((surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context))) &&
      (buffer = GDK_MACOS_SURFACE (surface)->front) &&
      (damage = _gdk_macos_buffer_get_damage (buffer)))
    return cairo_region_copy (damage);

  return GDK_GL_CONTEXT_CLASS (gdk_macos_gl_context_parent_class)->get_damage (context);
}

static guint
gdk_macos_gl_context_get_default_framebuffer (GdkGLContext *context)
{
  return GDK_MACOS_GL_CONTEXT (context)->fbo;
}

static void
gdk_macos_gl_context_dispose (GObject *gobject)
{
  GdkMacosGLContext *self = GDK_MACOS_GL_CONTEXT (gobject);

  self->texture = 0;
  self->fbo = 0;

  if (self->cgl_context != nil)
    {
      CGLContextObj cgl_context = g_steal_pointer (&self->cgl_context);

      if (cgl_context == CGLGetCurrentContext ())
        CGLSetCurrentContext (NULL);

      CGLClearDrawable (cgl_context);
      CGLDestroyContext (cgl_context);
    }

  G_OBJECT_CLASS (gdk_macos_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_macos_gl_context_class_init (GdkMacosGLContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkGLContextClass *gl_class = GDK_GL_CONTEXT_CLASS (klass);

  object_class->dispose = gdk_macos_gl_context_dispose;

  draw_context_class->begin_frame = gdk_macos_gl_context_begin_frame;
  draw_context_class->end_frame = gdk_macos_gl_context_end_frame;
  draw_context_class->empty_frame = gdk_macos_gl_context_empty_frame;
  draw_context_class->surface_resized = gdk_macos_gl_context_surface_resized;

  gl_class->get_damage = gdk_macos_gl_context_get_damage;
  gl_class->clear_current = gdk_macos_gl_context_clear_current;
  gl_class->is_current = gdk_macos_gl_context_is_current;
  gl_class->make_current = gdk_macos_gl_context_make_current;
  gl_class->realize = gdk_macos_gl_context_real_realize;
  gl_class->get_default_framebuffer = gdk_macos_gl_context_get_default_framebuffer;

  gl_class->backend_type = GDK_GL_CGL;
}

static void
gdk_macos_gl_context_init (GdkMacosGLContext *self)
{
  self->target = GL_TEXTURE_RECTANGLE;
}

G_GNUC_END_IGNORE_DEPRECATIONS
