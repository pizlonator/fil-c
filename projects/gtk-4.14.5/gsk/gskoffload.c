/* gskoffload.c
 *
 * Copyright 2023 Red Hat, Inc.
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gskoffloadprivate.h"

#include "gskrendernode.h"
#include "gskrectprivate.h"
#include "gskroundedrectprivate.h"
#include "gsktransform.h"
#include "gskdebugprivate.h"
#include "gskrendernodeprivate.h"
#include "gdksurfaceprivate.h"

#include <graphene.h>

typedef struct
{
  GskRoundedRect rect;
  guint is_rectilinear     : 1;
  guint is_fully_contained : 1;
  guint is_empty           : 1;
  guint is_complex         : 1;
} Clip;

struct _GskOffload
{
  GdkSurface *surface;
  GskOffloadInfo *subsurfaces;
  gsize n_subsurfaces;

  GSList *transforms;
  GSList *clips;

  Clip *current_clip;

  GskOffloadInfo *last_info;
};

static GdkTexture *
find_texture_to_attach (GskOffload          *self,
                        GdkSubsurface       *subsurface,
                        const GskRenderNode *node,
                        graphene_rect_t     *out_clip)
{
  gboolean has_clip = FALSE;
  graphene_rect_t clip;

  for (;;)
    {
      switch ((int)GSK_RENDER_NODE_TYPE (node))
        {
        case GSK_DEBUG_NODE:
          node = gsk_debug_node_get_child (node);
          break;

        case GSK_CONTAINER_NODE:
          if (gsk_container_node_get_n_children (node) != 1)
            {
              GDK_DISPLAY_DEBUG (gdk_surface_get_display (self->surface), OFFLOAD,
                                 "Can't offload subsurface %p: too much content, container with %d children",
                                 subsurface, gsk_container_node_get_n_children (node));
              return NULL;
            }
          node = gsk_container_node_get_child (node, 0);
          break;

        case GSK_TRANSFORM_NODE:
          {
            GskTransform *t = gsk_transform_node_get_transform (node);

            if (gsk_transform_get_category (t) < GSK_TRANSFORM_CATEGORY_2D_AFFINE)
              {
                char *s = gsk_transform_to_string (t);
                GDK_DISPLAY_DEBUG (gdk_surface_get_display (self->surface), OFFLOAD,
                                   "Can't offload subsurface %p: transform %s is not just scale/translate",
                                   subsurface, s);
                g_free (s);
                return NULL;
              }

            if (has_clip)
              {
                GskTransform *inv = gsk_transform_invert (gsk_transform_ref (t));
                gsk_transform_transform_bounds (inv, &clip, &clip);
                gsk_transform_unref (inv);
              }

            node = gsk_transform_node_get_child (node);
          }
          break;

        case GSK_CLIP_NODE:
          {
            const graphene_rect_t *c = gsk_clip_node_get_clip (node);

            if (has_clip)
              {
                if (!gsk_rect_intersection (c, &clip, &clip))
                  {
                    GDK_DISPLAY_DEBUG (gdk_surface_get_display (self->surface), OFFLOAD,
                                      "Can't offload subsurface %p: empty clip", subsurface);
                    return NULL;
                  }
              }
            else
              {
                clip = *c;
                has_clip = TRUE;
              }

            node = gsk_clip_node_get_child (node);
          }
          break;

        case GSK_TEXTURE_NODE:
          {
            GdkTexture *texture = gsk_texture_node_get_texture (node);

            if (has_clip)
              {
                float dx = node->bounds.origin.x;
                float dy = node->bounds.origin.y;
                float sx = gdk_texture_get_width (texture) / node->bounds.size.width;
                float sy = gdk_texture_get_height (texture) / node->bounds.size.height;

                gsk_rect_intersection (&node->bounds, &clip, &clip);

                out_clip->origin.x = (clip.origin.x - dx) * sx;
                out_clip->origin.y = (clip.origin.y - dy) * sy;
                out_clip->size.width = clip.size.width * sx;
                out_clip->size.height = clip.size.height * sy;
              }
            else
              {
                out_clip->origin.x = 0;
                out_clip->origin.y = 0;
                out_clip->size.width = gdk_texture_get_width (texture);
                out_clip->size.height = gdk_texture_get_height (texture);
              }

            return texture;
          }

        default:
          GDK_DISPLAY_DEBUG (gdk_surface_get_display (self->surface), OFFLOAD,
                             "Can't offload subsurface %p: Only textures supported but found %s",
                             subsurface, g_type_name_from_instance ((GTypeInstance *) node));
          return NULL;
        }
    }
}

static void
push_transform (GskOffload   *self,
                GskTransform *transform)
{
  if (self->transforms)
    {
      GskTransform *t = self->transforms->data;
      t = gsk_transform_transform (gsk_transform_ref (t), transform);
      self->transforms = g_slist_prepend (self->transforms, t);
    }
  else
    self->transforms = g_slist_prepend (NULL, gsk_transform_ref (transform));
}

static void
pop_transform (GskOffload *self)
{
  GSList *l = self->transforms;
  GskTransform *t = l->data;

  g_assert (self->transforms != NULL);

  self->transforms = self->transforms->next;

  g_slist_free_1 (l);
  gsk_transform_unref (t);
}

static inline void
transform_bounds (GskOffload            *self,
                  const graphene_rect_t *bounds,
                  graphene_rect_t       *rect)
{
  GskTransform *t = self->transforms ? self->transforms->data : NULL;

  gsk_transform_transform_bounds (t, bounds, rect);
}

static inline void
transform_rounded_rect (GskOffload       *self,
                        const GskRoundedRect *rect,
                        GskRoundedRect       *out_rect)
{
  GskTransform *t = self->transforms ? self->transforms->data : NULL;
  float sx, sy, dx, dy;

  g_assert (gsk_transform_get_category (t) >= GSK_TRANSFORM_CATEGORY_2D_AFFINE);

  gsk_transform_to_affine (t, &sx, &sy, &dx, &dy);
  gsk_rounded_rect_scale_affine (out_rect, rect, sx, sy, dx, dy);
}

static void
push_rect_clip (GskOffload           *self,
                const GskRoundedRect *rect)
{
  Clip *clip = g_new0 (Clip, 1);
  clip->rect = *rect;
  clip->is_rectilinear = gsk_rounded_rect_is_rectilinear (rect);
  clip->is_empty = (rect->bounds.size.width == 0 || rect->bounds.size.height == 0);

  self->clips = g_slist_prepend (self->clips, clip);
  self->current_clip = self->clips->data;
}

static void
push_empty_clip (GskOffload *self)
{
  push_rect_clip (self, &GSK_ROUNDED_RECT_INIT (0, 0, 0, 0));
}

static void
push_contained_clip (GskOffload *self)
{
  Clip *current_clip = self->clips->data;
  Clip *clip = g_new0 (Clip, 1);

  clip->rect = current_clip->rect;
  clip->is_rectilinear = TRUE;
  clip->is_fully_contained = TRUE;

  self->clips = g_slist_prepend (self->clips, clip);
  self->current_clip = self->clips->data;
}

static void
push_complex_clip (GskOffload *self)
{
  Clip *current_clip = self->clips->data;
  Clip *clip = g_new0 (Clip, 1);

  clip->rect = current_clip->rect;
  clip->is_complex = TRUE;

  self->clips = g_slist_prepend (self->clips, clip);
  self->current_clip = self->clips->data;
}

static void
pop_clip (GskOffload *self)
{
  GSList *l = self->clips;
  Clip *clip = l->data;

  g_assert (self->clips != NULL);

  self->clips = self->clips->next;
  if (self->clips)
    self->current_clip = self->clips->data;

  g_slist_free_1 (l);
  g_free (clip);
}

static inline void
rounded_rect_get_inner (const GskRoundedRect *rect,
                        graphene_rect_t      *inner)
{
  float left = MAX (rect->corner[GSK_CORNER_TOP_LEFT].width, rect->corner[GSK_CORNER_BOTTOM_LEFT].width);
  float right = MAX (rect->corner[GSK_CORNER_TOP_RIGHT].width, rect->corner[GSK_CORNER_BOTTOM_RIGHT].width);
  float top = MAX (rect->corner[GSK_CORNER_TOP_LEFT].height, rect->corner[GSK_CORNER_TOP_RIGHT].height);
  float bottom = MAX (rect->corner[GSK_CORNER_BOTTOM_LEFT].height, rect->corner[GSK_CORNER_BOTTOM_RIGHT].height);

  inner->origin.x = rect->bounds.origin.x + left;
  inner->size.width = rect->bounds.size.width - (left + right);

  inner->origin.y = rect->bounds.origin.y + top;
  inner->size.height = rect->bounds.size.height - (top + bottom);
}

static inline gboolean
interval_contains (float p1, float w1,
                   float p2, float w2)
{
  if (p2 < p1)
    return FALSE;

  if (p2 + w2 > p1 + w1)
    return FALSE;

  return TRUE;
}

static gboolean
update_clip (GskOffload            *self,
             const graphene_rect_t *transformed_bounds)
{
  gboolean no_clip = FALSE;
  gboolean rect_clip = FALSE;

  if (self->current_clip->is_fully_contained ||
      self->current_clip->is_empty ||
      self->current_clip->is_complex)
    return FALSE;

  if (!gsk_rect_intersects (&self->current_clip->rect.bounds, transformed_bounds))
    {
      push_empty_clip (self);
      return TRUE;
    }

  if (self->current_clip->is_rectilinear)
    {
      if (gsk_rect_contains_rect (&self->current_clip->rect.bounds, transformed_bounds))
        no_clip = TRUE;
      else
        rect_clip = TRUE;
    }
  else if (gsk_rounded_rect_contains_rect (&self->current_clip->rect, transformed_bounds))
    {
      no_clip = TRUE;
    }
  else
    {
      graphene_rect_t inner;

      rounded_rect_get_inner (&self->current_clip->rect, &inner);

      if (interval_contains (inner.origin.x, inner.size.width,
                             transformed_bounds->origin.x, transformed_bounds->size.width) ||
          interval_contains (inner.origin.y, inner.size.height,
                             transformed_bounds->origin.y, transformed_bounds->size.height))
        rect_clip = TRUE;
    }

  if (no_clip)
    {
      /* This node is completely contained inside the clip.
       * Record this fact on the clip stack, so we don't do
       * more work for child nodes.
       */
      push_contained_clip (self);
      return TRUE;
    }
  else if (rect_clip && !self->current_clip->is_rectilinear)
    {
      graphene_rect_t rect;

      /* The clip gets simpler for this node */

      gsk_rect_intersection (&self->current_clip->rect.bounds, transformed_bounds, &rect);
      push_rect_clip (self, &GSK_ROUNDED_RECT_INIT_FROM_RECT (rect));
      return TRUE;
    }

  return FALSE;
}

static GskOffloadInfo *
find_subsurface_info (GskOffload    *self,
                      GdkSubsurface *subsurface)
{
  for (gsize i = 0; i < self->n_subsurfaces; i++)
    {
      GskOffloadInfo *info = &self->subsurfaces[i];
      if (info->subsurface == subsurface)
        return info;
    }

  return NULL;
}

static void
visit_node (GskOffload    *self,
            GskRenderNode *node)
{
  gboolean has_clip;
  graphene_rect_t transformed_bounds;

  transform_bounds (self, &node->bounds, &transformed_bounds);

  for (gsize i = 0; i < self->n_subsurfaces; i++)
    {
      GskOffloadInfo *info = &self->subsurfaces[i];

      if (info->can_raise)
        {
          if (gsk_rect_intersects (&transformed_bounds, &info->dest))
            {
              GskRenderNodeType type = GSK_RENDER_NODE_TYPE (node);

              if (type != GSK_CONTAINER_NODE &&
                  type != GSK_TRANSFORM_NODE &&
                  type != GSK_CLIP_NODE &&
                  type != GSK_ROUNDED_CLIP_NODE &&
                  type != GSK_DEBUG_NODE)
                {
                  GDK_DISPLAY_DEBUG (gdk_surface_get_display (self->surface), OFFLOAD,
                                     "Can't raise subsurface %p because a %s overlaps",
                                     info->subsurface,
                                     g_type_name_from_instance ((GTypeInstance *) node));
                  info->can_raise = FALSE;
                }
            }
        }
    }

  has_clip = update_clip (self, &transformed_bounds);

  switch (GSK_RENDER_NODE_TYPE (node))
    {
    case GSK_BORDER_NODE:
    case GSK_COLOR_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_TEXT_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_CAIRO_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_BLEND_NODE:
    case GSK_BLUR_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_OPACITY_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_SHADOW_NODE:
    case GSK_REPEAT_NODE:
    case GSK_MASK_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    break;

    case GSK_CLIP_NODE:
      {
        const graphene_rect_t *clip = gsk_clip_node_get_clip (node);
        graphene_rect_t transformed_clip;
        GskRoundedRect intersection;

        transform_bounds (self, clip, &transformed_clip);

        if (self->current_clip->is_rectilinear)
          {
            memset (&intersection.corner, 0, sizeof intersection.corner);
            gsk_rect_intersection (&transformed_clip,
                                   &self->current_clip->rect.bounds,
                                   &intersection.bounds);

            push_rect_clip (self, &intersection);
            visit_node (self, gsk_clip_node_get_child (node));
            pop_clip (self);
          }
        else
          {
            GskRoundedRectIntersection result;

            result = gsk_rounded_rect_intersect_with_rect (&self->current_clip->rect,
                                                           &transformed_clip,
                                                           &intersection);

            if (result == GSK_INTERSECTION_EMPTY)
              push_empty_clip (self);
            else if (result == GSK_INTERSECTION_NONEMPTY)
              push_rect_clip (self, &intersection);
            else
              push_complex_clip (self);
            visit_node (self, gsk_clip_node_get_child (node));
            pop_clip (self);
          }
      }
      break;

    case GSK_ROUNDED_CLIP_NODE:
      {
        const GskRoundedRect *clip = gsk_rounded_clip_node_get_clip (node);
        GskRoundedRect transformed_clip;

        transform_rounded_rect (self, clip, &transformed_clip);

        if (self->current_clip->is_rectilinear)
          {
            GskRoundedRect intersection;
            GskRoundedRectIntersection result;

            result = gsk_rounded_rect_intersect_with_rect (&transformed_clip,
                                                           &self->current_clip->rect.bounds,
                                                           &intersection);

            if (result == GSK_INTERSECTION_EMPTY)
              push_empty_clip (self);
            else if (result == GSK_INTERSECTION_NONEMPTY)
              push_rect_clip (self, &intersection);
            else
              goto complex_clip;
            visit_node (self, gsk_rounded_clip_node_get_child (node));
            pop_clip (self);
          }
        else
          {
complex_clip:
            if (gsk_rounded_rect_contains_rect (&self->current_clip->rect, &transformed_clip.bounds))
              push_rect_clip (self, &transformed_clip);
            else
              push_complex_clip (self);
            visit_node (self, gsk_rounded_clip_node_get_child (node));
            pop_clip (self);
          }
      }
      break;

    case GSK_TRANSFORM_NODE:
      push_transform (self, gsk_transform_node_get_transform (node));
      visit_node (self, gsk_transform_node_get_child (node));
      pop_transform (self);
      break;

    case GSK_CONTAINER_NODE:
      for (gsize i = 0; i < gsk_container_node_get_n_children (node); i++)
        visit_node (self, gsk_container_node_get_child (node, i));
      break;

    case GSK_DEBUG_NODE:
      visit_node (self, gsk_debug_node_get_child (node));
      break;

    case GSK_SUBSURFACE_NODE:
      {
        GdkSubsurface *subsurface = gsk_subsurface_node_get_subsurface (node);

        GskOffloadInfo *info = find_subsurface_info (self, subsurface);

        if (info == NULL)
          {
            GDK_DISPLAY_DEBUG (gdk_surface_get_display (self->surface), OFFLOAD,
                               "Can't offload: unknown subsurface %p",
                               subsurface);
          }
        else if (!self->current_clip->is_fully_contained)
          {
            GDK_DISPLAY_DEBUG (gdk_surface_get_display (self->surface), OFFLOAD,
                               "Can't offload subsurface %p: clipped",
                               subsurface);
          }
        else if (self->transforms &&
                 gsk_transform_get_category ((GskTransform *)self->transforms->data) < GSK_TRANSFORM_CATEGORY_2D_AFFINE)
          {
            GDK_DISPLAY_DEBUG (gdk_surface_get_display (self->surface), OFFLOAD,
                               "Can't offload subsurface %p: non-affine transform",
                               subsurface);
          }
        else
          {
            graphene_rect_t clip;

            info->texture = find_texture_to_attach (self, subsurface, gsk_subsurface_node_get_child (node), &clip);
            if (info->texture)
              {
                info->can_offload = TRUE;
                info->can_raise = TRUE;
                info->source = clip;
                transform_bounds (self, &node->bounds, &info->dest);
                info->place_above = self->last_info ? self->last_info->subsurface : NULL;
                self->last_info = info;
              }
          }
      }
      break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    break;
    }

  if (has_clip)
    pop_clip (self);
}

GskOffload *
gsk_offload_new (GdkSurface     *surface,
                 GskRenderNode  *root,
                 cairo_region_t *diff)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GskOffload *self;

  self = g_new0 (GskOffload, 1);

  self->surface = surface;
  self->transforms = NULL;
  self->clips = NULL;

  self->last_info = NULL;

  self->n_subsurfaces = gdk_surface_get_n_subsurfaces (self->surface);
  self->subsurfaces = g_new0 (GskOffloadInfo, self->n_subsurfaces);

  for (gsize i = 0; i < self->n_subsurfaces; i++)
    {
      GskOffloadInfo *info = &self->subsurfaces[i];
      info->subsurface = gdk_surface_get_subsurface (self->surface, i);
      info->was_offloaded = gdk_subsurface_get_texture (info->subsurface) != NULL;
      info->was_above = gdk_subsurface_is_above_parent (info->subsurface);
    }

  if (self->n_subsurfaces > 0)
    {
      push_rect_clip (self, &GSK_ROUNDED_RECT_INIT (0, 0,
                                                    gdk_surface_get_width (surface),
                                                    gdk_surface_get_height (surface)));

      visit_node (self, root);

      pop_clip (self);
    }

  for (gsize i = 0; i < self->n_subsurfaces; i++)
    {
      GskOffloadInfo *info = &self->subsurfaces[i];
      graphene_rect_t old_dest;

      gdk_subsurface_get_dest (info->subsurface, &old_dest);

      if (info->can_offload)
        {
          if (info->can_raise)
            info->is_offloaded = gdk_subsurface_attach (info->subsurface,
                                                        info->texture,
                                                        &info->source,
                                                        &info->dest,
                                                        TRUE, NULL);
          else
            info->is_offloaded = gdk_subsurface_attach (info->subsurface,
                                                        info->texture,
                                                        &info->source,
                                                        &info->dest,
                                                        info->place_above != NULL,
                                                        info->place_above);
        }
      else
        {
          info->is_offloaded = FALSE;
          if (info->was_offloaded)
            {
              GDK_DISPLAY_DEBUG (display, OFFLOAD, "Hiding subsurface %p", info->subsurface);
              gdk_subsurface_detach (info->subsurface);
            }
        }

      if (info->is_offloaded && gdk_subsurface_is_above_parent (info->subsurface))
        {
          GDK_DISPLAY_DEBUG (display, OFFLOAD, "Raising subsurface %p", info->subsurface);
          info->is_above = TRUE;
        }

      if (info->is_offloaded != info->was_offloaded ||
          info->is_above != info->was_above ||
          (info->is_offloaded && !gsk_rect_equal (&info->dest, &old_dest)))
        {
          /* We changed things, need to invalidate everything */
          cairo_rectangle_int_t int_dest;

          if (info->is_offloaded)
            {
              gsk_rect_to_cairo_grow (&info->dest, &int_dest);
              cairo_region_union_rectangle (diff, &int_dest);
            }
          if (info->was_offloaded)
            {
              gsk_rect_to_cairo_grow (&old_dest, &int_dest);
              cairo_region_union_rectangle (diff, &int_dest);
            }
        }

    }

  return self;
}

void
gsk_offload_free (GskOffload *self)
{
  g_free (self->subsurfaces);
  g_free (self);
}

GskOffloadInfo *
gsk_offload_get_subsurface_info (GskOffload    *self,
                                 GdkSubsurface *subsurface)
{
  return find_subsurface_info (self, subsurface);
}
