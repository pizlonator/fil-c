#pragma once

#include "gskgputypesprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GPU_BUFFER         (gsk_gpu_buffer_get_type ())
#define GSK_GPU_BUFFER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSK_TYPE_GPU_BUFFER, GskGpuBuffer))
#define GSK_GPU_BUFFER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSK_TYPE_GPU_BUFFER, GskGpuBufferClass))
#define GSK_IS_GPU_BUFFER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSK_TYPE_GPU_BUFFER))
#define GSK_IS_GPU_BUFFER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSK_TYPE_GPU_BUFFER))
#define GSK_GPU_BUFFER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSK_TYPE_GPU_BUFFER, GskGpuBufferClass))

typedef struct _GskGpuBufferClass GskGpuBufferClass;

struct _GskGpuBuffer
{
  GObject parent_instance;
};

struct _GskGpuBufferClass
{
  GObjectClass parent_class;

  guchar *              (* map)                                         (GskGpuBuffer           *self);
  void                  (* unmap)                                       (GskGpuBuffer           *self,
                                                                         gsize                   used);
};

GType                   gsk_gpu_buffer_get_type                         (void) G_GNUC_CONST;

void                    gsk_gpu_buffer_setup                            (GskGpuBuffer           *self,
                                                                         gsize                   size);

gsize                   gsk_gpu_buffer_get_size                         (GskGpuBuffer           *self);

guchar *                gsk_gpu_buffer_map                              (GskGpuBuffer           *self);
void                    gsk_gpu_buffer_unmap                            (GskGpuBuffer           *self,
                                                                         gsize                   used);


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuBuffer, g_object_unref)

G_END_DECLS
