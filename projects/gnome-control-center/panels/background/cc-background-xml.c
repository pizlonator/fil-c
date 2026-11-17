/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *  Bastien Nocera <hadess@hadess.net>
 *
 *  Copyright 2003-2006 Novell, Inc. (www.novell.com)
 *  Copyright 2011 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <string.h>
#include <libxml/parser.h>
#include <gdesktop-enums.h>

#include "gdesktop-enums-types.h"
#include "cc-background-item.h"
#include "cc-background-xml.h"

/* The number of items we signal as "added" before
 * returning to the main loop */
#define NUM_ITEMS_PER_BATCH 1

struct _CcBackgroundXml
{
  GObject      parent_instance;

  GHashTable  *wp_hash;
  GAsyncQueue *item_added_queue;
  guint        item_added_id;
  GSList      *monitors; /* GSList of GFileMonitor */
};

enum {
	ADDED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (CcBackgroundXml, cc_background_xml, G_TYPE_OBJECT)

static gboolean
cc_background_xml_get_bool (const xmlNode *parent,
			    const gchar   *prop_name)
{
  xmlChar *prop;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (parent != NULL, FALSE);
  g_return_val_if_fail (prop_name != NULL, FALSE);

  prop = xmlGetProp ((xmlNode *) parent, (xmlChar*)prop_name);
  if (prop != NULL) {
    if (!g_ascii_strcasecmp ((gchar *)prop, "true") || !g_ascii_strcasecmp ((gchar *)prop, "1")) {
      ret_val = TRUE;
    } else {
      ret_val = FALSE;
    }
    xmlFree (prop);
  }

  return ret_val;
}

static struct {
	int value;
	const char *string;
} lookups[] = {
	{ G_DESKTOP_BACKGROUND_SHADING_HORIZONTAL, "horizontal-gradient" },
	{ G_DESKTOP_BACKGROUND_SHADING_VERTICAL, "vertical-gradient" },
};

static int
enum_string_to_value (GType type,
		      const char *string)
{
	GEnumClass *eclass;
	GEnumValue *value;

	eclass = G_ENUM_CLASS (g_type_class_peek (type));
	value = g_enum_get_value_by_nick (eclass, string);

	/* Here's a bit of hand-made parsing, bad bad */
	if (value == NULL) {
		guint i;
		for (i = 0; i < G_N_ELEMENTS (lookups); i++) {
			if (g_str_equal (lookups[i].string, string))
				return lookups[i].value;
		}
		g_warning ("Unhandled value '%s' for enum '%s'",
			   string, G_FLAGS_CLASS_TYPE_NAME (eclass));
		return 0;
	}

	return value->value;
}

static gboolean
idle_emit (CcBackgroundXml *xml)
{
	gint i;

	g_async_queue_lock (xml->item_added_queue);

	for (i = 0; i < NUM_ITEMS_PER_BATCH; i++) {
		g_autoptr(GObject) item = NULL;

		item = g_async_queue_try_pop_unlocked (xml->item_added_queue);
		if (item == NULL)
			break;
		g_signal_emit (G_OBJECT (xml), signals[ADDED], 0, item);
	}

	g_async_queue_unlock (xml->item_added_queue);

        if (g_async_queue_length (xml->item_added_queue) > 0) {
                return TRUE;
        } else {
                xml->item_added_id = 0;
                return FALSE;
        }
}

static void
emit_added_in_idle (CcBackgroundXml *xml,
		    GObject         *object)
{
	g_async_queue_lock (xml->item_added_queue);
	g_async_queue_push_unlocked (xml->item_added_queue, object);
	if (xml->item_added_id == 0)
		xml->item_added_id = g_idle_add ((GSourceFunc) idle_emit, xml);
	g_async_queue_unlock (xml->item_added_queue);
}

#define NONE "(none)"

static gboolean
cc_background_xml_load_xml_internal (CcBackgroundXml *xml,
				     const gchar     *filename,
				     gboolean         in_thread)
{
  xmlDoc * wplist;
  xmlNode * root, * list, * wpa;
  xmlChar * nodelang;
  const gchar * const * syslangs;
  gint i;
  gboolean retval;

  wplist = xmlParseFile (filename);
  retval = FALSE;

  if (!wplist)
    return retval;

  syslangs = g_get_language_names ();

  root = xmlDocGetRootElement (wplist);

  for (list = root->children; list != NULL; list = list->next) {
    if (!strcmp ((gchar *)list->name, "wallpaper")) {
      g_autoptr(CcBackgroundItem) item = NULL;
      g_autofree gchar *uri = NULL;
      g_autofree gchar *cname = NULL;
      g_autofree gchar *id = NULL;

      item = cc_background_item_new (NULL);

      g_object_set (G_OBJECT (item),
		    "is-deleted", cc_background_xml_get_bool (list, "deleted"),
		    "source-xml", filename,
		    NULL);

      for (wpa = list->children; wpa != NULL; wpa = wpa->next) {
	if (wpa->type == XML_COMMENT_NODE) {
	  continue;
	} else if (!strcmp ((gchar *)wpa->name, "filename")) {
	  if (wpa->last != NULL && wpa->last->content != NULL) {
	    gchar *content = g_strstrip ((gchar *)wpa->last->content);
	    g_autofree gchar *bg_uri = NULL;

	    /* FIXME same rubbish as in other parts of the code */
	    if (strcmp (content, NONE) == 0) {
	      bg_uri = NULL;
	    } else {
	      g_autoptr(GFile) file = NULL;
	      g_autofree gchar *dirname = NULL;

	      dirname = g_path_get_dirname (filename);
	      file = g_file_new_for_commandline_arg_and_cwd (content, dirname);
	      bg_uri = g_file_get_uri (file);
	    }
	    g_object_set (G_OBJECT (item), "uri", bg_uri, NULL);
	  } else {
	    break;
	  }
	} else if (!strcmp ((gchar *)wpa->name, "filename-dark")) {
	  if (wpa->last != NULL && wpa->last->content != NULL) {
	    gchar *content = g_strstrip ((gchar *)wpa->last->content);
	    g_autofree gchar *bg_uri = NULL;

	    /* FIXME same rubbish as in other parts of the code */
	    if (strcmp (content, NONE) == 0) {
	      bg_uri = NULL;
	    } else {
	      g_autoptr(GFile) file = NULL;
	      g_autofree gchar *dirname = NULL;

	      dirname = g_path_get_dirname (filename);
	      file = g_file_new_for_commandline_arg_and_cwd (content, dirname);
	      bg_uri = g_file_get_uri (file);
	    }
	    g_object_set (G_OBJECT (item), "uri-dark", bg_uri, NULL);
	  } else {
	    break;
	  }
	} else if (!strcmp ((gchar *)wpa->name, "name")) {
	  if (wpa->last != NULL && wpa->last->content != NULL) {
	    g_autofree gchar *name = NULL;
	    nodelang = xmlNodeGetLang (wpa->last);

	    g_object_get (G_OBJECT (item), "name", &name, NULL);

	    if (name == NULL && nodelang == NULL) {
	       g_free (cname);
	       cname = g_strdup (g_strstrip ((gchar *)wpa->last->content));
	       g_object_set (G_OBJECT (item), "name", cname, NULL);
            } else {
	       for (i = 0; syslangs[i] != NULL; i++) {
	         if (!strcmp (syslangs[i], (gchar *)nodelang)) {
		   g_object_set (G_OBJECT (item), "name",
				 g_strstrip ((gchar *)wpa->last->content), NULL);
	           break;
	         }
	       }
	    }

	    xmlFree (nodelang);
	  } else {
	    break;
	  }
	} else if (!strcmp ((gchar *)wpa->name, "options")) {
	  if (wpa->last != NULL) {
	    g_object_set (G_OBJECT (item), "placement",
			  enum_string_to_value (G_DESKTOP_TYPE_DESKTOP_BACKGROUND_STYLE,
						g_strstrip ((gchar *)wpa->last->content)), NULL);
	  }
	} else if (!strcmp ((gchar *)wpa->name, "shade_type")) {
	  if (wpa->last != NULL) {
	    g_object_set (G_OBJECT (item), "shading",
			  enum_string_to_value (G_DESKTOP_TYPE_DESKTOP_BACKGROUND_SHADING,
						g_strstrip ((gchar *)wpa->last->content)), NULL);
	  }
	} else if (!strcmp ((gchar *)wpa->name, "pcolor")) {
	  if (wpa->last != NULL) {
	    g_object_set (G_OBJECT (item), "primary-color",
			  g_strstrip ((gchar *)wpa->last->content), NULL);
	  }
	} else if (!strcmp ((gchar *)wpa->name, "scolor")) {
	  if (wpa->last != NULL) {
	    g_object_set (G_OBJECT (item), "secondary-color",
			  g_strstrip ((gchar *)wpa->last->content), NULL);
	  }
	} else if (!strcmp ((gchar *)wpa->name, "source_url")) {
	   if (wpa->last != NULL) {
             g_object_set (G_OBJECT (item),
			   "source-url", g_strstrip ((gchar *)wpa->last->content),
			   "needs-download", FALSE,
			   NULL);
	   }
	} else if (!strcmp ((gchar *)wpa->name, "text")) {
	  /* Do nothing here, libxml2 is being weird */
	} else {
	  g_debug ("Unknown Tag in %s: %s", filename, wpa->name);
	}
      }

      /* Check whether the target file exists */
      {
        const char *uri;

	uri = cc_background_item_get_uri (item);
	if (uri != NULL)
	  {
            g_autoptr(GFile) file = NULL;

            file = g_file_new_for_uri (uri);
	    if (g_file_query_exists (file, NULL) == FALSE)
	      {
	        g_clear_pointer (&cname, g_free);
	        g_clear_object (&item);
	        continue;
	      }
	  }
      }

      /* FIXME, this is a broken way of doing,
       * need to use proper code here */
      uri = g_filename_to_uri (filename, NULL, NULL);
      id = g_strdup_printf ("%s#%s", uri, cname);

      /* Make sure we don't already have this one and that filename exists */
      if (g_hash_table_lookup (xml->wp_hash, id) != NULL) {
	continue;
      }

      g_hash_table_insert (xml->wp_hash,
                           g_strdup (id),
                           g_object_ref (item));
      if (in_thread)
        emit_added_in_idle (xml, g_object_ref (G_OBJECT (item)));
      else
        g_signal_emit (G_OBJECT (xml), signals[ADDED], 0, item);
      retval = TRUE;
    }
  }
  xmlFreeDoc (wplist);

  return retval;
}

static void
gnome_wp_file_changed (CcBackgroundXml *xml,
		       GFile *file,
		       GFile *other_file,
		       GFileMonitorEvent event_type)
{
  g_autofree gchar *filename = NULL;

  switch (event_type) {
  case G_FILE_MONITOR_EVENT_CHANGED:
  case G_FILE_MONITOR_EVENT_CREATED:
    filename = g_file_get_path (file);
    cc_background_xml_load_xml_internal (xml, filename, FALSE);
    break;
  default:
    break;
  }
}

static void
cc_background_xml_add_monitor (GFile      *directory,
			       CcBackgroundXml *data)
{
  GFileMonitor *monitor;
  g_autoptr(GError) error = NULL;

  monitor = g_file_monitor_directory (directory,
                                      G_FILE_MONITOR_NONE,
                                      NULL,
                                      &error);
  if (error != NULL) {
    g_autofree gchar *path = NULL;

    path = g_file_get_parse_name (directory);
    g_warning ("Unable to monitor directory %s: %s",
               path, error->message);
    return;
  }

  g_signal_connect_swapped (monitor, "changed",
                            G_CALLBACK (gnome_wp_file_changed),
                            data);

  data->monitors = g_slist_prepend (data->monitors, monitor);
}

static void
cc_background_xml_load_from_dir (const gchar      *path,
				 CcBackgroundXml  *data,
				 gboolean          in_thread)
{
  g_autoptr(GFile) directory = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) error = NULL;

  if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
    return;
  }

  directory = g_file_new_for_path (path);
  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);
  if (error != NULL) {
    g_warning ("Unable to check directory %s: %s", path, error->message);
    return;
  }

  while (TRUE) {
    g_autoptr(GFileInfo) info = NULL;
    const gchar *filename;
    g_autofree gchar *fullpath = NULL;

    info = g_file_enumerator_next_file (enumerator, NULL, NULL);
    if (info == NULL) {
        g_file_enumerator_close (enumerator, NULL, NULL);
        cc_background_xml_add_monitor (directory, data);
        return;
    }

    filename = g_file_info_get_name (info);
    fullpath = g_build_filename (path, filename, NULL);

    cc_background_xml_load_xml_internal (data, fullpath, in_thread);
  }
}

static void
cc_background_xml_load_list (CcBackgroundXml *data,
			     gboolean         in_thread)
{
  const char * const *system_data_dirs;
  g_autofree gchar *datadir = NULL;
  gint i;

  datadir = g_build_filename (g_get_user_data_dir (),
                              "gnome-background-properties",
                              NULL);
  cc_background_xml_load_from_dir (datadir, data, in_thread);

  system_data_dirs = g_get_system_data_dirs ();
  for (i = 0; system_data_dirs[i]; i++) {
    g_autofree gchar *sdatadir = NULL;
    sdatadir = g_build_filename (system_data_dirs[i],
                                "gnome-background-properties",
				NULL);
    cc_background_xml_load_from_dir (sdatadir, data, in_thread);
  }
}

gboolean
cc_background_xml_load_list_finish (CcBackgroundXml *xml,
				    GAsyncResult    *result,
				    GError         **error)
{
	g_return_val_if_fail (g_task_is_valid (result, xml), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
load_list_thread (GTask *task,
		  gpointer source_object,
		  gpointer task_data,
		  GCancellable *cancellable)
{
	CcBackgroundXml *xml = CC_BACKGROUND_XML (source_object);
	cc_background_xml_load_list (xml, TRUE);
	g_task_return_boolean (task, TRUE);
}

void
cc_background_xml_load_list_async (CcBackgroundXml *xml,
				   GCancellable *cancellable,
				   GAsyncReadyCallback callback,
				   gpointer user_data)
{
	g_autoptr(GTask) task = NULL;

	g_return_if_fail (CC_IS_BACKGROUND_XML (xml));

	task = g_task_new (xml, cancellable, callback, user_data);
	g_task_run_in_thread (task, load_list_thread);
}

gboolean
cc_background_xml_load_xml (CcBackgroundXml *xml,
			    const gchar     *filename)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_XML (xml), FALSE);

	if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) == FALSE)
		return FALSE;

	return cc_background_xml_load_xml_internal (xml, filename, FALSE);
}

static void
single_xml_added (CcBackgroundXml   *xml,
		  CcBackgroundItem  *item,
		  CcBackgroundItem **ret)
{
	g_assert (*ret == NULL);
	*ret = g_object_ref (item);
}

CcBackgroundItem *
cc_background_xml_get_item (const char *filename)
{
	g_autoptr(CcBackgroundXml) xml = NULL;
	CcBackgroundItem *item = NULL;

	if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) == FALSE)
		return NULL;

	xml = cc_background_xml_new ();
	g_signal_connect (G_OBJECT (xml), "added",
			  G_CALLBACK (single_xml_added), &item);
	if (cc_background_xml_load_xml (xml, filename) == FALSE)
		return NULL;

	return item;
}

static const char *
enum_to_str (GType type,
	     int   v)
{
	GEnumClass *eclass;
	GEnumValue *value;

	eclass = G_ENUM_CLASS (g_type_class_peek (type));
	value = g_enum_get_value (eclass, v);

	g_assert (value);

	return value->value_nick;
}

void
cc_background_xml_save (CcBackgroundItem *item,
			const char       *filename)
{
  xmlDoc *wp;
  xmlNode *root, *wallpaper;
  xmlNode *xml_item G_GNUC_UNUSED;
  const char * none = "(none)";
  const char *placement_str, *shading_str;
  g_autofree gchar *name = NULL;
  g_autofree gchar *pcolor = NULL;
  g_autofree gchar *scolor = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *source_url = NULL;
  CcBackgroundItemFlags flags;
  GDesktopBackgroundStyle placement;
  GDesktopBackgroundShading shading;

  xmlKeepBlanksDefault (0);

  wp = xmlNewDoc ((xmlChar *)"1.0");
  xmlCreateIntSubset (wp, (xmlChar *)"wallpapers", NULL, (xmlChar *)"gnome-wp-list.dtd");
  root = xmlNewNode (NULL, (xmlChar *)"wallpapers");
  xmlDocSetRootElement (wp, root);

  g_object_get (G_OBJECT (item),
		"name", &name,
		"uri", &uri,
		"shading", &shading,
		"placement", &placement,
		"primary-color", &pcolor,
		"secondary-color", &scolor,
		"source-url", &source_url,
		"flags", &flags,
		NULL);

  placement_str = enum_to_str (G_DESKTOP_TYPE_DESKTOP_BACKGROUND_STYLE, placement);
  shading_str = enum_to_str (G_DESKTOP_TYPE_DESKTOP_BACKGROUND_SHADING, shading);

  wallpaper = xmlNewChild (root, NULL, (xmlChar *)"wallpaper", NULL);
  xml_item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"name", (xmlChar *)name);
  if (flags & CC_BACKGROUND_ITEM_HAS_URI &&
      uri != NULL)
    {
      g_autoptr(GFile) file = NULL;
      g_autofree gchar *fname = NULL;

      file = g_file_new_for_commandline_arg (uri);
      fname = g_file_get_path (file);
      xml_item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"filename", (xmlChar *)fname);
    }
  else if (flags & CC_BACKGROUND_ITEM_HAS_URI)
    {
      xml_item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"filename", (xmlChar *)none);
    }

  if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT)
    xml_item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"options", (xmlChar *)placement_str);
  if (flags & CC_BACKGROUND_ITEM_HAS_SHADING)
    xml_item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"shade_type", (xmlChar *)shading_str);
  if (flags & CC_BACKGROUND_ITEM_HAS_PCOLOR)
    xml_item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"pcolor", (xmlChar *)pcolor);
  if (flags & CC_BACKGROUND_ITEM_HAS_SCOLOR)
    xml_item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"scolor", (xmlChar *)scolor);
  if (source_url != NULL)
    xml_item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"source_url", (xmlChar *)source_url);

  xmlSaveFormatFile (filename, wp, 1);
  xmlFreeDoc (wp);
}

static void
cc_background_xml_finalize (GObject *object)
{
        CcBackgroundXml *xml;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_BACKGROUND_XML (object));

        xml = CC_BACKGROUND_XML (object);

        g_slist_free_full (xml->monitors, g_object_unref);

	g_clear_pointer (&xml->wp_hash, g_hash_table_destroy);
	g_clear_handle_id (&xml->item_added_id, g_source_remove);
	g_clear_pointer (&xml->item_added_queue, g_async_queue_unref);

        G_OBJECT_CLASS (cc_background_xml_parent_class)->finalize (object);
}

static void
cc_background_xml_class_init (CcBackgroundXmlClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = cc_background_xml_finalize;

	signals[ADDED] = g_signal_new ("added",
				       G_OBJECT_CLASS_TYPE (object_class),
				       G_SIGNAL_RUN_LAST,
				       0,
				       NULL, NULL,
				       g_cclosure_marshal_VOID__OBJECT,
				       G_TYPE_NONE, 1, CC_TYPE_BACKGROUND_ITEM);
}

static void
cc_background_xml_init (CcBackgroundXml *xml)
{
        xml->wp_hash = g_hash_table_new_full (g_str_hash,
                                              g_str_equal,
                                              (GDestroyNotify) g_free,
                                              (GDestroyNotify) g_object_unref);
	xml->item_added_queue = g_async_queue_new_full ((GDestroyNotify) g_object_unref);
}

CcBackgroundXml *
cc_background_xml_new (void)
{
	return CC_BACKGROUND_XML (g_object_new (CC_TYPE_BACKGROUND_XML, NULL));
}
