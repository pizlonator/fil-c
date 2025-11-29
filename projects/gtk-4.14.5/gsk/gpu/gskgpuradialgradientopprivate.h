#pragma once

#include "gskgpushaderopprivate.h"

#include "gskrendernode.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_radial_gradient_op                      (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         gboolean                        repeating,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *center,
                                                                         const graphene_point_t         *radius,
                                                                         float                           start,
                                                                         float                           end,
                                                                         const graphene_point_t         *offset,
                                                                         const GskColorStop             *stops,
                                                                         gsize                           n_stops);


G_END_DECLS

