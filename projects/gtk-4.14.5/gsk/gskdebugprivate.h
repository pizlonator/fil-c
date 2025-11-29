#pragma once

#include <glib.h>
#include "gdk/gdkdebugprivate.h"

G_BEGIN_DECLS

typedef enum {
  GSK_DEBUG_RENDERER              = 1 <<  0,
  GSK_DEBUG_SHADERS               = 1 <<  2,
  GSK_DEBUG_VULKAN                = 1 <<  3,
  GSK_DEBUG_FALLBACK              = 1 <<  4,
  GSK_DEBUG_GLYPH_CACHE           = 1 <<  5,
  GSK_DEBUG_VERBOSE               = 1 <<  6,
  /* flags below may affect behavior */
  GSK_DEBUG_GEOMETRY              = 1 <<  7,
  GSK_DEBUG_FULL_REDRAW           = 1 <<  8,
  GSK_DEBUG_STAGING               = 1 << 10,
  GSK_DEBUG_OFFLOAD_DISABLE       = 1 << 11,
  GSK_DEBUG_CAIRO                 = 1 << 12,
} GskDebugFlags;

#define GSK_DEBUG_ANY ((1 << 13) - 1)

GskDebugFlags gsk_get_debug_flags (void);
void          gsk_set_debug_flags (GskDebugFlags flags);

gboolean gsk_check_debug_flags (GskDebugFlags flags);

#define GSK_DEBUG_CHECK(type)           G_UNLIKELY (gsk_check_debug_flags (GSK_DEBUG_ ## type))
#define GSK_RENDERER_DEBUG_CHECK(renderer,type) \
  G_UNLIKELY ((gsk_renderer_get_debug_flags (renderer) & GSK_DEBUG_ ## type) != 0)

#define GSK_RENDERER_DEBUG(renderer,type,...)                               \
    G_STMT_START {                                                          \
    if (GSK_RENDERER_DEBUG_CHECK (renderer,type))                           \
      gdk_debug_message (__VA_ARGS__);                                      \
    } G_STMT_END

#define GSK_DEBUG(type,...)                                                 \
    G_STMT_START {                                                          \
    if (GSK_DEBUG_CHECK (type))                                             \
      gdk_debug_message (__VA_ARGS__);                                      \
    } G_STMT_END

G_END_DECLS

