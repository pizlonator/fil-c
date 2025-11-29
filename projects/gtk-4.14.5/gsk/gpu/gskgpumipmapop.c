#include "config.h"

#include "gskgpumipmapopprivate.h"

#include "gskglimageprivate.h"
#include "gskgpuprintprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkanimageprivate.h"
#endif

typedef struct _GskGpuMipmapOp GskGpuMipmapOp;

struct _GskGpuMipmapOp
{
  GskGpuOp op;

  GskGpuImage *image;
};

static void
gsk_gpu_mipmap_op_finish (GskGpuOp *op)
{
  GskGpuMipmapOp *self = (GskGpuMipmapOp *) op;

  g_object_unref (self->image);
}

static void
gsk_gpu_mipmap_op_print (GskGpuOp    *op,
                         GskGpuFrame *frame,
                         GString     *string,
                         guint        indent)
{
  GskGpuMipmapOp *self = (GskGpuMipmapOp *) op;

  gsk_gpu_print_op (string, indent, "mipmap");
  gsk_gpu_print_image (string, self->image);
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN
static GskGpuOp *
gsk_gpu_mipmap_op_vk_command (GskGpuOp              *op,
                              GskGpuFrame           *frame,
                              GskVulkanCommandState *state)
{
  GskGpuMipmapOp *self = (GskGpuMipmapOp *) op;
  GskVulkanImage *image;
  VkImage vk_image;
  gsize width, height;
  guint i, n_levels;

  image = GSK_VULKAN_IMAGE (self->image);
  vk_image = gsk_vulkan_image_get_vk_image (image);
  width = gsk_gpu_image_get_width (self->image);
  height = gsk_gpu_image_get_height (self->image);
  n_levels = gsk_vulkan_mipmap_levels (width, height);

  /* optimize me: only transition mipmap layers 1..n, but not 0 */
  gsk_vulkan_image_transition (image,
                               state->semaphores,
                               state->vk_command_buffer,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_ACCESS_TRANSFER_WRITE_BIT);

  for (i = 0; /* we break after the barrier */ ; i++)
    {
      vkCmdPipelineBarrier (state->vk_command_buffer,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            0,
                            0, NULL,
                            0, NULL,
                            1, &(VkImageMemoryBarrier) {
                                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .image = vk_image,
                                .subresourceRange = {
                                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = i,
                                  .levelCount = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1
                                },
                            });
      if (i + 1 == n_levels)
        break;
      vkCmdBlitImage (state->vk_command_buffer,
                      vk_image,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      vk_image,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      1,
                      &(VkImageBlit) {
                          .srcSubresource = {
                              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .mipLevel = i,
                              .baseArrayLayer = 0,
                              .layerCount = 1
                          },
                          .srcOffsets = {
                              {
                                  .x = 0,
                                  .y = 0,
                                  .z = 0,
                              },
                              {
                                  .x = width,
                                  .y = height,
                                  .z = 1
                              }
                          },
                          .dstSubresource = {
                              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .mipLevel = i + 1,
                              .baseArrayLayer = 0,
                              .layerCount = 1
                          },
                          .dstOffsets = {
                              {
                                  .x = 0,
                                  .y = 0,
                                  .z = 0,
                              },
                              {
                                  .x = MAX (1, width / 2),
                                  .y = MAX (1, height / 2),
                                  .z = 1,
                              }
                          },
                      },
                      VK_FILTER_LINEAR);
      width = MAX (1, width / 2);
      height = MAX (1, height / 2);
    }

  gsk_vulkan_image_set_vk_image_layout (image,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        VK_ACCESS_TRANSFER_READ_BIT);

  return op->next;
}
#endif

static GskGpuOp *
gsk_gpu_mipmap_op_gl_command (GskGpuOp          *op,
                              GskGpuFrame       *frame,
                              GskGLCommandState *state)
{
  GskGpuMipmapOp *self = (GskGpuMipmapOp *) op;

  glActiveTexture (GL_TEXTURE0);
  gsk_gl_image_bind_texture (GSK_GL_IMAGE (self->image));
  /* need to reset the images again */
  state->desc = NULL;

  glGenerateMipmap (GL_TEXTURE_2D);

  return op->next;
}

static const GskGpuOpClass GSK_GPU_MIPMAP_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuMipmapOp),
  GSK_GPU_STAGE_PASS,
  gsk_gpu_mipmap_op_finish,
  gsk_gpu_mipmap_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_mipmap_op_vk_command,
#endif
  gsk_gpu_mipmap_op_gl_command
};

void
gsk_gpu_mipmap_op (GskGpuFrame *frame,
                   GskGpuImage *image)
{
  GskGpuMipmapOp *self;

  g_assert ((gsk_gpu_image_get_flags (image) & (GSK_GPU_IMAGE_CAN_MIPMAP | GSK_GPU_IMAGE_MIPMAP)) == GSK_GPU_IMAGE_CAN_MIPMAP);

  self = (GskGpuMipmapOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_MIPMAP_OP_CLASS);

  self->image = g_object_ref (image);

  gsk_gpu_image_set_flags (image, GSK_GPU_IMAGE_MIPMAP);
}
