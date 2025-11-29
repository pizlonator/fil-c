#include "config.h"

#include "gskgpucolormatrixopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpucolormatrixinstance.h"

typedef struct _GskGpuColorMatrixOp GskGpuColorMatrixOp;

struct _GskGpuColorMatrixOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_color_matrix_op_print_instance (GskGpuShaderOp *shader,
                                        gpointer        instance_,
                                        GString        *string)
{
  GskGpuColormatrixInstance *instance = (GskGpuColormatrixInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->tex_id);
}

static const GskGpuShaderOpClass GSK_GPU_COLOR_MATRIX_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuColorMatrixOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpucolormatrix",
  sizeof (GskGpuColormatrixInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_colormatrix_info,
#endif
  gsk_gpu_color_matrix_op_print_instance,
  gsk_gpu_colormatrix_setup_attrib_locations,
  gsk_gpu_colormatrix_setup_vao
};

void
gsk_gpu_color_matrix_op (GskGpuFrame             *frame,
                         GskGpuShaderClip         clip,
                         GskGpuDescriptors       *desc,
                         guint32                  descriptor,
                         const graphene_rect_t   *rect,
                         const graphene_point_t  *offset,
                         const graphene_rect_t   *tex_rect,
                         const graphene_matrix_t *color_matrix,
                         const graphene_vec4_t   *color_offset)
{
  GskGpuColormatrixInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_COLOR_MATRIX_OP_CLASS,
                           0,
                           clip,
                           desc,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (tex_rect, offset, instance->tex_rect);
  instance->tex_id = descriptor;
  graphene_matrix_to_float (color_matrix, instance->color_matrix);
  graphene_vec4_to_float (color_offset, instance->color_offset);
}

void
gsk_gpu_color_matrix_op_opacity (GskGpuFrame             *frame,
                                 GskGpuShaderClip         clip,
                                 GskGpuDescriptors       *desc,
                                 guint32                  descriptor,
                                 const graphene_rect_t   *rect,
                                 const graphene_point_t  *offset,
                                 const graphene_rect_t   *tex_rect,
                                 float                    opacity)
{
  graphene_matrix_t matrix;

  graphene_matrix_init_from_float (&matrix,
                                   (float[16]) {
                                     1, 0, 0, 0,
                                     0, 1, 0, 0,
                                     0, 0, 1, 0,
                                     0, 0, 0, opacity
                                   });

  gsk_gpu_color_matrix_op (frame,
                           clip,
                           desc,
                           descriptor,
                           rect,
                           offset,
                           tex_rect,
                           &matrix,
                           graphene_vec4_zero ());
}
