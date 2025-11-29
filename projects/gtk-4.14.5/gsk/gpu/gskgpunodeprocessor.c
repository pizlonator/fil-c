#include "config.h"

#include "gskgpunodeprocessorprivate.h"

#include "gskgpuborderopprivate.h"
#include "gskgpuboxshadowopprivate.h"
#include "gskgpublendmodeopprivate.h"
#include "gskgpublendopprivate.h"
#include "gskgpublitopprivate.h"
#include "gskgpubluropprivate.h"
#include "gskgpuclearopprivate.h"
#include "gskgpuclipprivate.h"
#include "gskgpucolorizeopprivate.h"
#include "gskgpucolormatrixopprivate.h"
#include "gskgpucoloropprivate.h"
#include "gskgpuconicgradientopprivate.h"
#include "gskgpucrossfadeopprivate.h"
#include "gskgpudescriptorsprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpuglobalsopprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpulineargradientopprivate.h"
#include "gskgpumaskopprivate.h"
#include "gskgpumipmapopprivate.h"
#include "gskgpuradialgradientopprivate.h"
#include "gskgpurenderpassopprivate.h"
#include "gskgpuroundedcoloropprivate.h"
#include "gskgpuscissoropprivate.h"
#include "gskgpustraightalphaopprivate.h"
#include "gskgputextureopprivate.h"
#include "gskgpuuberopprivate.h"
#include "gskgpuuploadopprivate.h"

#include "gskcairoblurprivate.h"
#include "gskdebugprivate.h"
#include "gskpath.h"
#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskroundedrectprivate.h"
#include "gskstrokeprivate.h"
#include "gsktransformprivate.h"
#include "gskprivate.h"

#include "gdk/gdkrgbaprivate.h"
#include "gdk/gdksubsurfaceprivate.h"

/* the epsilon we allow pixels to be off due to rounding errors.
 * Chosen rather randomly.
 */
#define EPSILON 0.001

/* A note about coordinate systems
 *
 * The rendering code keeps track of multiple coordinate systems to optimize rendering as
 * much as possible and in the coordinate system it makes most sense in.
 * Sometimes there are cases where GL requires a certain coordinate system, too.
 *
 * 1. the node coordinate system
 * This is the coordinate system of the rendernode. It is basically not used outside of
 * looking at the node and basically never hits the GPU (it does for paths). We immediately
 * convert it to:
 *
 * 2. the basic coordinate system
 * convert on CPU: NodeProcessor.offset
 * convert on GPU: ---
 * This is the coordinate system we emit vertex state in, the clip is tracked here.
 * The main benefit is that most transform nodes only change the offset, so we can avoid
 * updating any state in this coordinate system when that happens.
 *
 * 3. the scaled coordinate system
 * converts on CPU: NodeProcessor.scale
 * converts on GPU: GSK_GLOBAL_SCALE
 * This includes the current scale of the transform. It is usually equal to the scale factor
 * of the window we are rendering to (which is bad because devs without hidpi screens can
 * forget this and then everyone else will see bugs). We make decisions about pixel sizes in
 * this coordinate system, like picking glyphs from the glyph cache or the sizes of offscreens
 * for offscreen rendering.
 *
 * 4. the device coordinate system
 * converts on CPU: NodeProcessor.modelview
 * converts on GPU: ---
 * The scissor rect is tracked in this coordinate system. It represents the actual device pixels.
 * A bunch of optimizations (like glScissor() and glClear()) can be done here, so in the case
 * that modelview == NULL and we end up with integer coordinates (because pixels), we try to go
 * here.
 * This coordinate system does not exist on shaders as they rarely reason about pixels, and if
 * they need to, they can ask the fragment shader via gl_FragCoord.
 *
 * 5. the GL coordinate system
 * converts on CPU: NodeProcessor.projection
 * converts on GPU: GSK_GLOBAL_MVP (from scaled coordinate system)
 * This coordinate system is what GL (or Vulkan) expect coordinates to appear in, and is usually
 * (-1, -1) => (1, 1), but may be flipped etc depending on the render target. The CPU essentially
 * never uses it, other than to allow the vertex shaders to emit its vertices.
 */

typedef struct _GskGpuNodeProcessor GskGpuNodeProcessor;
typedef struct _GskGpuPatternWriter GskGpuPatternWriter;

typedef enum {
  GSK_GPU_GLOBAL_MATRIX  = (1 << 0),
  GSK_GPU_GLOBAL_SCALE   = (1 << 1),
  GSK_GPU_GLOBAL_CLIP    = (1 << 2),
  GSK_GPU_GLOBAL_SCISSOR = (1 << 3),
  GSK_GPU_GLOBAL_BLEND   = (1 << 4),
} GskGpuGlobals;

struct _GskGpuNodeProcessor
{
  GskGpuFrame                   *frame;
  GskGpuDescriptors             *desc;
  cairo_rectangle_int_t          scissor;
  GskGpuBlend                    blend;
  graphene_point_t               offset;
  graphene_matrix_t              projection;
  graphene_vec2_t                scale;
  GskTransform                  *modelview;
  GskGpuClip                     clip;
  float                          opacity;

  GskGpuGlobals                  pending_globals;
};

#define GDK_ARRAY_NAME pattern_buffer
#define GDK_ARRAY_TYPE_NAME PatternBuffer
#define GDK_ARRAY_ELEMENT_TYPE guchar
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 2048
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

struct _GskGpuPatternWriter
{
  GskGpuFrame                   *frame;
  GskGpuDescriptors             *desc;

  graphene_rect_t                bounds;
  graphene_point_t               offset;
  graphene_vec2_t                scale;
  guint                          stack;

  PatternBuffer                  buffer;
};

static void             gsk_gpu_node_processor_add_node                 (GskGpuNodeProcessor            *self,
                                                                         GskRenderNode                  *node);
static gboolean         gsk_gpu_node_processor_create_node_pattern      (GskGpuPatternWriter            *self,
                                                                         GskRenderNode                  *node);

static void
gsk_gpu_node_processor_finish (GskGpuNodeProcessor *self)
{
  g_clear_pointer (&self->modelview, gsk_transform_unref);
  g_clear_object (&self->desc);
}

static void
gsk_gpu_node_processor_init (GskGpuNodeProcessor         *self,
                             GskGpuFrame                 *frame,
                             GskGpuDescriptors           *desc,
                             GskGpuImage                 *target,
                             const cairo_rectangle_int_t *clip,
                             const graphene_rect_t       *viewport)
{
  gsize width, height;

  width = gsk_gpu_image_get_width (target);
  height = gsk_gpu_image_get_height (target);

  self->frame = frame;
  if (desc)
    self->desc = g_object_ref (desc);
  else
    self->desc = NULL;

  self->scissor = *clip;
  self->blend = GSK_GPU_BLEND_OVER;
  if (clip->x == 0 && clip->y == 0 && clip->width == width && clip->height == height)
    {
      gsk_gpu_clip_init_empty (&self->clip, &GRAPHENE_RECT_INIT (0, 0, viewport->size.width, viewport->size.height));
    }
  else
    {
      float scale_x = viewport->size.width / width;
      float scale_y = viewport->size.height / height;
      gsk_gpu_clip_init_empty (&self->clip,
                               &GRAPHENE_RECT_INIT (
                                   scale_x * clip->x,
                                   scale_y * clip->y,
                                   scale_x * clip->width,
                                   scale_y * clip->height
                               ));
    }

  self->modelview = NULL;
  gsk_gpu_image_get_projection_matrix (target, &self->projection);
  graphene_vec2_init (&self->scale,
                      width / viewport->size.width,
                      height / viewport->size.height);
  self->offset = GRAPHENE_POINT_INIT (-viewport->origin.x,
                                      -viewport->origin.y);
  self->opacity = 1.0;
  self->pending_globals = GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND;
}

static void
gsk_gpu_node_processor_emit_globals_op (GskGpuNodeProcessor *self)
{
  graphene_matrix_t mvp;

  if (self->modelview)
    {
      gsk_transform_to_matrix (self->modelview, &mvp);
      graphene_matrix_multiply (&mvp, &self->projection, &mvp);
    }
  else
    graphene_matrix_init_from_matrix (&mvp, &self->projection);

  gsk_gpu_globals_op (self->frame,
                      &self->scale,
                      &mvp,
                      &self->clip.rect);

  self->pending_globals &= ~(GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP);
}

static void
gsk_gpu_node_processor_emit_scissor_op (GskGpuNodeProcessor *self)
{
  gsk_gpu_scissor_op (self->frame,
                      &self->scissor);
  self->pending_globals &= ~GSK_GPU_GLOBAL_SCISSOR;
}

static void
gsk_gpu_node_processor_emit_blend_op (GskGpuNodeProcessor *self)
{
  gsk_gpu_blend_op (self->frame, self->blend);
  self->pending_globals &= ~GSK_GPU_GLOBAL_BLEND;
}

static void
gsk_gpu_node_processor_sync_globals (GskGpuNodeProcessor *self,
                                     GskGpuGlobals        ignored)
{
  GskGpuGlobals required;

  required = self->pending_globals & ~ignored;

  if (required & (GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP))
    gsk_gpu_node_processor_emit_globals_op (self);
  if (required & GSK_GPU_GLOBAL_SCISSOR)
    gsk_gpu_node_processor_emit_scissor_op (self);
  if (required & GSK_GPU_GLOBAL_BLEND)
    gsk_gpu_node_processor_emit_blend_op (self);
}

static guint32
gsk_gpu_node_processor_add_image (GskGpuNodeProcessor *self,
                                  GskGpuImage         *image,
                                  GskGpuSampler        sampler)
{
  guint32 descriptor;

  if (self->desc != NULL)
    {
      if (gsk_gpu_descriptors_add_image (self->desc, image, sampler, &descriptor))
        return descriptor;

      g_object_unref (self->desc);
    }

  self->desc = gsk_gpu_frame_create_descriptors (self->frame);

  if (!gsk_gpu_descriptors_add_image (self->desc, image, sampler, &descriptor))
    {
      g_assert_not_reached ();
      return 0;
    }
  
  return descriptor;
}

static void
gsk_gpu_node_processor_add_images (GskGpuNodeProcessor *self,
                                   gsize                n_images,
                                   GskGpuImage        **images,
                                   GskGpuSampler       *samplers,
                                   guint32             *out_descriptors)
{
  GskGpuDescriptors *desc;
  gsize i;

  g_assert (n_images > 0);

  /* note: This will infloop if more images are requested than can fit into an empty descriptors,
   * so don't do that. */
  do
    {
      out_descriptors[0] = gsk_gpu_node_processor_add_image (self, images[0], samplers[0]);
      desc = self->desc;

      for (i = 1; i < n_images; i++)
        {
          out_descriptors[i] = gsk_gpu_node_processor_add_image (self, images[i], samplers[i]);
          if (desc != self->desc)
            break;
        }
    }
  while (desc != self->desc);
}

static void
rect_round_to_pixels (const graphene_rect_t  *src,
                      const graphene_vec2_t  *pixel_scale,
                      const graphene_point_t *pixel_offset,
                      graphene_rect_t        *dest)
{
  float x, y, xscale, yscale, inv_xscale, inv_yscale;

  xscale = graphene_vec2_get_x (pixel_scale);
  yscale = graphene_vec2_get_y (pixel_scale);
  inv_xscale = 1.0f / xscale;
  inv_yscale = 1.0f / yscale;

  x = floorf ((src->origin.x + pixel_offset->x) * xscale);
  y = floorf ((src->origin.y + pixel_offset->y) * yscale);
  *dest = GRAPHENE_RECT_INIT (
      x * inv_xscale - pixel_offset->x,
      y * inv_yscale - pixel_offset->y,
      (ceilf ((src->origin.x + pixel_offset->x + src->size.width) * xscale) - x) * inv_xscale,
      (ceilf ((src->origin.y + pixel_offset->y + src->size.height) * yscale) - y) * inv_yscale);
}

static GskGpuImage *
gsk_gpu_node_processor_init_draw (GskGpuNodeProcessor   *self,
                                  GskGpuFrame           *frame,
                                  GdkMemoryDepth         depth,
                                  const graphene_vec2_t *scale,
                                  const graphene_rect_t *viewport)
{
  GskGpuImage *image;
  cairo_rectangle_int_t area;

  area.x = 0;
  area.y = 0;
  area.width = MAX (1, ceilf (graphene_vec2_get_x (scale) * viewport->size.width - EPSILON));
  area.height = MAX (1, ceilf (graphene_vec2_get_y (scale) * viewport->size.height - EPSILON));

  image = gsk_gpu_device_create_offscreen_image (gsk_gpu_frame_get_device (frame),
                                                 FALSE,
                                                 depth,
                                                 area.width, area.height);
  if (image == NULL)
    return NULL;

  gsk_gpu_node_processor_init (self,
                               frame,
                               NULL,
                               image,
                               &area,
                               viewport);

  gsk_gpu_render_pass_begin_op (frame,
                                image,
                                &area,
                                GSK_RENDER_PASS_OFFSCREEN);

  return image;
}

static void
gsk_gpu_node_processor_finish_draw (GskGpuNodeProcessor *self,
                                    GskGpuImage         *image)
{
  gsk_gpu_render_pass_end_op (self->frame,
                              image,
                              GSK_RENDER_PASS_OFFSCREEN);

  gsk_gpu_node_processor_finish (self);
}

void
gsk_gpu_node_processor_process (GskGpuFrame                 *frame,
                                GskGpuImage                 *target,
                                const cairo_rectangle_int_t *clip,
                                GskRenderNode               *node,
                                const graphene_rect_t       *viewport)
{
  GskGpuNodeProcessor self;

  gsk_gpu_node_processor_init (&self,
                               frame,
                               NULL,
                               target,
                               clip,
                               viewport);

  gsk_gpu_node_processor_add_node (&self, node);

  gsk_gpu_node_processor_finish (&self);
}

static void
gsk_gpu_pattern_writer_init (GskGpuPatternWriter    *self,
                             GskGpuFrame            *frame,
                             const graphene_vec2_t  *scale,
                             const graphene_point_t *offset,
                             const graphene_rect_t  *bounds)
{
  self->frame = frame;
  self->desc = NULL;
  self->bounds = GRAPHENE_RECT_INIT (bounds->origin.x + offset->x,
                                     bounds->origin.y + offset->y,
                                     bounds->size.width,
                                     bounds->size.height);
  self->offset = *offset;
  self->scale = *scale;
  self->stack = 0;

  pattern_buffer_init (&self->buffer);
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

static void
gsk_gpu_pattern_writer_append (GskGpuPatternWriter *self,
                               gsize                align,
                               const guchar        *data,
                               gsize                size)
{
  pattern_buffer_set_size (&self->buffer, round_up (pattern_buffer_get_size (&self->buffer), align));
  
  pattern_buffer_splice (&self->buffer, 
                         pattern_buffer_get_size (&self->buffer),
                         0,
                         FALSE,
                         data,
                         size);
}

static void
gsk_gpu_pattern_writer_append_float (GskGpuPatternWriter *self,
                                     float                f)
{
  gsk_gpu_pattern_writer_append (self, G_ALIGNOF (float), (guchar *) &f, sizeof (float));
}

static void
gsk_gpu_pattern_writer_append_uint (GskGpuPatternWriter *self,
                                    guint32              u)
{
  gsk_gpu_pattern_writer_append (self, G_ALIGNOF (guint32), (guchar *) &u, sizeof (guint32));
}

static void
gsk_gpu_pattern_writer_append_matrix (GskGpuPatternWriter     *self,
                                      const graphene_matrix_t *matrix)
{
  float f[16];

  graphene_matrix_to_float (matrix, f);

  gsk_gpu_pattern_writer_append (self, G_ALIGNOF (float), (guchar *) f, sizeof (f));
}

static void
gsk_gpu_pattern_writer_append_vec4 (GskGpuPatternWriter   *self,
                                    const graphene_vec4_t *vec4)
{
  float f[4];

  graphene_vec4_to_float (vec4, f);

  gsk_gpu_pattern_writer_append (self, G_ALIGNOF (float), (guchar *) f, sizeof (f));
}

static void
gsk_gpu_pattern_writer_append_point (GskGpuPatternWriter    *self,
                                     const graphene_point_t *point,
                                     const graphene_point_t *offset)
{
  float f[2];

  f[0] = point->x + offset->x;
  f[1] = point->y + offset->y;

  gsk_gpu_pattern_writer_append (self, G_ALIGNOF (float), (guchar *) f, sizeof (f));
}

static void
gsk_gpu_pattern_writer_append_rect (GskGpuPatternWriter    *self,
                                    const graphene_rect_t  *rect,
                                    const graphene_point_t *offset)
{
  float f[4];

  gsk_gpu_rect_to_float (rect, offset, f);

  gsk_gpu_pattern_writer_append (self, G_ALIGNOF (float), (guchar *) f, sizeof (f));
}

static void
gsk_gpu_pattern_writer_append_rgba (GskGpuPatternWriter *self,
                                    const GdkRGBA       *rgba)
{
  float f[4] = { rgba->red, rgba->green, rgba->blue, rgba->alpha };

  gsk_gpu_pattern_writer_append (self, G_ALIGNOF (float), (guchar *) f, sizeof (f));
}

static void
gsk_gpu_pattern_writer_append_color_stops (GskGpuPatternWriter *self,
                                           const GskColorStop  *stops,
                                           gsize                n_stops)
{
  gsk_gpu_pattern_writer_append_uint (self, n_stops);
  gsk_gpu_pattern_writer_append (self, G_ALIGNOF (float), (guchar *) stops, sizeof (GskColorStop) * n_stops);
}

static gboolean
gsk_gpu_pattern_writer_push_stack (GskGpuPatternWriter *self)
{
  if (self->stack >= GSK_GPU_PATTERN_STACK_SIZE)
    return FALSE;

  self->stack++;
  return TRUE;
}

static void
gsk_gpu_pattern_writer_pop_stack (GskGpuPatternWriter *self)
{
  g_assert (self->stack > 0);
  self->stack--;
}

static void
gsk_gpu_pattern_writer_finish (GskGpuPatternWriter *self)
{
  pattern_buffer_clear (&self->buffer);
  g_assert (self->stack == 0);
  g_clear_object (&self->desc);
}

static gboolean
gsk_gpu_pattern_writer_add_image (GskGpuPatternWriter *self,
                                  GskGpuImage         *image,
                                  GskGpuSampler        sampler,
                                  guint32             *out_descriptor)
{
  if (self->desc == NULL)
    self->desc = gsk_gpu_frame_create_descriptors (self->frame);

  return gsk_gpu_descriptors_add_image (self->desc, image, sampler, out_descriptor);
}

static void
extract_scale_from_transform (GskTransform *transform,
                              float        *out_scale_x,
                              float        *out_scale_y)
{
  switch (gsk_transform_get_category (transform))
    {
    default:
      g_assert_not_reached ();
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      *out_scale_x = 1.0f;
      *out_scale_y = 1.0f;
      return;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float scale_x, scale_y, dx, dy;
        gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);
        *out_scale_x = fabs (scale_x);
        *out_scale_y = fabs (scale_y);
      }
      return;

    case GSK_TRANSFORM_CATEGORY_2D:
      {
        float skew_x, skew_y, scale_x, scale_y, angle, dx, dy;
        gsk_transform_to_2d_components (transform,
                                        &skew_x, &skew_y,
                                        &scale_x, &scale_y,
                                        &angle,
                                        &dx, &dy);
        *out_scale_x = fabs (scale_x);
        *out_scale_y = fabs (scale_y);
      }
      return;

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
      {
        graphene_quaternion_t rotation;
        graphene_matrix_t matrix;
        graphene_vec4_t perspective;
        graphene_vec3_t translation;
        graphene_vec3_t matrix_scale;
        graphene_vec3_t shear;

        gsk_transform_to_matrix (transform, &matrix);
        graphene_matrix_decompose (&matrix,
                                   &translation,
                                   &matrix_scale,
                                   &rotation,
                                   &shear,
                                   &perspective);

        *out_scale_x = fabs (graphene_vec3_get_x (&matrix_scale));
        *out_scale_y = fabs (graphene_vec3_get_y (&matrix_scale));
      }
      return;
    }
}

static gboolean
gsk_gpu_node_processor_rect_is_integer (GskGpuNodeProcessor   *self,
                                        const graphene_rect_t *rect,
                                        cairo_rectangle_int_t *int_rect)
{
  graphene_rect_t transformed_rect;
  float scale_x = graphene_vec2_get_x (&self->scale);
  float scale_y = graphene_vec2_get_y (&self->scale);

  switch (gsk_transform_get_category (self->modelview))
    {
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
      /* FIXME: We could try to handle 90° rotation here,
       * but I don't think there's a use case */
      return FALSE;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      gsk_transform_transform_bounds (self->modelview, rect, &transformed_rect);
      rect = &transformed_rect;
      break;

    case GSK_TRANSFORM_CATEGORY_IDENTITY:
    default:
      break;
    }

  int_rect->x = rect->origin.x * scale_x;
  int_rect->y = rect->origin.y * scale_y;
  int_rect->width = rect->size.width * scale_x;
  int_rect->height = rect->size.height * scale_y;

  return int_rect->x == rect->origin.x * scale_x
      && int_rect->y == rect->origin.y * scale_y
      && int_rect->width == rect->size.width * scale_x
      && int_rect->height == rect->size.height * scale_y;
}

static void
gsk_gpu_node_processor_get_clip_bounds (GskGpuNodeProcessor *self,
                                        graphene_rect_t     *out_bounds)
{
  graphene_rect_offset_r (&self->clip.rect.bounds,
                          - self->offset.x,
                          - self->offset.y,
                          out_bounds);
 
  /* FIXME: We could try the scissor rect here.
   * But how often is that smaller than the clip bounds?
   */
}
 
static gboolean G_GNUC_WARN_UNUSED_RESULT
gsk_gpu_node_processor_clip_node_bounds (GskGpuNodeProcessor *self,
                                         GskRenderNode       *node,
                                         graphene_rect_t     *out_bounds)
{
  graphene_rect_t tmp;

  gsk_gpu_node_processor_get_clip_bounds (self, &tmp);
  
  if (!gsk_rect_intersection (&tmp, &node->bounds, out_bounds))
    return FALSE;

  return TRUE;
}

static void
gsk_gpu_node_processor_image_op (GskGpuNodeProcessor   *self,
                                 GskGpuImage           *image,
                                 const graphene_rect_t *rect,
                                 const graphene_rect_t *tex_rect)
{
  guint32 descriptor;

  g_assert (self->pending_globals == 0);

  descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_DEFAULT);

  if (gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_STRAIGHT_ALPHA)
    {
      gsk_gpu_straight_alpha_op (self->frame,
                                 gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, rect),
                                 self->opacity,
                                 self->desc,
                                 descriptor,
                                 rect,
                                 &self->offset,
                                 tex_rect);
    }
  else if (self->opacity < 1.0)
    {
      gsk_gpu_color_matrix_op_opacity (self->frame,
                                       gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, rect),
                                       self->desc,
                                       descriptor,
                                       rect,
                                       &self->offset,
                                       tex_rect,
                                       self->opacity);
    }
  else
    {
      gsk_gpu_texture_op (self->frame,
                          gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, rect),
                          self->desc,
                          descriptor,
                          rect,
                          &self->offset,
                          tex_rect);
    }
}

static GskGpuImage *
gsk_gpu_node_processor_create_offscreen (GskGpuFrame           *frame,
                                         const graphene_vec2_t *scale,
                                         const graphene_rect_t *viewport,
                                         GskRenderNode         *node)
{
  GskGpuNodeProcessor self;
  GskGpuImage *image;

  image = gsk_gpu_node_processor_init_draw (&self,
                                            frame,
                                            gsk_render_node_get_preferred_depth (node),
                                            scale,
                                            viewport);
  if (image == NULL)
    return NULL;

  gsk_gpu_node_processor_add_node (&self, node);

  gsk_gpu_node_processor_finish_draw (&self, image);

  return image;
}

/*
 * gsk_gpu_get_node_as_image:
 * @frame: frame to render in
 * @clip_bounds: region of node that must be included in image
 * @scale: scale factor to use for the image
 * @node: the node to render
 * @out_bounds: the actual bounds of the result
 *
 * Get the part of the node indicated by the clip bounds as an image.
 *
 * It is perfectly valid for this function to return an image covering
 * a larger or smaller rectangle than the given clip bounds.
 * It can be smaller if the node is actually smaller than the clip
 * bounds and it's not necessary to create such a large offscreen, and
 * it can be larger if only part of a node is drawn but a cached image
 * for the full node (usually a texture node) already exists.
 *
 * The rectangle that is actually covered by the image is returned in
 * out_bounds.
 *
 * Returns: the image or %NULL if there was nothing to render
 **/
static GskGpuImage *
gsk_gpu_get_node_as_image (GskGpuFrame            *frame,
                           const graphene_rect_t  *clip_bounds,
                           const graphene_vec2_t  *scale,
                           GskRenderNode          *node,
                           graphene_rect_t        *out_bounds)
{
  GskGpuImage *result;

  switch ((guint) gsk_render_node_get_node_type (node))
    {
    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = gsk_texture_node_get_texture (node);
        GskGpuDevice *device = gsk_gpu_frame_get_device (frame);
        gint64 timestamp = gsk_gpu_frame_get_timestamp (frame);
        result = gsk_gpu_device_lookup_texture_image (device, texture, timestamp);
        if (result == NULL)
          result = gsk_gpu_frame_upload_texture (frame, FALSE, texture);

        if (result)
          {
            *out_bounds = node->bounds;
            return result;
          }
        break;
      }

    case GSK_CAIRO_NODE:
      result = gsk_gpu_upload_cairo_op (frame,
                                        scale,
                                        clip_bounds,
                                        (GskGpuCairoFunc) gsk_render_node_draw_fallback,
                                        gsk_render_node_ref (node),
                                        (GDestroyNotify) gsk_render_node_unref);

      g_object_ref (result);

      *out_bounds = *clip_bounds;
      return result;

    default:
      break;
    }

  GSK_DEBUG (FALLBACK, "Offscreening node '%s'", g_type_name_from_instance ((GTypeInstance *) node));
  result = gsk_gpu_node_processor_create_offscreen (frame,
                                                    scale,
                                                    clip_bounds,
                                                    node);

  *out_bounds = *clip_bounds;
  return result;
}

static GskGpuImage *
gsk_gpu_node_processor_ensure_image (GskGpuFrame      *frame,
                                     GskGpuImage      *image,
                                     GskGpuImageFlags  required_flags,
                                     GskGpuImageFlags  disallowed_flags)
{
  GskGpuImageFlags flags, missing_flags;
  GskGpuImage *copy;
  gsize width, height;

  g_assert ((required_flags & disallowed_flags) == 0);
  g_assert ((required_flags & (GSK_GPU_IMAGE_EXTERNAL | GSK_GPU_IMAGE_STRAIGHT_ALPHA | GSK_GPU_IMAGE_NO_BLIT)) == 0);

  flags = gsk_gpu_image_get_flags (image);
  missing_flags = required_flags & ~flags;
  if ((flags & disallowed_flags) == 0)
    {
      if (missing_flags == 0)
        return image;

      if (missing_flags == GSK_GPU_IMAGE_MIPMAP &&
          (flags & GSK_GPU_IMAGE_CAN_MIPMAP))
        {
          gsk_gpu_mipmap_op (frame, image);
          return image;
        }
    }

  width = gsk_gpu_image_get_width (image);
  height = gsk_gpu_image_get_height (image);

  copy = gsk_gpu_device_create_offscreen_image (gsk_gpu_frame_get_device (frame),
                                                required_flags & (GSK_GPU_IMAGE_CAN_MIPMAP | GSK_GPU_IMAGE_MIPMAP) ? TRUE : FALSE,
                                                gdk_memory_format_get_depth (gsk_gpu_image_get_format (image)),
                                                width, height);

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_BLIT) &&
      (flags & (GSK_GPU_IMAGE_NO_BLIT | GSK_GPU_IMAGE_STRAIGHT_ALPHA | GSK_GPU_IMAGE_FILTERABLE)) == GSK_GPU_IMAGE_FILTERABLE)
    {
      gsk_gpu_blit_op (frame,
                       image,
                       copy,
                       &(cairo_rectangle_int_t) { 0, 0, width, height },
                       &(cairo_rectangle_int_t) { 0, 0, width, height },
                       GSK_GPU_BLIT_NEAREST);
    }
  else
    {
      GskGpuNodeProcessor other;
      graphene_rect_t rect = GRAPHENE_RECT_INIT (0, 0, width, height);

      gsk_gpu_node_processor_init (&other,
                                   frame,
                                   NULL,
                                   copy,
                                   &(cairo_rectangle_int_t) { 0, 0, width, height },
                                   &rect);

      gsk_gpu_render_pass_begin_op (other.frame,
                                    copy,
                                    &(cairo_rectangle_int_t) { 0, 0, width, height },
                                    GSK_RENDER_PASS_OFFSCREEN);

      gsk_gpu_node_processor_sync_globals (&other, 0);

      gsk_gpu_node_processor_image_op (&other,
                                       image,
                                       &rect,
                                       &rect);

      gsk_gpu_render_pass_end_op (other.frame,
                                  copy,
                                  GSK_RENDER_PASS_OFFSCREEN);

      gsk_gpu_node_processor_finish (&other);
    }

  if (required_flags & GSK_GPU_IMAGE_MIPMAP)
    gsk_gpu_mipmap_op (frame, copy);

  g_object_unref (image);

  return copy;
}

/*
 * gsk_gpu_node_processor_get_node_as_image:
 * @self: a node processor
 * @required_flags: flags that the resulting image must have
 * @disallowed_flags: flags that the resulting image must NOT have
 * @clip_bounds: (nullable): clip rectangle to use or NULL to use
 *   the current clip
 * @node: the node to turn into an image
 * @out_bounds: bounds of the the image in node space
 *
 * Generates an image for the given node that conforms to the required flags
 * and does not contain the disallowed flags. The image is restricted to the
 * region in the clip bounds.
 *
 * Returns: (nullable): The node as an image or %NULL if the node is fully
 *     clipped
 **/
static GskGpuImage *
gsk_gpu_node_processor_get_node_as_image (GskGpuNodeProcessor   *self,
                                          GskGpuImageFlags       required_flags,
                                          GskGpuImageFlags       disallowed_flags,
                                          const graphene_rect_t *clip_bounds,
                                          GskRenderNode         *node,
                                          graphene_rect_t       *out_bounds)
{
  GskGpuImage *image, *ensure;
  graphene_rect_t clip;

  if (clip_bounds == NULL)
    {
      if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clip))
        return NULL;
    }
  else
    {
      if (!gsk_rect_intersection (clip_bounds, &node->bounds, &clip))
        return NULL;
    }
  rect_round_to_pixels (&clip, &self->scale, &self->offset, &clip);

  image = gsk_gpu_get_node_as_image (self->frame,
                                     &clip,
                                     &self->scale,
                                     node,
                                     out_bounds);
  if (image == NULL)
    return NULL;

  ensure = gsk_gpu_node_processor_ensure_image (self->frame,
                                                image,
                                                required_flags,
                                                disallowed_flags);

  /* if we fixed up a cached texture, cache the fixed up version instead */
  if (ensure != image && disallowed_flags &&
      gsk_render_node_get_node_type (node) == GSK_TEXTURE_NODE)
    {
      gsk_gpu_device_cache_texture_image (gsk_gpu_frame_get_device (self->frame),
                                          gsk_texture_node_get_texture (node),
                                          gsk_gpu_frame_get_timestamp (self->frame),
                                          ensure);
    }

  return ensure;
}

static void
gsk_gpu_node_processor_blur_op (GskGpuNodeProcessor       *self,
                                const graphene_rect_t     *rect,
                                const graphene_point_t    *shadow_offset,
                                float                      blur_radius,
                                const GdkRGBA             *shadow_color,
                                GskGpuDescriptors         *source_desc,
                                guint32                    source_descriptor,
                                GdkMemoryDepth             source_depth,
                                const graphene_rect_t     *source_rect)
{
  GskGpuNodeProcessor other;
  GskGpuImage *intermediate;
  guint32 intermediate_descriptor;
  graphene_vec2_t direction;
  graphene_rect_t clip_rect, intermediate_rect;
  graphene_point_t real_offset;
  float clip_radius;

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius / 2.0);

  /* FIXME: Handle clip radius growing the clip too much */
  gsk_gpu_node_processor_get_clip_bounds (self, &clip_rect);
  clip_rect.origin.x -= shadow_offset->x;
  clip_rect.origin.y -= shadow_offset->y;
  graphene_rect_inset (&clip_rect, 0.f, -clip_radius);
  if (!gsk_rect_intersection (rect, &clip_rect, &intermediate_rect))
    return;

  rect_round_to_pixels (&intermediate_rect, &self->scale, &self->offset, &intermediate_rect);

  intermediate = gsk_gpu_node_processor_init_draw (&other,
                                                   self->frame,
                                                   source_depth,
                                                   &self->scale,
                                                   &intermediate_rect);

  gsk_gpu_node_processor_sync_globals (&other, 0);

  graphene_vec2_init (&direction, blur_radius, 0.0f);
  gsk_gpu_blur_op (other.frame,
                   gsk_gpu_clip_get_shader_clip (&other.clip, &other.offset, &intermediate_rect),
                   source_desc,
                   source_descriptor,
                   &intermediate_rect,
                   &other.offset,
                   source_rect,
                   &direction);

  gsk_gpu_node_processor_finish_draw (&other, intermediate);

  real_offset = GRAPHENE_POINT_INIT (self->offset.x + shadow_offset->x,
                                     self->offset.y + shadow_offset->y);
  graphene_vec2_init (&direction, 0.0f, blur_radius);
  intermediate_descriptor = gsk_gpu_node_processor_add_image (self, intermediate, GSK_GPU_SAMPLER_TRANSPARENT);
  if (shadow_color)
    {
      gsk_gpu_blur_shadow_op (self->frame,
                              gsk_gpu_clip_get_shader_clip (&self->clip, &real_offset, rect),
                              self->desc,
                              intermediate_descriptor,
                              rect,
                              &real_offset,
                              &intermediate_rect,
                              &direction,
                              shadow_color);
    }
  else
    {
      gsk_gpu_blur_op (self->frame,
                       gsk_gpu_clip_get_shader_clip (&self->clip, &real_offset, rect),
                       self->desc,
                       intermediate_descriptor,
                       rect,
                       &real_offset,
                       &intermediate_rect,
                       &direction);
    }

  g_object_unref (intermediate);
}

static void
gsk_gpu_node_processor_add_fallback_node (GskGpuNodeProcessor *self,
                                          GskRenderNode       *node)
{
  GskGpuImage *image;
  graphene_rect_t clipped_bounds;

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clipped_bounds))
    return;

  rect_round_to_pixels (&clipped_bounds, &self->scale, &self->offset, &clipped_bounds);

  gsk_gpu_node_processor_sync_globals (self, 0);

  image = gsk_gpu_upload_cairo_op (self->frame,
                                   &self->scale,
                                   &clipped_bounds,
                                   (GskGpuCairoFunc) gsk_render_node_draw_fallback,
                                   gsk_render_node_ref (node),
                                   (GDestroyNotify) gsk_render_node_unref);

  gsk_gpu_node_processor_image_op (self,
                                   image,
                                   &node->bounds,
                                   &clipped_bounds);
}

static gboolean
gsk_gpu_node_processor_repeat_node_can_ubershader (GskRenderNode *node)
{
  const graphene_rect_t *child_bounds;

  child_bounds = gsk_repeat_node_get_child_bounds (node);

  if (child_bounds->size.width > node->bounds.size.width ||
      child_bounds->size.height > node->bounds.size.height)
    return FALSE;

  return TRUE;
}

/* To be called when code wants to run a shader which
 * would require rendering @node into an offscreen, but it
 * could also run an ubershader.
 */
static gboolean
gsk_gpu_node_processor_ubershader_instead_of_offscreen (GskGpuNodeProcessor *self,
                                                        GskRenderNode       *node)
{
  if (!gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_UBER))
    return FALSE;

  for (;;)
    {
      switch (gsk_render_node_get_node_type (node))
        {
        case GSK_TRANSFORM_NODE:
          node = gsk_transform_node_get_child (node);
          break;

        case GSK_CLIP_NODE:
          node = gsk_clip_node_get_child (node);
          break;

        case GSK_OPACITY_NODE:
          node = gsk_opacity_node_get_child (node);
          break;

        case GSK_DEBUG_NODE:
          node = gsk_debug_node_get_child (node);
          break;

        case GSK_SUBSURFACE_NODE:
          node = gsk_subsurface_node_get_child (node);
          break;

        case GSK_REPEAT_NODE:
          return gsk_gpu_node_processor_repeat_node_can_ubershader (node);

        case GSK_COLOR_NODE:
        case GSK_LINEAR_GRADIENT_NODE:
        case GSK_REPEATING_LINEAR_GRADIENT_NODE:
        case GSK_RADIAL_GRADIENT_NODE:
        case GSK_REPEATING_RADIAL_GRADIENT_NODE:
        case GSK_CONIC_GRADIENT_NODE:
        case GSK_ROUNDED_CLIP_NODE:
        case GSK_COLOR_MATRIX_NODE:
        case GSK_CROSS_FADE_NODE:
        case GSK_BLEND_NODE:
        case GSK_TEXT_NODE:
        case GSK_MASK_NODE:
          return TRUE;

        case GSK_CONTAINER_NODE:
        case GSK_CAIRO_NODE:
        case GSK_BORDER_NODE:
        case GSK_TEXTURE_NODE:
        case GSK_INSET_SHADOW_NODE:
        case GSK_OUTSET_SHADOW_NODE:
        case GSK_SHADOW_NODE:
        case GSK_BLUR_NODE:
        case GSK_GL_SHADER_NODE:
        case GSK_TEXTURE_SCALE_NODE:
        case GSK_FILL_NODE:
        case GSK_STROKE_NODE:
          return FALSE;

        case GSK_NOT_A_RENDER_NODE:
        default:
          g_return_val_if_reached (FALSE);
        }
    }
}

static gboolean
gsk_gpu_node_processor_try_node_as_pattern (GskGpuNodeProcessor *self,
                                            GskRenderNode       *node)
{
  GskGpuPatternWriter writer;
  graphene_rect_t clipped;
  GskGpuBuffer *buffer;
  gsize offset;
  guint32 pattern_id;
 
  g_assert (self->pending_globals == 0);

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clipped))
    return TRUE;

  gsk_gpu_pattern_writer_init (&writer,
                               self->frame,
                               &self->scale,
                               &self->offset,
                               &clipped);
 
  if (!gsk_gpu_node_processor_create_node_pattern (&writer, node))
    {
      gsk_gpu_pattern_writer_finish (&writer);
      return FALSE;
    }

  if (self->opacity < 1.0)
    {
      gsk_gpu_pattern_writer_append_uint (&writer, GSK_GPU_PATTERN_OPACITY);
      gsk_gpu_pattern_writer_append_float (&writer, self->opacity);
    }

  gsk_gpu_pattern_writer_append_uint (&writer, GSK_GPU_PATTERN_DONE);

  buffer = gsk_gpu_frame_write_storage_buffer (self->frame,
                                               pattern_buffer_get_data (&writer.buffer),
                                               pattern_buffer_get_size (&writer.buffer),
                                               &offset);
  if (writer.desc == NULL)
    {
      if (self->desc == NULL)
        self->desc = gsk_gpu_frame_create_descriptors (self->frame);
      if (!gsk_gpu_descriptors_add_buffer (self->desc, buffer, &pattern_id))
        writer.desc = gsk_gpu_frame_create_descriptors (self->frame);
    }
  if (writer.desc &&
      !gsk_gpu_descriptors_add_buffer (writer.desc, buffer, &pattern_id))
    {
      g_assert_not_reached ();
    }

  pattern_id = (pattern_id << 22) | (offset / sizeof (float));

  gsk_gpu_uber_op (self->frame,
                   gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                   &node->bounds,
                   &self->offset,
                   writer.desc ? writer.desc : self->desc,
                   pattern_id);

  gsk_gpu_pattern_writer_finish (&writer);

  return TRUE;
}
 
static void
gsk_gpu_node_processor_add_without_opacity (GskGpuNodeProcessor *self,
                                            GskRenderNode       *node)
{
  GskGpuImage *image;
  guint32 descriptor;
  graphene_rect_t tex_rect;

  gsk_gpu_node_processor_sync_globals (self, 0);

  if (gsk_gpu_node_processor_ubershader_instead_of_offscreen (self, node) &&
      gsk_gpu_node_processor_try_node_as_pattern (self, node))
    return;

  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    0,
                                                    GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                    NULL,
                                                    node,
                                                    &tex_rect);
  if (image == NULL)
    return;

  descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_DEFAULT);
  
  gsk_gpu_color_matrix_op_opacity (self->frame,
                                   gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                                   self->desc,
                                   descriptor,
                                   &node->bounds,
                                   &self->offset,
                                   &tex_rect,
                                   self->opacity);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_node_clipped (GskGpuNodeProcessor   *self,
                                         GskRenderNode         *node,
                                         const graphene_rect_t *clip_bounds)
{
  GskGpuClip old_clip;
  graphene_rect_t clip;
  cairo_rectangle_int_t scissor;

  if (gsk_rect_contains_rect (clip_bounds, &node->bounds))
    {
      gsk_gpu_node_processor_add_node (self, node);
      return;
    }

  graphene_rect_offset_r (clip_bounds,
                          self->offset.x, self->offset.y,
                          &clip);

  gsk_gpu_clip_init_copy (&old_clip, &self->clip);

  /* Check if we can use scissoring for the clip */
  if (gsk_gpu_node_processor_rect_is_integer (self, &clip, &scissor))
    {
      cairo_rectangle_int_t old_scissor;

      if (!gdk_rectangle_intersect (&scissor, &self->scissor, &scissor))
        return;

      old_scissor = self->scissor;

      if (gsk_gpu_clip_intersect_rect (&self->clip, &old_clip, &clip))
        {
          if (self->clip.type == GSK_GPU_CLIP_ALL_CLIPPED)
            {
              gsk_gpu_clip_init_copy (&self->clip, &old_clip);
              return;
            }
          else if ((self->clip.type == GSK_GPU_CLIP_RECT || self->clip.type == GSK_GPU_CLIP_CONTAINED) &&
                   gsk_rect_contains_rect (&self->clip.rect.bounds, &clip))
            {
              self->clip.type = GSK_GPU_CLIP_NONE;
            }

          self->scissor = scissor;
          self->pending_globals |= GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_CLIP;

          gsk_gpu_node_processor_add_node (self, node);

          gsk_gpu_clip_init_copy (&self->clip, &old_clip);
          self->scissor = old_scissor;
          self->pending_globals |= GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_CLIP;
        }
      else
        {
          self->scissor = scissor;
          self->pending_globals |= GSK_GPU_GLOBAL_SCISSOR;

          gsk_gpu_clip_init_copy (&self->clip, &old_clip);

          gsk_gpu_node_processor_add_node (self, node);

          self->scissor = old_scissor;
          self->pending_globals |= GSK_GPU_GLOBAL_SCISSOR;
        }
    }
  else
    {
      if (!gsk_gpu_clip_intersect_rect (&self->clip, &old_clip, &clip))
        {
          gsk_gpu_clip_init_copy (&self->clip, &old_clip);
          gsk_gpu_node_processor_sync_globals (self, 0);
          if (!gsk_gpu_node_processor_ubershader_instead_of_offscreen (self, node) ||
              !gsk_gpu_node_processor_try_node_as_pattern (self, node))
            {
              GskGpuImage *image;
              graphene_rect_t bounds, tex_rect;
              if (gsk_gpu_node_processor_clip_node_bounds (self, node, &bounds) &&
                  gsk_rect_intersection (&bounds, clip_bounds, &bounds))
                image = gsk_gpu_node_processor_get_node_as_image (self,
                                                                  0,
                                                                  0,
                                                                  &bounds,
                                                                  node,
                                                                  &tex_rect);
              else
                image = NULL;
              if (image)
                {
                  gsk_gpu_node_processor_image_op (self,
                                                   image,
                                                   &bounds,
                                                   &tex_rect);
                  g_object_unref (image);
                }
            }
          return;
        }

      if (self->clip.type == GSK_GPU_CLIP_ALL_CLIPPED)
        {
          gsk_gpu_clip_init_copy (&self->clip, &old_clip);
          return;
        }

      self->pending_globals |= GSK_GPU_GLOBAL_CLIP;

      gsk_gpu_node_processor_add_node (self, node);

      gsk_gpu_clip_init_copy (&self->clip, &old_clip);
      self->pending_globals |= GSK_GPU_GLOBAL_CLIP;
    }
}

static void
gsk_gpu_node_processor_add_clip_node (GskGpuNodeProcessor *self,
                                      GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_node_clipped (self,
                                           gsk_clip_node_get_child (node),
                                           gsk_clip_node_get_clip (node));
}

static gboolean
gsk_gpu_node_processor_create_clip_pattern (GskGpuPatternWriter *self,
                                            GskRenderNode       *node)
{
  if (!gsk_gpu_node_processor_create_node_pattern (self, gsk_opacity_node_get_child (node)))
    return FALSE;

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_CLIP);
  gsk_gpu_pattern_writer_append_rect (self,
                                     gsk_clip_node_get_clip (node),
                                     &self->offset);

  return TRUE;
}

static void
gsk_gpu_node_processor_add_rounded_clip_node_with_mask (GskGpuNodeProcessor *self,
                                                        GskRenderNode       *node)
{
  GskGpuNodeProcessor other;
  graphene_rect_t clip_bounds, child_rect;
  GskGpuImage *child_image, *mask_image;
  guint32 descriptors[2];

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clip_bounds))
    return;
  rect_round_to_pixels (&clip_bounds, &self->scale, &self->offset, &clip_bounds);

  child_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                          0,
                                                          GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                          &clip_bounds,
                                                          gsk_rounded_clip_node_get_child (node),
                                                          &child_rect);
  if (child_image == NULL)
    return;

  mask_image = gsk_gpu_node_processor_init_draw (&other,
                                                 self->frame,
                                                 gsk_render_node_get_preferred_depth (node),
                                                 &self->scale,
                                                 &clip_bounds);
  gsk_gpu_node_processor_sync_globals (&other, 0);
  gsk_gpu_rounded_color_op (other.frame,
                            gsk_gpu_clip_get_shader_clip (&other.clip, &other.offset, &node->bounds),
                            gsk_rounded_clip_node_get_clip (node),
                            &other.offset,
                            &GDK_RGBA_WHITE);
  gsk_gpu_node_processor_finish_draw (&other, mask_image);

  gsk_gpu_node_processor_add_images (self,
                                     2,
                                     (GskGpuImage *[2]) { child_image, mask_image },
                                     (GskGpuSampler[2]) { GSK_GPU_SAMPLER_DEFAULT, GSK_GPU_SAMPLER_DEFAULT },
                                     descriptors);

  gsk_gpu_node_processor_sync_globals (self, 0);
  gsk_gpu_mask_op (self->frame,
                   gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &clip_bounds),
                   self->desc,
                   &clip_bounds,
                   &self->offset,
                   self->opacity,
                   GSK_MASK_MODE_ALPHA,
                   descriptors[0],
                   &child_rect,
                   descriptors[1],
                   &clip_bounds);

  g_object_unref (child_image);
  g_object_unref (mask_image);
}

static void
gsk_gpu_node_processor_add_rounded_clip_node (GskGpuNodeProcessor *self,
                                              GskRenderNode       *node)
{
  GskGpuClip old_clip;
  GskRoundedRect clip;
  const GskRoundedRect *original_clip;
  GskRenderNode *child;

  child = gsk_rounded_clip_node_get_child (node);
  original_clip = gsk_rounded_clip_node_get_clip (node);

  /* Common case for entries etc: rounded solid color background.
   * And we have a shader for that */
  if (gsk_render_node_get_node_type (child) == GSK_COLOR_NODE &&
      gsk_rect_contains_rect (&child->bounds, &original_clip->bounds))
    {
      const GdkRGBA *rgba = gsk_color_node_get_color (child);
      gsk_gpu_node_processor_sync_globals (self, 0);
      gsk_gpu_rounded_color_op (self->frame,
                                gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &original_clip->bounds),
                                original_clip,
                                &self->offset,
                                &GDK_RGBA_INIT_ALPHA (rgba, self->opacity));
      return;
    }

  gsk_gpu_clip_init_copy (&old_clip, &self->clip);

  clip = *original_clip;
  gsk_rounded_rect_offset (&clip, self->offset.x, self->offset.y);

  if (!gsk_gpu_clip_intersect_rounded_rect (&self->clip, &old_clip, &clip))
    {
      gsk_gpu_clip_init_copy (&self->clip, &old_clip);
      gsk_gpu_node_processor_add_rounded_clip_node_with_mask (self, node);
      return;
    }

  if (self->clip.type == GSK_GPU_CLIP_ALL_CLIPPED)
    {
      gsk_gpu_clip_init_copy (&self->clip, &old_clip);
      return;
    }

  self->pending_globals |= GSK_GPU_GLOBAL_CLIP;

  gsk_gpu_node_processor_add_node (self, gsk_rounded_clip_node_get_child (node));

  gsk_gpu_clip_init_copy (&self->clip, &old_clip);
  self->pending_globals |= GSK_GPU_GLOBAL_CLIP;
}

static void
gsk_gpu_node_processor_add_transform_node (GskGpuNodeProcessor *self,
                                           GskRenderNode       *node)
{
  GskRenderNode *child;
  GskTransform *transform;
  graphene_point_t old_offset;
  graphene_vec2_t old_scale;
  GskTransform *old_modelview;
  GskGpuClip old_clip;

  child = gsk_transform_node_get_child (node);
  transform = gsk_transform_node_get_transform (node);

  switch (gsk_transform_get_category (transform))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;
        gsk_transform_to_translate (transform, &dx, &dy);
        old_offset = self->offset;
        self->offset.x += dx;
        self->offset.y += dy;
        gsk_gpu_node_processor_add_node (self, child);
        self->offset = old_offset;
      }
      return;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float dx, dy, scale_x, scale_y;

        gsk_gpu_clip_init_copy (&old_clip, &self->clip);
        old_offset = self->offset;
        old_scale = self->scale;
        old_modelview = gsk_transform_ref (self->modelview);

        gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);
        gsk_gpu_clip_scale (&self->clip, &old_clip, scale_x, scale_y);
        self->offset.x = (self->offset.x + dx) / scale_x;
        self->offset.y = (self->offset.y + dy) / scale_y;
        graphene_vec2_init (&self->scale, fabs (scale_x), fabs (scale_y));
        graphene_vec2_multiply (&self->scale, &old_scale, &self->scale);
        self->modelview = gsk_transform_scale (self->modelview,
                                               scale_x / fabs (scale_x),
                                               scale_y / fabs (scale_y));
      }
      break;

    case GSK_TRANSFORM_CATEGORY_2D:
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
      {
        GskTransform *clip_transform;
        float scale_x, scale_y, old_pixels, new_pixels;

        clip_transform = gsk_transform_transform (gsk_transform_translate (NULL, &self->offset), transform);
        gsk_gpu_clip_init_copy (&old_clip, &self->clip);

        if (gsk_gpu_clip_contains_rect (&self->clip, &self->offset, &node->bounds))
          {
            gsk_gpu_clip_init_contained (&self->clip, &child->bounds);
          }
        else if (old_clip.type == GSK_GPU_CLIP_NONE)
          {
            GskTransform *inverse;
            graphene_rect_t new_bounds;
            inverse = gsk_transform_invert (gsk_transform_ref (clip_transform));
            gsk_transform_transform_bounds (inverse, &old_clip.rect.bounds, &new_bounds);
            gsk_transform_unref (inverse);
            gsk_gpu_clip_init_empty (&self->clip, &new_bounds);
          }
        else if (!gsk_gpu_clip_transform (&self->clip, &old_clip, clip_transform, &child->bounds))
          {
            GskGpuImage *image;
            graphene_rect_t tex_rect;
            gsk_transform_unref (clip_transform);
            /* This cannot loop because the next time we'll hit the branch above */
            gsk_gpu_node_processor_sync_globals (self, 0);
            image = gsk_gpu_node_processor_get_node_as_image (self,
                                                              0,
                                                              0,
                                                              NULL,
                                                              node,
                                                              &tex_rect);
            if (image != NULL)
              {
                gsk_gpu_node_processor_image_op (self,
                                                 image,
                                                 &node->bounds,
                                                 &tex_rect);
                g_object_unref (image);
              }
            return;
          }

        old_offset = self->offset;
        old_scale = self->scale;
        old_modelview = gsk_transform_ref (self->modelview);

        self->modelview = gsk_transform_scale (self->modelview,
                                               graphene_vec2_get_x (&self->scale),
                                               graphene_vec2_get_y (&self->scale));
        self->modelview = gsk_transform_transform (self->modelview, clip_transform);
        gsk_transform_unref (clip_transform);

        extract_scale_from_transform (self->modelview, &scale_x, &scale_y);

        old_pixels = MAX (graphene_vec2_get_x (&old_scale) * old_clip.rect.bounds.size.width,
                          graphene_vec2_get_y (&old_scale) * old_clip.rect.bounds.size.height);
        new_pixels = MAX (scale_x * self->clip.rect.bounds.size.width,
                          scale_y * self->clip.rect.bounds.size.height);

        /* Check that our offscreen doesn't get too big.  1.5 ~ sqrt(2) */
        if (new_pixels > 1.5 * old_pixels)
          {
            float forced_downscale = 2 * old_pixels / new_pixels;
            scale_x *= forced_downscale;
            scale_y *= forced_downscale;
          }

        self->modelview = gsk_transform_scale (self->modelview, 1 / scale_x, 1 / scale_y);
        graphene_vec2_init (&self->scale, scale_x, scale_y);
        self->offset = *graphene_point_zero ();
      }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  self->pending_globals |= GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;

  gsk_gpu_node_processor_add_node (self, child);

  self->offset = old_offset;
  self->scale = old_scale;
  gsk_transform_unref (self->modelview);
  self->modelview = old_modelview;
  gsk_gpu_clip_init_copy (&self->clip, &old_clip);
  self->pending_globals |= GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;
}

static gboolean
gsk_gpu_node_processor_create_transform_pattern (GskGpuPatternWriter *self,
                                                 GskRenderNode       *node)
{
  GskRenderNode *child;
  GskTransform *transform;
  graphene_point_t old_offset;
  graphene_vec2_t old_scale;
  graphene_rect_t old_bounds;
  gboolean result;

  child = gsk_transform_node_get_child (node);
  transform = gsk_transform_node_get_transform (node);
  old_offset = self->offset;
  old_scale = self->scale;
  old_bounds = self->bounds;

  switch (gsk_transform_get_category (transform))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      return gsk_gpu_node_processor_create_node_pattern (self, child);

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;
        gsk_transform_to_translate (transform, &dx, &dy);
        self->offset.x += dx;
        self->offset.y += dy;
        result = gsk_gpu_node_processor_create_node_pattern (self, child);
        self->offset = old_offset;
        return result;
      }

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float sx, sy, dx, dy, inv_sx, inv_sy;
        graphene_vec4_t vec4;
        if (!gsk_gpu_pattern_writer_push_stack (self))
          return FALSE;
        gsk_transform_to_affine (transform, &sx, &sy, &dx, &dy);
        inv_sx = 1.f / sx;
        inv_sy = 1.f / sy;
        gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_AFFINE);
        graphene_vec4_init (&vec4, self->offset.x + dx, self->offset.y + dy, inv_sx, inv_sy);
        gsk_gpu_pattern_writer_append_vec4 (self, &vec4);
        self->bounds.origin.x = (self->bounds.origin.x - self->offset.x - dx) * inv_sx;
        self->bounds.origin.y = (self->bounds.origin.y - self->offset.y - dy) * inv_sy;
        self->bounds.size.width *= inv_sx;
        self->bounds.size.height *= inv_sy;
        self->offset = GRAPHENE_POINT_INIT (0, 0);
        graphene_vec2_init (&self->scale, fabs (sx), fabs (sy));
        graphene_vec2_multiply (&self->scale, &old_scale, &self->scale);
      }
      break;

    case GSK_TRANSFORM_CATEGORY_2D:
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
      /* could add a mat4 operation here? */
      return FALSE;

    default:
      g_assert_not_reached ();
      return FALSE;
    }

  result = gsk_gpu_node_processor_create_node_pattern (self, child);

  if (result)
    gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_POSITION_POP);

  gsk_gpu_pattern_writer_pop_stack (self);
  self->scale = old_scale; 
  self->bounds = old_bounds; 
  self->offset = old_offset; 

  return result;
}

static void
gsk_gpu_node_processor_add_opacity_node (GskGpuNodeProcessor *self,
                                         GskRenderNode       *node)
{
  float old_opacity = self->opacity;

  self->opacity *= gsk_opacity_node_get_opacity (node);

  gsk_gpu_node_processor_add_node (self, gsk_opacity_node_get_child (node));

  self->opacity = old_opacity;
}

static void
gsk_gpu_node_processor_add_color_node (GskGpuNodeProcessor *self,
                                       GskRenderNode       *node)
{
  cairo_rectangle_int_t int_clipped;
  graphene_rect_t rect, clipped;
  const GdkRGBA *color;

  color = gsk_color_node_get_color (node);
  graphene_rect_offset_r (&node->bounds,
                          self->offset.x, self->offset.y,
                          &rect);
  gsk_rect_intersection (&self->clip.rect.bounds, &rect, &clipped);

  if (gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_CLEAR) &&
      gdk_rgba_is_opaque (color) &&
      self->opacity >= 1.0 &&
      node->bounds.size.width * node->bounds.size.height > 100 * 100 && /* not worth the effort for small images */
      gsk_gpu_node_processor_rect_is_integer (self, &clipped, &int_clipped))
    {
      /* now handle all the clip */
      if (!gdk_rectangle_intersect (&int_clipped, &self->scissor, &int_clipped))
        return;

      /* we have handled the bounds, now do the corners */
      if (self->clip.type == GSK_GPU_CLIP_ROUNDED)
        {
          graphene_rect_t cover;
          GskGpuShaderClip shader_clip;
          float scale_x, scale_y;

          if (self->modelview)
            {
              /* Yuck, rounded clip and modelview. I give up. */
              gsk_gpu_color_op (self->frame,
                                gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                                &node->bounds,
                                &self->offset,
                                gsk_color_node_get_color (node));
              return;
            }

          scale_x = graphene_vec2_get_x (&self->scale);
          scale_y = graphene_vec2_get_y (&self->scale);
          clipped = GRAPHENE_RECT_INIT (int_clipped.x / scale_x, int_clipped.y / scale_y,
                                        int_clipped.width / scale_x, int_clipped.height / scale_y);
          shader_clip = gsk_gpu_clip_get_shader_clip (&self->clip, graphene_point_zero(), &clipped);
          if (shader_clip != GSK_GPU_SHADER_CLIP_NONE)
            {
              gsk_rounded_rect_get_largest_cover (&self->clip.rect, &clipped, &cover);
              int_clipped.x = ceilf (cover.origin.x * scale_x);
              int_clipped.y = ceilf (cover.origin.y * scale_y);
              int_clipped.width = floorf ((cover.origin.x + cover.size.width) * scale_x) - int_clipped.x;
              int_clipped.height = floorf ((cover.origin.y + cover.size.height) * scale_y) - int_clipped.y;
              if (int_clipped.width == 0 || int_clipped.height == 0)
                {
                  gsk_gpu_color_op (self->frame,
                                    shader_clip,
                                    &clipped,
                                    graphene_point_zero (),
                                    color);
                  return;
                }
              cover = GRAPHENE_RECT_INIT (int_clipped.x / scale_x, int_clipped.y / scale_y,
                                          int_clipped.width / scale_x, int_clipped.height / scale_y);
              if (clipped.origin.x != cover.origin.x)
                gsk_gpu_color_op (self->frame,
                                  shader_clip,
                                  &GRAPHENE_RECT_INIT (clipped.origin.x, clipped.origin.y, cover.origin.x - clipped.origin.x, clipped.size.height),
                                  graphene_point_zero (),
                                  color);
              if (clipped.origin.y != cover.origin.y)
                gsk_gpu_color_op (self->frame,
                                  shader_clip,
                                  &GRAPHENE_RECT_INIT (clipped.origin.x, clipped.origin.y, clipped.size.width, cover.origin.y - clipped.origin.y),
                                  graphene_point_zero (),
                                  color);
              if (clipped.origin.x + clipped.size.width != cover.origin.x + cover.size.width)
                gsk_gpu_color_op (self->frame,
                                  shader_clip,
                                  &GRAPHENE_RECT_INIT (cover.origin.x + cover.size.width,
                                                       clipped.origin.y,
                                                       clipped.origin.x + clipped.size.width - cover.origin.x - cover.size.width,
                                                       clipped.size.height),
                                  graphene_point_zero (),
                                  color);
              if (clipped.origin.y + clipped.size.height != cover.origin.y + cover.size.height)
                gsk_gpu_color_op (self->frame,
                                  shader_clip,
                                  &GRAPHENE_RECT_INIT (clipped.origin.x,
                                                       cover.origin.y + cover.size.height,
                                                       clipped.size.width,
                                                       clipped.origin.y + clipped.size.height - cover.origin.y - cover.size.height),
                                  graphene_point_zero (),
                                  color);
            }
        }

      gsk_gpu_clear_op (self->frame,
                        &int_clipped,
                        color);
      return;
    }

  gsk_gpu_color_op (self->frame,
                    gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                    &node->bounds,
                    &self->offset,
                    &GDK_RGBA_INIT_ALPHA (color, self->opacity));
}

static gboolean
gsk_gpu_node_processor_create_color_pattern (GskGpuPatternWriter *self,
                                             GskRenderNode       *node)
{
  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_COLOR);
  gsk_gpu_pattern_writer_append_rgba (self, gsk_color_node_get_color (node));

  return TRUE;
}

static void
gsk_gpu_node_processor_add_border_node (GskGpuNodeProcessor *self,
                                        GskRenderNode       *node)
{
  GdkRGBA colors[4];
  gsize i;

  memcpy (colors, gsk_border_node_get_colors (node), sizeof (colors));
  for (i = 0; i < G_N_ELEMENTS (colors); i++)
    colors[i].alpha *= self->opacity;

  gsk_gpu_border_op (self->frame,
                     gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                     gsk_border_node_get_outline (node),
                     &self->offset,
                     graphene_point_zero (),
                     gsk_border_node_get_widths (node),
                     colors);
}

static void
gsk_gpu_node_processor_add_texture_node (GskGpuNodeProcessor *self,
                                         GskRenderNode       *node)
{
  GskGpuDevice *device;
  GskGpuImage *image;
  GdkTexture *texture;
  gint64 timestamp;

  device = gsk_gpu_frame_get_device (self->frame);
  texture = gsk_texture_node_get_texture (node);
  timestamp = gsk_gpu_frame_get_timestamp (self->frame);

  image = gsk_gpu_device_lookup_texture_image (device, texture, timestamp);
  if (image == NULL)
    {
      image = gsk_gpu_frame_upload_texture (self->frame, FALSE, texture);
      if (image == NULL)
        {
          GSK_DEBUG (FALLBACK, "Unsupported texture format %u for size %dx%d",
                     gdk_texture_get_format (texture),
                     gdk_texture_get_width (texture),
                     gdk_texture_get_height (texture));
          gsk_gpu_node_processor_add_fallback_node (self, node);
          return;
        }
    }

  if (gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_MIPMAP) &&
      (gdk_texture_get_width (texture) > 2 * node->bounds.size.width * graphene_vec2_get_x (&self->scale) ||
       gdk_texture_get_height (texture) > 2 * node->bounds.size.height * graphene_vec2_get_y (&self->scale)))
    {
      guint32 descriptor;

      image = gsk_gpu_node_processor_ensure_image (self->frame,
                                                   image,
                                                   GSK_GPU_IMAGE_MIPMAP,
                                                   GSK_GPU_IMAGE_STRAIGHT_ALPHA);
      descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_MIPMAP_DEFAULT);
      if (self->opacity < 1.0)
        {
          gsk_gpu_color_matrix_op_opacity (self->frame,
                                           gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                                           self->desc,
                                           descriptor,
                                           &node->bounds,
                                           &self->offset,
                                           &node->bounds,
                                           self->opacity);
        }
      else
        {
          gsk_gpu_texture_op (self->frame,
                              gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                              self->desc,
                              descriptor,
                              &node->bounds,
                              &self->offset,
                              &node->bounds);
        }
    }
  else
    {
      gsk_gpu_node_processor_image_op (self,
                                       image,
                                       &node->bounds,
                                       &node->bounds);
    }

  g_object_unref (image);
}

static gboolean
gsk_gpu_node_processor_create_texture_pattern (GskGpuPatternWriter *self,
                                               GskRenderNode       *node)
{
  GskGpuDevice *device;
  GdkTexture *texture;
  gint64 timestamp;
  guint32 descriptor;
  GskGpuImage *image;
  GskGpuSampler sampler;

  device = gsk_gpu_frame_get_device (self->frame);
  texture = gsk_texture_node_get_texture (node);
  timestamp = gsk_gpu_frame_get_timestamp (self->frame);

  image = gsk_gpu_device_lookup_texture_image (device, texture, timestamp);
  if (image == NULL)
    {
      image = gsk_gpu_frame_upload_texture (self->frame, FALSE, texture);
      if (image == NULL)
        return FALSE;
    }

  if (gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_MIPMAP) &&
      (gdk_texture_get_width (texture) > 2 * node->bounds.size.width * graphene_vec2_get_x (&self->scale) ||
       gdk_texture_get_height (texture) > 2 * node->bounds.size.height * graphene_vec2_get_y (&self->scale)))
    {
      image = gsk_gpu_node_processor_ensure_image (self->frame,
                                                   image,
                                                   GSK_GPU_IMAGE_MIPMAP,
                                                   GSK_GPU_IMAGE_STRAIGHT_ALPHA);
      sampler = GSK_GPU_SAMPLER_MIPMAP_DEFAULT;
    }
  else
    {
      sampler = GSK_GPU_SAMPLER_DEFAULT;
    }

  if (!gsk_gpu_pattern_writer_add_image (self, image, sampler, &descriptor))
    {
      g_object_unref (image);
      return FALSE;
    }

  if (gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_STRAIGHT_ALPHA)
    gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_STRAIGHT_ALPHA);
  else
    gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_TEXTURE);
  gsk_gpu_pattern_writer_append_uint (self, descriptor);
  gsk_gpu_pattern_writer_append_rect (self, &node->bounds, &self->offset);

  g_object_unref (image);

  return TRUE;
}

static void
gsk_gpu_node_processor_add_texture_scale_node (GskGpuNodeProcessor *self,
                                               GskRenderNode       *node)
{
  GskGpuDevice *device;
  GskGpuImage *image;
  GdkTexture *texture;
  GskScalingFilter scaling_filter;
  gint64 timestamp;
  guint32 descriptor;
  gboolean need_mipmap, need_offscreen;

  need_offscreen = self->modelview != NULL ||
            !graphene_vec2_equal (&self->scale, graphene_vec2_one ());
  if (need_offscreen)
    {
      GskGpuImage *offscreen;
      graphene_rect_t clip_bounds;

      gsk_gpu_node_processor_get_clip_bounds (self, &clip_bounds);
      /* first round to pixel boundaries, so we make sure the full pixels are covered */
      rect_round_to_pixels (&clip_bounds, &self->scale, &self->offset, &clip_bounds);
      /* then expand by half a pixel so that pixels needed for eventual linear
       * filtering are available */
      graphene_rect_inset (&clip_bounds, -0.5, -0.5);
      /* finally, round to full pixels */
      gsk_rect_round_larger (&clip_bounds);
      /* now intersect with actual node bounds */
      if (!gsk_rect_intersection (&clip_bounds, &node->bounds, &clip_bounds))
        return;
      clip_bounds.size.width = ceilf (clip_bounds.size.width);
      clip_bounds.size.height = ceilf (clip_bounds.size.height);
      offscreen = gsk_gpu_node_processor_create_offscreen (self->frame,
                                                           graphene_vec2_one (),
                                                           &clip_bounds,
                                                           node);
      descriptor = gsk_gpu_node_processor_add_image (self, offscreen, GSK_GPU_SAMPLER_DEFAULT);
      gsk_gpu_texture_op (self->frame,
                          gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                          self->desc,
                          descriptor,
                          &node->bounds,
                          &self->offset,
                          &clip_bounds);
      g_object_unref (offscreen);
      return;
    }

  device = gsk_gpu_frame_get_device (self->frame);
  texture = gsk_texture_scale_node_get_texture (node);
  scaling_filter = gsk_texture_scale_node_get_filter (node);
  timestamp = gsk_gpu_frame_get_timestamp (self->frame);
  need_mipmap = scaling_filter == GSK_SCALING_FILTER_TRILINEAR;

  image = gsk_gpu_device_lookup_texture_image (device, texture, timestamp);
  if (image == NULL)
    {
      image = gsk_gpu_frame_upload_texture (self->frame, need_mipmap, texture);
      if (image == NULL)
        {
          GSK_DEBUG (FALLBACK, "Unsupported texture format %u for size %dx%d",
                     gdk_texture_get_format (texture),
                     gdk_texture_get_width (texture),
                     gdk_texture_get_height (texture));
          gsk_gpu_node_processor_add_fallback_node (self, node);
          return;
        }
    }

  image = gsk_gpu_node_processor_ensure_image (self->frame,
                                               image,
                                               need_mipmap ? (GSK_GPU_IMAGE_CAN_MIPMAP | GSK_GPU_IMAGE_MIPMAP) : 0,
                                               GSK_GPU_IMAGE_STRAIGHT_ALPHA);

  switch (scaling_filter)
    {
      case GSK_SCALING_FILTER_LINEAR:
        descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_DEFAULT);
        break;

      case GSK_SCALING_FILTER_NEAREST:
        descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_NEAREST);
        break;

      case GSK_SCALING_FILTER_TRILINEAR:
        descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_MIPMAP_DEFAULT);
        break;

      default:
        g_assert_not_reached ();
        return;
    }

  gsk_gpu_texture_op (self->frame,
                      gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                      self->desc,
                      descriptor,
                      &node->bounds,
                      &self->offset,
                      &node->bounds);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_inset_shadow_node (GskGpuNodeProcessor *self,
                                              GskRenderNode       *node)
{
  GdkRGBA color;
  float spread, blur_radius;

  spread = gsk_inset_shadow_node_get_spread (node);
  color = *gsk_inset_shadow_node_get_color (node);
  color.alpha *= self->opacity;
  blur_radius = gsk_inset_shadow_node_get_blur_radius (node);

  if (blur_radius == 0)
    {
      gsk_gpu_border_op (self->frame,
                         gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                         gsk_inset_shadow_node_get_outline (node),
                         &self->offset,
                         &GRAPHENE_POINT_INIT (gsk_inset_shadow_node_get_dx (node),
                                               gsk_inset_shadow_node_get_dy (node)),
                         (float[4]) { spread, spread, spread, spread },
                         (GdkRGBA[4]) { color, color, color, color });
    }
  else
    {
      gsk_gpu_box_shadow_op (self->frame,
                             gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                             TRUE,
                             &node->bounds,
                             gsk_inset_shadow_node_get_outline (node),
                             &GRAPHENE_POINT_INIT (gsk_inset_shadow_node_get_dx (node),
                                                   gsk_inset_shadow_node_get_dy (node)),
                             spread,
                             blur_radius,
                             &self->offset,
                             &color);
    }
}

static void
gsk_gpu_node_processor_add_outset_shadow_node (GskGpuNodeProcessor *self,
                                               GskRenderNode       *node)
{
  GdkRGBA color;
  float spread, blur_radius, dx, dy;

  spread = gsk_outset_shadow_node_get_spread (node);
  color = *gsk_outset_shadow_node_get_color (node);
  color.alpha *= self->opacity;
  blur_radius = gsk_outset_shadow_node_get_blur_radius (node);
  dx = gsk_outset_shadow_node_get_dx (node);
  dy = gsk_outset_shadow_node_get_dy (node);

  if (blur_radius == 0)
    {
      GskRoundedRect outline;

      gsk_rounded_rect_init_copy (&outline, gsk_outset_shadow_node_get_outline (node));
      gsk_rounded_rect_shrink (&outline, -spread, -spread, -spread, -spread);
      graphene_rect_offset (&outline.bounds, dx, dy);

      gsk_gpu_border_op (self->frame,
                         gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                         &outline,
                         &self->offset,
                         &GRAPHENE_POINT_INIT (-dx, -dy),
                         (float[4]) { spread, spread, spread, spread },
                         (GdkRGBA[4]) { color, color, color, color });
    }
  else
    {
      gsk_gpu_box_shadow_op (self->frame,
                             gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                             FALSE,
                             &node->bounds,
                             gsk_outset_shadow_node_get_outline (node),
                             &GRAPHENE_POINT_INIT (dx, dy),
                             spread,
                             blur_radius,
                             &self->offset,
                             &color);
    }
}

typedef void (* GradientOpFunc) (GskGpuNodeProcessor  *self,
                                 GskRenderNode        *node,
                                 const GskColorStop   *stops,
                                 gsize                 n_stops);

static void
gsk_gpu_node_processor_add_gradient_node (GskGpuNodeProcessor *self,
                                          GskRenderNode       *node,
                                          const GskColorStop  *stops,
                                          gsize                n_stops,
                                          GradientOpFunc       func)
{
  GskColorStop real_stops[7];
  GskGpuNodeProcessor other;
  graphene_rect_t bounds;
  gsize i, j;
  GskGpuImage *image;
  guint32 descriptor;

  if (n_stops < 8)
    {
      if (self->opacity < 1.0)
        {
          for (i = 0; i < n_stops; i++)
            {
              real_stops[i].offset = stops[i].offset;
              real_stops[i].color = GDK_RGBA_INIT_ALPHA (&stops[i].color, self->opacity);
            }
          stops = real_stops;
        }
      
      func (self, node, stops, n_stops);

      return;
    }

  if (gsk_gpu_node_processor_try_node_as_pattern (self, node))
    return;

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &bounds))
    return;
  rect_round_to_pixels (&bounds, &self->scale, &self->offset, &bounds);

  image = gsk_gpu_node_processor_init_draw (&other,
                                            self->frame,
                                            gsk_render_node_get_preferred_depth (node),
                                            &self->scale,
                                            &bounds);

  other.blend = GSK_GPU_BLEND_ADD;
  other.pending_globals |= GSK_GPU_GLOBAL_BLEND;
  gsk_gpu_node_processor_sync_globals (&other, 0);
  
  for (i = 0; i < n_stops; /* happens inside the loop */)
    {
      if (i == 0)
        {
          real_stops[0].offset = stops[i].offset;
          real_stops[0].color = GDK_RGBA_INIT_ALPHA (&stops[i].color, self->opacity);
          i++;
        }
      else
        {
          real_stops[0].offset = stops[i-1].offset;
          real_stops[0].color = GDK_RGBA_INIT_ALPHA (&stops[i-1].color, 0);
        }
      for (j = 1; j < 6 && i < n_stops; j++)
        {
          real_stops[j].offset = stops[i].offset;
          real_stops[j].color = GDK_RGBA_INIT_ALPHA (&stops[i].color, self->opacity);
          i++;
        }
      if (i == n_stops - 1)
        {
          g_assert (j == 6);
          real_stops[j].offset = stops[i].offset;
          real_stops[j].color = GDK_RGBA_INIT_ALPHA (&stops[i].color, self->opacity);
          j++;
          i++;
        }
      else if (i < n_stops)
        {
          real_stops[j].offset = stops[i].offset;
          real_stops[j].color = GDK_RGBA_INIT_ALPHA (&stops[i].color, 0);
          j++;
        }

      func (&other, node, real_stops, j);
    }

  gsk_gpu_node_processor_finish_draw (&other, image);

  descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_DEFAULT);

  gsk_gpu_texture_op (self->frame,
                      gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &bounds),
                      self->desc,
                      descriptor,
                      &node->bounds,
                      &self->offset,
                      &bounds);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_linear_gradient_op (GskGpuNodeProcessor  *self,
                                           GskRenderNode        *node,
                                           const GskColorStop   *stops,
                                           gsize                 n_stops)
{
  gsk_gpu_linear_gradient_op (self->frame,
                              gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                              GSK_RENDER_NODE_TYPE (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE,
                              &node->bounds,
                              gsk_linear_gradient_node_get_start (node),
                              gsk_linear_gradient_node_get_end (node),
                              &self->offset,
                              stops,
                              n_stops);
}

static void
gsk_gpu_node_processor_add_linear_gradient_node (GskGpuNodeProcessor *self,
                                                 GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_gradient_node (self,
                                            node,
                                            gsk_linear_gradient_node_get_color_stops (node, NULL),
                                            gsk_linear_gradient_node_get_n_color_stops (node),
                                            gsk_gpu_node_processor_linear_gradient_op);
}

static gboolean
gsk_gpu_node_processor_create_linear_gradient_pattern (GskGpuPatternWriter *self,
                                                       GskRenderNode       *node)
{
  if (gsk_render_node_get_node_type (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE)
    gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_REPEATING_LINEAR_GRADIENT);
  else
    gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_LINEAR_GRADIENT);

  gsk_gpu_pattern_writer_append_point (self,
                                      gsk_linear_gradient_node_get_start (node),
                                      &self->offset);
  gsk_gpu_pattern_writer_append_point (self,
                                      gsk_linear_gradient_node_get_end (node),
                                      &self->offset);
  gsk_gpu_pattern_writer_append_color_stops (self, 
                                            gsk_linear_gradient_node_get_color_stops (node, NULL),
                                            gsk_linear_gradient_node_get_n_color_stops (node));

  return TRUE;
}

static void
gsk_gpu_node_processor_radial_gradient_op (GskGpuNodeProcessor  *self,
                                           GskRenderNode        *node,
                                           const GskColorStop   *stops,
                                           gsize                 n_stops)
{
  gsk_gpu_radial_gradient_op (self->frame,
                              gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                              GSK_RENDER_NODE_TYPE (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE,
                              &node->bounds,
                              gsk_radial_gradient_node_get_center (node),
                              &GRAPHENE_POINT_INIT (
                                  gsk_radial_gradient_node_get_hradius (node),
                                  gsk_radial_gradient_node_get_vradius (node)
                              ),
                              gsk_radial_gradient_node_get_start (node),
                              gsk_radial_gradient_node_get_end (node),
                              &self->offset,
                              stops,
                              n_stops);
}

static void
gsk_gpu_node_processor_add_radial_gradient_node (GskGpuNodeProcessor *self,
                                                 GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_gradient_node (self,
                                            node,
                                            gsk_radial_gradient_node_get_color_stops (node, NULL),
                                            gsk_radial_gradient_node_get_n_color_stops (node),
                                            gsk_gpu_node_processor_radial_gradient_op);
}

static gboolean
gsk_gpu_node_processor_create_radial_gradient_pattern (GskGpuPatternWriter *self,
                                                       GskRenderNode       *node)
{
  if (gsk_render_node_get_node_type (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE)
    gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_REPEATING_RADIAL_GRADIENT);
  else
    gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_RADIAL_GRADIENT);

  gsk_gpu_pattern_writer_append_point (self,
                                      gsk_radial_gradient_node_get_center (node),
                                      &self->offset);
  gsk_gpu_pattern_writer_append_float (self, gsk_radial_gradient_node_get_hradius (node));
  gsk_gpu_pattern_writer_append_float (self, gsk_radial_gradient_node_get_vradius (node));
  gsk_gpu_pattern_writer_append_float (self, gsk_radial_gradient_node_get_start (node));
  gsk_gpu_pattern_writer_append_float (self, gsk_radial_gradient_node_get_end (node));
  gsk_gpu_pattern_writer_append_color_stops (self, 
                                            gsk_radial_gradient_node_get_color_stops (node, NULL),
                                            gsk_radial_gradient_node_get_n_color_stops (node));

  return TRUE;
}

static void
gsk_gpu_node_processor_conic_gradient_op (GskGpuNodeProcessor  *self,
                                          GskRenderNode        *node,
                                          const GskColorStop   *stops,
                                          gsize                 n_stops)
{
  gsk_gpu_conic_gradient_op (self->frame,
                             gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                             &node->bounds,
                             gsk_conic_gradient_node_get_center (node),
                             gsk_conic_gradient_node_get_angle (node),
                             &self->offset,
                             stops,
                             n_stops);
}

static void
gsk_gpu_node_processor_add_conic_gradient_node (GskGpuNodeProcessor *self,
                                                GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_gradient_node (self,
                                            node,
                                            gsk_conic_gradient_node_get_color_stops (node, NULL),
                                            gsk_conic_gradient_node_get_n_color_stops (node),
                                            gsk_gpu_node_processor_conic_gradient_op);
}

static gboolean
gsk_gpu_node_processor_create_conic_gradient_pattern (GskGpuPatternWriter *self,
                                                      GskRenderNode       *node)
{
  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_CONIC_GRADIENT);
  gsk_gpu_pattern_writer_append_point (self,
                                      gsk_conic_gradient_node_get_center (node),
                                      &self->offset);
  gsk_gpu_pattern_writer_append_float (self, gsk_conic_gradient_node_get_angle (node));
  gsk_gpu_pattern_writer_append_color_stops (self, 
                                            gsk_conic_gradient_node_get_color_stops (node, NULL),
                                            gsk_conic_gradient_node_get_n_color_stops (node));

  return TRUE;
}

static void
gsk_gpu_node_processor_add_blur_node (GskGpuNodeProcessor *self,
                                      GskRenderNode       *node)
{
  GskRenderNode *child;
  GskGpuImage *image;
  graphene_rect_t tex_rect, clip_rect;
  float blur_radius, clip_radius;
  guint32 descriptor;

  child = gsk_blur_node_get_child (node);
  blur_radius = gsk_blur_node_get_radius (node);
  if (blur_radius <= 0.f)
    {
      gsk_gpu_node_processor_add_node (self, child);
      return;
    }

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius / 2.0);
  gsk_gpu_node_processor_get_clip_bounds (self, &clip_rect);
  graphene_rect_inset (&clip_rect, -clip_radius, -clip_radius);
  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    0,
                                                    GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                    &clip_rect,
                                                    child,
                                                    &tex_rect);
  if (image == NULL)
    return;

  descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_TRANSPARENT);

  gsk_gpu_node_processor_blur_op (self,
                                  &node->bounds,
                                  graphene_point_zero (),
                                  blur_radius,
                                  NULL,
                                  self->desc,
                                  descriptor,
                                  gdk_memory_format_get_depth (gsk_gpu_image_get_format (image)),
                                  &tex_rect);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_shadow_node (GskGpuNodeProcessor *self,
                                        GskRenderNode       *node)
{
  GskGpuImage *image;
  graphene_rect_t clip_bounds, tex_rect;
  GskRenderNode *child;
  gsize i, n_shadows;
  GskGpuDescriptors *desc;
  guint32 descriptor;

  n_shadows = gsk_shadow_node_get_n_shadows (node);
  child = gsk_shadow_node_get_child (node);
  /* enlarge clip for shadow offsets */
  gsk_gpu_node_processor_get_clip_bounds (self, &clip_bounds);
  clip_bounds = GRAPHENE_RECT_INIT (clip_bounds.origin.x - node->bounds.size.width + child->bounds.size.width - node->bounds.origin.x + child->bounds.origin.x,
                                    clip_bounds.origin.y - node->bounds.size.height + child->bounds.size.height - node->bounds.origin.y + child->bounds.origin.y,
                                    clip_bounds.size.width + node->bounds.size.width - child->bounds.size.width,
                                    clip_bounds.size.height + node->bounds.size.height - child->bounds.size.height);

  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    0,
                                                    GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                    &clip_bounds, 
                                                    child,
                                                    &tex_rect);
  if (image == NULL)
    return;

  descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_TRANSPARENT);
  desc = self->desc;

  for (i = 0; i < n_shadows; i++)
    {
      const GskShadow *shadow = gsk_shadow_node_get_shadow (node, i);
      if (shadow->radius == 0)
        {
          graphene_point_t shadow_offset = GRAPHENE_POINT_INIT (self->offset.x + shadow->dx,
                                                                self->offset.y + shadow->dy);
          gsk_gpu_colorize_op (self->frame,
                               gsk_gpu_clip_get_shader_clip (&self->clip, &shadow_offset, &child->bounds),
                               desc,
                               descriptor,
                               &child->bounds,
                               &shadow_offset,
                               &tex_rect,
                               &shadow->color);
        }
      else
        {
          graphene_rect_t bounds;
          float clip_radius = gsk_cairo_blur_compute_pixels (0.5 * shadow->radius);
          graphene_rect_inset_r (&child->bounds, - clip_radius, - clip_radius, &bounds);
          gsk_gpu_node_processor_blur_op (self,
                                          &bounds,
                                          &GRAPHENE_POINT_INIT (shadow->dx, shadow->dy),
                                          shadow->radius,
                                          &shadow->color,
                                          desc,
                                          descriptor,
                                          gdk_memory_format_get_depth (gsk_gpu_image_get_format (image)),
                                          &tex_rect);
        }
    }

  descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_DEFAULT);
  gsk_gpu_texture_op (self->frame,
                      gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &child->bounds),
                      self->desc,
                      descriptor,
                      &child->bounds,
                      &self->offset,
                      &tex_rect);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_blend_node (GskGpuNodeProcessor *self,
                                       GskRenderNode       *node)
{
  GskRenderNode *bottom_child, *top_child;
  graphene_rect_t bottom_rect, top_rect;
  GskGpuImage *bottom_image, *top_image;
  guint32 descriptors[2];

  bottom_child = gsk_blend_node_get_bottom_child (node);
  top_child = gsk_blend_node_get_top_child (node);

  if ((gsk_gpu_node_processor_ubershader_instead_of_offscreen (self, bottom_child) ||
       gsk_gpu_node_processor_ubershader_instead_of_offscreen (self, top_child)) &&
      gsk_gpu_node_processor_try_node_as_pattern (self, node))
    return;

  bottom_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                           0,
                                                           GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                           NULL,
                                                           bottom_child,
                                                           &bottom_rect);
  top_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                        0,
                                                        GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                        NULL,
                                                        top_child,
                                                        &top_rect);

  if (bottom_image == NULL)
    {
      if (top_image == NULL)
        return;

      bottom_image = g_object_ref (top_image);
      bottom_rect = *graphene_rect_zero ();
    }
  else if (top_image == NULL)
    {
      top_image = g_object_ref (bottom_image);
      top_rect = *graphene_rect_zero ();
    }

  gsk_gpu_node_processor_add_images (self,
                                     2,
                                     (GskGpuImage *[2]) { bottom_image, top_image },
                                     (GskGpuSampler[2]) { GSK_GPU_SAMPLER_DEFAULT, GSK_GPU_SAMPLER_DEFAULT },
                                     descriptors);

  gsk_gpu_blend_mode_op (self->frame,
                         gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                         self->desc,
                         &node->bounds,
                         &self->offset,
                         self->opacity,
                         gsk_blend_node_get_blend_mode (node),
                         descriptors[0],
                         &bottom_rect,
                         descriptors[1],
                         &top_rect);

  g_object_unref (top_image);
  g_object_unref (bottom_image);
}

static gboolean
gsk_gpu_node_processor_create_blend_pattern (GskGpuPatternWriter *self,
                                             GskRenderNode       *node)
{
  GskRenderNode *bottom_child, *top_child;

  bottom_child = gsk_blend_node_get_bottom_child (node);
  top_child = gsk_blend_node_get_top_child (node);

  if (!gsk_gpu_node_processor_create_node_pattern (self, bottom_child))
    return FALSE;
  if (!gsk_rect_contains_rect (&bottom_child->bounds, &node->bounds))
    {
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_CLIP);
      gsk_gpu_pattern_writer_append_rect (self, &bottom_child->bounds, &self->offset);
    }

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_PUSH_COLOR);

  if (!gsk_gpu_pattern_writer_push_stack (self))
    return FALSE;

  if (!gsk_gpu_node_processor_create_node_pattern (self, top_child))
    {
      gsk_gpu_pattern_writer_pop_stack (self);
      return FALSE;
    }
  if (!gsk_rect_contains_rect (&top_child->bounds, &node->bounds))
    {
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_CLIP);
      gsk_gpu_pattern_writer_append_rect (self, &top_child->bounds, &self->offset);
    }

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_BLEND_DEFAULT + gsk_blend_node_get_blend_mode (node));

  gsk_gpu_pattern_writer_pop_stack (self);

  return TRUE;
}

static void
gsk_gpu_node_processor_add_cross_fade_node (GskGpuNodeProcessor *self,
                                            GskRenderNode       *node)
{
  GskRenderNode *start_child, *end_child;
  graphene_rect_t start_rect, end_rect;
  GskGpuImage *start_image, *end_image;
  guint32 descriptors[2];
  float progress, old_opacity;

  start_child = gsk_cross_fade_node_get_start_child (node);
  end_child = gsk_cross_fade_node_get_end_child (node);
  progress = gsk_cross_fade_node_get_progress (node);

  if ((gsk_gpu_node_processor_ubershader_instead_of_offscreen (self, start_child) ||
       gsk_gpu_node_processor_ubershader_instead_of_offscreen (self, end_child)) &&
      gsk_gpu_node_processor_try_node_as_pattern (self, node))
    return;

  start_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                          0,
                                                          GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                          NULL,
                                                          start_child,
                                                          &start_rect);
  end_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                        0,
                                                        GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                        NULL,
                                                        end_child,
                                                        &end_rect);

  if (start_image == NULL)
    {
      if (end_image == NULL)
        return;

      old_opacity = self->opacity;
      self->opacity *= progress;
      gsk_gpu_node_processor_image_op (self,
                                       end_image,
                                       &end_child->bounds,
                                       &end_rect);
      g_object_unref (end_image);
      self->opacity = old_opacity;
      return;
    }
  else if (end_image == NULL)
    {
      old_opacity = self->opacity;
      self->opacity *= (1 - progress);
      gsk_gpu_node_processor_image_op (self,
                                       start_image,
                                       &start_child->bounds,
                                       &start_rect);
      g_object_unref (start_image);
      self->opacity = old_opacity;
      return;
    }

  gsk_gpu_node_processor_add_images (self,
                                     2,
                                     (GskGpuImage *[2]) { start_image, end_image },
                                     (GskGpuSampler[2]) { GSK_GPU_SAMPLER_DEFAULT, GSK_GPU_SAMPLER_DEFAULT },
                                     descriptors);

  gsk_gpu_cross_fade_op (self->frame,
                         gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                         self->desc,
                         &node->bounds,
                         &self->offset,
                         self->opacity,
                         progress,
                         descriptors[0],
                         &start_rect,
                         descriptors[1],
                         &end_rect);

  g_object_unref (end_image);
  g_object_unref (start_image);
}

static gboolean
gsk_gpu_node_processor_create_cross_fade_pattern (GskGpuPatternWriter *self,
                                                  GskRenderNode       *node)
{
  GskRenderNode *start_child, *end_child;

  start_child = gsk_cross_fade_node_get_start_child (node);
  end_child = gsk_cross_fade_node_get_end_child (node);

  if (!gsk_gpu_node_processor_create_node_pattern (self, start_child))
    return FALSE;
  if (!gsk_rect_contains_rect (&start_child->bounds, &node->bounds))
    {
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_CLIP);
      gsk_gpu_pattern_writer_append_rect (self, &start_child->bounds, &self->offset);
    }

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_PUSH_COLOR);

  if (!gsk_gpu_pattern_writer_push_stack (self))
    return FALSE;

  if (!gsk_gpu_node_processor_create_node_pattern (self, end_child))
    {
      gsk_gpu_pattern_writer_pop_stack (self);
      return FALSE;
    }
  if (!gsk_rect_contains_rect (&end_child->bounds, &node->bounds))
    {
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_CLIP);
      gsk_gpu_pattern_writer_append_rect (self, &end_child->bounds, &self->offset);
    }

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_POP_CROSS_FADE);
  gsk_gpu_pattern_writer_append_float (self, gsk_cross_fade_node_get_progress (node));

  gsk_gpu_pattern_writer_pop_stack (self);

  return TRUE;
}

static void
gsk_gpu_node_processor_add_mask_node (GskGpuNodeProcessor *self,
                                      GskRenderNode       *node)
{
  GskRenderNode *source_child, *mask_child;
  GskGpuImage *mask_image;
  graphene_rect_t bounds, mask_rect;
  GskMaskMode mask_mode;

  source_child = gsk_mask_node_get_source (node);
  mask_child = gsk_mask_node_get_mask (node);
  mask_mode = gsk_mask_node_get_mask_mode (node);

  if ((gsk_gpu_node_processor_ubershader_instead_of_offscreen (self, mask_child) ||
       (gsk_gpu_node_processor_ubershader_instead_of_offscreen (self, source_child) &&
        gsk_render_node_get_node_type (source_child) != GSK_COLOR_NODE)) &&
      gsk_gpu_node_processor_try_node_as_pattern (self, node))
    return;

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &bounds))
    return;

  mask_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                         0,
                                                         GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                         &bounds,
                                                         mask_child,
                                                         &mask_rect);
  if (mask_image == NULL)
    {
      if (mask_mode == GSK_MASK_MODE_INVERTED_ALPHA)
        gsk_gpu_node_processor_add_node (self, source_child);
      return;
    }

  if (gsk_render_node_get_node_type (source_child) == GSK_COLOR_NODE &&
      mask_mode == GSK_MASK_MODE_ALPHA)
    {
      const GdkRGBA *rgba = gsk_color_node_get_color (source_child);
      guint32 descriptor = gsk_gpu_node_processor_add_image (self, mask_image, GSK_GPU_SAMPLER_DEFAULT);
      gsk_gpu_colorize_op (self->frame,
                           gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                           self->desc,
                           descriptor,
                           &node->bounds,
                           &self->offset,
                           &mask_rect,
                           &GDK_RGBA_INIT_ALPHA (rgba, self->opacity));
    }
  else
    {
      GskGpuImage *source_image;
      graphene_rect_t source_rect;
      guint32 descriptors[2];

      source_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                               0,
                                                               GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                               &bounds,
                                                               source_child,
                                                               &source_rect);
      if (source_image == NULL)
        {
          g_object_unref (mask_image);
          return;
        }
      gsk_gpu_node_processor_add_images (self,
                                         2,
                                         (GskGpuImage *[2]) { source_image, mask_image },
                                         (GskGpuSampler[2]) { GSK_GPU_SAMPLER_DEFAULT, GSK_GPU_SAMPLER_DEFAULT },
                                         descriptors);

      gsk_gpu_mask_op (self->frame,
                       gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                       self->desc,
                       &node->bounds,
                       &self->offset,
                       self->opacity,
                       mask_mode,
                       descriptors[0],
                       &source_rect,
                       descriptors[1],
                       &mask_rect);

      g_object_unref (source_image);
    }

  g_object_unref (mask_image);
}

static gboolean
gsk_gpu_node_processor_create_mask_pattern (GskGpuPatternWriter *self,
                                            GskRenderNode       *node)
{
  GskRenderNode *source_child, *mask_child;

  source_child = gsk_mask_node_get_source (node);
  mask_child = gsk_mask_node_get_mask (node);

  if (!gsk_gpu_node_processor_create_node_pattern (self, source_child))
    return FALSE;
  if (!gsk_rect_contains_rect (&source_child->bounds, &node->bounds))
    {
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_CLIP);
      gsk_gpu_pattern_writer_append_rect (self, &source_child->bounds, &self->offset);
    }

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_PUSH_COLOR);

  if (!gsk_gpu_pattern_writer_push_stack (self))
    return FALSE;

  if (!gsk_gpu_node_processor_create_node_pattern (self, mask_child))
    {
      gsk_gpu_pattern_writer_pop_stack (self);
      return FALSE;
    }
  if (!gsk_rect_contains_rect (&mask_child->bounds, &node->bounds))
    {
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_CLIP);
      gsk_gpu_pattern_writer_append_rect (self, &mask_child->bounds, &self->offset);
    }

  switch (gsk_mask_node_get_mask_mode (node))
  {
    case GSK_MASK_MODE_ALPHA:
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_POP_MASK_ALPHA);
      break;

    case GSK_MASK_MODE_INVERTED_ALPHA:
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_POP_MASK_INVERTED_ALPHA);
      break;

    case GSK_MASK_MODE_LUMINANCE:
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_POP_MASK_LUMINANCE);
      break;

    case GSK_MASK_MODE_INVERTED_LUMINANCE:
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_POP_MASK_INVERTED_LUMINANCE);
      break;

    default:
      g_return_val_if_reached (FALSE);
  }


  gsk_gpu_pattern_writer_pop_stack (self);

  return TRUE;
}

static void
gsk_gpu_node_processor_add_glyph_node (GskGpuNodeProcessor *self,
                                       GskRenderNode       *node)
{
  GskGpuDevice *device;
  const PangoGlyphInfo *glyphs;
  PangoFont *font;
  graphene_point_t offset;
  guint i, num_glyphs;
  float scale, inv_scale;
  GdkRGBA color;
  float align_scale_x, align_scale_y;
  float inv_align_scale_x, inv_align_scale_y;
  unsigned int flags_mask;
  GskGpuImage *last_image;
  guint32 descriptor;
  const float inv_pango_scale = 1.f / PANGO_SCALE;

  if (self->opacity < 1.0 &&
      gsk_text_node_has_color_glyphs (node))
    {
      gsk_gpu_node_processor_add_without_opacity (self, node);
      return;
    }

  device = gsk_gpu_frame_get_device (self->frame);

  color = *gsk_text_node_get_color (node);
  color.alpha *= self->opacity;
  num_glyphs = gsk_text_node_get_num_glyphs (node);
  glyphs = gsk_text_node_get_glyphs (node, NULL);
  font = gsk_text_node_get_font (node);
  offset = *gsk_text_node_get_offset (node);
  offset.x += self->offset.x;
  offset.y += self->offset.y;

  scale = MAX (graphene_vec2_get_x (&self->scale), graphene_vec2_get_y (&self->scale));
  inv_scale = 1.f / scale;

  if (gsk_font_get_hint_style (font) != CAIRO_HINT_STYLE_NONE)
    {
      align_scale_x = scale * 4;
      align_scale_y = scale;
      flags_mask = 3;
    }
  else
    {
      align_scale_x = align_scale_y = scale * 4;
      flags_mask = 15;
    }

  inv_align_scale_x = 1 / align_scale_x;
  inv_align_scale_y = 1 / align_scale_y;

  last_image = NULL;
  descriptor = 0;
  for (i = 0; i < num_glyphs; i++)
    {
      GskGpuImage *image;
      graphene_rect_t glyph_bounds, glyph_tex_rect;
      graphene_point_t glyph_offset, glyph_origin;
      GskGpuGlyphLookupFlags flags;

      glyph_origin = GRAPHENE_POINT_INIT (offset.x + glyphs[i].geometry.x_offset * inv_pango_scale,
                                          offset.y + glyphs[i].geometry.y_offset * inv_pango_scale);

      glyph_origin.x = floorf (glyph_origin.x * align_scale_x + 0.5f);
      glyph_origin.y = floorf (glyph_origin.y * align_scale_y + 0.5f);
      flags = (((int) glyph_origin.x & 3) | (((int) glyph_origin.y & 3) << 2)) & flags_mask;
      glyph_origin.x *= inv_align_scale_x;
      glyph_origin.y *= inv_align_scale_y;

      image = gsk_gpu_device_lookup_glyph_image (device,
                                                 self->frame,
                                                 font,
                                                 glyphs[i].glyph,
                                                 flags,
                                                 scale,
                                                 &glyph_bounds,
                                                 &glyph_offset);

      glyph_tex_rect = GRAPHENE_RECT_INIT (-glyph_bounds.origin.x * inv_scale,
                                           -glyph_bounds.origin.y * inv_scale,
                                           gsk_gpu_image_get_width (image) * inv_scale,
                                           gsk_gpu_image_get_height (image) * inv_scale);
      glyph_bounds = GRAPHENE_RECT_INIT (0,
                                         0,
                                         glyph_bounds.size.width * inv_scale,
                                         glyph_bounds.size.height * inv_scale);
      glyph_origin = GRAPHENE_POINT_INIT (glyph_origin.x - glyph_offset.x * inv_scale,
                                          glyph_origin.y - glyph_offset.y * inv_scale);

      if (image != last_image)
        {
          descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_DEFAULT);
          last_image = image;
        }

      if (glyphs[i].attr.is_color)
        gsk_gpu_texture_op (self->frame,
                            gsk_gpu_clip_get_shader_clip (&self->clip, &glyph_offset, &glyph_bounds),
                            self->desc,
                            descriptor,
                            &glyph_bounds,
                            &glyph_origin,
                            &glyph_tex_rect);
      else
        gsk_gpu_colorize_op (self->frame,
                             gsk_gpu_clip_get_shader_clip (&self->clip, &glyph_offset, &glyph_bounds),
                             self->desc,
                             descriptor,
                             &glyph_bounds,
                             &glyph_origin,
                             &glyph_tex_rect,
                             &color);

      offset.x += glyphs[i].geometry.width * inv_pango_scale;
    }
}

static gboolean
gsk_gpu_node_processor_create_glyph_pattern (GskGpuPatternWriter *self,
                                             GskRenderNode       *node)
{
  GskGpuDevice *device;
  const PangoGlyphInfo *glyphs;
  PangoFont *font;
  guint num_glyphs;
  gsize i;
  float scale, inv_scale;
  guint32 tex_id;
  GskGpuImage *last_image;
  graphene_point_t offset;
  float align_scale_x, align_scale_y;
  float inv_align_scale_x, inv_align_scale_y;
  unsigned int flags_mask;
  const float inv_pango_scale = 1.f / PANGO_SCALE;

  if (gsk_text_node_has_color_glyphs (node))
    return FALSE;

  device = gsk_gpu_frame_get_device (self->frame);
  num_glyphs = gsk_text_node_get_num_glyphs (node);
  glyphs = gsk_text_node_get_glyphs (node, NULL);
  font = gsk_text_node_get_font (node);
  offset = *gsk_text_node_get_offset (node);
  offset.x += self->offset.x;
  offset.y += self->offset.y;

  scale = MAX (graphene_vec2_get_x (&self->scale), graphene_vec2_get_y (&self->scale));
  inv_scale = 1.f / scale;

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_GLYPHS);
  gsk_gpu_pattern_writer_append_rgba (self, gsk_text_node_get_color (node));
  gsk_gpu_pattern_writer_append_uint (self, num_glyphs);

  if (gsk_font_get_hint_style (font) != CAIRO_HINT_STYLE_NONE)
    {
      align_scale_x = scale * 4;
      align_scale_y = scale;
      flags_mask = 3;
    }
  else
    {
      align_scale_x = align_scale_y = scale * 4;
      flags_mask = 15;
    }

  inv_align_scale_x = 1 / align_scale_x;
  inv_align_scale_y = 1 / align_scale_y;

  last_image = NULL;
  for (i = 0; i < num_glyphs; i++)
    {
      GskGpuImage *image;
      graphene_rect_t glyph_bounds;
      graphene_point_t glyph_offset, glyph_origin;
      GskGpuGlyphLookupFlags flags;

      glyph_origin = GRAPHENE_POINT_INIT (offset.x + glyphs[i].geometry.x_offset * inv_pango_scale,
                                          offset.y + glyphs[i].geometry.y_offset * inv_pango_scale);

      glyph_origin.x = floorf (glyph_origin.x * align_scale_x + 0.5f);
      glyph_origin.y = floorf (glyph_origin.y * align_scale_y + 0.5f);
      flags = (((int) glyph_origin.x & 3) | (((int) glyph_origin.y & 3) << 2)) & flags_mask;
      glyph_origin.x *= inv_align_scale_x;
      glyph_origin.y *= inv_align_scale_y;

      image = gsk_gpu_device_lookup_glyph_image (device,
                                                 self->frame,
                                                 font,
                                                 glyphs[i].glyph,
                                                 flags,
                                                 scale,
                                                 &glyph_bounds,
                                                 &glyph_offset);

      if (image != last_image)
        {
          if (!gsk_gpu_pattern_writer_add_image (self, image, GSK_GPU_SAMPLER_DEFAULT, &tex_id))
            return FALSE;

          last_image = image;
        }

      glyph_origin = GRAPHENE_POINT_INIT (glyph_origin.x - glyph_offset.x * inv_scale,
                                          glyph_origin.y - glyph_offset.y * inv_scale);

      gsk_gpu_pattern_writer_append_uint (self, tex_id);
      gsk_gpu_pattern_writer_append_rect (self,
                                          &GRAPHENE_RECT_INIT (
                                              0,
                                              0,
                                              glyph_bounds.size.width * inv_scale,
                                              glyph_bounds.size.height * inv_scale
                                          ),
                                          &glyph_origin);
      gsk_gpu_pattern_writer_append_rect (self,
                                          &GRAPHENE_RECT_INIT (
                                              - glyph_bounds.origin.x * inv_scale,
                                              - glyph_bounds.origin.y * inv_scale,
                                              gsk_gpu_image_get_width (image) * inv_scale,
                                              gsk_gpu_image_get_height (image) * inv_scale
                                          ),
                                          &glyph_origin);

      offset.x += glyphs[i].geometry.width * inv_pango_scale;
    }

  return TRUE;
}

static gboolean
gsk_gpu_node_processor_create_opacity_pattern (GskGpuPatternWriter *self,
                                               GskRenderNode       *node)
{
  if (!gsk_gpu_node_processor_create_node_pattern (self, gsk_opacity_node_get_child (node)))
    return FALSE;

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_OPACITY);
  gsk_gpu_pattern_writer_append_float (self, gsk_opacity_node_get_opacity (node));

  return TRUE;
}

static void
gsk_gpu_node_processor_add_color_matrix_node (GskGpuNodeProcessor *self,
                                              GskRenderNode       *node)
{
  GskGpuImage *image;
  guint32 descriptor;
  GskRenderNode *child;
  graphene_matrix_t opacity_matrix;
  const graphene_matrix_t *color_matrix;
  graphene_rect_t tex_rect;

  child = gsk_color_matrix_node_get_child (node);

  if (gsk_gpu_node_processor_ubershader_instead_of_offscreen (self, child) &&
      gsk_gpu_node_processor_try_node_as_pattern (self, node))
    return;

  color_matrix = gsk_color_matrix_node_get_color_matrix (node);
  if (self->opacity < 1.0f)
    {
      graphene_matrix_init_from_float (&opacity_matrix,
                                       (float[16]) {
                                           1.0f, 0.0f, 0.0f, 0.0f,
                                           0.0f, 1.0f, 0.0f, 0.0f,
                                           0.0f, 0.0f, 1.0f, 0.0f,
                                           0.0f, 0.0f, 0.0f, self->opacity
                                      });
      graphene_matrix_multiply (&opacity_matrix, color_matrix, &opacity_matrix);
      color_matrix = &opacity_matrix;
    }

  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    0,
                                                    GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                    NULL,
                                                    child,
                                                    &tex_rect);
  if (image == NULL)
    return;

  descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_DEFAULT);
  
  gsk_gpu_color_matrix_op (self->frame,
                           gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                           self->desc,
                           descriptor,
                           &node->bounds,
                           &self->offset,
                           &tex_rect,
                           color_matrix,
                           gsk_color_matrix_node_get_color_offset (node));

  g_object_unref (image);
}

static gboolean
gsk_gpu_node_processor_create_color_matrix_pattern (GskGpuPatternWriter *self,
                                                    GskRenderNode       *node)
{
  if (!gsk_gpu_node_processor_create_node_pattern (self, gsk_color_matrix_node_get_child (node)))
    return FALSE;

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_COLOR_MATRIX);
  gsk_gpu_pattern_writer_append_matrix (self, gsk_color_matrix_node_get_color_matrix (node));
  gsk_gpu_pattern_writer_append_vec4 (self, gsk_color_matrix_node_get_color_offset (node));

  return TRUE;
}

static void
gsk_gpu_node_processor_repeat_tile (GskGpuNodeProcessor    *self,
                                    const graphene_rect_t  *rect,
                                    float                   x,
                                    float                   y,
                                    GskRenderNode          *child,
                                    const graphene_rect_t  *child_bounds)
{
  GskGpuImage *image;
  graphene_rect_t clipped_child_bounds, offset_rect;
  guint32 descriptor;

  gsk_rect_init_offset (&offset_rect,
                        rect,
                        - x * child_bounds->size.width,
                        - y * child_bounds->size.height);
  if (!gsk_rect_intersection (&offset_rect, child_bounds, &clipped_child_bounds))
    {
      /* The math has gone wrong probably, someone should look at this. */
      g_warn_if_reached ();
      return;
    }

  GSK_DEBUG (FALLBACK, "Offscreening node '%s' for tiling", g_type_name_from_instance ((GTypeInstance *) child));
  image = gsk_gpu_node_processor_create_offscreen (self->frame,
                                                   &self->scale,
                                                   &clipped_child_bounds,
                                                   child);

  g_return_if_fail (image);

  descriptor = gsk_gpu_node_processor_add_image (self, image, GSK_GPU_SAMPLER_REPEAT);

  gsk_gpu_texture_op (self->frame,
                      gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, rect),
                      self->desc,
                      descriptor,
                      rect,
                      &self->offset,
                      &GRAPHENE_RECT_INIT (
                          clipped_child_bounds.origin.x + x * child_bounds->size.width,
                          clipped_child_bounds.origin.y + y * child_bounds->size.height,
                          clipped_child_bounds.size.width,
                          clipped_child_bounds.size.height
                      ));

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_repeat_node (GskGpuNodeProcessor *self,
                                        GskRenderNode       *node)
{
  GskRenderNode *child;
  const graphene_rect_t *child_bounds;
  graphene_rect_t bounds;
  float tile_left, tile_right, tile_top, tile_bottom;

  child = gsk_repeat_node_get_child (node);
  child_bounds = gsk_repeat_node_get_child_bounds (node);
  if (gsk_rect_is_empty (child_bounds))
    return;

  if (gsk_gpu_node_processor_ubershader_instead_of_offscreen (self, child) &&
      gsk_gpu_node_processor_repeat_node_can_ubershader (node) &&
      gsk_gpu_node_processor_try_node_as_pattern (self, node))
    return;

  gsk_gpu_node_processor_get_clip_bounds (self, &bounds);
  if (!gsk_rect_intersection (&bounds, &node->bounds, &bounds))
    return;

  tile_left = (bounds.origin.x - child_bounds->origin.x) / child_bounds->size.width;
  tile_right = (bounds.origin.x + bounds.size.width - child_bounds->origin.x) / child_bounds->size.width;
  tile_top = (bounds.origin.y - child_bounds->origin.y) / child_bounds->size.height;
  tile_bottom = (bounds.origin.y + bounds.size.height - child_bounds->origin.y) / child_bounds->size.height;

  /* the 1st check tests that a tile fully fits into the bounds,
   * the 2nd check is to catch the case where it fits exactly */
  if (ceilf (tile_left) < floorf (tile_right) &&
      bounds.size.width > child_bounds->size.width)
    {
      if (ceilf (tile_top) < floorf (tile_bottom) &&
          bounds.size.height > child_bounds->size.height)
        {
          /* tile in both directions */
          gsk_gpu_node_processor_repeat_tile (self,
                                              &bounds,
                                              ceilf (tile_left),
                                              ceilf (tile_top),
                                              child,
                                              child_bounds);
        }
      else
        {
          /* tile horizontally, repeat vertically */
          float y;
          for (y = floorf (tile_top); y < ceilf (tile_bottom); y++)
            {
              float start_y = MAX (bounds.origin.y,
                                   child_bounds->origin.y + y * child_bounds->size.height);
              float end_y = MIN (bounds.origin.y + bounds.size.height,
                                 child_bounds->origin.y + (y + 1) * child_bounds->size.height);
              gsk_gpu_node_processor_repeat_tile (self,
                                                  &GRAPHENE_RECT_INIT (
                                                    bounds.origin.x,
                                                    start_y,
                                                    bounds.size.width,
                                                    end_y - start_y
                                                  ),
                                                  ceilf (tile_left),
                                                  y,
                                                  child,
                                                  child_bounds);
            }
        }
    }
  else if (ceilf (tile_top) < floorf (tile_bottom) &&
           bounds.size.height > child_bounds->size.height)
    {
      /* repeat horizontally, tile vertically */
      float x;
      for (x = floorf (tile_left); x < ceilf (tile_right); x++)
        {
          float start_x = MAX (bounds.origin.x,
                               child_bounds->origin.x + x * child_bounds->size.width);
          float end_x = MIN (bounds.origin.x + bounds.size.width,
                             child_bounds->origin.x + (x + 1) * child_bounds->size.width);
          gsk_gpu_node_processor_repeat_tile (self,
                                              &GRAPHENE_RECT_INIT (
                                                start_x,
                                                bounds.origin.y,
                                                end_x - start_x,
                                                bounds.size.height
                                              ),
                                              x,
                                              ceilf (tile_top),
                                              child,
                                              child_bounds);
        }
    }
  else
    {
      /* repeat in both directions */
      graphene_point_t old_offset, offset;
      graphene_rect_t clip_bounds;
      float x, y;

      old_offset = self->offset;

      for (x = floorf (tile_left); x < ceilf (tile_right); x++)
        {
          offset.x = x * child_bounds->size.width;
          for (y = floorf (tile_top); y < ceilf (tile_bottom); y++)
            {
              offset.y = y * child_bounds->size.height;
              self->offset = GRAPHENE_POINT_INIT (old_offset.x + offset.x, old_offset.y + offset.y);
              clip_bounds = GRAPHENE_RECT_INIT (bounds.origin.x - offset.x,
                                                bounds.origin.y - offset.y,
                                                bounds.size.width,
                                                bounds.size.height);
              if (!gsk_rect_intersection (&clip_bounds, child_bounds, &clip_bounds))
                continue;
              gsk_gpu_node_processor_add_node_clipped (self,
                                                       child,
                                                       &clip_bounds);
            }
        }

      self->offset = old_offset;
    }
}

static gboolean
gsk_gpu_node_processor_create_repeat_pattern (GskGpuPatternWriter *self,
                                              GskRenderNode       *node)
{
  GskRenderNode *child;
  const graphene_rect_t *child_bounds;
  graphene_rect_t old_bounds;

  child = gsk_repeat_node_get_child (node);
  child_bounds = gsk_repeat_node_get_child_bounds (node);

  if (gsk_rect_is_empty (child_bounds))
    {
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_COLOR);
      gsk_gpu_pattern_writer_append_rgba (self, &GDK_RGBA_TRANSPARENT);
      return TRUE;
    }

  if (!gsk_gpu_node_processor_repeat_node_can_ubershader (node))
    return FALSE;

  if (!gsk_gpu_pattern_writer_push_stack (self))
    return FALSE;

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_REPEAT_PUSH);
  gsk_gpu_pattern_writer_append_rect (self, child_bounds, &self->offset);

  old_bounds = self->bounds;
  self->bounds = GRAPHENE_RECT_INIT (child_bounds->origin.x + self->offset.x,
                                     child_bounds->origin.y + self->offset.y,
                                     child_bounds->size.width,
                                     child_bounds->size.height);

  if (!gsk_gpu_node_processor_create_node_pattern (self, child))
    {
      gsk_gpu_pattern_writer_pop_stack (self);
      return FALSE;
    }
  self->bounds = old_bounds;

  if (!gsk_rect_contains_rect (&child->bounds, child_bounds))
    {
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_CLIP);
      gsk_gpu_pattern_writer_append_rect (self, &child->bounds, &self->offset);
    }

  gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_POSITION_POP);
  gsk_gpu_pattern_writer_pop_stack (self);

  return TRUE;
}

typedef struct _FillData FillData;
struct _FillData
{
  GskPath *path;
  GdkRGBA color;
  GskFillRule fill_rule;
};

static void
gsk_fill_data_free (gpointer data)
{
  FillData *fill = data;

  gsk_path_unref (fill->path);
  g_free (fill);
}

static void
gsk_gpu_node_processor_fill_path (gpointer  data,
                                  cairo_t  *cr)
{
  FillData *fill = data;

  switch (fill->fill_rule)
  {
    case GSK_FILL_RULE_WINDING:
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_WINDING);
      break;
    case GSK_FILL_RULE_EVEN_ODD:
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  gsk_path_to_cairo (fill->path, cr);
  gdk_cairo_set_source_rgba (cr, &fill->color);
  cairo_fill (cr);
}

static void
gsk_gpu_node_processor_add_fill_node (GskGpuNodeProcessor *self,
                                      GskRenderNode       *node)
{
  graphene_rect_t clip_bounds, source_rect;
  GskGpuImage *mask_image, *source_image;
  guint32 descriptors[2];
  GskRenderNode *child;

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clip_bounds))
    return;
  rect_round_to_pixels (&clip_bounds, &self->scale, &self->offset, &clip_bounds);

  child = gsk_fill_node_get_child (node);

  mask_image = gsk_gpu_upload_cairo_op (self->frame,
                                        &self->scale,
                                        &clip_bounds,
                                        gsk_gpu_node_processor_fill_path,
                                        g_memdup (&(FillData) {
                                            .path = gsk_path_ref (gsk_fill_node_get_path (node)),
                                            .color = GSK_RENDER_NODE_TYPE (child) == GSK_COLOR_NODE
                                                   ? *gsk_color_node_get_color (child)
                                                   : GDK_RGBA_WHITE,
                                            .fill_rule = gsk_fill_node_get_fill_rule (node)
                                        }, sizeof (FillData)),
                                        (GDestroyNotify) gsk_fill_data_free);
  g_return_if_fail (mask_image != NULL);
  if (GSK_RENDER_NODE_TYPE (child) == GSK_COLOR_NODE)
    {
      gsk_gpu_node_processor_image_op (self,
                                       mask_image,
                                       &clip_bounds,
                                       &clip_bounds);
      return;
    }

  source_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                           0,
                                                           GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                           &clip_bounds,
                                                           child,
                                                           &source_rect);
  if (source_image == NULL)
    return;

  gsk_gpu_node_processor_add_images (self,
                                     2,
                                     (GskGpuImage *[2]) { source_image, mask_image },
                                     (GskGpuSampler[2]) { GSK_GPU_SAMPLER_DEFAULT, GSK_GPU_SAMPLER_DEFAULT },
                                     descriptors);

  gsk_gpu_mask_op (self->frame,
                   gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &clip_bounds),
                   self->desc,
                   &clip_bounds,
                   &self->offset,
                   self->opacity,
                   GSK_MASK_MODE_ALPHA,
                   descriptors[0],
                   &source_rect,
                   descriptors[1],
                   &clip_bounds);

  g_object_unref (source_image);
}

typedef struct _StrokeData StrokeData;
struct _StrokeData
{
  GskPath *path;
  GdkRGBA color;
  GskStroke stroke;
};

static void
gsk_stroke_data_free (gpointer data)
{
  StrokeData *stroke = data;

  gsk_path_unref (stroke->path);
  gsk_stroke_clear (&stroke->stroke);
  g_free (stroke);
}

static void
gsk_gpu_node_processor_stroke_path (gpointer  data,
                                    cairo_t  *cr)
{
  StrokeData *stroke = data;

  gsk_stroke_to_cairo (&stroke->stroke, cr);
  gsk_path_to_cairo (stroke->path, cr);
  gdk_cairo_set_source_rgba (cr, &stroke->color);
  cairo_stroke (cr);
}

static void
gsk_gpu_node_processor_add_stroke_node (GskGpuNodeProcessor *self,
                                        GskRenderNode       *node)
{
  graphene_rect_t clip_bounds, source_rect;
  GskGpuImage *mask_image, *source_image;
  guint32 descriptors[2];
  GskRenderNode *child;

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clip_bounds))
    return;
  rect_round_to_pixels (&clip_bounds, &self->scale, &self->offset, &clip_bounds);

  child = gsk_stroke_node_get_child (node);

  mask_image = gsk_gpu_upload_cairo_op (self->frame,
                                        &self->scale,
                                        &clip_bounds,
                                        gsk_gpu_node_processor_stroke_path,
                                        g_memdup (&(StrokeData) {
                                            .path = gsk_path_ref (gsk_stroke_node_get_path (node)),
                                            .color = GSK_RENDER_NODE_TYPE (child) == GSK_COLOR_NODE
                                                   ? *gsk_color_node_get_color (child)
                                                   : GDK_RGBA_WHITE,
                                            .stroke = GSK_STROKE_INIT_COPY (gsk_stroke_node_get_stroke (node))
                                        }, sizeof (StrokeData)),
                                        (GDestroyNotify) gsk_stroke_data_free);
  g_return_if_fail (mask_image != NULL);
  if (GSK_RENDER_NODE_TYPE (child) == GSK_COLOR_NODE)
    {
      gsk_gpu_node_processor_image_op (self,
                                       mask_image,
                                       &clip_bounds,
                                       &clip_bounds);
      return;
    }

  source_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                           0,
                                                           GSK_GPU_IMAGE_STRAIGHT_ALPHA,
                                                           &clip_bounds,
                                                           child,
                                                           &source_rect);
  if (source_image == NULL)
    return;

  gsk_gpu_node_processor_add_images (self,
                                     2,
                                     (GskGpuImage *[2]) { source_image, mask_image },
                                     (GskGpuSampler[2]) { GSK_GPU_SAMPLER_DEFAULT, GSK_GPU_SAMPLER_DEFAULT },
                                     descriptors);

  gsk_gpu_mask_op (self->frame,
                   gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &clip_bounds),
                   self->desc,
                   &clip_bounds,
                   &self->offset,
                   self->opacity,
                   GSK_MASK_MODE_ALPHA,
                   descriptors[0],
                   &source_rect,
                   descriptors[1],
                   &clip_bounds);

  g_object_unref (source_image);
}

static void
gsk_gpu_node_processor_add_subsurface_node (GskGpuNodeProcessor *self,
                                            GskRenderNode       *node)
{
  GdkSubsurface *subsurface;

  subsurface = gsk_subsurface_node_get_subsurface (node);
  if (subsurface == NULL ||
      gdk_subsurface_get_texture (subsurface) == NULL ||
      gdk_subsurface_get_parent (subsurface) != gdk_draw_context_get_surface (gsk_gpu_frame_get_context (self->frame)))
    {
      gsk_gpu_node_processor_add_node (self, gsk_subsurface_node_get_child (node));
      return;
    }

  if (!gdk_subsurface_is_above_parent (subsurface))
    {
      cairo_rectangle_int_t int_clipped;
      graphene_rect_t rect, clipped;

      graphene_rect_offset_r (&node->bounds,
                              self->offset.x, self->offset.y,
                              &rect);
      gsk_rect_intersection (&self->clip.rect.bounds, &rect, &clipped);

      if (gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_CLEAR) &&
          node->bounds.size.width * node->bounds.size.height > 100 * 100 && /* not worth the effort for small images */
          (self->clip.type != GSK_GPU_CLIP_ROUNDED ||
           gsk_gpu_clip_contains_rect (&self->clip, &GRAPHENE_POINT_INIT(0,0), &clipped)) &&
          gsk_gpu_node_processor_rect_is_integer (self, &clipped, &int_clipped))
        {
          if (gdk_rectangle_intersect (&int_clipped, &self->scissor, &int_clipped))
            {
              gsk_gpu_clear_op (self->frame,
                                &int_clipped,
                                &GDK_RGBA_TRANSPARENT);
            }
        }
      else
        {
          self->blend = GSK_GPU_BLEND_CLEAR;
          self->pending_globals |= GSK_GPU_GLOBAL_BLEND;
          gsk_gpu_node_processor_sync_globals (self, 0);

          gsk_gpu_color_op (self->frame,
                            gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                            &node->bounds,
                            &self->offset,
                            &GDK_RGBA_WHITE);

          self->blend = GSK_GPU_BLEND_OVER;
          self->pending_globals |= GSK_GPU_GLOBAL_BLEND;
        }
    }
}

static gboolean
gsk_gpu_node_processor_create_subsurface_pattern (GskGpuPatternWriter *self,
                                                  GskRenderNode       *node)
{
  /* This can only ever happen if no offloading is happening */
  g_assert (gsk_subsurface_node_get_subsurface (node) == NULL ||
            gdk_subsurface_get_texture (gsk_subsurface_node_get_subsurface (node)) == NULL);

  return gsk_gpu_node_processor_create_node_pattern (self, gsk_subsurface_node_get_child (node));
}

static void
gsk_gpu_node_processor_add_container_node (GskGpuNodeProcessor *self,
                                           GskRenderNode       *node)
{
  if (self->opacity < 1.0 && !gsk_container_node_is_disjoint (node))
    {
      gsk_gpu_node_processor_add_without_opacity (self, node);
      return;
    }

  for (guint i = 0; i < gsk_container_node_get_n_children (node); i++)
    gsk_gpu_node_processor_add_node (self, gsk_container_node_get_child (node, i));
}

static gboolean
gsk_gpu_node_processor_create_debug_pattern (GskGpuPatternWriter *self,
                                             GskRenderNode       *node)
{
  return gsk_gpu_node_processor_create_node_pattern (self, gsk_debug_node_get_child (node));
}

static void
gsk_gpu_node_processor_add_debug_node (GskGpuNodeProcessor *self,
                                       GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_node (self, gsk_debug_node_get_child (node));
}

typedef enum {
  GSK_GPU_HANDLE_OPACITY = (1 << 0)
} GskGpuNodeFeatures;

static const struct
{
  GskGpuGlobals ignored_globals;
  GskGpuNodeFeatures features;
  void                  (* process_node)                        (GskGpuNodeProcessor    *self,
                                                                 GskRenderNode          *node);
  gboolean              (* create_pattern)                      (GskGpuPatternWriter    *self,
                                                                 GskRenderNode          *node);
} nodes_vtable[] = {
  [GSK_NOT_A_RENDER_NODE] = {
    0,
    0,
    NULL,
    NULL,
  },
  [GSK_CONTAINER_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_container_node,
    NULL,
  },
  [GSK_CAIRO_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    NULL,
    NULL,
  },
  [GSK_COLOR_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_color_node,
    gsk_gpu_node_processor_create_color_pattern,
  },
  [GSK_LINEAR_GRADIENT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_linear_gradient_node,
    gsk_gpu_node_processor_create_linear_gradient_pattern,
  },
  [GSK_REPEATING_LINEAR_GRADIENT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_linear_gradient_node,
    gsk_gpu_node_processor_create_linear_gradient_pattern,
  },
  [GSK_RADIAL_GRADIENT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_radial_gradient_node,
    gsk_gpu_node_processor_create_radial_gradient_pattern,
  },
  [GSK_REPEATING_RADIAL_GRADIENT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_radial_gradient_node,
    gsk_gpu_node_processor_create_radial_gradient_pattern,
  },
  [GSK_CONIC_GRADIENT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_conic_gradient_node,
    gsk_gpu_node_processor_create_conic_gradient_pattern,
  },
  [GSK_BORDER_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_border_node,
    NULL,
  },
  [GSK_TEXTURE_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_texture_node,
    gsk_gpu_node_processor_create_texture_pattern,
  },
  [GSK_INSET_SHADOW_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_inset_shadow_node,
    NULL,
  },
  [GSK_OUTSET_SHADOW_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_outset_shadow_node,
    NULL,
  },
  [GSK_TRANSFORM_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_transform_node,
    gsk_gpu_node_processor_create_transform_pattern,
  },
  [GSK_OPACITY_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_opacity_node,
    gsk_gpu_node_processor_create_opacity_pattern,
  },
  [GSK_COLOR_MATRIX_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_color_matrix_node,
    gsk_gpu_node_processor_create_color_matrix_pattern
  },
  [GSK_REPEAT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_repeat_node,
    gsk_gpu_node_processor_create_repeat_pattern
  },
  [GSK_CLIP_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_clip_node,
    gsk_gpu_node_processor_create_clip_pattern,
  },
  [GSK_ROUNDED_CLIP_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_rounded_clip_node,
    NULL,
  },
  [GSK_SHADOW_NODE] = {
    0,
    0,
    gsk_gpu_node_processor_add_shadow_node,
    NULL,
  },
  [GSK_BLEND_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_blend_node,
    gsk_gpu_node_processor_create_blend_pattern,
  },
  [GSK_CROSS_FADE_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_cross_fade_node,
    gsk_gpu_node_processor_create_cross_fade_pattern,
  },
  [GSK_TEXT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_glyph_node,
    gsk_gpu_node_processor_create_glyph_pattern,
  },
  [GSK_BLUR_NODE] = {
    0,
    0,
    gsk_gpu_node_processor_add_blur_node,
    NULL,
  },
  [GSK_DEBUG_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_debug_node,
    gsk_gpu_node_processor_create_debug_pattern,
  },
  [GSK_GL_SHADER_NODE] = {
    0,
    0,
    NULL,
    NULL,
  },
  [GSK_TEXTURE_SCALE_NODE] = {
    0,
    0,
    gsk_gpu_node_processor_add_texture_scale_node,
    NULL,
  },
  [GSK_MASK_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_mask_node,
    gsk_gpu_node_processor_create_mask_pattern,
  },
  [GSK_FILL_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_fill_node,
    NULL,
  },
  [GSK_STROKE_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_stroke_node,
    NULL,
  },
  [GSK_SUBSURFACE_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_subsurface_node,
    gsk_gpu_node_processor_create_subsurface_pattern,
  },
};

static void
gsk_gpu_node_processor_add_node (GskGpuNodeProcessor *self,
                                 GskRenderNode       *node)
{
  GskRenderNodeType node_type;

  /* This catches the corner cases of empty nodes, so after this check
   * there's quaranteed to be at least 1 pixel that needs to be drawn
   */
  if (node->bounds.size.width == 0 || node->bounds.size.height == 0)
    return;

  if (!gsk_gpu_clip_may_intersect_rect (&self->clip, &self->offset, &node->bounds))
    return;

  node_type = gsk_render_node_get_node_type (node);
  if (node_type >= G_N_ELEMENTS (nodes_vtable))
    {
      g_critical ("unknown node type %u for %s", node_type, g_type_name_from_instance ((GTypeInstance *) node));
      gsk_gpu_node_processor_add_fallback_node (self, node);
      return;
    }

  if (self->opacity < 1.0 && (nodes_vtable[node_type].features & GSK_GPU_HANDLE_OPACITY) == 0)
    {
      gsk_gpu_node_processor_add_without_opacity (self, node);
      return;
    }

  gsk_gpu_node_processor_sync_globals (self, nodes_vtable[node_type].ignored_globals);
  g_assert ((self->pending_globals & ~nodes_vtable[node_type].ignored_globals) == 0);

  if (nodes_vtable[node_type].process_node)
    {
      nodes_vtable[node_type].process_node (self, node);
    }
  else
    {
      GSK_DEBUG (FALLBACK, "Unsupported node '%s'",
                 g_type_name_from_instance ((GTypeInstance *) node));
      gsk_gpu_node_processor_add_fallback_node (self, node);
    }
}

static gboolean
gsk_gpu_node_processor_create_node_pattern (GskGpuPatternWriter *self,
                                            GskRenderNode       *node)
{
  GskRenderNodeType node_type;
  graphene_rect_t bounds;
  GskGpuImage *image;
  guint32 tex_id;

  if (!gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_UBER))
    return FALSE;

  /* This catches the corner cases of empty nodes, so after this check
   * there's quaranteed to be at least 1 pixel that needs to be drawn
   */
  if (node->bounds.size.width == 0 || node->bounds.size.height == 0)
    return TRUE;

  node_type = gsk_render_node_get_node_type (node);
  if (node_type >= G_N_ELEMENTS (nodes_vtable))
    {
      g_critical ("unknown node type %u for %s", node_type, g_type_name_from_instance ((GTypeInstance *) node));
      return FALSE;
    }

  if (nodes_vtable[node_type].create_pattern != NULL)
    {
      gsize size_before = pattern_buffer_get_size (&self->buffer);
      gsize images_before = self->desc ? gsk_gpu_descriptors_get_n_images (self->desc) : 0;
      gsize buffers_before = self->desc ? gsk_gpu_descriptors_get_n_buffers (self->desc) : 0;
      if (nodes_vtable[node_type].create_pattern (self, node))
        return TRUE;
      pattern_buffer_set_size (&self->buffer, size_before);
      if (self->desc)
        gsk_gpu_descriptors_set_size (self->desc, images_before, buffers_before);
    }

  rect_round_to_pixels (&GRAPHENE_RECT_INIT (
                            self->bounds.origin.x - self->offset.x,
                            self->bounds.origin.y - self->offset.y,
                            self->bounds.size.width,
                            self->bounds.size.height
                        ),
                        &self->scale,
                        &self->offset,
                        &bounds);
  image = gsk_gpu_get_node_as_image (self->frame,
                                     &bounds,
                                     &self->scale,
                                     node,
                                     &bounds);
  if (image == NULL)
    {
      gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_COLOR);
      gsk_gpu_pattern_writer_append_rgba (self, &GDK_RGBA_TRANSPARENT);
      return TRUE;
    }

  if (!gsk_gpu_pattern_writer_add_image (self, image, GSK_GPU_SAMPLER_DEFAULT, &tex_id))
    {
      g_object_unref (image);
      return FALSE;
    }

  if (gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_STRAIGHT_ALPHA)
    gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_STRAIGHT_ALPHA);
  else
    gsk_gpu_pattern_writer_append_uint (self, GSK_GPU_PATTERN_TEXTURE);
  gsk_gpu_pattern_writer_append_uint (self, tex_id);
  gsk_gpu_pattern_writer_append_rect (self, &bounds, &self->offset);

  g_object_unref (image);

  return TRUE;
}

