/* gskglrenderjob.c
 *
 * Copyright 2017 Timm Bäder <mail@baedert.org>
 * Copyright 2018 Matthias Clasen <mclasen@redhat.com>
 * Copyright 2018 Alexander Larsson <alexl@redhat.com>
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdkprofilerprivate.h>
#include <gdk/gdkrgbaprivate.h>
#include <gsk/gskrendernodeprivate.h>
#include <gsk/gskglshaderprivate.h>
#include <gdk/gdktextureprivate.h>
#include <gdk/gdkmemorytextureprivate.h>
#include <gdk/gdkdmabuftexture.h>
#include <gdk/gdksurfaceprivate.h>
#include <gdk/gdksubsurfaceprivate.h>
#include <gsk/gsktransformprivate.h>
#include <gsk/gskroundedrectprivate.h>
#include <gsk/gskrectprivate.h>
#include <gsk/gskrendererprivate.h>
#include <math.h>
#include <string.h>

#include "gskglcommandqueueprivate.h"
#include "gskgldriverprivate.h"
#include "gskglglyphlibraryprivate.h"
#include "gskgliconlibraryprivate.h"
#include "gskglprogramprivate.h"
#include "gskglrenderjobprivate.h"
#include "gskglshadowlibraryprivate.h"
#include "gskdebugprivate.h"

#include "ninesliceprivate.h"
#include "fp16private.h"

#define ORTHO_NEAR_PLANE   -10000
#define ORTHO_FAR_PLANE     10000
#define MAX_GRADIENT_STOPS  6
#define SHADOW_EXTRA_SIZE   4

/* Make sure gradient stops fits in packed array_count */
G_STATIC_ASSERT ((MAX_GRADIENT_STOPS * 5) < (1 << GSK_GL_UNIFORM_ARRAY_BITS));

#define ALPHA_IS_CLEAR(alpha) ((alpha) < ((float) 0x00ff / (float) 0xffff))
#define RGBA_IS_CLEAR(rgba) ALPHA_IS_CLEAR((rgba)->alpha)

typedef struct _GskGLRenderClip
{
  GskRoundedRect rect;
  guint          is_rectilinear : 1;
  guint          is_fully_contained : 1;
} GskGLRenderClip;

#define GDK_ARRAY_NAME clips
#define GDK_ARRAY_TYPE_NAME Clips
#define GDK_ARRAY_ELEMENT_TYPE GskGLRenderClip
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET
#include "gdk/gdkarrayimpl.c"

typedef struct _GskGLRenderModelview
{
  GskTransform *transform;
  float scale_x;
  float scale_y;
  float dx;
  float dy;
  float offset_x_before;
  float offset_y_before;
  graphene_matrix_t matrix;
} GskGLRenderModelview;

#define GDK_ARRAY_NAME modelviews
#define GDK_ARRAY_TYPE_NAME Modelviews
#define GDK_ARRAY_ELEMENT_TYPE GskGLRenderModelview
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET
#include "gdk/gdkarrayimpl.c"

struct _GskGLRenderJob
{
  /* The context containing the framebuffer we are drawing to. Generally this
   * is the context of the surface but may be a shared context if rendering to
   * an offscreen texture such as gsk_gl_renderer_render_texture().
   */
  GdkGLContext *context;

  /* The driver to be used. This is shared among all the renderers on a given
   * GdkDisplay and uses the shared GL context to send commands.
   */
  GskGLDriver *driver;

  /* The command queue (which is just a faster pointer to the driver's
   * command queue.
   */
  GskGLCommandQueue *command_queue;

  /* The region that we are clipping. Normalized to a single rectangle region. */
  cairo_region_t *region;

  /* The framebuffer to draw to in the @context GL context. So 0 would be the
   * default framebuffer of @context. This is important to note as many other
   * operations could be done using objects shared from the command queues
   * GL context.
   */
  guint framebuffer;
  guint default_framebuffer;

  /* The viewport we are using. This state is updated as we process render
   * nodes in the specific visitor callbacks.
   */
  graphene_rect_t viewport;

  /* The current projection, updated as we process nodes */
  graphene_matrix_t projection;

  /* An array of GskGLRenderModelview updated as nodes are processed. The
   * current modelview is the last element.
   */
  Modelviews modelview;

  /* An array of GskGLRenderClip updated as nodes are processed. The
   * current clip is the last element.
   */
  Clips clip;

  /* Our current alpha state as we process nodes */
  float alpha;

  /* Offset (delta x,y) as we process nodes. Occasionally this is merged into
   * a transform that is referenced from child transform nodes.
   */
  float offset_x;
  float offset_y;

  /* The scale we are processing, possibly updated by transforms */
  float scale_x;
  float scale_y;

  /* Cached pointers */
  const GskGLRenderClip *current_clip;
  const GskGLRenderModelview *current_modelview;
  GskGLProgram *current_program;

  guint source_is_glyph_atlas : 1;

  /* In some cases we might want to avoid clearing the framebuffer
   * because we're going to render over the existing contents.
   */
  guint clear_framebuffer : 1;

  /* Format we want to use for intermediate textures, determined by
   * looking at the format of the framebuffer we are rendering on.
   */
  int target_format;

  guint offscreen_count; /* if > 0, we're rendering to an offscreen */
};

typedef struct _GskGLRenderOffscreen
{
  /* The bounds to render */
  const graphene_rect_t *bounds;

  /* Return location for texture coordinates */
  struct {
    float x;
    float y;
    float x2;
    float y2;
  } area;

  /* Return location for texture ID */
  guint texture_id;
  gpointer sync;

  /* Whether to force creating a new texture, even if the
   * input already is a texture
   */
  guint force_offscreen : 1;
  guint reset_clip : 1;
  guint do_not_cache : 1;

  /* Return location for whether we created a texture */
  guint was_offscreen : 1;
  guint has_mipmap : 1;
} GskGLRenderOffscreen;

static void     gsk_gl_render_job_visit_node                (GskGLRenderJob       *job,
                                                             const GskRenderNode  *node);
static gboolean gsk_gl_render_job_visit_node_with_offscreen (GskGLRenderJob       *job,
                                                             const GskRenderNode  *node,
                                                             GskGLRenderOffscreen *offscreen);
static void     gsk_gl_render_job_upload_texture            (GskGLRenderJob       *job,
                                                             GdkTexture           *texture,
                                                             gboolean              ensure_mipmap,
                                                             GskGLRenderOffscreen *offscreen);

static inline GskGLRenderClip *
clips_grow_one (Clips *clips)
{
  guint len = clips_get_size (clips);
  clips_set_size (clips, len + 1);
  return clips_get (clips, len);
}

static inline GskGLRenderModelview *
modelviews_grow_one (Modelviews *modelviews)
{
  guint len = modelviews_get_size (modelviews);
  modelviews_set_size (modelviews, len + 1);
  return modelviews_get (modelviews, len);
}

static inline int
get_target_format (GskGLRenderJob      *job,
                   const GskRenderNode *node)
{
  if (gsk_render_node_get_preferred_depth (node) != GDK_MEMORY_U8)
    return job->target_format;

  return GL_RGBA8;
}

static inline void
init_full_texture_region (GskGLRenderOffscreen *offscreen)
{
  offscreen->area.x = 0;
  offscreen->area.y = 0;
  offscreen->area.x2 = 1;
  offscreen->area.y2 = 1;
}

static inline gboolean G_GNUC_PURE
node_is_invisible (const GskRenderNode *node)
{
  return node->bounds.size.width == 0.0f ||
         node->bounds.size.height == 0.0f;
}

static inline gboolean G_GNUC_PURE
rounded_rect_equal (const GskRoundedRect *r1,
                    const GskRoundedRect *r2)
{
  return memcmp (r1, r2, sizeof (GskRoundedRect)) == 0;
}

static inline void
gsk_rounded_rect_shrink_to_minimum (GskRoundedRect *self)
{
  self->bounds.size.width  = MAX (self->corner[0].width + self->corner[1].width,
                                  self->corner[3].width + self->corner[2].width);
  self->bounds.size.height = MAX (self->corner[0].height + self->corner[3].height,
                                  self->corner[1].height + self->corner[2].height);
}

static inline gboolean G_GNUC_PURE
node_supports_2d_transform (const GskRenderNode *node)
{
  switch (GSK_RENDER_NODE_TYPE (node))
    {
    case GSK_COLOR_NODE:
    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_DEBUG_NODE:
    case GSK_TEXT_NODE:
    case GSK_CAIRO_NODE:
    case GSK_BLEND_NODE:
    case GSK_BLUR_NODE:
    case GSK_MASK_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_SUBSURFACE_NODE:
      return TRUE;

    case GSK_SHADOW_NODE:
      return node_supports_2d_transform (gsk_shadow_node_get_child (node));

    case GSK_TRANSFORM_NODE:
      return node_supports_2d_transform (gsk_transform_node_get_child (node));

    case GSK_CONTAINER_NODE:
      for (guint i = 0, p = gsk_container_node_get_n_children (node); i < p; i++)
        {
          if (!node_supports_2d_transform (gsk_container_node_get_child (node, i)))
            return FALSE;
        }
      return TRUE;

    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_REPEAT_NODE:
    case GSK_CLIP_NODE:
    case GSK_ROUNDED_CLIP_NODE:
    case GSK_GL_SHADER_NODE:
      return FALSE;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    }
}

static inline gboolean G_GNUC_PURE
node_supports_transform (const GskRenderNode *node)
{
  /* Some nodes can't handle non-trivial transforms without being
   * rendered to a texture (e.g. rotated clips, etc.). Some however work
   * just fine, mostly because they already draw their child to a
   * texture and just render the texture manipulated in some way, think
   * opacity or color matrix.
   */

  switch (GSK_RENDER_NODE_TYPE (node))
    {
    case GSK_COLOR_NODE:
    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_DEBUG_NODE:
    case GSK_TEXT_NODE:
    case GSK_CAIRO_NODE:
    case GSK_BLEND_NODE:
    case GSK_BLUR_NODE:
    case GSK_MASK_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_SUBSURFACE_NODE:
      return TRUE;

    case GSK_SHADOW_NODE:
      return node_supports_transform (gsk_shadow_node_get_child (node));

    case GSK_TRANSFORM_NODE:
      return node_supports_transform (gsk_transform_node_get_child (node));

    case GSK_CONTAINER_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_REPEAT_NODE:
    case GSK_CLIP_NODE:
    case GSK_ROUNDED_CLIP_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_TEXTURE_SCALE_NODE:
      return FALSE;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    }
}

static inline gboolean G_GNUC_PURE
color_matrix_modifies_alpha (const GskRenderNode *node)
{
  const graphene_matrix_t *matrix = gsk_color_matrix_node_get_color_matrix (node);
  const graphene_vec4_t *offset = gsk_color_matrix_node_get_color_offset (node);
  graphene_vec4_t row3;

  if (graphene_vec4_get_w (offset) != 0.0f)
    return TRUE;

  graphene_matrix_get_row (matrix, 3, &row3);

  return !graphene_vec4_equal (graphene_vec4_w_axis (), &row3);
}

static inline void
init_projection_matrix (graphene_matrix_t     *projection,
                        const graphene_rect_t *viewport)
{
  graphene_matrix_init_ortho (projection,
                              viewport->origin.x,
                              viewport->origin.x + viewport->size.width,
                              viewport->origin.y,
                              viewport->origin.y + viewport->size.height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_matrix_scale (projection, 1, -1, 1);
}

static inline float
gsk_gl_render_job_set_alpha (GskGLRenderJob *job,
                             float           alpha)
{
  if (job->alpha != alpha)
    {
      float ret = job->alpha;
      job->alpha = alpha;
      job->driver->stamps[UNIFORM_SHARED_ALPHA]++;
      return ret;
    }

  return alpha;
}

static void
extract_matrix_metadata (GskGLRenderModelview *modelview)
{
  gsk_transform_to_matrix (modelview->transform, &modelview->matrix);

  switch (gsk_transform_get_category (modelview->transform))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      modelview->scale_x = 1;
      modelview->scale_y = 1;
      modelview->dx = 0;
      modelview->dy = 0;
      break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      modelview->scale_x = 1;
      modelview->scale_y = 1;
      gsk_transform_to_translate (modelview->transform,
                                  &modelview->dx, &modelview->dy);
      break;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      gsk_transform_to_affine (modelview->transform,
                               &modelview->scale_x, &modelview->scale_y,
                               &modelview->dx, &modelview->dy);
      break;

    case GSK_TRANSFORM_CATEGORY_2D:
      {
        float skew_x, skew_y, angle, dx, dy;

        gsk_transform_to_2d_components (modelview->transform,
                                        &skew_x, &skew_y,
                                        &modelview->scale_x, &modelview->scale_y,
                                        &angle, &dx, &dy);
        modelview->dx = 0;
        modelview->dy = 0;
      }
      break;

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
      {
        graphene_quaternion_t rotation;
        graphene_vec4_t perspective;
        graphene_vec3_t translation;
        graphene_vec3_t scale;
        graphene_vec3_t shear;

        graphene_matrix_decompose (&modelview->matrix,
                                   &translation,
                                   &scale,
                                   &rotation,
                                   &shear,
                                   &perspective);

        modelview->scale_x = graphene_vec3_get_x (&scale);
        modelview->scale_y = graphene_vec3_get_y (&scale);
        modelview->dx = 0;
        modelview->dy = 0;
      }
      break;

    default:
      break;
    }
}

/* takes ownership of transform */
static void
gsk_gl_render_job_set_modelview (GskGLRenderJob *job,
                                 GskTransform   *transform)
{
  GskGLRenderModelview *modelview;

  g_assert (job != NULL);

  job->driver->stamps[UNIFORM_SHARED_MODELVIEW]++;

  modelview = modelviews_grow_one (&job->modelview);

  modelview->transform = transform;

  modelview->offset_x_before = job->offset_x;
  modelview->offset_y_before = job->offset_y;

  extract_matrix_metadata (modelview);

  job->offset_x = 0;
  job->offset_y = 0;
  job->scale_x = modelview->scale_x;
  job->scale_y = modelview->scale_y;

  job->current_modelview = modelview;
}

/* doesn't take ownership of transform */
static void
gsk_gl_render_job_push_modelview (GskGLRenderJob *job,
                                  GskTransform   *transform)
{
  GskGLRenderModelview *modelview;

  g_assert (job != NULL);
  g_assert (transform != NULL);

  job->driver->stamps[UNIFORM_SHARED_MODELVIEW]++;

  modelview = modelviews_grow_one (&job->modelview);

  if G_LIKELY (modelviews_get_size (&job->modelview) > 1)
    {
      GskGLRenderModelview *last = job->modelview.end - 2;
      GskTransform *t = NULL;

      /* Multiply given matrix with our previous modelview */
      t = gsk_transform_translate (gsk_transform_ref (last->transform),
                                   &(graphene_point_t) {
                                     job->offset_x,
                                     job->offset_y
                                   });
      t = gsk_transform_transform (t, transform);
      modelview->transform = t;
    }
  else
    {
      modelview->transform = gsk_transform_ref (transform);
    }

  modelview->offset_x_before = job->offset_x;
  modelview->offset_y_before = job->offset_y;

  extract_matrix_metadata (modelview);

  job->offset_x = 0;
  job->offset_y = 0;
  job->scale_x = modelview->scale_x;
  job->scale_y = modelview->scale_y;

  job->current_modelview = modelview;
}

static void
gsk_gl_render_job_pop_modelview (GskGLRenderJob *job)
{
  const GskGLRenderModelview *head;

  g_assert (job != NULL);
  g_assert (modelviews_get_size (&job->modelview) > 0);

  job->driver->stamps[UNIFORM_SHARED_MODELVIEW]++;

  head = job->current_modelview;

  job->offset_x = head->offset_x_before;
  job->offset_y = head->offset_y_before;

  gsk_transform_unref (head->transform);

  job->modelview.end--;

  if (modelviews_get_size (&job->modelview) >= 1)
    {
      head = job->modelview.end - 1;

      job->scale_x = head->scale_x;
      job->scale_y = head->scale_y;

      job->current_modelview = head;
    }
  else
    {
      job->current_modelview = NULL;
    }
}

static void
gsk_gl_render_job_push_clip (GskGLRenderJob       *job,
                             const GskRoundedRect *rect)
{
  GskGLRenderClip *clip;

  g_assert (job != NULL);
  g_assert (rect != NULL);

  job->driver->stamps[UNIFORM_SHARED_CLIP_RECT]++;

  clip = clips_grow_one (&job->clip);

  memcpy (&clip->rect, rect, sizeof *rect);
  clip->is_rectilinear = gsk_rounded_rect_is_rectilinear (rect);
  clip->is_fully_contained = FALSE;

  job->current_clip = clip;
}

static void
gsk_gl_render_job_push_contained_clip (GskGLRenderJob *job)
{
  GskGLRenderClip *clip;
  GskGLRenderClip *old_clip;

  g_assert (job != NULL);
  g_assert (clips_get_size (&job->clip) > 0);

  job->driver->stamps[UNIFORM_SHARED_CLIP_RECT]++;

  clip = clips_grow_one (&job->clip);
  old_clip = clips_get (&job->clip, clips_get_size (&job->clip) - 2);

  memcpy (&clip->rect.bounds, &old_clip->rect.bounds, sizeof (graphene_rect_t));
  memset (clip->rect.corner, 0, sizeof clip->rect.corner);
  clip->is_rectilinear = TRUE;
  clip->is_fully_contained = TRUE;

  job->current_clip = clip;
}

static void
gsk_gl_render_job_pop_clip (GskGLRenderJob *job)
{
  g_assert (job != NULL);
  g_assert (clips_get_size (&job->clip) > 0);

  job->driver->stamps[UNIFORM_SHARED_CLIP_RECT]++;
  job->current_clip--;
  job->clip.end--;
}

static inline void
gsk_gl_render_job_offset (GskGLRenderJob *job,
                          float           offset_x,
                          float           offset_y)
{
  if (offset_x || offset_y)
    {
      job->offset_x += offset_x;
      job->offset_y += offset_y;
    }
}

static inline void
gsk_gl_render_job_set_projection (GskGLRenderJob          *job,
                                  const graphene_matrix_t *projection)
{
  memcpy (&job->projection, projection, sizeof job->projection);
  job->driver->stamps[UNIFORM_SHARED_PROJECTION]++;
}

static inline void
gsk_gl_render_job_set_projection_from_rect (GskGLRenderJob        *job,
                                            const graphene_rect_t *rect,
                                            graphene_matrix_t     *prev_projection)
{
  if (prev_projection)
    memcpy (prev_projection, &job->projection, sizeof *prev_projection);
  init_projection_matrix (&job->projection, rect);
  job->driver->stamps[UNIFORM_SHARED_PROJECTION]++;
}

static inline void
gsk_gl_render_job_set_projection_for_size (GskGLRenderJob    *job,
                                           float              width,
                                           float              height,
                                           graphene_matrix_t *prev_projection)
{
  if (prev_projection)
    memcpy (prev_projection, &job->projection, sizeof *prev_projection);
  graphene_matrix_init_ortho (&job->projection, 0, width, 0, height, ORTHO_NEAR_PLANE, ORTHO_FAR_PLANE);
  graphene_matrix_scale (&job->projection, 1, -1, 1);
  job->driver->stamps[UNIFORM_SHARED_PROJECTION]++;
}

static inline void
gsk_gl_render_job_set_viewport (GskGLRenderJob        *job,
                                const graphene_rect_t *viewport,
                                graphene_rect_t       *prev_viewport)
{
  if (prev_viewport)
    memcpy (prev_viewport, &job->viewport, sizeof *prev_viewport);
  memcpy (&job->viewport, viewport, sizeof job->viewport);
  job->driver->stamps[UNIFORM_SHARED_VIEWPORT]++;
}

static inline void
gsk_gl_render_job_set_viewport_for_size (GskGLRenderJob  *job,
                                         float            width,
                                         float            height,
                                         graphene_rect_t *prev_viewport)
{
  if (prev_viewport)
    memcpy (prev_viewport, &job->viewport, sizeof *prev_viewport);
  job->viewport.origin.x = 0;
  job->viewport.origin.y = 0;
  job->viewport.size.width = width;
  job->viewport.size.height = height;
  job->driver->stamps[UNIFORM_SHARED_VIEWPORT]++;
}

static inline void
gsk_gl_render_job_transform_bounds (GskGLRenderJob        *job,
                                    const graphene_rect_t *rect,
                                    graphene_rect_t       *out_rect)
{
  GskTransform *transform;
  GskTransformCategory category;

  g_assert (job != NULL);
  g_assert (modelviews_get_size (&job->modelview) > 0);
  g_assert (rect != NULL);
  g_assert (out_rect != NULL);

  transform = job->current_modelview->transform;
  category = gsk_transform_get_category (transform);

  /* Our most common transform is 2d-affine, so inline it.
   * Both identity and 2d-translate are virtually unseen here.
   */
  if G_LIKELY (category >= GSK_TRANSFORM_CATEGORY_2D_AFFINE)
    {
      float scale_x = job->current_modelview->scale_x;
      float scale_y = job->current_modelview->scale_y;
      float dx = job->current_modelview->dx;
      float dy = job->current_modelview->dy;

      /* Init directly into out rect */
      out_rect->origin.x = ((rect->origin.x + job->offset_x) * scale_x) + dx;
      out_rect->origin.y = ((rect->origin.y + job->offset_y) * scale_y) + dy;
      out_rect->size.width = rect->size.width * scale_x;
      out_rect->size.height = rect->size.height * scale_y;

      /* Normalize in place */
      if (out_rect->size.width < 0.f)
        {
          float size = fabsf (out_rect->size.width);

          out_rect->origin.x -= size;
          out_rect->size.width = size;
        }

      if (out_rect->size.height < 0.f)
        {
          float size = fabsf (out_rect->size.height);

          out_rect->origin.y -= size;
          out_rect->size.height = size;
        }
    }
  else
    {
      graphene_rect_t r;

      r.origin.x = rect->origin.x + job->offset_x;
      r.origin.y = rect->origin.y + job->offset_y;
      r.size.width = rect->size.width;
      r.size.height = rect->size.height;

      gsk_transform_transform_bounds (transform, &r, out_rect);
    }
}

static inline void
gsk_gl_render_job_untransform_bounds (GskGLRenderJob        *job,
                                      const graphene_rect_t *rect,
                                      graphene_rect_t       *out_rect)
{
  GskTransform *transform;

  transform = gsk_transform_invert (gsk_transform_ref (job->current_modelview->transform));

  gsk_transform_transform_bounds (transform, rect, out_rect);

  out_rect->origin.x -= job->offset_x;
  out_rect->origin.y -= job->offset_y;

  gsk_transform_unref (transform);
}

static inline void
gsk_gl_render_job_translate_rounded_rect (GskGLRenderJob       *job,
                                          const GskRoundedRect *rect,
                                          GskRoundedRect       *out_rect)
 {
  out_rect->bounds.origin.x = job->offset_x + rect->bounds.origin.x;
  out_rect->bounds.origin.y = job->offset_y + rect->bounds.origin.y;
  out_rect->bounds.size.width = rect->bounds.size.width;
  out_rect->bounds.size.height = rect->bounds.size.height;
  memcpy (out_rect->corner, rect->corner, sizeof rect->corner);
}

static inline void
rounded_rect_scale_corners (const GskRoundedRect *rect,
                            GskRoundedRect       *out_rect,
                            float                 scale_x,
                            float                 scale_y)
{
  for (guint i = 0; i < G_N_ELEMENTS (out_rect->corner); i++)
    {
      out_rect->corner[i].width = rect->corner[i].width * fabsf (scale_x);
      out_rect->corner[i].height = rect->corner[i].height * fabsf (scale_y);
    }

  if (scale_x < 0)
    {
      graphene_size_t p;

      p = out_rect->corner[GSK_CORNER_TOP_LEFT];
      out_rect->corner[GSK_CORNER_TOP_LEFT] = out_rect->corner[GSK_CORNER_TOP_RIGHT];
      out_rect->corner[GSK_CORNER_TOP_RIGHT] = p;
      p = out_rect->corner[GSK_CORNER_BOTTOM_LEFT];
      out_rect->corner[GSK_CORNER_BOTTOM_LEFT] = out_rect->corner[GSK_CORNER_BOTTOM_RIGHT];
      out_rect->corner[GSK_CORNER_BOTTOM_RIGHT] = p;
    }

  if (scale_y < 0)
    {
      graphene_size_t p;

      p = out_rect->corner[GSK_CORNER_TOP_LEFT];
      out_rect->corner[GSK_CORNER_TOP_LEFT] = out_rect->corner[GSK_CORNER_BOTTOM_LEFT];
      out_rect->corner[GSK_CORNER_BOTTOM_LEFT] = p;
      p = out_rect->corner[GSK_CORNER_TOP_RIGHT];
      out_rect->corner[GSK_CORNER_TOP_RIGHT] = out_rect->corner[GSK_CORNER_BOTTOM_RIGHT];
      out_rect->corner[GSK_CORNER_BOTTOM_RIGHT] = p;
    }
}

static inline void
gsk_gl_render_job_transform_rounded_rect (GskGLRenderJob       *job,
                                          const GskRoundedRect *rect,
                                          GskRoundedRect       *out_rect)
{
  gsk_gl_render_job_transform_bounds (job, &rect->bounds, &out_rect->bounds);
  rounded_rect_scale_corners (rect, out_rect, job->scale_x, job->scale_y);
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

static inline gboolean
gsk_gl_render_job_update_clip (GskGLRenderJob        *job,
                               const graphene_rect_t *bounds,
                               gboolean              *pushed_clip)
{
  graphene_rect_t transformed_bounds;
  gboolean no_clip = FALSE;
  gboolean rect_clip = FALSE;

  *pushed_clip = FALSE;

  if (job->current_clip->is_fully_contained)
    {
      /* Already fully contained - no further checks needed */
      return TRUE;
    }

  gsk_gl_render_job_transform_bounds (job, bounds, &transformed_bounds);

  if (!gsk_rect_intersects (&job->current_clip->rect.bounds, &transformed_bounds))
    {
      /* Completely clipped away */
      return FALSE;
    }

  if (job->current_clip->is_rectilinear)
    {
      if (gsk_rect_contains_rect (&job->current_clip->rect.bounds, &transformed_bounds))
        no_clip = TRUE;
      else
        rect_clip = TRUE;
    }
  else if (gsk_rounded_rect_contains_rect (&job->current_clip->rect, &transformed_bounds))
    {
      no_clip = TRUE;
    }
  else
    {
      graphene_rect_t inner;

      rounded_rect_get_inner (&job->current_clip->rect, &inner);

      if (interval_contains (inner.origin.x, inner.size.width,
                             transformed_bounds.origin.x, transformed_bounds.size.width) ||
          interval_contains (inner.origin.y, inner.size.height,
                             transformed_bounds.origin.y, transformed_bounds.size.height))
        rect_clip = TRUE;
    }

  if (no_clip)
    {
      /* This node is completely contained inside the clip.
       * Record this fact on the clip stack, so we don't do more work
       * for child nodes.
       */

      gsk_gl_render_job_push_contained_clip (job);

      *pushed_clip = TRUE;
    }
  else if (rect_clip && !job->current_clip->is_rectilinear)
    {
      graphene_rect_t rect;

      /* The clip gets simpler for this node */

      gsk_rect_intersection (&job->current_clip->rect.bounds, &transformed_bounds, &rect);
      gsk_gl_render_job_push_clip (job, &GSK_ROUNDED_RECT_INIT_FROM_RECT (rect));

      *pushed_clip = TRUE;
    }

  return TRUE;
}

static inline void
rgba_to_half (const GdkRGBA *rgba,
              guint16        h[4])
{
  float_to_half4 ((const float *)rgba, h);
}

/* fill_vertex_data */
static void
gsk_gl_render_job_draw_coords (GskGLRenderJob *job,
                               float           min_x,
                               float           min_y,
                               float           max_x,
                               float           max_y,
                               float           min_u,
                               float           min_v,
                               float           max_u,
                               float           max_v,
                               guint16         c[4])
{
  GskGLDrawVertex *vertices = gsk_gl_command_queue_add_vertices (job->command_queue);

  vertices[0] = (GskGLDrawVertex) { .position = { min_x, min_y }, .uv = { min_u, min_v }, .color = { c[0], c[1], c[2], c[3] } };
  vertices[1] = (GskGLDrawVertex) { .position = { min_x, max_y }, .uv = { min_u, max_v }, .color = { c[0], c[1], c[2], c[3] } };
  vertices[2] = (GskGLDrawVertex) { .position = { max_x, min_y }, .uv = { max_u, min_v }, .color = { c[0], c[1], c[2], c[3] } };
  vertices[3] = (GskGLDrawVertex) { .position = { max_x, max_y }, .uv = { max_u, max_v }, .color = { c[0], c[1], c[2], c[3] } };
  vertices[4] = (GskGLDrawVertex) { .position = { min_x, max_y }, .uv = { min_u, max_v }, .color = { c[0], c[1], c[2], c[3] } };
  vertices[5] = (GskGLDrawVertex) { .position = { max_x, min_y }, .uv = { max_u, min_v }, .color = { c[0], c[1], c[2], c[3] } };
}

/* load_vertex_data_with_region */
static inline void
gsk_gl_render_job_draw_offscreen_with_color (GskGLRenderJob             *job,
                                             const graphene_rect_t      *bounds,
                                             const GskGLRenderOffscreen *offscreen,
                                             guint16                     color[4])
{
  float min_x = job->offset_x + bounds->origin.x;
  float min_y = job->offset_y + bounds->origin.y;
  float max_x = min_x + bounds->size.width;
  float max_y = min_y + bounds->size.height;
  float y1 = offscreen->was_offscreen ? offscreen->area.y2 : offscreen->area.y;
  float y2 = offscreen->was_offscreen ? offscreen->area.y : offscreen->area.y2;

  gsk_gl_render_job_draw_coords (job,
                                 min_x, min_y, max_x, max_y,
                                 offscreen->area.x, y1, offscreen->area.x2, y2,
                                 color);
}

static inline void
gsk_gl_render_job_draw_offscreen (GskGLRenderJob             *job,
                                  const graphene_rect_t      *bounds,
                                  const GskGLRenderOffscreen *offscreen)
{
  gsk_gl_render_job_draw_offscreen_with_color (job, bounds, offscreen,
                                               (guint16[]) { FP16_ZERO, FP16_ZERO, FP16_ZERO, FP16_ZERO });
}

/* load_float_vertex_data */
static inline void
gsk_gl_render_job_draw_with_color (GskGLRenderJob *job,
                                   float           x,
                                   float           y,
                                   float           width,
                                   float           height,
                                   guint16         color[4])
{
  float min_x = job->offset_x + x;
  float min_y = job->offset_y + y;
  float max_x = min_x + width;
  float max_y = min_y + height;

  gsk_gl_render_job_draw_coords (job, min_x, min_y, max_x, max_y, 0, 0, 1, 1, color);
}

static inline void
gsk_gl_render_job_draw (GskGLRenderJob *job,
                        float           x,
                        float           y,
                        float           width,
                        float           height)
{
  gsk_gl_render_job_draw_with_color (job, x, y, width, height,
                                     (guint16[]) { FP_ZERO, FP_ZERO, FP_ZERO, FP_ZERO });
}

/* load_vertex_data */
static inline void
gsk_gl_render_job_draw_rect_with_color (GskGLRenderJob        *job,
                                        const graphene_rect_t *bounds,
                                        guint16                color[4])
{
  gsk_gl_render_job_draw_with_color (job,
                                     bounds->origin.x,
                                     bounds->origin.y,
                                     bounds->size.width,
                                     bounds->size.height,
                                     color);
}
static inline void
gsk_gl_render_job_draw_rect (GskGLRenderJob        *job,
                             const graphene_rect_t *bounds)
{
  gsk_gl_render_job_draw (job,
                          bounds->origin.x,
                          bounds->origin.y,
                          bounds->size.width,
                          bounds->size.height);
}

/* load_offscreen_vertex_data */
static inline void
gsk_gl_render_job_draw_offscreen_rect (GskGLRenderJob        *job,
                                       const graphene_rect_t *bounds)
{
  float min_x = job->offset_x + bounds->origin.x;
  float min_y = job->offset_y + bounds->origin.y;
  float max_x = min_x + bounds->size.width;
  float max_y = min_y + bounds->size.height;
  guint16 color[4] = { FP16_ZERO, FP16_ZERO, FP16_ZERO, FP16_ZERO };

  gsk_gl_render_job_draw_coords (job,
                                 min_x, min_y, max_x, max_y,
                                 0, 1, 1, 0,
                                 color);
}

static inline gboolean
gsk_gl_render_job_begin_draw (GskGLRenderJob *job,
                              GskGLProgram   *program)
{
  job->current_program = program;

  if (!gsk_gl_command_queue_begin_draw (job->command_queue,
                                        program->program_info,
                                        job->viewport.size.width,
                                        job->viewport.size.height))
    return FALSE;

  gsk_gl_uniform_state_set4fv (program->uniforms,
                               program->program_info,
                               UNIFORM_SHARED_VIEWPORT,
                               job->driver->stamps[UNIFORM_SHARED_VIEWPORT],
                               1,
                               (const float *)&job->viewport);

  gsk_gl_uniform_state_set_matrix (program->uniforms,
                                   program->program_info,
                                   UNIFORM_SHARED_MODELVIEW,
                                   job->driver->stamps[UNIFORM_SHARED_MODELVIEW],
                                   &job->current_modelview->matrix);

  gsk_gl_uniform_state_set_matrix (program->uniforms,
                                   program->program_info,
                                   UNIFORM_SHARED_PROJECTION,
                                   job->driver->stamps[UNIFORM_SHARED_PROJECTION],
                                   &job->projection);

  gsk_gl_uniform_state_set_rounded_rect (program->uniforms,
                                         program->program_info,
                                         UNIFORM_SHARED_CLIP_RECT,
                                         job->driver->stamps[UNIFORM_SHARED_CLIP_RECT],
                                         &job->current_clip->rect);

  gsk_gl_uniform_state_set1f (program->uniforms,
                              program->program_info,
                              UNIFORM_SHARED_ALPHA,
                              job->driver->stamps[UNIFORM_SHARED_ALPHA],
                              job->alpha);

  return TRUE;
}

#define CHOOSE_PROGRAM(job,name) \
  (job->current_clip->is_fully_contained \
      ? job->driver->name ## _no_clip \
      : (job->current_clip->is_rectilinear \
        ? job->driver->name ## _rect_clip \
        : job->driver->name))

static inline void
gsk_gl_render_job_split_draw (GskGLRenderJob *job)
{
  gsk_gl_command_queue_split_draw (job->command_queue);
}

static inline void
gsk_gl_render_job_end_draw (GskGLRenderJob *job)
{
  gsk_gl_command_queue_end_draw (job->command_queue);

  job->current_program = NULL;
}

static inline void
gsk_gl_render_job_visit_as_fallback (GskGLRenderJob      *job,
                                     const GskRenderNode *node)
{
  float scale_x = job->scale_x;
  float scale_y = job->scale_y;
  int surface_width = ceilf (node->bounds.size.width * fabsf (scale_x));
  int surface_height = ceilf (node->bounds.size.height * fabsf (scale_y));
  GdkTexture *texture;
  cairo_surface_t *surface;
  cairo_surface_t *rendered_surface;
  cairo_t *cr;
  int texture_id;
  GskTextureKey key;

  if (surface_width <= 0 || surface_height <= 0)
    return;

  key.pointer = node;
  key.pointer_is_child = FALSE;
  key.scale_x = scale_x;
  key.scale_y = scale_y;

  texture_id = gsk_gl_driver_lookup_texture (job->driver, &key, NULL);

  if (texture_id != 0)
    goto done;

  /* We first draw the recording surface on an image surface,
   * just because the scaleY(-1) later otherwise screws up the
   * rendering... */
  {
    rendered_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                   surface_width,
                                                   surface_height);

    cairo_surface_set_device_scale (rendered_surface, fabsf (scale_x), fabsf (scale_y));
    cr = cairo_create (rendered_surface);

    cairo_save (cr);
    cairo_translate (cr, - node->bounds.origin.x, - node->bounds.origin.y);
    /* Render nodes don't modify state, so casting away the const is fine here */
    gsk_render_node_draw_fallback ((GskRenderNode *)node, cr);
    cairo_restore (cr);
    cairo_destroy (cr);
  }

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        surface_width,
                                        surface_height);
  cairo_surface_set_device_scale (surface, fabsf (scale_x), fabsf (scale_y));
  cr = cairo_create (surface);

  /* We draw upside down here, so it matches what GL does. */
  cairo_save (cr);
  cairo_scale (cr, scale_x < 0 ? -1 : 1, scale_y < 0 ? 1 : -1);
  cairo_translate (cr, scale_x < 0 ? - surface_width / fabsf (scale_x) : 0,
                       scale_y < 0 ? 0 : - surface_height / fabsf (scale_y));
  cairo_set_source_surface (cr, rendered_surface, 0, 0);
  cairo_rectangle (cr, 0, 0, surface_width / fabsf (scale_x), surface_height / fabsf (scale_y));
  cairo_fill (cr);
  cairo_restore (cr);
  cairo_destroy (cr);

  /* Create texture to upload */
  texture = gdk_texture_new_for_surface (surface);
  texture_id = gsk_gl_driver_load_texture (job->driver, texture, FALSE);

  if (gdk_gl_context_has_feature (job->command_queue->context, GDK_GL_FEATURE_DEBUG))
    gdk_gl_context_label_object_printf (job->command_queue->context, GL_TEXTURE, texture_id,
                                        "Fallback %s %d",
                                        g_type_name_from_instance ((GTypeInstance *) node),
                                        texture_id);

  g_object_unref (texture);
  cairo_surface_destroy (surface);
  cairo_surface_destroy (rendered_surface);

  gsk_gl_driver_cache_texture (job->driver, &key, texture_id);

done:
  if (scale_x < 0 || scale_y < 0)
    {
      GskTransform *transform = gsk_transform_translate (gsk_transform_scale (NULL, scale_x < 0 ? -1 : 1, scale_y < 0 ? -1 : 1),
                                                         &GRAPHENE_POINT_INIT (scale_x < 0 ? - (node->bounds.size.width + 2 * node->bounds.origin.x) : 0,
                                                                               scale_y < 0 ? - (node->bounds.size.height + 2 * node->bounds.origin.y) : 0));
      gsk_gl_render_job_push_modelview (job, transform);
      gsk_transform_unref (transform);
    }

  if (!gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
    goto out;

  gsk_gl_program_set_uniform_texture (job->current_program,
                                      UNIFORM_SHARED_SOURCE, 0,
                                      GL_TEXTURE_2D,
                                      GL_TEXTURE0,
                                      texture_id);
  job->source_is_glyph_atlas = FALSE;
  gsk_gl_render_job_draw_offscreen_rect (job, &node->bounds);
  gsk_gl_render_job_end_draw (job);

out:
  if (scale_x < 0 || scale_y < 0)
    gsk_gl_render_job_pop_modelview (job);
}

static guint
blur_offscreen (GskGLRenderJob       *job,
                GskGLRenderOffscreen *offscreen,
                int                   texture_to_blur_width,
                int                   texture_to_blur_height,
                float                 blur_radius_x,
                float                 blur_radius_y)
{
  const GskRoundedRect new_clip = GSK_ROUNDED_RECT_INIT (0, 0, texture_to_blur_width, texture_to_blur_height);
  GskGLRenderTarget *pass1;
  GskGLRenderTarget *pass2;
  graphene_matrix_t prev_projection;
  graphene_rect_t prev_viewport;
  guint prev_fbo;

  g_assert (blur_radius_x > 0);
  g_assert (blur_radius_y > 0);
  g_assert (offscreen->texture_id > 0);
  g_assert (offscreen->area.x2 > offscreen->area.x);
  g_assert (offscreen->area.y2 > offscreen->area.y);

  if (!gsk_gl_driver_create_render_target (job->driver,
                                           MAX (texture_to_blur_width, 1),
                                           MAX (texture_to_blur_height, 1),
                                           job->target_format,
                                           &pass1))
    return 0;

  if (texture_to_blur_width <= 0 || texture_to_blur_height <= 0)
    return gsk_gl_driver_release_render_target (job->driver, pass1, FALSE);

  if (!gsk_gl_driver_create_render_target (job->driver,
                                           texture_to_blur_width,
                                           texture_to_blur_height,
                                           job->target_format,
                                           &pass2))
    return gsk_gl_driver_release_render_target (job->driver, pass1, FALSE);

  gsk_gl_render_job_set_viewport (job, &new_clip.bounds, &prev_viewport);
  gsk_gl_render_job_set_projection_from_rect (job, &new_clip.bounds, &prev_projection);
  gsk_gl_render_job_set_modelview (job, NULL);
  gsk_gl_render_job_push_clip (job, &new_clip);

  /* Bind new framebuffer and clear it */
  prev_fbo = gsk_gl_command_queue_bind_framebuffer (job->command_queue, pass1->framebuffer_id);
  gsk_gl_command_queue_clear (job->command_queue, 0, &job->viewport);

  /* Begin drawing the first horizontal pass, using offscreen as the
   * source texture for the program.
   */
  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blur)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          offscreen->texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_program_set_uniform1f (job->current_program,
                                    UNIFORM_BLUR_RADIUS, 0,
                                    blur_radius_x);
      gsk_gl_program_set_uniform2f (job->current_program,
                                    UNIFORM_BLUR_SIZE, 0,
                                    texture_to_blur_width,
                                    texture_to_blur_height);
      gsk_gl_program_set_uniform2f (job->current_program,
                                    UNIFORM_BLUR_DIR, 0,
                                    1, 0);
      gsk_gl_render_job_draw_coords (job,
                                     0, 0, texture_to_blur_width, texture_to_blur_height,
                                     0, 1, 1, 0,
                                     (guint16[]) { FP16_ZERO, FP16_ZERO, FP16_ZERO, FP16_ZERO });
      gsk_gl_render_job_end_draw (job);
    }

  /* Bind second pass framebuffer and clear it */
  gsk_gl_command_queue_bind_framebuffer (job->command_queue, pass2->framebuffer_id);
  gsk_gl_command_queue_clear (job->command_queue, 0, &job->viewport);

  /* Draw using blur program with first pass as source texture */
  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blur)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          pass1->texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_program_set_uniform1f (job->current_program,
                                    UNIFORM_BLUR_RADIUS, 0,
                                    blur_radius_y);
      gsk_gl_program_set_uniform2f (job->current_program,
                                    UNIFORM_BLUR_SIZE, 0,
                                    texture_to_blur_width,
                                    texture_to_blur_height);
      gsk_gl_program_set_uniform2f (job->current_program,
                                    UNIFORM_BLUR_DIR, 0,
                                    0, 1);
      gsk_gl_render_job_draw_coords (job,
                                     0, 0, texture_to_blur_width, texture_to_blur_height,
                                     0, 1, 1, 0,
                                     (guint16[]) { FP16_ZERO, FP16_ZERO, FP16_ZERO, FP16_ZERO });
      gsk_gl_render_job_end_draw (job);
    }

  gsk_gl_render_job_pop_modelview (job);
  gsk_gl_render_job_pop_clip (job);
  gsk_gl_render_job_set_viewport (job, &prev_viewport, NULL);
  gsk_gl_render_job_set_projection (job, &prev_projection);
  gsk_gl_command_queue_bind_framebuffer (job->command_queue, prev_fbo);

  gsk_gl_driver_release_render_target (job->driver, pass1, TRUE);

  return gsk_gl_driver_release_render_target (job->driver, pass2, FALSE);
}

static void
blur_node (GskGLRenderJob       *job,
           GskGLRenderOffscreen *offscreen,
           const GskRenderNode  *node,
           float                 blur_radius,
           float                *min_x,
           float                *max_x,
           float                *min_y,
           float                *max_y)
{
  const float blur_extra = blur_radius * 2.0; /* 2.0 = shader radius_multiplier */
  const float half_blur_extra = (blur_extra / 2.0);
  float scale_x = job->scale_x;
  float scale_y = job->scale_y;
  float texture_width;
  float texture_height;

  g_assert (blur_radius > 0);

  /* Increase texture size for the given blur radius and scale it */
  texture_width  = ceilf ((node->bounds.size.width  + blur_extra));
  texture_height = ceilf ((node->bounds.size.height + blur_extra));

  /* Only blur this if the out region has no texture id yet */
  if (offscreen->texture_id == 0)
    {
      const graphene_rect_t bounds = GRAPHENE_RECT_INIT (node->bounds.origin.x - half_blur_extra,
                                                         node->bounds.origin.y - half_blur_extra,
                                                         texture_width, texture_height);

      offscreen->bounds = &bounds;
      offscreen->reset_clip = TRUE;
      offscreen->force_offscreen = TRUE;

      if (!gsk_gl_render_job_visit_node_with_offscreen (job, node, offscreen))
        g_assert_not_reached ();

      /* Ensure that we actually got a real texture_id */
      g_assert (offscreen->texture_id != 0);

      offscreen->texture_id = blur_offscreen (job,
                                              offscreen,
                                              texture_width * fabsf (scale_x),
                                              texture_height * fabsf (scale_y),
                                              blur_radius * fabsf (scale_x),
                                              blur_radius * fabsf (scale_y));
      init_full_texture_region (offscreen);
    }

  *min_x = job->offset_x + node->bounds.origin.x - half_blur_extra;
  *max_x = job->offset_x + node->bounds.origin.x + node->bounds.size.width + half_blur_extra;
  *min_y = job->offset_y + node->bounds.origin.y - half_blur_extra;
  *max_y = job->offset_y + node->bounds.origin.y + node->bounds.size.height + half_blur_extra;
}

#define ATLAS_SIZE 512

static inline void
gsk_gl_render_job_visit_color_node (GskGLRenderJob      *job,
                                    const GskRenderNode *node)
{
  const GdkRGBA *rgba;
  guint16 color[4];
  GskGLProgram *program;
  GskGLCommandBatch *batch;

  rgba = gsk_color_node_get_color (node);
  if (RGBA_IS_CLEAR (rgba))
    return;

  rgba_to_half (rgba, color);

  /* Avoid switching away from the coloring program for
   * rendering a solid color.
   */
  program = CHOOSE_PROGRAM (job, coloring);
  batch = gsk_gl_command_queue_get_batch (job->command_queue);

  /* Limit the size, or we end up with a coordinate overflow somewhere. */
  if (job->source_is_glyph_atlas &&
      node->bounds.size.width < 300 &&
      node->bounds.size.height < 300 &&
      batch->any.kind == GSK_GL_COMMAND_KIND_DRAW &&
      batch->any.program == program->id)
    {
      GskGLRenderOffscreen offscreen = {0};

      if (gsk_gl_render_job_begin_draw (job, program))
        {
          /* The top left few pixels in our atlases are always
           * solid white, so we can use it here, without
           * having to choose any particular atlas texture.
           */
          offscreen.was_offscreen = FALSE;
          offscreen.area.x = 1.f / ATLAS_SIZE;
          offscreen.area.y = 1.f / ATLAS_SIZE;
          offscreen.area.x2 = 2.f / ATLAS_SIZE;
          offscreen.area.y2 = 2.f / ATLAS_SIZE;

          gsk_gl_render_job_draw_offscreen_with_color (job,
                                                       &node->bounds,
                                                       &offscreen,
                                                       color);

          gsk_gl_render_job_end_draw (job);
       }
    }
  else
    {
      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, color)))
        {
          gsk_gl_render_job_draw_rect_with_color (job, &node->bounds, color);
          gsk_gl_render_job_end_draw (job);
        }
    }
}

static inline void
gsk_gl_render_job_visit_linear_gradient_node (GskGLRenderJob      *job,
                                              const GskRenderNode *node)
{
  const GskColorStop *stops = gsk_linear_gradient_node_get_color_stops (node, NULL);
  const graphene_point_t *start = gsk_linear_gradient_node_get_start (node);
  const graphene_point_t *end = gsk_linear_gradient_node_get_end (node);
  int n_color_stops = gsk_linear_gradient_node_get_n_color_stops (node);
  gboolean repeat = GSK_RENDER_NODE_TYPE (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE;
  float x1 = job->offset_x + start->x;
  float x2 = job->offset_x + end->x;
  float y1 = job->offset_y + start->y;
  float y2 = job->offset_y + end->y;

  g_assert (n_color_stops < MAX_GRADIENT_STOPS);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, linear_gradient)))
    {
      gsk_gl_program_set_uniform1i (job->current_program,
                                    UNIFORM_LINEAR_GRADIENT_NUM_COLOR_STOPS, 0,
                                    n_color_stops);
      gsk_gl_program_set_uniform1fv (job->current_program,
                                     UNIFORM_LINEAR_GRADIENT_COLOR_STOPS, 0,
                                     n_color_stops * 5,
                                     (const float *)stops);
      gsk_gl_program_set_uniform4f (job->current_program,
                                    UNIFORM_LINEAR_GRADIENT_POINTS, 0,
                                    x1, y1, x2 - x1, y2 - y1);
      gsk_gl_program_set_uniform1i (job->current_program,
                                    UNIFORM_LINEAR_GRADIENT_REPEAT, 0,
                                    repeat);
      gsk_gl_render_job_draw_rect (job, &node->bounds);
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_conic_gradient_node (GskGLRenderJob      *job,
                                             const GskRenderNode *node)
{
  static const float scale = 0.5f * M_1_PI;

  const GskColorStop *stops = gsk_conic_gradient_node_get_color_stops (node, NULL);
  const graphene_point_t *center = gsk_conic_gradient_node_get_center (node);
  int n_color_stops = gsk_conic_gradient_node_get_n_color_stops (node);
  float angle = gsk_conic_gradient_node_get_angle (node);
  float bias = angle * scale + 2.0f;

  g_assert (n_color_stops < MAX_GRADIENT_STOPS);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, conic_gradient)))
    {
      gsk_gl_program_set_uniform1i (job->current_program,
                                    UNIFORM_CONIC_GRADIENT_NUM_COLOR_STOPS, 0,
                                    n_color_stops);
      gsk_gl_program_set_uniform1fv (job->current_program,
                                     UNIFORM_CONIC_GRADIENT_COLOR_STOPS, 0,
                                     n_color_stops * 5,
                                     (const float *)stops);
      gsk_gl_program_set_uniform4f (job->current_program,
                                    UNIFORM_CONIC_GRADIENT_GEOMETRY, 0,
                                    job->offset_x + center->x,
                                    job->offset_y + center->y,
                                    scale,
                                    bias);
      gsk_gl_render_job_draw_rect (job, &node->bounds);
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_radial_gradient_node (GskGLRenderJob      *job,
                                              const GskRenderNode *node)
{
  int n_color_stops = gsk_radial_gradient_node_get_n_color_stops (node);
  const GskColorStop *stops = gsk_radial_gradient_node_get_color_stops (node, NULL);
  const graphene_point_t *center = gsk_radial_gradient_node_get_center (node);
  float start = gsk_radial_gradient_node_get_start (node);
  float end = gsk_radial_gradient_node_get_end (node);
  float hradius = gsk_radial_gradient_node_get_hradius (node);
  float vradius = gsk_radial_gradient_node_get_vradius (node);
  gboolean repeat = GSK_RENDER_NODE_TYPE (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE;
  float scale = 1.0f / (end - start);
  float bias = -start * scale;

  g_assert (n_color_stops < MAX_GRADIENT_STOPS);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, radial_gradient)))
    {
      gsk_gl_program_set_uniform1i (job->current_program,
                                    UNIFORM_RADIAL_GRADIENT_NUM_COLOR_STOPS, 0,
                                    n_color_stops);
      gsk_gl_program_set_uniform1fv (job->current_program,
                                     UNIFORM_RADIAL_GRADIENT_COLOR_STOPS, 0,
                                     n_color_stops * 5,
                                     (const float *)stops);
      gsk_gl_program_set_uniform1i (job->current_program,
                                    UNIFORM_RADIAL_GRADIENT_REPEAT, 0,
                                    repeat);
      gsk_gl_program_set_uniform2f (job->current_program,
                                    UNIFORM_RADIAL_GRADIENT_RANGE, 0,
                                    scale, bias);
      gsk_gl_program_set_uniform4f (job->current_program,
                                    UNIFORM_RADIAL_GRADIENT_GEOMETRY, 0,
                                    job->offset_x + center->x,
                                    job->offset_y + center->y,
                                    1.0f / (hradius * job->scale_x),
                                    1.0f / (vradius * job->scale_y));
      gsk_gl_render_job_draw_rect (job, &node->bounds);
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_clipped_child (GskGLRenderJob        *job,
                                       const GskRenderNode   *child,
                                       const graphene_rect_t *clip)
{
  graphene_rect_t transformed_clip;
  GskRoundedRect intersection;
  GskRoundedRectIntersection result;

  gsk_gl_render_job_transform_bounds (job, clip, &transformed_clip);

  if (job->current_clip->is_rectilinear)
    {
      memset (&intersection.corner, 0, sizeof intersection.corner);
      gsk_rect_intersection (&transformed_clip,
                             &job->current_clip->rect.bounds,
                             &intersection.bounds);

      gsk_gl_render_job_push_clip (job, &intersection);
      gsk_gl_render_job_visit_node (job, child);
      gsk_gl_render_job_pop_clip (job);
      return;
    }

  result = gsk_rounded_rect_intersect_with_rect (&job->current_clip->rect,
                                                 &transformed_clip,
                                                 &intersection);

  if (result == GSK_INTERSECTION_EMPTY)
    return;

  if (result == GSK_INTERSECTION_NONEMPTY)
    {
      gsk_gl_render_job_push_clip (job, &intersection);
      gsk_gl_render_job_visit_node (job, child);
      gsk_gl_render_job_pop_clip (job);
    }
  else
    {
      GskGLRenderOffscreen offscreen = {0};

      offscreen.bounds = clip;
      offscreen.force_offscreen = TRUE;
      offscreen.reset_clip = TRUE;
      offscreen.do_not_cache = TRUE;

      gsk_gl_render_job_visit_node_with_offscreen (job, child, &offscreen);

      g_assert (offscreen.texture_id);

      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
        {
          gsk_gl_program_set_uniform_texture (job->current_program,
                                              UNIFORM_SHARED_SOURCE, 0,
                                              GL_TEXTURE_2D,
                                              GL_TEXTURE0,
                                              offscreen.texture_id);
          job->source_is_glyph_atlas = FALSE;
          gsk_gl_render_job_draw_offscreen_rect (job, clip);
          gsk_gl_render_job_end_draw (job);
        }
    }
}

static inline void
gsk_gl_render_job_visit_clip_node (GskGLRenderJob      *job,
                                   const GskRenderNode *node)
{
  const graphene_rect_t *clip = gsk_clip_node_get_clip (node);
  const GskRenderNode *child = gsk_clip_node_get_child (node);

  gsk_gl_render_job_visit_clipped_child (job, child, clip);
}

static inline void
gsk_gl_render_job_visit_rounded_clip_node (GskGLRenderJob      *job,
                                           const GskRenderNode *node)
{
  const GskRenderNode *child = gsk_rounded_clip_node_get_child (node);
  const GskRoundedRect *clip = gsk_rounded_clip_node_get_clip (node);
  GskRoundedRect transformed_clip;
  gboolean need_offscreen;

  if (node_is_invisible (child))
    return;

  gsk_gl_render_job_transform_rounded_rect (job, clip, &transformed_clip);

  if (job->current_clip->is_rectilinear)
    {
      GskRoundedRect intersected_clip;
      GskRoundedRectIntersection result;

      result = gsk_rounded_rect_intersect_with_rect (&transformed_clip,
                                                     &job->current_clip->rect.bounds,
                                                     &intersected_clip);

      if (result == GSK_INTERSECTION_EMPTY)
        return;

      if (result == GSK_INTERSECTION_NONEMPTY)
        {
          gsk_gl_render_job_push_clip (job, &intersected_clip);
          gsk_gl_render_job_visit_node (job, child);
          gsk_gl_render_job_pop_clip (job);
          return;
        }
    }

  /* After this point we are really working with a new and a current clip
   * which both have rounded corners.
   */

  if (clips_get_size (&job->clip) <= 1)
    need_offscreen = FALSE;
  else if (gsk_rounded_rect_contains_rect (&job->current_clip->rect, &transformed_clip.bounds))
    need_offscreen = FALSE;
  else
    need_offscreen = TRUE;

  if (!need_offscreen)
    {
      gsk_gl_render_job_push_clip (job, &transformed_clip);
      gsk_gl_render_job_visit_node (job, child);
      gsk_gl_render_job_pop_clip (job);
    }
  else
    {
      GskGLRenderOffscreen offscreen = {0};

      offscreen.bounds = &node->bounds;
      offscreen.force_offscreen = TRUE;
      offscreen.reset_clip = FALSE;

      gsk_gl_render_job_push_clip (job, &transformed_clip);
      if (!gsk_gl_render_job_visit_node_with_offscreen (job, child, &offscreen))
        g_assert_not_reached ();
      gsk_gl_render_job_pop_clip (job);

      g_assert (offscreen.texture_id);

      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
        {
          gsk_gl_program_set_uniform_texture (job->current_program,
                                              UNIFORM_SHARED_SOURCE, 0,
                                              GL_TEXTURE_2D,
                                              GL_TEXTURE0,
                                              offscreen.texture_id);
          job->source_is_glyph_atlas = FALSE;
          gsk_gl_render_job_draw_offscreen (job, &node->bounds, &offscreen);
          gsk_gl_render_job_end_draw (job);
        }
    }
}

static inline void
gsk_gl_render_job_visit_rect_border_node (GskGLRenderJob      *job,
                                          const GskRenderNode *node)
{
  const GdkRGBA *colors = gsk_border_node_get_colors (node);
  const float *widths = gsk_border_node_get_widths (node);
  const graphene_point_t *origin = &node->bounds.origin;
  const graphene_size_t *size = &node->bounds.size;
  guint16 color[4];

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, color)))
    {
      if (widths[0] > 0)
        {
          rgba_to_half (&colors[0], color);
          gsk_gl_render_job_draw_rect_with_color (job,
                                                  &GRAPHENE_RECT_INIT (origin->x, origin->y, size->width - widths[1], widths[0]),
                                                  color);
        }

      if (widths[1] > 0)
        {
          rgba_to_half (&colors[1], color);
          gsk_gl_render_job_draw_rect_with_color (job,
                                                  &GRAPHENE_RECT_INIT (origin->x + size->width - widths[1], origin->y, widths[1], size->height - widths[2]),
                                                  color);
        }

      if (widths[2] > 0)
        {
          rgba_to_half (&colors[2], color);
          gsk_gl_render_job_draw_rect_with_color (job,
                                                  &GRAPHENE_RECT_INIT (origin->x + widths[3], origin->y + size->height - widths[2], size->width - widths[3], widths[2]),
                                                  color);
        }

      if (widths[3] > 0)
        {
          rgba_to_half (&colors[3], color);
          gsk_gl_render_job_draw_rect_with_color (job,
                                                  &GRAPHENE_RECT_INIT (origin->x, origin->y + widths[0], widths[3], size->height - widths[0]),
                                                  color);
        }

      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_border_node (GskGLRenderJob      *job,
                                     const GskRenderNode *node)
{
  const GskRoundedRect *rounded_outline = gsk_border_node_get_outline (node);
  const GdkRGBA *colors = gsk_border_node_get_colors (node);
  const float *widths = gsk_border_node_get_widths (node);
  struct {
    float w;
    float h;
  } sizes[4];
  float min_x = job->offset_x + node->bounds.origin.x;
  float min_y = job->offset_y + node->bounds.origin.y;
  float max_x = min_x + node->bounds.size.width;
  float max_y = min_y + node->bounds.size.height;
  GskRoundedRect outline;
  guint16 color[4];

  memset (sizes, 0, sizeof sizes);

  if (widths[0] > 0)
    {
      sizes[0].h = MAX (widths[0], rounded_outline->corner[0].height);
      sizes[1].h = MAX (widths[0], rounded_outline->corner[1].height);
    }

  if (widths[1] > 0)
    {
      sizes[1].w = MAX (widths[1], rounded_outline->corner[1].width);
      sizes[2].w = MAX (widths[1], rounded_outline->corner[2].width);
    }

  if (widths[2] > 0)
    {
      sizes[2].h = MAX (widths[2], rounded_outline->corner[2].height);
      sizes[3].h = MAX (widths[2], rounded_outline->corner[3].height);
    }

  if (widths[3] > 0)
    {
      sizes[0].w = MAX (widths[3], rounded_outline->corner[0].width);
      sizes[3].w = MAX (widths[3], rounded_outline->corner[3].width);
    }

  gsk_gl_render_job_translate_rounded_rect (job, rounded_outline, &outline);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, border)))
    {
      gsk_gl_program_set_uniform4fv (job->current_program,
                                     UNIFORM_BORDER_WIDTHS, 0,
                                     1,
                                     widths);
      gsk_gl_program_set_uniform_rounded_rect (job->current_program,
                                               UNIFORM_BORDER_OUTLINE_RECT, 0,
                                               &outline);

      if (widths[0] > 0)
        {
          GskGLDrawVertex *vertices = gsk_gl_command_queue_add_vertices (job->command_queue);

          rgba_to_half (&colors[0], color);

          vertices[0] = (GskGLDrawVertex) { .position = { min_x,              min_y              }, .uv = { 0, 1 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[1] = (GskGLDrawVertex) { .position = { min_x + sizes[0].w, min_y + sizes[0].h }, .uv = { 0, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[2] = (GskGLDrawVertex) { .position = { max_x,              min_y              }, .uv = { 1, 1 }, .color = { color[0], color[1], color[2], color[3] } };

          vertices[3] = (GskGLDrawVertex) { .position = { max_x - sizes[1].w, min_y + sizes[1].h }, .uv = { 1, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[4] = (GskGLDrawVertex) { .position = { min_x + sizes[0].w, min_y + sizes[0].h }, .uv = { 0, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[5] = (GskGLDrawVertex) { .position = { max_x,              min_y              }, .uv = { 1, 1 }, .color = { color[0], color[1], color[2], color[3] } };
        }

      if (widths[1] > 0)
        {
          GskGLDrawVertex *vertices = gsk_gl_command_queue_add_vertices (job->command_queue);

          rgba_to_half (&colors[1], color);

          vertices[0] = (GskGLDrawVertex) { .position = { max_x - sizes[1].w, min_y + sizes[1].h }, .uv = { 0, 1 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[1] = (GskGLDrawVertex) { .position = { max_x - sizes[2].w, max_y - sizes[2].h }, .uv = { 0, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[2] = (GskGLDrawVertex) { .position = { max_x,              min_y              }, .uv = { 1, 1 }, .color = { color[0], color[1], color[2], color[3] } };

          vertices[3] = (GskGLDrawVertex) { .position = { max_x,              max_y              }, .uv = { 1, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[4] = (GskGLDrawVertex) { .position = { max_x - sizes[2].w, max_y - sizes[2].h }, .uv = { 0, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[5] = (GskGLDrawVertex) { .position = { max_x,              min_y              }, .uv = { 1, 1 }, .color = { color[0], color[1], color[2], color[3] } };
        }

      if (widths[2] > 0)
        {
          GskGLDrawVertex *vertices = gsk_gl_command_queue_add_vertices (job->command_queue);

          rgba_to_half (&colors[2], color);

          vertices[0] = (GskGLDrawVertex) { .position = { min_x + sizes[3].w, max_y - sizes[3].h }, .uv = { 0, 1 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[1] = (GskGLDrawVertex) { .position = { min_x,              max_y              }, .uv = { 0, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[2] = (GskGLDrawVertex) { .position = { max_x - sizes[2].w, max_y - sizes[2].h }, .uv = { 1, 1 }, .color = { color[0], color[1], color[2], color[3] } };

          vertices[3] = (GskGLDrawVertex) { .position = { max_x,              max_y              }, .uv = { 1, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[4] = (GskGLDrawVertex) { .position = { min_x            ,  max_y              }, .uv = { 0, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[5] = (GskGLDrawVertex) { .position = { max_x - sizes[2].w, max_y - sizes[2].h }, .uv = { 1, 1 }, .color = { color[0], color[1], color[2], color[3] } };
        }

      if (widths[3] > 0)
        {
          GskGLDrawVertex *vertices = gsk_gl_command_queue_add_vertices (job->command_queue);

          rgba_to_half (&colors[3], color);

          vertices[0] = (GskGLDrawVertex) { .position = { min_x,              min_y              }, .uv = { 0, 1 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[1] = (GskGLDrawVertex) { .position = { min_x,              max_y              }, .uv = { 0, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[2] = (GskGLDrawVertex) { .position = { min_x + sizes[0].w, min_y + sizes[0].h }, .uv = { 1, 1 }, .color = { color[0], color[1], color[2], color[3] } };

          vertices[3] = (GskGLDrawVertex) { .position = { min_x + sizes[3].w, max_y - sizes[3].h }, .uv = { 1, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[4] = (GskGLDrawVertex) { .position = { min_x,              max_y              }, .uv = { 0, 0 }, .color = { color[0], color[1], color[2], color[3] } };
          vertices[5] = (GskGLDrawVertex) { .position = { min_x + sizes[0].w, min_y + sizes[0].h }, .uv = { 1, 1 }, .color = { color[0], color[1], color[2], color[3] } };
        }

      gsk_gl_render_job_end_draw (job);
    }
}

/* A special case for a pattern that occurs frequently with CSS
 * backgrounds: two sibling nodes, the first of which is a rounded
 * clip node with a color node as child, and the second one is a
 * border node, with the same outline as the clip node. We render
 * this using the filled_border shader.
 */
static void
gsk_gl_render_job_visit_css_background (GskGLRenderJob      *job,
                                        const GskRenderNode *node,
                                        const GskRenderNode *node2)
{
  const GskRenderNode *child = gsk_rounded_clip_node_get_child (node);
  const GskRoundedRect *rounded_outline = gsk_border_node_get_outline (node2);
  const float *widths = gsk_border_node_get_widths (node2);
  float min_x = job->offset_x + node2->bounds.origin.x;
  float min_y = job->offset_y + node2->bounds.origin.y;
  float max_x = min_x + node2->bounds.size.width;
  float max_y = min_y + node2->bounds.size.height;
  GskRoundedRect outline;
  GskGLDrawVertex *vertices;
  guint16 color[4];
  guint16 color2[4];

  if (node_is_invisible (node2))
    return;

  rgba_to_half (&gsk_border_node_get_colors (node2)[0], color);
  rgba_to_half (gsk_color_node_get_color (child), color2);

  gsk_gl_render_job_translate_rounded_rect (job, rounded_outline, &outline);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, filled_border)))
    {
      gsk_gl_program_set_uniform4fv (job->current_program,
                                     UNIFORM_FILLED_BORDER_WIDTHS, 0,
                                     1,
                                     widths);
      gsk_gl_program_set_uniform_rounded_rect (job->current_program,
                                               UNIFORM_FILLED_BORDER_OUTLINE_RECT, 0,
                                               &outline);

      vertices = gsk_gl_command_queue_add_vertices (job->command_queue);

      vertices[0] = (GskGLDrawVertex) { .position = { min_x, min_y }, .color = { color[0], color[1], color[2], color[3] }, .color2 = { color2[0], color2[1], color2[2], color2[3] } };
      vertices[1] = (GskGLDrawVertex) { .position = { min_x, max_y }, .color = { color[0], color[1], color[2], color[3] }, .color2 = { color2[0], color2[1], color2[2], color2[3] } };
      vertices[2] = (GskGLDrawVertex) { .position = { max_x, min_y }, .color = { color[0], color[1], color[2], color[3] }, .color2 = { color2[0], color2[1], color2[2], color2[3] } };
      vertices[3] = (GskGLDrawVertex) { .position = { max_x, max_y }, .color = { color[0], color[1], color[2], color[3] }, .color2 = { color2[0], color2[1], color2[2], color2[3] } };
      vertices[4] = (GskGLDrawVertex) { .position = { min_x, max_y }, .color = { color[0], color[1], color[2], color[3] }, .color2 = { color2[0], color2[1], color2[2], color2[3] } };
      vertices[5] = (GskGLDrawVertex) { .position = { max_x, min_y }, .color = { color[0], color[1], color[2], color[3] }, .color2 = { color2[0], color2[1], color2[2], color2[3] } };

      gsk_gl_render_job_end_draw (job);
    }
}

/* Returns TRUE if applying @transform to @bounds
 * yields an axis-aligned rectangle
 */
static gboolean
result_is_axis_aligned (GskTransform          *transform,
                        const graphene_rect_t *bounds)
{
  graphene_matrix_t m;
  graphene_quad_t q;
  graphene_rect_t b;
  graphene_point_t b1, b2;
  const graphene_point_t *p;

  gsk_transform_to_matrix (transform, &m);
  gsk_matrix_transform_rect (&m, bounds, &q);
  graphene_quad_bounds (&q, &b);
  graphene_rect_get_top_left (&b, &b1);
  graphene_rect_get_bottom_right (&b, &b2);

  for (guint i = 0; i < 4; i++)
    {
      p = graphene_quad_get_point (&q, i);
      if (fabsf (p->x - b1.x) > FLT_EPSILON && fabsf (p->x - b2.x) > FLT_EPSILON)
        return FALSE;
      if (fabsf (p->y - b1.y) > FLT_EPSILON && fabsf (p->y - b2.y) > FLT_EPSILON)
        return FALSE;
    }

  return TRUE;
}

static inline void
gsk_gl_render_job_visit_transform_node (GskGLRenderJob      *job,
                                        const GskRenderNode *node)
{
  GskTransform *transform = gsk_transform_node_get_transform (node);
  const GskTransformCategory category = gsk_transform_get_category (transform);
  const GskRenderNode *child = gsk_transform_node_get_child (node);

  switch (category)
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      gsk_gl_render_job_visit_node (job, child);
    break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;

        gsk_transform_node_get_translate (node, &dx, &dy);
        gsk_gl_render_job_offset (job, dx, dy);
        gsk_gl_render_job_visit_node (job, child);
        gsk_gl_render_job_offset (job, -dx, -dy);
      }
    break;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        gsk_gl_render_job_push_modelview (job, transform);
        gsk_gl_render_job_visit_node (job, child);
        gsk_gl_render_job_pop_modelview (job);
      }
    break;

    case GSK_TRANSFORM_CATEGORY_2D:
      if (node_supports_2d_transform (child))
        {
          gsk_gl_render_job_push_modelview (job, transform);
          gsk_gl_render_job_visit_node (job, child);
          gsk_gl_render_job_pop_modelview (job);
          return;
        }
      G_GNUC_FALLTHROUGH;
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
      if (node_supports_transform (child))
        {
          gsk_gl_render_job_push_modelview (job, transform);
          gsk_gl_render_job_visit_node (job, child);
          gsk_gl_render_job_pop_modelview (job);
        }
      else
        {
          GskGLRenderOffscreen offscreen = {0};
          float sx = 1, sy  = 1;
          gboolean linear_filter = FALSE;

          offscreen.bounds = &child->bounds;
          offscreen.force_offscreen = FALSE;
          offscreen.reset_clip = TRUE;

          if (!result_is_axis_aligned (transform, &child->bounds))
            linear_filter = TRUE;

          if (category == GSK_TRANSFORM_CATEGORY_2D)
            {
              graphene_matrix_t m;
              double a, b, c, d, tx, ty;

              g_assert (transform != NULL);
              gsk_transform_to_matrix (transform, &m);
              if (graphene_matrix_to_2d (&m, &a, &b, &c, &d, &tx, &ty))
                {
                  sx = sqrt (a * a + b * b);
                  sy = sqrt (c * c + d * d);
                }
              else
                sx = sy = 1;

              if (sx != 1 || sy != 1)
                {
                  GskTransform *scale;

                  scale = gsk_transform_translate (gsk_transform_scale (NULL, sx, sy), &GRAPHENE_POINT_INIT (tx, ty));
                  gsk_gl_render_job_push_modelview (job, scale);
                  transform = gsk_transform_transform (gsk_transform_invert (scale), transform);
                }
            }

          if (gsk_gl_render_job_visit_node_with_offscreen (job, child, &offscreen))
            {
              /* For non-trivial transforms, we draw everything on a texture and then
               * draw the texture transformed.
               */
              if (transform)
                gsk_gl_render_job_push_modelview (job, transform);

              if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
                {
                  gsk_gl_program_set_uniform_texture_with_filter (job->current_program,
                                                                  UNIFORM_SHARED_SOURCE, 0,
                                                                  GL_TEXTURE_2D,
                                                                  GL_TEXTURE0,
                                                                  offscreen.texture_id,
                                                                  linear_filter ? GL_LINEAR : GL_NEAREST,
                                                                  linear_filter ? GL_LINEAR : GL_NEAREST);
                  job->source_is_glyph_atlas = FALSE;
                  gsk_gl_render_job_draw_offscreen (job, &child->bounds, &offscreen);
                  gsk_gl_render_job_end_draw (job);
                }

              if (transform)
                gsk_gl_render_job_pop_modelview (job);
            }

          if (category == GSK_TRANSFORM_CATEGORY_2D)
            {
              if (sx != 1 || sy != 1)
                {
                  gsk_gl_render_job_pop_modelview (job);
                  gsk_transform_unref (transform);
                }
            }
        }
    break;

    default:
      g_assert_not_reached ();
    }
}

static inline void
gsk_gl_render_job_visit_unblurred_inset_shadow_node (GskGLRenderJob      *job,
                                                     const GskRenderNode *node)
{
  const GskRoundedRect *outline = gsk_inset_shadow_node_get_outline (node);
  GskRoundedRect transformed_outline;
  guint16 color[4];

  gsk_gl_render_job_translate_rounded_rect (job, outline, &transformed_outline);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, inset_shadow)))
    {
      gsk_gl_program_set_uniform_rounded_rect (job->current_program,
                                               UNIFORM_INSET_SHADOW_OUTLINE_RECT, 0,
                                               &transformed_outline);
      gsk_gl_program_set_uniform1f (job->current_program,
                                    UNIFORM_INSET_SHADOW_SPREAD, 0,
                                    gsk_inset_shadow_node_get_spread (node));
      gsk_gl_program_set_uniform2f (job->current_program,
                                    UNIFORM_INSET_SHADOW_OFFSET, 0,
                                    gsk_inset_shadow_node_get_dx (node),
                                    gsk_inset_shadow_node_get_dy (node));
      rgba_to_half (gsk_inset_shadow_node_get_color (node), color);
      gsk_gl_render_job_draw_rect_with_color (job, &node->bounds, color);
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_blurred_inset_shadow_node (GskGLRenderJob      *job,
                                                   const GskRenderNode *node)
{
  const GskRoundedRect *node_outline = gsk_inset_shadow_node_get_outline (node);
  float blur_radius = gsk_inset_shadow_node_get_blur_radius (node);
  float offset_x = gsk_inset_shadow_node_get_dx (node);
  float offset_y = gsk_inset_shadow_node_get_dy (node);
  float scale_x = job->scale_x;
  float scale_y = job->scale_y;
  float blur_extra = blur_radius * 2.0; /* 2.0 = shader radius_multiplier */
  float half_blur_extra = blur_radius;
  float texture_width;
  float texture_height;
  int blurred_texture_id;
  GskTextureKey key;
  GskGLRenderOffscreen offscreen = {0};
  guint16 color[4];

  g_assert (blur_radius > 0);

  texture_width = ceilf ((node_outline->bounds.size.width + blur_extra) * scale_x);
  texture_height = ceilf ((node_outline->bounds.size.height + blur_extra) * scale_y);

  key.pointer = node;
  key.pointer_is_child = FALSE;
  key.scale_x = scale_x;
  key.scale_y = scale_y;

  blurred_texture_id = gsk_gl_driver_lookup_texture (job->driver, &key, NULL);

  if (blurred_texture_id == 0)
    {
      float spread = gsk_inset_shadow_node_get_spread (node) + half_blur_extra;
      GskRoundedRect transformed_outline;
      GskRoundedRect outline_to_blur;
      GskGLRenderTarget *render_target;
      graphene_matrix_t prev_projection;
      graphene_rect_t prev_viewport;
      guint prev_fbo;

      /* TODO: In the following code, we have to be careful about where we apply the scale.
       * We're manually scaling stuff (e.g. the outline) so we can later use texture_width
       * and texture_height (which are already scaled) as the geometry and keep the modelview
       * at a scale of 1. That's kinda complicated though... */

      /* Outline of what we actually want to blur later.
       * Spread grows inside, so we don't need to account for that. But the blur will need
       * to read outside of the inset shadow, so we need to draw some color in there. */
      outline_to_blur = *node_outline;
      gsk_rounded_rect_shrink (&outline_to_blur,
                               -half_blur_extra,
                               -half_blur_extra,
                               -half_blur_extra,
                               -half_blur_extra);

      /* Fit to our texture */
      outline_to_blur.bounds.origin.x = 0;
      outline_to_blur.bounds.origin.y = 0;
      outline_to_blur.bounds.size.width *= scale_x;
      outline_to_blur.bounds.size.height *= scale_y;

      for (guint i = 0; i < 4; i ++)
        {
          outline_to_blur.corner[i].width *= scale_x;
          outline_to_blur.corner[i].height *= scale_y;
        }

      if (!gsk_gl_driver_create_render_target (job->driver,
                                               texture_width, texture_height,
                                               get_target_format (job, node),
                                               &render_target))
        g_assert_not_reached ();

      gsk_gl_render_job_set_viewport_for_size (job, texture_width, texture_height, &prev_viewport);
      gsk_gl_render_job_set_projection_for_size (job, texture_width, texture_height, &prev_projection);
      gsk_gl_render_job_set_modelview (job, NULL);
      gsk_gl_render_job_push_clip (job, &GSK_ROUNDED_RECT_INIT (0, 0, texture_width, texture_height));

      prev_fbo = gsk_gl_command_queue_bind_framebuffer (job->command_queue, render_target->framebuffer_id);
      gsk_gl_command_queue_clear (job->command_queue, 0, &job->viewport);

      gsk_gl_render_job_translate_rounded_rect (job, &outline_to_blur, &transformed_outline);

      /* Actual inset shadow outline drawing */
      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, inset_shadow)))
        {
          gsk_gl_program_set_uniform_rounded_rect (job->current_program,
                                                   UNIFORM_INSET_SHADOW_OUTLINE_RECT, 0,
                                                   &transformed_outline);
          gsk_gl_program_set_uniform1f (job->current_program,
                                        UNIFORM_INSET_SHADOW_SPREAD, 0,
                                        spread * MAX (scale_x, scale_y));
          gsk_gl_program_set_uniform2f (job->current_program,
                                        UNIFORM_INSET_SHADOW_OFFSET, 0,
                                        offset_x * scale_x,
                                        offset_y * scale_y);
          rgba_to_half (gsk_inset_shadow_node_get_color (node), color);
          gsk_gl_render_job_draw_with_color (job,
                                             0, 0, texture_width, texture_height,
                                             color);
          gsk_gl_render_job_end_draw (job);
        }

      gsk_gl_render_job_pop_modelview (job);
      gsk_gl_render_job_pop_clip (job);
      gsk_gl_render_job_set_projection (job, &prev_projection);
      gsk_gl_render_job_set_viewport (job, &prev_viewport, NULL);
      gsk_gl_command_queue_bind_framebuffer (job->command_queue, prev_fbo);

      offscreen.texture_id = render_target->texture_id;
      init_full_texture_region (&offscreen);

      blurred_texture_id = blur_offscreen (job,
                                           &offscreen,
                                           texture_width,
                                           texture_height,
                                           blur_radius * fabsf (scale_x),
                                           blur_radius * fabsf (scale_y));

      gsk_gl_driver_release_render_target (job->driver, render_target, TRUE);

      gsk_gl_driver_cache_texture (job->driver, &key, blurred_texture_id);
    }

  g_assert (blurred_texture_id != 0);

  /* Blur the rendered unblurred inset shadow */
  /* Use a clip to cut away the unwanted parts outside of the original outline */
  {
    const gboolean needs_clip = !gsk_rounded_rect_is_rectilinear (node_outline);
    const float tx1 = half_blur_extra * scale_x / texture_width;
    const float tx2 = 1.0 - tx1;
    const float ty1 = half_blur_extra * scale_y / texture_height;
    const float ty2 = 1.0 - ty1;

    if (needs_clip)
      {
        GskRoundedRect node_clip;

        gsk_gl_render_job_translate_rounded_rect (job, node_outline, &node_clip);
        gsk_gl_render_job_push_clip (job, &node_clip);
      }

    offscreen.was_offscreen = TRUE;
    offscreen.area.x = tx1;
    offscreen.area.y = ty1;
    offscreen.area.x2 = tx2;
    offscreen.area.y2 = ty2;

    if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
      {
        gsk_gl_program_set_uniform_texture (job->current_program,
                                            UNIFORM_SHARED_SOURCE, 0,
                                            GL_TEXTURE_2D,
                                            GL_TEXTURE0,
                                            blurred_texture_id);
        job->source_is_glyph_atlas = FALSE;
        gsk_gl_render_job_draw_offscreen (job, &node->bounds, &offscreen);
        gsk_gl_render_job_end_draw (job);
      }

    if (needs_clip)
      gsk_gl_render_job_pop_clip (job);
  }
}

static inline void
gsk_gl_render_job_visit_unblurred_outset_shadow_node (GskGLRenderJob      *job,
                                                      const GskRenderNode *node)
{
  const GskRoundedRect *outline = gsk_outset_shadow_node_get_outline (node);
  GskRoundedRect transformed_outline;
  float x = node->bounds.origin.x;
  float y = node->bounds.origin.y;
  float w = node->bounds.size.width;
  float h = node->bounds.size.height;
  float spread = gsk_outset_shadow_node_get_spread (node);
  float dx = gsk_outset_shadow_node_get_dx (node);
  float dy = gsk_outset_shadow_node_get_dy (node);
  guint16 color[4];
  const float edge_sizes[] = { // Top, right, bottom, left
    spread - dy, spread + dx, spread + dy, spread - dx
  };
  const float corner_sizes[][2] = { // top left, top right, bottom right, bottom left
    { outline->corner[0].width + spread - dx, outline->corner[0].height + spread - dy },
    { outline->corner[1].width + spread + dx, outline->corner[1].height + spread - dy },
    { outline->corner[2].width + spread + dx, outline->corner[2].height + spread + dy },
    { outline->corner[3].width + spread - dx, outline->corner[3].height + spread + dy },
  };

  rgba_to_half (gsk_outset_shadow_node_get_color (node), color);

  gsk_gl_render_job_translate_rounded_rect (job, outline, &transformed_outline);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, unblurred_outset_shadow)))
    {
      gsk_gl_program_set_uniform_rounded_rect (job->current_program,
                                               UNIFORM_UNBLURRED_OUTSET_SHADOW_OUTLINE_RECT, 0,
                                               &transformed_outline);
      gsk_gl_program_set_uniform1f (job->current_program,
                                    UNIFORM_UNBLURRED_OUTSET_SHADOW_SPREAD, 0,
                                    spread);
      gsk_gl_program_set_uniform2f (job->current_program,
                                    UNIFORM_UNBLURRED_OUTSET_SHADOW_OFFSET, 0,
                                    dx, dy);

      /* Corners... */
      if (corner_sizes[0][0] > 0 && corner_sizes[0][1] > 0) /* Top left */
        gsk_gl_render_job_draw_with_color (job,
                                           x, y, corner_sizes[0][0], corner_sizes[0][1],
                                           color);
      if (corner_sizes[1][0] > 0 && corner_sizes[1][1] > 0) /* Top right */
        gsk_gl_render_job_draw_with_color (job,
                                           x + w - corner_sizes[1][0], y,
                                           corner_sizes[1][0], corner_sizes[1][1],
                                           color);
      if (corner_sizes[2][0] > 0 && corner_sizes[2][1] > 0) /* Bottom right */
        gsk_gl_render_job_draw_with_color (job,
                                           x + w - corner_sizes[2][0], y + h - corner_sizes[2][1],
                                           corner_sizes[2][0], corner_sizes[2][1],
                                           color);
      if (corner_sizes[3][0] > 0 && corner_sizes[3][1] > 0) /* Bottom left */
        gsk_gl_render_job_draw_with_color (job,
                                           x, y + h - corner_sizes[3][1],
                                           corner_sizes[3][0], corner_sizes[3][1],
                                           color);
      /* Edges... */;
      if (edge_sizes[0] > 0) /* Top */
        gsk_gl_render_job_draw_with_color (job,
                                           x + corner_sizes[0][0], y,
                                           w - corner_sizes[0][0] - corner_sizes[1][0], edge_sizes[0],
                                           color);
      if (edge_sizes[1] > 0) /* Right */
        gsk_gl_render_job_draw_with_color (job,
                                           x + w - edge_sizes[1], y + corner_sizes[1][1],
                                           edge_sizes[1], h - corner_sizes[1][1] - corner_sizes[2][1],
                                           color);
      if (edge_sizes[2] > 0) /* Bottom */
        gsk_gl_render_job_draw_with_color (job,
                                           x + corner_sizes[3][0], y + h - edge_sizes[2],
                                           w - corner_sizes[3][0] - corner_sizes[2][0], edge_sizes[2],
                                           color);
      if (edge_sizes[3] > 0) /* Left */
        gsk_gl_render_job_draw_with_color (job,
                                           x, y + corner_sizes[0][1],
                                           edge_sizes[3], h - corner_sizes[0][1] - corner_sizes[3][1],
                                           color);

      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_blurred_outset_shadow_node (GskGLRenderJob      *job,
                                                    const GskRenderNode *node)
{
  const GskRoundedRect *outline = gsk_outset_shadow_node_get_outline (node);
  float scale_x = job->scale_x;
  float scale_y = job->scale_y;
  float blur_radius = gsk_outset_shadow_node_get_blur_radius (node);
  float blur_extra = blur_radius * 2.0f; /* 2.0 = shader radius_multiplier */
  float half_blur_extra = blur_extra / 2.0f;
  int extra_blur_pixels_x = ceilf (half_blur_extra * scale_x);
  int extra_blur_pixels_y = ceilf (half_blur_extra * scale_y);
  float spread = gsk_outset_shadow_node_get_spread (node);
  float dx = gsk_outset_shadow_node_get_dx (node);
  float dy = gsk_outset_shadow_node_get_dy (node);
  GskRoundedRect scaled_outline;
  GskRoundedRect transformed_outline;
  GskGLRenderOffscreen offscreen = {0};
  int texture_width, texture_height;
  int blurred_texture_id;
  int cached_tid;
  gboolean do_slicing;
  guint16 color[4];
  float half_width = outline->bounds.size.width / 2;
  float half_height = outline->bounds.size.height / 2;

  rgba_to_half (gsk_outset_shadow_node_get_color (node), color);

  /* scaled_outline is the minimal outline we need to draw the given drop shadow,
   * enlarged by the spread and offset by the blur radius. */
  scaled_outline = *outline;

  if (outline->bounds.size.width < blur_extra ||
      outline->bounds.size.height < blur_extra ||
      outline->corner[0].width >= half_width ||
      outline->corner[1].width >= half_width ||
      outline->corner[2].width >= half_width ||
      outline->corner[3].width >= half_width ||
      outline->corner[0].height >= half_height ||
      outline->corner[1].height >= half_height ||
      outline->corner[2].height >= half_height ||
      outline->corner[3].height >= half_height)
    {
      do_slicing = FALSE;
      gsk_rounded_rect_shrink (&scaled_outline, -spread, -spread, -spread, -spread);
    }
  else
    {
      /* Shrink our outline to the minimum size that can still hold all the border radii */
      gsk_rounded_rect_shrink_to_minimum (&scaled_outline);
      /* Increase by the spread */
      gsk_rounded_rect_shrink (&scaled_outline, -spread, -spread, -spread, -spread);
      /* Grow bounds but don't grow corners */
      graphene_rect_inset (&scaled_outline.bounds, - blur_extra / 2.0, - blur_extra / 2.0);
      /* For the center part, we add a few pixels */
      scaled_outline.bounds.size.width += SHADOW_EXTRA_SIZE;
      scaled_outline.bounds.size.height += SHADOW_EXTRA_SIZE;

      do_slicing = TRUE;
    }

  texture_width = (int)ceilf ((scaled_outline.bounds.size.width + blur_extra) * scale_x);
  texture_height = (int)ceilf ((scaled_outline.bounds.size.height + blur_extra) * scale_y);

  scaled_outline.bounds.origin.x = extra_blur_pixels_x;
  scaled_outline.bounds.origin.y = extra_blur_pixels_y;
  scaled_outline.bounds.size.width = texture_width - (extra_blur_pixels_x * 2);
  scaled_outline.bounds.size.height = texture_height - (extra_blur_pixels_y * 2);

  for (guint i = 0; i < G_N_ELEMENTS (scaled_outline.corner); i++)
    {
      scaled_outline.corner[i].width *= scale_x;
      scaled_outline.corner[i].height *= scale_y;
    }

  cached_tid = gsk_gl_shadow_library_lookup (job->driver->shadows_library,
                                             &scaled_outline,
                                             blur_radius);

  if (cached_tid == 0)
    {
      GdkGLContext *context = job->command_queue->context;
      GskGLRenderTarget *render_target;
      graphene_matrix_t prev_projection;
      graphene_rect_t prev_viewport;
      guint prev_fbo;

      gsk_gl_driver_create_render_target (job->driver,
                                          texture_width, texture_height,
                                          get_target_format (job, node),
                                          &render_target);

      if (gdk_gl_context_has_feature (context, GDK_GL_FEATURE_DEBUG))
        {
          gdk_gl_context_label_object_printf (context,
                                              GL_TEXTURE,
                                              render_target->texture_id,
                                              "Outset Shadow Temp %d",
                                              render_target->texture_id);
          gdk_gl_context_label_object_printf (context,
                                              GL_FRAMEBUFFER,
                                              render_target->framebuffer_id,
                                              "Outset Shadow FB Temp %d",
                                              render_target->framebuffer_id);
        }

      /* Change state for offscreen */
      gsk_gl_render_job_set_projection_for_size (job, texture_width, texture_height, &prev_projection);
      gsk_gl_render_job_set_viewport_for_size (job, texture_width, texture_height, &prev_viewport);
      gsk_gl_render_job_set_modelview (job, NULL);
      gsk_gl_render_job_push_clip (job, &scaled_outline);

      /* Bind render target and clear it */
      prev_fbo = gsk_gl_command_queue_bind_framebuffer (job->command_queue, render_target->framebuffer_id);
      gsk_gl_command_queue_clear (job->command_queue, 0, &job->viewport);

      /* Draw the outline using color program */
      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, color)))
        {
          gsk_gl_render_job_draw_with_color (job, 0, 0, texture_width, texture_height,
                                             (guint16[]){ FP16_ONE, FP16_ONE, FP16_ONE, FP16_ONE });
          gsk_gl_render_job_end_draw (job);
        }

      /* Reset state from offscreen */
      gsk_gl_render_job_pop_clip (job);
      gsk_gl_render_job_pop_modelview (job);
      gsk_gl_render_job_set_viewport (job, &prev_viewport, NULL);
      gsk_gl_render_job_set_projection (job, &prev_projection);

      /* Now blur the outline */
      init_full_texture_region (&offscreen);
      offscreen.texture_id = gsk_gl_driver_release_render_target (job->driver, render_target, FALSE);
      blurred_texture_id = blur_offscreen (job,
                                           &offscreen,
                                           texture_width,
                                           texture_height,
                                           blur_radius * fabsf (scale_x),
                                           blur_radius * fabsf (scale_y));

      gsk_gl_shadow_library_insert (job->driver->shadows_library,
                                    &scaled_outline,
                                    blur_radius,
                                    blurred_texture_id);

      gsk_gl_command_queue_bind_framebuffer (job->command_queue, prev_fbo);
    }
  else
    {
      blurred_texture_id = cached_tid;
    }

  gsk_gl_render_job_translate_rounded_rect (job, outline, &transformed_outline);

  if (!do_slicing)
    {
      float min_x = floorf (outline->bounds.origin.x - spread - half_blur_extra + dx);
      float min_y = floorf (outline->bounds.origin.y - spread - half_blur_extra + dy);

      offscreen.was_offscreen = TRUE;
      offscreen.texture_id = blurred_texture_id;
      init_full_texture_region (&offscreen);

      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, outset_shadow)))
        {
          gsk_gl_program_set_uniform_texture (job->current_program,
                                              UNIFORM_SHARED_SOURCE, 0,
                                              GL_TEXTURE_2D,
                                              GL_TEXTURE0,
                                              blurred_texture_id);
          job->source_is_glyph_atlas = FALSE;
          gsk_gl_program_set_uniform_rounded_rect (job->current_program,
                                                   UNIFORM_OUTSET_SHADOW_OUTLINE_RECT, 0,
                                                   &transformed_outline);
          gsk_gl_render_job_draw_offscreen_with_color (job,
                                                       &GRAPHENE_RECT_INIT (min_x,
                                                                            min_y,
                                                                            texture_width / scale_x,
                                                                            texture_height / scale_y),
                                                       &offscreen,
                                                       color);
          gsk_gl_render_job_end_draw (job);
        }

      return;
    }

  /* slicing */

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, outset_shadow)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          blurred_texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_program_set_uniform_rounded_rect (job->current_program,
                                               UNIFORM_OUTSET_SHADOW_OUTLINE_RECT, 0,
                                               &transformed_outline);

      {
        float min_x = floorf (outline->bounds.origin.x - spread - half_blur_extra + dx);
        float min_y = floorf (outline->bounds.origin.y - spread - half_blur_extra + dy);
        float max_x = ceilf (outline->bounds.origin.x + outline->bounds.size.width +
                             half_blur_extra + dx + spread);
        float max_y = ceilf (outline->bounds.origin.y + outline->bounds.size.height +
                             half_blur_extra + dy + spread);
        const GskGLTextureNineSlice *slices;
        float left_width, center_width, right_width;
        float top_height, center_height, bottom_height;
        GskGLTexture *texture;

        texture = gsk_gl_driver_get_texture_by_id (job->driver, blurred_texture_id);
        slices = gsk_gl_texture_get_nine_slice (texture, &scaled_outline, extra_blur_pixels_x, extra_blur_pixels_y);

        offscreen.was_offscreen = TRUE;

        /* Our texture coordinates MUST be scaled, while the actual vertex coords
         * MUST NOT be scaled.
         */

        left_width = slices[NINE_SLICE_TOP_LEFT].rect.width / scale_x;
        right_width = slices[NINE_SLICE_TOP_RIGHT].rect.width / scale_x;
        center_width = (max_x - min_x) - (left_width + right_width);

        top_height = slices[NINE_SLICE_TOP_LEFT].rect.height / scale_y;
        bottom_height = slices[NINE_SLICE_BOTTOM_LEFT].rect.height / scale_y;
        center_height = (max_y - min_y) - (top_height + bottom_height);

        /* Top left */
        if (nine_slice_is_visible (&slices[NINE_SLICE_TOP_LEFT]))
          {
            memcpy (&offscreen.area, &slices[NINE_SLICE_TOP_LEFT].area, sizeof offscreen.area);
            gsk_gl_render_job_draw_offscreen_with_color (job,
                                                         &GRAPHENE_RECT_INIT (min_x,
                                                                              min_y,
                                                                              left_width,
                                                                              top_height),
                                                         &offscreen,
                                                         color);
          }

        /* Top center */
        if (nine_slice_is_visible (&slices[NINE_SLICE_TOP_CENTER]))
        {
          memcpy (&offscreen.area, &slices[NINE_SLICE_TOP_CENTER].area, sizeof offscreen.area);
          gsk_gl_render_job_draw_offscreen_with_color (job,
                                                       &GRAPHENE_RECT_INIT (min_x + left_width,
                                                                            min_y,
                                                                            center_width,
                                                                            top_height),
                                                       &offscreen,
                                                       color);
        }

        /* Top right */
        if (nine_slice_is_visible (&slices[NINE_SLICE_TOP_RIGHT]))
        {
          memcpy (&offscreen.area, &slices[NINE_SLICE_TOP_RIGHT].area, sizeof offscreen.area);
          gsk_gl_render_job_draw_offscreen_with_color (job,
                                                       &GRAPHENE_RECT_INIT (max_x - right_width,
                                                                            min_y,
                                                                            right_width,
                                                                            top_height),
                                                       &offscreen,
                                                       color);
        }

        /* Bottom right */
        if (nine_slice_is_visible (&slices[NINE_SLICE_BOTTOM_RIGHT]))
        {
          memcpy (&offscreen.area, &slices[NINE_SLICE_BOTTOM_RIGHT].area, sizeof offscreen.area);
          gsk_gl_render_job_draw_offscreen_with_color (job,
                                                       &GRAPHENE_RECT_INIT (max_x - right_width,
                                                                            max_y - bottom_height,
                                                                            right_width,
                                                                            bottom_height),
                                                       &offscreen,
                                                       color);
        }

        /* Bottom left */
        if (nine_slice_is_visible (&slices[NINE_SLICE_BOTTOM_LEFT]))
          {
            memcpy (&offscreen.area, &slices[NINE_SLICE_BOTTOM_LEFT].area, sizeof offscreen.area);
            gsk_gl_render_job_draw_offscreen_with_color (job,
                                                         &GRAPHENE_RECT_INIT (min_x,
                                                                              max_y - bottom_height,
                                                                              left_width,
                                                                              bottom_height),
                                                         &offscreen,
                                                         color);
          }

        /* Left side */
        if (nine_slice_is_visible (&slices[NINE_SLICE_LEFT_CENTER]))
          {
            memcpy (&offscreen.area, &slices[NINE_SLICE_LEFT_CENTER].area, sizeof offscreen.area);
            gsk_gl_render_job_draw_offscreen_with_color (job,
                                                         &GRAPHENE_RECT_INIT (min_x,
                                                                              min_y + top_height,
                                                                              left_width,
                                                                              center_height),
                                                         &offscreen,
                                                         color);
          }

        /* Right side */
        if (nine_slice_is_visible (&slices[NINE_SLICE_RIGHT_CENTER]))
          {
            memcpy (&offscreen.area, &slices[NINE_SLICE_RIGHT_CENTER].area, sizeof offscreen.area);
            gsk_gl_render_job_draw_offscreen_with_color (job,
                                                         &GRAPHENE_RECT_INIT (max_x - right_width,
                                                                              min_y + top_height,
                                                                              right_width,
                                                                              center_height),
                                                         &offscreen,
                                                         color);
          }

        /* Bottom side */
        if (nine_slice_is_visible (&slices[NINE_SLICE_BOTTOM_CENTER]))
          {
            memcpy (&offscreen.area, &slices[NINE_SLICE_BOTTOM_CENTER].area, sizeof offscreen.area);
            gsk_gl_render_job_draw_offscreen_with_color (job,
                                                         &GRAPHENE_RECT_INIT (min_x + left_width,
                                                                              max_y - bottom_height,
                                                                              center_width,
                                                                              bottom_height),
                                                         &offscreen,
                                                         color);
          }

        /* Middle */
        if (nine_slice_is_visible (&slices[NINE_SLICE_CENTER]))
          {
            if (!gsk_rounded_rect_contains_rect (outline, &GRAPHENE_RECT_INIT (min_x + left_width,
                                                                               min_y + top_height,
                                                                               center_width,
                                                                               center_height)))
              {
                memcpy (&offscreen.area, &slices[NINE_SLICE_CENTER].area, sizeof offscreen.area);
                gsk_gl_render_job_draw_offscreen_with_color (job,
                                                             &GRAPHENE_RECT_INIT (min_x + left_width,
                                                                                  min_y + top_height,
                                                                                  center_width,
                                                                                  center_height),
                                                             &offscreen,
                                                             color);
              }
          }
      }

      gsk_gl_render_job_end_draw (job);
    }
}

static inline gboolean G_GNUC_PURE
equal_texture_nodes (const GskRenderNode *node1,
                     const GskRenderNode *node2)
{
  if (GSK_RENDER_NODE_TYPE (node1) != GSK_TEXTURE_NODE ||
      GSK_RENDER_NODE_TYPE (node2) != GSK_TEXTURE_NODE)
    return FALSE;

  if (gsk_texture_node_get_texture (node1) !=
      gsk_texture_node_get_texture (node2))
    return FALSE;

  return gsk_rect_equal (&node1->bounds, &node2->bounds);
}

static inline void
gsk_gl_render_job_visit_cross_fade_node (GskGLRenderJob      *job,
                                         const GskRenderNode *node)
{
  const GskRenderNode *start_node = gsk_cross_fade_node_get_start_child (node);
  const GskRenderNode *end_node = gsk_cross_fade_node_get_end_child (node);
  float progress = gsk_cross_fade_node_get_progress (node);
  GskGLRenderOffscreen offscreen_start = {0};
  GskGLRenderOffscreen offscreen_end = {0};

  g_assert (progress > 0.0);
  g_assert (progress < 1.0);

  offscreen_start.force_offscreen = TRUE;
  offscreen_start.reset_clip = TRUE;
  offscreen_start.bounds = &node->bounds;

  offscreen_end.force_offscreen = TRUE;
  offscreen_end.reset_clip = TRUE;
  offscreen_end.bounds = &node->bounds;

  gsk_gl_render_job_set_modelview (job, gsk_transform_scale (NULL, fabsf (job->scale_x), fabsf (job->scale_y)));

  if (!gsk_gl_render_job_visit_node_with_offscreen (job, start_node, &offscreen_start))
    {
      gsk_gl_render_job_pop_modelview (job);

      gsk_gl_render_job_visit_node (job, end_node);
      return;
    }

  g_assert (offscreen_start.texture_id);

  if (!gsk_gl_render_job_visit_node_with_offscreen (job, end_node, &offscreen_end))
    {
      float prev_alpha;

      gsk_gl_render_job_pop_modelview (job);

      prev_alpha = gsk_gl_render_job_set_alpha (job, job->alpha * progress);
      gsk_gl_render_job_visit_node (job, start_node);
      gsk_gl_render_job_set_alpha (job, prev_alpha);
      return;
    }

  gsk_gl_render_job_pop_modelview (job);

  g_assert (offscreen_end.texture_id);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, cross_fade)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          offscreen_start.texture_id);
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_CROSS_FADE_SOURCE2, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE1,
                                          offscreen_end.texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_program_set_uniform1f (job->current_program,
                                    UNIFORM_CROSS_FADE_PROGRESS, 0,
                                    progress);
      gsk_gl_render_job_draw_offscreen (job, &node->bounds, &offscreen_end);
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_opacity_node (GskGLRenderJob      *job,
                                      const GskRenderNode *node)
{
  const GskRenderNode *child = gsk_opacity_node_get_child (node);
  float opacity = gsk_opacity_node_get_opacity (node);
  float new_alpha = job->alpha * opacity;

  if (!ALPHA_IS_CLEAR (new_alpha))
    {
      float prev_alpha = gsk_gl_render_job_set_alpha (job, new_alpha);

      if (!gsk_render_node_use_offscreen_for_opacity (child))
        {
          gsk_gl_render_job_visit_node (job, child);
          gsk_gl_render_job_set_alpha (job, prev_alpha);
        }
      else
        {
          GskGLRenderOffscreen offscreen = {0};

          offscreen.bounds = &child->bounds;
          offscreen.force_offscreen = TRUE;
          offscreen.reset_clip = TRUE;

          /* Note: offscreen rendering resets alpha to 1.0 */
          if (!gsk_gl_render_job_visit_node_with_offscreen (job, child, &offscreen))
            return;

          g_assert (offscreen.texture_id);

          if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
            {
              gsk_gl_program_set_uniform_texture (job->current_program,
                                                  UNIFORM_SHARED_SOURCE, 0,
                                                  GL_TEXTURE_2D,
                                                  GL_TEXTURE0,
                                                  offscreen.texture_id);
              job->source_is_glyph_atlas = FALSE;
              gsk_gl_render_job_draw_offscreen (job, &node->bounds, &offscreen);
              gsk_gl_render_job_end_draw (job);
            }
        }

      gsk_gl_render_job_set_alpha (job, prev_alpha);
    }
}

static inline int
compute_phase_and_pos (float value, float *pos)
{
  float v;

  *pos = floorf (value);

  v = value - *pos;

  if (v < 0.125)
    return 0;
  else if (v < 0.375)
    return 1;
  else if (v < 0.625)
    return 2;
  else if (v < 0.875)
    return 3;
  else
    {
      *pos += 1;
      return 0;
    }
}

static inline void
gsk_gl_render_job_visit_text_node (GskGLRenderJob      *job,
                                   const GskRenderNode *node,
                                   const GdkRGBA       *color,
                                   gboolean             force_color)
{
  const PangoFont *font = gsk_text_node_get_font (node);
  const PangoGlyphInfo *glyphs = gsk_text_node_get_glyphs (node, NULL);
  const graphene_point_t *offset = gsk_text_node_get_offset (node);
  float text_scale = MAX (fabsf (job->scale_x), fabsf (job->scale_y)); /* TODO: Fix for uneven scales? */
  guint num_glyphs = gsk_text_node_get_num_glyphs (node);
  float x = offset->x + job->offset_x;
  float y = offset->y + job->offset_y;
  GskGLGlyphLibrary *library = job->driver->glyphs_library;
  GskGLCommandBatch *batch;
  int x_position = 0;
  GskGLGlyphKey lookup;
  guint last_texture = 0;
  GskGLDrawVertex *vertices;
  guint used = 0;
  guint16 nc[4] = { FP16_MINUS_ONE, FP16_MINUS_ONE, FP16_MINUS_ONE, FP16_MINUS_ONE };
  guint16 cc[4];
  const guint16 *c;
  const PangoGlyphInfo *gi;
  guint i;
  int yshift;
  float ypos;

  if (num_glyphs == 0)
    return;

  if ((force_color || !gsk_text_node_has_color_glyphs (node)) &&
      RGBA_IS_CLEAR (color))
    return;

  rgba_to_half (color, cc);

  lookup.font = (PangoFont *)font;
  lookup.scale = (guint) (text_scale * 1024);

  yshift = compute_phase_and_pos (y, &ypos);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, coloring)))
    {
      batch = gsk_gl_command_queue_get_batch (job->command_queue);
      vertices = gsk_gl_command_queue_add_n_vertices (job->command_queue, num_glyphs);

      /* We use one quad per character */
      for (i = 0, gi = glyphs; i < num_glyphs; i++, gi++)
        {
          const GskGLGlyphValue *glyph;
          float glyph_x, glyph_y, glyph_x2, glyph_y2;
          float tx, ty, tx2, ty2;
          float cx;
          float cy;
          guint texture_id;

          lookup.glyph = gi->glyph;

          /* If the glyph has color, we don't need to recolor anything.
           * We tell the shader by setting the color to vec4(-1).
           */
          if (!force_color && gi->attr.is_color)
            c = nc;
          else
            c = cc;

          cx = (float)(x_position + gi->geometry.x_offset) / PANGO_SCALE;
          lookup.xshift = compute_phase_and_pos (x + cx, &cx);

          if G_UNLIKELY (gi->geometry.y_offset != 0)
            {
              cy = (float)(gi->geometry.y_offset) / PANGO_SCALE;
              lookup.yshift = compute_phase_and_pos (y + cy, &cy);
            }
          else
            {
              lookup.yshift = yshift;
              cy = ypos;
            }

          x_position += gi->geometry.width;

          texture_id = gsk_gl_glyph_library_lookup_or_add (library, &lookup, &glyph);
          if G_UNLIKELY (texture_id == 0)
            continue;

          if G_UNLIKELY (last_texture != texture_id || batch->draw.vbo_count + GSK_GL_N_VERTICES > 0xffff)
            {
              if G_LIKELY (last_texture != 0)
                {
                  guint vbo_offset = batch->draw.vbo_offset + batch->draw.vbo_count;

                  /* Since we have batched added our VBO vertices to avoid repeated
                   * calls to the buffer, we need to manually tweak the vbo offset
                   * of the new batch as otherwise it will point at the end of our
                   * vbo array.
                   */
                  gsk_gl_render_job_split_draw (job);
                  batch = gsk_gl_command_queue_get_batch (job->command_queue);
                  batch->draw.vbo_offset = vbo_offset;
                }

              gsk_gl_program_set_uniform_texture (job->current_program,
                                                  UNIFORM_SHARED_SOURCE, 0,
                                                  GL_TEXTURE_2D,
                                                  GL_TEXTURE0,
                                                  texture_id);
              job->source_is_glyph_atlas = TRUE;
              last_texture = texture_id;
            }

          tx = glyph->entry.area.x;
          ty = glyph->entry.area.y;
          tx2 = glyph->entry.area.x2;
          ty2 = glyph->entry.area.y2;

          glyph_x = cx + glyph->ink_rect.x;
          glyph_y = cy + glyph->ink_rect.y;
          glyph_x2 = glyph_x + glyph->ink_rect.width;
          glyph_y2 = glyph_y + glyph->ink_rect.height;

          *(vertices++) = (GskGLDrawVertex) { .position = { glyph_x,  glyph_y  }, .uv = { tx,  ty  }, .color = { c[0], c[1], c[2], c[3] } };
          *(vertices++) = (GskGLDrawVertex) { .position = { glyph_x,  glyph_y2 }, .uv = { tx,  ty2 }, .color = { c[0], c[1], c[2], c[3] } };
          *(vertices++) = (GskGLDrawVertex) { .position = { glyph_x2, glyph_y  }, .uv = { tx2, ty  }, .color = { c[0], c[1], c[2], c[3] } };

          *(vertices++) = (GskGLDrawVertex) { .position = { glyph_x2, glyph_y2 }, .uv = { tx2, ty2 }, .color = { c[0], c[1], c[2], c[3] } };
          *(vertices++) = (GskGLDrawVertex) { .position = { glyph_x,  glyph_y2 }, .uv = { tx,  ty2 }, .color = { c[0], c[1], c[2], c[3] } };
          *(vertices++) = (GskGLDrawVertex) { .position = { glyph_x2, glyph_y  }, .uv = { tx2, ty  }, .color = { c[0], c[1], c[2], c[3] } };

          batch->draw.vbo_count += GSK_GL_N_VERTICES;
          used++;
        }

      if (used != num_glyphs)
        gsk_gl_command_queue_retract_n_vertices (job->command_queue, num_glyphs - used);

      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_shadow_node (GskGLRenderJob      *job,
                                     const GskRenderNode *node)
{
  const gsize n_shadows = gsk_shadow_node_get_n_shadows (node);
  const GskRenderNode *original_child = gsk_shadow_node_get_child (node);
  const GskRenderNode *shadow_child = original_child;

  /* Shadow nodes recolor every pixel of the source texture, but leave the alpha in tact.
   * If the child is a color matrix node that doesn't touch the alpha, we can throw that away. */
  if (GSK_RENDER_NODE_TYPE (shadow_child) == GSK_COLOR_MATRIX_NODE &&
      !color_matrix_modifies_alpha (shadow_child))
    shadow_child = gsk_color_matrix_node_get_child (shadow_child);

  for (guint i = 0; i < n_shadows; i++)
    {
      const GskShadow *shadow = gsk_shadow_node_get_shadow (node, i);
      const float dx = shadow->dx;
      const float dy = shadow->dy;
      GskGLRenderOffscreen offscreen = {0};
      graphene_rect_t bounds;
      guint16 color[4];

      if (RGBA_IS_CLEAR (&shadow->color))
        continue;

      if (node_is_invisible (shadow_child))
        continue;

      if (shadow->radius == 0 &&
          GSK_RENDER_NODE_TYPE (shadow_child) == GSK_TEXT_NODE)
        {
          if (dx != 0 || dy != 0)
            {
              gsk_gl_render_job_offset (job, dx, dy);
              gsk_gl_render_job_visit_text_node (job, shadow_child, &shadow->color, TRUE);
              gsk_gl_render_job_offset (job, -dx, -dy);
            }
          continue;
        }

      if (shadow->radius > 0)
        {
          float min_x;
          float min_y;
          float max_x;
          float max_y;

          offscreen.do_not_cache = TRUE;

          blur_node (job,
                     &offscreen,
                     shadow_child,
                     shadow->radius,
                     &min_x, &max_x,
                     &min_y, &max_y);

          bounds.origin.x = min_x - job->offset_x;
          bounds.origin.y = min_y - job->offset_y;
          bounds.size.width = max_x - min_x;
          bounds.size.height = max_y - min_y;

          offscreen.was_offscreen = TRUE;
        }
      else
        {
          offscreen.bounds = &shadow_child->bounds;
          offscreen.reset_clip = TRUE;
          offscreen.do_not_cache = TRUE;

          if (!gsk_gl_render_job_visit_node_with_offscreen (job, shadow_child, &offscreen))
            g_assert_not_reached ();

          bounds = shadow_child->bounds;
        }

      gsk_gl_render_job_offset (job, dx, dy);
      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, coloring)))
        {
          gsk_gl_program_set_uniform_texture (job->current_program,
                                              UNIFORM_SHARED_SOURCE, 0,
                                              GL_TEXTURE_2D,
                                              GL_TEXTURE0,
                                              offscreen.texture_id);
          job->source_is_glyph_atlas = FALSE;
          rgba_to_half (&shadow->color, color);
          gsk_gl_render_job_draw_offscreen_with_color (job, &bounds, &offscreen, color);
          gsk_gl_render_job_end_draw (job);
        }
      gsk_gl_render_job_offset (job, -dx, -dy);
    }

  /* Now draw the child normally */
  gsk_gl_render_job_visit_node (job, original_child);
}

static inline void
gsk_gl_render_job_visit_blur_node (GskGLRenderJob      *job,
                                   const GskRenderNode *node)
{
  const GskRenderNode *child = gsk_blur_node_get_child (node);
  float blur_radius = gsk_blur_node_get_radius (node);
  GskGLRenderOffscreen offscreen = {0};
  GskTextureKey key;
  gboolean cache_texture;
  float min_x;
  float max_x;
  float min_y;
  float max_y;

  g_assert (blur_radius > 0);

  if (node_is_invisible (child))
    return;

  key.pointer = node;
  key.pointer_is_child = FALSE;
  key.scale_x = job->scale_x;
  key.scale_y = job->scale_y;

  offscreen.texture_id = gsk_gl_driver_lookup_texture (job->driver, &key, NULL);
  cache_texture = offscreen.texture_id == 0;

  blur_node (job,
             &offscreen,
             child,
             blur_radius,
             &min_x, &max_x, &min_y, &max_y);

  g_assert (offscreen.texture_id != 0);

  if (cache_texture)
    gsk_gl_driver_cache_texture (job->driver, &key, offscreen.texture_id);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          offscreen.texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_render_job_draw_coords (job,
                                     min_x, min_y, max_x, max_y,
                                     0, 1, 1, 0,
                                     (guint16[]) { FP16_ZERO, FP16_ZERO, FP16_ZERO, FP16_ZERO } );
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_blend_node (GskGLRenderJob      *job,
                                    const GskRenderNode *node)
{
  const GskRenderNode *top_child = gsk_blend_node_get_top_child (node);
  const GskRenderNode *bottom_child = gsk_blend_node_get_bottom_child (node);
  GskGLRenderOffscreen top_offscreen = {0};
  GskGLRenderOffscreen bottom_offscreen = {0};

  top_offscreen.bounds = &node->bounds;
  top_offscreen.force_offscreen = TRUE;
  top_offscreen.reset_clip = TRUE;

  bottom_offscreen.bounds = &node->bounds;
  bottom_offscreen.force_offscreen = TRUE;
  bottom_offscreen.reset_clip = TRUE;

  gsk_gl_render_job_set_modelview (job, gsk_transform_scale (NULL, fabsf (job->scale_x), fabsf (job->scale_y)));

  /* TODO: We create 2 textures here as big as the blend node, but both the
   * start and the end node might be a lot smaller than that. */
  if (!gsk_gl_render_job_visit_node_with_offscreen (job, bottom_child, &bottom_offscreen))
    {
      gsk_gl_render_job_pop_modelview (job);

      gsk_gl_render_job_visit_node (job, top_child);
      return;
    }

  g_assert (bottom_offscreen.was_offscreen);

  if (!gsk_gl_render_job_visit_node_with_offscreen (job, top_child, &top_offscreen))
    {
      gsk_gl_render_job_pop_modelview (job);

      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
        {
          gsk_gl_program_set_uniform_texture (job->current_program,
                                              UNIFORM_SHARED_SOURCE, 0,
                                              GL_TEXTURE_2D,
                                              GL_TEXTURE0,
                                              bottom_offscreen.texture_id);
          job->source_is_glyph_atlas = FALSE;
          gsk_gl_render_job_draw_offscreen (job, &node->bounds, &bottom_offscreen);
          gsk_gl_render_job_end_draw (job);
        }
      return;
    }

  g_assert (top_offscreen.was_offscreen);

  gsk_gl_render_job_pop_modelview (job);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blend)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          bottom_offscreen.texture_id);
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_BLEND_SOURCE2, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE1,
                                          top_offscreen.texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_program_set_uniform1i (job->current_program,
                                    UNIFORM_BLEND_MODE, 0,
                                    gsk_blend_node_get_blend_mode (node));
      gsk_gl_render_job_draw_offscreen_rect (job, &node->bounds);
      gsk_gl_render_job_end_draw (job);
    }
}

static gboolean
gsk_gl_render_job_texture_mask_for_color (GskGLRenderJob        *job,
                                          const GskRenderNode   *mask,
                                          const GskRenderNode   *color,
                                          const graphene_rect_t *bounds)
{
  int max_texture_size = job->command_queue->max_texture_size;
  GdkTexture *texture = gsk_texture_node_get_texture (mask);
  const GdkRGBA *rgba;

  rgba = gsk_color_node_get_color (color);
  if (RGBA_IS_CLEAR (rgba))
    return TRUE;

  if G_LIKELY (texture->width <= max_texture_size &&
               texture->height <= max_texture_size &&
               gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, coloring)))
    {
      GskGLRenderOffscreen offscreen = {0};
      float scale_x = mask->bounds.size.width / texture->width;
      float scale_y = mask->bounds.size.height / texture->height;
      gboolean use_mipmap;
      guint16 cc[4];

      use_mipmap = (scale_x * fabsf (job->scale_x)) < 0.5 ||
                   (scale_y * fabsf (job->scale_y)) < 0.5;

      rgba_to_half (rgba, cc);
      gsk_gl_render_job_upload_texture (job, texture, use_mipmap, &offscreen);
      gsk_gl_program_set_uniform_texture_with_sync (job->current_program,
                                                    UNIFORM_SHARED_SOURCE, 0,
                                                    GL_TEXTURE_2D,
                                                    GL_TEXTURE0,
                                                    offscreen.texture_id,
                                                    offscreen.has_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR,
                                                    GL_LINEAR,
                                                    offscreen.sync);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_render_job_draw_offscreen_with_color (job, bounds, &offscreen, cc);
      gsk_gl_render_job_end_draw (job);

      return TRUE;
    }

  return FALSE;
}

static inline void
gsk_gl_render_job_visit_mask_node (GskGLRenderJob      *job,
                                   const GskRenderNode *node)
{
  const GskRenderNode *source = gsk_mask_node_get_source (node);
  const GskRenderNode *mask = gsk_mask_node_get_mask (node);
  GskGLRenderOffscreen source_offscreen = {0};
  GskGLRenderOffscreen mask_offscreen = {0};

  /* If the mask is a texture and the source is a color node
   * then we can take a shortcut and avoid offscreens.
   */
  if (GSK_RENDER_NODE_TYPE (mask) == GSK_TEXTURE_NODE &&
      GSK_RENDER_NODE_TYPE (source) == GSK_COLOR_NODE &&
      gsk_mask_node_get_mask_mode (node) == GSK_MASK_MODE_ALPHA)
    {
      if (gsk_gl_render_job_texture_mask_for_color (job, mask, source, &node->bounds))
        return;
    }

  source_offscreen.bounds = &node->bounds;
  source_offscreen.force_offscreen = TRUE;
  source_offscreen.reset_clip = TRUE;

  mask_offscreen.bounds = &node->bounds;
  mask_offscreen.force_offscreen = TRUE;
  mask_offscreen.reset_clip = TRUE;
  mask_offscreen.do_not_cache = TRUE;

  gsk_gl_render_job_set_modelview (job, gsk_transform_scale (NULL, fabsf (job->scale_x), fabsf (job->scale_y)));

  /* TODO: We create 2 textures here as big as the mask node, but both
   * nodes might be a lot smaller than that.
   */
  if (!gsk_gl_render_job_visit_node_with_offscreen (job, source, &source_offscreen))
    {
      gsk_gl_render_job_pop_modelview (job);
      gsk_gl_render_job_visit_node (job, source);
      return;
    }

  g_assert (source_offscreen.was_offscreen);

  if (!gsk_gl_render_job_visit_node_with_offscreen (job, mask, &mask_offscreen))
    {
      gsk_gl_render_job_pop_modelview (job);
      return;
    }

  g_assert (mask_offscreen.was_offscreen);

  gsk_gl_render_job_pop_modelview (job);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, mask)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          source_offscreen.texture_id);
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_MASK_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE1,
                                          mask_offscreen.texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_program_set_uniform1i (job->current_program,
                                    UNIFORM_MASK_MODE, 0,
                                    gsk_mask_node_get_mask_mode (node));
      gsk_gl_render_job_draw_offscreen_rect (job, &node->bounds);
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_color_matrix_node (GskGLRenderJob      *job,
                                           const GskRenderNode *node)
{
  const GskRenderNode *child = gsk_color_matrix_node_get_child (node);
  GskGLRenderOffscreen offscreen = {0};
  float offset[4];

  if (node_is_invisible (child))
    return;

  offscreen.bounds = &node->bounds;
  offscreen.reset_clip = TRUE;

  if (!gsk_gl_render_job_visit_node_with_offscreen (job, child, &offscreen))
    g_assert_not_reached ();

  g_assert (offscreen.texture_id > 0);

  graphene_vec4_to_float (gsk_color_matrix_node_get_color_offset (node), offset);

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, color_matrix)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          offscreen.texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_program_set_uniform_matrix (job->current_program,
                                         UNIFORM_COLOR_MATRIX_COLOR_MATRIX, 0,
                                         gsk_color_matrix_node_get_color_matrix (node));
      gsk_gl_program_set_uniform4fv (job->current_program,
                                     UNIFORM_COLOR_MATRIX_COLOR_OFFSET, 0,
                                     1,
                                     offset);
      gsk_gl_render_job_draw_offscreen (job, &node->bounds, &offscreen);
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_gl_shader_node_fallback (GskGLRenderJob      *job,
                                                 const GskRenderNode *node)
{
  guint16 pink[4] = { 15360, 13975, 14758, 15360 }; /* 255 105 180 */

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, color)))
    {
      gsk_gl_render_job_draw_rect_with_color (job, &node->bounds, pink);
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_gl_shader_node (GskGLRenderJob      *job,
                                        const GskRenderNode *node)
{
  GError *error = NULL;
  GskGLShader *shader;
  GskGLProgram *program;
  int n_children;

  shader = gsk_gl_shader_node_get_shader (node);
  program = gsk_gl_driver_lookup_shader (job->driver, shader, &error);
  n_children = gsk_gl_shader_node_get_n_children (node);

  if G_UNLIKELY (program == NULL)
    {
      if (g_object_get_data (G_OBJECT (shader), "gsk-did-warn") == NULL)
        {
          g_object_set_data (G_OBJECT (shader), "gsk-did-warn", GUINT_TO_POINTER (1));
          g_warning ("Failed to compile gl shader: %s", error->message);
        }
      gsk_gl_render_job_visit_gl_shader_node_fallback (job, node);
      g_clear_error (&error);
    }
  else
    {
      GskGLRenderOffscreen offscreens[4] = {{0}};
      const GskGLUniform *uniforms;
      const guint8 *base;
      GBytes *args;
      int n_uniforms;

      g_assert (n_children < G_N_ELEMENTS (offscreens));

      for (guint i = 0; i < n_children; i++)
        {
          const GskRenderNode *child = gsk_gl_shader_node_get_child (node, i);

          offscreens[i].bounds = &node->bounds;
          offscreens[i].force_offscreen = TRUE;
          offscreens[i].reset_clip = TRUE;

          if (!gsk_gl_render_job_visit_node_with_offscreen (job, child, &offscreens[i]))
            return;
        }

      args = gsk_gl_shader_node_get_args (node);
      base = g_bytes_get_data (args, NULL);
      uniforms = gsk_gl_shader_get_uniforms (shader, &n_uniforms);

      if (gsk_gl_render_job_begin_draw (job, program))
        {
          for (guint i = 0; i < n_children; i++)
            gsk_gl_program_set_uniform_texture (program,
                                                UNIFORM_CUSTOM_TEXTURE1 + i, 0,
                                                GL_TEXTURE_2D,
                                                GL_TEXTURE0 + i,
                                                offscreens[i].texture_id);
          job->source_is_glyph_atlas = FALSE;
          gsk_gl_program_set_uniform2f (program,
                                        UNIFORM_CUSTOM_SIZE, 0,
                                        node->bounds.size.width,
                                        node->bounds.size.height);
          for (guint i = 0; i < n_uniforms; i++)
            {
              const GskGLUniform *u = &uniforms[i];
              const guint8 *data = base + u->offset;

              switch (u->type)
                {
                default:
                case GSK_GL_UNIFORM_TYPE_NONE:
                  break;
                case GSK_GL_UNIFORM_TYPE_FLOAT:
                  gsk_gl_uniform_state_set1fv (job->command_queue->uniforms,
                                               program->program_info,
                                               UNIFORM_CUSTOM_ARG0 + i,
                                               0, 1, (const float *)data);
                  break;
                case GSK_GL_UNIFORM_TYPE_INT:
                  gsk_gl_uniform_state_set1i (job->command_queue->uniforms,
                                              program->program_info,
                                              UNIFORM_CUSTOM_ARG0 + i,
                                              0, *(const gint32 *)data);
                  break;
                case GSK_GL_UNIFORM_TYPE_UINT:
                case GSK_GL_UNIFORM_TYPE_BOOL:
                  gsk_gl_uniform_state_set1ui (job->command_queue->uniforms,
                                               program->program_info,
                                               UNIFORM_CUSTOM_ARG0 + i,
                                               0, *(const guint32 *)data);
                  break;
                case GSK_GL_UNIFORM_TYPE_VEC2:
                  gsk_gl_uniform_state_set2fv (job->command_queue->uniforms,
                                               program->program_info,
                                               UNIFORM_CUSTOM_ARG0 + i,
                                               0, 1, (const float *)data);
                  break;
                case GSK_GL_UNIFORM_TYPE_VEC3:
                  gsk_gl_uniform_state_set3fv (job->command_queue->uniforms,
                                               program->program_info,
                                               UNIFORM_CUSTOM_ARG0 + i,
                                               0, 1, (const float *)data);
                  break;
                case GSK_GL_UNIFORM_TYPE_VEC4:
                  gsk_gl_uniform_state_set4fv (job->command_queue->uniforms,
                                               program->program_info,
                                               UNIFORM_CUSTOM_ARG0 + i,
                                               0, 1, (const float *)data);
                  break;
                }
            }
          gsk_gl_render_job_draw_offscreen_rect (job, &node->bounds);
          gsk_gl_render_job_end_draw (job);
        }
    }
}

static void
gsk_gl_render_job_upload_texture (GskGLRenderJob       *job,
                                  GdkTexture           *texture,
                                  gboolean              ensure_mipmap,
                                  GskGLRenderOffscreen *offscreen)
{
  /* Don't put GL or dmabuf textures into icon caches, they are already on the GPU side */
  if (!ensure_mipmap &&
      gsk_gl_texture_library_can_cache ((GskGLTextureLibrary *)job->driver->icons_library,
                                        texture->width,
                                        texture->height) &&
      !(GDK_IS_GL_TEXTURE (texture) || GDK_IS_DMABUF_TEXTURE (texture)))
    {
      const GskGLIconData *icon_data;

      gsk_gl_icon_library_lookup_or_add (job->driver->icons_library, texture, &icon_data);
      offscreen->texture_id = GSK_GL_TEXTURE_ATLAS_ENTRY_TEXTURE (icon_data);
      memcpy (&offscreen->area, &icon_data->entry.area, sizeof offscreen->area);
      offscreen->has_mipmap = FALSE;
    }
  else
    {
      /* Only generate a mipmap if it does not make use reupload
       * a GL texture which we could otherwise use directly.
       */
      if (GDK_IS_GL_TEXTURE (texture) &&
          gdk_gl_context_is_shared (gdk_gl_texture_get_context (GDK_GL_TEXTURE (texture)),
                                    job->command_queue->context))
        ensure_mipmap = gdk_gl_texture_has_mipmap (GDK_GL_TEXTURE (texture));
      else if (GDK_IS_DMABUF_TEXTURE (texture))
        ensure_mipmap = FALSE;

      offscreen->texture_id = gsk_gl_driver_load_texture (job->driver, texture, ensure_mipmap);
      init_full_texture_region (offscreen);
      offscreen->has_mipmap = ensure_mipmap;

      if (GDK_IS_GL_TEXTURE (texture) &&
          offscreen->texture_id == gdk_gl_texture_get_id (GDK_GL_TEXTURE (texture)))
        offscreen->sync = gdk_gl_texture_get_sync (GDK_GL_TEXTURE (texture));
    }
}

static inline void
gsk_gl_render_job_visit_texture (GskGLRenderJob        *job,
                                 GdkTexture            *texture,
                                 const graphene_rect_t *bounds)
{
  int max_texture_size = job->command_queue->max_texture_size;
  float scale_x = bounds->size.width / texture->width;
  float scale_y = bounds->size.height / texture->height;
  gboolean use_mipmap;

  use_mipmap = (scale_x * fabsf (job->scale_x)) < 0.5 ||
               (scale_y * fabsf (job->scale_y)) < 0.5;

  if G_LIKELY (texture->width <= max_texture_size &&
               texture->height <= max_texture_size)
    {
      GskGLRenderOffscreen offscreen = {0};

      gsk_gl_render_job_upload_texture (job, texture, use_mipmap, &offscreen);

      g_assert (offscreen.texture_id);
      g_assert (offscreen.was_offscreen == FALSE);

      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
        {
          gsk_gl_program_set_uniform_texture_with_sync (job->current_program,
                                                        UNIFORM_SHARED_SOURCE, 0,
                                                        GL_TEXTURE_2D,
                                                        GL_TEXTURE0,
                                                        offscreen.texture_id,
                                                        offscreen.has_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR,
                                                        GL_LINEAR,
                                                        offscreen.sync);
          job->source_is_glyph_atlas = FALSE;
          gsk_gl_render_job_draw_offscreen (job, bounds, &offscreen);
          gsk_gl_render_job_end_draw (job);
        }
    }
  else
    {
      float min_x = job->offset_x + bounds->origin.x;
      float min_y = job->offset_y + bounds->origin.y;
      GskGLTextureSlice *slices = NULL;
      guint n_slices = 0;

      gsk_gl_driver_slice_texture (job->driver, texture, use_mipmap, &slices, &n_slices);

      g_assert (slices != NULL);
      g_assert (n_slices > 0);

      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
        {
          for (unsigned int i = 0; i < n_slices; i++)
            {
              const GskGLTextureSlice *slice = &slices[i];
              float x1, x2, y1, y2;

              x1 = min_x + (scale_x * slice->rect.x);
              x2 = x1 + (slice->rect.width * scale_x);
              y1 = min_y + (scale_y * slice->rect.y);
              y2 = y1 + (slice->rect.height * scale_y);

              if (i > 0)
                gsk_gl_render_job_split_draw (job);
              gsk_gl_program_set_uniform_texture_with_filter (job->current_program,
                                                              UNIFORM_SHARED_SOURCE, 0,
                                                              GL_TEXTURE_2D,
                                                              GL_TEXTURE0,
                                                              slice->texture_id,
                                                              use_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR,
                                                              GL_LINEAR);
              job->source_is_glyph_atlas = FALSE;

              gsk_gl_render_job_draw_coords (job,
                                             x1, y1, x2, y2,
                                             slice->area.x, slice->area.y,
                                             slice->area.x2, slice->area.y2,
                                             (guint16[]) { FP16_ZERO, FP16_ZERO, FP16_ZERO, FP16_ZERO });
            }

          gsk_gl_render_job_end_draw (job);
        }
    }
}

static inline void
gsk_gl_render_job_visit_texture_node (GskGLRenderJob      *job,
                                      const GskRenderNode *node)
{
  GdkTexture *texture = gsk_texture_node_get_texture (node);
  const graphene_rect_t *bounds = &node->bounds;

  gsk_gl_render_job_visit_texture (job, texture, bounds);
}

static inline void
gsk_gl_render_job_visit_texture_scale_node (GskGLRenderJob      *job,
                                            const GskRenderNode *node)
{
  GdkTexture *texture = gsk_texture_scale_node_get_texture (node);
  const graphene_rect_t *bounds = &node->bounds;
  GskScalingFilter filter = gsk_texture_scale_node_get_filter (node);
  int min_filters[] = { GL_LINEAR, GL_NEAREST, GL_LINEAR_MIPMAP_LINEAR };
  int mag_filters[] = { GL_LINEAR, GL_NEAREST, GL_LINEAR };
  int min_filter = min_filters[filter];
  int mag_filter = mag_filters[filter];
  int max_texture_size = job->command_queue->max_texture_size;
  graphene_rect_t clip_rect;
  GskGLRenderTarget *render_target;
  graphene_rect_t viewport;
  graphene_rect_t prev_viewport;
  graphene_matrix_t prev_projection;
  float prev_alpha;
  guint prev_fbo;
  float u0, u1, v0, v1;
  GskTextureKey key;
  guint texture_id;
  gboolean need_mipmap;
  gboolean has_mipmap;

  gsk_gl_render_job_untransform_bounds (job, &job->current_clip->rect.bounds, &clip_rect);

  if (!gsk_rect_intersection (bounds, &clip_rect, &clip_rect))
    return;

  key.pointer = node;
  key.pointer_is_child = TRUE;
  key.parent_rect = clip_rect;
  key.scale_x = 1.;
  key.scale_y = 1.;

  need_mipmap = (filter == GSK_SCALING_FILTER_TRILINEAR);

  texture_id = gsk_gl_driver_lookup_texture (job->driver, &key, &has_mipmap);

  if (texture_id != 0 && (!need_mipmap || has_mipmap))
    goto render_texture;

  viewport = GRAPHENE_RECT_INIT (0, 0,
                                 clip_rect.size.width,
                                 clip_rect.size.height);

  if (!gsk_gl_driver_create_render_target (job->driver,
                                           (int) ceilf (clip_rect.size.width),
                                           (int) ceilf (clip_rect.size.height),
                                           get_target_format (job, node),
                                           &render_target))
    {
      gsk_gl_render_job_visit_texture (job, texture, bounds);
      return;
    }

  gsk_gl_render_job_set_viewport (job, &viewport, &prev_viewport);
  gsk_gl_render_job_set_projection_from_rect (job, &viewport, &prev_projection);
  gsk_gl_render_job_set_modelview (job, NULL);
  prev_alpha = gsk_gl_render_job_set_alpha (job, 1.0f);
  gsk_gl_render_job_push_clip (job, &GSK_ROUNDED_RECT_INIT_FROM_RECT (viewport));

  prev_fbo = gsk_gl_command_queue_bind_framebuffer (job->command_queue, render_target->framebuffer_id);
  gsk_gl_command_queue_clear (job->command_queue, 0, &viewport);

  if G_LIKELY (texture->width <= max_texture_size &&
               texture->height <= max_texture_size)
    {
      gpointer sync;

      texture_id = gsk_gl_driver_load_texture (job->driver, texture, need_mipmap);

      if (GDK_IS_GL_TEXTURE (texture) && texture_id == gdk_gl_texture_get_id (GDK_GL_TEXTURE (texture)))
        sync = gdk_gl_texture_get_sync (GDK_GL_TEXTURE (texture));
      else
        sync = NULL;

      u0 = (clip_rect.origin.x - bounds->origin.x) / bounds->size.width;
      v0 = (clip_rect.origin.y - bounds->origin.y) / bounds->size.height;
      u1 = (clip_rect.origin.x + clip_rect.size.width - bounds->origin.x) / bounds->size.width;
      v1 = (clip_rect.origin.y + clip_rect.size.height - bounds->origin.y) / bounds->size.height;

      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
        {
          gsk_gl_program_set_uniform_texture_with_sync (job->current_program,
                                                        UNIFORM_SHARED_SOURCE, 0,
                                                        GL_TEXTURE_2D,
                                                        GL_TEXTURE0,
                                                        texture_id,
                                                        min_filter,
                                                        mag_filter,
                                                        sync);
          job->source_is_glyph_atlas = FALSE;
          gsk_gl_render_job_draw_coords (job,
                                         0, 0, clip_rect.size.width, clip_rect.size.height,
                                         u0, v0, u1, v1,
                                         (guint16[]) { FP16_ZERO, FP16_ZERO, FP16_ZERO, FP16_ZERO });
          gsk_gl_render_job_end_draw (job);
        }
    }
  else
    {
      float scale_x = bounds->size.width / texture->width;
      float scale_y = bounds->size.height / texture->height;
      GskGLTextureSlice *slices = NULL;
      guint n_slices = 0;

      gsk_gl_driver_slice_texture (job->driver, texture, need_mipmap, &slices, &n_slices);

      if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
        {
          for (guint i = 0; i < n_slices; i++)
            {
              const GskGLTextureSlice *slice = &slices[i];
              graphene_rect_t slice_bounds;

              slice_bounds.origin.x = bounds->origin.x - clip_rect.origin.x + slice->rect.x * scale_x;
              slice_bounds.origin.y = bounds->origin.y - clip_rect.origin.y + slice->rect.y * scale_y;
              slice_bounds.size.width = slice->rect.width * scale_x;
              slice_bounds.size.height = slice->rect.height * scale_y;

              if (!gsk_rect_intersects (&slice_bounds, &viewport))
                continue;

              if (i > 0)
                gsk_gl_render_job_split_draw (job);

              gsk_gl_program_set_uniform_texture_with_filter (job->current_program,
                                                              UNIFORM_SHARED_SOURCE, 0,
                                                              GL_TEXTURE_2D,
                                                              GL_TEXTURE0,
                                                              slice->texture_id,
                                                              min_filter,
                                                              mag_filter);
              job->source_is_glyph_atlas = FALSE;
              gsk_gl_render_job_draw_coords (job,
                                             slice_bounds.origin.x,
                                             slice_bounds.origin.y,
                                             slice_bounds.origin.x + slice_bounds.size.width,
                                             slice_bounds.origin.y + slice_bounds.size.height,
                                             slice->area.x, slice->area.y,
                                             slice->area.x2, slice->area.y2,
                                             (guint16[]){ FP16_ZERO, FP16_ZERO, FP16_ZERO, FP16_ZERO } );
            }

          gsk_gl_render_job_end_draw (job);
        }
    }

  gsk_gl_render_job_pop_clip (job);
  gsk_gl_render_job_pop_modelview (job);
  gsk_gl_render_job_set_viewport (job, &prev_viewport, NULL);
  gsk_gl_render_job_set_projection (job, &prev_projection);
  gsk_gl_render_job_set_alpha (job, prev_alpha);
  gsk_gl_command_queue_bind_framebuffer (job->command_queue, prev_fbo);

  texture_id = gsk_gl_driver_release_render_target (job->driver, render_target, FALSE);
  gsk_gl_driver_cache_texture (job->driver, &key, texture_id);

render_texture:
  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_render_job_draw_coords (job,
                                     job->offset_x + clip_rect.origin.x,
                                     job->offset_y + clip_rect.origin.y,
                                     job->offset_x + clip_rect.origin.x + clip_rect.size.width,
                                     job->offset_y + clip_rect.origin.y + clip_rect.size.height,
                                     0, clip_rect.size.width / ceilf (clip_rect.size.width),
                                     clip_rect.size.height / ceilf (clip_rect.size.height), 0,
                                     (guint16[]){ FP16_ZERO, FP16_ZERO, FP16_ZERO, FP16_ZERO } );
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_repeat_node (GskGLRenderJob      *job,
                                     const GskRenderNode *node)
{
  const GskRenderNode *child = gsk_repeat_node_get_child (node);
  const graphene_rect_t *child_bounds = gsk_repeat_node_get_child_bounds (node);
  GskGLRenderOffscreen offscreen = {0};

  if (node_is_invisible (child))
    return;

  if (!gsk_rect_equal (child_bounds, &child->bounds))
    {
      /* TODO: implement these repeat nodes. */
      gsk_gl_render_job_visit_as_fallback (job, node);
      return;
    }

  /* If the size of the repeat node is smaller than the size of the
   * child node, we don't repeat at all and can just draw that part
   * of the child texture... */
  if (gsk_rect_contains_rect (child_bounds, &node->bounds))
    {
      gsk_gl_render_job_visit_clipped_child (job, child, &node->bounds);
      return;
    }

  offscreen.bounds = &child->bounds;
  offscreen.reset_clip = TRUE;

  if (!gsk_gl_render_job_visit_node_with_offscreen (job, child, &offscreen))
    g_assert_not_reached ();

  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, repeat)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          offscreen.texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_program_set_uniform4f (job->current_program,
                                    UNIFORM_REPEAT_CHILD_BOUNDS, 0,
                                    (node->bounds.origin.x - child_bounds->origin.x) / child_bounds->size.width,
                                    (node->bounds.origin.y - child_bounds->origin.y) / child_bounds->size.height,
                                    node->bounds.size.width / child_bounds->size.width,
                                    node->bounds.size.height / child_bounds->size.height);
      gsk_gl_program_set_uniform4f (job->current_program,
                                    UNIFORM_REPEAT_TEXTURE_RECT, 0,
                                    offscreen.area.x,
                                    offscreen.was_offscreen ? offscreen.area.y2 : offscreen.area.y,
                                    offscreen.area.x2,
                                    offscreen.was_offscreen ? offscreen.area.y : offscreen.area.y2);
      gsk_gl_render_job_draw_offscreen (job, &node->bounds, &offscreen);
      gsk_gl_render_job_end_draw (job);
    }
}

static inline void
gsk_gl_render_job_visit_subsurface_node (GskGLRenderJob      *job,
                                         const GskRenderNode *node)
{
  GdkSubsurface *subsurface;

  subsurface = (GdkSubsurface *) gsk_subsurface_node_get_subsurface (node);

  if (subsurface &&
      gdk_subsurface_get_texture (subsurface) && 
      gdk_subsurface_get_parent (subsurface) == gdk_gl_context_get_surface (job->command_queue->context))
    {
      if (!gdk_subsurface_is_above_parent (subsurface))
        {
          if (job->offscreen_count > 0)
            {
              GDK_DISPLAY_DEBUG (gdk_gl_context_get_display (job->command_queue->context), OFFLOAD, "Hiding subsurface %p in offscreen context", subsurface);
              gdk_subsurface_detach (subsurface);
              gsk_gl_render_job_visit_node (job, gsk_subsurface_node_get_child (node));
            }
          else if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, color)))
            {
              /* Clear the area so we can see through */
              GskGLCommandBatch *batch;
              guint16 color[4];
              rgba_to_half (&(GdkRGBA){0,0,0,0}, color);

              batch = gsk_gl_command_queue_get_batch (job->command_queue);
              batch->draw.blend = 0;
              gsk_gl_render_job_draw_rect_with_color (job, &node->bounds, color);
              gsk_gl_render_job_end_draw (job);
            }
        }
    }
  else
    {
      gsk_gl_render_job_visit_node (job, gsk_subsurface_node_get_child (node));
    }
}

static void
gsk_gl_render_job_visit_node (GskGLRenderJob      *job,
                              const GskRenderNode *node)
{
  gboolean has_clip;

  g_assert (job != NULL);
  g_assert (node != NULL);
  g_assert (GSK_IS_GL_DRIVER (job->driver));
  g_assert (GSK_IS_GL_COMMAND_QUEUE (job->command_queue));

  if (node_is_invisible (node))
    return;

  if (!gsk_gl_render_job_update_clip (job, &node->bounds, &has_clip))
    return;

  switch (GSK_RENDER_NODE_TYPE (node))
    {
    case GSK_BLEND_NODE:
      gsk_gl_render_job_visit_blend_node (job, node);
    break;

    case GSK_BLUR_NODE:
      if (gsk_blur_node_get_radius (node) > 0)
        gsk_gl_render_job_visit_blur_node (job, node);
      else
        gsk_gl_render_job_visit_node (job, gsk_blur_node_get_child (node));
    break;

    case GSK_BORDER_NODE:
      if (gsk_border_node_get_uniform_color (node) &&
          gsk_rounded_rect_is_rectilinear (gsk_border_node_get_outline (node)))
        gsk_gl_render_job_visit_rect_border_node (job, node);
      else
        gsk_gl_render_job_visit_border_node (job, node);
    break;

    case GSK_CLIP_NODE:
      gsk_gl_render_job_visit_clip_node (job, node);
    break;

    case GSK_COLOR_NODE:
      gsk_gl_render_job_visit_color_node (job, node);
    break;

    case GSK_COLOR_MATRIX_NODE:
      gsk_gl_render_job_visit_color_matrix_node (job, node);
    break;

    case GSK_CONIC_GRADIENT_NODE:
      if (gsk_conic_gradient_node_get_n_color_stops (node) < MAX_GRADIENT_STOPS)
        gsk_gl_render_job_visit_conic_gradient_node (job, node);
      else
        gsk_gl_render_job_visit_as_fallback (job, node);
    break;

    case GSK_CONTAINER_NODE:
      {
        GskRenderNode **children;
        guint n_children;

        children = gsk_container_node_get_children (node, &n_children);

        for (guint i = 0; i < n_children; i++)
          {
            const GskRenderNode *child = children[i];

            if (i + 1 < n_children &&
                job->current_clip->is_fully_contained &&
                GSK_RENDER_NODE_TYPE (child) == GSK_ROUNDED_CLIP_NODE)
              {
                const GskRenderNode *grandchild = gsk_rounded_clip_node_get_child (child);
                const GskRenderNode *child2 = children[i + 1];
                if (GSK_RENDER_NODE_TYPE (grandchild) == GSK_COLOR_NODE &&
                    GSK_RENDER_NODE_TYPE (child2) == GSK_BORDER_NODE &&
                    gsk_border_node_get_uniform_color (child2) &&
                    rounded_rect_equal (gsk_rounded_clip_node_get_clip (child),
                                        gsk_border_node_get_outline (child2)))
                  {
                    gsk_gl_render_job_visit_css_background (job, child, child2);
                    i++; /* skip the border node */
                    continue;
                  }
              }

            gsk_gl_render_job_visit_node (job, child);
          }
      }
    break;

    case GSK_CROSS_FADE_NODE:
      {
        const GskRenderNode *start_node = gsk_cross_fade_node_get_start_child (node);
        const GskRenderNode *end_node = gsk_cross_fade_node_get_end_child (node);
        float progress = gsk_cross_fade_node_get_progress (node);

        if (progress <= 0.0f)
          gsk_gl_render_job_visit_node (job, gsk_cross_fade_node_get_start_child (node));
        else if (progress >= 1.0f || equal_texture_nodes (start_node, end_node))
          gsk_gl_render_job_visit_node (job, gsk_cross_fade_node_get_end_child (node));
        else
          gsk_gl_render_job_visit_cross_fade_node (job, node);
      }
    break;

    case GSK_DEBUG_NODE:
      /* Debug nodes are ignored because draws get reordered anyway */
      gsk_gl_render_job_visit_node (job, gsk_debug_node_get_child (node));
    break;

    case GSK_GL_SHADER_NODE:
      gsk_gl_render_job_visit_gl_shader_node (job, node);
    break;

    case GSK_INSET_SHADOW_NODE:
      if (gsk_inset_shadow_node_get_blur_radius (node) > 0)
        gsk_gl_render_job_visit_blurred_inset_shadow_node (job, node);
      else
        gsk_gl_render_job_visit_unblurred_inset_shadow_node (job, node);
    break;

    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
      if (gsk_linear_gradient_node_get_n_color_stops (node) < MAX_GRADIENT_STOPS)
        gsk_gl_render_job_visit_linear_gradient_node (job, node);
      else
        gsk_gl_render_job_visit_as_fallback (job, node);
    break;

    case GSK_MASK_NODE:
      gsk_gl_render_job_visit_mask_node (job, node);
    break;

    case GSK_OPACITY_NODE:
      gsk_gl_render_job_visit_opacity_node (job, node);
    break;

    case GSK_OUTSET_SHADOW_NODE:
      if (gsk_outset_shadow_node_get_blur_radius (node) > 0)
        gsk_gl_render_job_visit_blurred_outset_shadow_node (job, node);
      else
        gsk_gl_render_job_visit_unblurred_outset_shadow_node (job, node);
    break;

    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
      if (gsk_radial_gradient_node_get_n_color_stops (node) < MAX_GRADIENT_STOPS)
        gsk_gl_render_job_visit_radial_gradient_node (job, node);
      else
        gsk_gl_render_job_visit_as_fallback (job, node);
    break;

    case GSK_REPEAT_NODE:
      gsk_gl_render_job_visit_repeat_node (job, node);
    break;

    case GSK_ROUNDED_CLIP_NODE:
      gsk_gl_render_job_visit_rounded_clip_node (job, node);
    break;

    case GSK_SHADOW_NODE:
      gsk_gl_render_job_visit_shadow_node (job, node);
    break;

    case GSK_TEXT_NODE:
      gsk_gl_render_job_visit_text_node (job,
                                         node,
                                         gsk_text_node_get_color (node),
                                         FALSE);
    break;

    case GSK_TEXTURE_NODE:
      gsk_gl_render_job_visit_texture_node (job, node);
    break;

    case GSK_TEXTURE_SCALE_NODE:
      gsk_gl_render_job_visit_texture_scale_node (job, node);
    break;

    case GSK_TRANSFORM_NODE:
      gsk_gl_render_job_visit_transform_node (job, node);
    break;

    case GSK_CAIRO_NODE:
      gsk_gl_render_job_visit_as_fallback (job, node);
    break;

    case GSK_FILL_NODE:
      gsk_gl_render_job_visit_as_fallback (job, node);
    break;

    case GSK_STROKE_NODE:
      gsk_gl_render_job_visit_as_fallback (job, node);
    break;

    case GSK_SUBSURFACE_NODE:
      gsk_gl_render_job_visit_subsurface_node (job, node);
    break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    break;
    }

  if (has_clip)
    gsk_gl_render_job_pop_clip (job);
}

static gboolean
gsk_gl_render_job_visit_node_with_offscreen (GskGLRenderJob       *job,
                                             const GskRenderNode  *node,
                                             GskGLRenderOffscreen *offscreen)
{
  GskTextureKey key;
  guint cached_id;

  g_assert (job != NULL);
  g_assert (node != NULL);
  g_assert (offscreen != NULL);
  g_assert (offscreen->texture_id == 0);
  g_assert (offscreen->bounds != NULL);

  if (node_is_invisible (node))
    {
      /* Just to be safe. */
      offscreen->texture_id = 0;
      init_full_texture_region (offscreen);
      offscreen->was_offscreen = FALSE;
      return FALSE;
    }

  if (GSK_RENDER_NODE_TYPE (node) == GSK_TEXTURE_NODE &&
      !offscreen->force_offscreen)
    {
      GdkTexture *texture = gsk_texture_node_get_texture (node);
      gsk_gl_render_job_upload_texture (job, texture, FALSE, offscreen);
      return TRUE;
    }

  key.pointer = node;
  key.pointer_is_child = TRUE; /* Don't conflict with the child using the cache too */
  key.parent_rect = *offscreen->bounds;
  key.scale_x = job->scale_x;
  key.scale_y = job->scale_y;

  float offset_x = job->offset_x;
  float offset_y = job->offset_y;
  gboolean flipped_x = job->scale_x < 0;
  gboolean flipped_y = job->scale_y < 0;
  graphene_rect_t viewport;
  gboolean reset_clip = FALSE;

  if (flipped_x || flipped_y)
    {
      GskTransform *transform = gsk_transform_scale (NULL,
                                                     flipped_x ? -1 : 1,
                                                     flipped_y ? -1 : 1);
      gsk_gl_render_job_push_modelview (job, transform);
      gsk_transform_unref (transform);
    }

  gsk_gl_render_job_transform_bounds (job, offscreen->bounds, &viewport);

  float aligned_x = floorf (viewport.origin.x);
  float padding_left = viewport.origin.x - aligned_x;
  float aligned_width = ceilf (viewport.size.width + padding_left);
  float padding_right = aligned_width - viewport.size.width - padding_left;

  float aligned_y = floorf (viewport.origin.y);
  float padding_top = viewport.origin.y - aligned_y;
  float aligned_height = ceilf (viewport.size.height + padding_top);
  float padding_bottom = aligned_height - viewport.size.height - padding_top;

  /* Tweak the scale factor so that the required texture doesn't
   * exceed the max texture limit. This will render with a lower
   * resolution, but this is better than clipping.
   */

  g_assert (job->command_queue->max_texture_size > 0);

  float downscale_x = 1;
  float downscale_y = 1;
  int texture_width;
  int texture_height;
  int max_texture_size = job->command_queue->max_texture_size;

  if (aligned_width > max_texture_size)
    downscale_x = (float)max_texture_size / viewport.size.width;

  if (aligned_height > max_texture_size)
    downscale_y = (float)max_texture_size / viewport.size.height;

  if (downscale_x != 1 || downscale_y != 1)
    {
      GskTransform *transform = gsk_transform_scale (NULL, downscale_x, downscale_y);
      gsk_gl_render_job_push_modelview (job, transform);
      gsk_transform_unref (transform);
      gsk_gl_render_job_transform_bounds (job, offscreen->bounds, &viewport);
      gsk_rect_scale (&viewport, downscale_x, downscale_y, &viewport);
    }

  if (downscale_x == 1)
    {
      viewport.origin.x = aligned_x;
      viewport.size.width = aligned_width;
      offscreen->area.x = padding_left / aligned_width;
      offscreen->area.x2 = 1.0f - (padding_right / aligned_width);
      texture_width = aligned_width;
    }
  else
    {
      offscreen->area.x = 0;
      offscreen->area.x2 = 1;
      texture_width = max_texture_size;
    }

  if (downscale_y == 1)
    {
      viewport.origin.y = aligned_y;
      viewport.size.height = aligned_height;
      offscreen->area.y = padding_bottom / aligned_height;
      offscreen->area.y2 = 1.0f - padding_top / aligned_height;
      texture_height = aligned_height;
    }
   else
    {
      offscreen->area.y = 0;
      offscreen->area.y2 = 1;
      texture_height = max_texture_size;
    }

  /* Check if we've already cached the drawn texture. */
  cached_id = gsk_gl_driver_lookup_texture (job->driver, &key, NULL);

  if (cached_id != 0)
    {
      if (downscale_x != 1 || downscale_y != 1)
        gsk_gl_render_job_pop_modelview (job);
      if (flipped_x || flipped_y)
        gsk_gl_render_job_pop_modelview (job);
      offscreen->texture_id = cached_id;
      /* We didn't render it offscreen, but hand out an offscreen texture id */
      offscreen->was_offscreen = TRUE;
      return TRUE;
    }

  GskGLRenderTarget *render_target;
  graphene_matrix_t prev_projection;
  graphene_rect_t prev_viewport;
  float prev_alpha;
  guint prev_fbo;

  if (!gsk_gl_driver_create_render_target (job->driver,
                                           texture_width, texture_height,
                                           get_target_format (job, node),
                                           &render_target))
    g_assert_not_reached ();

  if (gdk_gl_context_has_feature (job->command_queue->context, GDK_GL_FEATURE_DEBUG))
    {
      gdk_gl_context_label_object_printf (job->command_queue->context,
                                          GL_TEXTURE,
                                          render_target->texture_id,
                                          "Offscreen<%s> %d",
                                          g_type_name_from_instance ((GTypeInstance *) node),
                                          render_target->texture_id);
      gdk_gl_context_label_object_printf (job->command_queue->context,
                                          GL_FRAMEBUFFER,
                                          render_target->framebuffer_id,
                                          "Offscreen<%s> FB %d",
                                          g_type_name_from_instance ((GTypeInstance *) node),
                                          render_target->framebuffer_id);
    }

  gsk_gl_render_job_set_viewport (job, &viewport, &prev_viewport);
  gsk_gl_render_job_set_projection_from_rect (job, &job->viewport, &prev_projection);
  prev_alpha = gsk_gl_render_job_set_alpha (job, 1.0f);

  prev_fbo = gsk_gl_command_queue_bind_framebuffer (job->command_queue, render_target->framebuffer_id);
  gsk_gl_command_queue_clear (job->command_queue, 0, &job->viewport);

  if (offscreen->reset_clip)
    {
      gsk_gl_render_job_push_clip (job, &GSK_ROUNDED_RECT_INIT_FROM_RECT (job->viewport));
      reset_clip = TRUE;
    }
  else if (flipped_x || flipped_y || downscale_x != 1 || downscale_y != 1)
    {
      GskRoundedRect new_clip;
      float scale_x = flipped_x ? - downscale_x : downscale_x;
      float scale_y = flipped_y ? - downscale_y : downscale_y;

      gsk_rect_scale (&job->current_clip->rect.bounds, scale_x, scale_y, &new_clip.bounds);
      rounded_rect_scale_corners (&job->current_clip->rect, &new_clip, scale_x, scale_y);
      gsk_gl_render_job_push_clip (job, &new_clip);
      reset_clip = TRUE;
    }

  job->offscreen_count++;

  gsk_gl_render_job_visit_node (job, node);

  job->offscreen_count--;

  if (reset_clip)
    gsk_gl_render_job_pop_clip (job);

  if (downscale_x != 1 || downscale_y != 1)
    gsk_gl_render_job_pop_modelview (job);

  if (flipped_x || flipped_y)
    gsk_gl_render_job_pop_modelview (job);

  gsk_gl_render_job_set_viewport (job, &prev_viewport, NULL);
  gsk_gl_render_job_set_projection (job, &prev_projection);
  gsk_gl_render_job_set_alpha (job, prev_alpha);
  gsk_gl_command_queue_bind_framebuffer (job->command_queue, prev_fbo);

  job->offset_x = offset_x;
  job->offset_y = offset_y;

  offscreen->was_offscreen = TRUE;
  offscreen->texture_id = gsk_gl_driver_release_render_target (job->driver,
                                                               render_target,
                                                               FALSE);

  if (!offscreen->do_not_cache)
    gsk_gl_driver_cache_texture (job->driver, &key, offscreen->texture_id);

  return TRUE;
}

void
gsk_gl_render_job_render_flipped (GskGLRenderJob *job,
                                  GskRenderNode  *root)
{
  graphene_matrix_t proj;
  guint framebuffer_id;
  guint texture_id;
  guint surface_height;

  g_return_if_fail (job != NULL);
  g_return_if_fail (root != NULL);
  g_return_if_fail (GSK_IS_GL_DRIVER (job->driver));

  surface_height = job->viewport.size.height;

  graphene_matrix_init_ortho (&proj,
                              job->viewport.origin.x,
                              job->viewport.origin.x + job->viewport.size.width,
                              job->viewport.origin.y,
                              job->viewport.origin.y + job->viewport.size.height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_matrix_scale (&proj, 1, -1, 1);

  if (!gsk_gl_command_queue_create_render_target (job->command_queue,
                                                  MAX (1, job->viewport.size.width),
                                                  MAX (1, job->viewport.size.height),
                                                  job->target_format,
                                                  &framebuffer_id, &texture_id))
    return;

  /* Setup drawing to our offscreen texture/framebuffer which is flipped */
  gsk_gl_command_queue_bind_framebuffer (job->command_queue, framebuffer_id);
  gsk_gl_command_queue_clear (job->command_queue, 0, &job->viewport);

  /* Visit all nodes creating batches */
  gdk_gl_context_push_debug_group (job->command_queue->context, "Building command queue");
  gsk_gl_render_job_visit_node (job, root);
  gdk_gl_context_pop_debug_group (job->command_queue->context);

  /* Now draw to our real destination, but flipped */
  gsk_gl_render_job_set_alpha (job, 1.0f);
  gsk_gl_command_queue_bind_framebuffer (job->command_queue, job->framebuffer);
  gsk_gl_command_queue_clear (job->command_queue, 0, &job->viewport);
  if (gsk_gl_render_job_begin_draw (job, CHOOSE_PROGRAM (job, blit)))
    {
      gsk_gl_program_set_uniform_texture (job->current_program,
                                          UNIFORM_SHARED_SOURCE, 0,
                                          GL_TEXTURE_2D,
                                          GL_TEXTURE0,
                                          texture_id);
      job->source_is_glyph_atlas = FALSE;
      gsk_gl_render_job_draw_rect (job, &job->viewport);
      gsk_gl_render_job_end_draw (job);
    }

  gdk_gl_context_push_debug_group (job->command_queue->context, "Executing command queue");
  gsk_gl_command_queue_execute (job->command_queue, surface_height, 1, NULL, job->default_framebuffer);
  gdk_gl_context_pop_debug_group (job->command_queue->context);

  glDeleteFramebuffers (1, &framebuffer_id);
  glDeleteTextures (1, &texture_id);
}

void
gsk_gl_render_job_render (GskGLRenderJob *job,
                          GskRenderNode  *root)
{
  G_GNUC_UNUSED gint64 start_time;
  float scale;
  guint surface_height;

  g_return_if_fail (job != NULL);
  g_return_if_fail (root != NULL);
  g_return_if_fail (GSK_IS_GL_DRIVER (job->driver));

  scale = MAX (job->scale_x, job->scale_y);
  surface_height = job->viewport.size.height;

  gsk_gl_command_queue_make_current (job->command_queue);

  /* Build the command queue using the shared GL context for all renderers
   * on the same display.
   */
  start_time = GDK_PROFILER_CURRENT_TIME;
  gdk_gl_context_push_debug_group (job->command_queue->context, "Building command queue");
  gsk_gl_command_queue_bind_framebuffer (job->command_queue, job->framebuffer);
  if (job->clear_framebuffer)
    gsk_gl_command_queue_clear (job->command_queue, 0, &job->viewport);
  gsk_gl_render_job_visit_node (job, root);
  gdk_gl_context_pop_debug_group (job->command_queue->context);

  gdk_profiler_end_mark (start_time, "Build GL command queue", "");

#if 0
  /* At this point the atlases have uploaded content while we processed
   * nodes but have not necessarily been used by the commands in the queue.
   */
  gsk_gl_driver_save_atlases_to_png (job->driver, NULL);
#endif

  /* But now for executing the command queue, we want to use the context
   * that was provided to us when creating the render job as framebuffer 0
   * is bound to that context.
   */
  start_time = GDK_PROFILER_CURRENT_TIME;
  gsk_gl_command_queue_make_current (job->command_queue);
  gdk_gl_context_push_debug_group (job->command_queue->context, "Executing command queue");
  gsk_gl_command_queue_execute (job->command_queue, surface_height, scale, job->region, job->default_framebuffer);
  gdk_gl_context_pop_debug_group (job->command_queue->context);
  gdk_profiler_end_mark (start_time, "Execute GL command queue", "");
}

static int
get_framebuffer_format (GdkGLContext *context,
                        guint         framebuffer)
{
  int size;

  if (!gdk_gl_context_check_version (context, NULL, "3.0"))
    return GL_RGBA8;

  glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);
  glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                         framebuffer ? GL_COLOR_ATTACHMENT0
                                                     : gdk_gl_context_get_use_es (context) ? GL_BACK
                                                                                           : GL_BACK_LEFT,
                                         GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &size);

  if (size > 16)
    return GL_RGBA32F;
  else if (size > 8)
    return GL_RGBA16F;
  else
    return GL_RGBA8;
}

GskGLRenderJob *
gsk_gl_render_job_new (GskGLDriver           *driver,
                       const graphene_rect_t *viewport,
                       float                  scale,
                       const cairo_region_t  *region,
                       guint                  framebuffer,
                       gboolean               clear_framebuffer)
{
  const graphene_rect_t *clip_rect = viewport;
  graphene_rect_t transformed_extents;
  GskGLRenderJob *job;
  GdkGLContext *context;
  GLint default_framebuffer = 0;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), NULL);
  g_return_val_if_fail (viewport != NULL, NULL);
  g_return_val_if_fail (scale > 0, NULL);

  /* Check for non-standard framebuffer binding as we might not be using
   * the default framebuffer on systems like macOS where we've bound an
   * IOSurface to a GL_TEXTURE_RECTANGLE. Otherwise, no scissor clip will
   * be applied in the command queue causing overdrawing.
   */
  context = driver->command_queue->context;
  default_framebuffer = GDK_GL_CONTEXT_GET_CLASS (context)->get_default_framebuffer (context);
  if (framebuffer == 0 && default_framebuffer != 0)
    framebuffer = default_framebuffer;

  job = g_new0 (GskGLRenderJob, 1);
  job->driver = g_object_ref (driver);
  job->command_queue = job->driver->command_queue;
  clips_init (&job->clip);
  modelviews_init (&job->modelview);
  job->framebuffer = framebuffer;
  job->clear_framebuffer = !!clear_framebuffer;
  job->default_framebuffer = default_framebuffer;
  job->offset_x = 0;
  job->offset_y = 0;
  job->scale_x = scale;
  job->scale_y = scale;
  job->viewport = *viewport;
  job->target_format = get_framebuffer_format (job->command_queue->context, framebuffer);

  gsk_gl_render_job_set_alpha (job, 1.0f);
  gsk_gl_render_job_set_projection_from_rect (job, viewport, NULL);
  gsk_gl_render_job_set_modelview (job, gsk_transform_scale (NULL, scale, scale));

  /* Setup our initial clip. If region is NULL then we are drawing the
   * whole viewport. Otherwise, we need to convert the region to a
   * bounding box and clip based on that.
   */

  if (region != NULL)
    {
      cairo_rectangle_int_t extents;

      cairo_region_get_extents (region, &extents);
      gsk_gl_render_job_transform_bounds (job,
                                          &GRAPHENE_RECT_INIT (extents.x,
                                                               extents.y,
                                                               extents.width,
                                                               extents.height),
                                          &transformed_extents);
      clip_rect = &transformed_extents;
      job->region = cairo_region_create_rectangle (&extents);
    }

  gsk_gl_render_job_push_clip (job,
                               &GSK_ROUNDED_RECT_INIT (clip_rect->origin.x,
                                                       clip_rect->origin.y,
                                                       clip_rect->size.width,
                                                       clip_rect->size.height));

  return job;
}

void
gsk_gl_render_job_free (GskGLRenderJob *job)
{
  job->current_modelview = NULL;
  job->current_clip = NULL;

  while (job->modelview.end > job->modelview.start)
    {
      GskGLRenderModelview *modelview = job->modelview.end-1;
      g_clear_pointer (&modelview->transform, gsk_transform_unref);
      job->modelview.end--;
    }

  g_clear_object (&job->driver);
  g_clear_pointer (&job->region, cairo_region_destroy);
  modelviews_clear (&job->modelview);
  clips_clear (&job->clip);
  g_free (job);
}
