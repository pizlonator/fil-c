#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_uber_op                                 (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         GskGpuDescriptors              *desc,
                                                                         guint32                         pattern_id);


G_END_DECLS

