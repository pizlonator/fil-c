#pragma once

#include <gsk/gskglshader.h>

G_BEGIN_DECLS

typedef struct {
  char *name;
  GskGLUniformType type;
  gsize offset;
} GskGLUniform;

const GskGLUniform *gsk_gl_shader_get_uniforms (GskGLShader *shader,
                                                int         *n_uniforms);

G_END_DECLS

