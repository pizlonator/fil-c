#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <epoxy/gl.h>

#include "gtkshadertoy.h"

const char *default_image_shader =
  "void mainImage(out vec4 fragColor, in vec2 fragCoord) {\n"
  "    // Normalized pixel coordinates (from 0 to 1)\n"
  "    vec2 uv = fragCoord/iResolution.xy;\n"
  "\n"
  "   // Time varying pixel color\n"
  "    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));\n"
  "\n"
  "    if (distance(iMouse.xy, fragCoord.xy) <= 10.0) {\n"
  "        col = vec3(0.0);\n"
  "    }\n"
  "\n"
  "    // Output to screen\n"
  "    fragColor = vec4(col,1.0);\n"
  "}\n";

const char *shadertoy_vertex_shader =
  "#version 150 core\n"
  "\n"
  "uniform vec3 iResolution;\n"
  "\n"
  "in vec2 position;\n"
  "out vec2 fragCoord;\n"
  "\n"
  "void main() {\n"
  "    gl_Position = vec4(position, 0.0, 1.0);\n"
  "\n"
  "    // Convert from OpenGL coordinate system (with origin in center\n"
  "    // of screen) to Shadertoy/texture coordinate system (with origin\n"
  "    // in lower left corner)\n"
  "    fragCoord = (gl_Position.xy + vec2(1.0)) / vec2(2.0) * iResolution.xy;\n"
  "}\n";

const char *fragment_prefix =
  "#version 150 core\n"
  "\n"
  "uniform vec3      iResolution;           // viewport resolution (in pixels)\n"
  "uniform float     iTime;                 // shader playback time (in seconds)\n"
  "uniform float     iTimeDelta;            // render time (in seconds)\n"
  "uniform int       iFrame;                // shader playback frame\n"
  "uniform float     iChannelTime[4];       // channel playback time (in seconds)\n"
  "uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)\n"
  "uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click\n"
  "uniform sampler2D iChannel0;\n"
  "uniform sampler2D iChannel1;\n"
  "uniform sampler2D iChannel2;\n"
  "uniform sampler2D iChannel3;\n"
  "uniform vec4      iDate;                 // (year, month, day, time in seconds)\n"
  "uniform float     iSampleRate;           // sound sample rate (i.e., 44100)\n"
  "\n"
  "in vec2 fragCoord;\n"
  "out vec4 vFragColor;\n";


// Fragment shader suffix
const char *fragment_suffix =
  "    void main() {\n"
  "        vec4 c;\n"
  "        mainImage(c, fragCoord);\n"
  "         vFragColor = c;\n"
  "    }\n";

typedef struct {
  char *image_shader;
  gboolean image_shader_dirty;

  gboolean error_set;

  /* Vertex buffers */
  GLuint vao;
  GLuint buffer;

  /* Active program */
  GLuint program;

  /* Location of uniforms for program */
  GLuint resolution_location;
  GLuint time_location;
  GLuint timedelta_location;
  GLuint frame_location;
  GLuint mouse_location;

  /* Current uniform values */
  float resolution[3];
  float time;
  float timedelta;
  float mouse[4];
  int frame;

  /* Animation data */
  gint64 first_frame_time;
  gint64 first_frame;
  guint tick;
} GtkShadertoyPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkShadertoy, gtk_shadertoy, GTK_TYPE_GL_AREA)

static gboolean gtk_shadertoy_render        (GtkGLArea     *area,
                                             GdkGLContext  *context);
static void     gtk_shadertoy_reshape       (GtkGLArea     *area,
                                             int            width,
                                             int            height);
static void     gtk_shadertoy_realize       (GtkWidget     *widget);
static void     gtk_shadertoy_unrealize     (GtkWidget     *widget);
static gboolean gtk_shadertoy_tick          (GtkWidget     *widget,
                                             GdkFrameClock *frame_clock,
                                             gpointer       user_data);

GtkWidget *
gtk_shadertoy_new (void)
{
  return g_object_new (gtk_shadertoy_get_type (),
                       "allowed-apis", GDK_GL_API_GL,
                       NULL);
}

static void
drag_begin_cb (GtkGestureDrag *drag,
               double          x,
               double          y,
               gpointer        user_data)
{
  GtkShadertoy *shadertoy = GTK_SHADERTOY (user_data);
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);
  int height = gtk_widget_get_height (GTK_WIDGET (shadertoy));
  int scale = gtk_widget_get_scale_factor (GTK_WIDGET (shadertoy));

 priv->mouse[0] = x * scale;
 priv->mouse[1] = (height - y) * scale;
 priv->mouse[2] = priv->mouse[0];
 priv->mouse[3] = priv->mouse[1];
}

static void
drag_update_cb (GtkGestureDrag *drag,
                double          dx,
                double          dy,
                gpointer        user_data)
{
  GtkShadertoy *shadertoy = GTK_SHADERTOY (user_data);
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);
  int width = gtk_widget_get_width (GTK_WIDGET (shadertoy));
  int height = gtk_widget_get_height (GTK_WIDGET (shadertoy));
  int scale = gtk_widget_get_scale_factor (GTK_WIDGET (shadertoy));
  double x, y;

  gtk_gesture_drag_get_start_point (drag, &x, &y);
  x += dx;
  y += dy;

  if (x >= 0 && x < width &&
      y >= 0 && y < height)
    {
      priv->mouse[0] = x * scale;
      priv->mouse[1] = (height - y) * scale;
    }
}

static void
drag_end_cb (GtkGestureDrag *drag,
             double          dx,
             double          dy,
             gpointer        user_data)
{
  GtkShadertoy *shadertoy = GTK_SHADERTOY (user_data);
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);

  priv->mouse[2] = -priv->mouse[2];
  priv->mouse[3] = -priv->mouse[3];
}

static void
gtk_shadertoy_init (GtkShadertoy *shadertoy)
{
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);
  GtkGesture *drag;

  priv->image_shader = g_strdup (default_image_shader);
  priv->tick = gtk_widget_add_tick_callback (GTK_WIDGET (shadertoy), gtk_shadertoy_tick, shadertoy, NULL);

  drag = gtk_gesture_drag_new ();
  gtk_widget_add_controller (GTK_WIDGET (shadertoy), GTK_EVENT_CONTROLLER (drag));
  g_signal_connect (drag, "drag-begin", (GCallback)drag_begin_cb, shadertoy);
  g_signal_connect (drag, "drag-update", (GCallback)drag_update_cb, shadertoy);
  g_signal_connect (drag, "drag-end", (GCallback)drag_end_cb, shadertoy);
}

static void
gtk_shadertoy_finalize (GObject *obj)
{
  GtkShadertoy *shadertoy = GTK_SHADERTOY (obj);
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);

  gtk_widget_remove_tick_callback (GTK_WIDGET (shadertoy), priv->tick);
  g_free (priv->image_shader);

  G_OBJECT_CLASS (gtk_shadertoy_parent_class)->finalize (obj);
}

static void
gtk_shadertoy_class_init (GtkShadertoyClass *klass)
{
  GTK_GL_AREA_CLASS (klass)->render = gtk_shadertoy_render;
  GTK_GL_AREA_CLASS (klass)->resize = gtk_shadertoy_reshape;

  GTK_WIDGET_CLASS (klass)->realize = gtk_shadertoy_realize;
  GTK_WIDGET_CLASS (klass)->unrealize = gtk_shadertoy_unrealize;

  G_OBJECT_CLASS (klass)->finalize = gtk_shadertoy_finalize;
}

/* new window size or exposure */
static void
gtk_shadertoy_reshape (GtkGLArea *area, int width, int height)
{
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private ((GtkShadertoy *) area);

  priv->resolution[0] = width;
  priv->resolution[1] = height;
  priv->resolution[2] = 1.0; /* screen aspect ratio */

  /* Set the viewport */
  glViewport (0, 0, (GLint) width, (GLint) height);
}

static GLuint
create_shader (int         type,
               const char *src,
               GError    **error)
{
  GLuint shader;
  int status;

  shader = glCreateShader (type);
  glShaderSource (shader, 1, &src, NULL);
  glCompileShader (shader);

  glGetShaderiv (shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
    {
      int log_len;
      char *buffer;

      glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc (log_len + 1);
      glGetShaderInfoLog (shader, log_len, NULL, buffer);

      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_COMPILATION_FAILED,
                   "Compile failure in %s shader:\n%s",
                   type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                   buffer);

      g_free (buffer);

      glDeleteShader (shader);

      return 0;
    }

  return shader;
}

static gboolean
init_shaders (GtkShadertoy *shadertoy,
              const char *vertex_source,
              const char *fragment_source,
              GError **error)
{
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);
  GLuint vertex, fragment;
  GLuint program = 0;
  int status;
  gboolean res = TRUE;

  vertex = create_shader (GL_VERTEX_SHADER, vertex_source, error);
  if (vertex == 0)
    return FALSE;

  fragment = create_shader (GL_FRAGMENT_SHADER, fragment_source, error);
  if (fragment == 0)
    {
      glDeleteShader (vertex);
      return FALSE;
    }

  program = glCreateProgram ();
  glAttachShader (program, vertex);
  glAttachShader (program, fragment);

  glLinkProgram (program);

  glGetProgramiv (program, GL_LINK_STATUS, &status);
  if (status == GL_FALSE)
    {
      int log_len;
      char *buffer;

      glGetProgramiv (program, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc (log_len + 1);
      glGetProgramInfoLog (program, log_len, NULL, buffer);

      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_LINK_FAILED,
                   "Linking failure:\n%s", buffer);
      res = FALSE;

      g_free (buffer);

      glDeleteProgram (program);

      goto out;
    }

  if (priv->program != 0)
    glDeleteProgram (priv->program);

  priv->program = program;
  priv->resolution_location = glGetUniformLocation (program, "iResolution");
  priv->time_location = glGetUniformLocation (program, "iTime");
  priv->timedelta_location = glGetUniformLocation (program, "iTimeDelta");
  priv->frame_location = glGetUniformLocation (program, "iFrame");
  priv->mouse_location = glGetUniformLocation (program, "iMouse");

  glDetachShader (program, vertex);
  glDetachShader (program, fragment);

out:
  /* These are now owned by the program and can be deleted */
  glDeleteShader (vertex);
  glDeleteShader (fragment);

  return res;
}

static void
gtk_shadertoy_realize_shader (GtkShadertoy *shadertoy)
{
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);
  char *fragment_shader;
  GError *error = NULL;

  fragment_shader = g_strconcat (fragment_prefix, priv->image_shader, fragment_suffix, NULL);
  if (!init_shaders (shadertoy, shadertoy_vertex_shader, fragment_shader, &error))
    {
      priv->error_set = TRUE;
      gtk_gl_area_set_error (GTK_GL_AREA (shadertoy), error);
      g_error_free (error);
    }
  g_free (fragment_shader);

  /* Start new shader at time zero */
  priv->first_frame_time = 0;
  priv->first_frame = 0;

  priv->image_shader_dirty = FALSE;
}

static gboolean
gtk_shadertoy_render (GtkGLArea    *area,
                      GdkGLContext *context)
{
  GtkShadertoy *shadertoy = GTK_SHADERTOY (area);
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);

  if (gtk_gl_area_get_error (area) != NULL)
    return FALSE;

  if (priv->image_shader_dirty)
    gtk_shadertoy_realize_shader (shadertoy);

  /* Clear the viewport */
  glClearColor (0.0, 0.0, 0.0, 1.0);
  glClear (GL_COLOR_BUFFER_BIT);

  glUseProgram (priv->program);

  /* Update uniforms */
  if (priv->resolution_location != -1)
    glUniform3fv (priv->resolution_location, 1, priv->resolution);
  if (priv->time_location != -1)
    glUniform1f (priv->time_location, priv->time);
  if (priv->timedelta_location != -1)
    glUniform1f (priv->timedelta_location, priv->timedelta);
  if (priv->frame_location != -1)
    glUniform1i (priv->frame_location, priv->frame);
  if (priv->mouse_location != -1)
    glUniform4fv (priv->mouse_location, 1, priv->mouse);

  /* Use the vertices in our buffer */
  glBindBuffer (GL_ARRAY_BUFFER, priv->buffer);
  glEnableVertexAttribArray (0);
  glVertexAttribPointer (0, 4, GL_FLOAT, GL_FALSE, 0, 0);

  glDrawArrays (GL_TRIANGLES, 0, 6);

  /* We finished using the buffers and program */
  glDisableVertexAttribArray (0);
  glBindBuffer (GL_ARRAY_BUFFER, 0);
  glUseProgram (0);

  /* Flush the contents of the pipeline */
  glFlush ();

  return TRUE;
}

const char *
gtk_shadertoy_get_image_shader (GtkShadertoy *shadertoy)
{
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);

  return priv->image_shader;
}

void
gtk_shadertoy_set_image_shader (GtkShadertoy *shadertoy,
                                const char   *shader)
{
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);

  g_free (priv->image_shader);
  priv->image_shader = g_strdup (shader);

  /* Don't override error we didn't set it ourselves */
  if (priv->error_set)
    {
      gtk_gl_area_set_error (GTK_GL_AREA (shadertoy), NULL);
      priv->error_set = FALSE;
    }
  priv->image_shader_dirty = TRUE;
}

static void
gtk_shadertoy_realize (GtkWidget *widget)
{
  GtkGLArea *glarea = GTK_GL_AREA (widget);
  GtkShadertoy *shadertoy = GTK_SHADERTOY (widget);
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);

  /* Draw two triangles across whole screen */
  const GLfloat vertex_data[] = {
    -1.0f, -1.0f, 0.f, 1.f,
    -1.0f,  1.0f, 0.f, 1.f,
    1.0f,  1.0f, 0.f, 1.f,

    -1.0f, -1.0f, 0.f, 1.f,
    1.0f,  1.0f, 0.f, 1.f,
    1.0f, -1.0f, 0.f, 1.f,
  };

  GTK_WIDGET_CLASS (gtk_shadertoy_parent_class)->realize (widget);

  gtk_gl_area_make_current (glarea);
  if (gtk_gl_area_get_error (glarea) != NULL)
    return;

  glGenVertexArrays (1, &priv->vao);
  glBindVertexArray (priv->vao);

  glGenBuffers (1, &priv->buffer);
  glBindBuffer (GL_ARRAY_BUFFER, priv->buffer);
  glBufferData (GL_ARRAY_BUFFER, sizeof (vertex_data), vertex_data, GL_STATIC_DRAW);
  glBindBuffer (GL_ARRAY_BUFFER, 0);

  gtk_shadertoy_realize_shader (shadertoy);
}

static void
gtk_shadertoy_unrealize (GtkWidget *widget)
{
  GtkGLArea *glarea = GTK_GL_AREA (widget);
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private ((GtkShadertoy *) widget);

  gtk_gl_area_make_current (glarea);
  if (gtk_gl_area_get_error (glarea) == NULL)
    {
      if (priv->buffer != 0)
        glDeleteBuffers (1, &priv->buffer);

      if (priv->vao != 0)
        glDeleteVertexArrays (1, &priv->vao);

      if (priv->program != 0)
        glDeleteProgram (priv->program);
    }

  GTK_WIDGET_CLASS (gtk_shadertoy_parent_class)->unrealize (widget);
}

static gboolean
gtk_shadertoy_tick (GtkWidget     *widget,
                    GdkFrameClock *frame_clock,
                    gpointer       user_data)
{
  GtkShadertoy *shadertoy = GTK_SHADERTOY (widget);
  GtkShadertoyPrivate *priv = gtk_shadertoy_get_instance_private (shadertoy);
  gint64 frame_time;
  gint64 frame;
  float previous_time;

  frame = gdk_frame_clock_get_frame_counter (frame_clock);
  frame_time = gdk_frame_clock_get_frame_time (frame_clock);

  if (priv->first_frame_time == 0)
    {
      priv->first_frame_time = frame_time;
      priv->first_frame = frame;
      previous_time = 0;
    }
  else
    previous_time = priv->time;

  priv->time = (frame_time - priv->first_frame_time) / 1000000.0f;
  priv->frame = frame - priv->first_frame;
  priv->timedelta = priv->time - previous_time;

  gtk_widget_queue_draw (widget);

  return G_SOURCE_CONTINUE;
}
