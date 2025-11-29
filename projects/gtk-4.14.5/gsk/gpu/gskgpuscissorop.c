#include "config.h"

#include "gskgpuscissoropprivate.h"

#include "gskgpuopprivate.h"
#include "gskgpuprintprivate.h"

typedef struct _GskGpuScissorOp GskGpuScissorOp;

struct _GskGpuScissorOp
{
  GskGpuOp op;

  cairo_rectangle_int_t rect;
};

static void
gsk_gpu_scissor_op_finish (GskGpuOp *op)
{
}

static void
gsk_gpu_scissor_op_print (GskGpuOp    *op,
                          GskGpuFrame *frame,
                          GString     *string,
                          guint        indent)
{
  GskGpuScissorOp *self = (GskGpuScissorOp *) op;

  gsk_gpu_print_op (string, indent, "scissor");
  gsk_gpu_print_int_rect (string, &self->rect);
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN
static GskGpuOp *
gsk_gpu_scissor_op_vk_command (GskGpuOp              *op,
                               GskGpuFrame           *frame,
                               GskVulkanCommandState *state)
{
  GskGpuScissorOp *self = (GskGpuScissorOp *) op;

  vkCmdSetScissor (state->vk_command_buffer,
                   0,
                   1,
                   &(VkRect2D) {
                     { self->rect.x, self->rect.y },
                     { self->rect.width, self->rect.height },
                   });

  return op->next;
}
#endif

static GskGpuOp *
gsk_gpu_scissor_op_gl_command (GskGpuOp          *op,
                               GskGpuFrame       *frame,
                               GskGLCommandState *state)
{
  GskGpuScissorOp *self = (GskGpuScissorOp *) op;

  if (state->flip_y)
    glScissor (self->rect.x, state->flip_y - self->rect.y - self->rect.height, self->rect.width, self->rect.height);
  else
    glScissor (self->rect.x, self->rect.y, self->rect.width, self->rect.height);

  return op->next;
}

static const GskGpuOpClass GSK_GPU_SCISSOR_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuScissorOp),
  GSK_GPU_STAGE_COMMAND,
  gsk_gpu_scissor_op_finish,
  gsk_gpu_scissor_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_scissor_op_vk_command,
#endif
  gsk_gpu_scissor_op_gl_command
};

void
gsk_gpu_scissor_op (GskGpuFrame                 *frame,
                    const cairo_rectangle_int_t *rect)
{
  GskGpuScissorOp *self;

  self = (GskGpuScissorOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_SCISSOR_OP_CLASS);

  self->rect = *rect;
}
