#include <gtk/gtk.h>

void
replay_node (GskRenderNode *node, GtkSnapshot *snapshot);

static void
replay_container_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  for (guint i = 0; i < gsk_container_node_get_n_children (node); i++)
    replay_node (gsk_container_node_get_child (node, i), snapshot);
}

static void
replay_cairo_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  cairo_surface_t *surface = gsk_cairo_node_get_surface (node);
  graphene_rect_t bounds;
  gsk_render_node_get_bounds (node, &bounds);

  cairo_t *cr = gtk_snapshot_append_cairo (snapshot, &bounds);
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);
}

static void
replay_color_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  graphene_rect_t bounds;
  gsk_render_node_get_bounds (node, &bounds);
  gtk_snapshot_append_color (snapshot,
                             gsk_color_node_get_color (node),
                             &bounds);
}

static void
replay_linear_gradient_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  graphene_rect_t bounds;
  const graphene_point_t *start_point, *end_point;
  const GskColorStop *stops;
  gsize n_stops = 0;

  gsk_render_node_get_bounds (node, &bounds);
  start_point = gsk_linear_gradient_node_get_start (node);
  end_point = gsk_linear_gradient_node_get_end (node);
  stops = gsk_linear_gradient_node_get_color_stops (node, &n_stops);

  if (gsk_render_node_get_node_type (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE)
    gtk_snapshot_append_repeating_linear_gradient (snapshot, &bounds,
                                                   start_point, end_point,
                                                   stops, n_stops);
  else
    gtk_snapshot_append_linear_gradient (snapshot, &bounds,
                                         start_point, end_point,
                                         stops, n_stops);
}

static void
replay_radial_gradient_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  graphene_rect_t bounds;
  gsk_render_node_get_bounds (node, &bounds);
  const graphene_point_t *center = gsk_radial_gradient_node_get_center (node);
  float hradius = gsk_radial_gradient_node_get_hradius (node);
  float vradius = gsk_radial_gradient_node_get_vradius (node);
  float start = gsk_radial_gradient_node_get_start (node);
  float end = gsk_radial_gradient_node_get_end (node);
  gsize n_stops = 0;
  const GskColorStop *stops = gsk_radial_gradient_node_get_color_stops (node,
                                                                        &n_stops);

  if (gsk_render_node_get_node_type (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE)
    gtk_snapshot_append_repeating_radial_gradient (snapshot, &bounds, center,
                                                   hradius, vradius, start, end,
                                                   stops, n_stops);
  else
    gtk_snapshot_append_radial_gradient (snapshot, &bounds, center,
                                         hradius, vradius, start, end,
                                         stops, n_stops);
}

static void
replay_conic_gradient_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  graphene_rect_t bounds;
  gsk_render_node_get_bounds (node, &bounds);
  const graphene_point_t *center = gsk_conic_gradient_node_get_center (node);
  float rotation = gsk_conic_gradient_node_get_rotation (node);
  gsize n_stops = 0;
  const GskColorStop *stops = gsk_conic_gradient_node_get_color_stops (node,
                                                                       &n_stops);

  gtk_snapshot_append_conic_gradient (snapshot, &bounds, center,
                                      rotation, stops, n_stops);
}

static void
replay_border_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  const GskRoundedRect *outline = gsk_border_node_get_outline (node);
  const float *border_width = gsk_border_node_get_widths (node);
  const GdkRGBA *border_color = gsk_border_node_get_colors (node);

  gtk_snapshot_append_border (snapshot, outline, border_width, border_color);
}

static void
replay_texture_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  GdkTexture *texture = gsk_texture_node_get_texture (node);
  graphene_rect_t bounds;
  gsk_render_node_get_bounds (node, &bounds);

  gtk_snapshot_append_texture (snapshot, texture, &bounds);
}

static void
replay_inset_shadow_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  const GskRoundedRect *outline = gsk_inset_shadow_node_get_outline (node);
  const GdkRGBA *color = gsk_inset_shadow_node_get_color (node);
  float dx = gsk_inset_shadow_node_get_dx (node);
  float dy = gsk_inset_shadow_node_get_dy (node);
  float spread = gsk_inset_shadow_node_get_spread (node);
  float blur_radius = gsk_inset_shadow_node_get_blur_radius (node);

  gtk_snapshot_append_inset_shadow (snapshot, outline, color,
                                    dx, dy, spread, blur_radius);
}

static void
replay_outset_shadow_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  const GskRoundedRect *outline = gsk_outset_shadow_node_get_outline (node);
  const GdkRGBA *color = gsk_outset_shadow_node_get_color (node);
  float dx = gsk_outset_shadow_node_get_dx (node);
  float dy = gsk_outset_shadow_node_get_dy (node);
  float spread = gsk_outset_shadow_node_get_spread (node);
  float blur_radius = gsk_outset_shadow_node_get_blur_radius (node);

  gtk_snapshot_append_outset_shadow (snapshot, outline, color,
                                     dx, dy, spread, blur_radius);
}

static void
replay_transform_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  GskTransform *transform = gsk_transform_node_get_transform (node);
  GskRenderNode *child = gsk_transform_node_get_child (node);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_transform (snapshot, transform);
  replay_node (child, snapshot);
  gtk_snapshot_restore (snapshot);
}

static void
replay_opacity_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  float opacity = gsk_opacity_node_get_opacity (node);
  GskRenderNode *child = gsk_opacity_node_get_child (node);

  gtk_snapshot_push_opacity (snapshot, opacity);
  replay_node (child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_color_matrix_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  const graphene_matrix_t *matrix = gsk_color_matrix_node_get_color_matrix (node);
  const graphene_vec4_t *offset = gsk_color_matrix_node_get_color_offset (node);
  GskRenderNode *child = gsk_color_matrix_node_get_child (node);

  gtk_snapshot_push_color_matrix (snapshot, matrix, offset);
  replay_node (child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_repeat_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  GskRenderNode *child = gsk_repeat_node_get_child (node);
  const graphene_rect_t *child_bounds = gsk_repeat_node_get_child_bounds (node);
  graphene_rect_t bounds;
  gsk_render_node_get_bounds (node, &bounds);

  gtk_snapshot_push_repeat (snapshot, &bounds, child_bounds);
  replay_node (child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_clip_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  const graphene_rect_t *clip = gsk_clip_node_get_clip (node);
  GskRenderNode *child = gsk_clip_node_get_child (node);

  gtk_snapshot_push_clip (snapshot, clip);
  replay_node (child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_rounded_clip_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  const GskRoundedRect *bounds = gsk_rounded_clip_node_get_clip (node);
  GskRenderNode *child = gsk_rounded_clip_node_get_child (node);

  gtk_snapshot_push_rounded_clip (snapshot, bounds);
  replay_node (child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_shadow_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  gsize n_shadows = gsk_shadow_node_get_n_shadows (node);
  /* Hack: we know GskShadowNode stores shadows in a contiguous array.  */
  const GskShadow *shadow = gsk_shadow_node_get_shadow (node, 0);
  GskRenderNode *child = gsk_shadow_node_get_child (node);

  gtk_snapshot_push_shadow (snapshot, shadow, n_shadows);
  replay_node (child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_blend_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  GskRenderNode *bottom_child = gsk_blend_node_get_bottom_child (node);
  GskRenderNode *top_child = gsk_blend_node_get_top_child (node);
  GskBlendMode blend_mode = gsk_blend_node_get_blend_mode (node);

  gtk_snapshot_push_blend (snapshot, blend_mode);
  replay_node (bottom_child, snapshot);
  gtk_snapshot_pop (snapshot);
  replay_node (top_child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_cross_fade_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  GskRenderNode *start_child = gsk_cross_fade_node_get_start_child (node);
  GskRenderNode *end_child = gsk_cross_fade_node_get_end_child (node);
  float progress = gsk_cross_fade_node_get_progress (node);

  gtk_snapshot_push_cross_fade (snapshot, progress);
  replay_node (start_child, snapshot);
  gtk_snapshot_pop (snapshot);
  replay_node (end_child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_text_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
#if 0
  /* The following does not compile, since gtk_snapshot_append_text () is
   * not exported.  */

  PangoFont *font = gsk_text_node_get_font (node);
  PangoGlyphString glyphs;
  guint n_glyphs = 0;
  glyphs.glyphs = (PangoGlyphInfo *) gsk_text_node_get_glyphs (node, &n_glyphs);
  const GdkRGBA *color = gsk_text_node_get_color (node);
  const graphene_point_t *offset = gsk_text_node_get_offset (node);

  glyphs.num_glyphs = n_glyphs;
  glyphs.log_clusters = NULL;

  gtk_snapshot_append_text (snapshot, font, glyphs, color,
                            offset->x, offset->y);
#else
  gtk_snapshot_append_node (snapshot, node);
#endif
}

static void
replay_blur_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  float radius = gsk_blur_node_get_radius (node);
  GskRenderNode *child = gsk_blur_node_get_child (node);

  gtk_snapshot_push_blur (snapshot, radius);
  replay_node (child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_debug_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  const char *message = gsk_debug_node_get_message (node);
  GskRenderNode *child = gsk_debug_node_get_child (node);

  gtk_snapshot_push_debug (snapshot, "%s", message);
  replay_node (child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_gl_shader_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  graphene_rect_t bounds;
  gsk_render_node_get_bounds (node, &bounds);
  GskGLShader *shader = gsk_gl_shader_node_get_shader (node);
  GBytes *args = gsk_gl_shader_node_get_args (node);

  gtk_snapshot_push_gl_shader (snapshot, shader, &bounds, g_bytes_ref (args));
  for (guint i = 0; i < gsk_gl_shader_node_get_n_children (node); i++)
    {
      GskRenderNode *child = gsk_gl_shader_node_get_child (node, i);
      replay_node (child, snapshot);
      gtk_snapshot_gl_shader_pop_texture (snapshot);
    }
  gtk_snapshot_pop (snapshot);
}

static void
replay_texture_scale_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  GdkTexture *texture = gsk_texture_scale_node_get_texture (node);
  GskScalingFilter filter = gsk_texture_scale_node_get_filter (node);
  graphene_rect_t bounds;
  gsk_render_node_get_bounds (node, &bounds);

  gtk_snapshot_append_scaled_texture (snapshot, texture, filter, &bounds);
}

static void
replay_mask_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  GskMaskMode mask_mode = gsk_mask_node_get_mask_mode (node);
  GskRenderNode *source = gsk_mask_node_get_source (node);
  GskRenderNode *mask = gsk_mask_node_get_mask (node);

  gtk_snapshot_push_mask (snapshot, mask_mode);
  replay_node (mask, snapshot);
  gtk_snapshot_pop (snapshot);
  replay_node (source, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_fill_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  GskPath *path = gsk_fill_node_get_path (node);
  GskFillRule fill_rule = gsk_fill_node_get_fill_rule (node);
  GskRenderNode *child = gsk_fill_node_get_child (node);

  gtk_snapshot_push_fill (snapshot, path, fill_rule);
  replay_node (child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
replay_stroke_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  GskPath *path = gsk_stroke_node_get_path (node);
  const GskStroke *stroke = gsk_stroke_node_get_stroke (node);
  GskRenderNode *child = gsk_stroke_node_get_child (node);

  gtk_snapshot_push_stroke (snapshot, path, stroke);
  replay_node (child, snapshot);
  gtk_snapshot_pop (snapshot);
}

void
replay_node (GskRenderNode *node, GtkSnapshot *snapshot)
{
  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_CONTAINER_NODE:
      replay_container_node (node, snapshot);
      break;

    case GSK_CAIRO_NODE:
      replay_cairo_node (node, snapshot);
      break;

    case GSK_COLOR_NODE:
      replay_color_node (node, snapshot);
      break;

    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
      replay_linear_gradient_node (node, snapshot);
      break;

    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
      replay_radial_gradient_node (node, snapshot);
      break;

    case GSK_CONIC_GRADIENT_NODE:
      replay_conic_gradient_node (node, snapshot);
      break;

    case GSK_BORDER_NODE:
      replay_border_node (node, snapshot);
      break;

    case GSK_TEXTURE_NODE:
      replay_texture_node (node, snapshot);
      break;

    case GSK_INSET_SHADOW_NODE:
      replay_inset_shadow_node (node, snapshot);
      break;

    case GSK_OUTSET_SHADOW_NODE:
      replay_outset_shadow_node (node, snapshot);
      break;

    case GSK_TRANSFORM_NODE:
      replay_transform_node (node, snapshot);
      break;

    case GSK_OPACITY_NODE:
      replay_opacity_node (node, snapshot);
      break;

    case GSK_COLOR_MATRIX_NODE:
      replay_color_matrix_node (node, snapshot);
      break;

    case GSK_REPEAT_NODE:
      replay_repeat_node (node, snapshot);
      break;

    case GSK_CLIP_NODE:
      replay_clip_node (node, snapshot);
      break;

    case GSK_ROUNDED_CLIP_NODE:
      replay_rounded_clip_node (node, snapshot);
      break;

    case GSK_SHADOW_NODE:
      replay_shadow_node (node, snapshot);
      break;

    case GSK_BLEND_NODE:
      replay_blend_node (node, snapshot);
      break;

    case GSK_CROSS_FADE_NODE:
      replay_cross_fade_node (node, snapshot);
      break;

    case GSK_TEXT_NODE:
      replay_text_node (node, snapshot);
      break;

    case GSK_BLUR_NODE:
      replay_blur_node (node, snapshot);
      break;

    case GSK_DEBUG_NODE:
      replay_debug_node (node, snapshot);
      break;

    case GSK_GL_SHADER_NODE:
      replay_gl_shader_node (node, snapshot);
      break;

    case GSK_TEXTURE_SCALE_NODE:
      replay_texture_scale_node (node, snapshot);
      break;

    case GSK_MASK_NODE:
      replay_mask_node (node, snapshot);
      break;

    case GSK_FILL_NODE:
      replay_fill_node (node, snapshot);
      break;

    case GSK_STROKE_NODE:
      replay_stroke_node (node, snapshot);
      break;

    case GSK_SUBSURFACE_NODE:
    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert (FALSE);
    }
}
