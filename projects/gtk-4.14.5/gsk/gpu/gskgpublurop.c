#include "config.h"

#include "gskgpubluropprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gdk/gdkrgbaprivate.h"

#include "gpu/shaders/gskgpublurinstance.h"

#define VARIATION_COLORIZE 1

typedef struct _GskGpuBlurOp GskGpuBlurOp;

struct _GskGpuBlurOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_blur_op_print_instance (GskGpuShaderOp *shader,
                                gpointer        instance_,
                                GString        *string)
{
  GskGpuBlurInstance *instance = (GskGpuBlurInstance *) instance_;

  g_string_append_printf (string, "%g,%g ", instance->blur_direction[0], instance->blur_direction[1]);
  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->tex_id);
}

static const GskGpuShaderOpClass GSK_GPU_BLUR_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuBlurOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpublur",
  sizeof (GskGpuBlurInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_blur_info,
#endif
  gsk_gpu_blur_op_print_instance,
  gsk_gpu_blur_setup_attrib_locations,
  gsk_gpu_blur_setup_vao
};

static void
gsk_gpu_blur_op_full (GskGpuFrame            *frame,
                      guint32                 variation,
                      GskGpuShaderClip        clip,
                      GskGpuDescriptors      *desc,
                      guint32                 descriptor,
                      const graphene_rect_t  *rect,
                      const graphene_point_t *offset,
                      const graphene_rect_t  *tex_rect,
                      const graphene_vec2_t  *blur_direction,
                      const GdkRGBA          *blur_color)
{
  GskGpuBlurInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_BLUR_OP_CLASS,
                           variation,
                           clip,
                           desc,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (tex_rect, offset, instance->tex_rect);
  graphene_vec2_to_float (blur_direction, instance->blur_direction);
  gsk_gpu_rgba_to_float (blur_color, instance->blur_color);
  instance->tex_id = descriptor;
}

void
gsk_gpu_blur_op (GskGpuFrame            *frame,
                 GskGpuShaderClip        clip,
                 GskGpuDescriptors      *desc,
                 guint32                 descriptor,
                 const graphene_rect_t  *rect,
                 const graphene_point_t *offset,
                 const graphene_rect_t  *tex_rect,
                 const graphene_vec2_t  *blur_direction)
{
  gsk_gpu_blur_op_full (frame,
                        0,
                        clip,
                        desc,
                        descriptor,
                        rect,
                        offset,
                        tex_rect,
                        blur_direction,
                        &GDK_RGBA_TRANSPARENT);
}

void
gsk_gpu_blur_shadow_op (GskGpuFrame            *frame,
                        GskGpuShaderClip        clip,
                        GskGpuDescriptors      *desc,
                        guint32                 descriptor,
                        const graphene_rect_t  *rect,
                        const graphene_point_t *offset,
                        const graphene_rect_t  *tex_rect,
                        const graphene_vec2_t  *blur_direction,
                        const GdkRGBA          *shadow_color)
{
  gsk_gpu_blur_op_full (frame,
                        VARIATION_COLORIZE,
                        clip,
                        desc,
                        descriptor,
                        rect,
                        offset,
                        tex_rect,
                        blur_direction,
                        shadow_color);
}

