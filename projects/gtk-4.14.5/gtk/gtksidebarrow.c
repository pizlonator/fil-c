/* gtksidebarrow.c
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtksidebarrowprivate.h"
/* For section and place type enums */
#include "gtkplacessidebarprivate.h"
#include "gtkwidget.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkrevealer.h"
#include "gtkspinner.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkprivate.h"

#ifdef HAVE_CLOUDPROVIDERS
#include <cloudproviders.h>
#endif

struct _GtkSidebarRow
{
  GtkListBoxRow parent_instance;
  GIcon *start_icon;
  GIcon *end_icon;
  GtkWidget *start_icon_widget;
  GtkWidget *end_icon_widget;
  char *label;
  char *tooltip;
  GtkWidget *label_widget;
  gboolean ejectable;
  GtkWidget *eject_button;
  int order_index;
  GtkPlacesSectionType section_type;
  GtkPlacesPlaceType place_type;
  char *uri;
  GDrive *drive;
  GVolume *volume;
  GMount *mount;
  GObject *cloud_provider_account;
  gboolean placeholder;
  GtkPlacesSidebar *sidebar;
  GtkWidget *revealer;
  GtkWidget *busy_spinner;
};

G_DEFINE_TYPE (GtkSidebarRow, gtk_sidebar_row, GTK_TYPE_LIST_BOX_ROW)

enum
{
  PROP_0,
  PROP_START_ICON,
  PROP_END_ICON,
  PROP_LABEL,
  PROP_TOOLTIP,
  PROP_EJECTABLE,
  PROP_SIDEBAR,
  PROP_ORDER_INDEX,
  PROP_SECTION_TYPE,
  PROP_PLACE_TYPE,
  PROP_URI,
  PROP_DRIVE,
  PROP_VOLUME,
  PROP_MOUNT,
  PROP_CLOUD_PROVIDER_ACCOUNT,
  PROP_PLACEHOLDER,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

#ifdef HAVE_CLOUDPROVIDERS

static void
cloud_row_update (GtkSidebarRow *self)
{
  CloudProvidersAccount *account;
  GIcon *end_icon;
  int provider_status;

  account = CLOUD_PROVIDERS_ACCOUNT (self->cloud_provider_account);
  provider_status = cloud_providers_account_get_status (account);
  switch (provider_status)
    {
      case CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE:
        end_icon = NULL;
        break;

      case CLOUD_PROVIDERS_ACCOUNT_STATUS_SYNCING:
        end_icon = g_themed_icon_new ("emblem-synchronizing-symbolic");
        break;

      case CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR:
        end_icon = g_themed_icon_new ("dialog-warning-symbolic");
        break;

      default:
        return;
    }

  g_object_set (self,
                "label", cloud_providers_account_get_name (account),
                NULL);
  g_object_set (self,
                "tooltip", cloud_providers_account_get_status_details (account),
                NULL);
  g_object_set (self,
                "end-icon", end_icon,
                NULL);

  if (end_icon != NULL)
    g_object_unref (end_icon);
}

#endif

static void
gtk_sidebar_row_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkSidebarRow *self = GTK_SIDEBAR_ROW (object);

  switch (prop_id)
    {
    case PROP_SIDEBAR:
      g_value_set_object (value, self->sidebar);
      break;

    case PROP_START_ICON:
      g_value_set_object (value, self->start_icon);
      break;

    case PROP_END_ICON:
      g_value_set_object (value, self->end_icon);
      break;

    case PROP_LABEL:
      g_value_set_string (value, self->label);
      break;

    case PROP_TOOLTIP:
      g_value_set_string (value, self->tooltip);
      break;

    case PROP_EJECTABLE:
      g_value_set_boolean (value, self->ejectable);
      break;

    case PROP_ORDER_INDEX:
      g_value_set_int (value, self->order_index);
      break;

    case PROP_SECTION_TYPE:
      g_value_set_enum (value, self->section_type);
      break;

    case PROP_PLACE_TYPE:
      g_value_set_enum (value, self->place_type);
      break;

    case PROP_URI:
      g_value_set_string (value, self->uri);
      break;

    case PROP_DRIVE:
      g_value_set_object (value, self->drive);
      break;

    case PROP_VOLUME:
      g_value_set_object (value, self->volume);
      break;

    case PROP_MOUNT:
      g_value_set_object (value, self->mount);
      break;

    case PROP_CLOUD_PROVIDER_ACCOUNT:
      g_value_set_object (value, self->cloud_provider_account);
      break;

    case PROP_PLACEHOLDER:
      g_value_set_boolean (value, self->placeholder);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_sidebar_row_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkSidebarRow *self = GTK_SIDEBAR_ROW (object);

  switch (prop_id)
    {
    case PROP_SIDEBAR:
      self->sidebar = g_value_get_object (value);
      break;

    case PROP_START_ICON:
      {
        g_clear_object (&self->start_icon);
        object = g_value_get_object (value);
        if (object != NULL)
          {
            self->start_icon = G_ICON (g_object_ref (object));
            gtk_image_set_from_gicon (GTK_IMAGE (self->start_icon_widget), self->start_icon);
          }
        else
          {
            gtk_image_clear (GTK_IMAGE (self->start_icon_widget));
          }
        break;
      }

    case PROP_END_ICON:
      {
        g_clear_object (&self->end_icon);
        object = g_value_get_object (value);
        if (object != NULL)
          {
            self->end_icon = G_ICON (g_object_ref (object));
            gtk_image_set_from_gicon (GTK_IMAGE (self->end_icon_widget), self->end_icon);
          }
        else
          {
            gtk_image_clear (GTK_IMAGE (self->end_icon_widget));
          }
          gtk_widget_set_visible (self->end_icon_widget, object != NULL);
        break;
      }

    case PROP_LABEL:
      g_free (self->label);
      self->label = g_strdup (g_value_get_string (value));
      gtk_label_set_text (GTK_LABEL (self->label_widget), self->label);
      break;

    case PROP_TOOLTIP:
      g_free (self->tooltip);
      self->tooltip = g_strdup (g_value_get_string (value));
      gtk_widget_set_tooltip_text (GTK_WIDGET (self), self->tooltip);
      break;

    case PROP_EJECTABLE:
      self->ejectable = g_value_get_boolean (value);
      gtk_widget_set_visible (self->eject_button, self->ejectable);
      break;

    case PROP_ORDER_INDEX:
      self->order_index = g_value_get_int (value);
      break;

    case PROP_SECTION_TYPE:
      self->section_type = g_value_get_enum (value);
      break;

    case PROP_PLACE_TYPE:
      self->place_type = g_value_get_enum (value);
      break;

    case PROP_URI:
      g_free (self->uri);
      self->uri = g_strdup (g_value_get_string (value));
      break;

    case PROP_DRIVE:
      g_set_object (&self->drive, g_value_get_object (value));
      break;

    case PROP_VOLUME:
      g_set_object (&self->volume, g_value_get_object (value));
      break;

    case PROP_MOUNT:
      g_set_object (&self->mount, g_value_get_object (value));
      break;

    case PROP_CLOUD_PROVIDER_ACCOUNT:
#ifdef HAVE_CLOUDPROVIDERS
      if (self->cloud_provider_account != NULL)
        g_signal_handlers_disconnect_by_data (self->cloud_provider_account, self);

      self->cloud_provider_account = g_value_dup_object (value);

      if (self->cloud_provider_account != NULL)
        {
          g_signal_connect_swapped (self->cloud_provider_account, "notify::name",
                                    G_CALLBACK (cloud_row_update), self);
          g_signal_connect_swapped (self->cloud_provider_account, "notify::status",
                                    G_CALLBACK (cloud_row_update), self);
          g_signal_connect_swapped (self->cloud_provider_account, "notify::status-details",
                                    G_CALLBACK (cloud_row_update), self);
        }
#endif
      break;

    case PROP_PLACEHOLDER:
      {
        self->placeholder = g_value_get_boolean (value);
        if (self->placeholder)
          {
            g_clear_object (&self->start_icon);
            g_clear_object (&self->end_icon);
            g_free (self->label);
            self->label = NULL;
            g_free (self->tooltip);
            self->tooltip = NULL;
            gtk_widget_set_tooltip_text (GTK_WIDGET (self), NULL);
            self->ejectable = FALSE;
            self->section_type = GTK_PLACES_SECTION_BOOKMARKS;
            self->place_type = GTK_PLACES_BOOKMARK_PLACEHOLDER;
            g_free (self->uri);
            self->uri = NULL;
            g_clear_object (&self->drive);
            g_clear_object (&self->volume);
            g_clear_object (&self->mount);
            g_clear_object (&self->cloud_provider_account);

            gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (self), NULL);

            gtk_widget_add_css_class (GTK_WIDGET (self), "sidebar-placeholder-row");
          }

        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
on_child_revealed (GObject    *self,
                   GParamSpec *pspec,
                   gpointer    user_data)
{
 /* We need to hide the actual widget because if not the GtkListBoxRow will
  * still allocate the paddings, even if the revealer is not revealed, and
  * therefore the row will be still somewhat visible. */
  if (!gtk_revealer_get_reveal_child (GTK_REVEALER (self)))
    gtk_widget_set_visible (GTK_WIDGET (GTK_SIDEBAR_ROW (user_data)), FALSE);
}

void
gtk_sidebar_row_reveal (GtkSidebarRow *self)
{
  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);
}

void
gtk_sidebar_row_hide (GtkSidebarRow *self,
                      gboolean       immediate)
{
  guint transition_duration;

  transition_duration = gtk_revealer_get_transition_duration (GTK_REVEALER (self->revealer));
  if (immediate)
      gtk_revealer_set_transition_duration (GTK_REVEALER (self->revealer), 0);

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);

  gtk_revealer_set_transition_duration (GTK_REVEALER (self->revealer), transition_duration);
}

void
gtk_sidebar_row_set_start_icon (GtkSidebarRow *self,
                                GIcon         *icon)
{
  g_return_if_fail (GTK_IS_SIDEBAR_ROW (self));

  if (self->start_icon != icon)
    {
      g_set_object (&self->start_icon, icon);
      if (self->start_icon != NULL)
        gtk_image_set_from_gicon (GTK_IMAGE (self->start_icon_widget), self->start_icon);
      else
        gtk_image_clear (GTK_IMAGE (self->start_icon_widget));

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_START_ICON]);
    }
}

void
gtk_sidebar_row_set_end_icon (GtkSidebarRow *self,
                              GIcon         *icon)
{
  g_return_if_fail (GTK_IS_SIDEBAR_ROW (self));

  if (self->end_icon != icon)
    {
      g_set_object (&self->end_icon, icon);
      if (self->end_icon != NULL)
        gtk_image_set_from_gicon (GTK_IMAGE (self->end_icon_widget), self->end_icon);
      else
        if (self->end_icon_widget != NULL)
          gtk_image_clear (GTK_IMAGE (self->end_icon_widget));

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_END_ICON]);
    }
}

static void
gtk_sidebar_row_finalize (GObject *object)
{
  GtkSidebarRow *self = GTK_SIDEBAR_ROW (object);

  g_clear_object (&self->start_icon);
  g_clear_object (&self->end_icon);
  g_free (self->label);
  self->label = NULL;
  g_free (self->tooltip);
  self->tooltip = NULL;
  g_free (self->uri);
  self->uri = NULL;
  g_clear_object (&self->drive);
  g_clear_object (&self->volume);
  g_clear_object (&self->mount);
#ifdef HAVE_CLOUDPROVIDERS
  if (self->cloud_provider_account != NULL)
    g_signal_handlers_disconnect_by_data (self->cloud_provider_account, self);
  g_clear_object (&self->cloud_provider_account);
#endif

  G_OBJECT_CLASS (gtk_sidebar_row_parent_class)->finalize (object);
}

static void
gtk_sidebar_row_init (GtkSidebarRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_focus_on_click (GTK_WIDGET (self), FALSE);
}

static void
gtk_sidebar_row_class_init (GtkSidebarRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gtk_sidebar_row_get_property;
  object_class->set_property = gtk_sidebar_row_set_property;
  object_class->finalize = gtk_sidebar_row_finalize;

  properties [PROP_SIDEBAR] =
    g_param_spec_object ("sidebar", NULL, NULL,
                         GTK_TYPE_PLACES_SIDEBAR,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_START_ICON] =
    g_param_spec_object ("start-icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_END_ICON] =
    g_param_spec_object ("end-icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_TOOLTIP] =
    g_param_spec_string ("tooltip", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_EJECTABLE] =
    g_param_spec_boolean ("ejectable", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_ORDER_INDEX] =
    g_param_spec_int ("order-index", NULL, NULL,
                      0, G_MAXINT, 0,
                      (G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS));

  properties [PROP_SECTION_TYPE] =
    g_param_spec_enum ("section-type", NULL, NULL,
                       GTK_TYPE_PLACES_SECTION_TYPE,
                       GTK_PLACES_SECTION_INVALID,
                       (G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT_ONLY));

  properties [PROP_PLACE_TYPE] =
    g_param_spec_enum ("place-type", NULL, NULL,
                       GTK_TYPE_PLACES_PLACE_TYPE,
                       GTK_PLACES_INVALID,
                       (G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT_ONLY));

  properties [PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_DRIVE] =
    g_param_spec_object ("drive", NULL, NULL,
                         G_TYPE_DRIVE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_VOLUME] =
    g_param_spec_object ("volume", NULL, NULL,
                         G_TYPE_VOLUME,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_MOUNT] =
    g_param_spec_object ("mount", NULL, NULL,
                         G_TYPE_MOUNT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_CLOUD_PROVIDER_ACCOUNT] =
    g_param_spec_object ("cloud-provider-account", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PLACEHOLDER] =
    g_param_spec_boolean ("placeholder", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/libgtk/ui/gtksidebarrow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, start_icon_widget);
  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, end_icon_widget);
  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, label_widget);
  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, eject_button);
  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, revealer);
  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, busy_spinner);

  gtk_widget_class_bind_template_callback (widget_class, on_child_revealed);
  gtk_widget_class_set_css_name (widget_class, I_("row"));
}

GtkSidebarRow*
gtk_sidebar_row_clone (GtkSidebarRow *self)
{
 return g_object_new (GTK_TYPE_SIDEBAR_ROW,
                      "sidebar", self->sidebar,
                      "start-icon", self->start_icon,
                      "end-icon", self->end_icon,
                      "label", self->label,
                      "tooltip", self->tooltip,
                      "ejectable", self->ejectable,
                      "order-index", self->order_index,
                      "section-type", self->section_type,
                      "place-type", self->place_type,
                      "uri", self->uri,
                      "drive", self->drive,
                      "volume", self->volume,
                      "mount", self->mount,
                      "cloud-provider-account", self->cloud_provider_account,
                      NULL);
}

GtkWidget*
gtk_sidebar_row_get_eject_button (GtkSidebarRow *self)
{
  return self->eject_button;
}

void
gtk_sidebar_row_set_busy (GtkSidebarRow *row,
                          gboolean       is_busy)
{
  g_return_if_fail (GTK_IS_SIDEBAR_ROW (row));

  gtk_widget_set_visible (row->busy_spinner, is_busy);
}
