/* cc-permission-infobar.c
 *
 * Copyright (C) 2020 Red Hat, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Felipe Borges <felipeborges@gnome.org>
 *
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-permission-infobar"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib/gi18n.h>
#include <adwaita.h>

#include "cc-permission-infobar.h"

struct _CcPermissionInfobar
{
  AdwBin         parent_instance;

  AdwBanner     *banner;
  GPermission   *permission;

  GCancellable *cancellable;
};

G_DEFINE_TYPE (CcPermissionInfobar, cc_permission_infobar, ADW_TYPE_BIN)

static void
on_permission_changed (CcPermissionInfobar *self)
{
  gboolean is_authorized = g_permission_get_allowed (self->permission);

  adw_banner_set_revealed (self->banner, !is_authorized);
}

static void
acquire_cb (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
  CcPermissionInfobar *self = CC_PERMISSION_INFOBAR (user_data);
  g_autoptr (GError) error = NULL;

  if (!g_permission_acquire_finish (self->permission, result, &error))
    {
      g_warning ("Error acquiring permission: %s", error->message);
    }

  g_clear_object (&self->cancellable);
}

static void
release_cb (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
  CcPermissionInfobar *self = CC_PERMISSION_INFOBAR (user_data);
  g_autoptr (GError) error = NULL;

  if (!g_permission_release_finish (self->permission, result, &error))
    {
      g_warning ("Error releasing permission: %s", error->message);
      g_error_free (error);
    }

  g_clear_object (&self->cancellable);
}

static void
banner_button_clicked_cb (CcPermissionInfobar *self)
{
  /* if we already have a pending interactive check or permission is not set,
   * then do nothing
   */
  if (self->cancellable != NULL || self->permission == NULL)
    return;

  if (g_permission_get_allowed (self->permission))
    {
      if (g_permission_get_can_release (self->permission))
        {
          self->cancellable = g_cancellable_new ();

          g_permission_release_async (self->permission,
                                      self->cancellable,
                                      release_cb,
                                      self);
        }
    }
  else
    {
      if (g_permission_get_can_acquire (self->permission))
        {
          self->cancellable = g_cancellable_new ();

          g_permission_acquire_async (self->permission,
                                      self->cancellable,
                                      acquire_cb,
                                      self);
        }
    }
}

static void
cc_permission_infobar_dispose (GObject *object)
{
  CcPermissionInfobar *self = CC_PERMISSION_INFOBAR (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
    }

  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (cc_permission_infobar_parent_class)->dispose (object);
}

static void
cc_permission_infobar_class_init (CcPermissionInfobarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_permission_infobar_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "common/cc-permission-infobar.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPermissionInfobar, banner);
  gtk_widget_class_bind_template_callback (widget_class,
                                           banner_button_clicked_cb);
}

static void
cc_permission_infobar_init (CcPermissionInfobar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Set the default title. */
  cc_permission_infobar_set_title (self, NULL);
}

void
cc_permission_infobar_set_permission (CcPermissionInfobar *self,
                                      GPermission         *permission)
{
  g_return_if_fail (CC_IS_PERMISSION_INFOBAR (self));

  if (permission == NULL)
    {
      g_warning ("Missing GPermission object");
      return;
    }

  self->permission = permission;

  g_signal_connect_object (permission, "notify",
                           G_CALLBACK (on_permission_changed),
                           self,
                           G_CONNECT_SWAPPED);
  on_permission_changed (self);
}

/**
 * cc_permission_infobar_set_title:
 * @self: a #CcPermissionInfobar
 * @title: (nullable): title to display in the infobar, or %NULL for the default
 *
 * Set the title text to display in the infobar.
 */
void
cc_permission_infobar_set_title (CcPermissionInfobar *self,
                                 const gchar         *title)
{
  g_return_if_fail (CC_IS_PERMISSION_INFOBAR (self));

  if (self->permission == NULL)
    title = _("Error: some settings cannot be unlocked");

  if (title == NULL)
    title = _("Unlock to Change Settings");

  adw_banner_set_title (self->banner, title);
  adw_banner_set_button_label (self->banner, self->permission ? _("_Unlock…") : NULL);
}
