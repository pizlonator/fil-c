/* Copyright (C) <2021> Thibault Saunier <tsaunier@igalia.com>
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of the
 * GNU Library General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License along with this
 * library; if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "gstwpeextension.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

GST_DEBUG_CATEGORY (wpe_bus_msg_forwarder_debug);
#define GST_CAT_DEFAULT wpe_bus_msg_forwarder_debug

struct _GstWpeBusMsgForwarder
{
  GstTracer parent;
  GCancellable *cancellable;
};

G_DEFINE_TYPE_WITH_CODE (GstWpeBusMsgForwarder, gst_wpe_bus_msg_forwarder,
    GST_TYPE_TRACER, GST_DEBUG_CATEGORY_INIT (wpe_bus_msg_forwarder_debug,
        "wpebusmsgforwarder", 0, "WPE message forwarder"););

static void
dispose (GObject * object)
{
  GstWpeBusMsgForwarder *self = GST_WPE_BUS_MSG_FORWARDER (object);

  g_clear_object (&self->cancellable);
}

static WebKitUserMessage *
create_gerror_bus_msg (GstElement * element, GstMessage * message)
{
  GError *error;
  gchar *debug_str, *details_structure, *src_path;
  WebKitUserMessage *msg;
  const GstStructure *details = NULL;

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
    gst_message_parse_error (message, &error, &debug_str);
    gst_message_parse_error_details (message, &details);
  } else if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_WARNING) {
    gst_message_parse_warning (message, &error, &debug_str);
    gst_message_parse_warning_details (message, &details);
  } else {
    gst_message_parse_info (message, &error, &debug_str);
    gst_message_parse_info_details (message, &details);
  }

  details_structure =
      details ? gst_structure_to_string (details) : g_strdup ("");
  src_path = gst_object_get_path_string (GST_MESSAGE_SRC (message));

  msg = webkit_user_message_new ("gstwpe.bus_gerror_message",
      /* (message_type, src_path, error_domain, error_code, msg, debug_str, details_structure) */
      g_variant_new ("(issssusss)",
          GST_MESSAGE_TYPE (message),
          GST_MESSAGE_SRC_NAME (message),
          G_OBJECT_TYPE_NAME (GST_MESSAGE_SRC (message)),
          src_path,
          g_quark_to_string (error->domain),
          error->code, error->message, debug_str, details_structure)
      );
  g_free (src_path);

  return msg;
}

/* Those types can't be deserialized on the receiver
 * side, so we just ignore them for now */
#define IS_NOT_DESERIALIZABLE_TYPE(value) \
  (g_type_is_a ((G_VALUE_TYPE (value)), G_TYPE_OBJECT)  || \
   g_type_is_a ((G_VALUE_TYPE (value)), G_TYPE_ERROR) || \
   g_type_is_a ((G_VALUE_TYPE (value)), GST_TYPE_CONTEXT) || \
   g_type_is_a ((G_VALUE_TYPE (value)), G_TYPE_POINTER))

static gboolean
cleanup_structure (GQuark field_id, GValue * value, gpointer self)
{
  /* We need soome API in core to make that happen cleanly */
  if (IS_NOT_DESERIALIZABLE_TYPE (value)) {
    return FALSE;
  }

  if (GST_VALUE_HOLDS_LIST (value)) {
    gint i;

    for (i = 0; i < gst_value_list_get_size (value); i++) {
      if (IS_NOT_DESERIALIZABLE_TYPE (gst_value_list_get_value (value, i)))
        return FALSE;
    }
  }

  if (GST_VALUE_HOLDS_ARRAY (value)) {
    gint i;

    for (i = 0; i < gst_value_array_get_size (value); i++) {
      if (IS_NOT_DESERIALIZABLE_TYPE (gst_value_array_get_value (value, i)))
        return FALSE;
    }
  }

  return TRUE;
}

static void
gst_message_post_cb (GObject * object, GstClockTime ts, GstElement * element,
    GstMessage * message)
{
  gchar *str;
  WebKitUserMessage *msg = NULL;
  GstStructure *structure;
  GstWpeBusMsgForwarder *self;
  const GstStructure *message_struct;

  if (!GST_IS_PIPELINE (element))
    return;

  self = GST_WPE_BUS_MSG_FORWARDER (object);
  message_struct = gst_message_get_structure (message);
  structure = message_struct ? gst_structure_copy (message_struct) : NULL;

  if (structure) {
    gst_structure_filter_and_map_in_place (structure, cleanup_structure, self);
    str = gst_structure_to_string (structure);
  } else {
    str = g_strdup ("");
  }

  /* we special case error as gst can't serialize/de-serialize it */
  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR
      || GST_MESSAGE_TYPE (message) == GST_MESSAGE_WARNING
      || GST_MESSAGE_TYPE (message) == GST_MESSAGE_INFO) {
    msg = create_gerror_bus_msg (element, message);
  } else {
    gchar *src_path = gst_object_get_path_string (GST_MESSAGE_SRC (message));
    msg = webkit_user_message_new ("gstwpe.bus_message",
        g_variant_new ("(issss)",
            GST_MESSAGE_TYPE (message),
            GST_MESSAGE_SRC_NAME (message),
            G_OBJECT_TYPE_NAME (GST_MESSAGE_SRC (message)), src_path, str));
    g_free (src_path);
  }
  if (msg)
    gst_wpe_extension_send_message (msg, self->cancellable, NULL, NULL);
  g_free (str);
}

static void
constructed (GObject * object)
{
  GstTracer *tracer = GST_TRACER (object);
  gst_tracing_register_hook (tracer, "element-post-message-pre",
      G_CALLBACK (gst_message_post_cb));
}

static void
gst_wpe_bus_msg_forwarder_init (GstWpeBusMsgForwarder * self)
{
  self->cancellable = g_cancellable_new ();
}

static void
gst_wpe_bus_msg_forwarder_class_init (GstWpeBusMsgForwarderClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = dispose;
  object_class->constructed = constructed;
}
