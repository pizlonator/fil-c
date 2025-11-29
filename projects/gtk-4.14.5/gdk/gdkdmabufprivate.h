#pragma once

#include "gdkdmabufformatsbuilderprivate.h"

#ifdef GDK_RENDERING_VULKAN
#include <vulkan/vulkan.h>
#endif

#define GDK_DMABUF_MAX_PLANES 4

typedef struct _GdkDmabuf GdkDmabuf;

struct _GdkDmabuf
{
  guint32 fourcc;
  guint64 modifier;
  unsigned int n_planes;
  struct {
    int fd;
    unsigned int stride;
    unsigned int offset;
  } planes[GDK_DMABUF_MAX_PLANES];
};

#ifdef HAVE_DMABUF

GdkDmabufFormats *          gdk_dmabuf_get_mmap_formats         (void) G_GNUC_CONST;
void                        gdk_dmabuf_download_mmap            (GdkTexture                     *texture,
                                                                 GdkMemoryFormat                 format,
                                                                 guchar                         *data,
                                                                 gsize                           stride);


int                         gdk_dmabuf_ioctl                    (int                             fd,
                                                                 unsigned long                   request,
                                                                 void                           *arg);
gboolean                    gdk_dmabuf_import_sync_file         (int                             dmabuf_fd,
                                                                 guint32                         flags,
                                                                 int                             sync_file_fd);
int                         gdk_dmabuf_export_sync_file         (int                             dmabuf_fd,
                                                                 guint32                         flags);

gboolean                    gdk_dmabuf_sanitize                 (GdkDmabuf                      *dest,
                                                                 gsize                           width,
                                                                 gsize                           height,
                                                                 const GdkDmabuf                *src,
                                                                 GError                        **error);

gboolean                    gdk_dmabuf_is_disjoint              (const GdkDmabuf                *dmabuf);

gboolean                    gdk_dmabuf_fourcc_is_yuv            (guint32                         fourcc,
                                                                 gboolean                       *is_yuv);
gboolean                    gdk_dmabuf_get_memory_format        (guint32                         fourcc,
                                                                 gboolean                        premultiplied,
                                                                 GdkMemoryFormat                *out_format);
#ifdef GDK_RENDERING_VULKAN
gboolean                    gdk_dmabuf_vk_get_nth               (gsize                           n,
                                                                 guint32                        *fourcc,
                                                                 VkFormat                       *vk_format);
VkFormat                    gdk_dmabuf_get_vk_format            (guint32                         fourcc,
                                                                 VkComponentMapping             *out_components);
#endif

#endif
