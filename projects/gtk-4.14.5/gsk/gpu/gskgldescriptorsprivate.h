#pragma once

#include "gskgpudescriptorsprivate.h"

#include "gskgldeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_DESCRIPTORS (gsk_gl_descriptors_get_type ())

G_DECLARE_FINAL_TYPE (GskGLDescriptors, gsk_gl_descriptors, GSK, GL_DESCRIPTORS, GskGpuDescriptors)

GskGpuDescriptors *               gsk_gl_descriptors_new                (GskGLDevice            *device);

guint                             gsk_gl_descriptors_get_n_external     (GskGLDescriptors       *self);
void                              gsk_gl_descriptors_use                (GskGLDescriptors       *self);

G_END_DECLS

