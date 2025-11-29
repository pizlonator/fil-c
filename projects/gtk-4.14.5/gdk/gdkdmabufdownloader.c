#include "config.h"

#include "gdkdmabufdownloaderprivate.h"

G_DEFINE_INTERFACE (GdkDmabufDownloader, gdk_dmabuf_downloader, G_TYPE_OBJECT)

static void
gdk_dmabuf_downloader_default_init (GdkDmabufDownloaderInterface *iface)
{
}

void
gdk_dmabuf_downloader_close (GdkDmabufDownloader *self)
{
  GdkDmabufDownloaderInterface *iface;

  iface = GDK_DMABUF_DOWNLOADER_GET_IFACE (self);
  iface->close (self);
}

gboolean
gdk_dmabuf_downloader_supports (GdkDmabufDownloader  *self,
                                GdkDmabufTexture     *texture,
                                GError              **error)
{
  GdkDmabufDownloaderInterface *iface;

  g_return_val_if_fail (GDK_IS_DMABUF_DOWNLOADER (self), FALSE);

  iface = GDK_DMABUF_DOWNLOADER_GET_IFACE (self);
  return iface->supports (self, texture, error);
}

void
gdk_dmabuf_downloader_download (GdkDmabufDownloader *self,
                                GdkDmabufTexture    *texture,
                                GdkMemoryFormat      format,
                                guchar              *data,
                                gsize                stride)
{
  GdkDmabufDownloaderInterface *iface;

  g_return_if_fail (GDK_IS_DMABUF_DOWNLOADER (self));

  iface = GDK_DMABUF_DOWNLOADER_GET_IFACE (self);
  iface->download (self, texture, format, data, stride);
}

