#pragma once

#include "gskgputypesprivate.h"

#include "gsktypes.h"

#include <graphene.h>

G_BEGIN_DECLS

/* We only need this for the final VkImageLayout, but don't tell anyone */
typedef enum
{
  GSK_RENDER_PASS_OFFSCREEN,
  GSK_RENDER_PASS_PRESENT
} GskRenderPassType;

void                    gsk_gpu_render_pass_begin_op                    (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *image,
                                                                         const cairo_rectangle_int_t    *area,
                                                                         GskRenderPassType               pass_type);
void                    gsk_gpu_render_pass_end_op                      (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *image,
                                                                         GskRenderPassType               pass_type);

G_END_DECLS

