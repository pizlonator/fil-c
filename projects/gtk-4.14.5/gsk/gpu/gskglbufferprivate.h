#pragma once

#include "gskgpubufferprivate.h"

#include "gskgldeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_BUFFER (gsk_gl_buffer_get_type ())
#define GSK_TYPE_GL_MAPPED_BUFFER (gsk_gl_mapped_buffer_get_type ())
#define GSK_TYPE_GL_COPIED_BUFFER (gsk_gl_copied_buffer_get_type ())

G_DECLARE_FINAL_TYPE (GskGLBuffer, gsk_gl_buffer, GSK, GL_BUFFER, GskGpuBuffer)
G_DECLARE_FINAL_TYPE (GskGLMappedBuffer, gsk_gl_mapped_buffer, GSK, GL_MAPPED_BUFFER, GskGLBuffer)
G_DECLARE_FINAL_TYPE (GskGLCopiedBuffer, gsk_gl_copied_buffer, GSK, GL_COPIED_BUFFER, GskGLBuffer)

GskGpuBuffer *          gsk_gl_mapped_buffer_new                        (GLenum                  target,
                                                                         gsize                   size);
GskGpuBuffer *          gsk_gl_copied_buffer_new                        (GLenum                  target,
                                                                         gsize                   size);

void                    gsk_gl_buffer_bind                              (GskGLBuffer            *self);
void                    gsk_gl_buffer_bind_base                         (GskGLBuffer            *self,
                                                                         GLuint                  index);

G_END_DECLS

