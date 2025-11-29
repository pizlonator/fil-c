#include "config.h"

#include "gskglbufferprivate.h"

/* {{{ GskGLBuffer */

struct _GskGLBuffer
{
  GskGpuBuffer parent_instance;

  GLenum target;
  GLuint buffer_id;
  guchar *data;
};

G_DEFINE_TYPE (GskGLBuffer, gsk_gl_buffer, GSK_TYPE_GPU_BUFFER)

static void
gsk_gl_buffer_finalize (GObject *object)
{
  GskGLBuffer *self = GSK_GL_BUFFER (object);

  glDeleteBuffers (1, &self->buffer_id);

  G_OBJECT_CLASS (gsk_gl_buffer_parent_class)->finalize (object);
}

static guchar *
gsk_gl_buffer_map (GskGpuBuffer *buffer)
{
  GskGLBuffer *self = GSK_GL_BUFFER (buffer);

  return self->data;
}

static void
gsk_gl_buffer_unmap (GskGpuBuffer *buffer,
                     gsize         used)
{
}

static void
gsk_gl_buffer_class_init (GskGLBufferClass *klass)
{
  GskGpuBufferClass *buffer_class = GSK_GPU_BUFFER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  buffer_class->map = gsk_gl_buffer_map;
  buffer_class->unmap = gsk_gl_buffer_unmap;

  gobject_class->finalize = gsk_gl_buffer_finalize;
}

static void
gsk_gl_buffer_init (GskGLBuffer *self)
{
}

void
gsk_gl_buffer_bind (GskGLBuffer *self)
{
  glBindBuffer (self->target, self->buffer_id);
}

void
gsk_gl_buffer_bind_base (GskGLBuffer *self,
                         GLuint       index)
{
  glBindBufferBase (self->target, index, self->buffer_id);
}

static void
gsk_gl_buffer_setup (GskGLBuffer *self,
                     GLenum       target,
                     gsize        size)
{
  gsk_gpu_buffer_setup (GSK_GPU_BUFFER (self), size);

  self->target = target;

  glGenBuffers (1, &self->buffer_id);
}

/* }}} */
/* {{{ GskGLMappedBuffer */

struct _GskGLMappedBuffer
{
  GskGLBuffer parent_instance;
};

G_DEFINE_TYPE (GskGLMappedBuffer, gsk_gl_mapped_buffer, GSK_TYPE_GL_BUFFER)

static void
gsk_gl_mapped_buffer_finalize (GObject *object)
{
  GskGLBuffer *self = GSK_GL_BUFFER (object);

  gsk_gl_buffer_bind (self);
  glUnmapBuffer (self->target);

  G_OBJECT_CLASS (gsk_gl_mapped_buffer_parent_class)->finalize (object);
}

static void
gsk_gl_mapped_buffer_class_init (GskGLMappedBufferClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gsk_gl_mapped_buffer_finalize;
}

static void
gsk_gl_mapped_buffer_init (GskGLMappedBuffer *self)
{
}

GskGpuBuffer *
gsk_gl_mapped_buffer_new (GLenum target,
                          gsize  size)
{
  GskGLBuffer *self;

  self = g_object_new (GSK_TYPE_GL_MAPPED_BUFFER, NULL);

  gsk_gl_buffer_setup (self, target, size);
  gsk_gl_buffer_bind (self);

  glBufferStorage (target,
                   size,
                   NULL,
                   GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
  self->data = glMapBufferRange (target,
                                 0,
                                 size,
                                 GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

  return GSK_GPU_BUFFER (self);
}

/* }}} */
/* {{{ GskGLCopiedBuffer */

struct _GskGLCopiedBuffer
{
  GskGLBuffer parent_instance;
};

G_DEFINE_TYPE (GskGLCopiedBuffer, gsk_gl_copied_buffer, GSK_TYPE_GL_BUFFER)

static void
gsk_gl_copied_buffer_finalize (GObject *object)
{
  GskGLBuffer *self = GSK_GL_BUFFER (object);

  g_free (self->data);

  G_OBJECT_CLASS (gsk_gl_copied_buffer_parent_class)->finalize (object);
}

static void
gsk_gl_copied_buffer_unmap (GskGpuBuffer *buffer,
                            gsize         used)
{
  GskGLBuffer *self = GSK_GL_BUFFER (buffer);

  if (used == 0)
    return;

  gsk_gl_buffer_bind (self);

  glBufferSubData (self->target, 0, used, self->data);
}

static void
gsk_gl_copied_buffer_class_init (GskGLCopiedBufferClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GskGpuBufferClass *buffer_class = GSK_GPU_BUFFER_CLASS (klass);

  gobject_class->finalize = gsk_gl_copied_buffer_finalize;

  buffer_class->unmap = gsk_gl_copied_buffer_unmap;
}

static void
gsk_gl_copied_buffer_init (GskGLCopiedBuffer *self)
{
}

GskGpuBuffer *
gsk_gl_copied_buffer_new (GLenum target,
                          gsize  size)
{
  GskGLBuffer *self;

  self = g_object_new (GSK_TYPE_GL_COPIED_BUFFER, NULL);

  gsk_gl_buffer_setup (self, target, size);
  gsk_gl_buffer_bind (self);

  glBufferData (target, size, NULL, GL_STATIC_DRAW);
  self->data = malloc (size);

  return GSK_GPU_BUFFER (self);
}

/* }}} */
