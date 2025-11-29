#include "config.h"

#include "gskglprofilerprivate.h"

#include <epoxy/gl.h>

#define N_QUERIES       4

struct _GskGLProfiler
{
  GObject parent_instance;

  GdkGLContext *gl_context;

  /* Creating GL queries is kind of expensive, so we pay the
   * price upfront and create a circular buffer of queries
   */
  GLuint gl_queries[N_QUERIES];
  GLuint active_query;

  unsigned int has_queries : 1;
  unsigned int has_timer : 1;
  unsigned int first_frame : 1;
};

enum {
  PROP_GL_CONTEXT = 1,

  N_PROPERTIES
};

static GParamSpec *gsk_gl_profiler_properties[N_PROPERTIES];

G_DEFINE_TYPE (GskGLProfiler, gsk_gl_profiler, G_TYPE_OBJECT)

static void
gsk_gl_profiler_finalize (GObject *gobject)
{
  GskGLProfiler *self = GSK_GL_PROFILER (gobject);

  if (self->has_queries)
    glDeleteQueries (N_QUERIES, self->gl_queries);

  g_clear_object (&self->gl_context);

  G_OBJECT_CLASS (gsk_gl_profiler_parent_class)->finalize (gobject);
}

static void
gsk_gl_profiler_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GskGLProfiler *self = GSK_GL_PROFILER (gobject);

  switch (prop_id)
    {
    case PROP_GL_CONTEXT:
      self->gl_context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gsk_gl_profiler_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GskGLProfiler *self = GSK_GL_PROFILER (gobject);

  switch (prop_id)
    {
    case PROP_GL_CONTEXT:
      g_value_set_object (value, self->gl_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gsk_gl_profiler_class_init (GskGLProfilerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gsk_gl_profiler_set_property;
  gobject_class->get_property = gsk_gl_profiler_get_property;
  gobject_class->finalize = gsk_gl_profiler_finalize;

  gsk_gl_profiler_properties[PROP_GL_CONTEXT] =
    g_param_spec_object ("gl-context", NULL, NULL,
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, gsk_gl_profiler_properties);
}

static void
gsk_gl_profiler_init (GskGLProfiler *self)
{
  self->has_queries = epoxy_is_desktop_gl();
  self->has_timer = epoxy_is_desktop_gl() && (epoxy_gl_version () >= 33 || epoxy_has_gl_extension ("GL_ARB_timer_query"));

  if (!self->has_queries)
    return;

  glGenQueries (N_QUERIES, self->gl_queries);
  self->first_frame = TRUE;
}

GskGLProfiler *
gsk_gl_profiler_new (GdkGLContext *context)
{
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return g_object_new (GSK_TYPE_GL_PROFILER, "gl-context", context, NULL);
}

void
gsk_gl_profiler_begin_gpu_region (GskGLProfiler *profiler)
{
  GLuint query_id;

  g_return_if_fail (GSK_IS_GL_PROFILER (profiler));

  if (!profiler->has_timer || !profiler->has_queries)
    return;

  query_id = profiler->gl_queries[profiler->active_query];
  glBeginQuery (GL_TIME_ELAPSED, query_id);
}

guint64
gsk_gl_profiler_end_gpu_region (GskGLProfiler *profiler)
{
  GLuint last_query_id;
  GLint res;
  GLuint64 elapsed;

  g_return_val_if_fail (GSK_IS_GL_PROFILER (profiler), 0);

  if (!profiler->has_timer || !profiler->has_queries)
    return 0;

  glEndQuery (GL_TIME_ELAPSED);

  if (profiler->active_query == 0)
    last_query_id = N_QUERIES - 1;
  else
    last_query_id = profiler->active_query - 1;

  /* Advance iterator */
  profiler->active_query += 1;
  if (profiler->active_query == N_QUERIES)
    profiler->active_query = 0;

  /* If this is the first frame we already have a result */
  if (profiler->first_frame)
    {
      profiler->first_frame = FALSE;
      return 0;
    }

  glGetQueryObjectiv (profiler->gl_queries[last_query_id], GL_QUERY_RESULT_AVAILABLE, &res);
  if (res == 1)
    glGetQueryObjectui64v (profiler->gl_queries[last_query_id], GL_QUERY_RESULT, &elapsed);
  else
    elapsed = 0;

  return elapsed / 1000; /* Convert to usec to match other profiler APIs */
}
