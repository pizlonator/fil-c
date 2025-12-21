/*
 * GStreamer
 * Copyright (C) 2007 David Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
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
 * SECTION:gstglfilter
 * @title: GstGLFilter
 * @short_description: GstBaseTransform subclass for dealing with RGBA textures
 * @see_also: #GstBaseTransform, #GstGLContext, #GstGLFramebuffer
 *
 * #GstGLFilter helps to implement simple OpenGL filter elements taking a
 * single input and producing a single output with a #GstGLFramebuffer
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/gstvideometa.h>

#include "gstglfilter.h"

#include "gstglfuncs.h"

#define GST_CAT_DEFAULT gst_gl_filter_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

/* *INDENT-OFF* */
static GstStaticPadTemplate gst_gl_filter_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
      "format = (string) RGBA, "
      "width = " GST_VIDEO_SIZE_RANGE ", "
      "height = " GST_VIDEO_SIZE_RANGE ", "
      "framerate = " GST_VIDEO_FPS_RANGE ","
      "texture-target = (string) 2D ; "
      "video/x-raw(ANY), "
      "format = (string) RGBA, "
      "width = " GST_VIDEO_SIZE_RANGE ", "
      "height = " GST_VIDEO_SIZE_RANGE ", "
      "framerate = " GST_VIDEO_FPS_RANGE ","
      "texture-target = (string) 2D"
    ));

static GstStaticPadTemplate gst_gl_filter_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY), "
      "format = (string) RGBA, "
      "width = " GST_VIDEO_SIZE_RANGE ", "
      "height = " GST_VIDEO_SIZE_RANGE ", "
      "framerate = " GST_VIDEO_FPS_RANGE ","
      "texture-target = (string) 2D ; "
      "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
      "format = (string) RGBA, "
      "width = " GST_VIDEO_SIZE_RANGE ", "
      "height = " GST_VIDEO_SIZE_RANGE ", "
      "framerate = " GST_VIDEO_FPS_RANGE ","
      "texture-target = (string) 2D"
    ));
/* *INDENT-ON* */

/* Properties */
enum
{
  PROP_0,
};

#define gst_gl_filter_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLFilter, gst_gl_filter, GST_TYPE_GL_BASE_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_gl_filter_debug, "glfilter", 0,
        "glfilter element");
    );

static void gst_gl_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_filter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_gl_filter_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *default_transform_internal_caps (GstGLFilter * filter,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps);
static GstCaps *gst_gl_filter_fixate_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static void gst_gl_filter_reset (GstGLFilter * filter);
static gboolean gst_gl_filter_stop (GstBaseTransform * bt);
static gboolean gst_gl_filter_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, gsize * size);
static GstFlowReturn gst_gl_filter_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_gl_filter_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static gboolean gst_gl_filter_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static gboolean gst_gl_filter_set_caps (GstBaseTransform * bt, GstCaps * incaps,
    GstCaps * outcaps);
static void gst_gl_filter_gl_stop (GstGLBaseFilter * filter);
static gboolean gst_gl_filter_gl_set_caps (GstGLBaseFilter * bt,
    GstCaps * incaps, GstCaps * outcaps);

void
gst_gl_filter_add_rgba_pad_templates (GstGLFilterClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_add_static_pad_template (element_class,
      &gst_gl_filter_src_pad_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_gl_filter_sink_pad_template);
}

static void
gst_gl_filter_class_init (GstGLFilterClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_gl_filter_set_property;
  gobject_class->get_property = gst_gl_filter_get_property;

  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
      gst_gl_filter_transform_caps;
  GST_BASE_TRANSFORM_CLASS (klass)->fixate_caps = gst_gl_filter_fixate_caps;
  GST_BASE_TRANSFORM_CLASS (klass)->transform = gst_gl_filter_transform;
  GST_BASE_TRANSFORM_CLASS (klass)->stop = gst_gl_filter_stop;
  GST_BASE_TRANSFORM_CLASS (klass)->set_caps = gst_gl_filter_set_caps;
  GST_BASE_TRANSFORM_CLASS (klass)->propose_allocation =
      gst_gl_filter_propose_allocation;
  GST_BASE_TRANSFORM_CLASS (klass)->decide_allocation =
      gst_gl_filter_decide_allocation;
  GST_BASE_TRANSFORM_CLASS (klass)->get_unit_size = gst_gl_filter_get_unit_size;

  GST_GL_BASE_FILTER_CLASS (klass)->gl_stop = gst_gl_filter_gl_stop;
  GST_GL_BASE_FILTER_CLASS (klass)->gl_set_caps = gst_gl_filter_gl_set_caps;

  klass->transform_internal_caps = default_transform_internal_caps;
}

static void
gst_gl_filter_init (GstGLFilter * filter)
{
  filter->draw_attr_position_loc = -1;
  filter->draw_attr_texture_loc = -1;
}

static void
gst_gl_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filter_reset (GstGLFilter * filter)
{
  gst_caps_replace (&filter->out_caps, NULL);
}

static gboolean
gst_gl_filter_stop (GstBaseTransform * bt)
{
  GstGLFilter *filter = GST_GL_FILTER (bt);

  gst_gl_filter_reset (filter);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (bt);
}

static void
gst_gl_filter_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLFilter *filter = GST_GL_FILTER (base_filter);
  GstGLContext *context = base_filter->context;
  const GstGLFuncs *gl = context->gl_vtable;

  if (filter->vao) {
    gl->DeleteVertexArrays (1, &filter->vao);
    filter->vao = 0;
  }

  if (filter->vertex_buffer) {
    gl->DeleteBuffers (1, &filter->vertex_buffer);
    filter->vertex_buffer = 0;
  }

  if (filter->vbo_indices) {
    gl->DeleteBuffers (1, &filter->vbo_indices);
    filter->vbo_indices = 0;
  }

  if (filter->fbo != NULL) {
    gst_object_unref (filter->fbo);
    filter->fbo = NULL;
  }

  filter->default_shader = NULL;
  filter->draw_attr_position_loc = -1;
  filter->draw_attr_texture_loc = -1;

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static GstCaps *
gst_gl_filter_fixate_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstStructure *ins, *outs;
  const GValue *from_par, *to_par;
  GValue fpar = { 0, }, tpar = {
    0,
  };

  othercaps = gst_caps_make_writable (othercaps);
  othercaps = gst_caps_truncate (othercaps);

  GST_DEBUG_OBJECT (bt, "trying to fixate othercaps %" GST_PTR_FORMAT
      " based on caps %" GST_PTR_FORMAT, othercaps, caps);

  ins = gst_caps_get_structure (caps, 0);
  outs = gst_caps_get_structure (othercaps, 0);

  from_par = gst_structure_get_value (ins, "pixel-aspect-ratio");
  to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");

  /* If we're fixating from the sinkpad we always set the PAR and
   * assume that missing PAR on the sinkpad means 1/1 and
   * missing PAR on the srcpad means undefined
   */
  if (direction == GST_PAD_SINK) {
    if (!from_par) {
      g_value_init (&fpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&fpar, 1, 1);
      from_par = &fpar;
    }
    if (!to_par) {
      g_value_init (&tpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&tpar, 1, 1);
      to_par = &tpar;
    }
  } else {
    if (!to_par) {
      g_value_init (&tpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&tpar, 1, 1);
      to_par = &tpar;

      gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
          NULL);
    }
    if (!from_par) {
      g_value_init (&fpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&fpar, 1, 1);
      from_par = &fpar;
    }
  }

  /* we have both PAR but they might not be fixated */
  {
    gint from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d;
    gint w = 0, h = 0;
    gint from_dar_n, from_dar_d;
    gint num, den;

    /* from_par should be fixed */
    g_return_val_if_fail (gst_value_is_fixed (from_par), othercaps);

    from_par_n = gst_value_get_fraction_numerator (from_par);
    from_par_d = gst_value_get_fraction_denominator (from_par);

    gst_structure_get_int (ins, "width", &from_w);
    gst_structure_get_int (ins, "height", &from_h);

    gst_structure_get_int (outs, "width", &w);
    gst_structure_get_int (outs, "height", &h);

    /* if both width and height are already fixed, we can't do anything
     * about it anymore */
    if (w && h) {
      GST_DEBUG_OBJECT (bt, "dimensions already set to %dx%d, not fixating",
          w, h);
      if (!gst_value_is_fixed (to_par)) {
        GST_DEBUG_OBJECT (bt, "fixating to_par to %dx%d", 1, 1);
        if (gst_structure_has_field (outs, "pixel-aspect-ratio"))
          gst_structure_fixate_field_nearest_fraction (outs,
              "pixel-aspect-ratio", 1, 1);
      }
      goto done;
    }

    /* Calculate input DAR */
    if (!gst_util_fraction_multiply (from_w, from_h, from_par_n, from_par_d,
            &from_dar_n, &from_dar_d)) {
      GST_ELEMENT_ERROR (bt, CORE, NEGOTIATION, (NULL),
          ("Error calculating the output scaled size - integer overflow"));
      goto done;
    }

    GST_DEBUG_OBJECT (bt, "Input DAR is %d/%d", from_dar_n, from_dar_d);

    /* If either width or height are fixed there's not much we
     * can do either except choosing a height or width and PAR
     * that matches the DAR as good as possible
     */
    if (h) {
      gint num, den;

      GST_DEBUG_OBJECT (bt, "height is fixed (%d)", h);

      if (!gst_value_is_fixed (to_par)) {
        /* (shortcut) copy-paste (??) of videoscale seems to aim for 1/1,
         * so let's make it so ...
         * especially if following code assumes fixed */
        GST_DEBUG_OBJECT (bt, "fixating to_par to 1x1");
        gst_structure_fixate_field_nearest_fraction (outs,
            "pixel-aspect-ratio", 1, 1);
        to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");
      }

      /* PAR is fixed, choose the height that is nearest to the
       * height with the same DAR */
      to_par_n = gst_value_get_fraction_numerator (to_par);
      to_par_d = gst_value_get_fraction_denominator (to_par);

      GST_DEBUG_OBJECT (bt, "PAR is fixed %d/%d", to_par_n, to_par_d);

      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
              to_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (bt, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      w = (guint) gst_util_uint64_scale_int (h, num, den);
      gst_structure_fixate_field_nearest_int (outs, "width", w);

      goto done;
    } else if (w) {
      gint num, den;

      GST_DEBUG_OBJECT (bt, "width is fixed (%d)", w);

      if (!gst_value_is_fixed (to_par)) {
        /* (shortcut) copy-paste (??) of videoscale seems to aim for 1/1,
         * so let's make it so ...
         * especially if following code assumes fixed */
        GST_DEBUG_OBJECT (bt, "fixating to_par to 1x1");
        gst_structure_fixate_field_nearest_fraction (outs,
            "pixel-aspect-ratio", 1, 1);
        to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");
      }

      /* PAR is fixed, choose the height that is nearest to the
       * height with the same DAR */
      to_par_n = gst_value_get_fraction_numerator (to_par);
      to_par_d = gst_value_get_fraction_denominator (to_par);

      GST_DEBUG_OBJECT (bt, "PAR is fixed %d/%d", to_par_n, to_par_d);

      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
              to_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (bt, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      h = (guint) gst_util_uint64_scale_int (w, den, num);
      gst_structure_fixate_field_nearest_int (outs, "height", h);

      goto done;
    } else if (gst_value_is_fixed (to_par)) {
      GstStructure *tmp;
      gint set_h, set_w, f_h, f_w;

      to_par_n = gst_value_get_fraction_numerator (to_par);
      to_par_d = gst_value_get_fraction_denominator (to_par);

      /* Calculate scale factor for the PAR change */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_n,
              to_par_d, &num, &den)) {
        GST_ELEMENT_ERROR (bt, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      /* Try to keep the input height */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);

      /* This might have failed but try to scale the width
       * to keep the DAR nonetheless */
      w = (guint) gst_util_uint64_scale_int (set_h, num, den);
      gst_structure_fixate_field_nearest_int (tmp, "width", w);
      gst_structure_get_int (tmp, "width", &set_w);
      gst_structure_free (tmp);

      /* We kept the DAR and the height is nearest to the original height */
      if (set_w == w) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);
        goto done;
      }

      f_h = set_h;
      f_w = set_w;

      /* If the former failed, try to keep the input width at least */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
      gst_structure_get_int (tmp, "width", &set_w);

      /* This might have failed but try to scale the width
       * to keep the DAR nonetheless */
      h = (guint) gst_util_uint64_scale_int (set_w, den, num);
      gst_structure_fixate_field_nearest_int (tmp, "height", h);
      gst_structure_get_int (tmp, "height", &set_h);
      gst_structure_free (tmp);

      /* We kept the DAR and the width is nearest to the original width */
      if (set_h == h) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);
        goto done;
      }

      /* If all this failed, keep the height that was nearest to the original
       * height and the nearest possible width. This changes the DAR but
       * there's not much else to do here.
       */
      gst_structure_set (outs, "width", G_TYPE_INT, f_w, "height", G_TYPE_INT,
          f_h, NULL);
      goto done;
    } else {
      GstStructure *tmp;
      gint set_h, set_w, set_par_n, set_par_d, tmp2;

      /* width, height and PAR are not fixed */

      /* First try to keep the height and width as good as possible
       * and scale PAR */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);
      gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
      gst_structure_get_int (tmp, "width", &set_w);

      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_h, set_w,
              &to_par_n, &to_par_d)) {
        GST_ELEMENT_ERROR (bt, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        gst_structure_free (tmp);
        goto done;
      }

      if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
        gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
      gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
          to_par_n, to_par_d);
      gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
          &set_par_d);
      gst_structure_free (tmp);

      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);

        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* Otherwise try to scale width to keep the DAR with the set
       * PAR and height */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
              set_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (bt, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      w = (guint) gst_util_uint64_scale_int (set_h, num, den);
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "width", w);
      gst_structure_get_int (tmp, "width", &tmp2);
      gst_structure_free (tmp);

      if (tmp2 == w) {
        gst_structure_set (outs, "width", G_TYPE_INT, tmp2, "height",
            G_TYPE_INT, set_h, NULL);
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* ... or try the same with the height */
      h = (guint) gst_util_uint64_scale_int (set_w, den, num);
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", h);
      gst_structure_get_int (tmp, "height", &tmp2);
      gst_structure_free (tmp);

      if (tmp2 == h) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, tmp2, NULL);
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* If all fails we can't keep the DAR and take the nearest values
       * for everything from the first try */
      gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
          G_TYPE_INT, set_h, NULL);
      if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
          set_par_n != set_par_d)
        gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
            set_par_n, set_par_d, NULL);
    }
  }


done:
  othercaps = gst_caps_fixate (othercaps);

  GST_DEBUG_OBJECT (bt, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);

  if (from_par == &fpar)
    g_value_unset (&fpar);
  if (to_par == &tpar)
    g_value_unset (&tpar);

  return othercaps;
}

/* copies the given caps */
static GstCaps *
gst_gl_filter_caps_remove_size (GstCaps * caps)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;

  res = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    st = gst_caps_get_structure (caps, i);
    f = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (res, st, f))
      continue;

    st = gst_structure_copy (st);
    gst_structure_set (st, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
        "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);

    /* if pixel aspect ratio, make a range of it */
    if (gst_structure_has_field (st, "pixel-aspect-ratio")) {
      gst_structure_set (st, "pixel-aspect-ratio",
          GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1, NULL);
    }

    gst_caps_append_structure_full (res, st, gst_caps_features_copy (f));
  }

  return res;
}

static GstCaps *
gst_gl_filter_ensure_caps_contains_features (const GstCaps * caps,
    const gchar * feature_name)
{
  GstCaps *ret = gst_caps_copy (caps);
  guint n = gst_caps_get_size (ret);
  guint i = 0;

  for (i = 0; i < n; i++) {
    GstCapsFeatures *f = gst_caps_get_features (ret, i);
    if (!gst_caps_features_is_any (f)) {
      if (!gst_caps_features_contains (f, feature_name)) {
        gst_caps_features_add (f, GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
      }
    }
  }

  return ret;
}

static GstCaps *
default_transform_internal_caps (GstGLFilter * filter,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  GstCaps *tmp = gst_gl_filter_caps_remove_size (caps);

  GST_DEBUG_OBJECT (filter, "size removal returned caps %" GST_PTR_FORMAT, tmp);
  return tmp;
}

static GstCaps *
gst_gl_filter_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  GstGLFilter *filter = GST_GL_FILTER (bt);
  GstCaps *tmp = NULL;
  GstCaps *result = NULL;

  if (gst_base_transform_is_passthrough (bt)) {
    tmp = gst_caps_ref (caps);
  } else {
    tmp = GST_GL_FILTER_GET_CLASS (filter)->transform_internal_caps (filter,
        direction, caps, NULL);

    result =
        gst_gl_filter_ensure_caps_contains_features (tmp,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
    gst_caps_unref (tmp);
    tmp = result;
  }

  if (filter_caps) {
    result =
        gst_caps_intersect_full (filter_caps, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  GST_DEBUG_OBJECT (bt, "returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_gl_filter_get_unit_size (GstBaseTransform * trans, GstCaps * caps,
    gsize * size)
{
  gboolean ret = FALSE;
  GstVideoInfo info;

  ret = gst_video_info_from_caps (&info, caps);
  if (ret)
    *size = GST_VIDEO_INFO_SIZE (&info);

  return TRUE;
}

static gboolean
gst_gl_filter_gl_set_caps (GstGLBaseFilter * bt, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstGLFilter *filter = GST_GL_FILTER (bt);
  GstGLFilterClass *filter_class = GST_GL_FILTER_GET_CLASS (filter);
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;
  gint out_width, out_height;

  out_width = GST_VIDEO_INFO_WIDTH (&filter->out_info);
  out_height = GST_VIDEO_INFO_HEIGHT (&filter->out_info);

  if (filter->fbo)
    gst_object_unref (filter->fbo);

  if (!(filter->fbo =
          gst_gl_framebuffer_new_with_default_depth (context, out_width,
              out_height)))
    goto context_error;

  if (filter_class->init_fbo) {
    if (!filter_class->init_fbo (filter))
      goto error;
  }

  return TRUE;

context_error:
  {
    GST_ELEMENT_ERROR (filter, RESOURCE, NOT_FOUND, ("Could not generate FBO"),
        (NULL));
    return FALSE;
  }
error:
  {
    GST_ELEMENT_ERROR (filter, LIBRARY, INIT,
        ("Subclass failed to initialize."), (NULL));
    return FALSE;
  }
}

static gboolean
gst_gl_filter_set_caps (GstBaseTransform * bt, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstGLFilter *filter;
  GstGLFilterClass *filter_class;
  GstGLTextureTarget from_target, to_target;

  filter = GST_GL_FILTER (bt);
  filter_class = GST_GL_FILTER_GET_CLASS (filter);

  if (!gst_video_info_from_caps (&filter->in_info, incaps))
    goto wrong_caps;
  if (!gst_video_info_from_caps (&filter->out_info, outcaps))
    goto wrong_caps;

  {
    GstStructure *in_s = gst_caps_get_structure (incaps, 0);
    GstStructure *out_s = gst_caps_get_structure (outcaps, 0);

    if (gst_structure_has_field_typed (in_s, "texture-target", G_TYPE_STRING))
      from_target =
          gst_gl_texture_target_from_string (gst_structure_get_string (in_s,
              "texture-target"));
    else
      from_target = GST_GL_TEXTURE_TARGET_2D;

    if (gst_structure_has_field_typed (out_s, "texture-target", G_TYPE_STRING))
      to_target =
          gst_gl_texture_target_from_string (gst_structure_get_string (out_s,
              "texture-target"));
    else
      to_target = GST_GL_TEXTURE_TARGET_2D;

    if (to_target == GST_GL_TEXTURE_TARGET_NONE
        || from_target == GST_GL_TEXTURE_TARGET_NONE)
      /* invalid caps */
      goto wrong_caps;
  }

  if (filter_class->set_caps) {
    if (!filter_class->set_caps (filter, incaps, outcaps))
      goto error;
  }

  gst_caps_replace (&filter->out_caps, outcaps);
  filter->in_texture_target = from_target;
  filter->out_texture_target = to_target;

  GST_DEBUG_OBJECT (filter, "set_caps %dx%d in %" GST_PTR_FORMAT
      " out %" GST_PTR_FORMAT,
      GST_VIDEO_INFO_WIDTH (&filter->out_info),
      GST_VIDEO_INFO_HEIGHT (&filter->out_info), incaps, outcaps);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->set_caps (bt, incaps,
      outcaps);

/* ERRORS */
wrong_caps:
  {
    GST_WARNING ("Wrong caps - could not understand input or output caps");
    return FALSE;
  }
error:
  {
    return FALSE;
  }
}

static gboolean
gst_gl_filter_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstGLFilter *filter = GST_GL_FILTER (trans);
  GstGLContext *context =
      gst_gl_base_filter_get_gl_context (GST_GL_BASE_FILTER (filter));
  if (context == NULL) {
    return FALSE;
  }

  GstCaps *caps;
  GstVideoInfo info;
  guint size;
  GstBufferPool *pool = NULL;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  /* the normal size of a frame */
  size = info.size;

  if (need_pool) {
    GstStructure *config;

    GST_DEBUG_OBJECT (filter, "create new pool");
    pool = gst_gl_buffer_pool_new (context);

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config)) {
      gst_object_unref (pool);
      goto config_failed;
    }
  }

  gst_query_add_allocation_pool (query, pool, size, 1, 0);
  if (pool)
    gst_object_unref (pool);

  if (context->gl_vtable->FenceSync)
    gst_query_add_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, 0);

  gst_object_unref (context);
  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_DEBUG_OBJECT (trans, "no caps specified");
    gst_object_unref (context);
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (trans, "invalid caps specified");
    gst_object_unref (context);
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (trans, "failed setting config");
    gst_object_unref (context);
    return FALSE;
  }
}

static gboolean
gst_gl_filter_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  guint min, max, size;
  gboolean update_pool;

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps)
    return FALSE;

  /* get gl context */
  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
          query))
    return FALSE;

  GstGLContext *context =
      gst_gl_base_filter_get_gl_context (GST_GL_BASE_FILTER (trans));
  if (context == NULL) {
    return FALSE;
  }

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    update_pool = TRUE;
  } else {
    GstVideoInfo vinfo;

    gst_video_info_init (&vinfo);
    gst_video_info_from_caps (&vinfo, caps);
    size = vinfo.size;
    min = max = 0;
    update_pool = FALSE;
  }

  if (!pool || !GST_IS_GL_BUFFER_POOL (pool)) {
    if (pool)
      gst_object_unref (pool);
    pool = gst_gl_buffer_pool_new (context);
  }

  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  if (gst_query_find_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, NULL))
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_GL_SYNC_META);

  gst_buffer_pool_set_config (pool, config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);
  gst_object_unref (context);

  return TRUE;
}

/**
 * gst_gl_filter_filter_texture:
 * @filter: a #GstGLFilter
 * @input: an input buffer
 * @output: an output buffer
 *
 * Calls filter_texture vfunc with correctly mapped #GstGLMemorys
 *
 * Returns: whether the transformation succeeded
 *
 * Since: 1.4
 */
gboolean
gst_gl_filter_filter_texture (GstGLFilter * filter, GstBuffer * input,
    GstBuffer * output)
{
  GstGLFilterClass *filter_class;
  GstMemory *in_tex, *out_tex;
  GstVideoFrame gl_frame, out_frame;
  gboolean ret;

  filter_class = GST_GL_FILTER_GET_CLASS (filter);

  if (!gst_video_frame_map (&gl_frame, &filter->in_info, input,
          GST_MAP_READ | GST_MAP_GL)) {
    ret = FALSE;
    goto inbuf_error;
  }

  in_tex = gl_frame.map[0].memory;
  if (!gst_is_gl_memory (in_tex)) {
    ret = FALSE;
    GST_ERROR_OBJECT (filter, "Input memory must be GstGLMemory");
    goto unmap_out_error;
  }

  if (!gst_video_frame_map (&out_frame, &filter->out_info, output,
          GST_MAP_WRITE | GST_MAP_GL)) {
    ret = FALSE;
    goto unmap_out_error;
  }

  out_tex = out_frame.map[0].memory;
  g_return_val_if_fail (gst_is_gl_memory (out_tex), FALSE);

  GST_DEBUG ("calling filter_texture with textures in:%i out:%i",
      GST_GL_MEMORY_CAST (in_tex)->tex_id,
      GST_GL_MEMORY_CAST (out_tex)->tex_id);

  g_assert (filter_class->filter_texture);

  ret = filter_class->filter_texture (filter, GST_GL_MEMORY_CAST (in_tex),
      GST_GL_MEMORY_CAST (out_tex));

  gst_video_frame_unmap (&out_frame);
unmap_out_error:
  gst_video_frame_unmap (&gl_frame);
inbuf_error:

  return ret;
}

static void
_filter_gl (GstGLContext * context, GstGLFilter * filter)
{
  GstGLFilterClass *filter_class = GST_GL_FILTER_GET_CLASS (filter);

  gst_gl_insert_debug_marker (context,
      "processing in element %s", GST_OBJECT_NAME (filter));

  if (filter_class->filter)
    filter->gl_result =
        filter_class->filter (filter, filter->inbuf, filter->outbuf);
  else
    filter->gl_result =
        gst_gl_filter_filter_texture (filter, filter->inbuf, filter->outbuf);
}

static GstFlowReturn
gst_gl_filter_transform (GstBaseTransform * bt, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstGLFilter *filter = GST_GL_FILTER (bt);
  GstGLFilterClass *filter_class = GST_GL_FILTER_GET_CLASS (bt);
  GstGLDisplay *display = GST_GL_BASE_FILTER (bt)->display;
  GstGLContext *context = GST_GL_BASE_FILTER (bt)->context;
  GstGLSyncMeta *out_sync_meta, *in_sync_meta;
  gboolean ret;

  if (!display)
    return GST_FLOW_NOT_NEGOTIATED;

  g_assert (filter_class->filter || filter_class->filter_texture);

  in_sync_meta = gst_buffer_get_gl_sync_meta (inbuf);
  if (in_sync_meta)
    gst_gl_sync_meta_wait (in_sync_meta, context);

  filter->inbuf = inbuf;
  filter->outbuf = outbuf;
  gst_gl_context_thread_add (context, (GstGLContextThreadFunc) _filter_gl,
      filter);
  ret = filter->gl_result;

  out_sync_meta = gst_buffer_get_gl_sync_meta (outbuf);
  if (out_sync_meta)
    gst_gl_sync_meta_set_sync_point (out_sync_meta, context);

  /* since gl api is async operation, when return from transform()
   * function, basetransform will unref input buffer immidiately,
   * but gpu may still reading input buffer for rendering. Add parent
   * buffer meta to hold one reference of inbuf, this can avoid this
   * buffer sync problem.
   */
  if (inbuf != outbuf)
    gst_buffer_add_parent_buffer_meta (outbuf, inbuf);

  return ret ? GST_FLOW_OK : GST_FLOW_ERROR;
}

struct glcb
{
  GstGLFilter *filter;
  GstGLFilterRenderFunc func;
  GstGLMemory *in_tex;
  gpointer data;
};

static gboolean
_glcb (gpointer data)
{
  struct glcb *cb = data;

  return cb->func (cb->filter, cb->in_tex, cb->data);
}

/**
 * gst_gl_filter_render_to_target:
 * @filter: a #GstGLFilter
 * @input: the input texture
 * @output: the output texture
 * @func: (scope call): the function to transform @input into @output. called with @data
 * @data: (allow-none): the data associated with @func
 *
 * Transforms @input into @output using @func on through FBO.
 *
 * Returns: the return value of @func
 *
 * Since: 1.10
 */
gboolean
gst_gl_filter_render_to_target (GstGLFilter * filter, GstGLMemory * input,
    GstGLMemory * output, GstGLFilterRenderFunc func, gpointer data)
{
  struct glcb cb;

  cb.filter = filter;
  cb.func = func;
  cb.in_tex = input;
  cb.data = data;

  return gst_gl_framebuffer_draw_to_texture (filter->fbo, output, _glcb, &cb);
}

static void
_get_attributes (GstGLFilter * filter)
{
  if (!filter->default_shader)
    return;

  if (filter->valid_attributes)
    return;

  if (filter->draw_attr_position_loc == -1)
    filter->draw_attr_position_loc =
        gst_gl_shader_get_attribute_location (filter->default_shader,
        "a_position");

  if (filter->draw_attr_texture_loc == -1)
    filter->draw_attr_texture_loc =
        gst_gl_shader_get_attribute_location (filter->default_shader,
        "a_texcoord");

  filter->valid_attributes = TRUE;
}

static gboolean
_draw_with_shader_cb (GstGLFilter * filter, GstGLMemory * in_tex,
    gpointer unused)
{
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;
  GstGLFuncs *gl = context->gl_vtable;
  guint gl_target;

#if GST_GL_HAVE_OPENGL
  if (gst_gl_context_get_gl_api (context) & GST_GL_API_OPENGL) {
    gl->MatrixMode (GL_PROJECTION);
    gl->LoadIdentity ();
  }
#endif

  _get_attributes (filter);
  gst_gl_shader_use (filter->default_shader);
  gl_target = gst_gl_texture_target_to_gl (filter->in_texture_target);

  gl->ActiveTexture (GL_TEXTURE1);
  gl->BindTexture (gl_target, gst_gl_memory_get_texture_id (in_tex));

  gst_gl_shader_set_uniform_1i (filter->default_shader, "tex", 1);
  gst_gl_shader_set_uniform_1f (filter->default_shader, "width",
      GST_VIDEO_INFO_WIDTH (&filter->out_info));
  gst_gl_shader_set_uniform_1f (filter->default_shader, "height",
      GST_VIDEO_INFO_HEIGHT (&filter->out_info));

  gst_gl_filter_draw_fullscreen_quad (filter);

  return TRUE;
}

/**
 * gst_gl_filter_render_to_target_with_shader:
 * @filter: a #GstGLFilter
 * @input: the input texture
 * @output: the output texture
 * @shader: the shader to use.
 *
 * Transforms @input into @output using @shader with a FBO.
 *
 * See also: gst_gl_filter_render_to_target()
 *
 * Since: 1.4
 */
/* attach target to a FBO, use shader, pass input as "tex" uniform to
 * the shader, render input to a quad */
void
gst_gl_filter_render_to_target_with_shader (GstGLFilter * filter,
    GstGLMemory * input, GstGLMemory * output, GstGLShader * shader)
{
  if (filter->default_shader != shader)
    filter->valid_attributes = FALSE;
  filter->default_shader = shader;

  gst_gl_filter_render_to_target (filter, input, output, _draw_with_shader_cb,
      NULL);
}

/* *INDENT-OFF* */
static const GLfloat vertices[] = {
  -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
   1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
   1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
  -1.0f,  1.0f, 0.0f, 0.0f, 1.0f
};

static const GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
/* *INDENT-ON* */

static void
_bind_buffer (GstGLFilter * filter)
{
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;
  const GstGLFuncs *gl = context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, filter->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, filter->vertex_buffer);

  _get_attributes (filter);
  /* Load the vertex position */
  gl->VertexAttribPointer (filter->draw_attr_position_loc, 3, GL_FLOAT,
      GL_FALSE, 5 * sizeof (GLfloat), (void *) 0);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (filter->draw_attr_texture_loc, 2, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));


  gl->EnableVertexAttribArray (filter->draw_attr_position_loc);
  gl->EnableVertexAttribArray (filter->draw_attr_texture_loc);
}

static void
_unbind_buffer (GstGLFilter * filter)
{
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;
  const GstGLFuncs *gl = context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (filter->draw_attr_position_loc);
  gl->DisableVertexAttribArray (filter->draw_attr_texture_loc);
}

/**
 * gst_gl_filter_draw_fullscreen_quad:
 * @filter: a #GstGLFilter
 *
 * Render a fullscreen quad using the current GL state.  The only GL state this
 * modifies is the necessary vertex/index buffers and, if necessary, a
 * Vertex Array Object for drawing a fullscreen quad.  Framebuffer state,
 * any shaders, viewport state, etc must be setup by the caller.
 *
 * Since: 1.10
 */
void
gst_gl_filter_draw_fullscreen_quad (GstGLFilter * filter)
{
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;
  GstGLFuncs *gl = context->gl_vtable;

  {
    if (!filter->vertex_buffer) {
      if (gl->GenVertexArrays) {
        gl->GenVertexArrays (1, &filter->vao);
        gl->BindVertexArray (filter->vao);
      }

      gl->GenBuffers (1, &filter->vertex_buffer);
      gl->BindBuffer (GL_ARRAY_BUFFER, filter->vertex_buffer);
      gl->BufferData (GL_ARRAY_BUFFER, 4 * 5 * sizeof (GLfloat), vertices,
          GL_STATIC_DRAW);

      gl->GenBuffers (1, &filter->vbo_indices);
      gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, filter->vbo_indices);
      gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
          GL_STATIC_DRAW);
    }

    if (gl->GenVertexArrays)
      gl->BindVertexArray (filter->vao);
    _bind_buffer (filter);

    gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    if (gl->GenVertexArrays)
      gl->BindVertexArray (0);
    else
      _unbind_buffer (filter);
  }
}
