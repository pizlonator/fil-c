#include "config.h"

#include "gskgpucoloropprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpucolorinstance.h"

typedef struct _GskGpuColorOp GskGpuColorOp;

struct _GskGpuColorOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_color_op_print_instance (GskGpuShaderOp *shader,
                                 gpointer        instance_,
                                 GString        *string)
{
  GskGpuColorInstance *instance = (GskGpuColorInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_rgba (string, instance->color);
}

static const GskGpuShaderOpClass GSK_GPU_COLOR_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuColorOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpucolor",
  sizeof (GskGpuColorInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_color_info,
#endif
  gsk_gpu_color_op_print_instance,
  gsk_gpu_color_setup_attrib_locations,
  gsk_gpu_color_setup_vao
};

void
gsk_gpu_color_op (GskGpuFrame            *frame,
                  GskGpuShaderClip        clip,
                  const graphene_rect_t  *rect,
                  const graphene_point_t *offset,
                  const GdkRGBA          *color)
{
  GskGpuColorInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_COLOR_OP_CLASS,
                           0,
                           clip,
                           NULL,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rgba_to_float (color, instance->color);
}
