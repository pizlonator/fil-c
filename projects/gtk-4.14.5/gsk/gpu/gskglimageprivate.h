#pragma once

#include "gskgpuimageprivate.h"

#include "gskgldeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_IMAGE (gsk_gl_image_get_type ())

G_DECLARE_FINAL_TYPE (GskGLImage, gsk_gl_image, GSK, GL_IMAGE, GskGpuImage)

GskGpuImage *           gsk_gl_image_new_backbuffer                     (GskGLDevice            *device,
                                                                         GdkGLContext           *context,
                                                                         GdkMemoryFormat         format,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_gl_image_new                                (GskGLDevice            *device,
                                                                         GdkMemoryFormat         format,
                                                                         GskGpuImageFlags        required_flags,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_gl_image_new_for_texture                    (GskGLDevice            *device,
                                                                         GdkTexture             *owner,
                                                                         GLuint                  tex_id,
                                                                         gboolean                take_ownership,
                                                                         GskGpuImageFlags        extra_flags);
                                                                         

void                    gsk_gl_image_bind_texture                       (GskGLImage             *self);
void                    gsk_gl_image_bind_framebuffer                   (GskGLImage             *self);
void                    gsk_gl_image_bind_framebuffer_target            (GskGLImage             *self,
                                                                         GLenum                  target);

gboolean                gsk_gl_image_is_flipped                         (GskGLImage             *self);
GLint                   gsk_gl_image_get_gl_internal_format             (GskGLImage             *self);
GLenum                  gsk_gl_image_get_gl_format                      (GskGLImage             *self);
GLenum                  gsk_gl_image_get_gl_type                        (GskGLImage             *self);

GLuint                  gsk_gl_image_steal_texture                      (GskGLImage             *self);

G_END_DECLS
