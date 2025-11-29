/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkcairo.h"

#include "gdkglcontextprivate.h"
#include "gdksurfaceprivate.h"

#include <epoxy/gl.h>
#include <math.h>
#include <string.h>

/* x,y,width,height describes a rectangle in the gl render buffer
   coordinate space, and its top left corner is drawn at the current
   position according to the cairo translation. */

/**
 * gdk_cairo_draw_from_gl:
 * @cr: a cairo context
 * @surface: The surface we're rendering for (not necessarily into)
 * @source: The GL ID of the source buffer
 * @source_type: The type of the @source
 * @buffer_scale: The scale-factor that the @source buffer is allocated for
 * @x: The source x position in @source to start copying from in GL coordinates
 * @y: The source y position in @source to start copying from in GL coordinates
 * @width: The width of the region to draw
 * @height: The height of the region to draw
 *
 * The main way to not draw GL content in GTK.
 *
 * It takes a render buffer ID (@source_type == GL_RENDERBUFFER) or a texture
 * id (@source_type == GL_TEXTURE) and draws it onto @cr with an OVER operation,
 * respecting the current clip. The top left corner of the rectangle specified
 * by @x, @y, @width and @height will be drawn at the current (0,0) position of
 * the `cairo_t`.
 *
 * This will work for *all* `cairo_t`, as long as @surface is realized, but the
 * fallback implementation that reads back the pixels from the buffer may be
 * used in the general case. In the case of direct drawing to a surface with
 * no special effects applied to @cr it will however use a more efficient
 * approach.
 *
 * For GL_RENDERBUFFER the code will always fall back to software for buffers
 * with alpha components, so make sure you use GL_TEXTURE if using alpha.
 *
 * Calling this may change the current GL context.
 *
 * Deprecated: 4.6: The function is overly complex and produces broken output
 *   in various combinations of arguments. If you want to draw with GL textures
 *   in GTK, use [ctor@Gdk.GLTexture.new]; if you want to use that texture in
 *   Cairo, use [method@Gdk.Texture.download] to download the data into a Cairo
 *   image surface.
 */
void
gdk_cairo_draw_from_gl (cairo_t              *cr,
                        GdkSurface            *surface,
                        int                   source,
                        int                   source_type,
                        int                   buffer_scale,
                        int                   x,
                        int                   y,
                        int                   width,
                        int                   height)
{
  GdkGLContext *paint_context;
  cairo_surface_t *image;
  guint framebuffer;
  int alpha_size = 0;
  int major, minor, version;
  gboolean es_use_bgra = FALSE;

  paint_context = gdk_surface_get_paint_gl_context (surface, NULL);
  if (paint_context == NULL)
    {
      g_warning ("gdk_cairo_draw_gl_render_buffer failed - no paint context");
      return;
    }

  es_use_bgra = gdk_gl_context_use_es_bgra (paint_context);

  gdk_gl_context_make_current (paint_context);

  glGenFramebuffers (1, &framebuffer);

  if (source_type == GL_RENDERBUFFER)
    {
      glBindRenderbuffer (GL_RENDERBUFFER, source);
      glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE,  &alpha_size);
    }
  else if (source_type == GL_TEXTURE)
    {
      glBindTexture (GL_TEXTURE_2D, source);

      if (gdk_gl_context_get_use_es (paint_context))
        alpha_size = 1;
      else
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE,  &alpha_size);
    }
  else
    {
      g_warning ("Unsupported gl source type %d\n", source_type);
      return;
    }

  gdk_gl_context_get_version (paint_context, &major, &minor);
  version = major * 100 + minor;

  /* TODO: Use glTexSubImage2D() and do a row-by-row copy to replace
   * the GL_UNPACK_ROW_LENGTH support
   */
  if (gdk_gl_context_get_use_es (paint_context) &&
      !(version >= 300 || gdk_gl_context_has_feature (paint_context, GDK_GL_FEATURE_UNPACK_SUBIMAGE)))
    return;

  /* TODO: avoid reading back non-required data due to dest clip */
  image = cairo_surface_create_similar_image (cairo_get_target (cr),
                                              (alpha_size == 0) ? CAIRO_FORMAT_RGB24 : CAIRO_FORMAT_ARGB32,
                                              width, height);

  cairo_surface_set_device_scale (image, buffer_scale, buffer_scale);

  glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);

  if (source_type == GL_RENDERBUFFER)
    {
      /* Create a framebuffer with the source renderbuffer and
         make it the current target for reads */
      glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_RENDERBUFFER, source);
    }
  else
    {
      glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, source, 0);
    }

  glPixelStorei (GL_PACK_ALIGNMENT, 4);
  glPixelStorei (GL_PACK_ROW_LENGTH, cairo_image_surface_get_stride (image) / 4);

  /* The implicit format conversion is going to make this path slower */
  if (!gdk_gl_context_get_use_es (paint_context))
    glReadPixels (x, y, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                  cairo_image_surface_get_data (image));
  else
    glReadPixels (x, y, width, height, es_use_bgra ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE,
                  cairo_image_surface_get_data (image));

  glPixelStorei (GL_PACK_ROW_LENGTH, 0);

  glBindFramebuffer (GL_FRAMEBUFFER, 0);

  glDeleteFramebuffers (1, &framebuffer);

  cairo_surface_mark_dirty (image);

  cairo_set_source_surface (cr, image, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_paint (cr);

  cairo_surface_destroy (image);
}
