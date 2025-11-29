/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2021 Red Hat, Inc.
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

#include <stdlib.h>

#include "config.h"

#include "gdkjpegprivate.h"

#include <glib/gi18n-lib.h>
#include "gdktexture.h"
#include "gdktexturedownloaderprivate.h"

#include "gdkprofilerprivate.h"

#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

/* {{{ Error handling */

/* No sigsetjmp on Windows */
#ifndef HAVE_SIGSETJMP
#define sigjmp_buf jmp_buf
#define sigsetjmp(jb, x) setjmp(jb)
#define siglongjmp longjmp
#endif

struct error_handler_data {
        struct jpeg_error_mgr pub;
        sigjmp_buf setjmp_buffer;
        GError **error;
};

G_GNUC_NORETURN static void
fatal_error_handler (j_common_ptr cinfo)
{
  struct error_handler_data *errmgr;
  char buffer[JMSG_LENGTH_MAX];

  errmgr = (struct error_handler_data *) cinfo->err;

  cinfo->err->format_message (cinfo, buffer);

  if (errmgr->error && *errmgr->error == NULL)
    g_set_error (errmgr->error,
                 GDK_TEXTURE_ERROR,
                 GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                 _("Error interpreting JPEG image file (%s)"), buffer);

  siglongjmp (errmgr->setjmp_buffer, 1);

  g_assert_not_reached ();
}

static void
output_message_handler (j_common_ptr cinfo)
{
  /* do nothing */
}

/* }}} */
/* {{{ Format conversion */

static void
convert_grayscale_to_rgb (guchar *data,
                          int     width,
                          int     height,
                          int     stride)
{
  gsize x, y;
  guchar *dest, *src;

  for (y = 0; y < height; y++)
    {
      src = data + width;
      dest = data + 3 * width;
      for (x = 0; x < width; x++)
        {
          dest -= 3;
          src -= 1;
          dest[0] = *src;
          dest[1] = *src;
          dest[2] = *src;
        }
      data += stride;
    }
}

static void
convert_cmyk_to_rgba (guchar *data,
                      int     width,
                      int     height,
                      int     stride)
{
  gsize x, r;
  guchar *dest;

  for (r = 0; r < height; r++)
    {
      dest = data;
      for (x = 0; x < width; x++)
        {
          int c, m, y, k;

          c = dest[0];
          m = dest[1];
          y = dest[2];
          k = dest[3];
          dest[0] = k * c / 255;
          dest[1] = k * m / 255;
          dest[2] = k * y / 255;
          dest[3] = 255;
          dest += 4;
        }
      data += stride;
    }
}

 /* }}} */
/* {{{ Public API */

GdkTexture *
gdk_load_jpeg (GBytes  *input_bytes,
               GError **error)
{
  struct jpeg_decompress_struct info;
  struct error_handler_data jerr;
  guint width, height, stride;
  unsigned char *data = NULL;
  unsigned char *row[1];
  GBytes *bytes;
  GdkTexture *texture;
  GdkMemoryFormat format;
  G_GNUC_UNUSED guint64 before = GDK_PROFILER_CURRENT_TIME;

  info.err = jpeg_std_error (&jerr.pub);
  jerr.pub.error_exit = fatal_error_handler;
  jerr.pub.output_message = output_message_handler;
  jerr.error = error;

  if (sigsetjmp (jerr.setjmp_buffer, 1))
    {
      g_free (data);
      jpeg_destroy_decompress (&info);
      return NULL;
    }

  jpeg_create_decompress (&info);

  /* Limit to 1GB to avoid OOM with large images */
  info.mem->max_memory_to_use = 1024 * 1024 * 1024;

  jpeg_mem_src (&info,
                g_bytes_get_data (input_bytes, NULL),
                g_bytes_get_size (input_bytes));

  jpeg_read_header (&info, TRUE);
  jpeg_start_decompress (&info);

  width = info.output_width;
  height = info.output_height;

  switch ((int)info.out_color_space)
    {
    case JCS_GRAYSCALE:
    case JCS_RGB:
      stride = 3 * width;
      data = g_try_malloc_n (stride, height);
      format = GDK_MEMORY_R8G8B8;
      break;
    case JCS_CMYK:
      stride = 4 * width;
      data = g_try_malloc_n (stride, height);
      format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
      break;
    default:
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_CONTENT,
                   _("Unsupported JPEG colorspace (%d)"), info.out_color_space);
      jpeg_destroy_decompress (&info);
      return NULL;
    }

  if (!data)
    {
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_TOO_LARGE,
                   _("Not enough memory for image size %ux%u"), width, height);
      jpeg_destroy_decompress (&info);
      return NULL;
    }

  while (info.output_scanline < info.output_height)
    {
       row[0] = (unsigned char *)(&data[stride * info.output_scanline]);
       jpeg_read_scanlines (&info, row, 1);
    }

  switch ((int)info.out_color_space)
    {
    case JCS_GRAYSCALE:
      convert_grayscale_to_rgb (data, width, height, stride);
      format = GDK_MEMORY_R8G8B8;
      break;
    case JCS_RGB:
      break;
    case JCS_CMYK:
      convert_cmyk_to_rgba (data, width, height, stride);
      break;
    default:
      g_assert_not_reached ();
    }

  jpeg_finish_decompress (&info);
  jpeg_destroy_decompress (&info);

  bytes = g_bytes_new_take (data, stride * height);

  texture = gdk_memory_texture_new (width, height,
                                    format,
                                    bytes, stride);

  g_bytes_unref (bytes);

  gdk_profiler_end_mark (before, "Load jpeg", NULL);
 
  return texture;
}

GBytes *
gdk_save_jpeg (GdkTexture *texture)
{
  struct jpeg_compress_struct info;
  struct error_handler_data jerr;
  struct jpeg_error_mgr err;
  guchar *data = NULL;
  gulong size = 0;
  guchar *input = NULL;
  GdkTextureDownloader downloader;
  GBytes *texbytes = NULL;
  const guchar *texdata;
  gsize texstride;
  guchar *row;
  int width, height;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  info.err = jpeg_std_error (&jerr.pub);
  jerr.pub.error_exit = fatal_error_handler;
  jerr.pub.output_message = output_message_handler;
  jerr.error = NULL;

  if (sigsetjmp (jerr.setjmp_buffer, 1))
    {
      free (data);
      g_free (input);
      jpeg_destroy_compress (&info);
      g_clear_pointer (&texbytes, g_bytes_unref);
      return NULL;
    }

  info.err = jpeg_std_error (&err);
  jpeg_create_compress (&info);

  info.image_width = width;
  info.image_height = height;
  info.input_components = 3;
  info.in_color_space = JCS_RGB;

  jpeg_set_defaults (&info);
  jpeg_set_quality (&info, 75, TRUE);

  info.mem->max_memory_to_use = 300 * 1024 * 1024;

  jpeg_mem_dest (&info, &data, &size);

  gdk_texture_downloader_init (&downloader, texture);
  gdk_texture_downloader_set_format (&downloader, GDK_MEMORY_R8G8B8);
  texbytes = gdk_texture_downloader_download_bytes (&downloader, &texstride);
  gdk_texture_downloader_finish (&downloader);
  texdata = g_bytes_get_data (texbytes, NULL);

  jpeg_start_compress (&info, TRUE);

  while (info.next_scanline < info.image_height)
    {
      row = (guchar *) texdata + info.next_scanline * texstride;
      jpeg_write_scanlines (&info, &row, 1);
    }

  jpeg_finish_compress (&info);

  g_bytes_unref (texbytes);
  g_free (input);
  jpeg_destroy_compress (&info);

  return g_bytes_new_with_free_func (data, size, (GDestroyNotify) free, NULL);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
