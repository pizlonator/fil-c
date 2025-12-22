/*
 * Copyright (C) 2015 Arun Raghavan <git@arunraghavan.net>
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstavrcputil.h"
#include "bluez.h"

#include <gio/gio.h>

#define BLUEZ_NAME "org.bluez"
#define BLUEZ_PATH "/"
#define BLUEZ_MEDIA_PLAYER_IFACE BLUEZ_NAME ".MediaPlayer1"

struct _GstAvrcpConnection
{
  GMainContext *context;
  GMainLoop *mainloop;
  GThread *thread;

  gchar *dev_path;
  GDBusObjectManager *manager;
  BluezMediaPlayer1 *player;

  GstAvrcpMetadataCb cb;
  gpointer user_data;
  GDestroyNotify user_data_free_cb;
};

static const char *
tag_from_property (const char *name)
{
  if (g_str_equal (name, "Title"))
    return GST_TAG_TITLE;
  else if (g_str_equal (name, "Artist"))
    return GST_TAG_ARTIST;
  else if (g_str_equal (name, "Album"))
    return GST_TAG_ALBUM;
  else if (g_str_equal (name, "Genre"))
    return GST_TAG_GENRE;
  else if (g_str_equal (name, "NumberOfTracks"))
    return GST_TAG_TRACK_COUNT;
  else if (g_str_equal (name, "TrackNumber"))
    return GST_TAG_TRACK_NUMBER;
  else if (g_str_equal (name, "Duration"))
    return GST_TAG_DURATION;
  else
    return NULL;
}

static GstTagList *
tag_list_from_variant (GVariant * properties, gboolean track)
{
  const gchar *name, *s;
  GVariant *value;
  GVariantIter *iter;
  GstTagList *taglist = NULL;

  iter = g_variant_iter_new (properties);

  if (track)
    taglist = gst_tag_list_new_empty ();

  /* The properties are in two levels -- at the top level we have the position
   * and the 'track'. The 'track' is another level of {sv} so we recurse one
   * level to pick up the actual track data. We get the taglist from the
   * recursive call, and ignore the position for now. */

  while (g_variant_iter_next (iter, "{&sv}", &name, &value)) {
    if (!track && g_str_equal (name, "Track")) {
      /* Top level property */
      taglist = tag_list_from_variant (value, TRUE);

    } else if (track) {
      /* If we get here, we are in the recursive call and we're dealing with
       * properties under "Track" */
      GType type;
      const gchar *tag;
      guint i;
      guint64 i64;

      tag = tag_from_property (name);
      if (!tag)
        goto next;

      type = gst_tag_get_type (tag);

      switch (type) {
        case G_TYPE_STRING:
          s = g_variant_get_string (value, NULL);
          if (s && s[0] != '\0')
            gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE, tag, s, NULL);
          break;

        case G_TYPE_UINT:
          i = g_variant_get_uint32 (value);
          if (i > 0)
            gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE, tag, i, NULL);
          break;

        case G_TYPE_UINT64:
          /* If we're here, the tag is 'duration' */
          i64 = g_variant_get_uint32 (value);
          if (i64 > 0 && i64 != (guint32) (-1)) {
            gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE, tag,
                i64 * GST_MSECOND, NULL);
          }
          break;

        default:
          GST_WARNING ("Unknown property: %s", name);
          break;
      }
    }

  next:
    g_variant_unref (value);
  }

  g_variant_iter_free (iter);

  if (taglist && gst_tag_list_is_empty (taglist)) {
    gst_tag_list_unref (taglist);
    taglist = NULL;
  }

  return taglist;
}

static void
player_property_changed_cb (GDBusProxy * proxy, GVariant * properties,
    GStrv invalid, gpointer user_data)
{
  GstAvrcpConnection *avrcp = (GstAvrcpConnection *) user_data;
  GstTagList *taglist;

  taglist = tag_list_from_variant (properties, FALSE);

  if (taglist)
    avrcp->cb (avrcp, taglist, avrcp->user_data);
}

static GstTagList *
player_get_taglist (BluezMediaPlayer1 * player)
{
  GstTagList *taglist = NULL;
  GVariant *track;

  track = bluez_media_player1_get_track (player);
  if (track)
    taglist = tag_list_from_variant (track, TRUE);

  return taglist;
}

static void
gst_avrcp_connection_set_player (GstAvrcpConnection * avrcp,
    BluezMediaPlayer1 * player)
{
  GstTagList *taglist;

  if (avrcp->player)
    g_object_unref (avrcp->player);

  if (!player) {
    avrcp->player = NULL;
    return;
  }

  avrcp->player = g_object_ref (player);

  g_signal_connect (player, "g-properties-changed",
      G_CALLBACK (player_property_changed_cb), avrcp);

  taglist = player_get_taglist (avrcp->player);

  if (taglist)
    avrcp->cb (avrcp, taglist, avrcp->user_data);
}

static BluezMediaPlayer1 *
media_player_from_dbus_object (GDBusObject * object)
{
  return (BluezMediaPlayer1 *) g_dbus_object_get_interface (object,
      BLUEZ_MEDIA_PLAYER_IFACE);
}

static GType
manager_proxy_type_func (GDBusObjectManagerClient * manager,
    const gchar * object_path, const gchar * interface_name, gpointer user_data)
{
  if (!interface_name)
    return G_TYPE_DBUS_OBJECT_PROXY;

  if (g_str_equal (interface_name, BLUEZ_MEDIA_PLAYER_IFACE))
    return BLUEZ_TYPE_MEDIA_PLAYER1_PROXY;

  return G_TYPE_DBUS_PROXY;
}

static void
manager_object_added_cb (GDBusObjectManager * manager,
    GDBusObject * object, gpointer user_data)
{
  GstAvrcpConnection *avrcp = (GstAvrcpConnection *) user_data;
  BluezMediaPlayer1 *player;

  if (!(player = media_player_from_dbus_object (object)))
    return;

  gst_avrcp_connection_set_player (avrcp, player);
}

static void
manager_object_removed_cb (GDBusObjectManager * manager,
    GDBusObject * object, gpointer user_data)
{
  GstAvrcpConnection *avrcp = (GstAvrcpConnection *) user_data;
  BluezMediaPlayer1 *player;

  if (!(player = media_player_from_dbus_object (object)))
    return;

  if (player == avrcp->player)
    gst_avrcp_connection_set_player (avrcp, NULL);
}

static void
manager_ready_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
  GstAvrcpConnection *avrcp = (GstAvrcpConnection *) user_data;
  GList *objects, *i;
  GError *err = NULL;

  avrcp->manager = g_dbus_object_manager_client_new_for_bus_finish (res, &err);
  if (!avrcp->manager) {
    GST_WARNING ("Could not create ObjectManager proxy: %s", err->message);
    g_error_free (err);
    return;
  }

  g_signal_connect (avrcp->manager, "object-added",
      G_CALLBACK (manager_object_added_cb), avrcp);
  g_signal_connect (avrcp->manager, "object-removed",
      G_CALLBACK (manager_object_removed_cb), avrcp);

  objects = g_dbus_object_manager_get_objects (avrcp->manager);

  for (i = objects; i; i = i->next) {
    BluezMediaPlayer1 *player =
        media_player_from_dbus_object (G_DBUS_OBJECT (i->data));

    if (player && g_str_equal (avrcp->dev_path,
            bluez_media_player1_get_device (player))) {
      gst_avrcp_connection_set_player (avrcp, player);
      break;
    }
  }

  g_list_free_full (objects, g_object_unref);
}

GstAvrcpConnection *
gst_avrcp_connection_new (const gchar * dev_path, GstAvrcpMetadataCb cb,
    gpointer user_data, GDestroyNotify user_data_free_cb)
{
  GstAvrcpConnection *avrcp;

  avrcp = g_new0 (GstAvrcpConnection, 1);

  avrcp->cb = cb;
  avrcp->user_data = user_data;
  avrcp->user_data_free_cb = user_data_free_cb;

  avrcp->context = g_main_context_new ();
  avrcp->mainloop = g_main_loop_new (avrcp->context, FALSE);

  avrcp->dev_path = g_strdup (dev_path);

  g_main_context_push_thread_default (avrcp->context);

  g_dbus_object_manager_client_new_for_bus (G_BUS_TYPE_SYSTEM,
      G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE, BLUEZ_NAME, BLUEZ_PATH,
      manager_proxy_type_func, NULL, NULL, NULL, manager_ready_cb, avrcp);

  g_main_context_pop_thread_default (avrcp->context);

  avrcp->thread = g_thread_new ("gstavrcp", (GThreadFunc) g_main_loop_run,
      avrcp->mainloop);

  return avrcp;
}

void
gst_avrcp_connection_free (GstAvrcpConnection * avrcp)
{
  g_main_loop_quit (avrcp->mainloop);
  g_main_loop_unref (avrcp->mainloop);

  g_main_context_unref (avrcp->context);

  g_thread_join (avrcp->thread);

  if (avrcp->player)
    g_object_unref (avrcp->player);

  if (avrcp->manager)
    g_object_unref (avrcp->manager);

  if (avrcp->user_data_free_cb)
    avrcp->user_data_free_cb (avrcp->user_data);

  g_free (avrcp->dev_path);
  g_free (avrcp);
}
