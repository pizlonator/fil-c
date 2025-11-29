/* OpenGL/OpenGL Area
 *
 * GtkGLArea is a widget that allows custom drawing using OpenGL calls.
 */

#include <math.h>
#include <gtk/gtk.h>
#include <epoxy/gl.h>

static GtkWidget *demo_window = NULL;

/* the GtkGLArea widget */
static GtkWidget *gl_area = NULL;

enum {
  X_AXIS,
  Y_AXIS,
  Z_AXIS,

  N_AXIS
};

/* Rotation angles on each axis */
static float rotation_angles[N_AXIS] = { 0.0 };

/* The object we are drawing */
static const GLfloat vertex_data[] = {
  0.f,   0.5f,   0.f, 1.f,
  0.5f, -0.366f, 0.f, 1.f,
 -0.5f, -0.366f, 0.f, 1.f,
};

/* Initialize the GL buffers */
static void
init_buffers (GLuint *vao_out,
              GLuint *buffer_out)
{
  GLuint vao, buffer;

  /* We only use one VAO, so we always keep it bound */
  glGenVertexArrays (1, &vao);
  glBindVertexArray (vao);

  /* This is the buffer that holds the vertices */
  glGenBuffers (1, &buffer);
  glBindBuffer (GL_ARRAY_BUFFER, buffer);
  glBufferData (GL_ARRAY_BUFFER, sizeof (vertex_data), vertex_data, GL_STATIC_DRAW);
  glBindBuffer (GL_ARRAY_BUFFER, 0);

  if (vao_out != NULL)
    *vao_out = vao;

  if (buffer_out != NULL)
    *buffer_out = buffer;
}

/* Create and compile a shader */
static GLuint
create_shader (int         type,
               const char *src)
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

      g_warning ("Compile failure in %s shader:\n%s",
                 type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                 buffer);

      g_free (buffer);

      glDeleteShader (shader);

      return 0;
    }

  return shader;
}

/* Initialize the shaders and link them into a program */
static void
init_shaders (const char *vertex_path,
              const char *fragment_path,
              GLuint *program_out,
              GLuint *mvp_out)
{
  GLuint vertex, fragment;
  GLuint program = 0;
  GLuint mvp = 0;
  int status;
  GBytes *source;

  source = g_resources_lookup_data (vertex_path, 0, NULL);
  vertex = create_shader (GL_VERTEX_SHADER, g_bytes_get_data (source, NULL));
  g_bytes_unref (source);

  if (vertex == 0)
    {
      *program_out = 0;
      return;
    }

  source = g_resources_lookup_data (fragment_path, 0, NULL);
  fragment = create_shader (GL_FRAGMENT_SHADER, g_bytes_get_data (source, NULL));
  g_bytes_unref (source);

  if (fragment == 0)
    {
      glDeleteShader (vertex);
      *program_out = 0;
      return;
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

      g_warning ("Linking failure:\n%s", buffer);

      g_free (buffer);

      glDeleteProgram (program);
      program = 0;

      goto out;
    }

  /* Get the location of the "mvp" uniform */
  mvp = glGetUniformLocation (program, "mvp");

  glDetachShader (program, vertex);
  glDetachShader (program, fragment);

out:
  glDeleteShader (vertex);
  glDeleteShader (fragment);

  if (program_out != NULL)
    *program_out = program;

  if (mvp_out != NULL)
    *mvp_out = mvp;
}

static void
compute_mvp (float *res,
             float  phi,
             float  theta,
             float  psi)
{
  float x = phi * (G_PI / 180.f);
  float y = theta * (G_PI / 180.f);
  float z = psi * (G_PI / 180.f);
  float c1 = cosf (x), s1 = sinf (x);
  float c2 = cosf (y), s2 = sinf (y);
  float c3 = cosf (z), s3 = sinf (z);
  float c3c2 = c3 * c2;
  float s3c1 = s3 * c1;
  float c3s2s1 = c3 * s2 * s1;
  float s3s1 = s3 * s1;
  float c3s2c1 = c3 * s2 * c1;
  float s3c2 = s3 * c2;
  float c3c1 = c3 * c1;
  float s3s2s1 = s3 * s2 * s1;
  float c3s1 = c3 * s1;
  float s3s2c1 = s3 * s2 * c1;
  float c2s1 = c2 * s1;
  float c2c1 = c2 * c1;

  /* initialize to the identity matrix */
  res[0] = 1.f; res[4] = 0.f;  res[8] = 0.f; res[12] = 0.f;
  res[1] = 0.f; res[5] = 1.f;  res[9] = 0.f; res[13] = 0.f;
  res[2] = 0.f; res[6] = 0.f; res[10] = 1.f; res[14] = 0.f;
  res[3] = 0.f; res[7] = 0.f; res[11] = 0.f; res[15] = 1.f;

  /* apply all three rotations using the three matrices:
   *
   * ⎡  c3 s3 0 ⎤ ⎡ c2  0 -s2 ⎤ ⎡ 1   0  0 ⎤
   * ⎢ -s3 c3 0 ⎥ ⎢  0  1   0 ⎥ ⎢ 0  c1 s1 ⎥
   * ⎣   0  0 1 ⎦ ⎣ s2  0  c2 ⎦ ⎣ 0 -s1 c1 ⎦
   */
  res[0] = c3c2;  res[4] = s3c1 + c3s2s1;  res[8] = s3s1 - c3s2c1; res[12] = 0.f;
  res[1] = -s3c2; res[5] = c3c1 - s3s2s1;  res[9] = c3s1 + s3s2c1; res[13] = 0.f;
  res[2] = s2;    res[6] = -c2s1;         res[10] = c2c1;          res[14] = 0.f;
  res[3] = 0.f;   res[7] = 0.f;           res[11] = 0.f;           res[15] = 1.f;
}

static GLuint position_buffer;
static GLuint program;
static GLuint mvp_location;

/* We need to set up our state when we realize the GtkGLArea widget */
static void
realize (GtkWidget *widget)
{
  const char *vertex_path, *fragment_path;

  gtk_gl_area_make_current (GTK_GL_AREA (widget));

  if (gtk_gl_area_get_error (GTK_GL_AREA (widget)) != NULL)
    return;

  if (gtk_gl_area_get_api (GTK_GL_AREA (widget)) == GDK_GL_API_GLES)
    {
      vertex_path = "/glarea/glarea-gles.vs.glsl";
      fragment_path = "/glarea/glarea-gles.fs.glsl";
    }
  else
    {
      vertex_path = "/glarea/glarea-gl.vs.glsl";
      fragment_path = "/glarea/glarea-gl.fs.glsl";
    }

  init_buffers (NULL, &position_buffer);
  init_shaders (vertex_path, fragment_path, &program, &mvp_location);
}

/* We should tear down the state when unrealizing */
static void
unrealize (GtkWidget *widget)
{
  gtk_gl_area_make_current (GTK_GL_AREA (widget));

  if (gtk_gl_area_get_error (GTK_GL_AREA (widget)) != NULL)
    return;

  glDeleteBuffers (1, &position_buffer);
  glDeleteProgram (program);
}

static void
draw_triangle (void)
{
  float mvp[16];

  /* Compute the model view projection matrix using the
   * rotation angles specified through the GtkRange widgets
   */
  compute_mvp (mvp,
               rotation_angles[X_AXIS],
               rotation_angles[Y_AXIS],
               rotation_angles[Z_AXIS]);

  /* Use our shaders */
  glUseProgram (program);

  /* Update the "mvp" matrix we use in the shader */
  glUniformMatrix4fv (mvp_location, 1, GL_FALSE, &mvp[0]);

  /* Use the vertices in our buffer */
  glBindBuffer (GL_ARRAY_BUFFER, position_buffer);
  glEnableVertexAttribArray (0);
  glVertexAttribPointer (0, 4, GL_FLOAT, GL_FALSE, 0, 0);

  /* Draw the three vertices as a triangle */
  glDrawArrays (GL_TRIANGLES, 0, 3);

  /* We finished using the buffers and program */
  glDisableVertexAttribArray (0);
  glBindBuffer (GL_ARRAY_BUFFER, 0);
  glUseProgram (0);
}

static gboolean
render (GtkGLArea    *area,
        GdkGLContext *context)
{
  if (gtk_gl_area_get_error (area) != NULL)
    return FALSE;

  /* Clear the viewport */
  glClearColor (0.5, 0.5, 0.5, 1.0);
  glClear (GL_COLOR_BUFFER_BIT);

  /* Draw our object */
  draw_triangle ();

  /* Flush the contents of the pipeline */
  glFlush ();

  return TRUE;
}

static void
on_axis_value_change (GtkAdjustment *adjustment,
                      gpointer       data)
{
  int axis = GPOINTER_TO_INT (data);

  g_assert (axis >= 0 && axis < N_AXIS);

  /* Update the rotation angle */
  rotation_angles[axis] = gtk_adjustment_get_value (adjustment);

  /* Update the contents of the GL drawing area */
  gtk_widget_queue_draw (gl_area);
}

static GtkWidget *
create_axis_slider (int axis)
{
  GtkWidget *box, *label, *slider;
  GtkAdjustment *adj;
  const char *text;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  switch (axis)
    {
    case X_AXIS:
      text = "X axis";
      break;

    case Y_AXIS:
      text = "Y axis";
      break;

    case Z_AXIS:
      text = "Z axis";
      break;

    default:
      g_assert_not_reached ();
    }

  label = gtk_label_new (text);
  gtk_box_append (GTK_BOX (box), label);

  adj = gtk_adjustment_new (0.0, 0.0, 360.0, 1.0, 12.0, 0.0);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (on_axis_value_change),
                    GINT_TO_POINTER (axis));
  slider = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adj);
  gtk_box_append (GTK_BOX (box), slider);
  gtk_widget_set_hexpand (slider, TRUE);

  return box;
}

static void
close_window (GtkWidget *widget)
{
  /* Reset the state */
  demo_window = NULL;
  gl_area = NULL;

  rotation_angles[X_AXIS] = 0.0;
  rotation_angles[Y_AXIS] = 0.0;
  rotation_angles[Z_AXIS] = 0.0;
}

static GtkWidget *
create_glarea_window (GtkWidget *do_widget)
{
  GtkWidget *window, *box, *button, *controls;
  int i;

  window = gtk_window_new ();
  gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
  gtk_window_set_title (GTK_WINDOW (window), "OpenGL Area");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);
  g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_widget_set_margin_top (box, 12);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_window_set_child (GTK_WINDOW (window), box);

  gl_area = gtk_gl_area_new ();
  gtk_widget_set_hexpand (gl_area, TRUE);
  gtk_widget_set_vexpand (gl_area, TRUE);
  gtk_widget_set_size_request (gl_area, 100, 200);
  gtk_box_append (GTK_BOX (box), gl_area);

  /* We need to initialize and free GL resources, so we use
   * the realize and unrealize signals on the widget
   */
  g_signal_connect (gl_area, "realize", G_CALLBACK (realize), NULL);
  g_signal_connect (gl_area, "unrealize", G_CALLBACK (unrealize), NULL);

  /* The main "draw" call for GtkGLArea */
  g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);

  controls = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_box_append (GTK_BOX (box), controls);
  gtk_widget_set_hexpand (controls, TRUE);

  for (i = 0; i < N_AXIS; i++)
    gtk_box_append (GTK_BOX (controls), create_axis_slider (i));

  button = gtk_button_new_with_label ("Quit");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_box_append (GTK_BOX (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_window_destroy), window);

  return window;
}

GtkWidget*
do_glarea (GtkWidget *do_widget)
{
  if (demo_window == NULL)
    demo_window = create_glarea_window (do_widget);

  if (!gtk_widget_get_visible (demo_window))
    gtk_widget_set_visible (demo_window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (demo_window));

  return demo_window;
}
