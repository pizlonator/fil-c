#pragma once

#include "gskgputypesprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

#define GSK_TYPE_GPU_IMAGE         (gsk_gpu_image_get_type ())
#define GSK_GPU_IMAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSK_TYPE_GPU_IMAGE, GskGpuImage))
#define GSK_GPU_IMAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSK_TYPE_GPU_IMAGE, GskGpuImageClass))
#define GSK_IS_GPU_IMAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSK_TYPE_GPU_IMAGE))
#define GSK_IS_GPU_IMAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSK_TYPE_GPU_IMAGE))
#define GSK_GPU_IMAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSK_TYPE_GPU_IMAGE, GskGpuImageClass))

typedef struct _GskGpuImageClass GskGpuImageClass;

struct _GskGpuImage
{
  GObject parent_instance;
};

struct _GskGpuImageClass
{
  GObjectClass parent_class;

  void                  (* get_projection_matrix)                       (GskGpuImage            *self,
                                                                         graphene_matrix_t      *out_projection);
};

GType                   gsk_gpu_image_get_type                          (void) G_GNUC_CONST;

void                    gsk_gpu_image_setup                             (GskGpuImage            *self,
                                                                         GskGpuImageFlags        flags,
                                                                         GdkMemoryFormat         format,
                                                                         gsize                   width,
                                                                         gsize                   height);
void                    gsk_gpu_image_toggle_ref_texture                (GskGpuImage            *self,
                                                                         GdkTexture             *texture);

GdkMemoryFormat         gsk_gpu_image_get_format                        (GskGpuImage            *self);
gsize                   gsk_gpu_image_get_width                         (GskGpuImage            *self);
gsize                   gsk_gpu_image_get_height                        (GskGpuImage            *self);
GskGpuImageFlags        gsk_gpu_image_get_flags                         (GskGpuImage            *self);
void                    gsk_gpu_image_set_flags                         (GskGpuImage            *self,
                                                                         GskGpuImageFlags        flags);

void                    gsk_gpu_image_get_projection_matrix             (GskGpuImage            *self,
                                                                         graphene_matrix_t      *out_projection);


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuImage, g_object_unref)

G_END_DECLS
