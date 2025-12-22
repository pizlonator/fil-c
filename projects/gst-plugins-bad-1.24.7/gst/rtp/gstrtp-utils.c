/*
 * See: https://bugzilla.gnome.org/show_bug.cgi?id=779765
 */

#include "gstrtp-utils.h"

static void
gst_rtp_utils_uri_query_foreach (const gchar * key, const gchar * value,
    GObject * src)
{
  if (key == NULL) {
    GST_WARNING_OBJECT (src, "Refusing to use empty key.");
    return;
  }

  if (value == NULL) {
    GST_WARNING_OBJECT (src, "Refusing to use NULL for key %s.", key);
    return;
  }

  GST_DEBUG_OBJECT (src, "Setting property '%s' to '%s'", key, value);
  gst_util_set_object_arg (src, key, value);
}

void
gst_rtp_utils_set_properties_from_uri_query (GObject * obj, const GstUri * uri)
{
  GHashTable *hash_table;

  g_return_if_fail (uri != NULL);
  hash_table = gst_uri_get_query_table (uri);

  if (hash_table) {
    g_hash_table_foreach (hash_table,
        (GHFunc) gst_rtp_utils_uri_query_foreach, obj);

    g_hash_table_unref (hash_table);
  }
}
