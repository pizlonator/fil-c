#include "config.h"

#include "gskgpublendopprivate.h"

#include "gskgpuopprivate.h"
#include "gskgpuprintprivate.h"

typedef struct _GskGpuBlendOp GskGpuBlendOp;

struct _GskGpuBlendOp
{
  GskGpuOp op;

  GskGpuBlend blend;
};

static void
gsk_gpu_blend_op_finish (GskGpuOp *op)
{
}

static void
gsk_gpu_blend_op_print (GskGpuOp    *op,
                        GskGpuFrame *frame,
                        GString     *string,
                        guint        indent)
{
  GskGpuBlendOp *self = (GskGpuBlendOp *) op;

  gsk_gpu_print_op (string, indent, "blend");
  switch (self->blend)
    {
      case GSK_GPU_BLEND_OVER:
        gsk_gpu_print_string (string, "over");
        break;
      case GSK_GPU_BLEND_ADD:
        gsk_gpu_print_string (string, "add");
        break;
      case GSK_GPU_BLEND_CLEAR:
        gsk_gpu_print_string (string, "clear");
        break;
      default:
        g_assert_not_reached ();
        break;
    }
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN
static GskGpuOp *
gsk_gpu_blend_op_vk_command (GskGpuOp              *op,
                             GskGpuFrame           *frame,
                             GskVulkanCommandState *state)
{
  GskGpuBlendOp *self = (GskGpuBlendOp *) op;

  state->blend = self->blend;

  return op->next;
}
#endif

static GskGpuOp *
gsk_gpu_blend_op_gl_command (GskGpuOp          *op,
                             GskGpuFrame       *frame,
                             GskGLCommandState *state)
{
  GskGpuBlendOp *self = (GskGpuBlendOp *) op;

  switch (self->blend)
    {
      case GSK_GPU_BLEND_OVER:
        glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
      case GSK_GPU_BLEND_ADD:
        glBlendFunc (GL_ONE, GL_ONE);
        break;
      case GSK_GPU_BLEND_CLEAR:
        glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  return op->next;
}

static const GskGpuOpClass GSK_GPU_BLEND_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuBlendOp),
  GSK_GPU_STAGE_COMMAND,
  gsk_gpu_blend_op_finish,
  gsk_gpu_blend_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_blend_op_vk_command,
#endif
  gsk_gpu_blend_op_gl_command
};

void
gsk_gpu_blend_op (GskGpuFrame *frame,
                  GskGpuBlend  blend)
{
  GskGpuBlendOp *self;

  self = (GskGpuBlendOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_BLEND_OP_CLASS);

  self->blend = blend;
}
