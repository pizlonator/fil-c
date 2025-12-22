#include <string.h>

#include "utils.h"
#include <gst/pbutils/descriptions.h>

void
print (GstDebugColorFlags c, gboolean err, gboolean nline, const gchar * format,
    va_list var_args)
{
  GString *str = g_string_new (NULL);
  GstDebugColorMode color_mode;
  gchar *color = NULL;
  const gchar *clear = NULL;

  color_mode = gst_debug_get_color_mode ();
#ifdef G_OS_WIN32
  if (color_mode == GST_DEBUG_COLOR_MODE_UNIX) {
#else
  if (color_mode != GST_DEBUG_COLOR_MODE_OFF) {
#endif
    clear = "\033[00m";
    color = gst_debug_construct_term_color (c);
  }

  if (color) {
    g_string_append (str, color);
    g_free (color);
  }

  g_string_append_vprintf (str, format, var_args);

  if (nline)
    g_string_append_c (str, '\n');

  if (clear)
    g_string_append (str, clear);

  if (err)
    g_printerr ("%s", str->str);
  else
    g_print ("%s", str->str);

  g_string_free (str, TRUE);
}

void
ok (const gchar * format, ...)
{
  va_list var_args;

  va_start (var_args, format);
  print (GST_DEBUG_FG_GREEN, FALSE, TRUE, format, var_args);
  va_end (var_args);
}

void
warn (const gchar * format, ...)
{
  va_list var_args;

  va_start (var_args, format);
  print (GST_DEBUG_FG_YELLOW, TRUE, TRUE, format, var_args);
  va_end (var_args);
}

void
error (const gchar * format, ...)
{
  va_list var_args;

  va_start (var_args, format);
  print (GST_DEBUG_FG_RED, TRUE, TRUE, format, var_args);
  va_end (var_args);
}

gchar *
ensure_uri (const gchar * location)
{
  if (gst_uri_is_valid (location))
    return g_strdup (location);
  else
    return gst_filename_to_uri (location, NULL);
}

gchar *
get_file_extension (gchar * uri)
{
  size_t len;
  gint find;

  len = strlen (uri);
  find = len - 1;

  while (find >= 0) {
    if (uri[find] == '.')
      break;
    find--;
  }

  if (find < 0)
    return NULL;

  return &uri[find + 1];
}

GList *
get_usable_profiles (GstEncodingTarget * target)
{
  GList *tmpprof, *usable_profiles = NULL;

  for (tmpprof = (GList *) gst_encoding_target_get_profiles (target);
      tmpprof; tmpprof = tmpprof->next) {
    GstEncodingProfile *profile = tmpprof->data;
    GstElement *tmpencodebin = gst_element_factory_make ("encodebin", NULL);

    gst_encoding_profile_set_presence (profile, 1);
    if (GST_IS_ENCODING_CONTAINER_PROFILE (profile)) {
      GList *tmpsubprof;
      for (tmpsubprof = (GList *)
          gst_encoding_container_profile_get_profiles
          (GST_ENCODING_CONTAINER_PROFILE (profile)); tmpsubprof;
          tmpsubprof = tmpsubprof->next)
        gst_encoding_profile_set_presence (tmpsubprof->data, 1);
    }

    g_object_set (tmpencodebin, "profile", gst_object_ref (profile), NULL);
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (tmpencodebin),
        GST_DEBUG_GRAPH_SHOW_ALL, gst_encoding_profile_get_name (profile));

    /* The profile could be expended */
    if (GST_BIN (tmpencodebin)->children)
      usable_profiles = g_list_prepend (usable_profiles, profile);

    gst_object_unref (tmpencodebin);
  }

  return usable_profiles;
}

GstEncodingProfile *
create_encoding_profile (const gchar * pname)
{
  GstEncodingProfile *profile;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, GST_TYPE_ENCODING_PROFILE);

  if (!gst_value_deserialize (&value, pname)) {
    g_value_reset (&value);

    return NULL;
  }

  profile = g_value_dup_object (&value);
  g_value_reset (&value);

  return profile;
}

static const gchar *
get_profile_type (GstEncodingProfile * profile)
{
  if (GST_IS_ENCODING_CONTAINER_PROFILE (profile))
    return "Container";
  else if (GST_IS_ENCODING_AUDIO_PROFILE (profile))
    return "Audio";
  else if (GST_IS_ENCODING_VIDEO_PROFILE (profile))
    return "Video";
  else
    return "Unknown";
}

static void
print_profile (GstEncodingProfile * profile, const gchar * prefix)
{
  const gchar *name = gst_encoding_profile_get_name (profile);
  const gchar *desc = gst_encoding_profile_get_description (profile);
  GstCaps *format = gst_encoding_profile_get_format (profile);
  gchar *capsdesc;

  if (gst_caps_is_fixed (format))
    capsdesc = gst_pb_utils_get_codec_description (format);
  else
    capsdesc = gst_caps_to_string (format);

  g_print ("%s%s: %s%s%s%s%s%s\n", prefix, get_profile_type (profile),
      name ? name : capsdesc, desc ? ": " : "", desc ? desc : "",
      name ? " (" : "", name ? capsdesc : "", name ? ")" : "");

  g_free (capsdesc);
}

void
describe_encoding_profile (GstEncodingProfile * profile)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  print_profile (profile, "  ");
  if (GST_IS_ENCODING_CONTAINER_PROFILE (profile)) {
    const GList *tmp;

    for (tmp =
        gst_encoding_container_profile_get_profiles
        (GST_ENCODING_CONTAINER_PROFILE (profile)); tmp; tmp = tmp->next)
      print_profile (tmp->data, "    - ");
  }

}
