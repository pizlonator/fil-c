/*
 * Copyright 2023, Red Hat, Inc
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

#include "gskrendernodeattach.h"
#include "gsk/gsk.h"
#include "gsk/gskrendernodeprivate.h"
#include "gdk/gdksurfaceprivate.h"
#include "gdk/gdksubsurfaceprivate.h"


static GskRenderNode *
node_attach (const GskRenderNode *node,
             GdkSurface          *surface,
             int                 *idx)
{
  GskRenderNode *child, *res;

  switch (GSK_RENDER_NODE_TYPE (node))
    {
    case GSK_CAIRO_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_TEXT_NODE:
      return gsk_render_node_ref ((GskRenderNode *)node);

    case GSK_TRANSFORM_NODE:
      child = node_attach (gsk_transform_node_get_child (node), surface, idx);
      res = gsk_transform_node_new (child, gsk_transform_node_get_transform (node));
      gsk_render_node_unref (child);
      return res;

    case GSK_OPACITY_NODE:
      child = node_attach (gsk_opacity_node_get_child (node), surface, idx);
      res = gsk_opacity_node_new (child, gsk_opacity_node_get_opacity (node));
      gsk_render_node_unref (child);
      return res;

    case GSK_COLOR_MATRIX_NODE:
      child = node_attach (gsk_color_matrix_node_get_child (node), surface, idx);
      res = gsk_color_matrix_node_new (child,
                                       gsk_color_matrix_node_get_color_matrix (node),
                                       gsk_color_matrix_node_get_color_offset (node));
      gsk_render_node_unref (child);
      return res;

    case GSK_REPEAT_NODE:
      child = node_attach (gsk_repeat_node_get_child (node), surface, idx);
      res = gsk_repeat_node_new (&node->bounds, child, gsk_repeat_node_get_child_bounds (node));
      gsk_render_node_unref (child);
      return res;

    case GSK_CONTAINER_NODE:
      {
        GskRenderNode **children = g_newa (GskRenderNode *, gsk_container_node_get_n_children (node));
        for (int i = 0; i < gsk_container_node_get_n_children (node); i++)
          children[i] = node_attach (gsk_container_node_get_child (node, i), surface, idx);
        res = gsk_container_node_new (children, gsk_container_node_get_n_children (node));
        for (int i = 0; i < gsk_container_node_get_n_children (node); i++)
          gsk_render_node_unref (children[i]);
        return res;
      }

    case GSK_CLIP_NODE:
      child = node_attach (gsk_clip_node_get_child (node), surface, idx);
      res = gsk_clip_node_new (child, gsk_clip_node_get_clip (node));
      gsk_render_node_unref (child);
      return res;

    case GSK_ROUNDED_CLIP_NODE:
      child = node_attach (gsk_rounded_clip_node_get_child (node), surface, idx);
      res = gsk_rounded_clip_node_new (child, gsk_rounded_clip_node_get_clip (node));
      gsk_render_node_unref (child);
      return res;

    case GSK_SHADOW_NODE:
      {
        GskShadow *shadows = g_newa (GskShadow, gsk_shadow_node_get_n_shadows (node));
        for (int i = 0; i < gsk_shadow_node_get_n_shadows (node); i++)
          shadows[i] = *gsk_shadow_node_get_shadow (node, i);
        child = node_attach (gsk_shadow_node_get_child (node), surface, idx);
        res = gsk_shadow_node_new (child, shadows, gsk_shadow_node_get_n_shadows (node));
        gsk_render_node_unref (child);
        return res;
      }

    case GSK_BLEND_NODE:
      {
        GskRenderNode *top, *bottom;
        bottom = node_attach (gsk_blend_node_get_bottom_child (node), surface, idx);
        top = node_attach (gsk_blend_node_get_top_child (node), surface, idx);
        res = gsk_blend_node_new (bottom, top, gsk_blend_node_get_blend_mode (node));
        gsk_render_node_unref (bottom);
        gsk_render_node_unref (top);
        return res;
      }

    case GSK_CROSS_FADE_NODE:
      {
        GskRenderNode *start, *end;
        start = node_attach (gsk_cross_fade_node_get_start_child (node), surface, idx);
        end = node_attach (gsk_cross_fade_node_get_end_child (node), surface, idx);
        res = gsk_cross_fade_node_new (start, end, gsk_cross_fade_node_get_progress (node));
        gsk_render_node_unref (start);
        gsk_render_node_unref (end);
        return res;
      }

    case GSK_BLUR_NODE:
      child = node_attach (gsk_blur_node_get_child (node), surface, idx);
      res = gsk_blur_node_new (child, gsk_blur_node_get_radius (node));
      gsk_render_node_unref (child);
      return res;

    case GSK_DEBUG_NODE:
      child = node_attach (gsk_debug_node_get_child (node), surface, idx);
      res = gsk_debug_node_new (child, g_strdup (gsk_debug_node_get_message (node)));
      gsk_render_node_unref (child);
      return res;

    case GSK_GL_SHADER_NODE:
      {
        GskRenderNode **children;

        children = g_newa (GskRenderNode *, gsk_gl_shader_node_get_n_children (node));
        for (int i = 0; i < gsk_gl_shader_node_get_n_children (node); i++)
          children[i] = node_attach (gsk_gl_shader_node_get_child (node, i), surface, idx);
        res = gsk_gl_shader_node_new (gsk_gl_shader_node_get_shader (node),
                                      &node->bounds,
                                      gsk_gl_shader_node_get_args (node),
                                      children,
                                      gsk_gl_shader_node_get_n_children (node));
        for (int i = 0; i < gsk_gl_shader_node_get_n_children (node); i++)
          gsk_render_node_unref (children[i]);
        return res;
      }

    case GSK_MASK_NODE:
      {
        GskRenderNode *source, *mask;
        source = node_attach (gsk_mask_node_get_source (node), surface, idx);
        mask = node_attach (gsk_mask_node_get_mask (node), surface, idx);
        res = gsk_mask_node_new (source, mask, gsk_mask_node_get_mask_mode (node));
        gsk_render_node_unref (source);
        gsk_render_node_unref (mask);
        return res;
      }

    case GSK_FILL_NODE:
      child = node_attach (gsk_fill_node_get_child (node), surface, idx);
      res = gsk_fill_node_new (child,
                               gsk_fill_node_get_path (node),
                               gsk_fill_node_get_fill_rule (node));
      gsk_render_node_unref (child);
      return res;

    case GSK_STROKE_NODE:
      child = node_attach (gsk_stroke_node_get_child (node), surface, idx);
      res = gsk_stroke_node_new (child,
                                 gsk_stroke_node_get_path (node),
                                 gsk_stroke_node_get_stroke (node));
      gsk_render_node_unref (child);
      return res;

    case GSK_SUBSURFACE_NODE:
      {
        GdkSubsurface *subsurface;

        g_assert (gsk_subsurface_node_get_subsurface (node) == NULL);

        if (*idx == -1)
          subsurface = gdk_surface_create_subsurface (surface);
        else
          {
            subsurface = gdk_surface_get_subsurface (surface, *idx);
            (*idx)++;
          }
        child = node_attach (gsk_subsurface_node_get_child (node), surface, idx);
        res = gsk_subsurface_node_new (child, subsurface);
        gsk_render_node_unref (child);
        return res;
      }

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    }
}

/* Find all the subsurface nodes in the given tree, and attach them
 * to a subsurface of the given surface. If the surface already has
 * subsurfaces, we assume that we are just reattaching, and that the
 * nodes are still in the same order. Otherwise, we create new
 * subsurfaces.
 */
GskRenderNode *
gsk_render_node_attach (const GskRenderNode *node,
                        GdkSurface          *surface)
{
  int idx;

  if (gdk_surface_get_n_subsurfaces (surface) > 0)
    idx = 0;
  else
    idx = -1;

  return node_attach (node, surface, &idx);
}
