#include "config.h"

#include "gskgldescriptorsprivate.h"

#include "gskglbufferprivate.h"
#include "gskglimageprivate.h"

struct _GskGLDescriptors
{
  GskGpuDescriptors parent_instance;

  GskGLDevice *device;
  guint n_external;
};

G_DEFINE_TYPE (GskGLDescriptors, gsk_gl_descriptors, GSK_TYPE_GPU_DESCRIPTORS)

static void
gsk_gl_descriptors_finalize (GObject *object)
{
  GskGLDescriptors *self = GSK_GL_DESCRIPTORS (object);

  g_object_unref (self->device);

  G_OBJECT_CLASS (gsk_gl_descriptors_parent_class)->finalize (object);
}

static gboolean
gsk_gl_descriptors_add_image (GskGpuDescriptors *desc,
                              GskGpuImage       *image,
                              GskGpuSampler      sampler,
                              guint32           *out_descriptor)
{
  GskGLDescriptors *self = GSK_GL_DESCRIPTORS (desc);
  gsize used_texture_units;

  used_texture_units = gsk_gpu_descriptors_get_n_images (desc) + 2 * self->n_external;

  if (gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_EXTERNAL)
    {
      if (16 - used_texture_units < 3)
        return FALSE;

      *out_descriptor = (self->n_external << 1) | 1;
      self->n_external++;
      return TRUE;
    }
  else
    {
      if (used_texture_units >= 16)
        return FALSE;

      *out_descriptor = (gsk_gpu_descriptors_get_n_images (desc) - self->n_external) << 1;
      return TRUE;
    }
}

static gboolean
gsk_gl_descriptors_add_buffer (GskGpuDescriptors *desc,
                               GskGpuBuffer      *buffer,
                               guint32           *out_descriptor)
{
  gsize used_buffers;

  used_buffers = gsk_gpu_descriptors_get_n_buffers (desc);

  if (used_buffers >= 11)
    return FALSE;

  *out_descriptor = used_buffers;

  return TRUE;
}

static void
gsk_gl_descriptors_class_init (GskGLDescriptorsClass *klass)
{
  GskGpuDescriptorsClass *descriptors_class = GSK_GPU_DESCRIPTORS_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gsk_gl_descriptors_finalize;

  descriptors_class->add_image = gsk_gl_descriptors_add_image;
  descriptors_class->add_buffer = gsk_gl_descriptors_add_buffer;
}

static void
gsk_gl_descriptors_init (GskGLDescriptors *self)
{
}

GskGpuDescriptors *
gsk_gl_descriptors_new (GskGLDevice *device)
{
  GskGLDescriptors *self;

  self = g_object_new (GSK_TYPE_GL_DESCRIPTORS, NULL);

  self->device = g_object_ref (device);

  return GSK_GPU_DESCRIPTORS (self);
}

guint
gsk_gl_descriptors_get_n_external (GskGLDescriptors *self)
{
  return self->n_external;
}

void
gsk_gl_descriptors_use (GskGLDescriptors *self)
{
  GskGpuDescriptors *desc = GSK_GPU_DESCRIPTORS (self);
  gsize i, ext, n_textures;

  n_textures = 16 - 3 * self->n_external;
  ext = 0;

  for (i = 0; i < gsk_gpu_descriptors_get_n_images (desc); i++)
    {
      GskGLImage *image = GSK_GL_IMAGE (gsk_gpu_descriptors_get_image (desc, i));

      if (gsk_gpu_image_get_flags (GSK_GPU_IMAGE (image)) & GSK_GPU_IMAGE_EXTERNAL)
        {
          glActiveTexture (GL_TEXTURE0 + n_textures + 3 * ext);
          gsk_gl_image_bind_texture (image);
          ext++;
        }
      else
        {
          glActiveTexture (GL_TEXTURE0 + i - ext);
          gsk_gl_image_bind_texture (image);
          glBindSampler (i - ext, gsk_gl_device_get_sampler_id (self->device, gsk_gpu_descriptors_get_sampler (desc, i)));
        }
    }

  for (i = 0; i < gsk_gpu_descriptors_get_n_buffers (desc); i++)
    {
      GskGLBuffer *buffer = GSK_GL_BUFFER (gsk_gpu_descriptors_get_buffer (desc, i));

      /* index 0 are the globals, we start at 1 */
      gsk_gl_buffer_bind_base (buffer, i + 1);
    }
}
