#include "config.h"

#include "gskgpumaskopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpumaskinstance.h"

typedef struct _GskGpuMaskOp GskGpuMaskOp;

struct _GskGpuMaskOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_mask_op_print_instance (GskGpuShaderOp *shader,
                                gpointer        instance_,
                                GString        *string)
{
  GskGpuMaskInstance *instance = (GskGpuMaskInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->source_id);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->mask_id);
}

static const GskGpuShaderOpClass GSK_GPU_MASK_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuMaskOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpumask",
  sizeof (GskGpuMaskInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_mask_info,
#endif
  gsk_gpu_mask_op_print_instance,
  gsk_gpu_mask_setup_attrib_locations,
  gsk_gpu_mask_setup_vao
};

void
gsk_gpu_mask_op (GskGpuFrame            *frame,
                 GskGpuShaderClip        clip,
                 GskGpuDescriptors      *desc,
                 const graphene_rect_t  *rect,
                 const graphene_point_t *offset,
                 float                   opacity,
                 GskMaskMode             mask_mode,
                 guint32                 source_descriptor,
                 const graphene_rect_t  *source_rect,
                 guint32                 mask_descriptor,
                 const graphene_rect_t  *mask_rect)
{
  GskGpuMaskInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_MASK_OP_CLASS,
                           mask_mode,
                           clip,
                           desc,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (source_rect, offset, instance->source_rect);
  instance->source_id = source_descriptor;
  gsk_gpu_rect_to_float (mask_rect, offset, instance->mask_rect);
  instance->mask_id = mask_descriptor;
  instance->opacity = opacity;
}
