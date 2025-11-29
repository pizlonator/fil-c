#pragma once

#include "gskgpurenderer.h"

#include "gskgputypesprivate.h"
#include "gskrendererprivate.h"

G_BEGIN_DECLS

#define GSK_GPU_RENDERER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_GPU_RENDERER, GskGpuRendererClass))
#define GSK_IS_GPU_RENDERER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_GPU_RENDERER))
#define GSK_GPU_RENDERER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_GPU_RENDERER, GskGpuRendererClass))

typedef struct _GskGpuRendererClass           GskGpuRendererClass;

struct _GskGpuRenderer
{
  GskRenderer parent_instance;
};

struct _GskGpuRendererClass
{
  GskRendererClass parent_class;

  GType frame_type;
  GskGpuOptimizations optimizations; /* subclasses cannot override this */

  GskGpuDevice *        (* get_device)                                  (GdkDisplay             *display,
                                                                         GError                **error);
  GdkDrawContext *      (* create_context)                              (GskGpuRenderer         *self,
                                                                         GdkDisplay             *display,
                                                                         GdkSurface             *surface,
                                                                         GskGpuOptimizations    *supported,
                                                                         GError                **error);

  void                  (* make_current)                                (GskGpuRenderer         *self);
  GskGpuImage *         (* get_backbuffer)                              (GskGpuRenderer         *self);

  double                (* get_scale)                                   (GskGpuRenderer         *self);
  GdkDmabufFormats *    (* get_dmabuf_formats)                          (GskGpuRenderer         *self);
};

GdkDrawContext *        gsk_gpu_renderer_get_context                    (GskGpuRenderer         *self);
GskGpuDevice *          gsk_gpu_renderer_get_device                     (GskGpuRenderer         *self);
double                  gsk_gpu_renderer_get_scale                      (GskGpuRenderer         *self);

G_END_DECLS

