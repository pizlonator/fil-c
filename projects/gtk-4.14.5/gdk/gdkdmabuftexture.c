/* gdkdmabuftexture.c
 *
 * Copyright 2023  Red Hat, Inc.
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

#include "gdkdmabuftextureprivate.h"

#include "gdkdisplayprivate.h"
#include "gdkdmabufdownloaderprivate.h"
#include "gdkdmabufformatsbuilderprivate.h"
#include "gdkdmabuffourccprivate.h"
#include "gdkdmabufprivate.h"
#include "gdktextureprivate.h"
#include <gdk/gdkglcontext.h>
#include <gdk/gdkgltexturebuilder.h>
#include <gdk/gdktexturedownloader.h>

/**
 * GdkDmabufTexture:
 *
 * A `GdkTexture` representing a DMA buffer.
 *
 * To create a `GdkDmabufTexture`, use the auxiliary
 * [class@Gdk.DmabufTextureBuilder] object.
 *
 * Dma-buf textures can only be created on Linux.
 *
 * Since: 4.14
 */

struct _GdkDmabufTexture
{
  GdkTexture parent_instance;

  GdkDisplay *display;
  GdkDmabufDownloader *downloader;

  GdkDmabuf dmabuf;

  GDestroyNotify destroy;
  gpointer data;
};

struct _GdkDmabufTextureClass
{
  GdkTextureClass parent_class;
};

G_DEFINE_QUARK (gdk-dmabuf-error-quark, gdk_dmabuf_error)

G_DEFINE_TYPE (GdkDmabufTexture, gdk_dmabuf_texture, GDK_TYPE_TEXTURE)

static void
gdk_dmabuf_texture_dispose (GObject *object)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (object);

  if (self->destroy)
    {
      self->destroy (self->data);
      self->destroy = NULL;
    }

  g_clear_object (&self->downloader);
  g_clear_object (&self->display);

  G_OBJECT_CLASS (gdk_dmabuf_texture_parent_class)->dispose (object);
}

typedef struct _Download Download;

struct _Download
{
  GdkDmabufTexture *texture;
  GdkMemoryFormat format;
  guchar *data;
  gsize stride;
  volatile int spinlock;
};

static gboolean
gdk_dmabuf_texture_invoke_callback (gpointer data)
{
  Download *download = data;

  gdk_dmabuf_downloader_download (download->texture->downloader,
                                  download->texture,
                                  download->format,
                                  download->data,
                                  download->stride);

  g_atomic_int_set (&download->spinlock, 1);

  return FALSE;
}

static void
gdk_dmabuf_texture_download (GdkTexture      *texture,
                             GdkMemoryFormat  format,
                             guchar          *data,
                             gsize            stride)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (texture);
  Download download = { self, format, data, stride, 0 };

  if (self->downloader == NULL)
    {
#ifdef HAVE_DMABUF
      gdk_dmabuf_download_mmap (texture, format, data, stride);
#endif
      return;
    }

  g_main_context_invoke (NULL, gdk_dmabuf_texture_invoke_callback, &download);

  while (g_atomic_int_get (&download.spinlock) == 0);
}

static void
gdk_dmabuf_texture_class_init (GdkDmabufTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_dmabuf_texture_download;

  gobject_class->dispose = gdk_dmabuf_texture_dispose;
}

static void
gdk_dmabuf_texture_init (GdkDmabufTexture *self)
{
}

GdkDisplay *
gdk_dmabuf_texture_get_display (GdkDmabufTexture *self)
{
  return self->display;
}

const GdkDmabuf *
gdk_dmabuf_texture_get_dmabuf (GdkDmabufTexture *self)
{
  return &self->dmabuf;
}

GdkTexture *
gdk_dmabuf_texture_new_from_builder (GdkDmabufTextureBuilder *builder,
                                     GDestroyNotify           destroy,
                                     gpointer                 data,
                                     GError                 **error)
{
#ifdef HAVE_DMABUF
  GdkDmabufTexture *self;
  GdkTexture *update_texture;
  GdkDisplay *display;
  GdkDmabuf dmabuf;
  GError *local_error = NULL;
  int width, height;
  gboolean premultiplied;
  gsize i;

  display = gdk_dmabuf_texture_builder_get_display (builder);
  width = gdk_dmabuf_texture_builder_get_width (builder);
  height = gdk_dmabuf_texture_builder_get_height (builder);
  premultiplied = gdk_dmabuf_texture_builder_get_premultiplied (builder);

  if (!gdk_dmabuf_sanitize (&dmabuf,
                            width,
                            height,
                            gdk_dmabuf_texture_builder_get_dmabuf (builder),
                            error))
    return NULL;

  gdk_display_init_dmabuf (display);

  self = g_object_new (GDK_TYPE_DMABUF_TEXTURE,
                       "width", width,
                       "height", height,
                       NULL);

  g_set_object (&self->display, display);
  self->dmabuf = dmabuf;

  if (!gdk_dmabuf_get_memory_format (dmabuf.fourcc, premultiplied, &(GDK_TEXTURE (self)->format)))
    {
      GDK_DISPLAY_DEBUG (display, DMABUF,
                         "Falling back to generic RGBA for dmabuf format %.4s",
                         (char *) &dmabuf.fourcc);
      GDK_TEXTURE (self)->format = premultiplied ? GDK_MEMORY_R8G8B8A8_PREMULTIPLIED
                                                 : GDK_MEMORY_R8G8B8A8;
    }

  if (!gdk_dmabuf_formats_contains (gdk_dmabuf_get_mmap_formats (), dmabuf.fourcc, dmabuf.modifier))
    {
      for (i = 0; display->dmabuf_downloaders[i] != NULL; i++)
        {
          if (local_error && g_error_matches (local_error, GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT))
            g_clear_error (&local_error);

          if (gdk_dmabuf_downloader_supports (display->dmabuf_downloaders[i],
                                              self,
                                              local_error ? NULL : &local_error))
            {
              self->downloader = g_object_ref (display->dmabuf_downloaders[i]);
              break;
            }
        }

      if (self->downloader == NULL)
        {
          g_propagate_error (error, local_error);
          g_object_unref (self);
          return NULL;
        }
    }

  GDK_DISPLAY_DEBUG (display, DMABUF,
                     "Creating dmabuf texture, format %.4s:%#" G_GINT64_MODIFIER "x, %s%u planes, memory format %u, downloader %s",
                     (char *) &dmabuf.fourcc, dmabuf.modifier,
                     gdk_dmabuf_texture_builder_get_premultiplied (builder) ? " premultiplied, " : "",
                     dmabuf.n_planes,
                     GDK_TEXTURE (self)->format,
                     self->downloader ? G_OBJECT_TYPE_NAME (self->downloader) : "none");

  /* Set this only once we know that the texture will be created.
   * Otherwise dispose() will run the callback */
  self->destroy = destroy;
  self->data = data;

  update_texture = gdk_dmabuf_texture_builder_get_update_texture (builder);
  if (update_texture)
    {
      cairo_region_t *update_region = gdk_dmabuf_texture_builder_get_update_region (builder);
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

#else /* !HAVE_DMABUF */
  g_set_error_literal (error, GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_NOT_AVAILABLE,
                       "dmabuf support disabled at compile-time.");
  return NULL;
#endif
}

