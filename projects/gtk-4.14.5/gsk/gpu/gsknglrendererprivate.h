#pragma once

#include "gskgpurendererprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_NGL_RENDERER (gsk_ngl_renderer_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GskNglRenderer, gsk_ngl_renderer, GSK, NGL_RENDERER, GskGpuRenderer)

GDK_AVAILABLE_IN_ALL
GskRenderer *gsk_ngl_renderer_new      (void);

G_END_DECLS

