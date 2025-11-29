#pragma once

#include "gskgputypesprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_color_op                                (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const GdkRGBA                  *color);


G_END_DECLS

