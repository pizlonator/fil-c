#include "config.h"

#include "gskgpucrossfadeopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpucrossfadeinstance.h"

typedef struct _GskGpuCrossFadeOp GskGpuCrossFadeOp;

struct _GskGpuCrossFadeOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_cross_fade_op_print_instance (GskGpuShaderOp *shader,
                                      gpointer        instance_,
                                      GString        *string)
{
  GskGpuCrossfadeInstance *instance = (GskGpuCrossfadeInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->start_id);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->end_id);
  g_string_append_printf (string, "%g%%", 100 * instance->opacity_progress[1]);
}

static const GskGpuShaderOpClass GSK_GPU_CROSS_FADE_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuCrossFadeOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpucrossfade",
  sizeof (GskGpuCrossfadeInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_crossfade_info,
#endif
  gsk_gpu_cross_fade_op_print_instance,
  gsk_gpu_crossfade_setup_attrib_locations,
  gsk_gpu_crossfade_setup_vao
};

void
gsk_gpu_cross_fade_op (GskGpuFrame            *frame,
                       GskGpuShaderClip        clip,
                       GskGpuDescriptors      *desc,
                       const graphene_rect_t  *rect,
                       const graphene_point_t *offset,
                       float                   opacity,
                       float                   progress,
                       guint32                 start_descriptor,
                       const graphene_rect_t  *start_rect,
                       guint32                 end_descriptor,
                       const graphene_rect_t  *end_rect)
{
  GskGpuCrossfadeInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_CROSS_FADE_OP_CLASS,
                           0,
                           clip,
                           desc,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  instance->opacity_progress[0] = opacity;
  instance->opacity_progress[1] = progress;
  gsk_gpu_rect_to_float (start_rect, offset, instance->start_rect);
  instance->start_id = start_descriptor;
  gsk_gpu_rect_to_float (end_rect, offset, instance->end_rect);
  instance->end_id = end_descriptor;
}
