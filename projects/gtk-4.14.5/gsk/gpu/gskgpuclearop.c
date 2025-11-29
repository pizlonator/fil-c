#include "config.h"

#include "gskgpuclearopprivate.h"

#include "gskgpuopprivate.h"
#include "gskgpuprintprivate.h"
/* for gsk_gpu_rgba_to_float() */
#include "gskgpushaderopprivate.h"

typedef struct _GskGpuClearOp GskGpuClearOp;

struct _GskGpuClearOp
{
  GskGpuOp op;

  cairo_rectangle_int_t rect;
  GdkRGBA color;
};

static void
gsk_gpu_clear_op_finish (GskGpuOp *op)
{
}

static void
gsk_gpu_clear_op_print (GskGpuOp    *op,
                        GskGpuFrame *frame,
                        GString     *string,
                        guint        indent)
{
  GskGpuClearOp *self = (GskGpuClearOp *) op;
  float rgba[4];

  gsk_gpu_print_op (string, indent, "clear");
  gsk_gpu_print_int_rect (string, &self->rect);
  gsk_gpu_rgba_to_float (&self->color, rgba);
  gsk_gpu_print_rgba (string, rgba);
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN
static void
gsk_gpu_init_clear_value (VkClearValue  *value,
                          const GdkRGBA *rgba)
{
  gsk_gpu_rgba_to_float (rgba, value->color.float32);
}

static GskGpuOp *
gsk_gpu_clear_op_vk_command (GskGpuOp              *op,
                             GskGpuFrame           *frame,
                             GskVulkanCommandState *state)
{
  GskGpuClearOp *self = (GskGpuClearOp *) op;
  VkClearValue clear_value;

  gsk_gpu_init_clear_value (&clear_value, &self->color);

  vkCmdClearAttachments (state->vk_command_buffer,
                         1,
                         &(VkClearAttachment) {
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           0,
                           clear_value,
                         },
                         1,
                         &(VkClearRect) {
                           {
                             { self->rect.x, self->rect.y },
                             { self->rect.width, self->rect.height },
                           },
                           0,
                           1
                         });

  return op->next;
}
#endif

static GskGpuOp *
gsk_gpu_clear_op_gl_command (GskGpuOp          *op,
                             GskGpuFrame       *frame,
                             GskGLCommandState *state)
{
  GskGpuClearOp *self = (GskGpuClearOp *) op;
  int scissor[4];

  glGetIntegerv (GL_SCISSOR_BOX, scissor);

  if (state->flip_y)
    glScissor (self->rect.x, state->flip_y - self->rect.y - self->rect.height, self->rect.width, self->rect.height);
  else
    glScissor (self->rect.x, self->rect.y, self->rect.width, self->rect.height);

  glClearColor (self->color.red, self->color.green, self->color.blue, self->color.alpha);
  glClear (GL_COLOR_BUFFER_BIT);

  glScissor (scissor[0], scissor[1], scissor[2], scissor[3]);

  return op->next;
}

static const GskGpuOpClass GSK_GPU_CLEAR_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuClearOp),
  GSK_GPU_STAGE_COMMAND,
  gsk_gpu_clear_op_finish,
  gsk_gpu_clear_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_clear_op_vk_command,
#endif
  gsk_gpu_clear_op_gl_command
};

void
gsk_gpu_clear_op (GskGpuFrame                 *frame,
                  const cairo_rectangle_int_t *rect,
                  const GdkRGBA               *color)
{
  GskGpuClearOp *self;

  self = (GskGpuClearOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_CLEAR_OP_CLASS);

  self->rect = *rect;
  self->color = *color;
}
