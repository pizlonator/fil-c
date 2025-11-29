#include "config.h"

#include "gskglframeprivate.h"

#include "gskgpuglobalsopprivate.h"
#include "gskgpuopprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskglbufferprivate.h"
#include "gskgldescriptorsprivate.h"
#include "gskgldeviceprivate.h"
#include "gskglimageprivate.h"

#include "gdkdmabuftextureprivate.h"
#include "gdkglcontextprivate.h"
#include "gdkgltextureprivate.h"

struct _GskGLFrame
{
  GskGpuFrame parent_instance;

  GLuint globals_buffer_id;
  guint next_texture_slot;
  GLsync sync;

  GHashTable *vaos;
};

struct _GskGLFrameClass
{
  GskGpuFrameClass parent_class;
};

G_DEFINE_TYPE (GskGLFrame, gsk_gl_frame, GSK_TYPE_GPU_FRAME)

static gboolean
gsk_gl_frame_is_busy (GskGpuFrame *frame)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  if (!self->sync)
    return FALSE;

  return glClientWaitSync (self->sync, 0, 0) == GL_TIMEOUT_EXPIRED;
}

static void
gsk_gl_frame_wait (GskGpuFrame *frame)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  if (!self->sync)
    return;

  glClientWaitSync (self->sync, 0, G_MAXINT64);
}

static void
gsk_gl_frame_setup (GskGpuFrame *frame)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  glGenBuffers (1, &self->globals_buffer_id);
}

static void
gsk_gl_frame_cleanup (GskGpuFrame *frame)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  if (self->sync)
    {
      glClientWaitSync (self->sync, 0, -1);

      /* can't use g_clear_pointer() on glDeleteSync(), see MR !7294 */
      glDeleteSync (self->sync);
      self->sync = NULL;
    }

  self->next_texture_slot = 0;

  GSK_GPU_FRAME_CLASS (gsk_gl_frame_parent_class)->cleanup (frame);
}

static GskGpuImage *
gsk_gl_frame_upload_texture (GskGpuFrame  *frame,
                             gboolean      with_mipmap,
                             GdkTexture   *texture)
{
  if (GDK_IS_GL_TEXTURE (texture))
    {
      GdkGLTexture *gl_texture = GDK_GL_TEXTURE (texture);

      if (gdk_gl_context_is_shared (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                    gdk_gl_texture_get_context (gl_texture)))
        {
          GskGpuImage *image;
          GLsync sync;

          image = gsk_gl_image_new_for_texture (GSK_GL_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                texture,
                                                gdk_gl_texture_get_id (gl_texture),
                                                FALSE,
                                                gdk_gl_texture_has_mipmap (gl_texture) ? (GSK_GPU_IMAGE_CAN_MIPMAP | GSK_GPU_IMAGE_MIPMAP) : 0);
         
          /* This is a hack, but it works */
          sync = gdk_gl_texture_get_sync (gl_texture);
          if (sync)
            glWaitSync (sync, 0, GL_TIMEOUT_IGNORED);

          return image;
        }
    }
  else if (GDK_IS_DMABUF_TEXTURE (texture))
    {
      gboolean external;
      GLuint tex_id;

      tex_id = gdk_gl_context_import_dmabuf (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                             gdk_texture_get_width (texture),
                                             gdk_texture_get_height (texture),
                                             gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture)),
                                             &external);
      if (tex_id)
        {
          return gsk_gl_image_new_for_texture (GSK_GL_DEVICE (gsk_gpu_frame_get_device (frame)),
                                               texture,
                                               tex_id,
                                               TRUE,
                                               (external ? GSK_GPU_IMAGE_EXTERNAL | GSK_GPU_IMAGE_NO_BLIT : 0));
        }
    }

  return GSK_GPU_FRAME_CLASS (gsk_gl_frame_parent_class)->upload_texture (frame, with_mipmap, texture);
}

static GskGpuDescriptors *
gsk_gl_frame_create_descriptors (GskGpuFrame *frame)
{
  return GSK_GPU_DESCRIPTORS (gsk_gl_descriptors_new (GSK_GL_DEVICE (gsk_gpu_frame_get_device (frame))));
}

static GskGpuBuffer *
gsk_gl_frame_create_vertex_buffer (GskGpuFrame *frame,
                                   gsize        size)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  /* We could also reassign them all to the new buffer here?
   * Is that faster?
   */
  g_hash_table_remove_all (self->vaos);

  if (gdk_gl_context_has_feature (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                  GDK_GL_FEATURE_BUFFER_STORAGE))
    return gsk_gl_mapped_buffer_new (GL_ARRAY_BUFFER, size);
  else
    return gsk_gl_copied_buffer_new (GL_ARRAY_BUFFER, size);
}

static GskGpuBuffer *
gsk_gl_frame_create_storage_buffer (GskGpuFrame *frame,
                                    gsize        size)
{
  if (gdk_gl_context_has_feature (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                  GDK_GL_FEATURE_BUFFER_STORAGE))
    return gsk_gl_mapped_buffer_new (GL_UNIFORM_BUFFER, size);
  else
    return gsk_gl_copied_buffer_new (GL_UNIFORM_BUFFER, size);
}

static void
gsk_gl_frame_submit (GskGpuFrame  *frame,
                     GskGpuBuffer *vertex_buffer,
                     GskGpuOp     *op)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);
  GskGLCommandState state = { 0, };

  glEnable (GL_SCISSOR_TEST);

  glDisable (GL_DEPTH_TEST);
  glEnable (GL_BLEND);

  if (vertex_buffer)
    gsk_gl_buffer_bind (GSK_GL_BUFFER (vertex_buffer));

  gsk_gl_frame_bind_globals (self);
  glBufferData (GL_UNIFORM_BUFFER,
                sizeof (GskGpuGlobalsInstance),
                NULL,
                GL_STREAM_DRAW);

  while (op)
    {
      op = gsk_gpu_op_gl_command (op, frame, &state);
    }

  if (gdk_gl_context_has_feature (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                  GDK_GL_FEATURE_SYNC))
    self->sync = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

static void
gsk_gl_frame_finalize (GObject *object)
{
  GskGLFrame *self = GSK_GL_FRAME (object);

  g_hash_table_unref (self->vaos);
  glDeleteBuffers (1, &self->globals_buffer_id);

  G_OBJECT_CLASS (gsk_gl_frame_parent_class)->finalize (object);
}

static void
gsk_gl_frame_class_init (GskGLFrameClass *klass)
{
  GskGpuFrameClass *gpu_frame_class = GSK_GPU_FRAME_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_frame_class->is_busy = gsk_gl_frame_is_busy;
  gpu_frame_class->wait = gsk_gl_frame_wait;
  gpu_frame_class->setup = gsk_gl_frame_setup;
  gpu_frame_class->cleanup = gsk_gl_frame_cleanup;
  gpu_frame_class->upload_texture = gsk_gl_frame_upload_texture;
  gpu_frame_class->create_descriptors = gsk_gl_frame_create_descriptors;
  gpu_frame_class->create_vertex_buffer = gsk_gl_frame_create_vertex_buffer;
  gpu_frame_class->create_storage_buffer = gsk_gl_frame_create_storage_buffer;
  gpu_frame_class->submit = gsk_gl_frame_submit;

  object_class->finalize = gsk_gl_frame_finalize;
}

static void
free_vao (gpointer vao)
{
  glDeleteVertexArrays (1, (GLuint[1]) { GPOINTER_TO_UINT (vao) });
}

static void
gsk_gl_frame_init (GskGLFrame *self)
{
  self->vaos = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free_vao);
}

void
gsk_gl_frame_use_program (GskGLFrame                *self,
                          const GskGpuShaderOpClass *op_class,
                          guint32                    variation,
                          GskGpuShaderClip           clip,
                          guint                      n_external_textures)
{
  GLuint vao;

  gsk_gl_device_use_program (GSK_GL_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self))),
                             op_class,
                             variation,
                             clip,
                             n_external_textures);

  vao = GPOINTER_TO_UINT (g_hash_table_lookup (self->vaos, op_class));
  if (vao)
    {
      glBindVertexArray (vao);
      return;
    }
  glGenVertexArrays (1, &vao);
  glBindVertexArray (vao);
  op_class->setup_vao (0);

  g_hash_table_insert (self->vaos, (gpointer) op_class, GUINT_TO_POINTER (vao));
}

void
gsk_gl_frame_bind_globals (GskGLFrame *self)
{
  glBindBufferBase (GL_UNIFORM_BUFFER, 0, self->globals_buffer_id);
}

