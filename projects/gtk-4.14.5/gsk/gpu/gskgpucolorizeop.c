#include "config.h"

#include "gskgpucolorizeopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpucolorizeinstance.h"

typedef struct _GskGpuColorizeOp GskGpuColorizeOp;

struct _GskGpuColorizeOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_colorize_op_print_instance (GskGpuShaderOp *shader,
                                    gpointer        instance_,
                                    GString        *string)
{
  GskGpuColorizeInstance *instance = (GskGpuColorizeInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->tex_id);
  gsk_gpu_print_rgba (string, instance->color);
}

static const GskGpuShaderOpClass GSK_GPU_COLORIZE_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuColorizeOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpucolorize",
  sizeof (GskGpuColorizeInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_colorize_info,
#endif
  gsk_gpu_colorize_op_print_instance,
  gsk_gpu_colorize_setup_attrib_locations,
  gsk_gpu_colorize_setup_vao
};

void
gsk_gpu_colorize_op (GskGpuFrame            *frame,
                     GskGpuShaderClip        clip,
                     GskGpuDescriptors      *descriptors,
                     guint32                 descriptor,
                     const graphene_rect_t  *rect,
                     const graphene_point_t *offset,
                     const graphene_rect_t  *tex_rect,
                     const GdkRGBA          *color)
{
  GskGpuColorizeInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_COLORIZE_OP_CLASS,
                           0,
                           clip,
                           descriptors,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (tex_rect, offset, instance->tex_rect);
  instance->tex_id = descriptor;
  gsk_gpu_rgba_to_float (color, instance->color);
}
