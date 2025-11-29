#pragma once

#include "gskgputypesprivate.h"

#include <gdk/gdk.h>
#include <graphene.h>
#include <gsk/gskroundedrect.h>

G_BEGIN_DECLS

typedef enum {
  /* The whole area is clipped, no drawing is necessary.
   * This can't be handled by return values because for return
   * values we return if clips could even be computed.
   */
  GSK_GPU_CLIP_ALL_CLIPPED,
  /* No clipping is necessary, but the clip rect is set
   * to the actual bounds of the underlying framebuffer
   * or handled via the scissor.
   */
  GSK_GPU_CLIP_NONE,
  /* The clip exists outside the rect, so clipping must
   * happen if rendering can't be proven to stay in the rect */
  GSK_GPU_CLIP_CONTAINED,
  /* The clip is a rectangular area */
  GSK_GPU_CLIP_RECT,
  /* The clip is a rounded rectangle */
  GSK_GPU_CLIP_ROUNDED
} GskGpuClipComplexity;

typedef struct _GskGpuClip GskGpuClip;

struct _GskGpuClip
{
  GskGpuClipComplexity type;
  GskRoundedRect       rect;
};

void                    gsk_gpu_clip_init_empty                         (GskGpuClip          *clip,
                                                                         const graphene_rect_t  *rect);
void                    gsk_gpu_clip_init_contained                     (GskGpuClip          *clip,
                                                                         const graphene_rect_t  *rect);
void                    gsk_gpu_clip_init_copy                          (GskGpuClip          *self,
                                                                         const GskGpuClip    *src);
void                    gsk_gpu_clip_init_rect                          (GskGpuClip          *clip,
                                                                         const graphene_rect_t  *rect);

gboolean                gsk_gpu_clip_intersect_rect                     (GskGpuClip          *dest,
                                                                         const GskGpuClip    *src,
                                                                         const graphene_rect_t  *rect) G_GNUC_WARN_UNUSED_RESULT;
gboolean                gsk_gpu_clip_intersect_rounded_rect             (GskGpuClip          *dest,
                                                                         const GskGpuClip    *src,
                                                                         const GskRoundedRect   *rounded) G_GNUC_WARN_UNUSED_RESULT;
void                    gsk_gpu_clip_scale                              (GskGpuClip          *dest,
                                                                         const GskGpuClip    *src,
                                                                         float                   scale_x,
                                                                         float                   scale_y);
gboolean                gsk_gpu_clip_transform                          (GskGpuClip          *dest,
                                                                         const GskGpuClip    *src,
                                                                         GskTransform           *transform,
                                                                         const graphene_rect_t  *viewport) G_GNUC_WARN_UNUSED_RESULT;

gboolean                gsk_gpu_clip_contains_rect                      (const GskGpuClip    *self,
                                                                         const graphene_point_t *offset,
                                                                         const graphene_rect_t  *rect) G_GNUC_WARN_UNUSED_RESULT;
gboolean                gsk_gpu_clip_may_intersect_rect                 (const GskGpuClip    *self,
                                                                         const graphene_point_t *offset,
                                                                         const graphene_rect_t  *rect) G_GNUC_WARN_UNUSED_RESULT;
GskGpuShaderClip        gsk_gpu_clip_get_shader_clip                    (const GskGpuClip    *self,
                                                                         const graphene_point_t *offset,
                                                                         const graphene_rect_t  *rect);

G_END_DECLS

