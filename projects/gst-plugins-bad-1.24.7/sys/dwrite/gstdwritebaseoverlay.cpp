/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/d3d11/gstd3d11-private.h>
#include "gstdwritebaseoverlay.h"
#include "gstdwriteoverlayobject.h"
#include "gstdwrite-effect.h"
#include <wrl.h>
#include <mutex>

GST_DEBUG_CATEGORY_STATIC (dwrite_base_overlay_debug);
#define GST_CAT_DEFAULT dwrite_base_overlay_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)")
    );

enum
{
  PROP_0,
  PROP_VISIBLE,
  PROP_FONT_FAMILY,
  PROP_FONT_SIZE,
  PROP_AUTO_RESIZE,
  PROP_FONT_WEIGHT,
  PROP_FONT_STYLE,
  PROP_FONT_STRETCH,
  PROP_TEXT,
  PROP_COLOR,
  PROP_OUTLINE_COLOR,
  PROP_SHADOW_COLOR,
  PROP_BACKGROUND_COLOR,
  PROP_LAYOUT_X,
  PROP_LAYOUT_Y,
  PROP_LAYOUT_WIDTH,
  PROP_LAYOUT_HEIGHT,
  PROP_TEXT_ALIGNMENT,
  PROP_PARAGRAPH_ALIGNMENT,
  PROP_ENABLE_COLOR_FONT,
};

/* *INDENT-OFF* */
static std::vector <GParamSpec *> _pspec;
/* *INDENT-ON* */

#define DEFAULT_VISIBLE TRUE
#define DEFAULT_FONT_FAMILY "MS Reference Sans Serif"
#define DEFAULT_FONT_SIZE 24
#define DEFAULT_REFERENCE_FRAME_SIZE 640
#define DEFAULT_AUTO_RESIZE TRUE
#define DEFAULT_FONT_WEIGHT DWRITE_FONT_WEIGHT_NORMAL
#define DEFAULT_FONT_STYLE DWRITE_FONT_STYLE_NORMAL
#define DEFAULT_FONT_STRETCH DWRITE_FONT_STRETCH_NORMAL
#define DEFAULT_FOREGROUND_COLOR 0xffffffff
#define DEFAULT_OUTLINE_COLOR 0xff000000
#define DEFAULT_SHADOW_COLOR 0x80000000
#define DEFAULT_BACKGROUND_COLOR 0x0
#define DEFAULT_LAYOUT_XY 0.03f
#define DEFAULT_LAYOUT_WH 0.92f
#define DEFAULT_TEXT_ALIGNMENT DWRITE_TEXT_ALIGNMENT_LEADING
#define DEFAULT_PARAGRAPH_ALIGNMENT DWRITE_PARAGRAPH_ALIGNMENT_NEAR
#define DEFAULT_COLOR_FONT TRUE

/* *INDENT-OFF* */
struct _GstDWriteBaseOverlayPrivate
{
  _GstDWriteBaseOverlayPrivate ()
  {
    overlay = gst_dwrite_overlay_object_new ();
  }

  ~_GstDWriteBaseOverlayPrivate ()
  {
    gst_clear_object (&overlay);
  }

  std::mutex prop_lock;

  GstDWriteOverlayObject *overlay = nullptr;
  ComPtr<IDWriteFactory> dwrite_factory;
  ComPtr<IDWriteTextFormat> text_format;
  ComPtr<IDWriteTextLayout> layout;

  D2D_POINT_2F layout_origin;
  D2D_POINT_2F layout_size;

  std::wstring prev_text;
  std::wstring cur_text;

  GstDWriteBlendMode blend_mode = GstDWriteBlendMode::NOT_SUPPORTED;

  /* properties */
  gboolean visible = DEFAULT_VISIBLE;
  std::string font_family = DEFAULT_FONT_FAMILY;
  gfloat font_size = DEFAULT_FONT_SIZE;
  gboolean auto_resize = DEFAULT_AUTO_RESIZE;
  DWRITE_FONT_WEIGHT font_weight = DEFAULT_FONT_WEIGHT;
  DWRITE_FONT_STYLE font_style = DEFAULT_FONT_STYLE;
  DWRITE_FONT_STRETCH font_stretch = DEFAULT_FONT_STRETCH;

  std::wstring default_text;
  guint foreground_color = DEFAULT_FOREGROUND_COLOR;
  guint outline_color = DEFAULT_OUTLINE_COLOR;
  guint shadow_color = DEFAULT_SHADOW_COLOR;
  guint background_color = DEFAULT_BACKGROUND_COLOR;

  gdouble layout_x = DEFAULT_LAYOUT_XY;
  gdouble layout_y = DEFAULT_LAYOUT_XY;
  gdouble layout_width = DEFAULT_LAYOUT_WH;
  gdouble layout_height = DEFAULT_LAYOUT_WH;
  DWRITE_TEXT_ALIGNMENT text_align = DEFAULT_TEXT_ALIGNMENT;
  DWRITE_PARAGRAPH_ALIGNMENT paragraph_align = DEFAULT_PARAGRAPH_ALIGNMENT;

  gboolean color_font = FALSE;
};
/* *INDENT-ON* */

static void gst_dwrite_base_overlay_finalize (GObject * object);
static void gst_dwrite_base_overlay_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dwrite_base_overlay_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_dwrite_base_overlay_set_context (GstElement * elem,
    GstContext * context);
static gboolean gst_dwrite_base_overlay_start (GstBaseTransform * trans);
static gboolean gst_dwrite_base_overlay_stop (GstBaseTransform * trans);
static gboolean gst_dwrite_base_overlay_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);
static gboolean gst_dwrite_base_overlay_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean
gst_dwrite_base_overlay_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static gboolean
gst_dwrite_base_overlay_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static GstCaps *gst_dwrite_base_overlay_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static void gst_dwrite_base_overlay_before_transform (GstBaseTransform * trans,
    GstBuffer * buf);
static GstFlowReturn
gst_dwrite_base_overlay_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer ** outbuf);
static GstFlowReturn gst_dwrite_base_overlay_transform (GstBaseTransform *
    trans, GstBuffer * inbuf, GstBuffer * outbuf);

#define gst_dwrite_base_overlay_parent_class parent_class
G_DEFINE_TYPE (GstDWriteBaseOverlay, gst_dwrite_base_overlay,
    GST_TYPE_BASE_TRANSFORM);

static void
gst_dwrite_base_overlay_class_init (GstDWriteBaseOverlayClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  object_class->finalize = gst_dwrite_base_overlay_finalize;
  object_class->set_property = gst_dwrite_base_overlay_set_property;
  object_class->get_property = gst_dwrite_base_overlay_get_property;

  gst_dwrite_base_overlay_build_param_specs (_pspec);
  for (guint i = 0; i < (guint) _pspec.size (); i++)
    g_object_class_install_property (object_class, i + 1, _pspec[i]);

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_set_context);

  trans_class->start = GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_start);
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_stop);
  trans_class->query = GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_query);
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_set_caps);
  trans_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_decide_allocation);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_propose_allocation);
  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_transform_caps);
  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_before_transform);
  trans_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_prepare_output_buffer);
  trans_class->transform =
      GST_DEBUG_FUNCPTR (gst_dwrite_base_overlay_transform);

  GST_DEBUG_CATEGORY_INIT (dwrite_base_overlay_debug,
      "dwritebaseoverlay", 0, "dwritebaseoverlay");

  gst_type_mark_as_plugin_api (GST_TYPE_DWRITE_BASE_OVERLAY,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_DWRITE_FONT_WEIGHT,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_DWRITE_FONT_STYLE,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_DWRITE_FONT_STRETCH,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_DWRITE_TEXT_ALIGNMENT,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_DWRITE_PARAGRAPH_ALIGNMENT,
      (GstPluginAPIFlags) 0);
}

static void
gst_dwrite_base_overlay_init (GstDWriteBaseOverlay * self)
{
  GstDWriteBaseOverlayPrivate *priv = self->priv;

  self->priv = priv = new GstDWriteBaseOverlayPrivate ();
  if (gst_dwrite_is_windows_10_or_greater ())
    priv->color_font = DEFAULT_COLOR_FONT;
}

static void
gst_dwrite_base_overlay_finalize (GObject * object)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dwrite_base_overlay_clear_layout (GstDWriteBaseOverlay * self)
{
  GstDWriteBaseOverlayPrivate *priv = self->priv;

  priv->layout = nullptr;
}

static void
update_uint (GstDWriteBaseOverlay * self, guint * prev, const GValue * value)
{
  guint val = g_value_get_uint (value);
  if (val != *prev) {
    *prev = val;
    gst_dwrite_base_overlay_clear_layout (self);
  }
}

static void
update_double (GstDWriteBaseOverlay * self, gdouble * prev,
    const GValue * value)
{
  gdouble val = g_value_get_double (value);
  if (val != *prev) {
    *prev = val;
    gst_dwrite_base_overlay_clear_layout (self);
  }
}

static void
update_enum (GstDWriteBaseOverlay * self, gint * prev, const GValue * value)
{
  gint val = g_value_get_enum (value);
  if (val != *prev) {
    *prev = val;
    gst_dwrite_base_overlay_clear_layout (self);
  }
}

static void
gst_dwrite_base_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (object);
  GstDWriteBaseOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->prop_lock);

  switch (prop_id) {
    case PROP_VISIBLE:
      priv->visible = g_value_get_boolean (value);
      break;
    case PROP_FONT_FAMILY:{
      const gchar *font_family = g_value_get_string (value);
      std::string font;

      if (font_family)
        font = font_family;
      else
        font = DEFAULT_FONT_FAMILY;

      if (font != priv->font_family) {
        priv->font_family = font;
        priv->text_format = nullptr;
        gst_dwrite_base_overlay_clear_layout (self);
      }
      break;
    }
    case PROP_FONT_SIZE:
    {
      gfloat font_size = g_value_get_float (value);
      if (font_size != priv->font_size) {
        priv->font_size = font_size;
        priv->text_format = nullptr;
        gst_dwrite_base_overlay_clear_layout (self);
      }
      break;
    }
    case PROP_AUTO_RESIZE:
    {
      gboolean auto_resize = g_value_get_uint (value);
      if (auto_resize != priv->auto_resize) {
        priv->auto_resize = auto_resize;
        priv->text_format = nullptr;
        gst_dwrite_base_overlay_clear_layout (self);
      }
      break;
    }
    case PROP_FONT_WEIGHT:
    {
      DWRITE_FONT_WEIGHT weight = (DWRITE_FONT_WEIGHT) g_value_get_enum (value);
      if (weight != priv->font_weight) {
        priv->font_weight = weight;
        priv->text_format = nullptr;
        gst_dwrite_base_overlay_clear_layout (self);
      }
      break;
    }
    case PROP_FONT_STYLE:
    {
      DWRITE_FONT_STYLE style = (DWRITE_FONT_STYLE) g_value_get_enum (value);
      if (style != priv->font_style) {
        priv->font_style = style;
        priv->text_format = nullptr;
        gst_dwrite_base_overlay_clear_layout (self);
      }
      break;
    }
    case PROP_FONT_STRETCH:
    {
      DWRITE_FONT_STRETCH stretch =
          (DWRITE_FONT_STRETCH) g_value_get_enum (value);
      if (stretch != priv->font_stretch) {
        priv->font_stretch = stretch;
        priv->text_format = nullptr;
        gst_dwrite_base_overlay_clear_layout (self);
      }
      break;
    }
    case PROP_TEXT:
    {
      const gchar *new_text = g_value_get_string (value);
      std::wstring new_string;

      if (new_text)
        new_string = gst_dwrite_string_to_wstring (new_text);

      if (priv->default_text != new_string) {
        priv->default_text = new_string;
        gst_dwrite_base_overlay_clear_layout (self);
      }
      break;
    }
    case PROP_COLOR:
      update_uint (self, &priv->foreground_color, value);
      break;
    case PROP_OUTLINE_COLOR:
      update_uint (self, &priv->outline_color, value);
      break;
    case PROP_SHADOW_COLOR:
      update_uint (self, &priv->shadow_color, value);
      break;
    case PROP_BACKGROUND_COLOR:
      update_uint (self, &priv->background_color, value);
      break;
    case PROP_LAYOUT_X:
      update_double (self, &priv->layout_x, value);
      break;
    case PROP_LAYOUT_Y:
      update_double (self, &priv->layout_y, value);
      break;
    case PROP_LAYOUT_WIDTH:
      update_double (self, &priv->layout_width, value);
      break;
    case PROP_LAYOUT_HEIGHT:
      update_double (self, &priv->layout_height, value);
      break;
    case PROP_TEXT_ALIGNMENT:
      update_enum (self, (gint *) & priv->text_align, value);
      break;
    case PROP_PARAGRAPH_ALIGNMENT:
      update_enum (self, (gint *) & priv->paragraph_align, value);
      break;
    case PROP_ENABLE_COLOR_FONT:
      priv->color_font = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dwrite_base_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (object);
  GstDWriteBaseOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->prop_lock);

  switch (prop_id) {
    case PROP_VISIBLE:
      g_value_set_boolean (value, priv->visible);
      break;
    case PROP_FONT_FAMILY:
      g_value_set_string (value, priv->font_family.c_str ());
      break;
    case PROP_FONT_SIZE:
      g_value_set_float (value, priv->font_size);
      break;
    case PROP_AUTO_RESIZE:
      g_value_set_boolean (value, priv->auto_resize);
      break;
    case PROP_FONT_WEIGHT:
      g_value_set_enum (value, priv->font_weight);
      break;
    case PROP_FONT_STYLE:
      g_value_set_enum (value, priv->font_style);
      break;
    case PROP_FONT_STRETCH:
      g_value_set_enum (value, priv->font_stretch);
      break;
    case PROP_TEXT:
      if (priv->default_text.empty ()) {
        g_value_set_string (value, "");
      } else {
        std::string str = gst_dwrite_wstring_to_string (priv->default_text);
        g_value_set_string (value, str.c_str ());
      }
      break;
    case PROP_COLOR:
      g_value_set_uint (value, priv->foreground_color);
      break;
    case PROP_OUTLINE_COLOR:
      g_value_set_uint (value, priv->outline_color);
      break;
    case PROP_SHADOW_COLOR:
      g_value_set_uint (value, priv->shadow_color);
      break;
    case PROP_BACKGROUND_COLOR:
      g_value_set_uint (value, priv->background_color);
      break;
    case PROP_LAYOUT_X:
      g_value_set_double (value, priv->layout_x);
      break;
    case PROP_LAYOUT_Y:
      g_value_set_double (value, priv->layout_y);
      break;
    case PROP_LAYOUT_WIDTH:
      g_value_set_double (value, priv->layout_width);
      break;
    case PROP_LAYOUT_HEIGHT:
      g_value_set_double (value, priv->layout_height);
      break;
    case PROP_TEXT_ALIGNMENT:
      g_value_set_enum (value, priv->text_align);
      break;
    case PROP_PARAGRAPH_ALIGNMENT:
      g_value_set_enum (value, priv->paragraph_align);
      break;
    case PROP_ENABLE_COLOR_FONT:
      g_value_set_boolean (value, priv->color_font);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dwrite_base_overlay_set_context (GstElement * elem, GstContext * context)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (elem);
  GstDWriteBaseOverlayPrivate *priv = self->priv;

  gst_dwrite_overlay_object_set_context (priv->overlay, elem, context);

  GST_ELEMENT_CLASS (parent_class)->set_context (elem, context);
}

static gboolean
gst_dwrite_base_overlay_start (GstBaseTransform * trans)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (trans);
  GstDWriteBaseOverlayPrivate *priv = self->priv;
  HRESULT hr;

  hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED,
      __uuidof (IDWriteFactory), (IUnknown **) (&priv->dwrite_factory));
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't create dwrite factory");
    return FALSE;
  }

  gst_video_info_init (&self->info);

  return gst_dwrite_overlay_object_start (priv->overlay,
      priv->dwrite_factory.Get ());
}

static void
gst_dwrite_base_overlay_clear_resource (GstDWriteBaseOverlay * self)
{
  gst_dwrite_base_overlay_clear_layout (self);
}

static gboolean
gst_dwrite_base_overlay_stop (GstBaseTransform * trans)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (trans);
  GstDWriteBaseOverlayPrivate *priv = self->priv;

  gst_dwrite_base_overlay_clear_resource (self);

  gst_dwrite_overlay_object_stop (priv->overlay);

  priv->dwrite_factory = nullptr;

  return TRUE;
}

static gboolean
gst_dwrite_base_overlay_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (trans);
  GstDWriteBaseOverlayPrivate *priv = self->priv;

  if (gst_dwrite_overlay_object_handle_query (priv->overlay,
          GST_ELEMENT (self), query)) {
    return TRUE;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans,
      direction, query);
}

static gboolean
gst_dwrite_base_overlay_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (trans);
  GstDWriteBaseOverlayPrivate *priv = self->priv;

  if (!gst_dwrite_overlay_object_decide_allocation (priv->overlay,
          GST_ELEMENT (self), query)) {
    return FALSE;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
      query);
}

static gboolean
gst_dwrite_base_overlay_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (trans);
  GstDWriteBaseOverlayPrivate *priv = self->priv;
  gboolean ret;

  GST_DEBUG_OBJECT (self, "Propose allocation");

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
          decide_query, query))
    return FALSE;

  if (!decide_query) {
    GST_DEBUG_OBJECT (self, "Passthrough");
    return TRUE;
  }

  ret = gst_pad_peer_query (trans->srcpad, query);
  if (!ret)
    return FALSE;

  return gst_dwrite_overlay_object_propose_allocation (priv->overlay,
      GST_ELEMENT (self), query);
}

static GstCaps *
gst_dwrite_base_overlay_add_feature (GstCaps * caps)
{
  GstCaps *new_caps = gst_caps_new_empty ();
  guint caps_size = gst_caps_get_size (caps);

  for (guint i = 0; i < caps_size; i++) {
    GstStructure *s = gst_caps_get_structure (caps, i);
    GstCapsFeatures *f =
        gst_caps_features_copy (gst_caps_get_features (caps, i));
    GstCaps *c = gst_caps_new_full (gst_structure_copy (s), nullptr);

    if (!gst_caps_features_is_any (f) &&
        !gst_caps_features_contains (f,
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION)) {
      gst_caps_features_add (f,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
    }

    gst_caps_set_features (c, 0, f);
    gst_caps_append (new_caps, c);
  }

  return new_caps;
}

static GstCaps *
gst_dwrite_overlay_remove_feature (GstCaps * caps)
{
  GstCaps *new_caps = gst_caps_new_empty ();
  guint caps_size = gst_caps_get_size (caps);

  for (guint i = 0; i < caps_size; i++) {
    GstStructure *s = gst_caps_get_structure (caps, i);
    GstCapsFeatures *f =
        gst_caps_features_copy (gst_caps_get_features (caps, i));
    GstCaps *c = gst_caps_new_full (gst_structure_copy (s), nullptr);

    gst_caps_features_remove (f,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
    gst_caps_set_features (c, 0, f);
    gst_caps_append (new_caps, c);
  }

  return new_caps;
}

static GstCaps *
gst_dwrite_base_overlay_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *result, *tmp;

  GST_DEBUG_OBJECT (trans,
      "Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  if (direction == GST_PAD_SINK) {
    tmp = gst_dwrite_base_overlay_add_feature (caps);
    tmp = gst_caps_merge (tmp, gst_caps_ref (caps));
  } else {
    tmp = gst_dwrite_overlay_remove_feature (caps);
    tmp = gst_caps_merge (gst_caps_ref (caps), tmp);
  }

  if (filter) {
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  GST_DEBUG_OBJECT (trans, "returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_dwrite_base_overlay_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (trans);
  GstDWriteBaseOverlayPrivate *priv = self->priv;

  gst_dwrite_base_overlay_clear_layout (self);

  if (!gst_dwrite_overlay_object_set_caps (priv->overlay,
          GST_ELEMENT (self), incaps, outcaps, &self->info,
          &priv->blend_mode)) {
    GST_ERROR_OBJECT (self, "Set caps failed");
    return FALSE;
  }

  priv->prop_lock.lock ();
  priv->text_format = nullptr;
  priv->layout = nullptr;
  priv->prop_lock.unlock ();

  if (priv->blend_mode == GstDWriteBlendMode::NOT_SUPPORTED)
    gst_base_transform_set_passthrough (trans, TRUE);
  else
    gst_base_transform_set_passthrough (trans, FALSE);

  return TRUE;
}

static void
gst_dwrite_base_overlay_before_transform (GstBaseTransform * trans,
    GstBuffer * buf)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (trans);
  GstDWriteBaseOverlayPrivate *priv = self->priv;

  if (gst_dwrite_overlay_object_update_device (priv->overlay, buf))
    gst_base_transform_reconfigure (trans);
}

static GstFlowReturn
gst_dwrite_base_overlay_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (trans);
  GstDWriteBaseOverlayPrivate *priv = self->priv;
  GstDWriteBaseOverlayClass *klass = GST_DWRITE_BASE_OVERLAY_GET_CLASS (self);

  if (priv->blend_mode == GstDWriteBlendMode::NOT_SUPPORTED) {
    GST_TRACE_OBJECT (self, "Force passthrough");

    return
        GST_BASE_TRANSFORM_CLASS (parent_class)->prepare_output_buffer (trans,
        inbuf, outbuf);
  }

  std::lock_guard < std::mutex > lk (priv->prop_lock);
  /* Invisible, do passthrough */
  if (!priv->visible) {
    gst_base_transform_set_passthrough (trans, TRUE);
    *outbuf = inbuf;
    return GST_FLOW_OK;
  }

  priv->cur_text = klass->get_text (self, priv->default_text, inbuf);
  if (priv->cur_text.empty ()) {
    priv->prev_text.clear ();
    gst_dwrite_base_overlay_clear_layout (self);
    /* Nothing to render, passthrough */
    gst_base_transform_set_passthrough (trans, TRUE);
    *outbuf = inbuf;
    return GST_FLOW_OK;
  }

  gst_base_transform_set_passthrough (trans, FALSE);
  if (priv->prev_text != priv->cur_text)
    gst_dwrite_base_overlay_clear_layout (self);

  priv->prev_text = priv->cur_text;

  return gst_dwrite_overlay_object_prepare_output (priv->overlay, trans,
      parent_class, inbuf, outbuf);
}

static gboolean
gst_dwrite_base_overlay_update_text_format (GstDWriteBaseOverlay * self)
{
  GstDWriteBaseOverlayPrivate *priv = self->priv;
  FLOAT font_size;
  HRESULT hr;

  if (priv->text_format)
    return TRUE;

  gst_dwrite_base_overlay_clear_layout (self);

  if (priv->auto_resize) {
    font_size = (FLOAT) self->info.width * priv->font_size / 640;
  } else {
    font_size = priv->font_size;
  }

  std::wstring wfont_family = gst_dwrite_string_to_wstring (priv->font_family);

  hr = priv->dwrite_factory->CreateTextFormat (wfont_family.c_str (), nullptr,
      priv->font_weight, priv->font_style, priv->font_stretch,
      font_size, L"en-us", &priv->text_format);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't create text format");
    return FALSE;
  }

  return TRUE;
}

static inline D2D1_COLOR_F
unpack_argb (guint packed)
{
  D2D1_COLOR_F ret;
  ret.a = ((packed >> 24) & 0xff) / 255.f;
  ret.r = ((packed >> 16) & 0xff) / 255.f;
  ret.g = ((packed >> 8) & 0xff) / 255.f;
  ret.b = ((packed >> 0) & 0xff) / 255.f;

  return ret;
}

static gboolean
gst_dwrite_base_overlay_create_layout (GstDWriteBaseOverlay * self,
    const std::wstring & text)
{
  GstDWriteBaseOverlayPrivate *priv = self->priv;
  D2D1_COLOR_F color;
  ComPtr < IGstDWriteTextEffect > effect;
  HRESULT hr;
  DWRITE_TEXT_RANGE range;

  priv->layout_origin.x = priv->layout_x * self->info.width;
  priv->layout_origin.y = priv->layout_y * self->info.height;
  priv->layout_size.x = priv->layout_width * self->info.width;
  priv->layout_size.y = priv->layout_height * self->info.height;

  hr = priv->dwrite_factory->CreateTextLayout (text.c_str (), text.length (),
      priv->text_format.Get (), priv->layout_size.x, priv->layout_size.y,
      &priv->layout);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't create text layout");
    return FALSE;
  }

  hr = IGstDWriteTextEffect::CreateInstance (&effect);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't create text effect");
    priv->layout = nullptr;
    return FALSE;
  }

  range.startPosition = 0;
  range.length = G_MAXUINT32;

  priv->layout->SetTextAlignment (priv->text_align);
  priv->layout->SetParagraphAlignment (priv->paragraph_align);
  priv->layout->SetMaxWidth (priv->layout_size.x);
  priv->layout->SetMaxHeight (priv->layout_size.y);

  color = unpack_argb (priv->foreground_color);
  effect->SetBrushColor (GST_DWRITE_BRUSH_FORGROUND, &color);

  color = unpack_argb (priv->outline_color);
  effect->SetBrushColor (GST_DWRITE_BRUSH_OUTLINE, &color);

  color = unpack_argb (priv->shadow_color);
  effect->SetBrushColor (GST_DWRITE_BRUSH_SHADOW, &color);

  color = unpack_argb (priv->background_color);
  effect->SetBrushColor (GST_DWRITE_BRUSH_BACKGROUND, &color);

  effect->SetEnableColorFont (priv->color_font);

  hr = priv->layout->SetDrawingEffect (effect.Get (), range);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't set drawing effect");
    priv->layout = nullptr;
    return FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_dwrite_base_overlay_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstDWriteBaseOverlay *self = GST_DWRITE_BASE_OVERLAY (trans);
  GstDWriteBaseOverlayPrivate *priv = self->priv;
  GstDWriteBaseOverlayClass *klass = GST_DWRITE_BASE_OVERLAY_GET_CLASS (self);
  std::lock_guard < std::mutex > lk (priv->prop_lock);

  if (!priv->overlay) {
    GST_ERROR_OBJECT (self, "Overlay object is not configured");
    return GST_FLOW_ERROR;
  }

  if (!gst_dwrite_base_overlay_update_text_format (self))
    return GST_FLOW_ERROR;

  if (!priv->layout &&
      !gst_dwrite_base_overlay_create_layout (self, priv->cur_text)) {
    return GST_FLOW_ERROR;
  }

  if (!gst_dwrite_overlay_object_draw (priv->overlay, outbuf,
          priv->layout.Get (), priv->layout_origin.x, priv->layout_origin.y)) {
    GST_ERROR_OBJECT (self, "Draw failed");
    return GST_FLOW_ERROR;
  }

  if (klass->after_transform)
    klass->after_transform (self, outbuf);

  return GST_FLOW_OK;
}

void
gst_dwrite_base_overlay_build_param_specs (std::vector < GParamSpec * >&pspec)
{
  pspec.push_back (g_param_spec_boolean ("visible", "Visible",
          "Whether to draw text", DEFAULT_VISIBLE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_string ("font-family", "Font Family",
          "Font family to use", DEFAULT_FONT_FAMILY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_float ("font-size", "Font Size",
          "Font size to use", 0.1f, 1638.f, DEFAULT_FONT_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_boolean ("auto-resize", "Auto Resize",
          "Calculate font size to be equivalent to \"font-size\" at "
          "\"reference-frame-size\"", DEFAULT_AUTO_RESIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_enum ("font-weight", "Font Weight",
          "Font Weight", GST_TYPE_DWRITE_FONT_WEIGHT, DEFAULT_FONT_WEIGHT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_enum ("font-style", "Font Style", "Font Style",
          GST_TYPE_DWRITE_FONT_STYLE, DEFAULT_FONT_STYLE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_enum ("font-stretch", "Font Stretch",
          "Font Stretch", GST_TYPE_DWRITE_FONT_STRETCH, DEFAULT_FONT_STRETCH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_string ("text", "Text", "Text to render", "",
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_uint ("foreground-color", "Foreground Color",
          "Foreground color to use (big-endian ARGB)", 0, G_MAXUINT32,
          DEFAULT_FOREGROUND_COLOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_uint ("outline-color", "Outline Color",
          "Text outline color to use (big-endian ARGB)", 0, G_MAXUINT32,
          DEFAULT_OUTLINE_COLOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_uint ("shadow-color", "Shadow Color",
          "Shadow color to use (big-endian ARGB)", 0, G_MAXUINT32,
          DEFAULT_SHADOW_COLOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_uint ("background-color", "Background Color",
          "Background color to use (big-endian ARGB)", 0, G_MAXUINT32,
          DEFAULT_BACKGROUND_COLOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_double ("layout-x", "Layout X",
          "Normalized X coordinate of text layout", 0, 1, DEFAULT_LAYOUT_XY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_double ("layout-y", "Layout Y",
          "Normalized Y coordinate of text layout", 0, 1, DEFAULT_LAYOUT_XY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_double ("layout-width", "Layout Width",
          "Normalized width of text layout", 0, 1, DEFAULT_LAYOUT_WH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_double ("layout-height", "Layout Height",
          "Normalized height of text layout", 0, 1, DEFAULT_LAYOUT_WH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_enum ("text-alignment", "Text Alignment",
          "Text Alignment", GST_TYPE_DWRITE_TEXT_ALIGNMENT,
          DEFAULT_TEXT_ALIGNMENT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_enum ("paragraph-alignment",
          "Paragraph alignment", "Paragraph Alignment",
          GST_TYPE_DWRITE_PARAGRAPH_ALIGNMENT, DEFAULT_PARAGRAPH_ALIGNMENT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
#ifdef HAVE_DWRITE_COLOR_FONT
  if (gst_dwrite_is_windows_10_or_greater ()) {
    pspec.push_back (g_param_spec_boolean ("color-font", "Color Font",
            "Enable color font, requires Windows 10 or newer",
            DEFAULT_COLOR_FONT,
            (GParamFlags) (GST_PARAM_CONDITIONALLY_AVAILABLE |
                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  }
#endif
}
