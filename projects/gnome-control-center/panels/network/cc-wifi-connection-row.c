/*
 * Copyright © 2018 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <config.h>
#include "cc-wifi-connection-row.h"
#include "cc-qr-code.h"

struct _CcWifiConnectionRow
{
  AdwActionRow     parent_instance;

  gboolean         constructed;

  gboolean         checkable;
  gboolean         forgettable;
  gboolean         checked;

  NMDeviceWifi    *device;
  GPtrArray       *aps;
  NMConnection    *connection;
  gboolean         known_connection;

  GtkLabel        *active_label;
  GtkCheckButton  *checkbutton;
  GtkSpinner      *connecting_spinner;
  GtkImage        *encrypted_icon;
  GtkButton       *options_button;
  GtkButton       *forget_button;
  GtkButton       *qr_code_button;
  GtkImage        *strength_icon;
};

enum
{
  PROP_0,
  PROP_CHECKABLE,
  PROP_CHECKED,
  PROP_DEVICE,
  PROP_APS,
  PROP_CONNECTION,
  PROP_KNOWN_CONNECTION,
  PROP_FORGETTABLE,
  PROP_LAST
};

typedef enum
{
  NM_AP_SEC_UNKNOWN,
  NM_AP_SEC_NONE,
  NM_AP_SEC_WEP,
  NM_AP_SEC_WPA,
  NM_AP_SEC_WPA2,
  NM_AP_SEC_SAE,
  NM_AP_SEC_OWE,
  NM_AP_SEC_OWE_TM
} NMAccessPointSecurity;

G_DEFINE_TYPE (CcWifiConnectionRow, cc_wifi_connection_row, ADW_TYPE_ACTION_ROW)

static GParamSpec *props[PROP_LAST];

static void configure_clicked_cb (CcWifiConnectionRow *self);
static void forget_clicked_cb (CcWifiConnectionRow *self);
static void qr_code_clicked_cb (CcWifiConnectionRow *self);

static NMAccessPointSecurity
get_access_point_security (NMAccessPoint *ap)
{
  NM80211ApFlags flags;
  NM80211ApSecurityFlags wpa_flags;
  NM80211ApSecurityFlags rsn_flags;
  NMAccessPointSecurity type;

  flags = nm_access_point_get_flags (ap);
  wpa_flags = nm_access_point_get_wpa_flags (ap);
  rsn_flags = nm_access_point_get_rsn_flags (ap);

  if (!(flags & NM_802_11_AP_FLAGS_PRIVACY) &&
      wpa_flags == NM_802_11_AP_SEC_NONE &&
      rsn_flags == NM_802_11_AP_SEC_NONE)
    {
      type = NM_AP_SEC_NONE;
    }
  else if ((flags & NM_802_11_AP_FLAGS_PRIVACY) &&
           wpa_flags == NM_802_11_AP_SEC_NONE &&
           rsn_flags == NM_802_11_AP_SEC_NONE)
    {
      type = NM_AP_SEC_WEP;
    }
  else if (!(flags & NM_802_11_AP_FLAGS_PRIVACY) &&
           wpa_flags != NM_802_11_AP_SEC_NONE &&
           rsn_flags != NM_802_11_AP_SEC_NONE)
    {
      type = NM_AP_SEC_WPA;
    }
#if NM_CHECK_VERSION(1,20,6)
  else if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_SAE)
    {
      type = NM_AP_SEC_SAE;
    }
#endif
#if NM_CHECK_VERSION(1,24,0)
  else if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_OWE)
    {
      type = NM_AP_SEC_OWE;
    }
#endif
#if NM_CHECK_VERSION(1,26,0)
  else if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_OWE_TM)
    {
      type = NM_AP_SEC_OWE_TM;
    }
#endif
  else
    {
      type = NM_AP_SEC_WPA2;
    }

  return type;
}

static NMAccessPointSecurity
get_connection_security (NMConnection *con)
{
  NMSettingWirelessSecurity *sws;
  const gchar *key_mgmt;

  sws = nm_connection_get_setting_wireless_security (con);
  g_debug ("getting security from %p", sws);
  if (!sws)
    return NM_AP_SEC_NONE;

  key_mgmt = nm_setting_wireless_security_get_key_mgmt (sws);
  g_debug ("key management is %s", key_mgmt);

  if (!key_mgmt)
    return NM_AP_SEC_NONE;
  else if (g_str_equal (key_mgmt, "none"))
    return NM_AP_SEC_WEP;
  else if (g_str_equal (key_mgmt, "ieee8021x"))
    return NM_AP_SEC_WEP;
  else if (g_str_equal (key_mgmt, "wpa-eap"))
    return NM_AP_SEC_WPA2;
  else if (strncmp (key_mgmt, "wpa-", 4) == 0)
    return NM_AP_SEC_WPA;
  else if (g_str_equal (key_mgmt, "sae"))
    return NM_AP_SEC_SAE;
  else if (g_str_equal (key_mgmt, "owe"))
    return NM_AP_SEC_OWE;
  else
    return NM_AP_SEC_UNKNOWN;
}

static void
update_ui (CcWifiConnectionRow *self)
{
  GBytes *ssid;
  g_autofree gchar *title = NULL;
  NMActiveConnection *active_connection = NULL;
  gboolean active;
  gboolean connecting;
  NMAccessPointSecurity security = NM_AP_SEC_UNKNOWN;
  NMAccessPoint *best_ap;
  guint8 strength = 0;
  NMActiveConnectionState state;

  g_assert (self->device);
  g_assert (self->connection || self->aps->len > 0);

  best_ap = cc_wifi_connection_row_best_access_point (self);

  if (self->connection)
    {
      active_connection = nm_device_get_active_connection (NM_DEVICE (self->device));
      if (active_connection &&
          NM_CONNECTION (nm_active_connection_get_connection (active_connection)) != self->connection)
        active_connection = NULL;
    }

  if (self->connection)
    {
      NMSettingWireless *sw;
      const gchar *name = NULL;
      g_autofree gchar *ssid_str = NULL;
      gchar *ssid_pos;

      sw = nm_connection_get_setting_wireless (self->connection);

      ssid = nm_setting_wireless_get_ssid (sw);
      ssid_str = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
      name = nm_connection_get_id (NM_CONNECTION (self->connection));

      ssid_pos = strstr (name, ssid_str);
      if (ssid_pos == name && strlen (name) == strlen (ssid_str))
        {
          title = g_markup_escape_text (name, -1);
        }
      else if (ssid_pos)
        {
          g_autofree gchar *before = g_strndup (name, ssid_pos - name);
          g_autofree gchar *after = g_strndup (ssid_pos + strlen (ssid_str), strlen(ssid_pos) - strlen(ssid_str));
          title = g_markup_printf_escaped ("<i>%s</i>%s<i>%s</i>",
                                           before, ssid_str, after);
        }
      else
        {
          /* TRANSLATORS: This happens when the connection name does not contain the SSID. */
          title = g_markup_printf_escaped (C_("Wi-Fi Connection", "%s (SSID: %s)"),
                                           name, ssid_str);
        }

      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title);
    }
  else
    {
      g_autofree char *title_escaped = NULL;

      ssid = nm_access_point_get_ssid (best_ap);
      title = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
      title_escaped = g_markup_escape_text (title, -1);
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title_escaped);
    }

  if (active_connection)
    {
      state = nm_active_connection_get_state (active_connection);

      active = state == NM_ACTIVE_CONNECTION_STATE_ACTIVATED;
      connecting = state == NM_ACTIVE_CONNECTION_STATE_ACTIVATING;
    }
  else
    {
      active = FALSE;
      connecting = FALSE;
    }

  if (self->connection)
    security = get_connection_security (self->connection);

  if (best_ap != NULL)
    {
      security = get_access_point_security (best_ap);
      strength = nm_access_point_get_strength (best_ap);
    }

  gtk_widget_set_visible (GTK_WIDGET (self->connecting_spinner), connecting);
  if (connecting)
    {
      gtk_spinner_start (self->connecting_spinner);
    }
  else
    {
      gtk_spinner_stop (self->connecting_spinner);
    }

  gtk_widget_set_visible (GTK_WIDGET (self->active_label), active);
  gtk_widget_set_visible (GTK_WIDGET (self->options_button), active || connecting || self->known_connection);
  gtk_widget_set_visible (GTK_WIDGET (self->qr_code_button), (active || self->known_connection) && is_qr_code_supported (self->connection));

  if (security != NM_AP_SEC_UNKNOWN && security != NM_AP_SEC_NONE && security != NM_AP_SEC_OWE && security != NM_AP_SEC_OWE_TM)
    {
      const gchar *icon_name = "lock-small-symbolic";

      gtk_widget_set_child_visible (GTK_WIDGET (self->encrypted_icon), TRUE);
      if (security == NM_AP_SEC_WEP)
	{
          icon_name = "warning-small-symbolic";
	  gtk_widget_set_tooltip_text (GTK_WIDGET (self->encrypted_icon), _("Insecure network (WEP)"));
	}
      else if (security == NM_AP_SEC_WPA)
	{
          gtk_widget_set_tooltip_text (GTK_WIDGET (self->encrypted_icon), _("Secure network (WPA)"));
	}
      else if (security == NM_AP_SEC_WPA2)
	{
          gtk_widget_set_tooltip_text (GTK_WIDGET (self->encrypted_icon), _("Secure network (WPA2)"));
	}
	  else if (security == NM_AP_SEC_SAE)
	{
          gtk_widget_set_tooltip_text (GTK_WIDGET (self->encrypted_icon), _("Secure network (WPA3)"));
	}
      else
	{
          gtk_widget_set_tooltip_text (GTK_WIDGET (self->encrypted_icon), _("Secure network"));
	}

      gtk_image_set_from_icon_name (self->encrypted_icon, icon_name);
    }
  else
    {
      gtk_widget_set_child_visible (GTK_WIDGET (self->encrypted_icon), FALSE);
    }

  if (best_ap)
    {
      g_autofree char *description = NULL;
      gchar *icon_name;

      if (strength < 20)
        icon_name = "network-wireless-signal-none-symbolic";
      else if (strength < 40)
        icon_name = "network-wireless-signal-weak-symbolic";
      else if (strength < 50)
        icon_name = "network-wireless-signal-ok-symbolic";
      else if (strength < 80)
        icon_name = "network-wireless-signal-good-symbolic";
      else
        icon_name = "network-wireless-signal-excellent-symbolic";

      g_object_set (self->strength_icon, "icon-name", icon_name, NULL);
      gtk_widget_set_child_visible (GTK_WIDGET (self->strength_icon), TRUE);

      description = g_strdup_printf(_("Signal strength %d%%"), strength);
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->strength_icon), description);
      gtk_accessible_update_property (GTK_ACCESSIBLE (self->strength_icon),
                                      GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                      description,
                                      -1);
    }
  else
    {
      gtk_widget_set_child_visible (GTK_WIDGET (self->strength_icon), FALSE);
      gtk_accessible_reset_property (GTK_ACCESSIBLE (self->strength_icon), GTK_ACCESSIBLE_PROPERTY_DESCRIPTION);
    }
}

static void
cc_wifi_connection_row_constructed (GObject *object)
{
  CcWifiConnectionRow *self = CC_WIFI_CONNECTION_ROW (object);

  G_OBJECT_CLASS (cc_wifi_connection_row_parent_class)->constructed (object);

  /* Reparent the label into the checkbox */
  gtk_widget_set_visible (GTK_WIDGET (self->checkbutton), self->checkable);
  gtk_widget_set_visible (GTK_WIDGET (self->forget_button), self->forgettable);

  update_ui (CC_WIFI_CONNECTION_ROW (object));
}

static void
cc_wifi_connection_row_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  CcWifiConnectionRow *self = CC_WIFI_CONNECTION_ROW (object);
  GPtrArray *ptr_array;
  gint i;

  switch (prop_id)
    {
    case PROP_CHECKABLE:
      g_value_set_boolean (value, self->checkable);
      break;

    case PROP_CHECKED:
      g_value_set_boolean (value, self->checked);
      break;

    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    case PROP_APS:
      ptr_array = g_ptr_array_new_full (self->aps->len, NULL);
      for (i = 0; i < self->aps->len; i++)
        g_ptr_array_add (ptr_array, g_ptr_array_index (self->aps, i));

      g_value_take_boxed (value, ptr_array);
      break;

    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    case PROP_KNOWN_CONNECTION:
      g_value_set_boolean (value, self->known_connection);
      break;
    case PROP_FORGETTABLE:
      g_value_set_boolean (value, self->forgettable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wifi_connection_row_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  CcWifiConnectionRow *self = CC_WIFI_CONNECTION_ROW (object);
  GPtrArray *ptr_array;
  gint i;

  switch (prop_id)
    {
    case PROP_CHECKABLE:
      self->checkable = g_value_get_boolean (value);
      break;

    case PROP_CHECKED:
      self->checked = g_value_get_boolean (value);
      break;

    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    case PROP_APS:
      ptr_array = g_value_get_boxed (value);
      g_ptr_array_set_size (self->aps, 0);

      if (ptr_array)
        {
          for (i = 0; i < ptr_array->len; i++)
            g_ptr_array_add (self->aps, g_object_ref (g_ptr_array_index (ptr_array, i)));
        }
      if (self->constructed)
        update_ui (self);
      break;

    case PROP_CONNECTION:
      self->connection = g_value_dup_object (value);
      break;

    case PROP_KNOWN_CONNECTION:
      self->known_connection = g_value_get_boolean (value);
      break;

    case PROP_FORGETTABLE:
      self->forgettable = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_wifi_connection_row_finalize (GObject *object)
{
  CcWifiConnectionRow *self = CC_WIFI_CONNECTION_ROW (object);

  g_clear_object (&self->device);
  g_clear_pointer (&self->aps, g_ptr_array_unref);
  g_clear_object (&self->connection);

  G_OBJECT_CLASS (cc_wifi_connection_row_parent_class)->finalize (object);
}


void
cc_wifi_connection_row_class_init (CcWifiConnectionRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = cc_wifi_connection_row_constructed;
  object_class->get_property = cc_wifi_connection_row_get_property;
  object_class->set_property = cc_wifi_connection_row_set_property;
  object_class->finalize = cc_wifi_connection_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/cc-wifi-connection-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, active_label);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, checkbutton);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, connecting_spinner);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, encrypted_icon);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, options_button);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, forget_button);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, qr_code_button);
  gtk_widget_class_bind_template_child (widget_class, CcWifiConnectionRow, strength_icon);

  gtk_widget_class_bind_template_callback (widget_class, configure_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, forget_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, qr_code_clicked_cb);

  props[PROP_CHECKABLE] = g_param_spec_boolean ("checkable", "checkable",
                                                "Whether to show a checkbox to select the row",
                                                FALSE,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_CHECKED] = g_param_spec_boolean ("checked", "Checked",
                                              "Whether the row is selected by checking it",
                                              FALSE,
                                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_DEVICE] = g_param_spec_object ("device", "WiFi Device",
                                            "The WiFi Device for this connection/ap",
                                            NM_TYPE_DEVICE_WIFI,
                                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_APS] = g_param_spec_boxed ("aps", "Access Points",
                                        "The access points for this connection  (may be empty if a connection is given)",
                                         G_TYPE_PTR_ARRAY,
                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_CONNECTION] = g_param_spec_object ("connection", "Connection",
                                                "The NMConnection (may be NULL if there is an AP)",
                                                 NM_TYPE_CONNECTION,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_KNOWN_CONNECTION] = g_param_spec_boolean ("known-connection", "Known Connection",
                                                "Whether this row is a known connection or not",
                                                FALSE,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_FORGETTABLE] = g_param_spec_boolean ("forgettable", "forgettable",
                                                  "Whether to show a checkbox to select the row",
                                                  FALSE,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     PROP_LAST,
                                     props);

  g_signal_new ("configure",
                CC_TYPE_WIFI_CONNECTION_ROW,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);
  g_signal_new ("forget",
               CC_TYPE_WIFI_CONNECTION_ROW,
               G_SIGNAL_RUN_LAST,
               0, NULL, NULL, NULL,
               G_TYPE_NONE, 0);
  g_signal_new ("show-qr-code",
                CC_TYPE_WIFI_CONNECTION_ROW,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);
}

static void
configure_clicked_cb (CcWifiConnectionRow *self)
{
  g_signal_emit_by_name (self, "configure");
}

static void
forget_clicked_cb (CcWifiConnectionRow *self)
{
  g_signal_emit_by_name (self, "forget");
}

static void
qr_code_clicked_cb (CcWifiConnectionRow *self)
{
  g_signal_emit_by_name (self, "show-qr-code");
}

void
cc_wifi_connection_row_init (CcWifiConnectionRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->aps = g_ptr_array_new_with_free_func (g_object_unref);

  g_object_bind_property (self, "checked",
                          self->checkbutton, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_RELATION_DESCRIBED_BY,
                                  self->encrypted_icon, self->strength_icon, NULL,
                                  -1);
}

CcWifiConnectionRow *
cc_wifi_connection_row_new (NMDeviceWifi  *device,
                            NMConnection  *connection,
                            GPtrArray     *aps,
                            gboolean       checkable,
                            gboolean       known_connection,
                            gboolean       forgettable)
{
  return g_object_new (CC_TYPE_WIFI_CONNECTION_ROW,
                       "device", device,
                       "connection", connection,
                       "aps", aps,
                       "checkable", checkable,
                       "known-connection", known_connection,
                       "forgettable", forgettable,
                       NULL);
}

gboolean
cc_wifi_connection_row_get_checkable (CcWifiConnectionRow *self)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (self), FALSE);

  return self->checkable;
}

gboolean
cc_wifi_connection_row_get_checked (CcWifiConnectionRow *self)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (self), FALSE);

  return self->checked;
}

NMDeviceWifi*
cc_wifi_connection_row_get_device (CcWifiConnectionRow *self)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (self), NULL);

  return self->device;
}

const GPtrArray*
cc_wifi_connection_row_get_access_points (CcWifiConnectionRow *self)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (self), NULL);

  return self->aps;
}

NMConnection*
cc_wifi_connection_row_get_connection (CcWifiConnectionRow *self)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (self), NULL);

  return self->connection;
}

gboolean
cc_wifi_connection_row_get_forgettable (CcWifiConnectionRow *self)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (self), FALSE);

  return self->forgettable;
}

void
cc_wifi_connection_row_set_checked (CcWifiConnectionRow *self,
                                    gboolean             value)
{
  g_return_if_fail (CC_WIFI_CONNECTION_ROW (self));

  self->checked = value;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHECKED]);
}

NMAccessPoint*
cc_wifi_connection_row_best_access_point (CcWifiConnectionRow *self)
{
  NMAccessPoint *best_ap = NULL;
  NMAccessPoint *active_ap = NULL;
  guint8 strength = 0;
  gint i;

  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (self), NULL);

  if (self->aps->len == 0)
    return NULL;

  active_ap = nm_device_wifi_get_active_access_point (self->device);

  for (i = 0; i < self->aps->len; i++)
    {
      NMAccessPoint *cur;
      guint8 cur_strength;

      cur = g_ptr_array_index (self->aps, i);

      /* Prefer the active AP in all cases */
      if (cur == active_ap)
        return cur;

      cur_strength = nm_access_point_get_strength (cur);
      /* Use if we don't have an AP, this is the current AP, or it is better */
      if (!best_ap || cur_strength > strength)
        {
          best_ap = cur;
          strength = cur_strength;
        }
    }

  return best_ap;
}

void
cc_wifi_connection_row_add_access_point (CcWifiConnectionRow *self,
                                         NMAccessPoint       *ap)
{
  g_return_if_fail (CC_WIFI_CONNECTION_ROW (self));

  g_ptr_array_add (self->aps, g_object_ref (ap));
  update_ui (self);
}

gboolean
cc_wifi_connection_row_remove_access_point (CcWifiConnectionRow *self,
                                            NMAccessPoint       *ap)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (self), FALSE);

  if (!g_ptr_array_remove (self->aps, g_object_ref (ap)))
    return FALSE;

  /* Object might be invalid; this is alright if it is deleted right away */
  if (self->aps->len > 0 || self->connection)
    {
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APS]);
      update_ui (self);
    }

  return self->aps->len == 0;
}

gboolean
cc_wifi_connection_row_has_access_point (CcWifiConnectionRow *self,
                                         NMAccessPoint       *ap)
{
  g_return_val_if_fail (CC_WIFI_CONNECTION_ROW (self), FALSE);

  return g_ptr_array_find (self->aps, ap, NULL);
}

void
cc_wifi_connection_row_update (CcWifiConnectionRow *self)
{
  update_ui (self);

  gtk_list_box_row_changed (GTK_LIST_BOX_ROW (self));

}

