#pragma once

#include "gskgputypesprivate.h"

G_BEGIN_DECLS

void                    gsk_gpu_clear_op                                (GskGpuFrame                    *frame,
                                                                         const cairo_rectangle_int_t    *rect,
                                                                         const GdkRGBA                  *color);


G_END_DECLS

