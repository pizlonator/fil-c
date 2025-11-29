#pragma once

#include "gskgpuopprivate.h"

G_BEGIN_DECLS

typedef void            (* GskGpuDownloadFunc)                          (gpointer                        user_data,
                                                                         GdkTexture                     *texture);

void                    gsk_gpu_download_op                             (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *image,
                                                                         gboolean                        allow_dmabuf,
                                                                         GskGpuDownloadFunc              func,
                                                                         gpointer                        user_data);

void                    gsk_gpu_download_png_op                         (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *image,
                                                                         const char                     *filename_format,
                                                                         ...) G_GNUC_PRINTF(3, 4);

G_END_DECLS

