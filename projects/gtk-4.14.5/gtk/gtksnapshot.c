/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtksnapshot.h"
#include "gtksnapshotprivate.h"

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtkdebug.h"
#include "gtkrendernodepaintableprivate.h"
#include "gsktransformprivate.h"

#include "gdk/gdkrgbaprivate.h"

#include "gsk/gskrendernodeprivate.h"
#include "gsk/gskroundedrectprivate.h"
#include "gsk/gskstrokeprivate.h"

#include "gtk/gskpangoprivate.h"

#define GDK_ARRAY_NAME gtk_snapshot_nodes
#define GDK_ARRAY_TYPE_NAME GtkSnapshotNodes
#define GDK_ARRAY_ELEMENT_TYPE GskRenderNode *
#define GDK_ARRAY_FREE_FUNC gsk_render_node_unref
#include "gdk/gdkarrayimpl.c"

/**
 * GtkSnapshot:
 *
 * `GtkSnapshot` assists in creating [class@Gsk.RenderNode]s for widgets.
 *
 * It functions in a similar way to a cairo context, and maintains a stack
 * of render nodes and their associated transformations.
 *
 * The node at the top of the stack is the one that `gtk_snapshot_append_…()`
 * functions operate on. Use the `gtk_snapshot_push_…()` functions and
 * [method@Snapshot.pop] to change the current node.
 *
 * The typical way to obtain a `GtkSnapshot` object is as an argument to
 * the [vfunc@Gtk.Widget.snapshot] vfunc. If you need to create your own
 * `GtkSnapshot`, use [ctor@Gtk.Snapshot.new].
 */

typedef struct _GtkSnapshotState GtkSnapshotState;

typedef GskRenderNode * (* GtkSnapshotCollectFunc) (GtkSnapshot      *snapshot,
                                                    GtkSnapshotState *state,
                                                    GskRenderNode   **nodes,
                                                    guint             n_nodes);
typedef void            (* GtkSnapshotClearFunc)   (GtkSnapshotState *state);

struct _GtkSnapshotState {
  guint                  start_node_index;
  guint                  n_nodes;

  GskTransform *         transform;

  GtkSnapshotCollectFunc collect_func;
  GtkSnapshotClearFunc   clear_func;
  union {
    struct {
      double             opacity;
    } opacity;
    struct {
      double             radius;
    } blur;
    struct {
      graphene_matrix_t matrix;
      graphene_vec4_t offset;
    } color_matrix;
    struct {
      graphene_rect_t bounds;
      graphene_rect_t child_bounds;
    } repeat;
    struct {
      graphene_rect_t bounds;
    } clip;
    struct {
      GskGLShader *shader;
      GBytes *args;
      graphene_rect_t bounds;
      GskRenderNode **nodes;
      GskRenderNode *internal_nodes[4];
    } glshader;
    struct {
      graphene_rect_t bounds;
      int node_idx;
      int n_children;
    } glshader_texture;
    struct {
      GskRoundedRect bounds;
    } rounded_clip;
    struct {
      GskPath *path;
      GskFillRule fill_rule;
    } fill;
    struct {
      GskPath *path;
      GskStroke stroke;
    } stroke;
    struct {
      gsize n_shadows;
      GskShadow *shadows;
      GskShadow a_shadow; /* Used if n_shadows == 1 */
    } shadow;
    struct {
      GskBlendMode blend_mode;
      GskRenderNode *bottom_node;
    } blend;
    struct {
      double progress;
      GskRenderNode *start_node;
    } cross_fade;
    struct {
      char *message;
    } debug;
    struct {
      GskMaskMode mask_mode;
      GskRenderNode *mask_node;
    } mask;
    struct {
      GdkSubsurface *subsurface;
    } subsurface;
  } data;
};

static void gtk_snapshot_state_clear (GtkSnapshotState *state);

#define GDK_ARRAY_NAME gtk_snapshot_states
#define GDK_ARRAY_TYPE_NAME GtkSnapshotStates
#define GDK_ARRAY_ELEMENT_TYPE GtkSnapshotState
#define GDK_ARRAY_FREE_FUNC gtk_snapshot_state_clear
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

/* This is a nasty little hack. We typedef GtkSnapshot to the fake object GdkSnapshot
 * so that we don't need to typecast between them.
 * After all, the GdkSnapshot only exist so poor language bindings don't trip. Hardcore
 * C code can just blatantly ignore such layering violations with a typedef.
 */
struct _GdkSnapshot {
  GObject                parent_instance; /* it's really GdkSnapshot, but don't tell anyone! */

  GtkSnapshotStates      state_stack;
  GtkSnapshotNodes       nodes;
};

struct _GtkSnapshotClass {
  GObjectClass           parent_class; /* it's really GdkSnapshotClass, but don't tell anyone! */
};

G_DEFINE_TYPE (GtkSnapshot, gtk_snapshot, GDK_TYPE_SNAPSHOT)

static void
gtk_snapshot_dispose (GObject *object)
{
  GtkSnapshot *snapshot = GTK_SNAPSHOT (object);

  if (!gtk_snapshot_states_is_empty (&snapshot->state_stack))
    {
      GskRenderNode *node = gtk_snapshot_to_node (snapshot);
      g_clear_pointer (&node, gsk_render_node_unref);
    }

  g_assert (gtk_snapshot_states_is_empty (&snapshot->state_stack));
  g_assert (gtk_snapshot_nodes_is_empty (&snapshot->nodes));

  G_OBJECT_CLASS (gtk_snapshot_parent_class)->dispose (object);
}

static void
gtk_snapshot_class_init (GtkSnapshotClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_snapshot_dispose;
}

static GskRenderNode *
gtk_snapshot_collect_default (GtkSnapshot       *snapshot,
                              GtkSnapshotState  *state,
                              GskRenderNode    **nodes,
                              guint              n_nodes)
{
  GskRenderNode *node;

  if (n_nodes == 0)
    {
      node = NULL;
    }
  else if (n_nodes == 1)
    {
      node = gsk_render_node_ref (nodes[0]);
    }
  else
    {
      node = gsk_container_node_new (nodes, n_nodes);
    }

  return node;
}

static GtkSnapshotState *
gtk_snapshot_push_state (GtkSnapshot            *snapshot,
                         GskTransform           *transform,
                         GtkSnapshotCollectFunc  collect_func,
                         GtkSnapshotClearFunc    clear_func)
{
  const gsize n_states = gtk_snapshot_states_get_size (&snapshot->state_stack);
  GtkSnapshotState *state;

  gtk_snapshot_states_set_size (&snapshot->state_stack, n_states + 1);
  state = gtk_snapshot_states_get (&snapshot->state_stack, n_states);

  state->transform = gsk_transform_ref (transform);
  state->collect_func = collect_func;
  state->clear_func = clear_func;
  state->start_node_index = gtk_snapshot_nodes_get_size (&snapshot->nodes);
  state->n_nodes = 0;

  return state;
}

static GtkSnapshotState *
gtk_snapshot_get_current_state (const GtkSnapshot *snapshot)
{
  gsize size = gtk_snapshot_states_get_size (&snapshot->state_stack);

  g_assert (size > 0);

  return gtk_snapshot_states_get (&snapshot->state_stack, size - 1);
}

static GtkSnapshotState *
gtk_snapshot_get_previous_state (const GtkSnapshot *snapshot)
{
  gsize size = gtk_snapshot_states_get_size (&snapshot->state_stack);

  g_assert (size > 1);

  return gtk_snapshot_states_get (&snapshot->state_stack, size - 2);
}

/* n == 0 => current, n == 1, previous, etc */
static GtkSnapshotState *
gtk_snapshot_get_nth_previous_state (const GtkSnapshot *snapshot,
                                     int n)
{
  gsize size = gtk_snapshot_states_get_size (&snapshot->state_stack);

  g_assert (size > n);

  return gtk_snapshot_states_get (&snapshot->state_stack, size - (1 + n));
}

static void
gtk_snapshot_state_clear (GtkSnapshotState *state)
{
  if (state->clear_func)
    state->clear_func (state);

  gsk_transform_unref (state->transform);
}

static void
gtk_snapshot_init (GtkSnapshot *self)
{
  gtk_snapshot_states_init (&self->state_stack);
  gtk_snapshot_nodes_init (&self->nodes);

  gtk_snapshot_push_state (self,
                           NULL,
                           gtk_snapshot_collect_default,
                           NULL);
}

/**
 * gtk_snapshot_new:
 *
 * Creates a new `GtkSnapshot`.
 *
 * Returns: a newly-allocated `GtkSnapshot`
 */
GtkSnapshot *
gtk_snapshot_new (void)
{
  return g_object_new (GTK_TYPE_SNAPSHOT, NULL);
}

/**
 * gtk_snapshot_free_to_node: (skip)
 * @snapshot: (transfer full): a `GtkSnapshot`
 *
 * Returns the node that was constructed by @snapshot
 * and frees @snapshot.
 *
 * See also [method@Gtk.Snapshot.to_node].
 *
 * Returns: (transfer full) (nullable): a newly-created [class@Gsk.RenderNode]
 */
GskRenderNode *
gtk_snapshot_free_to_node (GtkSnapshot *snapshot)
{
  GskRenderNode *result;

  result = gtk_snapshot_to_node (snapshot);
  g_object_unref (snapshot);

  return result;
}

/**
 * gtk_snapshot_free_to_paintable: (skip)
 * @snapshot: (transfer full): a `GtkSnapshot`
 * @size: (nullable): The size of the resulting paintable
 *   or %NULL to use the bounds of the snapshot
 *
 * Returns a paintable for the node that was
 * constructed by @snapshot and frees @snapshot.
 *
 * Returns: (transfer full) (nullable): a newly-created [iface@Gdk.Paintable]
 */
GdkPaintable *
gtk_snapshot_free_to_paintable (GtkSnapshot           *snapshot,
                                const graphene_size_t *size)
{
  GdkPaintable *result;

  result = gtk_snapshot_to_paintable (snapshot, size);
  g_object_unref (snapshot);

  return result;
}

static GskRenderNode *
gtk_snapshot_collect_autopush_transform (GtkSnapshot      *snapshot,
                                         GtkSnapshotState *state,
                                         GskRenderNode   **nodes,
                                         guint             n_nodes)
{
  GskRenderNode *node, *transform_node;
  GtkSnapshotState *previous_state;

  previous_state = gtk_snapshot_get_previous_state (snapshot);

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  transform_node = gsk_transform_node_new (node, previous_state->transform);

  gsk_render_node_unref (node);

  return transform_node;
}

static void
gtk_snapshot_autopush_transform (GtkSnapshot *snapshot)
{
  gtk_snapshot_push_state (snapshot,
                           NULL,
                           gtk_snapshot_collect_autopush_transform,
                           NULL);
}

static gboolean
gtk_snapshot_state_should_autopop (const GtkSnapshotState *state)
{
  return state->collect_func == gtk_snapshot_collect_autopush_transform;
}

static GskRenderNode *
gtk_snapshot_collect_debug (GtkSnapshot      *snapshot,
                            GtkSnapshotState *state,
                            GskRenderNode   **nodes,
                            guint             n_nodes)
{
  GskRenderNode *node, *debug_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  debug_node = gsk_debug_node_new (node, state->data.debug.message);
  state->data.debug.message = NULL;

  gsk_render_node_unref (node);

  return debug_node;
}

static void
gtk_snapshot_clear_debug (GtkSnapshotState *state)
{
  g_clear_pointer (&state->data.debug.message, g_free);
}

/**
 * gtk_snapshot_push_debug:
 * @snapshot: a `GtkSnapshot`
 * @message: a printf-style format string
 * @...: arguments for @message
 *
 * Inserts a debug node with a message.
 *
 * Debug nodes don't affect the rendering at all, but can be
 * helpful in identifying parts of a render node tree dump,
 * for example in the GTK inspector.
 */
void
gtk_snapshot_push_debug (GtkSnapshot *snapshot,
                         const char  *message,
                         ...)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);

  if (GTK_DEBUG_CHECK (SNAPSHOT))
    {
      va_list args;
      GtkSnapshotState *state;

      state = gtk_snapshot_push_state (snapshot,
                                       current_state->transform,
                                       gtk_snapshot_collect_debug,
                                       gtk_snapshot_clear_debug);



      va_start (args, message);
      state->data.debug.message = g_strdup_vprintf (message, args);
      va_end (args);
    }
  else
    {
      gtk_snapshot_push_state (snapshot,
                               current_state->transform,
                               gtk_snapshot_collect_default,
                               NULL);
    }
}

static GskRenderNode *
gtk_snapshot_collect_opacity (GtkSnapshot      *snapshot,
                              GtkSnapshotState *state,
                              GskRenderNode   **nodes,
                              guint             n_nodes)
{
  GskRenderNode *node, *opacity_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  if (state->data.opacity.opacity == 1.0)
    {
      opacity_node = node;
    }
  else if (state->data.opacity.opacity == 0.0)
    {
      GdkRGBA color = GDK_RGBA ("00000000");
      graphene_rect_t bounds;

      gsk_render_node_get_bounds (node, &bounds);
      opacity_node = gsk_color_node_new (&color, &bounds);
      gsk_render_node_unref (node);
    }
  else
    {
      opacity_node = gsk_opacity_node_new (node, state->data.opacity.opacity);
      gsk_render_node_unref (node);
    }

  return opacity_node;
}

/**
 * gtk_snapshot_push_opacity:
 * @snapshot: a `GtkSnapshot`
 * @opacity: the opacity to use
 *
 * Modifies the opacity of an image.
 *
 * The image is recorded until the next call to [method@Gtk.Snapshot.pop].
 */
void
gtk_snapshot_push_opacity (GtkSnapshot *snapshot,
                           double       opacity)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->transform,
                                   gtk_snapshot_collect_opacity,
                                   NULL);
  state->data.opacity.opacity = CLAMP (opacity, 0.0, 1.0);
}

static GskRenderNode *
gtk_snapshot_collect_blur (GtkSnapshot      *snapshot,
                           GtkSnapshotState *state,
                           GskRenderNode   **nodes,
                           guint             n_nodes)
{
  GskRenderNode *node, *blur_node;
  double radius;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  radius = state->data.blur.radius;

  if (radius == 0.0)
    return node;

  if (radius < 0)
    return node;

  blur_node = gsk_blur_node_new (node, radius);

  gsk_render_node_unref (node);

  return blur_node;
}

/**
 * gtk_snapshot_push_blur:
 * @snapshot: a `GtkSnapshot`
 * @radius: the blur radius to use. Must be positive
 *
 * Blurs an image.
 *
 * The image is recorded until the next call to [method@Gtk.Snapshot.pop].
 */
void
gtk_snapshot_push_blur (GtkSnapshot *snapshot,
                        double       radius)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->transform,
                                   gtk_snapshot_collect_blur,
                                   NULL);
  state->data.blur.radius = radius;
}

static GskRenderNode *
merge_color_matrix_nodes (const graphene_matrix_t *matrix2,
                          const graphene_vec4_t   *offset2,
                          GskRenderNode           *child)
{
  const graphene_matrix_t *matrix1 = gsk_color_matrix_node_get_color_matrix (child);
  const graphene_vec4_t *offset1 = gsk_color_matrix_node_get_color_offset (child);
  graphene_matrix_t matrix;
  graphene_vec4_t offset;
  GskRenderNode *result;

  g_assert (gsk_render_node_get_node_type (child) == GSK_COLOR_MATRIX_NODE);

  /* color matrix node: color = trans(mat) * p + offset; for a pixel p.
   * color =  trans(mat2) * (trans(mat1) * p + offset1) + offset2
   *       =  trans(mat2) * trans(mat1) * p + trans(mat2) * offset1 + offset2
   *       = trans(mat1 * mat2) * p + (trans(mat2) * offset1 + offset2)
   * Which this code does.
   * mat1 and offset1 come from @child.
   */

  graphene_matrix_transform_vec4 (matrix2, offset1, &offset);
  graphene_vec4_add (&offset, offset2, &offset);

  graphene_matrix_multiply (matrix1, matrix2, &matrix);

  result = gsk_color_matrix_node_new (gsk_color_matrix_node_get_child (child),
                                      &matrix, &offset);

  return result;
}

static GskRenderNode *
gtk_snapshot_collect_color_matrix (GtkSnapshot      *snapshot,
                                   GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes)
{
  GskRenderNode *node, *result;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  if (gsk_render_node_get_node_type (node) == GSK_COLOR_MATRIX_NODE)
    {
      result = merge_color_matrix_nodes (&state->data.color_matrix.matrix,
                                         &state->data.color_matrix.offset,
                                         node);
      gsk_render_node_unref (node);
    }
  else if (gsk_render_node_get_node_type (node) == GSK_TRANSFORM_NODE)
    {
      GskRenderNode *transform_child = gsk_transform_node_get_child (node);
      GskRenderNode *color_matrix;

      if (gsk_render_node_get_node_type (transform_child) == GSK_COLOR_MATRIX_NODE)
        {
          color_matrix = merge_color_matrix_nodes (&state->data.color_matrix.matrix,
                                                   &state->data.color_matrix.offset,
                                                   transform_child);
        }
      else
        {
          color_matrix = gsk_color_matrix_node_new (transform_child,
                                                    &state->data.color_matrix.matrix,
                                                    &state->data.color_matrix.offset);
        }
      result = gsk_transform_node_new (color_matrix,
                                       gsk_transform_node_get_transform (node));
      gsk_render_node_unref (color_matrix);
      gsk_render_node_unref (node);
      node = NULL;
    }
  else
    {
      result = gsk_color_matrix_node_new (node,
                                          &state->data.color_matrix.matrix,
                                          &state->data.color_matrix.offset);
      gsk_render_node_unref (node);
    }

  return result;
}

/**
 * gtk_snapshot_push_color_matrix:
 * @snapshot: a `GtkSnapshot`
 * @color_matrix: the color matrix to use
 * @color_offset: the color offset to use
 *
 * Modifies the colors of an image by applying an affine transformation
 * in RGB space.
 *
 * In particular, the colors will be transformed by applying
 *
 *     pixel = transpose(color_matrix) * pixel + color_offset
 *
 * for every pixel. The transformation operates on unpremultiplied
 * colors, with color components ordered R, G, B, A.
 *
 * The image is recorded until the next call to [method@Gtk.Snapshot.pop].
 */
void
gtk_snapshot_push_color_matrix (GtkSnapshot             *snapshot,
                                const graphene_matrix_t *color_matrix,
                                const graphene_vec4_t   *color_offset)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->transform,
                                   gtk_snapshot_collect_color_matrix,
                                   NULL);

  graphene_matrix_init_from_matrix (&state->data.color_matrix.matrix, color_matrix);
  graphene_vec4_init_from_vec4 (&state->data.color_matrix.offset, color_offset);
}

static GskRenderNode *
gtk_snapshot_collect_repeat (GtkSnapshot      *snapshot,
                             GtkSnapshotState *state,
                             GskRenderNode   **nodes,
                             guint             n_nodes)
{
  GskRenderNode *node, *repeat_node;
  const graphene_rect_t *bounds = &state->data.repeat.bounds;
  const graphene_rect_t *child_bounds = &state->data.repeat.child_bounds;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  if (gsk_render_node_get_node_type (node) == GSK_COLOR_NODE &&
      graphene_rect_equal (child_bounds, &node->bounds))
    {
      /* Repeating a color node entirely is pretty easy by just increasing
       * the size of the color node. */
      GskRenderNode *color_node = gsk_color_node_new (gsk_color_node_get_color (node), bounds);

      gsk_render_node_unref (node);

      return color_node;
    }

  repeat_node = gsk_repeat_node_new (bounds,
                                     node,
                                     child_bounds->size.width > 0 ? child_bounds : NULL);

  gsk_render_node_unref (node);

  return repeat_node;
}

static GskRenderNode *
gtk_snapshot_collect_discard_repeat (GtkSnapshot      *snapshot,
                                     GtkSnapshotState *state,
                                     GskRenderNode   **nodes,
                                     guint             n_nodes)
{
  /* Drop the node and return nothing.  */
  return NULL;
}

static void
gtk_graphene_rect_scale_affine (const graphene_rect_t *rect,
                                float                  scale_x,
                                float                  scale_y,
                                float                  dx,
                                float                  dy,
                                graphene_rect_t       *res)
{
  res->origin.x = scale_x * rect->origin.x + dx;
  res->origin.y = scale_y * rect->origin.y + dy;
  res->size.width = scale_x * rect->size.width;
  res->size.height = scale_y * rect->size.height;

  if (scale_x < 0 || scale_y < 0)
    graphene_rect_normalize (res);
}

typedef enum {
  ENSURE_POSITIVE_SCALE = (1 << 0),
  ENSURE_UNIFORM_SCALE = (1 << 1),
} GtkEnsureFlags;

static void
gtk_snapshot_ensure_affine_with_flags (GtkSnapshot    *snapshot,
                                       GtkEnsureFlags  flags,
                                       float          *scale_x,
                                       float          *scale_y,
                                       float          *dx,
                                       float          *dy)
{
  const GtkSnapshotState *state = gtk_snapshot_get_current_state (snapshot);

  if (gsk_transform_get_category (state->transform) < GSK_TRANSFORM_CATEGORY_2D_AFFINE)
    {
      gtk_snapshot_autopush_transform (snapshot);
      state = gtk_snapshot_get_current_state (snapshot);
      gsk_transform_to_affine (state->transform, scale_x, scale_y, dx, dy);
    }
  else if (gsk_transform_get_category (state->transform) == GSK_TRANSFORM_CATEGORY_2D_AFFINE)
    {
      gsk_transform_to_affine (state->transform, scale_x, scale_y, dx, dy);
      if (((flags & ENSURE_POSITIVE_SCALE) && (*scale_x < 0.0 || *scale_y < 0.0)) ||
          ((flags & ENSURE_UNIFORM_SCALE) && (*scale_x != *scale_y)))
        {
          gtk_snapshot_autopush_transform (snapshot);
          state = gtk_snapshot_get_current_state (snapshot);
          gsk_transform_to_affine (state->transform, scale_x, scale_y, dx, dy);
        }
    }
  else
    {
      gsk_transform_to_affine (state->transform, scale_x, scale_y, dx, dy);
    }
}

static void
gtk_snapshot_ensure_affine (GtkSnapshot *snapshot,
                            float       *scale_x,
                            float       *scale_y,
                            float       *dx,
                            float       *dy)
{
  gtk_snapshot_ensure_affine_with_flags (snapshot,
                                         ENSURE_POSITIVE_SCALE,
                                         scale_x, scale_y,
                                         dx, dy);
}

static void
gtk_snapshot_ensure_translate (GtkSnapshot *snapshot,
                               float       *dx,
                               float       *dy)
{
  const GtkSnapshotState *state = gtk_snapshot_get_current_state (snapshot);

  if (gsk_transform_get_category (state->transform) < GSK_TRANSFORM_CATEGORY_2D_TRANSLATE)
    {
      gtk_snapshot_autopush_transform (snapshot);
      state = gtk_snapshot_get_current_state (snapshot);
    }

  gsk_transform_to_translate (state->transform, dx, dy);
}

static void
gtk_snapshot_ensure_identity (GtkSnapshot *snapshot)
{
  const GtkSnapshotState *state = gtk_snapshot_get_current_state (snapshot);

  if (gsk_transform_get_category (state->transform) < GSK_TRANSFORM_CATEGORY_IDENTITY)
    gtk_snapshot_autopush_transform (snapshot);
}

/**
 * gtk_snapshot_push_repeat:
 * @snapshot: a `GtkSnapshot`
 * @bounds: the bounds within which to repeat
 * @child_bounds: (nullable): the bounds of the child or %NULL
 *   to use the full size of the collected child node
 *
 * Creates a node that repeats the child node.
 *
 * The child is recorded until the next call to [method@Gtk.Snapshot.pop].
 */
void
gtk_snapshot_push_repeat (GtkSnapshot           *snapshot,
                          const graphene_rect_t *bounds,
                          const graphene_rect_t *child_bounds)
{
  GtkSnapshotState *state;
  gboolean empty_child_bounds = FALSE;
  graphene_rect_t real_child_bounds = { { 0 } };
  float scale_x, scale_y, dx, dy;

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);

  if (child_bounds)
    {
      gtk_graphene_rect_scale_affine (child_bounds, scale_x, scale_y, dx, dy, &real_child_bounds);
      if (real_child_bounds.size.width <= 0 || real_child_bounds.size.height <= 0)
        empty_child_bounds = TRUE;
    }

  state = gtk_snapshot_push_state (snapshot,
                                   gtk_snapshot_get_current_state (snapshot)->transform,
                                   empty_child_bounds
                                   ? gtk_snapshot_collect_discard_repeat
                                   : gtk_snapshot_collect_repeat,
                                   NULL);

  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &state->data.repeat.bounds);
  state->data.repeat.child_bounds = real_child_bounds;
}

static GskRenderNode *
gtk_snapshot_collect_clip (GtkSnapshot      *snapshot,
                           GtkSnapshotState *state,
                           GskRenderNode   **nodes,
                           guint             n_nodes)
{
  GskRenderNode *node, *clip_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  /* Check if the child node will even be clipped */
  if (graphene_rect_contains_rect (&state->data.clip.bounds, &node->bounds))
    return node;

  if (state->data.clip.bounds.size.width == 0 ||
      state->data.clip.bounds.size.height == 0)
    return NULL;

  clip_node = gsk_clip_node_new (node, &state->data.clip.bounds);

  gsk_render_node_unref (node);

  return clip_node;
}

/**
 * gtk_snapshot_push_clip:
 * @snapshot: a `GtkSnapshot`
 * @bounds: the rectangle to clip to
 *
 * Clips an image to a rectangle.
 *
 * The image is recorded until the next call to [method@Gtk.Snapshot.pop].
 */
void
gtk_snapshot_push_clip (GtkSnapshot           *snapshot,
                        const graphene_rect_t *bounds)
{
  GtkSnapshotState *state;
  float scale_x, scale_y, dx, dy;

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);

  state = gtk_snapshot_push_state (snapshot,
                                   gtk_snapshot_get_current_state (snapshot)->transform,
                                   gtk_snapshot_collect_clip,
                                   NULL);

  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &state->data.clip.bounds);
}

static GskRenderNode *
gtk_snapshot_collect_gl_shader (GtkSnapshot      *snapshot,
                                GtkSnapshotState *state,
                                GskRenderNode   **collected_nodes,
                                guint             n_collected_nodes)
{
  GskRenderNode *shader_node = NULL;
  GskRenderNode **nodes;
  int n_children;

  n_children = gsk_gl_shader_get_n_textures (state->data.glshader.shader);
  shader_node = NULL;

  if (n_collected_nodes != 0)
    g_warning ("Unexpected children when popping gl shader.");

  if (state->data.glshader.nodes)
    nodes = state->data.glshader.nodes;
  else
    nodes = &state->data.glshader.internal_nodes[0];

  if (state->data.glshader.bounds.size.width != 0 &&
      state->data.glshader.bounds.size.height != 0)
    shader_node = gsk_gl_shader_node_new (state->data.glshader.shader,
                                          &state->data.glshader.bounds,
                                          state->data.glshader.args,
                                          nodes, n_children);

  return shader_node;
}

static void
gtk_snapshot_clear_gl_shader (GtkSnapshotState *state)
{
  GskRenderNode **nodes;
  guint i, n_children;

  n_children = gsk_gl_shader_get_n_textures (state->data.glshader.shader);

  if (state->data.glshader.nodes)
    nodes = state->data.glshader.nodes;
  else
    nodes = &state->data.glshader.internal_nodes[0];

  g_object_unref (state->data.glshader.shader);
  g_bytes_unref (state->data.glshader.args);

  for (i = 0; i < n_children; i++)
    gsk_render_node_unref (nodes[i]);

  g_free (state->data.glshader.nodes);

}

static GskRenderNode *
gtk_snapshot_collect_gl_shader_texture (GtkSnapshot      *snapshot,
                                        GtkSnapshotState *state,
                                        GskRenderNode   **nodes,
                                        guint             n_nodes)
{
  GskRenderNode *child_node;
  GdkRGBA transparent = { 0, 0, 0, 0 };
  int n_children, node_idx;
  GtkSnapshotState *glshader_state;
  GskRenderNode **out_nodes;

  child_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);

  if (child_node == NULL)
    child_node = gsk_color_node_new (&transparent, &state->data.glshader_texture.bounds);

  n_children = state->data.glshader_texture.n_children;
  node_idx = state->data.glshader_texture.node_idx;

  glshader_state = gtk_snapshot_get_nth_previous_state (snapshot, n_children - node_idx);
  g_assert (glshader_state->collect_func == gtk_snapshot_collect_gl_shader);

  if (glshader_state->data.glshader.nodes)
    out_nodes = glshader_state->data.glshader.nodes;
  else
    out_nodes = &glshader_state->data.glshader.internal_nodes[0];

  out_nodes[node_idx] = child_node;

  return NULL;
}

/**
 * gtk_snapshot_push_gl_shader:
 * @snapshot: a `GtkSnapshot`
 * @shader: The code to run
 * @bounds: the rectangle to render into
 * @take_args: (transfer full): Data block with arguments for the shader.
 *
 * Push a [class@Gsk.GLShaderNode].
 *
 * The node uses the given [class@Gsk.GLShader] and uniform values
 * Additionally this takes a list of @n_children other nodes
 * which will be passed to the [class@Gsk.GLShaderNode].
 *
 * The @take_args argument is a block of data to use for uniform
 * arguments, as per types and offsets defined by the @shader.
 * Normally this is generated by [method@Gsk.GLShader.format_args]
 * or [struct@Gsk.ShaderArgsBuilder].
 *
 * The snapshotter takes ownership of @take_args, so the caller should
 * not free it after this.
 *
 * If the renderer doesn't support GL shaders, or if there is any
 * problem when compiling the shader, then the node will draw pink.
 * You should use [method@Gsk.GLShader.compile] to ensure the @shader
 * will work for the renderer before using it.
 *
 * If the shader requires textures (see [method@Gsk.GLShader.get_n_textures]),
 * then it is expected that you call [method@Gtk.Snapshot.gl_shader_pop_texture]
 * the number of times that are required. Each of these calls will generate
 * a node that is added as a child to the `GskGLShaderNode`, which in turn
 * will render these offscreen and pass as a texture to the shader.
 *
 * Once all textures (if any) are pop:ed, you must call the regular
 * [method@Gtk.Snapshot.pop].
 *
 * If you want to use pre-existing textures as input to the shader rather
 * than rendering new ones, use [method@Gtk.Snapshot.append_texture] to
 * push a texture node. These will be used directly rather than being
 * re-rendered.
 *
 * For details on how to write shaders, see [class@Gsk.GLShader].
 */
void
gtk_snapshot_push_gl_shader (GtkSnapshot           *snapshot,
                             GskGLShader           *shader,
                             const graphene_rect_t *bounds,
                             GBytes                *take_args)
{
  GtkSnapshotState *state;
  float scale_x, scale_y, dx, dy;
  graphene_rect_t transformed_bounds;
  int n_children = gsk_gl_shader_get_n_textures (shader);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);

  state = gtk_snapshot_push_state (snapshot,
                                   gtk_snapshot_get_current_state (snapshot)->transform,
                                   gtk_snapshot_collect_gl_shader,
                                   gtk_snapshot_clear_gl_shader);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &transformed_bounds);
  state->data.glshader.bounds = transformed_bounds;
  state->data.glshader.shader = g_object_ref (shader);
  state->data.glshader.args = take_args; /* Takes ownership */
  if (n_children <= G_N_ELEMENTS (state->data.glshader.internal_nodes))
    state->data.glshader.nodes = NULL;
  else
    state->data.glshader.nodes = g_new (GskRenderNode *, n_children);

  for (int i = 0; i  < n_children; i++)
    {
      state = gtk_snapshot_push_state (snapshot,
                                       gtk_snapshot_get_current_state (snapshot)->transform,
                                       gtk_snapshot_collect_gl_shader_texture,
                                       NULL);
      state->data.glshader_texture.bounds = transformed_bounds;
      state->data.glshader_texture.node_idx = n_children - 1 - i;/* We pop in reverse order */
      state->data.glshader_texture.n_children = n_children;
    }
}

static GskRenderNode *
gtk_snapshot_collect_rounded_clip (GtkSnapshot      *snapshot,
                                   GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes)
{
  GskRenderNode *node, *clip_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  /* If the given radius is 0 in all corners, we can just create a normal clip node */
  if (gsk_rounded_rect_is_rectilinear (&state->data.rounded_clip.bounds))
    {
      /* ... and do the same optimization */
      if (graphene_rect_contains_rect (&state->data.rounded_clip.bounds.bounds, &node->bounds))
        return node;

      clip_node = gsk_clip_node_new (node, &state->data.rounded_clip.bounds.bounds);
    }
  else
    {
      if (gsk_rounded_rect_contains_rect (&state->data.rounded_clip.bounds, &node->bounds))
        return node;

      clip_node = gsk_rounded_clip_node_new (node, &state->data.rounded_clip.bounds);
    }

  if (clip_node->bounds.size.width == 0 ||
      clip_node->bounds.size.height == 0)
    {
      gsk_render_node_unref (node);
      gsk_render_node_unref (clip_node);
      return NULL;
    }

  gsk_render_node_unref (node);

  return clip_node;
}

/**
 * gtk_snapshot_push_rounded_clip:
 * @snapshot: a `GtkSnapshot`
 * @bounds: the rounded rectangle to clip to
 *
 * Clips an image to a rounded rectangle.
 *
 * The image is recorded until the next call to [method@Gtk.Snapshot.pop].
 */
void
gtk_snapshot_push_rounded_clip (GtkSnapshot          *snapshot,
                                const GskRoundedRect *bounds)
{
  GtkSnapshotState *state;
  float scale_x, scale_y, dx, dy;

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);

  state = gtk_snapshot_push_state (snapshot,
                                   gtk_snapshot_get_current_state (snapshot)->transform,
                                   gtk_snapshot_collect_rounded_clip,
                                   NULL);

  gsk_rounded_rect_scale_affine (&state->data.rounded_clip.bounds, bounds, scale_x, scale_y, dx, dy);
}

static GskRenderNode *
gtk_snapshot_collect_fill (GtkSnapshot      *snapshot,
                           GtkSnapshotState *state,
                           GskRenderNode   **nodes,
                           guint             n_nodes)
{
  GskRenderNode *node, *fill_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  fill_node = gsk_fill_node_new (node,
                                 state->data.fill.path,
                                 state->data.fill.fill_rule);

  if (fill_node->bounds.size.width == 0 ||
      fill_node->bounds.size.height == 0)
    {
      gsk_render_node_unref (node);
      gsk_render_node_unref (fill_node);
      return NULL;
    }

  gsk_render_node_unref (node);

  return fill_node;
}

static void
gtk_snapshot_clear_fill (GtkSnapshotState *state)
{
  gsk_path_unref (state->data.fill.path);
}

/**
 * gtk_snapshot_push_fill:
 * @snapshot: a `GtkSnapshot`
 * @path: The path describing the area to fill
 * @fill_rule: The fill rule to use
 *
 * Fills the area given by @path and @fill_rule with an image and discards everything
 * outside of it.
 *
 * The image is recorded until the next call to [method@Gtk.Snapshot.pop].
 *
 * If you want to fill the path with a color, [method@Gtk.Snapshot.append_fill]
 * may be more convenient.
 *
 * Since: 4.14
 */
void
gtk_snapshot_push_fill (GtkSnapshot *snapshot,
                        GskPath     *path,
                        GskFillRule  fill_rule)
{
  GtkSnapshotState *state;

  gtk_snapshot_ensure_identity (snapshot);

  state = gtk_snapshot_push_state (snapshot,
                                   gtk_snapshot_get_current_state (snapshot)->transform,
                                   gtk_snapshot_collect_fill,
                                   gtk_snapshot_clear_fill);

  state->data.fill.path = gsk_path_ref (path);
  state->data.fill.fill_rule = fill_rule;
}

/**
 * gtk_snapshot_append_fill:
 * @snapshot: a `GtkSnapshot`
 * @path: The path describing the area to fill
 * @fill_rule: The fill rule to use
 * @color: the color to fill the path with
 *
 * A convenience method to fill a path with a color.
 *
 * See [method@Gtk.Snapshot.push_fill] if you need
 * to fill a path with more complex content than
 * a color.
 *
 * Since: 4.14
 */
void
gtk_snapshot_append_fill (GtkSnapshot   *snapshot,
                          GskPath       *path,
                          GskFillRule    fill_rule,
                          const GdkRGBA *color)
{
  graphene_rect_t bounds;

  gsk_path_get_bounds (path, &bounds);
  gtk_snapshot_push_fill (snapshot, path, fill_rule);
  gtk_snapshot_append_color (snapshot, color, &bounds);
  gtk_snapshot_pop (snapshot);
}

static GskRenderNode *
gtk_snapshot_collect_stroke (GtkSnapshot      *snapshot,
                             GtkSnapshotState *state,
                             GskRenderNode   **nodes,
                             guint             n_nodes)
{
  GskRenderNode *node, *stroke_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  stroke_node = gsk_stroke_node_new (node,
                                     state->data.stroke.path,
                                     &state->data.stroke.stroke);

  if (stroke_node->bounds.size.width == 0 ||
      stroke_node->bounds.size.height == 0)
    {
      gsk_render_node_unref (node);
      gsk_render_node_unref (stroke_node);
      return NULL;
    }

  gsk_render_node_unref (node);

  return stroke_node;
}

static void
gtk_snapshot_clear_stroke (GtkSnapshotState *state)
{
  gsk_path_unref (state->data.stroke.path);
  gsk_stroke_clear (&state->data.stroke.stroke);
}

/**
 * gtk_snapshot_push_stroke:
 * @snapshot: a #GtkSnapshot
 * @path: The path to stroke
 * @stroke: The stroke attributes
 *
 * Strokes the given @path with the attributes given by @stroke and
 * an image.
 *
 * The image is recorded until the next call to [method@Gtk.Snapshot.pop].
 *
 * Note that the strokes are subject to the same transformation as
 * everything else, so uneven scaling will cause horizontal and vertical
 * strokes to have different widths.
 *
 * If you want to stroke the path with a color, [method@Gtk.Snapshot.append_stroke]
 * may be more convenient.
 *
 * Since: 4.14
 */
void
gtk_snapshot_push_stroke (GtkSnapshot     *snapshot,
                          GskPath         *path,
                          const GskStroke *stroke)
{
  GtkSnapshotState *state;

  gtk_snapshot_ensure_identity (snapshot);

  state = gtk_snapshot_push_state (snapshot,
                                   gtk_snapshot_get_current_state (snapshot)->transform,
                                   gtk_snapshot_collect_stroke,
                                   gtk_snapshot_clear_stroke);

  state->data.stroke.path = gsk_path_ref (path);
  state->data.stroke.stroke = GSK_STROKE_INIT_COPY (stroke);
}

static GskRenderNode *
gtk_snapshot_collect_shadow (GtkSnapshot      *snapshot,
                             GtkSnapshotState *state,
                             GskRenderNode   **nodes,
                             guint             n_nodes)
{
  GskRenderNode *node, *shadow_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  shadow_node = gsk_shadow_node_new (node,
                                     state->data.shadow.shadows != NULL ?
                                     state->data.shadow.shadows :
                                     &state->data.shadow.a_shadow,
                                     state->data.shadow.n_shadows);

  gsk_render_node_unref (node);

  return shadow_node;
}

/**
 * gtk_snapshot_append_stroke:
 * @snapshot: a `GtkSnapshot`
 * @path: The path describing the area to fill
 * @stroke: The stroke attributes
 * @color: the color to fill the path with
 *
 * A convenience method to stroke a path with a color.
 *
 * See [method@Gtk.Snapshot.push_stroke] if you need
 * to stroke a path with more complex content than
 * a color.
 *
 * Since: 4.14
 */
void
gtk_snapshot_append_stroke (GtkSnapshot     *snapshot,
                            GskPath         *path,
                            const GskStroke *stroke,
                            const GdkRGBA   *color)
{
  graphene_rect_t bounds;

  gsk_path_get_stroke_bounds (path, stroke, &bounds);
  gtk_snapshot_push_stroke (snapshot, path, stroke);
  gtk_snapshot_append_color (snapshot, color, &bounds);
  gtk_snapshot_pop (snapshot);
}

static void
gtk_snapshot_clear_shadow (GtkSnapshotState *state)
{
  g_free (state->data.shadow.shadows);
}

/**
 * gtk_snapshot_push_shadow:
 * @snapshot: a `GtkSnapshot`
 * @shadow: (array length=n_shadows): the first shadow specification
 * @n_shadows: number of shadow specifications
 *
 * Applies a shadow to an image.
 *
 * The image is recorded until the next call to [method@Gtk.Snapshot.pop].
 */
void
gtk_snapshot_push_shadow (GtkSnapshot     *snapshot,
                          const GskShadow *shadow,
                          gsize            n_shadows)
{
  GtkSnapshotState *state;
  GskTransform *transform;
  float scale_x, scale_y, dx, dy;
  gsize i;

  gtk_snapshot_ensure_affine_with_flags (snapshot,
                                         ENSURE_POSITIVE_SCALE | ENSURE_UNIFORM_SCALE,
                                         &scale_x, &scale_y,
                                         &dx, &dy);
  transform = gsk_transform_scale (gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (dx, dy)), scale_x, scale_y);

  state = gtk_snapshot_push_state (snapshot,
                                   transform,
                                   gtk_snapshot_collect_shadow,
                                   gtk_snapshot_clear_shadow);

  state->data.shadow.n_shadows = n_shadows;
  if (n_shadows == 1)
    {
      state->data.shadow.shadows = NULL;
      memcpy (&state->data.shadow.a_shadow, shadow, sizeof (GskShadow));
      state->data.shadow.a_shadow.dx *= scale_x;
      state->data.shadow.a_shadow.dy *= scale_y;
      state->data.shadow.a_shadow.radius *= scale_x;
    }
  else
    {
      state->data.shadow.shadows = g_malloc (sizeof (GskShadow) * n_shadows);
      memcpy (state->data.shadow.shadows, shadow, sizeof (GskShadow) * n_shadows);
      for (i = 0; i < n_shadows; i++)
        {
          state->data.shadow.shadows[i].dx *= scale_x;
          state->data.shadow.shadows[i].dy *= scale_y;
          state->data.shadow.shadows[i].radius *= scale_x;
        }
    }

  gsk_transform_unref (transform);
}

static GskRenderNode *
gtk_snapshot_collect_blend_top (GtkSnapshot      *snapshot,
                                GtkSnapshotState *state,
                                GskRenderNode   **nodes,
                                guint             n_nodes)
{
  GskRenderNode *bottom_node, *top_node, *blend_node;
  GdkRGBA transparent = { 0, 0, 0, 0 };

  top_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  bottom_node = state->data.blend.bottom_node != NULL
              ? gsk_render_node_ref (state->data.blend.bottom_node)
              : NULL;

  g_assert (top_node != NULL || bottom_node != NULL);

  /* XXX: Is this necessary? Do we need a NULL node? */
  if (top_node == NULL)
    top_node = gsk_color_node_new (&transparent, &bottom_node->bounds);
  if (bottom_node == NULL)
    bottom_node = gsk_color_node_new (&transparent, &top_node->bounds);

  blend_node = gsk_blend_node_new (bottom_node, top_node, state->data.blend.blend_mode);

  gsk_render_node_unref (top_node);
  gsk_render_node_unref (bottom_node);

  return blend_node;
}

static void
gtk_snapshot_clear_blend_top (GtkSnapshotState *state)
{
  g_clear_pointer (&(state->data.blend.bottom_node), gsk_render_node_unref);
}

static GskRenderNode *
gtk_snapshot_collect_blend_bottom (GtkSnapshot      *snapshot,
                                   GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes)
{
  GtkSnapshotState *prev_state = gtk_snapshot_get_previous_state (snapshot);

  g_assert (prev_state->collect_func == gtk_snapshot_collect_blend_top);

  prev_state->data.blend.bottom_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);

  return NULL;
}

/**
 * gtk_snapshot_push_blend:
 * @snapshot: a `GtkSnapshot`
 * @blend_mode: blend mode to use
 *
 * Blends together two images with the given blend mode.
 *
 * Until the first call to [method@Gtk.Snapshot.pop], the
 * bottom image for the blend operation will be recorded.
 * After that call, the top image to be blended will be
 * recorded until the second call to [method@Gtk.Snapshot.pop].
 *
 * Calling this function requires two subsequent calls
 * to [method@Gtk.Snapshot.pop].
 */
void
gtk_snapshot_push_blend (GtkSnapshot  *snapshot,
                         GskBlendMode  blend_mode)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *top_state;

  top_state = gtk_snapshot_push_state (snapshot,
                                       current_state->transform,
                                       gtk_snapshot_collect_blend_top,
                                       gtk_snapshot_clear_blend_top);
  top_state->data.blend.blend_mode = blend_mode;

  gtk_snapshot_push_state (snapshot,
                           top_state->transform,
                           gtk_snapshot_collect_blend_bottom,
                           NULL);
}

static GskRenderNode *
gtk_snapshot_collect_mask_source (GtkSnapshot      *snapshot,
                                  GtkSnapshotState *state,
                                  GskRenderNode   **nodes,
                                  guint             n_nodes)
{
  GskRenderNode *source_child, *mask_child, *mask_node;

  mask_child = state->data.mask.mask_node;
  source_child = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (source_child == NULL)
    return NULL;

  if (mask_child)
    mask_node = gsk_mask_node_new (source_child, mask_child, state->data.mask.mask_mode);
  else
    mask_node = NULL;

  gsk_render_node_unref (source_child);

  return mask_node;
}

static void
gtk_snapshot_clear_mask_source (GtkSnapshotState *state)
{
  g_clear_pointer (&(state->data.mask.mask_node), gsk_render_node_unref);
}

static GskRenderNode *
gtk_snapshot_collect_mask_mask (GtkSnapshot      *snapshot,
                                GtkSnapshotState *state,
                                GskRenderNode   **nodes,
                                guint             n_nodes)
{
  GtkSnapshotState *prev_state = gtk_snapshot_get_previous_state (snapshot);

  g_assert (prev_state->collect_func == gtk_snapshot_collect_mask_source);

  prev_state->data.mask.mask_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);

  return NULL;
}

/**
 * gtk_snapshot_push_mask:
 * @snapshot: a #GtkSnapshot
 * @mask_mode: mask mode to use
 *
 * Until the first call to [method@Gtk.Snapshot.pop], the
 * mask image for the mask operation will be recorded.
 *
 * After that call, the source image will be recorded until
 * the second call to [method@Gtk.Snapshot.pop].
 *
 * Calling this function requires 2 subsequent calls to gtk_snapshot_pop().
 *
 * Since: 4.10
 */
void
gtk_snapshot_push_mask (GtkSnapshot *snapshot,
                        GskMaskMode  mask_mode)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *source_state;

  source_state = gtk_snapshot_push_state (snapshot,
                                          current_state->transform,
                                          gtk_snapshot_collect_mask_source,
                                          gtk_snapshot_clear_mask_source);

  source_state->data.mask.mask_mode = mask_mode;

  gtk_snapshot_push_state (snapshot,
                           source_state->transform,
                           gtk_snapshot_collect_mask_mask,
                           NULL);
}

static GskRenderNode *
gtk_snapshot_collect_cross_fade_end (GtkSnapshot      *snapshot,
                                     GtkSnapshotState *state,
                                     GskRenderNode   **nodes,
                                     guint             n_nodes)
{
  GskRenderNode *start_node, *end_node, *node;

  end_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  start_node = state->data.cross_fade.start_node;
  state->data.cross_fade.start_node = NULL;

  if (state->data.cross_fade.progress <= 0.0)
    {
      node = start_node;

      if (end_node)
        gsk_render_node_unref (end_node);
    }
  else if (state->data.cross_fade.progress >= 1.0)
    {
      node = end_node;

      if (start_node)
        gsk_render_node_unref (start_node);
    }
  else if (start_node && end_node)
    {
      node = gsk_cross_fade_node_new (start_node, end_node, state->data.cross_fade.progress);

      gsk_render_node_unref (start_node);
      gsk_render_node_unref (end_node);
    }
  else if (start_node)
    {
      node = gsk_opacity_node_new (start_node, 1.0 - state->data.cross_fade.progress);

      gsk_render_node_unref (start_node);
    }
  else if (end_node)
    {
      node = gsk_opacity_node_new (end_node, state->data.cross_fade.progress);

      gsk_render_node_unref (end_node);
    }
  else
    {
      node = NULL;
    }

  return node;
}

static void
gtk_snapshot_clear_cross_fade_end (GtkSnapshotState *state)
{
  g_clear_pointer (&state->data.cross_fade.start_node, gsk_render_node_unref);
}

static GskRenderNode *
gtk_snapshot_collect_cross_fade_start (GtkSnapshot      *snapshot,
                                       GtkSnapshotState *state,
                                       GskRenderNode   **nodes,
                                       guint             n_nodes)
{
  GtkSnapshotState *prev_state = gtk_snapshot_get_previous_state (snapshot);

  g_assert (prev_state->collect_func == gtk_snapshot_collect_cross_fade_end);

  prev_state->data.cross_fade.start_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);

  return NULL;
}

/**
 * gtk_snapshot_push_cross_fade:
 * @snapshot: a `GtkSnapshot`
 * @progress: progress between 0.0 and 1.0
 *
 * Snapshots a cross-fade operation between two images with the
 * given @progress.
 *
 * Until the first call to [method@Gtk.Snapshot.pop], the start image
 * will be snapshot. After that call, the end image will be recorded
 * until the second call to [method@Gtk.Snapshot.pop].
 *
 * Calling this function requires two subsequent calls
 * to [method@Gtk.Snapshot.pop].
 */
void
gtk_snapshot_push_cross_fade (GtkSnapshot *snapshot,
                              double       progress)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *end_state;

  end_state = gtk_snapshot_push_state (snapshot,
                                       current_state->transform,
                                       gtk_snapshot_collect_cross_fade_end,
                                       gtk_snapshot_clear_cross_fade_end);
  end_state->data.cross_fade.progress = progress;

  gtk_snapshot_push_state (snapshot,
                           end_state->transform,
                           gtk_snapshot_collect_cross_fade_start,
                           NULL);
}

static GskRenderNode *
gtk_snapshot_pop_one (GtkSnapshot *snapshot)
{
  GtkSnapshotState *state;
  guint state_index;
  GskRenderNode *node;

  if (gtk_snapshot_states_is_empty (&snapshot->state_stack))
    {
      g_warning ("Too many gtk_snapshot_pop() calls.");
      return NULL;
    }

  state = gtk_snapshot_get_current_state (snapshot);
  state_index = gtk_snapshot_states_get_size (&snapshot->state_stack) - 1;

  if (state->collect_func)
    {
      node = state->collect_func (snapshot,
                                  state,
                                  (GskRenderNode **) gtk_snapshot_nodes_index (&snapshot->nodes, state->start_node_index),
                                  state->n_nodes);

      /* The collect func may not modify the state stack... */
      g_assert (state_index == gtk_snapshot_states_get_size (&snapshot->state_stack) - 1);

      /* Remove all the state's nodes from the list of nodes */
      g_assert (state->start_node_index + state->n_nodes == gtk_snapshot_nodes_get_size (&snapshot->nodes));
      gtk_snapshot_nodes_splice (&snapshot->nodes, state->start_node_index, state->n_nodes, FALSE, NULL, 0);
    }
  else
    {
      GtkSnapshotState *previous_state;

      node = NULL;

      /* move the nodes to the parent */
      previous_state = gtk_snapshot_get_previous_state (snapshot);
      previous_state->n_nodes += state->n_nodes;
      g_assert (previous_state->start_node_index + previous_state->n_nodes == gtk_snapshot_nodes_get_size (&snapshot->nodes));
    }

  gtk_snapshot_states_splice (&snapshot->state_stack, state_index, 1, FALSE, NULL, 0);

  return node;
}

static void
gtk_snapshot_append_node_internal (GtkSnapshot   *snapshot,
                                   GskRenderNode *node)
{
  GtkSnapshotState *current_state;

  current_state = gtk_snapshot_get_current_state (snapshot);

  if (current_state)
    {
      gtk_snapshot_nodes_append (&snapshot->nodes, node);
      current_state->n_nodes ++;
    }
  else
    {
      g_critical ("Tried appending a node to an already finished snapshot.");
    }
}

static GskRenderNode *
gtk_snapshot_pop_internal (GtkSnapshot *snapshot,
                           gboolean     is_texture_pop)
{
  GtkSnapshotState *state;
  GskRenderNode *node;
  guint forgotten_restores = 0;

  for (state = gtk_snapshot_get_current_state (snapshot);
       gtk_snapshot_state_should_autopop (state) ||
       state->collect_func == NULL;
       state = gtk_snapshot_get_current_state (snapshot))
    {
      if (state->collect_func == NULL)
        forgotten_restores++;

      node = gtk_snapshot_pop_one (snapshot);
      if (node)
        gtk_snapshot_append_node_internal (snapshot, node);
    }

  if (forgotten_restores)
    {
      g_warning ("Too many gtk_snapshot_save() calls. %u saves remaining.", forgotten_restores);
    }

  if (is_texture_pop && (state->collect_func != gtk_snapshot_collect_gl_shader_texture))
    {
      g_critical ("Unexpected call to gtk_snapshot_gl_shader_pop_texture().");
      return NULL;
    }
  else if (!is_texture_pop && (state->collect_func == gtk_snapshot_collect_gl_shader_texture))
    {
      g_critical ("Expected a call to gtk_snapshot_gl_shader_pop_texture().");
      return NULL;
    }

  return gtk_snapshot_pop_one (snapshot);
}

/**
 * gtk_snapshot_push_collect:
 *
 * Private.
 *
 * Pushes state so a later pop_collect call can collect all nodes
 * appended until that point.
 */
void
gtk_snapshot_push_collect (GtkSnapshot *snapshot)
{
  gtk_snapshot_push_state (snapshot,
                           NULL,
                           gtk_snapshot_collect_default,
                           NULL);
}

GskRenderNode *
gtk_snapshot_pop_collect (GtkSnapshot *snapshot)
{
  GskRenderNode *result = gtk_snapshot_pop_internal (snapshot, FALSE);

  return result;
}

/**
 * gtk_snapshot_to_node:
 * @snapshot: a `GtkSnapshot`
 *
 * Returns the render node that was constructed
 * by @snapshot.
 *
 * Note that this function may return %NULL if nothing has been
 * added to the snapshot or if its content does not produce pixels
 * to be rendered.
 *
 * After calling this function, it is no longer possible to
 * add more nodes to @snapshot. The only function that should
 * be called after this is [method@GObject.Object.unref].
 *
 * Returns: (transfer full) (nullable): the constructed `GskRenderNode` or
 *   %NULL if there are no nodes to render.
 */
GskRenderNode *
gtk_snapshot_to_node (GtkSnapshot *snapshot)
{
  GskRenderNode *result;

  result = gtk_snapshot_pop_internal (snapshot, FALSE);

  /* We should have exactly our initial state */
  if (!gtk_snapshot_states_is_empty (&snapshot->state_stack))
    {
      g_warning ("Too many gtk_snapshot_push() calls. %zu states remaining.",
                 gtk_snapshot_states_get_size (&snapshot->state_stack));
    }

  gtk_snapshot_states_clear (&snapshot->state_stack);
  gtk_snapshot_nodes_clear (&snapshot->nodes);

  return result;
}

/**
 * gtk_snapshot_to_paintable:
 * @snapshot: a `GtkSnapshot`
 * @size: (nullable): The size of the resulting paintable
 *   or %NULL to use the bounds of the snapshot
 *
 * Returns a paintable encapsulating the render node
 * that was constructed by @snapshot.
 *
 * After calling this function, it is no longer possible to
 * add more nodes to @snapshot. The only function that should
 * be called after this is [method@GObject.Object.unref].
 *
 * Returns: (transfer full) (nullable): a new `GdkPaintable`
 */
GdkPaintable *
gtk_snapshot_to_paintable (GtkSnapshot           *snapshot,
                           const graphene_size_t *size)
{
  GskRenderNode *node;
  GdkPaintable *paintable;
  graphene_rect_t bounds;

  node = gtk_snapshot_to_node (snapshot);
  if (size)
    {
      graphene_size_init_from_size (&bounds.size, size);
    }
  else if (node)
    {
      gsk_render_node_get_bounds (node, &bounds);
      bounds.size.width += bounds.origin.x;
      bounds.size.height += bounds.origin.y;
    }
  else
    {
      bounds.size.width = 0;
      bounds.size.height = 0;
    }
  bounds.origin.x = 0;
  bounds.origin.y = 0;

  paintable = gtk_render_node_paintable_new (node, &bounds);
  g_clear_pointer (&node, gsk_render_node_unref);

  return paintable;
}

/**
 * gtk_snapshot_pop:
 * @snapshot: a `GtkSnapshot`
 *
 * Removes the top element from the stack of render nodes,
 * and appends it to the node underneath it.
 */
void
gtk_snapshot_pop (GtkSnapshot *snapshot)
{
  GskRenderNode *node;

  node = gtk_snapshot_pop_internal (snapshot, FALSE);

  if (node)
    gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_gl_shader_pop_texture:
 * @snapshot: a `GtkSnapshot`
 *
 * Removes the top element from the stack of render nodes and
 * adds it to the nearest [class@Gsk.GLShaderNode] below it.
 *
 * This must be called the same number of times as the number
 * of textures is needed for the shader in
 * [method@Gtk.Snapshot.push_gl_shader].
 */
void
gtk_snapshot_gl_shader_pop_texture (GtkSnapshot *snapshot)
{
  G_GNUC_UNUSED GskRenderNode *node;

  node = gtk_snapshot_pop_internal (snapshot, TRUE);
  g_assert (node == NULL);
}


/**
 * gtk_snapshot_save:
 * @snapshot: a `GtkSnapshot`
 *
 * Makes a copy of the current state of @snapshot and saves it
 * on an internal stack.
 *
 * When [method@Gtk.Snapshot.restore] is called, @snapshot will
 * be restored to the saved state.
 *
 * Multiple calls to [method@Gtk.Snapshot.save] and [method@Gtk.Snapshot.restore]
 * can be nested; each call to `gtk_snapshot_restore()` restores the state from
 * the matching paired `gtk_snapshot_save()`.
 *
 * It is necessary to clear all saved states with corresponding
 * calls to `gtk_snapshot_restore()`.
 */
void
gtk_snapshot_save (GtkSnapshot *snapshot)
{
  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));

  gtk_snapshot_push_state (snapshot,
                           gtk_snapshot_get_current_state (snapshot)->transform,
                           NULL,
                           NULL);
}

/**
 * gtk_snapshot_restore:
 * @snapshot: a `GtkSnapshot`
 *
 * Restores @snapshot to the state saved by a preceding call to
 * [method@Snapshot.save] and removes that state from the stack of
 * saved states.
 */
void
gtk_snapshot_restore (GtkSnapshot *snapshot)
{
  GtkSnapshotState *state;
  GskRenderNode *node;

  for (state = gtk_snapshot_get_current_state (snapshot);
       gtk_snapshot_state_should_autopop (state);
       state = gtk_snapshot_get_current_state (snapshot))
    {
      node = gtk_snapshot_pop_one (snapshot);
      if (node)
        gtk_snapshot_append_node_internal (snapshot, node);
    }

  if (state->collect_func != NULL)
    {
      g_warning ("Too many gtk_snapshot_restore() calls.");
      return;
    }

  node = gtk_snapshot_pop_one (snapshot);
  g_assert (node == NULL);
}

/**
 * gtk_snapshot_transform:
 * @snapshot: a `GtkSnapshot`
 * @transform: (nullable): the transform to apply
 *
 * Transforms @snapshot's coordinate system with the given @transform.
 */
void
gtk_snapshot_transform (GtkSnapshot  *snapshot,
                        GskTransform *transform)
{
  GtkSnapshotState *state;

  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));

  state = gtk_snapshot_get_current_state (snapshot);
  state->transform = gsk_transform_transform (state->transform, transform);
}

/**
 * gtk_snapshot_transform_matrix:
 * @snapshot: a `GtkSnapshot`
 * @matrix: the matrix to multiply the transform with
 *
 * Transforms @snapshot's coordinate system with the given @matrix.
 */
void
gtk_snapshot_transform_matrix (GtkSnapshot             *snapshot,
                               const graphene_matrix_t *matrix)
{
  GtkSnapshotState *state;

  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));
  g_return_if_fail (matrix != NULL);

  state = gtk_snapshot_get_current_state (snapshot);
  state->transform = gsk_transform_matrix (state->transform, matrix);
}

/**
 * gtk_snapshot_translate:
 * @snapshot: a `GtkSnapshot`
 * @point: the point to translate the snapshot by
 *
 * Translates @snapshot's coordinate system by @point in 2-dimensional space.
 */
void
gtk_snapshot_translate (GtkSnapshot            *snapshot,
                        const graphene_point_t *point)
{
  GtkSnapshotState *state;

  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));
  g_return_if_fail (point != NULL);

  state = gtk_snapshot_get_current_state (snapshot);
  state->transform = gsk_transform_translate (state->transform, point);
}

/**
 * gtk_snapshot_translate_3d:
 * @snapshot: a `GtkSnapshot`
 * @point: the point to translate the snapshot by
 *
 * Translates @snapshot's coordinate system by @point.
 */
void
gtk_snapshot_translate_3d (GtkSnapshot              *snapshot,
                           const graphene_point3d_t *point)
{
  GtkSnapshotState *state;

  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));
  g_return_if_fail (point != NULL);

  state = gtk_snapshot_get_current_state (snapshot);
  state->transform = gsk_transform_translate_3d (state->transform, point);
}

/**
 * gtk_snapshot_rotate:
 * @snapshot: a `GtkSnapshot`
 * @angle: the rotation angle, in degrees (clockwise)
 *
 * Rotates @@snapshot's coordinate system by @angle degrees in 2D space -
 * or in 3D speak, rotates around the Z axis. The rotation happens around
 * the origin point of (0, 0) in the @snapshot's current coordinate system.
 *
 * To rotate around axes other than the Z axis, use [method@Gsk.Transform.rotate_3d].
 */
void
gtk_snapshot_rotate (GtkSnapshot *snapshot,
                     float        angle)
{
  GtkSnapshotState *state;

  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));

  state = gtk_snapshot_get_current_state (snapshot);
  state->transform = gsk_transform_rotate (state->transform, angle);
}

/**
 * gtk_snapshot_rotate_3d:
 * @snapshot: a `GtkSnapshot`
 * @angle: the rotation angle, in degrees (clockwise)
 * @axis: The rotation axis
 *
 * Rotates @snapshot's coordinate system by @angle degrees around @axis.
 *
 * For a rotation in 2D space, use [method@Gsk.Transform.rotate].
 */
void
gtk_snapshot_rotate_3d (GtkSnapshot           *snapshot,
                        float                  angle,
                        const graphene_vec3_t *axis)
{
  GtkSnapshotState *state;

  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));
  g_return_if_fail (axis != NULL);

  state = gtk_snapshot_get_current_state (snapshot);
  state->transform = gsk_transform_rotate_3d (state->transform, angle, axis);
}

/**
 * gtk_snapshot_scale:
 * @snapshot: a `GtkSnapshot`
 * @factor_x: scaling factor on the X axis
 * @factor_y: scaling factor on the Y axis
 *
 * Scales @snapshot's coordinate system in 2-dimensional space by
 * the given factors.
 *
 * Use [method@Gtk.Snapshot.scale_3d] to scale in all 3 dimensions.
 */
void
gtk_snapshot_scale (GtkSnapshot *snapshot,
                    float        factor_x,
                    float        factor_y)
{
  GtkSnapshotState *state;

  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));

  state = gtk_snapshot_get_current_state (snapshot);
  state->transform = gsk_transform_scale (state->transform, factor_x, factor_y);
}

/**
 * gtk_snapshot_scale_3d:
 * @snapshot: a `GtkSnapshot`
 * @factor_x: scaling factor on the X axis
 * @factor_y: scaling factor on the Y axis
 * @factor_z: scaling factor on the Z axis
 *
 * Scales @snapshot's coordinate system by the given factors.
 */
void
gtk_snapshot_scale_3d (GtkSnapshot *snapshot,
                       float        factor_x,
                       float        factor_y,
                       float        factor_z)
{
  GtkSnapshotState *state;

  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));

  state = gtk_snapshot_get_current_state (snapshot);
  state->transform = gsk_transform_scale_3d (state->transform, factor_x, factor_y, factor_z);
}

/**
 * gtk_snapshot_perspective:
 * @snapshot: a `GtkSnapshot`
 * @depth: distance of the z=0 plane
 *
 * Applies a perspective projection transform.
 *
 * See [method@Gsk.Transform.perspective] for a discussion on the details.
 */
void
gtk_snapshot_perspective (GtkSnapshot *snapshot,
                          float        depth)
{
  GtkSnapshotState *state;

  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));

  state = gtk_snapshot_get_current_state (snapshot);
  state->transform = gsk_transform_perspective (state->transform, depth);
}

/**
 * gtk_snapshot_append_node:
 * @snapshot: a `GtkSnapshot`
 * @node: a `GskRenderNode`
 *
 * Appends @node to the current render node of @snapshot,
 * without changing the current node.
 *
 * If @snapshot does not have a current node yet, @node
 * will become the initial node.
 */
void
gtk_snapshot_append_node (GtkSnapshot   *snapshot,
                          GskRenderNode *node)
{
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  gtk_snapshot_ensure_identity (snapshot);

  gtk_snapshot_append_node_internal (snapshot, gsk_render_node_ref (node));
}

/**
 * gtk_snapshot_append_cairo:
 * @snapshot: a `GtkSnapshot`
 * @bounds: the bounds for the new node
 *
 * Creates a new [class@Gsk.CairoNode] and appends it to the current
 * render node of @snapshot, without changing the current node.
 *
 * Returns: a `cairo_t` suitable for drawing the contents of
 *   the newly created render node
 */
cairo_t *
gtk_snapshot_append_cairo (GtkSnapshot           *snapshot,
                           const graphene_rect_t *bounds)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float scale_x, scale_y, dx, dy;
  cairo_t *cr;

  g_return_val_if_fail (snapshot != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);

  node = gsk_cairo_node_new (&real_bounds);

  gtk_snapshot_append_node_internal (snapshot, node);

  cr = gsk_cairo_node_get_draw_context (node);

  cairo_scale (cr, scale_x, scale_y);
  cairo_translate (cr, dx, dy);

  return cr;
}

/**
 * gtk_snapshot_append_texture:
 * @snapshot: a `GtkSnapshot`
 * @texture: the texture to render
 * @bounds: the bounds for the new node
 *
 * Creates a new render node drawing the @texture
 * into the given @bounds and appends it to the
 * current render node of @snapshot.
 *
 * If the texture needs to be scaled to fill @bounds,
 * linear filtering is used. See [method@Gtk.Snapshot.append_scaled_texture]
 * if you need other filtering, such as nearest-neighbour.
 */
void
gtk_snapshot_append_texture (GtkSnapshot           *snapshot,
                             GdkTexture            *texture,
                             const graphene_rect_t *bounds)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float scale_x, scale_y, dx, dy;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GDK_IS_TEXTURE (texture));
  g_return_if_fail (bounds != NULL);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);
  node = gsk_texture_node_new (texture, &real_bounds);

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_scaled_texture:
 * @snapshot: a `GtkSnapshot`
 * @texture: the texture to render
 * @filter: the filter to use
 * @bounds: the bounds for the new node
 *
 * Creates a new render node drawing the @texture
 * into the given @bounds and appends it to the
 * current render node of @snapshot.
 *
 * In contrast to [method@Gtk.Snapshot.append_texture],
 * this function provides control about how the filter
 * that is used when scaling.
 *
 * Since: 4.10
 */
void
gtk_snapshot_append_scaled_texture (GtkSnapshot           *snapshot,
                                    GdkTexture            *texture,
                                    GskScalingFilter       filter,
                                    const graphene_rect_t *bounds)
{
  GskRenderNode *node;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GDK_IS_TEXTURE (texture));
  g_return_if_fail (bounds != NULL);

  gtk_snapshot_ensure_identity (snapshot);
  node = gsk_texture_scale_node_new (texture, bounds, filter);

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_color:
 * @snapshot: a `GtkSnapshot`
 * @color: the color to draw
 * @bounds: the bounds for the new node
 *
 * Creates a new render node drawing the @color into the
 * given @bounds and appends it to the current render node
 * of @snapshot.
 *
 * You should try to avoid calling this function if
 * @color is transparent.
 */
void
gtk_snapshot_append_color (GtkSnapshot           *snapshot,
                           const GdkRGBA         *color,
                           const graphene_rect_t *bounds)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float scale_x, scale_y, dx, dy;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (color != NULL);
  g_return_if_fail (bounds != NULL);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);

  node = gsk_color_node_new (color, &real_bounds);

  gtk_snapshot_append_node_internal (snapshot, node);
}

void
gtk_snapshot_append_text (GtkSnapshot           *snapshot,
                          PangoFont             *font,
                          PangoGlyphString      *glyphs,
                          const GdkRGBA         *color,
                          float                  x,
                          float                  y)
{
  GskRenderNode *node;
  float dx, dy;

  gtk_snapshot_ensure_translate (snapshot, &dx, &dy);

  node = gsk_text_node_new (font,
                            glyphs,
                            color,
                            &GRAPHENE_POINT_INIT (x + dx, y + dy));
  if (node == NULL)
    return;

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_linear_gradient:
 * @snapshot: a `GtkSnapshot`
 * @bounds: the rectangle to render the linear gradient into
 * @start_point: the point at which the linear gradient will begin
 * @end_point: the point at which the linear gradient will finish
 * @stops: (array length=n_stops): the color stops defining the gradient
 * @n_stops: the number of elements in @stops
 *
 * Appends a linear gradient node with the given stops to @snapshot.
 */
void
gtk_snapshot_append_linear_gradient (GtkSnapshot            *snapshot,
                                     const graphene_rect_t  *bounds,
                                     const graphene_point_t *start_point,
                                     const graphene_point_t *end_point,
                                     const GskColorStop     *stops,
                                     gsize                   n_stops)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float scale_x, scale_y, dx, dy;
  const GdkRGBA *first_color;
  gboolean need_gradient = FALSE;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (start_point != NULL);
  g_return_if_fail (end_point != NULL);
  g_return_if_fail (stops != NULL);
  g_return_if_fail (n_stops > 1);

  gtk_snapshot_ensure_affine_with_flags (snapshot,
                                         ENSURE_POSITIVE_SCALE | ENSURE_UNIFORM_SCALE,
                                         &scale_x, &scale_y,
                                         &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);

  first_color = &stops[0].color;
  for (gsize i = 0; i < n_stops; i ++)
    {
      if (!gdk_rgba_equal (first_color, &stops[i].color))
        {
          need_gradient = TRUE;
          break;
        }
    }

  if (need_gradient)
    {
      graphene_point_t real_start_point, real_end_point;

      real_start_point.x = scale_x * start_point->x + dx;
      real_start_point.y = scale_y * start_point->y + dy;
      real_end_point.x = scale_x * end_point->x + dx;
      real_end_point.y = scale_y * end_point->y + dy;

      node = gsk_linear_gradient_node_new (&real_bounds,
                                           &real_start_point,
                                           &real_end_point,
                                           stops,
                                           n_stops);
    }
  else
    {
      node = gsk_color_node_new (first_color, &real_bounds);
    }

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_repeating_linear_gradient:
 * @snapshot: a `GtkSnapshot`
 * @bounds: the rectangle to render the linear gradient into
 * @start_point: the point at which the linear gradient will begin
 * @end_point: the point at which the linear gradient will finish
 * @stops: (array length=n_stops): the color stops defining the gradient
 * @n_stops: the number of elements in @stops
 *
 * Appends a repeating linear gradient node with the given stops to @snapshot.
 */
void
gtk_snapshot_append_repeating_linear_gradient (GtkSnapshot            *snapshot,
                                               const graphene_rect_t  *bounds,
                                               const graphene_point_t *start_point,
                                               const graphene_point_t *end_point,
                                               const GskColorStop     *stops,
                                               gsize                   n_stops)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float scale_x, scale_y, dx, dy;
  gboolean need_gradient = FALSE;
  const GdkRGBA *first_color;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (start_point != NULL);
  g_return_if_fail (end_point != NULL);
  g_return_if_fail (stops != NULL);
  g_return_if_fail (n_stops > 1);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);

  first_color = &stops[0].color;
  for (gsize i = 0; i < n_stops; i ++)
    {
      if (!gdk_rgba_equal (first_color, &stops[i].color))
        {
          need_gradient = TRUE;
          break;
        }
    }

  if (need_gradient)
    {
      graphene_point_t real_start_point, real_end_point;

      real_start_point.x = scale_x * start_point->x + dx;
      real_start_point.y = scale_y * start_point->y + dy;
      real_end_point.x = scale_x * end_point->x + dx;
      real_end_point.y = scale_y * end_point->y + dy;

      node = gsk_repeating_linear_gradient_node_new (&real_bounds,
                                                     &real_start_point,
                                                     &real_end_point,
                                                     stops,
                                                     n_stops);
    }
  else
    {
      node = gsk_color_node_new (first_color, &real_bounds);
    }

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_conic_gradient:
 * @snapshot: a `GtkSnapshot`
 * @bounds: the rectangle to render the gradient into
 * @center: the center point of the conic gradient
 * @rotation: the clockwise rotation in degrees of the starting angle.
 *   0 means the starting angle is the top.
 * @stops: (array length=n_stops): the color stops defining the gradient
 * @n_stops: the number of elements in @stops
 *
 * Appends a conic gradient node with the given stops to @snapshot.
 */
void
gtk_snapshot_append_conic_gradient (GtkSnapshot            *snapshot,
                                    const graphene_rect_t  *bounds,
                                    const graphene_point_t *center,
                                    float                   rotation,
                                    const GskColorStop     *stops,
                                    gsize                   n_stops)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float dx, dy;
  const GdkRGBA *first_color;
  gboolean need_gradient = FALSE;
  int i;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (center != NULL);
  g_return_if_fail (stops != NULL);
  g_return_if_fail (n_stops > 1);

  gtk_snapshot_ensure_translate (snapshot, &dx, &dy);
  graphene_rect_offset_r (bounds, dx, dy, &real_bounds);

  first_color = &stops[0].color;
  for (i = 0; i < n_stops; i ++)
    {
      if (!gdk_rgba_equal (first_color, &stops[i].color))
        {
          need_gradient = TRUE;
          break;
        }
    }

  if (need_gradient)
    node = gsk_conic_gradient_node_new (&real_bounds,
                                        &GRAPHENE_POINT_INIT(
                                          center->x + dx,
                                          center->y + dy
                                        ),
                                        rotation,
                                        stops,
                                        n_stops);
  else
    node = gsk_color_node_new (first_color, &real_bounds);

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_radial_gradient:
 * @snapshot: a `GtkSnapshot`
 * @bounds: the rectangle to render the readial gradient into
 * @center: the center point for the radial gradient
 * @hradius: the horizontal radius
 * @vradius: the vertical radius
 * @start: the start position (on the horizontal axis)
 * @end: the end position (on the horizontal axis)
 * @stops: (array length=n_stops): the color stops defining the gradient
 * @n_stops: the number of elements in @stops
 *
 * Appends a radial gradient node with the given stops to @snapshot.
 */
void
gtk_snapshot_append_radial_gradient (GtkSnapshot            *snapshot,
                                     const graphene_rect_t  *bounds,
                                     const graphene_point_t *center,
                                     float                   hradius,
                                     float                   vradius,
                                     float                   start,
                                     float                   end,
                                     const GskColorStop     *stops,
                                     gsize                   n_stops)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float scale_x, scale_y, dx, dy;
  gboolean need_gradient = FALSE;
  const GdkRGBA *first_color;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (center != NULL);
  g_return_if_fail (stops != NULL);
  g_return_if_fail (n_stops > 1);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);

  first_color = &stops[0].color;
  for (gsize i = 0; i < n_stops; i ++)
    {
      if (!gdk_rgba_equal (first_color, &stops[i].color))
        {
          need_gradient = TRUE;
          break;
        }
    }

  if (need_gradient)
    {
      graphene_point_t real_center;

      real_center.x = scale_x * center->x + dx;
      real_center.y = scale_y * center->y + dy;

      node = gsk_radial_gradient_node_new (&real_bounds,
                                           &real_center,
                                           hradius * scale_x,
                                           vradius * scale_y,
                                           start,
                                           end,
                                           stops,
                                           n_stops);
    }
  else
    {
      node = gsk_color_node_new (first_color, &real_bounds);
    }

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_repeating_radial_gradient:
 * @snapshot: a `GtkSnapshot`
 * @bounds: the rectangle to render the readial gradient into
 * @center: the center point for the radial gradient
 * @hradius: the horizontal radius
 * @vradius: the vertical radius
 * @start: the start position (on the horizontal axis)
 * @end: the end position (on the horizontal axis)
 * @stops: (array length=n_stops): the color stops defining the gradient
 * @n_stops: the number of elements in @stops
 *
 * Appends a repeating radial gradient node with the given stops to @snapshot.
 */
void
gtk_snapshot_append_repeating_radial_gradient (GtkSnapshot            *snapshot,
                                               const graphene_rect_t  *bounds,
                                               const graphene_point_t *center,
                                               float                   hradius,
                                               float                   vradius,
                                               float                   start,
                                               float                   end,
                                               const GskColorStop     *stops,
                                               gsize                   n_stops)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float scale_x, scale_y, dx, dy;
  gboolean need_gradient = FALSE;
  const GdkRGBA *first_color;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (center != NULL);
  g_return_if_fail (stops != NULL);
  g_return_if_fail (n_stops > 1);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);

  first_color = &stops[0].color;
  for (gsize i = 0; i < n_stops; i ++)
    {
      if (!gdk_rgba_equal (first_color, &stops[i].color))
        {
          need_gradient = TRUE;
          break;
        }
    }

  if (need_gradient)
    {
      graphene_point_t real_center;

      real_center.x = scale_x * center->x + dx;
      real_center.y = scale_y * center->y + dy;
      node = gsk_repeating_radial_gradient_node_new (&real_bounds,
                                                     &real_center,
                                                     hradius * scale_x,
                                                     vradius * scale_y,
                                                     start,
                                                     end,
                                                     stops,
                                                     n_stops);
    }
  else
    {
      node = gsk_color_node_new (first_color, &real_bounds);
    }

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_border:
 * @snapshot: a `GtkSnapshot`
 * @outline: the outline of the border
 * @border_width: (array fixed-size=4): the stroke width of the border on
 *   the top, right, bottom and left side respectively.
 * @border_color: (array fixed-size=4): the color used on the top, right,
 *   bottom and left side.
 *
 * Appends a stroked border rectangle inside the given @outline.
 *
 * The four sides of the border can have different widths and colors.
 */
void
gtk_snapshot_append_border (GtkSnapshot          *snapshot,
                            const GskRoundedRect *outline,
                            const float           border_width[4],
                            const GdkRGBA         border_color[4])
{
  GskRenderNode *node;
  GskRoundedRect real_outline;
  float scale_x, scale_y, dx, dy;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (outline != NULL);
  g_return_if_fail (border_width != NULL);
  g_return_if_fail (border_color != NULL);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gsk_rounded_rect_scale_affine (&real_outline, outline, scale_x, scale_y, dx, dy);

  node = gsk_border_node_new (&real_outline,
                              (float[4]) {
                                border_width[0] * scale_y,
                                border_width[1] * scale_x,
                                border_width[2] * scale_y,
                                border_width[3] * scale_x,
                              },
                              border_color);

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_inset_shadow:
 * @snapshot: a `GtkSnapshot`
 * @outline: outline of the region surrounded by shadow
 * @color: color of the shadow
 * @dx: horizontal offset of shadow
 * @dy: vertical offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Appends an inset shadow into the box given by @outline.
 */
void
gtk_snapshot_append_inset_shadow (GtkSnapshot          *snapshot,
                                  const GskRoundedRect *outline,
                                  const GdkRGBA        *color,
                                  float                 dx,
                                  float                 dy,
                                  float                 spread,
                                  float                 blur_radius)
{
  GskRenderNode *node;
  GskRoundedRect real_outline;
  float scale_x, scale_y, x, y;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (outline != NULL);
  g_return_if_fail (color != NULL);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &x, &y);
  gsk_rounded_rect_scale_affine (&real_outline, outline, scale_x, scale_y, x, y);

  node = gsk_inset_shadow_node_new (&real_outline,
                                    color,
                                    scale_x * dx,
                                    scale_y * dy,
                                    spread,
                                    blur_radius);

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_outset_shadow:
 * @snapshot: a `GtkSnapshot`
 * @outline: outline of the region surrounded by shadow
 * @color: color of the shadow
 * @dx: horizontal offset of shadow
 * @dy: vertical offset of shadow
 * @spread: how far the shadow spreads towards the outside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Appends an outset shadow node around the box given by @outline.
 */
void
gtk_snapshot_append_outset_shadow (GtkSnapshot          *snapshot,
                                   const GskRoundedRect *outline,
                                   const GdkRGBA        *color,
                                   float                 dx,
                                   float                 dy,
                                   float                 spread,
                                   float                 blur_radius)
{
  GskRenderNode *node;
  GskRoundedRect real_outline;
  float scale_x, scale_y, x, y;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (outline != NULL);
  g_return_if_fail (color != NULL);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &x, &y);
  gsk_rounded_rect_scale_affine (&real_outline, outline, scale_x, scale_y, x, y);

  node = gsk_outset_shadow_node_new (&real_outline,
                                     color,
                                     scale_x * dx,
                                     scale_y * dy,
                                     spread,
                                     blur_radius);


  gtk_snapshot_append_node_internal (snapshot, node);
}

static GskRenderNode *
gtk_snapshot_collect_subsurface (GtkSnapshot      *snapshot,
                                 GtkSnapshotState *state,
                                 GskRenderNode   **nodes,
                                 guint             n_nodes)
{
  GskRenderNode *node, *subsurface_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  subsurface_node = gsk_subsurface_node_new (node, state->data.subsurface.subsurface);
  gsk_render_node_unref (node);

  return subsurface_node;
}

static void
gtk_snapshot_clear_subsurface (GtkSnapshotState *state)
{
  g_object_unref (state->data.subsurface.subsurface);
}

void
gtk_snapshot_push_subsurface (GtkSnapshot   *snapshot,
                              GdkSubsurface *subsurface)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->transform,
                                   gtk_snapshot_collect_subsurface,
                                   gtk_snapshot_clear_subsurface);

  state->data.subsurface.subsurface = g_object_ref (subsurface);
}
