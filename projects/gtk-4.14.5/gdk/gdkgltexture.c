/* gdkgltexture.c
 *
 * Copyright 2016  Benjamin Otte
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

#include "gdkgltextureprivate.h"

#include "gdkdisplayprivate.h"
#include "gdkglcontextprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"

#include <epoxy/gl.h>

/**
 * GdkGLTexture:
 *
 * A GdkTexture representing a GL texture object.
 */

struct _GdkGLTexture {
  GdkTexture parent_instance;

  GdkGLContext *context;
  guint id;
  gboolean has_mipmap;
  gpointer sync;

  GdkTexture *saved;

  GDestroyNotify destroy;
  gpointer data;
};

struct _GdkGLTextureClass {
  GdkTextureClass parent_class;
};

G_DEFINE_TYPE (GdkGLTexture, gdk_gl_texture, GDK_TYPE_TEXTURE)

static void
drop_gl_resources (GdkGLTexture *self)
{
  if (self->destroy)
    {
      self->destroy (self->data);
      self->destroy = NULL;
      self->data = NULL;
    }

  g_clear_object (&self->context);
  self->id = 0;
}

static void
gdk_gl_texture_dispose (GObject *object)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (object);

  drop_gl_resources (self);

  g_clear_object (&self->saved);

  G_OBJECT_CLASS (gdk_gl_texture_parent_class)->dispose (object);
}

typedef void (* GLFunc) (GdkGLTexture *self,
                         GdkGLContext *context,
                         gpointer      data);

typedef struct _InvokeData
{
  GdkGLTexture *self;
  volatile int spinlock;
  GLFunc func;
  gpointer data;
} InvokeData;

static gboolean
gdk_gl_texture_invoke_callback (gpointer data)
{
  InvokeData *invoke = data;
  GdkGLContext *context, *previous;

  context = gdk_display_get_gl_context (gdk_gl_context_get_display (invoke->self->context));

  previous = gdk_gl_context_get_current ();
  gdk_gl_context_make_current (context);

  if (invoke->self->sync && context != invoke->self->context)
    glWaitSync (invoke->self->sync, 0, GL_TIMEOUT_IGNORED);

  glBindTexture (GL_TEXTURE_2D, invoke->self->id);

  invoke->func (invoke->self, context, invoke->data);

  g_atomic_int_set (&invoke->spinlock, 1);

  if (previous)
    gdk_gl_context_make_current (previous);
  else
    gdk_gl_context_clear_current ();

  return FALSE;
}

static void
gdk_gl_texture_run (GdkGLTexture *self,
                    GLFunc        func,
                    gpointer      data)
{
  InvokeData invoke = { self, 0, func, data };

  g_main_context_invoke (NULL, gdk_gl_texture_invoke_callback, &invoke);

  while (g_atomic_int_get (&invoke.spinlock) == 0);
}

typedef struct _Download Download;

struct _Download
{
  GdkMemoryFormat format;
  guchar *data;
  gsize stride;
};

static gboolean
gdk_gl_texture_find_format (GdkGLContext    *context,
                            GdkMemoryAlpha   alpha,
                            GLint            gl_format,
                            GLint            gl_type,
                            GdkMemoryFormat *out_format)
{
  GdkMemoryFormat format;

  for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
    {
      GLint q_internal_format;
      GLenum q_format, q_type;
      GLint q_swizzle[4];

      if (gdk_memory_format_alpha (format) != alpha)
        continue;

      if (!(gdk_gl_context_get_format_flags (context, format) & GDK_GL_FORMAT_RENDERABLE))
        continue;

      gdk_memory_format_gl_format (format,
                                   gdk_gl_context_get_use_es (context),
                                   &q_internal_format,
                                   &q_format,
                                   &q_type,
                                   q_swizzle);

      if (q_format != gl_format || q_type != gl_type)
        continue;

      *out_format = format;
      return TRUE;
    }

  return FALSE;
}

static inline void
gdk_gl_texture_do_download (GdkGLTexture *self,
                            GdkGLContext *context,
                            gpointer      download_)
{
  GdkTexture *texture = GDK_TEXTURE (self);
  GdkMemoryFormat format;
  gsize expected_stride;
  Download *download = download_;
  GLint gl_internal_format;
  GLenum gl_format, gl_type;
  GLint gl_swizzle[4];

  format = gdk_texture_get_format (texture),
  expected_stride = texture->width * gdk_memory_format_bytes_per_pixel (download->format);

  if (!gdk_gl_context_get_use_es (context) &&
      ((gdk_gl_context_get_format_flags (context, format) & GDK_GL_FORMAT_USABLE) == GDK_GL_FORMAT_USABLE))
    {
      gdk_memory_format_gl_format (format,
                                   gdk_gl_context_get_use_es (context),
                                   &gl_internal_format,
                                   &gl_format, &gl_type, gl_swizzle);
      if (download->stride == expected_stride &&
          download->format == format)
        {
          glGetTexImage (GL_TEXTURE_2D,
                         0,
                         gl_format,
                         gl_type,
                         download->data);
        }
      else
        {
          gsize stride = texture->width * gdk_memory_format_bytes_per_pixel (format);
          guchar *pixels = g_malloc_n (stride, texture->height);

          glPixelStorei (GL_PACK_ALIGNMENT, 1);
          glGetTexImage (GL_TEXTURE_2D,
                         0,
                         gl_format,
                         gl_type,
                         pixels);

          gdk_memory_convert (download->data,
                              download->stride,
                              download->format,
                              pixels,
                              stride,
                              format,
                              texture->width,
                              texture->height);

          g_free (pixels);

        }
    }
  else
    {
      GdkMemoryFormat actual_format;
      GLenum gl_read_format, gl_read_type;
      GLuint fbo;

      glGenFramebuffers (1, &fbo);
      glBindFramebuffer (GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self->id, 0);
      if (gdk_gl_context_check_version (context, "4.3", "3.1"))
        {
          GLint read_format, read_type;
          glGetFramebufferParameteriv (GL_FRAMEBUFFER, GL_IMPLEMENTATION_COLOR_READ_FORMAT, &read_format);
          glGetFramebufferParameteriv (GL_FRAMEBUFFER, GL_IMPLEMENTATION_COLOR_READ_TYPE, &read_type);
          if (gdk_gl_texture_find_format (context, gdk_memory_format_alpha (format), read_format, read_type, &actual_format))
            {
              gl_read_format = read_format;
              gl_read_type = read_type;
            }
          else
            {
              actual_format = gdk_memory_depth_get_format (gdk_memory_format_get_depth (format));
              if (gdk_memory_format_alpha (format) == GDK_MEMORY_ALPHA_STRAIGHT)
                actual_format = gdk_memory_format_get_straight (actual_format);

              gdk_memory_format_gl_format (actual_format,
                                           gdk_gl_context_get_use_es (context),
                                           &gl_internal_format,
                                           &gl_read_format, &gl_read_type, gl_swizzle);
            }
        }
      else
        {
          actual_format = gdk_memory_depth_get_format (gdk_memory_format_get_depth (format));
          if (gdk_memory_format_alpha (format) == GDK_MEMORY_ALPHA_STRAIGHT)
            actual_format = gdk_memory_format_get_straight (actual_format);

          gdk_memory_format_gl_format (actual_format,
                                       gdk_gl_context_get_use_es (context),
                                       &gl_internal_format,
                                       &gl_read_format, &gl_read_type, gl_swizzle);
        }

      if (download->format == actual_format &&
          (download->stride == expected_stride))
        {
          glReadPixels (0, 0,
                        texture->width, texture->height,
                        gl_read_format,
                        gl_read_type,
                        download->data);
        }
      else
        {
          gsize actual_bpp = gdk_memory_format_bytes_per_pixel (actual_format);
          gsize stride = actual_bpp * texture->width;
          guchar *pixels = g_malloc_n (stride, texture->height);

          glPixelStorei (GL_PACK_ALIGNMENT, 1);
          glReadPixels (0, 0,
                        texture->width, texture->height,
                        gl_read_format,
                        gl_read_type,
                        pixels);

          /* Fix up gles inadequacies */

          if (gl_read_format == GL_RGBA &&
              gl_read_type == GL_UNSIGNED_BYTE &&
              (format == GDK_MEMORY_G8A8 ||
               format == GDK_MEMORY_G8A8_PREMULTIPLIED ||
               format == GDK_MEMORY_G8 ||
               format == GDK_MEMORY_A8))
            {
              for (unsigned int y = 0; y < texture->height; y++)
                {
                  for (unsigned int x = 0; x < texture->width; x++)
                    {
                      guchar *data = &pixels[y * stride + x * actual_bpp];
                      if (format == GDK_MEMORY_G8A8 ||
                          format == GDK_MEMORY_G8A8_PREMULTIPLIED)
                        {
                          data[3] = data[1];
                          data[1] = data[0];
                          data[2] = data[0];
                        }
                      else if (format == GDK_MEMORY_G8)
                        {
                          data[1] = data[0];
                          data[2] = data[0];
                          data[3] = 0xff;
                        }
                      else if (format == GDK_MEMORY_A8)
                        {
                          data[3] = data[0];
                          data[0] = 0;
                          data[1] = 0;
                          data[2] = 0;
                        }
                    }
                }
            }

          if (gl_read_format == GL_RGBA &&
              gl_read_type == GL_UNSIGNED_SHORT &&
              (format == GDK_MEMORY_G16A16 ||
               format == GDK_MEMORY_G16A16_PREMULTIPLIED ||
               format == GDK_MEMORY_G16 ||
               format == GDK_MEMORY_A16))
            {
              for (unsigned int y = 0; y < texture->height; y++)
                {
                  for (unsigned int x = 0; x < texture->width; x++)
                    {
                      guint16 *data = (guint16 *) &pixels[y * stride + x * actual_bpp];
                      if (format == GDK_MEMORY_G16A16 ||
                          format == GDK_MEMORY_G16A16_PREMULTIPLIED)
                        {
                          data[3] = data[1];
                          data[1] = data[0];
                          data[2] = data[0];
                        }
                      else if (format == GDK_MEMORY_G16)
                        {
                          data[1] = data[0];
                          data[2] = data[0];
                          data[3] = 0xffff;
                        }
                      else if (format == GDK_MEMORY_A16)
                        {
                          data[3] = data[0];
                          data[0] = 0;
                          data[1] = 0;
                          data[2] = 0;
                        }
                    }
                }
            }

          gdk_memory_convert (download->data,
                              download->stride,
                              download->format,
                              pixels,
                              stride,
                              actual_format,
                              texture->width,
                              texture->height);

          g_free (pixels);
        }
      glBindFramebuffer (GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers (1, &fbo);
    }
}

static void
gdk_gl_texture_download (GdkTexture      *texture,
                         GdkMemoryFormat  format,
                         guchar          *data,
                         gsize            stride)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);
  Download download;

  if (self->saved)
    {
      gdk_texture_do_download (self->saved, format, data, stride);
      return;
    }

  download.format = format;
  download.data = data;
  download.stride = stride;

  gdk_gl_texture_run (self, gdk_gl_texture_do_download, &download);
}

static void
gdk_gl_texture_class_init (GdkGLTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_gl_texture_download;

  gobject_class->dispose = gdk_gl_texture_dispose;
}

static void
gdk_gl_texture_init (GdkGLTexture *self)
{
}

GdkGLContext *
gdk_gl_texture_get_context (GdkGLTexture *self)
{
  return self->context;
}

guint
gdk_gl_texture_get_id (GdkGLTexture *self)
{
  return self->id;
}

gboolean
gdk_gl_texture_has_mipmap (GdkGLTexture *self)
{
  return self->has_mipmap;
}

gpointer
gdk_gl_texture_get_sync (GdkGLTexture *self)
{
  return self->sync;
}

/**
 * gdk_gl_texture_release:
 * @self: a `GdkTexture` wrapping a GL texture
 *
 * Releases the GL resources held by a `GdkGLTexture`.
 *
 * The texture contents are still available via the
 * [method@Gdk.Texture.download] function, after this
 * function has been called.
 */
void
gdk_gl_texture_release (GdkGLTexture *self)
{
  GdkTexture *texture;

  g_return_if_fail (GDK_IS_GL_TEXTURE (self));
  g_return_if_fail (self->saved == NULL);

  texture = GDK_TEXTURE (self);
  self->saved = GDK_TEXTURE (gdk_memory_texture_from_texture (texture,
                                                              gdk_texture_get_format (texture)));

  drop_gl_resources (self);
}

GdkTexture *
gdk_gl_texture_new_from_builder (GdkGLTextureBuilder *builder,
                                 GDestroyNotify       destroy,
                                 gpointer             data)
{
  GdkGLTexture *self;
  GdkTexture *update_texture;

  self = g_object_new (GDK_TYPE_GL_TEXTURE,
                       "width", gdk_gl_texture_builder_get_width (builder),
                       "height", gdk_gl_texture_builder_get_height (builder),
                       NULL);

  self->context = g_object_ref (gdk_gl_texture_builder_get_context (builder));
  self->id = gdk_gl_texture_builder_get_id (builder);
  GDK_TEXTURE (self)->format = gdk_gl_texture_builder_get_format (builder);
  self->has_mipmap = gdk_gl_texture_builder_get_has_mipmap (builder);
  if (gdk_gl_context_has_feature (self->context, GDK_GL_FEATURE_SYNC))
    self->sync = gdk_gl_texture_builder_get_sync (builder);
  self->destroy = destroy;
  self->data = data;

  update_texture = gdk_gl_texture_builder_get_update_texture (builder);
  if (update_texture)
    {
      cairo_region_t *update_region = gdk_gl_texture_builder_get_update_region (builder);
      if (update_region)
        {
          update_region = cairo_region_copy (update_region);
          cairo_region_intersect_rectangle (update_region,
                                            &(cairo_rectangle_int_t) {
                                              0, 0,
                                              update_texture->width, update_texture->height
                                            });
          gdk_texture_set_diff (GDK_TEXTURE (self), update_texture, update_region);
        }
    }

  return GDK_TEXTURE (self);
}

static void
gdk_gl_texture_determine_format (GdkGLTexture *self)
{
  GdkTexture *texture = GDK_TEXTURE (self);
  GdkGLContext *context;
  GLint active_texture;
  GLint internal_format;
  GLint width, height;

  /* Abort if somebody else is GL-ing here... */
  context = gdk_gl_context_get_current ();
  if (context == NULL ||
      !gdk_gl_context_is_shared (self->context, context) ||
      /* ... or glGetTexLevelParameter() isn't supported */
      !gdk_gl_context_check_version (context, NULL, "3.1"))
    {
      texture->format = GDK_MEMORY_DEFAULT;
      self->has_mipmap = FALSE;
      return;
    }

  /* We need to be careful about modifying the GL context, as this is not
   * expected during construction */
  glGetIntegerv (GL_TEXTURE_BINDING_2D, &active_texture);
  glBindTexture (GL_TEXTURE_2D, self->id);

  glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

  switch (internal_format)
  {
    case GL_RGB8:
    case GL_RGB:
      texture->format = GDK_MEMORY_R8G8B8;
      break;

    case GL_RGBA8:
      texture->format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
      break;

    case GL_RGB16:
      texture->format = GDK_MEMORY_R16G16B16;
      break;

    case GL_RGBA16:
      texture->format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
      break;

    case GL_RGB16F:
      texture->format = GDK_MEMORY_R16G16B16_FLOAT;
      break;

    case GL_RGBA16F:
      texture->format = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED;
      break;

    case GL_RGB32F:
      texture->format = GDK_MEMORY_R32G32B32_FLOAT;
      break;

    case GL_RGBA32F:
      texture->format = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
      break;

    case GL_RGBA:
      {
        GLint red_size = 0;
        GLint green_size = 0;
        GLint blue_size = 0;
        GLint alpha_size = 0;
        GLint red_type = 0;
        GLint green_type = 0;
        GLint blue_type = 0;
        GLint alpha_type = 0;

        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_RED_TYPE, &red_type);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_GREEN_TYPE, &green_type);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_BLUE_TYPE, &blue_type);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_TYPE, &alpha_type);

        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &red_size);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_GREEN_SIZE, &green_size);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_BLUE_SIZE, &blue_size);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE, &alpha_size);

#define CHECK_RGBA(rt,gt,bt,at,rs,gs,bs,as) \
        (red_type == rt && green_type == gt && blue_type == bt && alpha_type == at && \
         red_size == rs && green_size == gs && blue_size == bs && alpha_size == as)

        if (CHECK_RGBA (GL_UNSIGNED_NORMALIZED, GL_UNSIGNED_NORMALIZED, GL_UNSIGNED_NORMALIZED, GL_UNSIGNED_NORMALIZED, 8, 8, 8, 8))
          {
            texture->format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
            break;
          }

#undef CHECK_RGBA
      }

      G_GNUC_FALLTHROUGH;

    default:
      g_warning ("Texture in unexpected format 0x%X (%d). File a bug about adding it to GTK", internal_format, internal_format);
      /* fallback to the dumbest possible format
       * so that even age old GLES can do it */
      texture->format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
      break;
  }

  /* Determine if the texture has a mipmap.
   * We do this here, since it requires binding the texture,
   * and we're already doing that here.
   * GL has no way to directly query 'mipmap completeness' of textures,
   * so we just check that level 1 has the expected size, and assume
   * that means somebody called glGenerateMipmap().
   */
  glGetTexLevelParameteriv (GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &width);
  glGetTexLevelParameteriv (GL_TEXTURE_2D, 1, GL_TEXTURE_HEIGHT, &height);

  self->has_mipmap = width == texture->width / 2 &&
                     height == texture->height / 2;

  /* restore previous state */
  glBindTexture (GL_TEXTURE_2D, active_texture);
}

/**
 * gdk_gl_texture_new:
 * @context: a `GdkGLContext`
 * @id: the ID of a texture that was created with @context
 * @width: the nominal width of the texture
 * @height: the nominal height of the texture
 * @destroy: a destroy notify that will be called when the GL resources
 *   are released
 * @data: data that gets passed to @destroy
 *
 * Creates a new texture for an existing GL texture.
 *
 * Note that the GL texture must not be modified until @destroy is called,
 * which will happen when the GdkTexture object is finalized, or due to
 * an explicit call of [method@Gdk.GLTexture.release].
 *
 * Return value: (transfer full) (type GdkGLTexture): A newly-created
 *   `GdkTexture`
 *
 * Deprecated: 4.12: [class@Gdk.GLTextureBuilder] supersedes this function
 *   and provides extended functionality for creating GL textures.
 */
GdkTexture *
gdk_gl_texture_new (GdkGLContext   *context,
                    guint           id,
                    int             width,
                    int             height,
                    GDestroyNotify  destroy,
                    gpointer        data)
{
  GdkGLTexture *self;

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);
  g_return_val_if_fail (id != 0, NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  self = g_object_new (GDK_TYPE_GL_TEXTURE,
                       "width", width,
                       "height", height,
                       NULL);

  self->context = g_object_ref (context);
  self->id = id;
  self->destroy = destroy;
  self->data = data;

  gdk_gl_texture_determine_format (self);

  return GDK_TEXTURE (self);
}

