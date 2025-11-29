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

#include "config.h"

#include "gdktiffprivate.h"

#include <glib/gi18n-lib.h>
#include "gdkmemoryformatprivate.h"
#include "gdkmemorytexture.h"
#include "gdkprofilerprivate.h"
#include "gdktexturedownloaderprivate.h"

#include <tiffio.h>

/* Our main interest in tiff as an image format is that it is
 * flexible enough to save all our texture formats without
 * lossy conversions.
 *
 * The loader isn't meant to be a very versatile. It just aims
 * to load the subset that we're saving ourselves. For anything
 * else, we fall back to TIFFRGBAImage, which is the same api
 * that gdk-pixbuf uses.
 */

/* {{{ IO handling */

typedef struct
{
  GBytes **out_bytes;
  gchar *data;
  gsize size;
  gsize position;
} TiffIO;

static void
tiff_io_warning (const char *module,
                 const char *fmt,
                 va_list     ap) G_GNUC_PRINTF(2, 0);
static void
tiff_io_warning (const char *module,
                 const char *fmt,
                 va_list     ap)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, fmt, ap);
}

static void
tiff_io_error (const char *module,
               const char *fmt,
               va_list     ap) G_GNUC_PRINTF(2, 0);
static void
tiff_io_error (const char *module,
               const char *fmt,
               va_list     ap)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, fmt, ap);
}

static tsize_t
tiff_io_no_read (thandle_t handle,
                 tdata_t   buffer,
                 tsize_t   size)
{
  return (tsize_t) -1;
}

static tsize_t
tiff_io_read (thandle_t handle,
              tdata_t   buffer,
              tsize_t   size)
{
  TiffIO *io = (TiffIO *) handle;
  gsize read;

  if (io->position >= io->size)
    return 0;

  read = MIN (size, io->size - io->position);

  memcpy (buffer, io->data + io->position, read);
  io->position += read;

  return (tsize_t) read;
}

static tsize_t
tiff_io_no_write (thandle_t handle,
                  tdata_t   buffer,
                  tsize_t   size)
{
  return (tsize_t) -1;
}

static tsize_t
tiff_io_write (thandle_t handle,
               tdata_t   buffer,
               tsize_t   size)
{
  TiffIO *io = (TiffIO *) handle;

  if (io->position > io->size ||
      io->size - io->position < size)
    {
      io->size = io->position + size;
      io->data = g_realloc (io->data, io->size);
    }

  memcpy (io->data + io->position, buffer, size);
  io->position += size;

  return (tsize_t) size;
}

static toff_t
tiff_io_seek (thandle_t handle,
              toff_t    offset,
              int       whence)
{
  TiffIO *io = (TiffIO *) handle;

  switch (whence)
    {
    default:
      return -1;
    case SEEK_SET:
      break;
    case SEEK_CUR:
      offset += io->position;
      break;
    case SEEK_END:
      offset += io->size;
      break;
    }
  if (offset < 0)
    return -1;

  io->position = offset;

  return offset;
}

static int
tiff_io_close (thandle_t handle)
{
  TiffIO *io = (TiffIO *) handle;

  if (io->out_bytes)
    *io->out_bytes = g_bytes_new_take (io->data, io->size);

  g_free (io);

  return 0;
}

static toff_t
tiff_io_get_file_size (thandle_t handle)
{
  TiffIO *io = (TiffIO *) handle;

  return io->size;
}

static TIFF *
tiff_open_read (GBytes *bytes)
{
  TiffIO *io;

  TIFFSetWarningHandler ((TIFFErrorHandler) tiff_io_warning);
  TIFFSetErrorHandler ((TIFFErrorHandler) tiff_io_error);

  io = g_new0 (TiffIO, 1);

  io->data = (char *) g_bytes_get_data (bytes, &io->size);

  return TIFFClientOpen ("GTK-read", "r",
                         (thandle_t) io,
                         tiff_io_read,
                         tiff_io_no_write,
                         tiff_io_seek,
                         tiff_io_close,
                         tiff_io_get_file_size,
                         NULL, NULL);
}

static TIFF *
tiff_open_write (GBytes **result)
{
  TiffIO *io;

  TIFFSetWarningHandler ((TIFFErrorHandler) tiff_io_warning);
  TIFFSetErrorHandler ((TIFFErrorHandler) tiff_io_error);

  io = g_new0 (TiffIO, 1);

  io->out_bytes = result;

  return TIFFClientOpen ("GTK-write", "w",
                         (thandle_t) io,
                         tiff_io_no_read,
                         tiff_io_write,
                         tiff_io_seek,
                         tiff_io_close,
                         tiff_io_get_file_size,
                         NULL, NULL);
}

/* }}} */
/* {{{ Public API */

typedef struct _FormatData FormatData;
struct _FormatData {
  GdkMemoryFormat format;
  guint16 bits_per_sample;
  guint16 samples_per_pixel;
  guint16 sample_format;
  guint16 alpha_samples;
  guint16 photometric;
};

static const FormatData format_data[] = {
  [GDK_MEMORY_B8G8R8A8_PREMULTIPLIED]           = { GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,            8, 4, SAMPLEFORMAT_UINT,   EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_A8R8G8B8_PREMULTIPLIED]           = { GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,            8, 4, SAMPLEFORMAT_UINT,   EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_R8G8B8A8_PREMULTIPLIED]           = { GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,            8, 4, SAMPLEFORMAT_UINT,   EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_A8B8G8R8_PREMULTIPLIED]           = { GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,            8, 4, SAMPLEFORMAT_UINT,   EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_B8G8R8A8]                         = { GDK_MEMORY_R8G8B8A8,                          8, 4, SAMPLEFORMAT_UINT,   EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_A8R8G8B8]                         = { GDK_MEMORY_R8G8B8A8,                          8, 4, SAMPLEFORMAT_UINT,   EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_R8G8B8A8]                         = { GDK_MEMORY_R8G8B8A8,                          8, 4, SAMPLEFORMAT_UINT,   EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_A8B8G8R8]                         = { GDK_MEMORY_R8G8B8A8,                          8, 4, SAMPLEFORMAT_UINT,   EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_R8G8B8]                           = { GDK_MEMORY_R8G8B8,                            8, 3, SAMPLEFORMAT_UINT,   0,                      PHOTOMETRIC_RGB },
  [GDK_MEMORY_B8G8R8]                           = { GDK_MEMORY_R8G8B8,                            8, 3, SAMPLEFORMAT_UINT,   0,                      PHOTOMETRIC_RGB },
  [GDK_MEMORY_R8G8B8X8]                         = { GDK_MEMORY_R8G8B8,                            8, 3, SAMPLEFORMAT_UINT,   0,                      PHOTOMETRIC_RGB },
  [GDK_MEMORY_X8R8G8B8]                         = { GDK_MEMORY_R8G8B8,                            8, 3, SAMPLEFORMAT_UINT,   0,                      PHOTOMETRIC_RGB },
  [GDK_MEMORY_B8G8R8X8]                         = { GDK_MEMORY_R8G8B8,                            8, 3, SAMPLEFORMAT_UINT,   0,                      PHOTOMETRIC_RGB },
  [GDK_MEMORY_X8B8G8R8]                         = { GDK_MEMORY_R8G8B8,                            8, 3, SAMPLEFORMAT_UINT,   0,                      PHOTOMETRIC_RGB },
  [GDK_MEMORY_R16G16B16]                        = { GDK_MEMORY_R16G16B16,                        16, 3, SAMPLEFORMAT_UINT,   0,                      PHOTOMETRIC_RGB },
  [GDK_MEMORY_R16G16B16A16_PREMULTIPLIED]       = { GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,       16, 4, SAMPLEFORMAT_UINT,   EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_R16G16B16A16]                     = { GDK_MEMORY_R16G16B16A16,                     16, 4, SAMPLEFORMAT_UINT,   EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_R16G16B16_FLOAT]                  = { GDK_MEMORY_R16G16B16_FLOAT,                  16, 3, SAMPLEFORMAT_IEEEFP, 0,                      PHOTOMETRIC_RGB },
  [GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED] = { GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED, 16, 4, SAMPLEFORMAT_IEEEFP, EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_R16G16B16A16_FLOAT]               = { GDK_MEMORY_R16G16B16A16_FLOAT,               16, 4, SAMPLEFORMAT_IEEEFP, EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_R32G32B32_FLOAT]                  = { GDK_MEMORY_R32G32B32_FLOAT,                  32, 3, SAMPLEFORMAT_IEEEFP, 0,                      PHOTOMETRIC_RGB },
  [GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED] = { GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED, 32, 4, SAMPLEFORMAT_IEEEFP, EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_R32G32B32A32_FLOAT]               = { GDK_MEMORY_R32G32B32A32_FLOAT,               32, 4, SAMPLEFORMAT_IEEEFP, EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_G8A8_PREMULTIPLIED]               = { GDK_MEMORY_G8A8_PREMULTIPLIED,                8, 2, SAMPLEFORMAT_UINT,   EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_MINISBLACK },
  [GDK_MEMORY_G8A8]                             = { GDK_MEMORY_G8A8,                              8, 2, SAMPLEFORMAT_UINT,   EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_MINISBLACK },
  [GDK_MEMORY_G8]                               = { GDK_MEMORY_G8,                                8, 1, SAMPLEFORMAT_UINT,   0,                      PHOTOMETRIC_MINISBLACK },
  [GDK_MEMORY_G16A16_PREMULTIPLIED]             = { GDK_MEMORY_G16A16_PREMULTIPLIED,             16, 2, SAMPLEFORMAT_UINT,   EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_MINISBLACK },
  [GDK_MEMORY_G16A16]                           = { GDK_MEMORY_G16A16,                           16, 2, SAMPLEFORMAT_UINT,   EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_MINISBLACK },
  [GDK_MEMORY_G16]                              = { GDK_MEMORY_G16,                              16, 1, SAMPLEFORMAT_UINT,   0,                      PHOTOMETRIC_MINISBLACK },
  [GDK_MEMORY_A8]                               = { GDK_MEMORY_G8A8,                              8, 2, SAMPLEFORMAT_UINT,   EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_MINISBLACK },
  [GDK_MEMORY_A16]                              = { GDK_MEMORY_G16A16,                           16, 2, SAMPLEFORMAT_UINT,   EXTRASAMPLE_UNASSALPHA, PHOTOMETRIC_MINISBLACK },
  [GDK_MEMORY_A16_FLOAT]                        = { GDK_MEMORY_R16G16B16A16_FLOAT,               16, 4, SAMPLEFORMAT_IEEEFP, EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_RGB },
  [GDK_MEMORY_A32_FLOAT]                        = { GDK_MEMORY_R32G32B32A32_FLOAT,               32, 4, SAMPLEFORMAT_IEEEFP, EXTRASAMPLE_ASSOCALPHA, PHOTOMETRIC_RGB },
};

/* if this fails, somebody forgot to add formats above */
G_STATIC_ASSERT (G_N_ELEMENTS (format_data) == GDK_MEMORY_N_FORMATS);

GBytes *
gdk_save_tiff (GdkTexture *texture)
{
  TIFF *tif;
  int width, height;
  gsize stride;
  const guchar *line;
  const guchar *data;
  GBytes *bytes, *result = NULL;
  GdkTextureDownloader downloader;
  GdkMemoryFormat format;
  const FormatData *fdata = NULL;

  tif = tiff_open_write (&result);

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  format = gdk_texture_get_format (texture);
  fdata = &format_data[format];

  if (fdata == NULL)
    fdata = &format_data[0];

  TIFFSetField (tif, TIFFTAG_SOFTWARE, "GTK");
  TIFFSetField (tif, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField (tif, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField (tif, TIFFTAG_BITSPERSAMPLE, fdata->bits_per_sample);
  TIFFSetField (tif, TIFFTAG_SAMPLESPERPIXEL, fdata->samples_per_pixel);
  TIFFSetField (tif, TIFFTAG_SAMPLEFORMAT, fdata->sample_format);
  TIFFSetField (tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField (tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
  if (fdata->alpha_samples)
    TIFFSetField (tif, TIFFTAG_EXTRASAMPLES, 1, &fdata->alpha_samples);

  TIFFSetField (tif, TIFFTAG_PHOTOMETRIC, fdata->photometric);
  TIFFSetField (tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

  gdk_texture_downloader_init (&downloader, texture);
  gdk_texture_downloader_set_format (&downloader, fdata->format);
  bytes = gdk_texture_downloader_download_bytes (&downloader, &stride);
  gdk_texture_downloader_finish (&downloader);
  data = g_bytes_get_data (bytes, NULL);

  line = (const guchar *)data;
  for (int y = 0; y < height; y++)
    {
      if (TIFFWriteScanline (tif, (void *)line, y, 0) == -1)
        {
          TIFFClose (tif);
          g_bytes_unref (bytes);
          return NULL;
        }

      line += stride;
    }

  TIFFFlushData (tif);
  TIFFClose (tif);

  g_assert (result);

  g_bytes_unref (bytes);

  return result;
}

static GdkTexture *
load_fallback (TIFF    *tif,
               GError **error)
{
  int width, height;
  guchar *data;
  GBytes *bytes;
  GdkTexture *texture;

  TIFFGetField (tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &height);

  data = g_malloc (width * height * 4);

  if (!TIFFReadRGBAImageOriented (tif, width, height, (guint32 *)data, ORIENTATION_TOPLEFT, 1))
    {
      g_set_error_literal (error,
                           GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                           _("Failed to load RGB data from TIFF file"));
      g_free (data);
      return NULL;
    }

  bytes = g_bytes_new_take (data, width * height * 4);

  texture = gdk_memory_texture_new (width, height,
                                    GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                                    bytes,
                                    width * 4);

  g_bytes_unref (bytes);

  return texture;
}

GdkTexture *
gdk_load_tiff (GBytes  *input_bytes,
               GError **error)
{
  TIFF *tif;
  guint16 samples_per_pixel;
  guint16 bits_per_sample;
  guint16 photometric;
  guint16 planarconfig;
  guint16 sample_format;
  guint16 orientation;
  guint32 width, height;
  guint16 alpha_samples;
  GdkMemoryFormat format;
  guchar *data, *line;
  gsize stride;
  int bpp;
  GBytes *bytes;
  GdkTexture *texture;
  G_GNUC_UNUSED gint64 before = GDK_PROFILER_CURRENT_TIME;

  tif = tiff_open_read (input_bytes);
  if (!tif)
    {
      g_set_error_literal (error,
                           GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                           _("Could not load TIFF data"));
      return NULL;
    }

  TIFFSetDirectory (tif, 0);

  TIFFGetFieldDefaulted (tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
  TIFFGetFieldDefaulted (tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
  TIFFGetFieldDefaulted (tif, TIFFTAG_SAMPLEFORMAT, &sample_format);
  TIFFGetFieldDefaulted (tif, TIFFTAG_PHOTOMETRIC, &photometric);
  TIFFGetFieldDefaulted (tif, TIFFTAG_PLANARCONFIG, &planarconfig);
  TIFFGetFieldDefaulted (tif, TIFFTAG_ORIENTATION, &orientation);
  TIFFGetFieldDefaulted (tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetFieldDefaulted (tif, TIFFTAG_IMAGELENGTH, &height);

  if (samples_per_pixel == 2 || samples_per_pixel == 4)
    {
      guint16 extra;
      guint16 *extra_types;

      if (TIFFGetField (tif, TIFFTAG_EXTRASAMPLES, &extra, &extra_types))
        alpha_samples = extra_types[0];
      else
        alpha_samples = 0;

      if (alpha_samples != 0 && alpha_samples != EXTRASAMPLE_ASSOCALPHA && alpha_samples != EXTRASAMPLE_UNASSALPHA)
        {
          texture = load_fallback (tif, error);
          TIFFClose (tif);
          return texture;
        }
    }
  else
    alpha_samples = 0;

  for (format = 0; format < G_N_ELEMENTS (format_data); format++)
    {
      /* not a native format */
      if (format_data[format].format != format)
        continue;

      if (format_data[format].sample_format == sample_format &&
          format_data[format].bits_per_sample == bits_per_sample &&
          format_data[format].samples_per_pixel == samples_per_pixel &&
          format_data[format].alpha_samples == alpha_samples &&
          format_data[format].photometric == photometric)
        {
          break;
        }
    }

  if (format == G_N_ELEMENTS(format_data) ||
      (photometric != PHOTOMETRIC_RGB && photometric != PHOTOMETRIC_MINISBLACK) ||
      planarconfig != PLANARCONFIG_CONTIG ||
      TIFFIsTiled (tif) ||
      orientation != ORIENTATION_TOPLEFT)
    {
      texture = load_fallback (tif, error);
      TIFFClose (tif);
      return texture;
    }

  stride = width * gdk_memory_format_bytes_per_pixel (format);

  g_assert (TIFFScanlineSize (tif) == stride);

  data = g_try_malloc_n (height, stride);
  if (!data)
    {
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_TOO_LARGE,
                   _("Not enough memory for image size %ux%u"), width, height);
      TIFFClose (tif);
      return NULL;
    }

  line = data;
  for (int y = 0; y < height; y++)
    {
      if (TIFFReadScanline (tif, line, y, 0) == -1)
        {
          g_set_error (error,
                       GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                       _("Reading data failed at row %d"), y);
          TIFFClose (tif);
          g_free (data);
          return NULL;
        }

      line += stride;
    }

  bpp = gdk_memory_format_bytes_per_pixel (format);
  bytes = g_bytes_new_take (data, width * height * bpp);

  texture = gdk_memory_texture_new (width, height,
                                    format,
                                    bytes, width * bpp);
  g_bytes_unref (bytes);

  TIFFClose (tif);

  if (GDK_PROFILER_IS_RUNNING)
    {
      gint64 end = GDK_PROFILER_CURRENT_TIME;
      if (end - before > 500000)
        gdk_profiler_add_mark (before, end - before, "Load tiff", NULL);
    }

  return texture;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
