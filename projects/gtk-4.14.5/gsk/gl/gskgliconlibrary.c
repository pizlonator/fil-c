/* gskgliconlibrary.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdkmemoryformatprivate.h>
#include <gdk/gdkprofilerprivate.h>
#include <gdk/gdktextureprivate.h>

#include "gskglcommandqueueprivate.h"
#include "gskgldriverprivate.h"
#include "gskgliconlibraryprivate.h"

struct _GskGLIconLibrary
{
  GskGLTextureLibrary parent_instance;
};

G_DEFINE_TYPE (GskGLIconLibrary, gsk_gl_icon_library, GSK_TYPE_GL_TEXTURE_LIBRARY)

GskGLIconLibrary *
gsk_gl_icon_library_new (GskGLDriver *driver)
{
  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), NULL);

  return g_object_new (GSK_TYPE_GL_ICON_LIBRARY,
                       "driver", driver,
                       NULL);
}

static void
gsk_gl_icon_data_free (gpointer data)
{
  GskGLIconData *icon_data = data;

  g_clear_object (&icon_data->source_texture);
  g_free (icon_data);
}

static void
gsk_gl_icon_library_class_init (GskGLIconLibraryClass *klass)
{
}

static void
gsk_gl_icon_library_init (GskGLIconLibrary *self)
{
  GskGLTextureLibrary *tl = (GskGLTextureLibrary *)self;

  tl->max_entry_size = 128;
  gsk_gl_texture_library_set_funcs (tl,
                                    NULL, NULL, NULL,
                                    gsk_gl_icon_data_free);
}

void
gsk_gl_icon_library_add (GskGLIconLibrary     *self,
                         GdkTexture           *key,
                         const GskGLIconData **out_value)
{
  GskGLTextureLibrary *tl = (GskGLTextureLibrary *)self;
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  cairo_surface_t *surface;
  GskGLIconData *icon_data;
  guint8 *pixel_data;
  guint8 *surface_data;
  guint8 *free_data = NULL;
  guint gl_format;
  guint gl_type;
  guint packed_x;
  guint packed_y;
  int width;
  int height;
  guint texture_id;

  g_assert (GSK_IS_GL_ICON_LIBRARY (self));
  g_assert (GDK_IS_TEXTURE (key));
  g_assert (out_value != NULL);

  width = key->width;
  height = key->height;

  icon_data = gsk_gl_texture_library_pack (tl,
                                           key,
                                           sizeof (GskGLIconData),
                                           width, height, 1,
                                           &packed_x, &packed_y);
  icon_data->source_texture = g_object_ref (key);

  /* actually upload the texture */
  surface = gdk_texture_download_surface (key);
  surface_data = cairo_image_surface_get_data (surface);
  gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                          "Uploading texture");

  if (gdk_gl_context_get_use_es (gdk_gl_context_get_current ()))
    {
      pixel_data = free_data = g_malloc (width * height * 4);
      gdk_memory_convert (pixel_data, width * 4,
                          GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                          surface_data, cairo_image_surface_get_stride (surface),
                          GDK_MEMORY_DEFAULT, width, height);
      gl_format = GL_RGBA;
      gl_type = GL_UNSIGNED_BYTE;
    }
  else
    {
      pixel_data = surface_data;
      gl_format = GL_BGRA;
      gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
    }

  texture_id = GSK_GL_TEXTURE_ATLAS_ENTRY_TEXTURE (icon_data);

  glBindTexture (GL_TEXTURE_2D, texture_id);

  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + 1, packed_y + 1,
                   width, height,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding top */
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + 1, packed_y,
                   width, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding left */
  glPixelStorei (GL_UNPACK_ROW_LENGTH, width);
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x, packed_y + 1,
                   1, height,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding top left */
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x, packed_y,
                   1, 1,
                   gl_format, gl_type,
                   pixel_data);

  /* Padding right */
  glPixelStorei (GL_UNPACK_SKIP_PIXELS, width - 1);
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + width + 1, packed_y + 1,
                   1, height,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding top right */
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + width + 1, packed_y,
                   1, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding bottom */
  glPixelStorei (GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei (GL_UNPACK_SKIP_ROWS, height - 1);
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + 1, packed_y + 1 + height,
                   width, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding bottom left */
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x, packed_y + 1 + height,
                   1, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding bottom right */
  glPixelStorei (GL_UNPACK_ROW_LENGTH, width);
  glPixelStorei (GL_UNPACK_SKIP_PIXELS, width - 1);
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + 1 + width, packed_y + 1 + height,
                   1, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Reset this */
  glPixelStorei (GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei (GL_UNPACK_SKIP_ROWS, 0);

  gdk_gl_context_pop_debug_group (gdk_gl_context_get_current ());

  *out_value = icon_data;

  cairo_surface_destroy (surface);
  g_free (free_data);

  tl->driver->command_queue->n_uploads++;

  if (gdk_profiler_is_running ())
    {
      char message[64];
      g_snprintf (message, sizeof message, "Size %dx%d", width, height);
      gdk_profiler_add_mark (start_time, GDK_PROFILER_CURRENT_TIME-start_time, "Upload icon", message);
    }
}
