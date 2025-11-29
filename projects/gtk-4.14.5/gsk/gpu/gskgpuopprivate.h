#pragma once

#include <gdk/gdk.h>

#include "gskgputypesprivate.h"

G_BEGIN_DECLS

typedef enum
{
  GSK_GPU_STAGE_UPLOAD,
  GSK_GPU_STAGE_PASS,
  GSK_GPU_STAGE_COMMAND,
  GSK_GPU_STAGE_SHADER,
  /* magic ones */
  GSK_GPU_STAGE_BEGIN_PASS,
  GSK_GPU_STAGE_END_PASS
} GskGpuStage;

typedef struct _GskGLCommandState GskGLCommandState;
typedef struct _GskVulkanCommandState GskVulkanCommandState;

struct _GskGLCommandState
{
  gsize flip_y;
  struct {
    const GskGpuOpClass *op_class;
    guint32 variation;
    GskGpuShaderClip clip;
    gsize n_external;
  } current_program;
  GskGLDescriptors *desc;
};

#ifdef GDK_RENDERING_VULKAN
struct _GskVulkanCommandState
{
  VkRenderPass vk_render_pass;
  VkFormat vk_format;
  VkCommandBuffer vk_command_buffer;
  GskGpuBlend blend;

  GskVulkanDescriptors *desc;
  GskVulkanSemaphores *semaphores;
};
#endif

struct _GskGpuOp
{
  const GskGpuOpClass *op_class;

  GskGpuOp *next;
};

struct _GskGpuOpClass
{
  gsize                 size;
  GskGpuStage           stage;

  void                  (* finish)                                      (GskGpuOp               *op);

  void                  (* print)                                       (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GString                *string,
                                                                         guint                   indent);

#ifdef GDK_RENDERING_VULKAN
  GskGpuOp *            (* vk_command)                                  (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskVulkanCommandState  *state);
#endif
  GskGpuOp *            (* gl_command)                                  (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskGLCommandState      *state);
};

/* ensures alignment of ops to multiples of 16 bytes - and that makes graphene happy */
#define GSK_GPU_OP_SIZE(struct_name) ((sizeof(struct_name) + 15) & ~15)

GskGpuOp *              gsk_gpu_op_alloc                                (GskGpuFrame            *frame,
                                                                         const GskGpuOpClass    *op_class);
void                    gsk_gpu_op_finish                               (GskGpuOp               *op);

void                    gsk_gpu_op_print                                (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GString                *string,
                                                                         guint                   indent);

#ifdef GDK_RENDERING_VULKAN
GskGpuOp *              gsk_gpu_op_vk_command                           (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskVulkanCommandState  *state);
#endif
GskGpuOp *              gsk_gpu_op_gl_command                           (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskGLCommandState      *state);

G_END_DECLS

