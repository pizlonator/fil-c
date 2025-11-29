#include "config.h"

#include "gskgpudownloadopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskglimageprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpuprintprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkanbufferprivate.h"
#include "gskvulkanframeprivate.h"
#include "gskvulkanimageprivate.h"
#endif

#include "gdk/gdkdmabuftextureprivate.h"
#include "gdk/gdkglcontextprivate.h"

#ifdef HAVE_DMABUF
#include <linux/dma-buf.h>
#endif

typedef struct _GskGpuDownloadOp GskGpuDownloadOp;

typedef void (* GdkGpuDownloadOpCreateFunc) (GskGpuDownloadOp *);

struct _GskGpuDownloadOp
{
  GskGpuOp op;

  GskGpuImage *image;
  gboolean allow_dmabuf;
  GdkGpuDownloadOpCreateFunc create_func;
  GskGpuDownloadFunc func;
  gpointer user_data;

  GdkTexture *texture;
  GskGpuBuffer *buffer;
#ifdef GDK_RENDERING_VULKAN
  VkSemaphore vk_semaphore;
#endif
};

static void
gsk_gpu_download_op_finish (GskGpuOp *op)
{
  GskGpuDownloadOp *self = (GskGpuDownloadOp *) op;

  if (self->create_func)
    self->create_func (self);

  self->func (self->user_data, self->texture);

  g_object_unref (self->texture);
  g_object_unref (self->image);
  g_clear_object (&self->buffer);
}

static void
gsk_gpu_download_op_print (GskGpuOp    *op,
                           GskGpuFrame *frame,
                           GString     *string,
                           guint        indent)
{
  GskGpuDownloadOp *self = (GskGpuDownloadOp *) op;

  gsk_gpu_print_op (string, indent, "download");
  gsk_gpu_print_image (string, self->image);
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN

#ifdef HAVE_DMABUF
/* The code needs to run here because vkGetSemaphoreFdKHR() may
 * only be called after the semaphore has been submitted via
 * vkQueueSubmit().
 */
static void
gsk_gpu_download_op_vk_sync_semaphore (GskGpuDownloadOp *self)
{
  PFN_vkGetSemaphoreFdKHR func_vkGetSemaphoreFdKHR;
  GdkDisplay *display;
  int fd, sync_file_fd;

  /* Don't look at where I store my variables plz */
  display = gdk_dmabuf_texture_get_display (GDK_DMABUF_TEXTURE (self->texture));
  fd = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (self->texture))->planes[0].fd;
  func_vkGetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR) vkGetDeviceProcAddr (display->vk_device, "vkGetSemaphoreFdKHR");

  /* vkGetSemaphoreFdKHR implicitly resets the semaphore.
   * But who cares, we're about to delete it. */
  if (GSK_VK_CHECK (func_vkGetSemaphoreFdKHR, display->vk_device,
                                              &(VkSemaphoreGetFdInfoKHR) {
                                                  .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
                                                  .semaphore = self->vk_semaphore,
                                                  .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
                                             },
                                             &sync_file_fd) == VK_SUCCESS)
    {
      gdk_dmabuf_import_sync_file (fd, DMA_BUF_SYNC_WRITE, sync_file_fd);
      
      close (sync_file_fd);
    }

  vkDestroySemaphore (display->vk_device, self->vk_semaphore, NULL);
}
#endif

static void
gsk_gpu_download_op_vk_create (GskGpuDownloadOp *self)
{
  GBytes *bytes;
  guchar *data;
  gsize width, height, stride;
  GdkMemoryFormat format;

  data = gsk_gpu_buffer_map (self->buffer);
  width = gsk_gpu_image_get_width (self->image);
  height = gsk_gpu_image_get_height (self->image);
  format = gsk_gpu_image_get_format (self->image);
  stride = width * gdk_memory_format_bytes_per_pixel (format);
  bytes = g_bytes_new (data, stride * height);
  self->texture = gdk_memory_texture_new (width,
                                          height,
                                          format,
                                          bytes,
                                          stride);
  g_bytes_unref (bytes);
  gsk_gpu_buffer_unmap (self->buffer, 0);
}

static GskGpuOp *
gsk_gpu_download_op_vk_command (GskGpuOp              *op,
                                GskGpuFrame           *frame,
                                GskVulkanCommandState *state)
{
  GskGpuDownloadOp *self = (GskGpuDownloadOp *) op;
  gsize width, height, stride;

#ifdef HAVE_DMABUF
  if (self->allow_dmabuf)
    self->texture = gsk_vulkan_image_to_dmabuf_texture (GSK_VULKAN_IMAGE (self->image));
  if (self->texture)
    {
      GskVulkanDevice *device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame));
      VkDevice vk_device = gsk_vulkan_device_get_vk_device (device);

      gsk_gpu_device_cache_texture_image (GSK_GPU_DEVICE (device), self->texture, gsk_gpu_frame_get_timestamp (frame), self->image);

      if (gsk_vulkan_device_has_feature (device, GDK_VULKAN_FEATURE_SEMAPHORE_EXPORT))
        {
          GSK_VK_CHECK (vkCreateSemaphore, vk_device,
                                           &(VkSemaphoreCreateInfo) {
                                               .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  			                       .pNext = &(VkExportSemaphoreCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
                                                   .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
			                       },
                                           },
                                           NULL,
                                           &self->vk_semaphore);
          gsk_vulkan_semaphores_add_signal (state->semaphores, self->vk_semaphore);

          self->create_func = gsk_gpu_download_op_vk_sync_semaphore;
        }

      return op->next;
    }
#endif

  width = gsk_gpu_image_get_width (self->image);
  height = gsk_gpu_image_get_height (self->image);
  stride = width * gdk_memory_format_bytes_per_pixel (gsk_gpu_image_get_format (self->image));
  self->buffer = gsk_vulkan_buffer_new_read (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                             height * stride);

  gsk_vulkan_image_transition (GSK_VULKAN_IMAGE (self->image),
                               state->semaphores,
                               state->vk_command_buffer,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_ACCESS_TRANSFER_READ_BIT);

  vkCmdCopyImageToBuffer (state->vk_command_buffer,
                          gsk_vulkan_image_get_vk_image (GSK_VULKAN_IMAGE (self->image)),
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (self->buffer)),
                          1,
                          (VkBufferImageCopy[1]) {
                               {
                                   .bufferOffset = 0,
                                   .bufferRowLength = width,
                                   .bufferImageHeight = height,
                                   .imageSubresource = {
                                       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                       .mipLevel = 0,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1
                                   },
                                   .imageOffset = {
                                       .x = 0,
                                       .y = 0,
                                       .z = 0
                                   },
                                   .imageExtent = {
                                       .width = width,
                                       .height = height,
                                       .depth = 1
                                   }
                               }
                          });

  vkCmdPipelineBarrier (state->vk_command_buffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_HOST_BIT,
                        0,
                        0, NULL,
                        1, &(VkBufferMemoryBarrier) {
                            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .buffer = gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (self->buffer)),
                            .offset = 0,
                            .size = VK_WHOLE_SIZE,
                        },
                        0, NULL);

  self->create_func = gsk_gpu_download_op_vk_create;

  return op->next;
}
#endif

typedef struct _GskGLTextureData GskGLTextureData;

struct _GskGLTextureData
{
  GdkGLContext *context;
  GLuint texture_id;
  GLsync sync;
};

static void
gsk_gl_texture_data_free (gpointer user_data)
{
  GskGLTextureData *data = user_data;

  gdk_gl_context_make_current (data->context);

  /* can't use g_clear_pointer() on glDeleteSync(), see MR !7294 */
  if (data->sync)
    {
      glDeleteSync (data->sync);
      data->sync = NULL;
    }

  glDeleteTextures (1, &data->texture_id);
  g_object_unref (data->context);

  g_free (data);
}

static GskGpuOp *
gsk_gpu_download_op_gl_command (GskGpuOp          *op,
                                GskGpuFrame       *frame,
                                GskGLCommandState *state)
{
  GskGpuDownloadOp *self = (GskGpuDownloadOp *) op;
  GdkGLTextureBuilder *builder;
  GskGLTextureData *data;
  GdkGLContext *context;

  context = GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame));

  data = g_new (GskGLTextureData, 1);
  data->context = g_object_ref (context);
  data->texture_id = gsk_gl_image_steal_texture (GSK_GL_IMAGE (self->image));

  if (gdk_gl_context_has_feature (context, GDK_GL_FEATURE_SYNC))
    data->sync = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

  builder = gdk_gl_texture_builder_new ();
  gdk_gl_texture_builder_set_context (builder, context);
  gdk_gl_texture_builder_set_id (builder, data->texture_id);
  gdk_gl_texture_builder_set_format (builder, gsk_gpu_image_get_format (self->image));
  gdk_gl_texture_builder_set_width (builder, gsk_gpu_image_get_width (self->image));
  gdk_gl_texture_builder_set_height (builder, gsk_gpu_image_get_height (self->image));
  gdk_gl_texture_builder_set_sync (builder, data->sync);

  self->texture = gdk_gl_texture_builder_build (builder,
                                                gsk_gl_texture_data_free,
                                                data);

  g_object_unref (builder);

  return op->next;
}

static const GskGpuOpClass GSK_GPU_DOWNLOAD_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuDownloadOp),
  GSK_GPU_STAGE_COMMAND,
  gsk_gpu_download_op_finish,
  gsk_gpu_download_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_download_op_vk_command,
#endif
  gsk_gpu_download_op_gl_command
};

void
gsk_gpu_download_op (GskGpuFrame        *frame,
                     GskGpuImage        *image,
                     gboolean            allow_dmabuf,
                     GskGpuDownloadFunc  func,
                     gpointer            user_data)
{
  GskGpuDownloadOp *self;

  self = (GskGpuDownloadOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_DOWNLOAD_OP_CLASS);

  self->image = g_object_ref (image);
  self->allow_dmabuf = allow_dmabuf;
  self->func = func,
  self->user_data = user_data;
}

static void
gsk_gpu_download_save_png_cb (gpointer    filename,
                              GdkTexture *texture)
{
  gdk_texture_save_to_png (texture, filename);

  g_free (filename);
}

void
gsk_gpu_download_png_op (GskGpuFrame *frame,
                         GskGpuImage *image,
                         const char  *filename_format,
                         ...)
{
  va_list args;
  char *filename;

  va_start (args, filename_format);
  filename = g_strdup_vprintf (filename_format, args);
  va_end (args);

  gsk_gpu_download_op (frame,
                       image,
                       FALSE,
                       gsk_gpu_download_save_png_cb,
                       filename);
}
