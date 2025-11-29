#include "config.h"

#include "gskbroadwayrenderer.h"

#include "broadway/gdkprivate-broadway.h"

#include "gskdebugprivate.h"
#include "gsktransformprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gdk/gdktextureprivate.h"

struct _GskBroadwayRenderer
{
  GskRenderer parent_instance;
  GdkBroadwayDrawContext *draw_context;
  guint32 next_node_id;

  /* Set during rendering */
  GArray *nodes;              /* Owned by draw_contex */
  GPtrArray *node_textures;   /* Owned by draw_contex */
  GHashTable *node_lookup;

  /* Kept from last frame */
  GHashTable *last_node_lookup;
  GskRenderNode *last_root; /* Owning refs to the things in last_node_lookup */
};

struct _GskBroadwayRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskBroadwayRenderer, gsk_broadway_renderer, GSK_TYPE_RENDERER)

static gboolean
gsk_broadway_renderer_realize (GskRenderer  *renderer,
                               GdkDisplay   *display,
                               GdkSurface   *surface,
                               GError      **error)
{
  GskBroadwayRenderer *self = GSK_BROADWAY_RENDERER (renderer);

  if (!GDK_IS_BROADWAY_SURFACE (surface))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "Broadway renderer only works for broadway surfaces");
      return FALSE;
    }

  self->draw_context = gdk_broadway_draw_context_context (surface);

  return TRUE;
}

static void
gsk_broadway_renderer_unrealize (GskRenderer *renderer)
{
  GskBroadwayRenderer *self = GSK_BROADWAY_RENDERER (renderer);
  g_clear_object (&self->draw_context);
}

static GdkTexture *
gsk_broadway_renderer_render_texture (GskRenderer           *renderer,
                                      GskRenderNode         *root,
                                      const graphene_rect_t *viewport)
{
  GdkTexture *texture;
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ceil (viewport->size.width), ceil (viewport->size.height));
  cr = cairo_create (surface);

  cairo_translate (cr, - viewport->origin.x, - viewport->origin.y);

  gsk_render_node_draw (root, cr);

  cairo_destroy (cr);

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

/* uint32 is sent in native endianness, and then converted to little endian in broadwayd when sending to browser */
static void
add_uint32 (GArray *nodes, guint32 v)
{
  g_array_append_val (nodes, v);
}

static guint
add_uint32_placeholder (GArray *nodes)
{
  guint pos = nodes->len;
  guint32 v = 0;

  g_array_append_val (nodes, v);
  return pos;
}

static void
set_uint32_at (GArray *nodes, guint index, guint32 v)
{
  g_array_index (nodes, guint32, index) = v;
}


static void
add_float (GArray *nodes, float f)
{
  union {
    float f;
    guint32 i;
  } u;

  u.f = f;
  g_array_append_val (nodes, u.i);
}

static guint32
rgba_to_uint32 (const GdkRGBA *rgba)
{
  return
    ((guint32)(0.5 + CLAMP (rgba->alpha, 0., 1.) * 255.) << 24) |
    ((guint32)(0.5 + CLAMP (rgba->red, 0., 1.) * 255.) << 16) |
    ((guint32)(0.5 + CLAMP (rgba->green, 0., 1.) * 255.) << 8) |
    ((guint32)(0.5 + CLAMP (rgba->blue, 0., 1.) * 255.) << 0);
}


static void
add_rgba (GArray *nodes, const GdkRGBA *rgba)
{
  guint32 c = rgba_to_uint32 (rgba);
  g_array_append_val (nodes, c);
}

static void
add_xy (GArray *nodes, float x, float y, float offset_x, float offset_y)
{
  add_float (nodes, x - offset_x);
  add_float (nodes, y - offset_y);
}

static void
add_point (GArray *nodes, const graphene_point_t *point, float offset_x, float offset_y)
{
  add_xy (nodes, point->x, point->y, offset_x, offset_y);
}

static void
add_size (GArray *nodes, const graphene_size_t *size)
{
  add_float (nodes, size->width);
  add_float (nodes, size->height);
}

static void
add_rect (GArray *nodes, const graphene_rect_t *rect, float offset_x, float offset_y)
{
  add_point (nodes, &rect->origin, offset_x, offset_y);
  add_size (nodes, &rect->size);
}

static void
add_rounded_rect (GArray *nodes, const GskRoundedRect *rrect, float offset_x, float offset_y)
{
  int i;
  add_rect (nodes, &rrect->bounds, offset_x, offset_y);
  for (i = 0; i < 4; i++)
    add_size (nodes, &rrect->corner[i]);
}

static void
add_matrix (GArray *nodes, graphene_matrix_t *matrix)
{
  float matrix_floats[16];
  int i;

  graphene_matrix_to_float (matrix, matrix_floats);
  for (i = 0; i < 16; i++)
    add_float (nodes, matrix_floats[i]);
}

static void
add_color_stop (GArray *nodes, const GskColorStop *stop)
{
  add_float (nodes, stop->offset);
  add_rgba (nodes, &stop->color);
}

static void
add_string (GArray *nodes, const char *str)
{
  guint32 len = strlen(str);
  guint32 v, c;

  add_uint32 (nodes, len);

  v = 0;
  c = 0;
  while (*str != 0)
    {
      v |= (*str++) << 8*c++;
      if (c == 4)
        {
          add_uint32 (nodes, v);
          v = 0;
          c = 0;
        }
    }

  if (c != 0)
    add_uint32 (nodes, v);
}

static void
collect_reused_child_nodes (GskRenderer *renderer,
                            GskRenderNode *node);

static void
collect_reused_node (GskRenderer *renderer,
                     GskRenderNode *node)
{
  GskBroadwayRenderer *self = GSK_BROADWAY_RENDERER (renderer);
  guint32 old_id;

  if (self->last_node_lookup &&
      (old_id = GPOINTER_TO_INT(g_hash_table_lookup (self->last_node_lookup, node))) != 0)
    g_hash_table_insert (self->node_lookup, node, GINT_TO_POINTER (old_id));

  collect_reused_child_nodes (renderer, node);
}


static void
collect_reused_child_nodes (GskRenderer *renderer,
                            GskRenderNode *node)
{
  guint i;

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      return;

      /* Leaf nodes */

    case GSK_TEXTURE_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_CAIRO_NODE:
    case GSK_COLOR_NODE:
    case GSK_BORDER_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_LINEAR_GRADIENT_NODE:

      /* Fallbacks (=> leaf for now */
    case GSK_GL_SHADER_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_TEXT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_REPEAT_NODE:
    case GSK_BLEND_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_BLUR_NODE:
    case GSK_MASK_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_SUBSURFACE_NODE:

    default:

      break;

      /* Bin nodes */

    case GSK_SHADOW_NODE:
      collect_reused_node (renderer,
                           gsk_shadow_node_get_child (node));
      break;

    case GSK_OPACITY_NODE:
      collect_reused_node (renderer,
                           gsk_opacity_node_get_child (node));
      break;

    case GSK_ROUNDED_CLIP_NODE:
      collect_reused_node (renderer,
                           gsk_rounded_clip_node_get_child (node));
      break;

    case GSK_CLIP_NODE:
      collect_reused_node (renderer,
                           gsk_clip_node_get_child (node));
      break;

    case GSK_TRANSFORM_NODE:
      collect_reused_node (renderer,
                           gsk_transform_node_get_child (node));
      break;

    case GSK_DEBUG_NODE:
      collect_reused_node (renderer,
                           gsk_debug_node_get_child (node));
      break;

      /* Generic nodes */

    case GSK_CONTAINER_NODE:
      for (i = 0; i < gsk_container_node_get_n_children (node); i++)
        collect_reused_node (renderer,
                             gsk_container_node_get_child (node, i));
      break;

      break; /* Fallback */
    }
}

static gboolean
node_is_visible (GskRenderNode *node,
                 graphene_rect_t *clip_bounds)
{
  if (clip_bounds == NULL ||
      graphene_rect_intersection (clip_bounds, &node->bounds, NULL))
    return TRUE;

  return FALSE;
}

static gboolean
node_is_fully_visible (GskRenderNode *node,
                       graphene_rect_t *clip_bounds)
{
  if (clip_bounds == NULL ||
      graphene_rect_contains_rect (clip_bounds, &node->bounds))
    return TRUE;

  return FALSE;
}

static gboolean
node_type_is_container (BroadwayNodeType type)
{
  return
    type == BROADWAY_NODE_SHADOW ||
    type == BROADWAY_NODE_OPACITY ||
    type == BROADWAY_NODE_ROUNDED_CLIP ||
    type == BROADWAY_NODE_CLIP ||
    type == BROADWAY_NODE_TRANSFORM ||
    type == BROADWAY_NODE_DEBUG ||
    type == BROADWAY_NODE_CONTAINER;
}

static gboolean
add_new_node (GskRenderer *renderer,
              GskRenderNode *node,
              BroadwayNodeType type,
              graphene_rect_t *clip_bounds)
{
  GskBroadwayRenderer *self = GSK_BROADWAY_RENDERER (renderer);
  guint32 id, old_id;

  if (self->last_node_lookup &&
      (old_id = GPOINTER_TO_INT (g_hash_table_lookup (self->last_node_lookup, node))) != 0)
    {
      add_uint32 (self->nodes, BROADWAY_NODE_REUSE);
      add_uint32 (self->nodes, old_id);

      g_hash_table_insert (self->node_lookup, node, GINT_TO_POINTER(old_id));
      collect_reused_child_nodes (renderer, node);

      return FALSE;
    }

  id = ++self->next_node_id;

  /* Never try to reuse partially visible container types the next
   * frame, as they could be be partial due to pruning against clip_bounds,
   * and the clip_bounds may be different the next frame. However, anything
   * that is fully visible will not be pruned, so is ok to reuse.
   *
   * Note: its quite possible that the node is fully visible, but contains
   * a clip node which means the tree under that partial. That is fine and we can
   * still reuse *this* node next frame, but we can't use the child that is
   * partial, for example in a different place, because then it might see
   * the partial region of the tree.
   */
  if (!node_type_is_container (type) ||
      node_is_fully_visible (node, clip_bounds))
    g_hash_table_insert (self->node_lookup, node, GINT_TO_POINTER(id));

  add_uint32 (self->nodes, type);
  add_uint32 (self->nodes, id);

  return TRUE;
}

typedef struct ColorizedTexture {
  GdkTexture *texture;
  graphene_matrix_t color_matrix;
  graphene_vec4_t color_offset;
} ColorizedTexture;

static void
colorized_texture_free (ColorizedTexture *colorized)
{
  g_object_unref (colorized->texture);
  g_free (colorized);
}

static ColorizedTexture *
colorized_texture_new (GdkTexture *texture,
                       const graphene_matrix_t *color_matrix,
                       const graphene_vec4_t *color_offset)
{
  ColorizedTexture *colorized = g_new0 (ColorizedTexture, 1);
  colorized->texture = g_object_ref (texture);
  colorized->color_matrix = *color_matrix;
  colorized->color_offset = *color_offset;
  return colorized;
}

static void
colorized_texture_free_list (GList *list)
{
  g_list_free_full (list, (GDestroyNotify)colorized_texture_free);
}


static gboolean
matrix_equal (const graphene_matrix_t *a,
              const graphene_matrix_t *b)
{
  for (int i = 0; i < 4; i ++)
    {
      graphene_vec4_t ra, rb;
      graphene_matrix_get_row (a, i, &ra);
      graphene_matrix_get_row (b, i, &rb);
      if (!graphene_vec4_equal (&ra, &rb))
        return FALSE;
    }
  return TRUE;
}

static GdkTexture *
get_colorized_texture (GdkTexture *texture,
                       const graphene_matrix_t *color_matrix,
                       const graphene_vec4_t *color_offset)
{
  cairo_surface_t *surface;
  cairo_surface_t *image_surface;
  graphene_vec4_t pixel;
  guint32* pixel_data;
  guchar *data;
  gsize x, y, width, height, stride;
  float alpha;
  GdkTexture *colorized_texture;
  GList *colorized_list, *l;
  ColorizedTexture *colorized;

  colorized_list = g_object_get_data (G_OBJECT (texture), "broadway-colorized");

  for (l = colorized_list; l != NULL; l = l->next)
    {
      colorized = l->data;

      if (graphene_vec4_equal (&colorized->color_offset, color_offset) &&
          matrix_equal (&colorized->color_matrix, color_matrix))
        return g_object_ref (colorized->texture);
    }

  surface = gdk_texture_download_surface (texture);
  image_surface = cairo_surface_map_to_image (surface, NULL);
  data = cairo_image_surface_get_data (image_surface);
  width = cairo_image_surface_get_width (image_surface);
  height = cairo_image_surface_get_height (image_surface);
  stride = cairo_image_surface_get_stride (image_surface);

  for (y = 0; y < height; y++)
    {
      pixel_data = (guint32 *) data;
      for (x = 0; x < width; x++)
        {
          alpha = ((pixel_data[x] >> 24) & 0xFF) / 255.0;

          if (alpha == 0)
            {
              graphene_vec4_init (&pixel, 0.0, 0.0, 0.0, 0.0);
            }
          else
            {
              graphene_vec4_init (&pixel,
                                  ((pixel_data[x] >> 16) & 0xFF) / (255.0 * alpha),
                                  ((pixel_data[x] >>  8) & 0xFF) / (255.0 * alpha),
                                  ( pixel_data[x]        & 0xFF) / (255.0 * alpha),
                                  alpha);
              graphene_matrix_transform_vec4 (color_matrix, &pixel, &pixel);
            }

          graphene_vec4_add (&pixel, color_offset, &pixel);

          alpha = graphene_vec4_get_w (&pixel);
          if (alpha > 0.0)
            {
              alpha = MIN (alpha, 1.0);
              pixel_data[x] = (((guint32) (alpha * 255)) << 24) |
                              (((guint32) (CLAMP (graphene_vec4_get_x (&pixel), 0, 1) * alpha * 255)) << 16) |
                              (((guint32) (CLAMP (graphene_vec4_get_y (&pixel), 0, 1) * alpha * 255)) <<  8) |
                               ((guint32) (CLAMP (graphene_vec4_get_z (&pixel), 0, 1) * alpha * 255));
            }
          else
            {
              pixel_data[x] = 0;
            }
        }
      data += stride;
    }

  cairo_surface_mark_dirty (image_surface);
  cairo_surface_unmap_image (surface, image_surface);

  colorized_texture = gdk_texture_new_for_surface (surface);

  colorized = colorized_texture_new (colorized_texture, color_matrix, color_offset);
  if (colorized_list)
    colorized_list = g_list_append (colorized_list, colorized);
  else
    {
      colorized_list = g_list_append (NULL, colorized);
      g_object_set_data_full (G_OBJECT (texture), "broadway-colorized",
                              colorized_list, (GDestroyNotify)colorized_texture_free_list);
    }

  cairo_surface_destroy (surface);

  return colorized_texture;
}


/* Note: This tracks the offset so that we can convert
 * the absolute coordinates of the GskRenderNodes to
 * parent-relative which is what the dom uses, and
 * which is good for re-using subtrees.
 *
 * We also track the clip bounds which is a best-effort
 * clip region tracking (i.e. can be unset or larger
 * than real clip, but not smaller). This can be used
 * to avoid sending completely clipped nodes.
 */
static void
gsk_broadway_renderer_add_node (GskRenderer *renderer,
                                GskRenderNode *node,
                                float offset_x,
                                float offset_y,
                                graphene_rect_t *clip_bounds)
{
  GdkDisplay *display = gdk_surface_get_display (gsk_renderer_get_surface (renderer));
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);
  GskBroadwayRenderer *self = GSK_BROADWAY_RENDERER (renderer);
  GArray *nodes = self->nodes;

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      return;

    /* Leaf nodes */

    case GSK_TEXTURE_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_TEXTURE, clip_bounds))
        {
          GdkTexture *texture = gsk_texture_node_get_texture (node);
          guint32 texture_id;

          /* No need to add to self->node_textures here, the node will keep it alive until end of frame. */

          texture_id = gdk_broadway_display_ensure_texture (display, texture);

          add_rect (nodes, &node->bounds, offset_x, offset_y);
          add_uint32 (nodes, texture_id);
        }
      return;

    case GSK_CAIRO_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_TEXTURE, clip_bounds))
        {
          cairo_surface_t *surface = gsk_cairo_node_get_surface (node);
          cairo_surface_t *image_surface = NULL;
          GdkTexture *texture;
          guint32 texture_id;

          if (surface == NULL)
            return;
          if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE)
            image_surface = cairo_surface_reference (surface);
          else
            {
              cairo_t *cr;
              image_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                          ceilf (node->bounds.size.width),
                                                          ceilf (node->bounds.size.height));
              cr = cairo_create (image_surface);
              cairo_set_source_surface (cr, surface, 0, 0);
              cairo_rectangle (cr, 0, 0, node->bounds.size.width, node->bounds.size.height);
              cairo_fill (cr);
              cairo_destroy (cr);
            }

          texture = gdk_texture_new_for_surface (image_surface);
          g_ptr_array_add (self->node_textures, texture); /* Transfers ownership to node_textures */
          texture_id = gdk_broadway_display_ensure_texture (display, texture);

          add_rect (nodes, &node->bounds, offset_x, offset_y);
          add_uint32 (nodes, texture_id);

          cairo_surface_destroy (image_surface);
        }
      return;

    case GSK_COLOR_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_COLOR, clip_bounds))
        {
          add_rect (nodes, &node->bounds, offset_x, offset_y);
          add_rgba (nodes, gsk_color_node_get_color (node));
        }
      return;

    case GSK_BORDER_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_BORDER, clip_bounds))
        {
          int i;
          add_rounded_rect (nodes, gsk_border_node_get_outline (node), offset_x, offset_y);
          for (i = 0; i < 4; i++)
            add_float (nodes, gsk_border_node_get_widths (node)[i]);
          for (i = 0; i < 4; i++)
            add_rgba (nodes, &gsk_border_node_get_colors (node)[i]);
        }
      return;

    case GSK_OUTSET_SHADOW_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_OUTSET_SHADOW, clip_bounds))
        {
          add_rounded_rect (nodes, gsk_outset_shadow_node_get_outline (node), offset_x, offset_y);
          add_rgba (nodes, gsk_outset_shadow_node_get_color (node));
          add_float (nodes, gsk_outset_shadow_node_get_dx (node));
          add_float (nodes, gsk_outset_shadow_node_get_dy (node));
          add_float (nodes, gsk_outset_shadow_node_get_spread (node));
          add_float (nodes, gsk_outset_shadow_node_get_blur_radius (node));
        }
      return;

    case GSK_INSET_SHADOW_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_INSET_SHADOW, clip_bounds))
        {
          add_rounded_rect (nodes, gsk_inset_shadow_node_get_outline (node), offset_x, offset_y);
          add_rgba (nodes, gsk_inset_shadow_node_get_color (node));
          add_float (nodes, gsk_inset_shadow_node_get_dx (node));
          add_float (nodes, gsk_inset_shadow_node_get_dy (node));
          add_float (nodes, gsk_inset_shadow_node_get_spread (node));
          add_float (nodes, gsk_inset_shadow_node_get_blur_radius (node));
        }
      return;

    case GSK_LINEAR_GRADIENT_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_LINEAR_GRADIENT, clip_bounds))
        {
          guint i, n;

          add_rect (nodes, &node->bounds, offset_x, offset_y);
          add_point (nodes, gsk_linear_gradient_node_get_start (node), offset_x, offset_y);
          add_point (nodes, gsk_linear_gradient_node_get_end (node), offset_x, offset_y);
          n = gsk_linear_gradient_node_get_n_color_stops (node);
          add_uint32 (nodes, n);
          for (i = 0; i < n; i++)
            add_color_stop (nodes, &gsk_linear_gradient_node_get_color_stops (node, NULL)[i]);
        }
      return;

      /* Bin nodes */

    case GSK_SHADOW_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_SHADOW, clip_bounds))
        {
          gsize i, n_shadows = gsk_shadow_node_get_n_shadows (node);

          add_uint32 (nodes, n_shadows);
          for (i = 0; i < n_shadows; i++)
            {
              const GskShadow *shadow = gsk_shadow_node_get_shadow (node, i);
              add_rgba (nodes, &shadow->color);
              add_float (nodes, shadow->dx);
              add_float (nodes, shadow->dy);
              add_float (nodes, shadow->radius);
            }
          gsk_broadway_renderer_add_node (renderer,
                                          gsk_shadow_node_get_child (node),
                                          offset_x, offset_y, clip_bounds);
        }
      return;

    case GSK_OPACITY_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_OPACITY, clip_bounds))
        {
          add_float (nodes, gsk_opacity_node_get_opacity (node));
          gsk_broadway_renderer_add_node (renderer,
                                          gsk_opacity_node_get_child (node),
                                          offset_x, offset_y, clip_bounds);
        }
      return;

    case GSK_ROUNDED_CLIP_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_ROUNDED_CLIP, clip_bounds))
        {
          const GskRoundedRect *rclip = gsk_rounded_clip_node_get_clip (node);
          graphene_rect_t child_bounds = rclip->bounds;

          if (clip_bounds)
            graphene_rect_intersection (&child_bounds, clip_bounds, &child_bounds);

          add_rounded_rect (nodes, rclip, offset_x, offset_y);
          gsk_broadway_renderer_add_node (renderer,
                                          gsk_rounded_clip_node_get_child (node),
                                          rclip->bounds.origin.x,
                                          rclip->bounds.origin.y,
                                          &child_bounds);
        }
      return;

    case GSK_CLIP_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_CLIP, clip_bounds))
        {
          const graphene_rect_t *clip = gsk_clip_node_get_clip (node);
          graphene_rect_t child_bounds = *clip;

          if (clip_bounds)
            graphene_rect_intersection (&child_bounds, clip_bounds, &child_bounds);

          add_rect (nodes, clip, offset_x, offset_y);
          gsk_broadway_renderer_add_node (renderer,
                                          gsk_clip_node_get_child (node),
                                          clip->origin.x,
                                          clip->origin.y,
                                          &child_bounds);
        }
      return;

    case GSK_TRANSFORM_NODE:
      {
        GskTransform *transform = gsk_transform_node_get_transform (node);
        GskTransformCategory category = gsk_transform_get_category (transform);

        if (add_new_node (renderer, node, BROADWAY_NODE_TRANSFORM, clip_bounds)) {
          if (category >= GSK_TRANSFORM_CATEGORY_2D_TRANSLATE)
            {
              float dx, dy;
              graphene_rect_t child_bounds;
              graphene_rect_t *child_bounds_p = NULL;

              gsk_transform_to_translate (transform, &dx, &dy);
              add_uint32 (nodes, 0); // Translate
              add_xy (nodes, dx, dy, 0, 0);

              if (clip_bounds)
                {
                  graphene_rect_offset_r (clip_bounds, -dx, -dy, &child_bounds);
                  child_bounds_p = &child_bounds;
                }

              gsk_broadway_renderer_add_node (renderer,
                                              gsk_transform_node_get_child (node),
                                              0, 0, child_bounds_p);
            }
          else
            {
              graphene_matrix_t matrix;

              gsk_transform_to_matrix (transform, &matrix);
              add_uint32 (nodes, 1); // General transform
              add_matrix (nodes, &matrix);
              // We just drop the clip bounds here to make things simpler
              gsk_broadway_renderer_add_node (renderer,
                                              gsk_transform_node_get_child (node),
                                              0, 0, NULL);
            }
        }
      }
      return;

    case GSK_DEBUG_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_DEBUG, clip_bounds))
        {
          const char *message = gsk_debug_node_get_message (node);
          add_string (nodes, message);
          gsk_broadway_renderer_add_node (renderer,
                                          gsk_debug_node_get_child (node), offset_x, offset_y, clip_bounds);
        }
      return;

    case GSK_SUBSURFACE_NODE:
      gsk_broadway_renderer_add_node (renderer,
                                      gsk_subsurface_node_get_child (node), offset_x, offset_y, clip_bounds);
     return;

      /* Generic nodes */

    case GSK_CONTAINER_NODE:
      if (add_new_node (renderer, node, BROADWAY_NODE_CONTAINER, clip_bounds))
        {
          guint i, placeholder;
          guint32 n_children = 0;

          placeholder = add_uint32_placeholder (nodes);
          for (i = 0; i < gsk_container_node_get_n_children (node); i++)
            {
              /* We prune fully clipped children, but we only do this for container_node, as
               * we don't have a way for any other nodes to say there are children missing (i.e.
               * bins always assume there is a child).
               * Pruning is really only useful for large sets of children anyway, so that's
               * probably fine. */
              GskRenderNode *child = gsk_container_node_get_child (node, i);
              if (node_is_visible (child, clip_bounds))
                {
                  n_children++;
                  gsk_broadway_renderer_add_node (renderer,
                                                  child, offset_x, offset_y, clip_bounds);
                }
            }
          set_uint32_at (nodes, placeholder, n_children);
        }
      return;

    case GSK_COLOR_MATRIX_NODE:
      {
        GskRenderNode *child = gsk_color_matrix_node_get_child (node);
        if (gsk_render_node_get_node_type (child) == GSK_TEXTURE_NODE)
          {
            const graphene_matrix_t *color_matrix = gsk_color_matrix_node_get_color_matrix (node);
            const graphene_vec4_t *color_offset = gsk_color_matrix_node_get_color_offset (node);
            GdkTexture *texture = gsk_texture_node_get_texture (child);
            GdkTexture *colorized_texture = get_colorized_texture (texture, color_matrix, color_offset);
            if (add_new_node (renderer, node, BROADWAY_NODE_TEXTURE, clip_bounds))
              {
                guint32 texture_id = gdk_broadway_display_ensure_texture (display, colorized_texture);
                add_rect (nodes, &child->bounds, offset_x, offset_y);
                add_uint32 (nodes, texture_id);
              }

            return;
          }
      }
      break; /* Fallback */

    case GSK_MASK_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_TEXT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_REPEAT_NODE:
    case GSK_BLEND_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_BLUR_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    default:
      break; /* Fallback */
    }

  if (add_new_node (renderer, node, BROADWAY_NODE_TEXTURE, clip_bounds))
    {
      GdkTexture *texture;
      cairo_surface_t *surface;
      cairo_t *cr;
      guint32 texture_id;
      int x = floorf (node->bounds.origin.x);
      int y = floorf (node->bounds.origin.y);
      int width = ceil (node->bounds.origin.x + node->bounds.size.width) - x;
      int height = ceil (node->bounds.origin.y + node->bounds.size.height) - y;
      int scale = broadway_display->scale_factor;

#define MAX_IMAGE_SIZE 32767

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            MIN (width * scale, MAX_IMAGE_SIZE),
                                            MIN (height * scale, MAX_IMAGE_SIZE));

#undef MAX_IMAGE_SIZE

      cr = cairo_create (surface);
      cairo_scale (cr, scale, scale);
      cairo_translate (cr, -x, -y);
      gsk_render_node_draw (node, cr);
      cairo_destroy (cr);

      texture = gdk_texture_new_for_surface (surface);
      g_ptr_array_add (self->node_textures, texture); /* Transfers ownership to node_textures */

      texture_id = gdk_broadway_display_ensure_texture (display, texture);
      add_float (nodes, x - offset_x);
      add_float (nodes, y - offset_y);
      add_float (nodes, width);
      add_float (nodes, height);
      add_uint32 (nodes, texture_id);

      cairo_surface_destroy (surface);
    }
}

static void
gsk_broadway_renderer_render (GskRenderer          *renderer,
                              GskRenderNode        *root,
                              const cairo_region_t *update_area)
{
  GskBroadwayRenderer *self = GSK_BROADWAY_RENDERER (renderer);

  self->node_lookup = g_hash_table_new (g_direct_hash, g_direct_equal);

  gdk_draw_context_begin_frame (GDK_DRAW_CONTEXT (self->draw_context), update_area);

  /* These are owned by the draw context between begin and end, but
     cache them here for easier access during the render */
  self->nodes = self->draw_context->nodes;
  self->node_textures = self->draw_context->node_textures;

  gsk_broadway_renderer_add_node (renderer, root, 0, 0, NULL);

  self->nodes = NULL;
  self->node_textures = NULL;

  gdk_draw_context_end_frame (GDK_DRAW_CONTEXT (self->draw_context));

  if (self->last_node_lookup)
    g_hash_table_unref (self->last_node_lookup);
  self->last_node_lookup = self->node_lookup;
  self->node_lookup = NULL;

  if (self->last_root)
    gsk_render_node_unref (self->last_root);
  self->last_root = gsk_render_node_ref (root);

  if (self->next_node_id > G_MAXUINT32 / 2)
    {
      /* We're "near" a wrap of the ids, lets avoid reusing any of
       * these nodes next frame, then we can reset the id counter
       * without risk of any old nodes sticking around and conflicting. */

      g_hash_table_remove_all (self->last_node_lookup);
      self->next_node_id = 0;
    }
}

static void
gsk_broadway_renderer_class_init (GskBroadwayRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_broadway_renderer_realize;
  renderer_class->unrealize = gsk_broadway_renderer_unrealize;
  renderer_class->render = gsk_broadway_renderer_render;
  renderer_class->render_texture = gsk_broadway_renderer_render_texture;
}

static void
gsk_broadway_renderer_init (GskBroadwayRenderer *self)
{
}

/**
 * gsk_broadway_renderer_new:
 *
 * Creates a new Broadway renderer.
 *
 * The Broadway renderer is the default renderer for the broadway backend.
 * It will only work with broadway surfaces, otherwise it will fail the
 * call to gsk_renderer_realize().
 *
 * This function is only available when GTK was compiled with Broadway
 * support.
 *
 * Returns: a new Broadway renderer.
 **/
GskRenderer *
gsk_broadway_renderer_new (void)
{
  return g_object_new (GSK_TYPE_BROADWAY_RENDERER, NULL);
}
