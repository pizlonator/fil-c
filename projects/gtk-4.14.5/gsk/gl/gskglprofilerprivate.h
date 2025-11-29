#pragma once

#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_GL_PROFILER (gsk_gl_profiler_get_type ())
G_DECLARE_FINAL_TYPE (GskGLProfiler, gsk_gl_profiler, GSK, GL_PROFILER, GObject)

GskGLProfiler * gsk_gl_profiler_new                     (GdkGLContext  *context);

void            gsk_gl_profiler_begin_gpu_region        (GskGLProfiler *profiler);
guint64         gsk_gl_profiler_end_gpu_region          (GskGLProfiler *profiler);

G_END_DECLS

