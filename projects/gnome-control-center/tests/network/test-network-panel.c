/*
 * Copyright (c) 2010-2014, 2018 Red Hat, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Benjamin Berg <bberg@redhat.com>
 */

#define G_LOG_DOMAIN "test-network-panel"

#include "nm-macros-internal.h"

#include <NetworkManager.h>
#include <nm-client.h>
#include <nm-utils.h>

#include "nm-test-libnm-utils.h"

#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <gtk/gtk.h>

#include "cc-test-window.h"
#include "shell/cc-object-storage.h"

#include "nmtst-helpers.h"

typedef struct {
  NMTstcServiceInfo *sinfo;
  NMClient *client;

  NMDevice *main_ether;

  GtkWindow *shell;
  CcPanel *panel;
} NetworkPanelFixture;


extern GType cc_network_panel_get_type (void);

static void
fixture_set_up_empty (NetworkPanelFixture  *fixture,
                      gconstpointer         user_data)
{
  g_autoptr(GError) error = NULL;

  cc_object_storage_initialize ();

  /* Bring up the libnm service. */
  fixture->sinfo = nmtstc_service_init ();

  fixture->client = nm_client_new (NULL, &error);
  g_assert_no_error (error);

  /* Insert into object storage so that we see the same events as the panel. */
  cc_object_storage_add_object (CC_OBJECT_NMCLIENT, fixture->client);

  fixture->shell = GTK_WINDOW (cc_test_window_new ());

  fixture->panel = g_object_new (cc_network_panel_get_type (),
                                 "shell", CC_SHELL (fixture->shell),
                                 NULL);

  g_object_ref (fixture->panel);
  cc_shell_set_active_panel (CC_SHELL (fixture->shell), fixture->panel);

  gtk_window_present (fixture->shell);
}

static void
fixture_tear_down (NetworkPanelFixture  *fixture,
                   gconstpointer         user_data)
{
  g_clear_object (&fixture->panel);
  g_clear_object (&fixture->client);
  g_clear_pointer (&fixture->shell, gtk_window_destroy);

  cc_object_storage_destroy ();

  g_clear_pointer (&fixture->sinfo, nmtstc_service_cleanup);
}

static void
fixture_set_up_wired (NetworkPanelFixture  *fixture,
                      gconstpointer         user_data)
{
  NMDevice *second;

  fixture_set_up_empty (fixture, user_data);

  fixture->main_ether = nmtstc_service_add_wired_device (fixture->sinfo,
                                                         fixture->client,
                                                         "eth1000",
                                                         "52:54:00:ab:db:23",
                                                         NULL);

  /* Add/remove one to catch issues with signal disconnects. */
  second = nmtstc_service_add_wired_device (fixture->sinfo,
                                            fixture->client,
                                            "eth1001",
                                            "52:54:00:ab:db:24",
                                            NULL);
  nmtst_remove_device (fixture->sinfo, fixture->client, second);
}

/*****************************************************************************/

static GtkWidget *
find_label (GtkWidget   *widget,
            const gchar *label_pattern)
{
  GtkWidget *child;
  GtkWidget *label = NULL;

  if (GTK_IS_LABEL (widget)) {
    const gchar *text = gtk_label_get_text (GTK_LABEL (widget));
    if (g_pattern_match_simple (label_pattern, text))
      return widget;
  }

  if (ADW_IS_PREFERENCES_ROW (widget)) {
    const gchar *text = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (widget));
    if (g_pattern_match_simple (label_pattern, text))
      return widget;
  }

  if (ADW_IS_ACTION_ROW (widget)) {
    const gchar *text = adw_action_row_get_subtitle (ADW_ACTION_ROW (widget));
    if (g_pattern_match_simple (label_pattern, text))
      return widget;
  }

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child)) {
    label = find_label (child, label_pattern);
    if (label)
      break;
  }

  return label;
}

static int
widget_geo_dist (GtkWidget *a,
                 GtkWidget *b,
                 GtkWidget *base)
{
  GtkAllocation allocation;
  graphene_point_t a0, a1, b0, b1;
  graphene_point_t dist = GRAPHENE_POINT_INIT (0, 0);

  gtk_widget_get_allocation (a, &allocation);
  if (!gtk_widget_compute_point (a, base, &GRAPHENE_POINT_INIT (0, 0), &a0) ||
      !gtk_widget_compute_point (a, base, &GRAPHENE_POINT_INIT (allocation.width, allocation.height), &a1))
    return -G_MAXINT;

  gtk_widget_get_allocation (b, &allocation);
  if (!gtk_widget_compute_point (b, base, &GRAPHENE_POINT_INIT (0, 0), &b0) ||
      !gtk_widget_compute_point (b, base, &GRAPHENE_POINT_INIT (allocation.width, allocation.height), &b1))
    return +G_MAXINT;

  if (b0.x >= a1.x)
    graphene_point_distance (&b0, &a1, &dist.x, NULL);
  else if (a0.x >= b1.x)
    graphene_point_distance (&a0, &b1, &dist.x, NULL);

  if (b0.y >= a1.y)
    graphene_point_distance (&b0, &a1, NULL, &dist.y);
  else if (a0.y >= b1.y)
    graphene_point_distance (&a0, &b1, NULL, &dist.y);

  return dist.x + dist.y;
}

static GList*
test_list_descendants (GtkWidget *widget,
                       GType      widget_type)
{
  GtkWidget *child;
  GList *results = NULL;

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child)) {
    if (!widget_type || g_type_is_a (G_OBJECT_TYPE (child), widget_type))
      results = g_list_prepend (results, child);
    else
      results = g_list_concat (results, test_list_descendants (child, widget_type));
  }

  return results;
}

static int
widget_geo_cmp (gconstpointer a,
                gconstpointer b,
                gpointer      user_data)
{
  gpointer *data = user_data;
  GtkWidget *wa = (void*) a, *wb = (void*) b, *toplevel = data[0], *base_widget = data[1];
  int adist = widget_geo_dist (wa, base_widget, toplevel);
  int bdist = widget_geo_dist (wb, base_widget, toplevel);
  return adist > bdist ? +1 : adist == bdist ? 0 : -1;
}

static GtkWidget *
find_sibling (GtkWidget *widget,
              GType      parent_type,
              GType      sibling_type)
{
  g_autoptr(GList) siblings = NULL;
  GtkWidget *tmpwidget = widget;
  gpointer data[2];

  /* find all sibling candidates */
  while ((tmpwidget = gtk_widget_get_parent (tmpwidget)) != NULL)
    {
      siblings = g_list_concat (siblings, test_list_descendants (tmpwidget, sibling_type));

      /* Stop searching further up if we reached the defined parent */
      if (parent_type && g_type_is_a (G_OBJECT_TYPE (tmpwidget), parent_type))
        break;
    }

  /* sort them by distance to base_widget */
  data[0] = gtk_widget_get_native (widget);
  data[1] = widget;
  siblings = g_list_sort_with_data (siblings, widget_geo_cmp, data);

  /* pick nearest != base_widget */
  siblings = g_list_remove (siblings, widget);
  tmpwidget = siblings ? siblings->data : NULL;

  return tmpwidget;
}

/*****************************************************************************/

#if 0  /* See /network-panel-wired/vpn-sorting note */
static GtkWidget*
find_parent_of_type(GtkWidget *widget, GType parent)
{
  while (widget) {
    widget = gtk_widget_get_parent (widget);
    if (G_TYPE_CHECK_INSTANCE_TYPE (G_OBJECT (widget), parent))
      return widget;
  }

  return NULL;
}
#endif

/*****************************************************************************/

static void
test_empty_ui (NetworkPanelFixture  *fixture,
               gconstpointer         user_data)
{
  GtkWidget *bt_header;
  GtkWidget *wired_header;

  /* There should be no Wired or Bluetooth sections */
  wired_header = find_label (GTK_WIDGET (fixture->shell), "Wired");
  g_assert_false (wired_header && gtk_widget_is_visible(wired_header));

  bt_header = find_label (GTK_WIDGET (fixture->shell), "Bluetooth");
  g_assert_false (bt_header && gtk_widget_is_visible(bt_header));
}

/*****************************************************************************/

static void
test_device_add (NetworkPanelFixture  *fixture,
                 gconstpointer         user_data)
{
  const gchar *device_path;

  /* Tell the test service to add a new device.
   * We use some weird numbers so that the devices will not exist on the
   * host system. */
  fixture->main_ether = nmtstc_service_add_wired_device (fixture->sinfo,
                                                         fixture->client,
                                                         "eth1000",
                                                         "52:54:00:ab:db:23",
                                                         NULL);
  device_path = nm_object_get_path (NM_OBJECT (fixture->main_ether));
  g_debug("Device added: %s\n", device_path);

  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "Wired"));
}

static void
test_second_device_add (NetworkPanelFixture  *fixture,
                        gconstpointer         user_data)
{
  NMDevice *device;
  const gchar *device_path;

  test_device_add (fixture, user_data);

  device = nmtstc_service_add_wired_device (fixture->sinfo,
                                            fixture->client,
                                            "eth1001",
                                            "52:54:00:ab:db:24",
                                            NULL);
  device_path = nm_object_get_path (NM_OBJECT (device));
  g_debug("Second device added: %s\n", device_path);

  g_assert_null (find_label (GTK_WIDGET (fixture->shell), "Wired"));
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "Ethernet (eth1000)"));
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "Ethernet (eth1001)"));
}

static void
test_second_device_add_remove (NetworkPanelFixture  *fixture,
                               gconstpointer         user_data)
{
  NMDevice *device;
  const gchar *device_path;
  GtkWidget *bt_header;

  test_device_add (fixture, user_data);

  device = nmtstc_service_add_wired_device (fixture->sinfo,
                                            fixture->client,
                                            "eth1001",
                                            "52:54:00:ab:db:24",
                                            NULL);
  device_path = nm_object_get_path (NM_OBJECT (device));
  g_debug("Second device added: %s\n", device_path);

  nmtst_remove_device (fixture->sinfo, fixture->client, device);
  g_debug("Second device removed again\n");

  /* eth1000 should be labeled "Wired" again */
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "Wired"));
  g_assert_null (find_label (GTK_WIDGET (fixture->shell), "Ethernet (eth1000)"));
  g_assert_null (find_label (GTK_WIDGET (fixture->shell), "Ethernet (eth1001)"));

  /* Some more checks for unrelated UI not showing up randomly */
  bt_header = find_label (GTK_WIDGET (fixture->shell), "Bluetooth");
  g_assert_false (bt_header && gtk_widget_is_visible(bt_header));
}

/*****************************************************************************/

static void
add_cb (GObject       *object,
        GAsyncResult  *result,
        gpointer       user_data)
{
  NMClient *client = NM_CLIENT (object);
  EventWaitInfo *info = user_data;
  g_autoptr(GError) error = NULL;

  info->rc = nm_client_add_connection_finish (client, result, &error);
  g_assert_no_error (error);
  g_assert_nonnull (info->rc);

  info->other_remaining--;
  WAIT_CHECK_REMAINING()
}

static void
delete_cb (GObject       *object,
           GAsyncResult  *result,
           gpointer       user_data)
{
  NMRemoteConnection *connection = NM_REMOTE_CONNECTION (object);
  EventWaitInfo *info = user_data;
  g_autoptr(GError) error = NULL;

  nm_remote_connection_delete_finish (connection, result, &error);
  g_assert_no_error (error);

  info->other_remaining--;
  WAIT_CHECK_REMAINING()
}

static void
test_connection_add (NetworkPanelFixture  *fixture,
                     gconstpointer         user_data)
{
  NMConnection *conn;
  g_autoptr(GError) error = NULL;
  WAIT_DECL()

  conn = nmtst_create_minimal_connection ("test-inactive", NULL, NM_SETTING_WIRED_SETTING_NAME, NULL);
  nm_device_connection_compatible (fixture->main_ether, conn, &error);
  g_assert_no_error (error);

  nm_client_add_connection_async (fixture->client, conn, TRUE, NULL, add_cb, &info);

  info.other_remaining = 1;
  WAIT_CLIENT(fixture->client, 2, NM_CLIENT_CONNECTIONS, NM_CLIENT_CONNECTION_ADDED);

  WAIT_FINISHED(5)

  g_clear_object (&info.rc);
  g_object_unref (conn);

  /* We have one (non-active) connection only, so we get a special case */
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "Cable unplugged"));
}

/*****************************************************************************/

static void
test_unconnected_carrier_plug (NetworkPanelFixture  *fixture,
                               gconstpointer         user_data)
{
  nmtst_set_wired_speed (fixture->sinfo, fixture->main_ether, 1234);
  nmtst_set_device_state (fixture->sinfo, fixture->main_ether, NM_DEVICE_STATE_DISCONNECTED, NM_DEVICE_STATE_REASON_CARRIER);

  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "1234 Mb/s"));
}


/*****************************************************************************/


static void
test_connection_add_activate (NetworkPanelFixture  *fixture,
                              gconstpointer         user_data)
{
  NMConnection *conn;
  NMActiveConnection *active_conn = NULL;
  g_autoptr(GError) error = NULL;
  GtkWidget *label, *sw;

  /* First set us into disconnected state with a carrier. */
  nmtst_set_wired_speed (fixture->sinfo, fixture->main_ether, 1234);
  nmtst_set_device_state (fixture->sinfo, fixture->main_ether, NM_DEVICE_STATE_DISCONNECTED, NM_DEVICE_STATE_REASON_CARRIER);

  conn = nmtst_create_minimal_connection ("test-active", NULL, NM_SETTING_WIRED_SETTING_NAME, NULL);

  nm_device_connection_compatible (fixture->main_ether, conn, &error);
  g_assert_no_error (error);

  active_conn = nmtst_add_and_activate_connection (fixture->sinfo, fixture->client, fixture->main_ether, conn);
  g_object_unref (active_conn);

  label = find_label (GTK_WIDGET (fixture->shell), "1234 Mb/s");
  sw = find_sibling (label, GTK_TYPE_LIST_BOX_ROW, GTK_TYPE_SWITCH);
  g_assert_nonnull (sw);
  g_assert_false (gtk_switch_get_state (GTK_SWITCH (sw)));

  /* Now set the state to connected and check the switch state */
  nmtst_set_device_state (fixture->sinfo, fixture->main_ether, NM_DEVICE_STATE_ACTIVATED, NM_DEVICE_STATE_REASON_NONE);
  g_assert_true (gtk_switch_get_state (GTK_SWITCH (sw)));

  /* Let's toggle the switch back and check we get events */
  gtk_switch_set_active (GTK_SWITCH (sw), FALSE);

  /* Only one connection, so a generic label. */
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "Connected - 1234 Mb/s"));
}

#if 0  /* See note below, where this test is added */
static void
test_connection_multi_add_activate (NetworkPanelFixture  *fixture,
                                    gconstpointer         user_data)
{
  NMConnection *conn;
  GtkWidget *sw, *bt_header;
  g_autoptr(GError) error = NULL;

  /* Add a single connection (just changing up to other test). */
  test_connection_add (fixture, user_data);

  /* Basically same as test_connection_add_activate but with different assertions. */
  nmtst_set_wired_speed (fixture->sinfo, fixture->main_ether, 1234);
  nmtst_set_device_state (fixture->sinfo, fixture->main_ether, NM_DEVICE_STATE_DISCONNECTED, NM_DEVICE_STATE_REASON_CARRIER);

  conn = nmtst_create_minimal_connection ("test-active", NULL, NM_SETTING_WIRED_SETTING_NAME, NULL);

  nm_device_connection_compatible (fixture->main_ether, conn, &error);
  g_assert_no_error (error);

  g_object_unref (nmtst_add_and_activate_connection (fixture->sinfo, fixture->client, fixture->main_ether, conn));

  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "test-inactive"));
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "test-active"));
  g_assert_null (find_label (GTK_WIDGET (fixture->shell), "52:54:00:ab:db:23"));

  /* We have no switch if there are multiple connections */
  sw = find_sibling (find_label (GTK_WIDGET (fixture->shell), "test-active"), GTK_TYPE_LIST_BOX_ROW, GTK_TYPE_SWITCH);
  if (sw)
    g_assert_false (gtk_widget_is_visible (sw));

  /* Now set the state to connected and check the switch state */
  nmtst_set_device_state (fixture->sinfo, fixture->main_ether, NM_DEVICE_STATE_ACTIVATED, NM_DEVICE_STATE_REASON_NONE);

  /* Hardware address is shown at this point */
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "52:54:00:ab:db:23"));

  /* Some more checks for unrelated UI not showing up randomly */
  bt_header = find_label (GTK_WIDGET (fixture->shell), "Bluetooth");
  g_assert_false (bt_header && gtk_widget_is_visible(bt_header));
}
#endif

/*****************************************************************************/

static void
test_vpn_add (NetworkPanelFixture  *fixture,
              gconstpointer         user_data)
{
  NMConnection *conn;
  NMSettingConnection *connsetting;
  NMSettingVpn *setting;

  WAIT_DECL()

  conn = nmtst_create_minimal_connection ("test_vpn_a", NULL, NM_SETTING_VPN_SETTING_NAME, &connsetting);
  setting = nm_connection_get_setting_vpn (conn);
  g_object_set (G_OBJECT (connsetting), NM_SETTING_CONNECTION_ID, "A", NULL);
  g_object_set (G_OBJECT (setting), NM_SETTING_VPN_SERVICE_TYPE, "org.freedesktop.NetworkManager.vpnc", NULL);

  nm_client_add_connection_async (fixture->client, conn, TRUE, NULL, add_cb, &info);

  info.other_remaining = 1;
  WAIT_CLIENT(fixture->client, 2, NM_CLIENT_CONNECTIONS, NM_CLIENT_CONNECTION_ADDED);

  g_object_unref (conn);

  WAIT_FINISHED(5)

  g_clear_object (&info.rc);

  /* Make sure it shows up. */
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "A"));
}

/*****************************************************************************/

static void
test_vpn_add_remove (NetworkPanelFixture  *fixture,
                     gconstpointer         user_data)
{
  NMConnection *conn;
  NMSettingConnection *connsetting;
  NMSettingVpn *setting;
  WAIT_DECL()

  conn = nmtst_create_minimal_connection ("test_vpn_a", NULL, NM_SETTING_VPN_SETTING_NAME, &connsetting);
  setting = nm_connection_get_setting_vpn (conn);
  g_object_set (G_OBJECT (connsetting), NM_SETTING_CONNECTION_ID, "A", NULL);
  g_object_set (G_OBJECT (setting), NM_SETTING_VPN_SERVICE_TYPE, "org.freedesktop.NetworkManager.vpnc", NULL);

  nm_client_add_connection_async (fixture->client, conn, TRUE, NULL, add_cb, &info);

  info.other_remaining = 1;
  WAIT_CLIENT(fixture->client, 2, NM_CLIENT_CONNECTIONS, NM_CLIENT_CONNECTION_ADDED);

  WAIT_FINISHED(5)

  /* Make sure it shows up. */
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "A"));

  /* And delete again */
  nm_remote_connection_delete_async (info.rc, NULL, delete_cb, &info);

  info.other_remaining = 1;
  WAIT_CLIENT(fixture->client, 2, NM_CLIENT_CONNECTIONS, NM_CLIENT_CONNECTION_REMOVED);

  WAIT_FINISHED(5)

  g_clear_object (&info.rc);
  g_object_unref (conn);

  /* Make sure it does not show up. */
  g_assert_null (find_label (GTK_WIDGET (fixture->shell), "A"));
}

/*****************************************************************************/

static void
test_vpn_updating (NetworkPanelFixture  *fixture,
                   gconstpointer         user_data)
{
  NMConnection *conn;
  NMSettingConnection *connsetting;
  NMSettingVpn *setting;
  GVariantBuilder builder;
  WAIT_DECL()

  conn = nmtst_create_minimal_connection ("test_vpn_a", NULL, NM_SETTING_VPN_SETTING_NAME, &connsetting);
  setting = nm_connection_get_setting_vpn (conn);
  g_object_set (G_OBJECT (connsetting), NM_SETTING_CONNECTION_ID, "A", NULL);
  g_object_set (G_OBJECT (setting), NM_SETTING_VPN_SERVICE_TYPE, "org.freedesktop.NetworkManager.vpnc", NULL);

  nm_client_add_connection_async (fixture->client, conn, TRUE, NULL, add_cb, &info);

  info.other_remaining = 1;
  WAIT_CLIENT(fixture->client, 2, NM_CLIENT_CONNECTIONS, NM_CLIENT_CONNECTION_ADDED);

  WAIT_FINISHED(5)

  g_object_unref (conn);

  /* Make sure it shows up. */
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "A"));

  /* Rename VPN from A to B */
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sa{sv}}"));

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sa{sv}}"));
  g_variant_builder_add (&builder, "s", "connection");

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (&builder, "s", "id");
  g_variant_builder_add (&builder, "v", g_variant_new_string ("B"));
  g_variant_builder_close (&builder);

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (&builder, "s", "type");
  g_variant_builder_add (&builder, "v", g_variant_new_string (nm_connection_get_connection_type (NM_CONNECTION (info.rc))));
  g_variant_builder_close (&builder);

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (&builder, "s", "uuid");
  g_variant_builder_add (&builder, "v", g_variant_new_string (nm_connection_get_uuid (NM_CONNECTION (info.rc))));
  g_variant_builder_close (&builder);

  g_variant_builder_close (&builder);
  g_variant_builder_close (&builder);

  nmtstc_service_update_connection_variant (
    fixture->sinfo,
    nm_object_get_path (NM_OBJECT (info.rc)),
    g_variant_builder_end (&builder),
    FALSE);
  g_variant_builder_clear (&builder);

  WAIT_CONNECTION(info.rc, 1, "changed");

  WAIT_FINISHED(5)

  g_clear_object (&info.rc);

  /* Make sure it the label got renamed. */
  g_assert_null (find_label (GTK_WIDGET (fixture->shell), "A"));
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "B"));
}

/*****************************************************************************/

#if 0  /* See note below, where this test is added */
static void
test_vpn_sorting (NetworkPanelFixture  *fixture,
                   gconstpointer         user_data)
{
  NMConnection *conn;
  NMSettingConnection *connsetting;
  NMSettingVpn *setting;
  g_autoptr(GError) error = NULL;
  GVariantBuilder builder;
  GtkWidget *a, *b, *container;
  GList *list;
  WAIT_DECL()

  conn = nmtst_create_minimal_connection ("test_vpn_a", NULL, NM_SETTING_VPN_SETTING_NAME, &connsetting);
  setting = nm_connection_get_setting_vpn (conn);
  g_object_set (G_OBJECT (connsetting), NM_SETTING_CONNECTION_ID, "A", NULL);
  g_object_set (G_OBJECT (setting), NM_SETTING_VPN_SERVICE_TYPE, "org.freedesktop.NetworkManager.vpnc", NULL);

  nm_client_add_connection_async (fixture->client, conn, TRUE, NULL, add_cb, &info);

  info.other_remaining = 1;
  WAIT_CLIENT(fixture->client, 2, NM_CLIENT_CONNECTIONS, NM_CLIENT_CONNECTION_ADDED);

  WAIT_FINISHED(5)

  g_object_unref (conn);
  g_clear_object (&info.rc);

  /* Create a second VPN which should be in front in the list */
  conn = nmtst_create_minimal_connection ("test_vpn_b", NULL, NM_SETTING_VPN_SETTING_NAME, &connsetting);
  setting = nm_connection_get_setting_vpn (conn);
  g_object_set (G_OBJECT (connsetting), NM_SETTING_CONNECTION_ID, "1", NULL);
  g_object_set (G_OBJECT (setting), NM_SETTING_VPN_SERVICE_TYPE, "org.freedesktop.NetworkManager.vpnc", NULL);

  nm_client_add_connection_async (fixture->client, conn, TRUE, NULL, add_cb, &info);

  info.other_remaining = 1;
  WAIT_CLIENT(fixture->client, 2, NM_CLIENT_CONNECTIONS, NM_CLIENT_CONNECTION_ADDED);

  WAIT_FINISHED(5)

  g_object_unref (conn);

  /* Make sure both VPNs are there. */
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "A"));
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "1"));

  /* And test that A is after 1 */
  a = find_parent_of_type (find_label (GTK_WIDGET (fixture->shell), "A"), GTK_TYPE_STACK);
  b = find_parent_of_type (find_label (GTK_WIDGET (fixture->shell), "1"), GTK_TYPE_STACK);
  container = gtk_widget_get_parent (a);
  list = gtk_container_get_children (GTK_CONTAINER (container));
  g_assert_cmpint (g_list_index (list, a), >, g_list_index (list, b));
  g_list_free (list);

  /* Rename VPN from 1 to B */
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sa{sv}}"));

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sa{sv}}"));
  g_variant_builder_add (&builder, "s", "connection");

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (&builder, "s", "id");
  g_variant_builder_add (&builder, "v", g_variant_new_string ("B"));
  g_variant_builder_close (&builder);

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (&builder, "s", "type");
  g_variant_builder_add (&builder, "v", g_variant_new_string (nm_connection_get_connection_type (NM_CONNECTION (info.rc))));
  g_variant_builder_close (&builder);

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (&builder, "s", "uuid");
  g_variant_builder_add (&builder, "v", g_variant_new_string (nm_connection_get_uuid (NM_CONNECTION (info.rc))));
  g_variant_builder_close (&builder);

  g_variant_builder_close (&builder);
  g_variant_builder_close (&builder);

  nmtstc_service_update_connection_variant (
    fixture->sinfo,
    nm_object_get_path (NM_OBJECT (info.rc)),
    g_variant_builder_end (&builder),
    FALSE);
  g_variant_builder_clear (&builder);

  WAIT_CONNECTION(info.rc, 1, "changed");

  WAIT_FINISHED(5)

  g_clear_object (&info.rc);

  /* Make sure it the label got renamed. */
  g_assert_null (find_label (GTK_WIDGET (fixture->shell), "1"));
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "A"));
  g_assert_nonnull (find_label (GTK_WIDGET (fixture->shell), "B"));

  /* And test that A is before B */
  a = find_parent_of_type (find_label (GTK_WIDGET (fixture->shell), "A"), GTK_TYPE_STACK);
  b = find_parent_of_type (find_label (GTK_WIDGET (fixture->shell), "B"), GTK_TYPE_STACK);
  container = gtk_widget_get_parent (a);
  list = gtk_container_get_children (GTK_CONTAINER (container));
  g_assert_cmpint (g_list_index (list, a), <, g_list_index (list, b));
  g_list_free (list);
}
#endif

/*****************************************************************************/

int
main (int argc, char **argv)
{
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("LIBNM_USE_SESSION_BUS", "1", TRUE);
  g_setenv ("LC_ALL", "C", TRUE);

  gtk_test_init (&argc, &argv, NULL);
  adw_init ();

  g_test_add ("/network-panel-wired/empty-ui",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_empty,
              test_empty_ui,
              fixture_tear_down);

  g_test_add ("/network-panel-wired/device-add",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_empty,
              test_device_add,
              fixture_tear_down);

  g_test_add ("/network-panel-wired/second-device-add",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_empty,
              test_second_device_add,
              fixture_tear_down);

  g_test_add ("/network-panel-wired/second-device-add-remove",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_empty,
              test_second_device_add_remove,
              fixture_tear_down);

  g_test_add ("/network-panel-wired/unconnected-carrier-plug",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_wired,
              test_unconnected_carrier_plug,
              fixture_tear_down);

  g_test_add ("/network-panel-wired/connection-add",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_wired,
              test_connection_add,
              fixture_tear_down);

  g_test_add ("/network-panel-wired/connection-add-activate",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_wired,
              test_connection_add_activate,
              fixture_tear_down);

#if 0
  /*
   * FIXME: Currently broken on some CI runners, so test is disabled.
   *        Test is likely running into some race condition, but it's unknown
   *        if this can be solved by waiting on different signals or not.
   *
   *        See https://gitlab.gnome.org/GNOME/gnome-control-center/-/issues/1768
   */
  g_test_add ("/network-panel-wired/connection-multi-add-activate",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_wired,
              test_connection_multi_add_activate,
              fixture_tear_down);
#endif

  g_test_add ("/network-panel-wired/vpn-add",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_empty,
              test_vpn_add,
              fixture_tear_down);

  g_test_add ("/network-panel-wired/vpn-add-remove",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_empty,
              test_vpn_add_remove,
              fixture_tear_down);

  g_test_add ("/network-panel-wired/vpn-updating",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_empty,
              test_vpn_updating,
              fixture_tear_down);

#if 0
  /*
   * FIXME: Currently broken, so test is disabled. Test will likely need
   *        updating when this is fixed to look for GTK_TYPE_LIST_BOX_ROW rather
   *        than GTK_TYPE_STACK.
   */
  g_test_add ("/network-panel-wired/vpn-sorting",
              NetworkPanelFixture,
              NULL,
              fixture_set_up_empty,
              test_vpn_sorting,
              fixture_tear_down);
#endif

  return g_test_run ();
}

