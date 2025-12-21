/* GStreamer
 * Copyright (C) 2004 Benjamin Otte <otte@gnome.org>
 *               2012 Stefan Sauer <ensonic@users.sf.net>
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
#include "config.h"
#endif

#include "visual.h"

GST_DEBUG_CATEGORY (libvisual_debug);
#define GST_CAT_DEFAULT (libvisual_debug)

static void
libvisual_log_handler (const char *message, const char *funcname, void *priv)
{
  GST_CAT_LEVEL_LOG (libvisual_debug, (GstDebugLevel) GPOINTER_TO_INT (priv),
      NULL, "%s - %s", funcname, message);
}

/*
 * Replace invalid chars with _ in the type name
 */
static void
make_valid_name (gchar * name)
{
  static const gchar extra_chars[] = "-_+";
  gchar *p = name;

  for (; *p; p++) {
    gint valid = ((p[0] >= 'A' && p[0] <= 'Z') ||
        (p[0] >= 'a' && p[0] <= 'z') ||
        (p[0] >= '0' && p[0] <= '9') || strchr (extra_chars, p[0]));
    if (!valid)
      *p = '_';
  }
}

static gboolean
gst_visual_actor_plugin_is_gl (VisObject * plugin, const gchar * name)
{
  gboolean is_gl;
  gint depth;

  depth = VISUAL_ACTOR_PLUGIN (plugin)->vidoptions.depth;
  /* FIXME: how to figure this out correctly in 0.4? */
  is_gl = (depth & VISUAL_VIDEO_DEPTH_GL) == VISUAL_VIDEO_DEPTH_GL;

  if (!is_gl) {
    GST_DEBUG ("plugin %s is not a GL plugin (%d), registering", name, depth);
  } else {
    GST_DEBUG ("plugin %s is a GL plugin (%d), ignoring", name, depth);
  }

  return is_gl;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  guint i, count;
  VisList *list;

  GST_DEBUG_CATEGORY_INIT (libvisual_debug, "libvisual", 0,
      "libvisual audio visualisations");

#ifdef LIBVISUAL_PLUGINSBASEDIR
  gst_plugin_add_dependency_simple (plugin, "HOME/.libvisual/actor",
      LIBVISUAL_PLUGINSBASEDIR "/actor", NULL, GST_PLUGIN_DEPENDENCY_FLAG_NONE);
#endif

  visual_log_set_verboseness (VISUAL_LOG_VERBOSENESS_LOW);
  visual_log_set_info_handler (libvisual_log_handler,
      GINT_TO_POINTER (GST_LEVEL_INFO));
  visual_log_set_warning_handler (libvisual_log_handler,
      GINT_TO_POINTER (GST_LEVEL_WARNING));
  visual_log_set_critical_handler (libvisual_log_handler,
      GINT_TO_POINTER (GST_LEVEL_ERROR));
  visual_log_set_error_handler (libvisual_log_handler,
      GINT_TO_POINTER (GST_LEVEL_ERROR));

  if (!visual_is_initialized ())
    if (visual_init (NULL, NULL) != 0)
      return FALSE;

  list = visual_actor_get_list ();

  count = visual_collection_size (VISUAL_COLLECTION (list));

  for (i = 0; i < count; i++) {
    VisPluginRef *ref = visual_list_get (list, i);
    VisPluginData *visplugin = NULL;
    gboolean skip = FALSE;
    GType type;
    gchar *name;
    GTypeInfo info = {
      sizeof (GstVisualClass),
      NULL,
      NULL,
      gst_visual_class_init,
      NULL,
      ref,
      sizeof (GstVisual),
      0,
      NULL
    };

    visplugin = visual_plugin_load (ref);

    if (ref->info->plugname == NULL)
      continue;

    /* Blacklist some plugins */
    if (strcmp (ref->info->plugname, "gstreamer") == 0 ||
        strcmp (ref->info->plugname, "gdkpixbuf") == 0) {
      skip = TRUE;
    } else {
      /* Ignore plugins that only support GL output for now */
      skip = gst_visual_actor_plugin_is_gl (visplugin->info->plugin,
          visplugin->info->plugname);
    }

    visual_plugin_unload (visplugin);

    if (!skip) {
      name = g_strdup_printf ("GstVisual%s", ref->info->plugname);
      make_valid_name (name);
      type = g_type_register_static (GST_TYPE_VISUAL, name, &info, 0);
      g_free (name);

      name = g_strdup_printf ("libvisual_%s", ref->info->plugname);
      make_valid_name (name);
      if (!gst_element_register (plugin, name, GST_RANK_NONE, type)) {
        g_free (name);
        return FALSE;
      }
      g_free (name);
    }
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    libvisual,
    "libvisual visualization plugins",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
