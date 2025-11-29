#pragma once

#include "gskgpuframeprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_FRAME (gsk_gl_frame_get_type ())

G_DECLARE_FINAL_TYPE (GskGLFrame, gsk_gl_frame, GSK, GL_FRAME, GskGpuFrame)

void                    gsk_gl_frame_use_program                        (GskGLFrame             *self,
                                                                         const GskGpuShaderOpClass *op_class,
                                                                         guint32                 variation,
                                                                         GskGpuShaderClip        clip,
                                                                         guint                   n_external_textures);

void                    gsk_gl_frame_bind_globals                       (GskGLFrame             *self);

G_END_DECLS
