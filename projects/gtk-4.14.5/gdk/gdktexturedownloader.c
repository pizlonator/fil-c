/* GTK - The GIMP Toolkit
 * Copyright (C) 2023 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.          See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GdkTextureDownloader:
 *
 * The `GdkTextureDownloader` is used to download the contents of a
 * [class@Gdk.Texture].
 *
 * It is intended to be created as a short-term object for a single download,
 * but can be used for multiple downloads of different textures or with different
 * settings.
 *
 * `GdkTextureDownloader` can be used to convert data between different formats.
 * Create a `GdkTexture` for the existing format and then download it in a
 * different format.
 *
 * Since: 4.10
 */

#include "config.h"

#include "gdktexturedownloaderprivate.h"

#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gdktextureprivate.h"

G_DEFINE_BOXED_TYPE (GdkTextureDownloader, gdk_texture_downloader,
                     gdk_texture_downloader_copy,
                     gdk_texture_downloader_free)


void
gdk_texture_downloader_init (GdkTextureDownloader *self,
                             GdkTexture           *texture)
{
  self->texture = g_object_ref (texture);
  self->format = GDK_MEMORY_DEFAULT;
}

void
gdk_texture_downloader_finish (GdkTextureDownloader *self)
{
  g_object_unref (self->texture);
}

/**
 * gdk_texture_downloader_new:
 * @texture: texture to download
 *
 * Creates a new texture downloader for @texture.
 *
 * Returns: A new texture downloader
 *
 * Since: 4.10
 **/
GdkTextureDownloader *
gdk_texture_downloader_new (GdkTexture *texture)
{
  GdkTextureDownloader *self;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);

  self = g_new (GdkTextureDownloader, 1);
  gdk_texture_downloader_init (self, texture);

  return self;
}

/**
 * gdk_texture_downloader_copy:
 * @self: the downloader to copy
 *
 * Creates a copy of the downloader.
 *
 * This function is meant for language bindings.
 *
 * Returns: A copy of the downloader
 *
 * Since: 4.10
 **/
GdkTextureDownloader *
gdk_texture_downloader_copy (const GdkTextureDownloader *self)
{
  GdkTextureDownloader *copy;

  g_return_val_if_fail (self != NULL, NULL);

  copy = gdk_texture_downloader_new (self->texture);
  gdk_texture_downloader_set_format (copy, self->format);

  return copy;
}

/**
 * gdk_texture_downloader_free:
 * @self: texture downloader to free
 *
 * Frees the given downloader and all its associated resources.
 *
 * Since: 4.10
 **/
void
gdk_texture_downloader_free (GdkTextureDownloader *self)
{
  g_return_if_fail (self != NULL);

  gdk_texture_downloader_finish (self);
  g_free (self);
}

/**
 * gdk_texture_downloader_set_texture:
 * @self: a texture downloader
 * @texture: the new texture to download
 *
 * Changes the texture the downloader will download.
 *
 * Since: 4.10
 **/
void
gdk_texture_downloader_set_texture (GdkTextureDownloader *self,
                                    GdkTexture           *texture)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (GDK_IS_TEXTURE (texture));

  g_set_object (&self->texture, texture);
}

/**
 * gdk_texture_downloader_get_texture:
 * @self: a texture downloader
 *
 * Gets the texture that the downloader will download.
 *
 * Returns: (transfer none): The texture to download
 *
 * Since: 4.10
 **/
GdkTexture *
gdk_texture_downloader_get_texture (const GdkTextureDownloader *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->texture;
}

/**
 * gdk_texture_downloader_set_format:
 * @self: a texture downloader
 * @format: the format to use
 *
 * Sets the format the downloader will download.
 *
 * By default, GDK_MEMORY_DEFAULT is set.
 *
 * Since: 4.10
 */
void
gdk_texture_downloader_set_format (GdkTextureDownloader *self,
                                   GdkMemoryFormat       format)
{
  g_return_if_fail (self != NULL);

  self->format = format;
}

/**
 * gdk_texture_downloader_get_format:
 * @self: a texture downloader
 *
 * Gets the format that the data will be downloaded in.
 *
 * Returns: The format of the download
 *
 * Since: 4.10
 **/
GdkMemoryFormat
gdk_texture_downloader_get_format (const GdkTextureDownloader *self)
{
  g_return_val_if_fail (self != NULL, GDK_MEMORY_DEFAULT);

  return self->format;
}

/**
 * gdk_texture_downloader_download_into:
 * @self: a texture downloader
 * @data: (array): pointer to enough memory to be filled with the
 *   downloaded data of the texture
 * @stride: rowstride in bytes
 *
 * Downloads the @texture into local memory.
 *
 * Since: 4.10
 **/
void
gdk_texture_downloader_download_into (const GdkTextureDownloader *self,
                                      guchar                     *data,
                                      gsize                       stride)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (stride >= gdk_texture_get_width (self->texture) * gdk_memory_format_bytes_per_pixel (self->format));

  gdk_texture_do_download (self->texture, self->format, data, stride);
}

/**
 * gdk_texture_downloader_download_bytes:
 * @self: the downloader
 * @out_stride: (out): The stride of the resulting data in bytes
 *
 * Downloads the given texture pixels into a `GBytes`. The rowstride will
 * be stored in the stride value.
 *
 * This function will abort if it tries to download a large texture and
 * fails to allocate memory. If you think that may happen, you should handle
 * memory allocation yourself and use [method@Gdk.TextureDownloader.download_into]
 * once allocation succeeded.
 *
 * Returns: The downloaded pixels
 *
 * Since: 4.10
 **/
GBytes *
gdk_texture_downloader_download_bytes (const GdkTextureDownloader *self,
                                       gsize                      *out_stride)
{
  guchar *data;
  gsize stride;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (out_stride != NULL, NULL);

  if (GDK_IS_MEMORY_TEXTURE (self->texture) &&
      gdk_texture_get_format (self->texture) == self->format)
    {
      GdkMemoryTexture *memtex = GDK_MEMORY_TEXTURE (self->texture);

      return g_bytes_ref (gdk_memory_texture_get_bytes (memtex, out_stride));
    }

  stride = self->texture->width * gdk_memory_format_bytes_per_pixel (self->format);
  data = g_malloc_n (stride, self->texture->height);

  gdk_texture_do_download (self->texture, self->format, data, stride);

  *out_stride = stride;
  return g_bytes_new_take (data, stride * self->texture->height);
}

