/*
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "config.h"

#include <adwaita.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "cc-sharing-networks.h"
#include "org.gnome.SettingsDaemon.Sharing.h"
#include "gsd-sharing-enums.h"

struct _CcSharingNetworks {
  GtkBox parent_instance;

  GtkWidget *listbox;

  GtkWidget *current_row;
  GtkWidget *current_icon;
  GtkWidget *current_switch;

  GtkWidget *no_network_row;

  char *service_name;
  GsdSharing *proxy;
  CcSharingStatus status;

  GList *networks; /* list of CcSharingNetwork */
};


G_DEFINE_TYPE (CcSharingNetworks, cc_sharing_networks, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_PROXY,
  PROP_SERVICE_NAME,
  PROP_STATUS
};

static void     cc_sharing_networks_class_init     (CcSharingNetworksClass *klass);
static void     cc_sharing_networks_init           (CcSharingNetworks      *self);
static void     cc_sharing_networks_finalize       (GObject                *object);

static void     cc_sharing_update_networks_box     (CcSharingNetworks *self);

typedef struct {
  char *uuid;
  char *network_name;
  char *carrier_type;
} CcSharingNetwork;

static void
cc_sharing_network_free (gpointer data)
{
  CcSharingNetwork *net = data;

  g_free (net->uuid);
  g_free (net->network_name);
  g_free (net->carrier_type);
  g_free (net);
}

static void
cc_sharing_networks_update_status (CcSharingNetworks *self)
{
  CcSharingStatus status;

  if (self->networks == NULL)
    status = CC_SHARING_STATUS_OFF;
  else if (gtk_switch_get_active (GTK_SWITCH (self->current_switch)))
    status = CC_SHARING_STATUS_ACTIVE;
  else
    status = CC_SHARING_STATUS_ENABLED;

  if (status != self->status) {
    self->status = status;
    g_object_notify (G_OBJECT (self), "status");
  }
}

static void
cc_sharing_update_networks (CcSharingNetworks *self)
{
  g_autoptr(GVariant) networks = NULL;
  char *uuid, *network_name, *carrier_type;
  GVariantIter iter;
  g_autoptr(GError) error = NULL;

  g_list_free_full (self->networks, cc_sharing_network_free);
  self->networks = NULL;

  if (!gsd_sharing_call_list_networks_sync (self->proxy, self->service_name, &networks, NULL, &error)) {
    g_warning ("couldn't list networks: %s", error->message);
    g_dbus_proxy_set_cached_property (G_DBUS_PROXY (self->proxy),
				      "SharingStatus",
				      g_variant_new_uint32 (GSD_SHARING_STATUS_OFFLINE));
    return;
  }

  g_variant_iter_init (&iter, networks);
  while (g_variant_iter_next (&iter, "(sss)", &uuid, &network_name, &carrier_type)) {
    CcSharingNetwork *net;

    net = g_new0 (CcSharingNetwork, 1);
    net->uuid = uuid;
    net->network_name = network_name;
    net->carrier_type = carrier_type;
    self->networks = g_list_prepend (self->networks, net);
  }
  self->networks = g_list_reverse (self->networks);
}

static void
cc_sharing_networks_remove_network (CcSharingNetworks *self,
                                    GtkWidget         *button)
{
  GtkWidget *row;
  g_autoptr(GError) error = NULL;
  gboolean ret;
  const char *uuid;

  row = g_object_get_data (G_OBJECT (button), "row");
  uuid = g_object_get_data (G_OBJECT (row), "uuid");

  ret = gsd_sharing_call_disable_service_sync (self->proxy,
					       self->service_name,
					       uuid,
					       NULL,
					       &error);
  if (!ret)
    g_warning ("Failed to remove service %s: %s",
	       self->service_name, error->message);

  cc_sharing_update_networks (self);
  cc_sharing_update_networks_box (self);
}

static gboolean
cc_sharing_networks_enable_network (CcSharingNetworks *self,
				    gboolean   state,
                                    GtkSwitch *widget)
{
  g_autoptr(GError) error = NULL;
  gboolean ret;

  if (state) {
    ret = gsd_sharing_call_enable_service_sync (self->proxy,
						self->service_name,
						NULL,
						&error);
  } else {
    ret = gsd_sharing_call_disable_service_sync (self->proxy,
						 self->service_name,
						 gsd_sharing_get_current_network (self->proxy),
						 NULL,
						 &error);
  }

  if (ret) {
    gtk_switch_set_state (widget, state);
  } else {
    g_warning ("Failed to %s service %s: %s", state ? "enable" : "disable",
	       self->service_name, error->message);
    g_signal_handlers_block_by_func (widget,
                                     cc_sharing_networks_enable_network, self);
    gtk_switch_set_active (widget, !state);
    g_signal_handlers_unblock_by_func (widget,
                                       cc_sharing_networks_enable_network, self);
  }

  cc_sharing_update_networks (self);
  cc_sharing_networks_update_status (self);

  return TRUE;
}

static GtkWidget *
cc_sharing_networks_new_row (const char        *uuid,
			     const char        *network_name,
			     const char        *carrier_type,
			     CcSharingNetworks *self)
{
  GtkWidget *row, *w;
  const char *icon_name;

  row = adw_action_row_new ();
  adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), FALSE);

  if (g_strcmp0 (carrier_type, "802-11-wireless") == 0) {
    icon_name = "network-wireless-offline-symbolic";
  } else if (g_strcmp0 (carrier_type, "802-3-ethernet") == 0) {
    icon_name = "network-wired-disconnected-symbolic";
  } else {
    icon_name = "network-wired-symbolic";
  }

  adw_action_row_set_icon_name (ADW_ACTION_ROW (row), icon_name);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), network_name);

  /* Remove button */
  w = gtk_button_new_from_icon_name ("edit-delete-symbolic");
  gtk_widget_add_css_class (w, "flat");
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_accessible_update_property (GTK_ACCESSIBLE (w),
                                GTK_ACCESSIBLE_PROPERTY_LABEL, _("Remove"),
                                -1);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), w);
  g_signal_connect_object (G_OBJECT (w), "clicked",
                           G_CALLBACK (cc_sharing_networks_remove_network), self, G_CONNECT_SWAPPED);
  g_object_set_data (G_OBJECT (w), "row", row);

  g_object_set_data_full (G_OBJECT (row), "uuid", g_strdup (uuid), g_free);

  return row;
}

static GtkWidget *
cc_sharing_networks_new_current_row (CcSharingNetworks *self)
{
  GtkWidget *row, *w;

  row = adw_action_row_new ();
  adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), FALSE);

  /* Icon */
  w = gtk_image_new_from_icon_name ("image-missing");
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), w);
  self->current_icon = w;

  w = gtk_switch_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), w);
  adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), w);
  g_signal_connect_object (G_OBJECT (w), "state-set",
                           G_CALLBACK (cc_sharing_networks_enable_network), self, G_CONNECT_SWAPPED);
  self->current_switch = w;
  g_object_set_data (G_OBJECT (w), "row", row);

  return row;
}

static GtkWidget *
cc_sharing_networks_new_no_network_row (CcSharingNetworks *self)
{
  GtkWidget *row, *box, *w;

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);

  /* Label */
  w = gtk_label_new (_("No networks selected for sharing"));
  gtk_widget_set_hexpand (w, TRUE);
  gtk_widget_set_halign (w, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (w, "dim-label");
  gtk_box_append (GTK_BOX (box), w);

  return row;
}

static void
cc_sharing_update_networks_box (CcSharingNetworks *self)
{
  GtkWidget *child;
  gboolean current_visible, current_network_enabled = FALSE;
  const char *current_network;
  GList *l;

  child = gtk_widget_get_first_child (self->listbox);
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling (child);
    if (child != self->current_row && child != self->no_network_row)
      gtk_list_box_remove (GTK_LIST_BOX (self->listbox), child);
    child = next;
  }

  current_network = gsd_sharing_get_current_network (self->proxy);

  if (current_network != NULL &&
      !g_str_equal (current_network, "")) {
    gboolean available;
    const char *carrier_type, *icon_name, *current_network_name;

    gtk_widget_set_visible (self->current_row, TRUE);
    current_visible = TRUE;

    /* Network name */
    g_object_set_data_full (G_OBJECT (self->current_row),
			    "uuid", g_strdup (current_network), g_free);
    current_network_name = gsd_sharing_get_current_network_name (self->proxy);
    if (ADW_IS_PREFERENCES_ROW (self->current_row))
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->current_row), current_network_name);

    /* Icon */
    carrier_type = gsd_sharing_get_carrier_type (self->proxy);
    if (g_strcmp0 (carrier_type, "802-11-wireless") == 0) {
      icon_name = "network-wireless-signal-excellent-symbolic";
    } else if (g_strcmp0 (carrier_type, "802-3-ethernet") == 0) {
      icon_name = "network-wired-symbolic";
    } else {
      icon_name = "network-wired-symbolic";
    }
    gtk_image_set_from_icon_name (GTK_IMAGE (self->current_icon), icon_name);

    /* State */
    available = gsd_sharing_get_sharing_status (self->proxy) == GSD_SHARING_STATUS_AVAILABLE;
    gtk_widget_set_sensitive (self->current_switch, available);
    //FIXME add a subtitle explaining why it's disabled
  } else {
    gtk_widget_set_visible (self->current_row, FALSE);
    current_visible = FALSE;
  }

  for (l = self->networks; l != NULL; l = l->next) {
    CcSharingNetwork *net = l->data;
    GtkWidget *row;

    if (g_strcmp0 (net->uuid, current_network) == 0) {
      current_network_enabled = TRUE;
      continue;
    }

    row = cc_sharing_networks_new_row (net->uuid,
				       net->network_name,
				       net->carrier_type,
				       self);
    gtk_widget_set_visible (row, TRUE);
    gtk_list_box_insert (GTK_LIST_BOX (self->listbox), row, -1);
  }

  g_signal_handlers_block_by_func (self->current_switch,
                                   cc_sharing_networks_enable_network, self);
  gtk_switch_set_active (GTK_SWITCH (self->current_switch), current_network_enabled);
  g_signal_handlers_unblock_by_func (self->current_switch,
                                     cc_sharing_networks_enable_network, self);

  if (self->networks == NULL &&
      !current_visible) {
    gtk_widget_set_visible (self->no_network_row, TRUE);
  } else {
    gtk_widget_set_visible (self->no_network_row, FALSE);
  }

  cc_sharing_networks_update_status (self);
}

static void
current_network_changed (CcSharingNetworks *self)
{
  cc_sharing_update_networks (self);
  cc_sharing_update_networks_box (self);
}

static void
cc_sharing_networks_constructed (GObject *object)
{
  CcSharingNetworks *self;

  G_OBJECT_CLASS (cc_sharing_networks_parent_class)->constructed (object);

  self = CC_SHARING_NETWORKS (object);

  self->current_row = cc_sharing_networks_new_current_row (self);
  gtk_list_box_insert (GTK_LIST_BOX (self->listbox), self->current_row, -1);
  g_object_set_data (G_OBJECT (self), "switch", self->current_switch);

  self->no_network_row = cc_sharing_networks_new_no_network_row (self);
  gtk_list_box_insert (GTK_LIST_BOX (self->listbox), self->no_network_row, -1);

  cc_sharing_update_networks (self);
  cc_sharing_update_networks_box (self);

  g_signal_connect_object (self->proxy, "notify::current-network",
                           G_CALLBACK (current_network_changed), self, G_CONNECT_SWAPPED);
}

static void
cc_sharing_networks_init (CcSharingNetworks *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
cc_sharing_networks_new (GDBusProxy *proxy,
			 const char *service_name)
{
  g_return_val_if_fail (GSD_IS_SHARING (proxy), NULL);
  g_return_val_if_fail (service_name != NULL, NULL);

  return GTK_WIDGET (g_object_new (CC_TYPE_SHARING_NETWORKS,
				   "proxy", proxy,
				   "service-name", service_name,
				   NULL));
}

static void
cc_sharing_networks_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  CcSharingNetworks *self;

  self = CC_SHARING_NETWORKS (object);

  switch (prop_id) {
  case PROP_SERVICE_NAME:
    self->service_name = g_value_dup_string (value);
    break;
  case PROP_PROXY:
    self->proxy = g_value_dup_object (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_sharing_networks_get_property (GObject      *object,
				  guint         prop_id,
				  GValue       *value,
				  GParamSpec   *pspec)
{
  CcSharingNetworks *self;

  self = CC_SHARING_NETWORKS (object);

  switch (prop_id) {
  case PROP_STATUS:
    g_value_set_uint (value, self->status);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_sharing_networks_finalize (GObject *object)
{
  CcSharingNetworks *self;

  g_return_if_fail (object != NULL);
  g_return_if_fail (CC_IS_SHARING_NETWORKS (object));

  self = CC_SHARING_NETWORKS (object);

  g_return_if_fail (self != NULL);

  g_clear_object (&self->proxy);
  g_clear_pointer (&self->service_name, g_free);

  if (self->networks != NULL) {
    g_list_free_full (self->networks, cc_sharing_network_free);
    self->networks = NULL;
  }

  G_OBJECT_CLASS (cc_sharing_networks_parent_class)->finalize (object);
}


static void
cc_sharing_networks_class_init (CcSharingNetworksClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = cc_sharing_networks_set_property;
  object_class->get_property = cc_sharing_networks_get_property;
  object_class->finalize = cc_sharing_networks_finalize;
  object_class->constructed = cc_sharing_networks_constructed;

  g_object_class_install_property (object_class,
                                   PROP_PROXY,
                                   g_param_spec_object ("proxy",
                                                        "proxy",
                                                        "proxy",
                                                        GSD_TYPE_SHARING_PROXY,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_SERVICE_NAME,
                                   g_param_spec_string ("service-name",
                                                        "service-name",
                                                        "service-name",
                                                        NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_STATUS,
                                   g_param_spec_uint ("status",
                                                      "status",
                                                      "status",
                                                      CC_SHARING_STATUS_UNSET, CC_SHARING_STATUS_ACTIVE + 1, CC_SHARING_STATUS_OFF,
                                                      G_PARAM_READABLE));

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/sharing/cc-sharing-networks.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSharingNetworks, listbox);
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
