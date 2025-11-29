#include "config.h"

#include "gskgpuimageprivate.h"

typedef struct _GskGpuImagePrivate GskGpuImagePrivate;

struct _GskGpuImagePrivate
{
  GskGpuImageFlags flags;
  GdkMemoryFormat format;
  gsize width;
  gsize height;
};

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuImage, gsk_gpu_image, G_TYPE_OBJECT)

static void
gsk_gpu_image_get_default_projection_matrix (GskGpuImage       *self,
                                             graphene_matrix_t *out_projection)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  graphene_matrix_init_ortho (out_projection,
                              0, priv->width,
                              0, priv->height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);
}

static void
gsk_gpu_image_texture_toggle_ref_cb (gpointer  texture,
                                     GObject  *image,
                                     gboolean  is_last_ref)
{
  if (is_last_ref)
    g_object_unref (texture);
  else
    g_object_ref (texture);
}

static void
gsk_gpu_image_dispose (GObject *object)
{
  GskGpuImage *self = GSK_GPU_IMAGE (object);
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  if (priv->flags & GSK_GPU_IMAGE_TOGGLE_REF)
    {
      priv->flags &= ~GSK_GPU_IMAGE_TOGGLE_REF;
      G_OBJECT (self)->ref_count++;
      g_object_remove_toggle_ref (G_OBJECT (self), gsk_gpu_image_texture_toggle_ref_cb, NULL);
    }

  G_OBJECT_CLASS (gsk_gpu_image_parent_class)->dispose (object);
}

static void
gsk_gpu_image_class_init (GskGpuImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_gpu_image_dispose;

  klass->get_projection_matrix = gsk_gpu_image_get_default_projection_matrix;
}

static void
gsk_gpu_image_init (GskGpuImage *self)
{
}

void
gsk_gpu_image_setup (GskGpuImage      *self,
                     GskGpuImageFlags  flags,
                     GdkMemoryFormat   format,
                     gsize             width,
                     gsize             height)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  priv->flags = flags;
  priv->format = format;
  priv->width = width;
  priv->height = height;
}

/*
 * gsk_gpu_image_toggle_ref_texture:
 * @self: a GskGpuImage
 * @texture: the texture owning @self
 *
 * This function must be called whenever the texture owns the data
 * used by the image. It will then add a toggle ref, so that ref'ing
 * the image will ref the texture and unrefing the image will unref it
 * again.
 *
 * This ensures that whenever the image is used, the texture will keep
 * being referenced and not go away. But once all the image's references
 * get unref'ed, the texture is free to go away.
 **/
void
gsk_gpu_image_toggle_ref_texture (GskGpuImage *self,
                                  GdkTexture  *texture)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  g_assert ((priv->flags & GSK_GPU_IMAGE_TOGGLE_REF) == 0);

  priv->flags |= GSK_GPU_IMAGE_TOGGLE_REF;
  g_object_ref (texture);
  g_object_add_toggle_ref (G_OBJECT (self), gsk_gpu_image_texture_toggle_ref_cb, texture);
  g_object_unref (self);
}
                     
GdkMemoryFormat
gsk_gpu_image_get_format (GskGpuImage *self)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  return priv->format;
}

gsize
gsk_gpu_image_get_width (GskGpuImage *self)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  return priv->width;
}

gsize
gsk_gpu_image_get_height (GskGpuImage *self)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  return priv->height;
}

GskGpuImageFlags
gsk_gpu_image_get_flags (GskGpuImage *self)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  return priv->flags;
}

void
gsk_gpu_image_set_flags (GskGpuImage      *self,
                         GskGpuImageFlags  flags)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  priv->flags |= flags;
}

void
gsk_gpu_image_get_projection_matrix (GskGpuImage       *self,
                                     graphene_matrix_t *out_projection)
{
  GSK_GPU_IMAGE_GET_CLASS (self)->get_projection_matrix (self, out_projection);
}
