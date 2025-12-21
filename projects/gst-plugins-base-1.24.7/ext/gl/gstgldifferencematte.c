/*
 * GStreamer
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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
 * SECTION:element-gldifferencematte.
 * @title: gldifferencematte.
 *
 * Saves a background frame and replace it with a pixbuf.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 videotestsrc ! glupload ! gldifferencemate location=backgroundimagefile ! glimagesink
 * ]|
 * FBO (Frame Buffer Object) and GLSL (OpenGL Shading Language) are required.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <png.h>

#include <gst/gl/gstglfuncs.h>

#include "gstglelements.h"
#include "gstgldifferencematte.h"
#include "effects/gstgleffectssources.h"

#if PNG_LIBPNG_VER >= 10400
#define int_p_NULL         NULL
#define png_infopp_NULL    NULL
#endif

#define GST_CAT_DEFAULT gst_gl_differencematte_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_differencematte_debug, "gldifferencematte", 0, "gldifferencematte element");

#define gst_gl_differencematte_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLDifferenceMatte, gst_gl_differencematte,
    GST_TYPE_GL_FILTER, DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gldifferencematte, "gldifferencematte",
    GST_RANK_NONE, GST_TYPE_GL_DIFFERENCEMATTE, gl_element_init (plugin));

static void gst_gl_differencematte_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gl_differencematte_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_gl_differencematte_filter_texture (GstGLFilter * filter,
    GstGLMemory * in_tex, GstGLMemory * out_tex);

static gboolean gst_gl_differencematte_loader (GstGLFilter * filter);

enum
{
  PROP_0,
  PROP_LOCATION,
};


/* init resources that need a gl context */
static gboolean
gst_gl_differencematte_gl_start (GstGLBaseFilter * base_filter)
{
  GstGLDifferenceMatte *differencematte = GST_GL_DIFFERENCEMATTE (base_filter);
  GstGLFilter *filter = GST_GL_FILTER (base_filter);
  GstGLContext *context = base_filter->context;
  GstGLBaseMemoryAllocator *tex_alloc;
  GstGLAllocationParams *params;
  GError *error = NULL;
  const gchar *frags[2];
  gint i;

  if (!GST_GL_BASE_FILTER_CLASS (parent_class)->gl_start (base_filter))
    return FALSE;

  tex_alloc = (GstGLBaseMemoryAllocator *)
      gst_gl_memory_allocator_get_default (context);
  params =
      (GstGLAllocationParams *) gst_gl_video_allocation_params_new (context,
      NULL, &filter->out_info, 0, NULL, GST_GL_TEXTURE_TARGET_2D, GST_GL_RGBA);

  for (i = 0; i < 4; i++)
    differencematte->midtexture[i] =
        (GstGLMemory *) gst_gl_base_memory_alloc (tex_alloc, params);
  gst_gl_allocation_params_free (params);
  gst_object_unref (tex_alloc);

  if (!(differencematte->identity_shader =
          gst_gl_shader_new_default (context, &error))) {
    GST_ELEMENT_ERROR (differencematte, RESOURCE, NOT_FOUND, ("%s",
            "Failed to compile identity shader"), ("%s", error->message));
    return FALSE;
  }

  frags[0] =
      gst_gl_shader_string_get_highest_precision (context,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);

  frags[1] = difference_fragment_source;
  if (!(differencematte->shader[0] =
          gst_gl_shader_new_link_with_stages (context, &error,
              gst_glsl_stage_new_default_vertex (context),
              gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
                  GST_GLSL_VERSION_NONE,
                  GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
                  frags), NULL))) {
    GST_ELEMENT_ERROR (differencematte, RESOURCE, NOT_FOUND, ("%s",
            "Failed to compile difference shader"), ("%s", error->message));
    return FALSE;
  }

  frags[1] = hconv7_fragment_source_gles2;
  if (!(differencematte->shader[1] =
          gst_gl_shader_new_link_with_stages (context, &error,
              gst_glsl_stage_new_default_vertex (context),
              gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
                  GST_GLSL_VERSION_NONE,
                  GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
                  frags), NULL))) {
    GST_ELEMENT_ERROR (differencematte, RESOURCE, NOT_FOUND, ("%s",
            "Failed to compile convolution shader"), ("%s", error->message));
    return FALSE;
  }

  frags[1] = vconv7_fragment_source_gles2;
  if (!(differencematte->shader[2] =
          gst_gl_shader_new_link_with_stages (context, &error,
              gst_glsl_stage_new_default_vertex (context),
              gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
                  GST_GLSL_VERSION_NONE,
                  GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
                  frags), NULL))) {
    GST_ELEMENT_ERROR (differencematte, RESOURCE, NOT_FOUND, ("%s",
            "Failed to compile convolution shader"), ("%s", error->message));
    return FALSE;
  }

  frags[1] = texture_interp_fragment_source;
  if (!(differencematte->shader[3] =
          gst_gl_shader_new_link_with_stages (context, &error,
              gst_glsl_stage_new_default_vertex (context),
              gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
                  GST_GLSL_VERSION_NONE,
                  GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
                  frags), NULL))) {
    GST_ELEMENT_ERROR (differencematte, RESOURCE, NOT_FOUND, ("%s",
            "Failed to compile interpolation shader"), ("%s", error->message));
    return FALSE;
  }

  /* FIXME: this should really be per shader */
  filter->draw_attr_position_loc =
      gst_gl_shader_get_attribute_location (differencematte->shader[2],
      "a_position");
  filter->draw_attr_texture_loc =
      gst_gl_shader_get_attribute_location (differencematte->shader[2],
      "a_texcoord");

  return TRUE;
}

/* free resources that need a gl context */
static void
gst_gl_differencematte_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLDifferenceMatte *differencematte = GST_GL_DIFFERENCEMATTE (base_filter);
  gint i;

  if (differencematte->savedbgtexture) {
    gst_memory_unref (GST_MEMORY_CAST (differencematte->savedbgtexture));
    differencematte->savedbgtexture = NULL;
  }

  if (differencematte->newbgtexture) {
    gst_memory_unref (GST_MEMORY_CAST (differencematte->newbgtexture));
    differencematte->newbgtexture = NULL;
  }

  for (i = 0; i < 4; i++) {
    if (differencematte->identity_shader) {
      gst_object_unref (differencematte->identity_shader);
      differencematte->identity_shader = NULL;
    }

    if (differencematte->shader[i]) {
      gst_object_unref (differencematte->shader[i]);
      differencematte->shader[i] = NULL;
    }

    if (differencematte->midtexture[i]) {
      gst_memory_unref (GST_MEMORY_CAST (differencematte->midtexture[i]));
      differencematte->midtexture[i] = NULL;
    }
  }
  differencematte->location = NULL;
  differencematte->pixbuf = NULL;
  differencematte->bg_has_changed = FALSE;

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static void
gst_gl_differencematte_class_init (GstGLDifferenceMatteClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  gobject_class->set_property = gst_gl_differencematte_set_property;
  gobject_class->get_property = gst_gl_differencematte_get_property;

  GST_GL_BASE_FILTER_CLASS (klass)->gl_start = gst_gl_differencematte_gl_start;
  GST_GL_BASE_FILTER_CLASS (klass)->gl_stop = gst_gl_differencematte_gl_stop;

  GST_GL_FILTER_CLASS (klass)->filter_texture =
      gst_gl_differencematte_filter_texture;

  g_object_class_install_property (gobject_class,
      PROP_LOCATION,
      g_param_spec_string ("location",
          "Background image location",
          "Background image location", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (element_class,
      "Gstreamer OpenGL DifferenceMatte", "Filter/Effect/Video",
      "Saves a background frame and replace it with a pixbuf",
      "Filippo Argiolas <filippo.argiolas@gmail.com>");

  GST_GL_BASE_FILTER_CLASS (klass)->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2;
}

static void
gst_gl_differencematte_init (GstGLDifferenceMatte * differencematte)
{
  differencematte->shader[0] = NULL;
  differencematte->shader[1] = NULL;
  differencematte->shader[2] = NULL;
  differencematte->shader[3] = NULL;
  differencematte->location = NULL;
  differencematte->pixbuf = NULL;
  differencematte->savedbgtexture = 0;
  differencematte->newbgtexture = 0;
  differencematte->bg_has_changed = FALSE;

  fill_gaussian_kernel (differencematte->kernel, 7, 30.0);
}

static void
gst_gl_differencematte_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLDifferenceMatte *differencematte = GST_GL_DIFFERENCEMATTE (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_free (differencematte->location);
      differencematte->bg_has_changed = TRUE;
      differencematte->location = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_differencematte_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLDifferenceMatte *differencematte = GST_GL_DIFFERENCEMATTE (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, differencematte->location);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
init_pixbuf_texture (GstGLDifferenceMatte * differencematte)
{
  GstGLContext *context = GST_GL_BASE_FILTER (differencematte)->context;
  GstGLFilter *filter = GST_GL_FILTER (differencematte);
  GstGLBaseMemoryAllocator *tex_alloc;
  GstGLAllocationParams *params;
  GstVideoInfo v_info;

  tex_alloc = (GstGLBaseMemoryAllocator *)
      gst_gl_memory_allocator_get_default (context);
  gst_video_info_set_format (&v_info, GST_VIDEO_FORMAT_RGBA,
      differencematte->pbuf_width, differencematte->pbuf_height);
  params =
      (GstGLAllocationParams *) gst_gl_video_allocation_params_new (context,
      NULL, &v_info, 0, NULL, GST_GL_TEXTURE_TARGET_2D, GST_GL_RGBA);

  differencematte->newbgtexture =
      (GstGLMemory *) gst_gl_base_memory_alloc (tex_alloc, params);
  gst_gl_allocation_params_free (params);

  if (differencematte->savedbgtexture == NULL) {
    params =
        (GstGLAllocationParams *) gst_gl_video_allocation_params_new (context,
        NULL, &filter->out_info, 0, NULL, GST_GL_TEXTURE_TARGET_2D,
        GST_GL_RGBA);

    differencematte->savedbgtexture =
        (GstGLMemory *) gst_gl_base_memory_alloc (tex_alloc, params);
    gst_gl_allocation_params_free (params);
  }

  gst_object_unref (tex_alloc);
}

static gboolean
gst_gl_differencematte_diff (GstGLFilter * filter, GstGLMemory * in_tex,
    gpointer stuff)
{
  GstGLDifferenceMatte *differencematte = GST_GL_DIFFERENCEMATTE (filter);
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (filter)->context->gl_vtable;

  gst_gl_shader_use (differencematte->shader[0]);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, gst_gl_memory_get_texture_id (in_tex));

  gst_gl_shader_set_uniform_1i (differencematte->shader[0], "current", 0);

  gl->ActiveTexture (GL_TEXTURE1);
  gl->BindTexture (GL_TEXTURE_2D,
      gst_gl_memory_get_texture_id (differencematte->savedbgtexture));

  gst_gl_shader_set_uniform_1i (differencematte->shader[0], "saved", 1);

  gst_gl_filter_draw_fullscreen_quad (filter);

  return TRUE;
}

static gboolean
gst_gl_differencematte_hblur (GstGLFilter * filter, GstGLMemory * in_tex,
    gpointer stuff)
{
  GstGLDifferenceMatte *differencematte = GST_GL_DIFFERENCEMATTE (filter);
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (filter)->context->gl_vtable;

  gst_gl_shader_use (differencematte->shader[1]);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, gst_gl_memory_get_texture_id (in_tex));

  gst_gl_shader_set_uniform_1i (differencematte->shader[1], "tex", 0);

  gst_gl_shader_set_uniform_1fv (differencematte->shader[1], "kernel", 7,
      differencematte->kernel);
  gst_gl_shader_set_uniform_1f (differencematte->shader[1], "gauss_width",
      GST_VIDEO_INFO_WIDTH (&filter->out_info));

  gst_gl_filter_draw_fullscreen_quad (filter);

  return TRUE;
}

static gboolean
gst_gl_differencematte_vblur (GstGLFilter * filter, GstGLMemory * in_tex,
    gpointer stuff)
{
  GstGLDifferenceMatte *differencematte = GST_GL_DIFFERENCEMATTE (filter);
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (filter)->context->gl_vtable;

  gst_gl_shader_use (differencematte->shader[2]);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, gst_gl_memory_get_texture_id (in_tex));

  gst_gl_shader_set_uniform_1i (differencematte->shader[2], "tex", 0);

  gst_gl_shader_set_uniform_1fv (differencematte->shader[2], "kernel", 7,
      differencematte->kernel);
  gst_gl_shader_set_uniform_1f (differencematte->shader[2], "gauss_height",
      GST_VIDEO_INFO_HEIGHT (&filter->out_info));

  gst_gl_filter_draw_fullscreen_quad (filter);

  return TRUE;
}

static gboolean
gst_gl_differencematte_interp (GstGLFilter * filter, GstGLMemory * in_tex,
    gpointer stuff)
{
  GstGLDifferenceMatte *differencematte = GST_GL_DIFFERENCEMATTE (filter);
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (filter)->context->gl_vtable;

  gst_gl_shader_use (differencematte->shader[3]);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, gst_gl_memory_get_texture_id (in_tex));

  gst_gl_shader_set_uniform_1i (differencematte->shader[3], "blend", 0);

  gl->ActiveTexture (GL_TEXTURE1);
  gl->BindTexture (GL_TEXTURE_2D, differencematte->newbgtexture->tex_id);

  gst_gl_shader_set_uniform_1i (differencematte->shader[3], "base", 1);

  gl->ActiveTexture (GL_TEXTURE2);
  gl->BindTexture (GL_TEXTURE_2D, differencematte->midtexture[2]->tex_id);

  gst_gl_shader_set_uniform_1i (differencematte->shader[3], "alpha", 2);

  gst_gl_filter_draw_fullscreen_quad (filter);

  return TRUE;
}

static gboolean
gst_gl_differencematte_filter_texture (GstGLFilter * filter,
    GstGLMemory * in_tex, GstGLMemory * out_tex)
{
  GstGLDifferenceMatte *differencematte = GST_GL_DIFFERENCEMATTE (filter);

  differencematte->intexture = in_tex;

  if (differencematte->bg_has_changed && (differencematte->location != NULL)) {

    if (!gst_gl_differencematte_loader (filter))
      differencematte->pixbuf = NULL;

    init_pixbuf_texture (differencematte);

    /* save current frame, needed to calculate difference between
     * this frame and next ones */
    gst_gl_filter_render_to_target_with_shader (filter, in_tex,
        differencematte->savedbgtexture, differencematte->identity_shader);

    if (differencematte->pixbuf) {
      free (differencematte->pixbuf);
      differencematte->pixbuf = NULL;
    }

    differencematte->bg_has_changed = FALSE;
  }

  if (differencematte->savedbgtexture != NULL) {
    gst_gl_filter_render_to_target (filter, in_tex,
        differencematte->midtexture[0], gst_gl_differencematte_diff, NULL);
    gst_gl_filter_render_to_target (filter, differencematte->midtexture[0],
        differencematte->midtexture[1], gst_gl_differencematte_hblur, NULL);
    gst_gl_filter_render_to_target (filter, differencematte->midtexture[1],
        differencematte->midtexture[2], gst_gl_differencematte_vblur, NULL);
    gst_gl_filter_render_to_target (filter, in_tex, out_tex,
        gst_gl_differencematte_interp, NULL);
  } else {
    gst_gl_filter_render_to_target_with_shader (filter, in_tex, out_tex,
        differencematte->identity_shader);
  }

  return TRUE;
}

static void
user_warning_fn (png_structp png_ptr, png_const_charp warning_msg)
{
  g_warning ("%s\n", warning_msg);
}

#define LOAD_ERROR(msg) { GST_WARNING ("unable to load %s: %s", differencematte->location, msg); return FALSE; }

static gboolean
gst_gl_differencematte_loader (GstGLFilter * filter)
{
  GstGLDifferenceMatte *differencematte = GST_GL_DIFFERENCEMATTE (filter);

  png_structp png_ptr;
  png_infop info_ptr;
  guint sig_read = 0;
  png_uint_32 width = 0;
  png_uint_32 height = 0;
  gint bit_depth = 0;
  gint color_type = 0;
  gint interlace_type = 0;
  png_FILE_p fp = NULL;
  guint y = 0;
  guchar **rows = NULL;
  gint filler;

  if (!GST_GL_BASE_FILTER (filter)->context)
    return TRUE;

  if ((fp = fopen (differencematte->location, "rb")) == NULL)
    LOAD_ERROR ("file not found");

  png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (png_ptr == NULL) {
    fclose (fp);
    LOAD_ERROR ("failed to initialize the png_struct");
  }

  png_set_error_fn (png_ptr, NULL, NULL, user_warning_fn);

  info_ptr = png_create_info_struct (png_ptr);
  if (info_ptr == NULL) {
    fclose (fp);
    png_destroy_read_struct (&png_ptr, png_infopp_NULL, png_infopp_NULL);
    LOAD_ERROR ("failed to initialize the memory for image information");
  }

  png_init_io (png_ptr, fp);

  png_set_sig_bytes (png_ptr, sig_read);

  png_read_info (png_ptr, info_ptr);

  png_get_IHDR (png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
      &interlace_type, int_p_NULL, int_p_NULL);

  if (color_type == PNG_COLOR_TYPE_RGB) {
    filler = 0xff;
    png_set_filler (png_ptr, filler, PNG_FILLER_AFTER);
    color_type = PNG_COLOR_TYPE_RGB_ALPHA;
  }

  if (color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
    fclose (fp);
    png_destroy_read_struct (&png_ptr, png_infopp_NULL, png_infopp_NULL);
    LOAD_ERROR ("color type is not rgb");
  }

  differencematte->pbuf_width = width;
  differencematte->pbuf_height = height;

  differencematte->pixbuf =
      (guchar *) malloc (sizeof (guchar) * width * height * 4);

  rows = (guchar **) malloc (sizeof (guchar *) * height);

  for (y = 0; y < height; ++y)
    rows[y] = (guchar *) (differencematte->pixbuf + y * width * 4);

  png_read_image (png_ptr, rows);

  free (rows);

  png_read_end (png_ptr, info_ptr);
  png_destroy_read_struct (&png_ptr, &info_ptr, png_infopp_NULL);
  fclose (fp);

  return TRUE;
}
