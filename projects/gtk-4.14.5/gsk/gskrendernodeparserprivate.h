
#pragma once

#include "gskrendernode.h"

GskRenderNode * gsk_render_node_deserialize_from_bytes  (GBytes            *bytes,
                                                         GskParseErrorFunc  error_func,
                                                         gpointer           user_data);
