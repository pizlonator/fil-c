#pragma once

#include "gskgputypesprivate.h"
#include "gsktypes.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_rounded_color_op                               (GskGpuFrame                    *frame,
                                                                                GskGpuShaderClip                clip,
                                                                                const GskRoundedRect           *outline,
                                                                                const graphene_point_t         *offset,
                                                                                const GdkRGBA                  *color);


G_END_DECLS

