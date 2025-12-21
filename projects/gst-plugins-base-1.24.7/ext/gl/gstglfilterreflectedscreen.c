/*
 * GStreamer
 * Copyright (C) 2010 Pierre Pouzol<pierre.pouzol@hotmail.fr>
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
 * SECTION:element-glfilterreflectedscreen
 * @title: glfilterreflectedscreen
 *
 * Map Video Texture upon a screen, on a reflecting surface
 *
 * ## Examples
 * |[
 * gst-launch-1.0 videotestsrc ! glupload ! glfilterreflectedscreen ! glimagesink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "gstglelements.h"
#include "gstglfilterreflectedscreen.h"

#define GST_CAT_DEFAULT gst_gl_filter_reflected_screen_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_ACTIVE_GRAPHIC_MODE,
  PROP_SEPARATED_SCREEN,
  PROP_SHOW_FLOOR,
  PROP_FOVY,
  PROP_ASPECT,
  PROP_ZNEAR,
  PROP_ZFAR
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_filter_reflected_screen_debug, "glfilterreflectedscreen", 0, "glfilterreflectedscreen element");

G_DEFINE_TYPE_WITH_CODE (GstGLFilterReflectedScreen,
    gst_gl_filter_reflected_screen, GST_TYPE_GL_FILTER, DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glfilterreflectedscreen,
    "glfilterreflectedscreen", GST_RANK_NONE,
    GST_TYPE_GL_FILTER_REFLECTED_SCREEN, gl_element_init (plugin));

static void gst_gl_filter_reflected_screen_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gl_filter_reflected_screen_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_gl_filter_reflected_screen_filter_texture (GstGLFilter *
    filter, guint in_tex, guint out_tex);

static void gst_gl_filter_reflected_screen_draw_background ();
static void gst_gl_filter_reflected_screen_draw_floor ();
static void gst_gl_filter_reflected_screen_draw_screen (GstGLFilter * filter,
    gint width, gint height, guint texture);
static void gst_gl_filter_reflected_screen_draw_separated_screen (GstGLFilter *
    filter, gint width, gint height, guint texture, gfloat alphs, gfloat alphe);

static void gst_gl_filter_reflected_screen_callback (gint width, gint height,
    guint texture, gpointer stuff);

static const GLfloat LightPos[] = { 4.0f, -4.0f, 6.0f, 1.0f };  // Light Position
static const GLfloat LightAmb[] = { 4.0f, 4.0f, 4.0f, 1.0f };   // Ambient Light
static const GLfloat LightDif[] = { 1.0f, 1.0f, 1.0f, 1.0f };   // Diffuse Light

static void
gst_gl_filter_reflected_screen_class_init (GstGLFilterReflectedScreenClass *
    klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  gobject_class->set_property = gst_gl_filter_reflected_screen_set_property;
  gobject_class->get_property = gst_gl_filter_reflected_screen_get_property;

  GST_GL_FILTER_CLASS (klass)->filter_texture =
      gst_gl_filter_reflected_screen_filter_texture;

  g_object_class_install_property (gobject_class, PROP_ACTIVE_GRAPHIC_MODE,
      g_param_spec_boolean ("active-graphic-mode",
          "Activate graphic mode",
          "Allow user to activate stencil buffer and blending.",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SEPARATED_SCREEN,
      g_param_spec_boolean ("separated-screen",
          "Create a separation space",
          "Allow to insert a space between the two screen. Will cancel 'show floor' if active. Value are TRUE or FALSE(default)",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SHOW_FLOOR,
      g_param_spec_boolean ("show-floor",
          "Show the support",
          "Allow the user to show the supportive floor. Will cancel 'separated screen' if active. Value are TRUE(default) or FALSE",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FOVY,
      g_param_spec_double ("fovy", "Fovy", "Field of view angle in degrees",
          0.0, 180.0, 60, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ASPECT,
      g_param_spec_double ("aspect", "Aspect",
          "Field of view in the x direction", 1.0, 100, 1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ZNEAR,
      g_param_spec_double ("znear", "Znear",
          "Specifies the distance from the viewer to the near clipping plane",
          0.0000000001, 100.0, 0.1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ZFAR,
      g_param_spec_double ("zfar", "Zfar",
          "Specifies the distance from the viewer to the far clipping plane",
          0.0, 1000.0, 100.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (element_class,
      "OpenGL Reflected Screen filter", "Filter/Effect/Video",
      "Reflected Screen Filter", "Pierre POUZOL <pierre.pouzol@hotmail.fr>");

  GST_GL_BASE_FILTER_CLASS (klass)->supported_gl_api = GST_GL_API_OPENGL;
}

static void
gst_gl_filter_reflected_screen_init (GstGLFilterReflectedScreen * filter)
{
  filter->active_graphic_mode = TRUE;
  filter->separated_screen = FALSE;
  filter->show_floor = TRUE;
  filter->fovy = 90;
  filter->aspect = 1.0;
  filter->znear = 0.1;
  filter->zfar = 1000;
}

static void
gst_gl_filter_reflected_screen_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLFilterReflectedScreen *filter = GST_GL_FILTER_REFLECTED_SCREEN (object);

  switch (prop_id) {
    case PROP_ACTIVE_GRAPHIC_MODE:
      filter->active_graphic_mode = g_value_get_boolean (value);
      break;
    case PROP_SEPARATED_SCREEN:
      filter->separated_screen = g_value_get_boolean (value);
      break;
    case PROP_SHOW_FLOOR:
      filter->show_floor = g_value_get_boolean (value);
      break;
    case PROP_FOVY:
      filter->fovy = g_value_get_double (value);
      break;
    case PROP_ASPECT:
      filter->aspect = g_value_get_double (value);
      break;
    case PROP_ZNEAR:
      filter->znear = g_value_get_double (value);
      break;
    case PROP_ZFAR:
      filter->zfar = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filter_reflected_screen_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLFilterReflectedScreen *filter = GST_GL_FILTER_REFLECTED_SCREEN (object);

  switch (prop_id) {
    case PROP_ACTIVE_GRAPHIC_MODE:
      g_value_set_boolean (value, filter->active_graphic_mode);
      break;
    case PROP_SEPARATED_SCREEN:
      g_value_set_boolean (value, filter->separated_screen);
      break;
    case PROP_SHOW_FLOOR:
      g_value_set_boolean (value, filter->show_floor);
      break;
    case PROP_FOVY:
      g_value_set_double (value, filter->fovy);
      break;
    case PROP_ASPECT:
      g_value_set_double (value, filter->aspect);
      break;
    case PROP_ZNEAR:
      g_value_set_double (value, filter->znear);
      break;
    case PROP_ZFAR:
      g_value_set_double (value, filter->zfar);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_filter_reflected_screen_filter_texture (GstGLFilter * filter,
    guint in_tex, guint out_tex)
{
  GstGLFilterReflectedScreen *reflected_screen_filter =
      GST_GL_FILTER_REFLECTED_SCREEN (filter);

  //blocking call, use a FBO
  gst_gl_context_use_fbo (filter->context,
      GST_VIDEO_INFO_WIDTH (&filter->out_info),
      GST_VIDEO_INFO_HEIGHT (&filter->out_info),
      filter->fbo, filter->depthbuffer, out_tex,
      gst_gl_filter_reflected_screen_callback,
      GST_VIDEO_INFO_WIDTH (&filter->in_info),
      GST_VIDEO_INFO_HEIGHT (&filter->in_info), in_tex,
      reflected_screen_filter->fovy, reflected_screen_filter->aspect,
      reflected_screen_filter->znear, reflected_screen_filter->zfar,
      GST_GL_DISPLAY_PROJECTION_PERSPECTIVE,
      (gpointer) reflected_screen_filter);

  return TRUE;
}

static void
gst_gl_filter_reflected_screen_draw_separated_screen (GstGLFilter * filter,
    gint width, gint height, guint texture, gfloat alphs, gfloat alphe)
{
  //enable ARB Rectangular texturing
  //that's necessary to have the video displayed on our screen (with gstreamer)
  glEnable (GL_TEXTURE_2D);
  glBindTexture (GL_TEXTURE_2D, texture);
  //configure parameters for the texturing
  //the two first are used to specified how the texturing will be done if the screen is greater than the texture herself
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  //the next two specified how the texture will comport near the limits
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  //creating screen and setting the texture (depending on texture's height and width)
  glBegin (GL_QUADS);

  // right Face
  glColor4f (1.0f, 1.0f, 1.0f, alphs);
  glTexCoord2f (0.5f, 1.0f);
  glVertex3f (-0.75f, 0.0f, -1.0f);
  glColor4f (1.0f, 1.0f, 1.0f, alphe);
  glTexCoord2f (0.5f, 0.0f);
  glVertex3f (-0.75f, 1.25f, -1.0f);
  glTexCoord2f (1.0f, 0.0f);
  glVertex3f (1.25f, 1.25f, -1.0f);
  glColor4f (1.0f, 1.0f, 1.0f, alphs);
  glTexCoord2f (1.0f, 1.0f);
  glVertex3f (1.25f, 0.0f, -1.0f);
  // Left Face
  glColor4f (1.0f, 1.0f, 1.0f, alphs);
  glTexCoord2f (0.5f, 1.0f);
  glVertex3f (-1.0f, 0.0f, -0.75f);
  glTexCoord2f (0.0f, 1.0f);
  glVertex3f (-1.0f, 0.0f, 1.25f);
  glColor4f (1.0f, 1.0f, 1.0f, alphe);
  glTexCoord2f (0.0f, 0.0f);
  glVertex3f (-1.0f, 1.25f, 1.25f);
  glTexCoord2f (0.5f, 0.0f);
  glVertex3f (-1.0f, 1.25f, -0.75f);

  glEnd ();
  glDisable (GL_TEXTURE_2D);
}

static void
gst_gl_filter_reflected_screen_draw_screen (GstGLFilter * filter,
    gint width, gint height, guint texture)
{
  //enable ARB Rectangular texturing
  //that's necessary to have the video displayed on our screen (with gstreamer)
  glEnable (GL_TEXTURE_2D);
  glBindTexture (GL_TEXTURE_2D, texture);
  //configure parameters for the texturing
  //the two first are used to specified how the texturing will be done if the screen is greater than the texture herself
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  //the next two specified how the texture will comport near the limits
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  //creating screen and setting the texture (depending on texture's height and width)
  glBegin (GL_QUADS);

  glTexCoord2f (0.5f, 1.0f);
  glVertex3f (-1.0f, 0.0f, -1.0f);
  glTexCoord2f (0.5f, 0.0f);
  glVertex3f (-1.0f, 1.0f, -1.0f);
  glTexCoord2f (1.0f, 0.0f);
  glVertex3f (1.0f, 1.0f, -1.0f);
  glTexCoord2f (1.0f, 1.0f);
  glVertex3f (1.0f, 0.0f, -1.0f);
  // Left Face
  glTexCoord2f (0.5f, 1.0f);
  glVertex3f (-1.0f, 0.0f, -1.0f);
  glTexCoord2f (0.0f, 1.0f);
  glVertex3f (-1.0f, 0.0f, 1.0f);
  glTexCoord2f (0.0f, 0.0f);
  glVertex3f (-1.0f, 1.0f, 1.0f);
  glTexCoord2f (0.5f, 0.0f);
  glVertex3f (-1.0f, 1.0f, -1.0f);

  glEnd ();

  //disable this kind of texturing (useless for the gluDisk)
  glDisable (GL_TEXTURE_2D);
}

static void
gst_gl_filter_reflected_screen_draw_background (void)
{
  glBegin (GL_QUADS);

  // right Face

  glColor4f (0.0f, 0.0f, 0.0f, 1.0f);
  glVertex3f (-10.0f, -10.0f, -1.0f);

  glColor4f (0.0f, 0.0f, 0.2f, 1.0f);
  glVertex3f (-10.0f, 10.0f, -1.0f);
  glVertex3f (10.0f, 10.0f, -1.0f);
  glVertex3f (10.0f, -10.0f, -1.0f);

  glEnd ();
}

static void
gst_gl_filter_reflected_screen_draw_floor (void)
{
  GLUquadricObj *q;
  //create a quadric for the floor's drawing
  q = gluNewQuadric ();
  //configure this quadric's parameter (for lighting and texturing)
  gluQuadricNormals (q, GL_SMOOTH);
  gluQuadricTexture (q, GL_FALSE);

  //drawing the disk. The texture are mapped thanks to the parameter we gave to the GLUquadric q
  gluDisk (q, 0.0, 2.2, 50, 1);
}

//opengl scene, params: input texture (not the output filter->texture)
static void
gst_gl_filter_reflected_screen_callback (gint width, gint height, guint texture,
    gpointer stuff)
{
  GstGLFilter *filter = GST_GL_FILTER (stuff);
  GstGLFilterReflectedScreen *reflected_screen_filter =
      GST_GL_FILTER_REFLECTED_SCREEN (stuff);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  //load identity before tracing
  glLoadIdentity ();
  //camera translation
  glTranslatef (0.0f, 0.1f, -1.3f);
  //camera configuration
  if (reflected_screen_filter->separated_screen)
    gluLookAt (0.1, -0.25, 2.0, 0.025, 0.0, 0.0, 0.0, 1.0, 0.0);
  else
    gluLookAt (0.1, -0.35, 2.0, 0.025, 0.0, 0.0, 0.0, 1.0, 0.0);

  gst_gl_filter_reflected_screen_draw_background ();

  if (reflected_screen_filter->separated_screen) {
    glEnable (GL_BLEND);

    glPushMatrix ();
    glScalef (1.0f, -1.0f, 1.0f);
    glTranslatef (0.0f, 0.0f, 1.2f);
    glRotatef (-45.0f, 0.0, 1.0, 0.0);
    gst_gl_filter_reflected_screen_draw_separated_screen (filter, width, height,
        texture, 1.0f, 1.0f);
    glPopMatrix ();

    if (reflected_screen_filter->active_graphic_mode) {
      //configuration of the transparency function
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glTranslatef (0.0f, 0.0f, 1.2f);
      glRotatef (-45.0f, 0.0, 1.0, 0.0);
      gst_gl_filter_reflected_screen_draw_separated_screen (filter, width,
          height, texture, 0.5f, 0.0f);
      glDisable (GL_BLEND);
    }
  }
  if (reflected_screen_filter->show_floor) {
    glLightfv (GL_LIGHT0, GL_AMBIENT, LightAmb);
    glLightfv (GL_LIGHT0, GL_DIFFUSE, LightDif);
    glLightfv (GL_LIGHT0, GL_POSITION, LightPos);

    //enable lighting
    glEnable (GL_LIGHT0);
    glEnable (GL_LIGHTING);

    if (reflected_screen_filter->active_graphic_mode) {
      glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      //enable stencil buffer use
      glEnable (GL_STENCIL_TEST);
      //setting the stencil buffer. Each time a pixel will be drawn by now, this pixel value will be set to 1
      glStencilFunc (GL_ALWAYS, 1, 1);
      glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE);

      //disable the zbuffer
      glDisable (GL_DEPTH_TEST);
      //make a rotation of 90 degree on x axis. By default, gluDisk draw a disk on z axis
      glRotatef (-90.0f, 1.0, 0.0, 0.0);
      //draw the floor. Each pixel representing this floor will now have a value of 1 on stencil buffer
      gst_gl_filter_reflected_screen_draw_floor ();
      //make an anti-rotation of 90 degree to draw the rest of the scene on the right angle
      glRotatef (90.0f, 1.0, 0.0, 0.0);
      //enable zbuffer again
      glEnable (GL_DEPTH_TEST);
      //enable the drawing to be shown
      glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      //say that the next object have to be drawn ONLY where the stencil buffer's pixel's value is 1
      glStencilFunc (GL_EQUAL, 1, 1);
      glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);

      //save the actual matrix
      glPushMatrix ();
      glLightfv (GL_LIGHT0, GL_POSITION, LightPos);
      //translate the object on z axis
      glTranslatef (0.0f, 0.0f, 1.4f);
      //rotate it (because the drawing method place the user behind the left part of the screen)
      glRotatef (-45.0f, 0.0, 1.0, 0.0);
      //draw the reflexion
      gst_gl_filter_reflected_screen_draw_screen (filter, width, height,
          texture);
      //return to the saved matrix position
      glPopMatrix ();
      //end of the stencil buffer uses
      glDisable (GL_STENCIL_TEST);

      //enable the blending to mix the floor and reflexion color
      glEnable (GL_BLEND);
      glDisable (GL_LIGHTING);
      //specified a white color (for the floor) with 20% transparency
      glColor4f (1.0f, 1.0f, 1.0f, 0.8f);
      //configuration of the transparency function
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    //draw the floor (which will appear this time)
    glRotatef (-90.0f, 1.0, 0.0, 0.0);
    gst_gl_filter_reflected_screen_draw_floor ();
    glRotatef (90.0f, 1.0, 0.0, 0.0);
    glDisable (GL_BLEND);
    glEnable (GL_LIGHTING);
    //draw the real object
    //scale on y axis. The object must be drawn upside down (to suggest a reflexion)
    glScalef (1.0f, -1.0f, 1.0f);
    glTranslatef (0.0f, 0.0f, 1.4f);
    glRotatef (-45.0f, 0.0, 1.0, 0.0);
    gst_gl_filter_reflected_screen_draw_screen (filter, width, height, texture);
    glDisable (GL_LIGHTING);
  }
}
