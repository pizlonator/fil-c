/* The rendering code in here is taken from es2gears, which has the
 * following copyright notice:
 *
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Ported to GLES2.
 * Kristian Høgsberg <krh@bitplanet.net>
 * May 3, 2010
 *
 * Improve GLES2 port:
 *   * Refactor gear drawing.
 *   * Use correct normals for surfaces.
 *   * Improve shader.
 *   * Use perspective projection transformation.
 *   * Add FPS count.
 *   * Add comments.
 * Alexandros Frantzis <alexandros.frantzis@linaro.org>
 * Jul 13, 2010
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <epoxy/gl.h>

#include "gtkgears.h"

#define STRIPS_PER_TOOTH 7
#define VERTICES_PER_TOOTH 34
#define GEAR_VERTEX_STRIDE 6

static inline void
_sincos (double x, double *_sin, double *_cos)
{
#ifdef HAVE_SINCOS
  sincos (x, _sin, _cos);
#else
  *_sin = sin (x);
  *_cos = cos (x);
#endif
}

/**
 * Struct describing the vertices in triangle strip
 */
struct vertex_strip {
   /** The first vertex in the strip */
   GLint first;
   /** The number of consecutive vertices in the strip after the first */
   GLint count;
};

/* Each vertex consist of GEAR_VERTEX_STRIDE GLfloat attributes */
typedef GLfloat GearVertex[GEAR_VERTEX_STRIDE];

/**
 * Struct representing a gear.
 */
struct gear {
   /** The array of vertices comprising the gear */
   GearVertex *vertices;
   /** The number of vertices comprising the gear */
   int nvertices;
   /** The array of triangle strips comprising the gear */
   struct vertex_strip *strips;
   /** The number of triangle strips comprising the gear */
   int nstrips;
};

typedef struct {
  /* The view rotation [x, y, z] */
  GLfloat view_rot[GTK_GEARS_N_AXIS];

  /* The Vertex Array Object */
  GLuint vao;

  /* The shader program */
  GLuint program;

  /* The gears */
  struct gear *gear1;
  struct gear *gear2;
  struct gear *gear3;

  /** The Vertex Buffer Object holding the vertices in the graphics card */
  GLuint gear_vbo[3];

  /** The location of the shader uniforms */
  GLuint ModelViewProjectionMatrix_location;
  GLuint NormalMatrix_location;
  GLuint LightSourcePosition_location;
  GLuint MaterialColor_location;

  /* The current gear rotation angle */
  GLfloat angle;

  /* The projection matrix */
  GLfloat ProjectionMatrix[16];

  /* The direction of the directional light for the scene */
  GLfloat LightSourcePosition[4];

  gint64 first_frame_time;
  guint tick;
  GtkLabel *fps_label;
} GtkGearsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkGears, gtk_gears, GTK_TYPE_GL_AREA)

static gboolean gtk_gears_render        (GtkGLArea     *area,
                                         GdkGLContext  *context);
static void     gtk_gears_reshape       (GtkGLArea     *area,
                                         int            width,
                                         int            height);
static void     gtk_gears_realize       (GtkWidget     *widget);
static void     gtk_gears_unrealize     (GtkWidget     *widget);
static gboolean gtk_gears_tick          (GtkWidget     *widget,
                                         GdkFrameClock *frame_clock,
                                         gpointer       user_data);

static void destroy_gear (struct gear *g);

GtkWidget *
gtk_gears_new (void)
{
  return g_object_new (gtk_gears_get_type (),
                       "has-depth-buffer", TRUE,
                       NULL);
}

static void
gtk_gears_init (GtkGears *gears)
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);

  priv->view_rot[GTK_GEARS_X_AXIS] = 20.0;
  priv->view_rot[GTK_GEARS_Y_AXIS] = 30.0;
  priv->view_rot[GTK_GEARS_Z_AXIS] = 20.0;

  priv->LightSourcePosition[0] = 5.0;
  priv->LightSourcePosition[1] = 5.0;
  priv->LightSourcePosition[2] = 10.0;
  priv->LightSourcePosition[3] = 1.0;

  priv->tick = gtk_widget_add_tick_callback (GTK_WIDGET (gears), gtk_gears_tick, gears, NULL);
}

static void
gtk_gears_finalize (GObject *obj)
{
  GtkGears *gears = GTK_GEARS (obj);
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);

  gtk_widget_remove_tick_callback (GTK_WIDGET (gears), priv->tick);

  g_clear_object (&priv->fps_label);

  g_clear_pointer (&priv->gear1, destroy_gear);
  g_clear_pointer (&priv->gear2, destroy_gear);
  g_clear_pointer (&priv->gear3, destroy_gear);

  G_OBJECT_CLASS (gtk_gears_parent_class)->finalize (obj);
}

static void
gtk_gears_class_init (GtkGearsClass *klass)
{
  GTK_GL_AREA_CLASS (klass)->render = gtk_gears_render;
  GTK_GL_AREA_CLASS (klass)->resize = gtk_gears_reshape;

  GTK_WIDGET_CLASS (klass)->realize = gtk_gears_realize;
  GTK_WIDGET_CLASS (klass)->unrealize = gtk_gears_unrealize;

  G_OBJECT_CLASS (klass)->finalize = gtk_gears_finalize;
}

/*
 * Fills a gear vertex.
 *
 * @param v the vertex to fill
 * @param x the x coordinate
 * @param y the y coordinate
 * @param z the z coordinate
 * @param n pointer to the normal table
 *
 * @return the operation error code
 */
static GearVertex *
vert (GearVertex *v,
      GLfloat x,
      GLfloat y,
      GLfloat z,
      GLfloat n[3])
{
  v[0][0] = x;
  v[0][1] = y;
  v[0][2] = z;
  v[0][3] = n[0];
  v[0][4] = n[1];
  v[0][5] = n[2];

  return v + 1;
}

static void
destroy_gear (struct gear *g)
{
  g_free (g->strips);
  g_free (g);
}

/**
 *  Create a gear wheel.
 *
 *  @param inner_radius radius of hole at center
 *  @param outer_radius radius at center of teeth
 *  @param width width of gear
 *  @param teeth number of teeth
 *  @param tooth_depth depth of tooth
 *
 *  @return pointer to the constructed struct gear
 */
static struct gear *
create_gear (GLfloat inner_radius,
             GLfloat outer_radius,
             GLfloat width,
             GLint teeth,
             GLfloat tooth_depth)
{
  GLfloat r0, r1, r2;
  GLfloat da;
  GearVertex *v;
  struct gear *gear;
  double s[5], c[5];
  GLfloat normal[3];
  int cur_strip = 0;
  int i;

  /* Allocate memory for the gear */
  gear = g_malloc (sizeof *gear);

  /* Calculate the radii used in the gear */
  r0 = inner_radius;
  r1 = outer_radius - tooth_depth / 2.0;
  r2 = outer_radius + tooth_depth / 2.0;

  da = 2.0 * M_PI / teeth / 4.0;

  /* Allocate memory for the triangle strip information */
  gear->nstrips = STRIPS_PER_TOOTH * teeth;
  gear->strips = g_malloc0_n (gear->nstrips, sizeof (*gear->strips));

  /* Allocate memory for the vertices */
  gear->vertices = g_malloc0_n (VERTICES_PER_TOOTH * teeth, sizeof(*gear->vertices));
  v = gear->vertices;

  for (i = 0; i < teeth; i++) {
    /* A set of macros for making the creation of the gears easier */
#define  GEAR_POINT(p, r, da) do { p.x = (r) * c[(da)]; p.y = (r) * s[(da)]; } while(0)
#define  SET_NORMAL(x, y, z) do { \
   normal[0] = (x); normal[1] = (y); normal[2] = (z); \
} while(0)

#define  GEAR_VERT(v, point, sign) vert((v), p[(point)].x, p[(point)].y, (sign) * width * 0.5, normal)

#define START_STRIP do { \
   gear->strips[cur_strip].first = v - gear->vertices; \
} while(0);

#define END_STRIP do { \
   int _tmp = (v - gear->vertices); \
   gear->strips[cur_strip].count = _tmp - gear->strips[cur_strip].first; \
   cur_strip++; \
} while (0)

#define QUAD_WITH_NORMAL(p1, p2) do { \
   SET_NORMAL((p[(p1)].y - p[(p2)].y), -(p[(p1)].x - p[(p2)].x), 0); \
   v = GEAR_VERT(v, (p1), -1); \
   v = GEAR_VERT(v, (p1), 1); \
   v = GEAR_VERT(v, (p2), -1); \
   v = GEAR_VERT(v, (p2), 1); \
} while(0)
    struct point {
      GLfloat x;
      GLfloat y;
    };

    /* Create the 7 points (only x,y coords) used to draw a tooth */
    struct point p[7];

    /* Calculate needed sin/cos for various angles */
    _sincos(i * 2.0 * G_PI / teeth + da * 0, &s[0], &c[0]);
    _sincos(i * 2.0 * M_PI / teeth + da * 1, &s[1], &c[1]);
    _sincos(i * 2.0 * M_PI / teeth + da * 2, &s[2], &c[2]);
    _sincos(i * 2.0 * M_PI / teeth + da * 3, &s[3], &c[3]);
    _sincos(i * 2.0 * M_PI / teeth + da * 4, &s[4], &c[4]);

    GEAR_POINT(p[0], r2, 1);
    GEAR_POINT(p[1], r2, 2);
    GEAR_POINT(p[2], r1, 0);
    GEAR_POINT(p[3], r1, 3);
    GEAR_POINT(p[4], r0, 0);
    GEAR_POINT(p[5], r1, 4);
    GEAR_POINT(p[6], r0, 4);

    /* Front face */
    START_STRIP;
    SET_NORMAL(0, 0, 1.0);
    v = GEAR_VERT(v, 0, +1);
    v = GEAR_VERT(v, 1, +1);
    v = GEAR_VERT(v, 2, +1);
    v = GEAR_VERT(v, 3, +1);
    v = GEAR_VERT(v, 4, +1);
    v = GEAR_VERT(v, 5, +1);
    v = GEAR_VERT(v, 6, +1);
    END_STRIP;

    /* Inner face */
    START_STRIP;
    QUAD_WITH_NORMAL(4, 6);
    END_STRIP;

    /* Back face */
    START_STRIP;
    SET_NORMAL(0, 0, -1.0);
    v = GEAR_VERT(v, 6, -1);
    v = GEAR_VERT(v, 5, -1);
    v = GEAR_VERT(v, 4, -1);
    v = GEAR_VERT(v, 3, -1);
    v = GEAR_VERT(v, 2, -1);
    v = GEAR_VERT(v, 1, -1);
    v = GEAR_VERT(v, 0, -1);
    END_STRIP;

    /* Outer face */
    START_STRIP;
    QUAD_WITH_NORMAL(0, 2);
    END_STRIP;

    START_STRIP;
    QUAD_WITH_NORMAL(1, 0);
    END_STRIP;

    START_STRIP;
    QUAD_WITH_NORMAL(3, 1);
    END_STRIP;

    START_STRIP;
    QUAD_WITH_NORMAL(5, 3);
    END_STRIP;
  }

  gear->nvertices = (v - gear->vertices);

  return gear;
}

/** 
 * Multiplies two 4x4 matrices.
 *
 * The result is stored in matrix m.
 *
 * @param m the first matrix to multiply
 * @param n the second matrix to multiply
 */
static void
multiply (GLfloat *m, const GLfloat *n)
{
   GLfloat tmp[16];
   const GLfloat *row, *column;
   div_t d;
   int i, j;

   for (i = 0; i < 16; i++) {
      tmp[i] = 0;
      d = div(i, 4);
      row = n + d.quot * 4;
      column = m + d.rem;
      for (j = 0; j < 4; j++)
         tmp[i] += row[j] * column[j * 4];
   }
   memcpy(m, &tmp, sizeof tmp);
}

/** 
 * Rotates a 4x4 matrix.
 *
 * @param[in,out] m the matrix to rotate
 * @param angle the angle to rotate
 * @param x the x component of the direction to rotate to
 * @param y the y component of the direction to rotate to
 * @param z the z component of the direction to rotate to
 */
static void
rotate(GLfloat *m, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
   double s = sin (angle);
   double c = cos (angle);

   GLfloat r[16] = {
      x * x * (1 - c) + c,     y * x * (1 - c) + z * s, x * z * (1 - c) - y * s, 0,
      x * y * (1 - c) - z * s, y * y * (1 - c) + c,     y * z * (1 - c) + x * s, 0,
      x * z * (1 - c) + y * s, y * z * (1 - c) - x * s, z * z * (1 - c) + c,     0,
      0, 0, 0, 1
   };

   multiply(m, r);
}

/** 
 * Translates a 4x4 matrix.
 * 
 * @param[in,out] m the matrix to translate
 * @param x the x component of the direction to translate to
 * @param y the y component of the direction to translate to
 * @param z the z component of the direction to translate to
 */
static void
translate(GLfloat *m, GLfloat x, GLfloat y, GLfloat z)
{
   GLfloat t[16] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  x, y, z, 1 };

   multiply(m, t);
}

/** 
 * Creates an identity 4x4 matrix.
 * 
 * @param m the matrix make an identity matrix
 */
static void
identity(GLfloat *m)
{
   GLfloat t[16] = {
      1.0, 0.0, 0.0, 0.0,
      0.0, 1.0, 0.0, 0.0,
      0.0, 0.0, 1.0, 0.0,
      0.0, 0.0, 0.0, 1.0,
   };

   memcpy(m, t, sizeof(t));
}

/** 
 * Transposes a 4x4 matrix.
 *
 * @param m the matrix to transpose
 */
static void
transpose(GLfloat *m)
{
   GLfloat t[16] = {
      m[0], m[4], m[8],  m[12],
      m[1], m[5], m[9],  m[13],
      m[2], m[6], m[10], m[14],
      m[3], m[7], m[11], m[15]};

   memcpy(m, t, sizeof(t));
}

/**
 * Inverts a 4x4 matrix.
 *
 * This function can currently handle only pure translation-rotation matrices.
 * Read http://www.gamedev.net/community/forums/topic.asp?topic_id=425118
 * for an explanation.
 */
static void
invert(GLfloat *m)
{
   GLfloat t[16];
   identity(t);

   // Extract and invert the translation part 't'. The inverse of a
   // translation matrix can be calculated by negating the translation
   // coordinates.
   t[12] = -m[12]; t[13] = -m[13]; t[14] = -m[14];

   // Invert the rotation part 'r'. The inverse of a rotation matrix is
   // equal to its transpose.
   m[12] = m[13] = m[14] = 0;
   transpose(m);

   // inv(m) = inv(r) * inv(t)
   multiply(m, t);
}

/** 
 * Calculate a perspective projection transformation.
 * 
 * @param m the matrix to save the transformation in
 * @param fovy the field of view in the y direction
 * @param aspect the view aspect ratio
 * @param zNear the near clipping plane
 * @param zFar the far clipping plane
 */
static void perspective(GLfloat *m, GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
   GLfloat tmp[16];
   double sine, cosine, cotangent, deltaZ;
   GLfloat radians = fovy / 2 * M_PI / 180;
   identity(tmp);

   deltaZ = zFar - zNear;
   _sincos(radians, &sine, &cosine);

   if ((deltaZ == 0) || (sine == 0) || (aspect == 0))
      return;

   cotangent = cosine / sine;

   tmp[0] = cotangent / aspect;
   tmp[5] = cotangent;
   tmp[10] = -(zFar + zNear) / deltaZ;
   tmp[11] = -1;
   tmp[14] = -2 * zNear * zFar / deltaZ;
   tmp[15] = 0;

   memcpy(m, tmp, sizeof(tmp));
}

/**
 * Draws a gear.
 *
 * @param gear the gear to draw
 * @param transform the current transformation matrix
 * @param x the x position to draw the gear at
 * @param y the y position to draw the gear at
 * @param angle the rotation angle of the gear
 * @param color the color of the gear
 */
static void
draw_gear(GtkGears *self,
          struct gear *gear,
          GLuint gear_vbo,
          GLfloat *transform,
          GLfloat x,
          GLfloat y,
          GLfloat angle,
          const GLfloat color[4])
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (self);
  GLfloat model_view[16];
  GLfloat normal_matrix[16];
  GLfloat model_view_projection[16];
  int n;

  /* Translate and rotate the gear */
  memcpy(model_view, transform, sizeof (model_view));
  translate(model_view, x, y, 0);
  rotate(model_view, 2 * G_PI * angle / 360.0, 0, 0, 1);

  /* Create and set the ModelViewProjectionMatrix */
  memcpy(model_view_projection, priv->ProjectionMatrix, sizeof(model_view_projection));
  multiply(model_view_projection, model_view);

  glUniformMatrix4fv(priv->ModelViewProjectionMatrix_location, 1, GL_FALSE,
                     model_view_projection);

  /* 
   * Create and set the NormalMatrix. It's the inverse transpose of the
   * ModelView matrix.
   */
  memcpy(normal_matrix, model_view, sizeof (normal_matrix));
  invert(normal_matrix);
  transpose(normal_matrix);
  glUniformMatrix4fv(priv->NormalMatrix_location, 1, GL_FALSE, normal_matrix);

  /* Set the gear color */
  glUniform4fv(priv->MaterialColor_location, 1, color);

  /* Set the vertex buffer object to use */
  glBindBuffer(GL_ARRAY_BUFFER, gear_vbo);

  /* Set up the position of the attributes in the vertex buffer object */
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), NULL);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLfloat *) 0 + 3);

  /* Enable the attributes */
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  /* Draw the triangle strips that comprise the gear */
  for (n = 0; n < gear->nstrips; n++) {
    glDrawArrays(GL_TRIANGLE_STRIP, gear->strips[n].first, gear->strips[n].count);
  }

  /* Disable the attributes */
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(0);
}

/* new window size or exposure */
static void
gtk_gears_reshape (GtkGLArea *area, int width, int height)
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private ((GtkGears *) area);

  /* Update the projection matrix */
  perspective (priv->ProjectionMatrix, 60.0, width / (float)height, 1.0, 1024.0);

  /* Set the viewport */
  glViewport (0, 0, (GLint) width, (GLint) height);
}

static gboolean
gtk_gears_render (GtkGLArea    *area,
                  GdkGLContext *context)
{
  static const GLfloat red[4]   = { 0.8, 0.1, 0.0, 1.0 };
  static const GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
  static const GLfloat blue[4]  = { 0.2, 0.2, 1.0, 1.0 };

  GtkGears *self = GTK_GEARS (area);
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (self);
  GLfloat transform[16];

  identity (transform);

  glClearColor (0.0, 0.0, 0.0, 0.0);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /* Translate and rotate the view */
  translate (transform, 0, 0, -20);
  rotate (transform, 2 * G_PI * priv->view_rot[0] / 360.0, 1, 0, 0);
  rotate (transform, 2 * G_PI * priv->view_rot[1] / 360.0, 0, 1, 0);
  rotate (transform, 2 * G_PI * priv->view_rot[2] / 360.0, 0, 0, 1);

  /* Draw the gears */
  draw_gear (self, priv->gear1, priv->gear_vbo[0], transform, -3.0, -2.0,      priv->angle,        red);
  draw_gear (self, priv->gear2, priv->gear_vbo[1], transform,  3.1, -2.0, -2 * priv->angle - 9.0,  green);
  draw_gear (self, priv->gear3, priv->gear_vbo[2], transform, -3.1,  4.2, -2 * priv->angle - 25.0, blue);

  return TRUE;
}

static const char vertex_shader_gl[] =
"#version 150\n"
"\n"
"in vec3 position;\n"
"in vec3 normal;\n"
"\n"
"uniform mat4 ModelViewProjectionMatrix;\n"
"uniform mat4 NormalMatrix;\n"
"uniform vec4 LightSourcePosition;\n"
"uniform vec4 MaterialColor;\n"
"\n"
"smooth out vec4 Color;\n"
"\n"
"void main(void)\n"
"{\n"
"    // Transform the normal to eye coordinates\n"
"    vec3 N = normalize(vec3(NormalMatrix * vec4(normal, 1.0)));\n"
"\n"
"    // The LightSourcePosition is actually its direction for directional light\n"
"    vec3 L = normalize(LightSourcePosition.xyz);\n"
"\n"
"    // Multiply the diffuse value by the vertex color (which is fixed in this case)\n"
"    // to get the actual color that we will use to draw this vertex with\n"
"    float diffuse = max(dot(N, L), 0.0);\n"
"    Color = diffuse * MaterialColor;\n"
"\n"
"    // Transform the position to clip coordinates\n"
"    gl_Position = ModelViewProjectionMatrix * vec4(position, 1.0);\n"
"}";

static const char fragment_shader_gl[] =
"#version 150\n"
"\n"
"smooth in vec4 Color;\n"
"\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = Color;\n"
"}";

static const char vertex_shader_gles[] =
"attribute vec3 position;\n"
"attribute vec3 normal;\n"
"\n"
"uniform mat4 ModelViewProjectionMatrix;\n"
"uniform mat4 NormalMatrix;\n"
"uniform vec4 LightSourcePosition;\n"
"uniform vec4 MaterialColor;\n"
"\n"
"varying vec4 Color;\n"
"\n"
"void main(void)\n"
"{\n"
"    // Transform the normal to eye coordinates\n"
"    vec3 N = normalize(vec3(NormalMatrix * vec4(normal, 1.0)));\n"
"\n"
"    // The LightSourcePosition is actually its direction for directional light\n"
"    vec3 L = normalize(LightSourcePosition.xyz);\n"
"\n"
"    // Multiply the diffuse value by the vertex color (which is fixed in this case)\n"
"    // to get the actual color that we will use to draw this vertex with\n"
"    float diffuse = max(dot(N, L), 0.0);\n"
"    Color = diffuse * MaterialColor;\n"
"\n"
"    // Transform the position to clip coordinates\n"
"    gl_Position = ModelViewProjectionMatrix * vec4(position, 1.0);\n"
"}";

static const char fragment_shader_gles[] =
"precision mediump float;\n"
"varying vec4 Color;\n"
"\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = Color;\n"
"}";

static void
gtk_gears_realize (GtkWidget *widget)
{
  GtkGLArea *glarea = GTK_GL_AREA (widget);
  GtkGears *gears = GTK_GEARS (widget);
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);
  GdkGLContext *context;
  GLuint vao, v, f, program;
  const char *p;
  char msg[512];

  GTK_WIDGET_CLASS (gtk_gears_parent_class)->realize (widget);

  gtk_gl_area_make_current (glarea);
  if (gtk_gl_area_get_error (glarea) != NULL)
    return;

  context = gtk_gl_area_get_context (glarea);

  glEnable (GL_CULL_FACE);
  glEnable (GL_DEPTH_TEST);

  /* Create the VAO */
  glGenVertexArrays (1, &vao);
  glBindVertexArray (vao);
  priv->vao = vao;

  /* Compile the vertex shader */
  if (gdk_gl_context_get_use_es (context))
    p = vertex_shader_gles;
  else
    p = vertex_shader_gl;
  v = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(v, 1, &p, NULL);
  glCompileShader(v);
  glGetShaderInfoLog(v, sizeof msg, NULL, msg);
  g_print ("vertex shader info: %s\n", msg);

  /* Compile the fragment shader */
  if (gdk_gl_context_get_use_es (context))
    p = fragment_shader_gles;
  else
    p = fragment_shader_gl;
  f = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(f, 1, &p, NULL);
  glCompileShader(f);
  glGetShaderInfoLog(f, sizeof msg, NULL, msg);
  g_print ("fragment shader info: %s\n", msg);

  /* Create and link the shader program */
  program = glCreateProgram();
  glAttachShader(program, v);
  glAttachShader(program, f);
  glBindAttribLocation(program, 0, "position");
  glBindAttribLocation(program, 1, "normal");

  glLinkProgram(program);
  glGetProgramInfoLog(program, sizeof msg, NULL, msg);
  g_print ("program info: %s\n", msg);
  glDetachShader (program, v);
  glDetachShader (program, f);
  glDeleteShader (v);
  glDeleteShader (f);

  /* Enable the shaders */
  glUseProgram(program);
  priv->program = program;

  /* Get the locations of the uniforms so we can access them */
  priv->ModelViewProjectionMatrix_location = glGetUniformLocation(program, "ModelViewProjectionMatrix");
  priv->NormalMatrix_location = glGetUniformLocation(program, "NormalMatrix");
  priv->LightSourcePosition_location = glGetUniformLocation(program, "LightSourcePosition");
  priv->MaterialColor_location = glGetUniformLocation(program, "MaterialColor");

  /* Set the LightSourcePosition uniform which is constant throughout the program */
  glUniform4fv(priv->LightSourcePosition_location, 1, priv->LightSourcePosition);

  /* make the gears */
  priv->gear1 = create_gear(1.0, 4.0, 1.0, 20, 0.7);

  /* Store the vertices in a vertex buffer object (VBO) */
  glGenBuffers (1, &(priv->gear_vbo[0]));
  glBindBuffer (GL_ARRAY_BUFFER, priv->gear_vbo[0]);
  glBufferData (GL_ARRAY_BUFFER,
                priv->gear1->nvertices * sizeof(GearVertex),
                priv->gear1->vertices,
                GL_STATIC_DRAW);

  priv->gear2 = create_gear(0.5, 2.0, 2.0, 10, 0.7);
  glGenBuffers (1, &(priv->gear_vbo[1]));
  glBindBuffer (GL_ARRAY_BUFFER, priv->gear_vbo[1]);
  glBufferData (GL_ARRAY_BUFFER,
                priv->gear2->nvertices * sizeof(GearVertex),
                priv->gear2->vertices,
                GL_STATIC_DRAW);

  priv->gear3 = create_gear(1.3, 2.0, 0.5, 10, 0.7);
  glGenBuffers (1, &(priv->gear_vbo[2]));
  glBindBuffer (GL_ARRAY_BUFFER, priv->gear_vbo[2]);
  glBufferData (GL_ARRAY_BUFFER,
                priv->gear3->nvertices * sizeof(GearVertex),
                priv->gear3->vertices,
                GL_STATIC_DRAW);
}

static void
gtk_gears_unrealize (GtkWidget *widget)
{
  GtkGLArea *glarea = GTK_GL_AREA (widget);
  GtkGearsPrivate *priv = gtk_gears_get_instance_private ((GtkGears *) widget);

  gtk_gl_area_make_current (glarea);
  if (gtk_gl_area_get_error (glarea) != NULL)
    return;

  /* Release the resources associated with OpenGL */
  if (priv->gear_vbo[0] != 0)
    glDeleteBuffers (1, &(priv->gear_vbo[0]));

  if (priv->gear_vbo[1] != 0)
    glDeleteBuffers (1, &(priv->gear_vbo[1]));

  if (priv->gear_vbo[2] != 0)
    glDeleteBuffers (1, &(priv->gear_vbo[2]));

  if (priv->vao != 0)
    glDeleteVertexArrays (1, &priv->vao);

  if (priv->program != 0)
    glDeleteProgram (priv->program);

  priv->ModelViewProjectionMatrix_location = 0;
  priv->NormalMatrix_location = 0;
  priv->LightSourcePosition_location = 0;
  priv->MaterialColor_location = 0;

  GTK_WIDGET_CLASS (gtk_gears_parent_class)->unrealize (widget);
}

static gboolean
gtk_gears_tick (GtkWidget     *widget,
                GdkFrameClock *frame_clock,
                gpointer       user_data)
{
  GtkGears *gears = GTK_GEARS (widget);
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);
  GdkFrameTimings *timings, *previous_timings;
  gint64 previous_frame_time = 0;
  gint64 frame_time;
  gint64 history_start, history_len;
  gint64 frame;
  char *s;

  frame = gdk_frame_clock_get_frame_counter (frame_clock);
  frame_time = gdk_frame_clock_get_frame_time (frame_clock);

  if (priv->first_frame_time == 0)
    {
      /* No need for changes on first frame */
      priv->first_frame_time = frame_time;
      if (priv->fps_label)
        gtk_label_set_label (priv->fps_label, "FPS: ---");
      return G_SOURCE_CONTINUE;
    }

  /* glxgears advances 70 degrees per second, so do the same */

  priv->angle = fmod ((frame_time - priv->first_frame_time) / (double)G_USEC_PER_SEC * 70.0, 360.0);

  gtk_widget_queue_draw (widget);

  history_start = gdk_frame_clock_get_history_start (frame_clock);

  if (priv->fps_label && frame % 60 == 0)
    {
      history_len = frame - history_start;
      if (history_len > 0)
        {
          previous_timings = gdk_frame_clock_get_timings (frame_clock, frame - history_len);
          previous_frame_time = gdk_frame_timings_get_frame_time (previous_timings);

          s = g_strdup_printf ("FPS: %-4.1f", (G_USEC_PER_SEC * history_len) / (double)(frame_time - previous_frame_time));
          gtk_label_set_label (priv->fps_label, s);
          g_free (s);
        }
    }

  timings = gdk_frame_clock_get_current_timings (frame_clock);
  previous_timings = gdk_frame_clock_get_timings (frame_clock,
                                                  gdk_frame_timings_get_frame_counter (timings) - 1);
  if (previous_timings != NULL)
    previous_frame_time = gdk_frame_timings_get_frame_time (previous_timings);

  return G_SOURCE_CONTINUE;
}

void
gtk_gears_set_axis (GtkGears *gears, int axis, double value)
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);

  if (axis < 0 || axis >= GTK_GEARS_N_AXIS)
    return;

  priv->view_rot[axis] = value;

  gtk_widget_queue_draw (GTK_WIDGET (gears));
}

double
gtk_gears_get_axis (GtkGears *gears, int axis)
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);

  if (axis < 0 || axis >= GTK_GEARS_N_AXIS)
    return 0.0;

  return priv->view_rot[axis];
}

void
gtk_gears_set_fps_label (GtkGears *gears, GtkLabel *label)
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);

  if (label)
    g_object_ref (label);

  g_clear_object (&priv->fps_label);

  priv->fps_label = label;
}
