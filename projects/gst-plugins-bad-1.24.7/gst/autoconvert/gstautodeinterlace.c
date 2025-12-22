/* GStreamer
 * Copyright 2023 Igalia S.L.
 *  @author: Thibault Saunier <tsaunier@igalia.com>
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
 * SECTION:element-autodeinterlace
 * @title: autodeinterlace
 *
 * The #autodeinterlace element is used to automatically select the right
 * deinterlacer based on the caps of the stream. It can plug several elements
 * to download/upload to/from the GPU when required/best.
 *
 * Note: This element guarantees that caps negotiation works but when setting
 * properties that are not available on the best underlying deinterlacer some
 * upload/download might be required and performance are impacted.
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstautovideo.h"
#include "gstautodeinterlace.h"

GST_DEBUG_CATEGORY (autodeinterlace_debug);
#define GST_CAT_DEFAULT (autodeinterlace_debug)

#define DEFAULT_FIELDS          GST_AUTO_DEINTERLACE_FIELDS_ALL
#define DEFAULT_LAYOUT          GST_AUTO_DEINTERLACE_LAYOUT_AUTO
#define DEFAULT_MODE            GST_AUTO_DEINTERLACE_MODE_AUTO
enum
{
  PROP_0,
  PROP_FIELDS,
  PROP_MODE,
  PROP_LAYOUT,
};

/**
 * GstAutoDeinterlaceFieldLayout:
 * @GST_AUTO_DEINTERLACE_LAYOUT_AUTO: Auto detection
 * @GST_AUTO_DEINTERLACE_LAYOUT_TFF: Top field first
 * @GST_AUTO_DEINTERLACE_LAYOUT_BFF: Bottom field first
 *
 * Since: 1.24
 */
typedef enum
{
  GST_AUTO_DEINTERLACE_LAYOUT_AUTO,
  GST_AUTO_DEINTERLACE_LAYOUT_TFF,
  GST_AUTO_DEINTERLACE_LAYOUT_BFF,
} GstAutoDeinterlaceFieldLayout;

#define GST_TYPE_AUTO_DEINTERLACE_FIELD_LAYOUT (gst_auto_deinterlace_field_layout_get_type ())
static GType
gst_auto_deinterlace_field_layout_get_type (void)
{
  static GType auto_deinterlace_field_layout_type = 0;

  static const GEnumValue field_layout_types[] = {
    {GST_AUTO_DEINTERLACE_LAYOUT_AUTO, "Auto detection", "auto"},
    {GST_AUTO_DEINTERLACE_LAYOUT_TFF, "Top field first", "tff"},
    {GST_AUTO_DEINTERLACE_LAYOUT_BFF, "Bottom field first", "bff"},
    {0, NULL, NULL},
  };

  if (!auto_deinterlace_field_layout_type) {
    auto_deinterlace_field_layout_type =
        g_enum_register_static ("GstAutoDeinterlaceFieldLayout",
        field_layout_types);
  }
  return auto_deinterlace_field_layout_type;
}

/**
 * GstAutoDeinterlaceFields:
 * @GST_AUTO_DEINTERLACE_FIELDS_ALL: All fields (missing data is interpolated)
 * @GST_AUTO_DEINTERLACE_FIELDS_TOP: Top Fields only
 * @GST_AUTO_DEINTERLACE_FIELDS_BOTTOM: Bottom Fields Only
 * @GST_AUTO_DEINTERLACE_FIELDS_AUTO: Automatically detect
 *
 * Since: 1.24
 */
typedef enum
{
  GST_AUTO_DEINTERLACE_FIELDS_ALL,      /* All (missing data is interp.) */
  GST_AUTO_DEINTERLACE_FIELDS_TOP,      /* Top Fields Only */
  GST_AUTO_DEINTERLACE_FIELDS_BOTTOM,   /* Bottom Fields Only */
  GST_AUTO_DEINTERLACE_FIELDS_AUTO      /* Automatically detect */
} GstAutoDeinterlaceFields;


#define GST_TYPE_AUTO_DEINTERLACE_FIELDS (gst_auto_deinterlace_fields_get_type ())
static GType
gst_auto_deinterlace_fields_get_type (void)
{
  static GType deinterlace_fields_type = 0;

  static const GEnumValue fields_types[] = {
    {GST_AUTO_DEINTERLACE_FIELDS_ALL, "All fields", "all"},
    {GST_AUTO_DEINTERLACE_FIELDS_TOP, "Top fields only", "top"},
    {GST_AUTO_DEINTERLACE_FIELDS_BOTTOM, "Bottom fields only", "bottom"},
    {GST_AUTO_DEINTERLACE_FIELDS_AUTO, "Automatically detect", "auto"},
    {0, NULL, NULL},
  };

  if (!deinterlace_fields_type) {
    deinterlace_fields_type =
        g_enum_register_static ("GstAutoDeinterlaceFields", fields_types);
  }
  return deinterlace_fields_type;
}

/**
 * GstAutoDeinterlaceModes:
 * @GST_AUTO_DEINTERLACE_MODE_AUTO: Auto detection (best effort)
 * @GST_AUTO_DEINTERLACE_MODE_INTERLACED: Force deinterlacing
 * @GST_AUTO_DEINTERLACE_MODE_DISABLED: Run in passthrough modes_types
 * @GST_AUTO_DEINTERLACE_MODE_AUTO_STRICT: Auto detection (strict)
 *
 * Since: 1.24
 */
typedef enum
{
  GST_AUTO_DEINTERLACE_MODE_AUTO,
  GST_AUTO_DEINTERLACE_MODE_INTERLACED,
  GST_AUTO_DEINTERLACE_MODE_DISABLED,
  GST_AUTO_DEINTERLACE_MODE_AUTO_STRICT
} GstAutoDeinterlaceModes;

#define GST_TYPE_AUTO_DEINTERLACE_MODES (gst_auto_deinterlace_modes_get_type ())
static GType
gst_auto_deinterlace_modes_get_type (void)
{
  static GType deinterlace_modes_type = 0;

  static const GEnumValue modes_types[] = {
    {GST_AUTO_DEINTERLACE_MODE_AUTO, "Auto detection (best effort)", "auto"},
    {GST_AUTO_DEINTERLACE_MODE_INTERLACED, "Force deinterlacing", "interlaced"},
    {GST_AUTO_DEINTERLACE_MODE_DISABLED, "Run in passthrough mode", "disabled"},
    {GST_AUTO_DEINTERLACE_MODE_AUTO_STRICT, "Auto detection (strict)",
        "auto-strict"},
    {0, NULL, NULL},
  };

  if (!deinterlace_modes_type) {
    deinterlace_modes_type =
        g_enum_register_static ("GstAutoDeinterlaceModes", modes_types);
  }
  return deinterlace_modes_type;
}


typedef struct
{
  const gchar *factory_name;
  const gchar *our_name;
  const gchar *their_name;
  struct
  {
    gint our_value;
    const gchar *their_value;
  } values[5];
} EnumMap;

static EnumMap ENUM_MAP[] = {
  {
        .factory_name = "deinterlace",
        .our_name = "layout",
        .their_name = "tff",
        .values = {
              {
                    .our_value = GST_AUTO_DEINTERLACE_LAYOUT_AUTO,
                    .their_value = "auto",
                  },
              {
                    .our_value = GST_AUTO_DEINTERLACE_LAYOUT_TFF,
                    .their_value = "tff",
                  },
              {
                    .our_value = GST_AUTO_DEINTERLACE_LAYOUT_BFF,
                    .their_value = "bff",
                  },
            },
      },
  {
        .factory_name = "deinterlace",
        .our_name = "fields",
        .their_name = "fields",
        .values = {
              {
                    .our_value = GST_AUTO_DEINTERLACE_FIELDS_ALL,
                    .their_value = "all",
                  },
              {
                    .our_value = GST_AUTO_DEINTERLACE_FIELDS_BOTTOM,
                    .their_value = "bottom",
                  },
              {
                    .our_value = GST_AUTO_DEINTERLACE_FIELDS_TOP,
                    .their_value = "top",
                  },
              {
                    .our_value = GST_AUTO_DEINTERLACE_FIELDS_AUTO,
                    .their_value = "auto",
                  },
            },
      },
  {
        .factory_name = "deinterlace",
        .our_name = "mode",
        .their_name = "mode",
        .values = {
              {
                    .our_value = GST_AUTO_DEINTERLACE_MODE_AUTO,
                    .their_value = "auto",
                  },
              {
                    .our_value = GST_AUTO_DEINTERLACE_MODE_DISABLED,
                    .their_value = "disabled",
                  },
              {
                    .our_value = GST_AUTO_DEINTERLACE_MODE_INTERLACED,
                    .their_value = "interlaced",
                  },
              {
                    .our_value = GST_AUTO_DEINTERLACE_MODE_AUTO_STRICT,
                    .their_value = "auto-strict",
                  },
            },
      },
  {
        .factory_name = "gldeinterlace",
        .our_name = "layout",
        .their_name = NULL,
      },
  {
        .factory_name = "gldeinterlace",
        .our_name = "fields",
        .their_name = NULL,
      },
  {
        .factory_name = "gldeinterlace",
        .our_name = "mode",
        .their_name = NULL,
      },
  {
        .factory_name = "d3d11deinterlaceelement",
        .our_name = "layout",
        .their_name = NULL,
      },
  {
        .factory_name = "d3d11deinterlaceelement",
        .our_name = "fields",
        .their_name = NULL,
      },
  {
        .factory_name = "d3d11deinterlaceelement",
        .our_name = "mode",
        .their_name = NULL,
      }
};

#if !GLIB_CHECK_VERSION(2, 68, 0)
static GObject *
g_binding_dup_source (GBinding * binding)
{
  return g_object_ref (g_binding_get_source (binding));
}

static GObject *
g_binding_dup_target (GBinding * binding)
{
  return g_object_ref (g_binding_get_target (binding));
}
#endif

struct _GstAutoDeinterlace
{
  GstBaseAutoConvert parent;

  GstAutoDeinterlaceFieldLayout field_layout;
  GstAutoDeinterlaceFields fields;
  GstAutoDeinterlaceModes mode;
  GList *bindings;
};

G_DEFINE_TYPE (GstAutoDeinterlace, gst_auto_deinterlace,
    GST_TYPE_BASE_AUTO_CONVERT);

GST_ELEMENT_REGISTER_DEFINE (autodeinterlace, "autodeinterlace",
    GST_RANK_NONE, gst_auto_deinterlace_get_type ());

static void
gst_auto_deinterlace_register_filters (GstAutoDeinterlace * self)
{
  const GstAutoVideoFilterGenerator *g;

  /* Only the software deinterlacer supports all our properties so if we are using
   * defaults values for all of them, use hw deinterlacer otherwise ensure to use
   * our software one to respect what the user wants */
  /* *INDENT-OFF* */
  if (self->field_layout == DEFAULT_LAYOUT && self->fields == DEFAULT_FIELDS
      && self->mode == DEFAULT_MODE) {
    static const GstAutoVideoFilterGenerator gen[] = {
      {
        .first_elements = { "bayer2rgb", NULL},
        .colorspace_converters = { "videoconvert", NULL },
        .last_elements = { NULL } ,
        .filters = { "deinterlace", NULL},
        .rank = GST_RANK_SECONDARY,
      },
      {
        .first_elements = { NULL, },
        .colorspace_converters = { "videoconvert", NULL },
        .last_elements = { "rgb2bayer", NULL },
        .filters = { "deinterlace", NULL },
        .rank = GST_RANK_SECONDARY,
      },
      {
        .first_elements = { NULL, },
        .colorspace_converters = { "videoconvert", NULL },
        .last_elements = { NULL, },
        .filters = { "deinterlace", NULL },
        .rank = GST_RANK_SECONDARY,
      },
      {
        .first_elements = { NULL, },
        .colorspace_converters = { "glcolorconvert", "glcolorscale", "glcolorconvert", NULL },
        .last_elements = { NULL, },
        .filters = { "gldeinterlace", NULL },
        .rank = GST_RANK_PRIMARY,
      },
      {
        .first_elements = { "glupload", },
        .colorspace_converters = { "glcolorconvert", "glcolorscale", "glcolorconvert", NULL },
        .last_elements = { NULL, },
        .filters = { "gldeinterlace", NULL },
        .rank = GST_RANK_PRIMARY,
      },
      { /* Worst case we upload/download as required */
        .first_elements = { "glupload", "gldownload", NULL },
        .colorspace_converters = { "glcolorconvert",  "glcolorscale", "glcolorconvert", NULL },
        .last_elements = { "glupload", "gldownload", NULL },
        .filters = { "gldeinterlace", NULL },
        .rank = GST_RANK_MARGINAL,
      },
      {
        .first_elements = { NULL, },
        .colorspace_converters = { "d3d11convert", NULL },
        .last_elements = { NULL, },
        .filters = { "d3d11deinterlaceelement" },
        .rank = GST_RANK_PRIMARY,
      },
      {
        .first_elements = { NULL},
        .colorspace_converters = { "d3d11deinterlace", NULL },
        .last_elements = { NULL },
        .filters = { NULL },
        .rank = GST_RANK_MARGINAL,
      },
      { /* CUDA -> GL */
        .first_elements = { "capsfilter caps=video/x-raw(memory:CUDAMemory)", "cudadownload", NULL },
        .colorspace_converters = { "glcolorconvert",  "glcolorscale", "glcolorconvert", NULL },
        .last_elements = { NULL },
        .filters = { "gldeinterlace", NULL },
        .rank = GST_RANK_PRIMARY - 1,
      },
      { /* CUDA -> CUDA */
        .first_elements = { "capsfilter caps=video/x-raw(memory:CUDAMemory)", "cudadownload", NULL },
        .colorspace_converters = { "glcolorconvert",  "glcolorscale", "glcolorconvert", NULL },
        .last_elements = { "cudaupload", "capsfilter caps=video/x-raw(memory:CUDAMemory)", NULL },
        .filters = { "gldeinterlace", NULL },
        .rank = GST_RANK_SECONDARY - 1,
      },
      { /* Software -> CUDA (uploading as soon as possible) */
        .first_elements = { "glupload", NULL },
        .colorspace_converters = { "glcoloconvert", NULL },
        .last_elements = { "cudaupload", "capsfilter caps=video/x-raw(memory:CUDAMemory)", NULL },
        .filters = { "gldeinterlace", NULL },
        .rank = GST_RANK_MARGINAL,
      },
      { /* CUDA -> Software */
        .first_elements = { "capsfilter caps=video/x-raw(memory:CUDAMemory)", "cudadownload", NULL },
        .colorspace_converters = { "glcolorconvert",  "glcolorscale", "glcolorconvert", NULL },
        .last_elements = { "gldownload", NULL },
        .filters = { "gldeinterlace", NULL },
        .rank = GST_RANK_MARGINAL,
      },
      {
        .first_elements = { NULL},
        .colorspace_converters = { NULL },
        .last_elements = { NULL },
        .filters = { NULL },
        .rank = 0,
      },
    };

    g = gen;
  } else {
      static const GstAutoVideoFilterGenerator gen[] = {
        {
          .first_elements = { "bayer2rgb", NULL},
          .colorspace_converters = { "videoconvert", NULL },
          .last_elements = { NULL } ,
          .filters = { "deinterlace", NULL},
          .rank = GST_RANK_SECONDARY,
        },
        {
          .first_elements = { NULL, },
          .colorspace_converters = { "videoconvert", NULL },
          .last_elements = { "rgb2bayer", NULL },
          .filters = { "deinterlace", NULL },
          .rank = GST_RANK_SECONDARY,
        },
        {
          .first_elements = { NULL, },
          .colorspace_converters = { "videoconvertscale", NULL },
          .last_elements = { NULL, },
          .filters = { "deinterlace", NULL },
          .rank = GST_RANK_SECONDARY,
        },
        { /* Worst case we upload/download as required */
          .first_elements = { "gldownload", NULL },
          .colorspace_converters = { "videoconvert", NULL },
          .last_elements = { "glupload", NULL },
          .filters = { "deinterlace", NULL },
          .rank = GST_RANK_MARGINAL,
        },
        { /* Cuda -> Cuda */
          .first_elements = { "capsfilter caps=video/x-raw(memory:CUDAMemory)", "cudadownload", NULL },
          .colorspace_converters = { "videoconvert", NULL },
          .last_elements = { "cudaupload", "capsfilter caps=video/x-raw(memory:CUDAMemory)", NULL },
          .filters = { "deinterlace", NULL },
          .rank = GST_RANK_SECONDARY + 1,
        },
        { /* Cuda -> software */
          .first_elements = { "capsfilter caps=video/x-raw(memory:CUDAMemory)", "cudadownload", NULL },
          .colorspace_converters = { "videoconvert", NULL },
          .last_elements = { "glupload", NULL },
          .filters = { "deinterlace", NULL },
          .rank = GST_RANK_MARGINAL,
        },
        { /* Cuda -> software */
          .first_elements = { "capsfilter caps=video/x-raw(memory:CUDAMemory)", "cudadownload", NULL },
          .colorspace_converters = { "videoconvert", NULL },
          .last_elements = { NULL },
          .filters = { "deinterlace", NULL },
          .rank = GST_RANK_MARGINAL,
        },
        { /* Software -> cuda */
          .first_elements = { NULL },
          .colorspace_converters = { "videoconvert", NULL },
          .last_elements = { "cudaupload", "capsfilter caps=video/x-raw(memory:CUDAMemory)", NULL },
          .filters = { "deinterlace", NULL },
          .rank = GST_RANK_MARGINAL,
        },
        {
          .first_elements = { "d3d11upload", "d3d11download" },
          .colorspace_converters = { "videoconvert", NULL },
          .last_elements = { "d3d11upload", "d3d11download" },
          .filters = { "deinterlace" },
          .rank = GST_RANK_MARGINAL,
        },
        { /* Worst case we upload/download as required */
          .first_elements = { NULL},
          .colorspace_converters = { NULL },
          .last_elements = { NULL },
          .filters = { NULL },
          .rank = 0,
        },
      };

    g = gen;
  }
  /* *INDENT-ON* */

  gst_auto_video_register_well_known_bins (GST_BASE_AUTO_CONVERT (self), g);
}

static void
gst_auto_deinterlace_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAutoDeinterlace *autodeinterlace = GST_AUTO_DEINTERLACE (object);

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_LAYOUT:
      g_value_set_enum (value, autodeinterlace->field_layout);
      break;
    case PROP_FIELDS:
      g_value_set_enum (value, autodeinterlace->fields);
      break;
    case PROP_MODE:
      g_value_set_enum (value, autodeinterlace->mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_auto_deinterlace_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAutoDeinterlace *autodeinterlace = GST_AUTO_DEINTERLACE (object);
  gint v = g_value_get_enum (value);
  gboolean changed = FALSE;

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_LAYOUT:
      changed = (autodeinterlace->field_layout != v);
      autodeinterlace->field_layout = v;
      break;
    case PROP_FIELDS:
      changed = (autodeinterlace->fields != v);
      autodeinterlace->fields = v;
      break;
    case PROP_MODE:
      changed = (autodeinterlace->mode != v);
      autodeinterlace->mode = v;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);

  if (changed) {
    gst_base_auto_convert_reset_filters (GST_BASE_AUTO_CONVERT (object));
    gst_auto_deinterlace_register_filters (GST_AUTO_DEINTERLACE (object));

    /* Force a reconfigure so the new property can be taken into account if necessary */
    gst_pad_push_event (GST_BASE_AUTO_CONVERT (object)->sinkpad,
        gst_event_new_reconfigure ());
  }
}

static gboolean
element_is_handled_deinterlace (GstElement * element)
{
  GstElementFactory *factory = gst_element_get_factory (element);

  for (gint i = 0; i < G_N_ELEMENTS (ENUM_MAP); i++) {
    if (g_strcmp0 (GST_OBJECT_NAME (factory), ENUM_MAP[i].factory_name) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
gst_auto_deinterlace_transform_to (GBinding * binding, const GValue * from,
    GValue * to_value, gpointer _udata)
{
  const gchar *value = NULL;
  EnumMap *map = NULL;
  gboolean res = FALSE;
  GstAutoDeinterlace *self =
      GST_AUTO_DEINTERLACE (g_binding_dup_source (binding));
  GstElement *target = GST_ELEMENT (g_binding_dup_target (binding));
  GstElementFactory *factory = gst_element_get_factory (target);

  for (gint i = 0; i < G_N_ELEMENTS (ENUM_MAP); i++) {
    if (g_strcmp0 (GST_OBJECT_NAME (factory), ENUM_MAP[i].factory_name) == 0 &&
        g_strcmp0 (g_binding_get_source_property (binding),
            ENUM_MAP[i].our_name) == 0) {
      map = &ENUM_MAP[i];
      break;
    }
  }

  if (!map) {
    GST_WARNING_OBJECT (self, "Could not find mapping for %s"
        " property won't be set on the deinterlacing element",
        gst_plugin_feature_get_name (GST_PLUGIN_FEATURE (factory))
        );
    goto done;
  }


  for (gint j = 0; map->values[j].their_value; j++) {
    if (map->values[j].our_value == g_value_get_enum (from)) {
      value = map->values[j].their_value;
      break;
    }
  }

  if (value) {
    GParamSpec *pspec =
        g_object_class_find_property (G_OBJECT_GET_CLASS (target),
        g_binding_get_target_property (binding));
    GST_ERROR ("Setting %s - %s to %s", map->our_name,
        g_binding_get_source_property (binding), value);
    res = gst_value_deserialize_with_pspec (to_value, value, pspec);
  } else {
    GST_WARNING_OBJECT (self, "Could not transfer value for property %s to %s",
        g_binding_get_source_property (binding),
        g_binding_get_target_property (binding)
        );
  }

done:
  gst_clear_object (&self);
  gst_clear_object (&target);

  return res;
}

static void
gst_auto_deinterlace_deep_element_added (GstBin * bin, GstBin * sub_bin,
    GstElement * element)
{
  GstAutoDeinterlace *self = GST_AUTO_DEINTERLACE (bin);
  GList *new_bindings = NULL;
  if (!element_is_handled_deinterlace (element))
    goto done;

  GST_OBJECT_LOCK (bin);
  for (GList * tmp = self->bindings; tmp; tmp = tmp->next) {
    GBinding *binding = tmp->data;
    GObject *target = g_binding_dup_target (binding);

    if (GST_ELEMENT (target) == element) {
      GST_INFO_OBJECT (self, "Newly added element %s already bound",
          GST_OBJECT_NAME (gst_element_get_factory (element)));
      GST_OBJECT_UNLOCK (bin);
      gst_object_unref (target);
      goto done;
    }
    gst_object_unref (target);
  }
  GST_OBJECT_UNLOCK (bin);

  for (gint i = 0; i < G_N_ELEMENTS (ENUM_MAP); i++) {
    if (g_strcmp0 (GST_OBJECT_NAME (gst_element_get_factory (element)),
            ENUM_MAP[i].factory_name))
      continue;

    if (!ENUM_MAP[i].their_name) {
      GST_WARNING_OBJECT (self, "No mapping for our property %s on %s",
          ENUM_MAP[i].our_name,
          GST_OBJECT_NAME (gst_element_get_factory (element)));
      continue;
    }

    new_bindings = g_list_prepend (new_bindings,
        g_object_bind_property_full (bin, ENUM_MAP[i].our_name,
            element, ENUM_MAP[i].their_name,
            G_BINDING_SYNC_CREATE | G_BINDING_DEFAULT,
            gst_auto_deinterlace_transform_to, NULL, NULL, NULL)
        );
  }
  GST_OBJECT_LOCK (bin);
  self->bindings = g_list_concat (self->bindings, new_bindings);
  GST_OBJECT_UNLOCK (bin);

done:
  GST_BIN_CLASS (gst_auto_deinterlace_parent_class)->deep_element_added (bin,
      sub_bin, element);
}

static void
gst_auto_deinterlace_deep_element_removed (GstBin * bin, GstBin * sub_bin,
    GstElement * element)
{
  GList *bindings = NULL;
  GstAutoDeinterlace *self = GST_AUTO_DEINTERLACE (bin);

  if (!element_is_handled_deinterlace (element))
    goto done;

  GST_OBJECT_LOCK (bin);
  for (GList * tmp = self->bindings; tmp; tmp = tmp->next) {
    GBinding *binding = tmp->data;
    GObject *target = g_binding_dup_target (binding);

    if (GST_ELEMENT (target) == element) {
      GList *node = tmp;

      bindings = g_list_prepend (bindings, binding);
      tmp = tmp->prev;

      self->bindings = g_list_delete_link (self->bindings, node);
      if (!tmp)
        break;
    }
    gst_object_unref (target);
  }
  GST_OBJECT_UNLOCK (bin);

done:
  GST_BIN_CLASS (gst_auto_deinterlace_parent_class)->deep_element_removed (bin,
      sub_bin, element);
}

static void
gst_auto_deinterlace_class_init (GstAutoDeinterlaceClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBinClass *gstbin_class = (GstBinClass *) klass;

  GST_DEBUG_CATEGORY_INIT (autodeinterlace_debug, "autodeinterlace", 0,
      "Auto color space converter");

  gobject_class->set_property = gst_auto_deinterlace_set_property;
  gobject_class->get_property = gst_auto_deinterlace_get_property;

  /**
   * GstAutoDeinterlace:layout:
   *
   * This selects which fields is the first in time.
   *
   */
  g_object_class_install_property (gobject_class, PROP_LAYOUT,
      g_param_spec_enum ("layout",
          "layout",
          "Layout to use"
          " Note that if the underlying implementation doesn't support the property it will be ignored.",
          GST_TYPE_AUTO_DEINTERLACE_FIELD_LAYOUT,
          DEFAULT_LAYOUT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  /**
   * GstAutoDeinterlace:fields:
   *
   * This selects which fields should be output. If "all" is selected
   * the output framerate will be double.
   */
  g_object_class_install_property (gobject_class, PROP_FIELDS,
      g_param_spec_enum ("fields",
          "fields",
          "Fields to use for deinterlacing."
          " Note that if the underlying implementation doesn't support the property it will be ignored.",
          GST_TYPE_AUTO_DEINTERLACE_FIELDS,
          DEFAULT_FIELDS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  /**
   * GstAutoDeinterlace:mode:
   *
   * This selects whether the deinterlacing methods should
   * always be applied or if they should only be applied
   * on content that has the "interlaced" flag on the caps.
   */
  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_enum ("mode",
          "Mode",
          "Deinterlace Mode",
          GST_TYPE_AUTO_DEINTERLACE_MODES,
          DEFAULT_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  gst_type_mark_as_plugin_api (GST_TYPE_AUTO_DEINTERLACE_FIELDS, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_AUTO_DEINTERLACE_MODES, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_AUTO_DEINTERLACE_FIELD_LAYOUT, 0);

  gst_element_class_set_static_metadata (gstelement_class,
      "Select deinterlacer, and converters based on caps",
      "Bin/Colorspace/Scale/Video/Converter/Deinterlace",
      "Selects the right deinterlacer based on caps",
      "Thibault Saunier <tsaunier@igalia.com>");

  gstbin_class->deep_element_added = gst_auto_deinterlace_deep_element_added;
  gstbin_class->deep_element_removed =
      gst_auto_deinterlace_deep_element_removed;
}

static void
gst_auto_deinterlace_init (GstAutoDeinterlace * self)
{
  gst_auto_deinterlace_register_filters (self);
}
