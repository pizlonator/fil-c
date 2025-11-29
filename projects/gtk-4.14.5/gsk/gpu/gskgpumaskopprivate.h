#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_mask_op                                 (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GskGpuDescriptors              *desc,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         float                           opacity,
                                                                         GskMaskMode                     mask_mode,
                                                                         guint32                         source_descriptor,
                                                                         const graphene_rect_t          *source_rect,
                                                                         guint32                         mask_descriptor,
                                                                         const graphene_rect_t          *mask_rect);


G_END_DECLS

