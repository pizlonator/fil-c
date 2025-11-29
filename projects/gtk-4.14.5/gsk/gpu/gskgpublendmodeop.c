#include "config.h"

#include "gskgpublendmodeopprivate.h"

#include "gskenumtypes.h"
#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpublendmodeinstance.h"

typedef struct _GskGpuBlendModeOp GskGpuBlendModeOp;

struct _GskGpuBlendModeOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_blend_mode_op_print_instance (GskGpuShaderOp *shader,
                                      gpointer        instance_,
                                      GString        *string)
{
  GskGpuBlendmodeInstance *instance = (GskGpuBlendmodeInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->bottom_id);
  gsk_gpu_print_enum (string, GSK_TYPE_BLEND_MODE, shader->variation);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->top_id);
}

static const GskGpuShaderOpClass GSK_GPU_BLEND_MODE_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuBlendModeOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpublendmode",
  sizeof (GskGpuBlendmodeInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_blendmode_info,
#endif
  gsk_gpu_blend_mode_op_print_instance,
  gsk_gpu_blendmode_setup_attrib_locations,
  gsk_gpu_blendmode_setup_vao
};

void
gsk_gpu_blend_mode_op (GskGpuFrame            *frame,
                       GskGpuShaderClip        clip,
                       GskGpuDescriptors      *desc,
                       const graphene_rect_t  *rect,
                       const graphene_point_t *offset,
                       float                   opacity,
                       GskBlendMode            blend_mode,
                       guint32                 bottom_descriptor,
                       const graphene_rect_t  *bottom_rect,
                       guint32                 top_descriptor,
                       const graphene_rect_t  *top_rect)
{
  GskGpuBlendmodeInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_BLEND_MODE_OP_CLASS,
                           blend_mode,
                           clip,
                           desc,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  instance->opacity = opacity;
  gsk_gpu_rect_to_float (bottom_rect, offset, instance->bottom_rect);
  instance->bottom_id = bottom_descriptor;
  gsk_gpu_rect_to_float (top_rect, offset, instance->top_rect);
  instance->top_id = top_descriptor;
}
