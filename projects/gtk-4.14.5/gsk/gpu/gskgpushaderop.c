#include "config.h"

#include "gskgpushaderopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskgldescriptorsprivate.h"
#include "gskgldeviceprivate.h"
#include "gskglframeprivate.h"
#include "gskglimageprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkandescriptorsprivate.h"
#include "gskvulkandeviceprivate.h"
#endif

#include "gdkglcontextprivate.h"

/* maximum number of ops to merge into one call
 * If this number is too high, the command may take too long
 * causing the driver to kill us.
 */
#define MAX_MERGE_OPS (10 * 1000)

void
gsk_gpu_shader_op_finish (GskGpuOp *op)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;

  g_clear_object (&self->desc);
}

void
gsk_gpu_shader_op_print (GskGpuOp    *op,
                         GskGpuFrame *frame,
                         GString     *string,
                         guint        indent)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;
  const GskGpuShaderOpClass *shader_class = (const GskGpuShaderOpClass *) op->op_class;
  const char *shader_name;
  guchar *instance;
  gsize i;

  if (g_str_has_prefix (shader_class->shader_name, "gskgpu"))
    shader_name = shader_class->shader_name + 6;
  else
    shader_name = shader_class->shader_name;

  instance = gsk_gpu_frame_get_vertex_data (frame, self->vertex_offset);

  for (i = 0; i < self->n_ops; i++)
    {
      gsk_gpu_print_op (string, indent, shader_name);
      gsk_gpu_print_shader_info (string, self->clip);
      shader_class->print_instance (self,
                                    instance + i * shader_class->vertex_size,
                                    string);
      gsk_gpu_print_newline (string);
    }
}

#ifdef GDK_RENDERING_VULKAN
GskGpuOp *
gsk_gpu_shader_op_vk_command_n (GskGpuOp              *op,
                                GskGpuFrame           *frame,
                                GskVulkanCommandState *state,
                                gsize                  instance_scale)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;
  GskGpuShaderOpClass *shader_op_class = (GskGpuShaderOpClass *) op->op_class;
  GskVulkanDescriptors *desc;
  GskGpuOp *next;
  gsize i, n_ops, max_ops_per_draw;

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_MERGE) &&
      gsk_vulkan_device_has_feature (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                     GDK_VULKAN_FEATURE_NONUNIFORM_INDEXING))
    max_ops_per_draw = MAX_MERGE_OPS;
  else
    max_ops_per_draw = 1;

  desc = GSK_VULKAN_DESCRIPTORS (self->desc);
  if (desc && state->desc != desc)
    {
      gsk_vulkan_descriptors_bind (desc, state->desc, state->vk_command_buffer);
      state->desc = desc;
    }

  n_ops = self->n_ops;
  for (next = op->next; next; next = next->next)
    {
      GskGpuShaderOp *next_shader = (GskGpuShaderOp *) next;
  
      if (next->op_class != op->op_class ||
          next_shader->desc != self->desc ||
          next_shader->variation != self->variation ||
          next_shader->clip != self->clip ||
          next_shader->vertex_offset != self->vertex_offset + n_ops * shader_op_class->vertex_size)
        break;

      n_ops += next_shader->n_ops;
    }

  vkCmdBindPipeline (state->vk_command_buffer,
                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                     gsk_vulkan_device_get_vk_pipeline (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                        gsk_vulkan_descriptors_get_pipeline_layout (state->desc),
                                                        shader_op_class,
                                                        self->variation,
                                                        self->clip,
                                                        state->blend,
                                                        state->vk_format,
                                                        state->vk_render_pass));

  for (i = 0; i < n_ops; i += max_ops_per_draw)
    {
      vkCmdDraw (state->vk_command_buffer,
                 6 * instance_scale, MIN (max_ops_per_draw, n_ops - i),
                 0, self->vertex_offset / shader_op_class->vertex_size + i);
    }
 
  return next;
}

GskGpuOp *
gsk_gpu_shader_op_vk_command (GskGpuOp              *op,
                              GskGpuFrame           *frame,
                              GskVulkanCommandState *state)
{
  return gsk_gpu_shader_op_vk_command_n (op, frame, state, 1);
}
#endif

GskGpuOp *
gsk_gpu_shader_op_gl_command_n (GskGpuOp          *op,
                                GskGpuFrame       *frame,
                                GskGLCommandState *state,
                                gsize              instance_scale)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;
  GskGpuShaderOpClass *shader_op_class = (GskGpuShaderOpClass *) op->op_class;
  GskGLDescriptors *desc;
  GskGpuOp *next;
  gsize i, n_ops, n_external, max_ops_per_draw;

  desc = GSK_GL_DESCRIPTORS (self->desc);
  if (desc)
    n_external = gsk_gl_descriptors_get_n_external (desc);
  else
    n_external = 0;

  if (state->current_program.op_class != op->op_class ||
      state->current_program.variation != self->variation ||
      state->current_program.clip != self->clip ||
      state->current_program.n_external != n_external)
    {
      state->current_program.op_class = op->op_class;
      state->current_program.variation = self->variation;
      state->current_program.clip = self->clip;
      state->current_program.n_external = n_external;
      gsk_gl_frame_use_program (GSK_GL_FRAME (frame),
                                shader_op_class,
                                self->variation,
                                self->clip,
                                n_external);
    }

  if (desc != state->desc && desc)
    {
      gsk_gl_descriptors_use (desc);
      state->desc = desc;
    }

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_MERGE))
    max_ops_per_draw = MAX_MERGE_OPS;
  else
    max_ops_per_draw = 1;

  n_ops = self->n_ops;
  for (next = op->next; next; next = next->next)
    {
      GskGpuShaderOp *next_shader = (GskGpuShaderOp *) next;

      if (next->op_class != op->op_class ||
          next_shader->desc != self->desc ||
          next_shader->variation != self->variation ||
          next_shader->clip != self->clip ||
          next_shader->vertex_offset != self->vertex_offset + n_ops * shader_op_class->vertex_size)
        break;

      n_ops += next_shader->n_ops;
    }

  for (i = 0; i < n_ops; i += max_ops_per_draw)
    {
      if (gdk_gl_context_has_feature (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                      GDK_GL_FEATURE_BASE_INSTANCE))
        {
          glDrawArraysInstancedBaseInstance (GL_TRIANGLES,
                                             0,
                                             6 * instance_scale,
                                             MIN (max_ops_per_draw, n_ops - i),
                                             self->vertex_offset / shader_op_class->vertex_size + i);
        }
      else
        {
          shader_op_class->setup_vao (self->vertex_offset + i * shader_op_class->vertex_size);

          glDrawArraysInstanced (GL_TRIANGLES,
                                 0,
                                 6 * instance_scale,
                                 MIN (max_ops_per_draw, n_ops - i));
        }
    }

  return next;
}

GskGpuOp *
gsk_gpu_shader_op_gl_command (GskGpuOp          *op,
                              GskGpuFrame       *frame,
                              GskGLCommandState *state)
{
  return gsk_gpu_shader_op_gl_command_n (op, frame, state, 1);
}

void
gsk_gpu_shader_op_alloc (GskGpuFrame               *frame,
                         const GskGpuShaderOpClass *op_class,
                         guint32                    variation,
                         GskGpuShaderClip           clip,
                         GskGpuDescriptors         *desc,
                         gpointer                   out_vertex_data)
{
  GskGpuOp *last;
  GskGpuShaderOp *last_shader;
  gsize vertex_offset;

  vertex_offset = gsk_gpu_frame_reserve_vertex_data (frame, op_class->vertex_size);

  last = gsk_gpu_frame_get_last_op (frame);
  /* careful: We're casting without checking, but the if() does the check */
  last_shader = (GskGpuShaderOp *) last;
  if (last &&
      last->op_class == (const GskGpuOpClass *) op_class &&
      last_shader->desc == desc &&
      last_shader->variation == variation &&
      last_shader->clip == clip &&
      last_shader->vertex_offset + last_shader->n_ops * op_class->vertex_size == vertex_offset)
    {
      last_shader->n_ops++;
    }
  else
    {
      GskGpuShaderOp *self;
      self = (GskGpuShaderOp *) gsk_gpu_op_alloc (frame, &op_class->parent_class);

      self->variation = variation;
      self->clip = clip;
      self->vertex_offset = vertex_offset;
      if (desc)
        self->desc = g_object_ref (desc);
      else
        self->desc = NULL;
      self->n_ops = 1;
    }

  *((gpointer *) out_vertex_data) = gsk_gpu_frame_get_vertex_data (frame, vertex_offset);
}

