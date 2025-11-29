/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Red Hat, Inc.
 * Author: Matthias Clasen
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtklockbuttonprivate.h"

#include "gtkbox.h"
#include "gtkimage.h"
#include <glib/gi18n-lib.h>
#include "gtklabel.h"
#include "gtksizegroup.h"
#include "gtkstack.h"
#include "gtkprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkLockButton:
 *
 * `GtkLockButton` is a widget to obtain and revoke authorizations
 * needed to operate the controls.
 *
 * ![An example GtkLockButton](lock-button.png)
 *
 * It is typically used in preference dialogs or control panels.
 *
 * The required authorization is represented by a `GPermission` object.
 * Concrete implementations of `GPermission` may use PolicyKit or some
 * other authorization framework. To obtain a PolicyKit-based
 * `GPermission`, use `polkit_permission_new()`.
 *
 * If the user is not currently allowed to perform the action, but can
 * obtain the permission, the widget looks like this:
 *
 * ![](lockbutton-locked.png)
 *
 * and the user can click the button to request the permission. Depending
 * on the platform, this may pop up an authentication dialog or ask the user
 * to authenticate in some other way. Once the user has obtained the permission,
 * the widget changes to this:
 *
 * ![](lockbutton-unlocked.png)
 *
 * and the permission can be dropped again by clicking the button. If the user
 * is not able to obtain the permission at all, the widget looks like this:
 *
 * ![](lockbutton-sorry.png)
 *
 * If the user has the permission and cannot drop it, the button is hidden.
 *
 * The text (and tooltips) that are shown in the various cases can be adjusted
 * with the [property@Gtk.LockButton:text-lock],
 * [property@Gtk.LockButton:text-unlock],
 * [property@Gtk.LockButton:tooltip-lock],
 * [property@Gtk.LockButton:tooltip-unlock] and
 * [property@Gtk.LockButton:tooltip-not-authorized] properties.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */

struct _GtkLockButton
{
  GtkButton parent_instance;

  GPermission *permission;
  GCancellable *cancellable;

  char *tooltip_lock;
  char *tooltip_unlock;
  char *tooltip_not_authorized;
  GIcon *icon_lock;
  GIcon *icon_unlock;

  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *stack;
  GtkWidget *label_lock;
  GtkWidget *label_unlock;
};

typedef struct _GtkLockButtonClass   GtkLockButtonClass;
struct _GtkLockButtonClass
{
  GtkButtonClass parent_class;
};

enum
{
  PROP_0,
  PROP_PERMISSION,
  PROP_TEXT_LOCK,
  PROP_TEXT_UNLOCK,
  PROP_TOOLTIP_LOCK,
  PROP_TOOLTIP_UNLOCK,
  PROP_TOOLTIP_NOT_AUTHORIZED
};

static void update_state (GtkLockButton *button);
static void gtk_lock_button_clicked (GtkButton *button);

static void on_permission_changed (GPermission *permission,
                                   GParamSpec  *pspec,
                                   gpointer     user_data);

G_DEFINE_TYPE (GtkLockButton, gtk_lock_button, GTK_TYPE_BUTTON)

static void
gtk_lock_button_finalize (GObject *object)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (object);

  g_free (button->tooltip_lock);
  g_free (button->tooltip_unlock);
  g_free (button->tooltip_not_authorized);

  g_object_unref (button->icon_lock);
  g_object_unref (button->icon_unlock);

  if (button->cancellable != NULL)
    {
      g_cancellable_cancel (button->cancellable);
      g_object_unref (button->cancellable);
    }

  if (button->permission)
    {
      g_signal_handlers_disconnect_by_func (button->permission,
                                            on_permission_changed,
                                            button);
      g_object_unref (button->permission);
    }

  G_OBJECT_CLASS (gtk_lock_button_parent_class)->finalize (object);
}

static void
gtk_lock_button_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (object);

  switch (property_id)
    {
    case PROP_PERMISSION:
      g_value_set_object (value, button->permission);
      break;

    case PROP_TEXT_LOCK:
      g_value_set_string (value, gtk_label_get_text (GTK_LABEL (button->label_lock)));
      break;

    case PROP_TEXT_UNLOCK:
      g_value_set_string (value, gtk_label_get_text (GTK_LABEL (button->label_unlock)));
      break;

    case PROP_TOOLTIP_LOCK:
      g_value_set_string (value, button->tooltip_lock);
      break;

    case PROP_TOOLTIP_UNLOCK:
      g_value_set_string (value, button->tooltip_unlock);
      break;

    case PROP_TOOLTIP_NOT_AUTHORIZED:
      g_value_set_string (value, button->tooltip_not_authorized);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_lock_button_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (object);

  switch (property_id)
    {
    case PROP_PERMISSION:
      gtk_lock_button_set_permission (button, g_value_get_object (value));
      break;

    case PROP_TEXT_LOCK:
      gtk_label_set_text (GTK_LABEL (button->label_lock), g_value_get_string (value));
      break;

    case PROP_TEXT_UNLOCK:
      gtk_label_set_text (GTK_LABEL (button->label_unlock), g_value_get_string (value));
      break;

    case PROP_TOOLTIP_LOCK:
      g_free (button->tooltip_lock);
      button->tooltip_lock = g_value_dup_string (value);
      break;

    case PROP_TOOLTIP_UNLOCK:
      g_free (button->tooltip_unlock);
      button->tooltip_unlock = g_value_dup_string (value);
      break;

    case PROP_TOOLTIP_NOT_AUTHORIZED:
      g_free (button->tooltip_not_authorized);
      button->tooltip_not_authorized = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }

  update_state (button);
}

static void
gtk_lock_button_init (GtkLockButton *button)
{
  const char *names[3];

  gtk_widget_init_template (GTK_WIDGET (button));

  names[0] = "changes-allow-symbolic";
  names[1] = "changes-allow";
  names[2] = NULL;
  button->icon_unlock = g_themed_icon_new_from_names ((char **) names, -1);

  names[0] = "changes-prevent-symbolic";
  names[1] = "changes-prevent";
  names[2] = NULL;
  button->icon_lock = g_themed_icon_new_from_names ((char **) names, -1);

  update_state (button);

  gtk_widget_add_css_class (GTK_WIDGET (button), I_("lock"));
}

static void
gtk_lock_button_class_init (GtkLockButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);

  gobject_class->finalize     = gtk_lock_button_finalize;
  gobject_class->get_property = gtk_lock_button_get_property;
  gobject_class->set_property = gtk_lock_button_set_property;

  button_class->clicked = gtk_lock_button_clicked;

  /**
   * GtkLockButton:permission: (attributes org.gtk.Property.get=gtk_lock_button_get_permission org.gtk.Property.set=gtk_lock_button_set_permission)
   *
   * The `GPermission object controlling this button.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  g_object_class_install_property (gobject_class, PROP_PERMISSION,
    g_param_spec_object ("permission", NULL, NULL,
                         G_TYPE_PERMISSION,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS));

  /**
   * GtkLockButton:text-lock:
   *
   * The text to display when prompting the user to lock.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  g_object_class_install_property (gobject_class, PROP_TEXT_LOCK,
    g_param_spec_string ("text-lock", NULL, NULL,
                         _("Lock"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  /**
   * GtkLockButton:text-unlock:
   *
   * The text to display when prompting the user to unlock.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  g_object_class_install_property (gobject_class, PROP_TEXT_UNLOCK,
    g_param_spec_string ("text-unlock", NULL, NULL,
                         _("Unlock"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  /**
   * GtkLockButton:tooltip-lock:
   *
   * The tooltip to display when prompting the user to lock.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  g_object_class_install_property (gobject_class, PROP_TOOLTIP_LOCK,
    g_param_spec_string ("tooltip-lock", NULL, NULL,
                         _("Dialog is unlocked.\nClick to prevent further changes"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  /**
   * GtkLockButton:tooltip-unlock:
   *
   * The tooltip to display when prompting the user to unlock.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  g_object_class_install_property (gobject_class, PROP_TOOLTIP_UNLOCK,
    g_param_spec_string ("tooltip-unlock", NULL, NULL,
                         _("Dialog is locked.\nClick to make changes"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  /**
   * GtkLockButton:tooltip-not-authorized:
   *
   * The tooltip to display when the user cannot obtain authorization.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  g_object_class_install_property (gobject_class, PROP_TOOLTIP_NOT_AUTHORIZED,
    g_param_spec_string ("tooltip-not-authorized", NULL, NULL,
                         _("System policy prevents changes.\nContact your system administrator"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtklockbutton.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkLockButton, box);
  gtk_widget_class_bind_template_child (widget_class, GtkLockButton, image);
  gtk_widget_class_bind_template_child (widget_class, GtkLockButton, label_lock);
  gtk_widget_class_bind_template_child (widget_class, GtkLockButton, label_unlock);
  gtk_widget_class_bind_template_child (widget_class, GtkLockButton, stack);

  gtk_widget_class_set_css_name (widget_class, I_("button"));
}

static void
update_state (GtkLockButton *button)
{
  gboolean allowed;
  gboolean can_acquire;
  gboolean can_release;
  gboolean sensitive;
  gboolean visible;
  GIcon *icon;
  const char *tooltip;

  if (button->permission)
    {
      allowed = g_permission_get_allowed (button->permission);
      can_acquire = g_permission_get_can_acquire (button->permission);
      can_release = g_permission_get_can_release (button->permission);
    }
  else
    {
      allowed = TRUE;
      can_acquire = FALSE;
      can_release = FALSE;
    }

  if (allowed && can_release)
    {
      visible = TRUE;
      sensitive = TRUE;
      icon = button->icon_lock;
      tooltip = button->tooltip_lock;
    }
  else if (allowed && !can_release)
    {
      visible = FALSE;
      sensitive = TRUE;
      icon = button->icon_lock;
      tooltip = button->tooltip_lock;
    }
  else if (!allowed && can_acquire)
    {
      visible = TRUE;
      sensitive = TRUE;
      icon = button->icon_unlock;
      tooltip = button->tooltip_unlock;
    }
  else if (!allowed && !can_acquire)
    {
      visible = TRUE;
      sensitive = FALSE;
      icon = button->icon_unlock;
      tooltip = button->tooltip_not_authorized;
    }
  else
    {
      g_assert_not_reached ();
    }

  gtk_image_set_from_gicon (GTK_IMAGE (button->image), icon);
  gtk_stack_set_visible_child (GTK_STACK (button->stack),
                               allowed ? button->label_lock : button->label_unlock);
  gtk_widget_set_tooltip_markup (GTK_WIDGET (button), tooltip);
  gtk_widget_set_sensitive (GTK_WIDGET (button), sensitive);
  gtk_widget_set_visible (GTK_WIDGET (button), visible);
}

static void
on_permission_changed (GPermission *permission,
                       GParamSpec  *pspec,
                       gpointer     user_data)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (user_data);

  update_state (button);
}

static void
acquire_cb (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (user_data);
  GError *error;

  error = NULL;
  if (!g_permission_acquire_finish (button->permission, result, &error))
    {
      g_warning ("Error acquiring permission: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (button->cancellable);
  button->cancellable = NULL;

  update_state (button);
}

static void
release_cb (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (user_data);
  GError *error;

  error = NULL;
  if (!g_permission_release_finish (button->permission, result, &error))
    {
      g_warning ("Error releasing permission: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (button->cancellable);
  button->cancellable = NULL;

  update_state (button);
}

static void
gtk_lock_button_clicked (GtkButton *widget)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (widget);

  /* if we already have a pending interactive check or permission is not set,
   * then do nothing
   */
  if (button->cancellable != NULL || button->permission == NULL)
    return;

  if (g_permission_get_allowed (button->permission))
    {
      if (g_permission_get_can_release (button->permission))
        {
          button->cancellable = g_cancellable_new ();

          g_permission_release_async (button->permission,
                                      button->cancellable,
                                      release_cb,
                                      button);
        }
    }
  else
    {
      if (g_permission_get_can_acquire (button->permission))
        {
          button->cancellable = g_cancellable_new ();

          g_permission_acquire_async (button->permission,
                                      button->cancellable,
                                      acquire_cb,
                                      button);
        }
    }
}

/**
 * gtk_lock_button_new:
 * @permission: (nullable): a `GPermission`
 *
 * Creates a new lock button which reflects the @permission.
 *
 * Returns: a new `GtkLockButton`
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
GtkWidget *
gtk_lock_button_new (GPermission *permission)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_LOCK_BUTTON,
                                   "permission", permission,
                                   NULL));
}

/**
 * gtk_lock_button_get_permission: (attributes org.gtk.Method.get_property=permission)
 * @button: a `GtkLockButton`
 *
 * Obtains the `GPermission` object that controls @button.
 *
 * Returns: (transfer none) (nullable): the `GPermission` of @button
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
GPermission *
gtk_lock_button_get_permission (GtkLockButton *button)
{
  g_return_val_if_fail (GTK_IS_LOCK_BUTTON (button), NULL);

  return button->permission;
}

/**
 * gtk_lock_button_set_permission: (attributes org.gtk.Method.set_property=permission)
 * @button: a `GtkLockButton`
 * @permission: (nullable): a `GPermission` object
 *
 * Sets the `GPermission` object that controls @button.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_lock_button_set_permission (GtkLockButton *button,
                                GPermission   *permission)
{
  g_return_if_fail (GTK_IS_LOCK_BUTTON (button));
  g_return_if_fail (permission == NULL || G_IS_PERMISSION (permission));

  if (button->permission != permission)
    {
      if (button->permission)
        {
          g_signal_handlers_disconnect_by_func (button->permission,
                                                on_permission_changed,
                                                button);
          g_object_unref (button->permission);
        }

      button->permission = permission;

      if (button->permission)
        {
          g_object_ref (button->permission);
          g_signal_connect (button->permission, "notify",
                            G_CALLBACK (on_permission_changed), button);
        }

      update_state (button);

      g_object_notify (G_OBJECT (button), "permission");
    }
}

const char *
_gtk_lock_button_get_current_text (GtkLockButton *button)
{
  GtkWidget *label;

  g_return_val_if_fail (GTK_IS_LOCK_BUTTON (button), NULL);

  label = gtk_stack_get_visible_child (GTK_STACK (button->stack));

  return gtk_label_get_text (GTK_LABEL (label));
}

