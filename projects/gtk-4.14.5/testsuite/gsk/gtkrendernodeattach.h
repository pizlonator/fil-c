#pragma once

#include "gskrendernode.h"
#include "gskoffloadprivate.h"
#include <cairo.h>

#include "gdk/gdkmemoryformatprivate.h"

G_BEGIN_DECLS

typedef struct _GskRenderNodeClass GskRenderNodeClass;

/* Keep this in sync with the GskRenderNodeType enumeration.
 *
 * We don't add an "n-types" value to avoid having to handle
 * it in every single switch.
 */
#define GSK_RENDER_NODE_TYPE_N_TYPES    (GSK_SUBSURFACE_NODE + 1)

extern GType gsk_render_node_types[];

#define GSK_IS_RENDER_NODE_TYPE(node,type) \
  (G_TYPE_INSTANCE_GET_CLASS ((node), GSK_TYPE_RENDER_NODE, GskRenderNodeClass)->node_type == (type))

#define GSK_RENDER_NODE_TYPE(node) \
  (G_TYPE_INSTANCE_GET_CLASS ((node), GSK_TYPE_RENDER_NODE, GskRenderNodeClass)->node_type)

struct _GskRenderNode
{
  GTypeInstance parent_instance;

  gatomicrefcount ref_count;

  graphene_rect_t bounds;

  guint preferred_depth : 2;
  guint offscreen_for_opacity : 1;
};

typedef struct
{
  cairo_region_t *region;
  GskOffload *offload;
} GskDiffData;

struct _GskRenderNodeClass
{
  GTypeClass parent_class;

  GskRenderNodeType node_type;

  void            (* finalize)    (GskRenderNode  *node);
  void            (* draw)        (GskRenderNode  *node,
                                   cairo_t        *cr);
  gboolean        (* can_diff)    (const GskRenderNode  *node1,
                                   const GskRenderNode  *node2);
  void            (* diff)        (GskRenderNode  *node1,
                                   GskRenderNode  *node2,
                                   GskDiffData    *data);
};

void            gsk_render_node_init_types              (void);

GType           gsk_render_node_type_register_static    (const char                  *node_name,
                                                         gsize                        instance_size,
                                                         GClassInitFunc               class_init);

gpointer        gsk_render_node_alloc                   (GskRenderNodeType            node_type);

void            _gsk_render_node_unref                  (GskRenderNode               *node);

gboolean        gsk_render_node_can_diff                (const GskRenderNode         *node1,
                                                         const GskRenderNode         *node2) G_GNUC_PURE;
void            gsk_render_node_diff                    (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         GskDiffData                 *data);
void            gsk_render_node_diff_impossible         (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         GskDiffData                 *data);
void            gsk_container_node_diff_with            (GskRenderNode               *container,
                                                         GskRenderNode               *other,
                                                         GskDiffData                 *data);

bool            gsk_border_node_get_uniform             (const GskRenderNode         *self);
bool            gsk_border_node_get_uniform_color       (const GskRenderNode         *self);

void            gsk_text_node_serialize_glyphs          (GskRenderNode               *self,
                                                         GString                     *str);

GskRenderNode ** gsk_container_node_get_children        (const GskRenderNode         *node,
                                                         guint                       *n_children);

void            gsk_transform_node_get_translate        (const GskRenderNode         *node,
                                                         float                       *dx,
                                                         float                       *dy);
GdkMemoryDepth  gsk_render_node_get_preferred_depth     (const GskRenderNode         *node);

gboolean        gsk_container_node_is_disjoint          (const GskRenderNode         *node);

gboolean        gsk_render_node_use_offscreen_for_opacity (const GskRenderNode       *node);

GskRenderNode * gsk_render_node_attach                    (const GskRenderNode       *node,
                                                           GdkSurface                *surface);

#define gsk_render_node_ref(node)   _gsk_render_node_ref(node)
#define gsk_render_node_unref(node) _gsk_render_node_unref(node)

static inline GskRenderNode *
_gsk_render_node_ref (GskRenderNode *node)
{
  g_atomic_ref_count_inc (&node->ref_count);
  return node;
}

G_END_DECLS

