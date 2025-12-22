/* GStreamer faceoverlay plugin
 * Copyright (C) 2011 Laura Lucas Alday <lauralucas@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * SECTION:element-faceoverlay
 *
 * Overlays a SVG image over a detected face in a video stream.
 * x, y, w, and h properties are optional, and change the image position and
 * size relative to the detected face position and size.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 autovideosrc ! videoconvert ! faceoverlay location=/path/to/gnome-video-effects/pixmaps/bow.svg x=0.5 y=0.5 w=0.7 h=0.7 ! videoconvert ! autovideosink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>

#include "gstfaceoverlay.h"

GST_DEBUG_CATEGORY_STATIC (gst_face_overlay_debug);
#define GST_CAT_DEFAULT gst_face_overlay_debug

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_X,
  PROP_Y,
  PROP_W,
  PROP_H
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{RGB}")));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{BGRA}")));

#define gst_face_overlay_parent_class parent_class
G_DEFINE_TYPE (GstFaceOverlay, gst_face_overlay, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (faceoverlay, "faceoverlay",
    GST_RANK_NONE, GST_TYPE_FACEOVERLAY,
    GST_DEBUG_CATEGORY_INIT (gst_face_overlay_debug, "faceoverlay", 0,
        "SVG Face Overlay");
    );

static void gst_face_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_face_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_face_overlay_message_handler (GstBin * bin,
    GstMessage * message);
static GstStateChangeReturn gst_face_overlay_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_face_overlay_create_children (GstFaceOverlay * filter);

static gboolean
gst_face_overlay_create_children (GstFaceOverlay * filter)
{
  GstElement *csp, *face_detect, *overlay;
  GstPad *pad;

  csp = gst_element_factory_make ("videoconvert", NULL);
  face_detect = gst_element_factory_make ("facedetect", NULL);
  overlay = gst_element_factory_make ("rsvgoverlay", NULL);

  /* FIXME: post missing-plugin messages on NULL->READY if needed */
  if (csp == NULL || face_detect == NULL || overlay == NULL)
    goto missing_element;

  g_object_set (face_detect, "display", FALSE, NULL);

  gst_bin_add_many (GST_BIN (filter), face_detect, csp, overlay, NULL);
  filter->svg_overlay = overlay;

  if (!gst_element_link_many (face_detect, csp, overlay, NULL))
    GST_ERROR_OBJECT (filter, "couldn't link elements");

  pad = gst_element_get_static_pad (face_detect, "sink");
  if (!gst_ghost_pad_set_target (GST_GHOST_PAD (filter->sinkpad), pad))
    GST_ERROR_OBJECT (filter->sinkpad, "couldn't set sinkpad target");
  gst_object_unref (pad);

  pad = gst_element_get_static_pad (overlay, "src");
  if (!gst_ghost_pad_set_target (GST_GHOST_PAD (filter->srcpad), pad))
    GST_ERROR_OBJECT (filter->srcpad, "couldn't set srcpad target");
  gst_object_unref (pad);

  return TRUE;

/* ERRORS */
missing_element:
  {
    /* clean up */
    if (csp == NULL)
      GST_ERROR_OBJECT (filter, "videoconvert element not found");
    else
      gst_object_unref (csp);

    if (face_detect == NULL)
      GST_ERROR_OBJECT (filter, "facedetect element not found (opencv plugin)");
    else
      gst_object_unref (face_detect);

    if (overlay == NULL)
      GST_ERROR_OBJECT (filter, "rsvgoverlay element not found (rsvg plugin)");
    else
      gst_object_unref (overlay);

    return FALSE;
  }
}

static GstStateChangeReturn
gst_face_overlay_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstFaceOverlay *filter = GST_FACEOVERLAY (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (filter->svg_overlay == NULL) {
        GST_ELEMENT_ERROR (filter, CORE, MISSING_PLUGIN, (NULL),
            ("Some required plugins are missing, probably either the opencv "
                "facedetect element or rsvgoverlay"));
        return GST_STATE_CHANGE_FAILURE;
      }
      filter->update_svg = TRUE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    default:
      break;
  }

  return ret;
}

static void
gst_face_overlay_handle_faces (GstFaceOverlay * filter, const GstStructure * s)
{
  guint x, y, width, height;
  gint svg_x, svg_y, svg_width, svg_height;
  const GstStructure *face;
  const GValue *faces_list, *face_val;
  gchar *new_location = NULL;
  gint face_count;

#if 0
  /* optionally draw the image once every two messages for better performance */
  filter->process_message = !filter->process_message;
  if (!filter->process_message)
    return;
#endif

  faces_list = gst_structure_get_value (s, "faces");
  face_count = gst_value_list_get_size (faces_list);
  GST_LOG_OBJECT (filter, "face count: %d", face_count);

  if (face_count == 0) {
    GST_DEBUG_OBJECT (filter, "no face, clearing overlay");
    g_object_set (filter->svg_overlay, "location", NULL, NULL);
    GST_OBJECT_LOCK (filter);
    filter->update_svg = TRUE;
    GST_OBJECT_UNLOCK (filter);
    return;
  }

  /* The last face in the list seems to be the right one, objects mistakenly
   * detected as faces for a couple of frames seem to be in the list
   * beginning. TODO: needs confirmation. */
  face_val = gst_value_list_get_value (faces_list, face_count - 1);
  face = gst_value_get_structure (face_val);
  gst_structure_get_uint (face, "x", &x);
  gst_structure_get_uint (face, "y", &y);
  gst_structure_get_uint (face, "width", &width);
  gst_structure_get_uint (face, "height", &height);

  /* Apply x and y offsets relative to face position and size.
   * Set image width and height as a fraction of face width and height.
   * Cast to int since face position and size will never be bigger than
   * G_MAX_INT and we may have negative values as svg_x or svg_y */

  GST_OBJECT_LOCK (filter);

  svg_x = (gint) x + (gint) (filter->x * width);
  svg_y = (gint) y + (gint) (filter->y * height);

  svg_width = (gint) (filter->w * width);
  svg_height = (gint) (filter->h * height);

  if (filter->update_svg) {
    new_location = g_strdup (filter->location);
    filter->update_svg = FALSE;
  }
  GST_OBJECT_UNLOCK (filter);

  if (new_location != NULL) {
    GST_DEBUG_OBJECT (filter, "set rsvgoverlay location=%s", new_location);
    g_object_set (filter->svg_overlay, "location", new_location, NULL);
    g_free (new_location);
  }

  GST_LOG_OBJECT (filter, "overlay dimensions: %d x %d @ %d,%d",
      svg_width, svg_height, svg_x, svg_y);

  g_object_set (filter->svg_overlay,
      "x", svg_x, "y", svg_y, "width", svg_width, "height", svg_height, NULL);
}

static void
gst_face_overlay_message_handler (GstBin * bin, GstMessage * message)
{
  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT) {
    const GstStructure *s = gst_message_get_structure (message);

    if (gst_structure_has_name (s, "facedetect")) {
      gst_face_overlay_handle_faces (GST_FACEOVERLAY (bin), s);
    }
  }

  GST_BIN_CLASS (parent_class)->handle_message (bin, message);
}

static void
gst_face_overlay_class_init (GstFaceOverlayClass * klass)
{
  GObjectClass *gobject_class;
  GstBinClass *gstbin_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbin_class = GST_BIN_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_face_overlay_set_property;
  gobject_class->get_property = gst_face_overlay_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "Location",
          "Location of SVG file to use for face overlay",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_X,
      g_param_spec_float ("x", "face x offset",
          "Specify image x relative to detected face x.", -G_MAXFLOAT,
          G_MAXFLOAT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_Y,
      g_param_spec_float ("y", "face y offset",
          "Specify image y relative to detected face y.", -G_MAXFLOAT,
          G_MAXFLOAT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_W,
      g_param_spec_float ("w", "face width percent",
          "Specify image width relative to face width.", 0, G_MAXFLOAT, 1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_H,
      g_param_spec_float ("h", "face height percent",
          "Specify image height relative to face height.", 0, G_MAXFLOAT, 1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "faceoverlay",
      "Filter/Editor/Video",
      "Overlays SVG graphics over a detected face in a video stream",
      "Laura Lucas Alday <lauralucas@gmail.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  gstbin_class->handle_message =
      GST_DEBUG_FUNCPTR (gst_face_overlay_message_handler);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_face_overlay_change_state);
}

static void
gst_face_overlay_init (GstFaceOverlay * filter)
{
  GstPadTemplate *tmpl;

  filter->x = 0;
  filter->y = 0;
  filter->w = 1;
  filter->h = 1;
  filter->svg_overlay = NULL;
  filter->location = NULL;
  filter->process_message = TRUE;

  tmpl = gst_static_pad_template_get (&sink_factory);
  filter->sinkpad = gst_ghost_pad_new_no_target_from_template ("sink", tmpl);
  gst_object_unref (tmpl);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  tmpl = gst_static_pad_template_get (&src_factory);
  filter->srcpad = gst_ghost_pad_new_no_target_from_template ("src", tmpl);
  gst_object_unref (tmpl);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  gst_face_overlay_create_children (filter);
}

static void
gst_face_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFaceOverlay *filter = GST_FACEOVERLAY (object);

  switch (prop_id) {
    case PROP_LOCATION:
      GST_OBJECT_LOCK (filter);
      g_free (filter->location);
      filter->location = g_value_dup_string (value);
      filter->update_svg = TRUE;
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_X:
      GST_OBJECT_LOCK (filter);
      filter->x = g_value_get_float (value);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_Y:
      GST_OBJECT_LOCK (filter);
      filter->y = g_value_get_float (value);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_W:
      GST_OBJECT_LOCK (filter);
      filter->w = g_value_get_float (value);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_H:
      GST_OBJECT_LOCK (filter);
      filter->h = g_value_get_float (value);
      GST_OBJECT_UNLOCK (filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_face_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFaceOverlay *filter = GST_FACEOVERLAY (object);

  switch (prop_id) {
    case PROP_LOCATION:
      GST_OBJECT_LOCK (filter);
      g_value_set_string (value, filter->location);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_X:
      GST_OBJECT_LOCK (filter);
      g_value_set_float (value, filter->x);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_Y:
      GST_OBJECT_LOCK (filter);
      g_value_set_float (value, filter->y);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_W:
      GST_OBJECT_LOCK (filter);
      g_value_set_float (value, filter->w);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_H:
      GST_OBJECT_LOCK (filter);
      g_value_set_float (value, filter->h);
      GST_OBJECT_UNLOCK (filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
faceoverlay_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (faceoverlay, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    faceoverlay,
    "SVG Face Overlay",
    faceoverlay_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
