#pragma once

#include "gskgpushaderopprivate.h"

#include "gskrendernode.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_linear_gradient_op                      (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         gboolean                        repeating,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *start,
                                                                         const graphene_point_t         *end,
                                                                         const graphene_point_t         *offset,
                                                                         const GskColorStop             *stops,
                                                                         gsize                           n_stops);


G_END_DECLS

