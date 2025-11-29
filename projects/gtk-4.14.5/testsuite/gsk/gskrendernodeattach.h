#pragma once

#include "gsk/gsk.h"

GskRenderNode * gsk_render_node_attach (const GskRenderNode *node,
                                        GdkSurface          *surface);
