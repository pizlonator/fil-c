/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Nathan-J. Hirschauer <nathanhi@deepserve.info>
 *
 * Licensed under the GNU General Public License Version 2
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
 */

#include "config.h"

#include <glib/gi18n.h>
#include <NetworkManager.h>

#include "ce-page.h"
#include "ce-page-wireguard.h"
#include "nma-ui-utils.h"
#include "vpn-helpers.h"

#include <ui-helpers.h>

struct _CEPageWireguard
{
        GtkBox               parent;

        GtkGrid             *main_box;
        GtkEntry            *entry_conname;
        GtkEntry            *entry_ifname;
        GtkEntry            *entry_private_key;
        GtkSpinButton       *spin_listen_port;
        GtkSpinButton       *spin_fwmark;
        GtkSpinButton       *spin_mtu;
        GtkWidget           *peers_box;
        GtkWidget           *empty_listbox;
        GtkButton           *button_add_peer;
        GtkCheckButton      *checkbutton_peer_routes;

        NMConnection        *connection;
        NMSettingConnection *setting_connection;
        NMSettingWireGuard  *setting_wireguard;
};

struct _WireguardPeer
{
        GtkBox           parent;

        GtkBox          *box;
        GtkLabel        *peer_label;
        GtkMenuButton   *button_configure;
        GtkMenuButton   *button_delete;

        GtkPopover      *peer_popover;

        // Provided by peer_popover
        GtkEntry        *entry_public_key;
        GtkEntry        *entry_allowed_ips;
        GtkEntry        *entry_endpoint;
        GtkEntry        *entry_psk;
        GtkSpinButton   *spin_persistent_keepalive;
        GtkButton       *button_apply;

        // Used to track whether the peer was newly constructed
        gboolean         is_unsaved;

        CEPageWireguard *ce_pg_wg;
        NMWireGuardPeer *nm_wg_peer;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_TYPE_WITH_CODE (CEPageWireguard, ce_page_wireguard, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (ce_page_get_type (), ce_page_iface_init));
G_DEFINE_TYPE (WireguardPeer, wireguard_peer, GTK_TYPE_BOX);

static void
ce_page_wireguard_dispose (GObject *object)
{
        CEPageWireguard *self = CE_PAGE_WIREGUARD (object);

        g_clear_object (&self->setting_connection);
        g_clear_object (&self->setting_wireguard);      
        G_OBJECT_CLASS (ce_page_wireguard_parent_class)->dispose (object);
}

static const gchar *
ce_page_wireguard_get_security_setting (CEPage *page)
{
        return NM_SETTING_WIREGUARD_SETTING_NAME;
}

static const gchar *
ce_page_wireguard_get_title (CEPage *page)
{
        return _("WireGuard");
}

static void
ui_to_setting (CEPageWireguard *self,
               GError **error)
{
        // Transform UI values to NM_SETTING
        NMSettingSecretFlags secret_flags;

        // Ensure that the spin boxes are updated
        gtk_spin_button_update (self->spin_listen_port);
        gtk_spin_button_update (self->spin_fwmark);
        gtk_spin_button_update (self->spin_mtu);

        // Update peers
        GtkWidget *widget;
        GList *peers = NULL;
        guint num_peers = 0;
        for (widget = gtk_widget_get_first_child (GTK_WIDGET (self->peers_box));
             widget != NULL;
             widget = gtk_widget_get_next_sibling (widget))
                peers = g_list_append (peers, widget);

        for (GList *p = peers; p != NULL; p = p->next) {
                WireguardPeer *peer = p->data;
                if (!WIREGUARD_IS_PEER (peer))
                        continue;

                nm_setting_wireguard_set_peer (self->setting_wireguard, peer->nm_wg_peer, num_peers);

                num_peers++;
        }
  
        g_list_free (peers);
        g_object_set (self->setting_connection,
                      NM_SETTING_CONNECTION_INTERFACE_NAME, gtk_editable_get_text (GTK_EDITABLE (self->entry_ifname)),
                      NM_SETTING_CONNECTION_ID, gtk_editable_get_text (GTK_EDITABLE (self->entry_conname)),
                      NULL);

        g_object_set (self->setting_wireguard,
                      NM_SETTING_WIREGUARD_PRIVATE_KEY, gtk_editable_get_text (GTK_EDITABLE (self->entry_private_key)),
                      NM_SETTING_WIREGUARD_FWMARK, (guint32)gtk_spin_button_get_value_as_int (self->spin_fwmark),
                      NM_SETTING_WIREGUARD_MTU, (guint32)gtk_spin_button_get_value_as_int (self->spin_mtu),
                      NM_SETTING_WIREGUARD_LISTEN_PORT, (guint32)gtk_spin_button_get_value_as_int (self->spin_listen_port),
                      NULL);
        secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->entry_private_key));
        nm_setting_set_secret_flags ((NMSetting *)self->setting_wireguard,
                                     NM_SETTING_WIREGUARD_PRIVATE_KEY,
                                     secret_flags, NULL);
        nma_utils_update_password_storage (GTK_WIDGET (self->entry_private_key),
                                           secret_flags,
                                           (NMSetting *)self->setting_wireguard,
                                           NM_SETTING_WIREGUARD_PRIVATE_KEY);
}

static gboolean
ce_page_wireguard_validate (CEPage *page,
                            NMConnection *connection,
                            GError **error)
{
        CEPageWireguard *self = CE_PAGE_WIREGUARD (page);
        ui_to_setting (self, error);

        for (guint p = 0; p < nm_setting_wireguard_get_peers_len (self->setting_wireguard); p++) {
                NMWireGuardPeer *peer = nm_setting_wireguard_get_peer (self->setting_wireguard, p);

                if (!nm_wireguard_peer_is_valid (peer, TRUE, TRUE, error))
                        return FALSE;
        }

        return nm_setting_verify (NM_SETTING (self->setting_connection), connection, error) &&
               nm_setting_verify_secrets (NM_SETTING (self->setting_wireguard), connection, error) &&
               nm_setting_verify (NM_SETTING (self->setting_wireguard), connection, error);
}

static void
ce_page_wireguard_init (CEPageWireguard *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_wireguard_class_init (CEPageWireguardClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

        object_class->dispose = ce_page_wireguard_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/wireguard-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, main_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, entry_conname);
        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, entry_ifname);
        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, entry_private_key);
        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, spin_listen_port);
        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, spin_fwmark);
        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, spin_mtu);
        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, checkbutton_peer_routes);
        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, peers_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, empty_listbox);
        gtk_widget_class_bind_template_child (widget_class, CEPageWireguard, button_add_peer);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
        iface->get_security_setting = ce_page_wireguard_get_security_setting;
        iface->get_title = ce_page_wireguard_get_title;
        iface->validate = ce_page_wireguard_validate;
}

static void
toggle_show_secret_cb (GtkEntry *entry_private_key, gpointer user_data)
{
        gtk_entry_set_visibility (entry_private_key,
                                  !gtk_entry_get_visibility (entry_private_key));
}

gchar *
peer_allowed_ips_to_str (NMWireGuardPeer *peer)
{
        gchar *allowed_ips = NULL;
        if (nm_wireguard_peer_get_allowed_ips_len (peer) != 0) {
                allowed_ips = (gchar *)nm_wireguard_peer_get_allowed_ip (peer, 0, NULL);
                
                for (guint i = 1; i < nm_wireguard_peer_get_allowed_ips_len (peer); i++) {
                        allowed_ips = g_strconcat (allowed_ips, ", ",
                                                   nm_wireguard_peer_get_allowed_ip (peer, i, NULL),
                                                   NULL);
                }
        }
        return allowed_ips;
}

static void
handle_peer_changed_cb (GtkButton *apply_button, WireguardPeer *wg_peer)
{
        NMWireGuardPeer *nm_wg_peer;
        NMSettingSecretFlags secret_flags;

        gboolean peer_is_valid = TRUE;
        const gchar *endpoint = gtk_editable_get_text (GTK_EDITABLE (wg_peer->entry_endpoint));
        const gchar *public_key = gtk_editable_get_text (GTK_EDITABLE (wg_peer->entry_public_key));
        const gchar *psk = gtk_editable_get_text (GTK_EDITABLE (wg_peer->entry_psk));
        const gchar *allowed_ips = gtk_editable_get_text (GTK_EDITABLE (wg_peer->entry_allowed_ips));
        guint16 keepalive = gtk_spin_button_get_value_as_int (wg_peer->spin_persistent_keepalive);

        nm_wg_peer = nm_wireguard_peer_new_clone (wg_peer->nm_wg_peer, TRUE);

        widget_unset_error (GTK_WIDGET (wg_peer->entry_endpoint));
        if (!nm_wireguard_peer_set_endpoint (nm_wg_peer,
                                             endpoint && endpoint[0] ? endpoint : NULL,
                                             FALSE)) {
                widget_set_error (GTK_WIDGET (wg_peer->entry_endpoint));
                peer_is_valid = FALSE;
        }

        widget_unset_error (GTK_WIDGET (wg_peer->entry_public_key));
        if (!nm_wireguard_peer_set_public_key (nm_wg_peer,
                                               public_key && public_key[0] ? public_key : NULL,
                                               FALSE)) {
                widget_set_error (GTK_WIDGET (wg_peer->entry_public_key));
                peer_is_valid = FALSE;
        }

        widget_unset_error (GTK_WIDGET (wg_peer->entry_psk));
        if (!nm_wireguard_peer_set_preshared_key (nm_wg_peer,
                                                  psk && psk[0] ? psk : NULL,
                                                  FALSE)) {
                widget_set_error (GTK_WIDGET (wg_peer->entry_psk));
                peer_is_valid = FALSE;
        } else if (psk && psk[0]) {
                secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (wg_peer->entry_psk));
                nm_wireguard_peer_set_preshared_key_flags (nm_wg_peer, secret_flags);
                nma_utils_update_password_storage (GTK_WIDGET (wg_peer->entry_psk),
                                                   nm_wireguard_peer_get_preshared_key_flags (wg_peer->nm_wg_peer),
                                                   NULL,
                                                   NULL);
        }

        nm_wireguard_peer_set_persistent_keepalive (nm_wg_peer, keepalive);

        /* Only update allowed IPs if a value actually changed.
         * Otherwise, the comparison will always differ, touching
         * the connection without any real changes.
         */
        widget_unset_error (GTK_WIDGET (wg_peer->entry_allowed_ips));
        if (g_strcmp0(peer_allowed_ips_to_str (nm_wg_peer), allowed_ips) != 0) {
                nm_wireguard_peer_clear_allowed_ips (nm_wg_peer);
                char **strv = g_strsplit (allowed_ips, ",", -1);
                for (guint i = 0; strv && strv[i]; i++) {
                        if (!nm_wireguard_peer_append_allowed_ip (nm_wg_peer,
                                                                  g_strstrip (strv[i]),
                                                                  FALSE)) {
                                widget_set_error (GTK_WIDGET (wg_peer->entry_allowed_ips));
                                peer_is_valid = FALSE;
                        }
                }
                g_strfreev (strv);
        }

        if (!nm_wireguard_peer_is_valid (nm_wg_peer, TRUE, TRUE, NULL) || !peer_is_valid)
                return;

        if (nm_wireguard_peer_cmp (wg_peer->nm_wg_peer, nm_wg_peer, NM_SETTING_COMPARE_FLAG_EXACT) == 0) {
                gtk_popover_popdown (wg_peer->peer_popover);
                return;
        }

        // Indicate that the peer has now been succesfully configured
        wg_peer->is_unsaved = FALSE;

        // Update peer list
        gtk_label_set_text (wg_peer->peer_label, gtk_editable_get_text (GTK_EDITABLE (wg_peer->entry_endpoint)));

        wg_peer->nm_wg_peer = nm_wg_peer;
        g_signal_emit_by_name (wg_peer->ce_pg_wg, "changed", wg_peer->ce_pg_wg);
        gtk_popover_popdown (wg_peer->peer_popover);
}

static void
destroy_peer (WireguardPeer *wg_peer)
{
        nm_wireguard_peer_unref (wg_peer->nm_wg_peer);
        GtkWidget* parent = gtk_widget_get_parent (GTK_WIDGET (wg_peer));
        gtk_box_remove (GTK_BOX (parent), GTK_WIDGET (wg_peer));
        gtk_widget_set_visible (GTK_WIDGET (wg_peer->ce_pg_wg->empty_listbox),
                                nm_setting_wireguard_get_peers_len (wg_peer->ce_pg_wg->setting_wireguard) < 1);
}

static void
handle_peer_delete_cb (GtkButton *delete_button, WireguardPeer *wg_peer)
{
        NMWireGuardPeer *peer;
        for (guint p = 0; p < nm_setting_wireguard_get_peers_len (wg_peer->ce_pg_wg->setting_wireguard); p++) {
                peer = nm_setting_wireguard_get_peer (wg_peer->ce_pg_wg->setting_wireguard, p);
                if (nm_wireguard_peer_cmp (wg_peer->nm_wg_peer, peer, NM_SETTING_COMPARE_FLAG_EXACT) == 0) {
                        nm_setting_wireguard_remove_peer (wg_peer->ce_pg_wg->setting_wireguard, p);
                        break;
                }
        }

        destroy_peer (wg_peer);
        g_signal_emit_by_name (wg_peer->ce_pg_wg, "changed", wg_peer->ce_pg_wg);
}

static void
handle_abort_new_peer (GtkPopover *peer_popover, WireguardPeer *wg_peer)
{
        if (wg_peer->is_unsaved == TRUE)
                destroy_peer (wg_peer);
        else
                g_signal_handlers_disconnect_by_func (peer_popover, handle_abort_new_peer, wg_peer);

        gtk_widget_set_visible (GTK_WIDGET (wg_peer->ce_pg_wg->empty_listbox),
                                nm_setting_wireguard_get_peers_len (wg_peer->ce_pg_wg->setting_wireguard) < 1);
}

WireguardPeer *
add_nm_wg_peer_to_list (CEPageWireguard *self, NMWireGuardPeer *peer)
{
        WireguardPeer *wg_peer;
        gchar *endpoint = (gchar *)nm_wireguard_peer_get_endpoint (peer);
        if (!endpoint) {
                /* Translators: Unknown endpoint host for WireGuard (invalid setting) */
                endpoint = _("Unknown");
        }

        wg_peer = wireguard_peer_new (self);
        wg_peer->nm_wg_peer = peer;
        wg_peer->is_unsaved = FALSE;

        gtk_label_set_text (wg_peer->peer_label, endpoint);

        gtk_editable_set_text (GTK_EDITABLE (wg_peer->entry_endpoint), endpoint);
        if (peer_allowed_ips_to_str (peer) != NULL)
                gtk_editable_set_text (GTK_EDITABLE (wg_peer->entry_allowed_ips), peer_allowed_ips_to_str (peer));
        if (nm_wireguard_peer_get_preshared_key (peer) != NULL)
                gtk_editable_set_text (GTK_EDITABLE (wg_peer->entry_psk), nm_wireguard_peer_get_preshared_key (peer));
        if (nm_wireguard_peer_get_public_key (peer) != NULL)
                gtk_editable_set_text (GTK_EDITABLE (wg_peer->entry_public_key), nm_wireguard_peer_get_public_key (peer));

        gtk_spin_button_set_value (wg_peer->spin_persistent_keepalive, nm_wireguard_peer_get_persistent_keepalive (peer));

        g_signal_connect (wg_peer->button_apply, "clicked", G_CALLBACK (handle_peer_changed_cb), wg_peer);
        g_signal_connect (wg_peer->button_delete, "clicked", G_CALLBACK (handle_peer_delete_cb), wg_peer);
        g_signal_connect (wg_peer->entry_psk, "icon-press", G_CALLBACK (toggle_show_secret_cb), NULL);
        gtk_widget_show (GTK_WIDGET (wg_peer));
        gtk_box_append (GTK_BOX (self->peers_box), GTK_WIDGET (wg_peer));

        return wg_peer;
}

static void
handle_peer_add_cb (CEPageWireguard *self)
{
        NMWireGuardPeer *nm_wg_peer = nm_wireguard_peer_new ();
        WireguardPeer *wg_peer = add_nm_wg_peer_to_list (self, nm_wg_peer);
        wg_peer->is_unsaved = TRUE;
        wg_peer->ce_pg_wg = self;

        gtk_widget_set_visible (GTK_WIDGET (self->empty_listbox), FALSE);
        gtk_label_set_text (wg_peer->peer_label, _("Unsaved peer"));
        gtk_popover_popup (wg_peer->peer_popover);
        g_signal_connect (wg_peer->peer_popover, "closed", G_CALLBACK (handle_abort_new_peer), wg_peer);
}

static void
finish_setup (CEPageWireguard *self, gpointer unused, GError *error, gpointer user_data)
{
        const gchar *ifname, *conname, *privkey;
        const guint32 listen_port_default = 51820;
        guint32 listen_port, fwmark, mtu;

        self->setting_connection = nm_connection_get_setting_connection (self->connection);
        self->setting_wireguard = (NMSettingWireGuard *)nm_connection_get_setting (self->connection, NM_TYPE_SETTING_WIREGUARD);

        conname = nm_connection_get_id (self->connection);
        if (conname != NULL)
                gtk_editable_set_text (GTK_EDITABLE (self->entry_conname), conname);
        ifname = nm_connection_get_interface_name (self->connection);
        if (ifname != NULL)
                gtk_editable_set_text (GTK_EDITABLE (self->entry_ifname), ifname);
        privkey = nm_setting_wireguard_get_private_key (self->setting_wireguard);
        if (privkey != NULL)
                gtk_editable_set_text (GTK_EDITABLE (self->entry_private_key), privkey);
        g_signal_connect (self->entry_private_key, "icon-press", G_CALLBACK (toggle_show_secret_cb), NULL);

        listen_port = nm_setting_wireguard_get_listen_port (self->setting_wireguard);
        if (listen_port != 0 && listen_port != 51820)
                gtk_spin_button_set_value (self->spin_listen_port, listen_port);
        else {
                gtk_spin_button_set_value (self->spin_listen_port, listen_port_default);
        }
        fwmark = nm_setting_wireguard_get_fwmark (self->setting_wireguard);
        gtk_spin_button_set_value (self->spin_fwmark, fwmark);
        mtu = nm_setting_wireguard_get_mtu (self->setting_wireguard);
        gtk_spin_button_set_value (self->spin_mtu, mtu);
        for (guint p = 0;
             p < nm_setting_wireguard_get_peers_len (self->setting_wireguard);
             p++) {
                NMWireGuardPeer *peer = nm_setting_wireguard_get_peer (self->setting_wireguard, p);
                add_nm_wg_peer_to_list (self, peer);
        }

        gtk_widget_set_visible (self->empty_listbox,
                                nm_setting_wireguard_get_peers_len (self->setting_wireguard) < 1);
        gtk_check_button_set_active (self->checkbutton_peer_routes,
                                     nm_setting_wireguard_get_peer_routes (self->setting_wireguard));

        g_signal_connect_swapped (self->button_add_peer, "clicked", G_CALLBACK (handle_peer_add_cb), self);
}

CEPageWireguard *
ce_page_wireguard_new (NMConnection *connection)
{
        CEPageWireguard *self = CE_PAGE_WIREGUARD (g_object_new (ce_page_wireguard_get_type (), NULL));

        self->connection = g_object_ref (connection);

        g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

        return self;
}

static void
wireguard_peer_init (WireguardPeer *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

WireguardPeer *
wireguard_peer_new (CEPageWireguard *parent)
{
        WireguardPeer *self;

        self = g_object_new (wireguard_peer_get_type (), NULL);
        self->ce_pg_wg = parent;
        self->is_unsaved = TRUE;
        return self;
}

static void
wireguard_peer_class_init (WireguardPeerClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/wireguard-peer.ui");

        gtk_widget_class_bind_template_child (widget_class, WireguardPeer, peer_label);
        gtk_widget_class_bind_template_child (widget_class, WireguardPeer, button_configure);
        gtk_widget_class_bind_template_child (widget_class, WireguardPeer, button_delete);
        gtk_widget_class_bind_template_child (widget_class, WireguardPeer, entry_public_key);
        gtk_widget_class_bind_template_child (widget_class, WireguardPeer, entry_allowed_ips);
        gtk_widget_class_bind_template_child (widget_class, WireguardPeer, entry_endpoint);
        gtk_widget_class_bind_template_child (widget_class, WireguardPeer, entry_psk);
        gtk_widget_class_bind_template_child (widget_class, WireguardPeer, spin_persistent_keepalive);
        gtk_widget_class_bind_template_child (widget_class, WireguardPeer, peer_popover);
        gtk_widget_class_bind_template_child (widget_class, WireguardPeer, button_apply);
}