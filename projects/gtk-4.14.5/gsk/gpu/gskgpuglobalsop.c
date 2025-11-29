#include "config.h"

#include "gskgpuglobalsopprivate.h"

#include "gskglframeprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskroundedrectprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkandeviceprivate.h"
#include "gskvulkanframeprivate.h"
#include "gskvulkandescriptorsprivate.h"
#endif

typedef struct _GskGpuGlobalsOp GskGpuGlobalsOp;

struct _GskGpuGlobalsOp
{
  GskGpuOp op;

  GskGpuGlobalsInstance instance;
};

static void
gsk_gpu_globals_op_finish (GskGpuOp *op)
{
}

static void
gsk_gpu_globals_op_print (GskGpuOp    *op,
                          GskGpuFrame *frame,
                          GString     *string,
                          guint        indent)
{
  GskGpuGlobalsOp *globals = (GskGpuGlobalsOp *) op;
  GskGpuGlobalsInstance *instance = &globals->instance;

  gsk_gpu_print_op (string, indent, "globals");
  g_string_append_printf (string, "scale %g %g ", instance->scale[0], instance->scale[1]);
  g_string_append (string, "clip ");
  gsk_gpu_print_rounded_rect (string, instance->clip);
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN
static GskGpuOp *
gsk_gpu_globals_op_vk_command (GskGpuOp              *op,
                               GskGpuFrame           *frame,
                               GskVulkanCommandState *state)
{
  GskGpuGlobalsOp *self = (GskGpuGlobalsOp *) op;

  vkCmdPushConstants (state->vk_command_buffer,
                      gsk_vulkan_device_get_vk_pipeline_layout (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                                gsk_vulkan_descriptors_get_pipeline_layout (state->desc)),
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      0,
                      sizeof (self->instance),
                      &self->instance);

  return op->next;
}
#endif

static GskGpuOp *
gsk_gpu_globals_op_gl_command (GskGpuOp          *op,
                               GskGpuFrame       *frame,
                               GskGLCommandState *state)
{
  GskGpuGlobalsOp *self = (GskGpuGlobalsOp *) op;

  gsk_gl_frame_bind_globals (GSK_GL_FRAME (frame));

  /* FIXME: Does it matter if we glBufferData() or glSubBufferData() here? */
  glBufferSubData (GL_UNIFORM_BUFFER,
                   0,
                   sizeof (self->instance),
                   &self->instance);

  return op->next;
}

static const GskGpuOpClass GSK_GPU_GLOBALS_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuGlobalsOp),
  GSK_GPU_STAGE_COMMAND,
  gsk_gpu_globals_op_finish,
  gsk_gpu_globals_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_globals_op_vk_command,
#endif
  gsk_gpu_globals_op_gl_command
};

void
gsk_gpu_globals_op (GskGpuFrame             *frame,
                    const graphene_vec2_t   *scale,
                    const graphene_matrix_t *mvp,
                    const GskRoundedRect    *clip)
{
  GskGpuGlobalsOp *self;

  self = (GskGpuGlobalsOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_GLOBALS_OP_CLASS);

  graphene_matrix_to_float (mvp, self->instance.mvp);
  gsk_rounded_rect_to_float (clip, graphene_point_zero (), self->instance.clip);
  graphene_vec2_to_float (scale, self->instance.scale);
}
