#include "config.h"

#include "gskgpustraightalphaopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpustraightalphainstance.h"

#define VARIATION_OPACITY        (1 << 0)
#define VARIATION_STRAIGHT_ALPHA (1 << 1)

typedef struct _GskGpuStraightAlphaOp GskGpuStraightAlphaOp;

struct _GskGpuStraightAlphaOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_straight_alpha_op_print_instance (GskGpuShaderOp *shader,
                                          gpointer        instance_,
                                          GString        *string)
{
  GskGpuStraightalphaInstance *instance = (GskGpuStraightalphaInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->tex_id);
}

static const GskGpuShaderOpClass GSK_GPU_STRAIGHT_ALPHA_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuStraightAlphaOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpustraightalpha",
  sizeof (GskGpuStraightalphaInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_straightalpha_info,
#endif
  gsk_gpu_straight_alpha_op_print_instance,
  gsk_gpu_straightalpha_setup_attrib_locations,
  gsk_gpu_straightalpha_setup_vao
};

void
gsk_gpu_straight_alpha_op (GskGpuFrame            *frame,
                           GskGpuShaderClip        clip,
                           float                   opacity,
                           GskGpuDescriptors      *desc,
                           guint32                 descriptor,
                           const graphene_rect_t  *rect,
                           const graphene_point_t *offset,
                           const graphene_rect_t  *tex_rect)
{
  GskGpuStraightalphaInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_STRAIGHT_ALPHA_OP_CLASS,
                           (opacity < 1.0 ? VARIATION_OPACITY : 0) |
                           VARIATION_STRAIGHT_ALPHA,
                           clip,
                           desc,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (tex_rect, offset, instance->tex_rect);
  instance->tex_id = descriptor;
  instance->opacity = opacity;
}
