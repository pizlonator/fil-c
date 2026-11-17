/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2019 Purism SPC
 * Copyright (C) 2017 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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
 *
 */

#include "cc-about-page.h"
#include "cc-hostname-entry.h"
#include "cc-list-row.h"
#include "cc-system-details-window.h"

#include <config.h>
#include <glib/gi18n.h>

struct _CcAboutPage
{
  AdwNavigationPage parent_instance;

  CcListRow       *disk_row;
  CcListRow       *hardware_model_row;
  CcListRow       *memory_row;
  GtkPicture      *os_logo;
  CcListRow       *os_name_row;
  CcListRow       *processor_row;

  GtkWindow       *system_details_window;
  guint            create_system_details_id;
};

G_DEFINE_TYPE (CcAboutPage, cc_about_page, ADW_TYPE_NAVIGATION_PAGE)

static void
about_page_setup_overview (CcAboutPage *self)
{
  guint64 ram_size;
  g_autofree char *memory_text = NULL;
  g_autofree char *cpu_text = NULL;
  g_autofree char *os_name_text = NULL;
  g_autofree char *hardware_model_text = NULL;
  g_autofree gchar *disk_capacity_string = NULL;

  hardware_model_text = get_hardware_model_string ();
  cc_list_row_set_secondary_label (self->hardware_model_row, hardware_model_text);
  gtk_widget_set_visible (GTK_WIDGET (self->hardware_model_row), hardware_model_text != NULL);

  ram_size = get_ram_size_dmi ();
  if (ram_size == 0)
    ram_size = get_ram_size_libgtop ();
  memory_text = g_format_size_full (ram_size, G_FORMAT_SIZE_IEC_UNITS);
  cc_list_row_set_secondary_label (self->memory_row, memory_text);

  cpu_text = get_cpu_info ();
  cc_list_row_set_secondary_markup (self->processor_row, cpu_text);

  disk_capacity_string = get_primary_disk_info ();
  if (disk_capacity_string == NULL)
    disk_capacity_string = g_strdup (_("Unknown"));
  cc_list_row_set_secondary_label (self->disk_row, disk_capacity_string);

  os_name_text = get_os_name ();
  cc_list_row_set_secondary_label (self->os_name_row, os_name_text);
}

static gboolean
cc_about_page_create_system_details (CcAboutPage *self)
{
  if (!self->system_details_window)
    {
      self->system_details_window = GTK_WINDOW (cc_system_details_window_new ());
      g_object_ref_sink (self->system_details_window);
    }

  g_clear_handle_id (&self->create_system_details_id, g_source_remove);

  return G_SOURCE_REMOVE;
}

static void
cc_about_page_open_system_details (CcAboutPage *self)
{
  GtkNative *parent;

  cc_about_page_create_system_details (self);

  parent = gtk_widget_get_native (GTK_WIDGET (self));
  gtk_window_set_transient_for (self->system_details_window, GTK_WINDOW (parent));
  gtk_window_present (GTK_WINDOW (self->system_details_window));
}

#if !defined(DISTRIBUTOR_LOGO) || defined(DARK_MODE_DISTRIBUTOR_LOGO)
static gboolean
use_dark_theme (CcAboutPage *self)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default ();

  return adw_style_manager_get_dark (style_manager);
}
#endif

static void
setup_os_logo (CcAboutPage *self)
{
#ifdef DISTRIBUTOR_LOGO
#ifdef DARK_MODE_DISTRIBUTOR_LOGO
  if (use_dark_theme (self))
    {
      gtk_picture_set_filename (self->os_logo, DARK_MODE_DISTRIBUTOR_LOGO);
      return;
    }
#endif
  gtk_picture_set_filename (self->os_logo, DISTRIBUTOR_LOGO);
  return;
#else
  GtkIconTheme *icon_theme;
  g_autofree char *logo_name = g_get_os_info ("LOGO");
  g_autoptr(GtkIconPaintable) icon_paintable = NULL;
  g_autoptr(GPtrArray) array = NULL;
  g_autoptr(GIcon) icon = NULL;
  gboolean dark;

  dark = use_dark_theme (self);
  if (logo_name == NULL)
    logo_name = g_strdup ("gnome-logo");

  array = g_ptr_array_new_with_free_func (g_free);
  if (dark)
    g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s-text-dark", logo_name));
  g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s-text", logo_name));
  if (dark)
    g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s-dark", logo_name));
  g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s", logo_name));

  icon = g_themed_icon_new_from_names ((char **) array->pdata, array->len);
  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  icon_paintable = gtk_icon_theme_lookup_by_gicon (icon_theme, icon,
                                                   192,
                                                   gtk_widget_get_scale_factor (GTK_WIDGET (self)),
                                                   gtk_widget_get_direction (GTK_WIDGET (self)),
                                                   0);
  gtk_picture_set_paintable (self->os_logo, GDK_PAINTABLE (icon_paintable));
#endif
}

static void
cc_about_page_dispose (GObject *object)
{
  CcAboutPage *self = CC_ABOUT_PAGE (object);

  if (self->system_details_window)
    gtk_window_destroy (self->system_details_window);
  g_clear_object (&self->system_details_window);

  g_clear_handle_id (&self->create_system_details_id, g_source_remove);

  G_OBJECT_CLASS (cc_about_page_parent_class)->dispose (object);
}

static void
cc_about_page_class_init (CcAboutPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_about_page_dispose;

  g_type_ensure (CC_TYPE_HOSTNAME_ENTRY);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/about/cc-about-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, disk_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, hardware_model_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, memory_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, os_logo);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, os_name_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, processor_row);

  gtk_widget_class_bind_template_callback (widget_class, cc_about_page_open_system_details);

  g_type_ensure (CC_TYPE_LIST_ROW);
}

static void
cc_about_page_init (CcAboutPage *self)
{
  AdwStyleManager *style_manager;

  gtk_widget_init_template (GTK_WIDGET (self));

  about_page_setup_overview (self);

  style_manager = adw_style_manager_get_default ();
  g_signal_connect_object (style_manager, "notify::dark", G_CALLBACK (setup_os_logo), self, G_CONNECT_SWAPPED);
  setup_os_logo (self);

  self->create_system_details_id = g_idle_add (G_SOURCE_FUNC (cc_about_page_create_system_details), self);
}
