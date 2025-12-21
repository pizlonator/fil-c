/*
 * GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2002,2007 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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
 * SECTION:element-gltestsrc
 * @title: gltestsrc
 *
 * The gltestsrc element is used to produce test video texture.
 * The video test produced can be controlled with the "pattern"
 * property.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 -v gltestsrc pattern=smpte ! glimagesink
 * ]|
 * Shows original SMPTE color bars in a window.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gl/gstglfuncs.h>
#include <glib/gi18n-lib.h>

#include "gstglelements.h"
#include "gstgltestsrc.h"
#include "gltestsrc.h"

GST_DEBUG_CATEGORY_STATIC (gl_test_src_debug);
#define GST_CAT_DEFAULT gl_test_src_debug

enum
{
  PROP_0,
  PROP_PATTERN,
  PROP_IS_LIVE
      /* FILL ME */
};

/* *INDENT-OFF* */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
        "format = (string) RGBA, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE ","
        "texture-target = (string) 2D")
    );
/* *INDENT-ON* */

#define gst_gl_test_src_parent_class parent_class
G_DEFINE_TYPE (GstGLTestSrc, gst_gl_test_src, GST_TYPE_GL_BASE_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gltestsrc, "gltestsrc",
    GST_RANK_NONE, GST_TYPE_GL_TEST_SRC, gl_element_init (plugin));

static void gst_gl_test_src_set_pattern (GstGLTestSrc * gltestsrc,
    int pattern_type);
static void gst_gl_test_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_test_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_gl_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_gl_test_src_is_seekable (GstBaseSrc * psrc);
static gboolean gst_gl_test_src_callback (gpointer stuff);
static gboolean gst_gl_test_src_gl_start (GstGLBaseSrc * src);
static void gst_gl_test_src_gl_stop (GstGLBaseSrc * src);
static gboolean gst_gl_test_src_fill_memory (GstGLBaseSrc * src,
    GstGLMemory * memory);

#define GST_TYPE_GL_TEST_SRC_PATTERN (gst_gl_test_src_pattern_get_type ())
static GType
gst_gl_test_src_pattern_get_type (void)
{
  static GType gl_test_src_pattern_type = 0;
  static const GEnumValue pattern_types[] = {
    {GST_GL_TEST_SRC_SMPTE, "SMPTE 100% color bars", "smpte"},
    {GST_GL_TEST_SRC_SNOW, "Random (television snow)", "snow"},
    {GST_GL_TEST_SRC_BLACK, "100% Black", "black"},
    {GST_GL_TEST_SRC_WHITE, "100% White", "white"},
    {GST_GL_TEST_SRC_RED, "Red", "red"},
    {GST_GL_TEST_SRC_GREEN, "Green", "green"},
    {GST_GL_TEST_SRC_BLUE, "Blue", "blue"},
    {GST_GL_TEST_SRC_CHECKERS1, "Checkers 1px", "checkers-1"},
    {GST_GL_TEST_SRC_CHECKERS2, "Checkers 2px", "checkers-2"},
    {GST_GL_TEST_SRC_CHECKERS4, "Checkers 4px", "checkers-4"},
    {GST_GL_TEST_SRC_CHECKERS8, "Checkers 8px", "checkers-8"},
    {GST_GL_TEST_SRC_CIRCULAR, "Circular", "circular"},
    {GST_GL_TEST_SRC_BLINK, "Blink", "blink"},
    {GST_GL_TEST_SRC_MANDELBROT, "Mandelbrot Fractal", "mandelbrot"},
    {0, NULL, NULL}
  };

  if (!gl_test_src_pattern_type) {
    gl_test_src_pattern_type =
        g_enum_register_static ("GstGLTestSrcPattern", pattern_types);
  }
  return gl_test_src_pattern_type;
}

static void
gst_gl_test_src_class_init (GstGLTestSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstGLBaseSrcClass *gstglbasesrc_class;
  GstElementClass *element_class;

  GST_DEBUG_CATEGORY_INIT (gl_test_src_debug, "gltestsrc", 0,
      "Video Test Source");

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  gstglbasesrc_class = GST_GL_BASE_SRC_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_gl_test_src_set_property;
  gobject_class->get_property = gst_gl_test_src_get_property;

  g_object_class_install_property (gobject_class, PROP_PATTERN,
      g_param_spec_enum ("pattern", "Pattern",
          "Type of test pattern to generate", GST_TYPE_GL_TEST_SRC_PATTERN,
          GST_GL_TEST_SRC_SMPTE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (element_class, "Video test source",
      "Source/Video", "Creates a test video stream",
      "David A. Schleef <ds@schleef.org>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);

  gstbasesrc_class->is_seekable = gst_gl_test_src_is_seekable;
  gstbasesrc_class->fixate = gst_gl_test_src_fixate;

  gstglbasesrc_class->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2;
  gstglbasesrc_class->gl_start = gst_gl_test_src_gl_start;
  gstglbasesrc_class->gl_stop = gst_gl_test_src_gl_stop;
  gstglbasesrc_class->fill_gl_memory = gst_gl_test_src_fill_memory;

  gst_type_mark_as_plugin_api (GST_TYPE_GL_TEST_SRC_PATTERN, 0);
}

static void
gst_gl_test_src_init (GstGLTestSrc * src)
{
  gst_gl_test_src_set_pattern (src, GST_GL_TEST_SRC_SMPTE);
}

static GstCaps *
gst_gl_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstStructure *structure;

  GST_DEBUG ("fixate");

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (structure, "width", 320);
  gst_structure_fixate_field_nearest_int (structure, "height", 240);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", 30, 1);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

  return caps;
}

static void
gst_gl_test_src_set_pattern (GstGLTestSrc * gltestsrc, gint pattern_type)
{
  gltestsrc->set_pattern = pattern_type;
}

static void
gst_gl_test_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (object);

  switch (prop_id) {
    case PROP_PATTERN:
      gst_gl_test_src_set_pattern (src, g_value_get_enum (value));
      break;
    case PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (src), g_value_get_boolean (value));
      break;
    default:
      break;
  }
}

static void
gst_gl_test_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (object);

  switch (prop_id) {
    case PROP_PATTERN:
      g_value_set_enum (value, src->set_pattern);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (src)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_test_src_is_seekable (GstBaseSrc * psrc)
{
  /* we're seekable... */
  return TRUE;
}

static gboolean
gst_gl_test_src_callback (gpointer stuff)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (stuff);
  GstGLBaseSrc *glbasesrc = GST_GL_BASE_SRC (src);
  const struct SrcFuncs *funcs;

  funcs = src->src_funcs;

  if (!funcs || src->set_pattern != src->active_pattern) {
    if (src->src_impl && funcs)
      funcs->free (src->src_impl);
    src->src_funcs = funcs =
        gst_gl_test_src_get_src_funcs_for_pattern (src->set_pattern);
    if (funcs == NULL) {
      GST_ERROR_OBJECT (src, "Could not find an implementation of the "
          "requested pattern");
      return FALSE;
    }
    src->src_impl = funcs->new (src);
    if (!funcs->init (src->src_impl, glbasesrc->context, &glbasesrc->out_info)) {
      GST_ERROR_OBJECT (src, "Failed to initialize pattern");
      return FALSE;
    }
    src->active_pattern = src->set_pattern;
  }

  return funcs->fill_bound_fbo (src->src_impl);
}

static gboolean
gst_gl_test_src_fill_memory (GstGLBaseSrc * src, GstGLMemory * memory)
{
  GstGLTestSrc *test_src = GST_GL_TEST_SRC (src);
  return gst_gl_framebuffer_draw_to_texture (test_src->fbo, memory,
      gst_gl_test_src_callback, test_src);
}

static gboolean
gst_gl_test_src_gl_start (GstGLBaseSrc * bsrc)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (bsrc);
  src->fbo = gst_gl_framebuffer_new_with_default_depth (bsrc->context,
      GST_VIDEO_INFO_WIDTH (&bsrc->out_info),
      GST_VIDEO_INFO_HEIGHT (&bsrc->out_info));
  return TRUE;
}

static void
gst_gl_test_src_gl_stop (GstGLBaseSrc * bsrc)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (bsrc);

  if (src->fbo)
    gst_object_unref (src->fbo);
  src->fbo = NULL;

  if (src->src_impl)
    src->src_funcs->free (src->src_impl);
  src->src_impl = NULL;
  src->src_funcs = NULL;
}
