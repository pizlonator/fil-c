#pragma once

#include "gskgputypesprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GPU_DESCRIPTORS         (gsk_gpu_descriptors_get_type ())
#define GSK_GPU_DESCRIPTORS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSK_TYPE_GPU_DESCRIPTORS, GskGpuDescriptors))
#define GSK_GPU_DESCRIPTORS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSK_TYPE_GPU_DESCRIPTORS, GskGpuDescriptorsClass))
#define GSK_IS_GPU_DESCRIPTORS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSK_TYPE_GPU_DESCRIPTORS))
#define GSK_IS_GPU_DESCRIPTORS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSK_TYPE_GPU_DESCRIPTORS))
#define GSK_GPU_DESCRIPTORS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSK_TYPE_GPU_DESCRIPTORS, GskGpuDescriptorsClass))

typedef struct _GskGpuDescriptorsClass GskGpuDescriptorsClass;

struct _GskGpuDescriptors
{
  GObject parent_instance;
};

struct _GskGpuDescriptorsClass
{
  GObjectClass parent_class;

  gboolean              (* add_image)                                   (GskGpuDescriptors      *self,
                                                                         GskGpuImage            *image,
                                                                         GskGpuSampler           sampler,
                                                                         guint32                *out_id);
  gboolean              (* add_buffer)                                  (GskGpuDescriptors      *self,
                                                                         GskGpuBuffer           *buffer,
                                                                         guint32                *out_id);
};

GType                   gsk_gpu_descriptors_get_type                    (void) G_GNUC_CONST;

gsize                   gsk_gpu_descriptors_get_n_images                (GskGpuDescriptors      *self);
gsize                   gsk_gpu_descriptors_get_n_buffers               (GskGpuDescriptors      *self);
void                    gsk_gpu_descriptors_set_size                    (GskGpuDescriptors      *self,
                                                                         gsize                   n_images,
                                                                         gsize                   n_buffers);
GskGpuImage *           gsk_gpu_descriptors_get_image                   (GskGpuDescriptors      *self,
                                                                         gsize                   id);
GskGpuSampler           gsk_gpu_descriptors_get_sampler                 (GskGpuDescriptors      *self,
                                                                         gsize                   id);
gsize                   gsk_gpu_descriptors_find_image                  (GskGpuDescriptors      *self,
                                                                         guint32                 descriptor);
GskGpuBuffer *          gsk_gpu_descriptors_get_buffer                  (GskGpuDescriptors      *self,
                                                                         gsize                   id);

gboolean                gsk_gpu_descriptors_add_image                   (GskGpuDescriptors      *self,
                                                                         GskGpuImage            *image,
                                                                         GskGpuSampler           sampler,
                                                                         guint32                *out_descriptor);
gboolean                gsk_gpu_descriptors_add_buffer                  (GskGpuDescriptors      *self,
                                                                         GskGpuBuffer           *buffer,
                                                                         guint32                *out_descriptor);


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuDescriptors, g_object_unref)

G_END_DECLS
