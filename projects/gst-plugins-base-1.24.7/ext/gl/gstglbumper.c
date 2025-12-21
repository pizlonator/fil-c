/*
 * GStreamer
 * Copyright (C) 2008 Cyril Comparon <cyril.comparon@gmail.com>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-glbumper
 * @title: glbumper
 *
 * Bump mapping using the normal method.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 -v videotestsrc ! glupload ! glbumper location=normalmap.bmp ! glimagesink
 * ]| A pipeline to test normal mapping.
 * FBO (Frame Buffer Object) and GLSL (OpenGL Shading Language) are required.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <png.h>

#include "gstglelements.h"
#include "gstglbumper.h"

#if PNG_LIBPNG_VER >= 10400
#define int_p_NULL         NULL
#define png_infopp_NULL    NULL
#endif

#define GST_CAT_DEFAULT gst_gl_bumper_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_LOCATION
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_bumper_debug, "glbumper", 0, "glbumper element");

G_DEFINE_TYPE_WITH_CODE (GstGLBumper, gst_gl_bumper, GST_TYPE_GL_FILTER,
    DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glbumper, "glbumper",
    GST_RANK_NONE, GST_TYPE_GL_BUMPER, gl_element_init (plugin));

static void gst_gl_bumper_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_bumper_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_gl_bumper_reset (GstGLFilter * filter);
static gboolean gst_gl_bumper_init_shader (GstGLFilter * filter);
static gboolean gst_gl_bumper_filter_texture (GstGLFilter * filter,
    guint in_tex, guint out_tex);
static void gst_gl_bumper_callback (gint width, gint height, guint texture,
    gpointer stuff);

//vertex source
static const gchar *bumper_v_src =
    "attribute vec3 aTangent;\n"
    "\n"
    "varying vec3 vNormal;\n"
    "varying vec3 vTangent;\n"
    "varying vec3 vVertexToLight0;\n"
    "varying vec3 vVertexToLight1;\n"
    "\n"
    "void main()\n"
    "{\n"
    "  // transform the vertex\n"
    "  gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * gl_Vertex;\n"
    "\n"
    "  // transform the normal and the tangent to scene coords\n"
    "  vNormal = normalize(gl_NormalMatrix * gl_Normal);\n"
    "  vTangent = normalize(gl_NormalMatrix * aTangent);\n"
    "\n"
    "  // transforming the vertex position to modelview-space\n"
    "  //const vec4 vertexInSceneCoords = gl_ModelViewMatrix * gl_Vertex;\n"
    "\n"
    "  // calculate the vector from the vertex position to the light position\n"
    "  vVertexToLight0 = normalize(gl_LightSource[0].position).xyz;\n"
    "  vVertexToLight1 = normalize(gl_LightSource[1].position).xyz;\n"
    "\n"
    "  // transit vertex color\n"
    "  gl_FrontColor = gl_BackColor = gl_Color;\n"
    "\n"
    "  // use the two first sets of texture coordinates in the fragment shader\n"
    "  gl_TexCoord[0] = gl_MultiTexCoord0;\n"
    "  gl_TexCoord[1] = gl_MultiTexCoord1;\n" "}\n";

//fragment source
static const gchar *bumper_f_src =
    "uniform sampler2D texture0;\n"
    "uniform sampler2D texture1;\n"
    "\n"
    "varying vec3 vNormal;\n"
    "varying vec3 vTangent;\n"
    "varying vec3 vVertexToLight0;\n"
    "varying vec3 vVertexToLight1;\n"
    "\n"
    "void main()\n"
    "{\n"
    "  // get the color of the textures\n"
    "  vec4 textureColor = texture2D(texture0, gl_TexCoord[0].st);\n"
    "  vec3 normalmapItem = texture2D(texture1, gl_TexCoord[1].st).xyz * 2.0 - 1.0;\n"
    "\n"
    "  // calculate matrix that transform from tangent space to normalmap space (contrary of intuition)\n"
    "  vec3 binormal = cross(vNormal, vTangent);\n"
    "  mat3 tangentSpace2normalmapSpaceMat = mat3(vTangent, binormal, vNormal);\n"
    "\n"
    "  // disturb the normal\n"
    "  vec3 disturbedNormal = tangentSpace2normalmapSpaceMat * normalmapItem;\n"
    "\n"
    "  // calculate the diffuse term and clamping it to [0;1]\n"
    "  float diffuseTerm0 = clamp(dot(disturbedNormal, vVertexToLight0), 0.0, 1.0);\n"
    "  float diffuseTerm1 = clamp(dot(disturbedNormal, vVertexToLight1), 0.0, 1.0);\n"
    "\n"
    "  vec3 irradiance = (diffuseTerm0 * gl_LightSource[0].diffuse.rgb + diffuseTerm1 * gl_LightSource[1].diffuse.rgb);\n"
    "\n"
    "  // calculate the final color\n"
    "  gl_FragColor = vec4(irradiance * textureColor.rgb, textureColor.w);\n"
    "}\n";

#define LOAD_ERROR(context, msg) { gst_gl_context_set_error (context, "unable to load %s: %s", bumper->location, msg); return; }

//png reading error handler
static void
user_warning_fn (png_structp png_ptr, png_const_charp warning_msg)
{
  g_warning ("%s\n", warning_msg);
}

//Called in the gl thread
static void
gst_gl_bumper_init_resources (GstGLFilter * filter)
{
  GstGLBumper *bumper = GST_GL_BUMPER (filter);
  GstGLContext *context = filter->context;
  const GstGLFuncs *gl = context->gl_vtable;

  png_structp png_ptr;
  png_infop info_ptr;
  png_uint_32 width = 0;
  png_uint_32 height = 0;
  gint bit_depth = 0;
  gint color_type = 0;
  gint interlace_type = 0;
  png_FILE_p fp = NULL;
  guint y = 0;
  guchar *raw_data = NULL;
  guchar **rows = NULL;
  png_byte magic[8];
  gint n_read;

  if (!bumper->location) {
    gst_gl_context_set_error (context, "A filename is required");
    return;
  }

  /* BEGIN load png image file */

  if ((fp = fopen (bumper->location, "rb")) == NULL)
    LOAD_ERROR (context, "file not found");

  /* Read magic number */
  n_read = fread (magic, 1, sizeof (magic), fp);
  if (n_read != sizeof (magic)) {
    fclose (fp);
    LOAD_ERROR (context, "can't read PNG magic number");
  }

  /* Check for valid magic number */
  if (png_sig_cmp (magic, 0, sizeof (magic))) {
    fclose (fp);
    LOAD_ERROR (context, "not a valid PNG image");
  }

  png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (png_ptr == NULL) {
    fclose (fp);
    LOAD_ERROR (context, "failed to initialize the png_struct");
  }

  png_set_error_fn (png_ptr, NULL, NULL, user_warning_fn);

  info_ptr = png_create_info_struct (png_ptr);
  if (info_ptr == NULL) {
    fclose (fp);
    png_destroy_read_struct (&png_ptr, png_infopp_NULL, png_infopp_NULL);
    LOAD_ERROR (context,
        "failed to initialize the memory for image information");
  }

  png_init_io (png_ptr, fp);

  png_set_sig_bytes (png_ptr, sizeof (magic));

  png_read_info (png_ptr, info_ptr);

  png_get_IHDR (png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
      &interlace_type, int_p_NULL, int_p_NULL);

  if (color_type != PNG_COLOR_TYPE_RGB) {
    fclose (fp);
    png_destroy_read_struct (&png_ptr, png_infopp_NULL, png_infopp_NULL);
    LOAD_ERROR (context, "color type is not rgb");
  }

  raw_data = (guchar *) malloc (sizeof (guchar) * width * height * 3);

  rows = (guchar **) malloc (sizeof (guchar *) * height);

  for (y = 0; y < height; ++y)
    rows[y] = (guchar *) (raw_data + y * width * 3);

  png_read_image (png_ptr, rows);

  free (rows);

  png_read_end (png_ptr, info_ptr);
  png_destroy_read_struct (&png_ptr, &info_ptr, png_infopp_NULL);
  fclose (fp);

  /* END load png image file */

  bumper->bumpmap_width = width;
  bumper->bumpmap_height = height;

  gl->GenTextures (1, &bumper->bumpmap);
  gl->BindTexture (GL_TEXTURE_2D, bumper->bumpmap);
  gl->TexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
      bumper->bumpmap_width, bumper->bumpmap_height, 0,
      GL_RGB, GL_UNSIGNED_BYTE, raw_data);
  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  free (raw_data);
}

//Called in the gl thread
static void
gst_gl_bumper_reset_resources (GstGLFilter * filter)
{
  GstGLBumper *bumper = GST_GL_BUMPER (filter);

  if (bumper->bumpmap) {
    glDeleteTextures (1, &bumper->bumpmap);
    bumper->bumpmap = 0;
  }
}

static void
gst_gl_bumper_class_init (GstGLBumperClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);
  gobject_class->set_property = gst_gl_bumper_set_property;
  gobject_class->get_property = gst_gl_bumper_get_property;

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  GST_GL_FILTER_CLASS (klass)->filter_texture = gst_gl_bumper_filter_texture;
  GST_GL_FILTER_CLASS (klass)->display_init_cb = gst_gl_bumper_init_resources;
  GST_GL_FILTER_CLASS (klass)->display_reset_cb = gst_gl_bumper_reset_resources;
  GST_GL_FILTER_CLASS (klass)->init_fbo = gst_gl_bumper_init_shader;
  GST_GL_FILTER_CLASS (klass)->onReset = gst_gl_bumper_reset;

  g_object_class_install_property (gobject_class,
      PROP_LOCATION, g_param_spec_string ("location",
          "Normal map location",
          "Normal map location", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (element_class, "OpenGL bumper filter",
      "Filter/Effect/Video", "Bump mapping filter",
      "Cyril Comparon <cyril.comparon@gmail.com>, "
      "Julien Isorce <julien.isorce@gmail.com>");

  GST_GL_BASE_FILTER_CLASS (klass)->supported_gl_api = GST_GL_API_OPENGL;
}

static void
gst_gl_bumper_init (GstGLBumper * bumper)
{
  bumper->shader = NULL;
  bumper->bumpmap = 0;
  bumper->bumpmap_width = 0;
  bumper->bumpmap_height = 0;
  bumper->location = NULL;
}

static void
gst_gl_bumper_reset (GstGLFilter * filter)
{
  GstGLBumper *bumper_filter = GST_GL_BUMPER (filter);

  //blocking call, wait the opengl thread has destroyed the shader
  if (bumper_filter->shader)
    gst_gl_context_del_shader (filter->context, bumper_filter->shader);
  bumper_filter->shader = NULL;
  bumper_filter->xrot = 0.0;
  bumper_filter->yrot = 0.0;
  bumper_filter->zrot = 0.0;
}

static void
gst_gl_bumper_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLBumper *bumper = GST_GL_BUMPER (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_free (bumper->location);
      bumper->location = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_bumper_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLBumper *bumper = GST_GL_BUMPER (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, bumper->location);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_bumper_init_shader (GstGLFilter * filter)
{
  GstGLBumper *bumper = GST_GL_BUMPER (filter);

  //blocking call, wait the opengl thread has compiled the shader
  return gst_gl_context_gen_shader (filter->context, bumper_v_src, bumper_f_src,
      &bumper->shader);
}

static gboolean
gst_gl_bumper_filter_texture (GstGLFilter * filter, guint in_tex, guint out_tex)
{
  gpointer bumper_filter = GST_GL_BUMPER (filter);

  //blocking call, use a FBO
  gst_gl_context_use_fbo (filter->context,
      GST_VIDEO_INFO_WIDTH (&filter->out_info),
      GST_VIDEO_INFO_HEIGHT (&filter->out_info),
      filter->fbo, filter->depthbuffer, out_tex, gst_gl_bumper_callback,
      GST_VIDEO_INFO_WIDTH (&filter->in_info),
      GST_VIDEO_INFO_HEIGHT (&filter->in_info),
      in_tex, 45,
      (gdouble) GST_VIDEO_INFO_WIDTH (&filter->out_info) /
      (gdouble) GST_VIDEO_INFO_HEIGHT (&filter->out_info), 0.1, 50,
      GST_GL_DISPLAY_PROJECTION_PERSPECTIVE, bumper_filter);

  return TRUE;
}

typedef struct _MeshData
{
  float x, y, z;                /* Vertex */
  float nx, ny, nz;             /* Normal */
  float s0, t0;                 /* TexCoord0 */
  float s1, t1;                 /* TexCoord1 */
  float va0, vb0, vc0;          /* VertexAttrib */
} MeshData;

//opengl scene, params: input texture (not the output filter->texture)
static void
gst_gl_bumper_callback (gint width, gint height, guint texture, gpointer stuff)
{
  GstGLFuncs *gl;
  GstGLBumper *bumper = GST_GL_BUMPER (stuff);
  GstGLContext *context = GST_GL_FILTER (bumper)->context;
  GLint locTangent = 0;

  //choose the lights
  GLfloat light_direction0[] = { 1.0, 0.0, -1.0, 0.0 }; // light goes along -x
  GLfloat light_direction1[] = { -1.0, 0.0, -1.0, 0.0 };        // light goes along x
  GLfloat light_diffuse0[] = { 1.0, 1.0, 1.0, 1.0 };
  GLfloat light_diffuse1[] = { 1.0, 1.0, 1.0, 1.0 };
  GLfloat mat_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };

/* *INDENT-OFF* */
  MeshData mesh[] = {
   /* |     Vertex      |     Normal      |TexCoord0|TexCoord1|  VertexAttrib  | */
/*F*/ { 1.0,  1.0, -1.0,  0.0,  0.0, -1.0, 0.0, 0.0, 0.0, 0.0,  0.0,  1.0,  0.0},
/*r*/ { 1.0, -1.0, -1.0,  0.0,  0.0, -1.0, 1.0, 0.0, 1.0, 0.0,  0.0,  1.0,  0.0},
/*o*/ {-1.0, -1.0, -1.0,  0.0,  0.0, -1.0, 1.0, 1.0, 1.0, 1.0,  0.0,  1.0,  0.0},
      {-1.0,  1.0, -1.0,  0.0,  0.0, -1.0, 0.0, 1.0, 0.0, 1.0,  0.0,  1.0,  0.0},
/*R*/ {-1.0,  1.0, -1.0, -1.0,  0.0,  0.0, 0.0, 0.0, 0.0, 0.0,  0.0,  1.0,  0.0},
/*i*/ {-1.0, -1.0, -1.0, -1.0,  0.0,  0.0, 1.0, 0.0, 1.0, 0.0,  0.0,  1.0,  0.0},
/*g*/ {-1.0, -1.0,  1.0, -1.0,  0.0,  0.0, 1.0, 1.0, 1.0, 1.0,  0.0,  1.0,  0.0},
      {-1.0,  1.0,  1.0, -1.0,  0.0,  0.0, 0.0, 1.0, 0.0, 1.0,  0.0,  1.0,  0.0},
/*B*/ {-1.0,  1.0,  1.0,  0.0,  0.0,  1.0, 0.0, 0.0, 0.0, 0.0,  0.0,  1.0,  0.0},
/*a*/ {-1.0, -1.0,  1.0,  0.0,  0.0,  1.0, 1.0, 0.0, 1.0, 0.0,  0.0,  1.0,  0.0},
/*c*/ { 1.0, -1.0,  1.0,  0.0,  0.0,  1.0, 1.0, 1.0, 1.0, 1.0,  0.0,  1.0,  0.0},
      { 1.0,  1.0,  1.0,  0.0,  0.0,  1.0, 0.0, 1.0, 0.0, 1.0,  0.0,  1.0,  0.0},
/*L*/ { 1.0,  1.0,  1.0,  1.0,  0.0,  0.0, 0.0, 0.0, 0.0, 0.0,  0.0,  1.0,  0.0},
/*e*/ { 1.0, -1.0,  1.0,  1.0,  0.0,  0.0, 1.0, 0.0, 1.0, 0.0,  0.0,  1.0,  0.0},
/*f*/ { 1.0, -1.0, -1.0,  1.0,  0.0,  0.0, 1.0, 1.0, 1.0, 1.0,  0.0,  1.0,  0.0},
      { 1.0,  1.0, -1.0,  1.0,  0.0,  0.0, 0.0, 1.0, 0.0, 1.0,  0.0,  1.0,  0.0},
/*T*/ { 1.0,  1.0,  1.0,  0.0,  1.0,  0.0, 0.0, 0.0, 0.0, 0.0,  0.0,  0.0,  1.0},
/*o*/ { 1.0,  1.0, -1.0,  0.0,  1.0,  0.0, 1.0, 0.0, 1.0, 0.0,  0.0,  0.0,  1.0},
/*p*/ {-1.0,  1.0, -1.0,  0.0,  1.0,  0.0, 1.0, 1.0, 1.0, 1.0,  0.0,  0.0,  1.0},
      {-1.0,  1.0,  1.0,  0.0,  1.0,  0.0, 0.0, 1.0, 0.0, 1.0,  0.0,  0.0,  1.0},
/*B*/ { 1.0, -1.0, -1.0,  0.0, -1.0,  0.0, 0.0, 0.0, 0.0, 0.0,  0.0,  0.0, -1.0},
/*o*/ { 1.0, -1.0,  1.0,  0.0, -1.0,  0.0, 1.0, 0.0, 1.0, 0.0,  0.0,  0.0, -1.0},
/*t*/ {-1.0, -1.0,  1.0,  0.0, -1.0,  0.0, 1.0, 1.0, 1.0, 1.0,  0.0,  0.0, -1.0},
      {-1.0, -1.0, -1.0,  0.0, -1.0,  0.0, 0.0, 1.0, 0.0, 1.0,  0.0,  0.0, -1.0},
  };

  GLushort indices[] = {
    0, 1, 2,
    0, 2, 3,
    4, 5, 6,
    4, 6, 7,
    8, 9, 10,
    8, 10, 11,
    12, 13, 14,
    12, 14, 15,
    16, 17, 18,
    16, 18, 19,
    20, 21, 22,
    20, 22, 23
  };

/* *INDENT-ON* */

  gl = GST_GL_FILTER (bumper)->context->gl_vtable;

  //eye point
  gl->MatrixMode (GL_PROJECTION);
  gluLookAt (0.0, 0.0, -6.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
  gl->MatrixMode (GL_MODELVIEW);

  //scene conf
  gl->Enable (GL_DEPTH_TEST);
  gl->DepthFunc (GL_LEQUAL);
  gl->Hint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  gl->ShadeModel (GL_SMOOTH);

  //set the lights
  gl->Lightfv (GL_LIGHT0, GL_POSITION, light_direction0);
  gl->Lightfv (GL_LIGHT0, GL_DIFFUSE, light_diffuse0);
  gl->Lightfv (GL_LIGHT1, GL_POSITION, light_direction1);
  gl->Lightfv (GL_LIGHT1, GL_DIFFUSE, light_diffuse1);
  gl->Materialfv (GL_FRONT, GL_DIFFUSE, mat_diffuse);
  gl->ColorMaterial (GL_FRONT_AND_BACK, GL_DIFFUSE);
  gl->Enable (GL_COLOR_MATERIAL);
  gl->Enable (GL_LIGHTING);
  gl->Enable (GL_LIGHT0);
  gl->Enable (GL_LIGHT1);
  //configure shader
  gst_gl_shader_use (bumper->shader);
  locTangent =
      gst_gl_shader_get_attribute_location (bumper->shader, "aTangent");

  //set the normal map
  gl->ActiveTexture (GL_TEXTURE1);
  gst_gl_shader_set_uniform_1i (bumper->shader, "texture1", 1);
  gl->BindTexture (GL_TEXTURE_2D, bumper->bumpmap);

  //set the video texture
  gl->ActiveTexture (GL_TEXTURE0);
  gst_gl_shader_set_uniform_1i (bumper->shader, "texture0", 0);
  gl->BindTexture (GL_TEXTURE_2D, texture);

  gl->Rotatef (bumper->xrot, 1.0f, 0.0f, 0.0f);
  gl->Rotatef (bumper->yrot, 0.0f, 1.0f, 0.0f);
  gl->Rotatef (bumper->zrot, 0.0f, 0.0f, 1.0f);

  gl->EnableVertexAttribArray (locTangent);

  gl->ClientActiveTexture (GL_TEXTURE0);
  gl->EnableClientState (GL_TEXTURE_COORD_ARRAY);
  gl->EnableClientState (GL_VERTEX_ARRAY);
  gl->EnableClientState (GL_NORMAL_ARRAY);

  gl->VertexAttribPointer (locTangent, 3, GL_FLOAT, 0, sizeof (MeshData),
      &mesh[0].va0);
  gl->VertexPointer (3, GL_FLOAT, sizeof (MeshData), &mesh[0].x);
  gl->NormalPointer (GL_FLOAT, sizeof (MeshData), &mesh[0].nx);
  gl->TexCoordPointer (2, GL_FLOAT, sizeof (MeshData), &mesh[0].s0);

  gl->ClientActiveTexture (GL_TEXTURE1);
  gl->EnableClientState (GL_TEXTURE_COORD_ARRAY);
  gl->TexCoordPointer (2, GL_FLOAT, sizeof (MeshData), &mesh[0].s1);

  gl->DrawElements (GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, indices);

  gl->DisableClientState (GL_VERTEX_ARRAY);
  gl->DisableClientState (GL_TEXTURE_COORD_ARRAY);
  gl->DisableClientState (GL_NORMAL_ARRAY);

  gl->ClientActiveTexture (GL_TEXTURE0);
  gl->DisableClientState (GL_TEXTURE_COORD_ARRAY);

  gl->DisableVertexAttribArray (locTangent);

  gst_gl_context_clear_shader (context);

  gl->Disable (GL_LIGHT0);
  gl->Disable (GL_LIGHT1);
  gl->Disable (GL_LIGHTING);
  gl->Disable (GL_COLOR_MATERIAL);

  bumper->xrot += 1.0f;
  bumper->yrot += 0.9f;
  bumper->zrot += 0.6f;
}
