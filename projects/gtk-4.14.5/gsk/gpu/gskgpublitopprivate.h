#pragma once

#include "gskgpuopprivate.h"

G_BEGIN_DECLS

typedef enum {
  GSK_GPU_BLIT_NEAREST,
  GSK_GPU_BLIT_LINEAR
} GskGpuBlitFilter;

void                    gsk_gpu_blit_op                                 (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *src_image,
                                                                         GskGpuImage                    *dest_image,
                                                                         const cairo_rectangle_int_t    *src_rect,
                                                                         const cairo_rectangle_int_t    *dest_rect,
                                                                         GskGpuBlitFilter                filter);

G_END_DECLS

